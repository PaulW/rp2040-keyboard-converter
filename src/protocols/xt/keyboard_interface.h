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
 * This file implements the IBM XT keyboard protocol used by the original IBM PC/XT
 * systems (1981-1987). This is the simplest of the IBM keyboard protocols.
 * 
 * Protocol Characteristics:
 * - 2-wire interface: separate DATA and CLOCK lines
 * - Keyboard generates clock signal on dedicated CLOCK line
 * - Unidirectional communication (keyboard to host only)
 * - LSB-first bit transmission (bit 0 → bit 7)
 * - No parity, start, or stop bits
 * - Fixed scan code set (Set 1 equivalent)
 * - No host commands or acknowledgments
 * 
 * Physical Interface:
 * - DATA line for keyboard-to-host data transmission
 * - CLOCK line for keyboard-generated clock signal
 * - Both lines have pull-up resistors
 * - 5V TTL logic levels
 * - 5-pin DIN connector on original IBM keyboards
 * 
 * Note: Unlike AT/PS2 protocol, the CLOCK line in XT is primarily keyboard-driven
 * with no continuous host flow control. However, XT Type 2 keyboards support soft
 * reset by pulling both CLOCK and DATA low simultaneously.
 * 
 * Timing Requirements:
 * - Keyboard-generated clock: approximately 10 kHz
 * - Clock pulse width: ~40µs low, 60µs high
 * - Total bit time: ~100µs
 * - No host timing control or flow control
 * 
 * Data Format:
 * - 8 bits per scan code, LSB first
 * - Make codes: 0x01-0x53 (key press)
 * - Break codes: Make code + 0x80 (key release)
 * - No multi-byte sequences or special commands
 * 
 * Initialization:
 * - No initialization sequence required
 * - Keyboard begins transmitting immediately when powered
 * - Host simply receives and processes scan codes
 * 
 * Compatibility:
 * - Compatible with IBM PC (1981), PC/XT (1983)
 * - Subset of AT keyboard functionality
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
#define XT_RESP_BAT_PASSED          0xAA  /**< Basic Assurance Test (self-test) passed */
#define XT_RESP_BAT_FAILED          0xFC  /**< Basic Assurance Test (self-test) failed (rare) */

/**
 * @brief IBM XT Scan Code Ranges
 * 
 * XT protocol uses a fixed scan code set with these ranges.
 * All scan codes are single-byte values transmitted LSB-first.
 */
#define XT_SCANCODE_MAKE_MIN        0x01  /**< Minimum make code (ESC key) */
#define XT_SCANCODE_MAKE_MAX        0x53  /**< Maximum make code */
#define XT_SCANCODE_BREAK_OFFSET    0x80  /**< Offset added to make code for break code */

/**
 * @brief Common IBM XT Scan Code Examples
 * 
 * These are some commonly referenced scan codes for debugging
 * and validation purposes.
 */
#define XT_SCANCODE_ESC             0x01  /**< Escape key make code */
#define XT_SCANCODE_SPACE           0x39  /**< Space bar make code */
#define XT_SCANCODE_ENTER           0x1C  /**< Enter key make code */
#define XT_SCANCODE_LEFT_SHIFT      0x2A  /**< Left Shift key make code */
#define XT_SCANCODE_RIGHT_SHIFT     0x36  /**< Right Shift key make code */

/**
 * @brief Protocol Timing Constants
 * 
 * These timing values are used for PIO clock divider calculation and protocol operation.
 */
#define XT_TIMING_CLOCK_MIN_US      30    /**< Minimum pulse width for reliable detection (30µs) */
#define XT_TIMING_BIT_PERIOD_US     100   /**< Typical bit period (~100µs at ~10 kHz) */
#define XT_TIMING_SAMPLE_US         10    /**< PIO sampling interval for double-start-bit detection (10µs) */

/**
 * @brief Initializes the IBM XT keyboard interface
 * 
 * Sets up PIO state machine and GPIO configuration for XT keyboard
 * communication. XT keyboards require minimal setup since they use
 * a simple unidirectional protocol.
 * 
 * @param data_pin GPIO pin for DATA line (2-wire interface)
 */
void keyboard_interface_setup(uint data_pin);

/**
 * @brief Main task function for IBM XT keyboard interface
 * 
 * Processes received scan codes and manages the simple XT protocol
 * state machine. Should be called periodically from main loop.
 */
void keyboard_interface_task();

#endif /* KEYBOARD_INTERFACE_H */
