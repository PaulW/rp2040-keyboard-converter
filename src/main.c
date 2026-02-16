/*
 * This file is part of RP2040 Keyboard Converter.
 *
 * Copyright 2023-2026 Paul Bramhall (paulwamp@gmail.com)
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

#include "pico/unique_id.h"

#include "bsp/board.h"
#include "tusb.h"

#include "command_mode.h"
#include "config.h"
#include "config_storage.h"
#include "hid_interface.h"
#include "keylayers.h"
#include "log.h"
#include "uart.h"

#if KEYBOARD_ENABLED
#include "keyboard.h"
#include "keyboard_interface.h"
#endif

#if MOUSE_ENABLED
#include "mouse_interface.h"
#endif

#ifdef CONVERTER_LEDS
#include "ws2812/ws2812.h"
#endif

int main(void) {
  hid_device_setup();
  init_uart_dma();
  command_mode_init();  // Initialize command mode system
  
  // Load persistent configuration from flash
  if (!config_init()) {
    LOG_WARN("Using factory default configuration\n");
  }
  
  // Apply saved log level
  log_set_level(config_get()->log_level);
  
  // Initialize layer system and restore saved layer state
  keylayers_init();
  
  char pico_unique_id[32];
  pico_get_unique_board_id_string(pico_unique_id, sizeof(pico_unique_id));
  LOG_INFO("--------------------------------\n");
  LOG_INFO("RP2040 Device Converter\n");
  LOG_INFO("RP2040 Serial ID: %s\n", pico_unique_id);
  LOG_INFO("Build Time: %s\n", BUILD_TIME);
  LOG_INFO("--------------------------------\n");

#ifdef CONVERTER_LEDS
  ws2812_setup(LED_PIN);  // Setup the WS2812 LEDs.
  
  // Apply saved LED brightness from configuration
  uint8_t saved_brightness = config_get_led_brightness();
  ws2812_set_brightness(saved_brightness);
  LOG_INFO("LED brightness set to %u (0-10 range)\n", saved_brightness);
#endif

  // Initialise aspects of the Interface.
#if KEYBOARD_ENABLED
  LOG_INFO("Keyboard Support Enabled\n");
  LOG_INFO("Keyboard Make: %s\n", KEYBOARD_MAKE);
  LOG_INFO("Keyboard Model: %s\n", KEYBOARD_MODEL);
  LOG_INFO("Keyboard Description: %s\n", KEYBOARD_DESCRIPTION);
  LOG_INFO("Keyboard Protocol: %s\n", KEYBOARD_PROTOCOL);
  LOG_INFO("Keyboard Scancode Set: %s\n", KEYBOARD_CODESET);
  LOG_INFO("--------------------------------\n");
  keyboard_interface_setup(KEYBOARD_DATA_PIN);  // Setup the keyboard interface.
#else
  LOG_INFO("Keyboard Support Disabled\n");
#endif

#if MOUSE_ENABLED
  LOG_INFO("Mouse Support Enabled\n");
  LOG_INFO("Mouse Protocol: %s\n", MOUSE_PROTOCOL);
  LOG_INFO("--------------------------------\n");
  mouse_interface_setup(MOUSE_DATA_PIN);  // Setup the mouse interface.
#else
  LOG_INFO("Mouse Support Disabled\n");
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
