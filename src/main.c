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

#include <stdio.h>
#include <stdlib.h>

#include "bsp/board.h"
#include "buzzer.h"
#include "config.h"
#include "hid_interface.h"
#include "interface.h"
#include "keyboard.h"
#include "pico/unique_id.h"
#include "ringbuf.h"
#include "tusb.h"

static uint8_t __not_in_flash("buf") buf[BUF_SIZE];
static ringbuf_t __not_in_flash("rbuf") rbuf = {
    .buffer = buf,
    .head = 0,
    .tail = 0,
    .size_mask = BUF_SIZE - 1};

void __time_critical_func(kbd_input_event_processor)(unsigned char data_byte) {
  if (!ringbuf_is_full(&rbuf)) ringbuf_put(&rbuf, data_byte);
}

// Check and process any characters which may exist in the ringbuffer.
void __time_critical_func(keyboard_process_buffer)(void) {
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
  hid_device_setup();
  char pico_unique_id[32];
  pico_get_unique_board_id_string(pico_unique_id, sizeof(pico_unique_id));
  printf("--------------------------------\n");
  printf("[INFO] RP2040 Keyboard Converter\n");
  printf("[INFO] Build Time: %s\n", BUILD_TIME);
  printf("--------------------------------\n");
  printf("[INFO] Keyboard Make: %s\n", KEYBOARD_MAKE);
  printf("[INFO] Keyboard Model: %s\n", KEYBOARD_MODEL);
  printf("[INFO] Keyboard Description: %s\n", KEYBOARD_DESCRIPTION);
  printf("[INFO] Keyboard Protocol: %s\n", KEYBOARD_PROTOCOL);
  printf("[INFO] RP2040 Serial ID: %s\n", pico_unique_id);
  printf("--------------------------------\n");
  ringbuf_reset(&rbuf);
  keyboard_interface_setup(DATA_PIN);
  buzzer_init(PIEZO_PIN);
  printf("--------------------------------\n");
  while (1) {
    keyboard_process_buffer();
    tud_task();
    keyboard_interface_task();
  }

  return 0;
}
