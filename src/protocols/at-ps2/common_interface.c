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

#include "common_interface.h"

#include <stdbool.h>

#include "interface.pio.h" /* pio_interface_program, pio_interface_program_init */

#include "log.h"

/**
 * @brief Mapping of HEX to Parity Bit for input from AT Keyboard.
 *
 * This array maps each hexadecimal value (0-255) to its corresponding parity bit for input from an
 * AT Keyboard. The parity bit is used for error checking in the keyboard data.
 *
 * The array is organised in a 16x16 grid, where each row represents a hexadecimal digit (0-F) and
 * each column represents a parity bit (0 or 1). The value at each position in the grid represents
 * the parity bit for the corresponding hexadecimal value.
 *
 * Example:
 * - interface_parity_table[0x3C] = 0 (parity bit for hexadecimal value 3C is 0)
 * - interface_parity_table[0x8A] = 1 (parity bit for hexadecimal value 8A is 1)
 *
 * @note This array is used in the implementation of the AT and PS2 protocols.
 */
uint8_t interface_parity_table[256] = {
    /* Mapping of HEX to Parity Bit for input from AT Keyboard
    0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F */
    1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,  // 0
    0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,  // 1
    0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,  // 2
    1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,  // 3
    0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,  // 4
    1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,  // 5
    1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,  // 6
    0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,  // 7
    0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,  // 8
    1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,  // 9
    1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,  // A
    0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,  // B
    1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,  // C
    0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,  // D
    0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,  // E
    1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1   // F
};

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
void atps2_send_command(pio_engine_t* engine, uint8_t data_byte) {
    uint16_t data_with_parity = (uint16_t)(data_byte + (interface_parity_table[data_byte] << 8));
    pio_sm_put(engine->pio, engine->sm, data_with_parity);
}

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
                            const char* device_name) {
    *engine = claim_pio_and_sm(&pio_interface_program);
    if (engine->pio == NULL) {
        LOG_ERROR("AT/PS2 %s: No PIO resources available\n", device_name);
        return false;
    }

    // Calculate clock divider for AT/PS2 timing (30µs minimum pulse width)
    // Reference: IBM 84F9735 PS/2 Hardware Interface Technical Reference
    float clock_div = calculate_clock_divider(ATPS2_TIMING_CLOCK_MIN_US);

    // Initialise PIO program with calculated clock divider
    pio_interface_program_init(engine->pio, engine->sm, engine->offset, data_pin, clock_div);

    // Initialise shared PIO IRQ dispatcher (safe to call multiple times)
    pio_irq_dispatcher_init(engine->pio);

    // Register device-specific event handler with the dispatcher
    if (!pio_irq_register_callback(callback)) {
        LOG_ERROR("AT/PS2 %s: Failed to register IRQ callback\n", device_name);
        release_pio_and_sm(engine, &pio_interface_program);
        return false;
    }

    LOG_INFO("PIO%d SM%d Interface program loaded at offset %d with clock divider of %.2f\n",
             (engine->pio == pio0 ? 0 : 1), engine->sm, engine->offset, clock_div);
    return true;
}