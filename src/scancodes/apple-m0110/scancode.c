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
#include "log.h"

// clang-format off
// Apple M0110A will prefix certain keys with 0x79, or 0x71,0x79.
// These macros translate these codes.
#define SWITCH_71_CODE(code) \
  (code == 0x0F ? 0x2A : /* Cursor Up */ \
  (code == 0x11 ? 0x32 : /* Cursor Down */ \
  (code == 0x1B ? 0x3A : /* Cursor Left */ \
  (code == 0x05 ? 0x42 : /* Cursor Right */ \
  (code == 0x0D ? 0x4A : /* Keypad 0 */ \
  0)))))

#define SWITCH_79_CODE(code) \
  (code == 0x1B ? 0x10 : /* Cursor Up */ \
  (code == 0x11 ? 0x18 : /* Cursor Down */ \
  (code == 0x0D ? 0x20 : /* Cursor Left */ \
  (code == 0x05 ? 0x28 : /* Cursor Right */ \
  (code == 0x25 ? 0x30 : /* Keypad 0 */ \
  (code == 0x27 ? 0x38 : /* Keypad 1 */ \
  (code == 0x29 ? 0x40 : /* Keypad 2 */ \
  (code == 0x2B ? 0x48 : /* Keypad 3 */ \
  (code == 0x2D ? 0x50 : /* Keypad 4 */ \
  (code == 0x2F ? 0x58 : /* Keypad 5 */ \
  (code == 0x31 ? 0x60 : /* Keypad 6 */ \
  (code == 0x33 ? 0x68 : /* Keypad 7 */ \
  (code == 0x37 ? 0x70 : /* Keypad 8 */ \
  (code == 0x39 ? 0x78 : /* Keypad 9 */ \
  (code == 0x03 ? 0x12 : /* Keypad Dot */ \
  (code == 0x19 ? 0x1A : /* Keypad Enter */ \
  (code == 0x1D ? 0x22 : /* Keypad Minus */ \
  0)))))))))))))))))
// clang-format on

/**
 * @brief Process Apple M0110 Keyboard Scancode Data
 * 
 * The Apple M0110 protocol encoding:
 * - Bit 7: 1 = key up, 0 = key down
 * - Bit 0: Always 1 (for key transitions)
 * - Bits 6-1: Key code
 * 
 * @param code The scancode to process
 */
void process_scancode(uint8_t code) {
  // Validate that bit 0 is set (should always be 1 for key transitions)
  if ((code & 0x01) == 0) {
    LOG_DEBUG("Invalid M0110 scancode (bit 0 not set): 0x%02X\n", code);
    return;
  }
  
  bool is_key_up = (code & 0x80) != 0;  // Check bit 7
  uint8_t key_code = (code >> 1) & 0x3F; // Extract key code (bits 6-1)
  bool make = !is_key_up;                // Invert for make/break logic
  
  // Handle special cases or validate key codes
  if (key_code == 0x00) {
    LOG_DEBUG("Reserved key code in M0110 scancode: 0x%02X\n", code);
    return;
  }
  
  // Process the key event
  handle_keyboard_report(key_code, make);
  
  LOG_DEBUG("Apple M0110 Key: 0x%02X (%s) [raw: 0x%02X]\n", 
         key_code, make ? "make" : "break", code);
}