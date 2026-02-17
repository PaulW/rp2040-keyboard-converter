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

#include <math.h>

#include "hardware/clocks.h"
#include "interface.pio.h"

#include "bsp/board.h"

#include "common_interface.h"
#include "hid_interface.h"
#include "led_helper.h"
#include "log.h"
#include "pio_helper.h"

uint    mouse_sm     = 0;
uint    mouse_offset = 0;
PIO     mouse_pio;
uint8_t mouse_id = ATPS2_MOUSE_ID_UNKNOWN;
uint    mouse_data_pin;

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
    uint16_t data_with_parity = (uint16_t)(data_byte + (interface_parity_table[data_byte] << 8));
    pio_sm_put(mouse_pio, mouse_sm, data_with_parity);
}

/**
 * @brief Calculates the XY movement based on the given position and sign bit.
 * This function takes a position value and a sign bit and calculates the XY movement.
 * If the sign bit is set and the position is non-zero, the position is decremented by 0x100.
 * The resulting position is then clamped between -127 and 127.
 *
 * @param pos      The position value.
 * @param sign_bit The sign bit.
 *
 * @return The calculated XY movement as an int8_t value.
 */
int8_t get_xy_movement(uint8_t pos, int sign_bit) {
    int16_t new_pos = pos;
    if (new_pos && sign_bit) {
        new_pos -= 256;
    }
    new_pos = (new_pos > 127) ? 127 : (new_pos < -127) ? -127 : new_pos;
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
int8_t get_z_movement(uint8_t pos) {
    int8_t z_pos = pos & 0x0F;
    if (pos) {
        z_pos -= 8;
    }

    return z_pos;
}

/**
 * @brief Process the mouse event data.
 * This function is responsible for processing mouse events and updating the mouse state
 * accordingly. It handles various stages of mouse initialisation, including self-test, mouse type
 * detection, and configuration.  If the mouse is initialised, it processes the mouse data and sends
 * it to the HID interface.
 *
 * @param data_byte The data byte received from the mouse.
 *
 * @note Unlike the keyboard interface, the mouse interface does not utilise a ring buffer, and
 * instead sends data directly to the HID interface.
 */
void mouse_event_processor(uint8_t data_byte) {
    static uint8_t mouse_type_detect_sequence = 0;
    static uint8_t mouse_max_packets          = 0;

    switch (mouse_state) {
        case UNINITIALISED:
            switch (data_byte) {
                case ATPS2_RESP_BAT_PASSED:  // Self Test Passed
                    LOG_INFO("Mouse Self Test Passed\n");
                    LOG_INFO("Detecting Mouse Type\n");
                    mouse_id    = ATPS2_MOUSE_ID_UNKNOWN;  // Reset Mouse ID
                    mouse_state = INIT_AWAIT_ID;
                    break;
                default:
                    // If we hit this, then either the self test failed, or we have an error.
                    LOG_ERROR("Asking Mouse to Reset\n");
                    mouse_state = INIT_AWAIT_ACK;
                    mouse_command_handler(ATPS2_CMD_RESET);
            }
            break;
        case INIT_AWAIT_ACK:
            switch (data_byte) {
                case ATPS2_RESP_ACK:  // Acknowledged
                    LOG_INFO("ACK Received after Reset\n");
                    mouse_state = INIT_AWAIT_SELFTEST;
                    break;
                default:
                    LOG_DEBUG("Unknown ACK Response (0x%02X).  Asking again to Reset...\n",
                              data_byte);
                    mouse_command_handler(ATPS2_CMD_RESET);
            }
            break;
        case INIT_AWAIT_SELFTEST:
            switch (data_byte) {
                case ATPS2_RESP_BAT_PASSED:  // Self Test Passed
                    LOG_INFO("Mouse Self Test Passed\n");
                    LOG_INFO("Detecting Mouse Type\n");
                    mouse_id    = ATPS2_MOUSE_ID_UNKNOWN;  // Reset Mouse ID
                    mouse_state = INIT_AWAIT_ID;
                    break;
                default:
                    LOG_DEBUG("Self-Test invalid response (0x%02X).  Asking again to Reset...\n",
                              data_byte);
                    mouse_state = INIT_AWAIT_ACK;
                    mouse_command_handler(ATPS2_CMD_RESET);
            }
            break;
        case INIT_AWAIT_ID:
            switch (data_byte) {
                case ATPS2_RESP_ACK:
                    // Ack Received, silently ignore
                    break;
                case ATPS2_MOUSE_ID_STANDARD:
                    if (mouse_id == ATPS2_MOUSE_ID_UNKNOWN) {
                        mouse_id                   = ATPS2_MOUSE_ID_STANDARD;
                        mouse_type_detect_sequence = 0;
                        mouse_state                = INIT_DETECT_MOUSE_TYPE;
                        mouse_command_handler(ATPS2_MOUSE_CMD_SET_SAMPLE_RATE);
                    } else {
                        LOG_INFO("Mouse Type: Standard PS/2 Mouse\n");
                        mouse_max_packets = ATPS2_MOUSE_PACKET_STANDARD_SIZE;
                        mouse_state       = INIT_SET_CONFIG;
                        mouse_command_handler(ATPS2_MOUSE_CMD_SET_RESOLUTION);
                    }
                    break;
                case ATPS2_MOUSE_ID_INTELLIMOUSE:
                    if (mouse_id == ATPS2_MOUSE_ID_STANDARD) {
                        mouse_id    = ATPS2_MOUSE_ID_INTELLIMOUSE;
                        mouse_state = INIT_DETECT_MOUSE_TYPE;
                        mouse_command_handler(ATPS2_MOUSE_CMD_SET_SAMPLE_RATE);
                    } else {
                        LOG_INFO("Mouse Type: Mouse with Scroll Wheel\n");
                        mouse_max_packets = ATPS2_MOUSE_PACKET_EXTENDED_SIZE;
                        mouse_state       = INIT_SET_CONFIG;
                        mouse_command_handler(ATPS2_MOUSE_CMD_SET_RESOLUTION);
                    }
                    break;
                case ATPS2_MOUSE_ID_INTELLIMOUSE_EXPLORER:
                    LOG_INFO("Mouse Type: 5 Button Mouse\n");
                    mouse_max_packets = ATPS2_MOUSE_PACKET_EXTENDED_SIZE;
                    mouse_id          = ATPS2_MOUSE_ID_INTELLIMOUSE_EXPLORER;
                    mouse_state       = INIT_SET_CONFIG;
                    mouse_command_handler(ATPS2_MOUSE_CMD_SET_RESOLUTION);
                    break;
                default:
                    LOG_ERROR("Unknown Mouse Type (0x%02X), Asking again to Reset...\n", data_byte);
                    mouse_state = INIT_AWAIT_ACK;
                    mouse_command_handler(ATPS2_CMD_RESET);
                    break;
            }
            break;
        case INIT_DETECT_MOUSE_TYPE:
            // Detect Mouse Type
            switch (data_byte) {
                case ATPS2_RESP_ACK:
                    // Ack Received
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
                            mouse_command_handler(ATPS2_CMD_GET_ID);
                            break;
                    }
                    break;
                default:
                    LOG_DEBUG("Unhandled Response Received (0x%02X)\n", data_byte);
                    mouse_type_detect_sequence = 0;
                    mouse_id                   = ATPS2_MOUSE_ID_STANDARD;
                    mouse_state                = INIT_SET_CONFIG;
                    mouse_command_handler(ATPS2_MOUSE_CMD_SET_RESOLUTION);
            }
            break;
        case INIT_SET_CONFIG:
            // Finish Mouse Configuration
            //  - Set Resolution to 8 Counts/mm
            //  - Set Sampling to 1:1
            //  - Set Sample Rate to 40
            //  - Enable Mouse
            static const uint8_t config_sequence[] = {
                ATPS2_MOUSE_RES_8_COUNT_MM, ATPS2_MOUSE_CMD_SET_SCALING_1_1,
                ATPS2_MOUSE_CMD_SET_SAMPLE_RATE, ATPS2_MOUSE_RATE_40_HZ, ATPS2_MOUSE_CMD_ENABLE};
            if (data_byte == ATPS2_RESP_ACK) {
                static uint8_t mouse_config_sequence = 0;
                if (mouse_config_sequence == sizeof(config_sequence) / sizeof(config_sequence[0])) {
                    mouse_config_sequence = 0;
                    mouse_state           = INITIALISED;
                    LOG_INFO("Mouse Initialisation Complete\n");
                } else {
                    mouse_command_handler(config_sequence[mouse_config_sequence]);
                    mouse_config_sequence++;
                }
            }
            break;
        case INITIALISED:
            // Process Mouse Data.
            static uint8_t buttons[5]    = {0, 0, 0, 0, 0};
            static uint8_t parameters[4] = {0, 0, 0, 0};
            static int8_t  pos[3]        = {0, 0, 0};
            static int8_t  data_loop     = 0;
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
                    pos[X_POS] = !(parameters[X_OVERFLOW] || parameters[Y_OVERFLOW])
                                     ? get_xy_movement(data_byte, parameters[X_SIGN])
                                     : 0;
                    break;
                case 2:
                    // Read in Y Data
                    pos[Y_POS] = !(parameters[X_OVERFLOW] || parameters[Y_OVERFLOW])
                                     ? get_xy_movement(~data_byte, parameters[Y_SIGN] ^= 1)
                                     : 0;
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
                            pos[Z_POS] = get_z_movement(data_byte);              // Z Data Position
                            break;
                        default:
                            break;
                    }
                    break;
            }
            // Increment Loop Counter, but reset if we reach 3.
            data_loop++;

            // If we have processed all packets, then we can handle the mouse report.
            if (data_loop >= mouse_max_packets) {
                data_loop = 0;
                handle_mouse_report(buttons, pos);
            }
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
void mouse_input_event_handler() {
    io_ro_32 data_cast = mouse_pio->rxf[mouse_sm] >> 21;
    uint16_t data      = (uint16_t)data_cast;

    // Extract the Start Bit, Parity Bit and Stop Bit.
    uint8_t start_bit        = data & 0x1;
    uint8_t parity_bit       = (data >> 9) & 0x1;
    uint8_t stop_bit         = (data >> 10) & 0x1;
    uint8_t data_byte        = (uint8_t)((data_cast >> 1) & ATPS2_DATA_MASK);
    uint8_t parity_bit_check = interface_parity_table[data_byte];

    if (start_bit != 0 || parity_bit != parity_bit_check || stop_bit != 1) {
        if (start_bit != 0)
            LOG_ERROR("Start Bit Validation Failed: start_bit=%i\n", start_bit);
        if (stop_bit != 1)
            LOG_ERROR("Stop Bit Validation Failed: stop_bit=%i\n", stop_bit);
        if (parity_bit != parity_bit_check) {
            LOG_ERROR("Parity Bit Validation Failed: expected=%i, actual=%i\n", parity_bit_check,
                      parity_bit);
            mouse_command_handler(ATPS2_CMD_RESEND);  // Request Resend
            return;
        }
        // We should reset/restart the State Machine
        mouse_state = UNINITIALISED;
        mouse_id    = ATPS2_MOUSE_ID_UNKNOWN;
        pio_restart(mouse_pio, mouse_sm, mouse_offset);
    }

    mouse_event_processor(data_byte);
}

/**
 * @brief Task function for the mouse interface.
 * This function simply assists with the initialisation of the mouse interface and handles timeout
 * events.
 *
 * @note This function should be called periodically in the main loop, or within a task scheduler.
 */
void mouse_interface_task() {
    // Mouse Interface Initialisation helper
    // Here we handle Timeout events. If we don't receive responses from an attached Mouse with a
    // set period of time for any condition other than INITIALISED, we will then perform an
    // appropriate action.
    if (mouse_state != INITIALISED) {
        // If we are in an uninitialised state, we will send a reset command to the Mouse.
        static uint32_t detect_ms = 0;
        if (board_millis() - detect_ms > 200) {
            detect_ms                         = board_millis();
            static uint8_t detect_stall_count = 0;
            if (gpio_get(mouse_data_pin + 1) == 1) {
                // Only perform checks if the clock is HIGH
                detect_stall_count++;
                if (detect_stall_count > 5) {
                    // Reset Mouse as we have not received any data for 1 second.
                    LOG_ERROR("Mouse Interface Timeout.  Resetting Mouse...\n");
                    mouse_id           = ATPS2_MOUSE_ID_UNKNOWN;  // Reset Mouse ID
                    mouse_state        = INIT_AWAIT_ACK;     // Set State to Await Acknowledgement
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
 * @brief Initializes the AT/PS2 PIO interface for the mouse.
 * This function initializes the AT/PS2 PIO interface for the mouse by performing the following
 * steps:
 * 1. Resets the converter status if CONVERTER_LEDS is defined.
 * 2. Finds an available PIO to use for the keyboard interface program.
 * 3. Claims the PIO and loads the program.
 * 4. Sets up the IRQ for the PIO state machine.
 * 5. Defines the polling interval and cycles per clock for the state machine.
 * 6. Gets the base clock speed of the RP2040.
 * 7. Initializes the PIO interface program.
 * 8. Sets the IRQ handler and enables the IRQ.
 *
 * @param data_pin The data pin to be used for the mouse interface.
 */
void mouse_interface_setup(uint data_pin) {
#ifdef CONVERTER_LEDS
    converter.state.mouse_ready = 0;
    update_converter_status();  // Initialize converter status LEDs to "not ready" state
#endif

    // Find available PIO block
    mouse_pio = find_available_pio(&pio_interface_program);
    if (mouse_pio == NULL) {
        LOG_ERROR("AT/PS2 Mouse: No PIO available for mouse interface\n");
        return;
    }

    // Claim unused state machine
    mouse_sm = (uint)pio_claim_unused_sm(mouse_pio, true);

    // Load program into PIO instruction memory
    mouse_offset = pio_add_program(mouse_pio, &pio_interface_program);

    // Store pin assignment
    mouse_data_pin = data_pin;

    // Calculate clock divider for AT/PS2 timing (30µs minimum pulse width)
    // Reference: IBM 84F9735 PS/2 Hardware Interface Technical Reference
    float clock_div = calculate_clock_divider(ATPS2_TIMING_CLOCK_MIN_US);

    // Initialize PIO program with calculated clock divider
    pio_interface_program_init(mouse_pio, mouse_sm, mouse_offset, data_pin, clock_div);

    // Configure interrupt handling for PIO data reception
    uint pio_irq = mouse_pio == pio0 ? PIO0_IRQ_0 : PIO1_IRQ_0;

    // Register interrupt handler for RX FIFO events
    irq_set_exclusive_handler(pio_irq, &mouse_input_event_handler);

    // Set highest interrupt priority for time-critical mouse protocol timing
    irq_set_priority(pio_irq, 0x00);

    // Enable the IRQ
    irq_set_enabled(pio_irq, true);

    LOG_INFO("PIO%d SM%d Interface program loaded at mouse_offset %d with clock divider of %.2f\n",
             (mouse_pio == pio0 ? 0 : 1), mouse_sm, mouse_offset, clock_div);
}