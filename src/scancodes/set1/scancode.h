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
 * @brief XT Scancode Set 1 processing interface.
 *
 * @see keyboard_interface.h for the XT protocol specification and timing details.
 */

#ifndef SCANCODES_H
#define SCANCODES_H

#include <stdint.h>

/**
 * @brief Process Keyboard Input (Scancode Set 1) Data
 *
 * This function processes scan codes from XT and AT keyboards using Scan Code Set 1.
 * Set 1 is the original IBM PC/XT scan code set, still widely used for compatibility.
 *
 * Protocol Overview:
 * - Make codes: 0x01-0x53 (key press)
 * - Break codes: Make code + 0x80 (key release, e.g., 0x81 = key 0x01 released)
 * - Multi-byte sequences: E0-prefixed (2 bytes) and E1-prefixed (3-6 bytes)
 *
 * State Machine:
 *
 *     INIT ──[E0]──> E0 ──[code]──> INIT (process E0-prefixed code)
 *       │            │
 *       │            └──[2A,AA,36,B6]──> INIT (ignore fake shifts)
 *       │
 *       ├──[E1]──> E1 ──[1D]──> E1_1D ──[45]──> INIT (Pause press)
 *       │           │
 *       │           └──[9D]──> E1_9D ──[C5]──> INIT (Pause release)
 *       │
 *       └──[code]──> INIT (process normal code)
 *
 * Multi-byte Sequence Examples:
 * - Print Screen: E0 37 (make), E0 B7 (break)
 * - Pause/Break:  E1 1D 45 (make), E1 9D C5 (break)
 * - Right Ctrl:   E0 1D (make), E0 9D (break)
 * - Keypad Enter: E0 1C (make), E0 9C (break)
 *
 * Special Handling:
 * - E0 2A, E0 AA, E0 36, E0 B6: Fake shifts (ignored, some keyboards send these)
 * - E1 sequences: Only used for Pause/Break key
 * - Codes >= 0x80: Break codes (key release)
 * - Self-test codes (0xAA) are handled by protocol layer during initialisation
 *
 * @param code The keycode to process.
 *
 * @note Main loop only.
 * @note handle_keyboard_report() translates scan codes to HID keycodes via keymap lookup
 */
void process_scancode(uint8_t code);

#endif /* SCANCODES_H */
