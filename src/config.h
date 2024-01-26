/*
 * This file is part of Model-F 5170 Converter.
 *
 * Copyright 2023 Paul Bramhall (paulwamp@gmail.com)
 *
 * Portions of this specific file are licensed under the MIT License.
 * The original source can be found at: https://github.com/Zheoni/pico-simon
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

#ifndef CONFIG_H
#define CONFIG_H

// Ensure you specify the correct DATA Pin for the connector.  Also, ensure CLK is connected to DATA+1
// #define DATA_PIN 16   // We assume that DATA and CLOCK are positioned next to each other, as such Clock will be DATA_PIN + 1
#define DATA_PIN 6    // We assume that DATA and CLOCK are positioned next to each other, as such Clock will be DATA_PIN + 1
#define PIEZO_PIN 11  // Piezo Buzzer GPIO Pin
#define BUF_SIZE 16   // Define size for Ring Buffer

#define APPLE_EMULATED_NUMLOCK 1       // Apple Devices don't natively support Numlock, so we must emulate the functionality.
#define APPLE_INITIAL_NUMLOCK_STATE 1  // Define NumLock State when emulating Apple devices (default ON)

#endif /* CONFIG_H */
