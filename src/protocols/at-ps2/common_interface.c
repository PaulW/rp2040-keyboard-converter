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

void atps2_send_command(pio_engine_t* engine, uint8_t data_byte) {
    uint16_t data_with_parity = (uint16_t)(data_byte + (interface_parity_table[data_byte] << 8));
    pio_sm_put(engine->pio, engine->sm, data_with_parity);
}

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