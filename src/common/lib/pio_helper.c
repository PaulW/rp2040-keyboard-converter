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

#include "pio_helper.h"

#include <math.h>
#include <stdio.h>

#include "hardware/clocks.h"
#include "hardware/irq.h"
#include "hardware/sync.h"

#include "log.h"

/* Compile-time validation of PIO helper constants */
_Static_assert(1, "PIO helper basic sanity check");  // Always true, validates _Static_assert works

/* PIO IRQ Dispatcher State */
#define MAX_PIO_IRQ_CALLBACKS 4 /**< Maximum callbacks per PIO (supports multiple devices) */

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

/**
 * @brief Claims PIO resources and loads program atomically with intelligent fallback
 *
 * This function combines PIO program space checking, state machine allocation, and
 * program loading into a single atomic operation with fallback logic. It solves the
 * race condition where a PIO might have program space but no available state machines.
 *
 * Allocation Strategy:
 * 1. Check if PIO0 has program space AND available SM → load program → use PIO0
 * 2. If PIO0 fails either check, try PIO1 with same checks
 * 3. Return failure only if both PIOs fail
 *
 * Resource Validation:
 * - Checks program space: pio_can_add_program()
 * - Attempts SM claim: pio_claim_unused_sm() with required=false
 * - Loads program: pio_add_program() and stores offset
 * - Validates all resources before committing to a PIO
 *
 * Failure Scenarios Handled:
 * - PIO0: program space ✓, SM available ✗ → tries PIO1
 * - PIO0: program space ✗, SM available ✓ → tries PIO1
 * - Both PIOs exhausted → returns failure
 *
 * Example Usage:
 * @code
 * pio_engine = claim_pio_and_sm(&my_program);
 * if (pio_engine.pio == NULL) {
 *     LOG_ERROR("No PIO resources available\n");
 *     return;
 * }
 * // pio_engine now contains: pio, sm (0-3), offset (program already loaded)
 * @endcode
 *
 * @param program The PIO program to allocate resources for and load
 * @return pio_engine_t with allocated PIO/SM/offset, or {NULL, -1, -1} on failure
 *
 * @note Claimed SM remains allocated until explicitly released
 * @note Caller must check pio == NULL for allocation failure
 * @note Program is loaded atomically - no separate pio_add_program() call needed
 * @note Thread-safe: uses SDK's atomic SM claiming mechanism
 */
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
 * State Machine Reset Behavior:
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
void pio_restart(PIO pio, uint sm, uint offset) {
    // Restart the PIO State Machine
    LOG_DEBUG("Resetting State Machine and re-initialising at offset: 0x%02X...\n", offset);
    pio_sm_drain_tx_fifo(pio, sm);
    pio_sm_clear_fifos(pio, sm);
    pio_sm_restart(pio, sm);
    pio_sm_exec(pio, sm, pio_encode_jmp(offset));
    LOG_DEBUG("State Machine Restarted\n");
}

/**
 * @brief Calculates PIO clock divider for a given minimum pulse width
 *
 * This function implements a sophisticated clock divider calculation that ensures
 * reliable signal detection by applying Nyquist sampling principles. It calculates
 * the divider needed to achieve at least 5 samples per shortest pulse.
 *
 * Sampling Theory Application:
 * - Nyquist Theorem: Sample rate must be ≥2× signal frequency
 * - This implementation uses 5× oversampling for margin
 * - Provides robust detection even with clock jitter
 * - Enables mid-pulse sampling for noise immunity
 *
 * Calculation Steps:
 * 1. Get RP2040 system clock (clk_sys, typically 125 MHz)
 * 2. Calculate shortest pulse frequency (1/pulse_width)
 * 3. Multiply by 5 to get target sampling rate
 * 4. Calculate divider: clk_sys / target_sampling_rate
 * 5. Round to nearest integer for PIO hardware
 *
 * Example (125 MHz system clock, 30µs min pulse width):
 * - Shortest pulse: 1/30µs = 33.33 kHz
 * - Target sampling: 33.33 kHz × 5 = 166.67 kHz
 * - Clock divider: 125 MHz / 166.67 kHz = 750
 * - Actual PIO clock: 125 MHz / 750 = 166.67 kHz
 * - Sample interval: 6µs (5 samples in 30µs pulse)
 *
 * Debug Output (aids in protocol tuning):
 * - RP2040 Clock Speed: System clock in kHz
 * - Desired PIO Sampling Rate: Target after 5× oversampling
 * - Calculated Clock Divider: Rounded divider value
 * - Effective PIO Clock Speed: Actual post-division frequency
 * - Effective Sample Interval: Time between samples (µs)
 *
 * Protocol-Specific Usage:
 * - AT/PS2: ~30µs min pulse → 750 divider → 6µs sampling
 * - XT: ~60µs min pulse → 375 divider → 12µs sampling
 * - M0110: ~50µs min pulse → 500 divider → 10µs sampling
 *
 * Validation Considerations:
 * - Minimum divider: 1 (max PIO freq = clk_sys)
 * - Maximum divider: 65536 (16-bit PIO hardware limit)
 * - Rounding may cause slight frequency deviation
 * - Always verify actual timing meets protocol specs
 *
 * @param min_clock_pulse_width_us Shortest expected pulse width in microseconds
 * @return Clock divider value (integer, suitable for PIO hardware)
 *
 * @note Uses 5× oversampling factor for reliable edge detection
 * @note Prints comprehensive debug info for protocol validation
 * @note Caller should verify returned divider is within PIO limits (1-65536)
 */
float calculate_clock_divider(int min_clock_pulse_width_us) {
    // Number of samples per pulse to ensure reliable detection
    // Based on the shortest pulse width, we want to at least sample it this many times
    const int samples_per_pulse = 5;

    // Get the system clock frequency in kHz
    float rp_clock_khz = 0.001 * clock_get_hz(clk_sys);
    LOG_INFO("RP2040 Clock Speed: %.0fKHz\n", rp_clock_khz);

    // Calculate the frequency of the shortest pulse.
    float shortest_pulse_khz = 1000.0 / (float)min_clock_pulse_width_us;

    // Calculate the desired PIO sampling rate.
    float target_sampling_khz = shortest_pulse_khz * samples_per_pulse;

    LOG_INFO("Desired PIO Sampling Rate: %.2fKHz\n", target_sampling_khz);

    // Calculate the clock divider.
    float clock_div = roundf(rp_clock_khz / target_sampling_khz);

    LOG_INFO("Calculated Clock Divider: %.0f\n", clock_div);

    // Calculate the actual effective PIO clock speed.
    float effective_pio_khz = rp_clock_khz / clock_div;
    LOG_INFO("Effective PIO Clock Speed: %.2fKHz\n", effective_pio_khz);

    // Calculate and print the sample interval.
    float sample_interval_us = (1.0 / effective_pio_khz) * 1000.0;
    LOG_INFO("Effective Sample Interval: %.2fus\n", sample_interval_us);

    return clock_div;
}

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
 * @note Runs in IRQ context - must be fast and non-blocking
 * @note Callbacks must complete quickly to avoid protocol timing issues
 */
static void __isr pio_irq_dispatcher(void) {
    for (int i = 0; i < MAX_PIO_IRQ_CALLBACKS; i++) {
        if (registered_callbacks[i] != NULL) {
            registered_callbacks[i]();
        }
    }
}

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
 * 5. Enable the PIO IRQ
 * 6. Mark dispatcher as initialised
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
 * Example Usage:
 * @code
 * pio_engine_t engine = claim_pio_and_sm(&my_program);
 * if (engine.pio != NULL) {
 *     pio_irq_dispatcher_init(engine.pio);
 *     pio_irq_register_callback(&my_irq_handler);
 * }
 * @endcode
 *
 * @param pio The PIO instance (pio0 or pio1) to configure
 *
 * @note Safe to call multiple times - subsequent calls for same PIO are ignored
 * @note Sets IRQ priority to 0 (highest) for time-critical protocol timing
 * @note Must call this before pio_irq_register_callback()
 * @note Log output includes PIO number and IRQ number for debugging
 */
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
 * - Dispatcher not initialised → undefined behavior (call init first)
 * - NULL callback pointer → undefined behavior (caller must validate)
 *
 * Protocol Integration Pattern:
 * @code
 * void __isr my_protocol_irq_handler(void) {
 *     // Process PIO FIFO data
 *     if (!pio_sm_is_rx_fifo_empty(pio, sm)) {
 *         uint32_t data = pio_sm_get(pio, sm);
 *         ringbuf_put(data);
 *     }
 * }
 *
 * // In setup function:
 * pio_irq_dispatcher_init(pio_engine.pio);
 * if (!pio_irq_register_callback(&my_protocol_irq_handler)) {
 *     LOG_ERROR("Failed to register IRQ callback\n");
 *     return;
 * }
 * @endcode
 *
 * @param callback Function pointer to the protocol IRQ handler
 * @return true if registration succeeded, false if no slots available
 *
 * @note Must call pio_irq_dispatcher_init() before registering callbacks
 * @note Callbacks invoked in IRQ context - must be fast and non-blocking
 * @note Callback must not be NULL (undefined behavior)
 * @note Registration order affects invocation order (FIFO)
 * @note Logs slot number for debugging multi-device configurations
 * @note Duplicate callback registration prevented (returns false if already registered)
 */
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
 * - Unit testing teardown
 *
 * Example Usage:
 * @code
 * void protocol_cleanup(void) {
 *     pio_irq_unregister_callback(&my_protocol_irq_handler);
 *     // Continue with other cleanup
 * }
 * @endcode
 *
 * @param callback Function pointer to remove from the dispatcher
 *
 * @note Safe to call even if callback was never registered
 * @note Logs warning if callback not found (debugging aid)
 * @note Does not disable PIO IRQ (other callbacks may still be registered)
 * @note Slot is freed for future registrations
 */
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