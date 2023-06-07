/*
 * This file is part of Model-F 5170 Converter.
 *
 * Copyright 2023 Paul Bramhall (paulwamp@gmail.com)
 *
 * Model-F 5170 Converter is free software: you can redistribute it
 * and/or modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * Model-F 5170 Converter is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Model-F 5170 Converter.
 * If not, see <https://www.gnu.org/licenses/>.
 */

#include "keymaps.h"

#include <stdint.h>
#include <stdio.h>

#include "hid_keycodes.h"

static int keymap_layer = 0; /* TO-DO Add full Layer Switching Support */

static uint8_t keymap_search_layers(uint8_t row, uint8_t col) {
  uint8_t key_code = keymap_map[keymap_layer][row][col];

  if (keymap_layer > 0 && key_code == KC_TRNS) {
    uint8_t layer_key_code;
    for (int i = keymap_layer; i >= 0; i--) {
      layer_key_code = keymap_map[i][row][col];
      if (layer_key_code != KC_TRNS) {
        key_code = layer_key_code;
        break;
      }
    }
    if (key_code == KC_TRNS) {
      /* This shouldn't happen! */
      printf("[ERR] KC_TRNS Detected as far as Base Layer!\n");
      key_code = KC_NO;
    }
  }
  return key_code;
}

uint8_t keymap_get_key_val(uint8_t pos, bool make) {
  static bool action_key_pressed = false;
  const uint8_t row = (pos >> 4) & 0x0F;
  const uint8_t col = pos & 0x0F;
  uint8_t key_code = keymap_search_layers(row, col);

  if (key_code == KC_FN) {
    action_key_pressed = make;
    return KC_NO;
  } else {
    if (action_key_pressed) {
      /* We need to process an Action Key event from a seperate map */
      const uint8_t action_key_code = keymap_actions[0][row][col];
      if (action_key_code != KC_TRNS) {
        key_code = action_key_code;
      }
    }

    if (key_code == KC_NFLP) {
      const uint8_t flip_key_code = keymap_search_layers(row, col);
      key_code = NUMPAD_FLIP_CODE(flip_key_code);
    }

    return key_code;
  }
}