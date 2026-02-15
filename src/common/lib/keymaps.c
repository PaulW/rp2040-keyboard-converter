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

#include "hid_interface.h"
#include "hid_keycodes.h"
#include "log.h"
#include "numpad_flip.h"

// Layer state tracking
static layer_state_t layer_state = {
  .layer_state = 0x01,           // Bit 0 set = Layer 0 (base) active
  .momentary_keys = {0, 0, 0},   // No MO keys held
  .oneshot_layer = 0,            // No one-shot layer
  .oneshot_active = false        // One-shot not active
};

/**
 * @brief Resets layer state to base layer (layer 0)
 * 
 * Called during initialization and error recovery.
 * Clears all layer activations and returns to base layer.
 */
void keymap_reset_layers(void) {
  layer_state.layer_state = 0x01;     // Only layer 0 active
  layer_state.momentary_keys[0] = 0;
  layer_state.momentary_keys[1] = 0;
  layer_state.momentary_keys[2] = 0;
  layer_state.oneshot_layer = 0;
  layer_state.oneshot_active = false;
}

/**
 * @brief Gets the highest active layer number
 * 
 * Searches layer state bitmap from highest to lowest.
 * One-shot layer takes priority if active.
 * 
 * @return Active layer number (0-7)
 */
uint8_t keymap_get_active_layer(void) {
  // Check one-shot layer first (highest priority)
  if (layer_state.oneshot_active && layer_state.oneshot_layer > 0) {
    return layer_state.oneshot_layer - 1;  // oneshot_layer is 1-indexed
  }
  
  // Search bitmap from highest to lowest
  for (int8_t i = KEYMAP_MAX_LAYERS - 1; i >= 0; i--) {
    if (layer_state.layer_state & (1 << i)) {
      return i;
    }
  }
  
  // Fallback to layer 0 (should never reach here due to init)
  return 0;
}

/**
 * @brief Searches for the key code in the keymap based on the specified row and column.
 * 
 * Starts from the active layer and falls through transparent keys to lower layers.
 * Handles KC_TRNS (transparent) by searching down through layer stack.
 *
 * @param row The row index in the keymap.
 * @param col The column index in the keymap.
 *
 * @return The key code found in the keymap.
 */
static uint8_t keymap_search_layers(uint8_t row, uint8_t col) {
  uint8_t active_layer = keymap_get_active_layer();
  uint8_t key_code = keymap_map[active_layer][row][col];

  // Handle transparent keys - fall through to lower layers
  if (active_layer > 0 && key_code == KC_TRNS) {
    for (int8_t i = active_layer - 1; i >= 0; i--) {
      uint8_t layer_key_code = keymap_map[i][row][col];
      if (layer_key_code != KC_TRNS) {
        key_code = layer_key_code;
        break;
      }
    }
    if (key_code == KC_TRNS) {
      /* This shouldn't happen! Base layer should never have KC_TRNS */
      LOG_ERROR("KC_TRNS detected in base layer at [%d][%d]!\n", row, col);
      key_code = KC_NO;
    }
  }
  
  return key_code;
}

/**
 * @brief Processes layer switching operations
 * 
 * Handles MO (momentary), TG (toggle), TO (switch to), and OSL (one-shot) operations.
 * Updates layer_state based on keycode and make/break.
 * 
 * @param code The layer operation keycode (0xF0-0xFF)
 * @param make True if key pressed, false if released
 */
static void process_layer_key(uint8_t code, bool make) {
  uint8_t operation = GET_LAYER_OPERATION(code);  // 0=MO, 1=TG, 2=TO, 3=OSL
  uint8_t target_layer = GET_LAYER_TARGET(code);  // 1-4
  
  switch (operation) {
    case 0:  // MO (Momentary) - hold to activate
      if (make) {
        layer_state.layer_state |= (1 << target_layer);
        // Track which MO key is held (for release detection)
        if (target_layer <= 3) {
          layer_state.momentary_keys[target_layer - 1] = code;
        }
      } else {
        // Release momentary layer only if this specific MO key is released
        if (target_layer <= 3 && layer_state.momentary_keys[target_layer - 1] == code) {
          layer_state.layer_state &= ~(1 << target_layer);
          layer_state.momentary_keys[target_layer - 1] = 0;
        }
      }
      break;
      
    case 1:  // TG (Toggle) - press to toggle on/off
      if (make) {  // Only act on key press, ignore release
        if (layer_state.layer_state & (1 << target_layer)) {
          layer_state.layer_state &= ~(1 << target_layer);  // Turn off
        } else {
          layer_state.layer_state |= (1 << target_layer);   // Turn on
        }
      }
      break;
      
    case 2:  // TO (Switch to) - permanently switch to layer
      if (make) {  // Only act on key press
        // Clear all layers except base (layer 0)
        layer_state.layer_state = 0x01;
        // Activate target layer
        if (target_layer > 0) {
          layer_state.layer_state |= (1 << target_layer);
        }
        // Clear momentary tracking
        layer_state.momentary_keys[0] = 0;
        layer_state.momentary_keys[1] = 0;
        layer_state.momentary_keys[2] = 0;
      }
      break;
      
    case 3:  // OSL (One-shot layer) - activate for next key only
      if (make) {  // Only act on key press
        layer_state.oneshot_layer = target_layer;
        layer_state.oneshot_active = true;
      }
      break;
  }
}

/**
 * @brief Retrieves the key value at the specified position in the keymap.
 * 
 * Main keymap lookup function. Handles:
 * - Layer switching operations (MO/TG/TO/OSL)
 * - Transparent key fallthrough
 * - Numpad flip translation
 * - Shift-override (if keyboard defines keymap_shifted_keycode)
 * - One-shot layer consumption
 * 
 * @param pos  The position of the key in the keymap (upper nibble = row, lower nibble = col).
 * @param make True if key pressed, false if released.
 *
 * @return The HID keycode to send, or KC_NO if consumed by layer operation.
 */
uint8_t keymap_get_key_val(uint8_t pos, bool make) {
  const uint8_t row = (pos >> 4) & 0x0F;
  const uint8_t col = pos & 0x0F;
  uint8_t key_code = keymap_search_layers(row, col);

  // Handle layer switching keycodes (0xF0-0xFF)
  if (IS_LAYER_KEY(key_code)) {
    process_layer_key(key_code, make);
    return KC_NO;  // Layer keys don't generate HID events
  }

  // Consume one-shot layer after any non-layer key press
  if (layer_state.oneshot_active && make && key_code != KC_TRNS) {
    layer_state.oneshot_active = false;
    layer_state.oneshot_layer = 0;
  }

  // Handle numpad flip (KC_NFLP toggles numpad behavior)
  if (key_code == KC_NFLP) {
    const uint8_t flip_key_code = keymap_search_layers(row, col);
    key_code = numpad_flip_code(flip_key_code);
  }

  // Shift-override support (Phase 3)
  // Some keyboards have non-standard shift legends (e.g., terminal keyboards)
  // The sparse array keymap_shifted_keycode[] maps base keycode → override keycode
  // Only keyboards with non-standard legends define this array (weak symbol)
  if (keymap_shifted_keycode != NULL && hid_is_shift_pressed()) {
    uint8_t override = keymap_shifted_keycode[key_code];
    if (override != 0) {
      key_code = override;
    }
  }

  return key_code;
}