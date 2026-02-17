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

#include "hid_interface.h"

#include <stdio.h>

#include "bsp/board.h"
#include "tusb.h"

#include "command_mode.h"
#include "config.h"
#include "hid_keycodes.h"
#include "keymaps.h"
#include "led_helper.h"
#include "log.h"
#include "usb_descriptors.h"

/**
 * @brief HID Interface Usage Pages
 *
 * USB HID defines different "usage pages" for different types of input devices.
 * These constants identify the usage page in HID reports.
 */
enum {
    USAGE_PAGE_KEYBOARD = 0x0, /**< Standard keyboard usage page */
    USAGE_PAGE_CONSUMER = 0xC, /**< Consumer control usage page (multimedia keys) */
};

/**
 * @brief Shift modifier bit mask
 *
 * HID modifier byte layout (bit positions):
 * - Bit 1: Left Shift  (KC_LSHIFT = 0xE1)
 * - Bit 5: Right Shift (KC_RSHIFT = 0xE5)
 *
 * This mask is used to check or clear shift modifier states in keyboard reports.
 */
static const uint8_t SHIFT_MODIFIER_MASK = (1 << 1) | (1 << 5);

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
static hid_mouse_report_t    mouse_report;

/**
 * @brief Sends a HID report with automatic logging.
 *
 * This wrapper function logs the report contents when DEBUG enabled or on failure.
 * Using this function ensures all HID reports are consistently logged without
 * performance overhead (single function call vs separate log + send).
 *
 * The function builds a formatted hex dump of the report (matching USB wire format
 * with Report ID first) only when needed:
 * - Always when DEBUG logging enabled (log level >= LOG_LEVEL_DEBUG)
 * - Always when transmission fails (for error diagnostics)
 * - Never otherwise (zero overhead in production with DEBUG disabled)
 *
 * This optimized approach constructs the hex dump only once, even if both
 * DEBUG is enabled AND transmission fails. The appropriate log level (DEBUG
 * or ERROR) is chosen based on the final transmission result.
 *
 * Performance Characteristics:
 * - LOG_LEVEL < DEBUG + success: Only TinyUSB send (~1.6µs, near-zero overhead)
 * - LOG_LEVEL >= DEBUG + success: TinyUSB send + hex dump + UART (~16µs)
 * - Any level + failure: TinyUSB send attempt + hex dump + UART (~16µs)
 * - Inline function allows compiler to optimize based on call site
 * - Single hex dump construction (not duplicated if DEBUG enabled AND failure)
 *
 * Memory Overhead:
 * - Zero heap allocation (stack-only buffer)
 * - 128-byte stack buffer only allocated when logging needed
 * - Buffer immediately freed when function returns
 *
 * @param instance    The HID interface instance number
 * @param report_id   The HID report ID (automatically prepended by TinyUSB)
 * @param report      Pointer to the HID report data structure
 * @param len         Size of the report data in bytes
 * @return true if report was successfully queued for transmission, false on error
 *
 * @note This function should be used instead of calling tud_hid_n_report() directly
 * @note Failed transmissions are always logged regardless of log level
 * @note Hex dump only generated once (not duplicated on DEBUG+failure)
 */
static inline bool hid_send_report(uint8_t instance, uint8_t report_id, void const* report,
                                   uint16_t len) {
    // Check if endpoint is ready before attempting to send
    if (!tud_hid_n_ready(instance)) {
        LOG_DEBUG("HID endpoint not ready (instance=%u, report_id=0x%02X)\n", instance, report_id);
        return false;
    }

    // Check if we should build hex dump (DEBUG enabled or might fail)
    bool should_log = (log_get_level() >= (log_level_t)LOG_LEVEL_DEBUG);

    // Send via TinyUSB first
    bool result = tud_hid_n_report(instance, report_id, report, len);

    // Build hex dump if DEBUG enabled OR transmission failed
    // This ensures we only construct the report once, even if both conditions true
    if (should_log || !result) {
        // Build formatted hex dump: Report ID + data bytes (matches USB wire format)
        // Maximum HID report size is typically small (keyboard=8, mouse=5, consumer=2)
        // Buffer: prefix (32) + report ID (3) + hex bytes (len * 3) + null terminator
        char        buffer[128];
        const char* prefix = result ? "[SENT-HID-REPORT]" : "[FAILED-HID-REPORT]";
        size_t offset = (size_t)snprintf(buffer, sizeof(buffer), "%s %02X ", prefix, report_id);

        const uint8_t* p = (const uint8_t*)report;
        for (uint16_t i = 0; i < len && offset < sizeof(buffer) - 4; i++) {
            offset += (size_t)snprintf(buffer + offset, sizeof(buffer) - offset, "%02X ", *p++);
        }

        // Log at appropriate level based on result
        if (result) {
            LOG_DEBUG("%s\n", buffer);
        } else {
            LOG_ERROR("HID Report Send Failed (instance=%u, report_id=0x%02X, len=%u)\n", instance,
                      report_id, len);
            LOG_ERROR("%s\n", buffer);
        }
    }

    return result;
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
 * - Function scans ALL slots for duplicates before adding (prevents double-entry bug)
 * - Remembers first available empty slot during scan
 * - Returns false if all 6 slots are full (keyboard rollover limit)
 *
 * Duplicate Prevention:
 * - CRITICAL: Must check ALL slots before adding to any slot
 * - Previous implementation checked slot availability during duplicate scan,
 *   which could add a key to an early empty slot even if it existed later
 * - Bug scenario: Key A in slot 1, slot 0 empty, press A again → would add
 *   to slot 0, creating duplicate (A in both slot 0 and slot 1)
 * - Fix: Single loop that checks all slots for duplicates, remembers first empty slot,
 *   then adds to that slot only if key not already present
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
 * @note Fix for issue #19: Prevents duplicate keys in report array
 */
static bool hid_keyboard_add_key(uint8_t key) {
    if (IS_MOD(key)) {
        if ((keyboard_report.modifier & (uint8_t)(1 << (key & 0x7))) == 0) {
            keyboard_report.modifier |= (uint8_t)(1 << (key & 0x7));
            return true;
        }
        return false;
    }

    // Duplicate prevention approach to fix issue #19:
    // Single loop checks all 6 slots for duplicates while tracking first empty slot
    // Only adds key if: (1) not already present AND (2) empty slot available
    uint8_t validSlot = UINT8_MAX;  // Invalid slot sentinel
    for (uint8_t i = 0; i < 6; i++) {
        // Check for duplicate (already in report) - early return for performance
        if (keyboard_report.keycode[i] == key) {
            return false;  // Key already present, don't add again
        }
        // Remember first available slot (but don't use it yet!)
        // Only check if we haven't found a slot yet (validSlot == UINT8_MAX)
        if (keyboard_report.keycode[i] == 0 && validSlot == UINT8_MAX) {
            validSlot = i;
        }
    }

    // If we found an empty slot and key wasn't already present, add it
    if (validSlot < 6) {  // 0xFF > 6, so this checks if valid slot was found
        keyboard_report.keycode[validSlot] = key;
        return true;
    }

    // All 6 slots full (keyboard rollover limit reached)
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
 * @param rawcode Interface scan code (protocol-normalized, not raw scan code)
 *                Example: 0x48 for Pause key across all protocols
 * @param make    true for key press, false for key release
 *
 * @note Called from main task context via process_scancode()
 * @note Thread-safe: no interrupt access to keyboard_report or mouse_report
 */
void handle_keyboard_report(uint8_t rawcode, bool make) {
    // Convert the Interface Scancode to a HID Keycode
    bool    suppress_shift = false;
    uint8_t code           = keymap_get_key_val(rawcode, make, &suppress_shift);

    if (IS_KEY(code) || IS_MOD(code)) {
        bool report_modified = false;

        // Shift-override: temporarily remove shift modifiers if suppression requested
        uint8_t saved_modifiers = 0;
        if (suppress_shift) {
            saved_modifiers = keyboard_report.modifier;
            keyboard_report.modifier &=
                ~SHIFT_MODIFIER_MASK;  // Clear bits 1 (Left Shift) and 5 (Right Shift)
        }

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
            // Restore shift modifiers before returning (if they were suppressed)
            if (suppress_shift) {
                keyboard_report.modifier = saved_modifiers;
            }
            return;
        }

        if (report_modified) {
            // Send report with automatic logging (with shift still suppressed if needed)
            hid_send_report(ITF_NUM_KEYBOARD, REPORT_ID_KEYBOARD, &keyboard_report,
                            sizeof(keyboard_report));
        }

        // Restore shift modifiers after sending report (to reflect physical key state)
        if (suppress_shift) {
            keyboard_report.modifier = saved_modifiers;
        }
    } else if (IS_CONSUMER(code)) {
        uint16_t usage;
        if (make) {
            usage = CODE_TO_CONSUMER(code);
        } else {
            usage = 0;
        }

        // Send consumer control report with automatic logging
        hid_send_report(ITF_NUM_CONSUMER_CONTROL, REPORT_ID_CONSUMER_CONTROL, &usage,
                        sizeof(usage));
    }
}

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
 * - Shift-override system (keyboards with non-standard shift legends)
 * - Some keyboards have non-standard shift characters (terminal, vintage, international)
 * - E.g., key has '6' legend but sends scancode for '7'
 *
 * @return true if Left Shift OR Right Shift is currently pressed
 * @return false if neither shift key is pressed
 *
 * @note Called from keymap_get_key_val() during shift-override processing
 * @note Thread-safe: only called from main task context
 */
bool hid_is_shift_pressed(void) {
    // Check bits 1 (Left Shift) and 5 (Right Shift)
    return (keyboard_report.modifier & SHIFT_MODIFIER_MASK) != 0;
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
        .modifier = 0, .reserved = 0, .keycode = {0, 0, 0, 0, 0, 0}};

    // Send empty report with automatic logging
    if (hid_send_report(ITF_NUM_KEYBOARD, REPORT_ID_KEYBOARD, &empty_report,
                        sizeof(empty_report))) {
        LOG_INFO("Sent empty keyboard report (all keys released)\n");
    }
}

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
 * @note All calls from main task context (thread-safe)
 */
void handle_mouse_report(const uint8_t buttons[5], int8_t pos[3]) {
    // Handle Mouse Report
    mouse_report.buttons =
        buttons[0] | (buttons[1] << 1) | (buttons[2] << 2) | (buttons[3] << 3) | (buttons[4] << 4);
    mouse_report.x     = pos[0];
    mouse_report.y     = pos[1];
    mouse_report.wheel = pos[2];

    // Send mouse report with automatic logging
    hid_send_report(KEYBOARD_ENABLED ? ITF_NUM_MOUSE : ITF_NUM_KEYBOARD, REPORT_ID_MOUSE,
                    &mouse_report, sizeof(mouse_report));
}

/**
 * @brief TinyUSB callback invoked when host sends GET_REPORT control request
 *
 * This callback is invoked by TinyUSB when the USB host requests the current state
 * of a HID report via a GET_REPORT control transfer. The application should populate
 * the buffer with the requested report data and return its length.
 *
 * Use Cases:
 * - Host polls current keyboard LED state
 * - Host queries current key state after resume
 * - Debugging tools request report state
 *
 * Return Value:
 * - >0: Number of bytes written to buffer (report sent to host)
 * - 0: Request will be STALLed by TinyUSB stack (not supported)
 *
 * Current Implementation:
 * - Not implemented (returns 0, STALLs all GET_REPORT requests)
 * - Keyboards typically don't need this (reports sent via interrupt endpoint)
 * - Could be implemented for LED state queries if needed
 *
 * @param instance    HID interface instance number (ITF_NUM_KEYBOARD, ITF_NUM_MOUSE, etc.)
 * @param report_id   Report ID being requested (REPORT_ID_KEYBOARD, REPORT_ID_MOUSE, etc.)
 * @param report_type Report type (INPUT, OUTPUT, or FEATURE)
 * @param buffer      Buffer to fill with report data (provided by TinyUSB)
 * @param reqlen      Maximum buffer length available
 *
 * @return Number of bytes written to buffer, or 0 to STALL request
 *
 * @note Called from TinyUSB USB interrupt context
 * @note Currently not implemented - all requests STALLed
 * @note Standard keyboards rarely need this callback
 */
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type,
                               uint8_t* buffer, uint16_t reqlen) {
    // Not implemented - STALL all GET_REPORT requests
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)reqlen;

    return 0;
}

/**
 * @brief TinyUSB callback invoked when host sends SET_REPORT control request or OUT endpoint data
 *
 * This callback is invoked by TinyUSB when:
 * 1. USB host sends SET_REPORT control transfer (typically for keyboard LEDs)
 * 2. Host sends data to HID OUT endpoint (if configured)
 *
 * The application parses the received report and takes appropriate action based
 * on report type and report ID.
 *
 * Current Implementation:
 * - Handles keyboard OUTPUT reports (LED state: Caps Lock, Num Lock, Scroll Lock)
 * - Forwards LED state to keyboard interface via set_lock_values_from_hid()
 * - Ignores all other report types and instances
 *
 * Keyboard LED Mapping (buffer[0] bit field):
 * - Bit 0: Num Lock LED
 * - Bit 1: Caps Lock LED
 * - Bit 2: Scroll Lock LED
 * - Bit 3: Compose LED (unused)
 * - Bit 4: Kana LED (unused)
 * - Bits 5-7: Reserved
 *
 * Report Flow:
 * 1. Host updates LED state (user presses Caps Lock)
 * 2. TinyUSB receives OUTPUT report via control transfer
 * 3. This callback invoked with LED bit field
 * 4. set_lock_values_from_hid() updates protocol handler
 * 5. Protocol sends LED command to physical keyboard
 *
 * @param instance    HID interface instance number (ITF_NUM_KEYBOARD, ITF_NUM_MOUSE, etc.)
 * @param report_id   Report ID of received data (REPORT_ID_KEYBOARD, etc.)
 * @param report_type Report type (HID_REPORT_TYPE_OUTPUT, INPUT, or FEATURE)
 * @param buffer      Pointer to received report data
 * @param bufsize     Size of received report data in bytes
 *
 * @note Called from TinyUSB USB interrupt context
 * @note Only processes keyboard OUTPUT reports (LED state)
 * @note Non-keyboard instances and report types ignored
 */
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type,
                           uint8_t const* buffer, uint16_t bufsize) {
    // Ignore non-keyboard instances
    if (instance != ITF_NUM_KEYBOARD)
        return;

    if (report_type == HID_REPORT_TYPE_OUTPUT) {
        // Set keyboard LED e.g Capslock, Numlock etc...
        if (report_id == REPORT_ID_KEYBOARD) {
            // bufsize should be (at least) 1
            if (bufsize < 1)
                return;
            set_lock_values_from_hid(buffer[0]);
        }
    }
}

/**
 * @brief Initializes the USB HID device and TinyUSB stack
 *
 * This function performs the essential initialization sequence for USB HID operation.
 * Must be called once during startup before any USB or HID functionality is used.
 *
 * Initialization Sequence:
 * 1. board_init() - Initializes RP2040 board peripherals (GPIO, clocks, UART)
 * 2. tusb_init() - Initializes TinyUSB stack (USB device controller, endpoints, descriptors)
 *
 * After Initialization:
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
 * @note Called once from main() before entering main loop
 * @note Must be called before any tud_*() TinyUSB functions
 * @note Enables USB device enumeration with host
 */
void hid_device_setup(void) {
    board_init();
    tusb_init();
}
