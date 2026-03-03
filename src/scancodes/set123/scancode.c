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
 * @brief Combined Scancode Sets 1, 2, and 3 unified processing implementation.
 *
 * @see scancode.h for the public API and dispatch model.
 */

#include "scancode.h"

#include <stdbool.h>
#include <stdint.h>

#include "flow_tracker.h"
#include "hid_interface.h"
#include "log.h"

// Protocol Byte Constants

// Escape / prefix bytes common to Set 2 and Set 3 (and Set 1 extended)
#define SC_ESCAPE_E0 0xE0U /**< E0 prefix — extended key indicator */
#define SC_ESCAPE_E1 0xE1U /**< E1 prefix — Pause / Break multi-byte sequence */
#define SC_ESCAPE_F0 0xF0U /**< F0 prefix — Set 2/3 break code indicator */

// Set 1 (XT / original IBM PC) protocol-specific byte values
#define SC1_BREAK_BIT 0x80U /**< Set 1: OR'd into make code to form break code */
#define SC1_MAKE_MASK 0x7FU /**< Set 1: mask applied to break code to recover make code */
#define SC1_MAX_CODE                                                                      \
    0xD8U /**< Set 1: upper bound for valid break codes (0x58 | SC1_BREAK_BIT).           \
           *   0x58 is the highest make code on a 101-key keyboard (F12). Break codes for \
           *   F11 (0xD7) and F12 (0xD8) are therefore the upper valid limit.             \
           *   Codes above 0xD8 are out-of-range and discarded. */
#define SC1_FAKE_LSHIFT_MAKE  0x2AU /**< Set 1: spurious Left-Shift make  (E0-prefixed) */
#define SC1_FAKE_LSHIFT_BREAK 0xAAU /**< Set 1: spurious Left-Shift break (E0-prefixed) */
#define SC1_FAKE_RSHIFT_MAKE  0x36U /**< Set 1: spurious Right-Shift make  (E0-prefixed) */
#define SC1_FAKE_RSHIFT_BREAK 0xB6U /**< Set 1: spurious Right-Shift break (E0-prefixed) */
#define SC1_E1_CTRL_MAKE \
    0x1DU /**< Set 1: E1 Pause sequence — Left-Control make byte (reused as Pause encoding) */
#define SC1_E1_CTRL_BREAK \
    0x9DU /**< Set 1: E1 Pause sequence — Left-Control break byte (0x1D | SC1_BREAK_BIT) */
#define SC1_NUMLOCK_MAKE  0x45U /**< Set 1: E1 Pause make tail — Num Lock byte */
#define SC1_NUMLOCK_BREAK 0xC5U /**< Set 1: E1 Pause break tail — Num Lock break byte */

// Set 2 (AT / PS2 default) protocol-specific byte values
#define SC2_FAKE_LSHIFT 0x12U /**< Set 2: spurious Left-Shift (with E0 prefix) */
#define SC2_FAKE_RSHIFT 0x59U /**< Set 2: spurious Right-Shift (with E0 prefix) */
#define SC2_CTRL                                                                      \
    0x14U /**< Set 2: Left-Control make; also Right-Control when E0-prefixed. Used as \
           *   the first data byte of the E1 Pause make/break sequences. */
#define SC2_NUMLOCK 0x77U /**< Set 2: E1 Pause make/break tail — Num Lock code */

// Special scancodes shared by Sets 2 and 3 (fall outside the normal 0x01–0x7E range)
#define SC23_SPECIAL_F7     0x83U /**< Sets 2 and 3: F7 key scancode */
#define SC23_SPECIAL_SYSREQ 0x84U /**< Sets 2 and 3: SysReq / Alt+Print Screen scancode */

// Set 3 (IBM terminal) protocol-specific byte values
#define SC3_KP_PLUS 0x7CU /**< Set 3: Keypad Plus (KP-+) untranslated scancode */

// Interface codes — output values passed to handle_keyboard_report().
// These are protocol-to-HID translation table indices, not raw HID usage IDs.
#define IFACE_PAUSE    0x48U /**< Interface code: Pause / Break key */
#define IFACE_KP_COMMA 0x68U /**< Interface code: Keypad Comma */
#define IFACE_KP_PLUS  0x7FU /**< Interface code: Keypad Plus */

// E0-Prefix Translation Tables

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
static const uint8_t set1_e0_translation[256] = {
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
    [0x77] = 0x48,  // Unicomp New Model M Pause/Break
    [0x7E] = 0x48,  // Control'd Pause
    // Special codes that should be ignored:
    [0x12] = 0x00,  // Fake shift (ignore)
    [0x59] = 0x00,  // Fake shift (ignore)
    // All other entries implicitly 0 (unmapped/ignore)
};

// Configuration Structures

const scancode_config_t SCANCODE_CONFIG_SET1 = {.set            = SCANCODE_SET1,
                                                .e0_translation = set1_e0_translation,
                                                .has_e0_prefix  = true,
                                                .has_e1_prefix  = true};

const scancode_config_t SCANCODE_CONFIG_SET2 = {.set            = SCANCODE_SET2,
                                                .e0_translation = set2_e0_translation,
                                                .has_e0_prefix  = true,
                                                .has_e1_prefix  = true};

const scancode_config_t SCANCODE_CONFIG_SET3 = {
    .set = SCANCODE_SET3, .e0_translation = NULL, .has_e0_prefix = false, .has_e1_prefix = false};

// State Machine

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
    INIT,        // Initial state / ready for next scancode
    F0,          // Break prefix received (Set 2/3)
    E0,          // E0 prefix received (Set 1/2)
    E0_F0,       // E0 F0 sequence (Set 2)
    E1,          // E1 prefix received (Set 1/2)
    E1_1D,       // E1 1D sequence (Set 1 Pause make)
    E1_9D,       // E1 9D sequence (Set 1 Pause break)
    E1_14,       // E1 14 sequence (Set 2 Pause make)
    E1_F0,       // E1 F0 sequence (Set 2 Pause break)
    E1_F0_14,    // E1 F0 14 sequence (Set 2 Pause break)
    E1_F0_14_F0  // E1 F0 14 F0 sequence (Set 2 Pause break)
} scancode_state_t;

static scancode_state_t state = INIT;

// --- Private Functions ---

/**
 * @brief Translate E0-prefixed code using appropriate table
 *
 * @param code The E0-prefixed scancode to translate
 * @param config Pointer to scancode configuration
 * @return Translated interface code, or 0 if unmapped/ignored
 * @note Main loop only.
 */
static inline uint8_t translate_e0_code(uint8_t code, const scancode_config_t* config) {
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
 * @note Main loop only.
 */
static inline bool is_fake_shift(uint8_t code, const scancode_config_t* config) {
    switch (config->set) {
        case SCANCODE_SET1:
            return (code == SC1_FAKE_LSHIFT_MAKE || code == SC1_FAKE_LSHIFT_BREAK ||
                    code == SC1_FAKE_RSHIFT_MAKE || code == SC1_FAKE_RSHIFT_BREAK);
        case SCANCODE_SET2:
            return (code == SC2_FAKE_LSHIFT || code == SC2_FAKE_RSHIFT);
        // Set 3 explicitly has no fake shifts by design (see scancode_set_t docs);
        // the branch body is identical to default intentionally.
        // NOLINTNEXTLINE(bugprone-branch-clone)
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
 * @note Main loop only.
 */
static void process_normal_code(uint8_t code, const scancode_config_t* config) {
    // Validate configuration pointer
    if (!config) {
        return;
    }

    // Set-specific special codes
    switch (config->set) {
        case SCANCODE_SET1:
            // Set 1: Break = make | 0x80, valid break range 0x81-0xD8 (SC1_MAX_CODE)
            if (code <= SC1_MAX_CODE) {
                handle_keyboard_report(code & SC1_MAKE_MASK, code < SC1_BREAK_BIT);
            } else {
                LOG_DEBUG("!INIT! out-of-range (0x%02X)\n", code);
            }
            break;

        case SCANCODE_SET2:
            // Set 2: Special codes SC23_SPECIAL_F7 (F7) and SC23_SPECIAL_SYSREQ (SysReq)
            if (code == SC23_SPECIAL_F7) {
                handle_keyboard_report(0x02, true);
            } else if (code == SC23_SPECIAL_SYSREQ) {
                handle_keyboard_report(IFACE_KP_PLUS, true);
            } else if (code < SC1_BREAK_BIT) {
                handle_keyboard_report(code, true);
            } else {
                LOG_DEBUG("!INIT! (0x%02X)\n", code);
            }
            break;

        case SCANCODE_SET3:
            // Set 3: Special codes SC3_PRINT_SCREEN, SC23_SPECIAL_F7, SC23_SPECIAL_SYSREQ
            if (code == SC3_KP_PLUS) {
                handle_keyboard_report(IFACE_KP_COMMA, true);  // Keypad Comma
            } else if (code == SC23_SPECIAL_F7) {
                handle_keyboard_report(0x02, true);  // F7
            } else if (code == SC23_SPECIAL_SYSREQ) {
                handle_keyboard_report(IFACE_KP_PLUS, true);  // Keypad Plus
            } else if (code < SC1_BREAK_BIT) {
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
 * @note Main loop only.
 */
static void process_break_code(uint8_t code, const scancode_config_t* config) {
    switch (config->set) {
        case SCANCODE_SET2:
            // Set 2: Special codes SC23_SPECIAL_F7 (F7) and SC23_SPECIAL_SYSREQ (SysReq)
            if (code == SC23_SPECIAL_F7) {
                handle_keyboard_report(0x02, false);
            } else if (code == SC23_SPECIAL_SYSREQ) {
                handle_keyboard_report(IFACE_KP_PLUS, false);
            } else if (code < SC1_BREAK_BIT) {
                handle_keyboard_report(code, false);
            } else {
                LOG_DEBUG("!F0! (0x%02X)\n", code);
            }
            break;

        case SCANCODE_SET3:
            // Set 3: Special codes SC3_PRINT_SCREEN, SC23_SPECIAL_F7, SC23_SPECIAL_SYSREQ
            if (code == SC3_KP_PLUS) {
                handle_keyboard_report(IFACE_KP_COMMA, false);  // Keypad Comma
            } else if (code == SC23_SPECIAL_F7) {
                handle_keyboard_report(0x02, false);  // F7
            } else if (code == SC23_SPECIAL_SYSREQ) {
                handle_keyboard_report(IFACE_KP_PLUS, false);  // Keypad Plus
            } else if (code < SC1_BREAK_BIT) {
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

// Per-State Handlers
//
// Each function handles exactly one state in the scancode state machine.
// process_scancode() is a thin dispatcher that calls these; keeping the logic
// split this way makes each state's behaviour independently readable and keeps
// the cognitive complexity of any single function well within bounds.

/**
 * @brief INIT state — detect prefix bytes or dispatch normal code.
 * @param code     Raw scancode byte received from the ring buffer.
 * @param config   Pointer to the active scancode configuration for the detected keyboard.
 * @return void
 * @note Main loop only.
 */
static void handle_state_init(uint8_t code, const scancode_config_t* config) {
    if (code == SC_ESCAPE_E0 && config->has_e0_prefix) {
        state = E0;
    } else if (code == SC_ESCAPE_E1 && config->has_e1_prefix) {
        state = E1;
    } else if (code == SC_ESCAPE_F0 &&
               (config->set == SCANCODE_SET2 || config->set == SCANCODE_SET3)) {
        state = F0;
    } else {
        process_normal_code(code, config);
        state = INIT;
    }
}

/**
 * @brief E0 state — E0-prefixed make (Set 1/2) or start of E0 F0 break (Set 2).
 *
 * Set 1 E0-prefixed codes carry make/break in the high bit (same as unescaped
 * codes); Set 2 E0-prefixed codes are always make here — the break path goes
 * via E0_F0.  Fake shifts are silently discarded.
 *
 * @param code     Raw scancode byte received from the ring buffer.
 * @param config   Pointer to the active scancode configuration for the detected keyboard.
 * @return void
 * @note Main loop only.
 */
static void handle_state_e0(uint8_t code, const scancode_config_t* config) {
    if (is_fake_shift(code, config)) {
        state = INIT;
        return;
    }
    if (code == SC_ESCAPE_F0 && config->set == SCANCODE_SET2) {
        state = E0_F0;
        return;
    }

    // For Set 1, the high bit distinguishes break from make; Set 2 is always make.
    bool    is_set1   = (config->set == SCANCODE_SET1);
    bool    is_make   = !is_set1 || (code < SC1_BREAK_BIT);
    uint8_t base_code = is_make ? code : (code & SC1_MAKE_MASK);

    state = INIT;
    if (base_code < SC1_BREAK_BIT) {
        uint8_t translated = translate_e0_code(base_code, config);
        if (translated) {
            handle_keyboard_report(translated, is_make);
        }
    } else {
        LOG_DEBUG("!E0! (0x%02X)\n", code);
    }
}

/**
 * @brief E0_F0 state — byte following E0 F0 (Set 2 E0-prefixed break).
 *
 * Fake shifts that arrive here (e.g. E0 F0 12) are discarded.
 *
 * @param code     Raw scancode byte received from the ring buffer.
 * @param config   Pointer to the active scancode configuration for the detected keyboard.
 * @return void
 * @note Main loop only.
 */
static void handle_state_e0_f0(uint8_t code, const scancode_config_t* config) {
    state = INIT;
    if (is_fake_shift(code, config)) {
        return;
    }
    if (code < SC1_BREAK_BIT) {
        uint8_t translated = translate_e0_code(code, config);
        if (translated) {
            handle_keyboard_report(translated, false);
        }
    } else {
        LOG_DEBUG("!E0_F0! (0x%02X)\n", code);
    }
}

/**
 * @brief E1 state — first data byte of the E1 Pause/Break multi-byte sequence.
 *
 * Set 1 Pause make:  E1 1D 45   → expects 1D here, then 45 in E1_1D.
 * Set 1 Pause break: E1 9D C5   → expects 9D here, then C5 in E1_9D.
 * Set 2 Pause make:  E1 14 77   → expects 14 here, then 77 in E1_14.
 * Set 2 Pause break: E1 F0 14 F0 77 → expects F0 here, then routes via E1_F0.
 *
 * @param code     Raw scancode byte received from the ring buffer.
 * @param config   Pointer to the active scancode configuration for the detected keyboard.
 * @return void
 * @note Main loop only.
 */
static void handle_state_e1(uint8_t code, const scancode_config_t* config) {
    if (config->set == SCANCODE_SET1) {
        if (code == SC1_E1_CTRL_MAKE) {
            state = E1_1D;
        } else if (code == SC1_E1_CTRL_BREAK) {
            state = E1_9D;
        } else {
            state = INIT;
            LOG_DEBUG("!E1! (0x%02X)\n", code);
        }
    } else if (config->set == SCANCODE_SET2) {
        if (code == SC2_CTRL) {
            state = E1_14;
        } else if (code == SC_ESCAPE_F0) {
            state = E1_F0;
        } else {
            state = INIT;
            LOG_DEBUG("!E1! (0x%02X)\n", code);
        }
    } else {
        state = INIT;
    }
}

/**
 * @brief E1_1D state — final byte of Set 1 Pause make sequence (E1 1D [45]).
 * @param code     Raw scancode byte received from the ring buffer.
 * @param config   Pointer to the active scancode configuration (unused; suppressed via (void)).
 * @return void
 * @note Main loop only.
 */
static void handle_state_e1_1d(uint8_t code, const scancode_config_t* config) {
    (void)config;
    if (code == SC1_NUMLOCK_MAKE) {
        handle_keyboard_report(IFACE_PAUSE, true);
    } else {
        LOG_DEBUG("!E1_1D! (0x%02X)\n", code);
    }
    state = INIT;
}

/**
 * @brief E1_9D state — final byte of Set 1 Pause break sequence (E1 9D [C5]).
 * @param code     Raw scancode byte received from the ring buffer.
 * @param config   Pointer to the active scancode configuration (unused; suppressed via (void)).
 * @return void
 * @note Main loop only.
 */
static void handle_state_e1_9d(uint8_t code, const scancode_config_t* config) {
    (void)config;
    if (code == SC1_NUMLOCK_BREAK) {
        handle_keyboard_report(IFACE_PAUSE, false);
    } else {
        LOG_DEBUG("!E1_9D! (0x%02X)\n", code);
    }
    state = INIT;
}

/**
 * @brief E1_14 state — final byte of Set 2 Pause make sequence (E1 14 [77]).
 *
 * Routes through the E0 translation table: 0x77 maps to IFACE_PAUSE (0x48).
 *
 * @param code     Raw scancode byte received from the ring buffer.
 * @param config   Pointer to the active scancode configuration for the detected keyboard.
 * @return void
 * @note Main loop only.
 */
static void handle_state_e1_14(uint8_t code, const scancode_config_t* config) {
    if (code == SC2_NUMLOCK) {
        uint8_t translated = translate_e0_code(code, config);
        if (translated) {
            handle_keyboard_report(translated, true);
        }
    } else {
        LOG_DEBUG("!E1_14! (0x%02X)\n", code);
    }
    state = INIT;
}

/**
 * @brief E1_F0 state — second byte of Set 2 Pause break sequence (E1 F0 [14] F0 77).
 * @param code     Raw scancode byte received from the ring buffer.
 * @param config   Pointer to the active scancode configuration (unused; suppressed via (void)).
 * @return void
 * @note Main loop only.
 */
static void handle_state_e1_f0(uint8_t code, const scancode_config_t* config) {
    (void)config;
    if (code == SC2_CTRL) {
        state = E1_F0_14;
    } else {
        state = INIT;
        LOG_DEBUG("!E1_F0! (0x%02X)\n", code);
    }
}

/**
 * @brief E1_F0_14 state — third byte of Set 2 Pause break sequence (E1 F0 14 [F0] 77).
 * @param code     Raw scancode byte received from the ring buffer.
 * @param config   Pointer to the active scancode configuration (unused; suppressed via (void)).
 * @return void
 * @note Main loop only.
 */
static void handle_state_e1_f0_14(uint8_t code, const scancode_config_t* config) {
    (void)config;
    if (code == SC_ESCAPE_F0) {
        state = E1_F0_14_F0;
    } else {
        state = INIT;
        LOG_DEBUG("!E1_F0_14! (0x%02X)\n", code);
    }
}

/**
 * @brief E1_F0_14_F0 state — final byte of Set 2 Pause break sequence (E1 F0 14 F0 [77]).
 *
 * Routes through the E0 translation table: 0x77 maps to IFACE_PAUSE (0x48).
 *
 * @param code     Raw scancode byte received from the ring buffer.
 * @param config   Pointer to the active scancode configuration for the detected keyboard.
 * @return void
 * @note Main loop only.
 */
static void handle_state_e1_f0_14_f0(uint8_t code, const scancode_config_t* config) {
    if (code == SC2_NUMLOCK) {
        uint8_t translated = translate_e0_code(code, config);
        if (translated) {
            handle_keyboard_report(translated, false);
        }
    } else {
        LOG_DEBUG("!E1_F0_14_F0! (0x%02X)\n", code);
    }
    state = INIT;
}

// --- Public Functions ---

void process_scancode(uint8_t code, const scancode_config_t* config) {
    FLOW_STEP(code);
    switch (state) {
        case INIT:
            handle_state_init(code, config);
            break;
        case F0:
            process_break_code(code, config);
            state = INIT;
            break;
        case E0:
            handle_state_e0(code, config);
            break;
        case E0_F0:
            handle_state_e0_f0(code, config);
            break;
        case E1:
            handle_state_e1(code, config);
            break;
        case E1_1D:
            handle_state_e1_1d(code, config);
            break;
        case E1_9D:
            handle_state_e1_9d(code, config);
            break;
        case E1_14:
            handle_state_e1_14(code, config);
            break;
        case E1_F0:
            handle_state_e1_f0(code, config);
            break;
        case E1_F0_14:
            handle_state_e1_f0_14(code, config);
            break;
        case E1_F0_14_F0:
            handle_state_e1_f0_14_f0(code, config);
            break;
        default:
            state = INIT;
            break;
    }
}

void reset_scancode_state(void) {
    state = INIT;
}
