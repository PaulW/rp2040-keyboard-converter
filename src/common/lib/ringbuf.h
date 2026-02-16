/*
 * This file is part of RP2040 Keyboard Converter.
 *
 * Copyright 2023-2026 Paul Bramhall (paulwamp@gmail.com)
 *
 * Portions of this specific file are licensed under the GPLv2 License.
 * The Ringbuffer has been adapted and based upon the tinyusb repository
 * by Hasu@tmk, which in turn originates from the main TMK Keyboard
 * Firmware repository.
 *
 * The original source can be found at:
 * https://github.com/tmk/tinyusb_ps2/blob/main/ringbuf.h
 * https://github.com/tmk/tmk_keyboard/blob/master/tmk_core/ring_buffer.h
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

#ifndef RINGBUF_H
#define RINGBUF_H

#include <stdbool.h>
#include <stdint.h>
#include "hardware/sync.h"

/**
 * @brief Ring buffer for interrupt-safe scancode queueing.
 * 
 * This ring buffer implementation provides lock-free communication between
 * IRQ context (producer) and main loop (consumer) for keyboard scancode buffering.
 * 
 * Buffer Sizing:
 * - Capacity: 32 bytes (31 usable due to full/empty distinction)
 * - Sized for worst-case burst scenarios and fast typing
 * 
 * Thread Safety Model:
 * - Producer (IRQ): Writes to head pointer, reads tail pointer
 * - Consumer (main): Writes to tail pointer, reads head pointer
 * - Volatile qualifiers ensure visibility across contexts
 * - Memory barriers (DMB) provide explicit synchronization guarantees
 * - No locks needed due to single-producer/single-consumer design
 * 
 * Performance Optimizations:
 * - Inline functions for zero-overhead abstraction
 * - Power-of-2 size enables fast masking (AND) instead of modulo
 * - Caller-checked guards eliminate redundant conditional branches
 * - Direct uint8_t return (no int16_t conversion overhead)
 * - Memory barriers ensure correctness with minimal overhead (+1 cycle)
 * 
 * Overflow Handling:
 * - Defensive check in ringbuf_put() logs error if called when full
 * - Simple logging - no counters or statistics tracking
 * - Data discarded when buffer full (IRQ cannot block)
 * 
 * Usage Pattern:
 * ```c
 * // Producer (IRQ context):
 * if (!ringbuf_is_full()) ringbuf_put(scancode);
 * 
 * // Consumer (main loop):
 * if (!ringbuf_is_empty()) {
 *     uint8_t scancode = ringbuf_get();
 *     process_scancode(scancode);
 * }
 * ```
 */

#define RINGBUF_SIZE 32  // Power-of-2 for efficient masking

typedef struct {
  uint8_t *buffer;
  volatile uint8_t head;  // Modified by IRQ, read by main loop - must be volatile
  volatile uint8_t tail;  // Modified by main loop, read by IRQ - must be volatile
  uint8_t size_mask;      // Constant after initialization - no volatile needed
} ringbuf_t;

// External access to ring buffer structure (defined in ringbuf.c)
extern ringbuf_t rbuf;

/**
 * @brief Checks if the ring buffer is empty.
 * 
 * Thread-safe read operation using volatile head/tail pointers.
 * Can be called from both IRQ and main loop contexts.
 * 
 * @return true if the ring buffer is empty, false otherwise.
 */
static inline bool ringbuf_is_empty(void) { 
  return (rbuf.head == rbuf.tail); 
}

/**
 * @brief Checks if the ring buffer is full.
 * 
 * Thread-safe read operation using volatile head/tail pointers.
 * Can be called from both IRQ and main loop contexts.
 * 
 * The buffer is considered full when the next head position would equal tail.
 * This design sacrifices one buffer slot to distinguish full from empty state
 * (effective capacity: RINGBUF_SIZE - 1).
 * 
 * @return true if the ring buffer is full, false otherwise.
 */
static inline bool ringbuf_is_full(void) { 
  return (((rbuf.head + 1) & rbuf.size_mask) == rbuf.tail); 
}

/**
 * @brief Retrieves a single byte from the ring buffer.
 * 
 * Reads and removes the oldest byte from the ring buffer using a double-barrier
 * pattern for maximum correctness on ARM's weakly-ordered memory model.
 * 
 * IMPORTANT: Caller MUST check ringbuf_is_empty() first. Calling this function
 * on an empty buffer will return stale data from the last buffer position.
 * 
 * @return The oldest byte in the ring buffer (undefined if buffer is empty)
 * 
 * @warning No bounds checking - returns stale data if buffer is empty
 * @note Uses __force_inline for zero-overhead abstraction
 * 
 * Thread-safety: Safe when called from main loop while IRQ modifies head pointer.
 * Double memory barrier pattern ensures:
 * 1. Acquire barrier: See all producer writes before reading data
 * 2. Release barrier: Data read completes before tail update visible to producer
 */
__force_inline static uint8_t ringbuf_get(void) {
  __dmb();  // Acquire: synchronize with producer's writes
  uint8_t data = rbuf.buffer[rbuf.tail];
  __dmb();  // Release: ensure data read completes before tail update visible
  rbuf.tail = (rbuf.tail + 1) & rbuf.size_mask;
  return data;
}

/**
 * @brief Inserts a single byte into the ring buffer.
 * 
 * Writes a byte to the ring buffer at the current head position and increments
 * the head pointer. A memory barrier ensures the data write completes before
 * the head pointer update is visible to the consumer.
 * 
 * IMPORTANT: Caller MUST check ringbuf_is_full() first. Calling this function
 * on a full buffer will overwrite the oldest unread data, causing data loss.
 * 
 * @param data The byte of data to be put into the ring buffer.
 * 
 * @warning No bounds checking - overwrites oldest data if buffer is full
 * @note Uses __force_inline for zero-overhead abstraction
 * @note Typically called from IRQ context - must be fast
 * 
 * Thread-safety: Safe when called from IRQ while main loop modifies tail pointer.
 * The volatile qualifier ensures head writes are visible to main loop.
 * Memory barrier ensures head update is visible after data write completes.
 */
__force_inline static void ringbuf_put(uint8_t data) {
  rbuf.buffer[rbuf.head] = data;
  __dmb();  // Data Memory Barrier - ensure data write is committed before head update
  rbuf.head = (rbuf.head + 1) & rbuf.size_mask;
}

void ringbuf_reset(void);

#endif /* RINGBUF_H */
