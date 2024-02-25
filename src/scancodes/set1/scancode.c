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

// clang-format off
// Some XT Keyboards use EO-prefixed codes for some keys.
// This macro translates these codes.
#define SWITCH_E0_CODE(code) \
  (code == 0x37 ? 0x54 : /* Print Screen */ \
  (code == 0x46 ? 0x55 : /* Pause / Break */ \
  (code == 0x5E ? 0x70 : /* Power */ \
  (code == 0x5F ? 0x79 : /* Sleep */ \
  (code == 0x63 ? 0x7B : /* Wake */ \
  (code == 0x52 ? 0x71 : /* Insert */ \
  (code == 0x47 ? 0x74 : /* Home */ \
  (code == 0x49 ? 0x77 : /* Page Up */ \
  (code == 0x35 ? 0x7F : /* Keypad Slash */ \
  (code == 0x53 ? 0x72 : /* Delete */ \
  (code == 0x4F ? 0x75 : /* End */ \
  (code == 0x51 ? 0x78 : /* Page Down */ \
  (code == 0x48 ? 0x60 : /* Up Arrow */ \
  (code == 0x1C ? 0x6F : /* Keypad Enter */ \
  (code == 0x5B ? 0x5A : /* Left GUI */ \
  (code == 0x38 ? 0x7C : /* Right Alt */ \
  (code == 0x5C ? 0x5B : /* Right GUI */ \
  (code == 0x5D ? 0x5C : /* Application */ \
  (code == 0x1D ? 0x7A : /* Right Control */ \
  (code == 0x4B ? 0x61 : /* Left Arrow */ \
  (code == 0x50 ? 0x62 : /* Down Arrow */ \
  (code == 0x4D ? 0x63 : /* Right Arrow */ \
  0))))))))))))))))))))))
// clang-format on

// Called when character data exists in the ringbuffer from the main processing loop.
// Handles keycodes sent to it, and translates these to Scancode Set 3 and then sending
// a new HID report to the host to signal key press/release.
void process_scancode(uint8_t code) {
  static enum {
    INIT,
    E0,
    E1,
    E1_1D,
    E1_9D
  } state = INIT;

  switch (state) {
    case INIT:
      switch (code) {
        case 0xE0:
          state = E0;
          break;
        case 0xE1:
          state = E1;
          break;
        default:  // Handle normal key event
          state = INIT;
          if (code < 0x80) {
            handle_keyboard_report(code, true);
          } else {
            handle_keyboard_report(code & 0x7F, false);
          }
      }
      break;

    case E0:  // E0-Prefixed
      switch (code) {
        case 0x2A:
        case 0xAA:
        case 0x36:
        case 0xB6:
          //ignore fake shift
          state = INIT;
          break;
        default:
          state = INIT;
          if (code < 0x80) {
            handle_keyboard_report(SWITCH_E0_CODE(code), true);
          } else {
            handle_keyboard_report(SWITCH_E0_CODE((code & 0x7F)), false);
          }
      }
      break;

    case E1:  // E1-Prefixed
      switch (code) {
        case 0x1D:
          state = E1_1D;
          break;
        case 0x9D:
          state = E1_9D;
          break;
        default:
          state = INIT;
          break;
      }
      break;

    case E1_1D:  // E1-prefixed 1D
      switch (code) {
        case 0x45:
          handle_keyboard_report(0x55, true);
          state = INIT;
          break;
        default:
          state = INIT;
          printf("[DBG] !E1_1D! (0x%02X)\n", code);
      }
      break;

    case E1_9D:  // E1-prefixed 9D
      switch (code) {
        case 0xC5:
          handle_keyboard_report(0x55, false);
          state = INIT;
          break;
        default:
          state = INIT;
          printf("[DBG] !E1_9D! (0x%02X)\n", code);
      }
      break;

    default:
      state = INIT;
  }
}