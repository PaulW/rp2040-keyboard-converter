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
 * @file scancode.c
 * @brief Commodore Amiga Keyboard Scancode Processor
 *
 * This implementation processes scan codes from Commodore Amiga keyboards (A1000,
 * A500, A2000, A3000, A4000). The Amiga protocol uses a simple make/break encoding
 * with one special case for the CAPS LOCK key.
 *
 * Protocol Overview:
 * - Key codes: 0x00-0x67 (7-bit values, 104 keys)
 * - Bit 7: Key state flag
 *   * 0 = Key pressed (make code)
 *   * 1 = Key released (break code)
 * - No multi-byte sequences (unlike PC/XT protocols)
 * - No prefix codes (E0, E1, etc.)
 * - All keys send up/down events EXCEPT CAPS LOCK
 *
 * CAPS LOCK Special Behaviour:
 * The Amiga CAPS LOCK key is unique in keyboard history:
 * 1. Only sends code on PRESS (never on release)
 * 2. Bit 7 indicates LED state (not press/release):
 *    - Bit 7 = 0 (0x62): CAPS LOCK LED is now ON (key was pressed whilst LED was off)
 *    - Bit 7 = 1 (0xE2): CAPS LOCK LED is now OFF (key was pressed whilst LED was on)
 * 3. Computer must track LED state to stay synchronised
 * 4. On power-up, LED starts OFF
 *
 * Why This Design?:
 * The Amiga keyboard maintains its own CAPS LOCK LED state internally and toggles
 * it on each press. The bit 7 flag tells the computer the NEW state after toggle,
 * not whether this is a press or release. This prevents desync between keyboard
 * LED and computer state.
 *
 * Implementation Strategy:
 * - Most keys: Extract bit 7 for make/break, pass key code to HID layer
 * - CAPS LOCK: Special handling in this scancode processor
 *   * Compares keyboard LED state (from bit 7) with USB HID CAPS LOCK state
 *   * Only sends toggle when states differ (smart synchronisation)
 *   * Generates press+release pair with configurable hold time (CAPS_LOCK_TOGGLE_TIME_MS)
 *   * Timed release managed by scancode_task() from the main loop
 *
 * Data Flow:
 * Protocol Layer → De-rotation → Active-low inversion → THIS MODULE → HID Layer
 *
 * Example Scancodes:
 * - Space pressed:  0x40 (bit 7 = 0, key 0x40)
 * - Space released: 0xC0 (bit 7 = 1, key 0x40)
 * - CAPS LOCK (any press): 0x62 or 0xE2 → Generate press+release pair → Toggle USB CAPS LOCK
 *
 * Special Protocol Codes:
 * The following codes are handled by the protocol layer (keyboard_interface.c),
 * not by this scancode processor:
 * - 0x78: Reset warning (CTRL + both Amiga keys)
 * - 0xF9: Lost sync (keyboard will retransmit)
 * - 0xFA: Buffer overflow
 * - 0xFC: Self-test failure
 * - 0xFD: Power-up key stream start
 * - 0xFE: Power-up key stream end
 *
 * @see scancode.h for the public API.
 */

#include "scancode.h"

#include <stdbool.h>
#include <stdint.h>

#include "pico/time.h"

#include "config.h"
#include "flow_tracker.h"
#include "hid_interface.h"
#include "keyboard_interface.h"
#include "led_helper.h"
#include "log.h"

/**
 * @brief Amiga CAPS LOCK key code
 */
#define AMIGA_CAPSLOCK_KEY 0x62

/**
 * @brief CAPS LOCK timing state machine
 *
 * MacOS and some systems require CAPS LOCK held briefly (~100-150ms) to register.
 * This state machine delays the release event whilst keeping clean architecture:
 * - Protocol layer: ISR queues scancodes to ring buffer (single-producer)
 * - Main loop: Reads ring buffer, calls process_scancode()
 * - Scancode processor: Handles HID timing here (not in protocol layer)
 * - No ring buffer violations
 *
 * Hold time configured via CAPS_LOCK_TOGGLE_TIME_MS in config.h (reusable across protocols).
 */
typedef enum {
    CAPS_IDLE,       /**< No pending CAPS LOCK operation */
    CAPS_PRESS_SENT, /**< Press sent, waiting to send release */
} caps_lock_state_t;

typedef struct {
    caps_lock_state_t state;
    uint32_t          press_time_ms; /**< Time when press was sent */
} caps_lock_timing_t;

static caps_lock_timing_t caps_lock_timing = {.state = CAPS_IDLE, .press_time_ms = 0};

// --- Public Functions ---

void process_scancode(uint8_t code) {
    FLOW_STEP(code);
    // Extract key code (bits 6-0) and make/break flag (bit 7)
    uint8_t key_code = code & AMIGA_KEYCODE_MASK;      // Remove bit 7
    bool    is_break = (code & AMIGA_BREAK_BIT) != 0;  // Check bit 7: 1 = break, 0 = make
    bool    make     = !is_break;  // Invert for handle_keyboard_report convention

    // Validate key code range (0x00-0x67 are valid)
    // Special codes (0x78, 0xF9-0xFE) should be filtered by protocol layer
    if (key_code > AMIGA_MAX_KEYCODE) {
        LOG_ERROR("Invalid Amiga key code: 0x%02X (raw scancode: 0x%02X)\n", key_code, code);
        return;
    }

    // Special handling for CAPS LOCK (0x62/0xE2)
    // Amiga CAPS LOCK only sends on press, bit 7 = LED state (not press/release)
    // Generate press+release pair with timing delay for USB HID compatibility
    if (key_code == AMIGA_CAPSLOCK_KEY) {
        bool kbd_led_on  = !is_break;                // bit 7=0 → LED ON, bit 7=1 → LED OFF
        bool hid_caps_on = lock_leds.keys.capsLock;  // Current USB HID CAPS LOCK state

        LOG_DEBUG("Amiga CAPS LOCK: kbd LED %s, USB HID %s [raw: 0x%02X]\n",
                  kbd_led_on ? "ON" : "OFF", hid_caps_on ? "ON" : "OFF", code);

        // If a release is still pending, send it immediately before processing new press
        if (caps_lock_timing.state == CAPS_PRESS_SENT) {
            handle_keyboard_report(AMIGA_CAPSLOCK_KEY, false);
            caps_lock_timing.state = CAPS_IDLE;
            LOG_DEBUG("Amiga CAPS LOCK: forced early release before new press\n");
        }

        // Only toggle if states differ (synchronisation logic)
        if (kbd_led_on != hid_caps_on) {
            LOG_DEBUG("Amiga CAPS LOCK out of sync - sending toggle\n");
            // Send press immediately
            handle_keyboard_report(AMIGA_CAPSLOCK_KEY, true);
            // Start timing state machine - release will be sent after delay
            caps_lock_timing.state         = CAPS_PRESS_SENT;
            caps_lock_timing.press_time_ms = to_ms_since_boot(get_absolute_time());
        } else {
            LOG_DEBUG("Amiga CAPS LOCK already in sync - no action\n");
        }
        return;
    }

    // Normal keys: process as simple make/break
    handle_keyboard_report(key_code, make);
}

void scancode_task(void) {
    // CAPS LOCK timing state machine - send delayed release
    if (caps_lock_timing.state == CAPS_PRESS_SENT) {
        uint32_t now_ms = to_ms_since_boot(get_absolute_time());
        if (now_ms - caps_lock_timing.press_time_ms >= CAPS_LOCK_TOGGLE_TIME_MS) {
            // Time expired - send release directly to HID layer
            // Bypasses ring buffer (this is firmware-generated, not a keyboard event)
            handle_keyboard_report(AMIGA_CAPSLOCK_KEY, false);
            LOG_DEBUG("Amiga CAPS LOCK release sent after %ums hold\n",
                      (unsigned int)CAPS_LOCK_TOGGLE_TIME_MS);
            caps_lock_timing.state = CAPS_IDLE;
        }
    }
}
