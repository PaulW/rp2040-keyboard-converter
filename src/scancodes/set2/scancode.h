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
 * @brief AT/PS2 Scancode Set 2 processing interface.
 *
 * @see keyboard_interface.h for the AT/PS2 protocol specification and timing details.
 */

#ifndef SCANCODES_H
#define SCANCODES_H

#include <stdint.h>

/**
 * @brief Process Keyboard Input (Scancode Set 2) Data
 *
 * This function processes scan codes from AT/PS2 keyboards using Scan Code Set 2.
 * Set 2 is the default for PS/2 keyboards and is the most commonly used scan code set.
 *
 * Protocol Overview:
 * - Make codes: 0x01-0x82 (standard key press); 0x83 (F7) and 0x84 (SysReq/Alt+PrtScn) are
 *   out-of-range special cases handled separately in the state machine below
 * - Break codes: F0 followed by make code (key release, e.g., F0 1C = key 0x1C released)
 * - Multi-byte sequences: E0-prefixed (2-3 bytes) and E1-prefixed (8 bytes)
 *
 * State Machine:
 *
 *     INIT ──[F0]──> F0 ──[code]──> INIT (process break code)
 *       │
 *       ├──[E0]──> E0 ──[F0]──> E0_F0 ──[12,59]──> INIT (ignore Fake Shift Breaks)
 *       │           │              │  (Fake Shift Break sends E0 F0 12, E0 F0 59)
 *       │           │              └──[code]──> INIT (E0-prefixed break)
 *       │           │
 *       │           └──[12,59]──> INIT (ignore fake shift makes: E0 12, E0 59)
 *       │           │
 *       │           └──[code]──> INIT (process E0-prefixed make)
 *       │
 *       ├──[E1]──> E1 ──[14]──> E1_14 ──[77]──> INIT (Pause make)
 *       │           │  (Pause/Break key sends an 8-byte sequence: E1 14 77 E1 F0 14 F0 77)
 *       │           └──[F0]──> E1_F0 ──[14]──> E1_F0_14 ──[F0]──> E1_F0_14_F0 ──[77]──> INIT
 *       |
 *       ├──[83]──> INIT (F7 key special handling)
 *       ├──[84]──> INIT (SysReq/Alt+PrtScn)
 *       └──[code]──> INIT (process normal make code)
 *
 * @param code The Set 2 scancode byte to process.
 * @return void
 *
 * @note Main loop only.
 * @note Non-blocking.
 * @note handle_keyboard_report() translates scan codes to HID keycodes via keymap lookup.
 */
void process_scancode(uint8_t code);

#endif /* SCANCODES_H */
