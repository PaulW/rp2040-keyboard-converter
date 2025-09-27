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
 * - **Non-blocking operation**: All functions return immediately, never blocking
 * - **DMA-driven transmission**: Minimal CPU overhead during log output
 * - **stdio integration**: Works transparently with printf(), puts(), etc.
 * - **Thread-safe**: Safe to call from any context including interrupts
 * - **Low priority**: Designed to not interfere with PIO or USB operations
 * - **Graceful degradation**: Drops messages when overwhelmed rather than blocking
 * 
 * Usage:
 * ```c
 * // Initialize once during system startup
 * init_uart_dma();
 * 
 * // Use standard C library functions - they automatically use DMA
 * printf("Debug: key pressed = 0x%02X\n", scancode);
 * puts("System initialized");
 * 
 * // Or use direct DMA printf for specific formatting control
 * uart_dma_printf("Timing: %lu microseconds\n", time_us);
 * ```
 * 
 * Configuration:
 * The UART configuration (baud rate, pins) is defined in config.h:
 * - UART_BAUD: Transmission speed (typically 115200)
 * - UART_TX_PIN: GPIO pin for UART transmission
 * 
 * Performance Characteristics:
 * - Message formatting: ~10-50µs (depends on message complexity)
 * - Queue operation: ~1-2µs (atomic index update)
 * - DMA setup: ~5-10µs (when starting new transfer)
 * - Total overhead: Usually <100µs for typical log messages
 * 
 * Limitations:
 * - Output only (no UART input support)
 * - Maximum 16 queued messages (configurable in implementation)
 * - Maximum 256 characters per message (configurable in implementation)
 * - Messages dropped when queue is full (no blocking)
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
 */

#ifndef UART_H
#define UART_H

#include "config.h"
#include <stdint.h>

/**
 * @brief Initialize DMA-Based UART Logging System
 * 
 * Sets up the complete UART logging infrastructure including:
 * - UART hardware configuration (baud rate, pins)
 * - DMA channel allocation and configuration
 * - Interrupt handler registration
 * - stdio driver integration
 * 
 * This function must be called once during system initialization before
 * any logging operations. After initialization, all standard C library
 * output functions (printf, puts, putchar, etc.) will automatically
 * use the DMA-based transmission system.
 * 
 * Configuration is read from config.h:
 * - UART_BAUD: Transmission baud rate
 * - UART_TX_PIN: GPIO pin for UART TX
 * 
 * @note This function completely replaces the standard Pico SDK UART stdio
 * @note Must be called before any printf/logging operations
 * @note Safe to call multiple times (subsequent calls are ignored)
 */
void init_uart_dma();

#endif /* UART_H */
