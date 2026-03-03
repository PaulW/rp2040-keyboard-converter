/*
 * This file is part of RP2040 Keyboard Converter.
 *
 * Copyright 2023-2026 Paul Bramhall (paulwamp@gmail.com)
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
 * - Start bit(s) present; no parity or stop bits
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
 * - No initialisation sequence required
 * - Immediate operation on power-up
 * - Compatible with basic keyboard functionality
 *
 * Limitations:
 * - No LED control capability
 * - No typematic rate control
 * - No host-to-keyboard communication
 * - Limited to basic scan code set
 * - No keyboard identification or capabilities detection
 *
 * @see keyboard_interface.h for the public API and protocol constants.
 */

#include "keyboard_interface.h"

#include <stdint.h>

#include "keyboard_interface.pio.h"
#include "pico/stdlib.h"

#include "bsp/board.h"
#include "tusb.h"

#include "flow_tracker.h"
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

// PIO State Machine Configuration
static pio_engine_t pio_engine = {.pio = NULL, .sm = -1, .offset = -1};

static uint keyboard_data_pin; /**< GPIO pin for DATA line (2-wire interface) */

/**
 * @brief IBM XT Protocol State Machine
 *
 * XT keyboards use a very simple state machine since they require no
 * initialisation sequence and begin transmitting immediately on power-up.
 *
 * - UNINITIALISED: System startup, hardware setup complete
 * - INITIALISED: Normal operation, receiving and processing scan codes
 */
typedef enum {
    UNINITIALISED, /**< Initial state, waiting for system startup */
    INITIALISED,   /**< Normal operation, processing scan codes */
} keyboard_state_t;
static keyboard_state_t keyboard_state = UNINITIALISED;

// Keyboard detection timing constants
#define XT_DETECT_INTERVAL_MS 200 /**< Detection status check interval (ms) */
#define XT_DETECT_STALL_LIMIT 5   /**< Max stall count before reset */

// --- Private Functions ---

/**
 * @brief IBM XT Keyboard Scan Code Processor
 *
 * Processes scan codes received from IBM XT keyboards using the simple unidirectional
 * protocol. XT keyboards begin transmitting immediately upon power-up with no complex
 * initialisation sequence required:
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
 * @note IRQ-safe.
 */
static void keyboard_event_processor(uint8_t data_byte) {
    switch (keyboard_state) {
        case UNINITIALISED:
            if (data_byte == XT_RESP_BAT_PASSED) {
                LOG_DEBUG("Keyboard Self-Test Passed\n");
                keyboard_state = INITIALISED;
                __dmb();  // Memory barrier - ensure state write visible to main loop
            } else {
                LOG_ERROR("Keyboard Self-Test Failed: 0x%02X\n", data_byte);
                keyboard_state = UNINITIALISED;
                __dmb();  // Memory barrier - ensure state write visible to main loop
                pio_restart(pio_engine.pio, pio_engine.sm, pio_engine.offset);
            }
            break;
        case INITIALISED:
            // Always queue to ring buffer - processing happens in main task loop
            // This ensures HID reports are sent from the correct context
            if (!ringbuf_is_full()) {
                ringbuf_put(data_byte);
                isr_push_flow_token(__func__, data_byte);
            }
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
 * - PIO initialisation triggers hardware-level keyboard reset
 * - Automatic recovery from communication errors
 *
 * Performance Optimisation:
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
 * @note IRQ-safe.
 */
static void __isr keyboard_input_event_handler(void) {
    // Guard against spurious dispatch from shared IRQ - only process if our FIFO has data
    if (pio_sm_is_rx_fifo_empty(pio_engine.pio, pio_engine.sm)) {
        return;
    }

    io_ro_32 data_cast = pio_engine.pio->rxf[pio_engine.sm] >> 23;
    uint16_t data      = (uint16_t)data_cast;

    // Parse XT frame components from 9-bit PIO data (Start + 8 data bits)
    uint8_t start_bit = data & 0x1;
    uint8_t data_byte = (uint8_t)((data_cast >> 1) & 0xFF);

    if (start_bit != 1) {
        LOG_ERROR("Start Bit Validation Failed: start_bit=%i\n", start_bit);
        keyboard_state = UNINITIALISED;
        __dmb();  // Memory barrier - ensure state write visible to main loop
        pio_restart(pio_engine.pio, pio_engine.sm, pio_engine.offset);
        return;
    }
    keyboard_event_processor(data_byte);
}

// --- Public Functions ---

void keyboard_interface_task(void) {
    static uint8_t detect_stall_count = 0;

    __dmb();  // Memory barrier - ensure we see latest state from IRQ
    if (keyboard_state == INITIALISED) {
        detect_stall_count = 0;  // Clear detection counter - keyboard is operational
        if (!ringbuf_is_empty() && tud_hid_ready()) {
            // Process scan codes only when HID interface ready to prevent report queue overflow
            // HID ready check ensures reliable USB report transmission

            // Historical note: interrupt disabling during ring buffer read was tested
            // but provided no benefit and increased input latency - removed for performance
            uint8_t scancode = ringbuf_get();  // Retrieve next scan code from buffer

            // Begin flow tracking for this scancode byte
            flow_token_t flow_tok;
            if (main_pop_flow_token(&flow_tok)) {
                flow_start(&flow_tok);
            }

#if USING_SET123
            process_scancode_ct(scancode);  // Unified processor with compile-time Set 1
#else
            process_scancode(scancode);  // Legacy per-set processor
#endif
        }
    } else {
        // Keyboard detection and initialisation management
        static uint32_t detect_ms = 0;
        // Alternative detection method (unused):
        // if (gpio_get(keyboard_data_pin) == 1) {
        //   keyboard_state = INITIALISED;
        // }
        if (board_millis() - detect_ms > XT_DETECT_INTERVAL_MS) {
            detect_ms = board_millis();
            // Monitor CLOCK line idle state for keyboard presence. Keyboard also pulls CLOCK HIGH
            // to signify ACK during init. XT protocol has no host-to-keyboard communication, so
            // only presence detection is possible.
            if (gpio_get(keyboard_data_pin + 1) == 1) {
                if (detect_stall_count < XT_DETECT_STALL_LIMIT) {
                    detect_stall_count++;
                    LOG_DEBUG("Keyboard detected, awaiting ACK (%i/%d attempts)\n",
                              detect_stall_count, XT_DETECT_STALL_LIMIT);
                } else {
                    LOG_DEBUG("Keyboard detected, but no ACK received!\n");
                    LOG_DEBUG("Requesting keyboard reset\n");
                    keyboard_state = UNINITIALISED;
                    pio_restart(pio_engine.pio, pio_engine.sm, pio_engine.offset);
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

void keyboard_interface_setup(uint data_pin) {
#ifdef CONVERTER_LEDS
    converter.state.kb_ready = 0;
    update_converter_status();  // Initialise converter status LEDs to "not ready" state
#endif

    // Initialise ring buffer for key data communication between IRQ and main task
    ringbuf_reset();  // LINT:ALLOW ringbuf_reset - Safe: IRQs not yet enabled during init

    // Claim PIO instance and state machine atomically with fallback
    pio_engine = claim_pio_and_sm(&keyboard_interface_program);
    if (pio_engine.pio == NULL) {
        LOG_ERROR("XT: No PIO resources available for keyboard interface\n");
        return;
    }

    // Store pin assignment
    keyboard_data_pin = data_pin;

    // XT timing: 10µs sampling interval for double-start-bit detection
    // IBM XT keyboards send two 40µs start bits, clones send one
    // 4 samples per start bit ensures reliable detection of genuine IBM vs clone keyboards
    float clock_div = calculate_clock_divider(XT_TIMING_SAMPLE_US);

    // Initialise PIO program with calculated clock divider
    keyboard_interface_program_init(pio_engine.pio, pio_engine.sm, pio_engine.offset, data_pin,
                                    clock_div);

    // Initialise shared PIO IRQ dispatcher (safe to call multiple times)
    pio_irq_dispatcher_init(pio_engine.pio);

    // Register keyboard event handler with the dispatcher
    if (!pio_irq_register_callback(&keyboard_input_event_handler)) {
        LOG_ERROR("XT Keyboard: Failed to register IRQ callback\n");
        // Release PIO resources before returning
        release_pio_and_sm(&pio_engine, &keyboard_interface_program);
        return;
    }

    LOG_INFO("PIO%d SM%d Interface program loaded at offset %d with clock divider of %.2f\n",
             (pio_engine.pio == pio0 ? 0 : 1), pio_engine.sm, pio_engine.offset, clock_div);
}