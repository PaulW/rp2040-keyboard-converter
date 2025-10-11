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

/**
 * @file keyboard_interface.h  
 * @brief Commodore Amiga Keyboard Protocol Implementation
 * 
 * This file implements the Commodore Amiga keyboard protocol used by Amiga computers
 * (A1000, A500, A2000, A3000, A4000) from 1985 onwards. This is a sophisticated
 * bidirectional protocol with handshaking and synchronization recovery.
 * 
 * Protocol Characteristics:
 * - 2-wire interface: KCLK (keyboard clock) + KDAT (bidirectional data)
 * - KCLK: Unidirectional, always driven by keyboard
 * - KDAT: Bidirectional, driven by keyboard (data) and computer (handshake)
 * - Synchronous serial transmission with keyboard-generated clock
 * - Bit rotation: Transmitted as 6-5-4-3-2-1-0-7 (bit 7 last)
 * - Active-low logic: HIGH = 0, LOW = 1
 * - Mandatory handshake: Computer must pulse KDAT low for 85µs after each byte
 * - Automatic resynchronization on handshake timeout (143ms)
 * 
 * Physical Interface:
 * - KCLK: Keyboard clock output, open-collector with pull-up
 * - KDAT: Bidirectional data, open-collector with pull-up
 * - 5V TTL logic levels
 * - 4-pin connector: KCLK, KDAT, +5V, GND
 * - Pull-up resistors in both keyboard and computer
 * 
 * Timing Requirements:
 * - Bit rate: ~17 kbit/sec (keyboard-generated)
 * - Bit period: ~60µs (20µs setup + 20µs low + 20µs high)
 * - Handshake: Must start within 1µs of byte completion
 * - Handshake pulse: 85µs minimum (CRITICAL for compatibility)
 * - Timeout: 143ms without handshake triggers resync mode
 * 
 * Data Format:
 * - 8 bits per byte, rotated before transmission
 * - Original format: [7 6 5 4 3 2 1 0]
 * - Transmitted as: [6 5 4 3 2 1 0 7]
 * - Bit 7: Key up/down flag (0 = pressed, 1 = released)
 * - Bits 6-0: Key identification code
 * - Special codes: 0x78, 0xF9, 0xFA, 0xFC, 0xFD, 0xFE
 * 
 * Initialization Sequence:
 * 1. Keyboard powers up and performs self-test
 * 2. Keyboard clocks out 1-bits slowly until handshake received
 * 3. After sync: keyboard sends 0xFD (initiate powerup key stream)
 * 4. Keyboard sends codes for any currently pressed keys
 * 5. Keyboard sends 0xFE (terminate key stream)
 * 6. Normal operation begins
 * 
 * Handshake Protocol:
 * - Computer MUST respond to every byte within 1µs
 * - Response: Pull KDAT low for exactly 85µs
 * - Shorter pulses may not be detected by all keyboards
 * - Missing handshake triggers resync (keyboard clocks out 1-bits)
 * - After resync: keyboard sends 0xF9, then retransmits failed byte
 * 
 * Special Features:
 * - Reset warning: Three-key combo (CTRL + both Amiga keys)
 * - Reset sequence: Keyboard sends 0x78 twice, waits 10 seconds
 * - Hard reset: Keyboard pulls KCLK low for 500ms
 * - CAPS LOCK: Only generates code on press (not release), bit 7 = LED state
 * 
 * Compatibility:
 * - Compatible with all Amiga models (A1000, A500, A2000, A3000, A4000)
 * - Some keyboard variations exist (numeric pad codes differ)
 * - Protocol specification consistent across all models
 */

#ifndef KEYBOARD_INTERFACE_H
#define KEYBOARD_INTERFACE_H

#include "pico/stdlib.h"
#include <stdbool.h>

/**
 * @brief Amiga Protocol Special Codes
 * 
 * These 8-bit codes have special meaning and are transmitted without
 * the standard key up/down bit interpretation.
 */
#define AMIGA_CODE_RESET_WARNING    0x78  /**< CTRL + both Amiga keys pressed (reset in 10s) */
#define AMIGA_CODE_LOST_SYNC        0xF9  /**< Last key code transmission failed, next byte is retransmit */
#define AMIGA_CODE_BUFFER_OVERFLOW  0xFA  /**< Keyboard output buffer overflow */
#define AMIGA_CODE_SELFTEST_FAIL    0xFC  /**< Keyboard self-test failed on power-up */
#define AMIGA_CODE_POWERUP_START    0xFD  /**< Initiate power-up key stream */
#define AMIGA_CODE_POWERUP_END      0xFE  /**< Terminate power-up key stream */

/**
 * @brief Protocol Timing Constants
 * 
 * These timing values are critical for proper Amiga keyboard operation.
 */
#define AMIGA_TIMING_BIT_PERIOD_US  60    /**< Typical bit period (20µs × 3 phases) */
#define AMIGA_TIMING_CLOCK_MIN_US   20    /**< Minimum KCLK pulse width for PIO detection */
#define AMIGA_TIMING_HANDSHAKE_US   85    /**< Handshake pulse width (MUST be 85µs) */
#define AMIGA_TIMING_HANDSHAKE_MAX_US 1   /**< Maximum delay before handshake starts */
#define AMIGA_TIMING_TIMEOUT_MS     143   /**< Handshake timeout before keyboard resyncs */

/**
 * @brief Bit Rotation Masks
 * 
 * Amiga protocol transmits bits in rotated order: 6-5-4-3-2-1-0-7
 * These masks help with de-rotation after reception.
 */
#define AMIGA_BIT_ROTATION_BIT7     0x01  /**< Transmitted bit 0 = original bit 7 */
#define AMIGA_BIT_ROTATION_REST     0xFE  /**< Transmitted bits 7-1 = original bits 6-0 */

/**
 * @brief Initializes the Commodore Amiga keyboard interface
 * 
 * Sets up PIO state machine and GPIO configuration for Amiga keyboard
 * communication. Configures bidirectional KDAT line for data reception
 * and handshake transmission. KCLK pin is automatically set to data_pin + 1.
 * 
 * @param data_pin GPIO pin for KDAT (bidirectional: input for data, output for handshake)
 *                 KCLK is automatically assigned to data_pin + 1
 */
void keyboard_interface_setup(uint data_pin);

/**
 * @brief Main task function for Amiga keyboard interface
 * 
 * Processes received scan codes, manages handshake timing, handles special
 * codes, and manages the Amiga protocol state machine. Must be called
 * periodically from main loop to ensure handshake timing requirements are met.
 * 
 * Note: Handshake timing (≤1µs start, 85µs pulse) is handled in PIO/ISR.
 * This task should run regularly to drain scan codes and manage CAPS LOCK timing.
 */
void keyboard_interface_task();

/**
 * @brief De-rotate received Amiga byte
 * 
 * Amiga keyboards transmit bytes in rotated order (6-5-4-3-2-1-0-7).
 * This function converts the rotated byte back to normal bit order.
 * 
 * @param rotated Byte received from keyboard (rotated format)
 * @return Original byte in standard bit order [7-6-5-4-3-2-1-0]
 */
static inline uint8_t amiga_derotate_byte(uint8_t rotated) {
    // Received: [6 5 4 3 2 1 0 7]
    // Target:   [7 6 5 4 3 2 1 0]
    uint8_t original = 0;
    original |= (rotated & 0x01) << 7;  // Bit 0 → Bit 7
    original |= (rotated & 0xFE) >> 1;  // Bits 7-1 → Bits 6-0
    return original;
}

/**
 * @brief Check if code is a special Amiga protocol code
 * 
 * Special codes (0x78, 0xF9, 0xFA, 0xFC, 0xFD, 0xFE) have protocol-specific
 * meaning and should be handled differently from normal key codes.
 * 
 * @param code Byte received from keyboard (after de-rotation)
 * @return true if code is a special protocol code
 */
static inline bool amiga_is_special_code(uint8_t code) {
    return (code == AMIGA_CODE_RESET_WARNING ||
            code == AMIGA_CODE_LOST_SYNC ||
            code == AMIGA_CODE_BUFFER_OVERFLOW ||
            code == AMIGA_CODE_SELFTEST_FAIL ||
            code == AMIGA_CODE_POWERUP_START ||
            code == AMIGA_CODE_POWERUP_END);
}

#endif /* KEYBOARD_INTERFACE_H */
