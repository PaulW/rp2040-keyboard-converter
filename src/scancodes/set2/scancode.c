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
// AT Keyboards use EO-prefixed codes for some keys.
// This macro translates these codes.
#define SWITCH_E0_CODE(code) \
  (code == 0x11 ? 0x0F : /* Right Alt */ \
  (code == 0x14 ? 0x19 : /* Right Ctrl */ \
  (code == 0x1F ? 0x17 : /* Left GUI */ \
  (code == 0x27 ? 0x1F : /* Right GUI */ \
  (code == 0x2F ? 0x27 : /* Menu/App */ \
  (code == 0x4A ? 0x60 : /* Keypad / */ \
  (code == 0x5A ? 0x62 : /* Keypad Enter */ \
  (code == 0x69 ? 0x5C : /* End */ \
  (code == 0x6B ? 0x53 : /* Cursor Left */ \
  (code == 0x6C ? 0x2F : /* Home */ \
  (code == 0x70 ? 0x39 : /* Insert */ \
  (code == 0x71 ? 0x37 : /* Delete */ \
  (code == 0x72 ? 0x3F : /* Cursor Down */ \
  (code == 0x74 ? 0x47 : /* Cursor Right */ \
  (code == 0x75 ? 0x4F : /* Cursor Up */ \
  (code == 0x77 ? 0x00 : /* Unicomp New Model M Pause/Break key fix */ \
  (code == 0x7A ? 0x56 : /* Page Down */ \
  (code == 0x7D ? 0x5E : /* Page Up */ \
  (code == 0x7C ? 0x7F : /* Print Screen */ \
  (code == 0x7E ? 0x00 : /* Control'd Pause */ \
  (code == 0x21 ? 0x65 : /* Volume Down */ \
  (code == 0x32 ? 0x6E : /* Volume Up */ \
  (code == 0x23 ? 0x6F : /* Mute */ \
  (code == 0x10 ? 0x08 : /* WWW Search -> F13 */ \
  (code == 0x18 ? 0x10 : /* WWW Favourites -> F14 */ \
  (code == 0x20 ? 0x18 : /* WWW Refresh -> F15 */ \
  (code == 0x28 ? 0x20 : /* WWW Stop -> F16 */ \
  (code == 0x30 ? 0x28 : /* WWW Forward -> F17 */ \
  (code == 0x38 ? 0x30 : /* WWW Back -> F18 */ \
  (code == 0x3A ? 0x38 : /* WWW Home -> F19 */ \
  (code == 0x40 ? 0x40 : /* My Computer -> F20 */ \
  (code == 0x48 ? 0x48 : /* Email -> F21 */ \
  (code == 0x2B ? 0x50 : /* Calculator -> F22 */ \
  (code == 0x34 ? 0x08 : /* Play/Pause -> F13 */ \
  (code == 0x3B ? 0x10 : /* Stop -> F14 */ \
  (code == 0x15 ? 0x18 : /* Previous Track -> F15 */ \
  (code == 0x4D ? 0x20 : /* Next Track -> F16 */ \
  (code == 0x50 ? 0x28 : /* Media Select -> F17 */ \
  (code == 0x5E ? 0x50 : /* ACPI Wake -> F22 */ \
  (code == 0x3F ? 0x57 : /* ACPI Sleep -> F23 */ \
  (code == 0x37 ? 0x5F : /* ACPI Power -> F24 */ \
  0)))))))))))))))))))))))))))))))))))))))))
// clang-format on

// Called when character data exists in the ringbuffer from the main processing loop.
// Handles keycodes sent to it, and translates these to Scancode Set 3 and then sending
// a new HID report to the host to signal key press/release.
void process_scancode(uint8_t code) {
  static enum {
    INIT,
    F0,
    E0,
    E0_F0,
    E1,
    E1_14,
    E1_F0,
    E1_F0_14,
    E1_F0_14_F0
  } state = INIT;

  switch (state) {
    case INIT:
      switch (code) {
        case 0xE0:
          state = E0;
          break;
        case 0xF0:
          state = F0;
          break;
        case 0xE1:
          state = E1;
          break;
        case 0x83:  // F7
          handle_keyboard_report(0x02, true);
          break;
        case 0x84:  // SysReq / Alt'd PrintScreen
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
      state = INIT;
      switch (code) {
        case 0x83:  // F7
          handle_keyboard_report(0x02, false);
          break;
        case 0x84:  // SysReq
          handle_keyboard_report(0x7F, false);
          break;
        default:
          if (code < 0x80) {
            handle_keyboard_report(code, false);
          } else {
            printf("[DBG] !F0! (0x%02X)\n", code);
          }
      }
      break;

    case E0:  // E0-Prefixed
      switch (code) {
        case 0x12:  // to be ignored
        case 0x59:  // to be ignored
          state = INIT;
          break;
        case 0xF0:
          state = E0_F0;
          break;
        default:
          state = INIT;
          if (code < 0x80) {
            handle_keyboard_report(SWITCH_E0_CODE(code), true);
          } else {
            printf("[DBG] !E0! (0x%02X)\n", code);
          }
      }
      break;

    case E0_F0:  // Break code of E0-prefixed
      state = INIT;
      switch (code) {
        case 0x12:  // to be ignored
        case 0x59:  // to be ignored
          break;
        default:
          if (code < 0x80) {
            handle_keyboard_report(SWITCH_E0_CODE(code), false);
          } else {
            printf("!E0_F0! (0x%02X)\n", code);
          }
      }
      break;

    case E1:  // E1-Prefixed
      switch (code) {
        case 0x14:
          state = E1_14;
          break;
        case 0xF0:
          state = E1_F0;
          break;
        default:
          state = INIT;
          printf("[DBG] !E1! (0x%02X)\n", code);
      }
      break;

    case E1_14:  // E1-prefixed 14
      state = INIT;
      switch (code) {
        case 0x77:  // Pause
          handle_keyboard_report(SWITCH_E0_CODE(code), true);
          break;
        default:
          printf("[DBG] !E1_14! (0x%02X)\n", code);
      }
      break;

    case E1_F0:  // Break code of E1-prefixed
      switch (code) {
        case 0x14:
          state = E1_F0_14;
          break;
        default:
          state = INIT;
          printf("[DBG] !E1_F0! (0x%02X)\n", code);
      }
      break;

    case E1_F0_14:  // Break code of E1-prefixed 14
      switch (code) {
        case 0xF0:  // Pause
          state = E1_F0_14_F0;
          break;
        default:
          state = INIT;
          printf("[DBG] !E1_F0_14! (0x%02X)\n", code);
      }
      break;

    case E1_F0_14_F0:  // Break code of E1-prefixed 14
      state = INIT;
      switch (code) {
        case 0x77:  // Pause
          handle_keyboard_report(SWITCH_E0_CODE(code), false);
          break;
        default:
          printf("[DBG] !E1_F0_14_F0! (0x%02X)\n", code);
      }
      break;

    default:
      state = INIT;
  }
}