/*
 * This file is part of Model-F 5170 Converter.
 *
 * Copyright 2023 Paul Bramhall (paulwamp@gmail.com)
 *
 * Model-F 5170 Converter is free software: you can redistribute it
 * and/or modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * Model-F 5170 Converter is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Model-F 5170 Converter.
 * If not, see <https://www.gnu.org/licenses/>.
 */

#include "hid_interface.h"

#include <stdio.h>

#include "bsp/board.h"
#include "lock_leds.h"
#include "tusb.h"
#include "usb_descriptors.h"

static hid_keyboard_report_t keyboard_report;

static const uint16_t sc3_to_hid[] = {
    // IBM PC/AT Keyboard (5170) uses a subset of Scancode 3.
    // This is set from the standard key layout (UK) on IBM 6450225.
    // ,-------. ,-----------------------------------------------------------. ,---------------.
    // | F1| F2| |  \|  1|  2|  3|  4|  5|  6|  7|  8|  9|  0|  -|  =|  #| BS| |Esc|NmL|ScL|SyR|
    // |-------| |-----------------------------------------------------------| |---------------|
    // | F3| F4| |Tab  |  Q|  W|  E|  R|  T|  Y|  U|  I|  O|  P|  [|  ]|     | |  7|  8|  9|  *|
    // |-------| |-----------------------------------------------------|     | |-----------|---|
    // | F5| F6| |Ctrl  |  A|  S|  D|  F|  G|  H|  J|  K|  L|  ;|  '|     Ret| |  4|  5|  6|  -|
    // |-------| |-----------------------------------------------------------| |---------------|
    // | F7| F8| |Shift   |  Z|  X|  C|  V|  B|  N|  M|  ,|  ,|  /|     Shift| |  1|  2|  3|   |
    // |-------| |-----------------------------------------------------------| |-----------|  +|
    // | F9|F10| |Alt |    |                  Space                |    |CapL| |      0|  .|   |
    // `-------' `----'    `---------------------------------------'    `----' `---------------'
    //
    // Scan Code Set 3:
    // ,-------. ,-----------------------------------------------------------. ,---------------.
    // | 05| 06| | 0E| 16| 1E| 26| 25| 2E| 36| 3D| 3E| 46| 45| 4E| 55| 5D| 66| | 76| 77| 7E| 84|
    // |-------| |-----------------------------------------------------------| |---------------|
    // | 04| 0C| | 0D  | 15| 1D| 24| 2D| 2C| 35| 3C| 43| 44| 4D| 54| 5B|     | | 6C| 75| 7D| 7C|
    // |-------| |-----------------------------------------------------|     | |---------------|
    // | 03| 0B| | 14   | 1C| 1B| 23| 2B| 34| 33| 3B| 42| 4B| 4C| 52|     5A | | 6B| 73| 74| 7B|
    // |-------| |-----------------------------------------------------------| |---------------|
    // | 83| 0A| | 12     | 1A| 22| 21| 2A| 32| 31| 3A| 41| 49| 4A|       59 | | 69| 72| 7A| 79|
    // |-------| |-----------------------------------------------------------| |-----------|   |
    // | 01| 09| | 11  |   |                   29                  |   |  58 | |     70| 71|   |
    // `-------' `-----'   `---------------------------------------'   `-----' `---------------'
    //
    //   0       1       2       3       4       5       6       7       8       9       A       B       C       D       E       F
    0x0000, 0x0042, 0x0000, 0x003E, 0x003C, 0x003A, 0x003B, 0x0000, 0x0000, 0x0043, 0x0041, 0x003F, 0x003D, 0x002B, 0x0035, 0x0000,  // 0
    0x0000, 0x00E2, 0x00E1, 0x0000, 0x00E0, 0x0014, 0x001E, 0x0000, 0x0000, 0x0000, 0x001D, 0x0016, 0x0004, 0x001A, 0x001F, 0x0000,  // 1
    0x0000, 0x0006, 0x001B, 0x0007, 0x0008, 0x0021, 0x0020, 0x0000, 0x0000, 0x002C, 0x0019, 0x0009, 0x0017, 0x0015, 0x0022, 0x0000,  // 2
    0x0000, 0x0011, 0x0005, 0x000B, 0x000A, 0x001C, 0x0023, 0x0000, 0x0000, 0x0000, 0x0010, 0x000D, 0x0018, 0x0024, 0x0025, 0x0000,  // 3
    0x0000, 0x0036, 0x000E, 0x000C, 0x0012, 0x0027, 0x0026, 0x0000, 0x0000, 0x0037, 0x0038, 0x000F, 0x0033, 0x0013, 0x002D, 0x0000,  // 4
    0x0000, 0x0000, 0x0034, 0x0000, 0x002F, 0x002E, 0x0000, 0x0000, 0x0039, 0x00E5, 0x0028, 0x0030, 0x0000, 0x0031, 0x0000, 0x0000,  // 5
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x002A, 0x0000, 0x0000, 0x0059, 0x0000, 0x005C, 0x005F, 0x0000, 0x0000, 0x0000,  // 6
    0x0062, 0x0063, 0x005A, 0x005D, 0x005E, 0x0060, 0x0029, 0x0053, 0x0000, 0x0057, 0x005B, 0x0056, 0x0055, 0x0061, 0x0047, 0x0000,  // 7
    0x0000, 0x0000, 0x0000, 0x0040, 0x009A, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,  // 8
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,  // 9
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,  // A
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,  // B
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,  // C
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,  // D
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,  // E
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,  // F
};

enum {
  USAGE_PAGE_KEYBOARD = 0x0,
  USAGE_PAGE_CONSUMER = 0xC,
};

// Output the HID Report sent to the host.  Purely for debugging.
static void hid_print_report(void) {
  printf("[HID-REPORT] ");
  uint8_t* p = (uint8_t*)&keyboard_report;
  for (uint i = 0; i < sizeof(keyboard_report); i++) {
    printf("%02X ", *p++);
  }
  printf("\n");
}

// Add a new keycode to the HID report upon keypress.
static bool hid_keyboard_add_key(uint8_t key) {
  if (key >= 0xE0 && key <= 0xE8) {
    if ((keyboard_report.modifier & (uint8_t)(1 << (key & 0x7))) == 0) {
      keyboard_report.modifier |= (uint8_t)(1 << (key & 0x7));
      return true;
    }
    return false;
  }

  int empty = -1;
  for (size_t i = 0; i < 6; i++) {
    if (keyboard_report.keycode[i] == key) return false;
    if (empty == -1 && keyboard_report.keycode[i] == 0) empty = (int)i;
  }
  if (empty != -1) {
    keyboard_report.keycode[empty] = key;
    return true;
  }
  return false;
}

// Remove keycode from HID report when key is released.
static bool hid_keyboard_del_key(uint8_t key) {
  if (key >= 0xE0 && key <= 0xE8) {
    if ((keyboard_report.modifier & (uint8_t)(1 << (key & 0x7))) != 0) {
      keyboard_report.modifier &= (uint8_t) ~(1 << (key & 0x7));
      return true;
    }
    return false;
  }

  for (size_t i = 0; i < 6; i++) {
    if (keyboard_report.keycode[i] == key) {
      keyboard_report.keycode[i] = 0;
      return true;
    }
  }
  return false;
}

// Handle input reports for the keyboard usage page
// Only if the Keyboard Report changes do we then call `hid_send_report_with_retry`
// PS2 Keyboards send typematic key presses repetatively, yet with HID devices, we
// only want to send a report for each Key Press/Release Event.  The host will handle
// all typematic events itself.
static void handle_keyboard_report(uint16_t code, bool make) {
  uint8_t key = (uint8_t)(code & 0xFF);
  bool report_modified = false;
  if (make) {
    report_modified = hid_keyboard_add_key(key);
  } else {
    report_modified = hid_keyboard_del_key(key);
  }

  if (report_modified) {
    bool res = tud_hid_n_report(ITF_NUM_KEYBOARD, REPORT_ID_KEYBOARD, &keyboard_report, sizeof(keyboard_report));
    if (!res) {
      printf("[ERR] KEYBOARD HID REPORT FAIL\n");
      hid_print_report();
    }
  }
}

// Function to handle input reports.
// Currently we only support/utilise keyboard reports, but we may was consumer/system reports in future.
static void handle_input_report(uint16_t code, bool make) {
  // Get the usage page from the input code
  uint8_t page = (uint8_t)((code & 0xf000) >> 12);

  // Call the appropriate function based on the usage page
  switch (page) {
    case USAGE_PAGE_KEYBOARD:
    case USAGE_PAGE_KEYBOARD + 7:  // keyboard page
      handle_keyboard_report(code, make);
      break;
    case USAGE_PAGE_CONSUMER:  // consumer page
      // Not yet implemented
      break;
    default:
      break;
  }
}

// Called when character data exists in the ringbuffer from the main processing loop.
// Handles keycodes sent to it, and translates these to Scancode Set 3 and then sending
// a new HID report to the host to signal key press/release.
int8_t keyboard_process_key(uint8_t code) {
  static enum {
    WAITING_FOR_INPUT,
    RECEIVED_F0,
  } state = WAITING_FOR_INPUT;

  switch (state) {
    case WAITING_FOR_INPUT:

      switch (code) {
        case 0xF0:
          state = RECEIVED_F0;
          break;
        case 0x00 ... 0x7F:
        case 0x83:  // F7
        case 0x84:  // SysReq
          handle_input_report(sc3_to_hid[code], true);
          break;
        case 0xAA:  // Self-test passed
        default:    // unknown codes
          printf("!WAITING_FOR_INPUT!\n");
          return -1;
      }
      break;
    case RECEIVED_F0:  // Break code
      switch (code) {
        case 0x00 ... 0x7F:
        case 0x83:  // F7
        case 0x84:  // SysReq
          state = WAITING_FOR_INPUT;
          handle_input_report(sc3_to_hid[code], false);
          break;
        default:
          state = WAITING_FOR_INPUT;
          printf("!RECEIVED_F0! %02X\n", code);
          return -1;
      }
      break;
    default:
      state = WAITING_FOR_INPUT;
  }
  return 0;
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen) {
  // TODO not Implemented
  (void)instance;
  (void)report_id;
  (void)report_type;
  (void)buffer;
  (void)reqlen;

  return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {
  (void)buffer;

  if (instance != ITF_NUM_KEYBOARD) return;

  if (report_type == HID_REPORT_TYPE_OUTPUT) {
    // Set keyboard LED e.g Capslock, Numlock etc...
    if (report_id == REPORT_ID_KEYBOARD) {
      // bufsize should be (at least) 1
      if (bufsize < 1) return;
      set_lock_values_from_hid(buffer[0]);
    }
  }
}

// Public function which sets up the tinyusb stack
void hid_device_setup() {
  board_init();
  tusb_init();
}
