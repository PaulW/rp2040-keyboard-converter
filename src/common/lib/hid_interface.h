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

/**
 * @file hid_interface.h
 * @brief USB HID report generation interface for keyboard, consumer control, and mouse output.
 *
 * This module bridges the keyboard and mouse protocol implementations with the TinyUSB HID
 * device stack. It maintains report state for three independent HID endpoints and handles
 * report submission to the USB host.
 *
 * HID Endpoints:
 * - Keyboard: standard 6-key rollover report (8-bit modifier + 6-byte keycode array)
 * - Consumer Control: multimedia/media key report (single 16-bit HID usage code)
 * - Mouse: 5-button + XY movement + scroll wheel (signed 8-bit deltas, relative)
 *
 * Report Deduplication:
 * Reports are only transmitted when state actually changes. This prevents USB bus
 * saturation from typematic repeat bytes sent by some keyboards — the host handles
 * key-repeat, not the firmware. A `report_modified` flag guards each send path.
 *
 * TinyUSB Integration:
 * `tud_task()` must be called regularly from the main loop to service USB device events.
 * All send functions in this module require `tud_hid_ready()` to return true before
 * attempting transmission. Failed sends are logged but do not stall the main loop.
 *
 * Data Flow:
 *   PIO → IRQ → ring buffer → protocol task → process_scancode() →
 *   keymap_get_key_val() → handle_keyboard_report() → hid_send_report() → tud_hid_n_report()
 *
 * @note All public functions are main loop only. Check `tud_hid_ready()` before
 *       calling any send function.
 */

#ifndef HID_INTERFACE_H
#define HID_INTERFACE_H

#include <stdbool.h>
#include <stdint.h>

#include "pico/stdlib.h"

#include "config.h"

/**
 * @brief Handles input reports for keyboard and consumer control devices
 *
 * This is the main entry point for processing translated scan codes and sending
 * USB HID reports to the host. It handles three types of HID reports:
 *
 * 1. Keyboard Reports (Standard Keys and Modifiers):
 * - Translates interface scan codes to HID keycodes via keymap lookup
 * - Updates keyboard report structure (modifiers + 6-key array)
 * - Only sends USB report if state changed (prevents duplicate reports)
 * - Handles typematic prevention (host handles key repeat, not device)
 *
 * 2. Consumer Control Reports (Multimedia Keys):
 * - Media keys (play, pause, volume, etc.)
 * - Uses separate USB HID interface for consumer controls
 * - Single 16-bit usage code per report
 * - Code 0 indicates "no keys pressed" for key release
 *
 * 3. Command Mode System:
 * - Time-based special function access for 2KRO keyboards
 * - Hold both shifts for 3 seconds to enter command mode
 * - LED provides visual feedback (Green/Blue alternating flash)
 * - Press 'B' for bootloader entry (other commands can be added)
 * - 3 second timeout or shift release exits command mode
 * - Keyboard reports suppressed during command mode operation
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
 * @param rawcode Interface scan code (protocol-normalised, not raw scan code)
 *                Example: 0x48 for Pause key across all protocols
 * @param make    true for key press, false for key release
 *
 * @note Main loop only.
 * @note Thread-safe: no interrupt access to keyboard_report or mouse_report
 */
void handle_keyboard_report(uint8_t rawcode, bool make);

/**
 * @brief Handles mouse input reports and sends to USB HID interface
 *
 * This function processes mouse input by updating the mouse report structure with
 * button states and movement/scroll values, then transmits the report via USB HID.
 *
 * Report Structure:
 * - buttons: 5-button bit field (buttons[0-4] mapped to bits 0-4)
 * - x: Horizontal movement (-127 to +127, signed 8-bit)
 * - y: Vertical movement (-127 to +127, signed 8-bit)
 * - wheel: Scroll wheel movement (-127 to +127, signed 8-bit)
 *
 * Button Mapping:
 * - buttons[0]: Left button (bit 0)
 * - buttons[1]: Right button (bit 1)
 * - buttons[2]: Middle button (bit 2)
 * - buttons[3]: Button 4 (bit 3)
 * - buttons[4]: Button 5 (bit 4)
 *
 * Movement Characteristics:
 * - Movement values are deltas (relative to previous position)
 * - Range: -127 to +127 per report
 * - Host integrates deltas to track cursor position
 * - Zero values indicate no movement
 *
 * Error Handling:
 * - Failed transmissions logged automatically via hid_send_report()
 * - Operation continues on failure (transient USB issues)
 * - Report contents logged in DEBUG mode
 *
 * @param buttons Array of 5 button states (0=released, 1=pressed)
 * @param pos Array of 3 movement values: [x, y, wheel] (signed 8-bit deltas)
 *
 * @note Called from mouse protocol handlers (AT/PS2 mouse interface)
 * @note Uses separate USB endpoint from keyboard (ITF_NUM_MOUSE or ITF_NUM_KEYBOARD)
 * @note Main loop only.
 */
void handle_mouse_report(const uint8_t buttons[5], const int8_t pos[3]);

/**
 * @brief Initialises the USB HID device and TinyUSB stack
 *
 * This function performs the essential initialisation sequence for USB HID operation.
 * Must be called once during startup before any USB or HID functionality is used.
 *
 * Initialisation Sequence:
 * 1. board_init() - Initialises RP2040 board peripherals (GPIO, clocks, UART)
 * 2. tusb_init() - Initialises TinyUSB stack (USB device controller, endpoints, descriptors)
 *
 * After Initialisation:
 * - USB device controller configured and ready
 * - HID endpoints created (keyboard, consumer control, mouse)
 * - USB device visible to host (enumeration begins)
 * - Application can send HID reports via handle_keyboard_report(), handle_mouse_report()
 *
 * TinyUSB Configuration:
 * - Device descriptors defined in usb_descriptors.c
 * - HID report descriptors for keyboard, consumer control, mouse
 * - Boot protocol support for BIOS compatibility
 * - Endpoints: 0x81 (keyboard), 0x82 (consumer), 0x83 (mouse)
 *
 * @note Main loop only.
 * @note Called once from main() before entering main loop
 * @note Must be called before any tud_*() TinyUSB functions
 * @note Enables USB device enumeration with host
 */
void hid_device_setup(void);

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
 * @note Main loop only.
 * @note Called from command mode on COMMAND_ACTIVE entry and on any exit path
 * @note Only resets internal keyboard_report state on successful transmission.
 *       If the send fails, keyboard_report is left unchanged so host and firmware
 *       state remain consistent — no spurious break events are generated.
 */
void send_empty_keyboard_report(void);

/**
 * @brief Checks if either Shift key is currently pressed
 *
 * This function checks the modifier field of the current keyboard report
 * to determine if Left Shift (0xE1) or Right Shift (0xE5) is pressed.
 *
 * Modifier Bit Positions:
 * - Bit 0: Left Control (0xE0)
 * - Bit 1: Left Shift (0xE1)      <-- checked
 * - Bit 2: Left Alt (0xE2)
 * - Bit 3: Left GUI (0xE3)
 * - Bit 4: Right Control (0xE4)
 * - Bit 5: Right Shift (0xE5)     <-- checked
 * - Bit 6: Right Alt (0xE6)
 * - Bit 7: Right GUI (0xE7)
 *
 * Use Case:
 * - Shift-Override system (keyboards with non-standard shift legends)
 * - Some keyboards have non-standard shift characters (terminal, vintage, international)
 * - E.g., key has '6' legend but sends scancode for '7'
 *
 * @return true if Left Shift OR Right Shift is currently pressed
 * @return false if neither shift key is pressed
 *
 * @note Called from keymap_get_key_val() during shift-override processing
 * @note Main loop only.
 */
bool hid_is_shift_pressed(void);

#endif /* HID_INTERFACE_H */
