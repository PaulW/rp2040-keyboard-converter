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
 * @brief Commodore Amiga Keyboard Protocol Implementation
 * 
 * This implementation handles the Commodore Amiga keyboard protocol used by all
 * Amiga models (A1000, A500, A2000, A3000, A4000). This protocol features:
 * 
 * Physical Interface:
 * - Two-wire bidirectional communication
 * - KCLK: Keyboard clock (unidirectional, keyboard-driven only)
 * - KDAT: Data line (bidirectional: keyboard transmits, computer acknowledges)
 * - Open-collector design with pull-up resistors in both keyboard and computer
 * - 5V TTL logic levels
 * - 4-pin connector: KCLK, KDAT, +5V, GND
 * 
 * Communication Protocol:
 * - Synchronous serial: 8-bit bytes with keyboard-generated clock
 * - Bidirectional handshake required after every byte
 * - LSB-first after bit rotation (see below)
 * - Active-low logic: HIGH = 0, LOW = 1
 * - Special bit rotation: Transmitted as 6-5-4-3-2-1-0-7 (bit 7 last)
 * - Automatic resynchronization on handshake timeout
 * 
 * Timing Characteristics:
 * - Bit rate: ~17 kbit/sec (keyboard-controlled)
 * - Bit period: ~60µs (20µs setup + 20µs KCLK low + 20µs KCLK high)
 * - Handshake requirement: Computer must pulse KDAT low within 1µs of byte completion
 * - Handshake duration: MUST be 85µs (critical for compatibility with all keyboards)
 * - Timeout: 143ms without handshake triggers keyboard resync mode
 * - PIO sampling: 20µs interval for reliable signal detection
 * 
 * Data Format:
 * - Bit rotation before transmission: [7 6 5 4 3 2 1 0] → [6 5 4 3 2 1 0 7]
 * - Reason: Bit 7 = up/down flag sent last; resync 1-bits create key-up codes (safer)
 * - Key codes: 0x00-0x67 (7-bit values)
 * - Bit 7 flag: 0 = key pressed, 1 = key released
 * - Special codes: 0x78, 0xF9, 0xFA, 0xFC, 0xFD, 0xFE (no up/down interpretation)
 * 
 * Handshake Protocol (CRITICAL):
 * - PIO automatically sends 85µs handshake pulse after receiving 8 bits
 * - Handshake integrated into PIO program (no ISR delay)
 * - Hardware latch in keyboard detects pulses ≥1µs
 * - 85µs duration is MANDATORY for compatibility (not 1µs!)
 * - Missing handshake causes keyboard to enter resync mode
 * - Resync: Keyboard clocks out 1-bits until handshake received
 * 
 * State Machine:
 * - UNINITIALISED: System startup, waiting for first byte
 * - INITIALISED: Normal operation, processing key codes and special codes
 * 
 * Special Features:
 * - Reset warning: Three-key combo (CTRL + both Amiga keys) sends 0x78
 * - Lost sync recovery: Keyboard sends 0xF9, then retransmits failed byte
 * - CAPS LOCK special: Only press events (no release), bit 7 = LED state
 * - Buffer overflow: Keyboard sends 0xFA when internal buffer full
 * - Powerup stream: Keyboard sends 0xFD, keys held, 0xFE on boot
 * - Self-test failure: Keyboard sends 0xFC if self-test fails
 * 
 * Advantages:
 * - PIO-based handshake ensures precise timing without blocking
 * - Automatic resynchronization from communication errors
 * - Sophisticated error recovery without host intervention
 * - Supports advanced features like keyboard-initiated reset
 * 
 * Limitations:
 * - Bit de-rotation required in software
 * - KDAT and KCLK must be adjacent GPIO pins (PIO requirement)
 * - No LED control from computer (except CAPS LOCK via key state)
 * - No typematic rate control or configuration
 */

#include "keyboard_interface.h"

#include <math.h>

#include "bsp/board.h"
#include "config.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "keyboard_interface.pio.h"
#include "led_helper.h"
#include "log.h"
#include "pio_helper.h"
#include "ringbuf.h"
#include "scancode.h"

/* PIO State Machine Configuration */
uint keyboard_sm = 0;              /**< PIO state machine number */
uint keyboard_offset = 0;          /**< PIO program memory offset */
PIO keyboard_pio;                  /**< PIO instance (pio0 or pio1) */
uint keyboard_data_pin;            /**< GPIO pin for KDAT (bidirectional) */
uint keyboard_clock_pin;           /**< GPIO pin for KCLK (input only) */

/**
 * @brief Amiga Protocol State Machine
 * 
 * Amiga keyboards have a simple state machine:
 * - UNINITIALISED: Initial state, waiting for first byte from keyboard
 * - INITIALISED: Normal operation, processing all key codes and special codes
 * 
 * The protocol handles sync (0xFF bytes), powerup stream (0xFD...0xFE), and
 * special codes (0x78, 0xF9, 0xFA, 0xFC) through the event processor, not
 * through separate states.
 */
static enum {
  UNINITIALISED,    /**< Initial state, waiting for first byte from keyboard */
  INITIALISED       /**< Normal operation, processing key events and special codes */
} keyboard_state = UNINITIALISED;

/**
 * @brief Amiga CAPS LOCK key code (without bit 7)
 */
#define AMIGA_CAPSLOCK_KEY 0x62

/**
 * @brief Bit mask for make/break and LED state flags
 * - Used to extract bit 7 from scan codes
 * - For normal keys: bit 7 = 1 indicates key release (break)
 * - For CAPS LOCK: bit 7 = 1 indicates LED is OFF
 */
#define AMIGA_BREAK_BIT_MASK 0x80

/**
 * @brief CAPS LOCK timing state machine
 * 
 * MacOS and some other systems require CAPS LOCK to be held for a short period
 * (typically 100-150ms) before recognizing it. We use a non-blocking state machine
 * to delay the release event.
 * 
 * The hold time is configured globally via CAPS_LOCK_TOGGLE_TIME_MS in config.h,
 * allowing reuse across different keyboard protocols that require CAPS LOCK toggling.
 */
static struct {
  enum {
    CAPS_IDLE,       // No pending CAPS LOCK operation
    CAPS_PRESS_SENT, // Press sent, waiting to send release
  } state;
  uint32_t press_time_ms;  // Time when press was sent
} caps_lock_timing = { .state = CAPS_IDLE, .press_time_ms = 0 };

/**
 * @brief Amiga Keyboard Scan Code Processor
 * 
 * Processes scan codes received from Amiga keyboards using the bidirectional
 * protocol with handshake. Handles special codes and normal key events:
 * 
 * Protocol Characteristics:
 * - Bidirectional communication with mandatory 85µs handshake (done in PIO)
 * - Bit rotation: Received as 6-5-4-3-2-1-0-7, de-rotated to 7-6-5-4-3-2-1-0
 * - Active-low logic: Inverted from standard convention
 * - Special codes for protocol management (0x78, 0xF9, 0xFA, 0xFC, 0xFD, 0xFE)
 * 
 * Initialization Sequence:
 * 1. UNINITIALISED: Waiting for first real byte (0xFD or normal key code)
 * 2. First byte received → INITIALISED
 * 
 * Special Code Handling (processed in INITIALISED state):
 * - 0x78 (RESET WARNING): CTRL + both Amiga keys, system reset in 10 seconds
 * - 0xF9 (LOST SYNC): Previous transmission failed, keyboard will retransmit
 * - 0xFA (BUFFER OVERFLOW): Keyboard buffer full, some keys may be lost
 * - 0xFC (SELFTEST FAIL): Keyboard self-test failed, logged but operation continues
 * - 0xFD (POWERUP START): Beginning of power-up key stream
 * - 0xFE (POWERUP END): End of power-up key stream
 * 
 * Normal Key Processing:
 * - Bit 7 = 0: Key pressed (make code)
 * - Bit 7 = 1: Key released (break code)
 * - CAPS LOCK exception: Only press events, bit 7 = LED state
 * - Key codes 0x00-0x67 (7-bit values)
 * 
 * Data Flow:
 * - Bytes received via PIO (handshake done in PIO), de-rotated, then processed here
 * - Valid key codes queued to ring buffer for HID processing
 * - Special codes handled immediately for protocol management
 * - Ring buffer overflow protection prevents data loss
 * 
 * @param data_byte 8-bit code received from keyboard (after de-rotation)
 * 
 * @note Executes in interrupt context - processing kept minimal
 * @note Handshake already sent by PIO before this function called
 * @note De-rotation happens in ISR before calling this function
 */
static void keyboard_event_processor(uint8_t data_byte) {
  // Transition to INITIALISED on first byte received
  if (keyboard_state == UNINITIALISED) {
    keyboard_state = INITIALISED;
    LOG_INFO("Amiga keyboard initialized\n");
  }
  
  // Check for special codes first (these override normal key processing)
  if (amiga_is_special_code(data_byte)) {
    switch (data_byte) {
      case AMIGA_CODE_POWERUP_START:
        LOG_DEBUG("Amiga: Power-up key stream start (0xFD)\n");
        // Continue normal processing - powerup codes are valid key events
        break;
        
      case AMIGA_CODE_POWERUP_END:
        LOG_DEBUG("Amiga: Power-up key stream end (0xFE)\n");
        // Continue normal processing
        break;
        
      case AMIGA_CODE_SELFTEST_FAIL:
        LOG_ERROR("Amiga: Keyboard self-test FAILED (0xFC)\n");
        // Keyboard will blink CAPS LOCK LED in coded pattern
        // 1 blink = ROM checksum failure
        // 2 blinks = RAM test failure
        // 3 blinks = Watchdog timer failure
        // 4 blinks = Short between row lines
        // Continue operation - keyboard may still be functional
        break;
        
      case AMIGA_CODE_LOST_SYNC:
        LOG_WARN("Amiga: Lost sync (0xF9) - keyboard will retransmit\n");
        // Keyboard will retransmit the failed byte after this
        break;
        
      case AMIGA_CODE_BUFFER_OVERFLOW:
        LOG_WARN("Amiga: Keyboard buffer overflow (0xFA)\n");
        // Some key events may have been lost
        break;
        
      case AMIGA_CODE_RESET_WARNING:
        LOG_WARN("Amiga: RESET WARNING (0x78) - CTRL + both Amiga keys pressed\n");
        LOG_WARN("System reset in 10 seconds unless cancelled\n");
        break;
        
      default:
        LOG_ERROR("Amiga: Unknown special code 0x%02X\n", data_byte);
        break;
    }
    
    // Special codes don't go to ring buffer (they're protocol management)
    return;
  }
  
  // Normal key code processing (INITIALISED state only, but we're always INITIALISED after first byte)
  if (keyboard_state == INITIALISED) {
    // Extract key code (bits 6-0) for CAPS LOCK detection
    uint8_t key_code = data_byte & 0x7F;
    
    // Special handling for CAPS LOCK (0x62/0xE2)
    // Amiga CAPS LOCK only sends on press, bit 7 = LED state
    // We need to generate press+release pair to toggle USB HID CAPS LOCK
    if (key_code == AMIGA_CAPSLOCK_KEY) {
      bool kbd_led_on = (data_byte & AMIGA_BREAK_BIT_MASK) == 0;  // bit 7=0 → LED ON, bit 7=1 → LED OFF
      bool hid_caps_on = lock_leds.keys.capsLock;  // Current USB HID CAPS LOCK state
      
      LOG_DEBUG("Amiga: CAPS LOCK event - kbd LED %s, USB HID %s [raw: 0x%02X]\n",
             kbd_led_on ? "ON" : "OFF",
             hid_caps_on ? "ON" : "OFF",
             data_byte);
      
      // Synchronization logic: Only send toggle if states differ
      // - kbd_led_on == hid_caps_on: Already in sync, ignore
      // - kbd_led_on != hid_caps_on: Out of sync, send toggle to fix
      if (kbd_led_on == hid_caps_on) {
        // States match - keyboard and HID are synchronized
        LOG_DEBUG("Amiga: CAPS LOCK already in sync (both %s) - no action needed\n",
                  kbd_led_on ? "ON" : "OFF");
        // Do nothing - no HID events needed
      } else {
        // States differ - need to toggle HID to synchronize
        LOG_DEBUG("Amiga: CAPS LOCK out of sync - kbd=%s, HID=%s - sending toggle\n",
                  kbd_led_on ? "ON" : "OFF",
                  hid_caps_on ? "ON" : "OFF");
        
        // Queue press event (bit 7=0)
        if (!ringbuf_is_full()) {
          ringbuf_put(AMIGA_CAPSLOCK_KEY);
          LOG_DEBUG("Amiga: CAPS LOCK press queued (0x62)\n");
          
          // Start timing state machine - release will be sent after delay
          caps_lock_timing.state = CAPS_PRESS_SENT;
          caps_lock_timing.press_time_ms = to_ms_since_boot(get_absolute_time());
        }
      }
    } else {
      // Normal key - queue to ring buffer
      if (!ringbuf_is_full()) {
        ringbuf_put(data_byte);
      } else {
        LOG_WARN("Amiga: Ring buffer full, dropping key code 0x%02X\n", data_byte);
      }
    }
  }
  
#ifdef CONVERTER_LEDS
  // Update LED status - this pattern matches all other protocols
  // update_converter_status() is non-blocking and ISR-safe (only sets flags, no I/O)
  converter.state.kb_ready = (keyboard_state == INITIALISED) ? 1 : 0;
  update_converter_status();
#endif
}

/**
 * @brief Amiga Keyboard IRQ Event Handler
 * 
 * Interrupt service routine that processes raw scan code data received from Amiga
 * keyboards via the PIO state machine. The PIO handles both reception and handshake:
 * 
 * Frame Processing:
 * - Reads 8-bit byte from PIO RX FIFO (rotated format: 6-5-4-3-2-1-0-7)
 * - De-rotates bits to standard format: 7-6-5-4-3-2-1-0
 * - No parity or stop bits in Amiga protocol
 * - Direct bit manipulation for optimal interrupt performance
 * 
 * Implementation Note - Direct Register Access:
 * - Uses direct PIO RX FIFO register access (keyboard_pio->rxf[keyboard_sm])
 * - This matches the established pattern across ALL protocols (AT/PS2, XT, Apple M0110)
 * - Single-byte read is intentional - ISR fires once per byte, not burst
 * - Performance-optimized for minimal ISR latency
 * 
 * Handshake Protocol:
 * - PIO automatically sends 85µs handshake pulse after receiving 8 bits
 * - No ISR action required for handshake (handled in hardware by PIO)
 * - This ensures precise timing without software delays
 * 
 * Bit De-Rotation:
 * - Amiga transmits: [6 5 4 3 2 1 0 7]
 * - We reconstruct: [7 6 5 4 3 2 1 0]
 * - Reason: Bit 7 (up/down flag) transmitted last for safer sync recovery
 * - Efficient bit manipulation: (byte & 0x01) << 7 | (byte & 0xFE) >> 1
 * 
 * Data Flow:
 * - Valid frames de-rotated and passed to keyboard event processor
 * - Event processor handles state machine and special codes
 * - Scan codes queued to ring buffer for HID processing
 * 
 * Performance Optimization:
 * - Minimal interrupt processing time (no handshake delay in ISR)
 * - Direct FIFO access without buffering overhead
 * - Efficient bit rotation using shift operations
 * - No complex state tracking in interrupt context
 * 
 * Error Recovery:
 * - Missing handshake: Keyboard enters resync mode (clocks out 1-bits)
 * - After resync: Keyboard sends 0xF9, then retransmits failed byte
 * - Self-test failure: Keyboard sends 0xFC
 * - Automatic recovery handled by protocol
 * 
 * Hardware Integration:
 * - KCLK monitored by PIO (keyboard-generated clock)
 * - KDAT read by PIO for data, driven by PIO for handshake
 * - Open-collector design with pull-ups in both keyboard and computer
 * - No host clock generation or timing control needed
 * 
 * @note ISR function - completes quickly (handshake done by PIO)
 * @note PIO handles all timing-critical operations
 * @note No blocking operations in ISR
 */
static void __isr keyboard_input_event_handler() {
  // Read byte from PIO RX FIFO (8-bit, rotated format)
  // PIO has already sent the handshake automatically
  uint8_t rotated_byte = (uint8_t)(keyboard_pio->rxf[keyboard_sm] & 0xFF);
  
  // Amiga protocol is active-low: HIGH = 0, LOW = 1
  // Invert all bits to get correct logical values
  rotated_byte = ~rotated_byte;
  
  // De-rotate the byte to standard bit order
  // Received: [6 5 4 3 2 1 0 7]
  // Target:   [7 6 5 4 3 2 1 0]
  uint8_t data_byte = amiga_derotate_byte(rotated_byte);
  
  // Process the de-rotated byte through state machine
  keyboard_event_processor(data_byte);
}

/**
 * @brief Setup Amiga keyboard interface
 * 
 * Initializes PIO state machine, GPIO configuration, and interrupt handling
 * for Amiga keyboard protocol:
 * 
 * PIO Configuration:
 * - Finds available PIO block (pio0 or pio1)
 * - Claims unused state machine
 * - Loads keyboard interface program into PIO instruction memory
 * - Configures clock divider for ~20µs timing detection
 * 
 * GPIO Setup:
 * - KDAT (data_pin): Bidirectional, open-collector with pull-up
 * - KCLK (data_pin + 1): Input only, keyboard-driven, with pull-up
 * - Both pins initialized for PIO control
 * 
 * Interrupt Configuration:
 * - Enables RX FIFO not empty interrupt
 * - Registers keyboard_input_event_handler as ISR
 * - IRQ priority set for timely handshake response
 * 
 * @param data_pin GPIO pin for KDAT (bidirectional data line)
 *                 KCLK is automatically assigned to data_pin + 1
 */
void keyboard_interface_setup(uint data_pin) {
  // Store pin assignments - KCLK must be KDAT + 1 (PIO program requirement)
  keyboard_data_pin = data_pin;
  keyboard_clock_pin = data_pin + 1;
  
  // Find available PIO block
  keyboard_pio = find_available_pio(&keyboard_interface_program);
  if (keyboard_pio == NULL) {
    LOG_ERROR("Amiga: No PIO available for keyboard interface\n");
    return;
  }
  
  // Claim unused state machine
  keyboard_sm = (uint)pio_claim_unused_sm(keyboard_pio, true);
  
  // Load program into PIO instruction memory
  keyboard_offset = pio_add_program(keyboard_pio, &keyboard_interface_program);
  
  // Calculate clock divider for 20µs minimum pulse detection
  float clock_div = calculate_clock_divider(AMIGA_TIMING_CLOCK_MIN_US);
  
  // Calculate effective SM clock speed for logging
  float rp_clock_khz = 0.001f * (float)clock_get_hz(clk_sys);
  float effective_clock_khz = rp_clock_khz / clock_div;
  
  LOG_INFO("Amiga: Effective SM Clock Speed: %.2fkHz\n", effective_clock_khz);
  
  // Initialize PIO program with calculated clock divider
  // Note: keyboard_interface_program_init (in .pio file) enables RX FIFO interrupt via:
  //   pio_set_irq0_source_enabled(pio, pis_sm0_rx_fifo_not_empty + sm, true)
  keyboard_interface_program_init(keyboard_pio, keyboard_sm, keyboard_offset,
                                   data_pin, clock_div);
  
  // Register interrupt handler for RX FIFO events
  irq_set_exclusive_handler(
      keyboard_pio == pio0 ? PIO0_IRQ_0 : PIO1_IRQ_0,
      keyboard_input_event_handler);
  
  // Enable the IRQ (PIO RX FIFO source already enabled in program_init above)
  irq_set_enabled(keyboard_pio == pio0 ? PIO0_IRQ_0 : PIO1_IRQ_0, true);
  
  LOG_INFO("PIO%d SM%d Amiga Keyboard Interface loaded at offset %d (clock div %.2f)\n",
           keyboard_pio == pio0 ? 0 : 1, keyboard_sm, keyboard_offset, clock_div);
  
  // Set initial state
  keyboard_state = UNINITIALISED;
  
  LOG_INFO("Amiga keyboard interface initialized, waiting for first byte...\n");
}

/**
 * @brief Main task function for Amiga keyboard interface
 * 
 * This function must be called periodically from the main loop to process
 * scan codes from the ring buffer and handle keyboard state management.
 * 
 * Operation Modes:
 * 
 * UNINITIALISED State:
 * - Waiting for first byte from keyboard (power-up sequence)
 * - Keyboard sends 0xFD (power-up start) after self-test
 * - Transition to INITIALISED happens in ISR on first byte
 * 
 * INITIALISED State (Normal Operation):
 * - Process scan codes from ring buffer when HID interface ready
 * - Coordinates scan code translation for Amiga protocol
 * - Maintains optimal data flow between keyboard and USB HID
 * - Prevents HID report queue overflow through ready checking
 * 
 * Data Processing:
 * - Non-blocking ring buffer processing for real-time performance
 * - HID-ready checking prevents USB report queue overflow
 * - Scan codes processed through dedicated Amiga processor
 * - Handles CAPS LOCK special behavior (press only, LED state tracking)
 * 
 * Protocol Features:
 * - No initialization sequence needed (unlike AT/PS2)
 * - No LED control capability from computer (keyboard maintains state)
 * - Bidirectional handshake handled entirely in PIO hardware
 * - Special codes (0x78, 0xF9-0xFE) filtered by event processor
 * 
 * Error Recovery:
 * - Lost sync recovery automatic (keyboard sends 0xF9, retransmits)
 * - Buffer overflow protection (ring buffer full checks)
 * - Keyboard self-test failures logged (0xFC)
 * - Reset warning detection (0x78)
 * 
 * Performance:
 * - Called from main loop (not time-critical like ISR)
 * - Processing deferred until HID ready (prevents USB congestion)
 * - Efficient scan code processing without protocol overhead
 * - Direct translation to modern HID key codes via scancode processor
 * 
 * Hardware Monitoring:
 * - Converter status LED updates when CONVERTER_LEDS enabled
 * - Keyboard ready state reflects INITIALISED status
 * 
 * @note Must be called regularly from main loop for scan code processing
 * @note Ring buffer filled by ISR, consumed here (producer-consumer pattern)
 * @note HID ready check is critical to prevent USB report queue overflow
 * @note Handshake timing handled entirely in PIO (not here)
 */
void keyboard_interface_task() {
  // CAPS LOCK timing state machine - handle delayed release
  if (caps_lock_timing.state == CAPS_PRESS_SENT) {
    uint32_t now_ms = to_ms_since_boot(get_absolute_time());
    // Note: Unsigned subtraction handles wraparound correctly for time deltas
    // as long as elapsed time < 2^31 ms (~24 days). CAPS LOCK hold time is 125ms.
    if (now_ms - caps_lock_timing.press_time_ms >= CAPS_LOCK_TOGGLE_TIME_MS) {
      // Time to send release event
      if (!ringbuf_is_full()) {
        ringbuf_put(AMIGA_CAPSLOCK_KEY | AMIGA_BREAK_BIT_MASK);  // Release (bit 7=1)
        LOG_DEBUG("Amiga: CAPS LOCK release queued after %ums hold (0xE2)\n", 
                  (unsigned int)CAPS_LOCK_TOGGLE_TIME_MS);
        caps_lock_timing.state = CAPS_IDLE;
      }
      // If buffer full, we'll try again on next iteration
    }
  }
  
  // INITIALISED state: Process scan codes from ring buffer
  if (keyboard_state == INITIALISED) {
    if (!ringbuf_is_empty() && tud_hid_ready()) {
      // Process scan codes only when HID interface ready to prevent report queue overflow
      // HID ready check ensures reliable USB report transmission
      
      uint8_t scancode = ringbuf_get();  // Retrieve next scan code from interrupt-filled buffer
      
      // Process through Amiga scancode processor
      // CAPS LOCK handling already done in ISR (generates press+release pair)
      // All keys here are processed normally
      process_scancode(scancode);
    }
  }
  // UNINITIALISED state: Waiting for keyboard power-up
  // No action needed here - transition happens in ISR on first byte received
  
#ifdef CONVERTER_LEDS
  // Update LED status to reflect keyboard ready state
  converter.state.kb_ready = (keyboard_state == INITIALISED) ? 1 : 0;
  update_converter_status();
#endif
}
