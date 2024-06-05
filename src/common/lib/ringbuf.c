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

#define BUF_SIZE 16

typedef struct {
  uint8_t *buffer;
  uint8_t head;
  uint8_t tail;
  uint8_t size_mask;
} ringbuf_t;

static uint8_t buf[BUF_SIZE];

static ringbuf_t rbuf = {.buffer = buf,  // Cast to array of chars for initialization
                         .head = 0,
                         .tail = 0,
                         .size_mask = BUF_SIZE - 1};

/**
 * @brief Retrieves the next element from the ring buffer.
 * This function retrieves the next element from the ring buffer. If the buffer is empty, it will
 * returns -1.
 *
 * @return The next element from the ring buffer, or -1 if the buffer is empty.
 */
int16_t ringbuf_get() {
  if (ringbuf_is_empty()) return -1;
  uint8_t data = rbuf.buffer[rbuf.tail];
  rbuf.tail++;
  rbuf.tail &= rbuf.size_mask;
  return data;
}

/**
 * @brief Puts a byte of data into the ring buffer.
 * This function puts a byte of data into the ring buffer if it is not full.
 *
 * @param data The byte of data to be put into the ring buffer.
 *
 * @return Returns true if the data was successfully put into the ring buffer, false if the ring
 *         buffer is full.
 */
bool ringbuf_put(uint8_t data) {
  if (ringbuf_is_full()) {
    return false;
  }
  rbuf.buffer[rbuf.head] = data;
  rbuf.head++;
  rbuf.head &= rbuf.size_mask;
  return true;
}

/**
 * @brief Checks if the ring buffer is empty.
 * This function checks if the ring buffer is empty by comparing the head and tail indices.
 *
 * @return true if the ring buffer is empty, false otherwise.
 */
bool ringbuf_is_empty() { return (rbuf.head == rbuf.tail); }

/**
 * @brief Checks if the ring buffer is full.
 * This function checks whether the ring buffer is full by comparing the head and tail indices. If
 * the head index plus one (wrapped around by the size mask) is equal to the tail index, it means
 * the buffer is full.
 *
 * @return true if the ring buffer is full, false otherwise.
 */
bool ringbuf_is_full() { return (((rbuf.head + 1) & rbuf.size_mask) == rbuf.tail); }

/**
 * @brief Resets the ring buffer.
 * This function resets the head and tail pointers of the ring buffer to 0, effectively clearing the
 * buffer.
 */
void ringbuf_reset() {
  rbuf.head = 0;
  rbuf.tail = 0;
}
