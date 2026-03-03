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
 * @file keyboard_interface.h
 * @brief Apple M0110 Keyboard Protocol Implementation
 *
 * This file implements the Apple M0110 keyboard protocol used by early Macintosh computers.
 * The protocol is a synchronous, bidirectional communication system where the keyboard
 * controls the clock signal and the host (Mac) sends commands to request key data.
 *
 * Protocol Characteristics:
 * - 2-wire interface: DATA and CLOCK (both open-drain with pull-ups)
 * - Keyboard controls clock timing
 * - MSB-first bit transmission
 * - No parity or stop bits
 * - Keyboard-initiated transmission for key events
 * - Host-initiated transmission for commands
 *
 * Timing Requirements:
 * - KEYBOARD→HOST: 330µs cycles (160µs low, 170µs high) ≈ 3.03kHz
 * - HOST→KEYBOARD: 400µs cycles (180µs low, 220µs high) ≈ 2.5kHz
 * - Request-to-send: 840µs DATA low period
 * - Data hold time: 80µs after final clock edge
 *
 * Initialisation Sequence:
 * 1. Host sends Model Number command (0x16)
 * 2. Keyboard responds with model ID and resets itself
 * 3. Host begins continuous polling with Inquiry commands
 *
 * Supported Models:
 * - M0110: Original compact keyboard
 * - M0110A: Enhanced version with arrow keys and numeric keypad
 */

#ifndef KEYBOARD_INTERFACE_H
#define KEYBOARD_INTERFACE_H

#include "pico/stdlib.h"

/**
 * @brief Apple M0110 Protocol Command Codes
 *
 * These commands are sent by the host (Mac) to the keyboard to request
 * various operations and information.
 */
#define M0110_CMD_INQUIRY 0x10 /**< Inquiry command - requests key transitions from keyboard */
#define M0110_CMD_INSTANT 0x14 /**< Instant command - requests immediate key state */
#define M0110_CMD_MODEL   0x16 /**< Get Number command - requests keyboard model, triggers reset */

/**
 * @brief Apple M0110 Response Codes
 *
 * These are the response codes sent by the keyboard back to the host.
 * All responses are 8-bit values transmitted MSB-first.
 */
#define M0110_RESP_NULL   0x7B /**< Null response - no key pressed or released (0111 1011) */
#define M0110_RESP_KEYPAD 0x79 /**< Keypad attached - external keypad present (0111 1001) */

/**
 * @brief Apple M0110 Model Identification Codes
 *
 * Model response format (based on observed values):
 * - Bit 0: Always 1
 * - Bits 1-3: Keyboard model number (1-8)
 * - Bit 4: 1 if another device connected (keypad detection)
 * - Bits 5-6: Next device model number when bit 4 set (keypad model)
 * - Bit 7: Always 0 (reserved/unused)
 */
#define M0110_RESP_MODEL_M0110           0x03 /**< Model M0110(GS536) - (0000 0011) */
#define M0110_RESP_MODEL_M0110_ALT       0x09 /**< Model M0110(GS624) - (0000 1001) */
#define M0110_RESP_MODEL_M0110A          0x0B /**< Model M0110A - (0000 1011) */
#define M0110_RESP_MODEL_M0120           0x11 /**< Model M0120 - (0001 0001) */
#define M0110_RESP_MODEL_M0110_M0120     0x13 /**< M0110(GS536) + M0120 keypad - (0001 0011) */
#define M0110_RESP_MODEL_M0110_M0120_ALT 0x19 /**< M0110(GS624) + M0120 keypad - (0001 1001) */
#define M0110_RESP_MODEL_M0110A_M0120    0x1B /**< M0110A + M0120 keypad - (0001 1011) */

/**
 * @brief Protocol Timing Constants
 *
 * These timing values are used for PIO clock divider calculation and protocol operation.
 */
#define M0110_TIMING_KEYBOARD_LOW_US 160 /**< Keyboard→Host: Clock low period duration */

/**
 * @brief Protocol Operation Intervals (in milliseconds)
 */
#define M0110_RESPONSE_TIMEOUT_MS     500 /**< Max time to wait for keyboard response (1/2 second) */
#define M0110_MODEL_RETRY_INTERVAL_MS 500  /**< Model command retry interval */
#define M0110_INITIALISATION_DELAY_MS 1000 /**< Initial delay before first model command */

/**
 * @brief Initialises the Apple M0110 keyboard interface hardware and software
 *
 * This function configures the RP2040 PIO system to handle the Apple M0110 protocol:
 *
 * Hardware Configuration:
 * - Allocates and configures PIO state machine for M0110 timing
 * - Sets up GPIO pins with appropriate pull-ups for open-drain signaling
 * - Configures interrupt handling for received data
 *
 * Protocol Setup:
 * - Calculates timing dividers for accurate bit-level timing
 * - Initialises PIO program for MSB-first transmission/reception
 * - Sets up FIFO buffers for asynchronous data handling
 *
 * Pin Assignment:
 * - data_pin: Connected to keyboard DATA line
 * - data_pin + 1: Connected to keyboard CLOCK line (keyboard controlled)
 *
 * Timing Calculation:
 * - Base timing uses 160µs as minimum reliable detection period
 * - PIO clock divider ensures proper sampling of keyboard signals
 * - Supports both keyboard TX (330µs cycles) and host TX (400µs cycles)
 *
 * Error Handling:
 * - Validates PIO resource availability
 * - Graceful degradation if hardware resources unavailable
 * - Comprehensive logging for troubleshooting
 *
 * @param data_pin GPIO pin number for DATA line (CLOCK will be data_pin + 1)
 *
 * @note Call this function once during system initialisation
 * @note Requires keyboard_interface_task() to be called regularly for operation
 * @note Non-blocking.
 * @note Main loop only.
 */
void keyboard_interface_setup(uint data_pin);

/**
 * @brief Main task function for Apple M0110 keyboard protocol management
 *
 * This function implements the main state machine for M0110 protocol operation.
 * It should be called regularly from the main application loop (typically every few ms).
 *
 * State Machine Operation:
 *
 * 1. UNINITIALISED (Startup Phase):
 *    - Waits for initial 1000ms delay to allow keyboard power-up
 *    - Uses command timing to track startup delay
 *    - Transitions to model identification phase when delay expires
 *
 * 2. INIT_MODEL_REQUEST (Keyboard Detection):
 *    - Sends Model Number commands (0x16) every 500ms using command timing
 *    - Retries up to 5 times if no response received
 *    - Model command causes keyboard to reset and identify itself
 *    - Transitions to INITIALISED phase on successful response (handled by IRQ)
 *
 * 3. INITIALISED (Normal Operation):
 *    - Monitors keyboard responses using response timing for 500ms timeout
 *    - Processes buffered key data when USB HID interface ready
 *    - Timeout detection triggers return to UNINITIALISED for recovery
 *    - Key data processing occurs via ring buffer from interrupt handler
 *
 * Timing and Performance:
 * - Optimised timing calculations with single elapsed time computation
 * - Timer wraparound protection prevents false timeouts on system rollover
 *
 * Error Recovery:
 * - Timeout detection triggers full protocol restart
 * - State machine reset returns to UNINITIALISED phase
 * - Maximum retry limits prevent infinite retry loops
 *
 * @note This function manages protocol timing and state transitions
 * @note Actual data reception occurs in interrupt context
 * @note Timer calculations are optimised to handle uint32_t wraparound scenarios
 * @note Non-blocking.
 * @note Main loop only.
 */
void keyboard_interface_task(void);

#endif /* KEYBOARD_INTERFACE_H */