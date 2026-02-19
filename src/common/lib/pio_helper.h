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
#include <stdint.h>

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

/**
 * @brief Claim a PIO instance, state machine, and program offset atomically.
 *
 * @param program PIO program to load.
 * @return Allocated engine or {NULL, -1, -1} on failure.
 */
pio_engine_t claim_pio_and_sm(const pio_program_t* program);

/**
 * @brief Restart a PIO state machine at the given offset.
 *
 * @param pio    PIO instance.
 * @param sm     State machine index.
 * @param offset Program offset to jump to.
 */
void pio_restart(PIO pio, uint sm, uint offset);

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

/**
 * @brief PIO IRQ Callback Function Type
 *
 * Callback function type for PIO IRQ handlers. Protocols register their event
 * handlers using this type to receive PIO interrupt notifications.
 */
typedef void (*pio_irq_callback_t)(void);

/**
 * @brief Initialize the PIO IRQ dispatcher for a PIO instance
 *
 * Sets up a shared PIO IRQ handler that multiplexes between multiple protocol
 * event handlers. This allows multiple devices (e.g., keyboard + mouse) to share
 * the same PIO IRQ without conflicts. Must be called before registering callbacks.
 *
 * IMPORTANT: The callback registry is global (single registered_callbacks[4] array)
 * shared across ALL PIO instances. All registered callbacks are invoked when ANY
 * PIO IRQ fires, regardless of which PIO they were registered for.
 *
 * @param pio The PIO instance (pio0 or pio1) to configure
 *
 * @note Safe to call multiple times for the same PIO - subsequent calls are ignored
 * @note Sets IRQ priority to 0 (highest) for time-critical protocol timing
 * @note Supports only ONE PIO instance per session (see pio_helper.c for details)
 */
void pio_irq_dispatcher_init(PIO pio);

/**
 * @brief Register a callback for PIO IRQ events
 *
 * Adds a callback function to the global dispatcher invocation list. The dispatcher
 * invokes all registered callbacks when ANY PIO IRQ fires (PIO0 or PIO1).
 *
 * IMPORTANT: The callback registry is GLOBAL with only 4 slots TOTAL (not per-PIO).
 * All callbacks share these 4 slots across PIO instances. When any PIO IRQ fires,
 * ALL registered callbacks are invoked regardless of which PIO triggered the IRQ.
 *
 * Recommended strategies:
 * - Use only one PIO instance for all protocols (preferred)
 * - Make callbacks PIO-agnostic (handle events from any PIO)
 * - Add self-guarding in callbacks (check which PIO fired before processing)
 *
 * @param callback Function pointer to the IRQ handler
 * @return true if registration succeeded, false if no slots available
 *
 * @note Must call pio_irq_dispatcher_init() before registering callbacks
 * @note Callbacks are invoked in IRQ context - must be fast and non-blocking
 * @note Maximum 4 callbacks total across ALL PIO instances
 */
bool pio_irq_register_callback(pio_irq_callback_t callback);

/**
 * @brief Unregister a callback from PIO IRQ events
 *
 * Removes a previously registered callback from the global dispatcher. The callback
 * will no longer be invoked when any PIO IRQ fires.
 *
 * @param callback Function pointer to remove from the dispatcher
 *
 * @note Safe to call even if callback was never registered
 * @note Operates on global callback registry (affects all PIO instances)
 */
void pio_irq_unregister_callback(pio_irq_callback_t callback);

#endif /* PIO_HELPER_H */
