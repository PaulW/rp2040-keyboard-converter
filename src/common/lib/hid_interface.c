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
#include "log.h"

#include "bsp/board.h"
#include "command_mode.h"
#include "config.h"
#include "hid_keycodes.h"
#include "keymaps.h"
#include "led_helper.h"
#include "tusb.h"
#include "usb_descriptors.h"

/**
 * @brief HID Interface Usage Pages
 * 
 * USB HID defines different "usage pages" for different types of input devices.
 * These constants identify the usage page in HID reports.
 */
enum {
  USAGE_PAGE_KEYBOARD = 0x0,   /**< Standard keyboard usage page */
  USAGE_PAGE_CONSUMER = 0xC,   /**< Consumer control usage page (multimedia keys) */
};

/**
 * @brief HID Report Structures
 * 
 * These static variables maintain the current state of HID reports.
 * They are only accessed from the main task context, so no volatile
 * qualification or synchronization is needed.
 * 
 * Threading Model:
 * - All HID report functions called from main task context
 * - Ring buffer provides thread-safe boundary from interrupt handlers
 * - No direct interrupt access to these structures
 */
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
  // Build the entire report string in one buffer for single LOG_DEBUG call
  // Maximum HID report size is typically small (keyboard=8, mouse=5, consumer=2)
  // Buffer: prefix (32) + hex bytes (size * 3) + null terminator
  char buffer[128];
  size_t offset = (size_t)snprintf(buffer, sizeof(buffer), "[%s-HID-REPORT] ", message);
  
  uint8_t* p = (uint8_t*)report;
  for (size_t i = 0; i < size && offset < sizeof(buffer) - 4; i++) {
    offset += (size_t)snprintf(buffer + offset, sizeof(buffer) - offset, "%02X ", *p++);
  }
  
  LOG_DEBUG("%s\n", buffer);
}

/**
 * @brief Adds a key to the HID keyboard report
 * 
 * This function adds a key to the current HID keyboard report structure.
 * The HID keyboard report maintains state for:
 * - 8 modifier keys (Shift, Ctrl, Alt, GUI) in a bit field
 * - Up to 6 simultaneous regular key presses (6-key rollover)
 * 
 * Modifier Key Handling:
 * - Modifier keys (0xE0-0xE7) set bits in the modifier field
 * - Each modifier has its own bit position (Ctrl=0, Shift=1, Alt=2, GUI=3)
 * - Left and right variants use separate bits (e.g., Left Ctrl vs Right Ctrl)
 * 
 * Regular Key Handling:
 * - Regular keys stored in 6-element keycode array
 * - Function searches for empty slot (value 0)
 * - Duplicate keys ignored (prevents multiple entries for same key)
 * - Returns false if all 6 slots are full (keyboard rollover limit)
 * 
 * 6-Key Rollover Limitation:
 * - USB HID boot protocol supports maximum 6 simultaneous keys
 * - 7th key press will be ignored (function returns false)
 * - Could implement NKRO (N-key rollover) with custom report descriptor
 * 
 * @param key The HID keycode to add (modifier or regular key)
 * @return true if key was added, false if already present or buffer full
 * 
 * @note This function is called from main task context only
 * @note Modifier check uses IS_MOD() macro (checks range 0xE0-0xE7)
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
 * @brief Removes a key from the HID keyboard report when released
 * 
 * This function removes a key from the current HID keyboard report structure
 * when a key release event is detected. It handles both modifier keys and
 * regular keys appropriately.
 * 
 * Modifier Key Handling:
 * - Clears the corresponding bit in the modifier field
 * - Uses bitwise AND with inverted mask to clear specific bit
 * - Only clears if bit was set (returns false if already clear)
 * 
 * Regular Key Handling:
 * - Searches keycode array for matching key
 * - Sets matching entry to 0 (marks slot as empty)
 * - Subsequent keys can use freed slots
 * 
 * Report Optimization:
 * - Only returns true if report actually changed
 * - Prevents unnecessary USB HID report transmission
 * - Reduces USB bus traffic and improves performance
 * 
 * @param key The HID keycode to remove (modifier or regular key)
 * @return true if key was removed, false if key was not present
 * 
 * @note This function is called from main task context only
 * @note Pairs with hid_keyboard_add_key() for make/break handling
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
 * @brief Handles input reports for keyboard and consumer control devices
 * 
 * This is the main entry point for processing translated scan codes and sending
 * USB HID reports to the host. It handles three types of HID reports:
 * 
 * 1. Keyboard Reports (Standard Keys and Modifiers):
 *    - Translates interface scan codes to HID keycodes via keymap lookup
 *    - Updates keyboard report structure (modifiers + 6-key array)
 *    - Only sends USB report if state changed (prevents duplicate reports)
 *    - Handles typematic prevention (host handles key repeat, not device)
 * 
 * 2. Consumer Control Reports (Multimedia Keys):
 *    - Media keys (play, pause, volume, etc.)
 *    - Uses separate USB HID interface for consumer controls
 *    - Single 16-bit usage code per report
 *    - Code 0 indicates "no keys pressed" for key release
 * 
 * 3. Command Mode System:
 *    - Time-based special function access for 2KRO keyboards
 *    - Hold both shifts for 3 seconds to enter command mode
 *    - LED provides visual feedback (Green/Blue alternating flash)
 *    - Press 'B' for bootloader entry (other commands can be added)
 *    - 3 second timeout or shift release exits command mode
 *    - Keyboard reports suppressed during command mode operation
 * 
 * Command Mode vs. Legacy Macro System:
 * - Old: Action key + both shifts + 'B' (simultaneous press - fails on 2KRO)
 * - New: Hold both shifts 3s, then press 'B' (sequential - works on 2KRO)
 * - Legacy macro system (keymap_is_action_key_pressed) removed
 * - Command mode provides better UX with LED feedback
 * 
 * Report Deduplication:
 * - Uses report_modified flag to track actual state changes
 * - Some keyboards send typematic repeats (same key repeatedly)
 * - HID spec: host handles key repeat, device sends state changes only
 * - Prevents USB bus saturation from redundant reports
 * 
 * Error Handling:
 * - Logs failed USB transmissions with report contents
 * - Continues operation on failure (transient USB issues)
 * - Could be enhanced with retry queue for critical reports
 * 
 * @param code Interface scan code (protocol-normalized, not raw scan code)
 * @param make true for key press, false for key release
 * 
 * @note Called from main task context via process_scancode()
 * @note Thread-safe: no interrupt access to keyboard_report or mouse_report
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

    // Process command mode state machine
    // Returns false if keyboard report should be suppressed (command mode active)
    bool allow_normal_processing = command_mode_process(&keyboard_report);
    
    if (!allow_normal_processing) {
      // Command mode is active, suppress normal keyboard reports
      // This prevents shift keys and command keys from being sent to host
      return;
    }

    if (report_modified) {
      bool res = tud_hid_n_report(ITF_NUM_KEYBOARD, REPORT_ID_KEYBOARD, &keyboard_report,
                                  sizeof(keyboard_report));
      if (!res) {
        LOG_ERROR("Keyboard HID Report Failed:\n");
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
      LOG_ERROR("Consumer HID Report Failed: 0x%04X\n", usage);
    }
  }
}

/**
 * @brief Sends an empty keyboard report to release all keys
 * 
 * This function sends a HID keyboard report with no keys pressed and no
 * modifiers active. Used by command mode when transitioning from
 * SHIFT_HOLD_WAIT to COMMAND_ACTIVE to ensure that the shift keys are
 * released from the host's perspective.
 * 
 * Report Structure:
 * - modifier: 0x00 (no modifiers)
 * - reserved: 0x00
 * - keycode[0-5]: all 0x00 (no keys pressed)
 * 
 * @note Called from command mode when entering COMMAND_ACTIVE state
 * @note Does not modify the internal keyboard_report state
 */
void send_empty_keyboard_report(void) {
  hid_keyboard_report_t empty_report = {
    .modifier = 0,
    .reserved = 0,
    .keycode = {0, 0, 0, 0, 0, 0}
  };
  
  bool res = tud_hid_n_report(ITF_NUM_KEYBOARD, REPORT_ID_KEYBOARD, &empty_report,
                              sizeof(empty_report));
  if (!res) {
    LOG_ERROR("Empty keyboard report failed\n");
  } else {
    LOG_INFO("Sent empty keyboard report (all keys released)\n");
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
    LOG_ERROR("Mouse HID Report Failed:\n");
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
