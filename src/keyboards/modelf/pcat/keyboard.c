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

// clang-format on

/* Layer count override - matches number of layers defined in keymap_map below */
const uint8_t keymap_layer_count = 2;

/* Define Keyboard Layers */
const uint8_t keymap_map[][KEYMAP_ROWS][KEYMAP_COLS] = {
    KEYMAP_PCAT(                  /* Layer 0: Base Layer
                                   */
                // clang-format off
    F1,    F2,        GRV,   1,     2,     3,     4,     5,     6,     7,     8,     9,     0,     MINS,  EQL,   NUHS,  BSPC,      ESC,   NLCK,  SLCK,  PAUS, \
    F3,    F4,        TAB,          Q,     W,     E,     R,     T,     Y,     U,     I,     O,     P,     LBRC,  RBRC,             P7,    P8,    P9,    PAST, \
    F5,    F6,        LCTL,         A,     S,     D,     F,     G,     H,     J,     K,     L,     SCLN,  QUOT,         ENT,       P4,    P5,    P6,    PMNS, \
    F7,    F8,        LSFT,         Z,     X,     C,     V,     B,     N,     M,     COMM,  DOT,   SLSH,                RSFT,      P1,    P2,    P3,    PPLS, \
    MO_1,  LGUI,      LALT,                                            SPC,                                             CAPS,             P0,    PDOT  // clang-format on
                ),
    KEYMAP_PCAT(     /* Layer 1: Function Layer (activated by MO_1)
                      * Provides F9-F12, media controls, APP key, and NUBS.
                      * Numpad positions remapped to navigation keys for macOS users.
                      */
                // clang-format off
    F9,    F10,       NUBS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,      TRNS,  TRNS,  TRNS,  TRNS, \
    F11,   F12,       TRNS,         TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,             HOME,  UP,    PGUP,  TRNS, \
    VOLD,  VOLU,      TRNS,         TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,         TRNS,      LEFT,  NO,    RIGHT, TRNS, \
    BRTD,  BRTI,      TRNS,         TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,                TRNS,      END,   DOWN,  PGDN,  TRNS, \
    TRNS,  TRNS,      TRNS,                                            TRNS,                                            APP,              INS,   DEL  // clang-format on
                ),
};
