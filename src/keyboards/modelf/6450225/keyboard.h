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

#ifndef KEYMAP_H
#define KEYMAP_H

#include "keymaps.h"

/* IBM 5170 (Model F-AT)
 * Keyboard uses Scancode Set 2.
 * ,-------. ,-----------------------------------------------------------. ,---------------.
 * | 05| 06| | 0E| 16| 1E| 26| 25| 2E| 36| 3D| 3E| 46| 45| 4E| 55| 5D| 66| | 76| 77| 7E|~84|
 * |-------| |-----------------------------------------------------------| |---------------|
 * | 04| 0C| | 0D  | 15| 1D| 24| 2D| 2C| 35| 3C| 43| 44| 4D| 54| 5B| -5C | | 6C| 75| 7D| 7C|
 * |-------| |-----------------------------------------------------------| |---------------|
 * | 03| 0B| | 14   | 1C| 1B| 23| 2B| 34| 33| 3B| 42| 4B| 4C| 52|-53| 5A | | 6B| 73| 74| 7B|
 * |-------| |-----------------------------------------------------------| |---------------|
 * |~83| 0A| | 12 |-13| 1A| 22| 21| 2A| 32| 31| 3A| 41| 49| 4A|-51|   59 | | 69| 72| 7A| 79|
 * |-------| |-----------------------------------------------------------| |-----------|---|
 * | 01| 09| | 11  |   | -19 |             29            | -39 |   |  58 | |-68| 70| 71|-78|
 * `-------' `-----'   `---------------------------------------'   `-----' `---------------'
 * 
 *  -: Hidden Keys (not used in standard UK Layout)
 *  ~: Remaps to alternate code (83-03, 84-7F)
 */

// clang-format off
#define KEYMAP_PCAT( \
    K05,K06,  K0E,K16,K1E,K26,K25,K2E,K36,K3D,K3E,K46,K45,K4E,K55,K5D,K66,  K76,K77,K7E,K7F, \
    K04,K0C,  K0D,    K15,K1D,K24,K2D,K2C,K35,K3C,K43,K44,K4D,K54,K5B,      K6C,K75,K7D,K7C, \
    K03,K0B,  K14,    K1C,K1B,K23,K2B,K34,K33,K3B,K42,K4B,K4C,K52,    K5A,  K6B,K73,K74,K7B, \
    K02,K0A,  K12,    K1A,K22,K21,K2A,K32,K31,K3A,K41,K49,K4A,        K59,  K69,K72,K7A,K79, \
    K01,K09,  K11,                        K29,                        K58,      K70,K71  \
) { \
    { KC_NO,    KC_##K01, KC_##K02, KC_##K03, KC_##K04, KC_##K05, KC_##K06, KC_NO,     /* 00-07 */ \
      KC_NO,    KC_##K09, KC_##K0A, KC_##K0B, KC_##K0C, KC_##K0D, KC_##K0E, KC_NO },   /* 08-0F */ \
    { KC_NO,    KC_##K11, KC_##K12, KC_NO,    KC_##K14, KC_##K15, KC_##K16, KC_NO,     /* 10-17 */ \
      KC_NO,    KC_NO,    KC_##K1A, KC_##K1B, KC_##K1C, KC_##K1D, KC_##K1E, KC_NO },   /* 18-1F */ \
    { KC_NO,    KC_##K21, KC_##K22, KC_##K23, KC_##K24, KC_##K25, KC_##K26, KC_NO,     /* 20-27 */ \
      KC_NO,    KC_##K29, KC_##K2A, KC_##K2B, KC_##K2C, KC_##K2D, KC_##K2E, KC_NO },   /* 28-2F */ \
    { KC_NO,    KC_##K31, KC_##K32, KC_##K33, KC_##K34, KC_##K35, KC_##K36, KC_NO,     /* 30-37 */ \
      KC_NO,    KC_NO,    KC_##K3A, KC_##K3B, KC_##K3C, KC_##K3D, KC_##K3E, KC_NO },   /* 38-3F */ \
    { KC_NO,    KC_##K41, KC_##K42, KC_##K43, KC_##K44, KC_##K45, KC_##K46, KC_NO,     /* 40-47 */ \
      KC_NO,    KC_##K49, KC_##K4A, KC_##K4B, KC_##K4C, KC_##K4D, KC_##K4E, KC_NO },   /* 48-4F */ \
    { KC_NO,    KC_NO,    KC_##K52, KC_NO,    KC_##K54, KC_##K55, KC_NO,    KC_NO,     /* 50-57 */ \
      KC_##K58, KC_##K59, KC_##K5A, KC_##K5B, KC_NO,    KC_##K5D, KC_NO,    KC_NO },   /* 58-5F */ \
    { KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_##K66, KC_NO,     /* 60-67 */ \
      KC_NO,    KC_##K69, KC_NO,    KC_##K6B, KC_##K6C, KC_NO,    KC_NO,    KC_NO },   /* 68-6F */ \
    { KC_##K70, KC_##K71, KC_##K72, KC_##K73, KC_##K74, KC_##K75, KC_##K76, KC_##K77,  /* 70-77 */ \
      KC_NO,    KC_##K79, KC_##K7A, KC_##K7B, KC_##K7C, KC_##K7D, KC_##K7E, KC_##K7F } /* 78-7F */ \
}
// clang-format on

#endif /* KEYMAP_H */
