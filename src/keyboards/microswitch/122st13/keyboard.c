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

/*
 * MicroSwitch 122ST13 Terminal Keyboard:
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

/* Define Keyboard Layers */
const uint8_t keymap_map[][KEYMAP_ROWS][KEYMAP_COLS] = {
  /* We define 2 initial maps for the Base layer, these in turn define layers 0 and 1.
   * Layer 0 is the default base layer with all associated mappings.  This also encompasses the NumLock On state.
   * Layer 1 is the NumLock Off state, and only changes the state of keys assicated with this state.  Exisiting keys
   * are not remapped, and are instead left as TRNS.
   */
  KEYMAP_PC122( \
    /* Base Layer 0 (+NumLock On)
     * MacOS maps keys oddly, GRAVE and NUBS are swapped over when coupled with British-PC Layout.
     * Likewise, NUHS and BSLS appear to match. TODO: Have these as a config option to swap.
     */
                            F13,   F14,   F15,   F16,   F17,   F18,   F19,   F20,   F21,   F22,   F23,   F24, \
                            F1,    F2,    F3,    F4,    F5,    F6,    F7,    F8,    F9,    F10,   F11,   F12, \
    ESC,   NO,       GRV,   1,     2,     3,     4,     5,     6,     7,     8,     9,     0,     MINS,  EQL,          BSPC,     INS,   HOME,  PGUP,     NLCK,  PSLS,  PAST,  PMNS, \
    VOLD,  VOLU,     TAB,          Q,     W,     E,     R,     T,     Y,     U,     I,     O,     P,     LBRC,  RBRC,            DEL,   END,   PGDN,     P7,    P8,    P9,    PPLS, \
    BRTD,  BRTI,     CAPS,         A,     S,     D,     F,     G,     H,     J,     K,     L,     SCLN,  QUOT,  BSLS,  ENT,             UP,              P4,    P5,    P6,    PMNS, \
    FN,    APP,      LSFT,  NUBS,  Z,     X,     C,     V,     B,     N,     M,     COMM,  DOT,   SLSH,                RSFT,     LEFT,  NO,    RIGHT,    P1,    P2,    P3,    PENT, \
    LGUI,  RGUI,     LCTL,         LALT,                              SPC,                               RALT,         RCTL,            DOWN,            P0,    PDOT \
  ),
  KEYMAP_PC122( \
    /* Base Layer 1 (+Numlock Off)
     * Any keys which state does not change are mapped to TRNS, which in turn causes Layer 0 to be referenced for that specific key value.
     */
                            TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS, \
                            TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS, \
    TRNS,  TRNS,     TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,         TRNS,     TRNS,  TRNS,  TRNS,     TRNS,  TRNS,  TRNS,  TRNS,  \
    TRNS,  TRNS,     TRNS,         TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,            TRNS,  TRNS,  TRNS,     TRNS,  TRNS,  TRNS,  TRNS,  \
    TRNS,  TRNS,     TRNS,         TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,            TRNS,            TRNS,  TRNS,  TRNS,  TRNS,  \
    TRNS,  TRNS,     TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,                TRNS,     TRNS,  TRNS,  TRNS,     TRNS,  TRNS,  TRNS,  TRNS,  \
    TRNS,  TRNS,     TRNS,         TRNS,                              TRNS,                              TRNS,         TRNS,            TRNS,            TRNS,  TRNS \
  ),
};

/* Define Action Layers */
const uint8_t keymap_actions[][KEYMAP_ROWS][KEYMAP_COLS] = {
  KEYMAP_PC122( \
    /* Function Key Pressed */
                            TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS, \
                            TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS, \
    TRNS,  TRNS,     TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,         TRNS,     TRNS,  TRNS,  TRNS,     TRNS,  TRNS,  TRNS,  TRNS,  \
    TRNS,  TRNS,     TRNS,         TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,            TRNS,  TRNS,  TRNS,     TRNS,  TRNS,  TRNS,  TRNS,  \
    TRNS,  TRNS,     TRNS,         TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,            TRNS,            TRNS,  TRNS,  TRNS,  TRNS,  \
    TRNS,  TRNS,     TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,                TRNS,     TRNS,  TRNS,  TRNS,     TRNS,  TRNS,  TRNS,  TRNS,  \
    TRNS,  TRNS,     TRNS,         TRNS,                              TRNS,                              TRNS,         TRNS,            TRNS,            TRNS,  TRNS \
  ),
};
// clang-format on
