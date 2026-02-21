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

#include "keyboard.h"

#include "keymaps.h"

// clang-format off
/* MicroSwitch 122ST13 Terminal Keyboard:
 *               ,-----------------------------------------------.
 *               |F13|F14|F15|F16|F17|F18|F19|F20|F21|F22|F23|F24|
 *               |-----------------------------------------------|
 *               |F1 |F2 |F3 |F4 |F5 |F6 |F7 |F8 |F9 |F10|F11|F12|
 *               `-----------------------------------------------'
 * ,-------. ,-----------------------------------------------------------. ,-----------. ,---------------.
 * |VDn|VUp| |  `|  1|  2|  3|  4|  5|  6|  7|  8|  9|  0|  -|  =|     BS| |  /|PgU|PgD| |Esc|NmL|ScL|  *|
 * |-------| |-----------------------------------------------------------| |-----------| |---------------|
 * |BDn|BUp| |Tab  |  Q|  W|  E|  R|  T|  Y|  U|  I|  O|  P|  [|  ]|     | |End|Ins|Del| |  7|  8|  9|  +|
 * |-------| |------------------------------------------------------.    | `-----------' |-----------|---|
 * |   |   | |CapsL |  A|  S|  D|  F|  G|  H|  J|  K|  L|  ;|  '|  #| Ret|     |Up |     |  4|  5|  6|  -|
 * |-------| |-----------------------------------------------------------| ,-----------. |---------------|
 * |   |   | |Shft|  \|  Z|  X|  C|  V|  B|  N|  M|  ,|  ,|  /|     Shift| |Lef|Hom|Rig| |  1|  2|  3|   |
 * |-------| |-----------------------------------------------------------| `-----------' |-----------|Ent|
 * |App|Gui| |Ctrl|    | Alt |           Space           | Alt |    |Ctrl|     |Dow|     |      0|  .|   |
 * `-------' `----'    `---------------------------------------'    `----'     `---'     `---------------'
 */

// clang-format on

/* Define Keyboard Layers */
const uint8_t keymap_map[][KEYMAP_ROWS][KEYMAP_COLS] = {
    KEYMAP_PC122(                        /* Layer 0: Base Layer
                                          */
                 // clang-format off
                            F13,   F14,   F15,   F16,   F17,   F18,   F19,   F20,   F21,   F22,   F23,   F24, \
                            F1,    F2,    F3,    F4,    F5,    F6,    F7,    F8,    F9,    F10,   F11,   F12, \
    ESC,   NO,       GRV,   1,     2,     3,     4,     5,     6,     7,     8,     9,     0,     MINS,  EQL,          BSPC,     INS,   HOME,  PGUP,     NLCK,  PSLS,  PAST,  PMNS, \
    VOLD,  VOLU,     TAB,          Q,     W,     E,     R,     T,     Y,     U,     I,     O,     P,     LBRC,  RBRC,            DEL,   END,   PGDN,     P7,    P8,    P9,    PPLS, \
    BRTD,  BRTI,     CAPS,         A,     S,     D,     F,     G,     H,     J,     K,     L,     SCLN,  QUOT,  BSLS,  ENT,             UP,              P4,    P5,    P6,    PMNS, \
    NO,    APP,      LSFT,  NUBS,  Z,     X,     C,     V,     B,     N,     M,     COMM,  DOT,   SLSH,                RSFT,     LEFT,  NO,    RIGHT,    P1,    P2,    P3,    PENT, \
    LGUI,  RGUI,     LCTL,         LALT,                              SPC,                               RALT,         RCTL,            DOWN,            P0,    PDOT  // clang-format on
                 ),
};

/* Layer count - automatically calculated from keymap_map array size */
const uint8_t keymap_layer_count = sizeof(keymap_map) / sizeof(keymap_map[0]);
