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
uint8_t mouse_id = ATPS2_MOUSE_ID_UNKNOWN;
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

typedef enum { X_POS, Y_POS, Z_POS } mousepos_index;

/**
 * @brief Command Handler function to issue commands to the attached AT/PS2 Mouse.
 * This function is responsible for handling commands to be sent to the AT/PS2 Mouse. It takes a
 * single parameter, `data_byte`, which represents the command to be issued. The function calculates
 * the parity of the data byte and combines it with the data byte to form a 16-bit value. This value
 * is then sent to the peripheral using the PIO state machine.
 *
 * @param data_byte The command byte to be sent to the AT/PS2 Mouse.
 */
static void mouse_command_handler(uint8_t data_byte) {
  uint16_t data_with_parity = (uint16_t)(data_byte + (interface_parity_table[data_byte] << 8));
  pio_sm_put(mouse_pio, mouse_sm, data_with_parity);
}

/**
 * Interpret a raw mouse packet position byte together with its sign flag and
 * produce a signed X/Y displacement clamped to the range -127 to 127.
 *
 * @param pos      Raw position byte from the mouse packet.
 * @param sign_bit Non-zero if the sign flag is set (indicates negative movement).
 *
 * @returns Signed displacement in the range -127 to 127.
 */
int8_t get_xy_movement(uint8_t pos, int sign_bit) {
  int16_t new_pos = pos;
  if (new_pos && sign_bit) {
    new_pos -= 256;
  }
  new_pos = (new_pos > 127) ? 127 : (new_pos < -127) ? -127 : new_pos;
  return (int8_t)new_pos;
}

/**
 * Extracts the Z-axis (scroll) movement encoded in the low 4 bits of a PS/2 position byte.
 *
 * The function returns the signed Z movement in the range -8 to 7: when `pos` is 0 the result is 0;
 * otherwise the low 4 bits are interpreted and 8 is subtracted to produce a signed value.
 *
 * @param pos Position byte containing Z movement in its lower 4 bits.
 * @returns The Z-axis movement as an `int8_t` in the range -8 to 7.
 */
int8_t get_z_movement(uint8_t pos) {
  int8_t z_pos = pos & 0x0F;
  if (pos) {
    z_pos -= 8;
  }

  return z_pos;
}

/**
 * Process a single PS/2 mouse data byte and advance the mouse interface state machine.
 *
 * Handles initialization sequences (self-test, ID/type detection, configuration) by issuing
 * appropriate PS/2 commands and updating interface state. Once the device is INITIALISED,
 * assembles incoming bytes into complete mouse packets, converts them to button/XY/Z values,
 * and forwards the report to the HID/reporting layer.
 *
 * @param data_byte The raw PS/2 data byte received from the mouse.
 *
 * Side effects: may modify global mouse_state and mouse_id, call mouse_command_handler to send
 * PS/2 commands, and call handle_mouse_report to deliver completed HID reports.
 */
void mouse_event_processor(uint8_t data_byte) {
  static uint8_t mouse_type_detect_sequence = 0;
  static uint8_t mouse_max_packets = 0;

  switch (mouse_state) {
    case UNINITIALISED:
      switch (data_byte) {
        case ATPS2_RESP_BAT_PASSED:  // Self Test Passed
          printf("[INFO] Mouse Self Test Passed\n");
          printf("[INFO] Detecting Mouse Type\n");
          mouse_id = ATPS2_MOUSE_ID_UNKNOWN;  // Reset Mouse ID
          mouse_state = INIT_AWAIT_ID;
          break;
        default:
          // If we hit this, then either the self test failed, or we have an error.
          printf("[ERR] Asking Mouse to Reset\n");
          mouse_state = INIT_AWAIT_ACK;
          mouse_command_handler(ATPS2_CMD_RESET);
      }
      break;
    case INIT_AWAIT_ACK:
      switch (data_byte) {
        case ATPS2_RESP_ACK:  // Acknowledged
          printf("[INFO] ACK Received after Reset\n");
          mouse_state = INIT_AWAIT_SELFTEST;
          break;
        default:
          printf("[DBG] Unknown ACK Response (0x%02X).  Asking again to Reset...\n", data_byte);
          mouse_command_handler(ATPS2_CMD_RESET);
      }
      break;
    case INIT_AWAIT_SELFTEST:
      switch (data_byte) {
        case ATPS2_RESP_BAT_PASSED:  // Self Test Passed
          printf("[INFO] Mouse Self Test Passed\n");
          printf("[INFO] Detecting Mouse Type\n");
          mouse_id = ATPS2_MOUSE_ID_UNKNOWN;  // Reset Mouse ID
          mouse_state = INIT_AWAIT_ID;
          break;
        default:
          printf("[DBG] Self-Test invalid response (0x%02X).  Asking again to Reset...\n",
                 data_byte);
          mouse_state = INIT_AWAIT_ACK;
          mouse_command_handler(ATPS2_CMD_RESET);
      }
      break;
    case INIT_AWAIT_ID:
      switch (data_byte) {
        case ATPS2_RESP_ACK:
          // Ack Received, silently ignore
          break;
        case ATPS2_MOUSE_ID_STANDARD:
          if (mouse_id == ATPS2_MOUSE_ID_UNKNOWN) {
            mouse_id = ATPS2_MOUSE_ID_STANDARD;
            mouse_type_detect_sequence = 0;
            mouse_state = INIT_DETECT_MOUSE_TYPE;
            mouse_command_handler(ATPS2_MOUSE_CMD_SET_SAMPLE_RATE);
          } else {
            printf("[INFO] Mouse Type: Standard PS/2 Mouse\n");
            mouse_max_packets = ATPS2_MOUSE_PACKET_STANDARD_SIZE;
            mouse_state = INIT_SET_CONFIG;
            mouse_command_handler(ATPS2_MOUSE_CMD_SET_RESOLUTION);
          }
          break;
        case ATPS2_MOUSE_ID_INTELLIMOUSE:
          if (mouse_id == ATPS2_MOUSE_ID_STANDARD) {
            mouse_id = ATPS2_MOUSE_ID_INTELLIMOUSE;
            mouse_state = INIT_DETECT_MOUSE_TYPE;
            mouse_command_handler(ATPS2_MOUSE_CMD_SET_SAMPLE_RATE);
          } else {
            printf("[INFO] Mouse Type: Mouse with Scroll Wheel\n");
            mouse_max_packets = ATPS2_MOUSE_PACKET_EXTENDED_SIZE;
            mouse_state = INIT_SET_CONFIG;
            mouse_command_handler(ATPS2_MOUSE_CMD_SET_RESOLUTION);
          }
          break;
        case ATPS2_MOUSE_ID_INTELLIMOUSE_EXPLORER:
          printf("[INFO] Mouse Type: 5 Button Mouse\n");
          mouse_max_packets = ATPS2_MOUSE_PACKET_EXTENDED_SIZE;
          mouse_id = ATPS2_MOUSE_ID_INTELLIMOUSE_EXPLORER;
          mouse_state = INIT_SET_CONFIG;
          mouse_command_handler(ATPS2_MOUSE_CMD_SET_RESOLUTION);
          break;
        default:
          printf("[ERR] Unknown Mouse Type (0x%02X), Asking again to Reset...\n", data_byte);
          mouse_state = INIT_AWAIT_ACK;
          mouse_command_handler(ATPS2_CMD_RESET);
          break;
      }
      break;
    case INIT_DETECT_MOUSE_TYPE:
      // Detect Mouse Type
      switch (data_byte) {
        case ATPS2_RESP_ACK:
          // Ack Received
          switch (mouse_type_detect_sequence) {
            case 0:
              // Set Sample Rate to 200
              mouse_type_detect_sequence++;
              mouse_command_handler(ATPS2_MOUSE_RATE_200_HZ);
              break;
            case 1:
              // Ask to Set Sample Rate
              mouse_type_detect_sequence++;
              mouse_command_handler(ATPS2_MOUSE_CMD_SET_SAMPLE_RATE);
              break;
            case 2:
              // Set Sample Rate based on ID
              mouse_type_detect_sequence++;
              mouse_command_handler((mouse_id == ATPS2_MOUSE_ID_INTELLIMOUSE) ? ATPS2_MOUSE_RATE_200_HZ : ATPS2_MOUSE_RATE_100_HZ);
              break;
            case 3:
              // Ask to Set Sample Rate
              mouse_type_detect_sequence++;
              mouse_command_handler(ATPS2_MOUSE_CMD_SET_SAMPLE_RATE);
              break;
            case 4:
              // Set Sample Rate to 80
              mouse_type_detect_sequence++;
              mouse_command_handler(ATPS2_MOUSE_RATE_80_HZ);
              break;
            case 5:
              // Re-request Mouse ID
              mouse_type_detect_sequence = 0;
              mouse_state = INIT_AWAIT_ID;
              mouse_command_handler(ATPS2_CMD_GET_ID);
              break;
          }
          break;
        default:
          printf("[DBG] Unhandled Response Received (0x%02X)\n", data_byte);
          mouse_type_detect_sequence = 0;
          mouse_id = ATPS2_MOUSE_ID_STANDARD;
          mouse_state = INIT_SET_CONFIG;
          mouse_command_handler(ATPS2_MOUSE_CMD_SET_RESOLUTION);
      }
      break;
    case INIT_SET_CONFIG:
      // Finish Mouse Configuration
      //  - Set Resolution to 8 Counts/mm
      //  - Set Sampling to 1:1
      //  - Set Sample Rate to 40
      //  - Enable Mouse
      static const uint8_t config_sequence[] = {
          ATPS2_MOUSE_RES_8_COUNT_MM, 
          ATPS2_MOUSE_CMD_SET_SCALING_1_1, 
          ATPS2_MOUSE_CMD_SET_SAMPLE_RATE, 
          ATPS2_MOUSE_RATE_40_HZ, 
          ATPS2_MOUSE_CMD_ENABLE
      };
      if (data_byte == ATPS2_RESP_ACK) {
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
          pos[X_POS] = !(parameters[X_OVERFLOW] || parameters[Y_OVERFLOW])
                           ? get_xy_movement(data_byte, parameters[X_SIGN])
                           : 0;
          break;
        case 2:
          // Read in Y Data
          pos[Y_POS] = !(parameters[X_OVERFLOW] || parameters[Y_OVERFLOW])
                           ? get_xy_movement(~data_byte, parameters[Y_SIGN] ^= 1)
                           : 0;
          break;
        case 3:
          // Read in Z/Extended Data
          switch (mouse_id) {
            case ATPS2_MOUSE_ID_INTELLIMOUSE:
              // Scroll Wheel Mouse
              pos[Z_POS] = get_z_movement(data_byte);
              break;
            case ATPS2_MOUSE_ID_INTELLIMOUSE_EXPLORER:
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

/**
 * Handle a mouse PIO IRQ by validating and dispatching a received PS/2 data byte.
 *
 * Reads a value from the mouse PIO RX FIFO, validates the PS/2 framing (start, parity, stop)
 * and takes corrective action for errors: on parity mismatch requests a resend from the device,
 * on framing errors resets the mouse state and restarts the PIO state machine. When the byte
 * passes validation it is forwarded to mouse_event_processor().
 */
void mouse_input_event_handler() {
  io_ro_32 data_cast = mouse_pio->rxf[mouse_sm] >> 21;
  uint16_t data = (uint16_t)data_cast;

  // Extract the Start Bit, Parity Bit and Stop Bit.
  uint8_t start_bit = data & 0x1;
  uint8_t parity_bit = (data >> 9) & 0x1;
  uint8_t stop_bit = (data >> 10) & 0x1;
  uint8_t data_byte = (uint8_t)((data_cast >> 1) & ATPS2_DATA_MASK);
  uint8_t parity_bit_check = interface_parity_table[data_byte];

  if (start_bit != 0 || parity_bit != parity_bit_check || stop_bit != 1) {
    if (start_bit != 0) printf("[ERR] Start Bit Validation Failed: start_bit=%i\n", start_bit);
    if (stop_bit != 1) printf("[ERR] Stop Bit Validation Failed: stop_bit=%i\n", stop_bit);
    if (parity_bit != parity_bit_check) {
      printf("[ERR] Parity Bit Validation Failed: expected=%i, actual=%i\n", parity_bit_check,
             parity_bit);
      mouse_command_handler(ATPS2_CMD_RESEND);  // Request Resend
      return;
    }
    // We should reset/restart the State Machine
    mouse_state = UNINITIALISED;
    mouse_id = ATPS2_MOUSE_ID_UNKNOWN;
    pio_restart(mouse_pio, mouse_sm, mouse_offset);
  }

  mouse_event_processor(data_byte);
}

/**
 * Assist mouse interface initialization and handle initialization-timeout events.
 *
 * When the mouse is not yet INITIALISED, this function periodically checks for activity and
 * enforces a timeout sequence: if the mouse clock line remains idle for a sustained period
 * (~1 second), the function resets the detected mouse ID to ATPS2_MOUSE_ID_UNKNOWN, moves the
 * state machine to INIT_AWAIT_ACK, and issues an ATPS2_CMD_RESET to restart mouse initialization.
 *
 * The function also clears the internal stall counter when the clock is low and the interface is
 * UNINITIALISED, and (when compiled with CONVERTER_LEDS) updates converter LED/state indicators.
 *
 * This function has no parameters and no return value; it is intended to be called periodically
 * from the main loop or a scheduler.
 */
void mouse_interface_task() {
  // Mouse Interface Initialisation helper
  // Here we handle Timeout events. If we don't receive responses from an attached Mouse with a set
  // period of time for any condition other than INITIALISED, we will then perform an appropriate
  // action.
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
          mouse_id = ATPS2_MOUSE_ID_UNKNOWN;               // Reset Mouse ID
          mouse_state = INIT_AWAIT_ACK;  // Set State to Await Acknowledgement
          detect_stall_count = 0;        // Reset Stall Counter
          mouse_command_handler(ATPS2_CMD_RESET);   // Send Reset Command
        }
      } else if (mouse_state == UNINITIALISED) {
        printf("[DBG] Awaiting mouse detection. Please ensure a mouse is connected.\n");
        detect_stall_count =
            0;  // Reset the detect_stall_count as we are waiting for the clock to go HIGH.
      }
#ifdef CONVERTER_LEDS
      converter.state.mouse_ready = mouse_state == INITIALISED ? 1 : 0;
      update_converter_status();
#endif
    }
  }
}

/**
 * Initialize the AT/PS2 mouse interface on a chosen PIO and configure its state machine and IRQ.
 *
 * Configures and claims an available PIO/SM, loads the mouse PIO program, initializes the
 * PIO state machine with the appropriate clock divider for AT/PS2 timings, and registers
 * the IRQ handler used to receive mouse input.
 *
 * @param data_pin GPIO pin number to use as the PS/2 data line for the mouse interface.
 */
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

  // Define the polling interval and cycles per clock for the state machine.
  // This is determined from the shortest clock pulse width of the AT/PS2 protocol.
  // According to the 84F9735 PS2 Hardware Interface Technical Reference from IBM:
  //    https://archive.org/details/bitsavers_ibmpcps284erfaceTechnicalReferenceCommonInterfaces_39004874/page/n229/mode/2up?q=keyboard
  // Clock Pulse Width can be anywhere between 30us and 50us.
  float clock_div = calculate_clock_divider(ATPS2_TIMING_CLOCK_MIN_US);  // 30us minimum clock pulse width

  pio_interface_program_init(mouse_pio, mouse_sm, mouse_offset, data_pin, clock_div);

  irq_set_exclusive_handler(pio_irq, &mouse_input_event_handler);
  irq_set_enabled(pio_irq, true);
  irq_set_priority(pio_irq, 0);

  printf(
      "[INFO] PIO%d SM%d Interface program loaded at mouse_offset %d with clock divider of %.2f\n",
      (mouse_pio == pio0 ? 0 : 1), mouse_sm, mouse_offset, clock_div);
}