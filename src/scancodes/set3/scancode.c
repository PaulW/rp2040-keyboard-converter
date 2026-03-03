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
 * @brief Terminal Scancode Set 3 processing implementation.
 *
 * @see scancode.h for the public API.
 */

#include "scancode.h"

#include <stdbool.h>
#include <stdint.h>

#include "flow_tracker.h"
#include "hid_interface.h"
#include "log.h"

// --- Private Functions ---

typedef struct {
    uint8_t scancode;
    uint8_t mapped_code;
} scancode_remapping_entry_t;

typedef enum {
    INIT,
    F0,
} scancode_state_t;

// Scancode remapping table for Set 3 special keys
static const scancode_remapping_entry_t scancode_remapping[] = {
    {0x7C, 0x68},  // Keypad Comma
    {0x83, 0x02},  // Left F7 Position
    {0x84, 0x7F},  // Keypad Plus (Legend says minus)
};

/**
 * @brief Check if scancode needs remapping and return the mapped code.
 *
 * @param code Input scancode to check
 * @param mapped_code Output parameter for the mapped code (if found)
 * @return true if scancode was found in remapping table, false otherwise
 * @note Main loop only.
 */
static bool try_remap_scancode(uint8_t code, uint8_t* mapped_code) {
    for (size_t i = 0; i < sizeof(scancode_remapping) / sizeof(scancode_remapping[0]); i++) {
        if (scancode_remapping[i].scancode == code) {
            *mapped_code = scancode_remapping[i].mapped_code;
            return true;
        }
    }
    return false;
}

// --- Public Functions ---

void process_scancode(uint8_t code) {
    FLOW_STEP(code);
    static scancode_state_t state = INIT;

    switch (state) {
        case INIT: {
            // Check for remapped scancodes first
            uint8_t mapped_code = 0;
            if (try_remap_scancode(code, &mapped_code)) {
                handle_keyboard_report(mapped_code, true);
                break;
            }

            // Handle remaining scancodes
            switch (code) {
                case 0xF0:
                    state = F0;
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
        } break;

        case F0:  // Break code
        {
            // Check for remapped scancodes first
            uint8_t mapped_code = 0;
            if (try_remap_scancode(code, &mapped_code)) {
                handle_keyboard_report(mapped_code, false);
                state = INIT;
                break;
            }

            // Handle remaining scancodes
            state = INIT;
            if (code < 0x80) {
                handle_keyboard_report(code, false);
            } else {
                LOG_DEBUG("!F0! (0x%02X)\n", code);
            }
        } break;

        default:
            state = INIT;
    }
}