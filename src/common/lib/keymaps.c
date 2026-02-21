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

#include "keymaps.h"

#include <stdint.h>
#include <stdio.h>

#include "config_storage.h"
#include "hid_interface.h"
#include "hid_keycodes.h"
#include "keylayers.h"
#include "log.h"

// Enforce 4-bit encoding constraint: pos parameter uses upper/lower nibble for row/col
// This limits maximum keymap dimensions to 16x16 (0-15 range per nibble)
_Static_assert(KEYMAP_ROWS <= 16 && KEYMAP_COLS <= 16,
               "Keymap dimensions must fit in 4-bit encoding (pos parameter)");

/**
 * @brief Scans lower layers for layer modifiers only
 *
 * IMPORTANT: Layer modifier precedence only applies within the transparent fall-through chain.
 * The search honors IS_LAYER_KEY modifiers ONLY whilst traversing KC_TRNS entries. When a
 * non-TRNS regular key (e.g., KC_B, KC_A) is encountered on an intermediate lower layer, the
 * search stops immediately, and any IS_LAYER_KEY modifiers in further-lower layers are ignored.
 *
 * Example:
 *   Layer 3 active: KC_A (regular key, triggers this scan)
 *   Layer 2: KC_TRNS (transparent, search continues)
 *   Layer 1: MO_4 (layer modifier, would be returned - honoured through KC_TRNS chain)
 *
 * But if Layer 2 had KC_B instead:
 *   Layer 3 active: KC_A (regular key, triggers this scan)
 *   Layer 2: KC_B (non-TRNS regular key, search STOPS here)
 *   Layer 1: MO_4 (layer modifier, IGNORED - not reached because Layer 2 stopped search)
 *
 * Safety feature: Layer modifiers in lower layers take precedence over regular keys
 * in higher layers. This prevents accidentally overriding layer navigation keys.
 *
 * @param row           The row index in the keymap
 * @param col           The column index in the keymap
 * @param source_layer  Output parameter set to the layer where modifier was found (can be NULL)
 * @param start_layer   Layer to start searching from (searches down from here)
 * @return              Layer modifier keycode if found, KC_NO if not found
 */
static uint8_t scan_lower_layers_for_modifier(uint8_t row, uint8_t col, uint8_t* source_layer,
                                              int8_t start_layer) {
    for (int8_t i = (int8_t)(start_layer - 1); i >= 0; i--) {
        // Skip inactive layers (Layer 0 always active)
        if (i > 0 && !keylayers_is_active(i)) {
            continue;
        }

        const uint8_t lower_key = keymap_map[i][row][col];

        // Layer modifier found - takes precedence over active layer regular key
        if (IS_LAYER_KEY(lower_key)) {
            if (source_layer != NULL) {
                *source_layer = i;
            }
            return lower_key;
        }

        // Stop at first non-TRNS regular key (caller already has key from active layer)
        if (lower_key != KC_TRNS) {
            break;
        }
    }

    return KC_NO;  // No layer modifier found
}

/**
 * @brief Searches for the key code in the keymap based on the specified row and column.
 *
 * Starts from the highest active layer and falls through transparent keys to lower active layers.
 * Only checks layers that are set in the layer_state bitmap (Layer 0 always active).
 * This allows proper layer stacking with TG (toggle) and exclusive mode with TO (switch to).
 *
 * LAYER MODIFIER SAFETY FEATURE:
 * If a layer modifier key (MO_x, TO_x, TG_x, OSL_x) is found at any layer whilst searching down,
 * it takes precedence over any regular keys found in higher layers. This prevents users from
 * accidentally overriding layer navigation keys by placing regular keys at the same position
 * in higher layers.
 *
 * Example:
 *   Layer 1: KC_A
 *   Layer 2: KC_TRNS
 *   Layer 3: MO_4
 *   Layer 4: KC_C
 *
 * Results (assuming layers 1–4 are all active):
 *   - Active Layer 1 or 2: Returns KC_A (normal fallthrough)
 *   - Active Layer 3: Returns MO_4 (layer modifier)
 *   - Active Layer 4: Returns MO_4 (KC_C ignored because Layer 3 has modifier)
 *
 * Implementation structure:
 * - Checks active layer first, returns immediately if non-transparent
 * - Layer modifier in active layer returns without checking lower layers
 * - Non-transparent regular key in active layer triggers search of lower layers for modifiers
 * - Transparent key in active layer triggers full fallthrough search
 *
 * @param row          The row index in the keymap.
 * @param col          The column index in the keymap.
 * @param source_layer Output parameter set to the layer where the key was found (not active layer
 * if TRNS). Can be NULL if caller doesn't need this info.
 *
 * @return The key code found in the keymap.
 */
static uint8_t keymap_search_layers(uint8_t row, uint8_t col, uint8_t* source_layer) {
    const uint8_t active_layer = keylayers_get_active();

    // Fast path: Check active layer first (most common case - non-TRNS in active layer)
    uint8_t key_code = keymap_map[active_layer][row][col];

    // Active layer has non-transparent key
    if (key_code != KC_TRNS) {
        // Layer modifier in active layer - immediate return (highest priority)
        if (IS_LAYER_KEY(key_code)) {
            if (source_layer != NULL) {
                *source_layer = active_layer;
            }
            return key_code;
        }

        // Regular key in active layer - check lower layers for layer modifiers only
        // (Safety feature: layer modifiers override regular keys in higher layers)
        const uint8_t modifier =
            scan_lower_layers_for_modifier(row, col, source_layer, (int8_t)active_layer);
        if (modifier != KC_NO) {
            return modifier;  // Layer modifier found in lower layer
        }

        // No layer modifier in lower layers - use regular key from active layer
        if (source_layer != NULL) {
            *source_layer = active_layer;
        }
        return key_code;
    }

    // Slow path: Active layer is transparent - search lower layers
    for (int8_t i = (int8_t)(active_layer - 1); i >= 0; i--) {
        // Skip inactive layers (Layer 0 always active)
        if (i > 0 && !keylayers_is_active(i)) {
            continue;
        }

        const uint8_t layer_key = keymap_map[i][row][col];

        // Skip transparent keys
        if (layer_key == KC_TRNS) {
            continue;
        }

        // Found first non-transparent key (regular or layer modifier)
        if (source_layer != NULL) {
            *source_layer = i;
        }
        return layer_key;
    }

    // Error case: All layers transparent (should never happen with valid Layer 0)
    LOG_ERROR("KC_TRNS detected in base layer at [%d][%d]!\n", row, col);
    if (source_layer != NULL) {
        *source_layer = 0;
    }
    return KC_NO;
}

/**
 * @brief Applies per-layer shift-override mapping to a keycode.
 *
 * Keyboards with non-standard shift legends (terminal, vintage, international layouts)
 * define per-layer shift-override arrays. This function looks up the override for the
 * given keycode on the specified layer and applies it if present.
 *
 * The SUPPRESS_SHIFT flag (bit 7) signals whether to suppress the shift modifier:
 *   - Without flag: Keep shift modifier (e.g., Shift+6 → Shift+7)
 *   - With flag: Suppress shift (e.g., SUPPRESS_SHIFT|KC_QUOT sends unshifted quote)
 *
 * @param key_code        The original keycode to potentially override
 * @param source_layer    The layer the keycode came from
 * @param suppress_shift  Output parameter set to true if shift should be suppressed
 * @return                The possibly-modified keycode (original if no override)
 */
static uint8_t apply_shift_override(uint8_t key_code, uint8_t source_layer, bool* suppress_shift) {
    // Safety: key_code is uint8_t (0-255), guaranteeing valid index into 256-element arrays
    _Static_assert(SHIFT_OVERRIDE_ARRAY_SIZE == 256,
                   "SHIFT_OVERRIDE_ARRAY_SIZE must be 256 to match uint8_t keycode range");

    // Get the shift-override array for this layer (NULL if layer has no overrides)
    const uint8_t* shift_overrides = keymap_shift_override_layers[source_layer];
    if (shift_overrides == NULL) {
        return key_code;  // No shift-override defined for this layer
    }

    // Look up the override for this keycode
    uint8_t override = shift_overrides[key_code];
    if (override == 0) {
        return key_code;  // No override for this keycode (zero means no-op)
    }

    // Check if SUPPRESS_SHIFT flag is set
    if (override & SUPPRESS_SHIFT) {
        key_code = override & ~SUPPRESS_SHIFT;  // Clear flag to get actual keycode
        if (suppress_shift != NULL) {
            *suppress_shift = true;
        }
    } else {
        key_code = override;  // Keep shift modifier
    }

    return key_code;
}

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
 */
uint8_t keymap_get_key_val(uint8_t pos, bool make, bool* suppress_shift) {
    // Initialise output parameter to default false to avoid leaking previous value
    if (suppress_shift != NULL) {
        *suppress_shift = false;
    }

    const uint8_t row = (pos >> 4) & 0x0F;
    const uint8_t col = pos & 0x0F;

    // Bounds check: 4-bit encoding allows 0-15, but actual matrix may be smaller
    if (row >= KEYMAP_ROWS || col >= KEYMAP_COLS) {
        LOG_WARN("Invalid key position: row=%u, col=%u (max: %u x %u)\n", row, col, KEYMAP_ROWS,
                 KEYMAP_COLS);
        return KC_NO;
    }

    uint8_t source_layer = 0;  // Track which layer the key came from (for shift-override)
    uint8_t key_code     = keymap_search_layers(row, col, &source_layer);

    // Handle layer switching keycodes (0xF0-0xFF)
    if (IS_LAYER_KEY(key_code)) {
        keylayers_process_key(key_code, make);
        return KC_NO;  // Layer keys don't generate HID events
    }

    // Consume one-shot layer after any non-layer key press
    // Skip consumption if keymap_search_layers returned KC_NO (all-transparent error case)
    if (make && key_code != KC_NO) {
        keylayers_consume_oneshot();
    }

    // Apply shift-override if keyboard defines it, user enabled it, shift is pressed,
    // and source layer is within valid range
    if (keymap_shift_override_layers != NULL && config_get_shift_override_enabled() &&
        hid_is_shift_pressed() && source_layer < KEYMAP_MAX_LAYERS) {
        key_code = apply_shift_override(key_code, source_layer, suppress_shift);
    }

    return key_code;
}