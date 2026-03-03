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
 * @file pio_helper.h
 * @brief RP2040 PIO state machine lifecycle management and IRQ multiplexing.
 *
 * Provides a uniform allocation, configuration, and release model for RP2040 PIO state
 * machines used by keyboard and mouse protocol implementations.
 *
 * Allocation Model:
 * `claim_pio_and_sm()` tries PIO0 then PIO1, using the Pico SDK's `pio_claim_unused_sm()`
 * to acquire a state machine and `pio_add_program()` to load the protocol's PIO assembly
 * into instruction memory. The result is a `pio_engine_t` struct {pio, sm, offset}; failure
 * returns the sentinel {NULL, -1, -1}. `release_pio_and_sm()` reverses this atomically,
 * disabling the SM and freeing its program memory.
 *
 * Clock Divider:
 * `calculate_clock_divider()` computes a divisor for 5× oversampling of protocol clock
 * edges. The 5× factor provides at least 5 samples per shortest protocol pulse.
 *
 * IRQ Multiplexing:
 * The RP2040's PIO IRQ lines are shared resources. `pio_irq_dispatcher_init()` registers
 * a single top-level IRQ handler for a PIO instance that dispatches to up to four
 * registered callbacks. This allows the keyboard and AT/PS2 mouse interfaces to coexist
 * on separate state machines without IRQ conflicts.
 *
 */

#ifndef PIO_HELPER_H
#define PIO_HELPER_H

#include <stdbool.h>
#include <stdint.h>

#include "hardware/pio.h"

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
 *
 * @note Main loop only.
 */
pio_engine_t claim_pio_and_sm(const pio_program_t* program);

/**
 * @brief Release a PIO engine atomically, disabling the SM and freeing all resources
 *
 * Complement to claim_pio_and_sm(). Performs the canonical teardown sequence:
 * 1. Disable the state machine
 * 2. Clear TX and RX FIFOs
 * 3. Release the SM claim
 * 4. Remove the program from instruction memory
 * 5. Reset all engine fields to the failure sentinel
 *
 * This function is safe to call when engine->pio is NULL (returns immediately).
 *
 * @param engine  Pointer to the pio_engine_t to release. Modified in place.
 * @param program The PIO program to remove from instruction memory.
 *
 * @note Main loop only.
 * @note After return, engine->pio == NULL, engine->sm == -1, engine->offset == -1
 */
void release_pio_and_sm(pio_engine_t* engine, const pio_program_t* program);

/**
 * @brief Restarts a PIO state machine at a specified program offset
 *
 * PIO State Machine Reset Sequence:
 * 1. Drain TX FIFO (prevents stale data corruption)
 * 2. Clear both FIFOs (TX and RX)
 * 3. Restart state machine (resets PC, clears OSR/ISR)
 * 4. Jump to specified offset (sets entry point)
 *
 * Use Cases:
 * - Protocol error recovery (AT/PS2 inhibit, M0110 sync errors)
 * - Clock stretching timeout recovery
 * - Re-initialisation after configuration changes
 * - Transitioning between protocol states
 *
 * State Machine Reset Behaviour:
 * - Program Counter (PC) → 0
 * - Output Shift Register (OSR) → cleared
 * - Input Shift Register (ISR) → cleared
 * - FIFOs → empty
 * - Side-set pins → retain state (caller must reset if needed)
 * - GPIO configuration → unchanged
 *
 * Thread Safety:
 * - Safe to call from IRQ context (uses only PIO register writes and LOG_* macros)
 * - Caller must ensure no concurrent access to the same SM from another context
 * - Called from both ISR error-recovery paths and main task initialisation
 *
 * @param pio    The PIO instance (pio0 or pio1)
 * @param sm     The state machine number (0-3)
 * @param offset The program offset to jump to after restart
 *
 * @note Caller must reconfigure device-specific state after calling
 * @note Debug output shows offset for troubleshooting restart logic
 * @note Does not modify clock divider or GPIO mappings
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
 *
 * @note Main loop only.
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
 * @brief Initialises the PIO IRQ dispatcher for a PIO instance
 *
 * Sets up a shared PIO IRQ handler that multiplexes between multiple protocol
 * event handlers. This enables multiple devices (e.g., AT/PS2 keyboard + mouse)
 * to coexist on the same PIO without IRQ registration conflicts.
 *
 * Initialisation Workflow:
 * 1. Check if dispatcher already initialised for this PIO (idempotent)
 * 2. Determine IRQ number (PIO0_IRQ_0 or PIO1_IRQ_0)
 * 3. Register dispatcher as exclusive IRQ handler
 * 4. Set IRQ priority to 0 (highest) for time-critical protocol timing
 * 5. Mark dispatcher as initialised (IRQ is only enabled on the first
 *    pio_irq_register_callback() call, preventing premature dispatch)
 *
 * Resource Management:
 * - Uses single exclusive handler per PIO (no conflicts)
 * - Multiplexes up to MAX_PIO_IRQ_CALLBACKS callbacks
 * - Idempotent: safe to call multiple times for same PIO
 *
 * IRQ Priority Rationale:
 * - Priority 0x00 ensures minimal keyboard/mouse event latency
 * - Critical for protocols with microsecond-level timing requirements
 * - Higher priority than USB stack (prevents input lag)
 *
 * Thread Safety:
 * - Must be called during initialisation (main context)
 * - Not safe to call from IRQ context
 * - Single-core architecture eliminates race conditions
 *
 * IMPORTANT - Per-SM IRQ Source Enablement:
 * This function does NOT call pio_set_irq0_source_enabled() for individual state machines.
 * Each protocol's .pio program init function MUST enable its SM's rxnempty IRQ source.
 * Example from at-ps2.pio init:
 *   pio_sm_set_enabled(pio, sm, false);
 *   pio_set_irq0_source_enabled(pio, pis_sm0_rx_fifo_not_empty + sm, true);
 *   pio_sm_set_enabled(pio, sm, true);
 *
 * Protocols that omit this source enable in their .pio init will silently fail to
 * receive interrupts, as this dispatcher only registers the system-level IRQ handler.
 *
 * @param pio The PIO instance (pio0 or pio1) to configure
 *
 * @note Safe to call multiple times - subsequent calls for same PIO are ignored
 * @note Sets IRQ priority to 0 (highest) for time-critical protocol timing
 * @note Must call this before pio_irq_register_callback()
 * @note Log output includes PIO number and IRQ number for debugging
 * @note Main loop only.
 */
void pio_irq_dispatcher_init(PIO pio);

/**
 * @brief Registers a callback function for PIO IRQ events
 *
 * Adds a protocol-specific IRQ handler to the dispatcher's invocation list.
 * When the PIO IRQ fires, the dispatcher calls all registered callbacks
 * sequentially. This allows multiple protocols to share the same PIO IRQ.
 *
 * Registration Process:
 * 1. Search callback array for first NULL slot
 * 2. Store callback function pointer in slot
 * 3. Log slot number for debugging
 * 4. Return success status
 *
 * Callback Invocation:
 * - Called from IRQ context (highest priority 0x00)
 * - Must be fast and non-blocking
 * - No parameters passed (callbacks access state via static variables)
 * - Invoked sequentially (not concurrent)
 *
 * Slot Management:
 * - Maximum MAX_PIO_IRQ_CALLBACKS slots (currently 4)
 * - First-come-first-served allocation
 * - NULL slots skipped during dispatch (no overhead)
 *
 * Failure Scenarios:
 * - All callback slots occupied → returns false
 * - Dispatcher not initialised → undefined behaviour (call init first)
 * - NULL callback pointer → undefined behaviour (caller must validate)
 *
 * @param callback Function pointer to the protocol IRQ handler
 * @return true if registration succeeded, false if no slots available
 *
 * @note Must call pio_irq_dispatcher_init() before registering callbacks
 * @note Callbacks invoked in IRQ context - must be fast and non-blocking
 * @note Callback must not be NULL (undefined behaviour)
 * @note Registration order affects invocation order (FIFO)
 * @note Logs slot number for debugging multi-device configurations
 * @note Duplicate callback registration prevented (returns false if already registered)
 * @note Main loop only.
 */
bool pio_irq_register_callback(pio_irq_callback_t callback);

/**
 * @brief Unregisters a callback from PIO IRQ events
 *
 * Removes a previously registered callback from the dispatcher's invocation
 * list. The callback will no longer be invoked when the PIO IRQ fires. This
 * is typically used during protocol cleanup or device disconnection.
 *
 * Removal Process:
 * 1. Search callback array for matching function pointer
 * 2. Set matching slot to NULL (removes from dispatch list)
 * 3. Log slot number for debugging
 * 4. Return immediately after removal
 *
 * Slot Reclamation:
 * - NULL slots are reused by pio_irq_register_callback()
 * - Dispatcher skips NULL slots during IRQ (no overhead)
 * - No compaction or defragmentation needed
 *
 * Safety Characteristics:
 * - Safe to call even if callback was never registered
 * - Safe to call multiple times for same callback
 * - Safe to call during protocol teardown
 * - Single-core architecture eliminates race conditions
 *
 * Error Handling:
 * - Callback not found → logs warning, continues
 * - NULL callback → searches for NULL slot (harmless)
 * - Dispatcher not initialised → safe (no-op)
 *
 * Use Cases:
 * - Protocol cleanup during shutdown
 * - Device hot-unplug support (future)
 * - Error recovery and re-initialisation
 *
 * @param callback Function pointer to remove from the dispatcher
 *
 * @note Safe to call even if callback was never registered
 * @note Logs warning if callback not found (debugging aid)
 * @note Does not disable PIO IRQ (other callbacks may still be registered)
 * @note Slot is freed for future registrations
 * @note Main loop only.
 */
void pio_irq_unregister_callback(pio_irq_callback_t callback);

#endif /* PIO_HELPER_H */
