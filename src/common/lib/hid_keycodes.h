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

#ifndef HID_KEYCODES_H
#define HID_KEYCODES_H

/* Define specific Key Types */
#define IS_KEY(code) (KC_A <= (code) && (code) <= KC_EXSEL)
#define IS_MOD(code) (KC_LCTRL <= (code) && (code) <= KC_RGUI)

#define IS_SPECIAL(code) ((0xA5 <= (code) && (code) <= 0xDF) || (0xE8 <= (code) && (code) <= 0xFF))
#define IS_SYSTEM(code) (KC_PWR <= (code) && (code) <= KC_WAKE)
#define IS_CONSUMER(code) (KC_MPLY <= (code) && (code) <= KC_BRTD)

/* Define Super Macro Toggle */
#define SUPER_MACRO_INIT(code)                                         \
  (((code) & ((1 << (KC_LSHIFT & 0x7)) | (1 << (KC_RSHIFT & 0x7)))) == \
   ((1 << (KC_LSHIFT & 0x7)) | (1 << (KC_RSHIFT & 0x7))))

/*
 * Short names for ease of definition of keymap
 */
#define KC_LCTL KC_LCTRL
#define KC_RCTL KC_RCTRL
#define KC_LSFT KC_LSHIFT
#define KC_RSFT KC_RSHIFT
#define KC_ESC KC_ESCAPE
#define KC_BSPC KC_BSPACE
#define KC_ENT KC_ENTER
#define KC_DEL KC_DELETE
#define KC_INS KC_INSERT
#define KC_CAPS KC_CAPSLOCK
#define KC_CLCK KC_CAPSLOCK
#define KC_RGHT KC_RIGHT
#define KC_PGDN KC_PGDOWN
#define KC_PSCR KC_PSCREEN
#define KC_SLCK KC_SCROLLLOCK
#define KC_PAUS KC_PAUSE
#define KC_BRK KC_PAUSE
#define KC_NLCK KC_NUMLOCK
#define KC_SPC KC_SPACE
#define KC_MINS KC_MINUS
#define KC_EQL KC_EQUAL
#define KC_GRV KC_GRAVE
#define KC_RBRC KC_RBRACKET
#define KC_LBRC KC_LBRACKET
#define KC_COMM KC_COMMA
#define KC_BSLS KC_BSLASH
#define KC_SLSH KC_SLASH
#define KC_SCLN KC_SCOLON
#define KC_QUOT KC_QUOTE
#define KC_APP KC_APPLICATION
#define KC_NUHS KC_NONUS_HASH
#define KC_NUBS KC_NONUS_BSLASH
#define KC_LCAP KC_LOCKING_CAPS
#define KC_LNUM KC_LOCKING_NUM
#define KC_LSCR KC_LOCKING_SCROLL
#define KC_ERAS KC_ALT_ERASE
#define KC_CLR KC_CLEAR
/* Japanese specific */
#define KC_ZKHK KC_GRAVE
#define KC_RO KC_INT1
#define KC_KANA KC_INT2
#define KC_JYEN KC_INT3
#define KC_JPY KC_INT3
#define KC_HENK KC_INT4
#define KC_MHEN KC_INT5
#define KC_MACJ KC_LANG1
#define KC_MACE KC_LANG2
/* Korean specific */
#define KC_HAEN KC_LANG1
#define KC_HANJ KC_LANG2
/* Keypad */
#define KC_P1 KC_KP_1
#define KC_P2 KC_KP_2
#define KC_P3 KC_KP_3
#define KC_P4 KC_KP_4
#define KC_P5 KC_KP_5
#define KC_P6 KC_KP_6
#define KC_P7 KC_KP_7
#define KC_P8 KC_KP_8
#define KC_P9 KC_KP_9
#define KC_P0 KC_KP_0
#define KC_PDOT KC_KP_DOT
#define KC_PCMM KC_KP_COMMA
#define KC_PSLS KC_KP_SLASH
#define KC_PAST KC_KP_ASTERISK
#define KC_PMNS KC_KP_MINUS
#define KC_PPLS KC_KP_PLUS
#define KC_PEQL KC_KP_EQUAL
#define KC_PENT KC_KP_ENTER
/* Unix function key */
#define KC_EXEC KC_EXECUTE
#define KC_SLCT KC_SELECT
#define KC_AGIN KC_AGAIN
#define KC_PSTE KC_PASTE
/* Sytem Control */
#define KC_PWR KC_SYSTEM_POWER
#define KC_SLEP KC_SYSTEM_SLEEP
#define KC_WAKE KC_SYSTEM_WAKE
/* Consumer Page */
#define KC_MPLY KC_MEDIA_PLAY
#define KC_MPAS KC_MEDIA_PAUSE
#define KC_MREC KC_MEDIA_RECORD
#define KC_MFFD KC_MEDIA_FAST_FORWARD
#define KC_MRWD KC_MEDIA_REWIND
#define KC_MNXT KC_MEDIA_NEXT_TRACK
#define KC_MPRV KC_MEDIA_PREV_TRACK
#define KC_MSTP KC_MEDIA_STOP
#define KC_EJCT KC_MEDIA_EJECT
#define KC_STEJ KC_MEDIA_STOP_EJECT
#define KC_PLPS KC_MEDIA_PLAY_PAUSE
#define KC_MUTE KC_AUDIO_MUTE
#define KC_VOLU KC_AUDIO_VOL_UP
#define KC_VOLD KC_AUDIO_VOL_DOWN
/* App Launch */
#define KC_CCNF KC_APPLAUNCH_CC_CONFIG
#define KC_MAIL KC_APPLAUNCH_EMAIL
#define KC_CALC KC_APPLAUNCH_CALCULATOR
#define KC_LBRS KC_APPLAUNCH_LOCAL_BROWSER
#define KC_BRWS KC_APPLAUNCH_BROWSER
#define KC_LOCK KC_APPLAUNCH_LOCK
#define KC_CRUN KC_APPLAUNCH_COMMAND_RUN
#define KC_MYCM KC_APPLAUNCH_FILE_EXPLORER
/* App Control */
#define KC_MXMZ KC_APPCONTROL_MAXIMIZE
#define KC_MNMZ KC_APPCONTROL_MINIMIZE
#define KC_WSCH KC_APPCONTROL_SEARCH
#define KC_WHME KC_APPCONTROL_HOME
#define KC_WBAK KC_APPCONTROL_BACK
#define KC_WFWD KC_APPCONTROL_FORWARD
#define KC_WSTP KC_APPCONTROL_STOP
#define KC_WREF KC_APPCONTROL_REFRESH
#define KC_WBKM KC_APPCONTROL_BOOKMARKS
/* Display Brightness Controls */
#define KC_BRTI KC_BRIGHTNESS_INC
#define KC_BRTD KC_BRIGHTNESS_DEC
/* Jump to bootloader */
#define KC_BTLD KC_BOOTLOADER
/* Transparent */
#define KC_TRANSPARENT 0xD1
#define KC_TRNS KC_TRANSPARENT
/* Numpad Flip */
#define KC_NUMPAD_FLIP 0xD2
#define KC_NFLP KC_NUMPAD_FLIP
/* Function Key */
#define KC_FUNCTION_KEY 0xD3
#define KC_FN KC_FUNCTION_KEY
/* Special Macro Keys */
#define KC_SPECIAL_BOOT 0xD4
#define KC_BOOT KC_SPECIAL_BOOT

/* HID Usage Tables */
/* HID Generic Desktop Usage Page (0x01) */
enum hid_generic_desktop_usage_page {
  SYSTEM_POWER_DOWN = 0x0081,  // 0x0081 /* System Power Down */
  SYSTEM_SLEEP,                // 0x0082 /* System Sleep */
  SYSTEM_WAKE_UP,              // 0x0083 /* System Wake Up */
};

/* HID Keyboard Usage Page (0x07) */
enum hid_keyboard_usage_page {
  KC_NO = 0x00,       // 0x00 /* No event in this report */
  KC_ROLL_OVER,       // 0x01 /* Keyboard Error Roll Over */
  KC_POST_FAIL,       // 0x02 /* Keyboard POST Fail */
  KC_UNDEFINED,       // 0x03 /* Keyboard Error Undefined */
  KC_A,               // 0x04 /* Keyboard a and A */
  KC_B,               // 0x05 /* Keyboard b and B */
  KC_C,               // 0x06 /* Keyboard c and C */
  KC_D,               // 0x07 /* Keyboard d and D */
  KC_E,               // 0x08 /* Keyboard e and E */
  KC_F,               // 0x09 /* Keyboard f and F */
  KC_G,               // 0x0A /* Keyboard g and G */
  KC_H,               // 0x0B /* Keyboard h and H */
  KC_I,               // 0x0C /* Keyboard i and I */
  KC_J,               // 0x0D /* Keyboard j and J */
  KC_K,               // 0x0E /* Keyboard k and K */
  KC_L,               // 0x0F /* Keyboard l and L */
  KC_M,               // 0x10 /* Keyboard m and M */
  KC_N,               // 0x11 /* Keyboard n and N */
  KC_O,               // 0x12 /* Keyboard o and O */
  KC_P,               // 0x13 /* Keyboard p and P */
  KC_Q,               // 0x14 /* Keyboard q and Q */
  KC_R,               // 0x15 /* Keyboard r and R */
  KC_S,               // 0x16 /* Keyboard s and S */
  KC_T,               // 0x17 /* Keyboard t and T */
  KC_U,               // 0x18 /* Keyboard u and U */
  KC_V,               // 0x19 /* Keyboard v and V */
  KC_W,               // 0x1A /* Keyboard w and W */
  KC_X,               // 0x1B /* Keyboard x and X */
  KC_Y,               // 0x1C /* Keyboard y and Y */
  KC_Z,               // 0x1D /* Keyboard z and Z */
  KC_1,               // 0x1E /* Keyboard 1 and ! */
  KC_2,               // 0x1F /* Keyboard 2 and @ */
  KC_3,               // 0x20 /* Keyboard 3 and # */
  KC_4,               // 0x21 /* Keyboard 4 and $ */
  KC_5,               // 0x22 /* Keyboard 5 and % */
  KC_6,               // 0x23 /* Keyboard 6 and ^ */
  KC_7,               // 0x24 /* Keyboard 7 and & */
  KC_8,               // 0x25 /* Keyboard 8 and * */
  KC_9,               // 0x26 /* Keyboard 9 and ( */
  KC_0,               // 0x27 /* Keyboard 0 and ) */
  KC_ENTER,           // 0x28 /* Keyboard Enter (Return) */
  KC_ESCAPE,          // 0x29 /* Keyboard Escape */
  KC_BSPACE,          // 0x2A /* Keyboard Backspace */
  KC_TAB,             // 0x2B /* Keyboard Tab */
  KC_SPACE,           // 0x2C /* Keyboard Spacebar */
  KC_MINUS,           // 0x2D /* Keyboard - and _ */
  KC_EQUAL,           // 0x2E /* Keyboard = and + */
  KC_LBRACKET,        // 0x2F /* Keyboard [ and { */
  KC_RBRACKET,        // 0x30 /* Keyboard ] and } */
  KC_BSLASH,          // 0x31 /* Keyboard \ (and |) */
  KC_NONUS_HASH,      // 0x32 /* Keyboard Non-US # and ~ (Typically near the Enter key) */
  KC_SCOLON,          // 0x33 /* Keyboard ; (and :) */
  KC_QUOTE,           // 0x34 /* Keyboard ' and " */
  KC_GRAVE,           // 0x35 /* Keyboard Grave accent and tilde */
  KC_COMMA,           // 0x36 /* Keyboard , and < */
  KC_DOT,             // 0x37 /* Keyboard . and > */
  KC_SLASH,           // 0x38 /* Keyboard / and ? */
  KC_CAPSLOCK,        // 0x39 /* Keyboard Caps Lock */
  KC_F1,              // 0x3A /* Keyboard F1 */
  KC_F2,              // 0x3B /* Keyboard F2 */
  KC_F3,              // 0x3C /* Keyboard F3 */
  KC_F4,              // 0x3D /* Keyboard F4 */
  KC_F5,              // 0x3E /* Keyboard F5 */
  KC_F6,              // 0x3F /* Keyboard F6 */
  KC_F7,              // 0x40 /* Keyboard F7 */
  KC_F8,              // 0x41 /* Keyboard F8 */
  KC_F9,              // 0x42 /* Keyboard F9 */
  KC_F10,             // 0x43 /* Keyboard F10 */
  KC_F11,             // 0x44 /* Keyboard F11 */
  KC_F12,             // 0x45 /* Keyboard F12 */
  KC_PSCREEN,         // 0x46 /* Keyboard Print Screen */
  KC_SCROLLLOCK,      // 0x47 /* Keyboard Scroll Lock */
  KC_PAUSE,           // 0x48 /* Keyboard Pause */
  KC_INSERT,          // 0x49 /* Keyboard Insert */
  KC_HOME,            // 0x4A /* Keyboard Home */
  KC_PGUP,            // 0x4B /* Keyboard Page Up */
  KC_DELETE,          // 0x4C /* Keyboard Delete */
  KC_END,             // 0x4D /* Keyboard End */
  KC_PGDOWN,          // 0x4E /* Keyboard Page Down */
  KC_RIGHT,           // 0x4F /* Keyboard Right Arrow */
  KC_LEFT,            // 0x50 /* Keyboard Left Arrow */
  KC_DOWN,            // 0x51 /* Keyboard Down Arrow */
  KC_UP,              // 0x52 /* Keyboard Up Arrow */
  KC_NUMLOCK,         // 0x53 /* Keyboard Num Lock */
  KC_KP_SLASH,        // 0x54 /* Keypad Slash */
  KC_KP_ASTERISK,     // 0x55 /* Keypad Asterisk */
  KC_KP_MINUS,        // 0x56 /* Keypad Minus */
  KC_KP_PLUS,         // 0x57 /* Keypad Plus */
  KC_KP_ENTER,        // 0x58 /* Keypad Enter */
  KC_KP_1,            // 0x59 /* Keypad 1 */
  KC_KP_2,            // 0x5A /* Keypad 2 */
  KC_KP_3,            // 0x5B /* Keypad 3 */
  KC_KP_4,            // 0x5C /* Keypad 4 */
  KC_KP_5,            // 0x5D /* Keypad 5 */
  KC_KP_6,            // 0x5E /* Keypad 6 */
  KC_KP_7,            // 0x5F /* Keypad 7 */
  KC_KP_8,            // 0x60 /* Keypad 8 */
  KC_KP_9,            // 0x61 /* Keypad 9 */
  KC_KP_0,            // 0x62 /* Keypad 0 */
  KC_KP_DOT,          // 0x63 /* Keypad Dot */
  KC_NONUS_BSLASH,    // 0x64 /* Non-US \ and | (Typically near the Left-Shift key) */
  KC_APPLICATION,     // 0x65 /* Keyboard Application */
  KC_POWER,           // 0x66 /* Keyboard Power */
  KC_KP_EQUAL,        // 0x67 /* Keypad Equal Sign */
  KC_F13,             // 0x68 /* Keyboard F13 */
  KC_F14,             // 0x69 /* Keyboard F14 */
  KC_F15,             // 0x6A /* Keyboard F15 */
  KC_F16,             // 0x6B /* Keyboard F16 */
  KC_F17,             // 0x6C /* Keyboard F17 */
  KC_F18,             // 0x6D /* Keyboard F18 */
  KC_F19,             // 0x6E /* Keyboard F19 */
  KC_F20,             // 0x6F /* Keyboard F20 */
  KC_F21,             // 0x70 /* Keyboard F21 */
  KC_F22,             // 0x71 /* Keyboard F22 */
  KC_F23,             // 0x72 /* Keyboard F23 */
  KC_F24,             // 0x73 /* Keyboard F24 */
  KC_EXECUTE,         // 0x74 /* Keyboard Execute */
  KC_HELP,            // 0x75 /* Keyboard Help */
  KC_MENU,            // 0x76 /* Keyboard Menu */
  KC_SELECT,          // 0x77 /* Keyboard Select */
  KC_STOP,            // 0x78 /* Keyboard Stop */
  KC_AGAIN,           // 0x79 /* Keyboard Again */
  KC_UNDO,            // 0x7A /* Keyboard Undo */
  KC_CUT,             // 0x7B /* Keyboard Cut */
  KC_COPY,            // 0x7C /* Keyboard Copy */
  KC_PASTE,           // 0x7D /* Keyboard Paste */
  KC_FIND,            // 0x7E /* Keyboard Find */
  KC__MUTE,           // 0x7F /* Keyboard Mute */
  KC__VOLUP,          // 0x80 /* Keyboard Volume Up */
  KC__VOLDOWN,        // 0x81 /* Keyboard Volume Down */
  KC_LOCKING_CAPS,    // 0x82 /* Keyboard Locking Caps Lock */
  KC_LOCKING_NUM,     // 0x83 /* Keyboard Locking Num Lock */
  KC_LOCKING_SCROLL,  // 0x84 /* Keyboard Locking Scroll Lock */
  KC_KP_COMMA,        // 0x85 /* Keypad Comma */
  KC_KP_EQUAL_AS400,  // 0x86 /* Keypad Equal Sign on AS/400 */
  KC_INT1,            // 0x87 /* Keyboard International1 (Yen) */
  KC_INT2,            // 0x88 /* Keyboard International2 (Ro) */
  KC_INT3,            // 0x89 /* Keyboard International3 (Kana) */
  KC_INT4,            // 0x8A /* Keyboard International4 (JIS Katakana/Hiragana) */
  KC_INT5,            // 0x8B /* Keyboard International5 (JIS Zenkaku/Hankaku) */
  KC_INT6,            // 0x8C /* Keyboard International6 */
  KC_INT7,            // 0x8D /* Keyboard International7 */
  KC_INT8,            // 0x8E /* Keyboard International8 */
  KC_INT9,            // 0x8F /* Keyboard International9 */
  KC_LANG1,           // 0x90 /* Keyboard LANG1 (Hangul/English) */
  KC_LANG2,           // 0x91 /* Keyboard LANG2 (Hanja) */
  KC_LANG3,           // 0x92 /* Keyboard LANG3 (Katakana) */
  KC_LANG4,           // 0x93 /* Keyboard LANG4 (Hiragana) */
  KC_LANG5,           // 0x94 /* Keyboard LANG5 (Zenkaku/Hankaku) */
  KC_LANG6,           // 0x95 /* Keyboard LANG6 */
  KC_LANG7,           // 0x96 /* Keyboard LANG7 */
  KC_LANG8,           // 0x97 /* Keyboard LANG8 */
  KC_LANG9,           // 0x98 /* Keyboard LANG9 */
  KC_ALT_ERASE,       // 0x99 /* Keyboard Alternate Erase */
  KC_SYSREQ,          // 0x9A /* Keyboard SysReq/Attention */
  KC_CANCEL,          // 0x9B /* Keyboard Cancel */
  KC_CLEAR,           // 0x9C /* Keyboard Clear */
  KC_PRIOR,           // 0x9D /* Keyboard Prior */
  KC_RETURN,          // 0x9E /* Keyboard Return */
  KC_SEPARATOR,       // 0x9F /* Keyboard Separator */
  KC_OUT,             // 0xA0 /* Keyboard Out */
  KC_OPER,            // 0xA1 /* Keyboard Oper */
  KC_CLEAR_AGAIN,     // 0xA2 /* Keyboard Clear/Again */
  KC_CRSEL,           // 0xA3 /* Keyboard CrSel/Props */
  KC_EXSEL,           // 0xA4 /* Keyboard ExSel */

  /* Modifiers */
  KC_LCTRL = 0xE0,  // 0xE0 /* Keyboard Left Control */
  KC_LSHIFT,        // 0xE1 /* Keyboard Left Shift */
  KC_LALT,          // 0xE2 /* Keyboard Left Alt */
  KC_LGUI,          // 0xE3 /* Keyboard Left GUI */
  KC_RCTRL,         // 0xE4 /* Keyboard Right Control */
  KC_RSHIFT,        // 0xE5 /* Keyboard Right Shift */
  KC_RALT,          // 0xE6 /* Keyboard Right Alt */
  KC_RGUI,          // 0xE7 /* Keyboard Right GUI */
};

/* HID Consumer Usage Page (0x0C) */
enum hid_consumer_usage_page {
  /* Media Controls */
  MEDIA_PLAY = 0x00B0,        // 0x00B0 /* Keyboard Play */
  MEDIA_PAUSE,                // 0x00B1 /* Keyboard Pause */
  MEDIA_RECORD,               // 0x00B2 /* Keyboard Record */
  MEDIA_FAST_FORWARD,         // 0x00B3 /* Keyboard Fast Forward */
  MEDIA_REWIND,               // 0x00B4 /* Keyboard Rewind */
  MEDIA_NEXT_TRACK,           // 0x00B5 /* Keyboard Next */
  MEDIA_PREV_TRACK,           // 0x00B6 /* Keyboard Previous */
  MEDIA_STOP,                 // 0x00B7 /* Keyboard Stop */
  MEDIA_EJECT,                // 0x00B8 /* Keyboard Eject */
  MEDIA_STOP_EJECT = 0x00CC,  // 0x00CC /* Keyboard Stop/Eject */
  MEDIA_PLAY_PAUSE,           // 0x00CD /* Keyboard Play/Pause */
  AUDIO_MUTE = 0x00E2,        // 0x00E2 /* Keyboard Mute */
  AUDIO_VOL_UP = 0x00E9,      // 0x00E9 /* Keyboard Volume Up */
  AUDIO_VOL_DOWN,             // 0x00EA /* Keyboard Volume Down */

  /* App Launch */
  APPLAUNCH_CC_CONFIG = 0x0183,      // 0x0183 /* Keyboard Configuration */
  APPLAUNCH_EMAIL = 0x018A,          // 0x018A /* Keyboard Email */
  APPLAUNCH_CALCULATOR = 0x0192,     // 0x0192 /* Keyboard Calculator */
  APPLAUNCH_LOCAL_BROWSER = 0x0194,  // 0x0194 /* Keyboard Local Browser */
  APPLAUNCH_BROWSER = 0x0196,        // 0x0196 /* Keyboard Browser */
  APPLAUNCH_LOCK = 0x019E,           // 0x019E /* Keyboard Lock */
  APPLAUNCH_COMMAND_RUN = 0x01A0,    // 0x01A0 /* Keyboard Command Run */
  APPLAUNCH_FILE_EXPLORER = 0x01B4,  // 0x01B4 /* Keyboard File Explorer */

  /* App Control */
  APPCONTROL_MAXIMIZE = 0x0205,   // 0x0205 /* Keyboard Maximize */
  APPCONTROL_MINIMIZE,            // 0x0206 /* Keyboard Minimize */
  APPCONTROL_SEARCH = 0x0221,     // 0x0221 /* Keyboard Search */
  APPCONTROL_HOME = 0x0223,       // 0x0223 /* Keyboard Home */
  APPCONTROL_BACK,                // 0x0224 /* Keyboard Back */
  APPCONTROL_FORWARD,             // 0x0225 /* Keyboard Forward */
  APPCONTROL_STOP,                // 0x0226 /* Keyboard Stop */
  APPCONTROL_REFRESH,             // 0x0227 /* Keyboard Refresh */
  APPCONTROL_BOOKMARKS = 0x022A,  // 0x022A /* Keyboard Bookmarks */

  /* Display Brightness Controls */
  BRIGHTNESS_INCREMENT = 0x006F,  // 0x006F /* Keyboard Brightness Increment */
  BRIGHTNESS_DECREMENT,           // 0x0070 /* Keyboard Brightness Decrement */
};

/* Internal Special Codes
 * These are not part of the HID Usage Tables, but are used internally
 * to represent special keys that are not part of the standard HID Usage Tables.
 */
enum internal_special_codes {
  /* System Control */
  KC_SYSTEM_POWER = 0xA5,
  KC_SYSTEM_SLEEP,  // A6
  KC_SYSTEM_WAKE,   // A7

  /* Media Control */
  KC_MEDIA_PLAY,          // A8
  KC_MEDIA_PAUSE,         // A9
  KC_MEDIA_RECORD,        // AA
  KC_MEDIA_FAST_FORWARD,  // AB
  KC_MEDIA_REWIND,        // AC
  KC_MEDIA_NEXT_TRACK,    // AD
  KC_MEDIA_PREV_TRACK,    // AE
  KC_MEDIA_STOP,          // AF
  KC_MEDIA_EJECT,         // B0
  KC_MEDIA_STOP_EJECT,    // B1
  KC_MEDIA_PLAY_PAUSE,    // B2
  KC_AUDIO_MUTE,          // B3
  KC_AUDIO_VOL_UP,        // B4
  KC_AUDIO_VOL_DOWN,      // B5

  /* App Launch */
  KC_APPLAUNCH_CC_CONFIG,      // B6
  KC_APPLAUNCH_EMAIL,          // B7
  KC_APPLAUNCH_CALCULATOR,     // B8
  KC_APPLAUNCH_LOCAL_BROWSER,  // B9
  KC_APPLAUNCH_BROWSER,        // BA
  KC_APPLAUNCH_LOCK,           // BB
  KC_APPLAUNCH_COMMAND_RUN,    // BC
  KC_APPLAUNCH_FILE_EXPLORER,  // BD

  /* App Control */
  KC_APPCONTROL_MAXIMIZE,   // BE
  KC_APPCONTROL_MINIMIZE,   // BF
  KC_APPCONTROL_SEARCH,     // C0
  KC_APPCONTROL_HOME,       // C1
  KC_APPCONTROL_BACK,       // C2
  KC_APPCONTROL_FORWARD,    // C3
  KC_APPCONTROL_STOP,       // C4
  KC_APPCONTROL_REFRESH,    // C5
  KC_APPCONTROL_BOOKMARKS,  // C6

  /* Display Brightness Controls */
  KC_BRIGHTNESS_INC,  // C7
  KC_BRIGHTNESS_DEC,  // C8
};

// clang-format off

#define CODE_TO_SYSTEM(key) \
    (key == KC_SYSTEM_POWER ? SYSTEM_POWER_DOWN : \
    (key == KC_SYSTEM_SLEEP ? SYSTEM_SLEEP : \
    (key == KC_SYSTEM_WAKE  ? SYSTEM_WAKE_UP : 0)))

/* keycode to consumer usage */
#define CODE_TO_CONSUMER(key) \
    (key == KC_MEDIA_PLAY               ?  MEDIA_PLAY : \
    (key == KC_MEDIA_PAUSE              ?  MEDIA_PAUSE : \
    (key == KC_MEDIA_RECORD             ?  MEDIA_RECORD : \
    (key == KC_MEDIA_FAST_FORWARD       ?  MEDIA_FAST_FORWARD : \
    (key == KC_MEDIA_REWIND             ?  MEDIA_REWIND : \
    (key == KC_MEDIA_NEXT_TRACK         ?  MEDIA_NEXT_TRACK : \
    (key == KC_MEDIA_PREV_TRACK         ?  MEDIA_PREV_TRACK : \
    (key == KC_MEDIA_STOP               ?  MEDIA_STOP : \
    (key == KC_MEDIA_EJECT              ?  MEDIA_EJECT : \
    (key == KC_MEDIA_STOP_EJECT         ?  MEDIA_STOP_EJECT : \
    (key == KC_MEDIA_PLAY_PAUSE         ?  MEDIA_PLAY_PAUSE : \
    (key == KC_AUDIO_MUTE               ?  AUDIO_MUTE : \
    (key == KC_AUDIO_VOL_UP             ?  AUDIO_VOL_UP : \
    (key == KC_AUDIO_VOL_DOWN           ?  AUDIO_VOL_DOWN : \
    (key == KC_APPLAUNCH_CC_CONFIG      ?  APPLAUNCH_CC_CONFIG : \
    (key == KC_APPLAUNCH_EMAIL          ?  APPLAUNCH_EMAIL : \
    (key == KC_APPLAUNCH_CALCULATOR     ?  APPLAUNCH_CALCULATOR : \
    (key == KC_APPLAUNCH_LOCAL_BROWSER  ?  APPLAUNCH_LOCAL_BROWSER : \
    (key == KC_APPLAUNCH_BROWSER        ?  APPLAUNCH_BROWSER : \
    (key == KC_APPLAUNCH_LOCK           ?  APPLAUNCH_LOCK : \
    (key == KC_APPLAUNCH_COMMAND_RUN    ?  APPLAUNCH_COMMAND_RUN : \
    (key == KC_APPLAUNCH_FILE_EXPLORER  ?  APPLAUNCH_FILE_EXPLORER : \
    (key == KC_APPCONTROL_MAXIMIZE      ?  APPCONTROL_MAXIMIZE : \
    (key == KC_APPCONTROL_MINIMIZE      ?  APPCONTROL_MINIMIZE : \
    (key == KC_APPCONTROL_SEARCH        ?  APPCONTROL_SEARCH : \
    (key == KC_APPCONTROL_HOME          ?  APPCONTROL_HOME : \
    (key == KC_APPCONTROL_BACK          ?  APPCONTROL_BACK : \
    (key == KC_APPCONTROL_FORWARD       ?  APPCONTROL_FORWARD : \
    (key == KC_APPCONTROL_STOP          ?  APPCONTROL_STOP : \
    (key == KC_APPCONTROL_REFRESH       ?  APPCONTROL_REFRESH : \
    (key == KC_APPCONTROL_BOOKMARKS     ?  APPCONTROL_BOOKMARKS : \
    (key == KC_BRIGHTNESS_INC           ?  BRIGHTNESS_INCREMENT : \
    (key == KC_BRIGHTNESS_DEC           ?  BRIGHTNESS_DECREMENT : 0)))))))))))))))))))))))))))))))))

/* Flip values of NumPad when Function/Action key is pressed. */
#define NUMPAD_FLIP_CODE(key) \
    (key == KC_P0    ? KC_INS : \
    (key == KC_P1    ? KC_END : \
    (key == KC_P2    ? KC_DOWN : \
    (key == KC_P3    ? KC_PGDN : \
    (key == KC_P4    ? KC_LEFT : \
    (key == KC_P5    ? KC_NO : \
    (key == KC_P6    ? KC_RIGHT : \
    (key == KC_P7    ? KC_HOME : \
    (key == KC_P8    ? KC_UP : \
    (key == KC_P9    ? KC_PGUP : \
    (key == KC_PDOT  ? KC_DEL : \
    (key == KC_PAST  ? KC_PSCR : \
    (key == KC_INS   ? KC_P0 : \
    (key == KC_END   ? KC_P1 : \
    (key == KC_DOWN  ? KC_P2 : \
    (key == KC_PGDN  ? KC_P3 : \
    (key == KC_LEFT  ? KC_P4 : \
    (key == KC_NO    ? KC_P5 : \
    (key == KC_RIGHT ? KC_P6 : \
    (key == KC_HOME  ? KC_P7 : \
    (key == KC_UP    ? KC_P8 : \
    (key == KC_PGUP  ? KC_P9 : \
    (key == KC_DEL   ? KC_PDOT : \
    (key == KC_PSCR  ? KC_PAST : 0))))))))))))))))))))))))


#define MACRO_KEY_CODE(key) \
  (key == KC_B ? KC_BOOT : 0)

// clang-format on
#endif /* HID_KEYCODES_H */
