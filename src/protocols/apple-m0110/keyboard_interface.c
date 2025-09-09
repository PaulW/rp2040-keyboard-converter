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

#include "keyboard_interface.h"

#include <math.h>
#include <stdio.h>

#include "bsp/board.h"
#include "hardware/clocks.h"
#include "interface.pio.h"
#include "led_helper.h"
#include "pio_helper.h"
#include "ringbuf.h"
#include "scancode.h"

unsigned int apple_m0110_sm = 0;
unsigned int apple_m0110_offset = 0;
PIO apple_m0110_pio = pio1;
unsigned int apple_m0110_data_pin;

static enum {
  UNINITIALISED,
  INIT_INQUIRY,
  INIT_MODEL_REQUEST,
  INIT_COMPLETE,
  OPERATIONAL
} apple_m0110_state = UNINITIALISED;

static uint32_t last_inquiry_time = 0;
static uint8_t inquiry_retry_count = 0;
static bool keyboard_detected = false;

/**
 * @brief Sends a command to the Apple M0110 keyboard.
 * 
 * @param command The command byte to send
 */
void apple_m0110_send_command(uint8_t command) {
  if (pio_sm_is_tx_fifo_full(apple_m0110_pio, apple_m0110_sm)) {
    printf("[WARN] M0110 TX FIFO full, command 0x%02X dropped\n", command);
    return; // TX FIFO is full, cannot send
  }
  
  printf("[DBG] M0110 sending command: 0x%02X\n", command);
  pio_sm_put(apple_m0110_pio, apple_m0110_sm, command);
}

/**
 * @brief Processes keyboard event data from Apple M0110.
 * 
 * @param data_byte The data byte received from the keyboard
 */
static void apple_m0110_keyboard_event_processor(uint8_t data_byte) {
  printf("[DBG] M0110 received: 0x%02X\n", data_byte);
  
  switch (apple_m0110_state) {
    case INIT_INQUIRY:
      if (data_byte == M0110_RESP_NULL) {
        printf("[INFO] Apple M0110 Keyboard detected\n");
        keyboard_detected = true;
        apple_m0110_state = INIT_MODEL_REQUEST;
        apple_m0110_send_command(M0110_CMD_MODEL);
      } else if (data_byte == M0110_RESP_KEYPAD) {
        printf("[INFO] Apple M0110 Keypad detected\n");
        keyboard_detected = true;
        apple_m0110_state = INIT_MODEL_REQUEST;
        apple_m0110_send_command(M0110_CMD_MODEL);
      } else {
        printf("[DBG] Unexpected response during inquiry: 0x%02X\n", data_byte);
      }
      break;
      
    case INIT_MODEL_REQUEST:
      switch (data_byte) {
        case M0110_RESP_MODEL_M0110:
          printf("[INFO] Apple M0110 Keyboard Model: M0110\n");
          apple_m0110_state = INIT_COMPLETE;
          break;
        case M0110_RESP_MODEL_M0110A:
          printf("[INFO] Apple M0110 Keyboard Model: M0110A\n");
          apple_m0110_state = INIT_COMPLETE;
          break;
        default:
          printf("[DBG] Unknown model response: 0x%02X\n", data_byte);
          apple_m0110_state = INIT_COMPLETE;
          break;
      }
      break;
      
    case OPERATIONAL:
      if (data_byte == M0110_RESP_NULL) {
        // No key pressed, ignore
        return;
      }
      
      // Valid key data, add to ring buffer
      if (!ringbuf_is_full()) {
        ringbuf_put(data_byte);
      }
      break;
      
    default:
      break;
  }
  
#ifdef CONVERTER_LEDS
  converter.state.kb_ready = (apple_m0110_state == OPERATIONAL) ? 1 : 0;
  update_converter_status();
#endif
}

/**
 * @brief IRQ handler for Apple M0110 keyboard interface.
 */
static void __isr apple_m0110_input_event_handler(void) {
  while (!pio_sm_is_rx_fifo_empty(apple_m0110_pio, apple_m0110_sm)) {
    uint8_t data_byte = (uint8_t)pio_sm_get(apple_m0110_pio, apple_m0110_sm);
    apple_m0110_keyboard_event_processor(data_byte);
  }
}

/**
 * @brief Main task function for Apple M0110 keyboard interface.
 */
void keyboard_interface_task(void) {
  uint32_t current_time = board_millis();
  
  switch (apple_m0110_state) {
    case UNINITIALISED:
      if (current_time - last_inquiry_time > 1000) { // Wait 1 second before first inquiry
        printf("[INFO] Starting Apple M0110 keyboard detection\n");
        apple_m0110_state = INIT_INQUIRY;
        apple_m0110_send_command(M0110_CMD_INQUIRY);
        last_inquiry_time = current_time;
        inquiry_retry_count = 0;
      }
      break;
      
    case INIT_INQUIRY:
      if (current_time - last_inquiry_time > 500) { // 500ms timeout
        if (inquiry_retry_count < 5) {
          printf("[DBG] Retrying keyboard inquiry (%d/5)\n", inquiry_retry_count + 1);
          apple_m0110_send_command(M0110_CMD_INQUIRY);
          last_inquiry_time = current_time;
          inquiry_retry_count++;
        } else {
          printf("[ERR] Apple M0110 keyboard not detected after 5 attempts\n");
          apple_m0110_state = UNINITIALISED;
          last_inquiry_time = current_time;
        }
      }
      break;
      
    case INIT_COMPLETE:
      printf("[INFO] Apple M0110 keyboard initialization complete\n");
      apple_m0110_state = OPERATIONAL;
      last_inquiry_time = current_time;
      break;
      
    case OPERATIONAL:
      // Process ring buffer if we have data and HID is ready
      if (!ringbuf_is_empty() && tud_hid_ready()) {
        int c = ringbuf_get();
        if (c != -1) {
          process_scancode((uint8_t)c);
        }
      }
      
      // Send periodic inquiry to check for key presses
      if (current_time - last_inquiry_time > 16) { // ~60Hz polling
        apple_m0110_send_command(M0110_CMD_INQUIRY);
        last_inquiry_time = current_time;
      }
      break;

    default:
      break;
  }
}

/**
 * @brief Initializes the Apple M0110 keyboard interface.
 * 
 * @param data_pin The GPIO pin connected to the DATA line (CLOCK is data_pin + 1)
 */
void keyboard_interface_setup(unsigned int data_pin) {
#ifdef CONVERTER_LEDS
  converter.state.kb_ready = 0;
  update_converter_status();
#endif

  ringbuf_reset();
  
  // Find available PIO
  apple_m0110_pio = find_available_pio(&apple_m0110_interface_program);
  if (apple_m0110_pio == NULL) {
    printf("[ERR] No PIO available for Apple M0110 Interface Program\n");
    return;
  }
  
  // Claim PIO resources
  apple_m0110_sm = (unsigned int)pio_claim_unused_sm(apple_m0110_pio, true);
  apple_m0110_offset = (unsigned int)pio_add_program(apple_m0110_pio, &apple_m0110_interface_program);
  apple_m0110_data_pin = data_pin;
  
  // Setup IRQ
  uint pio_irq = apple_m0110_pio == pio0 ? PIO0_IRQ_0 : PIO1_IRQ_0;
  
  // Apple M0110 timing requirements:
  // KEYBOARD→HOST: 330µs cycles (160µs low, 170µs high) ≈ 3.03kHz
  // HOST→KEYBOARD: 400µs cycles (180µs low, 220µs high) ≈ 2.5kHz  
  // Request-to-send: 840µs DATA low period
  // DATA hold: 80µs after command
  //
  // The M0110 protocol operates at much slower speeds than XT/AT, so we need
  // to calculate appropriate timing. The critical path is the 840µs request period
  // which needs 20 loop iterations × 2 cycles = 40 cycles total.
  // We want sufficient resolution for the timing loops while still being fast enough
  // to respond to the keyboard's clock edges reliably.

  // Set polling interval based on the faster of the two clock rates (330µs cycle)
  // We'll use 1/4 of the cycle time to ensure good responsiveness
  int polling_interval_us = 80;  // 1/4 of 330µs cycle for good responsiveness
  int cycles_per_clock = 8;      // Need enough cycles for timing loops and edge detection

  // Get base clock speed of RP2040 (typically 125MHz)
  float rp_clock_khz = 0.001 * clock_get_hz(clk_sys);

  printf("[INFO] RP2040 Clock Speed: %.0fkHz\n", rp_clock_khz);
  printf("[INFO] M0110 Interface Polling Interval: %dus\n", polling_interval_us);
  float polling_interval_khz = 1000.0 / polling_interval_us;
  printf("[INFO] M0110 Interface Polling Clock: %.0fkHz\n", polling_interval_khz);
  
  // Calculate clock divider using the same method as XT/AT protocols
  float clock_div = roundf((rp_clock_khz / polling_interval_khz) / cycles_per_clock);
  
  printf("[INFO] Clock Divider based on %d SM Cycles per M0110 Clock Cycle: %.2f\n",
         cycles_per_clock, clock_div);
  printf("[INFO] Effective SM Clock Speed: %.2fkHz\n", rp_clock_khz / clock_div);
  
  apple_m0110_interface_program_init(apple_m0110_pio, apple_m0110_sm, apple_m0110_offset, 
                                     data_pin, clock_div);
  
  irq_set_exclusive_handler(pio_irq, &apple_m0110_input_event_handler);
  irq_set_enabled(pio_irq, true);
  
  printf("[INFO] PIO%d SM%u Apple M0110 Interface program loaded at offset %u with clock divider of %.2f\n",
         (apple_m0110_pio == pio0 ? 0 : 1), apple_m0110_sm, apple_m0110_offset, clock_div);
}