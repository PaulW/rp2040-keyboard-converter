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

/**
 * @file keyboard_interface.c
 * @brief AT/PS2 Keyboard Protocol Implementation
 *
 * This implementation handles the AT and PS/2 keyboard protocols used by IBM PC
 * compatible systems from the 1980s onwards. The protocols feature:
 *
 * Physical Interface:
 * - 2-wire synchronous serial communication (DATA + CLOCK)
 * - Open-drain signals with pull-up resistors
 * - 5V TTL logic levels (AT) or 5V/3.3V (PS/2)
 * - DIN connector (AT) or mini-DIN connector (PS/2)
 *
 * Communication Protocol:
 * - LSB-first bit transmission (bit 0 → bit 7)
 * - Frame format: Start(0) + 8 data bits + Parity(odd) + Stop(1)
 * - Keyboard controls clock for transmission to host
 * - Host can inhibit by pulling CLOCK low
 * - Bidirectional with command/response capability
 *
 * Timing Characteristics:
 * - Clock frequency: 10-16.7 kHz (typical 10-15 kHz)
 * - Clock pulse width: minimum 30µs low, 30µs high
 * - Data setup time: minimum 5µs before clock falling edge
 * - Data hold time: minimum 5µs after clock falling edge
 * - Inhibit time: 102µs CLOCK low (spec requires ≥100µs; exceeds max bit period, stops
 * transmission)
 *
 * State Machine:
 * 1. UNINITIALISED → Reset keyboard and wait for self-test
 * 2. INIT_AWAIT_SELFTEST → Wait for 0xAA (BAT completion)
 * 3. INIT_READ_ID → Request and read 2-byte keyboard ID
 * 4. INIT_SETUP → Configure scan code set and keyboard parameters
 * 5. SET_LOCK_LEDS → Initialise LED states
 * 6. INITIALISED → Normal operation with scan code processing
 *
 * Special Features:
 * - Multiple scan code set support (Sets 1, 2, 3)
 * - LED control (Caps/Num/Scroll Lock)
 * - Typematic repeat rate configuration
 * - Z-150 keyboard compatibility (non-standard stop bit)
 * - Terminal keyboard support (Set 3 scan codes)
 *
 * Error Handling:
 * - Automatic retry for failed commands
 * - Timeout detection with state machine reset
 * - Parity error detection and recovery
 * - Graceful handling of non-standard keyboards
 *
 * Known Limitation:
 * - Some older or non-compliant keyboards (observed in a 1988 Cherry unit)
 *   emit a bare sequence for Pause that lacks the standard E1 prefix and can
 *   therefore be indistinguishable from an intentional Ctrl+NumLock sequence.
 *   Detecting this in firmware without introducing heuristics that risk breaking
 *   normal Ctrl+key combinations is not safe. The recommended workaround is to
 *   remap an unused physical key (for example Scroll Lock) to PAUS in the
 *   keyboard configuration for that physical keyboard.
 *
 * @see keyboard_interface.h for the public API and keyboard command protocol constants.
 */

#include "keyboard_interface.h"

#include <stdbool.h>
#include <stdint.h>

#include "pico/stdlib.h"

#include "bsp/board.h"
#include "tusb.h"

#include "common_interface.h"
#include "flow_tracker.h"
#include "led_helper.h"
#include "log.h"
#include "pio_helper.h"
#include "ringbuf.h"

// Include appropriate scancode header
// Try unified processor first (set123), fall back to legacy
#if __has_include("scancode_runtime.h")
#include "scancode.h"
#include "scancode_runtime.h"
#define USING_SET123 1

// Global scancode configuration (set during keyboard initialisation)
static const scancode_config_t* g_scancode_config = NULL;
#else
#include "scancode.h"
#define USING_SET123 0
#endif

// PIO State Machine Configuration
static pio_engine_t pio_engine = {.pio = NULL, .sm = -1, .offset = -1};

static uint16_t keyboard_id =
    ATPS2_KEYBOARD_ID_UNKNOWN; /**< Keyboard identification code (2 bytes) */
static uint keyboard_data_pin; /**< GPIO pin for DATA line (CLOCK = DATA + 1) */

/**
 * @brief Terminal Keyboard Detection Macro
 *
 * Determines if the configured keyboard uses Scan Code Set 3, which indicates
 * a terminal keyboard requiring special initialisation and handling procedures.
 * Terminal keyboards often have different capabilities and command sets.
 */
#define CODESET_3 (strcmp(KEYBOARD_CODESET, "set3") == 0)

// Protocol State and Configuration Variables
static volatile uint8_t keyboard_lock_leds = 0;     /**< Current LED state (Caps/Num/Scroll Lock) */
static volatile bool    id_retry           = false; /**< Flag indicating ID read retry attempt */

// Initialisation timeout policy
#define ATPS2_INIT_CHECK_MS       200U
#define ATPS2_ID_RETRY_LIMIT      2U
#define ATPS2_DETECT_MAX_ATTEMPTS 5U

/**
 * @brief PIO Frame-Shift Artifact for BAT Response
 *
 * When a keyboard is physically connected during operation a PIO frame-shift may cause
 * the BAT response (0xAA) to arrive as 0x54 with incorrect parity.  This is a known
 * hardware artefact and does not represent a valid response code.
 */
#define ATPS2_RESP_BAT_PIO_CONNECT 0x54U

// Initialisation detection state — promoted to file scope to allow helper functions
static uint8_t  detect_stall_count = 0; /**< Timeout counter for init stall detection */
static uint32_t detect_ms          = 0; /**< Timestamp of last detection interval check */

/**
 * @brief Stop Bit Compliance Detection
 *
 * AT/PS2 protocol specifies stop bit should be HIGH (1) after parity bit.
 * Some non-standard keyboards (like Zenith Z-150) use LOW (0) stop bit.
 * This detection helps identify and accommodate non-compliant keyboards.
 *
 * @see interface.pio for detailed signal timing implementation
 */
typedef enum {
    STOP_BIT_LOW, /**< Non-standard stop bit (Z-150 style keyboards) */
    STOP_BIT_HIGH /**< Standard AT/PS2 stop bit (compliant keyboards) */
} stop_bit_state_t;
static stop_bit_state_t stop_bit_state = STOP_BIT_HIGH;

/**
 * @brief AT/PS2 Protocol State Machine
 *
 * Manages the complete initialisation and operational phases:
 * - UNINITIALISED: System startup, sending reset command
 * - INIT_AWAIT_ACK: Waiting for acknowledge to reset command
 * - INIT_AWAIT_SELFTEST: Waiting for Basic Assurance Test (BAT) completion (0xAA)
 * - INIT_READ_ID_1: Reading first byte of keyboard identification
 * - INIT_READ_ID_2: Reading second byte of keyboard identification
 * - INIT_SETUP: Configuring scan code set and keyboard parameters
 * - SET_LOCK_LEDS: Setting initial LED states
 * - INITIALISED: Normal operation, processing scan codes
 */
typedef enum {
    UNINITIALISED,       /**< Initial state, sending keyboard reset */
    INIT_AWAIT_ACK,      /**< Waiting for reset command acknowledgment */
    INIT_AWAIT_SELFTEST, /**< Waiting for self-test completion (0xAA) */
    INIT_READ_ID_1,      /**< Reading first byte of keyboard ID */
    INIT_READ_ID_2,      /**< Reading second byte of keyboard ID */
    INIT_SETUP,          /**< Configuring scan code set and options */
    SET_LOCK_LEDS,       /**< Setting LED states */
    INITIALISED,         /**< Normal operation, processing scan codes */
} keyboard_state_t;
static keyboard_state_t keyboard_state = UNINITIALISED;

/**
 * @brief AT/PS2 Keyboard Command Handler
 *
 * Transmits commands from host to keyboard using the AT/PS2 protocol. The function
 * handles the complete command transmission process including:
 *
 * Command Processing:
 * - Calculates odd parity bit for the 8-bit command
 * - Constructs 16-bit transmission frame (command + parity)
 * - Queues data to PIO state machine for transmission
 *
 * Protocol Compliance:
 * - Follows AT/PS2 host-to-keyboard transmission format
 * - Frame format: Start(0) + 8 data bits + Parity(odd) + Stop(1)
 * - Host controls transmission timing by pulling CLOCK low
 * - Automatic parity calculation ensures data integrity
 *
 * Common Commands:
 * - 0xED: Set LED states (followed by LED bitmap)
 * - 0xF2: Read keyboard ID (returns 2-byte response)
 * - 0xF3: Set typematic rate/delay (followed by rate byte)
 * - 0xF4: Enable scanning (start sending scan codes)
 * - 0xF5: Disable scanning (stop sending scan codes)
 * - 0xFE: Resend last response (error recovery)
 * - 0xFF: Reset keyboard (triggers self-test sequence)
 *
 * @param data_byte 8-bit command code to transmit to keyboard
 *
 * @note IRQ-safe.
 */
static void keyboard_command_handler(uint8_t data_byte) {
    atps2_send_command(&pio_engine, data_byte);
}

/**
 * @brief AT/PS2 Keyboard Event Processor
 *
 * Processes all data received from the keyboard and manages the complete initialisation
 * and operational state machine. Handles the complex AT/PS2 protocol sequence:
 *
 * Initialisation States:
 * - UNINITIALISED: Power-on detection and initial reset command
 * - INIT_AWAIT_ACK: Waiting for command acknowledgment (0xFA)
 * - INIT_AWAIT_SELFTEST: Waiting for BAT completion (0xAA = pass, 0xFC = fail)
 * - INIT_READ_ID_1/2: Reading 2-byte keyboard identification code
 * - INIT_SETUP: Configuring scan code set and terminal keyboard options
 * - SET_LOCK_LEDS: Setting Caps/Num/Scroll Lock LED states
 *
 * Operational State:
 * - INITIALISED: Normal scan code processing and LED management
 *
 * Protocol Features Handled:
 * - Automatic retry logic for failed initialisation steps
 * - Keyboard ID detection for terminal keyboards (Set 3 scan codes)
 * - Make/break code configuration for advanced keyboards
 * - LED state synchronisation with host lock key status
 * - Error recovery and state machine reset on protocol violations
 *
 * Data Flow:
 * - Initialisation responses trigger state transitions
 * - Scan codes queued to ring buffer for HID processing
 * - LED commands issued when host lock state changes
 * - Status updates for converter LED indicators
 *
 * @param data_byte 8-bit response/scan code received from keyboard
 *
 * @note IRQ-safe.
 */
static void keyboard_event_processor(uint8_t data_byte) {
    switch (keyboard_state) {
        case UNINITIALISED:
            id_retry = false;  // Clear retry flag - starting fresh initialisation sequence
            __dmb();  // Memory barrier - ensure volatile write completes before state change
            keyboard_id = ATPS2_KEYBOARD_ID_UNKNOWN;  // Reset keyboard ID to unknown state during
                                                      // initialisation
            switch (data_byte) {
                case ATPS2_RESP_BAT_PASSED:
                    // Power-on self-test completed successfully (Basic Assurance Test)
                    // Keyboard passed internal diagnostics and is ready
                    LOG_DEBUG("Keyboard Self Test OK!\n");
                    keyboard_lock_leds = 0;  // LINT:ALLOW barrier - volatile provides atomic
                                             // visibility on Cortex-M0+
                    LOG_DEBUG("Waiting for Keyboard ID...\n");
                    keyboard_state = INIT_READ_ID_1;
                    __dmb();  // Memory barrier - ensure state write visible to main loop
                    break;
                default:
                    // Unexpected response during initialisation - could be failed BAT (0xFC)
                    // or other power-on state requiring explicit reset command
                    LOG_DEBUG("Asking Keyboard to Reset\n");
                    keyboard_state = INIT_AWAIT_ACK;
                    __dmb();  // Memory barrier - ensure state write visible to main loop
                    keyboard_command_handler(ATPS2_CMD_RESET);
            }
            break;
        case INIT_AWAIT_ACK:
            switch (data_byte) {
                case ATPS2_RESP_ACK:
                    LOG_DEBUG("ACK Received after Reset\n");
                    keyboard_state = INIT_AWAIT_SELFTEST;
                    __dmb();  // Memory barrier - ensure state write visible to main loop
                    break;
                default:
                    LOG_DEBUG("Unknown ACK Response (0x%02X).  Asking again to Reset...\n",
                              data_byte);
                    keyboard_command_handler(ATPS2_CMD_RESET);
            }
            break;
        case INIT_AWAIT_SELFTEST:
            switch (data_byte) {
                case ATPS2_RESP_BAT_PASSED:
                    LOG_DEBUG("Keyboard Self Test OK!\n");
                    keyboard_lock_leds = 0;  // LINT:ALLOW barrier - volatile provides atomic
                                             // visibility on Cortex-M0+
                    // Proceed to keyboard identification phase for terminal keyboard detection
                    LOG_DEBUG("Waiting for Keyboard ID...\n");
                    keyboard_state = INIT_READ_ID_1;
                    __dmb();  // Memory barrier - ensure state write visible to main loop
                    break;
                default:
                    LOG_DEBUG("Self-Test invalid response (0x%02X).  Asking again to Reset...\n",
                              data_byte);
                    keyboard_state = INIT_AWAIT_ACK;
                    __dmb();  // Memory barrier - ensure state write visible to main loop
                    keyboard_command_handler(ATPS2_CMD_RESET);
            }
            break;

        // Terminal keyboards respond with 2-byte ID after 0xF2 command
        // Timeout handling (500ms) managed by main task function if no response received
        case INIT_READ_ID_1:
            switch (data_byte) {
                case ATPS2_RESP_ACK:
                    LOG_DEBUG("ACK Keyboard ID Request\n");
                    LOG_DEBUG("Waiting for Keyboard ID...\n");
                    break;
                default:
                    LOG_DEBUG("Keyboard First ID Byte read as 0x%02X\n", data_byte);
                    keyboard_id &= ATPS2_KEYBOARD_ID_LOW_MASK;
                    keyboard_id |= (uint16_t)data_byte << 8;
                    keyboard_state = INIT_READ_ID_2;
                    __dmb();  // Memory barrier - ensure state write visible to main loop
            }
            break;
        case INIT_READ_ID_2:
            LOG_DEBUG("Keyboard Second ID Byte read as 0x%02X\n", data_byte);
            keyboard_id &= ATPS2_KEYBOARD_ID_HIGH_MASK;
            keyboard_id |= (uint16_t)data_byte;
            LOG_DEBUG("Keyboard ID: 0x%04X\n", keyboard_id);

#if USING_SET123
            // Using unified processor - auto-detect scancode set from keyboard ID
            g_scancode_config = scancode_config_from_keyboard_id(keyboard_id);
            reset_scancode_state();  // Reset state machine when switching configs
            LOG_INFO("Auto-detected Scancode Set: %s\n",
                     g_scancode_config == &SCANCODE_CONFIG_SET1   ? "Set 1"
                     : g_scancode_config == &SCANCODE_CONFIG_SET2 ? "Set 2"
                                                                  : "Set 3");

            // Configure terminal keyboards for proper operation
            if (g_scancode_config == &SCANCODE_CONFIG_SET3) {
                // Terminal keyboards (Set 3) require explicit make/break mode configuration
                // Command enables both key press and release events for all keys
                LOG_DEBUG("Setting all Keys to Make/Break\n");
                keyboard_command_handler(ATPS2_KEYBOARD_CMD_SET_ALL_MAKEBREAK);
                keyboard_state = INIT_SETUP;
                __dmb();  // Memory barrier - ensure state write visible to main loop
            } else {
                LOG_DEBUG("Keyboard Initialised!\n");
                keyboard_state = INITIALISED;
                __dmb();  // Memory barrier - ensure state write visible to main loop
            }
#else
            // Using legacy per-set file - check compile-time CODESET
            // Configure terminal keyboards for proper operation
            if (CODESET_3) {
                // Terminal keyboards (Set 3) require explicit make/break mode configuration
                // Command enables both key press and release events for all keys
                LOG_DEBUG("Setting all Keys to Make/Break\n");
                keyboard_command_handler(ATPS2_KEYBOARD_CMD_SET_ALL_MAKEBREAK);
                keyboard_state = INIT_SETUP;
                __dmb();  // Memory barrier - ensure state write visible to main loop
            } else {
                LOG_DEBUG("Keyboard Initialised!\n");
                keyboard_state = INITIALISED;
                __dmb();  // Memory barrier - ensure state write visible to main loop
            }
#endif
            break;
        case INIT_SETUP:
            // Process acknowledgment for make/break configuration command
            switch (data_byte) {
                case ATPS2_RESP_ACK:
                    LOG_DEBUG("Keyboard Initialised!\n");
                    keyboard_state = INITIALISED;
                    __dmb();  // Memory barrier - ensure state write visible to main loop
                    break;
                default:
                    LOG_DEBUG("Unknown Response (0x%02X).  Continuing...\n", data_byte);
                    keyboard_id = ATPS2_KEYBOARD_ID_UNKNOWN;
                    LOG_DEBUG("Keyboard Initialised!\n");
                    keyboard_state = INITIALISED;
                    __dmb();  // Memory barrier - ensure state write visible to main loop
            }
            break;

        // Handle LED state synchronisation with host lock key status
        case SET_LOCK_LEDS:
            // Process LED command sequence responses (0xED command followed by LED bitmap)
            switch (data_byte) {
                case ATPS2_RESP_ACK:
                    if (lock_leds.value != keyboard_lock_leds) {
                        keyboard_lock_leds =
                            lock_leds.value;  // LINT:ALLOW barrier - volatile provides atomic
                                              // visibility on Cortex-M0+
                        __dmb();              // Ensure IRQ write visible before main-loop read
                        keyboard_command_handler((uint8_t)((lock_leds.keys.capsLock << 2) |
                                                           (lock_leds.keys.numLock << 1) |
                                                           lock_leds.keys.scrollLock));
                    } else {
                        // LED command sequence completed successfully - return to normal operation
                        keyboard_state = INITIALISED;
                        __dmb();  // Memory barrier - ensure state write visible to main loop
                    }
                    break;
                default:
                    LOG_DEBUG("SET_LOCK_LED FAILED (0x%02X)\n", data_byte);
                    keyboard_lock_leds = lock_leds.value;  // LINT:ALLOW barrier - volatile provides
                                                           // atomic visibility on Cortex-M0+
                    keyboard_state = INITIALISED;
                    __dmb();  // Memory barrier - ensure state write visible to main loop
            }
            break;

        // Normal operation: queue scan codes for HID processing
        case INITIALISED:
            // Always queue to ring buffer - processing happens in main task loop
            // This ensures HID reports are sent from the correct context
            if (!ringbuf_is_full()) {
                ringbuf_put(data_byte);
                isr_push_flow_token(__func__, data_byte);
            }
    }
    update_keyboard_ready_led(keyboard_state == INITIALISED);
}

/**
 * @brief AT/PS2 Keyboard IRQ Event Handler
 *
 * Interrupt service routine that processes raw data received from the AT/PS2 keyboard
 * via the PIO state machine. Implements complete frame validation and error recovery:
 *
 * Frame Extraction:
 * - Reads 11-bit frame from PIO RX FIFO (Start + 8 Data + Parity + Stop)
 * - Extracts individual components: start bit, 8 data bits, parity bit, stop bit
 * - Reconstructs original 8-bit data byte from received frame
 *
 * Protocol Validation:
 * - Start bit validation: Must be 0 (space condition)
 * - Parity validation: Calculated vs received odd parity bit
 * - Stop bit detection: Identifies keyboard compliance (high vs low)
 * - Frame integrity checks prevent processing corrupted data
 *
 * Error Recovery:
 * - Invalid start bit: Triggers state machine reset and restart
 * - Parity mismatch: Issues resend command (0xFE) to keyboard
 * - Special case handling for power-on connection artifacts
 * - Automatic protocol restart on persistent errors
 *
 * Stop Bit Compatibility:
 * - Dynamic detection of stop bit polarity (standard vs Z-150 style)
 * - Adapts to non-compliant keyboards automatically
 * - Logs stop bit state changes for diagnostic purposes
 *
 * Data Flow:
 * - Valid frames passed to keyboard event processor
 * - Invalid frames rejected with appropriate error logging
 * - State machine restarts maintain protocol synchronisation
 *
 * Performance Considerations:
 * - Executes in interrupt context - minimal processing time
 * - Direct FIFO access for optimal throughput
 * - Efficient bit manipulation for frame parsing
 *
 * @note IRQ-safe.
 */
static void __isr keyboard_input_event_handler(void) {
    // Guard against spurious dispatch from shared IRQ - only process if our FIFO has data
    if (pio_sm_is_rx_fifo_empty(pio_engine.pio, pio_engine.sm)) {
        return;
    }

    // Parse AT/PS2 frame components from 11-bit PIO data
    atps2_frame_t frame = atps2_parse_frame(&pio_engine);

    // Detect and adapt to non-standard stop bit behaviour (Z-150 compatibility)
    if (stop_bit_state != (frame.stop_bit ? STOP_BIT_HIGH : STOP_BIT_LOW)) {
        stop_bit_state = frame.stop_bit ? STOP_BIT_HIGH : STOP_BIT_LOW;
        LOG_DEBUG("Stop Bit %s Detected\n", frame.stop_bit ? "High" : "Low");
    }

    if (frame.start_bit != 0 || frame.parity_bit != frame.expected_parity) {
        if (frame.start_bit != 0) {
            LOG_ERROR("Start Bit Validation Failed: start_bit=%i\n", frame.start_bit);
        }
        if (frame.parity_bit != frame.expected_parity) {
            LOG_ERROR("Parity Bit Validation Failed: expected=%i, actual=%i\n",
                      frame.expected_parity, frame.parity_bit);
            if (frame.data == ATPS2_RESP_BAT_PIO_CONNECT && frame.parity_bit == 1) {
                // Power-on connection artifact: keyboard briefly pulls lines low during startup
                // This causes bit shift in FIFO changing 0xAA to 0x54 with invalid parity
                // Historical fix preserved for reference - indicates keyboard connection event
                LOG_DEBUG("Likely Keyboard Connect Event detected.\n");
                keyboard_state = UNINITIALISED;
                __dmb();  // Memory barrier - ensure state write visible to main loop
                id_retry = false;
                __dmb();  // Memory barrier - ensure volatile write completes
                pio_restart(pio_engine.pio, pio_engine.sm, pio_engine.offset);
            }
            // Request data retransmission on parity error
            keyboard_command_handler(ATPS2_CMD_RESEND);
            return;  // Abort processing - wait for retransmitted data
        }
        // Frame validation failed - restart protocol state machine for clean recovery
        keyboard_state = UNINITIALISED;
        __dmb();  // Memory barrier - ensure state write visible to main loop
        id_retry = false;
        __dmb();  // Memory barrier - ensure volatile write completes
        pio_restart(pio_engine.pio, pio_engine.sm, pio_engine.offset);
        return;
    }

    keyboard_event_processor(frame.data);
}

/**
 * @brief Handle Initialisation Timeout and Keyboard Detection
 *
 * Processes one detection-interval tick when the keyboard is not yet in the INITIALISED
 * state.  Checks hardware presence via the CLOCK line, increments the stall counter, and
 * drives the keyboard_state machine through timeout and recovery paths.
 *
 * Extracted from keyboard_interface_task to reduce cognitive complexity.
 *
 * @note Main loop only.
 */
static void handle_init_detection(void) {
    if (gpio_get(keyboard_data_pin + 1) == 1) {
        // Monitor initialisation progress when keyboard detected (CLOCK line high)
        detect_stall_count++;  // Increment timeout counter for initialisation state
        switch (keyboard_state) {
            case INIT_READ_ID_1:
            case INIT_READ_ID_2:
            case INIT_SETUP:
                if (detect_stall_count > ATPS2_ID_RETRY_LIMIT) {
                    // Initialisation timeout detected during ID read or setup phase
                    __dmb();  // Memory barrier - ensure we see latest volatile value
                    if (!id_retry) {
                        // First timeout - retry keyboard ID request before giving up
                        LOG_DEBUG("Keyboard ID/Setup Timeout, retrying...\n");
                        id_retry = true;
                        __dmb();  // Memory barrier - ensure volatile write visible to IRQ
                        keyboard_state = INIT_READ_ID_1;  // Return to ID read state for retry
                        keyboard_command_handler(ATPS2_CMD_GET_ID);  // Send keyboard ID request
                        detect_stall_count = 0;  // Reset timeout counter for retry attempt
                    } else {
                        LOG_DEBUG(
                            "Keyboard Read ID/Setup Timed out again, continuing with "
                            "defaults.\n");
                        keyboard_id = ATPS2_KEYBOARD_ID_UNKNOWN;
#if USING_SET123
                        // Timeout - use default configuration for keyboards that don't respond to
                        // ID command
                        g_scancode_config = scancode_config_from_keyboard_id(keyboard_id);
                        reset_scancode_state();  // Reset state machine when switching configs
                        LOG_INFO("No ID response - defaulting to Scancode Set: %s\n",
                                 g_scancode_config == &SCANCODE_CONFIG_SET1   ? "Set 1"
                                 : g_scancode_config == &SCANCODE_CONFIG_SET2 ? "Set 2"
                                                                              : "Set 3");
#endif
                        LOG_DEBUG("Keyboard Initialised!\n");
                        keyboard_state     = INITIALISED;
                        detect_stall_count = 0;
                    }
                }
                break;
            case SET_LOCK_LEDS:
                if (detect_stall_count > ATPS2_ID_RETRY_LIMIT) {
                    // LED command timeout - continue without LED synchronisation
                    LOG_DEBUG("Timeout whilst setting keyboard lock LEDs, continuing.\n");
                    keyboard_state     = INITIALISED;
                    detect_stall_count = 0;
                }
                break;
            default:
                if (detect_stall_count < ATPS2_DETECT_MAX_ATTEMPTS) {
                    LOG_DEBUG("Keyboard detected, awaiting ACK (%i/5 attempts)\n",
                              detect_stall_count);
                } else {
                    LOG_DEBUG("Keyboard detected, but no ACK received!\n");
                    LOG_DEBUG("Requesting keyboard reset\n");
                    keyboard_state     = INIT_AWAIT_ACK;
                    detect_stall_count = 0;
                    keyboard_command_handler(ATPS2_CMD_RESET);
                }
                break;
        }
    } else if (keyboard_state == UNINITIALISED) {
        LOG_DEBUG("Awaiting keyboard detection. Please ensure a keyboard is connected.\n");
        detect_stall_count = 0;  // Clear timeout counter whilst waiting for keyboard connection
    }
    update_keyboard_ready_led(keyboard_state == INITIALISED);
}

// --- Public Functions ---

void keyboard_interface_task(void) {
    // Guard against uninitialised PIO engine (setup may have failed due to resource exhaustion)
    if (pio_engine.pio == NULL) {
        return;
    }

    __dmb();  // Memory barrier - ensure we see latest state from IRQ
    if (keyboard_state == INITIALISED) {
        // Normal operation: process scan codes and manage LED synchronisation
        detect_stall_count = 0;  // Clear timeout counter - keyboard is operational
        if (lock_leds.value != keyboard_lock_leds) {
            keyboard_state = SET_LOCK_LEDS;
            __dmb();  // Memory barrier - ensure state write visible to IRQ
            keyboard_command_handler(ATPS2_KEYBOARD_CMD_SET_LEDS);
        } else {
            if (!ringbuf_is_empty() && tud_hid_ready()) {
                // Process scan codes only when HID interface ready to prevent report queue overflow
                uint8_t scancode = ringbuf_get();  // Retrieve next scan code from buffer

                // Begin flow tracking for this scancode byte
                flow_token_t flow_tok;
                if (main_pop_flow_token(&flow_tok)) {
                    flow_start(&flow_tok);
                }

#if USING_SET123
                process_scancode(scancode, g_scancode_config);
#else
                process_scancode(scancode);
#endif
            }
        }
    } else {
        // Initialisation timeout management and keyboard detection logic
        if (board_millis() - detect_ms > ATPS2_INIT_CHECK_MS) {
            detect_ms = board_millis();
            handle_init_detection();
        }
    }
}

void keyboard_interface_setup(uint data_pin) {
#ifdef CONVERTER_LEDS
    converter.state.kb_ready = 0;
    update_converter_status();  // Initialise converter status LEDs to "not ready" state
#endif

    // Initialise ring buffer for key data communication between IRQ and main task
    ringbuf_reset();  // LINT:ALLOW ringbuf_reset - Safe: IRQs not yet enabled during init

    if (!atps2_setup_pio_engine(&pio_engine, data_pin, &keyboard_input_event_handler, "Keyboard")) {
        return;
    }

    keyboard_data_pin = data_pin;
}