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

/* Commodore Amiga 500/2000 Keyboard Layout
 * 
 * ,---. ,------------------------. ,------------------------.
 * |Esc| |F1  |F2  |F3  |F4  |F5  | |F6  |F7  |F8  |F9  |F10 |
 * `---' `------------------------' `------------------------'
 * ,-------------------------------------------------------------. ,-----------. ,---------------.
 * |  `  |  1|  2|  3|  4|  5|  6|  7|  8|  9|  0|  -|  =|  \|Bsp| |Del  |Help | |  (|  )|  /|  *|
 * |-------------------------------------------------------------| `-----------' |---------------|
 * |  Tab  |  Q|  W|  E|  R|  T|  Y|  U|  I|  O|  P|  [|  ]|     |               |  7|  8|  9|  -|
 * |--------------------------------------------------------\Entr|     ,---.     |---------------|
 * |Ctrl|Cap|  A|  S|  D|  F|  G|  H|  J|  K|  L|  ;|  '|  #|    |     |Up |     |  4|  5|  6|  +|
 * |-------------------------------------------------------------| ,-----------. |---------------|
 * |Shift |  <|  Z|  X|  C|  V|  B|  N|  M|  ,|  .|  /|  Shift   | |Lef|Dow|Rig| |  1|  2|  3|   |
 * `-------------------------------------------------------------' `-----------' |-----------|Ent|
 *     |Alt |A   |            Space                  |A   |Alt |                 |      0|  .|   |
 *     `-------------------------------------------------------'                 `---------------'
 */

// clang-format on

/* Layer count override - matches number of layers defined in keymap_map below */
const uint8_t keymap_layer_count = 1;

/* Define Keyboard Layers */
const uint8_t keymap_map[][KEYMAP_ROWS][KEYMAP_COLS] = {
    KEYMAP_AMIGA(    /* Base Layer */
                 // clang-format off
        ESC,          F1,    F2,    F3,    F4,    F5,       F6,    F7,    F8,    F9,    F10, \
        GRV,   1,     2,     3,     4,     5,     6,     7,     8,     9,     0,     MINS,  EQL,   BSLS,  BSPC,     DEL,   INS,      PAST,  PPLS,  PSLS,  PMNS, \
        TAB,          Q,     W,     E,     R,     T,     Y,     U,     I,     O,     P,     LBRC,  RBRC,  ENT,                        P7,    P8,    P9,    PMNS, \
        LCTL,  CAPS,  A,     S,     D,     F,     G,     H,     J,     K,     L,     SCLN,  QUOT,  NUHS,                    UP,       P4,    P5,    P6,    PPLS, \
        LSFT,         NUBS,  Z,     X,     C,     V,     B,     N,     M,     COMM,  DOT,   SLSH,         RSFT,     LEFT,  DOWN,  RGHT,     P1,    P2,    P3,    PENT, \
                      LALT,  LGUI,                    SPC,                           RGUI,  RALT,                                           PDOT,  P0  // clang-format on
                 ),
};

/*
 * Key mapping notes for Commodore Amiga 500/2000:
 *
 * Standard Keys:
 * - Most keys map directly to USB HID equivalents
 * - Backslash position (0x0D) acts as both backslash and pipe in US layout
 * - UK/European layouts have # key (0x2B) where backslash would be
 * - < > key (0x30) between Left Shift and Z on European keyboards maps to NUBS (Non-US Backslash)
 *
 * Amiga-Specific Keys:
 * - Left Amiga key (0x66) -> LGUI (Left Windows/Command key)
 * - Right Amiga key (0x67) -> RGUI (Right Windows/Command key)
 * - Help key (0x5F) -> INS (Insert, as Help is similar function)
 *
 * Keypad Special Keys:
 * - Keypad ( -> PAST (Multiply) - alternative mapping, as modern keyboards lack ( key
 * - Keypad ) -> PPLS (Plus) - alternative mapping, as modern keyboards lack ) key
 * - Keypad / -> PSLS (Divide)
 * - Keypad * -> PMNS (Minus)
 * - Note: Scancodes 0x5A-0x5D don't map 1:1 to modern layouts, mapped for usability
 *
 * Arrow Keys:
 * - Positioned between main keyboard and numeric keypad (classic Amiga position)
 * - Up (0x4C), Down (0x4D), Left (0x4F), Right (0x4E)
 *
 * Numeric Keypad:
 * - Full numeric keypad with Enter (0x43)
 * - Keypad 0 (0x0F) is wide key like modern keyboards
 * - Comma key (0x3C) mapped to PDOT as modern layouts don't have keypad comma
 * - Note: Some mappings adjusted for modern USB HID compatibility
 *
 * Control Keys:
 * - Ctrl (0x63) -> LCTL (Left Control)
 * - Caps Lock (0x62) -> CAPS
 * - Left Shift (0x60), Right Shift (0x61)
 * - Alt (0x64, 0x65) -> LALT, RALT
 *
 * Function Keys:
 * - F1-F10 (0x50-0x59) in row above number row
 * - No F11/F12 on original Amiga keyboards
 *
 * Special Protocol Considerations:
 * - All scancodes are 7-bit (0x00-0x67)
 * - Bit 7 indicates key up (1) or key down (0)
 * - Special codes (0x78, 0xF9-0xFE) handled by protocol layer, not here
 * - Reset sequence (Ctrl + Left Amiga + Right Amiga) generates 0x78 before individual releases
 *
 * Layout Variations:
 * This is a US-based mapping. European Amiga keyboards had:
 * - Different # key position
 * - < > key between Left Shift and Z
 * - Some different punctuation key positions
 *
 * The NUBS (Non-US Backslash) and NUHS (Non-US Hash) mappings accommodate these variations.
 */
