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

#include "keymap.h"

#include "../../hid_keycodes.h"
#include "pico/stdlib.h"

// clang-format off

/*
 * IBM Model F AT UK 84-key (UK Standard Layout - 6450225):
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
 */

/* Define Keyboard Layers */
const uint8_t __not_in_flash("keymap_map") keymap_map[][KEYMAP_ROWS][KEYMAP_COLS] = {
  KEYMAP_5170( \
    /* Base Layer (NumLock On)
     * MacOS maps keys oddly, GRAVE and NUBS are swapped over when coupled with British-PC Layout.
     * Likewise, NUHS and BSLS appear to match. TODO: Have these as a config option to swap.
     */
    F1,    F2,        GRAVE, 1,     2,     3,     4,     5,     6,     7,     8,     9,     0,     MINS,  EQL,   NUHS,  BSPC,      ESC,   NLCK,  SLCK,  PAUS, \
    F3,    F4,        TAB,          Q,     W,     E,     R,     T,     Y,     U,     I,     O,     P,     LBRC,  RBRC,  NO,        P7,    P8,    P9,    PAST, \
    F5,    F6,        LCTL,         A,     S,     D,     F,     G,     H,     J,     K,     L,     SCLN,  QUOT,  NO,    ENT,       P4,    P5,    P6,    PMNS, \
    F7,    F8,        LSFT,  NO,    Z,     X,     C,     V,     B,     N,     M,     COMM,  DOT,   SLSH,  NO,           RSFT,      P1,    P2,    P3,    PPLS, \
    FN,    LGUI,      LALT,         NO,                                SPC,                               NO,           CAPS,      NO,    P0,    PDOT,  NO    \
  ),
  KEYMAP_5170( \
    /* Numlock Off (MacOS Compatibility Layer) */
    TRNS,  TRNS,      TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,      TRNS,  TRNS,  TRNS,  TRNS, \
    TRNS,  TRNS,      TRNS,         TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,      HOME,  UP,    PGUP,  PSCR, \
    TRNS,  TRNS,      TRNS,         TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,      LEFT,  NO,    RIGHT, TRNS, \
    TRNS,  TRNS,      TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,         TRNS,      END,   DOWN,  PGDN,  TRNS, \
    TRNS,  TRNS,      TRNS,         TRNS,                              TRNS,                              TRNS,         TRNS,      TRNS,  INS,   DEL,   TRNS  \
  ),
};

/* Define Action Layers */
const uint8_t __not_in_flash("keymap_actions") keymap_actions[][KEYMAP_ROWS][KEYMAP_COLS] = {
  KEYMAP_5170( \
    /* Function Key Pressed */
    F9,    F10,       NUBS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,      TRNS,  TRNS,  TRNS,  TRNS, \
    F11,   F12,       TRNS,         TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,      NFLP,  NFLP,  NFLP,  NFLP, \
    VOLD,  VOLU,      TRNS,         TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,      NFLP,  TRNS,  NFLP,  TRNS, \
    BRTD,  BRTI,      TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,         TRNS,      NFLP,  NFLP,  NFLP,  TRNS, \
    TRNS,  TRNS,      TRNS,         TRNS,                              TRNS,                              TRNS,         APP,       TRNS,  NFLP,  NFLP,  TRNS \
  ),
};
// clang-format on
