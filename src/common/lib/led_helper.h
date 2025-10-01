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

#ifndef LED_HELPER_H
#define LED_HELPER_H

#include <stdint.h>

#include "config.h"

/**
 * @brief Converter State Structure
 * 
 * Tracks the operational state of the keyboard converter hardware.
 * Used to control status LED indicators.
 */
typedef struct converter_state {
  unsigned char kb_ready : 1;     /**< Keyboard initialized and ready */
  unsigned char mouse_ready : 1;  /**< Mouse initialized and ready */
  unsigned char fw_flash : 1;     /**< Firmware flash mode (bootloader) */
} converter_state;

/**
 * @brief Converter State Union
 * 
 * Allows both bitfield access (for setting individual states) and
 * atomic value access (for efficient change detection).
 * 
 * Threading Model:
 * - Written from protocol tasks (keyboard_interface_task, mouse_interface_task)
 * - Read from update_converter_status() in main task context
 * - Value is volatile to ensure visibility across different execution contexts
 * - Union allows efficient change detection via single byte comparison
 */
typedef union converter_state_union {
  converter_state state;
  volatile unsigned char value;  /**< Volatile for cross-context visibility */
} converter_state_union;

extern converter_state_union converter;

/**
 * @brief Lock Key State Structure
 * 
 * Tracks the state of keyboard lock indicators (Num Lock, Caps Lock, Scroll Lock).
 * Synchronized with host operating system lock key states via USB HID reports.
 */
typedef struct lock_keys {
  unsigned char numLock : 1;     /**< Num Lock state */
  unsigned char capsLock : 1;    /**< Caps Lock state */
  unsigned char scrollLock : 1;  /**< Scroll Lock state */
} lock_keys;

/**
 * @brief Lock Key State Union
 * 
 * Allows both bitfield access (for individual lock states) and
 * byte access (for efficient operations and LED updates).
 * 
 * Threading Model:
 * - Updated from HID callback (tud_hid_set_report_cb) in USB context
 * - Read from protocol tasks for keyboard LED updates
 * - Read from update_converter_leds() for visual indicators
 */
typedef union lock_keys_union {
  lock_keys keys;
  unsigned char value;
} lock_keys_union;

extern lock_keys_union lock_leds;

void set_lock_values_from_hid(uint8_t lock_val);
void update_converter_status(void);

#endif /* LED_HELPER_H */
