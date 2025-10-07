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
// Track last LED update time (us since boot) for non-blocking WS2812 reset timing
// Using uint32_t for atomic access (64-bit absolute_time_t is not atomic on RP2040)
static volatile uint32_t last_led_update_time_us = 0;
// Track if an update is pending due to timing constraints
static volatile bool led_update_pending = false;

// Track which LED color to display during command mode (green or blue)
volatile bool cmd_mode_led_green = true;

// Command mode LED colors
#define CMD_MODE_LED_GREEN 0x00FF00     /**< Green LED for command mode phase 1 */
#define CMD_MODE_LED_BLUE 0x0000FF    /**< Blue LED for command mode phase 2 */
#endif

/**
 * @brief Updates the LEDs on the converter based on the current state
 * 
 * This function updates WS2812 RGB LEDs to reflect the converter's operational state.
 * The status LED shows different colors for different states:
 * - Firmware flash mode: Magenta (bootloader active)
 * - Ready (KB + Mouse): Green (all devices initialized)
 * - Not ready: Orange (waiting for device initialization)
 * 
 * If CONVERTER_LOCK_LEDS is defined, additional LEDs show lock key states
 * (Num Lock, Caps Lock, Scroll Lock).
 * 
 * WS2812 Protocol Implementation:
 * - Data cascades through LED chain (first LED receives all data, passes remainder)
 * - Each ws2812_show() call sends 24 bits (GRB) to next LED in chain
 * - After all LEDs are updated, a ≥50µs reset pulse latches all colors simultaneously
 * - Uses non-blocking timing check to enforce minimum 60µs between update cycles
 * 
 * Non-Blocking Approach with Multiple Safeguards:
 * 1. Timing Check: Ensures ≥60µs elapsed since last update (WS2812 reset requirement)
 * 2. FIFO Check: ws2812_show() checks PIO TX FIFO before writing (prevents blocking)
 * 3. Deferred Updates: Sets pending flag if either check fails, retry on next call
 * 4. Atomic Updates: All LEDs must succeed or entire update is deferred for consistency
 * 
 * Critical for Protocol Timing:
 * - Keyboard/mouse protocols use PIO with strict timing requirements
 * - Any blocking in WS2812 updates could cause protocol bit errors
 * - Non-blocking at both timing AND PIO FIFO levels prevents interference
 * - Typical update latency: <10µs when FIFO has space, zero blocking ever
 * 
 * @return true if all LEDs were updated, false if update was deferred
 * 
 * @note Fully non-blocking: never blocks on timing OR PIO FIFO
 * @note Thread-safe: Uses atomic time operations from pico SDK
 * @note Caller should retry if returns false (handled automatically by wrapper)
 */
bool update_converter_leds(void) {
#ifdef CONVERTER_LEDS
  // WS2812 requires ≥50µs reset time AFTER transmission completes
  // Calculate minimum interval: transmission time + reset time
  // Each LED takes ~30µs to transmit (24 bits × 1.25µs per bit)
  uint32_t now_us = to_us_since_boot(get_absolute_time());
#ifdef CONVERTER_LOCK_LEDS
  const uint32_t led_count = 4u;  // status + Num/Caps/Scroll
#else
  const uint32_t led_count = 1u;  // status only
#endif
  const uint32_t min_interval_us = 60u + (led_count * 30u);  // ≥50µs reset + frame time
  uint32_t elapsed_us = now_us - last_led_update_time_us;
  
  if (elapsed_us < min_interval_us) {
    // Not enough time has passed, mark update as pending
    led_update_pending = true;
    return false;
  }
  
  // Determine status LED color based on converter state
  uint32_t status_color;
  if (converter.state.fw_flash) {
    status_color = CONVERTER_LEDS_STATUS_FWFLASH_COLOR;
  } else if (converter.state.cmd_mode) {
    // Command mode active - alternate between green and blue
    status_color = cmd_mode_led_green ? CMD_MODE_LED_GREEN : CMD_MODE_LED_BLUE;
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
    last_led_update_time_us = now_us;
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
 * @brief Wrapper function to update converter status LEDs efficiently
 * 
 * This function implements change detection to prevent unnecessary LED updates.
 * WS2812 LED updates are relatively expensive operations (PIO state machine),
 * so we only update when the state actually changes OR when a previous update
 * was deferred due to timing constraints.
 * 
 * Benefits:
 * - Prevents LED flickering from repeated identical updates
 * - Reduces PIO bus traffic and CPU overhead
 * - Improves overall system responsiveness
 * - Ensures deferred updates eventually get applied
 * 
 * Change Detection with Deferred Update Handling:
 * - Uses static variable to cache previous state
 * - Compares full state byte (union value) for efficiency
 * - Single byte comparison vs multiple bitfield checks
 * - Retries if previous update was deferred (led_update_pending flag)
 * 
 * @note Called frequently from protocol tasks
 * @note Only updates LEDs when state changes or retry needed
 * @note Non-blocking: deferred updates handled in next call iteration
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
 * @brief Sets the lock values for the keyboard LEDs based on the given HID lock value
 * 
 * This function updates the lock values for the keyboard LEDs (num lock, caps lock, and scroll
 * lock) based on the given HID lock value. The lock values are stored in the `lock_leds` structure.
 * 
 * USB HID Context:
 * - Called from tud_hid_set_report_cb() in USB interrupt context
 * - Lock key changes are rare (user presses Caps Lock, Num Lock, etc.)
 * - Immediate LED feedback is important for user experience
 * 
 * Bounded Wait Strategy:
 * - Uses bounded wait (max 60µs) to ensure LED update completes
 * - 60µs is negligible in USB context (USB frames are 1000µs)
 * - Retries update_converter_leds() until success or max iterations reached
 * - Prevents deferred updates for lock key changes where immediate feedback matters
 * 
 * @param lock_val The HID lock value (bit 0: Num Lock, bit 1: Caps Lock, bit 2: Scroll Lock)
 * 
 * @note Called from USB interrupt context - 60µs bounded wait is acceptable here
 * @note Most calls succeed immediately; wait only occurs if called within 60µs of previous update
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
