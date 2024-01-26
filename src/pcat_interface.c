/*
 * This file is part of Model-F 5170 Converter.
 *
 * Copyright 2023 Paul Bramhall (paulwamp@gmail.com)
 *
 * Model-F 5170 Converter is free software: you can redistribute it
 * and/or modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * Model-F 5170 Converter is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Model-F 5170 Converter.
 * If not, see <https://www.gnu.org/licenses/>.
 */

#include "pcat_interface.h"

#include <hardware/gpio.h>
#include <stdio.h>

#include "bsp/board.h"
#include "buzzer.h"
#include "config.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "lock_leds.h"
#include "pcat_interface.pio.h"

uint pcat_sm = 0;
uint offset = 0;
PIO pcat_pio = pio1;

static const uint8_t __not_in_flash("pcat_parity_table") pcat_parity_table[256] = {
    /* Mapping of HEX to Parity Bit for input from IBM PC/AT Keyboard
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

static uint8_t pcat_lock_leds = 0;

static enum {
  UNINITIALISED,
  INIT_AWAIT_ACK,
  INIT_AWAIT_SELFTEST,
  SET_LOCK_LEDS,
  INITIALISED,
} pcat_state = UNINITIALISED;

// Command Handler function to issue commands to the attached IBM PC/AT Keyboard.
static void __time_critical_func(pcat_command_handler)(uint8_t data_byte) {
  uint16_t data_with_parity = (uint16_t)(data_byte + (pcat_parity_table[data_byte] << 8));
  pio_sm_put(pio1, pcat_sm, data_with_parity);
}

static void __time_critical_func(pcat_event_processor)(uint8_t data_byte) {
  switch (pcat_state) {
    case UNINITIALISED:
      switch (data_byte) {
        case 0xAA:
          // Likely we are powering on for the first time and initialising. Keyboard sends 0xAA on power on following successful BAT
          printf("[DBG] Keyboard Initialised!\n");
          buzzer_play_sound_sequence_non_blocking(READY_SEQUENCE);
          pcat_lock_leds = 0;
          pcat_state = INITIALISED;
          break;
        default:
          // This event is reached if we receieve any other event before intiiialisation.  This may be an unsuccesful BAT or just weird power-on state.
          printf("[DBG] Asking Keyboard to Reset\n");
          pcat_state = INIT_AWAIT_ACK;
          pcat_command_handler(0xFF);
      }
      break;
    case INIT_AWAIT_ACK:
      switch (data_byte) {
        case 0xFA:
          printf("[DBG] ACK Received after Reset\n");
          pcat_state = INIT_AWAIT_SELFTEST;
          break;
        default:
          printf("[DBG] Unknown ACK Response (0x%02X).  Asking again to Reset...\n", data_byte);
          pcat_command_handler(0xFF);
      }
      break;
    case INIT_AWAIT_SELFTEST:
      switch (data_byte) {
        case 0xAA:
          printf("[DBG] Keyboard Initialised!\n");
          buzzer_play_sound_sequence_non_blocking(READY_SEQUENCE);
          pcat_lock_leds = 0;
          pcat_state = INITIALISED;
          break;
        default:
          printf("[DBG] Self-Test invalid response (0x%02X).  Asking again to Reset...\n", data_byte);
          pcat_state = INIT_AWAIT_ACK;
          pcat_command_handler(0xFF);
      }
      break;
    case SET_LOCK_LEDS:
      // We likely need to check for a F0 event (key-up) but I think we should be OK?
      switch (data_byte) {
        case 0xFA:
          if (ps2_lock_values != pcat_lock_leds) {
            pcat_lock_leds = ps2_lock_values;
            pcat_command_handler(ps2_lock_values);
          } else {
            buzzer_play_sound_sequence_non_blocking(LOCK_LED);
            pcat_state = INITIALISED;
          }
          break;
        default:
          printf("[DBG] SET_LOCK_LED FAILED (0x%02X)\n", data_byte);
          pcat_lock_leds = ps2_lock_values;
          pcat_state = INITIALISED;
      }
      break;
    case INITIALISED:
      pcat_input_event_processor(data_byte);
  }
}

void pcat_pio_restart() {
  // Restart the PC/AT PIO State Machine
  printf("[DBG] Resetting State Machine and Keyboard (Offset: 0x%02X)\n", offset);
  pio_sm_drain_tx_fifo(pcat_pio, pcat_sm);
  pio_sm_clear_fifos(pcat_pio, pcat_sm);
  pio_sm_restart(pcat_pio, pcat_sm);
  // pio_sm_clkdiv_restart(pcat_pio, pcat_sm);
  pio_sm_exec(pcat_pio, pcat_sm, pio_encode_jmp(offset));
}

// IRQ Event Handler used to read keycode data from the IBM PC/AT Keyboard
// and ensure relevant code is valid when checking against relevant check bits.
static void __isr __time_critical_func(pcat_input_event_handler)() {
  pio_interrupt_clear(pcat_pio, pcat_sm);
  if (pio_sm_is_rx_fifo_empty(pcat_pio, pcat_sm)) return;  // no new codes in the fifo

  io_ro_32 data_cast = pcat_pio->rxf[pcat_sm] >> 21;
  uint16_t data = (uint16_t)data_cast;

  // Extract the Start Bit, Parity Bit and Stop Bit.
  uint8_t start_bit = data & 0x1;
  uint8_t parity_bit = (data >> 9) & 0x1;
  uint8_t stop_bit = (data >> 10) & 0x1;
  uint8_t data_byte = (uint8_t)((data_cast >> 1) & 0xFF);
  uint8_t parity_bit_check = pcat_parity_table[data_byte];

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
        pcat_pio_restart();
        pcat_state = UNINITIALISED;
      }
      pcat_command_handler(0xFE);
      return;  // We don't want to process this event any further.
    }
    // We should reset/restart the State Machine
    pcat_pio_restart();
    pcat_state = UNINITIALISED;
  }
  pcat_event_processor(data_byte);
}

void pcat_task() {
  // This function helps with Keyboard Initialisation, as well as handling Lock LED changes.

  static uint32_t detect_ms = 0;
  static uint8_t detect_init_count = 0;

  if (pcat_state < SET_LOCK_LEDS) {
    // This portion helps with initialisation of the keyboard.
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
          pcat_state = INIT_AWAIT_ACK;
          detect_init_count = 0;
          pcat_command_handler(0xFF);
        }
      } else {
        printf("[DBG] Waiting for Keyboard Clock HIGH\n");
      }
    }
  } else if (pcat_state == INITIALISED) {
    // This portion helps with Lock LED changes.  We only get here once the keyboard has initialised.
    if (ps2_lock_values != pcat_lock_leds) {
      pcat_state = SET_LOCK_LEDS;
      pcat_command_handler(0xED);
    }
  }
}

// Public function to initialise the PC/AT PIO interface.
void pcat_device_setup(uint data_pin) {
  pcat_sm = (uint)pio_claim_unused_sm(pcat_pio, true);
  offset = pio_add_program(pcat_pio, &pcat_interface_program);
  uint pio_irq = PIO1_IRQ_0;

  uint desired_clock_khz = 16;
  float rp_clock_hz = (float)clock_get_hz(clk_sys);
  float div = rp_clock_hz / ((2 * desired_clock_khz) * 10000);
  printf("[INFO] RP2040 Clock Speed: %.0fHz\n", rp_clock_hz);
  printf("[INFO] RP2040 Clock Speed: %.2fMHz\n", rp_clock_hz / 1000000);
  printf("[INFO] Target Interface Clock Speed: %dKHz\n", desired_clock_khz);
  printf("[INFO] Clock Divider: %.2f\n", div);
  printf("[INFO] Expected Clock Speed: %.2fkHz\n", (float)(rp_clock_hz / 10000) / div / 2);
  printf("[INFO] pcxt_interface program loaded at %d with clock divider of %.2f\n", offset, div);

  pcat_interface_program_init(pcat_pio, pcat_sm, offset, data_pin, div);

  irq_set_exclusive_handler(pio_irq, &pcat_input_event_handler);
  irq_set_enabled(pio_irq, true);
}
