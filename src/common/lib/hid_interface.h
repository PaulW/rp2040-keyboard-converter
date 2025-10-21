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

#ifndef HID_INTERFACE_H
#define HID_INTERFACE_H

#include "config.h"
#include "pico/stdlib.h"

void handle_keyboard_report(uint8_t rawcode, bool make);
void handle_mouse_report(const uint8_t buttons[5], int8_t pos[3]);
void hid_device_setup(void);
void send_empty_keyboard_report(void);

#endif /* HID_INTERFACE_H */
