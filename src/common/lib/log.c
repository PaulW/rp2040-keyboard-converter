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
 * Performance Characteristics:
 *
 * Storage:
 * - 1 byte: current_log_level (static volatile)
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
 *
 * Thread Safety:
 * - Written only from main loop context via log_set_level()
 * - Cortex-M0+ provides atomic byte read; safe to read from any context
 * - If log_set_level() were called from an IRQ, __dmb() would be required
 *   after the write to guarantee visibility to other contexts
 *
 * Default Value:
 * - Initialised to LOG_LEVEL_DEFAULT from config.h
 * - Typically LOG_LEVEL_INFO (errors + info, no debug)
 * - Can be changed at runtime via log_set_level()
 */
static volatile uint8_t current_log_level = LOG_LEVEL_DEFAULT;

void log_set_level(log_level_t level) {
    current_log_level = (uint8_t)level;
}

/**
 * @brief Get current runtime log level
 *
 * Returns the current minimum log level.
 *
 * Implementation:
 * - Single volatile uint8_t read (atomic on Cortex-M0+)
 * - No locking or synchronisation needed
 *
 * Thread Safety:
 * - Safe to call from any execution context
 * - Atomic read ensures consistent value
 * - Volatile qualifier prevents compiler caching
 *
 * @return Current log level
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

void log_init(void) {
    current_log_level = LOG_LEVEL_DEFAULT;
}
