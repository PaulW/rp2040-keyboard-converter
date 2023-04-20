/*
 * This file is part of Model-F 5170 Converter.
 *
 * Copyright 2023 Paul Bramhall (paulwamp@gmail.com)
 *
 * Model-F 5170 Converter is free software: you can redistribute it
 * and/or modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * Model-F 5170 Converter is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Model-F 5170 Converter.
 * If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef _BOARDS_PCAT_CONVERTER_BOARD_H
#define _BOARDS_PCAT_CONVERTER_BOARD_H

// For board detection
#define PCAT_CONVERTER_BOARD

/* Fix for incorrect PICO_FLASH_SPI_CLKDIV for WaveShare RP2040 - Zero
   Max Clock frequency for Read Data instruction (03h) is 50 MHz.
   PICO_FLASH_SPI_CLKDIV of 2 sets the clock to around 60 MHz exceeding the spec.
*/
#define PICO_FLASH_SPI_CLKDIV 4

// Pick up the rest of the default config for the board.
#include "boards/waveshare_rp2040_zero.h"

#endif /* _BOARDS_PCAT_CONVERTER_BOARD_H */
