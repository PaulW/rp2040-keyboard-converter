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

#include "keymaps.h"

#include <stdint.h>
#include <stdio.h>

#include "hid_keycodes.h"
#include "numpad_flip.h"

static int keymap_layer = 0; /* TO-DO Add full Layer Switching Support */
static bool action_key_pressed = false;

/**
 * @brief Searches for the key code in the keymap based on the specified row and column.
 * This function searches for the key code in the keymap based on the specified row and column. It
 * first checks the current layer, and if the key code is KC_TRNS (transparent), it searches through
 * the previous layers until a non-transparent key code is found. If no non-transparent key code is
 * found, it returns KC_NO (no key).
 *
 * @param row The row index in the keymap.
 * @param col The column index in the keymap.
 *
 * @return The key code found in the keymap.
 */
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

/**
 * Resolve the key code for a key position by performing layer lookup and applying action-key
 * substitution and numpad-flip conversion as required.
 *
 * When the resolved key is KC_FN this sets the global action_key_pressed flag to the value of
 * `make` and returns KC_NO (no key emitted). If action_key_pressed is set and an action key code
 * exists in keymap_actions[0] for the same position (not KC_TRNS), that action code replaces the
 * resolved key. If the resulting key is KC_NFLP, the flip key is looked up and converted via
 * numpad_flip_code().
 *
 * @param pos  Encoded key position: high nibble = row, low nibble = column.
 * @param make true if the key is being pressed, false if released.
 * @return The resolved key code to emit, or KC_NO when no key should be emitted (e.g., for KC_FN).
 */
uint8_t keymap_get_key_val(uint8_t pos, bool make) {
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
      key_code = numpad_flip_code(flip_key_code);
    }

    return key_code;
  }
}

/**
 * @brief Checks if the action key is currently pressed.
 * This function returns a boolean value indicating whether the action key is currently pressed or
 * not.  This ensures we only expose this value read-only to the rest of the code.
 *
 * @return true if the action key is pressed, false otherwise.
 */
bool keymap_is_action_key_pressed(void) { return action_key_pressed; }