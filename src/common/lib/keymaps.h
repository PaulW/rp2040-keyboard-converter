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
 * @file keymaps.h
 * @brief Scancode-to-HID keycode translation with multi-layer and shift-override support.
 *
 * This module provides the keymap lookup and layer management interface used by the HID
 * report pipeline. Each keyboard defines a `keymap_map` array in its `keyboard.c` file;
 * this module provides the resolution logic.
 *
 * Keymap Layout:
 * - Grid: KEYMAP_ROWS (8) × KEYMAP_COLS (16) = 128 addressable key positions
 * - Position byte encoding: upper nibble = row (0–7), lower nibble = column (0–15)
 * - Up to KEYMAP_MAX_LAYERS (5) independent layers per keyboard: base Layer 0
 *   plus 4 switchable layers (1–4)
 * - Layer 0 is always active; higher layers overlay lower ones
 *
 * Layer System:
 * - MO(n):  Momentary — active whilst held, releases on key-up
 * - TG(n):  Toggle — latches on/off with each press (state persists via config_storage)
 * - TO(n):  Switch — permanently activates layer n (state persists via config_storage)
 * - OSL(n): One-shot — active for the next key press only
 * - KC_TRANSPARENT (0xD1): Falls through to the next active layer below
 *
 * Shift-Override:
 * Keyboards with non-standard shift legends can define `keymap_shift_override_layers[]`
 * in `keyboard.c`. Each entry is a SHIFT_OVERRIDE_ARRAY_SIZE (256) byte array indexed by
 * KC_* code. A non-zero value replaces the shifted output; SUPPRESS_SHIFT (0x80) removes
 * the shift modifier from the HID report. See hid_keycodes.h for the flag definition.
 *
 * Usage:
 * Protocol scancode processors call `keymap_get_key_val()` with a normalised position
 * byte and make/break flag to retrieve the resolved HID keycode.
 *
 * @note All public functions are main loop only.
 */

#ifndef KEYMAPS_H
#define KEYMAPS_H

#include <stdbool.h>
#include <stdint.h>

#define KEYMAP_ROWS       8
#define KEYMAP_COLS       16
#define KEYMAP_MAX_LAYERS 5  // Layer 0 (base) + 4 switchable layers (1–4)

// Shift-Override array size: Each layer's shift-override array must be exactly this size
#define SHIFT_OVERRIDE_ARRAY_SIZE 256

// Shift-Override flag: Set bit 7 in keymap_shift_override_layers[] to suppress shift modifier
#define SUPPRESS_SHIFT 0x80

/**
 * @brief Retrieves the key value at the specified position in the keymap.
 *
 * Main keymap lookup function. Handles:
 * - Layer switching operations (MO/TG/TO/OSL)
 * - Transparent key fallthrough
 * - Per-layer shift-override (if keyboard defines keymap_shift_override_layers)
 * - One-shot layer consumption
 *
 * @param pos            The position of the key in the keymap (upper nibble = row, lower nibble =
 * col).
 * @param make           True if key pressed, false if released.
 * @param suppress_shift Output parameter set to true if shift should be suppressed for this key.
 *                       Used by shift-override system when remapping to the final character.
 *
 * @return The HID keycode to send, or KC_NO if consumed by layer operation.
 *
 * @note Main loop only.
 * @note Non-blocking.
 */
uint8_t keymap_get_key_val(uint8_t pos, bool make, bool* suppress_shift);

extern const uint8_t keymap_map[][KEYMAP_ROWS][KEYMAP_COLS];

// Optional: Per-layer shift-override arrays (weak symbol, keyboards can override)
// Array of pointers to SHIFT_OVERRIDE_ARRAY_SIZE-byte shift-override arrays, one per layer
// NULL entries mean no shift-override for that layer
// Note: Runtime validation checks for invalid layer access and array bounds
extern const uint8_t* const keymap_shift_override_layers[KEYMAP_MAX_LAYERS] __attribute__((weak));

#endif /* KEYMAPS_H */
