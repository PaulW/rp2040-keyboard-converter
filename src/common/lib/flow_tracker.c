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
 * @file flow_tracker.c
 * @brief Pipeline flow tracking implementation.
 *
 * The entire body of this file is conditional on FLOW_TRACKING_ENABLED.
 * When that macro is 0 (the default) the translation unit compiles to
 * nothing and has zero impact on the build.
 */

#include "flow_tracker.h"

#if FLOW_TRACKING_ENABLED

#include <stdbool.h>
#include <stdint.h>

#include "hardware/sync.h"
#include "hardware/timer.h"

#include "log.h"

_Static_assert(FLOW_TOKEN_QUEUE_SIZE <= 256U,
               "FLOW_TOKEN_QUEUE_SIZE must fit uint8_t head/tail indices");

// --- Token queue (SPSC: ISR producer / main-loop consumer) ---

/**
 * @brief Backing store for the lock-free token queue.
 *
 * Volatile because both ISR and main-loop contexts touch different fields
 * of the same array; the compiler must not cache these reads/writes.
 */
static volatile flow_token_t token_buf[FLOW_TOKEN_QUEUE_SIZE];

/** Written exclusively by the ISR. */
static volatile uint8_t token_head = 0;

/** Written exclusively by the main loop. */
static volatile uint8_t token_tail = 0;

/** Monotonic counter; incremented by the ISR for each successful push. */
static volatile uint16_t run_id_ctr = 0;

// --- Active flow state (main-loop only) ---

static active_flow_t active_flow;
static bool          flow_active = false;

/**
 * @brief Runtime enable switch.  Volatile because it is read from ISR context.
 *
 * Default is false — tracking must be explicitly enabled at runtime via
 * flow_tracker_set_enabled().  This allows the feature to be compiled in
 * (FLOW_TRACKING_ENABLED=1) but remain dormant until the operator
 * activates it via Command Mode.
 */
static volatile bool flow_tracking_runtime_enabled = false;

// --- Public Functions ---

void flow_tracker_init(void) {
    token_head                    = 0;
    token_tail                    = 0;
    run_id_ctr                    = 0;
    flow_active                   = false;
    flow_tracking_runtime_enabled = false;
    active_flow.step_count        = 0;
}

void isr_push_flow_token(const char* isr_func, uint8_t rx_byte) {
    if (!flow_tracking_runtime_enabled) {
        return;  // Tracking disabled at runtime — nothing to queue
    }
    uint8_t next_head = (token_head + 1U) & (FLOW_TOKEN_QUEUE_SIZE - 1U);
    if (next_head == token_tail) {
        return;  // Queue full — drop without blocking
    }

    token_buf[token_head].rx_byte      = rx_byte;
    token_buf[token_head].run_id       = run_id_ctr++;
    token_buf[token_head].isr_start_us = timer_hw->timerawl;
    token_buf[token_head].isr_func     = isr_func;
    __dmb();  // Ensure all token fields written before head advances
    token_head = next_head;
}

bool main_pop_flow_token(flow_token_t* out_token) {
    if (token_head == token_tail) {
        return false;  // Queue empty
    }
    __dmb();  // Ensure token_head advance from ISR is visible before reading

    out_token->rx_byte      = token_buf[token_tail].rx_byte;
    out_token->run_id       = token_buf[token_tail].run_id;
    out_token->isr_start_us = token_buf[token_tail].isr_start_us;
    out_token->isr_func     = token_buf[token_tail].isr_func;

    __dmb();  // Ensure read completes before tail advances
    token_tail = (token_tail + 1U) & (FLOW_TOKEN_QUEUE_SIZE - 1U);
    return true;
}

void flow_start(const flow_token_t* token) {
    active_flow.run_id     = token->run_id;
    active_flow.step_count = 0;
    flow_active            = true;

    // Step 0: ISR entry — captured at the moment isr_push_flow_token() was called
    active_flow.steps[0].timestamp_us = token->isr_start_us;
    active_flow.steps[0].func_name    = token->isr_func;
    active_flow.steps[0].data_val     = token->rx_byte;
    active_flow.step_count            = 1;
}

void flow_tracker_record_step(const char* func_name, uint32_t data_val) {
    if (!flow_active || active_flow.step_count >= FLOW_MAX_STEPS) {
        return;
    }
    uint8_t idx                         = active_flow.step_count++;
    active_flow.steps[idx].timestamp_us = timer_hw->timerawl;
    active_flow.steps[idx].func_name    = func_name;
    active_flow.steps[idx].data_val     = data_val;
}

void flow_tracker_record_end(const char* func_name, uint32_t data_val) {
    flow_tracker_record_step(func_name, data_val);
    if (!flow_active) {
        return;
    }

    LOG_DEBUG("[FLOW] run=%u\n", (unsigned)active_flow.run_id);
    for (uint8_t i = 0; i < active_flow.step_count; i++) {
        const flow_step_t* s = &active_flow.steps[i];
        LOG_DEBUG("  [%u] time=%lu  func=%s  data=0x%02X\n", (unsigned)i,
                  (unsigned long)s->timestamp_us, s->func_name, (unsigned)s->data_val);
    }

    flow_active = false;
}

void flow_tracker_set_enabled(bool enable) {
    flow_tracking_runtime_enabled = enable;
    __dmb();  // Publish updated runtime flag before continuing
    if (!enable) {
        uint32_t irq_state = save_and_disable_interrupts();
        token_tail         = token_head;  // Drop queued tokens immediately
        restore_interrupts(irq_state);
        flow_active = false;  // Discard any incomplete flow immediately
    }
}

bool flow_tracker_is_enabled(void) {
    return flow_tracking_runtime_enabled;
}

#endif /* FLOW_TRACKING_ENABLED */