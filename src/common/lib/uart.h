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
 * @file uart.h
 * @brief High-Performance DMA-Based UART Logging Interface
 *
 * This header defines the interface for a high-performance, non-blocking UART
 * logging system designed specifically for real-time applications such as
 * keyboard protocol converters where timing is critical.
 *
 * Key Features:
 * - Configurable queue policies: DROP, WAIT_FIXED, or WAIT_EXP behaviour when queue full
 * - Large message queue: 64-entry buffer handles initialisation bursts without loss
 * - DMA-driven transmission: Minimal CPU overhead during log output
 * - stdio integration: Works transparently with printf(), puts(), etc. in non-IRQ contexts
 * - Thread-safe multi-producer: Lock-free atomic operations prevent races
 * - IRQ-aware policies: Never blocks in interrupt context
 * - Low priority: Designed to not interfere with PIO or USB operations
 * - Compile-time optimisation: Zero runtime overhead for policy selection
 *
 * Usage:
 * ```c
 * // Initialise once during system startup
 * init_uart_dma();
 *
 * // Use standard C library functions - they automatically use DMA transmission
 * printf("Debug: key pressed = 0x%02X\n", scancode);
 * puts("System initialised");
 * ```
 *
 * Configuration:
 * The UART configuration is defined in config.h:
 *
 * Hardware Configuration:
 * - UART_BAUD: Transmission speed (typically 115200)
 * - UART_TX_PIN: GPIO pin for UART transmission
 *
 * Queue Policy Configuration:
 * - UART_DMA_POLICY: Queue behaviour selection
 *   - UART_DMA_POLICY_DROP: Always drop messages when queue full (real-time safe)
 *   - UART_DMA_POLICY_WAIT_FIXED: Poll with tight loop until timeout
 *   - UART_DMA_POLICY_WAIT_EXP: Exponential backoff delays (CPU-friendly)
 * - UART_DMA_WAIT_US: Maximum wait time for WAIT policies (microseconds)
 *
 * Policy Trade-offs:
 * - DROP: Zero blocking, may lose messages under extreme load (real-time safe)
 * - WAIT_FIXED: High CPU usage during waits, reliable message delivery
 * - WAIT_EXP: Low CPU usage during waits, reliable message delivery
 *
 * Limitations:
 * - Output only (no UART input support)
 * - Maximum 64 queued messages (fixed at compile time)
 * - Maximum 256 characters per message (fixed at compile time)
 * - Behaviour when queue full depends on selected policy
 *
 * Thread Safety:
 * Logging/stdio output functions are designed to be safely called from any execution context:
 * - Main application loop
 * - Interrupt service routines (including PIO interrupts) via LOG_* macros only
 * - DMA completion handlers
 * - USB callback functions
 * (init_uart_dma() and uart_dma_flush() are main-thread only; see warnings below)
 *
 * The implementation uses lock-free algorithms with atomic compare-and-swap
 * operations to ensure thread safety without explicit synchronisation primitives.
 * IRQ context is detected automatically and policies adapt accordingly.
 *
 * Queue Policy Behaviour:
 * - DROP Policy: Returns immediately if queue full (never blocks)
 * - WAIT_FIXED Policy: Polls continuously until space or timeout (IRQ-aware)
 * - WAIT_EXP Policy: Progressive delays: 1µs → 2µs → 4µs → ... → 1024µs (IRQ-aware)
 *
 * @note All policies automatically fall back to DROP behaviour in IRQ context; avoid printf/puts
 * in IRQs and use LOG_* instead
 * @note Only init_uart_dma() is exposed - this is a stdio replacement system
 */

#ifndef UART_H
#define UART_H

#include <stdint.h>

#include "config.h"

/**
 * @brief Initialise DMA-Based UART Logging System
 *
 * This function performs complete system initialisation for the high-performance
 * DMA-based UART logging infrastructure. It configures hardware, allocates
 * resources, and integrates with the Pico SDK stdio system to provide
 * transparent, non-blocking debug output.
 *
 * Initialisation Sequence:
 * 1. UART Hardware Setup:
 *    - Initialises UART0 with configured baud rate (from config.h)
 *    - Configures GPIO pin for UART TX function
 *    - Sets up hardware FIFO and transmission parameters
 *
 * 2. DMA Channel Configuration:
 *    - Claims unused DMA channel from hardware pool
 *    - Configures for 8-bit data transfers (character data)
 *    - Sets UART TX DREQ as transfer trigger signal
 *    - Points destination to UART hardware data register
 *
 * 3. Interrupt System Setup:
 *    - Enables DMA completion interrupts for allocated channel
 *    - Registers uart_dma_complete_handler() as exclusive interrupt handler
 *    - Sets low priority to avoid interfering with real-time code
 *    - Enables interrupt in NVIC for DMA_IRQ_0
 *
 * 4. stdio Integration:
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
 * Post-Initialisation Behaviour:
 * After this function completes successfully:
 * - All printf(), puts(), putchar() calls use DMA automatically
 * - System provides configurable non-blocking debug output behaviour
 * - No code changes required for existing printf usage
 * - Queue policy determines behaviour under load (drop/wait/backoff)
 *
 * @note Must be called once during system initialisation
 * @note Call before any printf/logging operations
 * @note Safe to call multiple times (subsequent calls ignored)
 * @note Claims hardware resources that remain allocated
 * @note Completely replaces standard Pico SDK UART stdio
 * @note Main loop only.
 *
 * @warning Calling printf() before this function results in no output
 */
void init_uart_dma(void);

/**
 * @brief Flush all pending UART messages
 *
 * This function blocks until all queued messages have been transmitted via DMA.
 * It's designed for clean shutdown scenarios where you need to ensure all log
 * messages are output before transitioning to a different state (e.g., bootloader).
 *
 * The function polls the queue state and waits for the DMA controller to finish
 * all pending transfers. It uses a tight polling loop for maximum responsiveness
 * whilst still allowing other interrupts to run.
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
 * @note Main loop only.
 *
 * @warning Do not call from interrupt context
 * @warning May block for extended periods if queue is full
 */
void uart_dma_flush(void);

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
 * Use this function to monitor queue behaviour and identify if queue sizing
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
 * @note Main loop only.
 *
 * Example:
 * ```c
 * uint32_t enq, drop;
 * uart_dma_get_stats(&enq, &drop);
 * printf("Drop rate: %lu%%\n", (drop * 100) / (enq + drop));
 * ```
 */
void uart_dma_get_stats(uint32_t* enqueued, uint32_t* dropped);
#endif

#endif /* UART_H */