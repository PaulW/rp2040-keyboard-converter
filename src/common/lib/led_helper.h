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
    unsigned char kb_ready    : 1; /**< Keyboard initialized and ready */
    unsigned char mouse_ready : 1; /**< Mouse initialized and ready */
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
 * Tracks which LED color to display during command mode (alternates between
 * green and blue for command active, green and pink for log level selection).
 * This is separate from converter.state to avoid polluting the state byte
 * with rapidly changing LED toggle information.
 *
 * Threading Model:
 * - Only accessed from main task context (command mode processing)
 * - No synchronization needed
 */
extern volatile bool cmd_mode_led_green;

/**
 * @brief Log Level Selection Mode Flag
 *
 * When true, command mode uses GREEN/PINK alternating colors instead of
 * GREEN/BLUE. This provides visual distinction between command mode active
 * (waiting for command key) and log level selection mode (waiting for 1/2/3).
 *
 * Threading Model:
 * - Only accessed from main task context (command mode processing)
 * - No synchronization needed
 */
extern volatile bool log_level_selection_mode;
#endif

// Forward declaration for inline function below
void update_converter_status(void);

/**
 * @brief Update keyboard ready LED status
 *
 * Inline helper function to update the keyboard ready LED indicator.
 * Consolidates the common pattern used across all protocol implementations
 * while allowing each protocol to maintain its specific ready condition.
 *
 * Benefits:
 * - Eliminates code duplication (4 protocols × multiple call sites = ~10 instances)
 * - Centralizes LED update logic for keyboard state
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
 * @param ready true if keyboard is initialized and ready, false otherwise
 *
 * @note Inline function provides zero-overhead abstraction
 * @note Compiled to nothing when CONVERTER_LEDS is not defined
 * @note ISR-safe: update_converter_status() only sets flags, no blocking I/O
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
 * Synchronized with host operating system lock key states via USB HID reports.
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

extern lock_keys_union lock_leds;

#ifdef CONVERTER_LEDS
/**
 * @brief Convert HSV color to RGB color
 *
 * Converts HSV (Hue, Saturation, Value) color space to RGB color space.
 * This is useful for generating rainbow effects and smooth color transitions.
 *
 * HSV Color Space:
 * - Hue: 0-359 degrees (0=red, 60=yellow, 120=green, 180=cyan, 240=blue, 300=magenta)
 * - Saturation: 0-255 (0=gray, 255=full color)
 * - Value: 0-255 (0=black, 255=full brightness)
 *
 * RGB Color Format:
 * - Returns 24-bit RGB color: 0xRRGGBB
 * - Red: bits 23-16, Green: bits 15-8, Blue: bits 7-0
 *
 * @param hue Hue value (0-359 degrees, wraps around if >359)
 * @param saturation Saturation level (0-255)
 * @param value Brightness level (0-255)
 * @return 24-bit RGB color value
 *
 * @note Uses integer math for efficiency (no floating point)
 * @note Thread-safe: pure function with no state
 */
uint32_t hsv_to_rgb(uint16_t hue, uint8_t saturation, uint8_t value);
#endif

void set_lock_values_from_hid(uint8_t lock_val);
bool update_converter_leds(void);

#endif /* LED_HELPER_H */
