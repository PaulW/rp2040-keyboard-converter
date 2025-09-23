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

uint keyboard_sm = 0;
uint keyboard_offset = 0;
PIO keyboard_pio = pio1;
uint keyboard_data_pin;

static enum {
  UNINITIALISED,
  INITIALISED,
} keyboard_state = UNINITIALISED;

/**
 * @brief Processes keyboard event data.
 * This function is responsible for processing keyboard events and updating the keyboard state
 * accordingly. It handles various stages of keyboard initialization. If the keyboard is
 * initialized, it puts the received keycode into the ring buffer for further processing.
 *
 * @param data_byte The data byte received from the keyboard.
 */
static void keyboard_event_processor(uint8_t data_byte) {
  switch (keyboard_state) {
    case UNINITIALISED:
      if (data_byte == 0xAA) {
        printf("[DBG] Keyboard Self-Test Passed\n");
        keyboard_state = INITIALISED;
      } else {
        printf("[ERR] Keyboard Self-Test Failed: 0x%02X\n", data_byte);
        keyboard_state = UNINITIALISED;
        pio_restart(keyboard_pio, keyboard_sm, keyboard_offset);
      }
      break;
    case INITIALISED:
      if (!ringbuf_is_full()) ringbuf_put(data_byte);
  }
#ifdef CONVERTER_LEDS
  converter.state.kb_ready = keyboard_state == INITIALISED ? 1 : 0;
  update_converter_status();
#endif
}

/**
 * @brief IRQ Event Handler used to read keycode data from the XT Keyboard.
 * This function is responsible for handling the interrupt request (IRQ) event that occurs when data
 * is received from the XT Keyboard.
 * - It extracts the start bit and data byte from the received data.
 * - It then performs validation checks on the start bit.
 * - If the validation check fails, an error messages are printed and appropriate action is taken.
 * - If the validation check passes, the data byte is processed by the keyboard_event_processor()
 * function.
 *
 * @note The XT Keyboard Reset Command is not handled here.  This is handled by the actual PIO Code
 * itself, and triggers each time the PIO State Machine is initialised.
 *
 * @note Depending on whether a Genuine IBM or Clone XT Keyboard is used, the start bit may be sent
 * as a single bit or a double bit.  This is handled transparently by the PIO code itself to filter
 * out the double start bit.
 */
static void __isr keyboard_input_event_handler() {
  io_ro_32 data_cast = keyboard_pio->rxf[keyboard_sm] >> 23;
  uint16_t data = (uint16_t)data_cast;

  // Extract the Start Bit.
  uint8_t start_bit = data & 0x1;
  uint8_t data_byte = (uint8_t)((data_cast >> 1) & 0xFF);

  if (start_bit != 1) {
    printf("[ERR] Start Bit Validation Failed: start_bit=%i\n", start_bit);
    keyboard_state = UNINITIALISED;
    pio_restart(keyboard_pio, keyboard_sm, keyboard_offset);
    return;
  }
  keyboard_event_processor(data_byte);
}

/**
 * @brief Task function for the keyboard interface.
 * This function handles the initialization and communication with the keyboard.
 * It is responsible for processing stored keypresses which are held within the ring buffer, and
 * then sends it to the relevant scancode processing function to be processed.  It also handles
 * events if certain conditions are not met within a certain time frame.
 *
 * @note This function should be called periodically in the main loop, or within a task scheduler.
 */
void keyboard_interface_task() {
  static uint8_t detect_stall_count = 0;

  if (keyboard_state == INITIALISED) {
    detect_stall_count = 0;  // Reset the stall count if we're initialised.
    if (!ringbuf_is_empty() && tud_hid_ready()) {
      // We only process the ringbuffer if it's not empty and we're ready to send a HID report.
      // If we don't check for HID ready, we can end up having reports fail to send.

      // Previously we would pause all interrupts while reading the ringbuffer.
      // However, this didn't seem to do anything other than cause latency for keypresses.
      // We may need to revisit this if we encounter issues.
      int c = ringbuf_get();  // Pull from the ringbuffer
      if (c != -1) process_scancode((uint8_t)c);
    }
  } else {
    // This portion helps with initialisation of the keyboard.
    static uint32_t detect_ms = 0;
    // if ((gpio_get(DATA_PIN) == 1) && (gpio_get(DATA_PIN + 1) == 1)) {
    //   printf("[DBG] Keyboard Initialised (BAT OK)!\n");
    //   keyboard_state = INITIALISED;
    // }
    if (board_millis() - detect_ms > 200) {
      detect_ms = board_millis();
      // int pin_state = gpio_get(DATA_PIN + 1);
      if (gpio_get(keyboard_data_pin + 1) == 1) {
        if (detect_stall_count < 5) {
          detect_stall_count++;
          printf("[DBG] Keyboard detected, awaiting ACK (%i/5 attempts)\n", detect_stall_count);
        } else {
          printf("[DBG] Keyboard detected, but no ACK received!\n");
          printf("[DBG] Requesting keyboard reset\n");
          keyboard_state = UNINITIALISED;
          pio_restart(keyboard_pio, keyboard_sm, keyboard_offset);
          detect_stall_count = 0;
        }
      } else if (keyboard_state == UNINITIALISED) {
        printf("[DBG] Awaiting keyboard detection. Please ensure a keyboard is connected.\n");
        detect_stall_count = 0;
      }
#ifdef CONVERTER_LEDS
      converter.state.kb_ready = keyboard_state == INITIALISED ? 1 : 0;
      update_converter_status();
#endif
    }
  }
}

/**
 * @brief Initializes the XT PIO interface for the keyboard.
 * This function initializes the XT PIO interface for the keyboard by performing the following
 * steps:
 * 1. Resets the converter status if CONVERTER_LEDS is defined.
 * 2. Resets the ring buffer.
 * 3. Finds an available PIO to use for the keyboard interface program.
 * 4. Claims the PIO and loads the program.
 * 5. Sets up the IRQ for the PIO state machine.
 * 6. Defines the polling interval and cycles per clock for the state machine.
 * 7. Gets the base clock speed of the RP2040.
 * 8. Initializes the PIO interface program.
 * 9. Sets the IRQ handler and enables the IRQ.
 *
 * @param data_pin The data pin to be used for the keyboard interface.
 */
void keyboard_interface_setup(uint data_pin) {
#ifdef CONVERTER_LEDS
  converter.state.kb_ready = 0;
  update_converter_status();  // Always reset Converter Status here
#endif

  ringbuf_reset();  // Even though Ringbuf is statically initialised, we reset it here to be sure
                    // it's empty.

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
  keyboard_offset = pio_add_program(keyboard_pio, &keyboard_interface_program);
  keyboard_data_pin = data_pin;

  // Define the IRQ for the PIO State Machine.
  // This should either be set to PIO0_IRQ_0 or PIO1_IRQ_0 depending on the PIO used.
  uint pio_irq = keyboard_pio == pio0 ? PIO0_IRQ_0 : PIO1_IRQ_0;

  // The XT Protocol clock will pulse high for ~66us, and low for ~30us.
  float clock_div = calculate_clock_divider(30);

  keyboard_interface_program_init(keyboard_pio, keyboard_sm, keyboard_offset, data_pin, clock_div);

  irq_set_exclusive_handler(pio_irq, &keyboard_input_event_handler);
  irq_set_enabled(pio_irq, true);

  printf("[INFO] PIO%d SM%d Interface program loaded at offset %d with clock divider of %.2f\n",
         (keyboard_pio == pio0 ? 0 : 1), keyboard_sm, keyboard_offset, clock_div);
}
