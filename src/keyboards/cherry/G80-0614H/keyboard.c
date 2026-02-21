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

/* Cherry G80 (ISO Layout - 1104H):
 * This Keyboard was used on the BT Cheetah Plus.
 * Key legends are unique, however for the most part the keycodes are the same as the IBM 101 Key.
 * ,-------.  ,--------------------------------------------------------------------------.
 * | F1| F2|  |Esc|  1|  2|  3|  4|  5|  6|  7|  8|  9|  0|  -|  =|  BS  |NumLck |ScrLck |
 * |-------|  |--------------------------------------------------------------------------|
 * | F3| F4|  | Tab |  Q|  W|  E|  R|  T|  Y|  U|  I|  O|  P|  [|  ] |   |  7|  8|  9|  -|
 * |-------|  |------------------------------------------------------|Ent|---------------|
 * | F5| F6|  | Ctrl |  A|  S|  D|  F|  G|  H|  J|  K|  L|  ;|  '|  `|   |  4|  5|  6|   |
 * |-------|  |----------------------------------------------------------------------|   |
 * | F7| F8|  |Shif|  \|  Z|  X|  C|  V|  B|  N|  M|  ,|  .|  /|Shift|  *|  1|  2|  3|  +|
 * |-------|  |----------------------------------------------------------------------|   |
 * | F9|F10|  |  Alt  |               Space                  |CapsLck|   0   |   .   |   |
 * `-------'  `--------------------------------------------------------------------------'
 */

// clang-format on

/* Define Keyboard Layers */
const uint8_t keymap_map[][KEYMAP_ROWS][KEYMAP_COLS] = {
    KEYMAP_XT(              /* Layer 0: Base Layer
                             */
              // clang-format off
    F1,    F2,        ESC,   1,     2,     3,     4,     5,     6,     7,     8,     9,     0,     MINS,  EQL,   BSPC,  NLCK,         SLCK, \
    F3,    F4,        TAB,   Q,     W,     E,     R,     T,     Y,     U,     I,     O,     P,     LBRC,  RBRC,  ENT,   P7,    P8,    P9,    PMNS, \
    F5,    F6,        LCTL,  A,     S,     D,     F,     G,     H,     J,     K,     L,     SCLN,  QUOT,  BSLS,         P4,    P5,    P6,          \
    F7,    F8,        LSFT,  NUBS,  Z,     X,     C,     V,     B,     N,     M,     COMM,  DOT,   SLSH,  RSFT,  PSCR,  P1,    P2,    P3,    PPLS, \
    MO_1,  LGUI,      LALT,                                     SPC,                                      CAPS,         P0,           PDOT  // clang-format on
              ),
    KEYMAP_XT(      /* Layer 1: Function Layer (activated by MO_1)
                     * Provides F9-F12, media controls, arrow key navigation, and application key
                     */
              // clang-format off
    F9,    F10,       TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,         TRNS, \
    F11,   F12,       TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS, \
    VOLD,  VOLU,      TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,         TRNS,  UP,    TRNS, \
    BRTD,  BRTI,      TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  LEFT,  DOWN,  RIGHT, TRNS, \
    TRNS,  TRNS,      TRNS,                                     TRNS,                                     APP,          TRNS,         TRNS  // clang-format on
              ),
};

/* Layer count - automatically calculated from keymap_map array size */
const uint8_t keymap_layer_count = sizeof(keymap_map) / sizeof(keymap_map[0]);
