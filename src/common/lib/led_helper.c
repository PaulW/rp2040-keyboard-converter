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

#include "led_helper.h"

#ifdef CONVERTER_LEDS
#include "ws2812/ws2812.h"
#endif

// Initialize the converter state with both keyboard and mouse states set to ready
converter_state_union converter = {.value = 0xC3};

lock_keys_union lock_leds;
uint8_t ps2_lock_values = 0;

/**
 * @brief Updates the LEDs on the converter based on the current state.
 * This function updates the LEDs on the converter board to reflect the current state of the
 * converter. If the firmware is being flashed, it shows a specific color. If both the keyboard and
 * mouse are ready, it shows a different color. Otherwise, it shows a color indicating that the
 * converter is not ready. Additionally, if the CONVERTER_LOCK_LEDS macro is defined, it updates the
 * lock LEDs based on their states.
 *
 * @note This function includes a small delay to ensure proper timing for the WS2812 LEDs and to
 * prevent flickering.
 */
void update_converter_leds(void) {
#ifdef CONVERTER_LEDS
  // Update the status LED first.
  if (converter.state.fw_flash) {
    ws2812_show(CONVERTER_LEDS_STATUS_FWFLASH_COLOR);
  } else {
    if (converter.state.kb_ready && converter.state.mouse_ready) {
      ws2812_show(CONVERTER_LEDS_STATUS_READY_COLOR);
    } else {
      ws2812_show(CONVERTER_LEDS_STATUS_NOT_READY_COLOR);
    }
  }

#ifdef CONVERTER_LOCK_LEDS
  // Now we update the Lock LEDs.
  // This is only done if the CONVERTER_LOCK_LEDS macro is defined.
  ws2812_show(lock_leds.keys.numLock ? CONVERTER_LOCK_LEDS_COLOR : 0);
  ws2812_show(lock_leds.keys.capsLock ? CONVERTER_LOCK_LEDS_COLOR : 0);
  ws2812_show(lock_leds.keys.scrollLock ? CONVERTER_LOCK_LEDS_COLOR : 0);
#endif
  // Add a small delay to ensure we respect the WS2812 timings to reset the data line, and also to
  // prevent flickering
  busy_wait_us(60);
#endif
}

/**
 * @brief Wrapper function to update the converter status LEDs.
 * This is to ensure that the LEDs are only updated if the status has changed.  Calling the
 * update_converter_leds() function every time the status is updated can cause flickering, as well
 * as unnecessary delays.
 */
void update_converter_status(void) {
  static uint8_t status;
  if (status != converter.value) {
    status = converter.value;
    update_converter_leds();
  }
}

/**
 * @brief Sets the lock values for the keyboard LEDs based on the given HID lock value.
 * This function updates the lock values for the keyboard LEDs (num lock, caps lock, and scroll
 * lock) based on the given HID lock value. The lock values are stored in the `lock_leds` structure.
 * Additionally, we ensure any controller-specific lock values are updated, regardless of whether we
 * are building that specific controller.
 *
 * @param lock_val The HID lock value.
 *
 * @todo Move controller-specific lock values to their respective files.
 */
void set_lock_values_from_hid(uint8_t lock_val) {
  lock_leds.keys.numLock = (unsigned char)(lock_val & 0x01);
  lock_leds.keys.capsLock = (unsigned char)((lock_val >> 1) & 0x01);
  lock_leds.keys.scrollLock = (unsigned char)((lock_val >> 2) & 0x01);

#ifdef CONVERTER_LEDS
  update_converter_leds();
#endif

  // Update interface-specific lock values
  // PS/2
  ps2_lock_values = (uint8_t)((lock_leds.keys.capsLock << 2) | (lock_leds.keys.numLock << 1) |
                              lock_leds.keys.scrollLock);
}
