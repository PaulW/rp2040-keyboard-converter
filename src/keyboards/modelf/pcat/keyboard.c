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
/* Define Keyboard Layers */
const uint8_t keymap_map[][KEYMAP_ROWS][KEYMAP_COLS] = {
    /* We define 2 initial maps for the Base layer, these in turn define layers 0 and 1.
     * Layer 0 is the default base layer with all associated mappings.  This also encompasses the
     * NumLock On state. Layer 1 is the NumLock Off state, and only changes the state of keys
     * assicated with this state.  Exisiting keys are not remapped, and are instead left as TRNS.
     */
    KEYMAP_PCAT(                  /* Base Layer 0 (+NumLock On)
                                   * MacOS maps keys oddly, GRAVE and NUBS are swapped over when coupled with
                                   * British-PC Layout.                   Likewise, NUHS and BSLS appear to match. TODO: Have these as a
                                   * config option to swap.
                                   */
                // clang-format off
    F1,    F2,        GRV,   1,     2,     3,     4,     5,     6,     7,     8,     9,     0,     MINS,  EQL,   NUHS,  BSPC,      ESC,   NLCK,  SLCK,  PAUS, \
    F3,    F4,        TAB,          Q,     W,     E,     R,     T,     Y,     U,     I,     O,     P,     LBRC,  RBRC,             P7,    P8,    P9,    PAST, \
    F5,    F6,        LCTL,         A,     S,     D,     F,     G,     H,     J,     K,     L,     SCLN,  QUOT,         ENT,       P4,    P5,    P6,    PMNS, \
    F7,    F8,        LSFT,         Z,     X,     C,     V,     B,     N,     M,     COMM,  DOT,   SLSH,                RSFT,      P1,    P2,    P3,    PPLS, \
    MO_1,  LGUI,      LALT,                                            SPC,                                             CAPS,             P0,    PDOT  // clang-format on
                ),
    KEYMAP_PCAT(     /* Base Layer 1 (+Numlock Off)
                      * Any keys which state does not change are mapped to TRNS, which in turn causes
                      * Layer 0 to be referenced for that specific key value.
                      */
                // clang-format off
    TRNS,  TRNS,      TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,      TRNS,  TRNS,  TRNS,  TRNS, \
    TRNS,  TRNS,      TRNS,         TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,             HOME,  UP,    PGUP,  TRNS, \
    TRNS,  TRNS,      TRNS,         TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,         TRNS,      LEFT,  NO,    RIGHT, TRNS, \
    TRNS,  TRNS,      TRNS,         TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,                TRNS,      END,   DOWN,  PGDN,  TRNS, \
    TRNS,  TRNS,      TRNS,                                            TRNS,                                            TRNS,             INS,   DEL  // clang-format on
                ),
  KEYMAP_PCAT( \
    /* Layer 1: Function Layer (activated by MO(1) - formerly KC_FN)
     * Provides F9-F12, media controls, numpad flip, and application key
     */
    // clang-format off
    F9,    F10,       NUBS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,      TRNS,  TRNS,  TRNS,  TRNS, \
    F11,   F12,       TRNS,         TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,             NFLP,  NFLP,  NFLP,  NFLP, \
    VOLD,  VOLU,      TRNS,         TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,         TRNS,      NFLP,  TRNS,  NFLP,  TRNS, \
    BRTD,  BRTI,      TRNS,         TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,                TRNS,      NFLP,  NFLP,  NFLP,  TRNS, \
    TRNS,  TRNS,      TRNS,                                            TRNS,                                            APP,              NFLP,  NFLP  // clang-format on
                ),
};
