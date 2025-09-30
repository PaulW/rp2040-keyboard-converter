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
 * Determine whether the ring buffer is full.
 *
 * The buffer reserves one slot to distinguish full from empty (effective capacity: BUF_SIZE - 1).
 *
 * @return `true` if the ring buffer is full, `false` otherwise.
 */
bool ringbuf_is_full(void) { 
  return (((rbuf.head + 1) & rbuf.size_mask) == rbuf.tail); 
}

/**
 * Retrieve the next byte from the ring buffer.
 *
 * Caller must ensure the buffer is not empty before calling; behavior is undefined if invoked when empty.
 *
 * @returns The next byte from the buffer (0–255).
 */
uint8_t ringbuf_get(void) {
  uint8_t data = rbuf.buffer[rbuf.tail];
  rbuf.tail++;
  rbuf.tail &= rbuf.size_mask;
  return data;
}

/**
 * Insert a byte into the ring buffer and advance the head index.
 *
 * The caller must ensure the buffer has space before calling; if the buffer is
 * full this function logs an error and discards the provided byte. Safe to
 * call from an interrupt context while the main loop modifies the tail.
 *
 * @param data Byte to store in the buffer.
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
 * Reset the ring buffer to an empty state.
 *
 * Sets the head and tail indices to zero, discarding any buffered data.
 * This operation is not thread-safe and must only be called when interrupts
 * that may access the buffer are disabled (for example during initialization,
 * explicit reset/error handling, or when IRQs are temporarily disabled).
 *
 * @note All buffered data is discarded.
 * @note The underlying buffer contents are not modified; only the indices are reset.
 */
void ringbuf_reset(void) {
  rbuf.head = 0;
  rbuf.tail = 0;
}
