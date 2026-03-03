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
 * @file led_helper.c
 * @brief LED state management implementation.
 *
 * @see led_helper.h for the public API documentation.
 */

#include "led_helper.h"

#include <stdbool.h>
#include <stdint.h>

#include "hardware/sync.h"  // for __dmb() memory barrier
#include "pico/time.h"

#ifdef CONVERTER_LEDS
#include "ws2812/ws2812.h"
#endif

// Initialise the converter state with both keyboard and mouse states set to ready.
// Lower two bits (0x03): kb_ready=1, mouse_ready=1.  Upper nibble (0xC0) sets the
// spare bits to a defined, non-zero pattern — useful as a sentinel during debugging.
#define CONVERTER_STATE_INITIAL 0xC3U

converter_state_union converter = {.value = CONVERTER_STATE_INITIAL};

volatile lock_keys_union lock_leds;

#ifdef CONVERTER_LEDS
// Track last LED update time (us since boot) for non-blocking WS2812 reset timing
// Using uint32_t for atomic access (64-bit absolute_time_t is not atomic on RP2040)
static volatile uint32_t last_led_update_time_us = 0;
// Track if an update is pending due to timing constraints
static volatile bool led_update_pending = false;

// Track which LED colour to display during command mode (green or blue/pink)
bool cmd_mode_led_green = true;

// Track if we're in log level selection mode (changes LED colours to GREEN/PINK)
bool log_level_selection_mode = false;

// Command mode LED colours
#define CMD_MODE_LED_GREEN 0x00FF00 /**< Green LED for command mode phase 1 */
#define CMD_MODE_LED_BLUE  0x0000FF /**< Blue LED for command mode phase 2 */
#define CMD_MODE_LED_PINK  0xFF1493 /**< Pink (Deep Pink) LED for log level selection */
#endif

// --- Public Functions ---

bool update_converter_leds(void) {
#ifdef CONVERTER_LEDS
    // WS2812 requires ≥50µs reset time AFTER transmission completes
    // Calculate minimum interval: transmission time + reset time
    // Each LED takes ~30µs to transmit (24 bits × 1.25µs per bit)
    uint32_t now_us = to_us_since_boot(get_absolute_time());
#ifdef CONVERTER_LOCK_LEDS
    const uint32_t led_count = 4U;  // status + Num/Caps/Scroll
#else
    const uint32_t led_count = 1U;  // status only
#endif
    const uint32_t min_interval_us =
        WS2812_RESET_PULSE_US + (led_count * WS2812_LED_TRANSMISSION_US);
    uint32_t elapsed_us = now_us - last_led_update_time_us;

    if (elapsed_us < min_interval_us) {
        // Not enough time has passed, mark update as pending
        led_update_pending = true;
        __dmb();  // Ensure IRQ-visible write is ordered
        return false;
    }

    // Determine status LED colour based on converter state
    uint32_t status_colour = CONVERTER_LEDS_STATUS_NOT_READY_COLOUR;
    if (converter.state.fw_flash) {
        status_colour = CONVERTER_LEDS_STATUS_FWFLASH_COLOUR;
    } else if (converter.state.cmd_mode) {
        // Command mode active - colour depends on mode:
        // - Log level selection: GREEN/PINK alternating
        // - Command active: GREEN/BLUE alternating
        if (log_level_selection_mode) {
            status_colour = cmd_mode_led_green ? CMD_MODE_LED_GREEN : CMD_MODE_LED_PINK;
        } else {
            status_colour = cmd_mode_led_green ? CMD_MODE_LED_GREEN : CMD_MODE_LED_BLUE;
        }
    } else if (converter.state.kb_ready && converter.state.mouse_ready) {
        status_colour = CONVERTER_LEDS_STATUS_READY_COLOUR;
    } else {
        status_colour = CONVERTER_LEDS_STATUS_NOT_READY_COLOUR;
    }

    // Update all LEDs in sequence - if any fail, defer entire update
    bool success = ws2812_show(status_colour);

#ifdef CONVERTER_LOCK_LEDS
    // Chain success checks - short-circuit on first failure
    success = success && ws2812_show(lock_leds.keys.numLock ? CONVERTER_LOCK_LEDS_COLOUR : 0);
    success = success && ws2812_show(lock_leds.keys.capsLock ? CONVERTER_LOCK_LEDS_COLOUR : 0);
    success = success && ws2812_show(lock_leds.keys.scrollLock ? CONVERTER_LOCK_LEDS_COLOUR : 0);
#endif

    if (success) {
        // All LEDs successfully queued - record time and clear pending flag
        last_led_update_time_us = now_us;
        led_update_pending      = false;
        __dmb();  // Memory barrier - ensure IRQ-visible writes complete
    } else {
        // One or more LEDs failed to queue - mark as pending for retry
        led_update_pending = true;
        __dmb();  // Memory barrier - ensure IRQ-visible writes complete
    }

    return success;
#else
    return true;
#endif
}

void update_converter_status(void) {
#ifdef CONVERTER_LEDS
    __dmb();  // Ensure latest IRQ updates are visible before reads
    static uint8_t status;
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
    // No LEDs configured - this function is a no-op
#endif
}

#ifdef CONVERTER_LEDS
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
uint32_t hsv_to_rgb(uint16_t hue, uint8_t saturation, uint8_t value) {
    // Normalise hue to 0-359
    hue = hue % 360;

    // Calculate RGB components using efficient integer math
    uint8_t region    = hue / 60;
    uint8_t remainder = (hue - (region * 60)) * 255 / 60;

    uint8_t min_channel = (value * (255 - saturation)) / 255;
    uint8_t fade_down   = (value * (255 - ((saturation * remainder) / 255))) / 255;
    uint8_t fade_up     = (value * (255 - ((saturation * (255 - remainder)) / 255))) / 255;

    uint8_t red_component = 0, green_component = 0, blue_component = 0;
    switch (region) {
        case 0:
            red_component   = value;
            green_component = fade_up;
            blue_component  = min_channel;
            break;
        case 1:
            red_component   = fade_down;
            green_component = value;
            blue_component  = min_channel;
            break;
        case 2:
            red_component   = min_channel;
            green_component = value;
            blue_component  = fade_up;
            break;
        case 3:
            red_component   = min_channel;
            green_component = fade_down;
            blue_component  = value;
            break;
        case 4:
            red_component   = fade_up;
            green_component = min_channel;
            blue_component  = value;
            break;
        default:
            red_component   = value;
            green_component = min_channel;
            blue_component  = fade_down;
            break;
    }

    return (red_component << 16) | (green_component << 8) | blue_component;
}
#endif

void set_lock_values_from_hid(uint8_t lock_val) {
    lock_leds.keys.numLock    = (unsigned char)(lock_val & 0x01);
    lock_leds.keys.capsLock   = (unsigned char)((lock_val >> 1) & 0x01);
    lock_leds.keys.scrollLock = (unsigned char)((lock_val >> 2) & 0x01);

#ifdef CONVERTER_LEDS
    // Update lock LEDs immediately if possible, otherwise defer to main loop
    // This function is called from USB ISR context, so no blocking operations allowed
    if (!update_converter_leds()) {
        // Update failed (timing constraint), set flag for main loop retry
        led_update_pending = true;
        __dmb();  // Ensure IRQ-visible write is ordered
    }
#endif
}
