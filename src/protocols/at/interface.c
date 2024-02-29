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
#include "hardware/pio.h"
#include "interface.pio.h"
#include "lock_leds.h"
#include "ringbuf.h"
#include "scancode.h"

uint keyboard_sm = 0;
uint offset = 0;
PIO keyboard_pio = pio1;

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

static uint8_t keyboard_lock_leds = 0;

static enum {
  UNINITIALISED,
  INIT_AWAIT_ACK,
  INIT_AWAIT_SELFTEST,
  SET_LOCK_LEDS,
  INITIALISED,
} keyboard_state = UNINITIALISED;

// Command Handler function to issue commands to the attached AT Keyboard.
static void keyboard_command_handler(uint8_t data_byte) {
  uint16_t data_with_parity = (uint16_t)(data_byte + (keyboard_parity_table[data_byte] << 8));
  pio_sm_put(pio1, keyboard_sm, data_with_parity);
}

static void keyboard_event_processor(uint8_t data_byte) {
  switch (keyboard_state) {
    case UNINITIALISED:
      switch (data_byte) {
        case 0xAA:
          // Likely we are powering on for the first time and initialising. Keyboard sends 0xAA on power on following successful BAT
          printf("[DBG] Keyboard Initialised!\n");
          buzzer_play_sound_sequence_non_blocking(READY_SEQUENCE);
          keyboard_lock_leds = 0;
          keyboard_state = INITIALISED;
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
          printf("[DBG] Keyboard Initialised!\n");
          buzzer_play_sound_sequence_non_blocking(READY_SEQUENCE);
          keyboard_lock_leds = 0;
          keyboard_state = INITIALISED;
          break;
        default:
          printf("[DBG] Self-Test invalid response (0x%02X).  Asking again to Reset...\n", data_byte);
          keyboard_state = INIT_AWAIT_ACK;
          keyboard_command_handler(0xFF);
      }
      break;
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
    case INITIALISED:
      if (!ringbuf_is_full()) ringbuf_put(data_byte);
  }
}

void keyboard_pio_restart() {
  // Restart the AT PIO State Machine
  printf("[DBG] Resetting State Machine and Keyboard (Offset: 0x%02X)\n", offset);
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
        keyboard_state = UNINITIALISED;
      }
      keyboard_command_handler(0xFE);
      return;  // We don't want to process this event any further.
    }
    // We should reset/restart the State Machine
    keyboard_pio_restart();
    keyboard_state = UNINITIALISED;
  }
  keyboard_event_processor(data_byte);
}

void keyboard_interface_task() {
  // This function is called from the main loop and is used to handle the keyboard interface.
  // It is responsible for initialising the keyboard, setting the lock LEDs, and processing
  // any keycodes that are sent from the keyboard.

  static uint8_t detect_init_count = 0;

  if (keyboard_state < SET_LOCK_LEDS) {
    // This portion helps with initialisation of the keyboard.
    static uint32_t detect_ms = 0;
    if (board_millis() - detect_ms > 1000) {
      detect_ms = board_millis();
      // int pin_state = gpio_get(DATA_PIN + 1);
      if (gpio_get(DATA_PIN + 1) == 1) {
        if (detect_init_count < 3) {
          detect_init_count++;
          printf("[DBG] Keyboard Detected, waiting for ACK (%i/3)\n", detect_init_count);
        } else {
          printf("[DBG] Keyboard Detected but we had no ACK!\n");
          printf("[DBG] Asking Keyboard to Reset\n");
          keyboard_state = INIT_AWAIT_ACK;
          detect_init_count = 0;
          keyboard_command_handler(0xFF);
        }
      } else {
        printf("[DBG] Waiting for Keyboard Clock HIGH\n");
      }
    }
  } else if (keyboard_state == INITIALISED) {
    // This portion helps with Lock LED changes.  We only get here once the keyboard has initialised.
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
            uint32_t status = save_and_disable_interrupts();  // disable IRQ
            int c = ringbuf_get();                            // critical_section
            restore_interrupts(status);
            if (c != -1) {
              process_scancode((uint8_t)c);
            }
          }
        }
      }
    }
  }
}

// Public function to initialise the AT PIO interface.
void keyboard_interface_setup(uint data_pin) {
  keyboard_sm = (uint)pio_claim_unused_sm(keyboard_pio, true);
  offset = pio_add_program(keyboard_pio, &keyboard_interface_program);
  uint pio_irq = PIO1_IRQ_0;

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
