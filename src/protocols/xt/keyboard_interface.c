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

/**
 * @file keyboard_interface.c
 * @brief IBM XT Keyboard Protocol Implementation
 * 
 * This implementation handles the IBM XT keyboard protocol used by the original
 * IBM PC and PC/XT systems. This is the simplest keyboard protocol featuring:
 * 
 * Physical Interface:
 * - Single-wire DATA connection (no separate clock)
 * - Keyboard self-clocks on the DATA line
 * - 5V TTL logic levels with pull-up resistor
 * - 5-pin DIN connector (original IBM design)
 * 
 * Communication Protocol:
 * - Unidirectional: Keyboard → Host only
 * - LSB-first bit transmission (bit 0 → bit 7)
 * - No start, stop, or parity bits
 * - Fixed 8-bit scan code format
 * - No host commands or bidirectional communication
 * 
 * Timing Characteristics:
 * - Keyboard-controlled timing: ~10 kHz bit rate
 * - Clock pulse: ~40µs low, ~60µs high (~100µs total)
 * - No host flow control or inhibit capability
 * - Immediate transmission when keys pressed/released
 * 
 * Scan Code Format:
 * - Make codes: 0x01-0x53 (key press events)
 * - Break codes: Make code + 0x80 (key release events) 
 * - Single-byte codes only (no multi-byte sequences)
 * - Fixed scan code set (equivalent to later "Set 1")
 * 
 * State Machine:
 * - UNINITIALISED: System startup, waiting for first scan code
 * - INITIALISED: Normal operation, processing scan codes
 * 
 * Advantages:
 * - Simple, reliable protocol with minimal overhead
 * - No initialization sequence required
 * - Immediate operation on power-up
 * - Compatible with basic keyboard functionality
 * 
 * Limitations:
 * - No LED control capability
 * - No typematic rate control
 * - No host-to-keyboard communication
 * - Limited to basic scan code set
 * - No keyboard identification or capabilities detection
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

/* PIO State Machine Configuration */
uint keyboard_sm = 0;              /**< PIO state machine number */
uint keyboard_offset = 0;          /**< PIO program memory offset */
PIO keyboard_pio = pio1;          /**< PIO instance (pio0 or pio1) */
uint keyboard_data_pin;            /**< GPIO pin for DATA line (single wire) */

/**
 * @brief IBM XT Protocol State Machine
 * 
 * XT keyboards use a very simple state machine since they require no
 * initialization sequence and begin transmitting immediately on power-up.
 * 
 * - UNINITIALISED: System startup, hardware setup complete
 * - INITIALISED: Normal operation, receiving and processing scan codes
 */
static enum {
  UNINITIALISED,    /**< Initial state, waiting for system startup */
  INITIALISED,      /**< Normal operation, processing scan codes */
} keyboard_state = UNINITIALISED;

/**
 * @brief Process a single IBM XT keyboard scan code and update the keyboard interface state.
 *
 * Evaluates a received XT scan code: if the interface is uninitialised the function
 * accepts the 0xAA power-on self-test pass code to transition to normal operation;
 * any other value causes the interface to remain uninitialised and restarts the PIO.
 * When initialised, valid scan codes are enqueued to the ring buffer for later HID processing.
 *
 * @param data_byte 8-bit XT scan code (LSB-first). 0xAA indicates self-test pass; make codes are 0x01–0x53 and break codes are make+0x80.
 *
 * @note May be called from interrupt context; work is intentionally minimal.
 * @note Enqueues scan codes to the ring buffer and may restart the PIO on self-test failure.
 */
static void keyboard_event_processor(uint8_t data_byte) {
  switch (keyboard_state) {
    case UNINITIALISED:
      if (data_byte == XT_RESP_BAT_PASSED) {
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
 * Handle incoming 9-bit XT keyboard frames from the PIO RX FIFO and forward valid scan codes for processing.
 *
 * Validates the frame start bit and, if valid, extracts the 8-bit scan code and passes it to keyboard_event_processor.
 * If the start bit is invalid, the function marks the keyboard as uninitialised and restarts the PIO state machine to recover.
 *
 * @note This function is an interrupt service routine and must complete quickly.
 * @note Start bit validation requires the start bit to be `1`; invalid frames trigger a PIO restart and state reset.
 */
static void __isr keyboard_input_event_handler() {
  io_ro_32 data_cast = keyboard_pio->rxf[keyboard_sm] >> 23;
  uint16_t data = (uint16_t)data_cast;

  // Parse XT frame components from 9-bit PIO data (Start + 8 data bits)
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
 * Manage XT keyboard state, detect and recover from connection failures, and forward received scan codes to the HID pipeline.
 *
 * When the interface is INITIALISED, attempts to consume scan codes from the ring buffer and submit them to HID when the HID stack is ready.
 * When UNINITIALISED, periodically checks the keyboard clock line to detect presence, performs a limited retry/detection cycle, and restarts the PIO state machine on failures to recover the interface.
 *
 * @note Call periodically from the main loop (typical interval: 1–10 ms).
 * @note Operation is non-blocking; scan codes are processed only when the HID layer is ready to accept reports.
void keyboard_interface_task() {
  static uint8_t detect_stall_count = 0;

  if (keyboard_state == INITIALISED) {
    detect_stall_count = 0;  // Clear detection counter - keyboard is operational
    if (!ringbuf_is_empty() && tud_hid_ready()) {
      // Process scan codes only when HID interface ready to prevent report queue overflow
      // HID ready check ensures reliable USB report transmission

      // Historical note: interrupt disabling during ring buffer read was tested
      // but provided no benefit and increased input latency - removed for performance
      uint8_t scancode = ringbuf_get();  // Retrieve next scan code from interrupt-filled buffer
      process_scancode(scancode);
    }
  } else {
    // Keyboard detection and initialization management
    static uint32_t detect_ms = 0;
    // Alternative detection method (unused):
    // if (gpio_get(keyboard_data_pin) == 1) {
    //   keyboard_state = INITIALISED;
    // }
    if (board_millis() - detect_ms > 200) {
      detect_ms = board_millis();
      // Monitor CLOCK line idle state for keyboard presence. Keyboard also pulls CLOCK HIGH to signify ACK during init.
      // XT protocol has no host-to-keyboard communication, so only presence detection is possible.
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
 * Initialize the IBM XT keyboard interface on the RP2040 and prepare it to receive XT scan codes.
 *
 * Configures an available PIO and state machine, loads the XT receive program, configures the provided
 * GPIO as the single DATA line, computes and applies an appropriate clock divider for XT timing,
 * installs the PIO IRQ handler, and resets internal buffers/state so the converter is ready to detect
 * and receive keyboard frames. If no suitable PIO is available the function returns without configuring
 * the interface.
 *
 * @param data_pin GPIO pin number to use for the XT single DATA line.
 */
void keyboard_interface_setup(uint data_pin) {
#ifdef CONVERTER_LEDS
  converter.state.kb_ready = 0;
  update_converter_status();  // Initialize converter status LEDs to "not ready" state
#endif

  ringbuf_reset();  // Ensure clean startup state despite static initialization

  // Locate available PIO instance with sufficient program memory space
  // find_available_pio() searches PIO0 and PIO1 for program capacity
  // Returns NULL if neither PIO has space for keyboard interface program

  keyboard_pio = find_available_pio(&keyboard_interface_program);
  if (keyboard_pio == NULL) {
    printf("[ERR] No PIO available for Keyboard Interface Program\n");
    return;
  }

  // Allocate PIO resources and load XT protocol program
  keyboard_sm = (uint)pio_claim_unused_sm(keyboard_pio, true);
  keyboard_offset = pio_add_program(keyboard_pio, &keyboard_interface_program);
  keyboard_data_pin = data_pin;

  // Configure PIO interrupt based on allocated instance (PIO0_IRQ_0 or PIO1_IRQ_0)
  uint pio_irq = keyboard_pio == pio0 ? PIO0_IRQ_0 : PIO1_IRQ_0;

  // XT timing: ~10µs minimum pulse width for reliable signal detection)
  float clock_div = calculate_clock_divider(10);

  keyboard_interface_program_init(keyboard_pio, keyboard_sm, keyboard_offset, data_pin, clock_div);

  irq_set_exclusive_handler(pio_irq, &keyboard_input_event_handler);
  irq_set_enabled(pio_irq, true);
  irq_set_priority(pio_irq, 0);

  printf("[INFO] PIO%d SM%d Interface program loaded at offset %d with clock divider of %.2f\n",
         (keyboard_pio == pio0 ? 0 : 1), keyboard_sm, keyboard_offset, clock_div);
}
