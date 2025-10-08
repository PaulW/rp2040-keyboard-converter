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

#ifndef COMMAND_MODE_H
#define COMMAND_MODE_H

#include <stdbool.h>
#include <stdint.h>

#include "tusb.h"

/**
 * @file command_mode.h
 * @brief Command Mode System for 2KRO-Compatible Special Functions
 * 
 * Command Mode provides a time-based alternative to simultaneous key presses
 * for accessing special functions like bootloader entry. This solves the
 * limitation of 2-key rollover (2KRO) keyboards that cannot register 3
 * simultaneous key presses.
 * 
 * Design Philosophy:
 * - Sequential key detection instead of simultaneous press
 * - Visual feedback via LED to indicate mode state
 * - Safe from accidental activation (requires ONLY two specific keys, 3 second hold)
 * - Extensible for future commands beyond bootloader entry
 * - Configurable activation keys per keyboard layout
 * 
 * Activation Key Configuration:
 * By default, Command Mode is activated by holding Left Shift + Right Shift.
 * Keyboards can override this by defining CMD_MODE_KEY1 and CMD_MODE_KEY2 in
 * their keyboard.h file (before any includes). Both keys must be HID modifier
 * keys (0xE0-0xE7). If not defined, defaults to KC_LSHIFT + KC_RSHIFT.
 * 
 * Example configurations:
 * - Standard keyboards: Left Shift + Right Shift (default, no override needed)
 * - Single-shift keyboards: Shift + Alt (e.g., #define CMD_MODE_KEY2 KC_LALT)
 * - Compact layouts: Control + Alt (e.g., KC_LCTRL + KC_LALT)
 * - Terminal keyboards: Both controls (e.g., KC_LCTRL + KC_RCTRL)
 * 
 * See keyboards/README.md for detailed override documentation.
 * 
 * User Experience:
 * 1. Press and hold ONLY the two configured command keys (default: both shifts, no other keys)
 * 2. Hold for 3 seconds (normal HID reports sent during wait)
 * 3. LED flashes Green/Blue alternating every 100ms
 * 4. Press 'B' to enter bootloader, 'D' to change log level, or wait 3s for auto-exit
 * 5. If 'D' pressed: Press '1' (ERROR), '2' (INFO), or '3' (DEBUG) to set log level
 * 6. If any other key is pressed during hold, command mode entry is aborted
 * 
 * Available Commands:
 * - 'B': Enter bootloader (BOOTSEL mode for firmware update)
 * - 'D': Change log level (then press 1/2/3 for ERROR/INFO/DEBUG)
 * 
 * State Machine:
 * - IDLE: Normal operation
 * - SHIFT_HOLD_WAIT: Command keys held, counting 3 seconds, HID reports active
 * - COMMAND_ACTIVE: Command mode active, LED flashing, HID reports suppressed
 * - LOG_LEVEL_SELECT: Waiting for log level selection (1/2/3)
 * 
 * Threading Model:
 * - All functions called from main task context only
 * - No interrupt access, no synchronization primitives needed
 * - Safe to call from handle_keyboard_report() context
 */

/**
 * @brief Initializes the command mode system
 * 
 * Sets up initial state for command mode processing. Should be called
 * during system initialization before any keyboard processing begins.
 * 
 * @note Called from main() during startup
 * @note Idempotent - safe to call multiple times
 */
void command_mode_init(void);

/**
 * @brief Updates command mode state machine and LED feedback
 * 
 * This function should be called regularly from the main loop to ensure
 * timely LED updates and timeout processing even when no keyboard events
 * are occurring. Handles state transitions and LED feedback.
 * 
 * Performance Optimization:
 * - Early exit when state is IDLE (~3 CPU cycles)
 * - Only checks time when in active states
 * - LED updates only occur when in COMMAND_ACTIVE state
 * - Inlined LED logic to avoid function call overhead
 * - Typical overhead: <1μs when idle, ~10μs when active
 * 
 * Responsibilities:
 * - Checks for SHIFT_HOLD_WAIT → COMMAND_ACTIVE transition (3 second timer)
 * - Sends empty HID report when entering COMMAND_ACTIVE state
 * - Handles COMMAND_ACTIVE timeout (3 second auto-exit)
 * - Updates LED flashing (Green/Blue alternating every 100ms)
 * 
 * LED Update Frequency:
 * - Should be called frequently from main loop (runs at ~100kHz)
 * - LED updates occur every 100ms when in COMMAND_ACTIVE state
 * - Non-blocking: returns immediately if no update needed
 * 
 * @note Called from main loop (main.c) continuously
 * @note Safe to call even when no keyboard activity
 * @note Must be called regularly for timely state transitions
 * @note Optimized for minimal overhead during normal operation (99.99% of time)
 */
void command_mode_task(void);

/**
 * @brief Processes command mode state machine during keyboard events
 * 
 * This is called from handle_keyboard_report() after the keyboard report
 * has been updated with the current key press/release event. It determines
 * whether the keyboard report should be sent to the host or suppressed
 * due to command mode being active.
 * 
 * The function implements a state machine that:
 * - Monitors for ONLY both shift keys being held (no other keys)
 * - Aborts entry if any other key is pressed during SHIFT_HOLD_WAIT
 * - Allows normal HID reports during SHIFT_HOLD_WAIT state
 * - Suppresses all HID reports during COMMAND_ACTIVE state
 * - Detects command keys ('B' for bootloader)
 * - Handles early exit conditions
 * 
 * State-Specific Behavior:
 * - IDLE: Returns true (normal HID processing)
 * - SHIFT_HOLD_WAIT: Returns true (shifts sent to host normally)
 * - COMMAND_ACTIVE: Returns false (all HID reports suppressed)
 * 
 * Entry Requirements:
 * - ONLY Left Shift + Right Shift must be pressed
 * - No other modifiers (Ctrl, Alt, etc.)
 * - No regular keys in keycode array
 * - If any other key is pressed, command mode entry is aborted
 * 
 * @param keyboard_report Pointer to current keyboard HID report structure
 * @return true if normal keyboard processing should continue (send report)
 *         false if keyboard report should be suppressed (command mode active)
 * 
 * @note Called from handle_keyboard_report() after report is updated
 * @note Non-blocking design using time-based state checks
 * @note Thread-safe: main task context only
 */
bool command_mode_process(const hid_keyboard_report_t *keyboard_report);

#endif /* COMMAND_MODE_H */
