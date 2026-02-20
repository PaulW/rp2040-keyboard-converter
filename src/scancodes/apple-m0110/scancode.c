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

// NOTE: Apple M0110/M0110A protocol and scancode support is currently under
// development and considered incomplete/placeholder. The protocol implementation
// and scancode translation may require further refinement based on hardware
// testing and real-world keyboard behavior.

#include "scancode.h"

#include "hid_interface.h"
#include "log.h"

/**
 * @brief Translation table for M0110A 0x79-prefixed key codes
 *
 * O(1) lookup for extended keypad and cursor keys. Unmapped codes return 0.
 */
static const uint8_t code_79_translation[64] = {
    [0x03] = 0x12,  // Keypad Dot
    [0x05] = 0x28,  // Cursor Right
    [0x0D] = 0x20,  // Cursor Left
    [0x11] = 0x18,  // Cursor Down
    [0x19] = 0x1A,  // Keypad Enter
    [0x1B] = 0x10,  // Cursor Up
    [0x1D] = 0x22,  // Keypad Minus
    [0x25] = 0x30,  // Keypad 0
    [0x27] = 0x38,  // Keypad 1
    [0x29] = 0x40,  // Keypad 2
    [0x2B] = 0x48,  // Keypad 3
    [0x2D] = 0x50,  // Keypad 4
    [0x2F] = 0x58,  // Keypad 5
    [0x31] = 0x60,  // Keypad 6
    [0x33] = 0x68,  // Keypad 7
    [0x37] = 0x70,  // Keypad 8
    [0x39] = 0x78,  // Keypad 9
    // All other entries implicitly 0 (unmapped)
};

/**
 * @brief Translation table for M0110A 0x71,0x79-prefixed key codes
 *
 * O(1) lookup for double-extended keypad and cursor keys. Unmapped codes return 0.
 */
static const uint8_t code_71_translation[64] = {
    [0x05] = 0x42,  // Cursor Right
    [0x0D] = 0x4A,  // Keypad 0
    [0x0F] = 0x2A,  // Cursor Up
    [0x11] = 0x32,  // Cursor Down
    [0x1B] = 0x3A,  // Cursor Left
    // All other entries implicitly 0 (unmapped)
};

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
 */
void process_scancode(uint8_t code) {
    // clang-format off
    static enum {
        INIT,
        PREFIX_79,      // Got 0x79, next byte uses code_79_translation[]
        PREFIX_71,      // Got 0x71, expecting 0x79 next
        PREFIX_71_79    // Got 0x71 then 0x79, next byte uses code_71_translation[]
    } state = INIT;
    // clang-format on

    // Validate that bit 0 is set (should always be 1 for key transitions)
    if ((code & 0x01) == 0) {
        LOG_DEBUG("Invalid M0110 scancode (bit 0 not set): 0x%02X\n", code);
        state = INIT;
        return;
    }

    // Check for prefix bytes only when in INIT state (prevents overwriting multi-byte sequences)
    // Prefix bytes: 0x79 (make) or 0xF9 (break), 0x71 (make) or 0xF1 (break)
    if (state == INIT) {
        if (code == 0x79 || code == 0xF9) {
            // Start of 0x79 prefix sequence (two-byte)
            state = PREFIX_79;
            return;
        } else if (code == 0x71 || code == 0xF1) {
            // Start of 0x71,0x79 prefix sequence (three-byte)
            state = PREFIX_71;
            return;
        }
    }

    bool    is_key_up = (code & 0x80) != 0;  // Check bit 7
    uint8_t key_code  = (code >> 1) & 0x3F;  // Extract key code (bits 6-1)
    bool    make      = !is_key_up;          // Invert for make/break logic

    switch (state) {
        case INIT:
            // Normal single-byte scancode
            if (key_code == 0x00) {
                LOG_DEBUG("Reserved key code in M0110 scancode: 0x%02X\n", code);
                return;
            }
            handle_keyboard_report(key_code, make);
            LOG_DEBUG("Apple M0110 Key: 0x%02X (%s) [raw: 0x%02X]\n", key_code,
                      make ? "make" : "break", code);
            break;

        case PREFIX_79:
            // Second byte of 0x79 prefix sequence - translate via lookup table
            state = INIT;
            {
                uint8_t translated_code = code_79_translation[key_code];
                if (translated_code != 0) {
                    handle_keyboard_report(translated_code, make);
                    LOG_DEBUG("Apple M0110 Key (0x79 prefix): 0x%02X->0x%02X (%s) [raw: 0x%02X]\n",
                              key_code, translated_code, make ? "make" : "break", code);
                } else {
                    LOG_DEBUG("Unknown 0x79 prefix scancode: 0x%02X\n", code);
                }
            }
            break;

        case PREFIX_71:
            // Second byte of 0x71,0x79 sequence - expecting 0x79 or 0xF9
            if (code == 0x79 || code == 0xF9) {
                // Valid 0x79/0xF9 after 0x71/0xF1, wait for third byte
                state = PREFIX_71_79;
                return;
            } else {
                // Invalid sequence - reset
                LOG_DEBUG("Invalid sequence: expected 0x79/0xF9 after 0x71/0xF1, got 0x%02X\n",
                          code);
                state = INIT;
            }
            break;

        case PREFIX_71_79:
            // Third byte of 0x71,0x79 prefix sequence - translate via lookup table
            state = INIT;
            {
                uint8_t translated_code = code_71_translation[key_code];
                if (translated_code != 0) {
                    handle_keyboard_report(translated_code, make);
                    LOG_DEBUG(
                        "Apple M0110 Key (0x71,0x79 prefix): 0x%02X->0x%02X (%s) [raw: 0x%02X]\n",
                        key_code, translated_code, make ? "make" : "break", code);
                } else {
                    LOG_DEBUG("Unknown 0x71,0x79 prefix scancode: 0x%02X\n", code);
                }
            }
            break;

        default:
            state = INIT;
            break;
    }
}