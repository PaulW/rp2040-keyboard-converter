/*
 * This file is part of RP2040 Keyboard Converter.
 *
 * Copyright 2023 Paul Bramhall (paulwamp@gmail.com)
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
 * @file uart.c
 * @brief High-Performance DMA-Based UART Logging Implementation
 * 
 * This file implements a non-blocking, DMA-driven UART logging system designed
 * to provide high-performance debug output without interfering with real-time
 * operations such as keyboard protocol timing or USB HID communication.
 * 
 * Key Features:
 * - **Configurable queue policies**: DROP, WAIT_FIXED, or WAIT_EXP behavior when queue full
 * - **Large circular buffer queue**: 64-entry buffer handles initialization bursts without loss
 * - **DMA-driven transmission**: Minimal CPU overhead during output
 * - **stdio integration**: Works transparently with printf(), puts(), etc.
 * - **Thread-safe multi-producer**: Lock-free atomic operations prevent races
 * - **IRQ-aware policies**: Never blocks in interrupt context
 * - **Compile-time optimization**: Zero runtime overhead for policy selection
 * - **Low interrupt priority**: Designed to not interfere with PIO state machines
 * 
 * Architecture:
 * The system uses stdio integration with a thread-safe circular buffer queue:
 * 
 * ```
 * Application Code
 *        ↓
 * printf(), puts(), etc. (Standard C Library)
 *        ↓
 * Pico SDK stdio system
 *        ↓  
 * my_out_chars() (stdio driver hook)
 *        ↓
 * uart_dma_write_raw() → try_reserve_slot() (atomic CAS)
 *        ↓
 * Queue Entry → DMA Controller → UART Hardware
 * ```
 * 
 * **stdio Integration**: All standard C library functions (printf, puts, etc.)
 * are redirected through the Pico SDK stdio system to our DMA-based implementation.
 * This provides:
 *    - Transparent replacement of standard UART stdio
 *    - No application code changes required
 *    - Direct memcpy() of pre-formatted strings to queue buffers
 *    - Maximum efficiency since formatting is handled by Pico SDK
 * 
 * When a message is queued, if the DMA controller is idle, transmission begins
 * immediately. The DMA interrupt handler automatically starts subsequent queued
 * messages, providing continuous transmission without CPU intervention.
 * 
 * Performance Optimizations:
 * - **stdio integration**: Direct memcpy() of pre-formatted strings bypasses additional formatting
 * - **Queue size**: 64 entries (power of 2) for handling initialization bursts
 * - **Buffer alignment**: 4-byte aligned for optimal DMA performance
 * - **Bitwise operations**: Uses AND instead of modulo for index calculations
 * - **Atomic operations**: Lock-free multi-producer safety via CAS operations
 * - **Low priority**: DMA and IRQ configured to not interfere with timing-critical code
 * - **Configurable queue policies**: Compile-time selection eliminates runtime overhead
 * - **Exponential backoff**: CPU-friendly waiting with progressive delays (WAIT_EXP policy)
 * - **IRQ-aware behavior**: Policies adapt to interrupt context automatically
 * 
 * Thread Safety:
 * The implementation is designed to be safely called from any context, including
 * interrupts, without the need for explicit locking. This is achieved through:
 * - **Lock-free circular buffer design** with atomic slot reservation
 * - **Compare-and-swap operations** for multi-producer safety
 * - **Acquire/release semantics** for proper memory ordering
 * - **DMA hardware handling** the actual transmission
 * - **IRQ context detection** that adapts policy behavior appropriately
 * 
 * Integration:
 * This UART implementation completely replaces the standard Pico SDK UART
 * stdio driver, providing a drop-in replacement that works with all standard
 * C library functions while offering superior performance characteristics:
 * 
 * - **stdio functions** (printf, puts, putchar, etc.): Automatically use DMA transmission
 * - **Transparent operation**: No code changes required for existing printf usage
 * - **Performance gains**: Non-blocking DMA transmission with configurable queue policies
 * - **Real-time safe**: Configurable behavior from immediate drop to CPU-friendly backoff
 */

#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/dma.h"
#include "hardware/structs/scb.h"   // for scb_hw->icsr
#include "hardware/regs/m0plus.h"   // for M0PLUS_ICSR_VECTACTIVE_BITS
#include "pico/stdio/driver.h"
#include "pico/time.h"

#include "uart.h"

#include <stdio.h>
#include <string.h>

/**
 * @brief UART Hardware Configuration
 * 
 * Uses UART0 which corresponds to GPIO pins defined in config.h
 * (typically GP0=TX, GP1=RX, though only TX is used for logging)
 */
#define UART_ID uart0

/**
 * @brief Performance and Buffer Configuration
 * 
 * These parameters are read from config.h and tuned for optimal performance
 * while maintaining compatibility with real-time keyboard protocol operations.
 * 
 * Configuration options:
 * - UART_DMA_BUFFER_SIZE: Maximum message length (typically 256 bytes)
 * - UART_DMA_QUEUE_SIZE: Number of queued messages (must be power of 2)
 */
#define PRINTF_BUF_SIZE  UART_DMA_BUFFER_SIZE   /**< Size of each individual log message buffer (bytes) */
#define PRINTF_QUEUE_LEN UART_DMA_QUEUE_SIZE    /**< Number of queued log messages (power of 2 for efficient indexing) */

// --- Sanity check: queue length must be power of 2 ---
#if (PRINTF_QUEUE_LEN & (PRINTF_QUEUE_LEN - 1)) != 0
#  error "PRINTF_QUEUE_LEN must be a power of 2"
#endif

// Compile-time bounds (uint8_t indices, uint16_t lengths)
_Static_assert(PRINTF_QUEUE_LEN <= 256, "PRINTF_QUEUE_LEN must fit uint8_t index");
_Static_assert(PRINTF_BUF_SIZE  <= 65535, "PRINTF_BUF_SIZE must fit uint16_t len");

// --- Queue full policy configuration ---
// Policy constants and configuration are now defined in config.h
// This ensures consistent configuration across the entire project

/**
 * @brief Log Entry Structure
 * 
 * Each log entry contains the formatted message and its length.
 * The length field allows for efficient DMA transfers of variable-length
 * messages without transmitting unused buffer space.
 */
typedef struct {
    char buf[PRINTF_BUF_SIZE];   /**< Formatted message buffer */
    uint16_t len;                /**< Actual length of formatted message */
} log_entry_t;

/**
 * @brief Circular Buffer Queue and State Management
 * 
 * The queue is implemented as a circular buffer with separate head and tail
 * indices. The alignment attribute ensures optimal DMA transfer performance.
 */
static log_entry_t log_queue[PRINTF_QUEUE_LEN] __attribute__((aligned(4))); /**< Message queue (4-byte aligned for DMA) */
static volatile uint8_t q_head = 0;        /**< Queue head index (next write position) */
static volatile uint8_t q_tail = 0;        /**< Queue tail index (next read position) */
static volatile bool dma_active = false;   /**< DMA transfer active flag */
static int uart_dma_chan;                  /**< DMA channel number allocated for UART TX */
static bool uart_dma_inited = false;       /**< One-time init guard */

#ifdef UART_DMA_DEBUG_STATS
/**
 * @brief Debug Statistics Counters
 * 
 * These counters track UART DMA operation statistics when UART_DMA_DEBUG_STATS
 * is defined in config.h. Statistics are used to identify queue sizing issues
 * and monitor system behavior under load.
 */
static volatile uint32_t stats_enqueued = 0;    /**< Total messages successfully enqueued */
static volatile uint32_t stats_dropped = 0;     /**< Total messages dropped (queue full) */
static volatile uint32_t stats_last_reported = 0; /**< Last reported drop count */
#endif

/**
 * @brief Queue Management Helper Functions
 * 
 * These inline functions provide efficient queue state checking using
 * optimized operations for power-of-2 queue sizes.
 */

/**
 * Check whether the UART DMA log queue has no pending entries.
 *
 * @return `true` if the queue contains no pending messages, `false` otherwise.
 */
static inline bool queue_empty() {
    return q_head == q_tail;
}

/**
 * Check whether the log queue has no free slots.
 *
 * @returns `true` if adding one more entry would overwrite unread entries, `false` otherwise.
 */
static inline bool queue_full() {
    return ((q_head + 1) & (PRINTF_QUEUE_LEN - 1)) == q_tail;
}

/**
 * Detect whether the current core is executing inside an interrupt or exception.
 *
 * Used by queue policy code to avoid blocking operations when invoked from ISR context.
 *
 * @return `true` if executing in interrupt/exception context, `false` otherwise.
 */
static inline bool in_irq(void) {
    return (scb_hw->icsr & M0PLUS_ICSR_VECTACTIVE_BITS) != 0;
}

#ifdef UART_DMA_DEBUG_STATS
/**
 * @brief Report drop statistics when threshold reached
 * 
 * This function reports UART message drop statistics in an event-triggered manner.
 * It only outputs when new drops have occurred, preventing log spam while still
 * providing visibility into queue overflow issues.
 * 
 * The function reports every 10 drops to balance between responsiveness and
 * avoiding excessive logging (which could itself cause more drops).
 * 
 * Statistics Format:
 * - Total drops: Cumulative count since boot
 * - Enqueued: Total successful enqueue operations
 * - Drop rate: Percentage of messages dropped
 * 
 * @note Called automatically from uart_dma_write_raw() when drops occur
 * @note Uses static tracking to avoid reporting unchanged statistics
 * @note Safe to call from any context (uses existing printf infrastructure)
 */
static inline void report_drop_stats(void) {
    uint32_t current_drops = stats_dropped;
    
    // Report every 10 drops to balance visibility vs log spam
    if (current_drops > stats_last_reported && (current_drops % 10 == 0)) {
        uint32_t total = stats_enqueued + stats_dropped;
        uint32_t drop_pct = (total > 0) ? (stats_dropped * 100 / total) : 0;
        
        printf("[UART Stats] Dropped: %lu, Enqueued: %lu, Drop rate: %lu%%\n",
               (unsigned long)stats_dropped,
               (unsigned long)stats_enqueued, 
               (unsigned long)drop_pct);
        
        stats_last_reported = current_drops;
    }
}
#endif

/**
 * Initiates a DMA transmission for the next ready log entry if one is available.
 *
 * If the queue is non-empty, no DMA transfer is already active, and the DMA
 * channel is not busy, this function marks the DMA as active and arms the DMA
 * to transfer the published length of the message at the queue tail to the
 * UART TX register. If the tail entry's `len` is zero (not yet published) or
 * any precondition fails, the function returns without starting a transfer.
 *
 * Side effects:
 * - sets `dma_active = true`
 * - configures the DMA read address to the tail entry buffer and sets the
 *   transfer count to the tail entry's length
 *
 * @note The queue tail (`q_tail`) is advanced and the entry cleared by the DMA
 *       completion handler (dma_handler()) after the transfer finishes.
 * @note This function must be called with interrupts enabled.
 */
static void start_next_dma_if_needed() {
    // Check hardware busy as well as our flag to avoid races
    if (queue_empty() || dma_active || dma_channel_is_busy(uart_dma_chan)) {
        return;
    }

    // Only start when the tail entry is marked ready (len > 0)
    log_entry_t *entry = &log_queue[q_tail];
    // Acquire-load pairs with producer's release-store to len
    uint16_t len = __atomic_load_n(&entry->len, __ATOMIC_ACQUIRE);
    if (len == 0) {
        return; // producer hasn’t published the entry yet
    }

    // Mark active before arming to avoid a window where producers see it as idle
    dma_active = true;

    dma_channel_set_read_addr(uart_dma_chan, entry->buf, false);
    dma_channel_set_trans_count(uart_dma_chan, len, true);
    // q_tail will be advanced in dma_handler() once this transfer completes
}

/**
 * Handle DMA TX completion for the UART: acknowledge the DMA interrupt,
 * mark the current transfer as finished, consume the completed queue slot,
 * and start the next queued message if one is available.
 *
 * @note Executed in interrupt context (ISR); must be fast and ISR-safe.
 */
void __isr dma_handler() {
    uint32_t mask = 1u << uart_dma_chan;
    if (dma_hw->ints0 & mask) {
        dma_hw->ints0 = mask;  // clear IRQ flag
        dma_active = false;

        // Finished current entry → consume it
        uint8_t finished = q_tail;
        q_tail = (q_tail + 1) & (PRINTF_QUEUE_LEN - 1);

        // Mark the slot as empty (len = 0) so a future start won’t arm early
        log_entry_t *done = &log_queue[finished];
        done->len = 0;

        // Start next if available
        start_next_dma_if_needed();
    }
}

/**
 * @brief Queue full policy handler with configurable behavior
 * 
 * This function implements the compile-time selected queue full policy:
 * 
 * - **DROP Policy**: Returns immediately if queue is full (real-time safe)
 * - **WAIT_FIXED Policy**: Busy-waits with tight polling until timeout
 * - **WAIT_EXP Policy**: Uses exponential backoff delays (CPU-friendly)
 * 
 * The behavior is determined at compile time by UART_DMA_POLICY setting,
 * ensuring zero runtime overhead for policy selection. The maximum wait
 * time is controlled by UART_DMA_WAIT_US configuration parameter.
 * 
 * Policy Details:
 * - DROP: Always returns immediately with queue status
 * - WAIT_FIXED: Polls continuously using tight_loop_contents() 
 * - WAIT_EXP: Progressive delays: 1μs → 2μs → 4μs → ... → 1024μs (capped)
 * 
 * @return true if queue has space available, false if full (DROP) or timeout reached
 * 
 * @note Policy is compile-time configured via UART_DMA_POLICY macro
 * @note Maximum wait time configured via UART_DMA_WAIT_US macro
 * @note Thread-safe: Uses atomic queue state checking
 */
/**
 * Ensure there is space in the transmit queue using the compile-time queue-full policy.
 *
 * Depending on UART_DMA_POLICY this either returns immediately when the queue is full (DROP),
 * blocks up to UART_DMA_WAIT_US while polling (WAIT_FIXED), or performs exponential backoff
 * up to UART_DMA_WAIT_US before giving up (WAIT_EXP). When called from an interrupt context,
 * wait policies behave like DROP and do not block.
 *
 * @returns `true` if a queue slot is available, `false` if the queue remained full (or a timeout/policy prevented waiting).
 */
static inline bool wait_for_queue_space(void) {
#if UART_DMA_POLICY == UART_DMA_POLICY_DROP
    // Always drop if full
    return !queue_full();

#elif UART_DMA_POLICY == UART_DMA_POLICY_WAIT_FIXED
    // If called from IRQ, avoid spinning; treat as DROP
    if (in_irq()) {
        return !queue_full();
    }
    // Poll until queue frees or timeout expires
    absolute_time_t deadline = make_timeout_time_us(UART_DMA_WAIT_US);
    while (queue_full() && !time_reached(deadline)) {
        tight_loop_contents();
    }
    return !queue_full();

#elif UART_DMA_POLICY == UART_DMA_POLICY_WAIT_EXP
    // Exponential backoff; in IRQ, avoid sleeping → treat as DROP
    if (in_irq()) {
        return !queue_full();
    }
    int delay_us = 1;
    int waited = 0;
    while (queue_full() && waited < UART_DMA_WAIT_US) {
        sleep_us(delay_us);
        waited += delay_us;
        delay_us <<= 1;  // double delay
        if (delay_us > 1024) delay_us = 1024; // cap step size
    }
    return !queue_full();

#else
#  error "Invalid UART_DMA_POLICY setting"
#endif
}

/**
 * Reserve the next available queue slot for enqueuing a message.
 *
 * Attempts a lock-free, atomic reservation of a single queue slot and writes
 * the reserved slot index to `out_idx` on success.
 *
 * In interrupt context the function performs a single attempt and returns
 * immediately on contention to avoid spinning. In non-interrupt (thread)
 * context it may retry briefly on transient contention. If the queue is full
 * or a reservation fails (in IRQ), the function returns `false`.
 *
 * @param out_idx Pointer to a uint8_t that will receive the reserved slot index on success.
 * @return `true` if a slot was reserved and `out_idx` was set, `false` if the queue is full or reservation failed.
 */
static inline bool try_reserve_slot(uint8_t *out_idx) {
    for (;;) {
        uint8_t head = q_head;
        uint8_t next = (head + 1) & (PRINTF_QUEUE_LEN - 1);
        if (next == q_tail) {
            return false; // full
        }
        if (__atomic_compare_exchange_n(&q_head, &head, next, false,
                                        __ATOMIC_RELAXED, __ATOMIC_RELAXED)) {
            *out_idx = head;
            return true;
        }
        // CAS failed due to concurrent producer
        if (in_irq()) {
            return false; // don't spin in IRQ
        }
        tight_loop_contents(); // brief yield before retry
    }
}

/**
 * Enqueues a preformatted string into the UART DMA transmit queue and triggers
 * a DMA transfer if the transmitter is idle.
 *
 * If the queue is full the message may be dropped according to the configured
 * queue policy; the provided length is clamped to the per-entry buffer size.
 *
 * @param s Pointer to the preformatted data to enqueue.
 * @param len Number of bytes from `s` to enqueue; values greater than the
 *            per-entry buffer size are limited to the buffer capacity minus one.
 *
 * Thread-safe and IRQ-safe: safe to call from any context, including interrupt
 * handlers. Optional enqueue/drop statistics are updated when enabled.
 */
static void uart_dma_write_raw(const char *s, int len) {
    if (len <= 0) return;
    if (len >= PRINTF_BUF_SIZE) len = PRINTF_BUF_SIZE - 1;

    // Apply configurable queue policy for stdio output
    if (!wait_for_queue_space()) {
#ifdef UART_DMA_DEBUG_STATS
        __atomic_fetch_add(&stats_dropped, 1, __ATOMIC_RELAXED);
        report_drop_stats();
#endif
        return;  // Queue full - drop message
    }

    uint8_t idx;
    if (!try_reserve_slot(&idx)) {
#ifdef UART_DMA_DEBUG_STATS
        __atomic_fetch_add(&stats_dropped, 1, __ATOMIC_RELAXED);
        report_drop_stats();
#endif
        return; // full or contended (ISR), or rare CAS contention (thread)
    }

    log_entry_t *entry = &log_queue[idx];

    // Write payload first
    memcpy(entry->buf, s, len);

    // Publish last: mark entry ready by setting len > 0
    // Use an atomic store (release) for extra safety, but simple store also works on RP2040.
    __atomic_store_n(&entry->len, (uint16_t)len, __ATOMIC_RELEASE);

#ifdef UART_DMA_DEBUG_STATS
    __atomic_fetch_add(&stats_enqueued, 1, __ATOMIC_RELAXED);
#endif

    start_next_dma_if_needed();
}



/**
 * @brief Standard I/O Integration
 * 
 * These functions integrate the DMA-based UART implementation with the
 * Pico SDK's stdio system, allowing standard C library functions like
 * printf(), puts(), putchar(), etc. to use the high-performance DMA
 * transmission automatically.
 */

/**
 * Redirect stdio output to the DMA-based UART logger.
 * @param s Pointer to the buffer containing characters to output (not necessarily NUL-terminated).
 * @param len Number of characters from `s` to output.
 */
static void my_out_chars(const char *s, int len) {
    uart_dma_write_raw(s, len);
}

/**
 * @brief stdio Driver Registration Structure
 * 
 * This structure registers our DMA-based implementation as the stdio driver,
 * completely replacing the standard Pico SDK UART stdio functionality.
 * 
 * Configuration:
 * - out_chars: Uses our DMA-based output function
 * - in_chars: NULL (no input support for logging-only implementation)
 * - crlf_enabled: true (automatic CR/LF conversion for proper line endings)
 */
static stdio_driver_t dma_stdio_driver = {
    .out_chars = my_out_chars,
    .in_chars  = NULL,          // No input support (logging only)
    .crlf_enabled = true,       // Enable automatic line ending conversion
};

/**
 * Initialize the DMA-driven UART logging subsystem and register it as the system stdio driver.
 *
 * Sets up UART0, configures a DMA channel for UART TX, installs the DMA completion IRQ
 * handler, and replaces the Pico SDK stdio driver so printf/puts/putchar route through
 * the DMA-based logger.
 *
 * Notes:
 * - Configuration values (baud rate, TX pin, buffer and queue sizes, queue policy) are
 *   read from config.h at compile time.
 * - This function is idempotent: subsequent calls have no effect after the first successful
 *   initialization.
 * - Hardware resources (UART0, one DMA channel, and the TX GPIO) are claimed and remain
 *   allocated for the lifetime of the program.
 *
 * Warning:
 * - Call this during system initialization before any printf/logging calls for reliable output.
 * - Do not call from an interrupt context while initialization is in progress.
 */
void init_uart_dma() {
    if (uart_dma_inited) return;
    uart_dma_inited = true;
    // Initialize UART hardware with configured baud rate and TX pin
    uart_init(UART_ID, UART_BAUD);                        // UART_BAUD defined in config.h
    uart_set_format(UART_ID, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(UART_ID, true);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);       // UART_TX_PIN defined in config.h

    // Allocate and configure DMA channel for UART transmission
    uart_dma_chan = dma_claim_unused_channel(true);       // Claim any available DMA channel
    dma_channel_config c = dma_channel_get_default_config(uart_dma_chan);
    
    // Configure DMA transfer parameters
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);      // 8-bit transfers (char data)
    channel_config_set_read_increment(&c, true);                // Increment source (queue buffer)
    channel_config_set_write_increment(&c, false);              // Fixed destination (UART data register)
    channel_config_set_dreq(&c, uart_get_dreq(UART_ID, true));  // Use UART TX data request signal
    
    // Configure the DMA channel with settings
    dma_channel_configure(uart_dma_chan, &c,
                          &uart_get_hw(UART_ID)->dr,     // Destination: UART data register (FIFO)
                          NULL, 0, false);               // Source and count set per transfer
    
    // Setup interrupt handling for DMA completion
    dma_channel_set_irq0_enabled(uart_dma_chan, true);   // Enable interrupts for this channel

    // Clear any pending IRQ for this channel before enabling
    uint32_t mask = 1u << uart_dma_chan;
    dma_hw->ints0 = mask;

    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);   // Set our handler function  
    irq_set_enabled(DMA_IRQ_0, true);                    // Enable the interrupt
    irq_set_priority(DMA_IRQ_0, 0xC0);                   // Low priority (0=highest, 255=lowest)

    // Register this implementation as the system stdio driver
    // This replaces the standard Pico SDK UART stdio functionality
    stdio_set_driver_enabled(&dma_stdio_driver, true);
}

/**
 * Block until all queued UART log messages have been transmitted.
 *
 * Waits for the software queue to be empty, for any in-progress DMA transfer to
 * complete, and for the UART TX FIFO to be drained before returning.
 *
 * @note This function blocks and should be called from thread/main context only.
 * @note If producers continue to enqueue messages, this function may wait
 *       indefinitely.
 * @warning Do not call from interrupt context.
 */
void uart_dma_flush(void) {
    // Wait for all queued messages to be transmitted
    while (!queue_empty() || dma_active) {
        tight_loop_contents();  // Allow interrupts to run
    }
    
    // Extra safety: ensure UART FIFO is also empty
    uart_tx_wait_blocking(UART_ID);
}

#ifdef UART_DMA_DEBUG_STATS
/**
 * @brief Get current UART DMA statistics
 * 
 * Retrieves the current statistics counters for UART DMA operation. This function
 * is only available when UART_DMA_DEBUG_STATS is defined in config.h.
 * 
 * Statistics include:
 * - Total messages successfully enqueued
 * - Total messages dropped due to queue full
 * - Derived drop rate percentage
 * 
 * Use this function to monitor queue behavior and identify if queue sizing
 * adjustments are needed. High drop rates may indicate:
 * - Queue too small for burst logging patterns
 * - UART baud rate too slow for message volume
 * - Excessive logging during critical operations
 * 
 * @param enqueued Pointer to store enqueued message count (can be NULL)
 * @param dropped Pointer to store dropped message count (can be NULL)
 * 
 * @note Only available when UART_DMA_DEBUG_STATS is defined
 * @note Uses atomic loads for thread-safe access
 * @note Counters are cumulative since system boot
 * 
 * Example:
 * ```c
 * uint32_t enq, drop;
 * uart_dma_get_stats(&enq, &drop);
 * printf("Drop rate: %lu%%\n", (drop * 100) / (enq + drop));
 * ```
 */
void uart_dma_get_stats(uint32_t *enqueued, uint32_t *dropped) {
    if (enqueued) {
        *enqueued = __atomic_load_n(&stats_enqueued, __ATOMIC_RELAXED);
    }
    if (dropped) {
        *dropped = __atomic_load_n(&stats_dropped, __ATOMIC_RELAXED);
    }
}
#endif

