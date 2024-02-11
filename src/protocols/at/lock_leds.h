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

#ifndef LOCK_LEDS_H
#define LOCK_LEDS_H

#include <stdint.h>

// Define the LED Locklight Indicators
typedef struct lock_keys {
  unsigned char numLock : 1;
  unsigned char capsLock : 1;
  unsigned char scrollLock : 1;
} lock_keys;

typedef union lock_keys_union {
  lock_keys keys;
  unsigned char value;
} lock_keys_union;

extern lock_keys_union lock_leds;

extern uint8_t hid_lock_values;
extern uint8_t ps2_lock_values;

void set_lock_values_from_hid(uint8_t lock_val);

#endif /* LOCK_LEDS_H */
