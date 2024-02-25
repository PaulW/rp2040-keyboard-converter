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

/* Cherry G80 (ISO Layout - 1104H):
 * Keyboard uses a Scancode Set 1
 * ,---.   ,---------------. ,---------------. ,---------------. ,-----------.
 * | 01|   | 3B| 3C| 3D| 3E| | 3F| 40| 41| 42| | 43| 44| 57| 58| |*37| 46|*46|
 * `---'   `---------------' `---------------' `---------------' `-----------'
 * ,-----------------------------------------------------------. ,-----------. ,---------------.
 * | 29| 02| 03| 04| 05| 06| 07| 08| 09| 0A| 0B| 0C| 0D|     0E| |e52|e47|e49| | 45|e35| 37| 4A|
 * |-----------------------------------------------------------| |-----------| |---------------|
 * | 0F  | 10| 11| 12| 13| 14| 15| 16| 17| 18| 19| 1A| 1B|     | |e53|e4F|e51| | 47| 48| 49| 4E|
 * |------------------------------------------------------.    | `-----------' |-----------|   |
 * | 3A   | 1E| 1F| 20| 21| 22| 23| 24| 25| 26| 27| 28| 2B| 1C |               | 4B| 4C| 4D| 7E|
 * |-----------------------------------------------------------|     ,---.     |---------------|
 * | 2A     | 2C| 2D| 2E| 2F| 30| 31| 32| 33| 34| 35|      36  |     |e48|     | 4F| 50| 51|e1C|
 * |-----------------------------------------------------------| ,-----------. |-----------|   |
 * | 1D |    | 7B |              39             | e38|    | e1D| |e4B|e50|e4D| |   52|   53| 59|
 * `----'    `---------------------------------------`    '----' `-----------' `---------------'
 * 
 * e: E0-prefixed codes.
 * *: special handling codes
 */

// clang-format off
#define KEYMAP_ISO( \
    K01,    K3B,K3C,K3D,K3E,  K3F,K40,K41,K42,  K43,K44,K57,K58,  K54,K46,K55, \
    K29,K02,K03,K04,K05,K06,K07,K08,K09,K0A,K0B,K0C,K0D,    K0E,  K71,K74,K77,  K45,K7F,K37,K4A, \
    K0F,    K10,K11,K12,K13,K14,K15,K16,K17,K18,K19,K1A,K1B,      K72,K75,K78,  K47,K48,K49,K4E, \
    K3A,    K1E,K1F,K20,K21,K22,K23,K24,K25,K26,K27,K28,K2B,K1C,                K4B,K4C,K4D,K7E, \
    K2A,    K2C,K2D,K2E,K2F,K30,K31,K32,K33,K34,K35,        K36,      K60,      K4F,K50,K51,K6F, \
    K1D,    K38,                K39,                K7C,    K7A,  K61,K62,K63,      K52,K53,K59  \
) { \
    { KC_NO,    KC_##K01, KC_##K02, KC_##K03, KC_##K04, KC_##K05, KC_##K06, KC_##K07,   /* 00-07 */ \
      KC_##K08, KC_##K09, KC_##K0A, KC_##K0B, KC_##K0C, KC_##K0D, KC_##K0E, KC_##K0F }, /* 08-0F */ \
    { KC_##K10, KC_##K11, KC_##K12, KC_##K13, KC_##K14, KC_##K15, KC_##K16, KC_##K17,   /* 10-17 */ \
      KC_##K18, KC_##K19, KC_##K1A, KC_##K1B, KC_##K1C, KC_##K1D, KC_##K1E, KC_##K1F }, /* 18-1F */ \
    { KC_##K20, KC_##K21, KC_##K22, KC_##K23, KC_##K24, KC_##K25, KC_##K26, KC_##K27,   /* 20-27 */ \
      KC_##K28, KC_##K29, KC_##K2A, KC_##K2B, KC_##K2C, KC_##K2D, KC_##K2E, KC_##K2F }, /* 28-2F */ \
    { KC_##K30, KC_##K31, KC_##K32, KC_##K33, KC_##K34, KC_##K35, KC_##K36, KC_##K37,   /* 30-37 */ \
      KC_##K38, KC_##K39, KC_##K3A, KC_##K3B, KC_##K3C, KC_##K3D, KC_##K3E, KC_##K3F }, /* 38-3F */ \
    { KC_##K40, KC_##K41, KC_##K42, KC_##K43, KC_##K44, KC_##K45, KC_##K46, KC_##K47,   /* 40-47 */ \
      KC_##K48, KC_##K49, KC_##K4A, KC_##K4B, KC_##K4C, KC_##K4D, KC_##K4E, KC_##K4F }, /* 48-4F */ \
    { KC_##K50, KC_##K51, KC_##K52, KC_##K53, KC_##K54, KC_##K55, KC_NO,    KC_##K57,   /* 50-57 */ \
      KC_##K58, KC_##K59, KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO    }, /* 58-5F */ \
    { KC_##K60, KC_##K61, KC_##K62, KC_##K63, KC_NO,    KC_NO,    KC_NO,    KC_NO   ,   /* 60-67 */ \
      KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_##K6F }, /* 68-6F */ \
    { KC_NO,    KC_##K71, KC_##K72, KC_NO,    KC_##K74, KC_##K75, KC_NO,    KC_##K77,   /* 70-77 */ \
      KC_##K78, KC_NO,    KC_##K7A, KC_NO,    KC_##K7C, KC_NO,    KC_##K7E, KC_##K7F }  /* 78-7F */ \
}

// clang-format on

#endif /* KEYMAP_H */
