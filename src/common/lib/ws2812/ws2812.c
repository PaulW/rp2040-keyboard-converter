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

#include "ws2812.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "bsp/board.h"
#include "config.h"
#include "pio_helper.h"
#include "ws2812.pio.h"  // Generated from ws2812.pio at build time

uint ws2812_sm = 0;
uint ws2812_offset = 0;
PIO ws2812_pio = NULL;

/*
 * Private Functions
 */

/**
 * @brief Sets the color of an LED in the WS2812 LED strip.
 * This function adjusts the color value based on the brightness and color order settings of
 * the LED.  This allows for different LED types to be used with the same codebase, such as RGB,
 * GRB, and BGR LEDs.
 *
 * @param led_color The color value to set for the LED.  The color value should be in RGB format,
 *                  and specified as a 32-bit integer.

 * @return The adjusted color value based on brightness and color order settings for the LED.
 */
static inline uint32_t ws2812_set_color(uint32_t led_color) {
  // Adjust color values to set brightness
  uint8_t r = (led_color >> 16) & 0xFF;
  uint8_t g = (led_color >> 8) & 0xFF;
  uint8_t b = led_color & 0xFF;

#ifdef CONVERTER_LEDS_BRIGHTNESS
  // Adjust brightness based on CONVERTER_LEDS_BRIGHTNESS
  r = (r * CONVERTER_LEDS_BRIGHTNESS) / 20;
  g = (g * CONVERTER_LEDS_BRIGHTNESS) / 20;
  b = (b * CONVERTER_LEDS_BRIGHTNESS) / 20;
#endif

#ifdef CONVERTER_LEDS_TYPE
  // Apply color order based on CONVERTER_LEDS_TYPE
  switch (CONVERTER_LEDS_TYPE) {
    case LED_RBG:
      return (r << 16) | (b << 8) | g;
    case LED_GRB:
      return (g << 16) | (r << 8) | b;
    case LED_GBR:
      return (g << 16) | (b << 8) | r;
    case LED_BRG:
      return (b << 16) | (r << 8) | g;
    case LED_BGR:
      return (b << 16) | (g << 8) | r;
    default:  // LED_RGB
      return (r << 16) | (g << 8) | b;
  }
#endif
  // Default to RGB
  return (r << 16) | (g << 8) | b;
}

/*
 * Public Functions
 */

/**
 * @brief Illuminates the WS2812 LED strip with the specified color.
 * This function sets the color of a single LED in the WS2812 LED strip (where applicable). It is
 * called in a loop to update the entire LED strip for the total number of LEDs present. The assumed
 * color value sent to the LED should already be adjusted for brightness and color order (see
 * ws2812_set_color). Please refer to the datasheet for the WS2812 LED strip for more information on
 * timing requirements.
 *
 * @param led_color The color value to set for the LED.
 */
void ws2812_show(uint32_t led_color) {
  pio_sm_put_blocking(ws2812_pio, ws2812_sm, ws2812_set_color(led_color) << 8u);
}

/**
 * @brief Sets up the WS2812 interface.
 * This function sets up the WS2812 interface by finding an available PIO, claiming an unused
 * state machine (SM), adding the WS2812 program to the PIO, initializing the program with the
 * specified parameters, and printing information about the setup.
 *
 * @param led_pin The GPIO pin connected to the WS2812 LED strip.
 */
void ws2812_setup(uint led_pin) {
  ws2812_pio = find_available_pio(&ws2812_program);
  if (ws2812_pio == NULL) {
    printf("[ERR] No PIO available for WS2812 Program\n");
    return;
  }

  ws2812_sm = (uint)pio_claim_unused_sm(ws2812_pio, true);
  ws2812_offset = pio_add_program(ws2812_pio, &ws2812_program);

  float rp_clock_khz = 0.001 * clock_get_hz(clk_sys);
  float clock_div = roundf(rp_clock_khz / (800 * 10));

  printf("[INFO] Effective SM Clock Speed: %.2fkHz\n", (float)(rp_clock_khz / clock_div));

  ws2812_program_init(ws2812_pio, ws2812_sm, ws2812_offset, led_pin, clock_div);

  printf(
      "[INFO] PIO%d SM%d WS2812 Interface program loaded at offset %d with clock divider of %.2f\n",
      ws2812_pio == pio0 ? 0 : 1, ws2812_sm, ws2812_offset, clock_div);
}
