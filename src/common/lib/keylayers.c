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
 * @file keylayers.c
 * @brief Keyboard layer management and state tracking
 *
 * Implements layer management functionality including layer state tracking,
 * layer lookup helpers, and shift-override handling. Provides the runtime
 * implementation for the public API defined in keylayers.h.
 *
 * The module manages both momentary (MO) and toggle (TG) layer states,
 * handles one-shot layer (OSL) timing, and coordinates with config_storage
 * for layer persistence across reboots.
 *
 * @see keylayers.h for public API documentation
 */

#include "keylayers.h"

#include <stdint.h>

#include "config_storage.h"
#include "hid_keycodes.h"
#include "keymaps.h"
#include "log.h"

/** Base layer bitmask (Layer 0 always active) */
#define LAYER_BASE_MASK 0x01u

/** Hash multiplier used by keylayers_compute_hash */
#define KEYLAYERS_HASH_PRIME 31u

/** Sentinel indicating uninitialised layers hash */
#define LAYERS_HASH_UNINITIALISED 0xFFFFFFFFu

// Layer count (exported by keyboard.c, defaults to max if undefined)
__attribute__((weak)) const uint8_t keymap_layer_count = KEYMAP_MAX_LAYERS;

/**
 * Persistent layer state (TG/TO only - never includes MO bits)
 * Saved to flash via config_storage. Only TG and TO operations modify this.
 * MO (momentary) operations affect only momentary_keys[] and never persist.
 */
static uint8_t persistent_layer_state = 0;

// Layer state tracking (accessible to inline functions in header)
layer_state_t layer_state = {
    .layer_state    = LAYER_BASE_MASK,  // Bit 0 set = Layer 0 (base) active
    .momentary_keys = {0},              // No MO keys held (supports layers 1 to MAX-1)
    .oneshot_layer  = 0,                // No one-shot layer
    .oneshot_active = false             // One-shot not active
};

/**
 * @brief Compute effective layer bitmap from persistent state and momentary keys
 *
 * Combines base layer, persistent layers (TG/TO), and active momentary layers (MO)
 * into the effective layer_state.layer_state bitmap.
 *
 * Formula: LAYER_BASE_MASK | persistent_layer_state | momentary_bits
 *
 * @note This function updates layer_state.layer_state in place.
 */
static void keylayers_update_effective_state(void) {
    uint8_t momentary_bitmap = 0;

    // Compute momentary layer bits from momentary_keys array
    // momentary_keys[i] tracks layer (i+1), so index 0 = layer 1, index 1 = layer 2, etc.
    for (uint8_t i = 0; i < KEYMAP_MAX_LAYERS - 1; i++) {
        if (layer_state.momentary_keys[i] != 0) {
            momentary_bitmap |= (1 << (i + 1));
        }
    }

    // Compute effective state: base | persistent | momentary
    layer_state.layer_state = LAYER_BASE_MASK | persistent_layer_state | momentary_bitmap;
}

/**
 * @brief Reset layer state to base layer (layer 0)
 *
 * Clears all layer activations and returns to base layer.
 * Used during initialization and error recovery.
 *
 * @note This function only modifies in-RAM state and does NOT persist changes to flash.
 *       If TG/TO layers were previously saved via config_save(), they will be restored
 *       on next boot by keylayers_init(). Callers requiring persistent reset must call
 *       config_set_layer_state() and config_save() explicitly after this function.
 */
void keylayers_reset(void) {
    persistent_layer_state = 0;  // Clear persistent layers
    for (uint8_t i = 0; i < KEYMAP_MAX_LAYERS - 1; i++) {
        layer_state.momentary_keys[i] = 0;
    }
    keylayers_update_effective_state();  // Recompute effective state
    layer_state.oneshot_layer  = 0;
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
 * momentary layers array (indices 0 to KEYMAP_MAX_LAYERS-2), then falls back to bitmap scan.
 *
 * @return Active layer number (0-7)
 */
uint8_t keylayers_get_active(void) {
    // Check one-shot layer first (highest priority)
    if (layer_state.oneshot_active && layer_state.oneshot_layer > 0) {
        return layer_state
            .oneshot_layer;  // oneshot_layer stores target layer (1 to KEYMAP_MAX_LAYERS-1)
    }

    // Check momentary layers (higher priority than toggles)
    // Search from highest to lowest momentary layer
    for (int8_t i = KEYMAP_MAX_LAYERS - 2; i >= 0; i--) {
        // momentary_keys indices 0 to MAX-2 correspond to layers 1 to MAX-1
        if (layer_state.momentary_keys[i] != 0) {
            return i + 1;  // Return layer number (1 to MAX-1)
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
 * Computes a hash based on keymap structure (dimensions and layer count only).
 * Used to detect when keymap layout has changed, invalidating saved layer state.
 *
 * Hash includes:
 * - Number of layers (keymap_layer_count from keyboard.c)
 * - Keymap width (KEYMAP_COLS)
 * - Keymap height (KEYMAP_ROWS)
 *
 * IMPORTANT LIMITATION:
 * This hash does NOT include actual key assignments within layers. Changing key
 * values (e.g., swapping KC_A and KC_B) will NOT invalidate the hash. Only
 * structural changes (adding/removing layers, changing matrix dimensions) are
 * detected. This is intentional to avoid unnecessary layer state resets when
 * users modify keymaps.
 *
 * This is a simple validation hash, not cryptographic. Its purpose is to detect
 * structural changes to the keymap layout, not to prevent malicious modification
 * or detect keymap content changes.
 *
 * @return 32-bit hash of keymap structure
 */
static uint32_t keylayers_compute_hash(void) {
    // Simple hash: combine layer count, cols, and rows
    // This will detect layer additions/removals and dimension changes
    uint32_t hash = 0;
    hash          = (hash * KEYLAYERS_HASH_PRIME) + (uint32_t)keymap_layer_count;
    hash          = (hash * KEYLAYERS_HASH_PRIME) + KEYMAP_COLS;
    hash          = (hash * KEYLAYERS_HASH_PRIME) + KEYMAP_ROWS;
    return hash;
}

/**
 * @brief Handle first-boot initialization of layers hash
 *
 * @param current_hash The computed hash for the current keymap configuration
 */
static void handle_first_boot(uint32_t current_hash) {
    LOG_INFO("Initializing layers hash: 0x%08X (layer_count=%d)\n", (unsigned int)current_hash,
             keymap_layer_count);
    persistent_layer_state = 0;  // No persistent layers on first boot
    keylayers_update_effective_state();
    config_set_layers_hash(current_hash);
    config_set_layer_state(LAYER_BASE_MASK);  // Base layer only
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
    persistent_layer_state = 0;  // Reset persistent layers
    keylayers_update_effective_state();
    config_set_layer_state(LAYER_BASE_MASK);
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
    // Defensive: Ensure at least 1 layer (Layer 0) is always valid, cap at KEYMAP_MAX_LAYERS
    const uint8_t effective_layer_count = (keymap_layer_count == 0) ? 1
                                          : (keymap_layer_count > KEYMAP_MAX_LAYERS)
                                              ? KEYMAP_MAX_LAYERS
                                              : keymap_layer_count;
    // Use type-safe shift on uint32_t to avoid UB, then narrow to uint8_t
    const uint8_t valid_mask = (uint8_t)(((uint32_t)1 << effective_layer_count) - 1);
    if ((saved_layer_state & ~valid_mask) != 0) {
        LOG_WARN("Saved layer state 0x%02X has invalid layers (max layer=%d)\n", saved_layer_state,
                 effective_layer_count - 1);
        LOG_INFO("Resetting to Layer 0\n");
        persistent_layer_state = 0;
        keylayers_update_effective_state();
        config_set_layer_state(LAYER_BASE_MASK);
        config_save();
    } else {
        // Restore only persistent bits (TG/TO layers) - MO bits never saved to flash
        persistent_layer_state = saved_layer_state & ~LAYER_BASE_MASK;  // Strip base layer bit
        keylayers_update_effective_state();
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
    const uint32_t current_hash      = keylayers_compute_hash();
    const uint32_t saved_hash        = config_get_layers_hash();
    const uint8_t  saved_layer_state = config_get_layer_state();

    // Sentinel value LAYERS_HASH_UNINITIALISED indicates first boot or config reset
    // (more robust than 0, which could theoretically be a valid hash)
    if (saved_hash == LAYERS_HASH_UNINITIALISED) {
        handle_first_boot(current_hash);
    } else if (saved_hash != current_hash) {
        handle_hash_mismatch(saved_hash, current_hash);
    } else {
        handle_valid_hash(saved_layer_state);
    }
}

/**
 * @brief Handle MO (momentary) layer operation
 *
 * MO layers are NEVER persisted - they only affect momentary_keys[] array.
 * The effective layer bitmap is computed by keylayers_update_effective_state().
 *
 * Bounds validation: target_layer is pre-validated by keylayers_process_key() caller
 * to ensure target_layer != 0 && target_layer < max_layers (prevents buffer underflow).
 *
 * @param target_layer The layer to activate (must be 1 or higher, never 0)
 * @param code The original keycode (for tracking which MO key is held)
 * @param make true if key pressed, false if released
 */
static void keylayers_handle_mo(uint8_t target_layer, uint8_t code, bool make) {
    if (make) {
        // Track which MO key is held (for release detection)
        // momentary_keys[i] corresponds to layer (i+1)
        layer_state.momentary_keys[target_layer - 1] = code;
    } else {
        // Release momentary layer only if this specific MO key is released
        if (layer_state.momentary_keys[target_layer - 1] == code) {
            layer_state.momentary_keys[target_layer - 1] = 0;
        }
    }
    // Recompute effective bitmap (base | persistent | momentary)
    keylayers_update_effective_state();
}

/**
 * @brief Handle TG (toggle) layer operation
 *
 * TG layers persist across reboots. Only persistent_layer_state is modified and saved.
 *
 * @param target_layer The layer to toggle (1-based)
 * @param make true if key pressed, false if released
 */
static void keylayers_handle_tg(uint8_t target_layer, bool make) {
    if (make) {  // Only act on key press, ignore release
        if (persistent_layer_state & (1 << target_layer)) {
            persistent_layer_state &= ~(1 << target_layer);  // Turn off
        } else {
            persistent_layer_state |= (1 << target_layer);  // Turn on
        }
        // Recompute effective bitmap
        keylayers_update_effective_state();
        // Persist toggle layer state to config (base layer + persistent layers)
        config_set_layer_state(LAYER_BASE_MASK | persistent_layer_state);
        config_save();
    }
}

/**
 * @brief Handle TO (switch to) layer operation
 *
 * TO clears all persistent layers and activates only the target layer.
 * Momentary (MO) layers are also cleared.
 *
 * @param target_layer The layer to switch to (1-based)
 * @param make true if key pressed, false if released
 */
static void keylayers_handle_to(uint8_t target_layer, bool make) {
    if (make) {  // Only act on key press
        // Clear all persistent layers, set only target layer
        persistent_layer_state = (1 << target_layer);
        // Clear momentary tracking (same pattern as keylayers_reset())
        for (uint8_t i = 0; i < KEYMAP_MAX_LAYERS - 1; i++) {
            layer_state.momentary_keys[i] = 0;
        }
        // Recompute effective bitmap
        keylayers_update_effective_state();
        // Persist layer state to config (base layer + target layer only)
        config_set_layer_state(LAYER_BASE_MASK | persistent_layer_state);
        config_save();
    }
}

/**
 * @brief Handle OSL (one-shot layer) operation
 *
 * @param target_layer The layer to activate for the next key (1-based)
 * @param make true if key pressed, false if released
 */
static void keylayers_handle_osl(uint8_t target_layer, bool make) {
    if (make) {  // Only act on key press
        layer_state.oneshot_layer  = target_layer;
        layer_state.oneshot_active = true;
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
    const uint8_t operation    = GET_LAYER_OPERATION(code);  // 0=MO, 1=TG, 2=TO, 3=OSL
    const uint8_t target_layer = GET_LAYER_TARGET(code);     // 1-3 (current MO/TG/TO/OSL macros)

    // Guard: Validate target_layer bounds to prevent buffer underflow and invalid operations
    // Lower bound: target_layer must not be 0 (Layer 0 is base, cannot be used with MO/TG)
    // Upper bound: target_layer must be < actual keymap layer count
    const uint8_t max_layers = (keymap_layer_count == 0)                  ? 1
                               : (keymap_layer_count > KEYMAP_MAX_LAYERS) ? KEYMAP_MAX_LAYERS
                                                                          : keymap_layer_count;
    if (target_layer == 0 || target_layer >= max_layers) {
        return;  // Invalid layer (0 or exceeds keymap layer count)
    }

    switch (operation) {
        case 0:  // MO (Momentary) - hold to activate
            keylayers_handle_mo(target_layer, code, make);
            break;

        case 1:  // TG (Toggle) - press to toggle on/off
            keylayers_handle_tg(target_layer, make);
            break;

        case 2:  // TO (Switch to) - permanently switch to layer
            keylayers_handle_to(target_layer, make);
            break;

        case 3:  // OSL (One-shot layer) - activate for next key only
            keylayers_handle_osl(target_layer, make);
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
    layer_state.oneshot_layer  = 0;
}
