/*
 * This file is part of RP2040 Keyboard Converter.
 *
 * Copyright 2023 Paul Bramhall (paulwamp@gmail.com)
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
 * Initialization Sequence:
 * 1. Host sends Model Number command (0x16)
 * 2. Keyboard responds with model ID and resets itself
 * 3. Host begins normal operation with Inquiry commands
 * 4. Inquiry commands sent every 250ms during normal operation
 * 
 * Supported Models:
 * - M0110: Original compact keyboard (1984)
 * - M0110A: Enhanced version with arrow keys and numeric keypad (1987)
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
#define M0110_CMD_INQUIRY    0x10  /**< Inquiry command - requests key transitions from keyboard */
#define M0110_CMD_INSTANT    0x14  /**< Instant command - requests immediate key state (not commonly used) */
#define M0110_CMD_MODEL      0x16  /**< Model Number command - requests keyboard model and triggers reset */

/**
 * @brief Apple M0110 Response Codes
 * 
 * These are the response codes sent by the keyboard back to the host.
 * All responses are 8-bit values transmitted MSB-first.
 */
#define M0110_RESP_NULL         0x7B  /**< Null response - no key pressed or released (0111 1011) */
#define M0110_RESP_KEYPAD       0x79  /**< Keypad attached response - indicates external keypad present (0111 1001) */

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
#define M0110_RESP_MODEL_M0110  0x03  /**< Model M0110(GS536) - original compact keyboard (0000 0011) */
#define M0110_RESP_MODEL_M0110_ALT 0x09  /**< Model M0110(GS624) - original compact keyboard variant (0000 1001) */
#define M0110_RESP_MODEL_M0110A 0x0B  /**< Model M0110A - enhanced keyboard with arrow keys (0000 1011) */
#define M0110_RESP_MODEL_M0120  0x11  /**< Model M0120 - numeric keypad (0001 0001) */
#define M0110_RESP_MODEL_M0110_M0120 0x13  /**< M0110(GS536) + M0120 keypad (0001 0011) */
#define M0110_RESP_MODEL_M0110_M0120_ALT 0x19  /**< M0110(GS624) + M0120 keypad (0001 1001) */
#define M0110_RESP_MODEL_M0110A_M0120 0x1B  /**< M0110A + M0120 keypad (0001 1011) */

/**
 * @brief Protocol Timing Constants
 * 
 * These timing values are used for PIO clock divider calculation and protocol operation.
 */
#define M0110_TIMING_KEYBOARD_LOW_US      160   /**< Keyboard→Host: Clock low period (used for timing calculation) */

/**
 * @brief Protocol Operation Intervals (in milliseconds)
 */
#define M0110_RESPONSE_TIMEOUT_MS         500   /**< Maximum time to wait for keyboard response (1/2 second) */
#define M0110_MODEL_RETRY_INTERVAL_MS     500   /**< Model command retry interval */
#define M0110_INITIALIZATION_DELAY_MS     1000  /**< Initial delay before first model command */

/**
 * @brief Initializes the Apple M0110 keyboard interface.
 * 
 * @param data_pin The GPIO pin connected to the DATA line
 */
void keyboard_interface_setup(uint data_pin);

/**
 * @brief Main task function for Apple M0110 keyboard interface.
 * Should be called periodically from the main loop.
 */
void keyboard_interface_task();

#endif /* KEYBOARD_INTERFACE_H */