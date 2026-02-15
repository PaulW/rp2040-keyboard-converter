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

#ifndef KEYMAPS_H
#define KEYMAPS_H

#include <stdbool.h>
#include <stdint.h>

#define KEYMAP_ROWS 8
#define KEYMAP_COLS 16
#define KEYMAP_MAX_LAYERS 8

// Layer state management
typedef struct {
  uint8_t layer_state;        // Bitmap of active layers (bit 0 = layer 0, etc.)
  uint8_t momentary_keys[3];  // Track which layer keys are held down (MO only)
  uint8_t oneshot_layer;      // One-shot layer (0 = none, 1-8 = active layer)
  bool oneshot_active;        // True if one-shot layer is waiting for next key
} layer_state_t;

uint8_t keymap_get_key_val(uint8_t pos, bool make);
void keymap_reset_layers(void);
uint8_t keymap_get_active_layer(void);

extern const uint8_t keymap_map[][KEYMAP_ROWS][KEYMAP_COLS];

// Optional: Shift-override sparse array (weak symbol, keyboards can override)
extern const uint8_t keymap_shifted_keycode[256] __attribute__((weak));

#endif /* KEYMAPS_H */
