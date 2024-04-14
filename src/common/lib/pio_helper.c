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

#include <stdio.h>

PIO find_available_pio(const pio_program_t *program) {
  // This is a simple Helper Function which will check if there is space in either PIO0 or PIO1 for a given PIO Program.
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