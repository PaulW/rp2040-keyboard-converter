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
 * @file keylayers.h
 * @brief Layer state management and layer switching operations
 *
 * Provides layer state tracking, priority resolution, and layer switching
 * operations (momentary, toggle, switch-to, one-shot). Manages the active
 * layer state and coordinates with the keymap system for multi-layer support.
 *
 * Key Features:
 * - Layer priority: One-shot > Momentary (MO) > Toggle (TG) > Base (Layer 0)
 * - Layer count cap via KEYMAP_MAX_LAYERS (5 total: base Layer 0 + 4 switchable;
 *   max by keycode encoding)
 * - Persistent layer state across reboots (toggle/switch-to layers)
 * - One-shot layer support with automatic deactivation
 */

#ifndef KEYLAYERS_H
#define KEYLAYERS_H

#include <stdbool.h>
#include <stdint.h>

#include "keymaps.h"

/**
 * @brief Layer state tracking structure
 *
 * Maintains the current state of all active layers and temporary layer modifiers.
 * Used internally by the layer management system.
 */
typedef struct {
    uint8_t layer_state; /**< Bitmap of active layers (bit 0 = layer 0, etc.) */
    uint8_t momentary_keys[KEYMAP_MAX_LAYERS - 1]; /**< Track MO keys held (layers 1 to MAX-1) */
    uint8_t
         oneshot_layer;  /**< One-shot layer (0 = none, 1 to KEYMAP_MAX_LAYERS-1 = target layer) */
    bool oneshot_active; /**< True if one-shot layer is waiting for next key */
} layer_state_t;

/** @brief Layer state tracking structure instance (accessible to inline functions). */
extern layer_state_t layer_state;

/** @brief Layer count exported by keyboard.c; defaults to KEYMAP_MAX_LAYERS if not defined. */
extern const uint8_t keymap_layer_count __attribute__((weak));

/**
 * @brief Initialise layer system and restore saved layer state
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
 *
 * @note Main loop only.
 */
void keylayers_init(void);

/**
 * @brief Reset layer state to base layer (layer 0)
 *
 * Clears all layer activations and returns to base layer.
 * Used during initialisation and error recovery.
 *
 * @note Main loop only.
 * @note This function only modifies in-RAM state and does NOT persist changes to flash.
 *       If TG/TO layers were previously saved via config_save(), they will be restored
 *       on next boot by keylayers_init(). Callers requiring persistent reset must call
 *       config_set_layer_state() and config_save() explicitly after this function.
 */
void keylayers_reset(void);

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
 * @return Active layer number (0–4)
 *
 * @note Main loop only.
 */
uint8_t keylayers_get_active(void);

/**
 * @brief Get the layer state bitmap
 *
 * Returns the raw bitmap of active layers.
 * Bit 0 = Layer 0, Bit 1 = Layer 1, etc.
 *
 * @return Layer state bitmap
 *
 * @note Main loop only.
 */
static inline uint8_t keylayers_get_state_bitmap(void) {
    return layer_state.layer_state;
}

/**
 * @brief Check if a specific layer is active
 *
 * Fast inline check for layer activity.
 * Layer 0 is always active (bit 0 always set).
 *
 * @param layer Layer number to check (0 through KEYMAP_MAX_LAYERS-1)
 * @return true if layer is active in the bitmap, false otherwise (or if layer out of range)
 *
 * @note Main loop only.
 */
static inline bool keylayers_is_active(uint8_t layer) {
    if (layer >= KEYMAP_MAX_LAYERS) {
        return false;
    }
    return (layer_state.layer_state & (1 << layer)) != 0;
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
 *
 * @note Main loop only.
 */
void keylayers_process_key(uint8_t code, bool make);

/**
 * @brief Consume one-shot layer after key press
 *
 * Should be called after processing any non-layer, non-transparent key press.
 * Deactivates the one-shot layer if currently active.
 *
 * Implementation clears oneshot_active flag and oneshot_layer value.
 *
 * @note Main loop only.
 */
void keylayers_consume_oneshot(void);

#endif /* KEYLAYERS_H */
