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

#include "ringbuf.h"

// Ring buffer storage
static uint8_t buf[RINGBUF_SIZE];

// Ring buffer structure (exposed via extern in header for inline functions)
ringbuf_t rbuf = {.buffer = buf, .head = 0, .tail = 0, .size_mask = RINGBUF_SIZE - 1};

/**
 * @brief Resets the ring buffer to empty state.
 *
 * Clears all buffered data by resetting head and tail pointers to zero.
 * This operation is NOT thread-safe and should only be called when:
 * - During initialisation (before IRQ is enabled)
 * - When keyboard state machine enters reset/error state
 * - When IRQs are temporarily disabled
 *
 * WARNING: Do not call this function whilst IRQ handlers may be active,
 * as it modifies both head and tail pointers without synchronisation.
 *
 * @note All buffered scancodes will be discarded
 * @note Does not modify the buffer contents, only the pointers
 */
void ringbuf_reset(void) {
    rbuf.head = 0;
    rbuf.tail = 0;
}
