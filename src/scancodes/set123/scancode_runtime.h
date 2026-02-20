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

#ifndef SCANCODE_RUNTIME_H
#define SCANCODE_RUNTIME_H

#include "scancode.h"

/**
 * @file scancode_runtime.h
 * @brief Runtime Configuration for Unified Scancode Processor
 *
 * This header provides runtime scancode set selection for keyboards that support
 * multiple scancode sets. This is primarily useful for AT/PS2 protocol keyboards
 * which can be queried during initialisation to determine their scancode set.
 *
 * Protocol-Specific Behavior:
 * ---------------------------
 *
 * **XT Protocol:**
 * - Always uses Set 1 (no choice available)
 * - No initialisation or ID commands
 * - Fixed scancode set determined by hardware
 * - Use compile-time configuration (SCANCODE_SET=1)
 *
 * **AT/PS2 Protocol:**
 * - Default: Set 2 (most common)
 * - Can support Set 1, Set 2, or Set 3
 * - Keyboard ID determines capabilities
 * - Can query current set with "Read Scancode Set" command
 * - Terminal keyboards typically use Set 3
 * - This is where runtime detection is useful!
 *
 * Usage Example (AT/PS2 Protocol):
 * ---------------------------------
 *
 * ```c
 * #include "scancode_runtime.h"
 *
 * // Global scancode configuration
 * static const scancode_config_t *g_scancode_config = NULL;
 *
 * void keyboard_interface_init(void) {
 *     // ... AT/PS2 initialisation ...
 *
 *     // Read keyboard ID (command 0xF2)
 *     send_command(0xF2);
 *     uint16_t keyboard_id = read_id_response();
 *
 *     // Determine scancode set based on keyboard type
 *     if (is_terminal_keyboard(keyboard_id)) {
 *         printf("[INFO] Terminal keyboard detected, using Set 3\n");
 *         g_scancode_config = &SCANCODE_CONFIG_SET3;
 *
 *         // Terminal keyboards need make/break mode
 *         send_command(0xF8);  // Set all keys to make/break
 *     } else {
 *         // Standard PS/2 keyboards default to Set 2
 *         printf("[INFO] Standard keyboard detected, using Set 2\n");
 *         g_scancode_config = &SCANCODE_CONFIG_SET2;
 *     }
 * }
 *
 * void keyboard_interface_task(void) {
 *     uint8_t code;
 *     if (get_scancode_from_buffer(&code)) {
 *         process_scancode(code, g_scancode_config);
 *     }
 * }
 * ```
 *
 * Hybrid Approach (Compile-Time Fallback):
 * -----------------------------------------
 *
 * You can combine runtime detection with compile-time fallback:
 *
 * ```c
 * // Set compile-time default via CMake: -DSCANCODE_SET_DEFAULT=2
 * static const scancode_config_t *g_scancode_config = NULL;
 *
 * void keyboard_interface_init(void) {
 *     // Try runtime detection
 *     if (can_read_keyboard_id()) {
 *         g_scancode_config = detect_scancode_set();
 *     } else {
 *         // Fallback to compile-time default
 *         #if SCANCODE_SET_DEFAULT == 1
 *             g_scancode_config = &SCANCODE_CONFIG_SET1;
 *         #elif SCANCODE_SET_DEFAULT == 2
 *             g_scancode_config = &SCANCODE_CONFIG_SET2;
 *         #elif SCANCODE_SET_DEFAULT == 3
 *             g_scancode_config = &SCANCODE_CONFIG_SET3;
 *         #endif
 *     }
 * }
 * ```
 *
 * AT/PS2 Keyboard ID Reference:
 * -----------------------------
 *
 * Common keyboard IDs and their scancode sets (from tmk's wiki):
 * Source: https://github.com/tmk/tmk_keyboard/wiki/IBM-PC-AT-Keyboard-Protocol#keyboard-id
 *
 * | ID (hex) | Description                    | Default Set | Notes                            |
 * |----------|--------------------------------|-------------|----------------------------------|
 * | None     | IBM AT 84-key (Model F)        | Set 2       | Doesn't respond to 0xF2          |
 * | AB 41    | MF2 keyboard (PS/2 standard)   | Set 2       |                                  |
 * | AB 83    | MF2 keyboard with translator   | Set 2       | Most common PS/2 ID              |
 * | AB 84    | Short keyboard (no numpad)     | Set 2       | ThinkPad, Spacesaver, tenkeyless |
 * | AB 85    | NCD N-97 or IBM 122-key Model M| Set 3 / 2   | NCD N-97 speaks Set 3            |
 * | AB 86    | Cherry G80-2551, IBM 1397000   | Set 2       | Can switch to Set 3              |
 * | AB 90    | IBM 5576-002 (Japanese)        | Set 2       | Can switch to Set 3              |
 * | AB 91    | IBM 5576-003 / Televideo DEC   | Set 1       | Supports Sets 1, 2, 3            |
 * | AB 92    | IBM 5576-001 (Japanese)        | Set 2       | Supports Set 3, 82h              |
 * | BF BF    | IBM Terminal 122-key           | Set 3       | DIP/jumper configurable          |
 * | BF B0    | IBM RT keyboard                | Set 3       | Swapped num/caps LEDs            |
 * | BF B1    | IBM RT keyboard (ISO)          | Set 3       |                                  |
 * | 7F 7F    | IBM Terminal 101-key (1394204) | Set 3       | Always Set 3, no F0 cmd          |
 * | FF FF    | Unknown (internal timeout ID)  | Set 2       | Safe default                     |
 *
 * **Terminal Keyboard Detection:**
 * This converter detects terminal keyboards that default to or support Set 3:
 * - 0xAB86-0xAB92: Keyboards that support Set 3 (many default to Set 2)
 * - 0xBF00-0xBFFF: Terminal keyboards that default to Set 3
 * - 0x7F00-0x7FFF: Terminal keyboards that always use Set 3
 *
 * Note: The converter DETECTS which scancode set these keyboards are currently using.
 * It does NOT send F0 03 commands to switch scancode sets. For Set 3 keyboards,
 * it only sends the 0xF8 command to configure make/break mode.
 *
 * ```c
 * bool is_terminal_keyboard(uint16_t keyboard_id) {
 *     uint8_t high_byte = (keyboard_id >> 8) & 0xFF;
 *     uint8_t low_byte = keyboard_id & 0xFF;
 *     return (high_byte == 0xAB && low_byte >= 0x86 && low_byte <= 0x92) ||
 *            high_byte == 0xBF || high_byte == 0x7F;
 * }
 * ```
 *
 * Performance Considerations:
 * ---------------------------
 *
 * **Runtime Overhead:**
 * - One pointer dereference per scancode: ~1 cycle
 * - Negligible compared to state machine processing
 * - Acceptable for keyboards that need flexibility
 *
 * **Binary Size:**
 * - All three configs compiled in: ~800 bytes (translation tables)
 * - Compile-time only: ~400 bytes (one config)
 * - Trade-off: 400 bytes for flexibility
 *
 * **Recommendation:**
 * - XT Protocol: Use compile-time (SCANCODE_SET=1)
 * - AT/PS2 Protocol: Use runtime detection
 * - Single known keyboard: Use compile-time
 * - Universal converter: Use runtime detection
 *
 * @see scancode.h for unified processor API
 * @see scancode_config.h for compile-time configuration
 */

/**
 * @brief Scancode Set Detection from Keyboard ID
 *
 * Determines the appropriate scancode set configuration based on the AT/PS2
 * keyboard identification code. This is useful during initialisation to
 * automatically select the correct scancode set.
 *
 * Detection Logic (based on tmk reference implementation):
 * - Terminal keyboards (ID 0xAB86-0xAB92): Use Set 3 (known terminal models)
 * - Terminal keyboards (ID 0xBF00-0xBFFF): Use Set 3 (Type 2 terminals)
 * - Terminal keyboards (ID 0x7F00-0x7FFF): Use Set 3 (Type 1 terminals)
 * - Standard PS/2 keyboards (ID 0xAB00-0xAB85): Use Set 2 (most common)
 * - Unknown/no ID (0xFFFF): Use Set 2 (safe default)
 *
 * Note: Most PS/2 keyboards use Set 2 by default. Set 1 is typically only
 * used by XT keyboards (which don't respond to ID commands anyway) or when
 * explicitly switched via AT/PS2 commands.
 *
 * PC/AT Keyboard Handling: PC/AT keyboards (like IBM Model F PC/AT) don't
 * respond to 0xF2 (Get ID) commands. The protocol implementation will timeout
 * after 2 retries and pass 0xFFFF (ATPS2_KEYBOARD_ID_UNKNOWN) to this function,
 * which correctly returns Set 2 as the default.
 *
 * @param keyboard_id The 16-bit keyboard ID (from 0xF2 command response, or 0xFFFF for timeout)
 * @return Pointer to appropriate scancode configuration
 *
 * @see ATPS2_KEYBOARD_ID_* constants in AT/PS2 protocol header
 */
static inline const scancode_config_t* scancode_config_from_keyboard_id(uint16_t keyboard_id) {
    uint8_t high_byte = (keyboard_id >> 8) & 0xFF;

    // Terminal keyboards that default to Set 3 (based on tmk's wiki):
    // Source: https://github.com/tmk/tmk_keyboard/wiki/IBM-PC-AT-Keyboard-Protocol#keyboard-id
    // - 0xBF00-0xBFFF: Terminal keyboards that default to Set 3 (IBM 1390876, 6110344, RT
    // keyboards)
    // - 0x7F00-0x7FFF: Terminal keyboards that always use Set 3 (IBM 1394204)
    if (high_byte == 0xBF || high_byte == 0x7F) {
        return &SCANCODE_CONFIG_SET3;
    }

    // Note: 0xAB86-0xAB92 keyboards (Cherry G80-2551, IBM 1397000, IBM 5576 series)
    // boot in Set 2 by default. They CAN be switched to Set 3 via F0 03 command,
    // but this converter does NOT send switching commands, so we treat them as Set 2.

    // All other PS/2 keyboards default to Set 2
    // (This is the most common configuration)
    return &SCANCODE_CONFIG_SET2;
}

/**
 * @brief Convenience Wrapper for Runtime Configuration
 *
 * This macro provides a simple way to use runtime configuration with a global
 * configuration pointer. The protocol implementation sets the pointer during
 * initialisation, and this macro provides a backward-compatible interface.
 *
 * Usage:
 * ```c
 * // In protocol implementation (global scope):
 * const scancode_config_t *g_scancode_config = NULL;
 *
 * // During init:
 * g_scancode_config = scancode_config_from_keyboard_id(keyboard_id);
 *
 * // In main task (include scancode_runtime.h):
 * void keyboard_interface_task(void) {
 *     uint8_t code;
 *     if (get_scancode_from_buffer(&code)) {
 *         SCANCODE_PROCESS_RUNTIME(code, g_scancode_config);
 *     }
 * }
 * ```
 *
 * @param code The scancode byte to process
 * @param config_ptr Pointer to scancode configuration (runtime variable)
 */
#define SCANCODE_PROCESS_RUNTIME(code, config_ptr) process_scancode((code), (config_ptr))

#endif  // SCANCODE_RUNTIME_H
