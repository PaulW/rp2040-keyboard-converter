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

#ifndef KEYMAP_H
#define KEYMAP_H

#include <stdint.h>

#define KEYMAP_ROWS 9
#define KEYMAP_COLS 16

uint16_t keymap_get_key_val(int layer, uint8_t pos);

extern const uint16_t keymap_map[][KEYMAP_ROWS][KEYMAP_COLS];
extern const uint16_t keymap_actions[][KEYMAP_ROWS][KEYMAP_COLS];

/*
 * IBM Model F AT UK 84-key (UK Standard Layout):
 * ,-------. ,-----------------------------------------------------------. ,---------------.
 * | F1| F2| |  \|  1|  2|  3|  4|  5|  6|  7|  8|  9|  0|  -|  =|  #| BS| |Esc|NmL|ScL|SyR|
 * |-------| |-----------------------------------------------------------| |---------------|
 * | F3| F4| |Tab  |  Q|  W|  E|  R|  T|  Y|  U|  I|  O|  P|  [|  ]|     | |  7|  8|  9|  *|
 * |-------| |-----------------------------------------------------|     | |-----------|---|
 * | F5| F6| |Ctrl  |  A|  S|  D|  F|  G|  H|  J|  K|  L|  ;|  '|     Ret| |  4|  5|  6|  -|
 * |-------| |-----------------------------------------------------------| |---------------|
 * | F7| F8| |Shift   |  Z|  X|  C|  V|  B|  N|  M|  ,|  ,|  /|     Shift| |  1|  2|  3|   |
 * |-------| |-----------------------------------------------------------| |-----------|  +|
 * | F9|F10| |Alt |    |                  Space                |    |CapL| |      0|  .|   |
 * `-------' `----'    `---------------------------------------'    `----' `---------------'
 * 
 * Keyboard uses a subset of Scancode Set 3 from the F122, but is not contiguous as is missing
 * extended function keys and block between main keyboard and numberpad.
 * ,-------. ,-----------------------------------------------------------. ,---------------.
 * | 05| 06| | 0E| 16| 1E| 26| 25| 2E| 36| 3D| 3E| 46| 45| 4E| 55| 5D| 66| | 76| 77| 7E| 84|
 * |-------| |-----------------------------------------------------------| |---------------|
 * | 04| 0C| | 0D  | 15| 1D| 24| 2D| 2C| 35| 3C| 43| 44| 4D| 54| 5B| -5C | | 6C| 75| 7D| 7C|
 * |-------| |-----------------------------------------------------------| |---------------|
 * | 03| 0B| | 14   | 1C| 1B| 23| 2B| 34| 33| 3B| 42| 4B| 4C| 52|-53| 5A | | 6B| 73| 74| 7B|
 * |-------| |-----------------------------------------------------------| |---------------|
 * | 83| 0A| | 12 |-13| 1A| 22| 21| 2A| 32| 31| 3A| 41| 49| 4A|-51|   59 | | 69| 72| 7A| 79|
 * |-------| |-----------------------------------------------------------| |-----------|---|
 * | 01| 09| | 11  |   | -19 |             29            | -39 |   |  58 | |-68| 70| 71|-78|
 * `-------' `-----'   `---------------------------------------'   `-----' `---------------'
 * 
 *  -: Hidden Keys (not used in standard UK Layout)
 */

// clang-format off
#define KEYMAP( \
    F00,F01,  K00,K01,K02,K03,K04,K05,K06,K07,K08,K09,K0A,K0B,K0C,K0D,K0E,  N00,N01,N02,N03, \
    F02,F03,  K0F,    K10,K11,K12,K13,K14,K15,K16,K17,K18,K19,K1A,K1B,K1C,  N04,N05,N06,N07, \
    F04,F05,  K1D,    K1E,K1F,K20,K21,K22,K23,K24,K25,K26,K27,K28,K29,K2A,  N08,N09,N0A,N0B, \
    F06,F07,  K2B,K2C,K2D,K2E,K2F,K30,K31,K32,K33,K34,K35,K36,K37,    K38,  N0C,N0D,N0E,N0F, \
    F08,F09,  K39,    K3A,                K3B,                K3C,    K3D,  N10,N11,N12,N13  \
) { \
    { KC_NO,    KC_##F08, KC_NO,    KC_##F04, KC_##F02, KC_##F00, KC_##F01, KC_NO,    /* 00-07 */ \
      KC_NO,    KC_##F09, KC_##F07, KC_##F05, KC_##F03, KC_##K0F, KC_##K00, KC_NO },  /* 08-0F */ \
    { KC_NO,    KC_##K39, KC_##K2B, KC_##K2C, KC_##K1D, KC_##K10, KC_##K01, KC_NO,    /* 10-17 */ \
      KC_NO,    KC_##K3A, KC_##K2D, KC_##K1F, KC_##K1E, KC_##K11, KC_##K02, KC_NO },  /* 18-1F */ \
    { KC_NO,    KC_##K2F, KC_##K2E, KC_##K20, KC_##K12, KC_##K04, KC_##K03, KC_NO,    /* 20-27 */ \
      KC_NO,    KC_##K3B, KC_##K30, KC_##K21, KC_##K14, KC_##K13, KC_##K05, KC_NO },  /* 28-2F */ \
    { KC_NO,    KC_##K32, KC_##K31, KC_##K23, KC_##K22, KC_##K15, KC_##K06, KC_NO,    /* 30-37 */ \
      KC_NO,    KC_##K3C, KC_##K33, KC_##K24, KC_##K16, KC_##K07, KC_##K08, KC_NO },  /* 38-3F */ \
    { KC_NO,    KC_##K34, KC_##K25, KC_##K17, KC_##K18, KC_##K0A, KC_##K09, KC_NO,    /* 40-47 */ \
      KC_NO,    KC_##K35, KC_##K36, KC_##K26, KC_##K27, KC_##K19, KC_##K0B, KC_NO },  /* 48-4F */ \
    { KC_NO,    KC_##K37, KC_##K28, KC_##K29, KC_##K1A, KC_##K0C, KC_NO,    KC_NO,    /* 50-57 */ \
      KC_##K3D, KC_##K38, KC_##K2A, KC_##K1B, KC_##K1C, KC_##K0D, KC_NO,    KC_NO },  /* 58-5F */ \
    { KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_##K0E, KC_NO,    /* 60-67 */ \
      KC_##N10, KC_##N0C, KC_NO,    KC_##N08, KC_##N04, KC_NO,    KC_NO,    KC_NO },  /* 68-6F */ \
    { KC_##N11, KC_##N12, KC_##N0D, KC_##N09, KC_##N0A, KC_##N05, KC_##N00, KC_##N01, /* 70-77 */ \
      KC_##N13, KC_##N0F, KC_##N0E, KC_##N0B, KC_##N07, KC_##N06, KC_##N02, KC_NO },  /* 78-7F */ \
    { KC_NO,    KC_NO,    KC_NO,    KC_##F06, KC_##N03, KC_NO,    KC_NO,    KC_NO,    /* 80-87 */ \
      KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO }   /* 88-8F */ \
}
// clang-format on

#endif /* KEYMAP_H */
