/*
 * This file is part of RP2040 Keyboard Converter.
 *
 * Copyright 2023 Paul Bramhall (paulwamp@gmail.com)
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

#ifndef CONFIG_H
#define CONFIG_H

// Include some common type definitions for this config.h file
#include "types.h"

// Define configuration options for the Keyboard Converter
#define CONVERTER_PIEZO              // Enable Piezo Buzzer on Converter Hardware
#define CONVERTER_LEDS               // Enable support for LED indicator lights on Converter Hardware
#define CONVERTER_LEDS_TYPE LED_GRB  // Define type of LED which we are using
#define CONVERTER_LOCK_LEDS          // Enable Lock LED Indicators on Converter Hardware

// Define the colors of the LEDs in HEX.  Regardless of LED Type, we always use RGB Value here.
#define CONVERTER_LEDS_BRIGHTNESS 5                     // Brightness of LEDs.  This ranges from 1 to 10.
#define CONVERTER_LEDS_STATUS_READY_COLOR 0x00FF00      // Color of Status LED when Converter is initialised
#define CONVERTER_LEDS_STATUS_NOT_READY_COLOR 0xFF2800  // Color of Status LED when Converter is not ready
#define CONVERTER_LEDS_STATUS_FWFLASH_COLOR 0xFF00FF    // Color of Status LED when in Bootloader Mode (Firmware Flashing)
#define CONVERTER_LOCK_LEDS_COLOR 0x00FF00              // Color of Lock Light LEDs

// Define the GPIO Pins for the Keyboard Converter.
#define KEYBOARD_DATA_PIN 6  // This is the starting pin for the connected Keyboard.  Depending on the keyboard, we may use 2, 3 or more pins.
#define MOUSE_DATA_PIN 3     // This is the starting pin for the connected Mouse.  Depending on the mouse, we may use 2, 3 or more pins.
#define PIEZO_PIN 11         // Piezo Buzzer GPIO Pin.  Only required if CONVERTER_PIEZO is defined
#define LED_PIN 5            // LED GPIO Pin.  If using WS2812 LEDs, this is the GPIO Pin for the Data Line, otherwise we require 4 total GPIO for individual LED connections

// Define some Compile Time variables.  Do not modify below this line
#define BUILD_TIME _BUILD_TIME

// Define the Keyboard Information
#ifdef _KEYBOARD_ENABLED
#define KEYBOARD_ENABLED _KEYBOARD_ENABLED
#define KEYBOARD_MAKE _KEYBOARD_MAKE
#define KEYBOARD_MODEL _KEYBOARD_MODEL
#define KEYBOARD_DESCRIPTION _KEYBOARD_DESCRIPTION
#define KEYBOARD_PROTOCOL _KEYBOARD_PROTOCOL
#define KEYBOARD_CODESET _KEYBOARD_CODESET
#else
#define KEYBOARD_ENABLED 0
#endif

// Define the Mouse Information
#ifdef _MOUSE_ENABLED
#define MOUSE_ENABLED _MOUSE_ENABLED
#define MOUSE_PROTOCOL _MOUSE_PROTOCOL
#else
#define MOUSE_ENABLED 0
#endif

// Some Validation rules for the configuration
#ifdef CONVERTER_LEDS_BRIGHTNESS
#if CONVERTER_LEDS_BRIGHTNESS < 1 || CONVERTER_LEDS_BRIGHTNESS > 10
#error "CONVERTER_LEDS_BRIGHTNESS must be between 1 and 10"
#endif
#endif

#endif /* CONFIG_H */
