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
 * **Key Features:**
 * - Layer priority: One-shot > Momentary (MO) > Toggle (TG) > Base (Layer 0)
 * - Configurable layer count via KEYMAP_MAX_LAYERS (default: 8)
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
    uint8_t layer_state;  // Bitmap of active layers (bit 0 = layer 0, etc.)
    uint8_t momentary_keys[KEYMAP_MAX_LAYERS - 1];  // Track MO keys held (layers 1 to MAX-1)
    uint8_t oneshot_layer;                          // One-shot layer (0 = none, 1-7 = target layer)
    bool    oneshot_active;  // True if one-shot layer is waiting for next key
} layer_state_t;

// Layer state tracking (accessible to inline functions)
extern layer_state_t layer_state;

// Layer count (exported by keyboard.c, defaults to max if undefined)
extern const uint8_t keymap_layer_count __attribute__((weak));

/**
 * @brief Initialize layer system and restore saved layer state
 *
 * Must be called after config_init() during boot.
 * Validates saved layer state against current keymap configuration.
 */
void keylayers_init(void);

/**
 * @brief Reset layer state to base layer (layer 0)
 *
 * Clears all layer activations and returns to base layer.
 * Used during initialization and error recovery.
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
 * @return Active layer number (0-7)
 */
uint8_t keylayers_get_active(void);

/**
 * @brief Get the layer state bitmap
 *
 * Returns the raw bitmap of active layers.
 * Bit 0 = Layer 0, Bit 1 = Layer 1, etc.
 *
 * @return Layer state bitmap
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
 * @param layer Layer number to check (0-7)
 * @return true if layer is active in the bitmap, false otherwise
 */
static inline bool keylayers_is_active(uint8_t layer) {
    return (layer_state.layer_state & (1 << layer)) != 0;
}

/**
 * @brief Process layer switching key (MO/TG/TO/OSL)
 *
 * Handles layer modifier key press/release.
 * Updates layer state based on operation type and make/break.
 *
 * @param code Layer operation keycode (0xF0-0xFF)
 * @param make true if key pressed, false if released
 */
void keylayers_process_key(uint8_t code, bool make);

/**
 * @brief Consume one-shot layer after key press
 *
 * Should be called after processing any non-layer key press.
 * Deactivates one-shot layer if active.
 */
void keylayers_consume_oneshot(void);

/**
 * @brief Compute validation hash for current keymap configuration
 *
 * Used to detect when keymap layout has changed, invalidating saved layer state.
 * Hash is based on layer count and keymap dimensions.
 *
 * @return 32-bit hash of keymap configuration
 */
uint32_t keylayers_compute_hash(void);

#endif /* KEYLAYERS_H */
