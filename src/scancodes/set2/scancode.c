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
#define SC_ESCAPE_F0 0xF0U /**< F0 prefix — break code indicator */

/* Set 2 fake shift bytes (E0-prefixed, silently discarded) */
#define SC2_FAKE_LSHIFT 0x12U /**< Spurious Left-Shift (with E0 prefix) */
#define SC2_FAKE_RSHIFT 0x59U /**< Spurious Right-Shift (with E0 prefix) */

/* Set 2 E1 Pause/Break sequence bytes */
#define SC2_CTRL                                                                       \
    0x14U                 /**< Left-Control make; also Right-Control when E0-prefixed. \
                           *   First data byte of the E1 Pause make/break sequences. */
#define SC2_NUMLOCK 0x77U /**< E1 Pause make/break tail — Num Lock code */

/* Special out-of-range Set 2 scancodes */
#define SC2_SPECIAL_F7     0x83U /**< F7 key scancode (outside normal 0x01–0x7E range) */
#define SC2_SPECIAL_SYSREQ 0x84U /**< SysReq / Alt+Print Screen scancode */

/* Protocol response bytes (handled by protocol layer; listed here for clarity) */
#define SC2_BAT_OK   0xAAU /**< Basic Assurance Test OK — self-test passed */
#define SC2_BAT_FAIL 0xFCU /**< BAT failure / mouse transmit error */

/* Interface code passed to handle_keyboard_report() for SysReq */
#define IFACE_KP_PLUS 0x7FU /**< Interface code: Keypad Plus (used for SysReq output) */

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

// ============================================================================
// State Machine
// ============================================================================

/**
 * @brief Unified State Machine States
 *
 * - Set 2: INIT, F0, E0, E0_F0, E1, E1_14, E1_F0, E1_F0_14, E1_F0_14_F0 (9 states)
 */
typedef enum {
    INIT,        // Initial state / ready for next scancode
    F0,          // Break prefix received
    E0,          // E0 prefix received
    E0_F0,       // E0 F0 sequence (break)
    E1,          // E1 prefix received
    E1_14,       // E1 14 sequence (Pause make)
    E1_F0,       // E1 F0 sequence (Pause break)
    E1_F0_14,    // E1 F0 14 sequence (Pause break)
    E1_F0_14_F0  // E1 F0 14 F0 sequence (Pause break)
} scancode_state_t;

static scancode_state_t state = INIT;

// ============================================================================
// Per-State Handlers
//
// Each function handles exactly one state.  process_scancode() is a thin
// dispatcher; keeping the logic split here makes each state independently
// readable and holds cognitive complexity well within bounds.
// ============================================================================

/**
 * @brief INIT state — detect prefix bytes, special codes, or dispatch make code.
 * @param code   Input scancode byte received from the ring buffer.
 * @return void
 * @note Main loop only.
 */
static void handle_state_init(uint8_t code) {
    switch (code) {
        case SC_ESCAPE_E0:
            state = E0;
            break;
        case SC_ESCAPE_F0:
            state = F0;
            break;
        case SC_ESCAPE_E1:
            state = E1;
            break;
        case SC2_SPECIAL_F7:
            handle_keyboard_report(0x02, true);
            break;
        case SC2_SPECIAL_SYSREQ:
            handle_keyboard_report(IFACE_KP_PLUS, true);
            break;
        case SC2_BAT_OK:    // Self-test passed — handled by protocol layer; ignored here
        case SC2_BAT_FAIL:  // Self-test failed — same
        default:
            state = INIT;
            if (code < 0x80) {
                handle_keyboard_report(code, true);
            } else {
                LOG_DEBUG("!INIT! (0x%02X)\n", code);
            }
    }
}

/**
 * @brief F0 state — byte following F0 break prefix.
 * @param code   Input scancode byte received from the ring buffer.
 * @return void
 * @note Main loop only.
 */
static void handle_state_f0(uint8_t code) {
    state = INIT;
    switch (code) {
        case SC2_SPECIAL_F7:
            handle_keyboard_report(0x02, false);
            break;
        case SC2_SPECIAL_SYSREQ:
            handle_keyboard_report(IFACE_KP_PLUS, false);
            break;
        default:
            if (code < 0x80) {
                handle_keyboard_report(code, false);
            } else {
                LOG_DEBUG("!F0! (0x%02X)\n", code);
            }
    }
}

/**
 * @brief E0 state — byte following E0 prefix: make code, fake shift, or start of E0 F0.
 * @param code   Input scancode byte received from the ring buffer.
 * @return void
 * @note Main loop only.
 */
static void handle_state_e0(uint8_t code) {
    switch (code) {
        case SC2_FAKE_LSHIFT:
        case SC2_FAKE_RSHIFT:
            state = INIT;
            break;
        case SC_ESCAPE_F0:
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
}

/**
 * @brief E0_F0 state — byte following E0 F0 (E0-prefixed break code).
 *
 * Fake shifts arriving via E0 F0 12 / E0 F0 59 are silently discarded.
 *
 * @param code   Input scancode byte received from the ring buffer.
 * @return void
 * @note Main loop only.
 */
static void handle_state_e0_f0(uint8_t code) {
    state = INIT;
    switch (code) {
        case SC2_FAKE_LSHIFT:
        case SC2_FAKE_RSHIFT:
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
}

/**
 * @brief E1 state — first data byte of the E1 Pause/Break multi-byte sequence.
 *
 * Pause make:  E1 [14] 77
 * Pause break: E1 [F0] 14 F0 77
 *
 * @param code   Input scancode byte received from the ring buffer.
 * @return void
 * @note Main loop only.
 */
static void handle_state_e1(uint8_t code) {
    switch (code) {
        case SC2_CTRL:
            state = E1_14;
            break;
        case SC_ESCAPE_F0:
            state = E1_F0;
            break;
        default:
            state = INIT;
            LOG_DEBUG("!E1! (0x%02X)\n", code);
    }
}

/**
 * @brief E1_14 state — final byte of Pause make sequence (E1 14 [77]).
 *
 * Routes through the E0 translation table: 0x77 maps to IFACE_PAUSE (0x48).
 *
 * @param code   Input scancode byte received from the ring buffer.
 * @return void
 * @note Main loop only.
 */
static void handle_state_e1_14(uint8_t code) {
    state = INIT;
    if (code == SC2_NUMLOCK) {
        uint8_t translated = switch_e0_code(code);
        if (translated) {
            handle_keyboard_report(translated, true);
        }
    } else {
        LOG_DEBUG("!E1_14! (0x%02X)\n", code);
    }
}

/**
 * @brief E1_F0 state — second byte of Pause break sequence (E1 F0 [14] F0 77).
 * @param code   Input scancode byte received from the ring buffer.
 * @return void
 * @note Main loop only.
 */
static void handle_state_e1_f0(uint8_t code) {
    if (code == SC2_CTRL) {
        state = E1_F0_14;
    } else {
        state = INIT;
        LOG_DEBUG("!E1_F0! (0x%02X)\n", code);
    }
}

/**
 * @brief E1_F0_14 state — third byte of Pause break sequence (E1 F0 14 [F0] 77).
 * @param code   Input scancode byte received from the ring buffer.
 * @return void
 * @note Main loop only.
 */
static void handle_state_e1_f0_14(uint8_t code) {
    if (code == SC_ESCAPE_F0) {
        state = E1_F0_14_F0;
    } else {
        state = INIT;
        LOG_DEBUG("!E1_F0_14! (0x%02X)\n", code);
    }
}

/**
 * @brief E1_F0_14_F0 state — final byte of Pause break sequence (E1 F0 14 F0 [77]).
 *
 * Routes through the E0 translation table: 0x77 maps to IFACE_PAUSE (0x48).
 *
 * @param code   Input scancode byte received from the ring buffer.
 * @return void
 * @note Main loop only.
 */
static void handle_state_e1_f0_14_f0(uint8_t code) {
    state = INIT;
    if (code == SC2_NUMLOCK) {
        uint8_t translated = switch_e0_code(code);
        if (translated) {
            handle_keyboard_report(translated, false);
        }
    } else {
        LOG_DEBUG("!E1_F0_14_F0! (0x%02X)\n", code);
    }
}

// ============================================================================
// Main Processor
// ============================================================================

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
 * @param code The keycode to process.
 *
 * @note Main loop only.
 * @note handle_keyboard_report() translates scan codes to HID keycodes via keymap lookup.
 */
void process_scancode(uint8_t code) {
    switch (state) {
        case INIT:
            handle_state_init(code);
            break;
        case F0:
            handle_state_f0(code);
            break;
        case E0:
            handle_state_e0(code);
            break;
        case E0_F0:
            handle_state_e0_f0(code);
            break;
        case E1:
            handle_state_e1(code);
            break;
        case E1_14:
            handle_state_e1_14(code);
            break;
        case E1_F0:
            handle_state_e1_f0(code);
            break;
        case E1_F0_14:
            handle_state_e1_f0_14(code);
            break;
        case E1_F0_14_F0:
            handle_state_e1_f0_14_f0(code);
            break;
        default:
            state = INIT;
            break;
    }
}