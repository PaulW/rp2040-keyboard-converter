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
 * @file led_helper.h
 * @brief Converter status LED management and USB HID lock indicator synchronisation.
 *
 * Manages two independent LED concerns: the converter's own status indicators and the
 * host-controlled lock indicator LEDs (Num Lock, Caps Lock, Scroll Lock).
 *
 * Converter Status LED:
 * When CONVERTER_LEDS is defined, a WS2812 RGB LED reflects the converter's operational
 * state via the `converter_state_union` bitfield:
 * - kb_ready:    keyboard protocol initialised and communicating
 * - mouse_ready: mouse protocol initialised and communicating
 * - fw_flash:    bootloader mode pending (magenta)
 * - cmd_mode:    command mode active (green/blue alternating, or green/pink for log selection)
 * Without CONVERTER_LEDS, all LED update calls compile away to nothing.
 *
 * Lock Indicator LEDs:
 * The `lock_leds` union (lock_keys_union) is updated by the TinyUSB HID
 * `tud_hid_set_report_cb()` callback when the host changes lock key state.
 * Protocol implementations read `lock_leds.value` to forward the state to the
 * original keyboard's indicator LEDs.
 *
 * Update Cycle:
 * `update_converter_status()` is called by protocol implementations whenever the keyboard or
 * mouse ready state changes. It compares the current `converter.value` against a cached copy
 * and calls `update_converter_leds()` only when state has changed or when a previous WS2812
 * transmission was deferred (FIFO full). This is event-driven, not per-iteration polling.
 */

#ifndef LED_HELPER_H
#define LED_HELPER_H

#include <stdbool.h>
#include <stdint.h>

#include "config.h"

/**
 * @brief Converter State Structure
 *
 * Tracks the operational state of the keyboard converter hardware.
 * Used to control status LED indicators.
 */
typedef struct converter_state {
    unsigned char kb_ready    : 1; /**< Keyboard initialised and ready */
    unsigned char mouse_ready : 1; /**< Mouse initialised and ready */
    unsigned char fw_flash    : 1; /**< Firmware flash mode (bootloader) */
    unsigned char cmd_mode    : 1; /**< Command mode active (for LED feedback) */
} converter_state;

/**
 * @brief Converter State Union
 *
 * Allows both bitfield access (for setting individual states) and
 * atomic value access (for efficient change detection).
 *
 * Threading Model:
 * - Written from protocol tasks (keyboard_interface_task, mouse_interface_task)
 * - Read from update_converter_status() in main task context
 * - Value is volatile to ensure visibility across different execution contexts
 * - Union allows efficient change detection via single byte comparison
 */
typedef union converter_state_union {
    converter_state        state;
    volatile unsigned char value; /**< Volatile for cross-context visibility */
} converter_state_union;

extern converter_state_union converter;

#ifdef CONVERTER_LEDS
/**
 * @brief Command Mode LED State
 *
 * Tracks which LED colour to display during command mode (alternates between
 * green and blue for command active, green and pink for log level selection).
 * This is separate from converter.state to avoid polluting the state byte
 * with rapidly changing LED toggle information.
 *
 * Threading Model:
 * - Only accessed from main task context (command mode processing)
 * - No synchronisation needed
 */
extern bool cmd_mode_led_green;

/**
 * @brief Log Level Selection Mode Flag
 *
 * When true, command mode uses GREEN/PINK alternating colours instead of
 * GREEN/BLUE. This provides visual distinction between command mode active
 * (waiting for command key) and log level selection mode (waiting for 1/2/3).
 *
 * Threading Model:
 * - Only accessed from main task context (command mode processing)
 * - No synchronisation needed
 */
extern bool log_level_selection_mode;
#endif

/**
 * @brief Updates converter status LEDs if state has changed or a deferred update is pending.
 *
 * Caches the previous converter state and calls update_converter_leds() only when the state
 * has changed or a previous WS2812 transmission was deferred. Prevents unnecessary PIO bus
 * traffic whilst ensuring all state transitions are eventually reflected in the LEDs.
 *
 * @note Main loop only.
 */
void update_converter_status(void);

/**
 * @brief Update keyboard ready LED status
 *
 * Inline helper function to update the keyboard ready LED indicator.
 * Consolidates the common pattern used across all protocol implementations
 * whilst allowing each protocol to maintain its specific ready condition.
 *
 * Benefits:
 * - Eliminates code duplication (4 protocols × multiple call sites = ~10 instances)
 * - Centralises LED update logic for keyboard state
 * - Maintains protocol-specific condition flexibility
 * - Zero overhead (inline, compiled away when CONVERTER_LEDS undefined)
 *
 * Usage Examples:
 * ```c
 * // AT/PS2, XT, Amiga protocols:
 * update_keyboard_ready_led(keyboard_state == INITIALISED);
 *
 * // Apple M0110 protocol (uses >= comparison):
 * update_keyboard_ready_led(keyboard_state >= INITIALISED);
 * ```
 *
 * @param ready true if keyboard is initialised and ready, false otherwise
 *
 * @note Inline function provides zero-overhead abstraction.
 * @note Compiled to nothing when CONVERTER_LEDS is not defined.
 * @note IRQ-safe and non-blocking.
 * @note Used from both main-loop protocol tasks and selected ISR event paths.
 */
static inline void update_keyboard_ready_led(bool ready) {
#ifdef CONVERTER_LEDS
    converter.state.kb_ready = (unsigned char)ready;
    update_converter_status();
#else
    // Suppress unused parameter warning when LEDs disabled
    (void)ready;
#endif
}

/**
 * @brief Lock Key State Structure
 *
 * Tracks the state of keyboard lock indicators (Num Lock, Caps Lock, Scroll Lock).
 * Synchronised with host operating system lock key states via USB HID reports.
 */
typedef struct lock_keys {
    unsigned char numLock    : 1; /**< Num Lock state */
    unsigned char capsLock   : 1; /**< Caps Lock state */
    unsigned char scrollLock : 1; /**< Scroll Lock state */
} lock_keys;

/**
 * @brief Lock Key State Union
 *
 * Allows both bitfield access (for individual lock states) and
 * byte access (for efficient operations and LED updates).
 *
 * Threading Model:
 * - Updated from HID callback (tud_hid_set_report_cb) in USB context
 * - Read from protocol tasks for keyboard LED updates
 * - Read from update_converter_leds() for visual indicators
 */
typedef union lock_keys_union {
    lock_keys     keys;
    unsigned char value;
} lock_keys_union;

extern volatile lock_keys_union lock_leds;

#ifdef CONVERTER_LEDS
/**
 * @brief Convert HSV colour to RGB colour
 *
 * Converts HSV (Hue, Saturation, Value) colour space to RGB colour space.
 * This is useful for generating rainbow effects and smooth colour transitions.
 *
 * HSV Colour Space:
 * - Hue: 0-359 degrees (0=red, 60=yellow, 120=green, 180=cyan, 240=blue, 300=magenta)
 * - Saturation: 0-255 (0=gray, 255=full colour)
 * - Value: 0-255 (0=black, 255=full brightness)
 *
 * RGB Colour Format:
 * - Returns 24-bit RGB colour: 0xRRGGBB
 * - Red: bits 23-16, Green: bits 15-8, Blue: bits 7-0
 *
 * @param hue Hue value (0-359 degrees, wraps around if >359)
 * @param saturation Saturation level (0-255)
 * @param value Brightness level (0-255)
 * @return 24-bit RGB colour value
 *
 * @note Uses integer math for efficiency (no floating point).
 * @note Pure function — thread-safe.
 * @note clang-tidy bugprone-easily-swappable-parameters suppressed: @p hue (0–359°)
 *       and @p saturation (0–255) occupy distinct domains; swapping would produce
 *       clearly wrong behaviour.
 */
uint32_t hsv_to_rgb(uint16_t hue, uint8_t saturation, uint8_t value);
#endif

/**
 * @brief Updates keyboard lock indicator state from a USB HID lock report byte.
 *
 * Updates the lock_leds union (Num Lock, Caps Lock, Scroll Lock) from the given HID
 * report byte and attempts an immediate LED update. If the update cannot complete due
 * to timing constraints, sets an internal pending flag so update_converter_status()
 * retries on the next main-loop iteration.
 *
 * @param lock_val HID lock byte (bit 0: Num Lock, bit 1: Caps Lock, bit 2: Scroll Lock).
 *
 * @note Non-blocking.
 * @note Called from USB interrupt context.
 */
void set_lock_values_from_hid(uint8_t lock_val);

/**
 * @brief Updates the converter status LEDs to reflect the current converter state.
 *
 * Updates WS2812 RGB LEDs to reflect the converter's operational state. The status
 * LED shows different colours for different states:
 * - Firmware flash mode: Magenta (bootloader active)
 * - Command mode: Green/Blue alternating (Green/Pink for log level selection)
 * - Ready (keyboard and mouse): Green
 * - Not ready: Orange
 *
 * Non-blocking: enforces a ≥60µs minimum interval between WS2812 transmissions.
 * Returns false and sets an internal pending flag if the interval has not elapsed;
 * the caller should retry on the next cycle.
 *
 * @return true if all LEDs were updated successfully, false if update was deferred.
 *
 * @note Non-blocking. Called from main loop and USB interrupt context.
 */
bool update_converter_leds(void);

#endif /* LED_HELPER_H */
