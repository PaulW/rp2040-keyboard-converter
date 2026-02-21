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

#include "scancode.h"

#include <stdio.h>

#include "hid_interface.h"
#include "log.h"

/**
 * @brief E0-Prefixed Scancode Translation Table for AT/PS2 Set 2 Protocol
 *
 * AT/PS2 keyboards use E0-prefixed codes extensively for extended functionality.
 * This lookup table translates E0-prefixed Set 2 scan codes to normalised interface
 * codes used throughout the converter.
 *
 * The table is sparse (most entries are 0) to allow direct indexing by scan code.
 * A return value of 0 indicates the code should be ignored or is unmapped.
 *
 * Protocol Reference:
 * - E0 prefix indicates extended key codes in AT/PS2 Set 2 protocol
 * - Used for navigation keys (arrows, home, end, page up/down, insert, delete)
 * - Used for special keys (Right Alt, Right Ctrl, GUI/Windows keys, Menu/App key)
 * - Used for multimedia and ACPI keys (volume, media control, power management)
 * - Some multimedia keys mapped to F13-F24 for compatibility
 *
 * Note: Code 0x12 and 0x59 are fake shifts and should be ignored (mapped to 0).
 */
static const uint8_t e0_code_translation[256] = {
    [0x10] = 0x08,  // WWW Search -> F13
    [0x11] = 0x0F,  // Right Alt
    [0x14] = 0x19,  // Right Ctrl
    [0x15] = 0x18,  // Previous Track -> F15
    [0x18] = 0x10,  // WWW Favourites -> F14
    [0x1F] = 0x17,  // Left GUI (Windows key)
    [0x20] = 0x18,  // WWW Refresh -> F15
    [0x21] = 0x65,  // Volume Down
    [0x23] = 0x6F,  // Mute
    [0x27] = 0x1F,  // Right GUI (Windows key)
    [0x28] = 0x20,  // WWW Stop -> F16
    [0x2B] = 0x50,  // Calculator -> F22
    [0x2F] = 0x27,  // Menu/App (Application key)
    [0x30] = 0x28,  // WWW Forward -> F17
    [0x32] = 0x6E,  // Volume Up
    [0x34] = 0x08,  // Play/Pause -> F13
    [0x37] = 0x5F,  // ACPI Power -> F24
    [0x38] = 0x30,  // WWW Back -> F18
    [0x3A] = 0x38,  // WWW Home -> F19
    [0x3B] = 0x10,  // Stop -> F14
    [0x3F] = 0x57,  // ACPI Sleep -> F23
    [0x40] = 0x40,  // My Computer -> F20
    [0x48] = 0x48,  // Email -> F21
    [0x4A] = 0x60,  // Keypad / (Slash)
    [0x4D] = 0x20,  // Next Track -> F16
    [0x50] = 0x28,  // Media Select -> F17
    [0x5A] = 0x62,  // Keypad Enter
    [0x5E] = 0x50,  // ACPI Wake -> F22
    [0x69] = 0x5C,  // End
    [0x6B] = 0x53,  // Cursor Left
    [0x6C] = 0x2F,  // Home
    [0x70] = 0x39,  // Insert
    [0x71] = 0x37,  // Delete
    [0x72] = 0x3F,  // Cursor Down
    [0x74] = 0x47,  // Cursor Right
    [0x75] = 0x4F,  // Cursor Up
    [0x7A] = 0x56,  // Page Down
    [0x7C] = 0x7F,  // Print Screen
    [0x7D] = 0x5E,  // Page Up
    [0x77] = 0x48,  // Unicomp New Model M Pause/Break
    [0x7E] = 0x48,  // Control'd Pause
    // Special codes that should be ignored:
    [0x12] = 0x00,  // Fake shift (ignore)
    [0x59] = 0x00,  // Fake shift (ignore)
    // All other entries implicitly 0 (unmapped/ignore)
};

/**
 * @brief Translates E0-prefixed scan codes to interface codes
 *
 * Performs a fast lookup in the translation table for E0-prefixed codes.
 * Returns 0 for unmapped codes or fake shifts (which should be ignored by caller).
 *
 * @param code The E0-prefixed scan code to translate
 * @return Translated interface code, or 0 if code should be ignored
 */
static inline uint8_t switch_e0_code(uint8_t code) {
    return e0_code_translation[code];
}

/**
 * @brief Process Keyboard Input (Scancode Set 2) Data
 *
 * This function processes scan codes from AT/PS2 keyboards using Scan Code Set 2.
 * Set 2 is the default for PS/2 keyboards and is the most commonly used scan code set.
 *
 * Protocol Overview:
 * - Make codes: 0x01-0x83 (key press)
 * - Break codes: F0 followed by make code (key release, e.g., F0 1C = key 0x1C released)
 * - Multi-byte sequences: E0-prefixed (2-3 bytes) and E1-prefixed (8 bytes)
 *
 * State Machine:
 *
 *     INIT ──[F0]──> F0 ──[code]──> INIT (process break code)
 *       │
 *       ├──[E0]──> E0 ──[F0]──> E0_F0 ──[code]──> INIT (E0-prefixed break)
 *       │           │
 *       │           └──[12,59]──> INIT (ignore fake shifts)
 *       │           │
 *       │           └──[code]──> INIT (process E0-prefixed make)
 *       │
 *       ├──[E1]──> E1 ──[14]──> E1_14 ──[77]──> INIT (Pause make)
 *       │           │  (Pause/Break key sends an 8-byte sequence: E1 14 77 E1 F0 14 F0 77)
 *       │           └──[F0]──> E1_F0 ──[14]──> E1_F0_14 ──[F0]──> E1_F0_14_F0 ──[77]──> INIT
 *       |
 *       ├──[83]──> INIT (F7 special handling)
 *       ├──[84]──> INIT (SysReq/Alt+PrtScn)
 *       └──[code]──> INIT (process normal make code)
 *
 * Multi-byte Sequence Examples:
 * - Normal key press:     1C (A key make)
 * - Normal key release:   F0 1C (A key break)
 * - Right Ctrl press:     E0 14 (make)
 * - Right Ctrl release:   E0 F0 14 (break)
 * - Pause press:          E1 14 77 E1 F0 14 F0 77 (make and break together)
 * - Print Screen:         E0 7C (make), E0 F0 7C (break)
 *
 * Special Handling:
 * - E0 12, E0 59: Fake shifts (ignored, sent by some keyboards with certain keys)
 * - 83: F7 key (special code)
 * - 84: SysReq / Alt+PrintScreen (special code)
 * - E1 sequences: Only used for Pause/Break key (complex 8-byte sequence)
 * - Multimedia keys: Translated to F13-F24 range for compatibility
 *
 * @param code The keycode to process.
 *
 * @note This function is called from the keyboard_interface_task() in main task context
 * @note handle_keyboard_report() translates scan codes to HID keycodes via keymap lookup
 */
void process_scancode(uint8_t code) {
    // clang-format off
  static enum {
    INIT,
    F0,
    E0,
    E0_F0,
    E1,
    E1_14,
    E1_F0,
    E1_F0_14,
    E1_F0_14_F0
  } state = INIT;
    // clang-format on

    switch (state) {
        case INIT:
            switch (code) {
                case 0xE0:
                    state = E0;
                    break;
                case 0xF0:
                    state = F0;
                    break;
                case 0xE1:
                    state = E1;
                    break;
                case 0x83:  // F7
                    handle_keyboard_report(0x02, true);
                    break;
                case 0x84:  // SysReq / Alt'd PrintScreen
                    handle_keyboard_report(0x7F, true);
                    break;
                case 0xAA:  // Self-test passed
                case 0xFC:  // Self-test failed
                default:    // Handle normal key event
                    state = INIT;
                    if (code < 0x80) {
                        handle_keyboard_report(code, true);
                    } else {
                        LOG_DEBUG("!INIT! (0x%02X)\n", code);
                    }
            }
            break;

        case F0:  // Break code
            state = INIT;
            switch (code) {
                case 0x83:  // F7
                    handle_keyboard_report(0x02, false);
                    break;
                case 0x84:  // SysReq
                    handle_keyboard_report(0x7F, false);
                    break;
                default:
                    if (code < 0x80) {
                        handle_keyboard_report(code, false);
                    } else {
                        LOG_DEBUG("!F0! (0x%02X)\n", code);
                    }
            }
            break;

        case E0:  // E0-Prefixed
            switch (code) {
                case 0x12:  // to be ignored
                case 0x59:  // to be ignored
                    state = INIT;
                    break;
                case 0xF0:
                    state = E0_F0;
                    break;
                default:
                    state = INIT;
                    if (code < 0x80) {
                        uint8_t translated = switch_e0_code(code);
                        if (translated) {
                            handle_keyboard_report(translated, true);
                        }
                    } else {
                        LOG_DEBUG("!E0! (0x%02X)\n", code);
                    }
            }
            break;

        case E0_F0:  // Break code of E0-prefixed
            state = INIT;
            switch (code) {
                case 0x12:  // to be ignored
                case 0x59:  // to be ignored
                    break;
                default:
                    if (code < 0x80) {
                        uint8_t translated = switch_e0_code(code);
                        if (translated) {
                            handle_keyboard_report(translated, false);
                        }
                    } else {
                        LOG_DEBUG("!E0_F0! (0x%02X)\n", code);
                    }
            }
            break;

        case E1:  // E1-Prefixed
            switch (code) {
                case 0x14:
                    state = E1_14;
                    break;
                case 0xF0:
                    state = E1_F0;
                    break;
                default:
                    state = INIT;
                    LOG_DEBUG("!E1! (0x%02X)\n", code);
            }
            break;

        case E1_14:  // E1-prefixed 14
            state = INIT;
            switch (code) {
                case 0x77:  // Pause
                {
                    uint8_t translated = switch_e0_code(code);
                    if (translated) {
                        handle_keyboard_report(translated, true);
                    }
                } break;
                default:
                    LOG_DEBUG("!E1_14! (0x%02X)\n", code);
            }
            break;

        case E1_F0:  // Break code of E1-prefixed
            switch (code) {
                case 0x14:
                    state = E1_F0_14;
                    break;
                default:
                    state = INIT;
                    LOG_DEBUG("!E1_F0! (0x%02X)\n", code);
            }
            break;

        case E1_F0_14:  // Break code of E1-prefixed 14
            switch (code) {
                case 0xF0:  // Pause
                    state = E1_F0_14_F0;
                    break;
                default:
                    state = INIT;
                    LOG_DEBUG("!E1_F0_14! (0x%02X)\n", code);
            }
            break;

        case E1_F0_14_F0:  // Break code of E1-prefixed 14
            state = INIT;
            switch (code) {
                case 0x77:  // Pause
                {
                    uint8_t translated = switch_e0_code(code);
                    if (translated) {
                        handle_keyboard_report(translated, false);
                    }
                } break;
                default:
                    LOG_DEBUG("!E1_F0_14_F0! (0x%02X)\n", code);
            }
            break;

        default:
            state = INIT;
    }
}