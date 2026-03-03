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
 * @file scancode.h
 * @brief Apple M0110/M0110A keyboard scancode processing interface.
 *
 * @note This implementation is currently under development and considered incomplete.
 *       The current production configuration for Apple M0110A uses CODESET=set1 instead.
 * @see keyboard_interface.h for the full M0110 protocol specification.
 */

#ifndef APPLE_M0110_SCANCODE_H
#define APPLE_M0110_SCANCODE_H

#include <stdint.h>

/**
 * @brief Process Apple M0110 Keyboard Scancode Data
 *
 * The Apple M0110 protocol encoding:
 * - Bit 7: 1 = key up, 0 = key down
 * - Bit 0: Always 1 (for key transitions)
 * - Bits 6-1: Key code
 *
 * M0110A Extended Sequences:
 * - Normal keys: Single byte
 * - Extended keys: 0x79 followed by code (two bytes) → code_79_translation[]
 * - Special extended: 0x71, 0x79, then code (three bytes) → code_71_translation[]
 *
 * @param code The scancode to process
 * @note Main loop only; non-blocking.
 */
void process_scancode(uint8_t code);

#endif /* APPLE_M0110_SCANCODE_H */