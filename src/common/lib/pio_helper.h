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

#ifndef PIO_HELPER_H
#define PIO_HELPER_H

#include "hardware/pio.h"
#include <stdbool.h>

/**
 * @brief PIO engine instance for protocol/library use
 *
 * This structure combines PIO allocation result and validated storage.
 * Both sm and offset use int (matching SDK function return types) which can
 * represent both error states (-1) and valid values.
 *
 * After successful allocation:
 * - pio is non-NULL (pio0 or pio1)
 * - sm is 0-3 (valid state machine index)
 * - offset is 0-31 (program memory offset)
 *
 * On allocation failure:
 * - pio is NULL
 * - sm is -1
 * - offset is -1
 */
typedef struct {
    PIO pio;    /**< PIO instance (pio0/pio1), or NULL on failure */
    int sm;     /**< State machine (0-3), or -1 on failure */
    int offset; /**< PIO program memory offset (0-31), or -1 on failure */
} pio_engine_t;

pio_engine_t claim_pio_and_sm(const pio_program_t* program);
void         pio_restart(PIO pio, uint sm, uint offset);

/**
 * @brief Calculate PIO clock divider for protocol timing requirements
 *
 * Computes the optimal clock divider to achieve reliable sampling of protocol signals
 * based on the minimum expected pulse width. Uses 5× oversampling for edge detection.
 *
 * @param min_clock_pulse_width_us Minimum clock pulse width in microseconds
 * @return Computed clock divider as float (suitable for PIO hardware)
 */
float calculate_clock_divider(int min_clock_pulse_width_us);

#endif /* PIO_HELPER_H */
