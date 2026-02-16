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

#include "keylayers.h"

#include <stdint.h>

#include "config_storage.h"
#include "hid_keycodes.h"
#include "keymaps.h"
#include "log.h"

// Layer count (exported by keyboard.c, defaults to max if undefined)
__attribute__((weak)) const uint8_t keymap_layer_count = KEYMAP_MAX_LAYERS;

// Layer state tracking (accessible to inline functions in header)
layer_state_t layer_state = {
    .layer_state = 0x01,           // Bit 0 set = Layer 0 (base) active
    .momentary_keys = {0, 0, 0},   // No MO keys held
    .oneshot_layer = 0,            // No one-shot layer
    .oneshot_active = false        // One-shot not active
};

/**
* @brief Reset layer state to base layer (layer 0)
* 
* Clears all layer activations and returns to base layer.
* Used during initialization and error recovery.
*/
void keylayers_reset(void) {
    layer_state.layer_state = 0x01;     // Only layer 0 active
    layer_state.momentary_keys[0] = 0;
    layer_state.momentary_keys[1] = 0;
    layer_state.momentary_keys[2] = 0;
    layer_state.oneshot_layer = 0;
    layer_state.oneshot_active = false;
}

/**
* @brief Get the highest active layer number
* 
* Priority order (highest to lowest):
* 1. One-shot layer (if active)
* 2. Momentary layers (highest MO layer held)
* 3. Toggle/base layers (highest bit in bitmap)
* 
* This ensures MO (momentary) takes precedence over TG (toggle).
* 
* Implementation uses early returns - checks one-shot first, then iterates
* momentary layers array (indices 0-2), then falls back to bitmap scan.
* 
* @return Active layer number (0-7)
*/
uint8_t keylayers_get_active(void) {
    // Check one-shot layer first (highest priority)
    if (layer_state.oneshot_active && layer_state.oneshot_layer > 0) {
        return layer_state.oneshot_layer;  // oneshot_layer stores target layer (1-4)
    }
  
    // Check momentary layers (higher priority than toggles)
    // Search from highest to lowest momentary layer
    for (int8_t i = 2; i >= 0; i--) {  // momentary_keys indices 0-2 = layers 1-3
        if (layer_state.momentary_keys[i] != 0) {
            return i + 1;  // Return layer number (1-3)
        }
    }
  
    // Check toggle/base layers (search bitmap from highest to lowest)
    for (int8_t i = KEYMAP_MAX_LAYERS - 1; i >= 0; i--) {
        if (layer_state.layer_state & (1 << i)) {
            return i;
        }
    }
  
    // Fallback to layer 0 (should never reach here due to init)
    return 0;
}

/**
* @brief Compute validation hash for current keymap configuration
* 
* Computes a hash based on the keymap dimensions and layer count.
* Used to detect when keymap layout has changed, invalidating saved layer state.
* 
* Hash includes:
* - Number of layers (keymap_layer_count from keyboard.c)
* - Keymap width (KEYMAP_COLS)
* - Keymap height (KEYMAP_ROWS)
* 
* This is a simple validation hash, not cryptographic. Its purpose is to detect
* changes to the keymap layout, not to prevent malicious modification.
* 
* @return 32-bit hash of keymap configuration
*/
uint32_t keylayers_compute_hash(void) {
    // Simple hash: combine layer count, cols, and rows
    // This will detect layer additions/removals and dimension changes
    uint32_t hash = 0;
    hash = (hash * 31) + (uint32_t)keymap_layer_count;
    hash = (hash * 31) + KEYMAP_COLS;
    hash = (hash * 31) + KEYMAP_ROWS;
    return hash;
}

/**
* @brief Handle first-boot initialization of layers hash
* 
* @param current_hash The computed hash for the current keymap configuration
*/
static void handle_first_boot(uint32_t current_hash) {
    LOG_INFO("Initializing layers hash: 0x%08X (layer_count=%d)\n", 
                    (unsigned int)current_hash, keymap_layer_count);
    config_set_layers_hash(current_hash);
    config_save();
}

/**
* @brief Handle keymap configuration change (hash mismatch)
* 
* @param saved_hash The previously saved hash value
* @param current_hash The computed hash for the current keymap configuration
*/
static void handle_hash_mismatch(uint32_t saved_hash, uint32_t current_hash) {
    LOG_INFO("Keymap config changed (hash 0x%08X → 0x%08X, layer_count=%d)\n",
                    (unsigned int)saved_hash, (unsigned int)current_hash, keymap_layer_count);
    LOG_INFO("Resetting layer state to Layer 0\n");
    layer_state.layer_state = 0x01;
    config_set_layer_state(0x01);
    config_set_layers_hash(current_hash);
    config_save();
}

/**
* @brief Validate and restore saved layer state
* 
* @param saved_layer_state The layer state bitmap from config storage
*/
static void handle_valid_hash(uint8_t saved_layer_state) {
    // Validate no invalid layers are active
    // Clamp to valid layer range (layers 0 to keymap_layer_count-1)
    const uint8_t valid_mask = (1 << keymap_layer_count) - 1;
    if ((saved_layer_state & ~valid_mask) != 0) {
        LOG_WARN("Saved layer state 0x%02X has invalid layers (max layer=%d)\n",
                        saved_layer_state, keymap_layer_count - 1);
        LOG_INFO("Resetting to Layer 0\n");
        layer_state.layer_state = 0x01;
        config_set_layer_state(0x01);
        config_save();
    } else {
        layer_state.layer_state = saved_layer_state;
        LOG_INFO("Restored layer state: 0x%02X\n", saved_layer_state);
    }
}

/**
* @brief Initialize layer system and restore saved layer state
* 
* This function should be called after config_init() during boot.
* It computes the current keymap hash and attempts to restore the saved
* layer state from config storage.
* 
* Validation checks:
* - If keyboard_id changed → reset to base layer (Layer 0)
* - If layers_hash changed → reset to base layer (Layer 0)
* - Otherwise → restore saved layer_state
* 
* The keyboard_id validation happens in config_storage.c during config_init().
* This function handles the layers_hash validation.
*/
void keylayers_init(void) {
    const uint32_t current_hash = keylayers_compute_hash();
    const uint32_t saved_hash = config_get_layers_hash();
    const uint8_t saved_layer_state = config_get_layer_state();
  
    // Sentinel value 0xFFFFFFFF indicates first boot or config reset
    // (more robust than 0, which could theoretically be a valid hash)
    if (saved_hash == 0xFFFFFFFF) {
        handle_first_boot(current_hash);
    } else if (saved_hash != current_hash) {
        handle_hash_mismatch(saved_hash, current_hash);
    } else {
        handle_valid_hash(saved_layer_state);
    }
}

/**
* @brief Process layer switching key operations
* 
* Handles MO (momentary), TG (toggle), TO (switch to), and OSL (one-shot) operations.
* Updates layer_state based on keycode and make/break.
* 
* Implementation uses bitmap operations and array indexing.
* Config persistence triggered only for TG and TO (not MO or OSL).
* 
* @param code The layer operation keycode (0xF0-0xFF)
* @param make true if key pressed, false if released
*/
void keylayers_process_key(uint8_t code, bool make) {
    const uint8_t operation = GET_LAYER_OPERATION(code);  // 0=MO, 1=TG, 2=TO, 3=OSL
    const uint8_t target_layer = GET_LAYER_TARGET(code);  // 1-3 (MO/TG/TO/OSL macros support layers 1-3)
  
    switch (operation) {
        case 0:  // MO (Momentary) - hold to activate (layers 1-3 only)
            // Guard: MO only supports layers 1-3 (momentary_keys array size)
            if (target_layer < 1 || target_layer > 3) {
                break;  // Invalid layer for MO operation
            }
            if (make) {
                layer_state.layer_state |= (1 << target_layer);
                // Track which MO key is held (for release detection)
                layer_state.momentary_keys[target_layer - 1] = code;
            } else {
                // Release momentary layer only if this specific MO key is released
                if (layer_state.momentary_keys[target_layer - 1] == code) {
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
                // Persist toggle layer state to config
                config_set_layer_state(layer_state.layer_state);
                config_save();
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
                // Persist layer state to config
                config_set_layer_state(layer_state.layer_state);
                config_save();
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
* @brief Consume one-shot layer after key press
* 
* Should be called after processing any non-layer, non-transparent key press.
* Deactivates the one-shot layer if currently active.
* 
* Implementation clears oneshot_active flag and oneshot_layer value.
*/
void keylayers_consume_oneshot(void) {
    layer_state.oneshot_active = false;
    layer_state.oneshot_layer = 0;
}
