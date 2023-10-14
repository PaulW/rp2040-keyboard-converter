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

// --- UART ---
#ifndef PICO_DEFAULT_UART
#define PICO_DEFAULT_UART 0
#endif
#ifndef PICO_DEFAULT_UART_TX_PIN
#define PICO_DEFAULT_UART_TX_PIN 0
#endif
#ifndef PICO_DEFAULT_UART_RX_PIN
#define PICO_DEFAULT_UART_RX_PIN 1
#endif

// --- SPI/I2C ---
/* We don't use either capability, so there is no need to define these here. */

// --- FLASH ---
#define PICO_BOOT_STAGE2_CHOOSE_W25Q080 1

/* When using W25Q16JVUXIQ Flash Chip, max clock frequency for Read Data instruction (03h) is 50 MHz.
   Default value of PICO_FLASH_SPI_CLKDIV of 2 sets the clock to around 60 MHz exceeding the spec.
*/
#define PICO_FLASH_SPI_CLKDIV 4

#ifndef PICO_FLASH_SIZE_BYTES
#define PICO_FLASH_SIZE_BYTES (2 * 1024 * 1024)
#endif

// All (current) Custom Boards have B2 RP2040
#ifndef PICO_RP2040_B0_SUPPORTED
#define PICO_RP2040_B0_SUPPORTED 0
#endif

#ifndef PICO_RP2040_B1_SUPPORTED
#define PICO_RP2040_B1_SUPPORTED 0
#endif

#endif /* _BOARDS_PCAT_CONVERTER_BOARD_H */
