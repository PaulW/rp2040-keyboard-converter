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

#ifndef TYPES_H
#define TYPES_H

// This contains some standard type values which are used throughout the codebase.

enum led_types {
    // Define the different types of LED which we can use
    LED_RGB = 0x00,  // 0x00 /* RGB LED Type */
    LED_RBG,         // 0x01 /* RBG LED Type */
    LED_GRB,         // 0x02 /* GRB LED Type */
    LED_GBR,         // 0x03 /* GBR LED Type */
    LED_BRG,         // 0x04 /* BRG LED Type */
    LED_BGR,         // 0x05 /* BGR LED Type */
};

#endif /* TYPES_H */
