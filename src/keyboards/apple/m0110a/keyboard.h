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

#include "hid_keycodes.h"

// clang-format off
/* Apple M0110A Base Layer
 * ,-------------------------------------------------------- . ,---------------.
 * |  `|  1|  2|  3|  4|  5|  6|  7|  8|  9|  0|  -|  =|   BS| |Clr|  =|  /|  *|
 * |---------------------------------------------------------| |---------------|
 * |Tab  |  Q|  W|  E|  R|  T|  Y|  U|  I|  O|  P|  [|  ]|   | |  7|  8|  9|  -|
 * |----------------------------------------------------'    | |---------------|
 * |CapsL |  A|  S|  D|  F|  G|  H|  J|  K|  L|  ;|  '|   Ret| |  4|  5|  6|  +|
 * |---------------------------------------------------------| |---------------|
 * |Shift   |  Z|  X|  C|  V|  B|  N|  M|  ,|  .|  /|Shft| Up| |  1|  2|  3|   |
 * |--------------------------------------- -----------------| |-----------|Ent|
 * | Optn|    Mac|           Space           |  \|Lft|Rig|Dwn| |      0|  .|   |
 * `---------------------------------------------------------' `---------------'
 * 
 * Raw Codes (in hex):
 * ,---------------------------------------------------------. ,---------------.
 * | 65| 25| 27| 29| 2B| 2F| 2D| 35| 39| 33| 3B| 37| 31|   67| |*0F|*11|*1B|*05|
 * |---------------------------------------------------------| |---------------|
 * |   61| 19| 1B| 1D| 1F| 23| 21| 41| 45| 3F| 47| 43| 3D|   | |+33|+37|+39|+1D|
 * |-----------------------------------------------------'   | |---------------|
 * |    73| 01| 03| 05| 07| 0B| 09| 4D| 51| 4B| 53| 4F|    49| |+2D|+2F|+31|*0D|
 * |---------------------------------------------------------| |---------------|
 * |      71| 0D| 0F| 11| 13| 17| 5B| 5D| 57| 5F| 59| ~71|+1B| |+27|+29|+2B|+19|
 * |---------------------------------------------------------| |-----------|   |
 * |   75|     6F|            63             | 55|+0D|+05|+11| |    +25|+03|   |
 * `---------------------------------------------------------' `---------------'
 * 
 * [+] Keypad keys: preceded by 0x79 on press/release
 * [*] Special keypad keys: preceded by 0x71/0xF1 + 0x79 on press/release
 * [~] Duplicate keys: Both Left and Right Shift return 0x71, so we only expect Left Shift to be defined.
 * 
 * Note: Arrow keys and some keypad keys share codes:
 * Left/Pad+:   0x79,0x0D / 0x71,0x79,0x0D
 * Right/Pad*:  0x79,0x05 / 0x71,0x79,0x05  
 * Up/Pad/:     0x79,0x1B / 0x71,0x79,0x1B
 * Down/Pad=:   0x79,0x11 / 0x71,0x79,0x11
 * 
 * Both Left and Right Shift Keys return 0x71, so we don't assign a second K71 key against right shift.
 */

#define KEYMAP_M0110A( \
    K65,K25,K27,K29,K2B,K2F,K2D,K35,K39,K33,K3B,K37,K31,K67,      K2A,K32,K3A,K42, \
    K61,    K19,K1B,K1D,K1F,K23,K21,K41,K45,K3F,K47,K43,K3D,      K68,K70,K78,K22, \
    K73,    K01,K03,K05,K07,K0B,K09,K4D,K51,K4B,K53,K4F,K49,      K50,K58,K60,K4A, \
    K71,    K0D,K0F,K11,K13,K17,K5B,K5D,K57,K5F,K59,    K10,      K38,K40,K48,K1A,  \
    K75,    K6F,            K63,            K55,K20,K28,K18,          K30,K12  \
) { \
    { KC_NO,    KC_##K01, KC_NO,    KC_##K03, KC_NO,    KC_##K05, KC_NO,    KC_##K07,   /* 00-07 */ \
      KC_NO,    KC_##K09, KC_NO,    KC_##K0B, KC_NO,    KC_##K0D, KC_NO,    KC_NO },    /* 08-0F */ \
    { KC_##K10, KC_##K11, KC_##K12, KC_##K13, KC_NO,    KC_NO,    KC_NO,    KC_##K17,   /* 10-17 */ \
      KC_##K18, KC_##K19, KC_##K1A, KC_##K1B, KC_NO,    KC_##K1D, KC_NO,    KC_##K1F }, /* 18-1F */ \
    { KC_##K20, KC_##K21, KC_##K22, KC_##K23, KC_NO,    KC_##K25, KC_NO,    KC_##K27,   /* 20-27 */ \
      KC_##K28, KC_##K29, KC_##K2A, KC_##K2B, KC_NO,    KC_##K2D, KC_NO,    KC_##K2F }, /* 28-2F */ \
    { KC_##K30, KC_##K31, KC_##K32, KC_##K33, KC_NO,    KC_##K35, KC_NO,    KC_##K37,   /* 30-37 */ \
      KC_##K38, KC_##K39, KC_##K3A, KC_##K3B, KC_NO,    KC_##K3D, KC_NO,    KC_##K3F }, /* 38-3F */ \
    { KC_##K40, KC_##K41, KC_##K42, KC_##K43, KC_NO,    KC_##K45, KC_NO,    KC_##K47,   /* 40-47 */ \
      KC_##K48, KC_##K49, KC_##K4A, KC_##K4B, KC_NO,    KC_##K4D, KC_NO,    KC_##K4F }, /* 48-4F */ \
    { KC_##K50, KC_##K51, KC_NO,    KC_##K53, KC_NO,    KC_##K55, KC_NO,    KC_##K57,   /* 50-57 */ \
      KC_##K58, KC_##K59, KC_NO,    KC_##K5B, KC_NO,    KC_##K5D, KC_NO,    KC_##K5F }, /* 58-5F */ \
    { KC_##K60, KC_##K61, KC_NO,    KC_##K63, KC_NO,    KC_##K65, KC_NO,    KC_##K67,   /* 60-67 */ \
      KC_##K68, KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_##K6F }, /* 68-6F */ \
    { KC_##K70, KC_##K71, KC_NO,    KC_##K73, KC_NO,    KC_##K75, KC_NO,    KC_NO,      /* 70-77 */ \
      KC_##K78, KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO,    KC_NO },    /* 78-7F */ \
}
// clang-format on

#endif /* KEYMAP_H */
