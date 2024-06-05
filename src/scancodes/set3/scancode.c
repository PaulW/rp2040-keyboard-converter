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
 * This function is called from the keyboard_interface_task() function whenever there is data in the
 * ringbuffer.  It will then process the relevant scancode and then call handle_keyboard_report() to
 * send the relevant HID report to the host.  Key press and release events are also determined here
 * depending on the scancode sequence relating to any received Break code (0xF0).
 *
 * @param code The keycode to process.
 *
 * @note handle_keyboard_report() function directly handles translation from scancode to HID report.
 * It used a lookup against the relevant keyboard configuration to determine the associated Keycode,
 * and then sends the relevant HID report to the host.
 *
 * @note With Scancode Set 3, most keyboard default to typematic mode, which means that the keyboard
 * will send a key press event, followed by a delay, and then a stream of key press events. However,
 * we assume that the keyboard has been configured to send make/break codes, and as such we don't
 * need to handle typematic mode, and will process release events from the Break code.
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