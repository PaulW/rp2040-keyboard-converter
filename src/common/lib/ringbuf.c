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

#include "ringbuf.h"

#include "bsp/board.h"

#define BUF_SIZE 16

typedef struct {
  uint8_t *buffer;
  uint8_t head;
  uint8_t tail;
  uint8_t size_mask;
} ringbuf_t;

static uint8_t __not_in_flash("buf") buf[BUF_SIZE];

static ringbuf_t __not_in_flash("rbuf") rbuf = {
    .buffer = buf,  // Cast to array of chars for initialization
    .head = 0,
    .tail = 0,
    .size_mask = BUF_SIZE - 1};

int16_t __time_critical_func(ringbuf_get)() {
  if (ringbuf_is_empty()) return -1;
  uint8_t data = rbuf.buffer[rbuf.tail];
  rbuf.tail++;
  rbuf.tail &= rbuf.size_mask;
  return data;
}

bool __time_critical_func(ringbuf_put)(uint8_t data) {
  if (ringbuf_is_full()) {
    return false;
  }
  rbuf.buffer[rbuf.head] = data;
  rbuf.head++;
  rbuf.head &= rbuf.size_mask;
  return true;
}

bool __time_critical_func(ringbuf_is_empty)() {
  return (rbuf.head == rbuf.tail);
}

bool __time_critical_func(ringbuf_is_full)() {
  return (((rbuf.head + 1) & rbuf.size_mask) == rbuf.tail);
}

void ringbuf_reset() {
  rbuf.head = 0;
  rbuf.tail = 0;
}
