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
 * @file keyboard_interface.h
 * @brief IBM XT Keyboard Protocol Implementation
 *
 * This file implements the IBM XT keyboard protocol used by the original IBM PC and
 * PC/XT systems.
 *
 * Protocol Characteristics:
 * - 2-wire interface: separate DATA and CLOCK lines
 * - Keyboard generates clock signal on dedicated CLOCK line
 * - Unidirectional communication (keyboard to host only)
 * - LSB-first bit transmission (bit 0 → bit 7)
 * - Start bit present; no parity or stop bits
 * - Fixed scan code set (equivalent to later "Set 1")
 * - No host commands or acknowledgments
 *
 * Physical Interface:
 * - DATA line for keyboard-to-host data transmission
 * - CLOCK line for keyboard-generated clock signal
 * - 5V TTL logic levels with pull-up resistor
 * - 5-pin DIN connector on original IBM keyboards
 *
 * Note: Unlike AT/PS2 protocol, the CLOCK line in XT is primarily keyboard-driven
 * with no continuous host flow control.
 *
 * Timing Requirements:
 * - Keyboard-generated clock: approximately 10 kHz
 * - Clock pulse width: ~40µs low, ~60µs high
 * - Total bit time: ~100µs
 * - No host timing control or flow control
 *
 * Data Format:
 * - 8 bits per scan code, LSB first
 * - Make codes: 0x01-0x53 (key press)
 * - Break codes: Make code + 0x80 (key release)
 * - No multi-byte sequences or special commands
 *
 * Initialisation:
 * - No initialisation sequence required
 * - Keyboard begins transmitting immediately when powered
 * - Host simply receives and processes scan codes
 *
 * Advantages:
 * - Simple, reliable protocol with minimal overhead
 */

#ifndef KEYBOARD_INTERFACE_H
#define KEYBOARD_INTERFACE_H

#include "pico/stdlib.h"

/**
 * @brief IBM XT Protocol Response Codes
 *
 * XT keyboards only send scan codes and self-test results to the host.
 * There are no command codes since XT protocol is unidirectional.
 */
#define XT_RESP_BAT_PASSED 0xAA /**< Basic Assurance Test (self-test) passed */
#define XT_RESP_BAT_FAILED 0xFC /**< Basic Assurance Test (self-test) failed (rare) */

/**
 * @brief IBM XT Scan Code Ranges
 *
 * XT protocol uses a fixed scan code set with these ranges.
 * All scan codes are single-byte values transmitted LSB-first.
 */
#define XT_SCANCODE_MAKE_MIN     0x01 /**< Minimum make code (ESC key) */
#define XT_SCANCODE_MAKE_MAX     0x53 /**< Maximum make code */
#define XT_SCANCODE_BREAK_OFFSET 0x80 /**< Offset added to make code for break code */

/**
 * @brief Common IBM XT Scan Code Examples
 *
 * These are some commonly referenced scan codes for debugging
 * and validation purposes.
 */
#define XT_SCANCODE_ESC         0x01 /**< Escape key make code */
#define XT_SCANCODE_SPACE       0x39 /**< Space bar make code */
#define XT_SCANCODE_ENTER       0x1C /**< Enter key make code */
#define XT_SCANCODE_LEFT_SHIFT  0x2A /**< Left Shift key make code */
#define XT_SCANCODE_RIGHT_SHIFT 0x36 /**< Right Shift key make code */

/**
 * @brief Protocol Timing Constants
 *
 * These timing values are used for PIO clock divider calculation and protocol operation.
 */
#define XT_TIMING_CLOCK_MIN_US  30  /**< Minimum pulse width for reliable detection (30µs) */
#define XT_TIMING_BIT_PERIOD_US 100 /**< Typical bit period (~100µs at ~10 kHz) */
#define XT_TIMING_SAMPLE_US     10 /**< PIO sampling interval for double-start-bit detection (10µs) */

/**
 * @brief IBM XT Keyboard Interface Initialisation
 *
 * Configures and initialises the IBM XT keyboard interface using RP2040 PIO hardware
 * for accurate timing and protocol implementation. Sets up the minimal hardware
 * required for the simple XT unidirectional protocol:
 *
 * Hardware Setup:
 * - Atomically claims PIO resources using claim_pio_and_sm(&keyboard_interface_program)
 * - Uses returned pio_engine_t for program init and runtime access
 * - Configures GPIO pins for DATA and CLOCK lines (2-wire interface)
 * - Keyboard generates clock signal on dedicated CLOCK line
 *
 * PIO Configuration:
 * - Calculates clock divider for 10µs sampling (XT_TIMING_SAMPLE_US)
 * - Configures state machine for unidirectional receive-only operation
 * - Sets up automatic scan code reception and frame processing
 * - Optimised for ~10 kHz keyboard transmission rate
 *
 * Interrupt System:
 * - Configures PIO-specific IRQ (PIO0_IRQ_0 or PIO1_IRQ_0)
 * - Installs XT keyboard input event handler for scan code processing
 * - Enables interrupt-driven reception for optimal performance
 * - Links PIO RX FIFO to interrupt processing system
 *
 * Software Initialisation:
 * - Resets ring buffer to ensure clean startup state
 * - Initialises protocol state machine to UNINITIALISED
 * - Resets converter status LEDs when CONVERTER_LEDS enabled
 * - Clears all protocol state variables
 *
 * Timing Configuration:
 * - Based on IBM PC/XT Technical Reference specifications
 * - 30µs minimum timing for reliable signal detection
 * - Accommodates keyboard clock variations (~40µs low, ~60µs high)
 * - Supports original IBM and compatible clone keyboards
 *
 * Protocol Simplicity:
 * - No bidirectional communication setup needed
 * - No command/response initialisation required
 * - No LED control or advanced feature configuration
 * - Immediate operation once keyboard powers on
 *
 * Compatibility Features:
 * - Works with original IBM PC/XT keyboards (1981-1987)
 * - Compatible with modern XT-protocol keyboards
 * - Handles timing variations in clone keyboards
 * - No keyboard-specific configuration needed
 *
 * Error Handling:
 * - Validates PIO resource availability before configuration
 * - Reports initialisation failures with detailed error messages
 * - Graceful failure if insufficient PIO resources available
 * - Logs successful initialisation with configuration details
 *
 * @param data_pin GPIO pin number for DATA line; CLOCK is expected on (data_pin + 1)
 *
 * @note Main loop only.
 */
void keyboard_interface_setup(uint data_pin);

/**
 * @brief IBM XT Keyboard Interface Main Task
 *
 * Main task function that manages IBM XT keyboard communication and data processing.
 * Designed for the simple XT protocol which requires minimal state management:
 *
 * Normal Operation (INITIALISED state):
 * - Processes scan codes from ring buffer when HID interface ready
 * - Coordinates scan code translation for IBM XT Set 1 format
 * - Maintains optimal data flow between keyboard and USB HID
 * - Prevents HID report queue overflow through ready checking
 *
 * Initialisation Management (UNINITIALISED state):
 * - Monitors GPIO clock line for keyboard presence detection
 * - Implements simple retry logic for keyboard detection
 * - Handles keyboard connection/disconnection events
 * - Manages PIO restart for clean protocol recovery
 *
 * Protocol Simplicity:
 * - No complex initialisation sequence (unlike AT/PS2)
 * - No LED control capability (hardware limitation)
 * - No bidirectional communication or command/response
 * - Immediate operation once keyboard detected and self-test passed
 *
 * Detection and Recovery:
 * - 200ms periodic checks for keyboard presence via clock line
 * - 5-attempt detection cycle before PIO restart
 * - Automatic recovery from communication failures
 * - Simple state machine reflects protocol simplicity
 *
 * Data Processing Features:
 * - Non-blocking ring buffer processing for real-time performance
 * - HID-ready checking prevents USB report queue overflow
 * - Efficient scan code processing without protocol overhead
 * - Direct translation to modern HID key codes
 *
 * Hardware Monitoring:
 * - GPIO clock line monitoring for presence detection
 * - Converter status LED updates when CONVERTER_LEDS enabled
 * - Simple hardware interface monitoring (single DATA line)
 * - No complex timing or signal management required
 *
 * Performance Characteristics:
 * - Minimal CPU overhead due to protocol simplicity
 * - Fast response time (no initialisation delays)
 * - Efficient ring buffer usage without protocol buffering
 * - Real-time scan code processing capability
 *
 * Compatibility Notes:
 * - Works with original IBM PC/XT keyboards (1981-1987)
 * - Compatible with modern XT-protocol keyboards
 * - No special handling for keyboard variants needed
 * - Plug-and-play operation (no configuration required)
 *
 * @note Main loop only.
 */
void keyboard_interface_task(void);

#endif /* KEYBOARD_INTERFACE_H */
