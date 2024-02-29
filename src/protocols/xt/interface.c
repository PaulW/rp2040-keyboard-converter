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
#include "config.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "interface.pio.h"
#include "ringbuf.h"
#include "scancode.h"

uint keyboard_sm = 0;
uint offset = 0;
PIO keyboard_pio = pio1;

static enum {
  UNINITIALISED,
  INITIALISED,
} keyboard_state = UNINITIALISED;

void keyboard_pio_restart() {
  // Restart the XT PIO State Machine
  printf("[DBG] Resetting State Machine and Keyboard (Offset: 0x%02X)\n", offset);
  keyboard_state = UNINITIALISED;
  pio_sm_drain_tx_fifo(keyboard_pio, keyboard_sm);
  pio_sm_clear_fifos(keyboard_pio, keyboard_sm);
  pio_sm_restart(keyboard_pio, keyboard_sm);
  pio_sm_exec(keyboard_pio, keyboard_sm, pio_encode_jmp(offset));
  printf("[DBG] State Machine Restarted\n");
}

static void keyboard_event_processor(uint8_t data_byte) {
  switch (keyboard_state) {
    case UNINITIALISED:
      if (data_byte == 0xAA) {
        printf("[DBG] Keyboard Self-Test Passed\n");
        keyboard_state = INITIALISED;
      } else {
        printf("[ERR] Keyboard Self-Test Failed: 0x%02X\n", data_byte);
        keyboard_pio_restart();
        keyboard_state = UNINITIALISED;
      }
      break;
    case INITIALISED:
      if (!ringbuf_is_full()) ringbuf_put(data_byte);
  }
}

// IRQ Event Handler used to read keycode data from the XT Keyboard
// and ensure relevant code is valid when checking against relevant check bits.
static void __isr keyboard_input_event_handler() {
  io_ro_32 data_cast = keyboard_pio->rxf[keyboard_sm] >> 23;
  uint16_t data = (uint16_t)data_cast;

  // Extract the Start Bit.
  uint8_t start_bit = data & 0x1;
  uint8_t data_byte = (uint8_t)((data_cast >> 1) & 0xFF);

  if (start_bit != 1) {
    printf("[ERR] Start Bit Validation Failed: start_bit=%i\n", start_bit);
    keyboard_pio_restart();
    return;
  }

  keyboard_event_processor(data_byte);
}

void keyboard_interface_task() {
  if (keyboard_state == INITIALISED) {
    // This portion helps with Lock LED changes.  We only get here once the keyboard has initialised.

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
  } else {
    // This portion helps with initialisation of the keyboard.
    static uint8_t detect_init_count = 0;
    static uint32_t detect_ms = 0;
    // if ((gpio_get(DATA_PIN) == 1) && (gpio_get(DATA_PIN + 1) == 1)) {
    //   printf("[DBG] Keyboard Initialised (BAT OK)!\n");
    //   keyboard_state = INITIALISED;
    // }
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
          keyboard_pio_restart();
          detect_init_count = 0;
        }
      } else {
        printf("[DBG] Waiting for Keyboard Clock HIGH\n");
      }
    }
  }
}

// Public function to initialise the XT PIO interface.
void keyboard_interface_setup(uint data_pin) {
  keyboard_sm = (uint)pio_claim_unused_sm(keyboard_pio, true);
  offset = pio_add_program(keyboard_pio, &keyboard_interface_program);
  uint pio_irq = PIO1_IRQ_0;

  // Define the Polling Inteval for the State Machine.
  // The XT Protocol runs at a clock speed of around 31kHz due to the clock
  // cycle of 12.5us LOW and 20us HIGH (32.5us total) per Clock Cycle.
  // There are a total maximum of 11 data bits we need to read per clock, due to
  // the possibility of a double-start bit on Genuine XT Keyboards.  XT Clone Keyboards
  // will only ever send a single start bit, but we need to be able to handle both.
  // As such, we set the polling interval to the duration of the HIGH clock cycle (20us),
  // but ensure we have 11 SM cycles per clock cycle to ensure we operate at a high enough
  // clock speed to ensure we can capture the first start bit if we are receiving a double,
  // as this does need detecting within 5us of clock going LOW.

  int polling_interval_us = 20;
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
