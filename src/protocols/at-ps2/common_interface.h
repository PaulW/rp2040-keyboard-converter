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
 * @file common_interface.h
 * @brief AT/PS2 Protocol Common Definitions
 * 
 * This file contains shared constants and definitions used by both
 * keyboard and mouse AT/PS2 protocol implementations. These constants
 * represent commands and responses that are common between both device types.
 */

#ifndef COMMON_INTERFACE_H
#define COMMON_INTERFACE_H

#include "pico/stdlib.h"

/**
 * @brief AT/PS2 Protocol Shared Command Codes
 * 
 * These commands have the same meaning and value for both keyboards and mice.
 * They are defined here to avoid duplication between device-specific headers.
 */
#define ATPS2_CMD_GET_ID            0xF2  /**< Read device ID (keyboard: 2-byte, mouse: 1-byte) */
#define ATPS2_CMD_RESEND            0xFE  /**< Resend last response (error recovery) */
#define ATPS2_CMD_RESET             0xFF  /**< Reset device (triggers self-test sequence) */

/**
 * @brief AT/PS2 Protocol Shared Response Codes
 * 
 * These responses have the same meaning and value for both keyboards and mice.
 */
#define ATPS2_RESP_BAT_PASSED       0xAA  /**< Basic Assurance Test (self-test) passed */
#define ATPS2_RESP_ACK              0xFA  /**< Command acknowledgment (success) */

/**
 * @brief Protocol Timing Constants
 * 
 * These timing values are shared between keyboard and mouse implementations.
 */
#define ATPS2_TIMING_CLOCK_MIN_US   30    /**< Minimum clock pulse width (30µs per IBM specification) */

/**
 * @brief Protocol Data Constants
 * 
 * These constants are used for data processing in both keyboard and mouse interfaces.
 */
#define ATPS2_DATA_MASK             0xFF  /**< Data byte extraction mask */

extern uint8_t interface_parity_table[];

#endif /* COMMON_INTERFACE_H */
