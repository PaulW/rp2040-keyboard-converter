/*
 * This file is part of RP2040 Keyboard Converter.
 *
 * Copyright 2023-2026 Paul Bramhall (paulwamp@gmail.com)
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

#include "command_mode.h"

#include <stdio.h>

#include "hardware/watchdog.h"
#include "pico/bootrom.h"
#include "pico/time.h"

#include "config.h"
#include "config_storage.h"
#include "hid_interface.h"
#include "hid_keycodes.h"
#include "keymaps.h"
#include "led_helper.h"
#include "log.h"
#include "uart.h"

#ifdef CONVERTER_LEDS
#include "ws2812/ws2812.h"
#endif

#ifdef CONVERTER_LEDS
// External flag from led_helper.c to control LED colors during log level selection
extern volatile bool log_level_selection_mode;
#endif

/**
 * @brief Command Mode State Machine States
 * 
 * State Transitions:
 * 
 *   IDLE ──┬─> SHIFT_HOLD_WAIT (only both shifts pressed, no other keys)
 *          └─> IDLE (normal operation)
 * 
 *   SHIFT_HOLD_WAIT ──┬─> COMMAND_ACTIVE (held 3 seconds with only shifts)
 *                      ├─> IDLE (shifts released before 3s)
 *                      └─> IDLE (any other key pressed - abort)
 * 
 *   COMMAND_ACTIVE ──┬─> Bootloader (command 'B' executed - device resets)
 *                     ├─> LOG_LEVEL_SELECT (command 'D' for debug level)
 *                     ├─> IDLE (command 'F' for factory reset)
 *                     └─> IDLE (3 second timeout)
 * 
 *   LOG_LEVEL_SELECT ──┬─> IDLE (key '1' = ERROR level)
 *                       ├─> IDLE (key '2' = INFO level)
 *                       ├─> IDLE (key '3' = DEBUG level)
 *                       └─> IDLE (3 second timeout)
 * 
 * HID Report Behavior:
 * - IDLE: Normal HID reports sent to host
 * - SHIFT_HOLD_WAIT: Normal HID reports sent (shifts visible to host)
 * - COMMAND_ACTIVE: ALL HID reports suppressed (empty report sent on entry)
 * - LOG_LEVEL_SELECT: ALL HID reports suppressed (waiting for level choice)
 * - BRIGHTNESS_SELECT: ALL HID reports suppressed (waiting for +/- keys, LED cycling)
 */
typedef enum {
  CMD_MODE_IDLE,                  /**< Normal operation, no command mode active */
  CMD_MODE_SHIFT_HOLD_WAIT,       /**< Both shifts held, waiting for 3 second hold */
  CMD_MODE_COMMAND_ACTIVE,        /**< Command mode active, waiting for command key */
  CMD_MODE_LOG_LEVEL_SELECT,      /**< Waiting for log level selection (1/2/3) */
#ifdef CONVERTER_LEDS
  CMD_MODE_BRIGHTNESS_SELECT,     /**< Waiting for LED brightness adjustment (+/-) */
#endif
} command_mode_state_t;

/**
 * @brief Command Mode State Structure
 * 
 * Tracks the current state of the command mode system including timing.
 * 
 * Threading Model:
 * - Only accessed from main task context (via command_mode_process)
 * - No interrupt access, no synchronization needed
 */
typedef struct {
  command_mode_state_t state;       /**< Current command mode state */
  uint32_t state_start_time_ms;     /**< Time when current state started (milliseconds) */
  uint32_t last_led_toggle_ms;      /**< Time of last LED toggle in COMMAND_ACTIVE */
} command_mode_context_t;

static command_mode_context_t cmd_mode = {
  .state = CMD_MODE_IDLE,
  .state_start_time_ms = 0,
  .last_led_toggle_ms = 0
};

#ifdef CONVERTER_LEDS
/** Brightness selection state tracking */
static uint8_t brightness_original_value = 0;    /**< Original brightness when entering selection mode */
static uint16_t brightness_rainbow_hue = 0;      /**< Current rainbow hue (0-359) for visual feedback */
#endif

/** Command mode timing constants (milliseconds) */
#define CMD_MODE_HOLD_TIME_MS 3000      /**< Time to hold command keys to enter command mode */
#define CMD_MODE_TIMEOUT_MS 3000        /**< Timeout for all command mode states before returning to idle */
#define CMD_MODE_LED_TOGGLE_MS 100      /**< LED toggle interval in command mode */

/**
 * @brief Command Mode Activation Key Configuration
 * 
 * Defines which two HID modifier keys must be held simultaneously to enter Command Mode.
 * Keyboards can override these defaults by defining CMD_MODE_KEY1 and CMD_MODE_KEY2 
 * in their keyboard.h file (before any #include statements).
 * 
 * Override Mechanism:
 * - The #ifndef guards implement a "first definition wins" pattern
 * - If keyboard.h defines these keys, those values are used
 * - If keyboard.h doesn't define them, the defaults below are used
 * - No build system changes or configuration files needed
 * 
 * Default Configuration: Left Shift + Right Shift
 * - Works on standard keyboards with two distinct shift keys
 * - Most keyboards have both shifts, making this a safe default
 * - Least likely to interfere with normal typing
 * - Intuitive for users (hold both shifts for 3 seconds)
 * 
 * When to Override:
 * - Single shift keyboards (e.g., Apple M0110A: both keys return same scancode)
 * - Keyboards where both shifts aren't easily accessible
 * - Layouts where a different combination makes more sense
 * 
 * Example Overrides (define in keyboard.h before includes):
 * - Single shift keyboards: #define CMD_MODE_KEY2 KC_LALT (Shift + Alt)
 * - Compact layouts: #define CMD_MODE_KEY1 KC_LCTRL, #define CMD_MODE_KEY2 KC_LALT
 * - Terminal keyboards: #define CMD_MODE_KEY1 KC_LCTRL, #define CMD_MODE_KEY2 KC_RCTRL
 * 
 * Constraints:
 * - Both keys MUST be HID modifier keys (0xE0-0xE7): validated by _Static_assert below
 * - Both keys MUST be different: no validation (would require complex preprocessor logic)
 * - Detection uses bit masking for O(1) performance (modifier-only requirement)
 * 
 * See keyboards/README.md for complete override documentation.
 */
#ifndef CMD_MODE_KEY1
#define CMD_MODE_KEY1 KC_LSHIFT  /**< First command mode activation key (default: Left Shift) */
#endif

#ifndef CMD_MODE_KEY2
#define CMD_MODE_KEY2 KC_RSHIFT  /**< Second command mode activation key (default: Right Shift) */
#endif

/**
 * @brief Modifier byte mask for command mode activation keys
 * 
 * HID modifier byte layout (each modifier is one bit):
 * - Bit 0: Left Control  (KC_LCTRL  = 0xE0)
 * - Bit 1: Left Shift    (KC_LSHIFT = 0xE1)
 * - Bit 2: Left Alt      (KC_LALT   = 0xE2)
 * - Bit 3: Left GUI      (KC_LGUI   = 0xE3)
 * - Bit 4: Right Control (KC_RCTRL  = 0xE4)
 * - Bit 5: Right Shift   (KC_RSHIFT = 0xE5)
 * - Bit 6: Right Alt     (KC_RALT   = 0xE6)
 * - Bit 7: Right GUI     (KC_RGUI   = 0xE7)
 * 
 * The calculation (keycode & 0x7) extracts the lower 3 bits to get the bit position:
 * - KC_LSHIFT (0xE1) & 0x7 = 1 → Bit 1
 * - KC_RSHIFT (0xE5) & 0x7 = 5 → Bit 5
 * 
 * This pattern matches the existing SUPER_MACRO_INIT macro in hid_keycodes.h.
 * 
 * @note Only works for modifier keys (0xE0-0xE7)
 * @note If CMD_MODE_KEY1/KEY2 are changed to non-modifiers, this logic must be updated
 */
#define CMD_MODE_KEYS_MASK ((1 << (CMD_MODE_KEY1 & 0x7)) | (1 << (CMD_MODE_KEY2 & 0x7)))

/**
 * @brief Compile-time validation of command mode activation keys
 * 
 * Static assertions ensure that CMD_MODE_KEY1 and CMD_MODE_KEY2 are HID modifier
 * keys (0xE0-0xE7). The bit mask calculation and command_keys_pressed() function
 * only work for modifier keys. If regular keys are needed, the detection logic
 * must be completely redesigned to check the keycode array instead.
 */
_Static_assert(CMD_MODE_KEY1 >= 0xE0 && CMD_MODE_KEY1 <= 0xE7, 
               "CMD_MODE_KEY1 must be a HID modifier key (0xE0-0xE7)");
_Static_assert(CMD_MODE_KEY2 >= 0xE0 && CMD_MODE_KEY2 <= 0xE7, 
               "CMD_MODE_KEY2 must be a HID modifier key (0xE0-0xE7)");

/**
 * @brief Checks if any key is pressed in keyboard report
 * 
 * Scans the 6-key rollover array to see if any key is pressed.
 * Used for validating "only modifiers pressed" condition.
 * 
 * @param keyboard_report Pointer to keyboard HID report
 * @return true if any key is pressed, false if array is empty
 * 
 * @note Compiler likely inlines this (10-15 cycles)
 * @note Returns on first non-zero key (early exit optimization)
 */
static inline bool any_key_pressed(const hid_keyboard_report_t *keyboard_report) {
  for (int i = 0; i < 6; i++) {
    if (keyboard_report->keycode[i] != 0) {
      return true;
    }
  }
  return false;
}

/**
 * @brief Checks if a specific key is present in keyboard report
 * 
 * Scans the 6-key rollover array for a specific keycode.
 * Optimized for the common case where the key is not found.
 * 
 * @param keyboard_report Pointer to keyboard HID report
 * @param keycode HID keycode to search for (KC_* constants)
 * @return true if key is pressed, false otherwise
 * 
 * @note Compiler likely inlines this (10-15 cycles)
 * @note Returns on first match (early exit optimization)
 */
static inline bool is_key_pressed(const hid_keyboard_report_t *keyboard_report, 
                                   uint8_t keycode) {
  for (int i = 0; i < 6; i++) {
    if (keyboard_report->keycode[i] == keycode) {
      return true;
    }
  }
  return false;
}

/**
 * @brief Checks if ONLY the command mode activation keys are pressed
 * 
 * This function verifies that both configured command mode keys are pressed
 * AND no other keys are present in the report. This ensures command mode is
 * only entered when the user is intentionally pressing just the two activation
 * keys, preventing accidental activation during normal typing.
 * 
 * By default, the activation keys are Left Shift + Right Shift, but this can
 * be customized per keyboard by defining CMD_MODE_KEY1 and CMD_MODE_KEY2.
 * 
 * @param keyboard_report Pointer to keyboard HID report structure
 * @return true if only the command keys are pressed, false if other keys present
 * 
 * @note Returns false if other modifiers are pressed
 * @note Returns false if any regular keys are in the keycode array
 * @note This is the ONLY validation function needed for command mode entry
 */
static inline bool command_keys_pressed(const hid_keyboard_report_t *keyboard_report) {
  // Check that both command mode keys are pressed
  if ((keyboard_report->modifier & CMD_MODE_KEYS_MASK) != CMD_MODE_KEYS_MASK) {
    return false;
  }
  
  // Check that no other modifiers are pressed (only the command key bits should be set)
  if ((keyboard_report->modifier & ~CMD_MODE_KEYS_MASK) != 0) {
    return false;
  }
  
  // Check that no regular keys are pressed
  if (any_key_pressed(keyboard_report)) {
    return false;
  }
  
  return true;
}


/**
 * @brief Executes bootloader entry command
 * 
 * This function handles the bootloader entry sequence when the 'B' command
 * key is pressed in command mode:
 * 1. Clears all LED states except Status LED
 * 2. Sets firmware flash LED indicator (magenta)
 * 3. Flushes UART to ensure debug messages are sent
 * 4. Resets into BOOTSEL mode for firmware update
 * 
 * @note This function never returns (device resets via reset_usb_boot)
 * @note Called from command_mode_process when 'B' is detected
 */
static void command_execute_bootloader(void) {
  LOG_INFO("Bootloader command received\n");
  
  // Initiate Bootloader
  LOG_INFO("Initiate Bootloader\n");
#ifdef CONVERTER_LEDS
  // Clear all LED states so only the Status LED (magenta) is lit
  lock_leds.value = 0;  // Clear all lock LED states (Num/Caps/Scroll)
  converter.state.cmd_mode = 0;  // Clear command mode flag
  
  // Set Status LED to magenta to indicate Bootloader Mode
  converter.state.fw_flash = 1;
  update_converter_status();
#endif
  // Flush all pending UART messages before bootloader transition
  uart_dma_flush();
  
  // Reboot into Bootloader
  reset_usb_boot(0, 0);
  
  // Never returns
}

/**
 * @brief Exits command mode and restores normal operation
 * 
 * This function cleans up command mode state and restores normal LED
 * operation. Called when:
 * - Shifts are released during command mode
 * - Command mode times out (3 seconds)
 * - After a command is executed
 * 
 * @param reason Debug message describing why command mode is exiting
 * 
 * @note Safe to call multiple times (idempotent)
 */
static void command_mode_exit(const char *reason) {
  LOG_INFO("%s\n", reason);
  cmd_mode.state = CMD_MODE_IDLE;
#ifdef CONVERTER_LEDS
  log_level_selection_mode = false;  // Reset LED colors to GREEN/BLUE
  // Disable command mode LED and restore normal LED operation
  converter.state.cmd_mode = 0;
#endif
  // Restore normal LED operation
  update_converter_status();
}

void command_mode_init(void) {
  cmd_mode.state = CMD_MODE_IDLE;
  cmd_mode.state_start_time_ms = 0;
  cmd_mode.last_led_toggle_ms = 0;
}

void command_mode_task(void) {
  // Early exit if idle - avoids time check overhead 99.99% of the time
  // Most common case: user is typing normally, not in command mode
  if (cmd_mode.state == CMD_MODE_IDLE) {
    return;  // ~3 cycles: load, compare, branch
  }
  
  // Only get time if we're actually in an active state
  uint32_t now_ms = to_ms_since_boot(get_absolute_time());
  
  // Handle SHIFT_HOLD_WAIT timeout (transition to COMMAND_ACTIVE)
  // This must run from main loop to work even when no keyboard events occur
  if (cmd_mode.state == CMD_MODE_SHIFT_HOLD_WAIT) {
    if (now_ms - cmd_mode.state_start_time_ms >= CMD_MODE_HOLD_TIME_MS) {
      cmd_mode.state = CMD_MODE_COMMAND_ACTIVE;
      cmd_mode.state_start_time_ms = now_ms;
      cmd_mode.last_led_toggle_ms = now_ms;
      
      // Send empty keyboard report to release all keys from host's perspective
      // This ensures that the shift keys are released when entering command mode
      send_empty_keyboard_report();
      
#ifdef CONVERTER_LEDS
      // Enable command mode LED and set initial color to green
      converter.state.cmd_mode = 1;
      cmd_mode_led_green = true;
      update_converter_status();
#endif
      LOG_INFO("Command mode active! Press:\n");
      LOG_INFO("  B = Bootloader     D = Log level    F = Factory reset\n");
      LOG_INFO("  L = LED brightness S = Shift-override\n");
      LOG_INFO("Or wait 3s to cancel\n");
    }
    return;  // Exit early - no LED update needed in SHIFT_HOLD_WAIT
  }
  
  // Handle COMMAND_ACTIVE, LOG_LEVEL_SELECT, and BRIGHTNESS_SELECT timeout (return to IDLE)
  // All states use the same timeout value and transition logic
  if (cmd_mode.state == CMD_MODE_COMMAND_ACTIVE || 
      cmd_mode.state == CMD_MODE_LOG_LEVEL_SELECT
#ifdef CONVERTER_LEDS
      || cmd_mode.state == CMD_MODE_BRIGHTNESS_SELECT
#endif
      ) {
    if (now_ms - cmd_mode.state_start_time_ms >= CMD_MODE_TIMEOUT_MS) {
#ifdef CONVERTER_LEDS
      // Save to flash if brightness changed
      if (cmd_mode.state == CMD_MODE_BRIGHTNESS_SELECT) {
        uint8_t current_brightness = ws2812_get_brightness();
        if (current_brightness != brightness_original_value) {
          config_save();
          LOG_INFO("LED brightness saved to flash: %u\n", current_brightness);
        }
      }
#endif
      
      // Use different log messages for debugging clarity
      const char *reason;
      if (cmd_mode.state == CMD_MODE_COMMAND_ACTIVE) {
        reason = "Command mode timeout, returning to idle";
      } else if (cmd_mode.state == CMD_MODE_LOG_LEVEL_SELECT) {
        reason = "Log level selection timeout, returning to idle";
#ifdef CONVERTER_LEDS
      } else {
        reason = "LED brightness selection timeout, returning to idle";
#endif
      }
      command_mode_exit(reason);
      return;  // Exit early after mode exit
    }
  }
  
  // Update LED feedback when in COMMAND_ACTIVE or LOG_LEVEL_SELECT state
  if (cmd_mode.state == CMD_MODE_COMMAND_ACTIVE || cmd_mode.state == CMD_MODE_LOG_LEVEL_SELECT) {
    // Update LED feedback only when in COMMAND_ACTIVE or LOG_LEVEL_SELECT state
    // Moved inline to avoid function call overhead
#ifdef CONVERTER_LEDS
    // Toggle LED every 100ms (CMD_MODE_LED_TOGGLE_MS)
    if (now_ms - cmd_mode.last_led_toggle_ms >= CMD_MODE_LED_TOGGLE_MS) {
      // Toggle the LED color
      cmd_mode_led_green = !cmd_mode_led_green;
      cmd_mode.last_led_toggle_ms = now_ms;
      
      // Force LED update by calling update_converter_leds() directly
      // This bypasses the change detection in update_converter_status()
      // since we're only changing cmd_mode_led_green, not converter.value
      update_converter_leds();
    }
#endif
  }
  
#ifdef CONVERTER_LEDS
  // Update LED feedback when in BRIGHTNESS_SELECT state (rainbow cycling)
  if (cmd_mode.state == CMD_MODE_BRIGHTNESS_SELECT) {
    // Update rainbow hue every 50ms for smooth cycling (faster than command mode toggle)
    const uint32_t RAINBOW_CYCLE_MS = 50;
    if (now_ms - cmd_mode.last_led_toggle_ms >= RAINBOW_CYCLE_MS) {
      // Increment hue for rainbow effect (360 degrees for full cycle)
      brightness_rainbow_hue = (brightness_rainbow_hue + 6) % 360;  // 6 degrees per update = 3 seconds per cycle
      
      // Convert HSV to RGB (full saturation and brightness for vivid colors)
      uint32_t rainbow_color = hsv_to_rgb(brightness_rainbow_hue, 255, 255);
      
      // Set the status LED directly (bypass normal LED helper logic)
      ws2812_show(rainbow_color);
      
      cmd_mode.last_led_toggle_ms = now_ms;
    }
  }
#endif
}

bool command_mode_process(const hid_keyboard_report_t *keyboard_report) {
  switch (cmd_mode.state) {
    case CMD_MODE_IDLE:
      // Check if ONLY the command keys are pressed (no other keys) to enter hold-wait state
      if (command_keys_pressed(keyboard_report)) {
        cmd_mode.state = CMD_MODE_SHIFT_HOLD_WAIT;
        cmd_mode.state_start_time_ms = to_ms_since_boot(get_absolute_time());
        LOG_INFO("Command keys hold detected, waiting for 3 second hold...\n");
      }
      // Normal keyboard processing continues
      return true;
      
    case CMD_MODE_SHIFT_HOLD_WAIT:
      // Check if command keys were released early OR if any other keys were pressed
      // Using command_keys_pressed() for both checks ensures strict validation
      if (!command_keys_pressed(keyboard_report)) {
        LOG_INFO("Command keys released or other keys pressed, aborting\n");
        cmd_mode.state = CMD_MODE_IDLE;
        return true;
      }
      
      // Note: Transition to COMMAND_ACTIVE after 3 seconds is handled in command_mode_task()
      // This ensures it works even when no keyboard events occur (user just holds keys)
      
      // Allow normal keyboard reports during wait (command keys are being sent normally)
      return true;
      
    case CMD_MODE_COMMAND_ACTIVE:
      // Note: User can release shifts once command mode is active
      // We only care about command key presses (e.g., 'B' for bootloader, 'D' for log level, 'F' for factory reset)
      
      // Note: Timeout check is handled in command_mode_task() which runs from main loop
      // This ensures timeout works even when no keyboard events are occurring
      
      // Check for bootloader command
      if (is_key_pressed(keyboard_report, KC_B)) {
        command_execute_bootloader();
        // Never returns, but return false to suppress keyboard report
        return false;
      }
      
      // Check for log level selection command
      if (is_key_pressed(keyboard_report, KC_D)) {
        cmd_mode.state = CMD_MODE_LOG_LEVEL_SELECT;
        cmd_mode.state_start_time_ms = to_ms_since_boot(get_absolute_time());
#ifdef CONVERTER_LEDS
        log_level_selection_mode = true;  // Change LED colors to GREEN/PINK
#endif
        LOG_INFO("Log level selection: Press 1=ERROR, 2=INFO, 3=DEBUG\n");
        return false;
      }
      
      // Check for factory reset command
      if (is_key_pressed(keyboard_report, KC_F)) {
        LOG_WARN("Factory reset requested - restoring default configuration\n");
        config_factory_reset();
        LOG_INFO("Factory reset complete - rebooting device...\n");
        
#ifdef CONVERTER_LEDS
        // Clear all LED states
        lock_leds.value = 0;
        converter.state.cmd_mode = 0;
        converter.state.fw_flash = 1;  // Set status LED to indicate reboot
        update_converter_status();
#endif
        
        // Flush UART before reboot
        uart_dma_flush();
        
        // Reboot device using watchdog (cleanest reboot method)
        // Parameters: delay_ms=0 (immediate), scratch0=0, scratch1=0 (no custom values)
        watchdog_reboot(0, 0, 0);
        
        // Never returns
        return false;
      }
      
      // Check for LED brightness adjustment command
      if (is_key_pressed(keyboard_report, KC_L)) {
#ifdef CONVERTER_LEDS
        cmd_mode.state = CMD_MODE_BRIGHTNESS_SELECT;
        cmd_mode.state_start_time_ms = to_ms_since_boot(get_absolute_time());
        
        // Save original brightness to detect changes
        brightness_original_value = ws2812_get_brightness();
        brightness_rainbow_hue = 0;  // Start rainbow cycle at red
        
        LOG_INFO("LED brightness selection: Press +/- to adjust (0-10), current=%u\n", brightness_original_value);
#else
        LOG_WARN("LED brightness control not available (CONVERTER_LEDS not defined)\n");
#endif
        return false;
      }
      
      // Check for shift-override toggle command
      if (is_key_pressed(keyboard_report, KC_S)) {
        // Only allow toggle if keyboard defines shift-override arrays
        extern const uint8_t * const keymap_shift_override_layers[KEYMAP_MAX_LAYERS] __attribute__((weak));
        
        if (keymap_shift_override_layers == NULL) {
          LOG_WARN("Shift-override not available (keyboard doesn't define custom shift mappings)\n");
          command_mode_exit("Shift-override not available");
          return false;
        }
        
        bool current = config_get_shift_override_enabled();
        bool new_value = !current;
        config_set_shift_override_enabled(new_value);
        config_save();  // Persist to flash
        
        command_mode_exit(new_value ? "Shift-override enabled" : "Shift-override disabled");
        return false;
      }
      
      // Note: LED update is handled in command_mode_task() which runs from main loop
      // This ensures smooth LED flashing even when no keys are being pressed
      
      // Suppress ALL keyboard reports while in command mode
      return false;
      
    case CMD_MODE_LOG_LEVEL_SELECT:
      // Wait for user to press 1, 2, or 3 to select log level
      if (is_key_pressed(keyboard_report, KC_1)) {
        // Set log level to ERROR (only errors visible)
        log_set_level(LOG_LEVEL_ERROR);
        config_set_log_level(LOG_LEVEL_ERROR);
        config_save();  // Persist to flash
        command_mode_exit("Log level changed to ERROR");
        return false;
      }
      
      if (is_key_pressed(keyboard_report, KC_2)) {
        // Set log level to INFO (info + error visible, debug hidden)
        log_set_level(LOG_LEVEL_INFO);
        config_set_log_level(LOG_LEVEL_INFO);
        config_save();  // Persist to flash
        command_mode_exit("Log level changed to INFO");
        return false;
      }
      
      if (is_key_pressed(keyboard_report, KC_3)) {
        // Set log level to DEBUG (all messages visible)
        log_set_level(LOG_LEVEL_DEBUG);
        config_set_log_level(LOG_LEVEL_DEBUG);
        config_save();  // Persist to flash
        command_mode_exit("Log level changed to DEBUG");
        return false;
      }
      
      // Suppress ALL keyboard reports while waiting for level selection
      return false;
      
#ifdef CONVERTER_LEDS
    case CMD_MODE_BRIGHTNESS_SELECT:
      // Wait for user to press + or - to adjust brightness
      // KC_EQUAL is the physical '=' key which produces '+' when shifted
      if (is_key_pressed(keyboard_report, KC_EQUAL) || is_key_pressed(keyboard_report, KC_KP_PLUS)) {
        uint8_t current = ws2812_get_brightness();
        if (current < 10) {
          uint8_t new_brightness = current + 1;
          ws2812_set_brightness(new_brightness);
          config_set_led_brightness(new_brightness);
          LOG_INFO("LED brightness increased to %u\n", new_brightness);
          
          // Reset timeout on brightness change
          cmd_mode.state_start_time_ms = to_ms_since_boot(get_absolute_time());
        }
        return false;
      }
      
      // '-' key
      if (is_key_pressed(keyboard_report, KC_MINUS) || is_key_pressed(keyboard_report, KC_KP_MINUS)) {
        uint8_t current = ws2812_get_brightness();
        if (current > 0) {
          uint8_t new_brightness = current - 1;
          ws2812_set_brightness(new_brightness);
          config_set_led_brightness(new_brightness);
          LOG_INFO("LED brightness decreased to %u\n", new_brightness);
          
          // Reset timeout on brightness change
          cmd_mode.state_start_time_ms = to_ms_since_boot(get_absolute_time());
        }
        return false;
      }
      
      // Suppress ALL keyboard reports while in brightness selection mode
      return false;
#endif
  }
  
  // Should never reach here, but default to normal processing
  return true;
}
