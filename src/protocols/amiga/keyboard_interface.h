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
 * @brief Commodore Amiga Keyboard Protocol Implementation
 *
 * This file implements the Commodore Amiga keyboard protocol used by all Amiga
 * models (A1000, A500, A2000, A3000, A4000*). This is a bidirectional protocol with
 * handshaking and synchronisation recovery.
 *
 * A4000* Note: The A4000's keyboard protocol was not directly referenced or mentioned in the
 * official Commodore documentation, so this implementation may not be compatible.
 *
 * Protocol Characteristics:
 * - 2-wire interface: CLOCK (keyboard clock) + DATA (bidirectional data)
 * - CLOCK: Unidirectional, always driven by keyboard
 * - DATA: Bidirectional, driven by keyboard (data) and computer (handshake)
 * - Synchronous serial transmission with keyboard-generated clock
 * - Bit rotation: Transmitted as 6-5-4-3-2-1-0-7 (bit 7 last)
 * - Active-low logic: HIGH = 0, LOW = 1
 * - Mandatory handshake: Computer must pulse DATA low after each byte (PIO-driven)
 * - Automatic resynchronisation on handshake timeout (143ms)
 *
 * Physical Interface:
 * - CLOCK: Keyboard clock output, open-collector with pull-up
 * - DATA: Bidirectional data, open-collector with pull-up
 * - 5V TTL logic levels
 * - 4-pin connector: CLOCK, DATA, +5V, GND
 * - Pull-up resistors in both keyboard and computer
 *
 * Timing Requirements:
 * - Bit rate: ~17 kbit/sec (keyboard-generated)
 * - Bit period: ~60µs (20µs setup + 20µs low + 20µs high)
 * - Handshake: Must start within 1µs of byte completion
 * - Handshake pulse: intended target 85µs; actual ~15.5ms with current PIO divider
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
 * Initialisation Sequence:
 * 1. Keyboard sends 0xFD (initiate powerup key stream)
 * 2. Keyboard sends codes for any currently pressed keys
 * 3. Keyboard sends 0xFE (terminate key stream)
 * 4. Normal operation begins
 *
 * Handshake Protocol:
 * - Computer must pulse DATA low within 1µs of byte completion
 * - PIO handles handshake automatically (no software delay)
 * - Actual pulse duration: ~15.5ms with current divider (within keyboard tolerance)
 * - Missing handshake triggers resync (keyboard clocks out 1-bits)
 * - After resync: keyboard sends 0xF9, then retransmits failed byte
 *
 * Special Features:
 * - Reset warning: Three-key combo (CTRL + both Amiga keys)
 * - Reset: Keyboard sends 0x78; system reset in 10 seconds
 * - CAPS LOCK: Only generates code on press (not release), bit 7 = LED state
 *
 */

#ifndef KEYBOARD_INTERFACE_H
#define KEYBOARD_INTERFACE_H

#include <stdbool.h>
#include <stdint.h>

#include "pico/stdlib.h"

/**
 * @brief Amiga Protocol Special Codes
 *
 * These 8-bit codes have special meaning and are transmitted without
 * the standard key up/down bit interpretation.
 */
#define AMIGA_CODE_RESET_WARNING   0x78 /**< CTRL + both Amiga keys pressed (reset in 10s) */
#define AMIGA_CODE_LOST_SYNC       0xF9 /**< Last transmission failed, next byte retransmit */
#define AMIGA_CODE_BUFFER_OVERFLOW 0xFA /**< Keyboard output buffer overflow */
#define AMIGA_CODE_SELFTEST_FAIL   0xFC /**< Keyboard self-test failed on power-up */
#define AMIGA_CODE_POWERUP_START   0xFD /**< Initiate power-up key stream */
#define AMIGA_CODE_POWERUP_END     0xFE /**< Terminate power-up key stream */

/**
 * @brief Protocol Timing Constants
 *
 * These timing values are critical for proper Amiga keyboard operation.
 */
#define AMIGA_TIMING_CLOCK_MIN_US 20  /**< Minimum CLOCK pulse width for PIO detection */
#define AMIGA_TIMING_TIMEOUT_MS   143 /**< Handshake timeout before keyboard resyncs */

/**
 * @brief Bit Rotation Masks
 *
 * Amiga protocol transmits bits in rotated order: 6-5-4-3-2-1-0-7
 * These masks help with de-rotation after reception.
 */
#define AMIGA_BIT_ROTATION_BIT7 0x01U /**< Transmitted bit 0 = original bit 7 */
#define AMIGA_BIT_ROTATION_REST 0xFEU /**< Transmitted bits 7-1 = original bits 6-0 */

/**
 * @brief Key Code Range and Encoding Constants
 *
 * The Amiga 8-bit scancode byte encodes make/break in bit 7 and the physical key
 * identity in bits 6-0.  Valid key codes span 0x00-0x67 (104 keys).
 */
#define AMIGA_MAX_KEYCODE  0x67U /**< Highest valid Amiga key code (Right Amiga key) */
#define AMIGA_KEYCODE_MASK 0x7FU /**< Mask to extract 7-bit key code (strips bit 7) */
#define AMIGA_BREAK_BIT    0x80U /**< Bit 7 set = key release (break); clear = press (make) */

/**
 * @brief Setup Amiga keyboard interface
 *
 * Initialises PIO state machine, GPIO configuration, and interrupt handling
 * for Amiga keyboard protocol:
 *
 * PIO Configuration:
 * - Atomically claims a PIO block + state machine via claim_pio_and_sm()
 * - Loads keyboard interface program into PIO instruction memory
 * - Configures clock divider for ~20µs timing detection
 *
 * GPIO Setup:
 * - DATA (data_pin): Bidirectional, open-collector with pull-up
 * - CLOCK (data_pin + 1): Input only, keyboard-driven, with pull-up
 * - Both pins initialised for PIO control
 *
 * Interrupt Configuration:
 * - Enables RX FIFO not empty interrupt
 * - Registers keyboard_input_event_handler as ISR
 * - IRQ priority set for timely handshake response
 *
 * @param data_pin GPIO pin for DATA (bidirectional data line)
 *                 CLOCK is automatically assigned to data_pin + 1
 * @note Main loop only.
 */
void keyboard_interface_setup(uint data_pin);

/**
 * @brief Main task function for Amiga keyboard interface
 *
 * This function must be called periodically from the main loop to process
 * scan codes from the ring buffer and handle keyboard state management.
 *
 * Operation Modes:
 *
 * UNINITIALISED State:
 * - Waiting for first byte from keyboard (power-up sequence)
 * - Keyboard sends 0xFD (power-up start) after self-test
 * - Transition to INITIALISED happens in ISR on first byte
 *
 * INITIALISED State (Normal Operation):
 * - Process scan codes from ring buffer when HID interface ready
 * - Coordinates scan code translation for Amiga protocol
 * - Maintains optimal data flow between keyboard and USB HID
 * - Prevents HID report queue overflow through ready checking
 *
 * Data Processing:
 * - Non-blocking ring buffer processing for real-time performance
 * - HID-ready checking prevents USB report queue overflow
 * - Scan codes processed through dedicated Amiga processor
 * - Handles CAPS LOCK special behaviour (press only, LED state tracking)
 *
 * Protocol Features:
 * - No host-driven initialisation sequence (unlike AT/PS2) — the keyboard performs its own power-up
 * startup stream (0xFD … 0xFE); the host simply waits and receives
 * - No LED control capability from computer (keyboard maintains state)
 * - Bidirectional handshake handled entirely in PIO hardware
 * - Special codes (0x78, 0xF9-0xFE) filtered by event processor
 *
 * Error Recovery:
 * - Lost sync recovery automatic (keyboard sends 0xF9, retransmits)
 * - Buffer overflow protection (ring buffer full checks)
 * - Keyboard self-test failures logged (0xFC)
 * - Reset warning detection (0x78)
 *
 * Performance:
 * - Called from main loop (not time-critical like ISR)
 * - Processing deferred until HID ready (prevents USB congestion)
 * - Efficient scan code processing without protocol overhead
 * - Direct translation to modern HID key codes via scancode processor
 *
 * Hardware Monitoring:
 * - Converter status LED updates when CONVERTER_LEDS enabled
 * - Keyboard ready state reflects INITIALISED status
 *
 * @note Must be called regularly from main loop for scan code processing
 * @note Ring buffer filled by ISR, consumed here (producer-consumer pattern)
 * @note HID ready check is critical to prevent USB report queue overflow
 * @note Handshake timing handled entirely in PIO (not here)
 * @note Main loop only.
 */
void keyboard_interface_task(void);

/**
 * @brief De-rotate received Amiga byte
 *
 * Amiga keyboards transmit bytes in rotated order (6-5-4-3-2-1-0-7).
 * This function converts the rotated byte back to normal bit order.
 *
 * @param rotated Byte received from keyboard (rotated format)
 * @return Original byte in standard bit order [7-6-5-4-3-2-1-0]
 * @note IRQ-safe.
 */
static inline uint8_t amiga_derotate_byte(uint8_t rotated) {
    // Received: [6 5 4 3 2 1 0 7]
    // Target:   [7 6 5 4 3 2 1 0]
    uint8_t original = 0;
    original |= (rotated & AMIGA_BIT_ROTATION_BIT7) << 7;  // Bit 0 → Bit 7
    original |= (rotated & AMIGA_BIT_ROTATION_REST) >> 1;  // Bits 7-1 → Bits 6-0
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
 * @note IRQ-safe.
 */
static inline bool amiga_is_special_code(uint8_t code) {
    return (code == AMIGA_CODE_RESET_WARNING || code == AMIGA_CODE_LOST_SYNC ||
            code == AMIGA_CODE_BUFFER_OVERFLOW || code == AMIGA_CODE_SELFTEST_FAIL ||
            code == AMIGA_CODE_POWERUP_START || code == AMIGA_CODE_POWERUP_END);
}

#endif /* KEYBOARD_INTERFACE_H */
