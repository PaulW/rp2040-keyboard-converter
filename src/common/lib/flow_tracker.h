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
 * @file flow_tracker.h
 * @brief Pipeline flow tracking for keyboard event latency analysis.
 *
 * Instruments the path a scancode takes from the moment the ISR fires
 * through the ring buffer, scancode processor, and HID interface until
 * the USB HID report is sent.  Each step is timestamped in microseconds
 * using the RP2040 hardware timer; the completed trace is emitted via
 * the existing LOG_DEBUG channel when the HID report is dispatched.
 *
 * Usage
 * -----
 * 1. Set FLOW_TRACKING_ENABLED to 1 in config.h.
 * 2. In the ISR path: call isr_push_flow_token(__func__, data_byte) immediately
 *    after a successful ringbuf_put().
 * 3. In the main loop: call main_pop_flow_token() / flow_start() immediately
 *    after ringbuf_get().
 * 4. In each intermediate function: add FLOW_STEP(data) as the first statement.
 * 5. At the HID send point: add FLOW_END(data) immediately before the send.
 *
 * Concurrency model
 * -----------------
 * The token queue is a lock-free single-producer / single-consumer ring buffer
 * that mirrors the existing ring buffer.  ISR is the sole producer (writes
 * token_head); the main loop is the sole consumer (writes token_tail).  Both
 * queues must be kept in strict lockstep: push a token only on a successful
 * ringbuf_put(), pop a token only immediately after a successful ringbuf_get().
 *
 * When FLOW_TRACKING_ENABLED is 0 (the default), every API call expands to
 * an empty inline stub or a void cast, producing no code at optimisation
 * level -O2 or higher.
 */

#ifndef FLOW_TRACKER_H
#define FLOW_TRACKER_H

#include <stdbool.h>
#include <stdint.h>

#include "config.h"

/**
 * @brief Master enable switch.
 *
 * Set FLOW_TRACKING_ENABLED in config.h (the canonical location).
 * 0 (default) — all instrumentation compiled out; zero runtime overhead.
 * 1           — flow tracking compiled in; toggle at runtime via Command Mode 'T'.
 *
 * This #ifndef acts as a fallback only — it is not normally exercised because
 * config.h (included above) always defines the value.
 */
#ifndef FLOW_TRACKING_ENABLED
#define FLOW_TRACKING_ENABLED 0
#endif

// --- Sizing constants --------------------------------------------------

/**
 * @brief Token queue capacity.  Must be a power of 2 and match the ring
 *        buffer size so that the two queues remain in lockstep.
 */
#define FLOW_TOKEN_QUEUE_SIZE 32

_Static_assert((FLOW_TOKEN_QUEUE_SIZE & (FLOW_TOKEN_QUEUE_SIZE - 1U)) == 0U,
               "FLOW_TOKEN_QUEUE_SIZE must be a power of 2");

/** @brief Maximum pipeline steps recorded per flow event. */
#define FLOW_MAX_STEPS 8

// --- Data structures ---------------------------------------------------

/**
 * @brief A single timed step in the processing pipeline.
 */
typedef struct {
    uint32_t    timestamp_us; /**< Microseconds since boot (timer_hw->timerawl) */
    const char* func_name;    /**< Capturing function name via __func__ */
    uint32_t    data_val;     /**< Call-site payload: scancode, keycode, report ID, etc. */
} flow_step_t;

/**
 * @brief Active flow state.  Only accessed from main-loop context.
 *
 * Tracks a single in-progress pipeline event from ISR entry through to the
 * HID report send.  There is at most one active flow at any time.
 */
typedef struct {
    uint16_t run_id;     /**< Run ID copied from the triggering flow_token_t */
    uint8_t  step_count; /**< Number of steps recorded so far */
    flow_step_t
        steps[FLOW_MAX_STEPS]; /**< Per-step timestamp and metadata; indexed 0..step_count-1 */
} active_flow_t;

/**
 * @brief Token pushed from ISR context and consumed by the main loop.
 *
 * Carries the raw scancode byte, a monotonic run ID that ties this ISR event
 * to its downstream main-loop flow, the microsecond timestamp at ISR entry,
 * and the ISR function name captured via __func__ at the push site.
 */
typedef struct {
    uint8_t     rx_byte;      /**< Scancode byte queued to the ring buffer */
    uint16_t    run_id;       /**< Monotonic counter; unique per ring-buffer entry */
    uint32_t    isr_start_us; /**< timer_hw->timerawl at the moment of ISR entry */
    const char* isr_func;     /**< __func__ at isr_push_flow_token() call site */
} flow_token_t;

// --- Public API --------------------------------------------------------

#if FLOW_TRACKING_ENABLED

/**
 * @brief Initialise the flow tracker.  Call once from main before any tasks.
 *
 * Resets queue pointers, clears the active-flow state, and ensures that
 * runtime tracking starts in the disabled state.
 *
 * @note Main loop only.
 * @note Non-blocking — completes in constant time.
 */
void flow_tracker_init(void);

/**
 * @brief Push a new tracking token from ISR context.
 *
 * Must be called immediately after a successful ringbuf_put() so that the
 * token queue stays in lockstep with the ring buffer.  If the token queue
 * is full (which mirrors a full ring buffer) the push is silently dropped.
 *
 * @param isr_func  Pass __func__ from the ISR call site.
 * @param rx_byte   The byte just written to the ring buffer.
 * @note ISR context only.
 * @note Non-blocking — drops the token silently if the queue is full; never
 *       spins or waits.
 */
void isr_push_flow_token(const char* isr_func, uint8_t rx_byte);

/**
 * @brief Pop the next token in main-loop context.
 *
 * Must be called immediately after a successful ringbuf_get() to maintain
 * lockstep with the ring buffer.
 *
 * @param out_token Receives the next token.
 * @return true if a token was available; false if the queue was empty.
 * @note Main loop only.
 * @note Non-blocking — returns immediately if no token is available.
 */
bool main_pop_flow_token(flow_token_t* out_token);

/**
 * @brief Start tracking a new flow for the just-popped token.
 *
 * Records the ISR function name, timestamp, and raw byte (from the token)
 * as step 0 of the new flow.  Any previously incomplete flow is discarded.
 *
 * @param token  Token returned by main_pop_flow_token().
 * @note Main loop only.
 * @note Non-blocking — completes in constant time.
 */
void flow_start(const flow_token_t* token);

/**
 * @brief Internal: append a mid-pipeline step.  Use the FLOW_STEP() macro.
 *
 * Appends a mid-pipeline step to the active flow.
 * No-op if no flow is active or if the step array is full.
 *
 * @param func_name  Caller's __func__ string; captured automatically by FLOW_STEP.
 * @param data_val   Payload at this pipeline stage (scancode, keycode, etc.).
 * @note Main loop only.
 * @note Non-blocking — no-op if no flow is active or the step array is full.
 */
void flow_tracker_record_step(const char* func_name, uint32_t data_val);

/**
 * @brief Internal: append the final step and emit the completed flow log.
 *        Use the FLOW_END() macro.
 *
 * Emits one LOG_DEBUG line per step in the format:
 *   [FLOW] run=<id>
 *     [0] time=<us>  func=<name>  data=0x<hex>
 *     [1] ...
 *
 * Performance Characteristics:
 * - Tracking disabled:                   zero overhead (isr_push_flow_token returns immediately)
 * - Tracking enabled, no active flow:    ~0µs (break codes / repeats never start a flow)
 * - Tracking enabled, typical make key:  ~8µs end-to-end, ISR entry to USB report send
 * - Command Mode evaluation path:        ~24µs (to_ms_since_boot() + LOG_INFO in
 *                                        evaluate_command_mode() — well within USB budget)
 *
 * @param func_name  Caller's __func__ string; captured automatically by FLOW_END.
 * @param data_val   Payload at the final stage (e.g. HID report ID).
 * @note Main loop only.
 * @note Non-blocking — trace output is sent via the DMA-backed UART channel.
 */
void flow_tracker_record_end(const char* func_name, uint32_t data_val);

/**
 * @brief Enable or disable runtime flow tracking.
 *
 * When @p enable is true the ISR will begin pushing tokens and the main
 * loop will record pipeline steps.  When false the ISR stops pushing
 * immediately (in-flight tokens already in the queue are drained silently).
 *
 * The caller is responsible for ensuring the log level is at least
 * LOG_LEVEL_DEBUG before enabling — trace output uses LOG_DEBUG.
 *
 * @param enable true to start tracking; false to stop.
 * @note Main loop only.
 */
void flow_tracker_set_enabled(bool enable);

/**
 * @brief Return true if runtime flow tracking is currently active.
 *
 * @return true if tracking is enabled; false if disabled.
 * @note Safe to call from any context (reads a volatile bool).
 */
bool flow_tracker_is_enabled(void);

/**
 * @brief Record a mid-pipeline checkpoint.
 *
 * Place as the first statement in any function you want to instrument.
 * Has no effect if no flow is currently active (e.g. break codes that
 * do not produce a HID report).
 *
 * @param data_val  A uint32_t-compatible value representing the data at
 *                  this point in the pipeline (scancode, keycode, etc.).
 */
#define FLOW_STEP(data_val) flow_tracker_record_step(__func__, (uint32_t)(data_val))

/**
 * @brief Record the final pipeline step and emit the flow trace log.
 *
 * Place immediately before the HID report send call.  After this macro
 * the active flow is cleared; subsequent FLOW_STEP calls are no-ops until
 * the next flow_start().
 *
 * @param data_val  A uint32_t-compatible value (e.g. HID report ID).
 */
#define FLOW_END(data_val) flow_tracker_record_end(__func__, (uint32_t)(data_val))

#else /* FLOW_TRACKING_ENABLED == 0 — compile out entirely */

static inline void flow_tracker_init(void) {
}
static inline void isr_push_flow_token(const char* isr_func, uint8_t rx_byte) {
    (void)isr_func;
    (void)rx_byte;
}
static inline bool main_pop_flow_token(flow_token_t* out_token) {
    (void)out_token;
    return false;
}
static inline void flow_start(const flow_token_t* token) {
    (void)token;
}
static inline void flow_tracker_set_enabled(bool enable) {
    (void)enable;
}
static inline bool flow_tracker_is_enabled(void) {
    return false;
}

#define FLOW_STEP(data_val) ((void)(data_val))
#define FLOW_END(data_val)  ((void)(data_val))

#endif /* FLOW_TRACKING_ENABLED */

#endif /* FLOW_TRACKER_H */