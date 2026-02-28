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

// ============================================================================
// Protocol Byte Constants
// ============================================================================

/* Prefix bytes */
#define SC_ESCAPE_E0 0xE0U /**< E0 prefix — extended key indicator */
#define SC_ESCAPE_E1 0xE1U /**< E1 prefix — Pause / Break multi-byte sequence */

/* Set 1 make/break encoding */
#define SC1_MAKE_MASK 0x7FU /**< Set 1: mask applied to break code to recover make code */
#define SC1_MAX_CODE                                                                      \
    0xD8U /**< Set 1: upper bound for valid break codes (0x58 | 0x80).                    \
           *   0x58 is the highest make code on a 101-key keyboard (F12). Break codes for \
           *   F11 (0xD7) and F12 (0xD8) are therefore the upper valid limit.             \
           *   Codes above 0xD8 are out-of-range and discarded. */

/* Set 1 fake shift bytes (E0-prefixed, silently discarded) */
#define SC1_FAKE_LSHIFT_MAKE  0x2AU /**< Spurious Left-Shift make  (E0-prefixed) */
#define SC1_FAKE_LSHIFT_BREAK 0xAAU /**< Spurious Left-Shift break (E0-prefixed) */
#define SC1_FAKE_RSHIFT_MAKE  0x36U /**< Spurious Right-Shift make  (E0-prefixed) */
#define SC1_FAKE_RSHIFT_BREAK 0xB6U /**< Spurious Right-Shift break (E0-prefixed) */

/* Set 1 E1 Pause/Break sequence bytes */
#define SC1_E1_CTRL_MAKE  0x1DU /**< E1 Pause make:  Left-Control make byte (E1 [1D] 45) */
#define SC1_E1_CTRL_BREAK 0x9DU /**< E1 Pause break: Left-Control break byte (E1 [9D] C5) */
#define SC1_NUMLOCK_MAKE  0x45U /**< E1 Pause make tail  (E1 1D [45]) */
#define SC1_NUMLOCK_BREAK 0xC5U /**< E1 Pause break tail (E1 9D [C5]) */

/* Interface code passed to handle_keyboard_report() */
#define IFACE_PAUSE 0x48U /**< Interface code: Pause / Break key */

/**
 * @brief E0-Prefixed Scancode Translation Table for XT/Set 1 Protocol
 *
 * Some XT keyboards use E0-prefixed codes for extended keys (arrows, function keys,
 * multimedia keys, etc.). This lookup table translates E0-prefixed scan codes to
 * their normalised interface codes used throughout the converter.
 *
 * The table is sparse (most entries are 0) to allow direct indexing by scan code.
 * A return value of 0 indicates the code should be ignored (e.g., fake shifts).
 *
 * Protocol Reference:
 * - E0 prefix indicates extended key codes in XT/Set 1 protocol
 * - Used for navigation keys (arrows, home, end, page up/down)
 * - Used for special keys (Windows/GUI keys, application key)
 * - Used for multimedia keys (power, sleep, wake)
 */
static const uint8_t e0_code_translation[128] = {
    [0x1C] = 0x6F,  // Keypad Enter
    [0x1D] = 0x7A,  // Right Control
    [0x35] = 0x7F,  // Keypad Slash (/)
    [0x37] = 0x54,  // Print Screen
    [0x38] = 0x7C,  // Right Alt
    [0x45] = 0x48,  // Pause / Break
    [0x46] = 0x48,  // Ctrl'd Pause
    [0x47] = 0x74,  // Home
    [0x48] = 0x60,  // Up Arrow
    [0x49] = 0x77,  // Page Up
    [0x4B] = 0x61,  // Left Arrow
    [0x4D] = 0x63,  // Right Arrow
    [0x4F] = 0x75,  // End
    [0x50] = 0x62,  // Down Arrow
    [0x51] = 0x78,  // Page Down
    [0x52] = 0x71,  // Insert
    [0x53] = 0x72,  // Delete
    [0x5B] = 0x5A,  // Left GUI (Windows key)
    [0x5C] = 0x5B,  // Right GUI (Windows key)
    [0x5D] = 0x5C,  // Application (Menu key)
    [0x5E] = 0x70,  // Power
    [0x5F] = 0x79,  // Sleep
    [0x63] = 0x7B,  // Wake
    // All other entries implicitly 0 (unmapped/ignore)
};

/**
 * @brief Translates E0-prefixed scan codes to interface codes
 *
 * Performs a fast lookup in the translation table for E0-prefixed codes.
 * Returns 0 for unmapped codes (which should be ignored by caller).
 *
 * @param code The E0-prefixed scan code to translate
 * @return Translated interface code, or 0 if code should be ignored
 */
static inline uint8_t switch_e0_code(uint8_t code) {
    return e0_code_translation[code];
}

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
 * @note This function is called from the keyboard_interface_task() in main task context
 * @note handle_keyboard_report() translates scan codes to HID keycodes via keymap lookup
 */
void process_scancode(uint8_t code) {
    // clang-format off
    static enum {
        INIT,
        E0,
        E1,
        E1_1D,
        E1_9D
    } state = INIT;
    // clang-format on

    switch (state) {
        case INIT:
            switch (code) {
                case SC_ESCAPE_E0:
                    state = E0;
                    break;
                case SC_ESCAPE_E1:
                    state = E1;
                    break;
                default:  // Handle normal key event
                    if (code <= SC1_MAX_CODE) {
                        // 0xAA (170 decimal) <= SC1_MAX_CODE (211 decimal) - treated as normal
                        // break code
                        handle_keyboard_report(code & SC1_MAKE_MASK, code < 0x80);
                    } else {
                        LOG_DEBUG("!INIT! (0x%02X)\n", code);
                    }
            }
            break;

        case E0:  // E0-Prefixed
            switch (code) {
                case SC1_FAKE_LSHIFT_MAKE:
                case SC1_FAKE_LSHIFT_BREAK:  // Filtered as fake shift, NOT as self-test code
                case SC1_FAKE_RSHIFT_MAKE:
                case SC1_FAKE_RSHIFT_BREAK:
                    // ignore fake shift
                    state = INIT;
                    break;
                default:
                    state = INIT;
                    {
                        uint8_t translated =
                            switch_e0_code(code < 0x80 ? code : (code & SC1_MAKE_MASK));
                        if (translated) {
                            handle_keyboard_report(translated, code < 0x80);
                        } else {
                            LOG_DEBUG("!E0! (0x%02X)\n", code);
                        }
                    }
            }
            break;

        case E1:  // E1-Prefixed
            switch (code) {
                case SC1_E1_CTRL_MAKE:
                    state = E1_1D;
                    break;
                case SC1_E1_CTRL_BREAK:
                    state = E1_9D;
                    break;
                default:
                    state = INIT;
                    LOG_DEBUG("!E1! (0x%02X)\n", code);
                    break;
            }
            break;

        case E1_1D:  // E1-prefixed 1D
            switch (code) {
                case SC1_NUMLOCK_MAKE:
                    handle_keyboard_report(IFACE_PAUSE, true);
                    state = INIT;
                    break;
                default:
                    state = INIT;
                    LOG_DEBUG("!E1_1D! (0x%02X)\n", code);
            }
            break;

        case E1_9D:  // E1-prefixed 9D
            switch (code) {
                case SC1_NUMLOCK_BREAK:
                    handle_keyboard_report(IFACE_PAUSE, false);
                    state = INIT;
                    break;
                default:
                    state = INIT;
                    LOG_DEBUG("!E1_9D! (0x%02X)\n", code);
            }
            break;

        default:
            state = INIT;
    }
}