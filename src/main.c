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
#include "command_mode.h"
#include "config.h"
#include "hid_interface.h"
#include "pico/unique_id.h"
#include "tusb.h"
#include "uart.h"

#if KEYBOARD_ENABLED
#include "keyboard.h"
#include "keyboard_interface.h"
#endif

#if MOUSE_ENABLED
#include "mouse_interface.h"
#endif

// The following includes are optional and are only included if the relevant features are enabled.
#ifdef CONVERTER_PIEZO
#include "buzzer.h"
#endif
#ifdef CONVERTER_LEDS
#include "ws2812/ws2812.h"
#endif

int main(void) {
  hid_device_setup();
  init_uart_dma();
  command_mode_init();  // Initialize command mode system
  char pico_unique_id[32];
  pico_get_unique_board_id_string(pico_unique_id, sizeof(pico_unique_id));
  printf("--------------------------------\n");
  printf("[INFO] RP2040 Device Converter\n");
  printf("[INFO] RP2040 Serial ID: %s\n", pico_unique_id);
  printf("[INFO] Build Time: %s\n", BUILD_TIME);
  printf("--------------------------------\n");

  // Initialise Optional Components
#ifdef CONVERTER_PIEZO
  buzzer_init(PIEZO_PIN);  // Setup the buzzer.
#endif

#ifdef CONVERTER_LEDS
  ws2812_setup(LED_PIN);  // Setup the WS2812 LEDs.
#endif

  // Initialise aspects of the Interface.
#if KEYBOARD_ENABLED
  printf("[INFO] Keyboard Support Enabled\n");
  printf("[INFO] Keyboard Make: %s\n", KEYBOARD_MAKE);
  printf("[INFO] Keyboard Model: %s\n", KEYBOARD_MODEL);
  printf("[INFO] Keyboard Description: %s\n", KEYBOARD_DESCRIPTION);
  printf("[INFO] Keyboard Protocol: %s\n", KEYBOARD_PROTOCOL);
  printf("[INFO] Keyboard Scancode Set: %s\n", KEYBOARD_CODESET);
  printf("--------------------------------\n");
  keyboard_interface_setup(KEYBOARD_DATA_PIN);  // Setup the keyboard interface.
#else
  printf("[INFO] Keyboard Support Disabled\n");
#endif

#if MOUSE_ENABLED
  printf("[INFO] Mouse Support Enabled\n");
  printf("[INFO] Mouse Protocol: %s\n", MOUSE_PROTOCOL);
  printf("--------------------------------\n");
  mouse_interface_setup(MOUSE_DATA_PIN);  // Setup the mouse interface.
#else
  printf("[INFO] Mouse Support Disabled\n");
#endif

  // Main loop: run interface tasks and USB stack continuously.
  while (1) {
#if KEYBOARD_ENABLED
    keyboard_interface_task();  // Keyboard interface task.
#endif
#if MOUSE_ENABLED
    mouse_interface_task();  // Mouse interface task.
#endif
    command_mode_task();  // Command mode LED updates and timeout handling
    tud_task();  // TinyUSB device task.
  }

  return 0;
}
