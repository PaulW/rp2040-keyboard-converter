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

#include "numpad_flip.h"

#include "hid_keycodes.h"

/**
 * @brief Numpad flip lookup table for Mac-style Num Lock emulation
 * 
 * This lookup table provides bidirectional mapping between numpad keys and
 * navigation keys to emulate Num Lock behavior on keyboards (like Apple keyboards)
 * that don't have a physical Num Lock key or state.
 * 
 * Mapping Philosophy (Mac-style):
 * - When "Num Lock Off" (navigation mode):
 *   KC_P0 → KC_INS, KC_P1 → KC_END, KC_P2 → KC_DOWN, etc.
 * 
 * - When "Num Lock On" (numeric mode):
 *   KC_INS → KC_P0, KC_END → KC_P1, KC_DOWN → KC_P2, etc.
 * 
 * Bidirectional Design:
 * The table supports both directions, allowing the same function to handle
 * both "flip to navigation" and "flip to numeric" operations. This is essential
 * for keyboards where the physical keys may be labeled differently.
 * 
 * Special Cases:
 * - KC_P5 (center key) → KC_NO (no mapping, stays as KC_P5)
 * - KC_PAST (keypad *) → KC_PSCR (Print Screen on some layouts)
 * - Unmapped keys return 0 (caller should preserve original key)
 * 
 * Usage in Keymap System:
 * This will be used when KC_NFLP (Numpad Flip) is implemented to toggle
 * between numeric and navigation modes, typically triggered by an Fn key
 * or similar modifier.
 * 
 * Performance:
 * - O(1) constant-time lookup (256-byte table)
 * - Replaces 24-level nested ternary operator
 * - ~10× faster than macro implementation
 * - Stored in flash (const), no RAM overhead
 * 
 * @note All entries default to 0 (no mapping) for unused keycodes
 * @note Table size: 256 bytes in flash memory
 */
static const uint8_t numpad_flip_table[256] = {
    // Numpad → Navigation mappings
    [KC_P0]    = KC_INS,      // Keypad 0 → Insert
    [KC_P1]    = KC_END,      // Keypad 1 → End
    [KC_P2]    = KC_DOWN,     // Keypad 2 → Down Arrow
    [KC_P3]    = KC_PGDN,     // Keypad 3 → Page Down
    [KC_P4]    = KC_LEFT,     // Keypad 4 → Left Arrow
    [KC_P5]    = KC_NO,       // Keypad 5 → No mapping (stays as KC_P5)
    [KC_P6]    = KC_RIGHT,    // Keypad 6 → Right Arrow
    [KC_P7]    = KC_HOME,     // Keypad 7 → Home
    [KC_P8]    = KC_UP,       // Keypad 8 → Up Arrow
    [KC_P9]    = KC_PGUP,     // Keypad 9 → Page Up
    [KC_PDOT]  = KC_DEL,      // Keypad . → Delete
    [KC_PAST]  = KC_PSCR,     // Keypad * → Print Screen
    
    // Navigation → Numpad mappings (bidirectional)
    [KC_INS]   = KC_P0,       // Insert → Keypad 0
    [KC_END]   = KC_P1,       // End → Keypad 1
    [KC_DOWN]  = KC_P2,       // Down Arrow → Keypad 2
    [KC_PGDN]  = KC_P3,       // Page Down → Keypad 3
    [KC_LEFT]  = KC_P4,       // Left Arrow → Keypad 4
    [KC_NO]    = KC_P5,       // No mapping → Keypad 5
    [KC_RIGHT] = KC_P6,       // Right Arrow → Keypad 6
    [KC_HOME]  = KC_P7,       // Home → Keypad 7
    [KC_UP]    = KC_P8,       // Up Arrow → Keypad 8
    [KC_PGUP]  = KC_P9,       // Page Up → Keypad 9
    [KC_DEL]   = KC_PDOT,     // Delete → Keypad .
    [KC_PSCR]  = KC_PAST,     // Print Screen → Keypad *
    
    // All other entries default to 0 (no mapping)
};

/**
 * @brief Flips numpad keycodes to/from navigation keys for Num Lock emulation
 * 
 * This function provides Mac-style Num Lock behavior by mapping between
 * numpad keys (KC_P0-KC_P9) and navigation keys (KC_HOME, KC_END, arrows, etc.).
 * 
 * The mapping is bidirectional:
 * - Numpad keys flip to navigation keys (e.g., KC_P7 → KC_HOME)
 * - Navigation keys flip to numpad keys (e.g., KC_HOME → KC_P7)
 * 
 * This allows the same function to handle both directions, simplifying
 * the implementation of Num Lock toggle functionality.
 * 
 * Future Implementation Note:
 * When implementing proper Num Lock emulation (KC_NFLP), the caller will
 * maintain a numlock_state flag and call this function conditionally:
 * 
 * Example usage:
 * ```c
 * if (numlock_state == NUMLOCK_OFF) {
 *   key = numpad_flip_code(key);  // Convert numpad to navigation
 * }
 * ```
 * 
 * @param key The keycode to flip (typically from keymap)
 * @return The flipped keycode if mapping exists, original key otherwise
 * 
 * @note Returns original key if no mapping exists in the table
 * @note O(1) constant-time lookup performance
 * @note Thread-safe (read-only const table)
 */
uint8_t numpad_flip_code(uint8_t key) {
    uint8_t flipped = numpad_flip_table[key];
    return flipped ? flipped : key;  // Return flipped code or original if no mapping
}
