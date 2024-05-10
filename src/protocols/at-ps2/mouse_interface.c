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

#include "mouse_interface.h"

#include <math.h>

#include "bsp/board.h"
#include "common_interface.h"
#include "hardware/clocks.h"
#include "hid_interface.h"
#include "interface.pio.h"
#include "led_helper.h"
#include "pio_helper.h"

uint mouse_sm = 0;
uint mouse_offset = 0;
PIO mouse_pio;
uint8_t mouse_id = 0xFF;
uint mouse_data_pin;

static enum {
  UNINITIALISED,
  INIT_AWAIT_ACK,
  INIT_AWAIT_SELFTEST,
  INIT_AWAIT_ID,
  INIT_DETECT_MOUSE_TYPE,
  INIT_SET_CONFIG,
  INITIALISED,
} mouse_state = UNINITIALISED;

typedef enum {
  BUTTON_LEFT,
  BUTTON_RIGHT,
  BUTTON_MIDDLE,
  BUTTON_BACKWARD,
  BUTTON_FORWARD
} button_index;

typedef enum {
  X_SIGN,
  Y_SIGN,
  X_OVERFLOW,
  Y_OVERFLOW,
} parameter_index;

typedef enum {
  X_POS,
  Y_POS,
  Z_POS
} mousepos_index;

// Command Handler function to issue commands to the attached AT/PS2 Mouse.
static void
mouse_command_handler(uint8_t data_byte) {
  uint16_t data_with_parity = (uint16_t)(data_byte + (interface_parity_table[data_byte] << 8));
  pio_sm_put(mouse_pio, mouse_sm, data_with_parity);
}

int8_t get_xy_movement(uint8_t pos, int sign_bit) {
  int16_t new_pos = pos;
  if (new_pos && sign_bit) {
    new_pos -= 0x100;
  }
  new_pos = (new_pos > 127) ? 127 : (new_pos < -127) ? -127
                                                     : new_pos;
  return (int8_t)new_pos;
}

int8_t get_z_movement(uint8_t pos) {
  int8_t z_pos = pos & 0xF;
  if (pos) {
    z_pos -= 8;
  }

  return z_pos;
}

void mouse_event_processor(uint8_t data_byte) {
  static uint8_t mouse_type_detect_sequence = 0;
  static uint8_t mouse_max_packets = 0;

  switch (mouse_state) {
    case UNINITIALISED:
      switch (data_byte) {
        case 0xAA:  // Self Test Passed
          printf("[INFO] Mouse Self Test Passed\n");
          printf("[INFO] Detecting Mouse Type\n");
          mouse_id = 0xFF;  // Reset Mouse ID
          mouse_state = INIT_AWAIT_ID;
          break;
        default:
          // If we hit this, then either the self test failed, or we have an error.
          printf("[ERR] Asking Mouse to Reset\n");
          mouse_state = INIT_AWAIT_ACK;
          mouse_command_handler(0xFF);
      }
      break;
    case INIT_AWAIT_ACK:
      switch (data_byte) {
        case 0xFA:  // Acknowledged
          printf("[INFO] ACK Received after Reset\n");
          mouse_state = INIT_AWAIT_SELFTEST;
          break;
        default:
          printf("[DBG] Unknown ACK Response (0x%02X).  Asking again to Reset...\n", data_byte);
          mouse_command_handler(0xFF);
      }
      break;
    case INIT_AWAIT_SELFTEST:
      switch (data_byte) {
        case 0xAA:  // Self Test Passed
          printf("[INFO] Mouse Self Test Passed\n");
          printf("[INFO] Detecting Mouse Type\n");
          mouse_id = 0xFF;  // Reset Mouse ID
          mouse_state = INIT_AWAIT_ID;
          break;
        default:
          printf("[DBG] Self-Test invalid response (0x%02X).  Asking again to Reset...\n", data_byte);
          mouse_state = INIT_AWAIT_ACK;
          mouse_command_handler(0xFF);
      }
      break;
    case INIT_AWAIT_ID:
      switch (data_byte) {
        case 0xFA:
          // Ack Received, silently ignore
          break;
        case 0x00:
          if (mouse_id == 0xFF) {
            mouse_id = 0x00;
            mouse_type_detect_sequence = 0;
            mouse_state = INIT_DETECT_MOUSE_TYPE;
            mouse_command_handler(0xF3);
          } else {
            printf("[INFO] Mouse Type: Standard PS/2 Mouse\n");
            mouse_max_packets = 3;
            mouse_state = INIT_SET_CONFIG;
            mouse_command_handler(0xE8);
          }
          break;
        case 0x03:
          if (mouse_id == 0x00) {
            mouse_id = 0x03;
            mouse_state = INIT_DETECT_MOUSE_TYPE;
            mouse_command_handler(0xF3);
          } else {
            printf("[INFO] Mouse Type: Mouse with Scroll Wheel\n");
            mouse_max_packets = 4;
            mouse_state = INIT_SET_CONFIG;
            mouse_command_handler(0xE8);
          }
          break;
        case 0x04:
          printf("[INFO] Mouse Type: 5 Button Mouse\n");
          mouse_max_packets = 4;
          mouse_id = 0x04;
          mouse_state = INIT_SET_CONFIG;
          mouse_command_handler(0xE8);
          break;
        default:
          printf("[ERR] Unknown Mouse Type (0x%02X), Asking again to Reset...\n", data_byte);
          mouse_state = INIT_AWAIT_ACK;
          mouse_command_handler(0xFF);
          break;
      }
      break;
    case INIT_DETECT_MOUSE_TYPE:
      // Detect Mouse Type
      switch (data_byte) {
        case 0xFA:
          // Ack Received
          switch (mouse_type_detect_sequence) {
            case 0:
              // Set Sample Rate to 200
              mouse_type_detect_sequence++;
              mouse_command_handler(0xC8);
              break;
            case 1:
              // Ask to Set Sample Rate
              mouse_type_detect_sequence++;
              mouse_command_handler(0xF3);
              break;
            case 2:
              // Set Sample Rate based on ID
              mouse_type_detect_sequence++;
              mouse_command_handler((mouse_id == 0x03) ? 0xC8 : 0x64);
              break;
            case 3:
              // Ask to Set Sample Rate
              mouse_type_detect_sequence++;
              mouse_command_handler(0xF3);
              break;
            case 4:
              // Set Sample Rate to 80
              mouse_type_detect_sequence++;
              mouse_command_handler(0x50);
              break;
            case 5:
              // Re-request Mouse ID
              mouse_type_detect_sequence = 0;
              mouse_state = INIT_AWAIT_ID;
              mouse_command_handler(0xF2);
              break;
          }
          break;
        default:
          printf("[DBG] Unhandled Response Received (0x%02X)\n", data_byte);
          mouse_type_detect_sequence = 0;
          mouse_id = 0x00;
          mouse_state = INIT_SET_CONFIG;
          mouse_command_handler(0xE8);
      }
      break;
    case INIT_SET_CONFIG:
      // Finish Mouse Configuration
      //  - Set Resolution to 8 Counts/mm
      //  - Set Sampling to 1:1
      //  - Set Sample Rate to 40
      //  - Enable Mouse
      static const uint8_t config_sequence[] = {0x03, 0xE6, 0xF3, 0x28, 0xF4};
      if (data_byte == 0xFA) {
        static uint8_t mouse_config_sequence = 0;
        if (mouse_config_sequence == sizeof(config_sequence) / sizeof(config_sequence[0])) {
          mouse_config_sequence = 0;
          mouse_state = INITIALISED;
          printf("[INFO] Mouse Initialisation Complete\n");
        } else {
          mouse_command_handler(config_sequence[mouse_config_sequence]);
          mouse_config_sequence++;
        }
      }
      break;
    case INITIALISED:
      // Process Mouse Data.
      static uint8_t buttons[5] = {0, 0, 0, 0, 0};
      static uint8_t parameters[4] = {0, 0, 0, 0};
      static int8_t pos[3] = {0, 0, 0};
      static int8_t data_loop = 0;
      switch (data_loop) {
        case 0:
          // Read in Button Data, as well as X and Y Overflow Data
          buttons[BUTTON_LEFT] = data_byte & 0x01;           // Left Button
          buttons[BUTTON_RIGHT] = (data_byte >> 1) & 0x01;   // Right Button
          buttons[BUTTON_MIDDLE] = (data_byte >> 2) & 0x01;  // Middle Button
          parameters[X_SIGN] = (data_byte >> 4) & 0x01;      // X Sign Bit
          parameters[Y_SIGN] = (data_byte >> 5) & 0x01;      // Y Sign Bit
          parameters[X_OVERFLOW] = (data_byte >> 6) & 0x01;  // X Overflow Bit
          parameters[Y_OVERFLOW] = (data_byte >> 7) & 0x01;  // Y Overflow Bit
          break;
        case 1:
          // Read in X Data
          pos[X_POS] = !(parameters[X_OVERFLOW] || parameters[Y_OVERFLOW]) ? get_xy_movement(data_byte, parameters[X_SIGN]) : 0;
          break;
        case 2:
          // Read in Y Data
          pos[Y_POS] = !(parameters[X_OVERFLOW] || parameters[Y_OVERFLOW]) ? get_xy_movement(~data_byte, parameters[Y_SIGN] ^= 1) : 0;
          break;
        case 3:
          // Read in Z/Extended Data
          switch (mouse_id) {
            case 0x03:
              // Scroll Wheel Mouse
              pos[Z_POS] = get_z_movement(data_byte);
              break;
            case 0x04:
              // 5 Button Mouse
              buttons[BUTTON_BACKWARD] = (data_byte >> 4) & 0x01;  // Button 4
              buttons[BUTTON_FORWARD] = (data_byte >> 5) & 0x01;   // Button 5
              pos[Z_POS] = get_z_movement(data_byte);              // Z Data Position
              break;
            default:
              break;
          }
          break;
      }
      // Increment Loop Counter, but reset if we reach 3.
      data_loop++;

      // If we have processed all packets, then we can handle the mouse report.
      if (data_loop >= mouse_max_packets) {
        data_loop = 0;
        handle_mouse_report(buttons, pos);
      }
  }
#ifdef CONVERTER_LEDS
  converter.state.mouse_ready = mouse_state == INITIALISED ? 1 : 0;
  update_converter_status();
#endif
}

void mouse_pio_restart() {
  // Restart the AT PIO State Machine
  printf("[DBG] Resetting State Machine and Mouse (Offset: 0x%02X)\n", mouse_offset);
  mouse_state = UNINITIALISED;
  mouse_id = 0xFF;
  pio_sm_drain_tx_fifo(mouse_pio, mouse_sm);
  pio_sm_clear_fifos(mouse_pio, mouse_sm);
  pio_sm_restart(mouse_pio, mouse_sm);
  pio_sm_exec(mouse_pio, mouse_sm, pio_encode_jmp(mouse_offset));
}

void mouse_input_event_handler() {
  io_ro_32 data_cast = mouse_pio->rxf[mouse_sm] >> 21;
  uint16_t data = (uint16_t)data_cast;

  // Extract the Start Bit, Parity Bit and Stop Bit.
  uint8_t start_bit = data & 0x1;
  uint8_t parity_bit = (data >> 9) & 0x1;
  uint8_t stop_bit = (data >> 10) & 0x1;
  uint8_t data_byte = (uint8_t)((data_cast >> 1) & 0xFF);
  uint8_t parity_bit_check = interface_parity_table[data_byte];

  // printf("[INFO] Mouse Data: 0x%04X\n", data_byte);

  if (start_bit != 0 || parity_bit != parity_bit_check || stop_bit != 1) {
    if (start_bit != 0) printf("[ERR] Start Bit Validation Failed: start_bit=%i\n", start_bit);
    if (stop_bit != 1) printf("[ERR] Stop Bit Validation Failed: stop_bit=%i\n", stop_bit);
    if (parity_bit != parity_bit_check) {
      printf("[ERR] Parity Bit Validation Failed: expected=%i, actual=%i\n", parity_bit_check, parity_bit);
      mouse_command_handler(0xFE);  // Request Resend
      return;
    }
    // We should reset/restart the State Machine
    mouse_pio_restart();
  }

  mouse_event_processor(data_byte);
}

void mouse_interface_task() {
  // Mouse Interface Initialisation helper
  // Here we handle Timeout events. If we don't receive responses from an attached Mouse with a set period of time
  // for any condition other than INITIALISED, we will then perform an appropriate action.
  if (mouse_state != INITIALISED) {
    // If we are in an uninitialised state, we will send a reset command to the Mouse.
    static uint32_t detect_ms = 0;
    if (board_millis() - detect_ms > 200) {
      detect_ms = board_millis();
      static uint8_t detect_stall_count = 0;
      if (gpio_get(mouse_data_pin + 1) == 1) {
        // Only perform checks if the clock is HIGH
        detect_stall_count++;
        if (detect_stall_count > 5) {
          // Reset Mouse as we have not received any data for 1 second.
          printf("[ERR] Mouse Interface Timeout.  Resetting Mouse...\n");
          mouse_id = 0xFF;               // Reset Mouse ID
          mouse_state = INIT_AWAIT_ACK;  // Set State to Await Acknowledgement
          detect_stall_count = 0;        // Reset Stall Counter
          mouse_command_handler(0xFF);   // Send Reset Command
        }
      } else if (mouse_state == UNINITIALISED) {
        printf("[DBG] Awaiting mouse detection. Please ensure a mouse is connected.\n");
        detect_stall_count = 0;  // Reset the detect_stall_count as we are waiting for the clock to go HIGH.
      }
#ifdef CONVERTER_LEDS
      converter.state.mouse_ready = mouse_state == INITIALISED ? 1 : 0;
      update_converter_status();
#endif
    }
  }
}

// Public function to initialise the XT PIO interface.
void mouse_interface_setup(uint data_pin) {
#ifdef CONVERTER_LEDS
  converter.state.mouse_ready = 0;
  update_converter_status();  // Always reset Converter Status here
#endif
  // First we need to determine which PIO to use for the Mouse Interface.
  // To do this, we check each PIO to see if there is space to load the Mouse Interface program.
  // If there is space, we claim the PIO and load the program.  If not, we continue to the next PIO.
  // We can call `pio_can_add_program` to check if there is space for the program.
  // `find_available_pio` is a helper function that will check both PIO0 and PIO1 for space.
  // If the function returns NULL, then there is no space available, and we should return.
  // mouse_state = INITIALISED;
  mouse_pio = find_available_pio(&pio_interface_program);
  if (mouse_pio == NULL) {
    printf("[ERR] No PIO available for Mouse Interface Program\n");
    return;
  }

  // Now we can claim the PIO and load the program.
  mouse_sm = (uint)pio_claim_unused_sm(mouse_pio, true);
  mouse_offset = pio_add_program(mouse_pio, &pio_interface_program);
  mouse_data_pin = data_pin;

  // Define the IRQ for the PIO State Machine.
  // This should either be set to PIO0_IRQ_0 or PIO1_IRQ_0 depending on the PIO used.
  uint pio_irq = mouse_pio == pio0 ? PIO0_IRQ_0 : PIO1_IRQ_0;

  // Define the Polling Inteval for the State Machine.
  // The AT/PS2 Protocol runs at a clock speed range of 10-16.7KHz.
  // At 16.7KHz, the maximum polling interval would be 60us, however, clock
  // must be HIGH for 30us to 50us, and LOW for 30us to 50us.  As such, we
  // will use a polling interval of 50us, which should equate to a clock of 20KHz.
  // We also want to ensure we have 11 SM cycles per clock cycle, as there are
  // approx 11 bits per byte, and we want to ensure we are reading the data correctly.

  int polling_interval_us = 50;
  int cycles_per_clock = 11;

  // Get base clock speed of RP2040.  This should always equal 125MHz, but we will check anyway.
  float rp_clock_khz = 0.001 * clock_get_hz(clk_sys);

  printf("[INFO] RP2040 Clock Speed: %.0fKHz\n", rp_clock_khz);
  printf("[INFO] Interface Polling Interval: %dus\n", polling_interval_us);
  float polling_interval_khz = 1000 / polling_interval_us;
  printf("[INFO] Interface Polling Clock: %.0fkHz\n", polling_interval_khz);
  // Always round clock_div to prevent jitter by having a fractional divider.
  float clock_div = roundf((rp_clock_khz / polling_interval_khz) / cycles_per_clock);
  printf("[INFO] Clock Divider based on %d SM Cycles per Mouse Clock Cycle: %.2f\n", cycles_per_clock, clock_div);
  printf("[INFO] Effective SM Clock Speed: %.2fkHz\n", (float)(rp_clock_khz / clock_div));

  pio_interface_program_init(mouse_pio, mouse_sm, mouse_offset, data_pin, clock_div);

  irq_set_exclusive_handler(pio_irq, &mouse_input_event_handler);
  irq_set_enabled(pio_irq, true);

  printf("[INFO] PIO%d SM%d Interface program loaded at mouse_offset %d with clock divider of %.2f\n", (mouse_pio == pio0 ? 0 : 1), mouse_sm, mouse_offset, clock_div);
}