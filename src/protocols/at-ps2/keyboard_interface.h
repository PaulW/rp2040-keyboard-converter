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
 * @brief AT/PS2 Keyboard Protocol Implementation
 * 
 * This file implements the AT and PS/2 keyboard protocols used by IBM PC compatible systems.
 * The protocol is a synchronous, bidirectional communication system where the host can send
 * commands and the keyboard responds with data and acknowledgments.
 * 
 * Protocol Characteristics:
 * - 2-wire interface: DATA and CLOCK (both open-drain with pull-ups)
 * - Keyboard normally controls clock timing
 * - Host can inhibit communication by pulling CLOCK low
 * - LSB-first bit transmission (bit 0 → bit 7)
 * - Start bit (0), 8 data bits, odd parity bit, stop bit (1)
 * - Bidirectional communication with acknowledgments
 * 
 * Supported Scan Code Sets:
 * - Set 1: Original XT scan codes (default for AT keyboards)
 * - Set 2: Standard PS/2 scan codes (default for PS/2 keyboards)
 * - Set 3: Terminal keyboard scan codes (less common)
 * 
 * Timing Requirements:
 * - Clock frequency: 10-16.7 kHz
 * - Clock pulse width: minimum 30µs
 * - Data setup/hold: minimum 5µs before/after clock edge
 * - Inhibit time: minimum 100µs CLOCK low to stop transmission
 * 
 * Initialization Sequence:
 * 1. Power-on self-test (keyboard sends 0xAA on success)
 * 2. Host reads keyboard ID (2-byte response)
 * 3. Host configures scan code set and options
 * 4. Host sets LED states
 * 5. Normal operation begins
 * 
 * Special Features:
 * - LED control (Caps Lock, Num Lock, Scroll Lock)
 * - Typematic rate and delay configuration
 * - Multiple scan code set support
 * - Keyboard identification and capability detection
 * - Compatible with both AT and PS/2 electrical interfaces
 */

#ifndef KEYBOARD_INTERFACE_H
#define KEYBOARD_INTERFACE_H

#include "pico/stdlib.h"

/**
 * @brief AT/PS2 Protocol Command Codes
 * 
 * These commands are sent by the host to the keyboard to control
 * various operations and configurations.
 */
#define ATPS2_KEYBOARD_CMD_SET_LEDS          0xED  /**< Set LED states command (followed by LED bitmap) */
#define ATPS2_KEYBOARD_CMD_SET_ALL_MAKEBREAK 0xF8  /**< Set all keys to make/break (Set 3 only) */

/**
 * @brief Common AT/PS2 Keyboard ID Codes
 * 
 * These are typical 2-byte identification codes returned by keyboards
 * in response to the Get ID (0xF2) command.
 */
#define ATPS2_KEYBOARD_ID_LOW_MASK        0x00FF  /**< Mask for low byte of keyboard ID */
#define ATPS2_KEYBOARD_ID_HIGH_MASK       0xFF00  /**< Mask for high byte of keyboard ID */
#define ATPS2_KEYBOARD_ID_UNKNOWN            0xFFFF  /**< Unknown or unrecognized keyboard ID */

/**
 * @brief Initializes the AT/PS2 keyboard interface
 * 
 * Sets up PIO state machine, GPIO configuration, and protocol handlers
 * for AT/PS2 keyboard communication.
 * 
 * @param data_pin GPIO pin for DATA line (CLOCK will be data_pin + 1)
 */
void keyboard_interface_setup(uint data_pin);

/**
 * @brief Main task function for AT/PS2 keyboard interface
 * 
 * Manages protocol state machine, processes received data, handles
 * initialization sequence, and maintains keyboard communication.
 * Should be called periodically from main loop.
 */
void keyboard_interface_task();

#endif /* KEYBOARD_INTERFACE_H */
