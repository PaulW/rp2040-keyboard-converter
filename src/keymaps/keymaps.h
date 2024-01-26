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

#ifndef KEYMAPS_H
#define KEYMAPS_H

#include <stdbool.h>
#include <stdint.h>

#define KEYMAP_ROWS 16
#define KEYMAP_COLS 16

uint8_t keymap_get_key_val(uint8_t pos, bool make);
bool keymap_is_action_key_pressed(void);

extern const uint8_t keymap_map[][KEYMAP_ROWS][KEYMAP_COLS];
extern const uint8_t keymap_actions[][KEYMAP_ROWS][KEYMAP_COLS];

#endif /* KEYMAPS_H */
