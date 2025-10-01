/*
 * This file is part of RP2040 Keyboard Converter.
 *
 * Copyright 2023 Paul Bramhall (paulwamp@gmail.com)
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

#include "ringbuf.h"

#include <stdio.h>

#define BUF_SIZE 32  // Increased from 16 for better safety margin

typedef struct {
  uint8_t *buffer;
  volatile uint8_t head;  // Modified by IRQ, read by main loop - must be volatile
  volatile uint8_t tail;  // Modified by main loop, read by IRQ - must be volatile
  uint8_t size_mask;      // Constant after initialization - no volatile needed
} ringbuf_t;

static uint8_t buf[BUF_SIZE];

static ringbuf_t rbuf = {.buffer = buf,
                         .head = 0,
                         .tail = 0,
                         .size_mask = BUF_SIZE - 1};

/**
 * @brief Checks if the ring buffer is empty.
 * 
 * Thread-safe read operation using volatile head/tail pointers.
 * Can be called from both IRQ and main loop contexts.
 * 
 * @return true if the ring buffer is empty, false otherwise.
 */
bool ringbuf_is_empty(void) { 
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
 * (effective capacity: BUF_SIZE - 1).
 * 
 * @return true if the ring buffer is full, false otherwise.
 */
bool ringbuf_is_full(void) { 
  return (((rbuf.head + 1) & rbuf.size_mask) == rbuf.tail); 
}

/**
 * @brief Retrieves the next element from the ring buffer.
 * 
 * CRITICAL: Caller MUST check ringbuf_is_empty() before calling this function.
 * This function assumes the buffer is not empty and does NOT perform redundant checks.
 * 
 * Thread-safety: Safe when called from main loop while IRQ modifies head pointer.
 * The volatile qualifier ensures tail writes are visible to IRQ context.
 * 
 * @return The next element from the ring buffer (0-255).
 * @note Returns 0 if buffer is empty (undefined behavior - caller's responsibility)
 */
uint8_t ringbuf_get(void) {
  uint8_t data = rbuf.buffer[rbuf.tail];
  rbuf.tail++;
  rbuf.tail &= rbuf.size_mask;
  return data;
}

/**
 * @brief Puts a byte of data into the ring buffer.
 * 
 * CRITICAL: Caller MUST check ringbuf_is_full() before calling this function.
 * This function assumes the buffer has space and does NOT perform redundant checks.
 * 
 * Overflow Protection:
 * Defensive check logs error if called when buffer is full (API contract violation).
 * This should never happen if caller follows protocol, but provides safety net.
 * 
 * Thread-safety: Safe when called from IRQ while main loop modifies tail pointer.
 * The volatile qualifier ensures head writes are visible to main loop.
 * 
 * @param data The byte of data to be put into the ring buffer.
 */
void ringbuf_put(uint8_t data) {
  // Defensive check: should never trigger if caller checks is_full() first
  if (ringbuf_is_full()) {
    printf("[ERR] Ring buffer overflow! Lost scancode: 0x%02X\n", data);
    return;  // Discard data - buffer is full
  }
  
  rbuf.buffer[rbuf.head] = data;
  rbuf.head++;
  rbuf.head &= rbuf.size_mask;
}

/**
 * @brief Resets the ring buffer to empty state.
 * 
 * Clears all buffered data by resetting head and tail pointers to zero.
 * This operation is NOT thread-safe and should only be called when:
 * - During initialization (before IRQ is enabled)
 * - When keyboard state machine enters reset/error state
 * - When IRQs are temporarily disabled
 * 
 * WARNING: Do not call this function while IRQ handlers may be active,
 * as it modifies both head and tail pointers without synchronization.
 * 
 * @note All buffered scancodes will be discarded
 * @note Does not modify the buffer contents, only the pointers
 */
void ringbuf_reset(void) {
  rbuf.head = 0;
  rbuf.tail = 0;
}
