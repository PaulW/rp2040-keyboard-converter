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

#ifndef RINGBUF_H
#define RINGBUF_H

#include <stdbool.h>
#include <stdint.h>

int16_t ringbuf_get();
bool ringbuf_put(uint8_t data);
bool ringbuf_is_empty();
bool ringbuf_is_full();
void ringbuf_reset();

#endif /* RINGBUF_H */
