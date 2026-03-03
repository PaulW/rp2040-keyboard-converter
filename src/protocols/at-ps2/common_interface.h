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

#include <stdbool.h>
#include <stdint.h>

#include "pico/stdlib.h"

#include "pio_helper.h"  // pio_engine_t, pio_irq_callback_t

/**
 * @brief AT/PS2 Protocol Shared Command Codes
 *
 * These commands have the same meaning and value for both keyboards and mice.
 * They are defined here to avoid duplication between device-specific headers.
 */
#define ATPS2_CMD_GET_ID 0xF2 /**< Read device ID (keyboard: 2-byte, mouse: 1-byte) */
#define ATPS2_CMD_RESEND 0xFE /**< Resend last response (error recovery) */
#define ATPS2_CMD_RESET  0xFF /**< Reset device (triggers self-test sequence) */

/**
 * @brief AT/PS2 Protocol Shared Response Codes
 *
 * These responses have the same meaning and value for both keyboards and mice.
 */
#define ATPS2_RESP_BAT_PASSED 0xAA /**< Basic Assurance Test (self-test) passed */
#define ATPS2_RESP_ACK        0xFA /**< Command acknowledgment (success) */

/**
 * @brief Protocol Timing Constants
 *
 * These timing values are shared between keyboard and mouse implementations.
 */
#define ATPS2_TIMING_CLOCK_MIN_US 30 /**< Min clock pulse width (30µs per IBM specification) */

/**
 * @brief Protocol Data Constants
 *
 * These constants are used for data processing in both keyboard and mouse interfaces.
 */
#define ATPS2_DATA_MASK 0xFF /**< Data byte extraction mask */

/**
 * @brief AT/PS2 serial frame bit layout within the PIO FIFO aligned word.
 *
 * The PIO state machine captures 11-bit frames and right-packs them into a 32-bit FIFO
 * word.  After right-shifting by ATPS2_FRAME_FIFO_SHIFT, the bits are laid out as:
 *
 *   [0]     = start bit (must be 0)
 *   [1..8]  = data byte (LSB first)
 *   [9]     = parity bit (odd parity over data byte)
 *   [10]    = stop bit (must be 1 for compliant devices)
 */
#define ATPS2_FRAME_FIFO_SHIFT 21U /**< Right-shift to align 11-bit frame from PIO FIFO word */
#define ATPS2_FRAME_PARITY_BIT 9U  /**< Bit index of odd-parity bit in the aligned frame */
#define ATPS2_FRAME_STOP_BIT   10U /**< Bit index of stop bit (must be HIGH) in aligned frame */
#define ATPS2_FRAME_DATA_SHIFT                                        \
    1U /**< Right-shift to extract 8-bit data byte from aligned frame \
        */

/**
 * @brief Parsed AT/PS2 serial frame.
 *
 * Populated by atps2_parse_frame() from a single PIO FIFO word.  Used by both keyboard
 * and mouse IRQ handlers after the shared extraction step.
 */
typedef struct {
    uint8_t start_bit;       /**< Start bit (must be 0) */
    uint8_t parity_bit;      /**< Received odd-parity bit */
    uint8_t stop_bit;        /**< Stop bit (1 = compliant, 0 = non-standard) */
    uint8_t data;            /**< 8-bit data byte extracted from frame */
    uint8_t expected_parity; /**< Computed odd-parity bit for @p data */
} atps2_frame_t;

/**
 * @brief Mapping of HEX to parity bit for AT/PS2 byte values.
 *
 * Maps each byte value (0x00–0xFF) to its corresponding odd-parity bit as
 * required by the AT/PS2 serial frame format. Used by both keyboard and mouse
 * interface drivers to construct outgoing command frames and to validate incoming
 * parity bits against received data.
 *
 * Example:
 * - interface_parity_table[0x3C] = 1 (0x3C has 4 ones, parity bit is 1)
 * - interface_parity_table[0x8A] = 0 (0x8A has 3 ones, parity bit is 0)
 */
extern uint8_t interface_parity_table[];

/**
 * @brief Parse one AT/PS2 frame from the PIO RX FIFO.
 *
 * Reads the next word from the given PIO state machine RX FIFO, right-aligns the
 * 11-bit serial frame, and extracts all constituent fields into an atps2_frame_t.
 * Caller must ensure the FIFO is non-empty before calling (use
 * pio_sm_is_rx_fifo_empty()).
 *
 * @param engine  Pointer to the active PIO engine for this interface.
 * @return        Fully populated atps2_frame_t.
 *
 * @note Inline to avoid call overhead in __isr context.
 * @note IRQ-safe — reads hardware FIFO only; no shared state modified.
 */
static inline atps2_frame_t atps2_parse_frame(const pio_engine_t* engine) {
    io_ro_32      raw  = engine->pio->rxf[(uint)engine->sm] >> ATPS2_FRAME_FIFO_SHIFT;
    uint16_t      bits = (uint16_t)raw;
    atps2_frame_t f;
    f.start_bit       = (uint8_t)(bits & 0x1U);
    f.parity_bit      = (uint8_t)((bits >> ATPS2_FRAME_PARITY_BIT) & 0x1U);
    f.stop_bit        = (uint8_t)((bits >> ATPS2_FRAME_STOP_BIT) & 0x1U);
    f.data            = (uint8_t)((raw >> ATPS2_FRAME_DATA_SHIFT) & ATPS2_DATA_MASK);
    f.expected_parity = interface_parity_table[f.data];
    return f;
}

/**
 * @brief   Send a PS/2 command byte to the device via the PIO engine, with parity appended.
 *
 * Packs @p data_byte together with its odd-parity bit (looked up from
 * @c interface_parity_table) into a 16-bit word and writes it to the PIO state
 * machine TX FIFO.  The PIO programme is responsible for shifting out the data
 * and parity bits over the PS/2 CLK/DATA lines.
 *
 * @param   engine      Pointer to the @c pio_engine_t that owns the PIO state machine.
 * @param   data_byte   Command byte to transmit; parity is appended automatically
 *                      using @c interface_parity_table[data_byte].
 * @return  void
 * @note    Main loop only — must not be called from an ISR.  No locking is
 *          performed; callers must ensure single-threaded access to the PIO TX FIFO.
 */
void atps2_send_command(pio_engine_t* engine, uint8_t data_byte);

/**
 * @brief   Claim PIO resources and initialise the AT/PS2 interface engine.
 *
 * Allocates a PIO instance and state machine via @c claim_pio_and_sm(), loads
 * and initialises the AT/PS2 PIO programme with the correct clock divider for
 * the protocol timing requirements, initialises the shared PIO IRQ dispatcher,
 * and registers @p callback as the device-specific event handler.
 *
 * On any failure the function logs the error, releases all claimed resources
 * via @c release_pio_and_sm(), and returns @c false so the caller need not
 * perform additional teardown.
 *
 * @param   engine       Output parameter; receives the claimed @c pio_engine_t
 *                       (PIO instance, state machine index, and program offset).
 * @param   data_pin     GPIO pin number used as the combined PS/2 CLK/DATA line.
 * @param   callback     PIO IRQ callback to register with the shared dispatcher
 *                       for this device's state machine events.
 * @param   device_name  Human-readable name used in log messages (e.g. @c "keyboard").
 * @return  @c true on success; @c false if PIO resources are unavailable or
 *          the IRQ callback slot could not be registered (all resources freed).
 * @note    Main loop only — must not be called from an ISR.  Not reentrant;
 *          call once per device during initialisation.
 */
bool atps2_setup_pio_engine(pio_engine_t* engine, uint data_pin, pio_irq_callback_t callback,
                            const char* device_name);

#endif /* COMMON_INTERFACE_H */
