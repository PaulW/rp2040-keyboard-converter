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
 * - **Configurable queue policies**: DROP, WAIT_FIXED, or WAIT_EXP behaviour when queue full
 * - **Large message queue**: 64-entry buffer handles initialisation bursts without loss
 * - **DMA-driven transmission**: Minimal CPU overhead during log output
 * - **stdio integration**: Works transparently with printf(), puts(), etc.
 * - **Thread-safe multi-producer**: Lock-free atomic operations prevent races
 * - **IRQ-aware policies**: Never blocks in interrupt context
 * - **Low priority**: Designed to not interfere with PIO or USB operations
 * - **Compile-time optimisation**: Zero runtime overhead for policy selection
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
 * **Hardware Configuration:**
 * - UART_BAUD: Transmission speed (typically 115200)
 * - UART_TX_PIN: GPIO pin for UART transmission
 *
 * **Queue Policy Configuration:**
 * - UART_DMA_POLICY: Queue behaviour selection
 *   - UART_DMA_POLICY_DROP: Always drop messages when queue full (real-time safe)
 *   - UART_DMA_POLICY_WAIT_FIXED: Poll with tight loop until timeout
 *   - UART_DMA_POLICY_WAIT_EXP: Exponential backoff delays (CPU-friendly)
 * - UART_DMA_WAIT_US: Maximum wait time for WAIT policies (microseconds)
 *
 * Policy Trade-offs:
 * - **DROP**: Zero blocking, may lose messages under extreme load (real-time safe)
 * - **WAIT_FIXED**: High CPU usage during waits, reliable message delivery
 * - **WAIT_EXP**: Low CPU usage during waits, reliable message delivery
 *
 * Performance Characteristics:
 * - Queue operation: ~1-2µs (atomic slot reservation with CAS operations)
 * - DMA setup: ~5-10µs (when starting new transfer)
 * - Policy overhead: 0µs (DROP), variable (WAIT policies, IRQ-aware)
 * - Total overhead: Usually ~20µs for typical log messages
 *
 * Limitations:
 * - Output only (no UART input support)
 * - Maximum 64 queued messages (fixed at compile time)
 * - Maximum 256 characters per message (fixed at compile time)
 * - Behaviour when queue full depends on selected policy
 *
 * Thread Safety:
 * All functions are designed to be safely called from any execution context:
 * - Main application loop
 * - Interrupt service routines (including PIO interrupts)
 * - DMA completion handlers
 * - USB callback functions
 *
 * The implementation uses lock-free algorithms with atomic compare-and-swap
 * operations to ensure thread safety without explicit synchronisation primitives.
 * IRQ context is detected automatically and policies adapt accordingly.
 *
 * Queue Policy Behaviour:
 * - **DROP Policy**: Returns immediately if queue full (never blocks)
 * - **WAIT_FIXED Policy**: Polls continuously until space or timeout (IRQ-aware)
 * - **WAIT_EXP Policy**: Progressive delays: 1µs → 2µs → 4µs → ... → 1024µs (IRQ-aware)
 *
 * @note All policies automatically fall back to DROP behaviour in IRQ context
 * @note Only init_uart_dma() is exposed - this is a stdio replacement system
 */

#ifndef UART_H
#define UART_H

#include "config.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Initialise DMA-Based UART Logging System
 *
 * Sets up the complete UART logging infrastructure including:
 * - UART hardware configuration (baud rate, pins from config.h)
 * - DMA channel allocation and configuration
 * - Interrupt handler registration (low priority)
 * - stdio driver integration for transparent printf() support
 *
 * This function must be called once during system initialisation before
 * any logging operations. After initialisation, all standard C library
 * output functions (printf, puts, putchar, etc.) will automatically
 * use the DMA-based transmission system with the configured queue policy.
 *
 * Initialisation Sequence:
 * 1. **Hardware Setup**: UART0 configuration with format and FIFO settings
 * 2. **DMA Configuration**: Channel allocation with explicit transfer parameters
 * 3. **Interrupt Setup**: Low-priority handler registration with IRQ clearing
 * 4. **stdio Integration**: Complete replacement of standard UART stdio driver
 *
 * Configuration Parameters (from config.h):
 * - UART_BAUD: Transmission baud rate (typically 115200)
 * - UART_TX_PIN: GPIO pin for UART TX (typically GP0)
 * - UART_DMA_POLICY: Queue full behaviour policy (DROP/WAIT_FIXED/WAIT_EXP)
 * - UART_DMA_WAIT_US: Maximum wait time for WAIT policies (microseconds)
 *
 * Hardware Resources Allocated:
 * - 1x DMA channel (claimed from available pool)
 * - UART0 peripheral (configured for transmission only)
 * - 1x GPIO pin (configured for UART function)
 * - DMA_IRQ_0 interrupt vector (low priority 0xC0)
 *
 * Memory Resources:
 * - 16KB: Message queue (64 × 256-byte entries)
 * - ~100 bytes: Control structures and state
 *
 * @note **CRITICAL**: Must be called once during system initialisation
 * @note **TIMING**: Call before any printf/logging operations
 * @note **IDEMPOTENT**: Safe to call multiple times (subsequent calls ignored)
 * @note **STDIO REPLACEMENT**: Completely replaces standard Pico SDK UART stdio
 * @note **REAL-TIME SAFE**: Uses low-priority interrupts and configurable policies
 *
 * @warning Do not call from interrupt context during initialisation
 * @warning Calling printf() before this function results in no output
 */
void init_uart_dma(void);

/**
 * @brief Flush all pending UART messages
 *
 * Blocks until all queued log messages have been transmitted via DMA and the
 * UART hardware FIFO is empty. This ensures all debug output is complete before
 * transitioning to a different system state.
 *
 * Primary Use Case:
 * - Before entering bootloader mode (ensures final log messages are visible)
 * - Before system reset or power-down
 * - When critical messages must be guaranteed to output
 *
 * Behaviour:
 * - Polls queue state until empty (non-sleeping busy-wait)
 * - Waits for active DMA transfer to complete
 * - Waits for UART hardware FIFO to drain
 * - Allows interrupts to run during polling (uses tight_loop_contents)
 *
 * Performance Considerations:
 * - Blocking time depends on queue depth and UART_BAUD setting
 * - At 115200 baud: ~87µs per character
 * - Full queue (64 × 256 bytes): worst case ~1.4 seconds
 * - Empty queue: returns immediately
 *
 * @note **BLOCKING**: Function blocks until all messages transmitted
 * @note **CONTEXT**: Safe to call from main thread only (not ISR)
 * @note **IDEMPOTENT**: Safe to call multiple times
 * @note **INTERFERENCE**: Will block indefinitely if messages keep being added
 *
 * @warning Do not call from interrupt context
 * @warning May block for extended periods if queue is full
 * @warning Ensure no other code continues to call printf() during flush
 */
void uart_dma_flush(void);

#ifdef UART_DMA_DEBUG_STATS
/**
 * @brief Get UART DMA statistics
 *
 * Retrieves current statistics for UART DMA operation including total messages
 * enqueued and dropped. Only available when UART_DMA_DEBUG_STATS is enabled
 * in config.h.
 *
 * Statistics Usage:
 * - Monitor queue behaviour under different load conditions
 * - Identify if queue sizing needs adjustment
 * - Detect excessive logging that may impact real-time operations
 * - Calculate drop rates to assess reliability
 *
 * @param enqueued Pointer to receive total enqueued count (NULL to skip)
 * @param dropped Pointer to receive total dropped count (NULL to skip)
 *
 * @note Only available when UART_DMA_DEBUG_STATS is defined
 * @note Thread-safe: Uses atomic loads
 * @note Counters are cumulative since boot
 *
 * Example:
 * ```c
 * #ifdef UART_DMA_DEBUG_STATS
 * uint32_t enq, drop;
 * uart_dma_get_stats(&enq, &drop);
 * if (enq + drop > 0) {
 *     uint32_t rate = (drop * 100) / (enq + drop);
 *     printf("UART drop rate: %lu%%\n", rate);
 * }
 * #endif
 * ```
 */
void uart_dma_get_stats(uint32_t* enqueued, uint32_t* dropped);
#endif

#endif /* UART_H */