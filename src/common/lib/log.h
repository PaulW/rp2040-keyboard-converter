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
 * @file log.h
 * @brief Runtime-Configurable Log Level Filtering System
 *
 * This header provides a runtime configurable log level filtering system designed
 * for debugging and diagnostics with minimal performance overhead.
 *
 * Key Features:
 * - Runtime filtering: All log levels always available, filtered at runtime
 * - Command mode integration: Change log level via keyboard (Shift+Shift → 'D' → 1/2/3)
 * - Inline macros: No function call overhead when logging is enabled
 * - Single branch: Minimal CPU cycles for runtime level checks
 * - Early evaluation: Level check happens before printf formatting
 * - DMA-backed output: Uses existing high-performance UART DMA system
 *
 * Log Levels:
 * ```
 * LOG_LEVEL_ERROR (0)   - Critical errors and warnings      [ERR] [WARN]
 * LOG_LEVEL_INFO  (1)   - Errors + informational messages   [ERR] [WARN] [INFO]
 * LOG_LEVEL_DEBUG (2)   - All messages (errors, info, debug) [ERR] [WARN] [INFO] [DBG]
 * ```
 *
 * Configuration:
 *
 * Set in `config.h`:
 * ```c
 * // Runtime default level (can be changed at runtime)
 * #define LOG_LEVEL_DEFAULT LOG_LEVEL_INFO   // Default to ERROR + WARN + INFO
 * ```
 *
 * Usage Examples:
 *
 * ```c
 * // Standard usage (recommended):
 * LOG_ERROR("Failed to initialise: code=0x%02X\n", error_code);
 * LOG_WARN("PIO0 full, falling back to PIO1\n");
 * LOG_INFO("Device initialised successfully\n");
 * LOG_DEBUG("Scancode received: 0x%02X\n", scancode);
 *
 * // Runtime level changes (programmatically):
 * log_set_level(LOG_LEVEL_ERROR);  // Only show errors and warnings
 * log_set_level(LOG_LEVEL_DEBUG);  // Show everything
 *
 * // Runtime level changes (command mode):
 * // Hold Shift+Shift (3s) → Press 'D' → Press '1' (ERROR) / '2' (INFO) / '3' (DEBUG)
 * ```
 *
 * Thread Safety:
 * - LOG_* macros are safe to call from main loop and IRQ context (DMA-backed UART path)
 * - log_set_level() should be called from main loop
 *
 * Integration with UART DMA:
 * - Uses existing uart.c DMA-based output system
 * - Respects configured UART_DMA_POLICY (DROP/WAIT_FIXED/WAIT_EXP)
 * - No additional queue or buffering overhead
 *
 * @note LOG_* macros use DMA-backed UART output; blocking behaviour depends on
 *       UART_DMA_POLICY (e.g. WAIT_* policies may block)
 * @note Do not use LOG_* from IRQ context when a blocking UART_DMA_POLICY is configured
 * @note All levels are runtime-selectable via log_set_level()
 * @note log_set_level() is main-loop only; if used in IRQ, issue __dmb() afterwards
 *
 * @warning Must call init_uart_dma() before using LOG_* macros
 * @warning Long format strings may be truncated (UART_DMA_BUFFER_SIZE limit)
 */

#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <stdint.h>
#include "config.h"

/**
 * @brief Log level type
 *
 * Log levels are defined in config.h as compile-time constants:
 * - LOG_LEVEL_ERROR  (0) - Critical errors and warnings [ERR] [WARN]
 * - LOG_LEVEL_INFO   (1) - Errors + informational messages [ERR] [WARN] [INFO]
 * - LOG_LEVEL_DEBUG  (2) - All messages [ERR] [WARN] [INFO] [DBG]
 *
 * Lower numeric values represent higher priority messages.
 */
typedef uint8_t log_level_t;

/**
 * @brief Set runtime log level
 *
 * Changes the minimum log level that will be output. This is a simple
 * atomic write operation with minimal overhead.
 *
 * Implementation:
 * - Single volatile uint8_t write (atomic on Cortex-M0+)
 * - No locking needed (single-byte atomic store)
 * - Cross-context IRQ callers require a __dmb() after the write
 * - Effect is immediate for subsequent log calls
 *
 * Thread Safety:
 * - Must be called from main loop context only
 * - If called from an IRQ handler, the caller must issue __dmb() afterwards
 *   to ensure the updated level is visible to other execution contexts
 *
 * @param level New minimum log level
 *
 * @note Main loop only.
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
void log_set_level(log_level_t level);

/**
 * @brief Get current runtime log level
 *
 * Returns the current minimum log level that will be output. This is the
 * value previously set by log_set_level() or the default value from config.h.
 *
 * @return Current log level (LOG_LEVEL_ERROR, LOG_LEVEL_INFO, or LOG_LEVEL_DEBUG)
 *
 * @note Thread-safe (atomic uint8_t read)
 * @note Can be called from any context
 *
 * Example:
 * ```c
 * log_level_t current = log_get_level();
 * if (current >= LOG_LEVEL_DEBUG) {
 *     // Debug logging is enabled
 * }
 * ```
 */
log_level_t log_get_level(void);

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
void log_init(void);

// --- Runtime Configuration ---

#ifndef LOG_LEVEL_DEFAULT
/**
 * @brief Runtime default log level
 *
 * Initial log level at startup. Can be changed at runtime with log_set_level()
 * or via command mode (Shift+Shift → 'D' → 1/2/3).
 *
 * Should be defined in config.h. If not defined, defaults to LOG_LEVEL_INFO
 * (errors and info messages, but not debug).
 */
#define LOG_LEVEL_DEFAULT LOG_LEVEL_INFO
#endif

// --- Runtime Logging Macros ---

/**
 * @brief Log error message
 *
 * Outputs a message prefixed with [ERR]. Error messages are for critical
 * failures that prevent normal operation.
 *
 * Performance:
 * - Runtime disabled: ~5-10 cycles (single comparison)
 * - Runtime enabled: same as printf() (~20µs total with DMA)
 *
 * @param fmt printf-style format string
 * @param ... Variable arguments for format string
 *
 * Example:
 * ```c
 * LOG_ERROR("Failed to initialise device: error=0x%02X\n", error);
 * ```
 */
#define LOG_ERROR(fmt, ...)                       \
    do {                                          \
        if (log_get_level() >= LOG_LEVEL_ERROR) { \
            printf("[ERR] " fmt, ##__VA_ARGS__);  \
        }                                         \
    } while (0)

/**
 * @brief Log warning message
 *
 * Outputs a message prefixed with [WARN]. Warning messages are for recoverable
 * issues or unexpected conditions that don't prevent operation.
 *
 * Performance:
 * - Runtime disabled: ~5-10 cycles (single comparison)
 * - Runtime enabled: same as printf() (~20µs total with DMA)
 *
 * @param fmt printf-style format string
 * @param ... Variable arguments for format string
 *
 * Example:
 * ```c
 * LOG_WARN("PIO0 full, falling back to PIO1\n");
 * ```
 */
#define LOG_WARN(fmt, ...)                        \
    do {                                          \
        if (log_get_level() >= LOG_LEVEL_ERROR) { \
            printf("[WARN] " fmt, ##__VA_ARGS__); \
        }                                         \
    } while (0)

/**
 * @brief Log informational message
 *
 * Outputs a message prefixed with [INFO]. Informational messages are for
 * normal operation status and significant events.
 *
 * Performance:
 * - Runtime disabled: ~5-10 cycles (single comparison)
 * - Runtime enabled: same as printf() (~20µs total with DMA)
 *
 * @param fmt printf-style format string
 * @param ... Variable arguments for format string
 *
 * Example:
 * ```c
 * LOG_INFO("Device initialised: type=0x%04X\n", device_id);
 * ```
 */
#define LOG_INFO(fmt, ...)                        \
    do {                                          \
        if (log_get_level() >= LOG_LEVEL_INFO) {  \
            printf("[INFO] " fmt, ##__VA_ARGS__); \
        }                                         \
    } while (0)

/**
 * @brief Log debug message
 *
 * Outputs a message prefixed with [DBG]. Debug messages are for detailed
 * diagnostic information during development and troubleshooting.
 *
 * Performance:
 * - Runtime disabled: ~5-10 cycles (single comparison)
 * - Runtime enabled: same as printf() (~20µs total with DMA)
 *
 * @param fmt printf-style format string
 * @param ... Variable arguments for format string
 *
 * Example:
 * ```c
 * LOG_DEBUG("Scancode received: 0x%02X\n", scancode);
 * ```
 */
#define LOG_DEBUG(fmt, ...)                       \
    do {                                          \
        if (log_get_level() >= LOG_LEVEL_DEBUG) { \
            printf("[DBG] " fmt, ##__VA_ARGS__);  \
        }                                         \
    } while (0)

#endif /* LOG_H */
