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
 * @file types.h
 * @brief Common type definitions shared across the firmware.
 *
 * Defines shared enumerations (e.g. LED colour-order types) used by multiple
 * modules throughout the codebase.
 */

#ifndef TYPES_H
#define TYPES_H

/**
 * @brief LED colour channel ordering for WS2812-compatible RGB LEDs.
 *
 * Selects the byte order in which red, green, and blue channels are transmitted
 * to the LED strip. The correct value depends on the physical LED variant.
 */
typedef enum {
    LED_RGB = 0x00, /**< Red-Green-Blue channel order */
    LED_RBG = 0x01, /**< Red-Blue-Green channel order */
    LED_GRB = 0x02, /**< Green-Red-Blue channel order */
    LED_GBR = 0x03, /**< Green-Blue-Red channel order */
    LED_BRG = 0x04, /**< Blue-Red-Green channel order */
    LED_BGR = 0x05, /**< Blue-Green-Red channel order */
} led_types_t;

#endif /* TYPES_H */
