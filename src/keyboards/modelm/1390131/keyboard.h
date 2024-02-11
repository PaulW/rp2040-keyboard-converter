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

/* IBM Model M 101 Key
 * Keyboard uses a Scancode Set 2
 * ,---.   ,---------------. ,---------------. ,---------------. ,-----------.
 * | 76|   | 05| 06| 04| 0C| | 03| 0B| 83| 0A| | 01| 09| 78| 07| |+7C| 7E|+77|
 * `---'   `---------------' `---------------' `---------------' `-----------'
 * ,-----------------------------------------------------------. ,-----------. ,---------------.
 * | 0E| 16| 1E| 26| 25| 2E| 36| 3D| 3E| 46| 45| 4E| 55| 6A| 66| |*70|*6C|*7D| | 77|*4A| 7C| 7B|
 * |-----------------------------------------------------------| |-----------| |---------------|
 * | 0D  | 15| 1D| 24| 2D| 2C| 35| 3C| 43| 44| 4D| 54| 5B|  5D | |*71|*69|*7A| | 6C| 75| 7D| 79|
 * |-----------------------------------------------------------| `-----------' |---------------|
 * | 58   | 1C| 1B| 23| 2B| 34| 33| 3B| 42| 4B| 4C| 52| ^a| 5A |               | 6B| 73| 74| 6D|
 * |-----------------------------------------------------------|     ,---.     |---------------|
 * | 12 | 61| 1A| 22| 21| 2A| 32| 31| 3A| 41| 49| 4A| 51|  59  |     |*75|     | 69| 72| 7A|*5A|
 * |-----------------------------------------------------------| ,-----------. |---------------|
 * | 14|   | 11|          29                   |*11|       |*14| |*6B|*72|*74| | 68| 70| 71| 63|
 * `-----------------------------------------------------------' `-----------' `---------------'
 * *: E0-prefixed codes.
 * +: Special codes sequence
 * ^a: ISO hash key and US backslash use identical code 5D.
 * 51, 63, 68, 6A, 6D: Hidden keys in IBM model M [6]
 * 
 *  -: Hidden Keys (not used in standard UK Layout)
 */

// clang-format off
#define KEYMAP_IBM_101KEY( \
    K76,    K05,K06,K04,K0C,  K03,K0B,K83,K0A,  K01,K09,K78,K07,  KFC,K7E,KF7, \
    K0E,K16,K1E,K26,K25,K2E,K36,K3D,K3E,K46,K45,K4E,K55,    K66,  KF0,KEC,KFD,  K77,KCA,K7C,K7B, \
    K0D,    K15,K1D,K24,K2D,K2C,K35,K3C,K43,K44,K4D,K54,K5B,K5D,  KF1,KE9,KFA,  K6C,K75,K7D,K79, \
    K58,    K1C,K1B,K23,K2B,K34,K33,K3B,K42,K4B,K4C,K52,    K5A,                K6B,K73,K74,   \
    K12,    K1A,K22,K21,K2A,K32,K31,K3A,K41,K49,K4A,        K59,      KF5,      K69,K72,K7A,KDA, \
    K14,    K11,                K29,                K91,    K94,  KEB,KF2,KF4,      K70,K71 \
) { \
    { KC_NO,    KC_##K01, KC_NO,    KC_##K03, KC_##K04, KC_##K05, KC_##K06, KC_##K07, /* 00-07 */ \
      KC_NO,    KC_##K09, KC_##K0A, KC_##K0B, KC_##K0C, KC_##K0D, KC_##K0E, KC_NO },  /* 08-0F */ \
    { KC_NO,    KC_##K11, KC_##K12, KC_NO,    KC_##K14, KC_##K15, KC_##K16, KC_NO,    /* 10-17 */ \
      KC_NO,    KC_NO,    KC_##K1A, KC_##K1B, KC_##K1C, KC_##K1D, KC_##K1E, KC_NO },  /* 18-1F */ \
    { KC_NO,    KC_##K21, KC_##K22, KC_##K23, KC_##K24, KC_##K25, KC_##K26, KC_NO,    /* 20-27 */ \
      KC_NO,    KC_##K29, KC_##K2A, KC_##K2B, KC_##K2C, KC_##K2D, KC_##K2E, KC_NO },  /* 28-2F */ \
    { KC_NO,    KC_##K31, KC_##K32, KC_##K33, KC_##K34, KC_##K35, KC_##K36, KC_NO,    /* 30-37 */ \
      KC_NO,    KC_NO,    KC_##K3A, KC_##K3B, KC_##K3C, KC_##K3D, KC_##K3E, KC_NO },  /* 38-3F */ \
    { KC_NO,    KC_##K41, KC_##K42, KC_##K43, KC_##K44, KC_##K45, KC_##K46, KC_NO,    /* 40-47 */ \
      KC_NO,    KC_##K49, KC_##K4A, KC_##K4B, KC_##K4C, KC_##K4D, KC_##K4E, KC_NO },  /* 48-4F */ \
    { KC_NO,    KC_NO,    KC_##K52, KC_NO,    KC_##K54, KC_##K55, KC_NO,    KC_NO,    /* 50-57 */ \
      KC_##K58, KC_##K59, KC_##K5A, KC_##K5B, KC_NO,    KC_##K5D, KC_NO,    KC_NO },  /* 58-5F */ \
    { KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_##K66, KC_NO,    /* 60-67 */ \
      KC_NO,    KC_##K69, KC_NO,    KC_##K6B, KC_##K6C, KC_NO,    KC_NO,    KC_NO },  /* 68-6F */ \
    { KC_##K70, KC_##K71, KC_##K72, KC_##K73, KC_##K74, KC_##K75, KC_##K76, KC_##K77, /* 70-77 */ \
      KC_##K78, KC_##K79, KC_##K7A, KC_##K7B, KC_##K7C, KC_##K7D, KC_##K7E, KC_NO },  /* 78-7F */ \
    { KC_NO,    KC_NO,    KC_NO,    KC_##K83, KC_NO,    KC_NO,    KC_NO,    KC_NO,    /* 80-87 */ \
      KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO },  /* 88-8F */ \
    { KC_NO,    KC_##K91, KC_NO,    KC_NO,    KC_##K94, KC_NO,    KC_NO,    KC_NO,    /* 90-97 */ \
      KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO },  /* 98-9F */ \
    { KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO,    /* A0-A7 */ \
      KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO },  /* A8-AF */ \
    { KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO,    /* B0-B7 */ \
      KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO },  /* B8-BF */ \
    { KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO,    /* C0-C7 */ \
      KC_NO,    KC_NO,    KC_##KCA, KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO },  /* C8-CF */ \
    { KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO,    /* D0-D7 */ \
      KC_NO,    KC_NO,    KC_##KDA, KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO },  /* D8-DF */ \
    { KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO,    /* E0-E7 */ \
      KC_NO,    KC_##KE9, KC_NO,    KC_##KEB, KC_##KEC, KC_NO,    KC_NO,    KC_NO },  /* E8-EF */ \
    { KC_##KF0, KC_##KF1, KC_##KF2, KC_NO,    KC_##KF4, KC_##KF5, KC_NO,    KC_##KF7, /* F0-F7 */ \
      KC_NO,    KC_NO,    KC_##KFA, KC_NO,    KC_##KFC, KC_##KFD, KC_NO,    KC_NO }   /* F8-FF */ \
}

// clang-format on

#endif /* KEYMAP_H */
