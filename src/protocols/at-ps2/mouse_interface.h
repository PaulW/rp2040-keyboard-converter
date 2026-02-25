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
 * @file mouse_interface.h
 * @brief AT/PS2 Mouse Protocol Implementation
 *
 * This file implements the AT and PS/2 mouse protocols used by IBM PC compatible systems.
 * The mouse protocol shares the same physical interface as keyboards but uses different
 * command sets and data packet formats for position and button reporting.
 *
 * Protocol Characteristics:
 * - 2-wire interface: DATA and CLOCK (shared with keyboard, same electrical specs)
 * - Mouse normally controls clock timing during data transmission
 * - Host can inhibit communication by pulling CLOCK low
 * - LSB-first bit transmission (bit 0 → bit 7)
 * - Start bit (0), 8 data bits, odd parity bit, stop bit (1)
 * - Bidirectional communication with command acknowledgments
 *
 * Supported Mouse Types:
 * - Standard PS/2 Mouse (3-byte packets): Position + 3 buttons
 * - IntelliMouse (4-byte packets): Position + 3 buttons + scroll wheel
 * - IntelliMouse Explorer (4-byte packets): Position + 5 buttons + scroll wheel
 *
 * Data Packet Formats:
 * - Standard: [Status Byte] [X Movement] [Y Movement]
 * - Extended: [Status Byte] [X Movement] [Y Movement] [Z Movement/Extra Buttons]
 *
 * Timing Requirements:
 * - Same as keyboard: 10-16.7 kHz clock, minimum 30µs pulse width
 * - Data setup/hold: minimum 5µs before/after clock edge
 * - Inhibit time: minimum 100µs CLOCK low to stop transmission
 *
 * Initialisation Sequence:
 * 1. Power-on self-test (mouse sends 0xAA on success, 0x00 as device ID)
 * 2. Mouse type detection via sample rate sequence (200-100-80 or 200-200-80)
 * 3. Configuration: resolution, scaling, sample rate, enable data reporting
 * 4. Continuous data streaming of movement/button packets
 *
 * Special Features:
 * - Multiple resolution settings (1-8 counts/mm)
 * - Adjustable sample rate (10-200 reports/second)
 * - 1:1 or 2:1 scaling modes
 * - Stream mode (continuous) or remote mode (on-request) reporting
 */

#ifndef MOUSE_INTERFACE_H
#define MOUSE_INTERFACE_H

#include "pico/stdlib.h"

/**
 * @brief AT/PS2 Mouse Protocol Command Codes
 *
 * These commands are sent by the host to the mouse to control
 * various operations and configurations.
 */
#define ATPS2_MOUSE_CMD_ENABLE          0xF4 /**< Enable mouse (start sending data) */
#define ATPS2_MOUSE_CMD_SET_SCALING_1_1 0xE6 /**< Set 1:1 scaling (no acceleration) */
#define ATPS2_MOUSE_CMD_SET_RESOLUTION  0xE8 /**< Set resolution (followed by resolution code) */
#define ATPS2_MOUSE_CMD_SET_SAMPLE_RATE 0xF3 /**< Set sample rate (followed by rate value) */

/**
 * @brief AT/PS2 Mouse Device ID Codes
 *
 * These identification codes are returned in response to the Get ID (0xF2) command
 * and are used to determine mouse capabilities and packet format.
 */
#define ATPS2_MOUSE_ID_STANDARD     0x00 /**< Standard PS/2 mouse (3-byte packets) */
#define ATPS2_MOUSE_ID_INTELLIMOUSE 0x03 /**< IntelliMouse with scroll wheel (4-byte packets) */
#define ATPS2_MOUSE_ID_INTELLIMOUSE_EXPLORER \
    0x04                            /**< IntelliMouse Explorer with 5 buttons (4-byte packets) */
#define ATPS2_MOUSE_ID_UNKNOWN 0xFF /**< Unknown or unrecognised mouse ID */

/**
 * @brief Mouse Resolution Codes
 *
 * These values are used with the Set Resolution (0xE8) command.
 */
#define ATPS2_MOUSE_RES_8_COUNT_MM 0x03 /**< 8 count/mm resolution */

/**
 * @brief Mouse Sample Rate Values
 *
 * These values are used with the Set Sample Rate (0xF3) command.
 * Used for both configuration and IntelliMouse detection sequences.
 */
#define ATPS2_MOUSE_RATE_40_HZ  0x28 /**< 40 reports per second */
#define ATPS2_MOUSE_RATE_80_HZ  0x50 /**< 80 reports per second */
#define ATPS2_MOUSE_RATE_100_HZ 0x64 /**< 100 reports per second */
#define ATPS2_MOUSE_RATE_200_HZ 0xC8 /**< 200 reports per second */

/**
 * @brief Mouse Data Packet Constants
 *
 * These constants define packet structure and bit positions for mouse data.
 */
#define ATPS2_MOUSE_PACKET_STANDARD_SIZE 3 /**< Standard mouse packet size (bytes) */
#define ATPS2_MOUSE_PACKET_EXTENDED_SIZE 4 /**< Extended mouse packet size (bytes) */

/**
 * @brief Initialises the AT/PS2 mouse interface
 *
 * Sets up PIO state machine, GPIO configuration, and protocol handlers
 * for AT/PS2 mouse communication. Shares physical interface with keyboard
 * but uses separate state machine and command processing.
 *
 * @param data_pin GPIO pin for DATA line (CLOCK will be data_pin + 1)
 */
void mouse_interface_setup(uint data_pin);

/**
 * @brief Main task function for AT/PS2 mouse interface
 *
 * Manages protocol state machine, processes received data, handles
 * initialisation sequence, and maintains mouse communication.
 * Should be called periodically from main loop.
 */
void mouse_interface_task();

#endif /* MOUSE_INTERFACE_H */
