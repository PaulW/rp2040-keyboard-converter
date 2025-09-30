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

/* Compile-time validation of PIO helper constants */
_Static_assert(1, "PIO helper basic sanity check");  // Always true, validates _Static_assert works

/**
 * Selects a PIO block that has enough contiguous instruction memory for the given program.
 *
 * Checks PIO0 first, then PIO1, and returns the first block able to accommodate the program.
 *
 * @param program The PIO program whose instruction memory requirements will be checked.
 * @return The PIO instance (pio0 or pio1) with sufficient instruction memory, or NULL if neither has space.
 *
 * @note The function only checks program space; it does not load the program or claim state machines.
 *       Caller must handle a NULL return value. 
 */
PIO find_available_pio(const pio_program_t *program) {
  if (!pio_can_add_program(pio0, program)) {
    printf("[WARN] PIO0 has no space for PIO Program\nChecking to see if we can load into PIO1\n");
    if (!pio_can_add_program(pio1, program)) {
      printf("[ERR] PIO1 has no space for PIO Program\n");
      return NULL;
    }
    return pio1;
  } else {
    return pio0;
  }
}

/**
 * Restart a PIO state machine and set its program counter to the given program offset.
 *
 * This clears and drains FIFOs, resets the state machine internal registers (PC, OSR, ISR),
 * and then executes a jump to the specified program offset so execution resumes at that entry.
 *
 * @param pio    PIO instance (e.g., pio0 or pio1).
 * @param sm     State machine index (0–3).
 * @param offset Program offset (instruction index) to jump to after the restart.
 *
 * @note This function is not interrupt-safe; callers must ensure exclusive PIO access.
 * @note Does not modify clock dividers, GPIO mappings, or side-set pin states; caller must reconfigure device-specific state if required.
 */
void pio_restart(PIO pio, uint sm, uint offset) {
  // Restart the PIO State Machine
  printf("[DBG] Resetting State Machine and re-initialising at offset: 0x%02X...\n", offset);
  pio_sm_drain_tx_fifo(pio, sm);
  pio_sm_clear_fifos(pio, sm);
  pio_sm_restart(pio, sm);
  pio_sm_exec(pio, sm, pio_encode_jmp(offset));
  printf("[DBG] State Machine Restarted\n");
}

/**
 * Compute a PIO clock divider that targets approximately five samples per the
 * shortest expected pulse to improve edge detection robustness.
 *
 * @param min_clock_pulse_width_us Shortest expected pulse width in microseconds.
 * @return Clock divider value rounded to the nearest integer suitable for PIO
 * hardware; expected valid range is 1 — 65536.
 *
 * @note Uses a fixed 5× oversampling factor to determine the target sampling rate.
 */
float calculate_clock_divider(int min_clock_pulse_width_us) {
  // Number of samples per pulse to ensure reliable detection
  // Based on the shortest pulse width, we want to at least sample it this many times
  const int samples_per_pulse = 5;

  // Get the system clock frequency in kHz
  float rp_clock_khz = 0.001 * clock_get_hz(clk_sys);
  printf("[INFO] RP2040 Clock Speed: %.0fKHz\n", rp_clock_khz);

    // Calculate the frequency of the shortest pulse.
  float shortest_pulse_khz = 1000.0 / (float)min_clock_pulse_width_us;
  
  // Calculate the desired PIO sampling rate.
  float target_sampling_khz = shortest_pulse_khz * samples_per_pulse;
  
  printf("[INFO] Desired PIO Sampling Rate: %.2fKHz\n", target_sampling_khz);

  // Calculate the clock divider.
  float clock_div = roundf(rp_clock_khz / target_sampling_khz);
  
  printf("[INFO] Calculated Clock Divider: %.0f\n", clock_div);
  
  // Calculate the actual effective PIO clock speed.
  float effective_pio_khz = rp_clock_khz / clock_div;
  printf("[INFO] Effective PIO Clock Speed: %.2fKHz\n", effective_pio_khz);
  
  // Calculate and print the sample interval.
  float sample_interval_us = (1.0 / effective_pio_khz) * 1000.0;
  printf("[INFO] Effective Sample Interval: %.2fus\n", sample_interval_us);

  return clock_div;

}