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

#ifndef SCANCODE_CONFIG_H
#define SCANCODE_CONFIG_H

#include "scancode.h"

/**
 * @file scancode_config.h
 * @brief Compile-Time Configuration for XT Protocol
 * 
 * This header provides compile-time scancode configuration for XT protocol.
 * XT keyboards always use Scancode Set 1, so this header simply provides
 * a convenience wrapper that hardcodes Set 1 configuration.
 * 
 * Usage (XT Protocol Only):
 * -------------------------
 * ```c
 * #include "scancode_config.h"
 * 
 * void keyboard_interface_task(void) {
 *     uint8_t code;
 *     if (get_scancode_from_buffer(&code)) {
 *         process_scancode_ct(code);  // Compile-time Set 1
 *     }
 * }
 * ```
 * 
 * Benefits:
 * ---------
 * ✅ Zero runtime overhead (Set 1 hardcoded)
 * ✅ No CMake configuration needed
 * ✅ Self-contained header
 * ✅ Simple and explicit
 * 
 * @note For AT/PS2 protocol, use scancode_runtime.h instead (supports Set 2/3 detection)
 * @see scancode.h for the unified processor API
 * @see scancode_runtime.h for AT/PS2 runtime detection
 */

/**
 * @brief Compile-Time Wrapper for XT Protocol (Set 1)
 * 
 * This macro wraps process_scancode() with hardcoded Set 1 configuration.
 * XT keyboards always use Set 1, so no runtime detection is needed.
 * 
 * @param code The scancode byte to process
 */
#define process_scancode_ct(code) process_scancode((code), &SCANCODE_CONFIG_SET1)

#endif // SCANCODE_CONFIG_H
