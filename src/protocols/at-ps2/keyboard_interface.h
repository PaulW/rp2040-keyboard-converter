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
 * @brief AT/PS2 Keyboard Protocol Implementation
 *
 * This file implements the AT and PS/2 keyboard protocols used by IBM PC compatible systems.
 * The protocol is a synchronous, bidirectional communication system where the host can send
 * commands and the keyboard responds with data and acknowledgments.
 *
 * Protocol Characteristics:
 * - 2-wire interface: DATA and CLOCK (both open-drain with pull-ups)
 * - Keyboard normally controls clock timing
 * - Host can inhibit communication by pulling CLOCK low
 * - LSB-first bit transmission (bit 0 → bit 7)
 * - Start bit (0), 8 data bits, odd parity bit, stop bit (1)
 * - Bidirectional communication with acknowledgments
 *
 * Supported Scan Code Sets:
 * - Sets 1, 2, and 3 supported
 * - Terminal keyboards (identified by device ID response) use Set 3
 * - All other keyboards use Set 2 by default
 *
 * Timing Requirements:
 * - Clock frequency: 10-16.7 kHz
 * - Clock pulse width: minimum 30µs
 * - Data setup/hold: minimum 5µs before/after clock edge
 * - Inhibit time: 96µs CLOCK low to stop transmission
 *
 * Initialisation Sequence:
 * 1. Power-on self-test (keyboard sends 0xAA on success)
 * 2. Host reads keyboard ID (2-byte response)
 * 3. Host configures scan code set and options
 * 4. Host sets LED states
 * 5. Normal operation begins
 *
 * Special Features:
 * - LED control (Caps Lock, Num Lock, Scroll Lock)
 * - Typematic repeat rate configuration
 * - Multiple scan code set support (Sets 1, 2, 3)
 * - Keyboard identification from device ID response
 */

#ifndef KEYBOARD_INTERFACE_H
#define KEYBOARD_INTERFACE_H

#include "pico/stdlib.h"

/**
 * @brief AT/PS2 Protocol Command Codes
 *
 * These commands are sent by the host to the keyboard to control
 * various operations and configurations.
 */
#define ATPS2_KEYBOARD_CMD_SET_LEDS          0xED /**< Set LED states command (followed by LED bitmap) */
#define ATPS2_KEYBOARD_CMD_SET_ALL_MAKEBREAK 0xF8 /**< Set all keys to make/break (Set 3 only) */

/**
 * @brief Common AT/PS2 Keyboard ID Codes
 *
 * These are typical 2-byte identification codes returned by keyboards
 * in response to the Get ID (0xF2) command.
 */
#define ATPS2_KEYBOARD_ID_LOW_MASK  0x00FF /**< Mask for low byte of keyboard ID */
#define ATPS2_KEYBOARD_ID_HIGH_MASK 0xFF00 /**< Mask for high byte of keyboard ID */
#define ATPS2_KEYBOARD_ID_UNKNOWN   0xFFFF /**< Unknown or unrecognised keyboard ID */

/**
 * @brief AT/PS2 Keyboard Interface Initialisation
 *
 * Configures and initialises the complete AT/PS2 keyboard interface using RP2040 PIO
 * hardware for precise timing and protocol implementation. Sets up all hardware and
 * software components required for reliable keyboard communication:
 *
 * Hardware Setup:
 * - Finds available PIO instance (PIO0 or PIO1) with sufficient program space
 * - Claims unused state machine and loads AT/PS2 protocol program
 * - Configures GPIO pins: DATA line and CLOCK line (DATA + 1)
 * - Sets up pull-up resistors for open-drain signaling
 *
 * PIO Configuration:
 * - Calculates clock divider for 30µs minimum pulse width timing
 * - Configures state machine for bidirectional communication
 * - Sets up automatic frame reception and transmission
 * - Enables precise timing compliance with AT/PS2 specifications
 *
 * Interrupt System:
 * - Configures PIO-specific IRQ (PIO0_IRQ_0 or PIO1_IRQ_0)
 * - Installs keyboard input event handler for frame processing
 * - Enables interrupt-driven data reception for optimal performance
 * - Links PIO RX FIFO to interrupt system
 *
 * Software Initialisation:
 * - Resets ring buffer to ensure clean startup state
 * - Initialises protocol state machine to UNINITIALISED
 * - Resets converter status LEDs when CONVERTER_LEDS enabled
 * - Clears all protocol state variables
 *
 * Timing Calibration:
 * - Uses IBM PS/2 Technical Reference timing specifications
 * - 30µs minimum clock pulse width (per IBM 84F9735 document)
 * - Supports 10-16.7 kHz clock frequency range
 * - Accommodates both AT and PS/2 timing requirements
 *
 * Error Handling:
 * - Validates PIO resource availability before configuration
 * - Reports initialisation failures with detailed error messages
 * - Graceful failure if insufficient PIO resources available
 * - Logs successful initialisation with configuration details
 *
 * @param data_pin GPIO pin number for DATA line (CLOCK automatically assigned as data_pin + 1)
 *
 * @note CLOCK pin must be DATA pin + 1 for hardware compatibility
 * @note Function blocks until hardware initialisation complete
 * @note Call before any other keyboard interface operations
 * @note Main loop only.
 */
void keyboard_interface_setup(uint data_pin);

/**
 * @brief AT/PS2 Keyboard Interface Main Task
 *
 * Main task function that coordinates all aspects of AT/PS2 keyboard communication
 * and protocol management. Handles both operational data flow and initialisation:
 *
 * Normal Operation (INITIALISED state):
 * - Processes scan codes from ring buffer when HID interface ready
 * - Manages LED state synchronisation (Caps/Num/Scroll Lock)
 * - Coordinates scan code translation and HID report generation
 * - Maintains optimal throughput whilst preventing report queue overflow
 *
 * Initialisation Management (Non-INITIALISED states):
 * - Monitors initialisation timeouts and implements retry logic
 * - Detects keyboard presence via clock line monitoring
 * - Handles stalled initialisation with automatic recovery
 * - Manages keyboard ID read timeouts with graceful fallback
 * - Coordinates LED setup timeout recovery
 *
 * Timeout Handling:
 * - 200ms periodic checks for initialisation progress
 * - 2-retry limit for keyboard ID operations before fallback
 * - 5-attempt detection cycle before reset request
 * - Automatic state machine recovery on persistent failures
 *
 * LED Synchronisation:
 * - Detects host lock key state changes
 * - Issues LED command sequence (0xED followed by LED bitmap)
 * - Handles LED command timeout and recovery
 * - Provides audio feedback on successful LED changes
 *
 * Hardware Integration:
 * - Monitors GPIO clock line for keyboard presence detection
 * - Updates converter status LEDs based on initialisation state
 * - Integrates with USB HID subsystem for optimal report timing
 * - Coordinates with ring buffer for efficient data flow
 *
 * Performance Features:
 * - Non-blocking ring buffer processing
 * - HID-ready checking prevents report queue overflow
 * - Efficient state-based processing reduces CPU overhead
 * - Interrupt-driven data reception with task-level processing
 *
 * @note Must be called periodically from main loop (typically 1-10ms intervals)
 * @note Function is non-blocking and maintains real-time responsiveness
 * @note Integrates with converter LED system when CONVERTER_LEDS enabled
 * @note Main loop only.
 */
void keyboard_interface_task(void);

#endif /* KEYBOARD_INTERFACE_H */
