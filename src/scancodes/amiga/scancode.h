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
 * @brief Commodore Amiga keyboard scancode processing interface.
 *
 * @see keyboard_interface.h for the Amiga keyboard protocol specification.
 */

#ifndef AMIGA_SCANCODE_H
#define AMIGA_SCANCODE_H

#include <stdint.h>

/**
 * @brief Process Commodore Amiga Keyboard Scancode Data
 *
 * This function processes scan codes from Amiga keyboards after they have been
 * de-rotated and inverted by the protocol layer. The Amiga protocol uses a simple
 * make/break encoding with one critical exception for CAPS LOCK.
 *
 * Normal Key Processing:
 * - Bit 7 = 0: Key pressed (make code)
 * - Bit 7 = 1: Key released (break code)
 * - Extract key code from bits 6-0
 * - Pass to HID layer with make/break flag
 *
 * CAPS LOCK Special Handling:
 * The Amiga CAPS LOCK key (0x62) has unique behaviour that requires special handling:
 *
 * 1. Keyboard Behaviour:
 *    - Only sends code on press (never on release)
 *    - Maintains internal LED state
 *    - Toggles LED on each press
 *    - Bit 7 indicates NEW LED state after toggle:
 *      * 0x62 (bit 7 = 0): LED now ON (was OFF, pressed → turned ON)
 *      * 0xE2 (bit 7 = 1): LED now OFF (was ON, pressed → turned OFF)
 *
 * 2. Our Implementation:
 *    - Detect CAPS LOCK by key code (0x62)
 *    - Use bit 7 to infer the keyboard LED state after toggle
 *    - Compare keyboard LED state with current USB HID CAPS state
 *    - Generate a press+release pair only when states are out of sync
 *    - If states are already synchronised, no HID event is emitted
 *
 * 3. Why This Works:
 *    - USB HID CAPS LOCK is a toggle key (not a state key)
 *    - Each press+release cycle toggles the CAPS LOCK state
 *    - Amiga keyboard toggles LED on each physical press
 *    - Our press+release pair causes USB to toggle on each physical press
 *    - Both keyboard LED and USB CAPS LOCK toggle together, staying synchronised
 *
 * Protocol Notes:
 * - All codes arrive after de-rotation (6-5-4-3-2-1-0-7 → 7-6-5-4-3-2-1-0)
 * - All codes arrive after active-low inversion (HIGH=0, LOW=1 corrected)
 * - Special codes (0x78, 0xF9-0xFE) filtered by protocol layer before reaching here
 * - Key codes 0x00-0x67 are valid (104 keys total)
 * - Codes > 0x67 (excluding special codes) are invalid/reserved
 *
 * Example Sequences:
 *
 * Normal Key (Space = 0x40):
 *   Press:   0x40 → keycode=0x40, make=true  → handle_keyboard_report(0x40, true)
 *   Release: 0xC0 → keycode=0x40, make=false → handle_keyboard_report(0x40, false)
 *
 * CAPS LOCK (0x62):
 *   First press:  0x62 (LED ON)  → Send press+release → USB toggles OFF→ON
 *   Second press: 0xE2 (LED OFF) → Send press+release → USB toggles ON→OFF
 *   (Both codes generate identical press+release pair, causing USB to toggle)
 *
 * Modifier Keys (independent matrix positions):
 *   Left Shift:  0x60 (press), 0xE0 (release)
 *   Right Shift: 0x61 (press), 0xE1 (release)
 *   Control:     0x63 (press), 0xE3 (release)
 *   Left Alt:    0x64 (press), 0xE4 (release)
 *   Right Alt:   0x65 (press), 0xE5 (release)
 *   Left Amiga:  0x66 (press), 0xE6 (release)
 *   Right Amiga: 0x67 (press), 0xE7 (release)
 *
 * @param code The scancode to process (8-bit: bit 7 = state, bits 6-0 = key code)
 *
 * @note Main loop only.
 * @note Special protocol codes (0x78, 0xF9-0xFE) are pre-filtered by protocol layer
 * @note Invalid key codes are logged but not processed (defensive programming)
 */
void process_scancode(uint8_t code);

/**
 * @brief Task function for scancode processor timing operations
 *
 * Handles delayed CAPS LOCK release after configurable hold time.
 * Called from main loop to check if release event should be sent.
 *
 * @note Main loop only.
 * @note Direct HID injection bypasses ring buffer (no producer/consumer violation)
 */
void scancode_task(void);

#endif /* AMIGA_SCANCODE_H */
