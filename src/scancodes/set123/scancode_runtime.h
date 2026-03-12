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
 * @file scancode_runtime.h
 * @brief Runtime Configuration for Unified Scancode Processor
 *
 * This header provides runtime scancode set selection for keyboards that support
 * multiple scancode sets. This is primarily useful for AT/PS2 protocol keyboards
 * which can be queried during initialisation to determine their scancode set.
 *
 * Protocol-Specific Behaviour:
 * ---------------------------
 *
 * XT Protocol:
 * - Always uses Set 1 (no choice available)
 * - No initialisation or ID commands
 * - Fixed scancode set determined by hardware
 *
 * AT/PS2 Protocol:
 * - Default: Set 2 (most common)
 * - Can support Set 1, Set 2, or Set 3
 * - Keyboard ID determines scancode set selection
 * - Terminal keyboards that default to Set 3 are detected via keyboard ID
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
 * | AB 90    | IBM 5576-002 (Japanese)        | Set 2       | Can switch to Sets 3, 82h        |
 * | AB 91    | IBM 5576-003 / Televideo DEC   | Set 2 / 1   | Sets 1-3, 82h; Televideo: no     |
 * | AB 92    | IBM 5576-001 (Japanese)        | Set 2       | Supports Set 3, 82h              |
 * | BF BF    | IBM Terminal 122-key           | Set 3       | DIP/jumper configurable          |
 * | BF B0    | IBM RT keyboard                | Set 3       | Swapped num/caps LEDs            |
 * | BF B1    | IBM RT keyboard (ISO)          | Set 3       |                                  |
 * | 7F 7F    | IBM Terminal 101-key (1394204) | Set 3       | Always Set 3, no F0 cmd          |
 * | FF FF    | Unknown (internal timeout ID)  | Set 2       | Safe default                     |
 *
 * Terminal Keyboard Detection:
 * The converter selects Set 3 only for keyboards that natively default to it:
 * - 0xBF00-0xBFFF: Terminal keyboards that default to Set 3 (IBM RT, 1390876, 6110344)
 * - 0x7F00-0x7FFF: Terminal keyboards that always use Set 3 (IBM 1394204)
 *
 * Note: Most 0xAB86-0xAB92 keyboards (Cherry G80-2551, IBM 1397000, IBM 5576-002/003)
 * boot in Set 2 by default. Exception: Televideo 990/995 DEC style (also AB 91) boots
 * in Set 1. This converter does NOT send F0 03 switching commands and processes all
 * AB 91 keyboards as Set 2 — the Televideo 990/995 is therefore NOT supported by this
 * converter. Full support would require probing with F0 82 at init, falling back to F0 03
 * on rejection, and processing in Set 3 (Set 2 is partially broken on this keyboard as
 * some distinct keys produce identical codes). See scancode_config_from_keyboard_id() for
 * the full logic.
 *
 * @see scancode.h for unified processor API
 * @see scancode_config.h for compile-time configuration
 */

#ifndef SCANCODE_RUNTIME_H
#define SCANCODE_RUNTIME_H

#include <stdint.h>

#include "scancode.h"

/**
 * @brief Scancode Set Detection from Keyboard ID
 *
 * Determines the appropriate scancode set configuration based on the AT/PS2
 * keyboard identification code. This is useful during initialisation to
 * automatically select the correct scancode set.
 *
 * Detection Logic (based on tmk reference implementation):
 * - 0xBF00-0xBFFF: returns Set 3 (IBM RT and terminal keyboards)
 * - 0x7F00-0x7FFF: returns Set 3 (IBM 1394204 terminal keyboard)
 * - All other IDs: returns Set 2 (including all 0xAB* PS/2 keyboards and 0xFFFF timeout)
 *
 * PC/AT Keyboard Handling: Some keyboards do not respond to 0xF2 (Get ID)
 * commands. The protocol implementation times out after ATPS2_ID_RETRY_LIMIT (2)
 * retries and passes 0xFFFF (ATPS2_KEYBOARD_ID_UNKNOWN) to this function,
 * which returns Set 2 as the default.
 *
 * @param keyboard_id The 16-bit keyboard ID (from 0xF2 command response, or 0xFFFF for timeout)
 * @return Pointer to appropriate scancode configuration
 *
 * @see ATPS2_KEYBOARD_ID_* constants in AT/PS2 protocol header
 */
/** High byte of keyboard ID range for terminal keyboards that default to Scan Code Set 3.
 *  Covers 0xBF00\u20130xBFFF (IBM 1390876, IBM 6110344, RT keyboards).
 *  Source: https://github.com/tmk/tmk_keyboard/wiki/IBM-PC-AT-Keyboard-Protocol#keyboard-id */
#define KBID_HIGH_TERM_SET3_DEFAULT 0xBFU
/** High byte of keyboard ID range for terminal keyboards that always use Scan Code Set 3.
 *  Covers 0x7F00\u20130x7FFF (IBM 1394204).
 *  Source: https://github.com/tmk/tmk_keyboard/wiki/IBM-PC-AT-Keyboard-Protocol#keyboard-id */
#define KBID_HIGH_TERM_SET3_ALWAYS 0x7FU

static inline const scancode_config_t* scancode_config_from_keyboard_id(uint16_t keyboard_id) {
    uint8_t high_byte = (keyboard_id >> 8) & 0xFF;

    // Terminal keyboards that default to Set 3 (based on tmk's wiki):
    // Source: https://github.com/tmk/tmk_keyboard/wiki/IBM-PC-AT-Keyboard-Protocol#keyboard-id
    // - 0xBF00-0xBFFF: Terminal keyboards that default to Set 3 (IBM 1390876, 6110344, RT
    // keyboards)
    // - 0x7F00-0x7FFF: Terminal keyboards that always use Set 3 (IBM 1394204)
    if (high_byte == KBID_HIGH_TERM_SET3_DEFAULT || high_byte == KBID_HIGH_TERM_SET3_ALWAYS) {
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
 * // In main task:
 * if (!ringbuf_is_empty() && tud_hid_ready()) {
 *     uint8_t code = ringbuf_get();
 *     SCANCODE_PROCESS_RUNTIME(code, g_scancode_config);
 * }
 * ```
 *
 * @param code The scancode byte to process
 * @param config_ptr Pointer to scancode configuration (runtime variable)
 */
#define SCANCODE_PROCESS_RUNTIME(code, config_ptr) process_scancode((code), (config_ptr))

#endif /* SCANCODE_RUNTIME_H */
