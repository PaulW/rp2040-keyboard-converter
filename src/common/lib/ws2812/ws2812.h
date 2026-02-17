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

#ifndef WS2812_H
#define WS2812_H

#include <stdbool.h>
#include "pico/stdlib.h"

/**
 * @brief Update WS2812 LED with the specified color (non-blocking)
 *
 * Queues a color update to a WS2812 LED via the PIO state machine. This is a
 * non-blocking operation that checks the PIO TX FIFO before writing. If the
 * FIFO is full, the function returns false immediately without blocking.
 *
 * Color Format:
 * - 24-bit RGB color value (0x00RRGGBB format)
 * - Automatically converted to WS2812 GRB format
 * - Brightness adjustment applied based on configuration
 *
 * Non-Blocking Behavior:
 * - Returns true if color data was successfully queued to PIO FIFO
 * - Returns false if PIO TX FIFO is full (caller should defer and retry)
 * - Returns false if called before ws2812_setup() (uninitialized)
 * - Never blocks waiting for FIFO space
 *
 * Multi-LED Cascading:
 * - Call once per LED in the chain
 * - First call updates first LED, second call updates second LED, etc.
 * - After all LEDs updated, a ≥50µs reset pulse latches colors simultaneously
 *
 * @param led_color RGB color value in 0x00RRGGBB format
 * @return true if update was queued successfully, false if deferred (retry later)
 *
 * @note Fully non-blocking - safe to call from any context
 * @note Caller must handle false return by deferring update
 * @note Call sites in led_helper.c implement proper deferral logic
 */
bool ws2812_show(uint32_t led_color);

/**
 * @brief Initialize WS2812 LED interface using PIO
 *
 * Sets up the PIO state machine for WS2812 LED control. Must be called
 * before ws2812_show() can be used.
 *
 * @param data_pin GPIO pin connected to WS2812 data line
 *
 * @note Call once during initialization
 * @note Configures PIO hardware for WS2812 timing requirements
 */
void ws2812_setup(uint data_pin);

/**
 * @brief Set LED brightness level
 *
 * Updates the runtime LED brightness level. The new brightness takes effect
 * immediately on the next LED update.
 *
 * @param level Brightness level (0-10, where 0=off and 10=max)
 *
 * @note Values >10 are clamped to 10
 * @note Takes effect on next ws2812_show() call
 * @note Non-blocking operation
 */
#ifdef CONVERTER_LEDS
void ws2812_set_brightness(uint8_t level);

/**
 * @brief Get current LED brightness level
 *
 * Returns the current runtime LED brightness setting.
 *
 * @return Current brightness level (0-10)
 *
 * @note Fast RAM read, no hardware access
 */
uint8_t ws2812_get_brightness(void);
#endif

#endif /* WS2812_H */
