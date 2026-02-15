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

#include "hardware/clocks.h"
#include "keyboard_interface.pio.h"

#include "bsp/board.h"

#include "led_helper.h"
#include "log.h"
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
 * @brief Returns a human-readable description for an M0110 model response code
 * 
 * Model response format (Apple spec): Bits 1-3=keyboard model, Bits 4-6=next device, Bit 7=chained device
 * This function is called only during initialization, so performance is not a concern.
 * 
 * @param model_byte Model identification byte from keyboard
 * @return Pointer to static string describing the keyboard model, or NULL if unrecognized
 */
static const char* get_model_description(uint8_t model_byte) {
  switch (model_byte) {
    case M0110_RESP_MODEL_M0110:           return "M0110 (GS536) - original compact keyboard";
    case M0110_RESP_MODEL_M0110_ALT:       return "M0110 (GS624) - original compact keyboard variant";
    case M0110_RESP_MODEL_M0110A:          return "M0110A - enhanced keyboard with arrow keys";
    case M0110_RESP_MODEL_M0120:           return "M0120 - numeric keypad only";
    case M0110_RESP_MODEL_M0110_M0120:     return "M0110 (GS536) + M0120 keypad detected";
    case M0110_RESP_MODEL_M0110_M0120_ALT: return "M0110 (GS624) + M0120 keypad detected";
    case M0110_RESP_MODEL_M0110A_M0120:    return "M0110A + M0120 keypad detected";
    default:                               return NULL;
  }
}

/**
 * @brief Sends a command byte to the Apple M0110 keyboard (internal function)
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
 * @note If the TX FIFO is full, the command is dropped and logged.
 * @note This is an internal function - not exposed in keyboard_interface.h
 */
static void keyboard_command_handler(uint8_t command) {
  if (pio_sm_is_tx_fifo_full(keyboard_pio, keyboard_sm)) {
    LOG_ERROR("M0110 TX FIFO full, command 0x%02X dropped\n", command);
    return;
  }

  // LOG_DEBUG("M0110 sending command: 0x%02X\n", command);
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
 * - INIT_MODEL_REQUEST: Process model ID and advance to INITIALISED state
 * - INITIALISED: Handle key data (store in ring buffer) or NULL responses
 * 
 * Timing Considerations:
 * - Updates response timestamp used by main task for optimized timeout detection
 * - Response timing enables wraparound-safe timeout calculations in main loop
 * - Key data buffered for main task processing when USB HID interface ready
 * 
 * @param data_byte 8-bit response received from keyboard (MSB-first)
 */
static void keyboard_event_processor(uint8_t data_byte) {
  // LOG_DEBUG("M0110 received: 0x%02X\n", data_byte);
  last_response_time = board_millis(); // Record response time for timeout management
  __dmb();  // Memory barrier - ensure volatile write completes
  
  switch (keyboard_state) {
    case UNINITIALISED:
      // Should not receive data in UNINITIALISED state - ignore
      LOG_ERROR("M0110 received data in UNINITIALISED state: 0x%02X\n", data_byte);
      break;
    case INIT_MODEL_REQUEST:
      // Handle model number response during initialization phase
      {
        const char* model_desc = get_model_description(data_byte);
        if (model_desc) {
          LOG_INFO("Apple M0110 Keyboard Model: %s, reset and ready\n", model_desc);
        } else {
          LOG_DEBUG("Unknown model response: 0x%02X - proceeding with initialization\n", data_byte);
        }
        keyboard_state = INITIALISED;
        keyboard_command_handler(M0110_CMD_INQUIRY);
        last_command_time = board_millis();
      }
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
            LOG_ERROR("Ring buffer full! Scancode 0x%02X lost\n", data_byte);
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
  
  // M0110 uses >= comparison because INITIALISED is not the final state
  update_keyboard_ready_led(keyboard_state >= INITIALISED);
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
 * @brief Main task function for Apple M0110 keyboard protocol management
 * 
 * This function implements the main state machine for M0110 protocol operation.
 * It should be called regularly from the main application loop (typically every few ms).
 * 
 * State Machine Operation:
 * 
 * 1. UNINITIALISED (Startup Phase):
 *    - Waits for initial 1000ms delay to allow keyboard power-up
 *    - Uses command timing to track startup delay
 *    - Transitions to model identification phase when delay expires
 * 
 * 2. INIT_MODEL_REQUEST (Keyboard Detection):
 *    - Sends Model Number commands (0x16) every 500ms using command timing
 *    - Retries up to 5 times if no response received
 *    - Model command causes keyboard to reset and identify itself
 *    - Transitions to INITIALISED phase on successful response (handled by IRQ)
 * 
 * 3. INITIALISED (Normal Operation):
 *    - Monitors keyboard responses using response timing for 500ms timeout
 *    - Processes buffered key data when USB HID interface ready
 *    - Timeout detection triggers return to UNINITIALISED for recovery
 *    - Key data processing occurs via ring buffer from interrupt handler
 * 
 * Timing and Performance:
 * - Optimized timing calculations with single elapsed time computation
 * - Timer wraparound protection prevents false timeouts on system rollover
 * 
 * Error Recovery:
 * - Timeout detection triggers full protocol restart
 * - State machine reset returns to UNINITIALISED phase
 * - Maximum retry limits prevent infinite retry loops
 * 
 * @note This function manages protocol timing and state transitions
 * @note Actual data reception occurs in interrupt context
 * @note Timer calculations are optimized to handle uint32_t wraparound scenarios
 */
void keyboard_interface_task(void) {
  uint32_t current_time = board_millis();
  __dmb();  // Memory barrier - ensure we see latest volatile values from IRQ
  
  // Calculate elapsed time once for all timing checks (handles timer wraparound)
  uint32_t elapsed_since_command = current_time - last_command_time;
  uint32_t elapsed_since_response = current_time - last_response_time;
  bool command_elapsed_valid = elapsed_since_command < (UINT32_MAX / 2);
  bool response_elapsed_valid = elapsed_since_response < (UINT32_MAX / 2);
  
  switch (keyboard_state) {
    case UNINITIALISED:
      // Wait for keyboard power-up and internal initialization
      if (command_elapsed_valid && elapsed_since_command > M0110_INITIALIZATION_DELAY_MS) {
        LOG_INFO("Attempting to determine which M0110 keyboard model is connected...\n");
        keyboard_state = INIT_MODEL_REQUEST;
        keyboard_command_handler(M0110_CMD_MODEL);
        last_command_time = current_time;
        model_retry_count = 0;  // LINT:ALLOW barrier - read-side protected at line 310
      }
      break;
      
    case INIT_MODEL_REQUEST:
      // Model identification phase with automatic retry logic
      // Apple spec requires 500ms intervals between retry attempts
      if (command_elapsed_valid && elapsed_since_command > M0110_MODEL_RETRY_INTERVAL_MS) {
        if (model_retry_count < 5) {
          LOG_DEBUG("Keyboard detected, retrying Model Number command (%d/5)\n", model_retry_count + 1);
          keyboard_command_handler(M0110_CMD_MODEL);
          last_command_time = current_time;
          model_retry_count++;
        } else {
          // Maximum retries exceeded - keyboard may be disconnected or incompatible
          LOG_ERROR("Apple M0110 keyboard not responding to Model Number command after 5 attempts\n");
          LOG_DEBUG("Restarting detection sequence\n");
          keyboard_state = UNINITIALISED;
          last_command_time = current_time;
        }
      }
      break;
      
    case INITIALISED:
      // Check for keyboard timeout first - if no response in 500ms, assume keyboard failure
      // The keyboard should ALWAYS respond, so timeout indicates a problem
      if (response_elapsed_valid && elapsed_since_response > M0110_RESPONSE_TIMEOUT_MS) {
        LOG_ERROR("No response from keyboard within 1/2 second - keyboard not behaving, reinitializing\n");
        ringbuf_reset();  // LINT:ALLOW ringbuf_reset - Clear any stale buffered data during reinitialization
        keyboard_state = UNINITIALISED;
        last_command_time = current_time;
        break;  // Skip processing, restart initialization
      }
      
      // Process buffered key data (only reached if keyboard is responding normally)
      if (!ringbuf_is_empty() && tud_hid_ready()) {
        uint8_t scancode = ringbuf_get();
        LOG_DEBUG("Processing scancode: 0x%02X\n", scancode);
        process_scancode(scancode);
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
  ringbuf_reset();  // LINT:ALLOW ringbuf_reset - Safe: IRQs not yet enabled during init
  
  // Find available PIO block
  keyboard_pio = find_available_pio(&keyboard_interface_program);
  if (keyboard_pio == NULL) {
    LOG_ERROR("Apple M0110: No PIO available for keyboard interface\n");
    return;
  }
  
  // Claim unused state machine (don't panic on failure)
  int sm = pio_claim_unused_sm(keyboard_pio, false);
  if (sm < 0) {
    LOG_ERROR("Apple M0110: No PIO state machine available\n");
    return;
  }
  keyboard_sm = (unsigned int)sm;
  
  // Load program into PIO instruction memory
  keyboard_offset = (unsigned int)pio_add_program(keyboard_pio, &keyboard_interface_program);
  
  // Store pin assignment
  keyboard_data_pin = data_pin;
  
  // Calculate clock divider for M0110 timing requirements
  // KEYBOARD→HOST: 330µs cycles (160µs low, 170µs high)
  // HOST→KEYBOARD: 400µs cycles (180µs low, 220µs high)
  // Using 160µs base timing ensures reliable detection of fastest keyboard signals
  float clock_div = calculate_clock_divider(M0110_TIMING_KEYBOARD_LOW_US);
  
  // Initialize PIO program with calculated clock divider
  keyboard_interface_program_init(keyboard_pio, keyboard_sm, keyboard_offset, 
                                     data_pin, clock_div);
  
  // Configure interrupt handling for PIO data reception
  uint pio_irq = keyboard_pio == pio0 ? PIO0_IRQ_0 : PIO1_IRQ_0;
  
  // Register interrupt handler for RX FIFO events
  irq_set_exclusive_handler(pio_irq, &keyboard_input_event_handler);
  
  // Set highest interrupt priority for time-critical keyboard protocol timing
  irq_set_priority(pio_irq, 0x00);
  
  // Enable the IRQ
  irq_set_enabled(pio_irq, true);
  
  LOG_INFO("PIO%d SM%u Apple M0110 Interface program loaded at offset %u with clock divider of %.2f\n",
         (keyboard_pio == pio0 ? 0 : 1), keyboard_sm, keyboard_offset, clock_div);
}