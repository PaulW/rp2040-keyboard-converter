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
#include "hid_keycodes.h"
#include "keymap.h"
#include "lock_leds.h"
#include "tusb.h"
#include "usb_descriptors.h"

static hid_keyboard_report_t keyboard_report;

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
  if (IS_MOD(key)) {
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
  if (IS_MOD(key)) {
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
static void handle_keyboard_report(uint8_t code, bool make) {
  // uint8_t key = (uint8_t)(code & 0xFF);
  if (IS_KEY(code) || IS_MOD(code)) {
    bool report_modified = false;
    if (make) {
      report_modified = hid_keyboard_add_key(code);
    } else {
      report_modified = hid_keyboard_del_key(code);
    }

    if (report_modified) {
      bool res = tud_hid_n_report(ITF_NUM_KEYBOARD, REPORT_ID_KEYBOARD, &keyboard_report, sizeof(keyboard_report));
      if (!res) {
        printf("[ERR] KEYBOARD HID REPORT FAIL\n");
        hid_print_report();
      }
    }
  } else if (IS_CONSUMER(code)) {
    uint16_t usage;
    if (make) {
      usage = CODE_TO_CONSUMER(code);
    } else {
      usage = 0;
    }
    bool res = tud_hid_n_report(ITF_NUM_CONSUMER_CONTROL, REPORT_ID_CONSUMER_CONTROL, &usage, sizeof(usage));
    if (!res) {
      printf("[ERR] Consumer Report Failed: 0x%04X\n", usage);
    } else {
      printf("[DBG] Consumer Report Sent: 0x%04X\n", usage);
    }
  } else {
    printf("[ERR] Unsupported KeyCode presented: 0x%02X\n", code);
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
          handle_keyboard_report(keymap_get_key_val(0, code), true);
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
          handle_keyboard_report(keymap_get_key_val(0, code), false);
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
