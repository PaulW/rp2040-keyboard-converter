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
 * @brief AT/PS2 Keyboard Protocol Implementation
 * 
 * This implementation handles the AT and PS/2 keyboard protocols used by IBM PC
 * compatible systems from the 1980s onwards. The protocols feature:
 * 
 * Physical Interface:
 * - 2-wire synchronous serial communication (DATA + CLOCK)
 * - Open-drain signals with pull-up resistors
 * - 5V TTL logic levels (AT) or 5V/3.3V (PS/2)
 * - DIN connector (AT) or mini-DIN connector (PS/2)
 * 
 * Communication Protocol:
 * - LSB-first bit transmission (bit 0 → bit 7)
 * - Frame format: Start(0) + 8 data bits + Parity(odd) + Stop(1)
 * - Keyboard controls clock for transmission to host
 * - Host can inhibit by pulling CLOCK low
 * - Bidirectional with command/response capability
 * 
 * Timing Characteristics:
 * - Clock frequency: 10-16.7 kHz (typical 10-15 kHz)
 * - Clock pulse width: minimum 30µs low, 30µs high
 * - Data setup time: minimum 5µs before clock falling edge
 * - Data hold time: minimum 5µs after clock falling edge
 * - Inhibit time: minimum 100µs CLOCK low stops transmission
 * 
 * State Machine:
 * 1. UNINITIALISED → Reset keyboard and wait for self-test
 * 2. INIT_AWAIT_SELFTEST → Wait for 0xAA (BAT completion)
 * 3. INIT_READ_ID → Request and read 2-byte keyboard ID
 * 4. INIT_SETUP → Configure scan code set and keyboard parameters
 * 5. SET_LOCK_LEDS → Initialize LED states
 * 6. INITIALISED → Normal operation with scan code processing
 * 
 * Special Features:
 * - Multiple scan code set support (Sets 1, 2, 3)
 * - LED control (Caps/Num/Scroll Lock)
 * - Typematic repeat rate configuration
 * - Z-150 keyboard compatibility (non-standard stop bit)
 * - Terminal keyboard support (Set 3 scan codes)
 * 
 * Error Handling:
 * - Automatic retry for failed commands
 * - Timeout detection with state machine reset
 * - Parity error detection and recovery
 * - Graceful handling of non-standard keyboards
 */

#include "keyboard_interface.h"

#include <math.h>

#include "bsp/board.h"
#include "buzzer.h"
#include "common_interface.h"
#include "hardware/clocks.h"
#include "interface.pio.h"
#include "led_helper.h"
#include "pio_helper.h"
#include "ringbuf.h"
#include "scancode.h"

/* PIO State Machine Configuration */
uint keyboard_sm = 0;              /**< PIO state machine number */
uint keyboard_offset = 0;          /**< PIO program memory offset */
PIO keyboard_pio;                  /**< PIO instance (pio0 or pio1) */
uint16_t keyboard_id = ATPS2_KEYBOARD_ID_UNKNOWN;     /**< Keyboard identification code (2 bytes) */
uint keyboard_data_pin;            /**< GPIO pin for DATA line (CLOCK = DATA + 1) */

/**
 * @brief Terminal Keyboard Detection Macro
 * 
 * Determines if the configured keyboard uses Scan Code Set 3, which indicates
 * a terminal keyboard requiring special initialization and handling procedures.
 * Terminal keyboards often have different capabilities and command sets.
 */
#define CODESET_3 (strcmp(KEYBOARD_CODESET, "set3") == 0)

/* Protocol State and Configuration Variables */
static volatile uint8_t keyboard_lock_leds = 0;     /**< Current LED state (Caps/Num/Scroll Lock) */
static bool id_retry = false;                       /**< Flag indicating ID read retry attempt */

/**
 * @brief Stop Bit Compliance Detection
 * 
 * AT/PS2 protocol specifies stop bit should be HIGH (1) after parity bit.
 * Some non-standard keyboards (like Zenith Z-150) use LOW (0) stop bit.
 * This detection helps identify and accommodate non-compliant keyboards.
 * 
 * @see interface.pio for detailed signal timing implementation
 */
static enum {
  STOP_BIT_LOW,     /**< Non-standard stop bit (Z-150 style keyboards) */
  STOP_BIT_HIGH     /**< Standard AT/PS2 stop bit (compliant keyboards) */
} stop_bit_state = STOP_BIT_HIGH;

/**
 * @brief AT/PS2 Protocol State Machine
 * 
 * Manages the complete initialization and operational phases:
 * - UNINITIALISED: System startup, sending reset command
 * - INIT_AWAIT_ACK: Waiting for acknowledge to reset command  
 * - INIT_AWAIT_SELFTEST: Waiting for Basic Assurance Test (BAT) completion (0xAA)
 * - INIT_READ_ID_1: Reading first byte of keyboard identification
 * - INIT_READ_ID_2: Reading second byte of keyboard identification  
 * - INIT_SETUP: Configuring scan code set and keyboard parameters
 * - SET_LOCK_LEDS: Setting initial LED states
 * - INITIALISED: Normal operation, processing scan codes
 */
static enum {
  UNINITIALISED,        /**< Initial state, sending keyboard reset */
  INIT_AWAIT_ACK,       /**< Waiting for reset command acknowledgment */
  INIT_AWAIT_SELFTEST,  /**< Waiting for self-test completion (0xAA) */
  INIT_READ_ID_1,       /**< Reading first byte of keyboard ID */
  INIT_READ_ID_2,       /**< Reading second byte of keyboard ID */
  INIT_SETUP,           /**< Configuring scan code set and options */
  SET_LOCK_LEDS,        /**< Setting LED states */
  INITIALISED,          /**< Normal operation, processing scan codes */
} keyboard_state = UNINITIALISED;

/**
 * Queue a host-to-keyboard command framed with odd parity for transmission over AT/PS/2.
 *
 * Formats the 8-bit command into the AT/PS/2 host-to-keyboard frame (data plus odd parity)
 * and enqueues the resulting word to the PIO transmitter.
 *
 * @param data_byte Host command byte to send to the keyboard.
 * @note Blocks until the PIO state machine accepts the frame.
 * @note Keyboard responses are handled asynchronously by the input IRQ handler.
 */
static void keyboard_command_handler(uint8_t data_byte) {
  uint16_t data_with_parity = (uint16_t)(data_byte + (interface_parity_table[data_byte] << 8));
  pio_sm_put(keyboard_pio, keyboard_sm, data_with_parity);
}

/**
 * Process a single byte received from the AT/PS/2 keyboard and advance the interface state machine.
 *
 * Handles initialization sequencing (power-on self-test, ACK handling, 2‑byte ID reads, setup),
 * LED synchronization, terminal-keyboard configuration, error recovery/retry logic, and normal
 * scan-code queuing for HID processing.
 *
 * This function updates keyboard_state, issues host-to-keyboard commands as needed, and enqueues
 * valid scan codes into the ring buffer; it also updates converter LED status when enabled.
 *
 * @param data_byte 8-bit value received from the keyboard (initialization response or scan code)
 *
 * @note Executes in interrupt context; keep processing efficient. 
 */
static void keyboard_event_processor(uint8_t data_byte) {
  switch (keyboard_state) {
    case UNINITIALISED:
      id_retry = false;      // Clear retry flag - starting fresh initialization sequence
      keyboard_id = ATPS2_KEYBOARD_ID_UNKNOWN;  // Reset keyboard ID to unknown state during initialization
      switch (data_byte) {
        case ATPS2_RESP_BAT_PASSED:
          // Power-on self-test completed successfully (Basic Assurance Test)
          // Keyboard passed internal diagnostics and is ready
          printf("[DBG] Keyboard Self Test OK!\n");
          buzzer_play_sound_sequence_non_blocking(READY_SEQUENCE);
          keyboard_lock_leds = 0;
          printf("[DBG] Waiting for Keyboard ID...\n");
          keyboard_state = INIT_READ_ID_1;
          break;
        default:
          // Unexpected response during initialization - could be failed BAT (0xFC)
          // or other power-on state requiring explicit reset command
          printf("[DBG] Asking Keyboard to Reset\n");
          keyboard_state = INIT_AWAIT_ACK;
          keyboard_command_handler(ATPS2_CMD_RESET);
      }
      break;
    case INIT_AWAIT_ACK:
      switch (data_byte) {
        case ATPS2_RESP_ACK:
          printf("[DBG] ACK Received after Reset\n");
          keyboard_state = INIT_AWAIT_SELFTEST;
          break;
        default:
          printf("[DBG] Unknown ACK Response (0x%02X).  Asking again to Reset...\n", data_byte);
          keyboard_command_handler(ATPS2_CMD_RESET);
      }
      break;
    case INIT_AWAIT_SELFTEST:
      switch (data_byte) {
        case ATPS2_RESP_BAT_PASSED:
          printf("[DBG] Keyboard Self Test OK!\n");
          buzzer_play_sound_sequence_non_blocking(READY_SEQUENCE);
          keyboard_lock_leds = 0;
          // Proceed to keyboard identification phase for terminal keyboard detection
          printf("[DBG] Waiting for Keyboard ID...\n");
          keyboard_state = INIT_READ_ID_1;
          break;
        default:
          printf("[DBG] Self-Test invalid response (0x%02X).  Asking again to Reset...\n",
                 data_byte);
          keyboard_state = INIT_AWAIT_ACK;
          keyboard_command_handler(ATPS2_CMD_RESET);
      }
      break;

    // Terminal keyboards respond with 2-byte ID after 0xF2 command
    // Timeout handling (500ms) managed by main task function if no response received
    case INIT_READ_ID_1:
      switch (data_byte) {
        case ATPS2_RESP_ACK:
          printf("[DBG] ACK Keyboard ID Request\n");
          printf("[DBG] Waiting for Keyboard ID...\n");
          break;
        default:
          printf("[DBG] Keyboard First ID Byte read as 0x%02X\n", data_byte);
          keyboard_id &= ATPS2_KEYBOARD_ID_LOW_MASK;
          keyboard_id |= (uint16_t)data_byte << 8;
          keyboard_state = INIT_READ_ID_2;
      }
      break;
    case INIT_READ_ID_2:
      printf("[DBG] Keyboard Second ID Byte read as 0x%02X\n", data_byte);
      keyboard_id &= ATPS2_KEYBOARD_ID_HIGH_MASK;
      keyboard_id |= (uint16_t)data_byte;
      printf("[DBG] Keyboard ID: 0x%04X\n", keyboard_id);
      // Configure terminal keyboards for proper operation
      if (CODESET_3) {
        // Terminal keyboards (Set 3) require explicit make/break mode configuration
        // Command enables both key press and release events for all keys
        printf("[DBG] Setting all Keys to Make/Break\n");
        keyboard_command_handler(ATPS2_KEYBOARD_CMD_SET_ALL_MAKEBREAK);
        keyboard_state = INIT_SETUP;
      } else {
        printf("[DBG] Keyboard Initialised!\n");
        keyboard_state = INITIALISED;
      }
      break;
    case INIT_SETUP:
      // Process acknowledgment for make/break configuration command
      switch (data_byte) {
        case ATPS2_RESP_ACK:
          printf("[DBG] Keyboard Initialised!\n");
          keyboard_state = INITIALISED;
          break;
        default:
          printf("[DBG] Unknown Response (0x%02X).  Continuing...\n", data_byte);
          keyboard_id = ATPS2_KEYBOARD_ID_UNKNOWN;
          printf("[DBG] Keyboard Initialised!\n");
          keyboard_state = INITIALISED;
      }
      break;

    // Handle LED state synchronization with host lock key status
    case SET_LOCK_LEDS:
      // Process LED command sequence responses (0xED command followed by LED bitmap)
      switch (data_byte) {
        case 0xFA:
          if (lock_leds.value != keyboard_lock_leds) {
            keyboard_lock_leds = lock_leds.value;
            keyboard_command_handler((uint8_t)((lock_leds.keys.capsLock << 2) |
                                               (lock_leds.keys.numLock << 1) |
                                               lock_leds.keys.scrollLock));
          } else {
            // LED command sequence completed successfully - return to normal operation
            buzzer_play_sound_sequence_non_blocking(LOCK_LED);
            keyboard_state = INITIALISED;
          }
          break;
        default:
          printf("[DBG] SET_LOCK_LED FAILED (0x%02X)\n", data_byte);
          keyboard_lock_leds = lock_leds.value;
          keyboard_state = INITIALISED;
      }
      break;

    // Normal operation: queue scan codes for HID processing
    case INITIALISED:
      if (!ringbuf_is_full()) ringbuf_put(data_byte);
  }
#ifdef CONVERTER_LEDS
  converter.state.kb_ready = keyboard_state == INITIALISED ? 1 : 0;
  update_converter_status();
#endif
}

/**
 * Handle a single AT/PS/2 frame received from the PIO, validate it, and forward valid scancodes to the keyboard event processor.
 *
 * Validates start bit, odd parity, and stop-bit polarity; adapts stop-bit state for non‑standard keyboards.
 * On parity mismatch issues an ATPS2 RESEND (0xFE) and aborts processing; on start-bit or other fatal frame errors
 * resets the keyboard state machine and restarts the PIO. Detects a known power‑on connection artifact (0x54 with
 * invalid parity) and treats it as a keyboard connection event by returning to UNINITIALISED and restarting the PIO.
 *
 * @note Executes in interrupt context; must not perform blocking operations.
 * @note Reads raw 11-bit frames directly from the PIO RX FIFO (Start + 8 data bits + Parity + Stop).
 */
static void __isr keyboard_input_event_handler() {
  io_ro_32 data_cast = keyboard_pio->rxf[keyboard_sm] >> 21;
  uint16_t data = (uint16_t)data_cast;

  // Parse AT/PS2 frame components from 11-bit PIO data
  uint8_t start_bit = data & 0x1;
  uint8_t parity_bit = (data >> 9) & 0x1;
  uint8_t stop_bit = (data >> 10) & 0x1;
  uint8_t data_byte = (uint8_t)((data_cast >> 1) & 0xFF);
  uint8_t parity_bit_check = interface_parity_table[data_byte];

  // Detect and adapt to non-standard stop bit behavior (Z-150 compatibility)
  if (stop_bit_state != (stop_bit ? STOP_BIT_HIGH : STOP_BIT_LOW)) {
    stop_bit_state = stop_bit ? STOP_BIT_HIGH : STOP_BIT_LOW;
    printf("[DBG] Stop Bit %s Detected\n", stop_bit ? "High" : "Low");
  }

  if (start_bit != 0 || parity_bit != parity_bit_check) {
    if (start_bit != 0) printf("[ERR] Start Bit Validation Failed: start_bit=%i\n", start_bit);
    if (parity_bit != parity_bit_check) {
      printf("[ERR] Parity Bit Validation Failed: expected=%i, actual=%i\n", parity_bit_check,
             parity_bit);
      if (data_byte == 0x54 && parity_bit == 1) {
        // Power-on connection artifact: keyboard briefly pulls lines low during startup
        // This causes bit shift in FIFO changing 0xAA to 0x54 with invalid parity
        // Historical fix preserved for reference - indicates keyboard connection event
        printf("[DBG] Likely Keyboard Connect Event detected.\n");
        keyboard_state = UNINITIALISED;
        id_retry = false;
        pio_restart(keyboard_pio, keyboard_sm, keyboard_offset);
      }
      // Request data retransmission on parity error
      keyboard_command_handler(ATPS2_CMD_RESEND);
      return;  // Abort processing - wait for retransmitted data
    }
    // Frame validation failed - restart protocol state machine for clean recovery
    keyboard_state = UNINITIALISED;
    id_retry = false;
    pio_restart(keyboard_pio, keyboard_sm, keyboard_offset);
    return;
  }

  keyboard_event_processor(data_byte);
}

/**
 * Coordinate AT/PS/2 keyboard initialization, LED synchronization, and scan-code processing.
 *
 * Handles the interface state machine: when INITIALISED it synchronizes lock LEDs
 * and forwards scan codes from the ring buffer to HID when the HID layer is ready;
 * when not INITIALISED it manages initialization timeouts, retry logic, keyboard
 * presence detection, ID/read/LED setup timeouts, and state recovery.
 *
 * @note Call periodically from the main loop (recommended interval: 1–10 ms).
 * @note Function is non-blocking and designed to avoid HID report queue overflow.
 */
void keyboard_interface_task() {
  static uint8_t detect_stall_count = 0;

  if (keyboard_state == INITIALISED) {
    // Normal operation: process scan codes and manage LED synchronization
    // LED state changes and terminal keyboard features handled here
    detect_stall_count = 0;  // Clear timeout counter - keyboard is operational
    if (lock_leds.value != keyboard_lock_leds) {
      keyboard_state = SET_LOCK_LEDS;
      keyboard_command_handler(ATPS2_KEYBOARD_CMD_SET_LEDS);
    } else {
      if (!ringbuf_is_empty() && tud_hid_ready()) {
        // Process scan codes only when HID interface ready to prevent report queue overflow
        // Historical note: interrupt disabling during ring buffer read was tested
        // but provided no benefit and increased input latency - removed for performance
        uint8_t scancode = ringbuf_get();  // Retrieve next scan code from interrupt-filled buffer
        process_scancode(scancode);
      }
    }
  } else {
    // Initialization timeout management and keyboard detection logic
    static uint32_t detect_ms = 0;
    if (board_millis() - detect_ms > 200) {
      detect_ms = board_millis();
      if (gpio_get(keyboard_data_pin + 1) == 1) {
        // Monitor initialization progress when keyboard detected (CLOCK line high)
        detect_stall_count++;  // Increment timeout counter for initialization state monitoring
        switch (keyboard_state) {
          case INIT_READ_ID_1 ... INIT_SETUP:
            if (detect_stall_count > 2) {
              // Initialization timeout detected during ID read or setup phase
              if (!id_retry) {
                // First timeout - retry keyboard ID request before giving up
                printf("[DBG] Keyboard ID/Setup Timeout, retrying...\n");
                id_retry = true;
                keyboard_state = INIT_READ_ID_1;  // Return to ID read state for retry
                keyboard_command_handler(ATPS2_CMD_GET_ID);   // Send keyboard identification request
                detect_stall_count = 0;  // Reset timeout counter for retry attempt
              } else {
                printf("[DBG] Keyboard Read ID/Setup Timed out again, continuing with defaults.\n");
                keyboard_id = ATPS2_KEYBOARD_ID_UNKNOWN;
                printf("[DBG] Keyboard Initialised!\n");
                keyboard_state = INITIALISED;
                detect_stall_count = 0;
              }
            }
            break;
          case SET_LOCK_LEDS:
            if (detect_stall_count > 2) {
              // LED command timeout - continue without LED synchronization
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
              keyboard_command_handler(ATPS2_CMD_RESET);
            }
            break;
        }
      } else if (keyboard_state == UNINITIALISED) {
        printf("[DBG] Awaiting keyboard detection. Please ensure a keyboard is connected.\n");
        // Clear timeout counter while waiting for keyboard connection
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
 * Initialize the AT/PS/2 keyboard interface, configure PIO state machine, GPIOs, interrupts, and related software state for keyboard communication.
 *
 * Performs PIO selection and program loading, claims a state machine, configures DATA (and CLOCK = DATA+1) pins and pull-ups, sets timing for AT/PS/2 protocol, installs the PIO IRQ handler, and resets internal buffers and protocol state.
 *
 * @param data_pin GPIO pin number to use for the DATA line; the CLOCK line used by this interface is data_pin + 1.
 *
 * @note CLOCK pin must be DATA pin + 1.
 * @note Call this before any other keyboard interface operations; the function completes hardware and software initialization before returning.
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

  keyboard_pio = find_available_pio(&pio_interface_program);
  if (keyboard_pio == NULL) {
    printf("[ERR] No PIO available for Keyboard Interface Program\n");
    return;
  }

  // Allocate PIO resources and load AT/PS2 protocol program
  keyboard_sm = (uint)pio_claim_unused_sm(keyboard_pio, true);
  keyboard_offset = pio_add_program(keyboard_pio, &pio_interface_program);
  keyboard_data_pin = data_pin;

  // Configure PIO interrupt based on allocated instance (PIO0_IRQ_0 or PIO1_IRQ_0)
  uint pio_irq = keyboard_pio == pio0 ? PIO0_IRQ_0 : PIO1_IRQ_0;

  // AT/PS2 timing: 30µs minimum pulse width per IBM Technical Reference
  // Reference: IBM 84F9735 PS/2 Hardware Interface Technical Reference
  float clock_div = calculate_clock_divider(ATPS2_TIMING_CLOCK_MIN_US);

  pio_interface_program_init(keyboard_pio, keyboard_sm, keyboard_offset, data_pin, clock_div);

  irq_set_exclusive_handler(pio_irq, &keyboard_input_event_handler);
  irq_set_enabled(pio_irq, true);
  irq_set_priority(pio_irq, 0);

  printf("[INFO] PIO%d SM%d Interface program loaded at offset %d with clock divider of %.2f\n",
         (keyboard_pio == pio0 ? 0 : 1), keyboard_sm, keyboard_offset, clock_div);
}
