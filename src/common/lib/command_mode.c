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

#include "command_mode.h"

#include <stdio.h>

#include "config.h"
#include "hid_interface.h"
#include "hid_keycodes.h"
#include "led_helper.h"
#include "pico/bootrom.h"
#include "pico/time.h"
#include "uart.h"

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
 *                     └─> IDLE (3 second timeout)
 * 
 * HID Report Behavior:
 * - IDLE: Normal HID reports sent to host
 * - SHIFT_HOLD_WAIT: Normal HID reports sent (shifts visible to host)
 * - COMMAND_ACTIVE: ALL HID reports suppressed (empty report sent on entry)
 */
typedef enum {
  CMD_MODE_IDLE,              /**< Normal operation, no command mode active */
  CMD_MODE_SHIFT_HOLD_WAIT,   /**< Both shifts held, waiting for 3 second hold */
  CMD_MODE_COMMAND_ACTIVE,    /**< Command mode active, waiting for command key */
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

/** Command mode timing constants (milliseconds) */
#define CMD_MODE_HOLD_TIME_MS 3000      /**< Time to hold both shifts to enter command mode */
#define CMD_MODE_TIMEOUT_MS 3000        /**< Timeout for command mode before returning to idle */
#define CMD_MODE_LED_TOGGLE_MS 100      /**< LED toggle interval in command mode */

/**
 * @brief Checks if both shift keys are currently pressed
 * 
 * This function examines the keyboard HID report modifier byte to determine
 * if both Left Shift and Right Shift keys are currently pressed. Used by
 * the command mode system to detect the entry trigger.
 * 
 * Modifier Bit Layout (keyboard_report->modifier):
 * - Bit 1: Left Shift (KC_LSHIFT)
 * - Bit 5: Right Shift (KC_RSHIFT)
 * 
 * @param keyboard_report Pointer to keyboard HID report structure
 * @return true if both shift keys are pressed, false otherwise
 * 
 * @note This function only checks the current report state
 * @note Does not check for any other keys being pressed
 */
static inline bool both_shifts_pressed(const hid_keyboard_report_t *keyboard_report) {
  const uint8_t both_shifts = (1 << (KC_LSHIFT & 0x7)) | (1 << (KC_RSHIFT & 0x7));
  return (keyboard_report->modifier & both_shifts) == both_shifts;
}

/**
 * @brief Checks if ONLY the two shift keys are pressed (no other keys)
 * 
 * This function verifies that both shift keys are pressed AND no other keys
 * are present in the report. This is used to ensure command mode is only
 * entered when the user is intentionally pressing just the two shift keys,
 * preventing accidental activation during normal typing.
 * 
 * @param keyboard_report Pointer to keyboard HID report structure
 * @return true if only both shifts are pressed, false if other keys present
 * 
 * @note Returns false if other modifiers (Ctrl, Alt, etc.) are pressed
 * @note Returns false if any regular keys are in the keycode array
 */
static inline bool only_shifts_pressed(const hid_keyboard_report_t *keyboard_report) {
  // Check that both shifts are pressed
  const uint8_t both_shifts = (1 << (KC_LSHIFT & 0x7)) | (1 << (KC_RSHIFT & 0x7));
  if ((keyboard_report->modifier & both_shifts) != both_shifts) {
    return false;
  }
  
  // Check that no other modifiers are pressed (only the shift bits should be set)
  if ((keyboard_report->modifier & ~both_shifts) != 0) {
    return false;
  }
  
  // Check that no regular keys are pressed
  for (int i = 0; i < 6; i++) {
    if (keyboard_report->keycode[i] != 0) {
      return false;
    }
  }
  
  return true;
}


/**
 * @brief Handles LED feedback during command mode
 * 
 * This function manages the LED feedback patterns for command mode:
 * - SHIFT_HOLD_WAIT: Normal LED operation (handled by update_converter_status)
 * - COMMAND_ACTIVE: Alternating Green/Blue flash every 100ms
 * 
 * The alternating LED pattern provides clear visual feedback that the
 * converter is in command mode and ready to accept command keys.
 * 
 * Non-Blocking Implementation:
 * - Uses time-based LED toggling without delays
 * - Sets converter.state.cmd_mode flag to enable command mode LED
 * - Toggles cmd_mode_led_green to alternate colors
 * - Calls update_converter_status() to apply changes
 * 
 * @note Called every time command_mode_process is invoked
 * @note Only performs updates when state machine is in COMMAND_ACTIVE
 */
static void command_mode_update_leds(void) {
#ifdef CONVERTER_LEDS
  if (cmd_mode.state == CMD_MODE_COMMAND_ACTIVE) {
    uint32_t now_ms = to_ms_since_boot(get_absolute_time());
    
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
  }
#endif
}

/**
 * @brief Executes bootloader entry command
 * 
 * This function handles the bootloader entry sequence when the 'B' command
 * key is pressed in command mode:
 * 1. Sets firmware flash LED indicator (magenta)
 * 2. Flushes UART to ensure debug messages are sent
 * 3. Resets into BOOTSEL mode for firmware update
 * 
 * @note This function never returns (device resets)
 * @note Called from command_mode_process when 'B' is detected
 */
static void command_execute_bootloader(void) {
  printf("[CMD] Bootloader command received\n");
  cmd_mode.state = CMD_MODE_IDLE;
  
  // Initiate Bootloader
  printf("[INFO] Initiate Bootloader\n");
#ifdef CONVERTER_LEDS
  // Clear all LED states so only the Status LED (purple) is lit
  lock_leds.value = 0;  // Clear all lock LED states (Num/Caps/Scroll)
  converter.state.cmd_mode = 0;  // Clear command mode flag
  
  // Set Status LED to purple to indicate Bootloader Mode
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
  printf("[CMD] %s\n", reason);
  cmd_mode.state = CMD_MODE_IDLE;
#ifdef CONVERTER_LEDS
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
      printf("[CMD] Command mode active! Press 'B' for bootloader, or wait 3s to cancel\n");
    }
  }
  
  // Handle COMMAND_ACTIVE timeout (return to IDLE)
  // This ensures timeout works even without keyboard activity
  if (cmd_mode.state == CMD_MODE_COMMAND_ACTIVE) {
    if (now_ms - cmd_mode.state_start_time_ms >= CMD_MODE_TIMEOUT_MS) {
      command_mode_exit("Command mode timeout, returning to idle");
    }
  }
  
  // Update LED feedback if in command mode
  // This runs continuously from main loop to ensure LED updates
  // even when no keyboard events are occurring
  command_mode_update_leds();
}

bool command_mode_process(const hid_keyboard_report_t *keyboard_report) {
  uint32_t now_ms = to_ms_since_boot(get_absolute_time());
  
  switch (cmd_mode.state) {
    case CMD_MODE_IDLE:
      // Check if ONLY both shifts are pressed (no other keys) to enter hold-wait state
      if (only_shifts_pressed(keyboard_report)) {
        cmd_mode.state = CMD_MODE_SHIFT_HOLD_WAIT;
        cmd_mode.state_start_time_ms = now_ms;
        printf("[CMD] Shift hold detected, waiting for 3 second hold...\n");
      }
      // Normal keyboard processing continues
      return true;
      
    case CMD_MODE_SHIFT_HOLD_WAIT:
      // Check if shifts were released early
      if (!both_shifts_pressed(keyboard_report)) {
        printf("[CMD] Shifts released early, returning to idle\n");
        cmd_mode.state = CMD_MODE_IDLE;
        return true;
      }
      
      // Check if any other keys were pressed - abort command mode entry
      if (!only_shifts_pressed(keyboard_report)) {
        printf("[CMD] Other keys pressed, aborting command mode entry\n");
        cmd_mode.state = CMD_MODE_IDLE;
        return true;
      }
      
      // Note: Transition to COMMAND_ACTIVE after 3 seconds is handled in command_mode_task()
      // This ensures it works even when no keyboard events occur (user just holds shifts)
      
      // Allow normal keyboard reports during wait (shifts are being sent normally)
      return true;
      
    case CMD_MODE_COMMAND_ACTIVE:
      // Note: User can release shifts once command mode is active
      // We only care about command key presses (e.g., 'B' for bootloader)
      
      // Note: Timeout check is handled in command_mode_task() which runs from main loop
      // This ensures timeout works even when no keyboard events are occurring
      
      // Check for command keys (only process key presses, not releases)
      // We detect the 'B' key in the keycode array
      for (int i = 0; i < 6; i++) {
        if (keyboard_report->keycode[i] == KC_B) {
          command_execute_bootloader();
          // Never returns, but return false to suppress keyboard report
          return false;
        }
      }
      
      // Note: LED update is handled in command_mode_task() which runs from main loop
      // This ensures smooth LED flashing even when no keys are being pressed
      
      // Suppress ALL keyboard reports while in command mode
      return false;
  }
  
  // Should never reach here, but default to normal processing
  return true;
}
