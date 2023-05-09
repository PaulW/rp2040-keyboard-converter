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

#include <stdint.h>

#include "hid_keycodes.h"
#include "keymap.h"

// clang-format off

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

/* Define Keyboard Layers */
const uint8_t keymap_map[][KEYMAP_ROWS][KEYMAP_COLS] = {
  KEYMAP( \
    /* Base Layer (NumLock On)
     * MacOS maps keys oddly, GRAVE and NUBS are swapped over when coupled with British-PC Layout.
     * Likewise, NUHS and BSLS appear to match. TODO: Have these as a config option to swap.
     */
    F1,    F2,        NUBS,  1,     2,     3,     4,     5,     6,     7,     8,     9,     0,     MINS,  EQL,   NUHS,  BSPC,      ESC,   NLCK,  SLCK,  SYSREQ, \
    F3,    F4,        TAB,          Q,     W,     E,     R,     T,     Y,     U,     I,     O,     P,     LBRC,  RBRC,  NO,        P7,    P8,    P9,    PAST, \
    F5,    F6,        LCTL,         A,     S,     D,     F,     G,     H,     J,     K,     L,     SCLN,  QUOT,  NO,    ENT,       P4,    P5,    P6,    PMNS, \
    F7,    F8,        LSFT,  NO,    Z,     X,     C,     V,     B,     N,     M,     COMM,  DOT,   SLSH,  NO,           RSFT,      P1,    P2,    P3,    PPLS, \
    FN,    LGUI,      LALT,         NO,                                SPC,                               NO,           CAPS,      NO,    P0,    PDOT,  NO    \
  ),
  KEYMAP( \
    /* Numlock Off (MacOS Compatibility Layer) */
    TRNS,  TRNS,      TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,      TRNS,  TRNS,  TRNS,  TRNS, \
    TRNS,  TRNS,      TRNS,         TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,      HOME,  UP,    PGUP,  TRNS, \
    TRNS,  TRNS,      TRNS,         TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,      LEFT,  NO,    RIGHT, TRNS, \
    TRNS,  TRNS,      TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,         TRNS,      END,   DOWN,  PGDN,  TRNS, \
    TRNS,  TRNS,      TRNS,         TRNS,                              TRNS,                              TRNS,         TRNS,      TRNS,  INS,   DEL,   TRNS  \
  ),
};

/* Define Action Layers */
const uint8_t keymap_actions[][KEYMAP_ROWS][KEYMAP_COLS] = {
  KEYMAP( \
    /* Function Key Pressed */
    F9,    F10,       TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,      TRNS,  TRNS,  TRNS,  TRNS, \
    F11,   F12,       TRNS,         TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,      NFLP,  NFLP,  NFLP,  PSCR, \
    VOLD,  VOLU,      TRNS,         TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,      NFLP,  TRNS,  NFLP,  TRNS, \
    BRTD,  BRTI,      TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,         TRNS,      NFLP,  NFLP,  NFLP,  TRNS, \
    TRNS,  TRNS,      TRNS,         TRNS,                              TRNS,                              TRNS,         TRNS,      TRNS,  NFLP,  NFLP,  TRNS \
  ),
};

// clang-format on
