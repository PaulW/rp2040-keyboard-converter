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

/* Apple M0110A Base Layer
 * ,-------------------------------------------------------- . ,---------------.
 * |  `|  1|  2|  3|  4|  5|  6|  7|  8|  9|  0|  -|  =|   BS| |Clr|  =|  /|  *|
 * |---------------------------------------------------------| |---------------|
 * |Tab  |  Q|  W|  E|  R|  T|  Y|  U|  I|  O|  P|  [|  ]|   | |  7|  8|  9|  -|
 * |----------------------------------------------------'    | |---------------|
 * |CapsL |  A|  S|  D|  F|  G|  H|  J|  K|  L|  ;|  '|   Ret| |  4|  5|  6|  +|
 * |---------------------------------------------------------| |---------------|
 * |Shift   |  Z|  X|  C|  V|  B|  N|  M|  ,|  .|  /|*Shf| Up| |  1|  2|  3|   |
 * |--------------------------------------- -----------------| |-----------|Ent|
 * | Optn|    Mac|           Space           |  \|Lft|Rig|Dwn| |      0|  .|   |
 * `---------------------------------------------------------' `---------------'
 * 
 * *Shf: Both Shift Keys on the M0110A return the same raw keycode, so we only
 *       define the association from the LEFT Shift position.
 */

// clang-format on

/* Layer count override - matches number of layers defined in keymap_map below */
const uint8_t keymap_layer_count = 1;

// Define Keyboard Layers
const uint8_t keymap_map[][KEYMAP_ROWS][KEYMAP_COLS] = {
    KEYMAP_M0110A(      /* Layer 0: Base Layer
                         */
                  // clang-format off
    GRV,   1,     2,     3,     4,     5,     6,     7,     8,     9,     0,  MINS,   EQL,  BSPC,    NLCK,  PEQL,  PSLS,   PAST, \
    TAB,          Q,     W,     E,     R,     T,     Y,     U,     I,     O,     P,  LBRC,  RBRC,      P7,    P8,    P9,   PMNS, \
    CAPS,         A,     S,     D,     F,     G,     H,     J,     K,     L,  SCLN,  QUOT,   ENT,      P4,    P5,    P6,   PPLS, \
    LSFT,         Z,     X,     C,     V,     B,     N,     M,  COMM,   DOT,  SLSH,           UP,      P1,    P2,    P3,   PENT, \
    LALT,      LGUI,                        SPC,                       BSLS,  LEFT, RIGHT,  DOWN,             P0,  PDOT  // clang-format on
                  ),
};

/*
 * Key mapping notes for Apple M0110A:
 *
 * Standard keys map directly to their USB HID equivalents.
 *
 * Special Apple keys:
 * - Option key (0x75) -> LALT (Left Alt)
 * - Mac key (0x6F) -> LGUI (Left GUI/Command)
 * - Clear key (0x0F with 0x71 prefix) -> CALC (Calculator)
 *
 * Keypad handling:
 * - Regular keypad keys send 0x79 prefix before their code
 * - Special keypad keys (Clear, =, /, *) send 0x71 + 0x79 prefix
 * - Arrow keys conflict with some keypad operations and need special handling in scancode.c
 *
 * Layout differences from modern keyboards:
 * - No Windows/Super key (Mac key serves this purpose)
 * - No Menu/Application key
 * - No separate numeric keypad Enter (shares main Enter)
 * - Unique Clear key on keypad
 * - Different modifier key placement (Option where Alt would be)
 *
 * Command Mode Override:
 * This keyboard has only one physical shift key (both physical keys return the
 * same scancode 0x71), so the default Command Mode activation (Left Shift +
 * Right Shift) is impossible. Instead, Command Mode uses Shift + Option (Alt).
 * See keyboard.h for the CMD_MODE_KEY1/KEY2 definitions.
 */
