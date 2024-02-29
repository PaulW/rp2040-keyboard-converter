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

#include "keyboard.h"

#include "hid_keycodes.h"

// clang-format off

/* IBM Model M 101-key (US ANSI Standard Layout - 1390131):
 * ,---.   ,---------------. ,---------------. ,---------------. ,-----------.
 * |Esc|   |F1 |F2 |F3 |F4 | |F5 |F6 |F7 |F8 | |F9 |F10|F11|F12| |PrS|ScL|Pau|
 * `---'   `---------------' `---------------' `---------------' `-----------'
 * ,-----------------------------------------------------------. ,-----------. ,---------------.
 * |  `|  1|  2|  3|  4|  5|  6|  7|  8|  9|  0|  -|  =|     BS| |Ins|Hom|PgU| |NmL|  /|  *|  -|
 * |-----------------------------------------------------------| |-----------| |---------------|
 * |Tab  |  Q|  W|  E|  R|  T|  Y|  U|  I|  O|  P|  [|  ]|    \| |Del|End|PgD| |  7|  8|  9|   |
 * |-----------------------------------------------------------| `-----------' |-----------|  +|
 * |CapsL |  A|  S|  D|  F|  G|  H|  J|  K|  L|  ;|  '|     Ret|               |  4|  5|  6|   |
 * |-----------------------------------------------------------|     ,---.     |---------------|
 * |Shift   |  Z|  X|  C|  V|  B|  N|  M|  ,|  ,|  /|     Shift|     |Up |     |  1|  2|  3|   |
 * |-----------------------------------------------------------| ,-----------. |-----------|Ent|
 * |Ctrl|    |Alt |          Space              |Alt |    |Ctrl| |Lef|Dow|Rig| |      0|  .|   |
 * `----'    `---------------------------------------'    `----' `-----------' `---------------'
 */

/* Define Keyboard Layers */
const uint8_t keymap_map[][KEYMAP_ROWS][KEYMAP_COLS] = {
  KEYMAP_ANSI( \
    /* Base Layer (NumLock On)
     * MacOS maps keys oddly, GRAVE and NUBS are swapped over when coupled with British-PC Layout.
     * Likewise, NUHS and BSLS appear to match. TODO: Have these as a config option to swap.
     */
    ESC,          F1,    F2,    F3,    F4,       F5,    F6,    F7,    F8,        F9,    F10,   F11,   F12,      PSCR,  SLCK,  PAUS, \
    GRV,   1,     2,     3,     4,     5,     6,     7,     8,     9,     0,     MINS,  EQL,          BSPC,     INS,   HOME,  PGUP,     NLCK,  PSLS,  PAST,  PMNS, \
    TAB,          Q,     W,     E,     R,     T,     Y,     U,     I,     O,     P,     LBRC,  RBRC,  BSLS,     DEL,   END,   PGDN,     P7,    P8,    P9,    PPLS, \
    CAPS,         A,     S,     D,     F,     G,     H,     J,     K,     L,     SCLN,  QUOT,         ENT,                              P4,    P5,    P6, \
    LSFT,         Z,     X,     C,     V,     B,     N,     M,     COMM,  DOT,   SLSH,                RSFT,            UP,              P1,    P2,    P3,    PENT, \
    LCTL,         LALT,                     SPC,                                 FN,                  LGUI,     LEFT,  DOWN,  RIGHT,           P0,    PDOT \
  ),
  KEYMAP_ANSI( \
    /* Numlock Off (MacOS Compatibility Layer) */
    TRNS,         TRNS,  TRNS,  TRNS,  TRNS,     TRNS,  TRNS,  TRNS,  TRNS,      TRNS,  TRNS,  TRNS,  TRNS,     TRNS,  TRNS,  TRNS, \
    TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,         TRNS,     TRNS,  TRNS,  TRNS,     TRNS,  TRNS,  TRNS,  TRNS, \
    TRNS,         TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,     TRNS,  TRNS,  TRNS,     TRNS,  TRNS,  TRNS,  TRNS, \
    TRNS,         TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,         TRNS,                             TRNS,  TRNS,  TRNS, \
    TRNS,         TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,                TRNS,            TRNS,            TRNS,  TRNS,  TRNS,  TRNS, \
    TRNS,         TRNS,                     TRNS,                                TRNS,                TRNS,     TRNS,  TRNS,  TRNS,            TRNS,  TRNS \
  ),
};

/* Define Action Layers */
const uint8_t keymap_actions[][KEYMAP_ROWS][KEYMAP_COLS] = {
  KEYMAP_ANSI( \
    /* Function Key Pressed */
    TRNS,         VOLD,  VOLU,  BRTD,  BRTI,     TRNS,  TRNS,  TRNS,  TRNS,      TRNS,  TRNS,  TRNS,  TRNS,     TRNS,  TRNS,  TRNS, \
    TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,         TRNS,     TRNS,  TRNS,  TRNS,     TRNS,  TRNS,  TRNS,  TRNS, \
    TRNS,         TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,     TRNS,  TRNS,  TRNS,     TRNS,  TRNS,  TRNS,  TRNS, \
    APP,          TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,         TRNS,                             TRNS,  TRNS,  TRNS, \
    TRNS,         TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,                TRNS,            TRNS,            TRNS,  TRNS,  TRNS,  TRNS, \
    TRNS,         TRNS,                     TRNS,                                TRNS,                TRNS,     TRNS,  TRNS,  TRNS,            TRNS,  TRNS \
  ),
};
// clang-format on
