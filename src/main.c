/*
 * This file is part of Model-F 5170 Converter.
 *
 * Copyright 2023 Paul Bramhall (paulwamp@gmail.com)
 *
 * Model-F 5170 Converter is free software: you can redistribute it
 * and/or modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * Model-F 5170 Converter is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Model-F 5170 Converter.
 * If not, see <https://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>

#include "bsp/board.h"
#include "buzzer.h"
#include "config.h"
#include "hid_interface.h"
#include "pcat_interface.h"
#include "ringbuf.h"
#include "tusb.h"

static uint8_t __not_in_flash("buf") buf[BUF_SIZE];
static ringbuf_t __not_in_flash("rbuf") rbuf = {
    .buffer = buf,
    .head = 0,
    .tail = 0,
    .size_mask = BUF_SIZE - 1};

void pcat_input_event_processor(unsigned char data_byte) {
  if (!ringbuf_is_full(&rbuf)) ringbuf_put(&rbuf, data_byte);
}

// Check and process any characters which may exist in the ringbuffer.
void keyboard_process_buffer(void) {
  if (!ringbuf_is_empty(&rbuf)) {
    if (tud_suspended()) {
      // Wake up host if we are in suspend mode
      // and REMOTE_WAKEUP feature is enabled by host
      tud_remote_wakeup();
    } else {
      if (tud_hid_ready()) {
        uint32_t status = save_and_disable_interrupts();  // disable IRQ
        int c = ringbuf_get(&rbuf);                       // critical_section
        restore_interrupts(status);
        if (c != -1) keyboard_process_key((uint8_t)c);
      }
    }
  }
}

int main(void) {
  ringbuf_reset(&rbuf);
  hid_device_setup();
  pcat_device_setup(DATA_PIN);
  buzzer_init(PIEZO_PIN);

  while (1) {
    keyboard_process_buffer();
    tud_task();
    pcat_lock_leds_handler();
  }

  return 0;
}
