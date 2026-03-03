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
 * @file hid_interface.c
 * @brief HID interface implementation for USB report generation.
 *
 * Implements TinyUSB HID callback handlers and report submission logic for
 * keyboard, consumer control, and mouse HID endpoints.
 *
 * @note Report-generation entry points are main-loop only.
 * @note TinyUSB callbacks in this file are invoked by USB stack callback context.
 */

#include "hid_interface.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "bsp/board.h"
#include "tusb.h"

#include "command_mode.h"
#include "config.h"
#include "flow_tracker.h"
#include "hid_keycodes.h"
#include "keymaps.h"
#include "led_helper.h"
#include "log.h"
#include "usb_descriptors.h"

// --- Configuration Constants ---

/**
 * @brief Maximum buffer size for HID report logging
 *
 * This defines the size of the stack buffer used to construct hex dumps of HID reports for logging
 * purposes.
 */
#define HID_REPORT_LOG_BUFFER_SIZE 128

/**
 * @brief Shift modifier bit mask
 *
 * HID modifier byte layout (bit positions):
 * - Bit 1: Left Shift  (KC_LSHIFT = 0xE1)
 * - Bit 5: Right Shift (KC_RSHIFT = 0xE5)
 *
 * This mask is used to check or clear shift modifier states in keyboard reports.
 */
#define SHIFT_MODIFIER_MASK ((uint8_t)((1U << 1) | (1U << 5)))

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
 * @brief HID Report Structures
 *
 * These static variables maintain the current state of HID reports.
 * They are only accessed from the main task context, so no volatile
 * qualification or synchronisation is needed.
 *
 * Threading Model:
 * - All HID report functions called from main task context
 * - Ring buffer provides thread-safe boundary from interrupt handlers
 * - No direct interrupt access to these structures
 */
static hid_keyboard_report_t keyboard_report;
static hid_mouse_report_t    mouse_report;

// --- Private Functions ---

/**
 * @brief Dispatches log messages for a HID report send outcome
 *
 * Emits the appropriate LOG_DEBUG or LOG_ERROR message(s) for a given outcome.
 * The caller is responsible for building the hex dump buffer and deciding whether
 * to invoke this function — it is only called when logging is needed.
 *
 * - Success (result=true):           LOG_DEBUG with hex dump
 * - Endpoint not ready (!ready):     LOG_ERROR — report dropped, hex dump always emitted
 * - TinyUSB failure (ready, !result): LOG_ERROR — send failed, hex dump always emitted
 *
 * @param result    Return value from tud_hid_n_report() (true = queued successfully)
 * @param ready     Return value from tud_hid_n_ready() (true = endpoint was ready)
 * @param instance  HID interface instance number (for error messages)
 * @param report_id HID report ID (for error messages)
 * @param hex_buf   Pre-built, null-terminated hex dump string of the report
 * @param len       Size of the original report data in bytes (for error messages)
 *
 * @note Main loop only.
 * @note Only called when logging is required; guard logic lives in hid_send_report()
 */
static void log_hid_report_outcome(bool result, bool ready, uint8_t instance, uint8_t report_id,
                                   const char* hex_buf, uint16_t len) {
    if (result) {
        LOG_DEBUG("%s\n", hex_buf);
        return;
    }
    // Both failure paths share the hex dump line; only the header message differs
    if (!ready) {
        LOG_ERROR("HID report dropped, endpoint not ready (instance=%u, report_id=0x%02X)\n",
                  instance, report_id);
    } else {
        LOG_ERROR("HID Report Send Failed (instance=%u, report_id=0x%02X, len=%u)\n", instance,
                  report_id, len);
    }
    LOG_ERROR("%s\n", hex_buf);
}

/**
 * @brief Sends a HID report with automatic logging.
 *
 * Wrapper around tud_hid_n_report() that consistently logs report contents
 * on failure or when DEBUG logging is enabled, with zero overhead otherwise.
 *
 * Owns the guard decision and hex dump construction; delegates log dispatch
 * to log_hid_report_outcome() so the two responsibilities stay separate:
 * - This function: send attempt, should-log guard, buffer construction
 * - log_hid_report_outcome(): log level selection and message formatting
 *
 * Performance Characteristics:
 * - LOG_LEVEL < DEBUG + success: Only TinyUSB send (~1.6µs, near-zero overhead)
 * - LOG_LEVEL >= DEBUG + success: TinyUSB send + hex dump + UART (~16µs)
 * - Any level + send failure: send attempt + hex dump + UART (~16µs)
 * - Inline function allows compiler to optimise based on call site
 * - Single hex dump construction (not duplicated if DEBUG enabled AND failure)
 *
 * Memory Overhead:
 * - Zero heap allocation (stack-only buffer)
 * - HID_REPORT_LOG_BUFFER_SIZE-byte stack buffer only allocated when logging needed
 * - Buffer immediately freed when function returns
 *
 * @param instance    The HID interface instance number
 * @param report_id   The HID report ID (automatically prepended by TinyUSB)
 * @param report      Pointer to the HID report data structure
 * @param len         Size of the report data in bytes
 * @return true if report was successfully queued for transmission, false on error
 *
 * @note Use instead of calling tud_hid_n_report() directly
 * @note All send failures are always logged at ERROR with hex dump
 * @note Successful sends are only hex-dumped when DEBUG logging is enabled
 * @note Hex dump built at most once per call (not duplicated on DEBUG+failure)
 */
static inline bool hid_send_report(uint8_t instance, uint8_t report_id, void const* report,
                                   uint16_t len) {
    FLOW_END(report_id);
    // Check endpoint readiness and attempt to send
    bool ready  = tud_hid_n_ready(instance);
    bool result = ready && tud_hid_n_report(instance, report_id, report, len);

    bool should_log  = (log_get_level() >= (log_level_t)LOG_LEVEL_DEBUG);
    bool send_failed = !result;

    // Build hex dump and dispatch logging only when needed
    if (should_log || send_failed) {
        // Build formatted hex dump: Report ID + data bytes (matches USB wire format)
        // Maximum HID report size is typically small (keyboard=8, mouse=5, consumer=2)
        // Buffer: prefix (32) + report ID (3) + hex bytes (len * 3) + null terminator
        char        buffer[HID_REPORT_LOG_BUFFER_SIZE];
        const char* prefix = result ? "[SENT-HID-REPORT]" : "[FAILED-HID-REPORT]";
        size_t offset = (size_t)snprintf(buffer, sizeof(buffer), "%s %02X ", prefix, report_id);

        const uint8_t* report_ptr = (const uint8_t*)report;
        for (uint16_t i = 0; i < len && offset < sizeof(buffer) - 4; i++) {
            offset +=
                (size_t)snprintf(buffer + offset, sizeof(buffer) - offset, "%02X ", *report_ptr++);
        }

        log_hid_report_outcome(result, ready, instance, report_id, buffer, len);
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
    FLOW_STEP(key);
    if (IS_MOD(key)) {
        if ((keyboard_report.modifier & (uint8_t)(1 << (key & 0x7))) == 0) {
            keyboard_report.modifier |= (uint8_t)(1 << (key & 0x7));
            return true;
        }
        return false;
    }

    // Duplicate prevention approach to fix issue #19:
    // Single loop checks all 6 slots for duplicates whilst tracking first empty slot
    // Only adds key if: (1) not already present AND (2) empty slot available
    const uint8_t keycode_slots =
        (uint8_t)(sizeof(keyboard_report.keycode) / sizeof(keyboard_report.keycode[0]));
    uint8_t valid_slot = UINT8_MAX;  // Invalid slot sentinel
    for (uint8_t i = 0; i < keycode_slots; i++) {
        // Check for duplicate (already in report) - early return for performance
        if (keyboard_report.keycode[i] == key) {
            return false;  // Key already present, don't add again
        }
        // Remember first available slot (but don't use it yet!)
        // Only check if we haven't found a slot yet (valid_slot == UINT8_MAX)
        if (keyboard_report.keycode[i] == 0 && valid_slot == UINT8_MAX) {
            valid_slot = i;
        }
    }

    // If we found an empty slot and key wasn't already present, add it
    if (valid_slot < keycode_slots) {  // 0xFF > slots, so this checks if valid slot was found
        keyboard_report.keycode[valid_slot] = key;
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
 * Report Optimisation:
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
    FLOW_STEP(key);
    if (IS_MOD(key)) {
        uint8_t modifier_bit = (uint8_t)(1 << (key & 0x7));
        if ((keyboard_report.modifier & modifier_bit) != 0) {
            keyboard_report.modifier &= (uint8_t)~modifier_bit;
            return true;
        }
        return false;
    }

    const size_t keycode_slots =
        sizeof(keyboard_report.keycode) / sizeof(keyboard_report.keycode[0]);
    for (size_t i = 0; i < keycode_slots; i++) {
        if (keyboard_report.keycode[i] == key) {
            keyboard_report.keycode[i] = 0;
            return true;
        }
    }
    return false;
}

/**
 * @brief Apply shift-suppression to the current keyboard report
 *
 * If suppress is true, saves the current modifier byte into *saved_modifiers and
 * clears the shift bits (Left Shift bit 1, Right Shift bit 5) from
 * keyboard_report.modifier so that the key event is delivered without shift held.
 *
 * @param suppress        Whether to apply suppression
 * @param saved_modifiers Caller-supplied storage for the saved modifier byte
 */
static void apply_shift_suppression(bool suppress, uint8_t* saved_modifiers) {
    if (suppress) {
        *saved_modifiers = keyboard_report.modifier;
        keyboard_report.modifier &=
            (uint8_t)~SHIFT_MODIFIER_MASK;  // Clear bits 1 (LShift) and 5 (RShift)
    }
}

/**
 * @brief Restore modifier byte after shift-suppression
 *
 * If suppress is true, writes saved_modifiers back to keyboard_report.modifier,
 * restoring the physical shift state that was cleared by apply_shift_suppression().
 *
 * @param suppress        Whether suppression was applied
 * @param saved_modifiers The byte previously saved by apply_shift_suppression()
 */
static void restore_shift_suppression(bool suppress, uint8_t saved_modifiers) {
    if (suppress) {
        keyboard_report.modifier = saved_modifiers;
    }
}

/**
 * @brief Evaluate command-mode processing for the current key event
 *
 * Builds a temporary copy of keyboard_report with the original modifiers (undoing
 * any active shift-suppression) so command_mode_process() can observe the real
 * physical shift state, then returns whether normal report processing is allowed.
 *
 * @param suppress_shift  Whether shift-suppression is active for this event
 * @param saved_modifiers The original modifier byte saved before suppression
 * @return true  Normal HID report processing should continue
 * @return false Command mode consumed the event; caller must suppress the report
 */
static bool evaluate_command_mode(bool suppress_shift, uint8_t saved_modifiers) {
    hid_keyboard_report_t cmd_report = keyboard_report;
    if (suppress_shift) {
        cmd_report.modifier = saved_modifiers;
    }
    return command_mode_process(&cmd_report);
}

// --- Public Functions ---

void handle_keyboard_report(uint8_t rawcode, bool make) {
    FLOW_STEP(rawcode);
    // Convert the Interface Scancode to a HID Keycode
    bool    suppress_shift = false;
    uint8_t code           = keymap_get_key_val(rawcode, make, &suppress_shift);

    if (IS_KEY(code) || IS_MOD(code)) {
        bool    report_modified = false;
        uint8_t saved_modifiers = 0;

        // Shift-Override: temporarily remove shift modifiers while mutating/sending
        apply_shift_suppression(suppress_shift, &saved_modifiers);

        if (make) {
            report_modified = hid_keyboard_add_key(code);
        } else {
            report_modified = hid_keyboard_del_key(code);
        }

        // Process command mode; pass a copy with original modifiers so command mode
        // can observe the real physical shift state regardless of suppression
        bool allow_normal_processing = evaluate_command_mode(suppress_shift, saved_modifiers);

        if (!allow_normal_processing) {
            // Command mode is active — suppress normal keyboard report
            restore_shift_suppression(suppress_shift, saved_modifiers);
            return;
        }

        if (report_modified) {
            // Send report with shift still suppressed as required by shift-override
            hid_send_report(ITF_NUM_KEYBOARD, REPORT_ID_KEYBOARD, &keyboard_report,
                            sizeof(keyboard_report));
        }

        // Restore shift modifiers after send to reflect physical key state
        restore_shift_suppression(suppress_shift, saved_modifiers);
    } else if (IS_CONSUMER(code)) {
        uint16_t usage = make ? CODE_TO_CONSUMER(code) : 0;

        // Send consumer control report with automatic logging
        hid_send_report(ITF_NUM_CONSUMER_CONTROL, REPORT_ID_CONSUMER_CONTROL, &usage,
                        sizeof(usage));
    }
}

bool hid_is_shift_pressed(void) {
    // Check bits 1 (Left Shift) and 5 (Right Shift)
    return (keyboard_report.modifier & SHIFT_MODIFIER_MASK) != 0;
}

void send_empty_keyboard_report(void) {
    // Prepare a local zeroed report to send without touching keyboard_report yet.
    // keyboard_report is only cleared once the host has been told all keys are released,
    // keeping host and firmware state consistent on send failure.
    hid_keyboard_report_t zero_report = {0};

    if (hid_send_report(ITF_NUM_KEYBOARD, REPORT_ID_KEYBOARD, &zero_report, sizeof(zero_report))) {
        // Host acknowledged — now safe to discard tracked keys from firmware state
        keyboard_report = zero_report;
        LOG_INFO("Sent empty keyboard report (all keys released)\n");
    } else {
        // Send failed; hid_send_report() has already logged the error + hex dump.
        // Leave keyboard_report unchanged to avoid diverging host/firmware state.
        LOG_ERROR("Failed to send empty keyboard report; firmware state unchanged\n");
    }
}

void handle_mouse_report(const uint8_t buttons[5], const int8_t pos[3]) {
    // Handle Mouse Report
    // Cast each shifted uint8_t explicitly to avoid implicit integer promotion warnings
    mouse_report.buttons =
        (uint8_t)(buttons[0] | (uint8_t)(buttons[1] << 1) | (uint8_t)(buttons[2] << 2) |
                  (uint8_t)(buttons[3] << 3) | (uint8_t)(buttons[4] << 4));
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
uint16_t tud_hid_get_report_cb(
    uint8_t instance, uint8_t report_id,  // NOLINT(bugprone-easily-swappable-parameters)
    hid_report_type_t report_type,        // NOLINT(bugprone-easily-swappable-parameters)
    uint8_t* buffer, uint16_t reqlen) {   // NOLINT(readability-non-const-parameter)
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
void tud_hid_set_report_cb(
    uint8_t instance, uint8_t report_id,  // NOLINT(bugprone-easily-swappable-parameters)
    hid_report_type_t report_type,        // NOLINT(bugprone-easily-swappable-parameters)
    uint8_t const* buffer, uint16_t bufsize) {
    // Ignore non-keyboard instances
    if (instance != ITF_NUM_KEYBOARD) {
        return;
    }

    if (report_type == HID_REPORT_TYPE_OUTPUT) {
        // Set keyboard LED e.g Capslock, Numlock etc...
        if (report_id == REPORT_ID_KEYBOARD) {
            // bufsize should be (at least) 1
            if (bufsize == 0) {
                return;
            }
            set_lock_values_from_hid(buffer[0]);
        }
    }
}

void hid_device_setup(void) {
    board_init();
    tusb_init();
}
