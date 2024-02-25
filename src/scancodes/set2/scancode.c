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
      switch (code) {
        case 0x83:  // F7
          handle_keyboard_report(0x02, false);
          break;
        case 0x84:  // SysReq
          handle_keyboard_report(0x7F, false);
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
            handle_keyboard_report(code | 0x80, true);
          } else {
            printf("[DBG] !E0! (0x%02X)\n", code);
          }
      }
      break;

    case E0_F0:  // Break code of E0-prefixed
      switch (code) {
        case 0x12:  // to be ignored
        case 0x59:  // to be ignored
          state = INIT;
          break;
        default:
          state = INIT;
          if (code < 0x80) {
            handle_keyboard_report(code | 0x80, false);
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
      switch (code) {
        case 0x77:  // Pause
          state = INIT;
          handle_keyboard_report(code | 0x80, true);
          break;
        default:
          state = INIT;
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
      switch (code) {
        case 0x77:  // Pause
          state = INIT;
          handle_keyboard_report(code | 0x80, false);
          break;
        default:
          state = INIT;
          printf("[DBG] !E1_F0_14_F0! (0x%02X)\n", code);
      }
      break;

    default:
      state = INIT;
  }
}