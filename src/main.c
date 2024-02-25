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

#if !PICO_NO_FLASH && !PICO_COPY_TO_RAM
#error "This must be built to run from SRAM!"
#endif

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
  printf("[INFO] Keyboard Scancode Set: %s\n", KEYBOARD_CODESET);
  printf("[INFO] RP2040 Serial ID: %s\n", pico_unique_id);
  printf("--------------------------------\n");

  // Initialise aspects of the program.
  ringbuf_reset();                     // Even though Ringbuf is statically initialised, we reset it here to be sure it's empty.
  keyboard_interface_setup(DATA_PIN);  // Setup the keyboard interface.
  buzzer_init(PIEZO_PIN);              // Setup the buzzer.
  printf("--------------------------------\n");
  while (1) {
    keyboard_interface_task();  // Keyboard interface task.
    tud_task();                 // TinyUSB device task.
  }

  return 0;
}
