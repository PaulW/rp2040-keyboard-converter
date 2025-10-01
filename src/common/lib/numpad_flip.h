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

#ifndef NUMPAD_FLIP_H
#define NUMPAD_FLIP_H

#include <stdint.h>

/**
 * @brief Optimized numpad flip function for Mac numlock emulation.
 * 
 * Maps between numpad keys and navigation/editing keys bidirectionally:
 * - Numpad → Navigation: P0→INS, P1→END, P2→DOWN, etc.
 * - Navigation → Numpad: INS→P0, END→P1, DOWN→P2, etc.
 * 
 * Uses a 256-byte lookup table for O(1) constant-time performance,
 * replacing the previous 24-level nested ternary macro.
 * 
 * @param key The HID keycode to flip
 * @return The flipped keycode, or the original if no mapping exists
 */
uint8_t numpad_flip_code(uint8_t key);

#endif /* NUMPAD_FLIP_H */
