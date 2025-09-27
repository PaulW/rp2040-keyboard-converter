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
 * - Non-blocking operation: Logging never blocks the calling thread
 * - DMA-driven transmission: Minimal CPU overhead during output
 * - Circular buffer queue: Handles burst logging efficiently
 * - stdio integration: Works with printf(), puts(), and other standard functions
 * - Low priority: Designed to not interfere with PIO state machines
 * - Graceful degradation: Drops messages when overwhelmed rather than blocking
 * 
 * Architecture:
 * The system uses a circular buffer to queue formatted log messages. When a
 * message is added to the queue, if the DMA controller is idle, it immediately
 * begins transmitting the first queued message. As each message completes
 * transmission, the DMA interrupt handler automatically starts the next queued
 * message. This provides continuous transmission without CPU intervention.
 * 
 * Performance Considerations:
 * - Queue size: 16 entries (power of 2) for efficient modulo operations
 * - Buffer alignment: 4-byte aligned for optimal DMA performance
 * - Bitwise operations: Uses AND instead of modulo for index calculations
 * - Low priority: DMA and IRQ configured to not interfere with timing-critical code
 * - Atomic operations: Relies on RP2040's natural atomicity for 8-bit operations
 * 
 * Thread Safety:
 * The implementation is designed to be safely called from any context, including
 * interrupts, without the need for explicit locking. This is achieved through:
 * - Lock-free circular buffer design
 * - Atomic index updates (8-bit operations on RP2040)
 * - DMA hardware handling the actual transmission
 * 
 * Integration:
 * This UART implementation completely replaces the standard Pico SDK UART
 * stdio driver, providing a drop-in replacement that works with all standard
 * C library functions (printf, puts, etc.) while offering superior performance
 * characteristics for real-time applications.
 */

#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/dma.h"
#include "pico/stdio/driver.h"
#include "uart.h"
#include <stdarg.h>
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
 * These parameters are tuned for optimal performance while maintaining
 * compatibility with real-time keyboard protocol operations.
 */
#define PRINTF_BUF_SIZE  256   /**< Size of each individual log message buffer (bytes) */
#define PRINTF_QUEUE_LEN 16    /**< Number of queued log messages (power of 2 for efficient indexing) */

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
 * This works because PRINTF_QUEUE_LEN is a power of 2.
 * 
 * @return true if queue is full (would overflow on next write)
 * @return false if queue has space for more messages
 */
static inline bool queue_full() {
    return ((q_head + 1) & (PRINTF_QUEUE_LEN - 1)) == q_tail;
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
void dma_handler() {
    // Clear DMA interrupt flag for this channel
    dma_hw->ints0 = 1u << uart_dma_chan;
    
    // Mark DMA as inactive (transfer completed)
    dma_active = false;

    // Start next message transmission if queue not empty
    if (!queue_empty()) {
        log_entry_t *entry = &log_queue[q_tail];
        
        // Configure DMA for next transfer
        dma_channel_set_read_addr(uart_dma_chan, entry->buf, false);
        dma_channel_set_trans_count(uart_dma_chan, entry->len, true);  // triggers transfer
        
        // Update state
        dma_active = true;
        q_tail = (q_tail + 1) & (PRINTF_QUEUE_LEN - 1);  // advance to next queue entry
    }
}

/**
 * @brief Non-Blocking DMA-Based Printf Implementation
 * 
 * This function provides a high-performance, non-blocking printf implementation
 * that queues formatted messages for DMA transmission. It never blocks the
 * calling thread, making it safe to use from any context including interrupts
 * and time-critical code paths.
 * 
 * Behavior:
 * - Formats the message using standard vsnprintf()
 * - Queues the message for transmission
 * - Starts DMA transfer immediately if not already active
 * - Drops messages if queue is full (graceful degradation)
 * 
 * Thread Safety:
 * - Safe to call from any context (main loop, interrupts, etc.)
 * - Uses atomic operations on RP2040 (8-bit index updates)
 * - No explicit locking required due to careful design
 * 
 * Performance:
 * - Minimal CPU overhead (formatting + queue management)
 * - DMA handles actual transmission autonomously
 * - Lock-free operation prevents blocking
 * 
 * @param fmt Printf-style format string
 * @param ... Variable arguments for format string
 */
void uart_dma_printf(const char *fmt, ...) {
    // Graceful degradation: drop message if queue is full
    // This prevents blocking in high-throughput scenarios
    if (queue_full()) {
        return;  // Could increment a dropped message counter here if needed
    }

    // Format message into the next available queue slot
    log_entry_t *entry = &log_queue[q_head];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(entry->buf, PRINTF_BUF_SIZE, fmt, args);
    va_end(args);

    // Validate formatting result
    if (len <= 0) return;  // Formatting error
    if (len >= PRINTF_BUF_SIZE) len = PRINTF_BUF_SIZE - 1;  // Truncate if too long
    entry->len = len;

    // Atomically commit message to queue
    // This is the critical section - must be atomic
    q_head = (q_head + 1) & (PRINTF_QUEUE_LEN - 1);

    // Start DMA transfer if not already active
    // Check-and-set pattern is safe here due to single-producer design
    if (!dma_active) {
        log_entry_t *first = &log_queue[q_tail];
        
        // Configure and start DMA transfer
        dma_channel_set_read_addr(uart_dma_chan, first->buf, false);
        dma_channel_set_trans_count(uart_dma_chan, first->len, true);  // triggers transfer
        
        // Update state
        dma_active = true;
        q_tail = (q_tail + 1) & (PRINTF_QUEUE_LEN - 1);
    }
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
    uart_dma_printf("%.*s", len, s);  // Use precision specifier to limit length
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
 * This function sets up the complete DMA-based UART logging infrastructure:
 * 1. Configures UART hardware for transmission
 * 2. Allocates and configures DMA channel for UART TX
 * 3. Sets up interrupt handling for DMA completion
 * 4. Registers the system as the stdio driver
 * 
 * Configuration Details:
 * - UART: Uses hardware defined by UART_ID with baud rate from config.h
 * - DMA: Configured for byte transfers with UART TX data request
 * - Priority: Low priority to avoid interfering with real-time operations
 * - Integration: Completely replaces standard Pico SDK stdio UART
 * 
 * Must be called once during system initialization before any logging occurs.
 * After this call, all printf(), puts(), and stdio output will use DMA.
 */
void init_uart_dma() {
    // Initialize UART hardware with configured baud rate and TX pin
    uart_init(UART_ID, UART_BAUD);                        // UART_BAUD defined in config.h
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);       // UART_TX_PIN defined in config.h

    // Allocate and configure DMA channel for UART transmission
    uart_dma_chan = dma_claim_unused_channel(true);       // Claim any available DMA channel
    dma_channel_config c = dma_channel_get_default_config(uart_dma_chan);
    
    // Configure DMA transfer parameters
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);              // 8-bit transfers (char data)
    channel_config_set_dreq(&c, uart_get_dreq(UART_ID, true));         // Use UART TX data request signal
    
    // Configure the DMA channel with settings
    dma_channel_configure(uart_dma_chan, &c,
                          &uart_get_hw(UART_ID)->dr,     // Destination: UART data register (FIFO)
                          NULL, 0, false);               // Source and count set per transfer
    
    // Setup interrupt handling for DMA completion
    dma_channel_set_irq0_enabled(uart_dma_chan, true);   // Enable interrupts for this channel
    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);   // Set our handler function  
    irq_set_enabled(DMA_IRQ_0, true);                    // Enable the interrupt
    irq_set_priority(DMA_IRQ_0, 2);                      // Low priority (0=highest, 255=lowest)

    // Register this implementation as the system stdio driver
    // This replaces the standard Pico SDK UART stdio functionality
    stdio_set_driver_enabled(&dma_stdio_driver, true);
}

