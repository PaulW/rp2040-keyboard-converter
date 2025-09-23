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

#ifndef KEYBOARD_INTERFACE_H
#define KEYBOARD_INTERFACE_H

#include "pico/stdlib.h"

/**
 * @brief Apple M0110 Protocol Commands
 */
#define M0110_CMD_INQUIRY    0x10  // Inquiry command - ask for key transitions
#define M0110_CMD_INSTANT    0x14  // Instant mode - immediate key state
#define M0110_CMD_MODEL      0x16  // Model number request
#define M0110_CMD_TEST       0x36  // Self test

/**
 * @brief Apple M0110 Response Codes
 */
#define M0110_RESP_NULL      0x7B  // Null response (no key pressed) - bit pattern: 0111 1011
#define M0110_RESP_KEYPAD    0x79  // Keypad response - bit pattern: 0111 1001
#define M0110_RESP_MODEL_M0110    0x0B  // Model M0110 - bit pattern: 0000 1011  
#define M0110_RESP_MODEL_M0110A   0x05  // Model M0110A - bit pattern: 0000 0101

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