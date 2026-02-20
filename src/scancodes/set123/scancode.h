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

#ifndef SCANCODE_SET123_H
#define SCANCODE_SET123_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Scancode Set Type for XT/AT Protocol Keyboards (Sets 1, 2, 3)
 *
 * This enum defines the three scancode sets used by XT and AT/PS2 protocol keyboards.
 * Each set has different characteristics in terms of encoding, complexity, and usage.
 *
 * Protocol Comparison:
 *
 * | Feature          | Set 1 (XT)      | Set 2 (AT/PS2)  | Set 3 (Terminal) |
 * |------------------|-----------------|-----------------|------------------|
 * | Break Encoding   | `code | 0x80`   | F0 + code       | F0 + code        |
 * | E0 Prefix        | Yes (extended)  | Yes (extended)  | No               |
 * | E1 Prefix        | Yes (Pause)     | Yes (Pause)     | No               |
 * | Fake Shifts      | 2A,AA,36,B6     | 12,59           | None             |
 * | State Complexity | Medium (5)      | High (9)        | Low (2)          |
 * | Common Usage     | XT, some AT     | PS/2 default    | Terminal KBs     |
 *
 * Historical Context:
 * - Set 1: Original IBM PC/XT scancode set (1981)
 * - Set 2: IBM AT scancode set, became PS/2 standard (1984)
 * - Set 3: IBM terminal keyboards, most logical design (1987)
 */
typedef enum {
    SCANCODE_SET1,  ///< XT/Set 1: Original IBM PC/XT scancode set (break = code | 0x80)
    SCANCODE_SET2,  ///< AT/Set 2: PS/2 default scancode set (break = F0 + code)
    SCANCODE_SET3   ///< Terminal/Set 3: Simplest scancode set (break = F0 + code, no E0/E1)
} scancode_set_t;

/**
 * @brief Unified Scancode Processor Configuration
 *
 * This structure configures the behavior of the unified scancode processor.
 * It allows a single processor to handle all three XT/AT scancode sets with
 * appropriate translation tables and behavior flags.
 *
 * Usage Example:
 * ```c
 * static const scancode_config_t set2_config = {
 *     .set = SCANCODE_SET2,
 *     .e0_translation = set2_e0_table,
 *     .has_e0_prefix = true,
 *     .has_e1_prefix = true
 * };
 *
 * void keyboard_task(void) {
 *     uint8_t code = get_scancode();
 *     process_scancode(code, &set2_config);
 * }
 * ```
 */
typedef struct {
    scancode_set_t set;             ///< Which scancode set to use
    const uint8_t* e0_translation;  ///< E0-prefix translation table (256 bytes, NULL if N/A)
    bool           has_e0_prefix;   ///< True if this set uses E0 prefix
    bool           has_e1_prefix;   ///< True if this set uses E1 prefix (Pause/Break)
} scancode_config_t;

/**
 * @brief Process Unified XT/AT Scancode
 *
 * This function provides a unified processor for all three XT/AT scancode sets (Set 1, 2, and 3).
 * It consolidates the common state machine logic whilst allowing per-set configuration via the
 * config parameter.
 *
 * Protocol Overview:
 *
 * **Set 1 (XT/AT):**
 * - Make: 0x01-0x53
 * - Break: Make + 0x80 (e.g., 0x81 = key 0x01 released)
 * - E0 prefix: Extended keys (arrows, navigation, Windows keys)
 * - E1 prefix: Pause/Break only
 * - Fake shifts: E0 2A, E0 AA, E0 36, E0 B6 (ignored)
 *
 * **Set 2 (PS/2 default):**
 * - Make: 0x01-0x83
 * - Break: F0 + make (e.g., F0 1C = key 0x1C released)
 * - E0 prefix: Extended keys, multimedia keys
 * - E1 prefix: Pause/Break (complex 8-byte sequence)
 * - Fake shifts: E0 12, E0 59 (ignored)
 * - Special codes: 83 (F7), 84 (SysReq)
 *
 * **Set 3 (Terminal):**
 * - Make: 0x01-0x84
 * - Break: F0 + make (e.g., F0 1C = key 0x1C released)
 * - No E0/E1 prefixes (simplest)
 * - Special codes: 7C (Keypad Comma), 83 (F7), 84 (Keypad Plus)
 *
 * State Machine (All Sets):
 * ```
 * INIT ──[F0]──> F0 ──[code]──> INIT (break, Set 2/3)
 *   │
 *   ├──[E0]──> E0 ──[code/F0]──> ... (extended, Set 1/2)
 *   │
 *   ├──[E1]──> E1 ──[sequences]──> ... (Pause, Set 1/2)
 *   │
 *   └──[code]──> INIT (make or break depending on set)
 * ```
 *
 * @param code The scancode byte to process
 * @param config Pointer to scancode set configuration (defines behavior)
 *
 * @note This function maintains internal state across calls (static variables)
 * @note State is reset to INIT after processing each complete sequence
 * @note Calls handle_keyboard_report(code, is_make) for each complete key event
 * @note Debug output via LOG_DEBUG() for unexpected states
 *
 * @see scancode_config_t for configuration details
 * @see handle_keyboard_report() for key event processing
 */
void process_scancode(uint8_t code, const scancode_config_t* config);

/**
 * @brief Reset Scancode Processor State
 *
 * Resets the internal state machine to INIT. This should be called when:
 * - Switching scancode sets dynamically
 * - Re-initialising the keyboard interface
 * - Recovering from protocol errors
 *
 * @note Not normally needed during regular operation
 * @note State is automatically reset after each complete sequence
 */
void reset_scancode_state(void);

// ============================================================================
// Pre-configured Constants (for convenience)
// ============================================================================

/**
 * @brief Pre-configured Scancode Set Configurations
 *
 * These constants provide ready-to-use configurations for each scancode set.
 * Simply pass the appropriate constant to process_scancode().
 *
 * Example:
 * ```c
 * process_scancode(scancode_byte, &SCANCODE_CONFIG_SET2);
 * ```
 */
extern const scancode_config_t SCANCODE_CONFIG_SET1;  ///< XT/Set 1 configuration
extern const scancode_config_t SCANCODE_CONFIG_SET2;  ///< AT/PS2 Set 2 configuration
extern const scancode_config_t SCANCODE_CONFIG_SET3;  ///< Terminal Set 3 configuration

#endif  // SCANCODE_SET123_H
