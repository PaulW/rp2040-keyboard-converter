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

/**
 * @brief Finds an available PIO (Programmable I/O) instance for a given PIO program.
 * This function checks if there is space in either PIO0 or PIO1 for the specified PIO program, and
 * returns the available PIO instance. If there is no space in either PIO instance, it returns NULL.
 *
 * @param program The PIO program to check for available space.
 *
 * @return The available PIO instance (PIO0 or PIO1) if space is available, or NULL if no space is
 *         available.
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
 * @brief Restarts the specified PIO state machine for the.
 * This function restarts the specified PIO State Machine and resets the it to its initial state. It
 * clears the FIFOs, drains the transmit FIFO, and restarts the state machine. After restarting, it
 * jumps then to the specified offset in the state machine code.
 *
 * @param pio    The PIO instance.
 * @param sm     The state machine number.
 * @param offset The offset to re-initialize the state machine at.
 *
 * @note The relevant device state will need to be set accordingly within the calling function. This
 * method simply resets the PIO state machine without any feedback.
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
 * @brief Calculates the clock divider for a given minimum clock pulse width.
 * This function calculates the clock divider value needed to achieve a specified minimum clock
 * pulse width in microseconds. The calculation is based on the system clock frequency and the
 * desired pulse width.
 *
 * @param min_clock_pulse_width_us The minimum clock pulse width in microseconds.
 *
 * @return The calculated clock divider value.
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