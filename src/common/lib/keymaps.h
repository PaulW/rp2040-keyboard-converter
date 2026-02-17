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

#ifndef KEYMAPS_H
#define KEYMAPS_H

#include <stdbool.h>
#include <stdint.h>

#define KEYMAP_ROWS       8
#define KEYMAP_COLS       16
#define KEYMAP_MAX_LAYERS 8

// Shift-override flag: Set bit 7 in keymap_shift_override_layers[] to suppress shift modifier
#define SUPPRESS_SHIFT 0x80

/**
 * @brief Retrieve key value at specified position in keymap
 *
 * Main keymap lookup function. Handles layer fallthrough, layer modifiers,
 * and per-layer shift-override. Coordinates with layer system for proper
 * key resolution across active layers.
 *
 * @param pos            Key position (upper nibble = row, lower nibble = col)
 * @param make           true if key pressed, false if released
 * @param suppress_shift Output parameter for shift suppression (can be NULL)
 * @return HID keycode to send, or KC_NO if consumed by layer operation
 */
uint8_t keymap_get_key_val(uint8_t pos, bool make, bool* suppress_shift);

extern const uint8_t keymap_map[][KEYMAP_ROWS][KEYMAP_COLS];

// Optional: Per-layer shift-override arrays (weak symbol, keyboards can override)
// Array of pointers to 256-byte shift-override arrays, one per layer
// NULL entries mean no shift-override for that layer
// Note: Runtime validation checks for invalid layer access
extern const uint8_t* const keymap_shift_override_layers[KEYMAP_MAX_LAYERS] __attribute__((weak));

#endif /* KEYMAPS_H */
