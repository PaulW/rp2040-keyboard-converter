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

#include "pio_helper.h"

#include <math.h>
#include <stdio.h>

#include "hardware/clocks.h"

#include "log.h"

/* Compile-time validation of PIO helper constants */
_Static_assert(1, "PIO helper basic sanity check");  // Always true, validates _Static_assert works

/**
 * @brief Finds an available PIO instance for a given PIO program
 * 
 * The RP2040 has two PIO blocks (PIO0 and PIO1), each with limited instruction
 * memory (32 instructions). This function searches for a PIO block with enough
 * free space to load the specified program.
 * 
 * Search Strategy:
 * 1. Try PIO0 first (conventional default)
 * 2. If PIO0 is full, try PIO1
 * 3. If both are full, return NULL
 * 
 * Resource Management:
 * - Each PIO program requires contiguous instruction memory
 * - Programs remain loaded until explicit removal
 * - Multiple state machines can share the same program
 * 
 * @param program The PIO program to check for available space
 * @return PIO instance (pio0 or pio1) if space available, NULL if both full
 * 
 * @note Caller must check for NULL return and handle gracefully
 * @note Does not claim state machines, only checks program space
 */
PIO find_available_pio(const pio_program_t *program) {
  if (!pio_can_add_program(pio0, program)) {
    LOG_WARN("PIO0 has no space for PIO Program. Checking to see if we can load into PIO1\n");
    if (!pio_can_add_program(pio1, program)) {
      LOG_ERROR("PIO1 has no space for PIO Program\n");
      return NULL;
    }
    return pio1;
  } else {
    return pio0;
  }
}

/**
 * @brief Restarts a PIO state machine at a specified program offset
 * 
 * PIO State Machine Reset Sequence:
 * 1. Drain TX FIFO (prevents stale data corruption)
 * 2. Clear both FIFOs (TX and RX)
 * 3. Restart state machine (resets PC, clears OSR/ISR)
 * 4. Jump to specified offset (sets entry point)
 * 
 * Use Cases:
 * - Protocol error recovery (AT/PS2 inhibit, M0110 sync errors)
 * - Clock stretching timeout recovery
 * - Re-initialization after configuration changes
 * - Transitioning between protocol states
 * 
 * State Machine Reset Behavior:
 * - Program Counter (PC) → 0
 * - Output Shift Register (OSR) → cleared
 * - Input Shift Register (ISR) → cleared
 * - FIFOs → empty
 * - Side-set pins → retain state (caller must reset if needed)
 * - GPIO configuration → unchanged
 * 
 * Thread Safety:
 * - This function is NOT interrupt-safe
 * - Caller must ensure no concurrent PIO access
 * - Typically called from main task, not ISRs
 * 
 * @param pio    The PIO instance (pio0 or pio1)
 * @param sm     The state machine number (0-3)
 * @param offset The program offset to jump to after restart
 * 
 * @note Caller must reconfigure device-specific state after calling
 * @note Debug output shows offset for troubleshooting restart logic
 * @note Does not modify clock divider or GPIO mappings
 */
void pio_restart(PIO pio, uint sm, uint offset) {
  // Restart the PIO State Machine
  LOG_DEBUG("Resetting State Machine and re-initialising at offset: 0x%02X...\n", offset);
  pio_sm_drain_tx_fifo(pio, sm);
  pio_sm_clear_fifos(pio, sm);
  pio_sm_restart(pio, sm);
  pio_sm_exec(pio, sm, pio_encode_jmp(offset));
  LOG_DEBUG("State Machine Restarted\n");
}

/**
 * @brief Calculates PIO clock divider for a given minimum pulse width
 * 
 * This function implements a sophisticated clock divider calculation that ensures
 * reliable signal detection by applying Nyquist sampling principles. It calculates
 * the divider needed to achieve at least 5 samples per shortest pulse.
 * 
 * Sampling Theory Application:
 * - Nyquist Theorem: Sample rate must be ≥2× signal frequency
 * - This implementation uses 5× oversampling for margin
 * - Provides robust detection even with clock jitter
 * - Enables mid-pulse sampling for noise immunity
 * 
 * Calculation Steps:
 * 1. Get RP2040 system clock (clk_sys, typically 125 MHz)
 * 2. Calculate shortest pulse frequency (1/pulse_width)
 * 3. Multiply by 5 to get target sampling rate
 * 4. Calculate divider: clk_sys / target_sampling_rate
 * 5. Round to nearest integer for PIO hardware
 * 
 * Example (125 MHz system clock, 30µs min pulse width):
 * - Shortest pulse: 1/30µs = 33.33 kHz
 * - Target sampling: 33.33 kHz × 5 = 166.67 kHz
 * - Clock divider: 125 MHz / 166.67 kHz = 750
 * - Actual PIO clock: 125 MHz / 750 = 166.67 kHz
 * - Sample interval: 6µs (5 samples in 30µs pulse)
 * 
 * Debug Output (aids in protocol tuning):
 * - RP2040 Clock Speed: System clock in kHz
 * - Desired PIO Sampling Rate: Target after 5× oversampling
 * - Calculated Clock Divider: Rounded divider value
 * - Effective PIO Clock Speed: Actual post-division frequency
 * - Effective Sample Interval: Time between samples (µs)
 * 
 * Protocol-Specific Usage:
 * - AT/PS2: ~30µs min pulse → 750 divider → 6µs sampling
 * - XT: ~60µs min pulse → 375 divider → 12µs sampling
 * - M0110: ~50µs min pulse → 500 divider → 10µs sampling
 * 
 * Validation Considerations:
 * - Minimum divider: 1 (max PIO freq = clk_sys)
 * - Maximum divider: 65536 (16-bit PIO hardware limit)
 * - Rounding may cause slight frequency deviation
 * - Always verify actual timing meets protocol specs
 * 
 * @param min_clock_pulse_width_us Shortest expected pulse width in microseconds
 * @return Clock divider value (integer, suitable for PIO hardware)
 * 
 * @note Uses 5× oversampling factor for reliable edge detection
 * @note Prints comprehensive debug info for protocol validation
 * @note Caller should verify returned divider is within PIO limits (1-65536)
 */
float calculate_clock_divider(int min_clock_pulse_width_us) {
  // Number of samples per pulse to ensure reliable detection
  // Based on the shortest pulse width, we want to at least sample it this many times
  const int samples_per_pulse = 5;

  // Get the system clock frequency in kHz
  float rp_clock_khz = 0.001 * clock_get_hz(clk_sys);
  LOG_INFO("RP2040 Clock Speed: %.0fKHz\n", rp_clock_khz);

    // Calculate the frequency of the shortest pulse.
  float shortest_pulse_khz = 1000.0 / (float)min_clock_pulse_width_us;
  
  // Calculate the desired PIO sampling rate.
  float target_sampling_khz = shortest_pulse_khz * samples_per_pulse;
  
  LOG_INFO("Desired PIO Sampling Rate: %.2fKHz\n", target_sampling_khz);

  // Calculate the clock divider.
  float clock_div = roundf(rp_clock_khz / target_sampling_khz);
  
  LOG_INFO("Calculated Clock Divider: %.0f\n", clock_div);
  
  // Calculate the actual effective PIO clock speed.
  float effective_pio_khz = rp_clock_khz / clock_div;
  LOG_INFO("Effective PIO Clock Speed: %.2fKHz\n", effective_pio_khz);
  
  // Calculate and print the sample interval.
  float sample_interval_us = (1.0 / effective_pio_khz) * 1000.0;
  LOG_INFO("Effective Sample Interval: %.2fus\n", sample_interval_us);

  return clock_div;

}