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
 * @file ws2812.h
 * @brief PIO-driven WS2812 addressable RGB LED driver.
 *
 * Drives WS2812 and WS2812B single-wire addressable LEDs using an RP2040 PIO state
 * machine to generate the exact 800 kHz NRZ (non-return-to-zero) signal without CPU
 * involvement in bit timing.
 *
 * Protocol Summary:
 * - 800 kHz single-wire; 24 bits per LED encoded in GRB order (per WS2812B specification)
 * - Pulse widths differentiate 0-bits from 1-bits (NRZ encoding)
 * - ≥50µs low reset pulse latches colour data into all LEDs simultaneously
 * - PIO program: T1=2, T2=5, T3=3 cycles (see ws2812.pio for exact timing)
 *
 * This driver accepts colours in 0xRRGGBB format and reorders to GRB internally.
 * Brightness scaling (0–WS2812_BRIGHTNESS_MAX) is applied before transmission.
 *
 * Multi-LED Chains:
 * For chains of N LEDs, call `ws2812_show()` N times in sequence (once per LED in order),
 * then wait at least WS2812_RESET_PULSE_US before the next update cycle to latch the data.
 *
 * Non-Blocking Design:
 * `ws2812_show()` writes to the PIO TX FIFO and returns immediately. If the FIFO is full
 * it returns false; callers in `led_helper.c` implement deferral logic so the LED update
 * is retried next iteration without blocking the main loop.
 *
 * @see types.h for the `led_types_t` colour-order enumeration used by `CONVERTER_LEDS_TYPE`.
 */

#ifndef WS2812_H
#define WS2812_H

#include <stdbool.h>

#include "pico/stdlib.h"

#include "types.h"
#include "config.h"

// WS2812 Timing Constants (microseconds)
#define WS2812_RESET_PULSE_US      60U  // Minimum reset pulse duration (≥50µs)
#define WS2812_LED_TRANSMISSION_US 30U  // Per-LED transmission time (24 bits × 1.25µs)

#ifdef CONVERTER_LEDS
// WS2812 Brightness Configuration
#define WS2812_BRIGHTNESS_MIN 0   // Minimum brightness (off)
#define WS2812_BRIGHTNESS_MAX 10  // Maximum brightness
#endif

/**
 * @brief Update WS2812 LED with the specified colour (non-blocking)
 *
 * Queues a colour update to a WS2812 LED via the PIO state machine. This is a
 * non-blocking operation that checks the PIO TX FIFO before writing. If the
 * FIFO is full, the function returns false immediately without blocking.
 *
 * Colour Format:
 * - 24-bit RGB colour value (0x00RRGGBB format)
 * - Automatically converted to WS2812 GRB format
 * - Brightness adjustment applied based on configuration
 *
 * Non-Blocking Behaviour:
 * - Returns true if colour data was successfully queued to PIO FIFO
 * - Returns false if PIO TX FIFO is full (caller should defer and retry)
 * - Returns false if called before ws2812_setup() (uninitialised)
 * - Never blocks waiting for FIFO space
 *
 * Multi-LED Cascading:
 * - Call once per LED in the chain
 * - First call updates first LED, second call updates second LED, etc.
 * - After all LEDs updated, a ≥50µs reset pulse latches colours simultaneously
 *
 * @param led_colour RGB colour value in 0x00RRGGBB format
 * @return true if update was queued successfully, false if deferred (retry later)
 *
 * @note Main loop only — not IRQ-safe.
 * @note Caller must handle false return by deferring update
 * @note Call sites in led_helper.c implement proper deferral logic
 */
bool ws2812_show(uint32_t led_colour);

/**
 * @brief Initialises the WS2812 PIO driver for the specified GPIO pin.
 *
 * Claims a PIO instance and state machine, loads the WS2812 program, and
 * configures the clock divider for 800 kHz output. Logs the PIO/SM assignment
 * and effective clock speed. Must be called before any ws2812_show() calls.
 *
 * @param led_pin The GPIO pin connected to the WS2812 LED data line.
 *
 * @note Main loop only — call before enabling IRQs.
 * @note Non-reentrant — call once during initialisation.
 */
void ws2812_setup(uint led_pin);

#ifdef CONVERTER_LEDS
/**
 * @brief Sets the runtime LED brightness level.
 *
 * Updates the brightness used by ws2812_show(). Values outside 0–10 are
 * clamped to the valid range. Gamma correction is applied via the internal
 * lookup table, giving perceptually uniform steps.
 *
 * @param level Brightness level (0=off, 10=maximum; clamped if out of range).
 *
 * @note Main loop only.
 * @note Takes effect on the next ws2812_show() call — does not trigger an update.
 */
void ws2812_set_brightness(uint8_t level);

/**
 * @brief Returns the current runtime LED brightness level.
 *
 * @return Current brightness level (0–10).
 *
 * @note Main loop only.
 */
uint8_t ws2812_get_brightness(void);
#endif

#endif /* WS2812_H */
