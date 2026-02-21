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

#ifndef AMIGA_SCANCODE_H
#define AMIGA_SCANCODE_H

#include <stdint.h>

/**
 * @brief Process Commodore Amiga keyboard scancode.
 *
 * Amiga keyboards use a simple make/break protocol:
 * - Bit 7: 0 = key press (make), 1 = key release (break)
 * - Bits 6-0: Key code (0x00-0x67)
 *
 * CAPS LOCK special case (0x62/0xE2):
 * - Only sends on press (never on release)
 * - Bit 7 indicates keyboard LED state (0=ON, 1=OFF)
 * - This processor handles synchronisation and timing
 *
 * @param code The scancode byte to process (after de-rotation and inversion)
 */
void process_scancode(uint8_t code);

/**
 * @brief Task function for scancode processor state machines
 *
 * Handles time-based operations like delayed CAPS LOCK release.
 * Must be called regularly from main loop.
 *
 * @note Non-blocking, safe to call frequently
 */
void scancode_task(void);

#endif  // AMIGA_SCANCODE_H
