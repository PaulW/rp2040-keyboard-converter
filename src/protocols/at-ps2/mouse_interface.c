/*
 * This file is part of RP2040 Keyboard Converter.
 *
 * Copyright 2023-2026 Paul Bramhall (paulwamp@gmail.com)
 *
 * RP2040 Keyboard Converter is free software: you can redistribute it
 * and/or modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * RP2040 Keyboard Converter is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with RP2040 Keyboard Converter.
 * If not, see <https://www.gnu.org/licenses/>.
 */

#include "mouse_interface.h"

#include <stdbool.h>
#include <stdint.h>

#include "bsp/board.h"

#include "common_interface.h"
#include "hid_interface.h"
#include "led_helper.h"
#include "log.h"
#include "pio_helper.h"

static pio_engine_t pio_engine = {.pio = NULL, .sm = -1, .offset = -1};

static uint8_t mouse_id = ATPS2_MOUSE_ID_UNKNOWN;
static uint    mouse_data_pin;
static int8_t  data_loop             = 0;  // Packet alignment counter (reset on re-init)
static uint8_t mouse_config_sequence = 0;  // Configuration sequence counter (reset on re-init)

/* Mouse packet state (cleared on re-initialisation) */
static uint8_t buttons[5]    = {0, 0, 0, 0, 0};
static uint8_t parameters[4] = {0, 0, 0, 0};
static int8_t  pos[3]        = {0, 0, 0};

/* Mouse detection timing constants */
enum {
    ATPS2_MOUSE_DETECT_INTERVAL_MS = 200, /**< Detection status check interval (ms) */
    ATPS2_MOUSE_STALL_LIMIT        = 5,   /**< Max stall count before reset (5 × 200ms = 1s) */
};

static enum {
    UNINITIALISED,
    INIT_AWAIT_ACK,
    INIT_AWAIT_SELFTEST,
    INIT_AWAIT_ID,
    INIT_DETECT_MOUSE_TYPE,
    INIT_SET_CONFIG,
    INITIALISED,
} mouse_state = UNINITIALISED;

typedef enum {
    BUTTON_LEFT,
    BUTTON_RIGHT,
    BUTTON_MIDDLE,
    BUTTON_BACKWARD,
    BUTTON_FORWARD
} button_index;

typedef enum {
    X_SIGN,
    Y_SIGN,
    X_OVERFLOW,
    Y_OVERFLOW,
} parameter_index;

typedef enum { X_POS, Y_POS, Z_POS } mousepos_index;

/**
 * @brief Command Handler function to issue commands to the attached AT/PS2 Mouse.
 * This function is responsible for handling commands to be sent to the AT/PS2 Mouse. It takes a
 * single parameter, `data_byte`, which represents the command to be issued. The function calculates
 * the parity of the data byte and combines it with the data byte to form a 16-bit value. This value
 * is then sent to the peripheral using the PIO state machine.
 *
 * @param data_byte The command byte to be sent to the AT/PS2 Mouse.
 */
static void mouse_command_handler(uint8_t data_byte) {
    atps2_send_command(&pio_engine, data_byte);
}

/**
 * @brief Calculates the XY movement based on the given position and sign bit.
 * This function takes a position value and a sign bit and calculates the XY movement.
 * If the sign bit is set, the position is decremented by 256 for proper 9-bit signed conversion.
 * The resulting position is then clamped between -127 and 127.
 *
 * @param pos      The position value (8-bit magnitude).
 * @param sign_bit The sign bit (9th bit for signed interpretation).
 *
 * @return The calculated XY movement as an int8_t value.
 */
static int8_t get_xy_movement(uint8_t pos, bool sign_bit) {
    int16_t new_pos = (int16_t)pos;
    if (sign_bit) {
        new_pos -= 256;
    }
    if (new_pos > (int16_t)INT8_MAX) {
        new_pos = (int16_t)INT8_MAX;
    } else if (new_pos < (int16_t)(-INT8_MAX)) {
        new_pos = (int16_t)(-INT8_MAX);
    }
    return (int8_t)new_pos;
}

/**
 * @brief Retrieves the Z-axis movement from the given position.
 * This function extracts the Z-axis movement from the given position byte.
 * The position byte is expected to be in the format: [0 | 0 | 0 | 0 | Z3 | Z2 | Z1 | Z0],
 * where Z3-Z0 represent the Z-axis movement.
 *
 * @param pos The position byte from which to extract the Z-axis movement.
 *
 * @return The Z-axis movement as a signed 8-bit integer.
 */

/** Nibble mask for extracting the Z-axis value from the lower 4 bits of an extended mouse packet
 * byte. */
#define ATPS2_MOUSE_Z_NIBBLE_MASK 0x0FU

static int8_t get_z_movement(uint8_t pos) {
    int8_t z_pos = (int8_t)(pos & ATPS2_MOUSE_Z_NIBBLE_MASK);
    if (z_pos & 0x08) {
        z_pos -= 16;
    }

    return z_pos;
}

/* Packet processing state — promoted to file scope to support handler extraction */
static uint8_t mouse_type_detect_sequence =
    0;                                /**< Type-detection sequence counter (cleared on re-init) */
static uint8_t mouse_max_packets = 0; /**< Packet count for current mouse type */

/**
 * @brief Handle mouse event in UNINITIALISED state.
 * @param data_byte The data byte received from the mouse.
 * @return void
 * @note IRQ-safe — called exclusively from mouse_input_event_handler() which runs in ISR context.
 *       Must not call blocking functions or non-reentrant code.
 */
static void handle_mouse_uninitialised(uint8_t data_byte) {
    switch (data_byte) {
        case ATPS2_RESP_BAT_PASSED:
            LOG_INFO("Mouse Self Test Passed\n");
            LOG_INFO("Detecting Mouse Type\n");
            mouse_id    = ATPS2_MOUSE_ID_UNKNOWN;
            mouse_state = INIT_AWAIT_ID;
            __dmb();  // Memory barrier - ensure state write visible to main loop
            break;
        default:
            LOG_ERROR("Asking Mouse to Reset\n");
            mouse_state = INIT_AWAIT_ACK;
            __dmb();  // Memory barrier - ensure state write visible to main loop
            mouse_command_handler(ATPS2_CMD_RESET);
    }
}

/**
 * @brief Handle mouse event in INIT_AWAIT_ACK state.
 * @param data_byte The data byte received from the mouse.
 * @return void
 * @note IRQ-safe — called exclusively from mouse_input_event_handler() which runs in ISR context.
 *       Must not call blocking functions or non-reentrant code.
 */
static void handle_mouse_await_ack(uint8_t data_byte) {
    switch (data_byte) {
        case ATPS2_RESP_ACK:
            LOG_INFO("ACK Received after Reset\n");
            mouse_state = INIT_AWAIT_SELFTEST;
            __dmb();  // Memory barrier - ensure state write visible to main loop
            break;
        default:
            LOG_DEBUG("Unknown ACK Response (0x%02X).  Asking again to Reset...\n", data_byte);
            mouse_command_handler(ATPS2_CMD_RESET);
    }
}

/**
 * @brief Handle mouse event in INIT_AWAIT_SELFTEST state.
 * @param data_byte The data byte received from the mouse.
 * @return void
 * @note IRQ-safe — called exclusively from mouse_input_event_handler() which runs in ISR context.
 *       Must not call blocking functions or non-reentrant code.
 */
static void handle_mouse_await_selftest(uint8_t data_byte) {
    switch (data_byte) {
        case ATPS2_RESP_BAT_PASSED:
            LOG_INFO("Mouse Self Test Passed\n");
            LOG_INFO("Detecting Mouse Type\n");
            mouse_id    = ATPS2_MOUSE_ID_UNKNOWN;
            mouse_state = INIT_AWAIT_ID;
            __dmb();  // Memory barrier - ensure state write visible to main loop
            break;
        default:
            LOG_DEBUG("Self-Test invalid response (0x%02X).  Asking again to Reset...\n",
                      data_byte);
            mouse_state = INIT_AWAIT_ACK;
            __dmb();  // Memory barrier - ensure state write visible to main loop
            mouse_command_handler(ATPS2_CMD_RESET);
    }
}

/**
 * @brief Handle mouse event in INIT_AWAIT_ID state.
 * @param data_byte The data byte received from the mouse.
 * @return void
 * @note IRQ-safe — called exclusively from mouse_input_event_handler() which runs in ISR context.
 *       Must not call blocking functions or non-reentrant code.
 */
static void handle_mouse_await_id(uint8_t data_byte) {
    switch (data_byte) {
        case ATPS2_RESP_ACK:
            // Ack Received, silently ignore
            break;
        case ATPS2_MOUSE_ID_STANDARD:
            if (mouse_id == ATPS2_MOUSE_ID_UNKNOWN) {
                mouse_id                   = ATPS2_MOUSE_ID_STANDARD;
                mouse_type_detect_sequence = 0;
                mouse_state                = INIT_DETECT_MOUSE_TYPE;
                __dmb();  // Memory barrier - ensure state write visible to main loop
                mouse_command_handler(ATPS2_MOUSE_CMD_SET_SAMPLE_RATE);
            } else {
                LOG_INFO("Mouse Type: Standard PS/2 Mouse\n");
                mouse_max_packets     = ATPS2_MOUSE_PACKET_STANDARD_SIZE;
                mouse_config_sequence = 0;
                mouse_state           = INIT_SET_CONFIG;
                __dmb();  // Memory barrier - ensure state write visible to main loop
                mouse_command_handler(ATPS2_MOUSE_CMD_SET_RESOLUTION);
            }
            break;
        case ATPS2_MOUSE_ID_INTELLIMOUSE:
            if (mouse_id == ATPS2_MOUSE_ID_STANDARD) {
                mouse_id    = ATPS2_MOUSE_ID_INTELLIMOUSE;
                mouse_state = INIT_DETECT_MOUSE_TYPE;
                __dmb();  // Memory barrier - ensure state write visible to main loop
                mouse_command_handler(ATPS2_MOUSE_CMD_SET_SAMPLE_RATE);
            } else {
                LOG_INFO("Mouse Type: Mouse with Scroll Wheel\n");
                mouse_max_packets     = ATPS2_MOUSE_PACKET_EXTENDED_SIZE;
                mouse_config_sequence = 0;
                mouse_state           = INIT_SET_CONFIG;
                __dmb();  // Memory barrier - ensure state write visible to main loop
                mouse_command_handler(ATPS2_MOUSE_CMD_SET_RESOLUTION);
            }
            break;
        case ATPS2_MOUSE_ID_INTELLIMOUSE_EXPLORER:
            LOG_INFO("Mouse Type: 5 Button Mouse\n");
            mouse_max_packets     = ATPS2_MOUSE_PACKET_EXTENDED_SIZE;
            mouse_id              = ATPS2_MOUSE_ID_INTELLIMOUSE_EXPLORER;
            mouse_config_sequence = 0;
            mouse_state           = INIT_SET_CONFIG;
            __dmb();  // Memory barrier - ensure state write visible to main loop
            mouse_command_handler(ATPS2_MOUSE_CMD_SET_RESOLUTION);
            break;
        default:
            LOG_ERROR("Unknown Mouse Type (0x%02X), Asking again to Reset...\n", data_byte);
            mouse_state = INIT_AWAIT_ACK;
            __dmb();  // Memory barrier - ensure state write visible to main loop
            mouse_command_handler(ATPS2_CMD_RESET);
            break;
    }
}

/**
 * @brief Handle mouse event in INIT_DETECT_MOUSE_TYPE state.
 * @param data_byte The data byte received from the mouse.
 * @return void
 * @note IRQ-safe — called exclusively from mouse_input_event_handler() which runs in ISR context.
 *       Must not call blocking functions or non-reentrant code.
 */
static void handle_mouse_detect_type(uint8_t data_byte) {
    switch (data_byte) {
        case ATPS2_RESP_ACK:
            switch (mouse_type_detect_sequence) {
                case 0:
                    // Set Sample Rate to 200
                    mouse_type_detect_sequence++;
                    mouse_command_handler(ATPS2_MOUSE_RATE_200_HZ);
                    break;
                case 1:
                    // Ask to Set Sample Rate
                    mouse_type_detect_sequence++;
                    mouse_command_handler(ATPS2_MOUSE_CMD_SET_SAMPLE_RATE);
                    break;
                case 2:
                    // Set Sample Rate based on ID
                    mouse_type_detect_sequence++;
                    mouse_command_handler((mouse_id == ATPS2_MOUSE_ID_INTELLIMOUSE)
                                              ? ATPS2_MOUSE_RATE_200_HZ
                                              : ATPS2_MOUSE_RATE_100_HZ);
                    break;
                case 3:
                    // Ask to Set Sample Rate
                    mouse_type_detect_sequence++;
                    mouse_command_handler(ATPS2_MOUSE_CMD_SET_SAMPLE_RATE);
                    break;
                case 4:
                    // Set Sample Rate to 80
                    mouse_type_detect_sequence++;
                    mouse_command_handler(ATPS2_MOUSE_RATE_80_HZ);
                    break;
                case 5:
                    // Re-request Mouse ID
                    mouse_type_detect_sequence = 0;
                    mouse_state                = INIT_AWAIT_ID;
                    __dmb();  // Memory barrier - ensure state write visible to main loop
                    mouse_command_handler(ATPS2_CMD_GET_ID);
                    break;
                default:
                    break;
            }
            break;
        default:
            LOG_DEBUG("Unhandled Response Received (0x%02X)\n", data_byte);
            mouse_type_detect_sequence = 0;
            mouse_id                   = ATPS2_MOUSE_ID_STANDARD;
            mouse_max_packets          = ATPS2_MOUSE_PACKET_STANDARD_SIZE;
            mouse_config_sequence      = 0;
            mouse_state                = INIT_SET_CONFIG;
            __dmb();  // Memory barrier - ensure state write visible to main loop
            mouse_command_handler(ATPS2_MOUSE_CMD_SET_RESOLUTION);
    }
}

/**
 * @brief Handle mouse event in INIT_SET_CONFIG state.
 * @param data_byte The data byte received from the mouse.
 * @return void
 * @note IRQ-safe — called exclusively from mouse_input_event_handler() which runs in ISR context.
 *       Must not call blocking functions or non-reentrant code.
 */
static void handle_mouse_set_config(uint8_t data_byte) {
    // Configuration sequence:
    //  - Set Resolution to 8 Counts/mm
    //  - Set Sampling to 1:1
    //  - Set Sample Rate to 40
    //  - Enable Mouse
    static const uint8_t config_sequence[] = {
        ATPS2_MOUSE_RES_8_COUNT_MM, ATPS2_MOUSE_CMD_SET_SCALING_1_1,
        ATPS2_MOUSE_CMD_SET_SAMPLE_RATE, ATPS2_MOUSE_RATE_40_HZ, ATPS2_MOUSE_CMD_ENABLE};
    if (data_byte == ATPS2_RESP_ACK) {
        if (mouse_config_sequence == sizeof(config_sequence) / sizeof(config_sequence[0])) {
            mouse_config_sequence = 0;
            mouse_state           = INITIALISED;
            __dmb();        // Memory barrier - ensure state write visible to main loop
            data_loop = 0;  // Reset packet alignment on initialisation
            // Clear extended-mouse fields in case mouse type changed
            buttons[BUTTON_BACKWARD] = 0;
            buttons[BUTTON_FORWARD]  = 0;
            pos[Z_POS]               = 0;
            LOG_INFO("Mouse Initialisation Complete\n");
        } else {
            mouse_command_handler(config_sequence[mouse_config_sequence]);
            mouse_config_sequence++;
        }
    }
}

/**
 * @brief Handle mouse event in INITIALISED state (packet assembly and HID reporting).
 * @param data_byte The data byte received from the mouse.
 * @return void
 * @note IRQ-safe — called exclusively from mouse_input_event_handler() which runs in ISR context.
 *       Must not call blocking functions or non-reentrant code.
 */
static void handle_mouse_initialised(uint8_t data_byte) {
    switch (data_loop) {
        case 0:
            // Read in Button Data, as well as X and Y Overflow Data
            buttons[BUTTON_LEFT]   = data_byte & 0x01;         // Left Button
            buttons[BUTTON_RIGHT]  = (data_byte >> 1) & 0x01;  // Right Button
            buttons[BUTTON_MIDDLE] = (data_byte >> 2) & 0x01;  // Middle Button
            parameters[X_SIGN]     = (data_byte >> 4) & 0x01;  // X Sign Bit
            parameters[Y_SIGN]     = (data_byte >> 5) & 0x01;  // Y Sign Bit
            parameters[X_OVERFLOW] = (data_byte >> 6) & 0x01;  // X Overflow Bit
            parameters[Y_OVERFLOW] = (data_byte >> 7) & 0x01;  // Y Overflow Bit
            break;
        case 1:
            // Read in X Data
            pos[X_POS] = (int8_t)(!(parameters[X_OVERFLOW] || parameters[Y_OVERFLOW])
                                      ? get_xy_movement(data_byte, parameters[X_SIGN])
                                      : 0);
            break;
        case 2:
            // Read in Y Data
            // Y-axis requires data inversion and sign flip (protocol quirk)
            parameters[Y_SIGN] ^= 1;
            pos[Y_POS] = (int8_t)(!(parameters[X_OVERFLOW] || parameters[Y_OVERFLOW])
                                      ? get_xy_movement(~data_byte, parameters[Y_SIGN])
                                      : 0);
            break;
        case 3:
            // Read in Z/Extended Data
            switch (mouse_id) {
                case ATPS2_MOUSE_ID_INTELLIMOUSE:
                    // Scroll Wheel Mouse
                    pos[Z_POS] = get_z_movement(data_byte);
                    break;
                case ATPS2_MOUSE_ID_INTELLIMOUSE_EXPLORER:
                    // 5 Button Mouse
                    buttons[BUTTON_BACKWARD] = (data_byte >> 4) & 0x01;  // Button 4
                    buttons[BUTTON_FORWARD]  = (data_byte >> 5) & 0x01;  // Button 5
                    pos[Z_POS]               = get_z_movement(data_byte);
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
    // Increment loop counter; reset when all packets for this mouse type are received.
    data_loop++;

    // If we have processed all packets, then we can handle the mouse report.
    if (data_loop >= mouse_max_packets) {
        data_loop = 0;
        handle_mouse_report(buttons, pos);
    }
}

/**
 * @brief Process the mouse event data.
 *
 * Dispatches a received data byte to the appropriate per-state handler based on the current
 * protocol state.  Handles all initialisation phases and normal operation (packet assembly
 * and HID reporting).
 *
 * @param data_byte The data byte received from the mouse.
 *
 * @note Unlike the keyboard interface, the mouse interface does not utilise a ring buffer, and
 * instead sends data directly to the HID interface.
 */
void mouse_event_processor(uint8_t data_byte) {
    switch (mouse_state) {
        case UNINITIALISED:
            handle_mouse_uninitialised(data_byte);
            break;
        case INIT_AWAIT_ACK:
            handle_mouse_await_ack(data_byte);
            break;
        case INIT_AWAIT_SELFTEST:
            handle_mouse_await_selftest(data_byte);
            break;
        case INIT_AWAIT_ID:
            handle_mouse_await_id(data_byte);
            break;
        case INIT_DETECT_MOUSE_TYPE:
            handle_mouse_detect_type(data_byte);
            break;
        case INIT_SET_CONFIG:
            handle_mouse_set_config(data_byte);
            break;
        case INITIALISED:
            handle_mouse_initialised(data_byte);
            break;
    }
#ifdef CONVERTER_LEDS
    converter.state.mouse_ready = mouse_state == INITIALISED ? 1 : 0;
    update_converter_status();
#endif
}

/**
 * @brief IRQ Event Handler used to read data from the AT/PS2 Mouse.
 * This function is responsible for handling the interrupt request (IRQ) event that occurs when data
 * is received from the AT/PS2 Mouse.
 * - It extracts the start bit, parity bit, stop bit, and data byte from the received data.
 * - It then performs validation checks on the start bit, parity bit, and stop bit.
 * - If any of the validation checks fail, error messages are printed and appropriate actions are
 * taken.
 * - If all the validation checks pass, the data byte is processed by the mouse_event_processor()
 * function.
 */
static void __isr mouse_input_event_handler(void) {
    if (pio_sm_is_rx_fifo_empty(pio_engine.pio, pio_engine.sm)) {
        return;
    }

    // Parse AT/PS2 frame components from 11-bit PIO data
    atps2_frame_t frame = atps2_parse_frame(&pio_engine);

    // Validate start bit (must be 0)
    if (frame.start_bit != 0) {
        LOG_ERROR("Start Bit Validation Failed: start_bit=%i\n", frame.start_bit);
        mouse_state = UNINITIALISED;
        __dmb();  // Memory barrier - ensure state write visible to main loop
        mouse_id = ATPS2_MOUSE_ID_UNKNOWN;
        pio_restart(pio_engine.pio, pio_engine.sm, pio_engine.offset);
        return;
    }

    // Validate stop bit (must be 1)
    if (frame.stop_bit != 1) {
        LOG_ERROR("Stop Bit Validation Failed: stop_bit=%i\n", frame.stop_bit);
        mouse_state = UNINITIALISED;
        __dmb();  // Memory barrier - ensure state write visible to main loop
        mouse_id = ATPS2_MOUSE_ID_UNKNOWN;
        pio_restart(pio_engine.pio, pio_engine.sm, pio_engine.offset);
        return;
    }

    // Validate parity bit
    if (frame.parity_bit != frame.expected_parity) {
        LOG_ERROR("Parity Bit Validation Failed: expected=%i, actual=%i\n", frame.expected_parity,
                  frame.parity_bit);
        mouse_state = UNINITIALISED;
        __dmb();  // Memory barrier - ensure state write visible to main loop
        mouse_id = ATPS2_MOUSE_ID_UNKNOWN;
        pio_restart(pio_engine.pio, pio_engine.sm, pio_engine.offset);
        mouse_command_handler(ATPS2_CMD_RESEND);  // Request Resend (after restart so it transmits)
        return;
    }

    // All validations passed - process the data byte
    mouse_event_processor(frame.data);
}

/**
 * @brief Task function for the mouse interface.
 * This function simply assists with the initialisation of the mouse interface and handles timeout
 * events.
 *
 * @note This function should be called periodically in the main loop, or within a task scheduler.
 */
void mouse_interface_task() {
    // Guard against uninitialised PIO engine
    if (pio_engine.pio == NULL) {
        return;
    }

    // Mouse Interface Initialisation helper
    // Here we handle Timeout events. If we don't receive responses from an attached Mouse with a
    // set period of time for any condition other than INITIALISED, we will then perform an
    // appropriate action.
    __dmb();  // Memory barrier - ensure we see latest state from IRQ
    if (mouse_state != INITIALISED) {
        // If we are in an uninitialised state, we will send a reset command to the Mouse.
        static uint32_t detect_ms = 0;
        if (board_millis() - detect_ms > ATPS2_MOUSE_DETECT_INTERVAL_MS) {
            detect_ms                         = board_millis();
            static uint8_t detect_stall_count = 0;
            if (gpio_get(mouse_data_pin + 1) == 1) {
                // Only perform checks if the clock is HIGH
                detect_stall_count++;
                if (detect_stall_count > ATPS2_MOUSE_STALL_LIMIT) {
                    // Reset Mouse as we have not received any data for 1 second.
                    LOG_ERROR("Mouse Interface Timeout.  Resetting Mouse...\n");
                    mouse_id    = ATPS2_MOUSE_ID_UNKNOWN;  // Reset Mouse ID
                    mouse_state = INIT_AWAIT_ACK;          // Set State to Await Acknowledgement
                    __dmb();  // Memory barrier - ensure state write visible to main loop
                    detect_stall_count = 0;                  // Reset Stall Counter
                    mouse_command_handler(ATPS2_CMD_RESET);  // Send Reset Command
                }
            } else if (mouse_state == UNINITIALISED) {
                LOG_DEBUG("Awaiting mouse detection. Please ensure a mouse is connected.\n");
                detect_stall_count =
                    0;  // Reset the detect_stall_count as we are waiting for the clock to go HIGH.
            }
#ifdef CONVERTER_LEDS
            converter.state.mouse_ready = mouse_state == INITIALISED ? 1 : 0;
            update_converter_status();
#endif
        }
    }
}

/**
 * @brief Initialises the AT/PS2 PIO interface for the mouse.
 * This function initialises the AT/PS2 PIO interface for the mouse by performing the following
 * steps:
 * 1. Resets the converter status if CONVERTER_LEDS is defined.
 * 2. Finds an available PIO to use for the keyboard interface program.
 * 3. Claims the PIO and loads the program.
 * 4. Sets up the IRQ for the PIO state machine.
 * 5. Defines the polling interval and cycles per clock for the state machine.
 * 6. Gets the base clock speed of the RP2040.
 * 7. Initialises the PIO interface program.
 * 8. Sets the IRQ handler and enables the IRQ.
 *
 * @param data_pin The data pin to be used for the mouse interface.
 */
void mouse_interface_setup(uint data_pin) {
#ifdef CONVERTER_LEDS
    converter.state.mouse_ready = 0;
    update_converter_status();  // Initialise converter status LEDs to "not ready" state
#endif

    if (!atps2_setup_pio_engine(&pio_engine, data_pin, &mouse_input_event_handler, "Mouse")) {
        return;
    }

    mouse_data_pin = data_pin;
}