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
 * @file uart.h
 * @brief High-Performance DMA-Based UART Logging Interface
 * 
 * This header defines the interface for a high-performance, non-blocking UART
 * logging system designed specifically for real-time applications such as
 * keyboard protocol converters where timing is critical.
 * 
 * Key Features:
 * - **Configurable queue policies**: DROP, WAIT_FIXED, or WAIT_EXP behavior when queue full
 * - **Large message queue**: 64-entry buffer handles initialization bursts without loss
 * - **DMA-driven transmission**: Minimal CPU overhead during log output
 * - **stdio integration**: Works transparently with printf(), puts(), etc.
 * - **Performance optimized**: Direct memcpy() bypasses formatting overhead
 * - **Thread-safe**: Safe to call from any context including interrupts
 * - **Low priority**: Designed to not interfere with PIO or USB operations
 * - **Compile-time optimization**: Zero runtime overhead for policy selection
 * 
 * Usage:
 * ```c
 * // Initialize once during system startup
 * init_uart_dma();
 * 
 * // Use standard C library functions - they automatically use DMA transmission
 * printf("Debug: key pressed = 0x%02X\n", scancode);
 * puts("System initialized");
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
 * - UART_DMA_POLICY: Queue behavior selection
 *   - UART_DMA_POLICY_DROP: Always drop messages when queue full (real-time safe)
 *   - UART_DMA_POLICY_WAIT_FIXED: Poll with tight loop until timeout
 *   - UART_DMA_POLICY_WAIT_EXP: Exponential backoff delays (CPU-friendly)
 * - UART_DMA_WAIT_US: Maximum wait time for WAIT policies (microseconds)
 * 
 * Policy Trade-offs:
 * - DROP: Zero blocking, may lose messages under extreme load
 * - WAIT_FIXED: High CPU usage during waits, reliable message delivery
 * - WAIT_EXP: Low CPU usage during waits, reliable message delivery
 * 
 * Performance Characteristics:
 * - Queue operation: ~1-2µs (atomic index update with bitwise operations)
 * - DMA setup: ~5-10µs (when starting new transfer)
 * - Policy overhead: 0µs (DROP), variable (WAIT policies)
 * - Total overhead: Usually ~20µs for typical log messages
 * 
 * Limitations:
 * - Output only (no UART input support)
 * - Maximum 64 queued messages (fixed at compile time)
 * - Maximum 256 characters per message (fixed at compile time)
 * - Behavior when queue full depends on selected policy
 * 
 * Thread Safety:
 * All functions are designed to be safely called from any execution context:
 * - Main application loop
 * - Interrupt service routines (including PIO interrupts)
 * - DMA completion handlers
 * - USB callback functions
 * 
 * The implementation uses lock-free algorithms and relies on the RP2040's
 * atomic operations for 8-bit values to ensure thread safety without
 * explicit synchronization primitives.
 * 
 * Queue Policy Behavior:
 * - **DROP Policy**: Returns immediately if queue full (never blocks)
 * - **WAIT_FIXED Policy**: Polls continuously until space or timeout
 * - **WAIT_EXP Policy**: Progressive delays: 1µs → 2µs → 4µs → ... → 1024µs (capped)
 */

#ifndef UART_H
#define UART_H

#include "config.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Initialize DMA-Based UART Logging System
 * 
 * Sets up the complete UART logging infrastructure including:
 * - UART hardware configuration (baud rate, pins from config.h)
 * - DMA channel allocation and configuration
 * - Interrupt handler registration (low priority)
 * - stdio driver integration for transparent printf() support
 * 
 * This function must be called once during system initialization before
 * any logging operations. After initialization, all standard C library
 * output functions (printf, puts, putchar, etc.) will automatically
 * use the DMA-based transmission system with the configured queue policy.
 * 
 * Configuration Parameters (from config.h):
 * - UART_BAUD: Transmission baud rate
 * - UART_TX_PIN: GPIO pin for UART TX
 * - UART_DMA_POLICY: Queue full behavior policy
 * - UART_DMA_WAIT_US: Maximum wait time for WAIT policies
 * 
 * @note This function completely replaces the standard Pico SDK UART stdio
 * @note Must be called before any printf/logging operations
 * @note Safe to call multiple times (subsequent calls are ignored)
 * @note Queue policy behavior is determined at compile time for optimal performance
 */
void init_uart_dma();

#endif /* UART_H */