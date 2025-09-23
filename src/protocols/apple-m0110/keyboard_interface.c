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

#include "bsp/board.h"
#include "hardware/clocks.h"
#include "keyboard_interface.pio.h"
#include "led_helper.h"
#include "pio_helper.h"
#include "ringbuf.h"
#include "scancode.h"

unsigned int keyboard_sm = 0;
unsigned int keyboard_offset = 0;
PIO keyboard_pio = pio1;
unsigned int keyboard_data_pin;

static enum {
  UNINITIALISED,
  INIT_MODEL_REQUEST,
  INIT_COMPLETE,
  INQUIRY,
  INQUIRY_RESPONSE
} keyboard_state = UNINITIALISED;

static uint32_t last_command_time = 0;
static uint8_t model_retry_count = 0;
static uint32_t last_inquiry_time = 0;
static uint32_t last_response_time = 0;
static bool keyboard_detected = false;

/**
 * @brief Sends a command to the Apple M0110 keyboard.
 * 
 * @param command The command byte to send
 */
void keyboard_command_handler(uint8_t command) {
  if (pio_sm_is_tx_fifo_full(keyboard_pio, keyboard_sm)) {
    printf("[WARN] M0110 TX FIFO full, command 0x%02X dropped\n", command);
    return; // TX FIFO is full, cannot send
  }
  
  // printf("[DBG] M0110 sending command: 0x%02X\n", command);
  // With right shift configuration, put data in lower 8 bits for MSB-first transmission
  pio_sm_put(keyboard_pio, keyboard_sm, (uint32_t)command << 24);
}

/**
 * @brief Processes keyboard event data from Apple M0110.
 * 
 * @param data_byte The data byte received from the keyboard
 */
static void keyboard_event_processor(uint8_t data_byte) {
  // printf("[DBG] M0110 received: 0x%02X\n", data_byte);
  last_response_time = board_millis(); // Update response time for any received data
  
  switch (keyboard_state) {
    case INIT_MODEL_REQUEST:
      switch (data_byte) {
        case M0110_RESP_MODEL_M0110:
          printf("[INFO] Apple M0110 Keyboard Model: M0110 - keyboard reset and ready\n");
          keyboard_detected = true;
          keyboard_state = INIT_COMPLETE;
          break;
        case M0110_RESP_MODEL_M0110A:
          printf("[INFO] Apple M0110 Keyboard Model: M0110A - keyboard reset and ready\n");
          keyboard_detected = true;
          keyboard_state = INIT_COMPLETE;
          break;
        default:
          printf("[DBG] Unknown model response: 0x%02X\n", data_byte);
          // Still proceed to complete initialization
          keyboard_detected = true;
          keyboard_state = INIT_COMPLETE;
          break;
      }
      break;
      
    case INQUIRY_RESPONSE:
      // We're expecting key data or null responses
      switch (data_byte) {
        case M0110_RESP_NULL:
          // Null response - keyboard is still there but no key pressed
          keyboard_state = INQUIRY; // Go back to sending inquiries
          break;
        default:
          if (!ringbuf_is_full()) {
            ringbuf_put(data_byte);
          }
      }

      default:
        break;
  }
  
#ifdef CONVERTER_LEDS
  converter.state.kb_ready = (keyboard_state >= INIT_COMPLETE) ? 1 : 0;
  update_converter_status();
#endif
}

/**
 * @brief IRQ handler for Apple M0110 keyboard interface.
 */
static void __isr keyboard_input_event_handler(void) {
  // Simply read one byte from RX FIFO and process it
  uint8_t data_byte = (uint8_t)pio_sm_get(keyboard_pio, keyboard_sm);
  keyboard_event_processor(data_byte);
}

/**
 * @brief Main task function for Apple M0110 keyboard interface.
 */
void keyboard_interface_task(void) {
  uint32_t current_time = board_millis();
  
  switch (keyboard_state) {
    case UNINITIALISED:
      if (current_time - last_command_time > 1000) { // Wait 1 second before first model request
        printf("[INFO] Starting Apple M0110 keyboard detection with Model Number command\n");
        keyboard_state = INIT_MODEL_REQUEST;
        keyboard_command_handler(M0110_CMD_MODEL);
        last_command_time = current_time;
        model_retry_count = 0;
      }
      break;
      
    case INIT_MODEL_REQUEST:
      if (current_time - last_command_time > 500) { // 500ms timeout (1/2 second)
        if (model_retry_count < 5) {
          printf("[DBG] Retrying Model Number command (%d/5)\n", model_retry_count + 1);
          keyboard_command_handler(M0110_CMD_MODEL);
          last_command_time = current_time;
          model_retry_count++;
        } else {
          printf("[ERR] Apple M0110 keyboard not responding to Model Number command after 5 attempts\n");
          keyboard_state = UNINITIALISED;
          last_command_time = current_time;
        }
      }
      break;
      
    case INIT_COMPLETE:
      printf("[INFO] Apple M0110 keyboard initialization complete - beginning normal operation\n");
      keyboard_state = INQUIRY;
      last_inquiry_time = current_time;
      last_response_time = current_time;
      break;
      
    case INQUIRY:
      // printf("[DBG] Sending Inquiry command to Apple M0110 keyboard\n");
      keyboard_command_handler(M0110_CMD_INQUIRY);
      keyboard_state = INQUIRY_RESPONSE;
      last_inquiry_time = current_time;
      break;

    case INQUIRY_RESPONSE:
      if (current_time - last_response_time > 500) { // 500ms timeout (1/2 second)
        printf("[WARN] No response from keyboard within 1/2 second - restarting with Model Number\n");
        keyboard_state = UNINITIALISED;
        last_command_time = current_time;
        keyboard_detected = false;
        break;
      }

      if (!ringbuf_is_empty() && tud_hid_ready()) {
        int c = ringbuf_get();
        if (c != -1) {
          printf("[DBG] Processing scancode: 0x%02X\n", (uint8_t)c);
          keyboard_state = INQUIRY; // Go back to sending inquiries
          // process_scancode((uint8_t)c);
        }
      }
      // Process ring buffer if we have data and HID is ready
      // if (!ringbuf_is_empty() && tud_hid_ready()) {
      //   int c = ringbuf_get();
      //   if (c != -1) {
      //     process_scancode((uint8_t)c);
      //   }
      // }
      
      // // Send Inquiry command every 1/4 second (250ms) in normal operation
      // if (current_time - last_inquiry_time >= 250) {
      //   keyboard_command_handler(M0110_CMD_INQUIRY);
      //   last_inquiry_time = current_time;
      // }
      
      // // If no response from keyboard within 1/2 second, assume keyboard missing
      // if (current_time - last_response_time > 500) {
      //   printf("[WARN] No response from keyboard within 1/2 second - restarting with Model Number\n");
      //   keyboard_state = UNINITIALISED;
      //   last_command_time = current_time;
      //   keyboard_detected = false;
      // }
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
void keyboard_interface_setup(uint data_pin) {
#ifdef CONVERTER_LEDS
  converter.state.kb_ready = 0;
  update_converter_status();
#endif

  ringbuf_reset();
  
  // Find available PIO
  keyboard_pio = find_available_pio(&keyboard_interface_program);
  if (keyboard_pio == NULL) {
    printf("[ERR] No PIO available for Apple M0110 Interface Program\n");
    return;
  }
  
  // Claim PIO resources
  keyboard_sm = (unsigned int)pio_claim_unused_sm(keyboard_pio, true);
  keyboard_offset = (unsigned int)pio_add_program(keyboard_pio, &keyboard_interface_program);
  keyboard_data_pin = data_pin;
  
  // Setup IRQ
  uint pio_irq = keyboard_pio == pio0 ? PIO0_IRQ_0 : PIO1_IRQ_0;
  
  // Apple M0110 timing requirements:
  // KEYBOARD→HOST: 330µs cycles (160µs low, 170µs high) ≈ 3.03kHz
  // HOST→KEYBOARD: 400µs cycles (180µs low, 220µs high) ≈ 2.5kHz  
  // Request-to-send: 840µs DATA low period
  // DATA hold: 80µs after command
  float clock_div = calculate_clock_divider(160); // 160µs minimum clock pulse width for reliable detection

  keyboard_interface_program_init(keyboard_pio, keyboard_sm, keyboard_offset, 
                                     data_pin, clock_div);
  
  irq_set_exclusive_handler(pio_irq, &keyboard_input_event_handler);
  irq_set_enabled(pio_irq, true);
  
  printf("[INFO] PIO%d SM%u Apple M0110 Interface program loaded at offset %u with clock divider of %.2f\n",
         (keyboard_pio == pio0 ? 0 : 1), keyboard_sm, keyboard_offset, clock_div);
}