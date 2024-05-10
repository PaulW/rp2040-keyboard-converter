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

#include "hid_interface.h"

#include <stdio.h>

#include "bsp/board.h"
#include "config.h"
#include "hid_keycodes.h"
#include "keymaps.h"
#include "led_helper.h"
#include "pico/bootrom.h"
#include "tusb.h"
#include "usb_descriptors.h"

enum {
  USAGE_PAGE_KEYBOARD = 0x0,
  USAGE_PAGE_CONSUMER = 0xC,
};

static hid_keyboard_report_t keyboard_report;
static hid_mouse_report_t mouse_report;

// Output the HID Report sent to the host.  Purely for debugging.
static void hid_print_report(void* report, size_t size, const char* message) {
  printf("[%s-HID-REPORT] ", message);
  uint8_t* p = (uint8_t*)report;
  for (size_t i = 0; i < size; i++) {
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

  for (size_t i = 0; i < 6; i++) {
    if (keyboard_report.keycode[i] == key) return false;
    if (keyboard_report.keycode[i] == 0) {
      keyboard_report.keycode[i] = key;
      return true;
    }
  }
  return false;
}

// Remove keycode from HID report when key is released.
static bool hid_keyboard_del_key(uint8_t key) {
  if (IS_MOD(key)) {
    uint8_t modifier_bit = (uint8_t)(1 << (key & 0x7));
    if ((keyboard_report.modifier & modifier_bit) != 0) {
      keyboard_report.modifier &= (uint8_t)~modifier_bit;
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
void handle_keyboard_report(uint8_t code, bool make) {
  // Convert the Interface Scancode to a HID Keycode
  code = keymap_get_key_val(code, make);
  if (IS_KEY(code) || IS_MOD(code)) {
    bool report_modified = false;
    if (make) {
      report_modified = hid_keyboard_add_key(code);
    } else {
      report_modified = hid_keyboard_del_key(code);
    }

    // Check for any Macro Combinations here
    if (keymap_is_action_key_pressed()) {
      if (SUPER_MACRO_INIT(keyboard_report.modifier)) {
        printf("[DBG] Macro Modifiers HOLD\n");
        uint8_t macro_key = MACRO_KEY_CODE(code);
        if (macro_key == KC_BOOT) {
          // Initiate Bootloader
          printf("[INFO] Initiate Bootloader\n");
#ifdef CONVERTER_LEDS
          // Set Status LED to indicate Bootloader Mode
          converter.state.fw_flash = 1;
          update_converter_status();
#endif
          // Reboot into Bootloader
          reset_usb_boot(0, 0);
        }
      }
    }

    if (report_modified) {
      bool res = tud_hid_n_report(ITF_NUM_KEYBOARD, REPORT_ID_KEYBOARD, &keyboard_report, sizeof(keyboard_report));
      if (!res) {
        printf("[ERR] Keyboard HID Report Failed:\n");
        hid_print_report(&keyboard_report, sizeof(keyboard_report), "handle_keyboard_report");
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
      printf("[ERR] Consumer HID Report Failed: 0x%04X\n", usage);
    }
  }
}

void handle_mouse_report(const uint8_t buttons[5], int8_t pos[3]) {
  // Handle Mouse Report
  mouse_report.buttons = buttons[0] | (buttons[1] << 1) | (buttons[2] << 2) | (buttons[3] << 3) | (buttons[4] << 4);
  mouse_report.x = pos[0];
  mouse_report.y = pos[1];
  mouse_report.wheel = pos[2];
  bool res = tud_hid_n_report(KEYBOARD_ENABLED ? ITF_NUM_MOUSE : ITF_NUM_KEYBOARD, REPORT_ID_MOUSE, &mouse_report, sizeof(mouse_report));
  if (!res) {
    printf("[ERR] Mouse HID Report Failed:\n");
    hid_print_report(&mouse_report, sizeof(mouse_report), "handle_mouse_report");
  }
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
void hid_device_setup(void) {
  board_init();
  tusb_init();
}
