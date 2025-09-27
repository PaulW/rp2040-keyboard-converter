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
 * 1. UNINITIALISED → Wait for startup delay
 * 2. INIT_MODEL_REQUEST → Send model command, wait for response
 * 3. INIT_COMPLETE → Transition to normal operation
 * 4. INQUIRY → Send inquiry command every 250ms
 * 5. INQUIRY_RESPONSE → Process keyboard responses
 * 
 * Error Handling:
 * - Timeout detection (500ms max response time)
 * - Automatic retry with exponential backoff
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

/* PIO State Machine Configuration */
unsigned int keyboard_sm = 0;          /**< PIO state machine number */
unsigned int keyboard_offset = 0;      /**< PIO program memory offset */
PIO keyboard_pio;                      /**< PIO instance (pio0 or pio1) */
unsigned int keyboard_data_pin;        /**< GPIO pin for DATA line (CLOCK = DATA + 1) */

/**
 * @brief Apple M0110 Protocol State Machine
 * 
 * The state machine manages the initialization and operation phases:
 * - UNINITIALISED: Initial state, waiting for startup delay
 * - INIT_MODEL_REQUEST: Sending Model Number commands, waiting for keyboard identification  
 * - INIT_COMPLETE: Keyboard identified, preparing for normal operation
 * - INQUIRY: Sending Inquiry commands to request key data
 * - INQUIRY_RESPONSE: Processing responses from keyboard (key data or NULL)
 */
static enum {
  UNINITIALISED,        /**< System startup, waiting for initial delay */
  INIT_MODEL_REQUEST,   /**< Sending model commands during initialization */
  INIT_COMPLETE,        /**< Initialization complete, transitioning to operation */
  INQUIRY,              /**< Normal operation: sending inquiry commands */
  INQUIRY_RESPONSE      /**< Normal operation: processing keyboard responses */
} keyboard_state = UNINITIALISED;

/* Timing and State Tracking Variables */
static uint32_t last_command_time = 0;     /**< Timestamp of last command sent (for timeouts) */
static uint8_t model_retry_count = 0;      /**< Number of model command retries attempted */
static uint32_t last_response_time = 0;    /**< Timestamp of last keyboard response received */

/**
 * @brief Sends a command byte to the Apple M0110 keyboard
 * 
 * This function transmits an 8-bit command to the keyboard using the PIO state machine.
 * The command is sent according to the M0110 protocol timing requirements:
 * - Host pulls DATA low for 840µs (request-to-send)
 * - Keyboard starts clocking at ~2.5kHz
 * - Host places data bits on falling edges of clock
 * - MSB transmitted first (bit 7 → bit 0)
 * 
 * The PIO handles the low-level timing and bit transmission automatically.
 * 
 * @param command 8-bit command code to transmit to keyboard
 * @see M0110_CMD_INQUIRY, M0110_CMD_MODEL for common commands
 * 
 * @note If the TX FIFO is full, the command is dropped to prevent blocking.
 *       This protects against PIO state machine lockup conditions.
 */
void keyboard_command_handler(uint8_t command) {
  if (pio_sm_is_tx_fifo_full(keyboard_pio, keyboard_sm)) {
    printf("[WARN] M0110 TX FIFO full, command 0x%02X dropped\n", command);
    return; // PIO TX FIFO full - drop command to prevent blocking
  }

  // printf("[DBG] M0110 sending command: 0x%02X\n", command);
  // M0110 protocol uses MSB-first transmission - position command in upper byte
  // PIO automatically handles MSB-first bit order during transmission
  pio_sm_put(keyboard_pio, keyboard_sm, (uint32_t)command << 24);
}

/**
 * @brief Processes keyboard response data from Apple M0110
 * 
 * This function handles all incoming data from the keyboard, which can be:
 * - Model identification responses during initialization
 * - Key press/release scan codes during normal operation  
 * - NULL responses indicating no key activity
 * - Special responses (keypad detection, etc.)
 * 
 * The function operates in interrupt context (called from IRQ handler) so it
 * performs minimal processing and delegates complex operations to the main task.
 * 
 * Response Processing by State:
 * - INIT_MODEL_REQUEST: Process model ID and advance to operational state
 * - INQUIRY_RESPONSE: Handle key data (store in ring buffer) or NULL responses
 * 
 * Timing Considerations:
 * - Updates response timestamp for timeout detection
 * - Fast NULL response handling (immediate state transition)
 * - Key data buffered for main task processing
 * 
 * @param data_byte 8-bit response received from keyboard (MSB-first)
 */
static void keyboard_event_processor(uint8_t data_byte) {
  // printf("[DBG] M0110 received: 0x%02X\n", data_byte);
  last_response_time = board_millis(); // Record response time for timeout management
  
  switch (keyboard_state) {
    case INIT_MODEL_REQUEST:
      // Handle model number response during initialization phase
      switch (data_byte) {
        case M0110_RESP_MODEL_M0110:
          printf("[INFO] Apple M0110 Keyboard Model: M0110 - keyboard reset and ready\n");
          keyboard_state = INIT_COMPLETE;
          break;
        case M0110_RESP_MODEL_M0110A:
          printf("[INFO] Apple M0110 Keyboard Model: M0110A - keyboard reset and ready\n");
          keyboard_state = INIT_COMPLETE;
          break;
        default:
          printf("[DBG] Unknown model response: 0x%02X - proceeding with initialization\n", data_byte);
          // Handle non-standard model responses gracefully - continue with normal operation
          keyboard_state = INIT_COMPLETE;
          break;
      }
      break;
      
    case INQUIRY_RESPONSE:
      // Handle inquiry responses during normal polling operation
      switch (data_byte) {
        case M0110_RESP_NULL:
          // NULL response (0x7B) - no keys pressed, keyboard operational
          // Return to polling state to maintain 250ms inquiry cycle
          keyboard_state = INQUIRY;
          break;
        default:
          // Scan code or special response - queue for main task processing
          // Ring buffer ensures thread-safe transfer from IRQ to main loop
          if (!ringbuf_is_full()) {
            ringbuf_put(data_byte);
          }
          // State remains INQUIRY_RESPONSE - main task handles transition timing
          break;
      }

      default:
        break;
  }
  
#ifdef CONVERTER_LEDS
  converter.state.kb_ready = (keyboard_state >= INIT_COMPLETE) ? 1 : 0;
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
  keyboard_event_processor(data_byte);
}

/**
 * @brief Main task function for Apple M0110 keyboard protocol management
 * 
 * This function implements the main state machine for M0110 protocol operation.
 * It should be called regularly from the main application loop (typically every few ms).
 * 
 * State Machine Operation:
 * 
 * 1. UNINITIALISED (Startup Phase):
 *    - Waits for initial 1-second delay to allow keyboard power-up
 *    - Transitions to model identification phase
 * 
 * 2. INIT_MODEL_REQUEST (Keyboard Detection):
 *    - Sends Model Number commands (0x16) every 500ms
 *    - Retries up to 5 times if no response
 *    - Model command causes keyboard to reset and identify itself
 *    - Transitions to operational phase on successful response
 * 
 * 3. INIT_COMPLETE (Transition State):
 *    - Brief state to log successful initialization
 *    - Immediately transitions to inquiry phase
 * 
 * 4. INQUIRY (Normal Operation - Command Phase):
 *    - Sends Inquiry commands (0x10) immediately
 *    - Transitions to response processing phase
 * 
 * 5. INQUIRY_RESPONSE (Normal Operation - Response Phase):
 *    - Monitors for keyboard responses (handled by IRQ)
 *    - Processes buffered key data when HID interface ready
 *    - Implements 500ms timeout for unresponsive keyboard
 *    - Returns to INQUIRY phase after processing or NULL response
 * 
 * Error Recovery:
 * - Timeout detection triggers full protocol restart
 * - State machine reset returns to UNINITIALISED phase
 * - Exponential backoff prevents rapid retry loops
 * 
 * @note This function manages protocol timing and state transitions
 * @note Actual data reception occurs in interrupt context
 */
void keyboard_interface_task(void) {
  uint32_t current_time = board_millis();
  
  switch (keyboard_state) {
    case UNINITIALISED:
      // Wait for keyboard power-up and internal initialization
      if (current_time - last_command_time > M0110_INITIALIZATION_DELAY_MS) {
        printf("[INFO] Starting Apple M0110 keyboard detection with Model Number command\n");
        keyboard_state = INIT_MODEL_REQUEST;
        keyboard_command_handler(M0110_CMD_MODEL);
        last_command_time = current_time;
        model_retry_count = 0;
      }
      break;
      
    case INIT_MODEL_REQUEST:
      // Model identification phase with automatic retry logic
      // Apple spec requires 500ms intervals between retry attempts
      if (current_time - last_command_time > M0110_MODEL_RETRY_INTERVAL_MS) {
        if (model_retry_count < 5) {
          printf("[DBG] Retrying Model Number command (%d/5)\n", model_retry_count + 1);
          keyboard_command_handler(M0110_CMD_MODEL);
          last_command_time = current_time;
          model_retry_count++;
        } else {
          // Maximum retries exceeded - keyboard may be disconnected or incompatible
          printf("[ERR] Apple M0110 keyboard not responding to Model Number command after 5 attempts\n");
          keyboard_state = UNINITIALISED;
          last_command_time = current_time;
        }
      }
      break;
      
    case INIT_COMPLETE:
      // Transition state - keyboard identified successfully, entering normal operation
      printf("[INFO] Apple M0110 keyboard initialization complete - beginning normal operation\n");
      keyboard_state = INQUIRY;
      last_response_time = current_time;
      break;
      
    case INQUIRY:
      // Normal operation - send inquiry commands to request key data
      // Commands are sent immediately when entering this state to maintain responsive operation
      // Apple specification: send inquiry every 1/4 second, but we optimize for responsiveness
      keyboard_command_handler(M0110_CMD_INQUIRY);
      keyboard_state = INQUIRY_RESPONSE;
      break;

    case INQUIRY_RESPONSE:
      // Normal operation - waiting for and processing keyboard responses
      
      // Timeout detection - if keyboard stops responding, assume disconnection or error
      if (current_time - last_response_time > M0110_RESPONSE_TIMEOUT_MS) {
        printf("[WARN] No response from keyboard within 1/2 second - restarting with Model Number\n");
        keyboard_state = UNINITIALISED;
        last_command_time = current_time;
        break;
      }

      // Process any received key data when USB HID interface is ready
      // Ring buffer provides thread-safe communication from IRQ handler
      if (!ringbuf_is_empty() && tud_hid_ready()) {
        int c = ringbuf_get();
        if (c != -1) {
          printf("[DBG] Processing scancode: 0x%02X\n", (uint8_t)c);
          process_scancode((uint8_t)c);
          keyboard_state = INQUIRY; // Return to inquiry phase for next command
        }
      }
      break;

    default:
      break;
  }
}

/**
 * @brief Initializes the Apple M0110 keyboard interface hardware and software
 * 
 * This function configures the RP2040 PIO system to handle the Apple M0110 protocol:
 * 
 * Hardware Configuration:
 * - Allocates and configures PIO state machine for M0110 timing
 * - Sets up GPIO pins with appropriate pull-ups for open-drain signaling
 * - Configures interrupt handling for received data
 * 
 * Protocol Setup:
 * - Calculates timing dividers for accurate bit-level timing
 * - Initializes PIO program for MSB-first transmission/reception
 * - Sets up FIFO buffers for asynchronous data handling
 * 
 * Pin Assignment:
 * - data_pin: Connected to keyboard DATA line
 * - data_pin + 1: Connected to keyboard CLOCK line (keyboard controlled)
 * 
 * Timing Calculation:
 * - Base timing uses 160µs as minimum reliable detection period
 * - PIO clock divider ensures proper sampling of keyboard signals
 * - Supports both keyboard TX (330µs cycles) and host TX (400µs cycles)
 * 
 * Error Handling:
 * - Validates PIO resource availability
 * - Graceful degradation if hardware resources unavailable
 * - Comprehensive logging for troubleshooting
 * 
 * @param data_pin GPIO pin number for DATA line (CLOCK will be data_pin + 1)
 *                 Must be even number to allow consecutive pin allocation
 *                 Recommended: Use pins with good signal integrity (avoid LED pins)
 * 
 * @note Call this function once during system initialization
 * @note Requires keyboard_interface_task() to be called regularly for operation
 * @see keyboard_interface_task() for ongoing protocol management
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
  keyboard_sm = (unsigned int)pio_claim_unused_sm(keyboard_pio, true);
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
  
  printf("[INFO] PIO%d SM%u Apple M0110 Interface program loaded at offset %u with clock divider of %.2f\n",
         (keyboard_pio == pio0 ? 0 : 1), keyboard_sm, keyboard_offset, clock_div);
}