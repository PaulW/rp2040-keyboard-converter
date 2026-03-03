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
 * @file pio_helper.c
 * @brief PIO state machine management and clock divider calculation implementation.
 *
 * @see pio_helper.h for the full public API documentation.
 */

#include "pio_helper.h"

#include <math.h>
#include <stdbool.h>

#include "hardware/clocks.h"
#include "hardware/irq.h"
#include "hardware/sync.h"

#include "log.h"

// Compile-time validation of PIO helper constants
_Static_assert(1, "PIO helper basic sanity check");  // Always true, validates _Static_assert works

// PIO IRQ Dispatcher State
#define MAX_PIO_IRQ_CALLBACKS 4 /**< Maximum callbacks per PIO (supports multiple devices) */

// Unit-conversion scaling factors used in clock divider calculation
#define HZ_TO_KHZ  0.001F  /**< Hz to kHz: divide by 1000 */
#define US_PER_KHZ 1000.0F /**< µs per kHz period: 1000µs / 1kHz = 1ms */

static volatile pio_irq_callback_t registered_callbacks[MAX_PIO_IRQ_CALLBACKS] = {NULL, NULL, NULL,
                                                                                  NULL};
static bool dispatcher_initialised = false;  // Init-only flag, not IRQ-shared
static PIO  active_pio             = NULL;   // Init-only storage, not IRQ-shared

/**
 * IMPORTANT LIMITATION: Single PIO Instance per Session
 *
 * The dispatcher maintains single-instance state (active_pio, registered_callbacks).
 * Calling pio_irq_dispatcher_init() with a different PIO instance overwrites active_pio,
 * causing both PIO0_IRQ_0 and PIO1_IRQ_0 handlers to dispatch from the same callback array.
 *
 * This is architecturally safe because:
 * - Only one protocol loads at runtime (XT, AT/PS2, Amiga, or Apple M0110)
 * - All devices in that protocol share the same pio_engine.pio instance
 * - Mouse (if enabled) shares the keyboard's PIO via the dispatcher
 *
 * If future requirements demand concurrent multi-protocol operation, the dispatcher
 * must be extended to maintain per-PIO callback arrays.
 */

// --- Private Functions ---

/**
 * @brief PIO IRQ Dispatcher Handler
 *
 * Multiplexes PIO IRQ events between multiple protocol handlers. Iterates through
 * all registered callbacks and invokes them. This allows multiple devices (e.g.,
 * keyboard and mouse on AT/PS2 protocol) to share the same PIO IRQ without conflicts.
 *
 * Execution Model:
 * - Runs in IRQ context (highest priority 0x00)
 * - Invokes callbacks sequentially (not concurrent)
 * - NULL callbacks are skipped (no overhead)
 * - No locking needed (single-core architecture)
 *
 * Performance Characteristics:
 * - Minimal overhead: NULL checks only
 * - Deterministic: fixed iteration count (MAX_PIO_IRQ_CALLBACKS)
 * - Non-blocking: callbacks must be fast
 *
 * @note Runs in IRQ context — must be fast and non-blocking.
 * @note Callbacks must complete quickly to avoid protocol timing issues.
 */
static void __isr pio_irq_dispatcher(void) {
    for (int i = 0; i < MAX_PIO_IRQ_CALLBACKS; i++) {
        pio_irq_callback_t cb = registered_callbacks[i];  // Single volatile read
        if (cb != NULL) {
            cb();
        }
    }
}

// --- Public Functions ---

pio_engine_t claim_pio_and_sm(const pio_program_t* program) {
    pio_engine_t result = {NULL, -1, -1};

    // Try PIO0 first: check program space AND SM availability
    if (pio_can_add_program(pio0, program)) {
        int sm = pio_claim_unused_sm(pio0, false);
        if (sm >= 0) {
            result.pio    = pio0;
            result.sm     = sm;
            result.offset = pio_add_program(pio0, program);
            return result;
        }
        LOG_WARN("PIO0 has program space but no available state machines\n");
    } else {
        LOG_WARN("PIO0 has no space for PIO program\n");
    }

    // PIO0 failed, try PIO1: check program space AND SM availability
    if (pio_can_add_program(pio1, program)) {
        int sm = pio_claim_unused_sm(pio1, false);
        if (sm >= 0) {
            result.pio    = pio1;
            result.sm     = sm;
            result.offset = pio_add_program(pio1, program);
            return result;
        }
        LOG_WARN("PIO1 has program space but no available state machines\n");
    } else {
        LOG_WARN("PIO1 has no space for PIO program\n");
    }

    // Both PIOs exhausted
    LOG_ERROR("No PIO resources available (both PIO0 and PIO1 exhausted)\n");
    return result;
}

void release_pio_and_sm(pio_engine_t* engine, const pio_program_t* program) {
    if (engine->pio == NULL) {
        return;
    }
    pio_sm_set_enabled(engine->pio, (uint)engine->sm, false);
    pio_sm_clear_fifos(engine->pio, (uint)engine->sm);
    pio_sm_unclaim(engine->pio, (uint)engine->sm);
    pio_remove_program(engine->pio, program, engine->offset);
    engine->pio    = NULL;
    engine->sm     = -1;
    engine->offset = -1;
}

void pio_restart(PIO pio, uint sm, uint offset) {
    // Restart the PIO State Machine
    LOG_DEBUG("Resetting State Machine and re-initialising at offset: 0x%02X...\n", offset);
    pio_sm_drain_tx_fifo(pio, sm);
    pio_sm_clear_fifos(pio, sm);
    pio_sm_restart(pio, sm);
    pio_sm_exec(pio, sm, pio_encode_jmp(offset));
    LOG_DEBUG("State Machine Restarted\n");
}

float calculate_clock_divider(int min_clock_pulse_width_us) {
    // Number of samples per pulse to ensure reliable detection
    // Based on the shortest pulse width, we want to at least sample it this many times
    const int samples_per_pulse = 5;

    // Get the system clock frequency in kHz
    float rp_clock_khz = HZ_TO_KHZ * (float)clock_get_hz(clk_sys);
    LOG_INFO("RP2040 Clock Speed: %.0fKHz\n", rp_clock_khz);

    // Calculate the frequency of the shortest pulse.
    float shortest_pulse_khz = US_PER_KHZ / (float)min_clock_pulse_width_us;

    // Calculate the desired PIO sampling rate.
    float target_sampling_khz = shortest_pulse_khz * (float)samples_per_pulse;

    LOG_INFO("Desired PIO Sampling Rate: %.2fKHz\n", target_sampling_khz);

    // Calculate the clock divider.
    float clock_div = roundf(rp_clock_khz / target_sampling_khz);

    LOG_INFO("Calculated Clock Divider: %.0f\n", clock_div);

    // Calculate the actual effective PIO clock speed.
    float effective_pio_khz = rp_clock_khz / clock_div;
    LOG_INFO("Effective PIO Clock Speed: %.2fKHz\n", effective_pio_khz);

    // Calculate and print the sample interval.
    float sample_interval_us = (1.0F / effective_pio_khz) * US_PER_KHZ;
    LOG_INFO("Effective Sample Interval: %.2fus\n", sample_interval_us);

    return clock_div;
}

void pio_irq_dispatcher_init(PIO pio) {
    // Prevent re-initialisation for the same PIO instance
    if (dispatcher_initialised && active_pio == pio) {
        return;
    }

    // Prevent conflicting PIO: dispatcher supports only one PIO instance per session
    if (dispatcher_initialised && active_pio != pio) {
        LOG_ERROR("PIO IRQ dispatcher already initialised for PIO%d, cannot switch to PIO%d\n",
                  active_pio == pio0 ? 0 : 1, pio == pio0 ? 0 : 1);
        return;
    }

    active_pio   = pio;
    uint pio_irq = pio == pio0 ? PIO0_IRQ_0 : PIO1_IRQ_0;

    // Register the dispatcher as the exclusive handler
    irq_set_exclusive_handler(pio_irq, &pio_irq_dispatcher);

    // Set highest priority for time-critical protocol timing
    irq_set_priority(pio_irq, 0x00);

    // IRQ enabled after first callback registered to avoid race
    // (prevents IRQ from observing uninitialised callback array)

    dispatcher_initialised = true;

    LOG_INFO("PIO IRQ dispatcher initialised for PIO%d (IRQ %d, priority 0x00)\n",
             pio == pio0 ? 0 : 1, pio_irq);
}

bool pio_irq_register_callback(pio_irq_callback_t callback) {
    // Check for duplicate registration (prevent double-invocation)
    for (int i = 0; i < MAX_PIO_IRQ_CALLBACKS; i++) {
        if (registered_callbacks[i] == callback) {
            LOG_WARN("PIO IRQ callback already registered at slot %d (duplicate prevented)\n", i);
            return false;  // Callback already registered
        }
    }

    // Find first available slot
    for (int i = 0; i < MAX_PIO_IRQ_CALLBACKS; i++) {
        if (registered_callbacks[i] == NULL) {
            registered_callbacks[i] = callback;
            __dmb();  // Memory barrier: ensure callback write visible to IRQ before enabling

            // Enable IRQ after first callback registered (race prevention)
            if (dispatcher_initialised) {
                uint pio_irq = active_pio == pio0 ? PIO0_IRQ_0 : PIO1_IRQ_0;
                if (!irq_is_enabled(pio_irq)) {
                    irq_set_enabled(pio_irq, true);
                    LOG_DEBUG("PIO IRQ enabled (first callback registered at slot %d)\n", i);
                } else {
                    LOG_DEBUG("Registered PIO IRQ callback at slot %d\n", i);
                }
            }
            return true;
        }
    }
    LOG_ERROR("Failed to register PIO IRQ callback: all %d slots occupied\n",
              MAX_PIO_IRQ_CALLBACKS);
    return false;  // No free slots
}

void pio_irq_unregister_callback(pio_irq_callback_t callback) {
    for (int i = 0; i < MAX_PIO_IRQ_CALLBACKS; i++) {
        if (registered_callbacks[i] == callback) {
            registered_callbacks[i] = NULL;
            __dmb();  // Memory barrier: ensure NULL visible to IRQ before caller proceeds
            LOG_DEBUG("Unregistered PIO IRQ callback from slot %d\n", i);
            return;
        }
    }
    LOG_WARN("PIO IRQ callback not found during unregister\n");
}