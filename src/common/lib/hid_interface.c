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

/**
 * @brief Prints the contents of a HID report.
 * This function takes a HID report, its size, and a message as input and prints the contents of the
 * report in hexadecimal format. The message is prefixed to the printed output for identification
 * purposes.
 *
 * @param report   Pointer to the HID report.
 * @param size     Size of the HID report.
 * @param message  Message to be printed before the HID report.
 */
static void hid_print_report(void* report, size_t size, const char* message) {
  printf("[%s-HID-REPORT] ", message);
  uint8_t* p = (uint8_t*)report;
  for (size_t i = 0; i < size; i++) {
    printf("%02X ", *p++);
  }
  printf("\n");
}

/**
 * @brief Adds a key to the HID keyboard report.
 * This function is responsible for adding a key to the HID keyboard report. The HID keyboard report
 * is a data structure that represents the state of the keyboard, including pressed keys and
 * modifier keys (e.g., Shift, Ctrl, Alt). The function first checks if the key is a modifier key.
 *
 * If it is, it checks if the modifier key is already pressed. If not, it sets the corresponding bit
 * in the modifier field of the keyboard report. If the key is not a modifier key, it searches for
 * an empty slot in the keycode array of the keyboard report and adds the key to that slot. The
 * function returns true if the key was successfully added, and false otherwise.
 *
 * @param key The key to be added.
 *
 * @return True if the key was successfully added, false otherwise.
 */
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

/**
 * @brief Removes a keycode from the HID report when a key is released.
 * This function checks if the given key is a modifier key. If it is, the corresponding modifier bit
 * is cleared in the keyboard report. If the key is not a modifier key, it is searched for in the
 * keycode array of the keyboard report and removed if found.
 *
 * @param key The keycode to be removed.
 *
 * @return true if the keycode was successfully removed, false otherwise.
 */
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

/**
 * @brief Handles input reports for the keyboard usage page.
 * Only if the Keyboard Report changes do we then call `hid_send_report_with_retry`. Some PS2
 * Keyboards and other types send typematic key presses repetitively, yet with HID devices, we only
 * want to send a report for each Key Press/Release Event. The host will handle all typematic events
 * itself.
 *
 * @param code The interface scancode of the key.
 * @param make A boolean indicating whether the key is being pressed (true) or released (false).
 */
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
      bool res = tud_hid_n_report(ITF_NUM_KEYBOARD, REPORT_ID_KEYBOARD, &keyboard_report,
                                  sizeof(keyboard_report));
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
    bool res = tud_hid_n_report(ITF_NUM_CONSUMER_CONTROL, REPORT_ID_CONSUMER_CONTROL, &usage,
                                sizeof(usage));
    if (!res) {
      printf("[ERR] Consumer HID Report Failed: 0x%04X\n", usage);
    }
  }
}

/**
 * @brief Handles the mouse report.
 * This function handles the mouse report by updating the mouse_report structure with the provided
 * button states and position values. It then sends the updated report to the USB HID interface
 * using the tud_hid_n_report function. If the report fails to send, an error message is printed and
 * the report is printed for debugging purposes.
 *
 * @param buttons An array of uint8_t representing the button states.
 * @param pos An array of int8_t representing the mouse position values (x, y, wheel).
 */
void handle_mouse_report(const uint8_t buttons[5], int8_t pos[3]) {
  // Handle Mouse Report
  mouse_report.buttons =
      buttons[0] | (buttons[1] << 1) | (buttons[2] << 2) | (buttons[3] << 3) | (buttons[4] << 4);
  mouse_report.x = pos[0];
  mouse_report.y = pos[1];
  mouse_report.wheel = pos[2];
  bool res = tud_hid_n_report(KEYBOARD_ENABLED ? ITF_NUM_MOUSE : ITF_NUM_KEYBOARD, REPORT_ID_MOUSE,
                              &mouse_report, sizeof(mouse_report));
  if (!res) {
    printf("[ERR] Mouse HID Report Failed:\n");
    hid_print_report(&mouse_report, sizeof(mouse_report), "handle_mouse_report");
  }
}

/**
 * @brief Callback function invoked when a GET_REPORT control request is received.
 * This function is called when a GET_REPORT control request is received by the application. The
 * application should fill the buffer with the content of the report and return the length of the
 * report. If the application returns zero, the request will be stalled by the stack.
 *
 * @param instance    The instance of the HID interface.
 * @param report_id   The ID of the report.
 * @param report_type The type of the report.
 * @param buffer      The buffer to store the report content.
 * @param reqlen      The length of the request.
 *
 * @return The length of the report. Returning zero will cause the stack to stall the request.
 *
 * @note This function is not implemented, and always returns zero.
 */
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type,
                               uint8_t* buffer, uint16_t reqlen) {
  // TODO not Implemented
  (void)instance;
  (void)report_id;
  (void)report_type;
  (void)buffer;
  (void)reqlen;

  return 0;
}

/**
 * @brief Callback function invoked when a SET_REPORT control request is received or data is
 * received on the OUT endpoint.
 * This is responsible for handling HID reports dependant on the report type and report ID. The
 * function is called when a SET_REPORT control request is received or data is received on the OUT
 * endpoint. The function should parse the received HID report and take the appropriate action based
 * on the report type and report ID.
 *
 * @param instance    The instance number of the HID interface.
 * @param report_id   The report ID of the received HID report.
 * @param report_type The type of the received HID report.
 * @param buffer      Pointer to the buffer containing the received HID report data.
 * @param bufsize     The size of the received HID report data buffer.
 *
 * @note This function only handles the keyboard LED report.
 */
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type,
                           uint8_t const* buffer, uint16_t bufsize) {
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

/**
 * @brief Sets up the HID device by initializing the board and the tinyusb stack.
 * This function should be called before using any HID device functionality. It initializes the
 * board and the tinyusb stack, allowing the device to communicate with the host.
 */
void hid_device_setup(void) {
  board_init();
  tusb_init();
}
