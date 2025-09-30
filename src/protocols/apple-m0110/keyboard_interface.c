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
 * @brief Apple M0110 Keyboard Protocol Implementation
 * 
 * This implementation handles the Apple M0110 keyboard protocol used by early Macintosh
 * computers (1984-1987). The protocol features:
 * 
 * Physical Interface:
 * - 2-wire synchronous serial communication (DATA + CLOCK)
 * - Open-drain signals with pull-up resistors
 * - Keyboard controls clock timing
 * - 5V TTL logic levels
 * 
 * Communication Protocol:
 * - MSB-first bit transmission (bit 7 → bit 0)
 * - No start, parity, or stop bits
 * - 8-bit command/response format
 * - Keyboard-controlled clock for both TX and RX
 * 
 * Timing Characteristics:
 * - Keyboard TX: ~3.03kHz (330µs cycles: 160µs low, 170µs high)
 * - Host TX: ~2.5kHz (400µs cycles: 180µs low, 220µs high)
 * - Request-to-Send: 840µs DATA low pulse
 * - Data hold: 80µs minimum after final clock edge
 * 
 * State Machine:
 * 1. UNINITIALISED → Wait for startup delay (1000ms)
 * 2. INIT_MODEL_REQUEST → Send model command, wait for response (500ms retry intervals)
 * 3. INITIALISED → Normal operation with response timeout monitoring (500ms)
 * 
 * Error Handling:
 * - Timeout detection (500ms max response time)
 * - Timer wraparound protection for robust timeout calculations
 * - Automatic retry with maximum attempt limits
 * - State machine reset on communication failure
 * - PIO FIFO overflow protection
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

/* Compile-time validation of timing constants */
_Static_assert(M0110_INITIALIZATION_DELAY_MS >= 500, 
               "M0110 initialization delay must be at least 500ms for reliable keyboard startup");
_Static_assert(M0110_MODEL_RETRY_INTERVAL_MS >= 100, 
               "M0110 model retry interval must be at least 100ms");
_Static_assert(M0110_RESPONSE_TIMEOUT_MS >= 100, 
               "M0110 response timeout must be at least 100ms for reliable communication");
_Static_assert(M0110_MODEL_RETRY_INTERVAL_MS <= 2000,
               "M0110 model retry interval should not exceed 2 seconds");

/* PIO State Machine Configuration */
unsigned int keyboard_sm = 0;          /**< PIO state machine number */
unsigned int keyboard_offset = 0;      /**< PIO program memory offset */
PIO keyboard_pio;                      /**< PIO instance (pio0 or pio1) */
unsigned int keyboard_data_pin;        /**< GPIO pin for DATA line (CLOCK = DATA + 1) */

/**
 * @brief Apple M0110 Protocol State Machine
 * 
 * The state machine manages the initialization and operation phases:
 * - UNINITIALISED: Initial state, waiting for 1000ms startup delay
 * - INIT_MODEL_REQUEST: Sending Model Number commands every 500ms, waiting for identification  
 * - INITIALISED: Normal operation with key processing and 500ms response timeout monitoring
 */
static enum {
  UNINITIALISED,        /**< System startup, waiting for initial delay */
  INIT_MODEL_REQUEST,   /**< Sending model commands during initialization */
  INITIALISED           /**< Initialization complete, transitioning to operation */
} keyboard_state = UNINITIALISED;

/* Timing and State Tracking Variables */
static volatile uint32_t last_command_time = 0;     /**< Timestamp of last command sent (for timeouts) */
static volatile uint8_t model_retry_count = 0;      /**< Number of model command retries attempted */
static volatile uint32_t last_response_time = 0;    /**< Timestamp of last keyboard response received */

/**
 * Transmit an 8-bit M0110 command to the keyboard using the configured PIO state machine.
 *
 * The command is sent MSB-first according to the M0110 wire protocol. If the PIO TX FIFO is full
 * at the time of invocation, the command is dropped.
 *
 * @param command 8-bit M0110 command code to transmit (MSB transmitted first)
 */
static void keyboard_command_handler(uint8_t command) {
  if (pio_sm_is_tx_fifo_full(keyboard_pio, keyboard_sm)) {
    printf("[WARN] M0110 TX FIFO full, command 0x%02X dropped\n", command);
    return;
  }

  // printf("[DBG] M0110 sending command: 0x%02X\n", command);
  // M0110 protocol uses MSB-first transmission - position command in upper byte
  // PIO automatically handles MSB-first bit order during transmission
  pio_sm_put(keyboard_pio, keyboard_sm, (uint32_t)command << 24);
}

/**
 * Process a single 8-bit response byte from the Apple M0110 keyboard and advance the protocol state.
 *
 * This function is called from interrupt context to perform minimal, observable processing of a keyboard
 * response: it updates the last-response timestamp, advances initialization when a model response arrives,
 * and handles normal polling responses. In the INIT_MODEL_REQUEST state it recognizes the M0110A model,
 * transitions to INITIALISED and issues an inquiry. In the INITIALISED state a NULL response triggers another
 * inquiry; other bytes are treated as scan codes or special responses and are enqueued for main-task
 * processing (dropped if the ring buffer is full), followed by issuing another inquiry. Side effects include
 * state transitions, enqueueing of scancodes, issuing commands to the keyboard, and updates to timing state.
 *
 * @param data_byte 8-bit response received from the keyboard (MSB-first)
 */
static void keyboard_event_processor(uint8_t data_byte) {
  // printf("[DBG] M0110 received: 0x%02X\n", data_byte);
  last_response_time = board_millis(); // Record response time for timeout management
  
  switch (keyboard_state) {
    case UNINITIALISED:
      // Should not receive data in UNINITIALISED state - ignore
      printf("[WARN] M0110 received data in UNINITIALISED state: 0x%02X\n", data_byte);
      break;
    case INIT_MODEL_REQUEST:
      // Handle model number response during initialization phase
      switch (data_byte) {
        case M0110_RESP_MODEL_M0110A:
          printf("[INFO] Apple M0110 Keyboard Model: M0110A - keyboard reset and ready\n");
          break;
        default:
          printf("[DBG] Unknown model response: 0x%02X - proceeding with initialization\n", data_byte);
          break;
      }
      keyboard_state = INITIALISED;
      keyboard_command_handler(M0110_CMD_INQUIRY);
      last_command_time = board_millis();
      break;
      
    case INITIALISED:
      // Handle inquiry responses during normal polling operation
      switch (data_byte) {
        case M0110_RESP_NULL:
          // NULL response (0x7B) - no keys pressed, keyboard is ready
          // Send next inquiry command to maintain continuous polling
          keyboard_command_handler(M0110_CMD_INQUIRY);
          last_command_time = board_millis();
          break;
        default:
          // Scan code or special response - queue for main task processing
          if (!ringbuf_is_full()) {
            ringbuf_put(data_byte);
          } else {
            printf("[ERR] Ring buffer full! Scancode 0x%02X lost\n", data_byte);
          }
          
          // Send next inquiry to continue polling - critical for responsiveness
          keyboard_command_handler(M0110_CMD_INQUIRY);
          last_command_time = board_millis();
          break;
      }
      break;
      
    default:
      break;
  }
  
#ifdef CONVERTER_LEDS
  converter.state.kb_ready = (keyboard_state >= INITIALISED) ? 1 : 0;
  update_converter_status();
#endif
}

/**
 * @brief Interrupt Service Routine for Apple M0110 keyboard data reception
 * 
 * This ISR is triggered when the PIO state machine receives a complete 8-bit
 * response from the keyboard. The PIO automatically handles:
 * - Clock synchronization (keyboard-controlled timing)
 * - Bit reception on rising clock edges  
 * - MSB-first bit ordering and assembly
 * - Automatic FIFO push when 8 bits received
 * 
 * The ISR performs minimal processing to reduce interrupt latency:
 * - Reads single byte from PIO RX FIFO
 * - Delegates to event processor for state-dependent handling
 * - Updates timing for timeout detection
 * 
 * @note This function executes in interrupt context - keep processing minimal
 * @note PIO FIFO depth provides buffering for multiple rapid key events
 */
static void __isr keyboard_input_event_handler(void) {
  // Extract received byte from PIO RX FIFO (MSB-aligned 8-bit data)
  uint8_t data_byte = (uint8_t)keyboard_pio->rxf[keyboard_sm];

  // Route response to appropriate handler based on protocol state
  // We don't have any parity or framing errors to check for in M0110
  keyboard_event_processor(data_byte);
}

/**
 * Drive the Apple M0110 keyboard protocol state machine, handling timing, retries, and delivery of scancodes to the HID layer.
 *
 * Call this from the main loop at regular intervals (typically every few milliseconds). It performs:
 * - UNINITIALISED: waits the initial startup delay, then transitions to model-identification.
 * - INIT_MODEL_REQUEST: sends the model-number command at fixed intervals and retries up to five times before restarting detection.
 * - INITIALISED: monitors for response timeouts (triggers full reset on timeout) and forwards buffered scancodes to the USB HID layer when available.
 *
 * Reception of keyboard bytes is performed in interrupt context; this function manages high-level state, timeouts, retry limits, and ring-buffer consumption.
 */
void keyboard_interface_task(void) {
  uint32_t current_time = board_millis();
  
  // Calculate elapsed time once for all timing checks (handles timer wraparound)
  uint32_t elapsed_since_command = current_time - last_command_time;
  uint32_t elapsed_since_response = current_time - last_response_time;
  bool command_elapsed_valid = elapsed_since_command < (UINT32_MAX / 2);
  bool response_elapsed_valid = elapsed_since_response < (UINT32_MAX / 2);
  
  switch (keyboard_state) {
    case UNINITIALISED:
      // Wait for keyboard power-up and internal initialization
      if (command_elapsed_valid && elapsed_since_command > M0110_INITIALIZATION_DELAY_MS) {
        printf("[INFO] Attempting to determine which M0110 keyboard model is connected...\n");
        keyboard_state = INIT_MODEL_REQUEST;
        keyboard_command_handler(M0110_CMD_MODEL);
        last_command_time = current_time;
        model_retry_count = 0;
      }
      break;
      
    case INIT_MODEL_REQUEST:
      // Model identification phase with automatic retry logic
      // Apple spec requires 500ms intervals between retry attempts
      if (command_elapsed_valid && elapsed_since_command > M0110_MODEL_RETRY_INTERVAL_MS) {
        if (model_retry_count < 5) {
          printf("[DBG] Keyboard detected, retrying Model Number command (%d/5)\n", model_retry_count + 1);
          keyboard_command_handler(M0110_CMD_MODEL);
          last_command_time = current_time;
          model_retry_count++;
        } else {
          // Maximum retries exceeded - keyboard may be disconnected or incompatible
          printf("[ERR] Apple M0110 keyboard not responding to Model Number command after 5 attempts\n");
          printf("[DBG] Restarting detection sequence\n");
          keyboard_state = UNINITIALISED;
          last_command_time = current_time;
        }
      }
      break;
      
    case INITIALISED:
      // Check for keyboard timeout first - if no response in 500ms, assume keyboard failure
      // The keyboard should ALWAYS respond, so timeout indicates a problem
      if (response_elapsed_valid && elapsed_since_response > M0110_RESPONSE_TIMEOUT_MS) {
        printf("[WARN] No response from keyboard within 1/2 second - keyboard not behaving, reinitializing\n");
        ringbuf_reset();  // Clear any stale buffered data
        keyboard_state = UNINITIALISED;
        last_command_time = current_time;
        break;  // Skip processing, restart initialization
      }
      
      // Process buffered key data (only reached if keyboard is responding normally)
      if (!ringbuf_is_empty() && tud_hid_ready()) {
        uint8_t scancode = ringbuf_get();
        printf("[DBG] Processing scancode: 0x%02X\n", scancode);
        process_scancode(scancode);
      }
      break;

    default:
      break;
  }
}

/**
 * Initialize hardware and software needed to communicate with an Apple M0110 keyboard.
 *
 * Configures PIO, state machine, pins, interrupts, and internal buffers so the driver can
 * send commands and receive responses from the keyboard. After this call the interface
 * is prepared to be driven by periodic calls to keyboard_interface_task().
 *
 * @param data_pin GPIO pin number connected to the keyboard DATA line. CLOCK is expected
 *                 to be the next consecutive pin (data_pin + 1).
 * @note Call once during system initialization.
 * @see keyboard_interface_task()
 */
void keyboard_interface_setup(uint data_pin) {
#ifdef CONVERTER_LEDS
  // Initialize LED status indicators (if supported by hardware)
  converter.state.kb_ready = 0;
  update_converter_status();
#endif

  // Initialize ring buffer for key data communication between IRQ and main task
  ringbuf_reset();
  
  // Find and allocate available PIO instance for M0110 protocol handling
  keyboard_pio = find_available_pio(&keyboard_interface_program);
  if (keyboard_pio == NULL) {
    printf("[ERR] No PIO available for Apple M0110 Interface Program\n");
    return;
  }
  
  // Claim PIO resources for exclusive use by M0110 protocol
  int sm = pio_claim_unused_sm(keyboard_pio, false);  // Don't panic on failure
  if (sm < 0) {
    printf("[ERR] No PIO state machine available for Apple M0110 Interface\n");
    return;
  }
  keyboard_sm = (unsigned int)sm;
  keyboard_offset = (unsigned int)pio_add_program(keyboard_pio, &keyboard_interface_program);
  keyboard_data_pin = data_pin;
  
  // Configure interrupt handling for PIO data reception
  uint pio_irq = keyboard_pio == pio0 ? PIO0_IRQ_0 : PIO1_IRQ_0;
  
  // Calculate PIO clock divider for M0110 timing requirements
  // Apple M0110 timing specifications:
  // - KEYBOARD→HOST: 330µs cycles (160µs low, 170µs high) ≈ 3.03kHz
  // - HOST→KEYBOARD: 400µs cycles (180µs low, 220µs high) ≈ 2.5kHz  
  // - Request-to-send: 840µs DATA low period to initiate host transmission
  // - Data hold time: 80µs minimum after final clock edge
  // 
  // Using 160µs as base timing ensures reliable detection of fastest keyboard signals
  float clock_div = calculate_clock_divider(M0110_TIMING_KEYBOARD_LOW_US);

  // Initialize PIO state machine with calculated timing and pin configuration
  keyboard_interface_program_init(keyboard_pio, keyboard_sm, keyboard_offset, 
                                     data_pin, clock_div);
  
  // Install and enable interrupt handler for keyboard data reception
  irq_set_exclusive_handler(pio_irq, &keyboard_input_event_handler);
  irq_set_enabled(pio_irq, true);
  irq_set_priority(pio_irq, 0);
  
  printf("[INFO] PIO%d SM%u Apple M0110 Interface program loaded at offset %u with clock divider of %.2f\n",
         (keyboard_pio == pio0 ? 0 : 1), keyboard_sm, keyboard_offset, clock_div);
}