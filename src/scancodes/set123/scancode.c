/*
 * This file is part of RP2040 Keyboard Converter.
 *
 * Copyright 2023 Paul Bramhall (paulwamp@gmail.com)
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
#include <string.h>

#include "hid_interface.h"
#include "log.h"

// ============================================================================
// E0-Prefix Translation Tables
// ============================================================================

/**
 * @brief E0-Prefixed Scancode Translation Table for XT/Set 1 Protocol
 * 
 * Some XT keyboards use E0-prefixed codes for extended keys (arrows, function keys,
 * multimedia keys, etc.). This lookup table translates E0-prefixed scan codes to
 * their normalized interface codes used throughout the converter.
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
static const uint8_t set1_e0_translation[256] = {
    [0x1C] = 0x6F,  // Keypad Enter
    [0x1D] = 0x7A,  // Right Control
    [0x35] = 0x7F,  // Keypad Slash (/)
    [0x37] = 0x54,  // Print Screen
    [0x38] = 0x7C,  // Right Alt
    [0x46] = 0x55,  // Pause / Break
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
 * @brief E0-Prefixed Scancode Translation Table for AT/PS2 Set 2 Protocol
 * 
 * AT/PS2 keyboards use E0-prefixed codes extensively for extended functionality.
 * This lookup table translates E0-prefixed Set 2 scan codes to normalized interface
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
static const uint8_t set2_e0_translation[256] = {
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
    // Special codes that should be ignored:
    [0x12] = 0x00,  // Fake shift (ignore)
    [0x59] = 0x00,  // Fake shift (ignore)
    [0x77] = 0x00,  // Unicomp New Model M Pause/Break key fix (ignore)
    [0x7E] = 0x00,  // Control'd Pause (ignore)
    // All other entries implicitly 0 (unmapped/ignore)
};

// ============================================================================
// Configuration Structures (Exported for easy setup)
// ============================================================================

const scancode_config_t SCANCODE_CONFIG_SET1 = {
    .set = SCANCODE_SET1,
    .e0_translation = set1_e0_translation,
    .has_e0_prefix = true,
    .has_e1_prefix = true
};

const scancode_config_t SCANCODE_CONFIG_SET2 = {
    .set = SCANCODE_SET2,
    .e0_translation = set2_e0_translation,
    .has_e0_prefix = true,
    .has_e1_prefix = true
};

const scancode_config_t SCANCODE_CONFIG_SET3 = {
    .set = SCANCODE_SET3,
    .e0_translation = NULL,
    .has_e0_prefix = false,
    .has_e1_prefix = false
};

// ============================================================================
// State Machine
// ============================================================================

/**
 * @brief Unified State Machine States
 * 
 * This enum defines all possible states for the unified XT/AT scancode processor.
 * Different scancode sets use different subsets of these states:
 * 
 * - Set 1: INIT, E0, E1, E1_1D, E1_9D (5 states)
 * - Set 2: INIT, F0, E0, E0_F0, E1, E1_14, E1_F0, E1_F0_14, E1_F0_14_F0 (9 states)
 * - Set 3: INIT, F0 (2 states)
 */
typedef enum {
    INIT,           // Initial state / ready for next scancode
    F0,             // Break prefix received (Set 2/3)
    E0,             // E0 prefix received (Set 1/2)
    E0_F0,          // E0 F0 sequence (Set 2)
    E1,             // E1 prefix received (Set 1/2)
    E1_1D,          // E1 1D sequence (Set 1 Pause make)
    E1_9D,          // E1 9D sequence (Set 1 Pause break)
    E1_14,          // E1 14 sequence (Set 2 Pause make)
    E1_F0,          // E1 F0 sequence (Set 2 Pause break)
    E1_F0_14,       // E1 F0 14 sequence (Set 2 Pause break)
    E1_F0_14_F0     // E1 F0 14 F0 sequence (Set 2 Pause break)
} scancode_state_t;

static scancode_state_t state = INIT;

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Translate E0-prefixed code using appropriate table
 * 
 * @param code The E0-prefixed scancode to translate
 * @param config Pointer to scancode configuration
 * @return Translated interface code, or 0 if unmapped/ignored
 */
static inline uint8_t translate_e0_code(uint8_t code, const scancode_config_t *config) {
    if (config->e0_translation == NULL) {
        return 0;
    }
    return config->e0_translation[code];
}

/**
 * @brief Check if code is a fake shift for the current set
 * 
 * Fake shifts are spurious shift codes sent by some keyboards with certain
 * extended keys. They should be ignored.
 * 
 * Set 1: E0 2A, E0 AA, E0 36, E0 B6
 * Set 2: E0 12, E0 59
 * Set 3: None
 * 
 * @param code The scancode to check
 * @param config Pointer to scancode configuration
 * @return true if code is a fake shift (should be ignored)
 */
static inline bool is_fake_shift(uint8_t code, const scancode_config_t *config) {
    switch (config->set) {
        case SCANCODE_SET1:
            return (code == 0x2A || code == 0xAA || code == 0x36 || code == 0xB6);
        case SCANCODE_SET2:
            return (code == 0x12 || code == 0x59);
        case SCANCODE_SET3:
            return false;
        default:
            return false;
    }
}

/**
 * @brief Process normal (non-prefixed) scancode
 * 
 * @param code The scancode to process
 * @param config Pointer to scancode configuration
 */
static void process_normal_code(uint8_t code, const scancode_config_t *config) {
    // Validate configuration pointer
    if (!config) {
        return;
    }

    // Set-specific special codes
    switch (config->set) {
        case SCANCODE_SET1:
            // Set 1: Break = make | 0x80
            if (code < 0x80) {
                handle_keyboard_report(code, true);
            } else {
                handle_keyboard_report(code & 0x7F, false);
            }
            break;

        case SCANCODE_SET2:
            // Set 2: Special codes 0x83 (F7) and 0x84 (SysReq)
            if (code == 0x83) {
                handle_keyboard_report(0x02, true);
            } else if (code == 0x84) {
                handle_keyboard_report(0x7F, true);
            } else if (code < 0x80) {
                handle_keyboard_report(code, true);
            } else {
                LOG_DEBUG("!INIT! (0x%02X)\n", code);
            }
            break;

        case SCANCODE_SET3:
            // Set 3: Special codes 0x7C, 0x83, 0x84
            if (code == 0x7C) {
                handle_keyboard_report(0x68, true);  // Keypad Comma
            } else if (code == 0x83) {
                handle_keyboard_report(0x02, true);  // F7
            } else if (code == 0x84) {
                handle_keyboard_report(0x7F, true);  // Keypad Plus
            } else if (code < 0x80) {
                handle_keyboard_report(code, true);
            } else {
                LOG_DEBUG("!INIT! (0x%02X)\n", code);
            }
            break;
    }
}

/**
 * @brief Process break code (F0 prefix for Set 2/3, or Set 2 special codes)
 * 
 * @param code The scancode following F0 prefix
 * @param config Pointer to scancode configuration
 */
static void process_break_code(uint8_t code, const scancode_config_t *config) {
    switch (config->set) {
        case SCANCODE_SET2:
            // Set 2: Special codes 0x83 (F7) and 0x84 (SysReq)
            if (code == 0x83) {
                handle_keyboard_report(0x02, false);
            } else if (code == 0x84) {
                handle_keyboard_report(0x7F, false);
            } else if (code < 0x80) {
                handle_keyboard_report(code, false);
            } else {
                LOG_DEBUG("!F0! (0x%02X)\n", code);
            }
            break;

        case SCANCODE_SET3:
            // Set 3: Special codes 0x7C, 0x83, 0x84
            if (code == 0x7C) {
                handle_keyboard_report(0x68, false);  // Keypad Comma
            } else if (code == 0x83) {
                handle_keyboard_report(0x02, false);  // F7
            } else if (code == 0x84) {
                handle_keyboard_report(0x7F, false);  // Keypad Plus
            } else if (code < 0x80) {
                handle_keyboard_report(code, false);
            } else {
                LOG_DEBUG("!F0! (0x%02X)\n", code);
            }
            break;

        default:
            LOG_DEBUG("!F0! unexpected for Set %d\n", config->set);
            break;
    }
}

// ============================================================================
// Main Processor
// ============================================================================

void process_scancode(uint8_t code, const scancode_config_t *config) {
    switch (state) {
        case INIT:
            // Check for prefix codes
            if (code == 0xE0 && config->has_e0_prefix) {
                state = E0;
            } else if (code == 0xE1 && config->has_e1_prefix) {
                state = E1;
            } else if (code == 0xF0 && (config->set == SCANCODE_SET2 || config->set == SCANCODE_SET3)) {
                state = F0;
            } else {
                // Normal scancode
                process_normal_code(code, config);
                state = INIT;
            }
            break;

        case F0:
            // Break code (Set 2/3)
            process_break_code(code, config);
            state = INIT;
            break;

        case E0:
            // E0-prefixed code
            if (is_fake_shift(code, config)) {
                // Ignore fake shifts
                state = INIT;
            } else if (code == 0xF0 && config->set == SCANCODE_SET2) {
                // E0 F0 sequence (Set 2 break)
                state = E0_F0;
            } else {
                // E0-prefixed make code
                state = INIT;
                uint8_t base_code = (config->set == SCANCODE_SET1 && code >= 0x80) ? (code & 0x7F) : code;
                bool is_make = (config->set == SCANCODE_SET1) ? (code < 0x80) : true;
                
                if (base_code < 0x80) {
                    uint8_t translated = translate_e0_code(base_code, config);
                    if (translated) {
                        handle_keyboard_report(translated, is_make);
                    }
                } else {
                    LOG_DEBUG("!E0! (0x%02X)\n", code);
                }
            }
            break;

        case E0_F0:
            // E0 F0 sequence (Set 2 break)
            if (is_fake_shift(code, config)) {
                // Ignore fake shifts
                state = INIT;
            } else {
                state = INIT;
                if (code < 0x80) {
                    uint8_t translated = translate_e0_code(code, config);
                    if (translated) {
                        handle_keyboard_report(translated, false);
                    }
                } else {
                    LOG_DEBUG("!E0_F0! (0x%02X)\n", code);
                }
            }
            break;

        // E1 sequences (Pause/Break key)
        case E1:
            if (config->set == SCANCODE_SET1) {
                // Set 1: E1 1D 45 (make) or E1 9D C5 (break)
                if (code == 0x1D) {
                    state = E1_1D;
                } else if (code == 0x9D) {
                    state = E1_9D;
                } else {
                    state = INIT;
                    LOG_DEBUG("!E1! (0x%02X)\n", code);
                }
            } else if (config->set == SCANCODE_SET2) {
                // Set 2: E1 14 77 (make) or E1 F0 14 F0 77 (break)
                if (code == 0x14) {
                    state = E1_14;
                } else if (code == 0xF0) {
                    state = E1_F0;
                } else {
                    state = INIT;
                    LOG_DEBUG("!E1! (0x%02X)\n", code);
                }
            } else {
                state = INIT;
            }
            break;

        case E1_1D:
            // Set 1: E1 1D 45 (Pause make)
            if (code == 0x45) {
                handle_keyboard_report(0x55, true);
            } else {
                LOG_DEBUG("!E1_1D! (0x%02X)\n", code);
            }
            state = INIT;
            break;

        case E1_9D:
            // Set 1: E1 9D C5 (Pause break)
            if (code == 0xC5) {
                handle_keyboard_report(0x55, false);
            } else {
                LOG_DEBUG("!E1_9D! (0x%02X)\n", code);
            }
            state = INIT;
            break;

        case E1_14:
            // Set 2: E1 14 77 (Pause make)
            if (code == 0x77) {
                // Pause key uses direct HID code 0x55, not E0 translation
                handle_keyboard_report(0x55, true);
            } else {
                LOG_DEBUG("!E1_14! (0x%02X)\n", code);
            }
            state = INIT;
            break;

        case E1_F0:
            // Set 2: E1 F0 14 ... (Pause break)
            if (code == 0x14) {
                state = E1_F0_14;
            } else {
                state = INIT;
                LOG_DEBUG("!E1_F0! (0x%02X)\n", code);
            }
            break;

        case E1_F0_14:
            // Set 2: E1 F0 14 F0 ... (Pause break)
            if (code == 0xF0) {
                state = E1_F0_14_F0;
            } else {
                state = INIT;
                LOG_DEBUG("!E1_F0_14! (0x%02X)\n", code);
            }
            break;

        case E1_F0_14_F0:
            // Set 2: E1 F0 14 F0 77 (Pause break)
            if (code == 0x77) {
                // Pause key uses direct HID code 0x55, not E0 translation
                handle_keyboard_report(0x55, false);
            } else {
                LOG_DEBUG("!E1_F0_14_F0! (0x%02X)\n", code);
            }
            state = INIT;
            break;

        default:
            state = INIT;
            break;
    }
}

void reset_scancode_state(void) {
    state = INIT;
}
