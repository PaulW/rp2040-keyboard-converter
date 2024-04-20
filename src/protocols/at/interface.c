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

#include "interface.h"

#include <math.h>

#include "bsp/board.h"
#include "buzzer.h"
#include "config.h"
#include "hardware/clocks.h"
#include "interface.pio.h"
#include "led_helper.h"
#include "pio_helper.h"
#include "ringbuf.h"
#include "scancode.h"

uint keyboard_sm = 0;
uint offset = 0;
PIO keyboard_pio;
uint16_t keyboard_id = 0xFFFF;

static const uint8_t keyboard_parity_table[256] = {
    /* Mapping of HEX to Parity Bit for input from AT Keyboard
    0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F */
    1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,  // 0
    0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,  // 1
    0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,  // 2
    1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,  // 3
    0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,  // 4
    1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,  // 5
    1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,  // 6
    0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,  // 7
    0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,  // 8
    1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,  // 9
    1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,  // A
    0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,  // B
    1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,  // C
    0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,  // D
    0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,  // E
    1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1   // F
};

// Check if we are a Terminal Keyboard (Keyboards utilising Set 3 Scancodes).
// This is used to determine if we should perform the additional steps required for Terminal Keyboards.
#define CODESET_3 (strcmp(KEYBOARD_CODESET, "set3") == 0)

static uint8_t keyboard_lock_leds = 0;
static bool id_retry = false;  // Used to determine whether we've already retried reading the Keyboard ID.

static enum {
  UNINITIALISED,
  INIT_AWAIT_ACK,
  INIT_AWAIT_SELFTEST,
  INIT_READ_ID_1,
  INIT_READ_ID_2,
  INIT_SETUP,
  SET_LOCK_LEDS,
  INITIALISED,
} keyboard_state = UNINITIALISED;

// Command Handler function to issue commands to the attached AT Keyboard.
static void keyboard_command_handler(uint8_t data_byte) {
  uint16_t data_with_parity = (uint16_t)(data_byte + (keyboard_parity_table[data_byte] << 8));
  pio_sm_put(keyboard_pio, keyboard_sm, data_with_parity);
}

static void keyboard_event_processor(uint8_t data_byte) {
  switch (keyboard_state) {
    case UNINITIALISED:
      id_retry = false;      // Reset the id_retry flag as we are uninitialised.
      keyboard_id = 0xFFFF;  // Reset the keyboard_id as we are uninitialised.
      switch (data_byte) {
        case 0xAA:
          // Likely we are powering on for the first time and initialising. Keyboard sends 0xAA on power on following successful BAT
          printf("[DBG] Keyboard Self Test OK!\n");
          buzzer_play_sound_sequence_non_blocking(READY_SEQUENCE);
          keyboard_lock_leds = 0;
          printf("[DBG] Waiting for Keyboard ID...\n");
          keyboard_state = INIT_READ_ID_1;
          break;
        default:
          // This event is reached if we receieve any other event before intiiialisation.  This may be an unsuccesful BAT or just weird power-on state.
          printf("[DBG] Asking Keyboard to Reset\n");
          keyboard_state = INIT_AWAIT_ACK;
          keyboard_command_handler(0xFF);
      }
      break;
    case INIT_AWAIT_ACK:
      switch (data_byte) {
        case 0xFA:
          printf("[DBG] ACK Received after Reset\n");
          keyboard_state = INIT_AWAIT_SELFTEST;
          break;
        default:
          printf("[DBG] Unknown ACK Response (0x%02X).  Asking again to Reset...\n", data_byte);
          keyboard_command_handler(0xFF);
      }
      break;
    case INIT_AWAIT_SELFTEST:
      switch (data_byte) {
        case 0xAA:
          printf("[DBG] Keyboard Self Test OK!\n");
          buzzer_play_sound_sequence_non_blocking(READY_SEQUENCE);
          keyboard_lock_leds = 0;
          // Move on to attempting to read the Keyboard ID.
          printf("[DBG] Waiting for Keyboard ID...\n");
          keyboard_state = INIT_READ_ID_1;
          break;
        default:
          printf("[DBG] Self-Test invalid response (0x%02X).  Asking again to Reset...\n", data_byte);
          keyboard_state = INIT_AWAIT_ACK;
          keyboard_command_handler(0xFF);
      }
      break;

    // If we are a Terminal Keyboard, then we should receive 2 more responses which will be the ID of the keyboard.
    // This will timeout after 500ms if no response is received, and this is handled within the keyboard_interface_task function.
    case INIT_READ_ID_1:
      switch (data_byte) {
        case 0xFA:
          printf("[DBG] ACK Keyboard ID Request\n");
          printf("[DBG] Waiting for Keyboard ID...\n");
          break;
        default:
          printf("[DBG] Keyboard First ID Byte read as 0x%02X\n", data_byte);
          keyboard_id &= 0x00FF;
          keyboard_id |= (uint16_t)data_byte << 8;
          keyboard_state = INIT_READ_ID_2;
      }
      break;
    case INIT_READ_ID_2:
      printf("[DBG] Keyboard Second ID Byte read as 0x%02X\n", data_byte);
      keyboard_id &= 0xFF00;
      keyboard_id |= (uint16_t)data_byte;
      printf("[DBG] Keyboard ID: 0x%04X\n", keyboard_id);
      // Handle Make/Break Setup for Terminal Keyboards
      if (CODESET_3) {
        // We then want to ensure we set all keys to Make/Break if we're a Terminal Keyboard (Keyboards utilising Set 3 Scancodes)
        printf("[DBG] Setting all Keys to Make/Break\n");
        keyboard_command_handler(0xF8);
        keyboard_state = INIT_SETUP;
      } else {
        printf("[DBG] Keyboard Initialised!\n");
        keyboard_state = INITIALISED;
      }
      break;
    case INIT_SETUP:
      // We want to ensure we set Make/Break
      switch (data_byte) {
        case 0xFA:
          printf("[DBG] Keyboard Initialised!\n");
          keyboard_state = INITIALISED;
          break;
        default:
          printf("[DBG] Unknown Response (0x%02X).  Continuing...\n", data_byte);
          keyboard_id = 0xFFFF;
          printf("[DBG] Keyboard Initialised!\n");
          keyboard_state = INITIALISED;
      }
      break;

    // Handle LED Events.
    case SET_LOCK_LEDS:
      // We likely need to check for a F0 event (key-up) but I think we should be OK?
      switch (data_byte) {
        case 0xFA:
          if (ps2_lock_values != keyboard_lock_leds) {
            keyboard_lock_leds = ps2_lock_values;
            keyboard_command_handler(ps2_lock_values);
          } else {
            buzzer_play_sound_sequence_non_blocking(LOCK_LED);
            keyboard_state = INITIALISED;
          }
          break;
        default:
          printf("[DBG] SET_LOCK_LED FAILED (0x%02X)\n", data_byte);
          keyboard_lock_leds = ps2_lock_values;
          keyboard_state = INITIALISED;
      }
      break;

    // If we are initialised, then we should process the keycodes.
    case INITIALISED:
      if (!ringbuf_is_full()) ringbuf_put(data_byte);
  }
#ifdef CONVERTER_LEDS
  update_converter_status(keyboard_state == INITIALISED ? 1 : 2);
#endif
}

void keyboard_pio_restart() {
  // Restart the AT PIO State Machine
  printf("[DBG] Resetting State Machine and Keyboard (Offset: 0x%02X)\n", offset);
  keyboard_state = UNINITIALISED;
  id_retry = false;
  pio_sm_drain_tx_fifo(keyboard_pio, keyboard_sm);
  pio_sm_clear_fifos(keyboard_pio, keyboard_sm);
  pio_sm_restart(keyboard_pio, keyboard_sm);
  pio_sm_exec(keyboard_pio, keyboard_sm, pio_encode_jmp(offset));
}

// IRQ Event Handler used to read keycode data from the AT Keyboard
// and ensure relevant code is valid when checking against relevant check bits.
static void __isr keyboard_input_event_handler() {
  io_ro_32 data_cast = keyboard_pio->rxf[keyboard_sm] >> 21;
  uint16_t data = (uint16_t)data_cast;

  // Extract the Start Bit, Parity Bit and Stop Bit.
  uint8_t start_bit = data & 0x1;
  uint8_t parity_bit = (data >> 9) & 0x1;
  uint8_t stop_bit = (data >> 10) & 0x1;
  uint8_t data_byte = (uint8_t)((data_cast >> 1) & 0xFF);
  uint8_t parity_bit_check = keyboard_parity_table[data_byte];

  if (start_bit != 0 || parity_bit != parity_bit_check || stop_bit != 1) {
    if (start_bit != 0) printf("[ERR] Start Bit Validation Failed: start_bit=%i\n", start_bit);
    if (stop_bit != 1) printf("[ERR] Stop Bit Validation Failed: stop_bit=%i\n", stop_bit);
    if (parity_bit != parity_bit_check) {
      printf("[ERR] Parity Bit Validation Failed: expected=%i, actual=%i\n", parity_bit_check, parity_bit);
      if (data_byte == 0x54 && parity_bit == 1) {
        // Likely we are powering on, or initialising. Keyboard sends 0xAA on power on following successful BAT
        // however, it also drops clock & data low briefly during initial power on, and as such, causes an extra
        // bit to be read so shifts data in fifo to change from 0xAA to 0x54 with invalid parity.
        // This has been fixed, but left here for reference (or if it breaks again!)
        printf("[DBG] Likely Keyboard Connect Event detected.\n");
        keyboard_pio_restart();
      }
      // Ask Keyboard to re-send the data.
      keyboard_command_handler(0xFE);
      return;  // We don't want to process this event any further.
    }
    // We should reset/restart the State Machine
    keyboard_pio_restart();
    return;
  }

  keyboard_event_processor(data_byte);
}

void keyboard_interface_task() {
  // This function is called from the main loop and is used to handle the keyboard interface.
  // It is responsible for initialising the keyboard, setting the lock LEDs, and processing
  // any keycodes that are sent from the keyboard.

  static uint8_t detect_stall_count = 0;

  if (keyboard_state == INITIALISED) {
    // Handle further initialization steps now, this is more for terminal keyboard support.
    // This portion helps with Lock LED changes.  We only get here once the keyboard has initialised.
    detect_stall_count = 0;  // Reset the detect_stall_count as we are initialised.
    if (ps2_lock_values != keyboard_lock_leds) {
      keyboard_state = SET_LOCK_LEDS;
      keyboard_command_handler(0xED);
    } else {
      if (!ringbuf_is_empty()) {
        if (tud_suspended()) {
          // Wake up host if we are in suspend mode
          // and REMOTE_WAKEUP feature is enabled by host
          tud_remote_wakeup();
        } else {
          if (tud_hid_ready()) {
            uint32_t status = save_and_disable_interrupts();  // Stop any interrupts from triggering
            int c = ringbuf_get();                            // Pull from the ringbuffer
            restore_interrupts(status);                       // Release the interrupts, allowing key events to resume.
            process_scancode((uint8_t)c);
          }
        }
      }
    }
  } else {
    // This portion helps with initialisation of the keyboard.
    // Here we handle Timeout events.  If we don't receive a response from the keyboard when in an alternate state,
    // then we will reset the keyboard and try again.  This is to handle the case where the keyboard is not responding.
    static uint32_t detect_ms = 0;
    if (board_millis() - detect_ms > 200) {
      detect_ms = board_millis();
      if (gpio_get(DATA_PIN + 1) == 1) {
        // Only perform timeout checks if the clock is HIGH (Keyboard Detected).
        detect_stall_count++;  // Always increment the detect_stall_count if we have a HIGH clock and not in INITIALISED state.
        switch (keyboard_state) {
          case INIT_READ_ID_1 ... INIT_SETUP:
            if (detect_stall_count > 2) {
              // Stall Detected during Reading of Keyboard ID or Setup Process
              if (!id_retry) {
                // We've not tried re-requesting the ID, let's do that first...
                printf("[DBG] Keyboard ID/Setup Timeout, retrying...\n");
                id_retry = true;
                keyboard_state = INIT_READ_ID_1;  // Set State to Reading of Keyboard ID
                keyboard_command_handler(0xF2);   // Request Keyboard ID
                detect_stall_count = 0;           // Reset the detect_stall_count as we are retrying.
              } else {
                printf("[DBG] Keyboard Read ID/Setup Timed out again, continuing with defaults.\n");
                keyboard_id = 0xFFFF;
                printf("[DBG] Keyboard Initialised!\n");
                keyboard_state = INITIALISED;
                detect_stall_count = 0;
              }
            }
            break;
          case SET_LOCK_LEDS:
            if (detect_stall_count > 2) {
              // Stall Detected during Setting of Lock LEDs
              printf("[DBG] Timeout while setting keyboard lock LEDs, continuing.\n");
              keyboard_state = INITIALISED;
              detect_stall_count = 0;
            }
            break;
          default:
            if (detect_stall_count < 5) {
              printf("[DBG] Keyboard detected, awaiting ACK (%i/5 attempts)\n", detect_stall_count);
            } else {
              printf("[DBG] Keyboard detected, but no ACK received!\n");
              printf("[DBG] Requesting keyboard reset\n");
              keyboard_state = INIT_AWAIT_ACK;
              detect_stall_count = 0;
              keyboard_command_handler(0xFF);
            }
            break;
        }
      } else if (keyboard_state == UNINITIALISED) {
        printf("[DBG] Awaiting keyboard detection. Please ensure a keyboard is connected.\n");
        detect_stall_count = 0;  // Reset the detect_stall_count as we are waiting for the clock to go HIGH.
      }
#ifdef CONVERTER_LEDS
      update_converter_status(keyboard_state == INITIALISED ? 1 : 2);
#endif
    }
  }
}

// Public function to initialise the AT PIO interface.
void keyboard_interface_setup(uint data_pin) {
#ifdef CONVERTER_LEDS
  update_converter_status(2);  // Always reset Converter Status here
#endif

  // First we need to determine which PIO to use for the Keyboard Interface.
  // To do this, we check each PIO to see if there is space to load the Keyboard Interface program.
  // If there is space, we claim the PIO and load the program.  If not, we continue to the next PIO.
  // We can call `pio_can_add_program` to check if there is space for the program.
  // `find_available_pio` is a helper function that will check both PIO0 and PIO1 for space.
  // If the function returns NULL, then there is no space available, and we should return.

  keyboard_pio = find_available_pio(&keyboard_interface_program);
  if (keyboard_pio == NULL) {
    printf("[ERR] No PIO available for Keyboard Interface Program\n");
    return;
  }

  // Now we can claim the PIO and load the program.
  keyboard_sm = (uint)pio_claim_unused_sm(keyboard_pio, true);
  offset = pio_add_program(keyboard_pio, &keyboard_interface_program);

  // Define the IRQ for the PIO State Machine.
  // This should either be set to PIO0_IRQ_0 or PIO1_IRQ_0 depending on the PIO used.
  uint pio_irq = keyboard_pio == pio0 ? PIO0_IRQ_0 : PIO1_IRQ_0;

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
  printf("[INFO] Clock Divider based on %d SM Cycles per Keyboard Clock Cycle: %.2f\n", cycles_per_clock, clock_div);
  printf("[INFO] Effective SM Clock Speed: %.2fkHz\n", (float)(rp_clock_khz / clock_div));

  keyboard_interface_program_init(keyboard_pio, keyboard_sm, offset, data_pin, clock_div);

  printf("[INFO] PIO SM Interface program loaded at %d with clock divider of %.2f\n", offset, clock_div);

  irq_set_exclusive_handler(pio_irq, &keyboard_input_event_handler);
  irq_set_enabled(pio_irq, true);
}
