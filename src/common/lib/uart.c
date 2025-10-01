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
 * These parameters are defined in config.h and tuned for optimal performance
 * while maintaining compatibility with real-time keyboard protocol operations.
 * 
 * Configuration options:
 * - UART_DMA_BUFFER_SIZE: Maximum message length (typically 256 bytes)
 * - UART_DMA_QUEUE_SIZE: Number of queued messages (must be power of 2)
 */

// --- Sanity check: queue length must be power of 2 ---
#if (UART_DMA_QUEUE_SIZE & (UART_DMA_QUEUE_SIZE - 1)) != 0
#  error "UART_DMA_QUEUE_SIZE must be a power of 2"
#endif

// Compile-time bounds (uint8_t indices, uint16_t lengths)
_Static_assert(UART_DMA_QUEUE_SIZE <= 256, "UART_DMA_QUEUE_SIZE must fit uint8_t index");
_Static_assert(UART_DMA_BUFFER_SIZE  <= 65535, "UART_DMA_BUFFER_SIZE must fit uint16_t len");

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
    char buf[UART_DMA_BUFFER_SIZE];   /**< Formatted message buffer */
    uint16_t len;                      /**< Actual length of formatted message */
} log_entry_t;

/**
 * @brief Circular Buffer Queue and State Management
 * 
 * The queue is implemented as a circular buffer with separate head and tail
 * indices. The alignment attribute ensures optimal DMA transfer performance.
 */
static log_entry_t log_queue[UART_DMA_QUEUE_SIZE] __attribute__((aligned(4))); /**< Message queue (4-byte aligned for DMA) */
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
 * @brief Check if the message queue is empty
 * 
 * @return true if queue is empty (head equals tail)
 * @return false if queue contains messages
 */
static inline bool queue_empty() {
    return q_head == q_tail;
}

/**
 * @brief Check if the message queue is full
 * 
 * Uses bitwise AND operation instead of modulo for performance.
 * This works because UART_DMA_QUEUE_SIZE is a power of 2.
 * 
 * @return true if queue is full (would overflow on next write)
 * @return false if queue has space for more messages
 */
static inline bool queue_full() {
    return ((q_head + 1) & (UART_DMA_QUEUE_SIZE - 1)) == q_tail;
}

/**
 * @brief IRQ Context Detection Helper
 * 
 * Determines if the current execution context is within an interrupt service
 * routine by checking the VECTACTIVE field of the Interrupt Control and State Register.
 * 
 * This function is used by the queue policy implementation to adapt behavior
 * in interrupt context, ensuring that potentially blocking operations (like
 * sleep_us or extended polling) are avoided when called from ISRs.
 * 
 * @return true if executing in interrupt/exception context
 * @return false if executing in thread mode
 * 
 * @note Uses ARM Cortex-M0+ ICSR register for reliable detection
 * @note Zero overhead inline function
 */
static inline bool in_irq(void) {
    return (scb_hw->icsr & M0PLUS_ICSR_VECTACTIVE_BITS) != 0;
}

// Forward declarations for functions used in debug stats reporting
static inline bool try_reserve_slot(uint8_t *out_idx);
static void start_next_dma_if_needed(void);

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
 * Delivery Guarantee Strategy:
 * - Formats stats message to local buffer using snprintf()
 * - Manually reserves queue slot via try_reserve_slot()
 * - Only updates stats_last_reported on confirmed enqueue
 * - Uses direct queue access to avoid stdio recursion issues
 * - Ensures stats are eventually reported when queue has space
 * 
 * This approach guarantees accurate delivery confirmation because it directly
 * reserves a queue slot rather than relying on stats_enqueued counter changes,
 * which could be affected by other concurrent producers.
 * 
 * @note Called automatically from uart_dma_write_raw() when drops occur
 * @note Uses static tracking to avoid reporting unchanged statistics
 * @note Non-blocking: Uses direct slot reservation without waiting
 * @note Delivery-aware: Only advances last_reported on successful reservation
 */
static inline void report_drop_stats(void) {
    static bool in_report = false;  // Prevent recursion
    
    // Skip if already reporting (shouldn't happen with direct enqueue, but safety first)
    if (in_report) {
        return;
    }
    
    uint32_t current_drops = stats_dropped;
    
    // Report every 10 drops to balance visibility vs log spam
    if (current_drops > stats_last_reported && (current_drops % 10 == 0)) {
        uint32_t total = stats_enqueued + stats_dropped;
        uint32_t drop_pct = (total > 0) ? (stats_dropped * 100 / total) : 0;
        
        // Format stats message to local buffer
        char stats_msg[UART_DMA_BUFFER_SIZE];
        int len = snprintf(stats_msg, sizeof(stats_msg),
                          "[UART Stats] Dropped: %lu, Enqueued: %lu, Drop rate: %lu%%\n",
                          (unsigned long)stats_dropped,
                          (unsigned long)stats_enqueued,
                          (unsigned long)drop_pct);
        
        if (len <= 0 || len >= (int)sizeof(stats_msg)) {
            // Formatting error or truncation - skip this report
            return;
        }
        
        // Try to reserve a slot directly (non-blocking)
        uint8_t idx;
        in_report = true;  // Set flag to prevent any nested calls
        
        if (try_reserve_slot(&idx)) {
            // Successfully reserved slot - publish the stats message
            log_entry_t *entry = &log_queue[idx];
            memcpy(entry->buf, stats_msg, len);
            __atomic_store_n(&entry->len, (uint16_t)len, __ATOMIC_RELEASE);
            
            // Start DMA if needed
            start_next_dma_if_needed();
            
            // Update last_reported only on successful enqueue
            stats_last_reported = current_drops;
            
            // Don't increment stats_enqueued here - it's for normal messages
            // Stats messages are meta-data about the queue, not user data
        }
        // If try_reserve_slot fails, we'll retry on next call
        
        in_report = false;
    }
}
#endif

/**
 * @brief Start next DMA transfer if conditions are met
 * 
 * This function initiates a DMA transfer for the next queued message if:
 * - No DMA transfer is currently active
 * - The message queue is not empty
 * 
 * The function sets up the DMA channel to transfer data from the queue entry
 * at the tail position to the UART hardware. It configures:
 * - Source address: Points to the message buffer in the queue
 * - Transfer count: Number of bytes to transfer (message length)
 * - DMA active flag: Marked true to prevent concurrent transfers
 * 
 * Design Notes:
 * - Producer functions (enqueuers) never modify q_tail
 * - Consumer (DMA IRQ handler) advances q_tail when transfer completes
 * - This separation ensures clean producer/consumer relationship
 * - Called from both enqueue functions and DMA completion handler
 * 
 * @note This function must be called with interrupts enabled
 * @note The q_tail index will be advanced by dma_handler() upon completion
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
 * @brief DMA Transfer Complete Interrupt Handler
 * 
 * This interrupt handler is called when a DMA transfer to the UART completes.
 * It automatically starts the next queued message if available, providing
 * continuous output without CPU intervention.
 * 
 * The handler is designed to be as fast as possible to minimize interrupt
 * latency impact on real-time operations. It uses low-level register access
 * and efficient queue operations.
 * 
 * Handler Priority: Low (configured in init_uart_dma)
 * Execution Context: Interrupt (keep minimal and fast)
 */
void __isr dma_handler() {
    uint32_t mask = 1u << uart_dma_chan;
    if (dma_hw->ints0 & mask) {
        dma_hw->ints0 = mask;  // clear IRQ flag

        // Finished current entry → consume it
        uint8_t finished = q_tail;
        q_tail = (q_tail + 1) & (UART_DMA_QUEUE_SIZE - 1);

        // Mark the slot as empty (len = 0) so a future start won’t arm early
        log_entry_t *done = &log_queue[finished];
        done->len = 0;

        dma_active = false;

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
// --- Queue full policy handling ---
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
 * @brief Atomically reserve a queue slot for thread-safe operation
 * 
 * This function provides lock-free multi-producer safety by atomically
 * reserving a queue slot using compare-and-swap operations. It prevents
 * race conditions when multiple execution contexts (main thread + ISRs)
 * attempt to enqueue messages simultaneously.
 * 
 * Behavior:
 * - **Thread context**: Retries briefly on CAS contention to handle rare races
 * - **IRQ context**: Single attempt only to avoid spinning in interrupt handlers
 * - **Queue full**: Returns false immediately regardless of context
 * - **Success**: Returns the reserved slot index via out parameter
 * 
 * The function uses relaxed memory ordering for the CAS operation since the
 * RP2040's single-core nature and the queue's design make stronger ordering
 * unnecessary while still providing the required atomicity.
 * 
 * @param out_idx Pointer to store the reserved slot index on success
 * @return true if slot successfully reserved, false if queue full or contention (IRQ)
 * 
 * @note Uses __atomic_compare_exchange_n for lock-free operation
 * @note IRQ-safe: Never spins when called from interrupt context
 * @note Thread-safe: Handles concurrent access from multiple contexts
 */
static inline bool try_reserve_slot(uint8_t *out_idx) {
    for (;;) {
        uint8_t head = q_head;
        uint8_t next = (head + 1) & (UART_DMA_QUEUE_SIZE - 1);
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
 * @brief Raw message enqueue for pre-formatted strings
 * 
 * This function provides the core message enqueueing functionality for the
 * stdio integration path. It handles thread-safe slot reservation, message
 * copying, and DMA initiation with configurable queue policies.
 * 
 * Key Features:
 * - **Thread-safe operation**: Uses atomic slot reservation to prevent races
 * - **Configurable queue policies**: DROP/WAIT_FIXED/WAIT_EXP behavior selection
 * - **Input validation**: Length limiting and bounds checking
 * - **Publish-subscribe pattern**: Atomic length update signals message ready
 * - **Automatic DMA initiation**: Starts transmission when controller is idle
 * - **Statistics tracking**: Optional drop/enqueue counting (UART_DMA_DEBUG_STATS)
 * 
 * Message Flow:
 * 1. **Policy check**: wait_for_queue_space() applies configured behavior
 * 2. **Slot reservation**: try_reserve_slot() atomically claims queue entry  
 * 3. **Data copy**: memcpy() transfers pre-formatted message to queue buffer
 * 4. **Publish**: Atomic length store marks entry ready for DMA consumption
 * 5. **DMA start**: start_next_dma_if_needed() initiates transfer if idle
 * 6. **Stats update**: Track success/drops if debug stats enabled
 * 
 * The function uses acquire/release semantics for the length field to ensure
 * proper memory ordering between producers and the DMA consumer, guaranteeing
 * that DMA never reads partially written message data.
 * 
 * @param s Pointer to pre-formatted string buffer
 * @param len Length of string to enqueue (bytes)
 * 
 * @note Used internally by stdio integration - not for direct application calls  
 * @note Thread-safe: Safe to call from any context including interrupts
 * @note IRQ-aware: Adapts behavior based on execution context
 * @note Performance: Direct memcpy with atomic publish for efficiency
 */
static void uart_dma_write_raw(const char *s, int len) {
    if (len <= 0) return;
    if (len > UART_DMA_BUFFER_SIZE) len = UART_DMA_BUFFER_SIZE;

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
 * @brief stdio Output Character Hook
 * 
 * This function is called by the Pico SDK stdio system whenever characters
 * need to be output. It redirects all stdio output to our DMA-based printf.
 * 
 * @param s Pointer to character buffer to output
 * @param len Number of characters to output
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
 * @brief Initialize DMA-Based UART Logging System
 * 
 * This function performs complete system initialization for the high-performance
 * DMA-based UART logging infrastructure. It configures hardware, allocates
 * resources, and integrates with the Pico SDK stdio system to provide
 * transparent, non-blocking debug output.
 * 
 * Initialization Sequence:
 * 1. **UART Hardware Setup**:
 *    - Initializes UART0 with configured baud rate (from config.h)
 *    - Configures GPIO pin for UART TX function
 *    - Sets up hardware FIFO and transmission parameters
 * 
 * 2. **DMA Channel Configuration**:
 *    - Claims unused DMA channel from hardware pool
 *    - Configures for 8-bit data transfers (character data)
 *    - Sets UART TX DREQ as transfer trigger signal
 *    - Points destination to UART hardware data register
 * 
 * 3. **Interrupt System Setup**:
 *    - Enables DMA completion interrupts for allocated channel
 *    - Registers dma_handler() as exclusive interrupt handler
 *    - Sets low priority to avoid interfering with real-time code
 *    - Enables interrupt in NVIC for DMA_IRQ_0
 * 
 * 4. **stdio Integration**:
 *    - Registers DMA-based driver with Pico SDK stdio system
 *    - Replaces standard blocking UART stdio completely
 *    - Enables automatic CRLF conversion for proper line endings
 * 
 * Configuration Sources:
 * All configuration parameters are read from config.h:
 * - UART_BAUD: Transmission baud rate (typically 115200)
 * - UART_TX_PIN: GPIO pin number for UART TX (typically GP0)
 * 
 * Hardware Resources Used:
 * - UART0: Primary UART peripheral for transmission
 * - 1x DMA Channel: Automatically claimed from available pool  
 * - 1x GPIO Pin: Configured for UART TX function
 * - DMA_IRQ_0: Interrupt vector for DMA completion handling
 * 
 * Memory Resources:
 * - 64 x 256 bytes = 16KB: Message queue buffer (static allocation)
 * - ~100 bytes: Control structures and state variables
 * 
 * Post-Initialization Behavior:
 * After this function completes successfully:
 * - All printf(), puts(), putchar() calls use DMA automatically
 * - System provides configurable non-blocking debug output behavior
 * - No code changes required for existing printf usage
 * - Queue policy determines behavior under load (drop/wait/backoff)
 * 
 * @note **CRITICAL**: Must be called once during system initialization
 * @note **TIMING**: Call before any printf/logging operations
 * @note **SAFETY**: Safe to call multiple times (subsequent calls ignored)
 * @note **RESOURCES**: Claims hardware resources that remain allocated
 * @note **INTEGRATION**: Completely replaces standard Pico SDK UART stdio
 * 
 * @warning Calling printf() before this function results in no output
 * @warning Do not call from interrupt context during initialization
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
 * @brief Flush all pending UART messages
 * 
 * This function blocks until all queued messages have been transmitted via DMA.
 * It's designed for clean shutdown scenarios where you need to ensure all log
 * messages are output before transitioning to a different state (e.g., bootloader).
 * 
 * The function polls the queue state and waits for the DMA controller to finish
 * all pending transfers. It uses a tight polling loop for maximum responsiveness
 * while still allowing other interrupts to run.
 * 
 * Use Cases:
 * - Before entering bootloader mode
 * - Before system reset or power-down
 * - When ensuring critical log messages are output
 * 
 * Performance:
 * - Blocking time depends on queue depth and UART baud rate
 * - At 115200 baud: ~87µs per character, ~22ms per 256-byte message
 * - 64-entry full queue: up to ~1.4 seconds worst case
 * 
 * @note This function blocks until queue is empty - use sparingly
 * @note Safe to call from main context only (not from ISR)
 * @note Will wait indefinitely if new messages keep being enqueued
 * 
 * @warning Do not call from interrupt context
 * @warning May block for extended periods if queue is full
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

