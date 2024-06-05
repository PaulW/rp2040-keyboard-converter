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
 * @brief Retrieves the key value at the specified position in the keymap.
 * This function takes a position value and a boolean indicating whether the key is being pressed or
 * released. It calculates the row and column values from the position and searches the keymap for
 * the corresponding key code.
 *
 * If the key code is KC_FN, it sets the action_key_pressed flag and returns KC_NO. Otherwise, if
 * the action_key_pressed flag is set, it checks if there is an action key code at the specified
 * position in the action key map. If there is, it updates the key code with the action key code.
 *
 * If the key code is KC_NFLP, it searches the keymap again to get the flip key code and converts it
 * to the corresponding numpad flip code. Finally, it returns the key code.
 *
 * @param pos  The position of the key in the keymap.
 * @param make A boolean indicating whether the key is being pressed (true) or released (false).
 *
 * @return The key code at the specified position in the keymap.
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
      key_code = NUMPAD_FLIP_CODE(flip_key_code);
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