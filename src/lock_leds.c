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

#include "lock_leds.h"

lock_keys_union lock_leds;
uint8_t hid_lock_values = 0;
uint8_t ps2_lock_values = 0;

void set_lock_values_from_hid(uint8_t lock_val) {
  lock_leds.keys.numLock = (unsigned char)(lock_val & 0x01);
  lock_leds.keys.capsLock = (unsigned char)((lock_val >> 1) & 0x01);
  lock_leds.keys.scrollLock = (unsigned char)((lock_val >> 2) & 0x01);

  hid_lock_values = (uint8_t)lock_leds.value;
  ps2_lock_values = (uint8_t)((lock_leds.keys.capsLock << 2) | (lock_leds.keys.numLock << 1) | lock_leds.keys.scrollLock);
}
