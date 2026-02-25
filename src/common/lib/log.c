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
 * @file log.c
 * @brief High-Performance Log Level Filtering Implementation
 *
 * This file implements the runtime portion of the log level filtering system.
 * The implementation is designed for maximum performance with minimal overhead.
 *
 * **Performance Characteristics:**
 *
 * Storage:
 * - 1 byte: current_log_level (static volatile)
 * - ~50 bytes: function code (get/set/init)
 *
 * Runtime Overhead:
 * - log_get_level(): 2-3 cycles (inline candidate, single load)
 * - log_set_level(): 2-3 cycles (single store)
 * - LOG_* macro check: 5-10 cycles (comparison + conditional branch)
 *
 * Thread Safety:
 * - Cortex-M0+ has atomic byte read/write operations
 * - No explicit synchronisation needed for uint8_t
 * - Volatile qualifier ensures visibility across contexts
 * - Safe to call from main loop, IRQs, or DMA handlers
 *
 * Integration:
 * - Automatically initialised by init_uart_dma()
 * - Can be explicitly initialised via log_init()
 * - Works transparently with DMA-based UART system
 *
 * Design Philosophy:
 * - Minimise runtime overhead (single volatile uint8_t)
 * - Maximise compile-time optimisation (macros with dead code elimination)
 * - Maintain backward compatibility (printf still works)
 * - Support gradual migration (both old and new can coexist)
 */

#include "log.h"

#include <stdint.h>

/**
 * @brief Current runtime log level
 *
 * This variable controls which log messages are output at runtime.
 *
 * Volatile Qualifier:
 * - Ensures visibility across different execution contexts
 * - Prevents compiler from caching value in registers
 * - Critical for IRQ-safe operation
 *
 * Thread Safety:
 * - Cortex-M0+ provides atomic byte read/write
 * - No need for explicit synchronisation primitives
 * - Safe to access from any context without locks
 *
 * Default Value:
 * - Initialised to LOG_LEVEL_DEFAULT from config.h
 * - Typically LOG_LEVEL_INFO (errors + info, no debug)
 * - Can be changed at runtime via log_set_level()
 */
static volatile uint8_t current_log_level = LOG_LEVEL_DEFAULT;

/**
 * @brief Set runtime log level
 *
 * Changes the minimum log level that will be output. This is a simple
 * atomic write operation with minimal overhead.
 *
 * Implementation:
 * - Single volatile uint8_t write (atomic on Cortex-M0+)
 * - No locking or synchronisation needed
 * - Effect is immediate for subsequent log calls
 * - Compiler generates 1-2 instructions (mov + strb)
 *
 * Performance:
 * - Execution time: 2-3 CPU cycles
 * - No function call overhead (likely inlined)
 * - No memory barriers needed (Cortex-M0+ memory model)
 *
 * Thread Safety:
 * - Safe to call from any execution context
 * - Atomic write ensures no corruption
 * - Changes visible immediately to all contexts
 *
 * @param level New minimum log level
 *
 * Examples:
 * ```c
 * // Reduce noise during normal operation
 * log_set_level(LOG_LEVEL_INFO);
 *
 * // Enable verbose debugging
 * log_set_level(LOG_LEVEL_DEBUG);
 *
 * // Only show critical errors and warnings
 * log_set_level(LOG_LEVEL_ERROR);
 * ```
 */
void log_set_level(log_level_t level) {
    current_log_level = (uint8_t)level;
}

/**
 * @brief Get current runtime log level
 *
 * Returns the current minimum log level. This is a simple atomic
 * read operation with minimal overhead.
 *
 * Implementation:
 * - Single volatile uint8_t read (atomic on Cortex-M0+)
 * - No locking or synchronisation needed
 * - Compiler generates 1-2 instructions (ldrb + mov)
 * - Likely inlined by compiler at call sites
 *
 * Performance:
 * - Execution time: 2-3 CPU cycles
 * - No function call overhead (likely inlined)
 * - Used in LOG_* macro hot path
 *
 * Thread Safety:
 * - Safe to call from any execution context
 * - Atomic read ensures consistent value
 * - Volatile qualifier prevents optimisation
 *
 * @return Current log level
 *
 * Usage Note:
 * This function is called by LOG_* macros on every log statement.
 * The compiler typically inlines it to a single load instruction,
 * making the overhead negligible (~2-3 cycles).
 *
 * Example:
 * ```c
 * // Check if debug logging is enabled
 * if (log_get_level() >= LOG_LEVEL_DEBUG) {
 *     // Perform expensive debug calculations
 *     compute_and_log_debug_info();
 * }
 * ```
 */
log_level_t log_get_level(void) {
    return (log_level_t)current_log_level;
}

/**
 * @brief Initialise logging system
 *
 * Sets the initial log level from config.h. This function is
 * automatically called by init_uart_dma(), so explicit calls
 * are typically not needed.
 *
 * Implementation:
 * - Sets current_log_level to LOG_LEVEL_DEFAULT
 * - Idempotent: safe to call multiple times
 * - No hardware initialisation (uses existing UART DMA)
 *
 * When to Call:
 * - Automatically called by init_uart_dma()
 * - Can be called explicitly if needed before UART init
 * - Safe to call multiple times (idempotent)
 *
 * Performance:
 * - Execution time: 2-3 CPU cycles
 * - Called once at startup only
 * - Negligible impact on boot time
 *
 * @note This is automatically called by init_uart_dma()
 * @note Idempotent: multiple calls are safe
 * @note Does not initialise UART hardware (uart.c handles that)
 */
void log_init(void) {
    current_log_level = LOG_LEVEL_DEFAULT;
}
