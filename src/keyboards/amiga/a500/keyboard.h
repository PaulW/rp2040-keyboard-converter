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

#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "hid_keycodes.h"

// clang-format off
/* Commodore Amiga 500/2000 Keyboard Layout
 * 
 * ,---. ,------------------------. ,------------------------.
 * |Esc| |F1  |F2  |F3  |F4  |F5  | |F6  |F7  |F8  |F9  |F10 |
 * `---' `------------------------' `------------------------'
 * ,-------------------------------------------------------------. ,-----------. ,---------------.
 * |  `  |  1|  2|  3|  4|  5|  6|  7|  8|  9|  0|  -|  =|  \|Bsp| |Del  |Help | |  (|  )|  /|  *|
 * |-------------------------------------------------------------| `-----------' |---------------|
 * |  Tab  |  Q|  W|  E|  R|  T|  Y|  U|  I|  O|  P|  [|  ]|     |               |  7|  8|  9|  -|
 * |--------------------------------------------------------\Entr|     ,---.     |---------------|
 * |Ctrl|Cap|  A|  S|  D|  F|  G|  H|  J|  K|  L|  ;|  '|  #|    |     |Up |     |  4|  5|  6|  +|
 * |-------------------------------------------------------------| ,-----------. |---------------|
 * |Shift |  <|  Z|  X|  C|  V|  B|  N|  M|  ,|  .|  /|  Shift   | |Lef|Dow|Rig| |  1|  2|  3|   |
 * `-------------------------------------------------------------' `-----------' |-----------|Ent|
 *     |Alt |A   |            Space                  |A   |Alt |                 |      0|  .|   |
 *     `-------------------------------------------------------'                 `---------------'
 * 
 * Raw Scancode Map (in hex):
 * ,---. ,------------------------. ,------------------------.
 * | 45| | 50 | 51 | 52 | 53 | 54 | | 55 | 56 | 57 | 58 | 59 |
 * `---' `------------------------' `------------------------'
 * ,-------------------------------------------------------------. ,-----------. ,---------------.
 * | 00  | 01| 02| 03| 04| 05| 06| 07| 08| 09| 0A| 0B| 0C| 0D| 41| | 46  | 5F  | | 5A| 5B| 5C| 5D|
 * |-------------------------------------------------------------| `-----------' |---------------|
 * | 42    | 10| 11| 12| 13| 14| 15| 16| 17| 18| 19| 1A| 1B|     |               | 3D| 3E| 3F| 4A|
 * |--------------------------------------------------------\ 44 |     ,---.     |---------------|
 * | 63 | 62| 20| 21| 22| 23| 24| 25| 26| 27| 28| 29| 2A| 2B|    |     | 4C|     | 2D| 2E| 2F| 5E|
 * |-------------------------------------------------------------| ,-----------. |---------------|
 * | 60   | 30| 31| 32| 33| 34| 35| 36| 37| 38| 39| 3A|    61    | | 4F| 4D| 4E| | 1D| 1E| 1F|   |
 * `-------------------------------------------------------------' `-----------' |-----------| 43|
 *     | 64 | 66 |              40                   | 67 | 65 |                 |     0F| 3C|   |
 *     `-------------------------------------------------------'                 `---------------'
 * 
 * Special Keys:
 * - Left Amiga: 0x66
 * - Right Amiga: 0x67
 * - Help: 0x5F (acts like a function key, can be mapped to Insert/Help)
 * - Keypad ( and ): 0x5A, 0x5B (unique to Amiga)
 * - UK Layout: # key at 0x2B (backslash on US layout)
 * - < > key: 0x30 (between Left Shift and Z on European keyboards)
 * 
 * Notes:
 * - Scancodes 0x00-0x67 are valid key positions
 * - Special codes (0x78, 0xF9, 0xFA, 0xFC, 0xFD, 0xFE) handled by protocol layer
 * - All keys send up/down events (bit 7 set for key release)
 * - No modifier prefix codes - straight scancode mapping
 */

#define KEYMAP_AMIGA( \
    K45,      K50,K51,K52,K53,K54,  K55,K56,K57,K58,K59, \
    K00,K01,K02,K03,K04,K05,K06,K07,K08,K09,K0A,K0B,K0C,K0D,K41,  K46,K5F,  K5A,K5B,K5C,K5D, \
    K42,  K10,K11,K12,K13,K14,K15,K16,K17,K18,K19,K1A,K1B,K44,              K3D,K3E,K3F,K4A, \
    K63,K62,K20,K21,K22,K23,K24,K25,K26,K27,K28,K29,K2A,K2B,        K4C,    K2D,K2E,K2F,K5E, \
    K60,  K30,K31,K32,K33,K34,K35,K36,K37,K38,K39,K3A,K61,      K4F,K4D,K4E,K1D,K1E,K1F,K43, \
         K64,K66,            K40,            K67,K65,                        K0F,  K3C       \
) { \
    { KC_##K00, KC_##K01, KC_##K02, KC_##K03, KC_##K04, KC_##K05, KC_##K06, KC_##K07,   /* 00-07 */ \
      KC_##K08, KC_##K09, KC_##K0A, KC_##K0B, KC_##K0C, KC_##K0D, KC_NO,    KC_##K0F }, /* 08-0F */ \
    { KC_##K10, KC_##K11, KC_##K12, KC_##K13, KC_##K14, KC_##K15, KC_##K16, KC_##K17,   /* 10-17 */ \
      KC_##K18, KC_##K19, KC_##K1A, KC_##K1B, KC_NO,    KC_##K1D, KC_##K1E, KC_##K1F }, /* 18-1F */ \
    { KC_##K20, KC_##K21, KC_##K22, KC_##K23, KC_##K24, KC_##K25, KC_##K26, KC_##K27,   /* 20-27 */ \
      KC_##K28, KC_##K29, KC_##K2A, KC_##K2B, KC_NO,    KC_##K2D, KC_##K2E, KC_##K2F }, /* 28-2F */ \
    { KC_##K30, KC_##K31, KC_##K32, KC_##K33, KC_##K34, KC_##K35, KC_##K36, KC_##K37,   /* 30-37 */ \
      KC_##K38, KC_##K39, KC_##K3A, KC_NO,    KC_##K3C, KC_##K3D, KC_##K3E, KC_##K3F }, /* 38-3F */ \
    { KC_##K40, KC_##K41, KC_##K42, KC_##K43, KC_##K44, KC_##K45, KC_##K46, KC_NO,      /* 40-47 */ \
      KC_NO,    KC_NO,    KC_##K4A, KC_NO,    KC_##K4C, KC_##K4D, KC_##K4E, KC_##K4F }, /* 48-4F */ \
    { KC_##K50, KC_##K51, KC_##K52, KC_##K53, KC_##K54, KC_##K55, KC_##K56, KC_##K57,   /* 50-57 */ \
      KC_##K58, KC_##K59, KC_##K5A, KC_##K5B, KC_##K5C, KC_##K5D, KC_##K5E, KC_##K5F }, /* 58-5F */ \
    { KC_##K60, KC_##K61, KC_##K62, KC_##K63, KC_##K64, KC_##K65, KC_##K66, KC_##K67,   /* 60-67 */ \
      KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO },    /* 68-6F */ \
    { KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO,      /* 70-77 */ \
      KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO },    /* 78-7F */ \
}
// clang-format on

#endif /* KEYBOARD_H */
