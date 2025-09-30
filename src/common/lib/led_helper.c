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

#include <stdbool.h>

#ifdef CONVERTER_LEDS
#include "ws2812/ws2812.h"
#include "pico/time.h"
#endif

// Initialize the converter state with both keyboard and mouse states set to ready
converter_state_union converter = {.value = 0xC3};

lock_keys_union lock_leds;

#ifdef CONVERTER_LEDS
// Track last LED update time for non-blocking WS2812 reset timing
static absolute_time_t last_led_update_time = {0};
// Track if an update is pending due to timing constraints
static bool led_update_pending = false;
#endif

/**
 * Update the converter's WS2812 LEDs to reflect the current operational state.
 *
 * The status LED shows one of: magenta for firmware-flash mode, green when both
 * keyboard and mouse are ready, or orange when not ready. If CONVERTER_LOCK_LEDS
 * is defined, additional LEDs represent Num Lock, Caps Lock, and Scroll Lock.
 *
 * This function attempts a non-blocking LED update; callers should retry when
 * the update is deferred.
 *
 * @return `true` if the LED update was successfully queued/applied, `false` if
 * the update was deferred and should be retried later.
 */
bool update_converter_leds(void) {
#ifdef CONVERTER_LEDS
  // WS2812 requires ≥50µs reset time between updates
  // Check if enough time has elapsed since last update (non-blocking)
  absolute_time_t now = get_absolute_time();
  int64_t elapsed_us = absolute_time_diff_us(last_led_update_time, now);
  
  if (elapsed_us < 60) {
    // Not enough time has passed, mark update as pending
    led_update_pending = true;
    return false;
  }
  
  // Determine status LED color based on converter state
  uint32_t status_color;
  if (converter.state.fw_flash) {
    status_color = CONVERTER_LEDS_STATUS_FWFLASH_COLOR;
  } else if (converter.state.kb_ready && converter.state.mouse_ready) {
    status_color = CONVERTER_LEDS_STATUS_READY_COLOR;
  } else {
    status_color = CONVERTER_LEDS_STATUS_NOT_READY_COLOR;
  }
  
  // Update all LEDs in sequence - if any fail, defer entire update
  bool success = ws2812_show(status_color);
  
#ifdef CONVERTER_LOCK_LEDS
  // Chain success checks - short-circuit on first failure
  success = success && ws2812_show(lock_leds.keys.numLock ? CONVERTER_LOCK_LEDS_COLOR : 0);
  success = success && ws2812_show(lock_leds.keys.capsLock ? CONVERTER_LOCK_LEDS_COLOR : 0);
  success = success && ws2812_show(lock_leds.keys.scrollLock ? CONVERTER_LOCK_LEDS_COLOR : 0);
#endif

  if (success) {
    // All LEDs successfully queued - record time and clear pending flag
    last_led_update_time = get_absolute_time();
    led_update_pending = false;
  } else {
    // One or more LEDs failed to queue - mark as pending for retry
    led_update_pending = true;
  }
  
  return success;
#else
  return true;
#endif
}

/**
 * Trigger LED updates for the converter only when the converter state changes or a previous update was deferred.
 *
 * Caches the last applied converter state to avoid redundant WS2812 updates and retries any deferred update on subsequent calls so pending LED changes are eventually applied.
 *
 * @note Intended to be called frequently from protocol tasks; inexpensive when no change is detected.
 */
void update_converter_status(void) {
  static uint8_t status;
#ifdef CONVERTER_LEDS
  // Update if state changed OR if a previous update is pending
  if (status != converter.value || led_update_pending) {
    if (update_converter_leds()) {
      // Update succeeded, cache the new status
      status = converter.value;
    }
    // If update_converter_leds() returns false, led_update_pending will be true
    // and we'll retry on the next call
  }
#else
  status = converter.value;
#endif
}

/**
 * Update lock LED state from a HID lock bitmap.
 *
 * Sets numLock, capsLock, and scrollLock in the global lock_leds structure from the
 * corresponding bits of lock_val and attempts to apply the change to the physical LEDs.
 * When LED support is enabled, this function will perform a bounded retry loop (up to
 * 10 attempts, ~60 µs total) to provide immediate visual feedback when invoked from
 * USB HID interrupt context.
 *
 * @param lock_val HID lock bitmap: bit 0 = Num Lock, bit 1 = Caps Lock, bit 2 = Scroll Lock.
 *
 * @note Called from USB HID interrupt context; the bounded retry is intended to remain short
 *       and predictable to preserve USB timing. 
 */
void set_lock_values_from_hid(uint8_t lock_val) {
  lock_leds.keys.numLock = (unsigned char)(lock_val & 0x01);
  lock_leds.keys.capsLock = (unsigned char)((lock_val >> 1) & 0x01);
  lock_leds.keys.scrollLock = (unsigned char)((lock_val >> 2) & 0x01);

#ifdef CONVERTER_LEDS
  // Retry LED update with bounded wait (max ~60µs total)
  // Lock key changes are rare, so bounded wait is acceptable for immediate feedback
  for (int retry = 0; retry < 10; retry++) {
    if (update_converter_leds()) {
      // Update succeeded
      break;
    }
    // Wait a bit before retry (total max wait: ~60µs)
    // This only happens if called within 60µs of previous update (rare)
    sleep_us(6);
  }
#endif
}
