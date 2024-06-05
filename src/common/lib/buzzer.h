/*
 * This file is part of RP2040 Keyboard Converter.
 *
 * Copyright 2023 Paul Bramhall (paulwamp@gmail.com)
 *
 * Portions of this specific file are licensed under the MIT License.
 * The original source can be found at: https://github.com/Zheoni/pico-simon
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

#ifndef BUZZER_H
#define BUZZER_H

#include "pico/types.h"

typedef uint32_t sound;

typedef struct {
  sound s;
  uint32_t d;
} note;

extern note READY_SEQUENCE[];
extern note LOCK_LED[];

#define BUZZER_END_SEQUENCE \
  { 0, 0 }

sound buzzer_calc_sound(uint freq);
void buzzer_init(uint buzzer_pin);
bool buzzer_play_sound_sequence_non_blocking(note *notes);

#endif /* BUZZER_H */