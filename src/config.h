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

// Turn off clang-format for this file as we want to keep the formatting as is
// clang-format off

// --- UART Hardware Configuration ---
#define UART_TX_PIN 0       // GPIO pin for UART transmission (typically GP0)
#define UART_BAUD   115200  // UART transmission baud rate

// --- UART DMA Buffer Configuration ---
// Controls memory allocation for the DMA-based UART logging system.
//
// UART_DMA_BUFFER_SIZE: Maximum length of a single log message (bytes)
//   - Typical: 256 bytes (sufficient for most log messages)
//   - Memory impact: BUFFER_SIZE × QUEUE_SIZE total RAM usage
//   - Messages longer than this will be truncated
//
// UART_DMA_QUEUE_SIZE: Number of messages that can be queued
//   - Must be a power of 2 (enforced by compile-time check)
//   - Typical: 64 entries (handles initialization bursts)
//   - Memory usage: 256 × 64 = 16KB (current configuration)
//   - Larger queue = more burst tolerance but more RAM usage
//
#define UART_DMA_BUFFER_SIZE 256  // Size of each message buffer in bytes (max message length)
#define UART_DMA_QUEUE_SIZE  64   // Number of queued messages (must be power of 2)

// --- UART DMA Debug Statistics ---
// Uncomment to enable drop statistics tracking and reporting.
// When enabled, the system tracks and reports when messages are dropped due to queue full.
// Statistics are reported automatically when drops occur (event-triggered, non-interactive).
//
// Memory overhead: ~12 bytes (counters)
// CPU overhead: One atomic increment per drop + occasional printf
//
// #define UART_DMA_DEBUG_STATS

// --- UART DMA Queue Policy Configuration ---
// Controls behavior when the message queue becomes full during logging bursts.
// The policy is selected at compile time for zero runtime overhead.

#define UART_DMA_WAIT_US 5000  // Maximum wait time in microseconds (applies to WAIT policies)

// UART DMA Policy Constants - do not modify these values
#define UART_DMA_POLICY_DROP        0  // Immediately drop messages if queue is full
#define UART_DMA_POLICY_WAIT_FIXED  1  // Wait up to UART_DMA_WAIT_US with tight polling
#define UART_DMA_POLICY_WAIT_EXP    2  // Exponential backoff delays (CPU-friendly)

// Define the UART DMA Policy - choose ONE of the following:
//
// UART_DMA_POLICY_DROP:       Immediately drop messages if queue is full
//                              * Real-time safe (never blocks)
//                              * Recommended for keyboard converter application
//                              * May lose messages under extreme load
//
// UART_DMA_POLICY_WAIT_FIXED: Wait up to UART_DMA_WAIT_US with tight polling
//                              * Better message preservation
//                              * CPU intensive during waits
//                              * May cause timing issues in real-time code
//
// UART_DMA_POLICY_WAIT_EXP:    Exponential backoff delays (1μs, 2μs, 4μs, ...)
//                              * CPU-friendly waiting with progressive delays
//                              * Good balance of message preservation and performance
//                              * Delays capped at 1024μs per step
//
#define UART_DMA_POLICY UART_DMA_POLICY_DROP

// --- Log Level Configuration ---
// Controls runtime filtering of log messages for debugging and diagnostics.
//
// Log Level Constants:
//   LOG_LEVEL_ERROR  (0) - Critical errors and warnings only [ERR] [WARN]
//   LOG_LEVEL_INFO   (1) - Errors + informational messages [ERR] [WARN] [INFO]
//   LOG_LEVEL_DEBUG  (2) - All messages [ERR] [WARN] [INFO] [DBG]
//
// LOG_LEVEL_DEFAULT: Runtime default log level at startup
//   - Can be changed at runtime with log_set_level()
//   - Can be changed via command mode: Hold Shift+Shift → Press 'D' → Press 1/2/3
//
// Performance Impact:
//   - Runtime disabled: ~5-10 cycles (single comparison + branch)
//   - Enabled: same as printf() (~20µs total with DMA)
//
// Recommended Settings:
//   - Development: LOG_LEVEL_DEFAULT=LOG_LEVEL_DEBUG  (show all messages)
//   - Production:  LOG_LEVEL_DEFAULT=LOG_LEVEL_INFO   (show errors + info)
//
#define LOG_LEVEL_ERROR  0
#define LOG_LEVEL_INFO   1
#define LOG_LEVEL_DEBUG  2

#define LOG_LEVEL_DEFAULT LOG_LEVEL_INFO   // Default to INFO level at startup

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
#define KEYBOARD_DATA_PIN 2  // This is the starting pin for the connected Keyboard.  Depending on the keyboard, we may use 2, 3 or more pins.
#define MOUSE_DATA_PIN 6     // This is the starting pin for the connected Mouse.  Depending on the mouse, we may use 2, 3 or more pins.
#define PIEZO_PIN 11         // Piezo Buzzer GPIO Pin.  Only required if CONVERTER_PIEZO is defined
#define LED_PIN 29           // LED GPIO Pin.  If using WS2812 LEDs, this is the GPIO Pin for the Data Line, otherwise we require 4 total GPIO for individual LED connections

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

// Quick Validation to ensure that we have at least one of Keyboard or Mouse enabled
#if MOUSE_ENABLED == 0 && KEYBOARD_ENABLED == 0
#error "You must build with either a Keyboard or Mouse or both enabled"
#endif

// Some Validation rules for the configuration
#ifdef CONVERTER_LEDS_BRIGHTNESS
#if CONVERTER_LEDS_BRIGHTNESS < 1 || CONVERTER_LEDS_BRIGHTNESS > 10
#error "CONVERTER_LEDS_BRIGHTNESS must be between 1 and 10"
#endif
#endif

// clang-format on

#endif /* CONFIG_H */
