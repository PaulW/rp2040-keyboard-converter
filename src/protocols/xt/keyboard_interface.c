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
 * - 2-wire interface: separate DATA and CLOCK lines
 * - Keyboard generates clock signal on dedicated CLOCK line
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
 * - PIO sampling: 10µs interval for double-start-bit detection
 * - IBM XT sends two start bits, clones send one (differentiation required)
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

#include "hardware/clocks.h"
#include "keyboard_interface.pio.h"

#include "bsp/board.h"

#include "led_helper.h"
#include "log.h"
#include "pio_helper.h"
#include "ringbuf.h"

// Include appropriate scancode header
// Try unified processor first (set123), fall back to legacy
#if __has_include("scancode_config.h")
  #include "scancode.h"
  #include "scancode_config.h"
  #define USING_SET123 1
#else
  #include "scancode.h"
  #define USING_SET123 0
#endif

/* PIO State Machine Configuration */
uint keyboard_sm = 0;              /**< PIO state machine number */
uint keyboard_offset = 0;          /**< PIO program memory offset */
PIO keyboard_pio;                  /**< PIO instance (pio0 or pio1) */
uint keyboard_data_pin;            /**< GPIO pin for DATA line (2-wire interface) */

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
 * @brief IBM XT Keyboard Scan Code Processor
 * 
 * Processes scan codes received from IBM XT keyboards using the simple unidirectional
 * protocol. XT keyboards begin transmitting immediately upon power-up with no complex
 * initialization sequence required:
 * 
 * Protocol Characteristics:
 * - Unidirectional communication (keyboard to host only)
 * - Single-byte scan codes transmitted immediately
 * - LSB-first bit transmission format
 * - No acknowledgment or flow control required
 * - Fixed scan code set (equivalent to later "Set 1")
 * 
 * Scan Code Format:
 * - Make codes: 0x01-0x53 (key press events)
 * - Break codes: Make code + 0x80 (key release events, e.g., 0x81 = key 0x01 released)
 * - Self-test pass: 0xAA (transmitted once on successful power-on)
 * - Self-test fail: Other values (indicates keyboard malfunction)
 * 
 * State Machine Operation:
 * - UNINITIALISED: Waiting for first valid data (typically 0xAA self-test result)
 * - INITIALISED: Normal operation, processing all scan codes
 * - Simple 2-state machine reflects protocol simplicity
 * 
 * Data Processing:
 * - First byte expected to be 0xAA (self-test passed)
 * - Failed self-test triggers state machine reset and PIO restart
 * - Valid scan codes queued to ring buffer for HID processing
 * - No protocol-level error recovery needed
 * 
 * Performance Features:
 * - Minimal interrupt processing overhead
 * - Direct scan code queuing to ring buffer
 * - No complex state tracking or timeout handling
 * - Ring buffer overflow protection
 * 
 * Compatibility:
 * - Compatible with IBM PC/XT keyboards (1981-1987)
 * - Supports clone keyboards following XT protocol
 * - Works with modern XT-compatible keyboards
 * - No special handling for keyboard variants needed
 * 
 * @param data_byte 8-bit scan code received from keyboard (LSB-first transmission)
 * 
 * @note Executes in interrupt context - processing kept minimal for real-time performance
 * @note XT protocol simplicity eliminates need for complex error handling
 * @note Self-test validation ensures keyboard is functional before accepting scan codes
 */
static void keyboard_event_processor(uint8_t data_byte) {
  switch (keyboard_state) {
    case UNINITIALISED:
      if (data_byte == XT_RESP_BAT_PASSED) {
        LOG_DEBUG("Keyboard Self-Test Passed\n");
        keyboard_state = INITIALISED;
      } else {
        LOG_ERROR("Keyboard Self-Test Failed: 0x%02X\n", data_byte);
        keyboard_state = UNINITIALISED;
        pio_restart(keyboard_pio, keyboard_sm, keyboard_offset);
      }
      break;
    case INITIALISED:
      // Always queue to ring buffer - processing happens in main task loop
      // This ensures HID reports are sent from the correct context
      if (!ringbuf_is_full()) ringbuf_put(data_byte);
  }
  update_keyboard_ready_led(keyboard_state == INITIALISED);
}

/**
 * @brief IBM XT Keyboard IRQ Event Handler
 * 
 * Interrupt service routine that processes raw scan code data received from IBM XT
 * keyboards via the PIO state machine. Implements the simple XT protocol with minimal
 * overhead and maximum reliability:
 * 
 * Frame Processing:
 * - Reads 9-bit frame from PIO RX FIFO (Start bit + 8 data bits)
 * - Extracts start bit and reconstructs 8-bit scan code
 * - No parity or stop bits in XT protocol (simplified format)
 * - Direct bit manipulation for optimal interrupt performance
 * 
 * Protocol Validation:
 * - Start bit validation: Must be 1 (mark condition for XT protocol)
 * - Invalid start bit triggers state machine reset and PIO restart
 * - Simple validation reflects XT protocol simplicity
 * - Minimal error checking reduces interrupt processing time
 * 
 * Keyboard Compatibility:
 * - Handles both genuine IBM and clone XT keyboards
 * - PIO code automatically filters double start bits (clone keyboard artifact)
 * - Transparent handling of timing variations between keyboard types
 * - No special detection or configuration needed for different variants
 * 
 * Data Flow:
 * - Valid frames passed directly to keyboard event processor
 * - Invalid frames trigger protocol restart for clean recovery
 * - Scan codes processed in interrupt context for minimal latency
 * - Ring buffer queuing handled at event processor level
 * 
 * Reset Handling:
 * - XT keyboards have no software reset command capability
 * - Reset achieved by restarting PIO state machine
 * - PIO initialization triggers hardware-level keyboard reset
 * - Automatic recovery from communication errors
 * 
 * Performance Optimization:
 * - Minimal interrupt processing time (< 10µs typical)
 * - Direct FIFO access without buffering overhead  
 * - Efficient bit extraction using shift operations
 * - No complex state tracking in interrupt context
 * 
 * Hardware Integration:
 * - Single DATA line monitoring (no separate clock)
 * - Keyboard self-clocks data transmission
 * - No host timing control or flow management needed
 * - PIO handles all signal timing automatically
 * 
 * @note ISR function - must complete quickly to maintain real-time performance
 * @note PIO state machine handles complex timing requirements transparently  
 * @note Start bit filtering for clone keyboards handled in PIO code, not here
 * @note No software reset capability - reset via PIO restart only
 */
static void __isr keyboard_input_event_handler(void) {
  io_ro_32 data_cast = keyboard_pio->rxf[keyboard_sm] >> 23;
  uint16_t data = (uint16_t)data_cast;

  // Parse XT frame components from 9-bit PIO data (Start + 8 data bits)
  uint8_t start_bit = data & 0x1;
  uint8_t data_byte = (uint8_t)((data_cast >> 1) & 0xFF);

  if (start_bit != 1) {
    LOG_ERROR("Start Bit Validation Failed: start_bit=%i\n", start_bit);
    keyboard_state = UNINITIALISED;
    pio_restart(keyboard_pio, keyboard_sm, keyboard_offset);
    return;
  }
  keyboard_event_processor(data_byte);
}

/**
 * @brief IBM XT Keyboard Interface Main Task
 * 
 * Main task function that manages IBM XT keyboard communication and data processing.
 * Designed for the simple XT protocol which requires minimal state management:
 * 
 * Normal Operation (INITIALISED state):
 * - Processes scan codes from ring buffer when HID interface ready
 * - Coordinates scan code translation for IBM XT Set 1 format
 * - Maintains optimal data flow between keyboard and USB HID
 * - Prevents HID report queue overflow through ready checking
 * 
 * Initialization Management (UNINITIALISED state):
 * - Monitors GPIO clock line for keyboard presence detection
 * - Implements simple retry logic for keyboard detection
 * - Handles keyboard connection/disconnection events
 * - Manages PIO restart for clean protocol recovery
 * 
 * Protocol Simplicity:
 * - No complex initialization sequence (unlike AT/PS2)
 * - No LED control capability (hardware limitation)
 * - No bidirectional communication or command/response
 * - Immediate operation once keyboard detected and self-test passed
 * 
 * Detection and Recovery:
 * - 200ms periodic checks for keyboard presence via clock line
 * - 5-attempt detection cycle before PIO restart
 * - Automatic recovery from communication failures
 * - Simple state machine reflects protocol simplicity
 * 
 * Data Processing Features:
 * - Non-blocking ring buffer processing for real-time performance
 * - HID-ready checking prevents USB report queue overflow
 * - Efficient scan code processing without protocol overhead
 * - Direct translation to modern HID key codes
 * 
 * Hardware Monitoring:
 * - GPIO clock line monitoring for presence detection
 * - Converter status LED updates when CONVERTER_LEDS enabled
 * - Simple hardware interface monitoring (single DATA line)
 * - No complex timing or signal management required
 * 
 * Performance Characteristics:
 * - Minimal CPU overhead due to protocol simplicity
 * - Fast response time (no initialization delays)
 * - Efficient ring buffer usage without protocol buffering
 * - Real-time scan code processing capability
 * 
 * Compatibility Notes:
 * - Works with original IBM PC/XT keyboards (1981-1987)
 * - Compatible with modern XT-protocol keyboards
 * - No special handling for keyboard variants needed
 * - Plug-and-play operation (no configuration required)
 * 
 * @note Must be called periodically from main loop (typically 1-10ms intervals)
 * @note Function is non-blocking and maintains real-time responsiveness
 * @note Much simpler than AT/PS2 due to XT protocol limitations and design
 * @note No LED synchronization capability due to XT protocol constraints
 */
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
#if USING_SET123
      process_scancode_ct(scancode);  // Unified processor with compile-time Set 1
#else
      process_scancode(scancode);  // Legacy per-set processor
#endif
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
          LOG_DEBUG("Keyboard detected, awaiting ACK (%i/5 attempts)\n", detect_stall_count);
        } else {
          LOG_DEBUG("Keyboard detected, but no ACK received!\n");
          LOG_DEBUG("Requesting keyboard reset\n");
          keyboard_state = UNINITIALISED;
          pio_restart(keyboard_pio, keyboard_sm, keyboard_offset);
          detect_stall_count = 0;
        }
      } else if (keyboard_state == UNINITIALISED) {
        LOG_DEBUG("Awaiting keyboard detection. Please ensure a keyboard is connected.\n");
        detect_stall_count = 0;
      }
      update_keyboard_ready_led(keyboard_state == INITIALISED);
    }
  }
}

/**
 * @brief IBM XT Keyboard Interface Initialization
 * 
 * Configures and initializes the IBM XT keyboard interface using RP2040 PIO hardware
 * for accurate timing and protocol implementation. Sets up the minimal hardware
 * required for the simple XT unidirectional protocol:
 * 
 * Hardware Setup:
 * - Finds available PIO instance (PIO0 or PIO1) with program space
 * - Claims unused state machine and loads XT protocol program  
 * - Configures GPIO pins for DATA and CLOCK lines (2-wire interface)
 * - Keyboard generates clock signal on dedicated CLOCK line
 * 
 * PIO Configuration:
 * - Calculates clock divider for 30µs minimum pulse timing
 * - Configures state machine for unidirectional receive-only operation
 * - Sets up automatic scan code reception and frame processing
 * - Optimized for ~10 kHz keyboard transmission rate
 * 
 * Interrupt System:
 * - Configures PIO-specific IRQ (PIO0_IRQ_0 or PIO1_IRQ_0)
 * - Installs XT keyboard input event handler for scan code processing
 * - Enables interrupt-driven reception for optimal performance
 * - Links PIO RX FIFO to interrupt processing system
 * 
 * Software Initialization:
 * - Resets ring buffer to ensure clean startup state
 * - Initializes protocol state machine to UNINITIALISED
 * - Resets converter status LEDs when CONVERTER_LEDS enabled
 * - Clears all protocol state variables
 * 
 * Timing Configuration:
 * - Based on IBM PC/XT Technical Reference specifications
 * - 30µs minimum timing for reliable signal detection
 * - Accommodates keyboard clock variations (~40µs low, ~60µs high)
 * - Supports original IBM and compatible clone keyboards
 * 
 * Protocol Simplicity:
 * - No bidirectional communication setup needed
 * - No command/response initialization required
 * - No LED control or advanced feature configuration
 * - Immediate operation once keyboard powers on
 * 
 * Compatibility Features:
 * - Works with original IBM PC/XT keyboards (1981-1987)
 * - Compatible with modern XT-protocol keyboards
 * - Handles timing variations in clone keyboards
 * - No keyboard-specific configuration needed
 * 
 * Error Handling:
 * - Validates PIO resource availability before configuration
 * - Reports initialization failures with detailed error messages
 * - Graceful failure if insufficient PIO resources available
 * - Logs successful initialization with configuration details
 * 
 * @param data_pin GPIO pin number for DATA line (2-wire interface)
 * 
 * @note XT protocol uses 2-wire interface: DATA and CLOCK lines
 * @note Function blocks until hardware initialization complete  
 * @note Much simpler setup than AT/PS2 due to XT protocol design
 * @note Call before any other XT keyboard interface operations
 */
void keyboard_interface_setup(uint data_pin) {
#ifdef CONVERTER_LEDS
  converter.state.kb_ready = 0;
  update_converter_status();  // Initialize converter status LEDs to "not ready" state
#endif

  // Initialize ring buffer for key data communication between IRQ and main task
  ringbuf_reset();  // LINT:ALLOW ringbuf_reset - Safe: IRQs not yet enabled during init
  
  // Find available PIO block
  keyboard_pio = find_available_pio(&keyboard_interface_program);
  if (keyboard_pio == NULL) {
    LOG_ERROR("XT: No PIO available for keyboard interface\n");
    return;
  }
  
  // Claim unused state machine
  keyboard_sm = (uint)pio_claim_unused_sm(keyboard_pio, true);
  
  // Load program into PIO instruction memory
  keyboard_offset = pio_add_program(keyboard_pio, &keyboard_interface_program);
  
  // Store pin assignment
  keyboard_data_pin = data_pin;
  
  // XT timing: 10µs sampling interval for double-start-bit detection
  // IBM XT keyboards send two 40µs start bits, clones send one
  // 4 samples per start bit ensures reliable detection of genuine IBM vs clone keyboards
  float clock_div = calculate_clock_divider(XT_TIMING_SAMPLE_US);
  
  // Initialize PIO program with calculated clock divider
  keyboard_interface_program_init(keyboard_pio, keyboard_sm, keyboard_offset, data_pin, clock_div);
  
  // Configure interrupt handling for PIO data reception
  uint pio_irq = keyboard_pio == pio0 ? PIO0_IRQ_0 : PIO1_IRQ_0;
  
  // Register interrupt handler for RX FIFO events
  irq_set_exclusive_handler(pio_irq, &keyboard_input_event_handler);
  
  // Set highest interrupt priority for time-critical keyboard protocol timing
  irq_set_priority(pio_irq, 0x00);
  
  // Enable the IRQ
  irq_set_enabled(pio_irq, true);

  LOG_INFO("PIO%d SM%d Interface program loaded at offset %d with clock divider of %.2f\n",
         (keyboard_pio == pio0 ? 0 : 1), keyboard_sm, keyboard_offset, clock_div);
}
