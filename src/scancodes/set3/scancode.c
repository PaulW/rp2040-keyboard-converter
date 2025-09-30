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

#include "scancode.h"

#include <stdio.h>

#include "hid_interface.h"

/**
 * @brief Process Keyboard Input (Scancode Set 3) Data
 * 
 * This function processes scan codes from terminal keyboards using Scan Code Set 3.
 * Set 3 is the most logical and consistent set, designed for terminal keyboards.
 * 
 * Protocol Overview:
 * - Make codes: 0x01-0x84 (key press)
 * - Break codes: F0 followed by make code (key release, e.g., F0 1C = key 0x1C released)
 * - No multi-byte E0/E1 sequences (cleaner than Set 1 and Set 2)
 * - Single-byte codes for all keys (simplest scan code set)
 * 
 * State Machine:
 * 
 *     INIT ──[F0]──> F0 ──[code]──> INIT (process break code)
 *       │
 *       └──[code]──> INIT (process make code)
 * 
 * Sequence Examples:
 * - Normal key press:     1C (A key make)
 * - Normal key release:   F0 1C (A key break)
 * - Arrow Up press:       43 (make)
 * - Arrow Up release:     F0 43 (break)
 * 
 * Special Codes:
 * - 7C: Keypad Comma (mapped to 0x68)
 * - 83: F7 (Left F7 position, mapped to 0x02)
 * - 84: Keypad Plus position (legend says minus, mapped to 0x7F)
 * - AA: Self-test passed (ignored)
 * - FC: Self-test failed (ignored)
 * 
 * Typematic Mode Note:
 * - Most Set 3 keyboards default to typematic mode (auto-repeat)
 * - This implementation assumes make/break mode is configured
 * - AT/PS2 protocol initialization sets make/break mode via command 0xFA
 * 
 * @param code The keycode to process.
 *
 * @note This function is called from the keyboard_interface_task() in main task context
 * @note handle_keyboard_report() translates scan codes to HID keycodes via keymap lookup
 * @note Set 3 is the cleanest scan code set with no complex multi-byte sequences
 */
void process_scancode(uint8_t code) {
  // clang-format off
  static enum {
    INIT,
    F0,
  } state = INIT;
  // clang-format on

  switch (state) {
    case INIT:
      switch (code) {
        case 0xF0:
          state = F0;
          break;
        case 0x7C:  // Keypad Comma
          handle_keyboard_report(0x68, true);
          break;
        case 0x83:  // Left F7 Position
          handle_keyboard_report(0x02, true);
          break;
        case 0x84:  // Keypad Plus (Legend says minus)
          handle_keyboard_report(0x7F, true);
          break;
        case 0xAA:  // Self-test passed
        case 0xFC:  // Self-test failed
        default:    // Handle normal key event
          state = INIT;
          if (code < 0x80) {
            handle_keyboard_report(code, true);
          } else {
            printf("[DBG] !INIT! (0x%02X)\n", code);
          }
      }
      break;

    case F0:  // Break code
      switch (code) {
        case 0x7C:  // Keypad Comma
          handle_keyboard_report(0x68, false);
          state = INIT;
          break;
        case 0x83:  // Left F7 Position
          handle_keyboard_report(0x02, false);
          state = INIT;
          break;
        case 0x84:  // Keypad Plus (Legend says minus)
          handle_keyboard_report(0x7F, false);
          state = INIT;
          break;
        default:
          state = INIT;
          if (code < 0x80) {
            handle_keyboard_report(code, false);
          } else {
            printf("[DBG] !F0! (0x%02X)\n", code);
          }
      }
      break;

    default:
      state = INIT;
  }
}