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
 * @file scancode.c
 * @brief Commodore Amiga Keyboard Scancode Processor
 *
 * This implementation processes scan codes from Commodore Amiga keyboards (A1000,
 * A500, A2000, A3000, A4000). The Amiga protocol uses a simple make/break encoding
 * with one special case for the CAPS LOCK key.
 *
 * Protocol Overview:
 * - Key codes: 0x00-0x67 (7-bit values, 104 keys)
 * - Bit 7: Key state flag
 *   * 0 = Key pressed (make code)
 *   * 1 = Key released (break code)
 * - No multi-byte sequences (unlike PC/XT protocols)
 * - No prefix codes (E0, E1, etc.)
 * - All keys send up/down events EXCEPT CAPS LOCK
 *
 * CAPS LOCK Special Behavior:
 * The Amiga CAPS LOCK key is unique in keyboard history:
 * 1. Only sends code on PRESS (never on release)
 * 2. Bit 7 indicates LED state (not press/release):
 *    - Bit 7 = 0 (0x62): CAPS LOCK LED is now ON (key was pressed while LED was off)
 *    - Bit 7 = 1 (0xE2): CAPS LOCK LED is now OFF (key was pressed while LED was on)
 * 3. Computer must track LED state to stay synchronized
 * 4. On power-up, LED starts OFF
 *
 * Why This Design?:
 * The Amiga keyboard maintains its own CAPS LOCK LED state internally and toggles
 * it on each press. The bit 7 flag tells the computer the NEW state after toggle,
 * not whether this is a press or release. This prevents desync between keyboard
 * LED and computer state.
 *
 * Implementation Strategy:
 * - Most keys: Extract bit 7 for make/break, pass key code to HID layer
 * - CAPS LOCK: Special handling is done in the protocol layer (keyboard_interface.c)
 *   * Protocol compares keyboard LED state (from bit 7) with USB HID CAPS LOCK state
 *   * Only sends toggle when states differ (smart synchronization)
 *   * Generates press+release pair with configurable hold time (CAPS_LOCK_TOGGLE_TIME_MS)
 *   * This module forwards CAPS LOCK codes like any other key
 *
 * Data Flow:
 * Protocol Layer → De-rotation → Active-low inversion → THIS MODULE → HID Layer
 *
 * Example Scancodes:
 * - Space pressed:  0x40 (bit 7 = 0, key 0x40)
 * - Space released: 0xC0 (bit 7 = 1, key 0x40)
 * - CAPS LOCK (any press): 0x62 or 0xE2 → Generate press+release pair → Toggle USB CAPS LOCK
 *
 * Special Protocol Codes:
 * The following codes are handled by the protocol layer (keyboard_interface.c),
 * not by this scancode processor:
 * - 0x78: Reset warning (CTRL + both Amiga keys)
 * - 0xF9: Lost sync (keyboard will retransmit)
 * - 0xFA: Buffer overflow
 * - 0xFC: Self-test failure
 * - 0xFD: Power-up key stream start
 * - 0xFE: Power-up key stream end
 *
 * @note This function executes in main task context (not ISR)
 * @note Scancodes arrive after protocol-level processing (de-rotation, inversion)
 */

#include "scancode.h"

#include <stdio.h>
#include <stdbool.h>

#include "hid_interface.h"
#include "log.h"

/**
 * @brief Maximum valid Amiga key code
 *
 * Amiga keyboards use 7-bit key codes (0x00-0x67 = 0-103 decimal).
 * Key code 0x67 is the Right Amiga key (rightmost position).
 * Codes above 0x67 are invalid (except special protocol codes handled elsewhere).
 */
#define AMIGA_MAX_KEYCODE 0x67

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
 * The Amiga CAPS LOCK key (0x62) has unique behavior that requires special handling:
 *
 * 1. Keyboard Behavior:
 *    - Only sends code on press (never on release)
 *    - Maintains internal LED state
 *    - Toggles LED on each press
 *    - Bit 7 indicates NEW LED state after toggle:
 *      * 0x62 (bit 7 = 0): LED now ON (was OFF, pressed → turned ON)
 *      * 0xE2 (bit 7 = 1): LED now OFF (was ON, pressed → turned OFF)
 *
 * 2. Our Implementation:
 *    - Detect CAPS LOCK by key code (0x62)
 *    - Generate press+release pair for EVERY CAPS LOCK event
 *    - Ignore bit 7 (LED state is informational only)
 *    - USB HID CAPS LOCK toggles on each press+release cycle
 *    - Each Amiga press (0x62 or 0xE2) generates one USB toggle
 *
 * 3. Why This Works:
 *    - USB HID CAPS LOCK is a toggle key (not a state key)
 *    - Each press+release cycle toggles the CAPS LOCK state
 *    - Amiga keyboard toggles LED on each physical press
 *    - Our press+release pair causes USB to toggle on each physical press
 *    - Both keyboard LED and USB CAPS LOCK toggle together, staying synchronized
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
 * @note Executes in main task context (called from keyboard_interface_task)
 * @note Special protocol codes (0x78, 0xF9-0xFE) are pre-filtered by protocol layer
 * @note Invalid key codes are logged but not processed (defensive programming)
 */
void process_scancode(uint8_t code) {
    // Extract key code (bits 6-0) and make/break flag (bit 7)
    uint8_t key_code = code & 0x7F;         // Remove bit 7
    bool    is_break = (code & 0x80) != 0;  // Check bit 7: 1 = break, 0 = make
    bool    make     = !is_break;           // Invert for handle_keyboard_report convention

    // Validate key code range (0x00-0x67 are valid)
    // Special codes (0x78, 0xF9-0xFE) should be filtered by protocol layer
    if (key_code > AMIGA_MAX_KEYCODE) {
        LOG_ERROR("Invalid Amiga key code: 0x%02X (raw scancode: 0x%02X)\n", key_code, code);
        return;
    }

    // All keys processed as simple make/break codes at this level
    // CAPS LOCK special handling (LED state comparison, press+release generation)
    // is performed earlier in the protocol layer (keyboard_interface.c) before
    // codes reach this scancode processor
    handle_keyboard_report(key_code, make);
}
