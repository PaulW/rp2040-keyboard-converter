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

#ifndef HID_KEYCODES_H
#define HID_KEYCODES_H

/* Define specific Key Types */
#define IS_KEY(code) (KC_A <= (code) && (code) <= KC_EXSEL)
#define IS_MOD(code) (KC_LCTRL <= (code) && (code) <= KC_RGUI)

#define IS_SPECIAL(code) ((0xA5 <= (code) && (code) <= 0xDF) || (0xE8 <= (code) && (code) <= 0xFF))
#define IS_SYSTEM(code) (KC_PWR <= (code) && (code) <= KC_WAKE)
#define IS_CONSUMER(code) (KC_MPLY <= (code) && (code) <= KC_BRTD)

/* Define Super Macro Toggle */
#define SUPER_MACRO_INIT(code) (((code) & ((1 << (KC_LSHIFT & 0x7)) | (1 << (KC_RSHIFT & 0x7)))) == ((1 << (KC_LSHIFT & 0x7)) | (1 << (KC_RSHIFT & 0x7))))

/* Raw Hid Code Definitions */
/* Generic Desktop Page(0x01) */
/* System Power Control */
#define SYSTEM_POWER_DOWN 0x0081
#define SYSTEM_SLEEP 0x0082
#define SYSTEM_WAKE_UP 0x0083
/* Consumer Page Page (0x0C) */
/* Media Controls */
#define MEDIA_PLAY 0x00B0
#define MEDIA_PAUSE 0x00B1
#define MEDIA_RECORD 0x00B2
#define MEDIA_FAST_FORWARD 0x00B3
#define MEDIA_REWIND 0x00B4
#define MEDIA_NEXT_TRACK 0x00B5
#define MEDIA_PREV_TRACK 0x00B6
#define MEDIA_STOP 0x00B7
#define MEDIA_EJECT 0x00B8
#define MEDIA_STOP_EJECT 0x00CC
#define MEDIA_PLAY_PAUSE 0x00CD
#define AUDIO_MUTE 0x00E2
#define AUDIO_VOL_UP 0x00E9
#define AUDIO_VOL_DOWN 0x00EA
/* App Launch */
#define APPLAUNCH_CC_CONFIG 0x0183
#define APPLAUNCH_EMAIL 0x018A
#define APPLAUNCH_CALCULATOR 0x0192
#define APPLAUNCH_LOCAL_BROWSER 0x0194
#define APPLAUNCH_BROWSER 0x0196
#define APPLAUNCH_LOCK 0x019E
#define APPLAUNCH_COMMAND_RUN 0x01A0
#define APPLAUNCH_FILE_EXPLORER 0x01B4
/* App Control */
#define APPCONTROL_MAXIMIZE 0x0205
#define APPCONTROL_MINIMIZE 0x0206
#define APPCONTROL_SEARCH 0x0221
#define APPCONTROL_HOME 0x0223
#define APPCONTROL_BACK 0x0224
#define APPCONTROL_FORWARD 0x0225
#define APPCONTROL_STOP 0x0226
#define APPCONTROL_REFRESH 0x0227
#define APPCONTROL_BOOKMARKS 0x022A
/* Display Brightness Controls  https://www.usb.org/sites/default/files/hutrr41_0.pdf */
#define BRIGHTNESS_INCREMENT 0x006F
#define BRIGHTNESS_DECREMENT 0x0070

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

/* USB HID Keyboard/Keypad Usage(0x07) */
enum hid_keyboard_keypad_usage {
  KC_NO = 0x00,
  KC_ROLL_OVER,       // 01
  KC_POST_FAIL,       // 02
  KC_UNDEFINED,       // 03
  KC_A,               // 04
  KC_B,               // 05
  KC_C,               // 06
  KC_D,               // 07
  KC_E,               // 08
  KC_F,               // 09
  KC_G,               // 0A
  KC_H,               // 0B
  KC_I,               // 0C
  KC_J,               // 0D
  KC_K,               // 0E
  KC_L,               // 0F
  KC_M,               // 10
  KC_N,               // 11
  KC_O,               // 12
  KC_P,               // 13
  KC_Q,               // 14
  KC_R,               // 15
  KC_S,               // 16
  KC_T,               // 17
  KC_U,               // 18
  KC_V,               // 19
  KC_W,               // 1A
  KC_X,               // 1B
  KC_Y,               // 1C
  KC_Z,               // 1D
  KC_1,               // 1E
  KC_2,               // 1F
  KC_3,               // 20
  KC_4,               // 21
  KC_5,               // 22
  KC_6,               // 23
  KC_7,               // 24
  KC_8,               // 25
  KC_9,               // 26
  KC_0,               // 27
  KC_ENTER,           // 28
  KC_ESCAPE,          // 29
  KC_BSPACE,          // 2A
  KC_TAB,             // 2B
  KC_SPACE,           // 2C
  KC_MINUS,           // 2D
  KC_EQUAL,           // 2E
  KC_LBRACKET,        // 2F
  KC_RBRACKET,        // 30
  KC_BSLASH,          // 31   \ (and |)
  KC_NONUS_HASH,      // 32   Non-US # and ~ (Typically near the Enter key)
  KC_SCOLON,          // 33   ; (and :)
  KC_QUOTE,           // 34   ' and "
  KC_GRAVE,           // 35   Grave accent and tilde
  KC_COMMA,           // 36   , and <
  KC_DOT,             // 37   . and >
  KC_SLASH,           // 38   / and ?
  KC_CAPSLOCK,        // 39
  KC_F1,              // 3A
  KC_F2,              // 3B
  KC_F3,              // 3C
  KC_F4,              // 3D
  KC_F5,              // 3E
  KC_F6,              // 3F
  KC_F7,              // 40
  KC_F8,              // 41
  KC_F9,              // 42
  KC_F10,             // 43
  KC_F11,             // 44
  KC_F12,             // 45
  KC_PSCREEN,         // 46
  KC_SCROLLLOCK,      // 47
  KC_PAUSE,           // 48
  KC_INSERT,          // 49
  KC_HOME,            // 4A
  KC_PGUP,            // 4B
  KC_DELETE,          // 4C
  KC_END,             // 4D
  KC_PGDOWN,          // 4E
  KC_RIGHT,           // 4F
  KC_LEFT,            // 50
  KC_DOWN,            // 51
  KC_UP,              // 52
  KC_NUMLOCK,         // 53
  KC_KP_SLASH,        // 54
  KC_KP_ASTERISK,     // 55
  KC_KP_MINUS,        // 56
  KC_KP_PLUS,         // 57
  KC_KP_ENTER,        // 58
  KC_KP_1,            // 59
  KC_KP_2,            // 5A
  KC_KP_3,            // 5B
  KC_KP_4,            // 5C
  KC_KP_5,            // 5D
  KC_KP_6,            // 5E
  KC_KP_7,            // 5F
  KC_KP_8,            // 60
  KC_KP_9,            // 61
  KC_KP_0,            // 62
  KC_KP_DOT,          // 63
  KC_NONUS_BSLASH,    // 64   Non-US \ and | (Typically near the Left-Shift key) */
  KC_APPLICATION,     // 65
  KC_POWER,           // 66
  KC_KP_EQUAL,        // 67
  KC_F13,             // 68
  KC_F14,             // 69
  KC_F15,             // 6A
  KC_F16,             // 6B
  KC_F17,             // 6C
  KC_F18,             // 6D
  KC_F19,             // 6E
  KC_F20,             // 6F
  KC_F21,             // 70
  KC_F22,             // 71
  KC_F23,             // 72
  KC_F24,             // 73
  KC_EXECUTE,         // 74
  KC_HELP,            // 75
  KC_MENU,            // 76
  KC_SELECT,          // 77
  KC_STOP,            // 78
  KC_AGAIN,           // 79
  KC_UNDO,            // 7A
  KC_CUT,             // 7B
  KC_COPY,            // 7C
  KC_PASTE,           // 7D
  KC_FIND,            // 7E
  KC__MUTE,           // 7F
  KC__VOLUP,          // 80
  KC__VOLDOWN,        // 81
  KC_LOCKING_CAPS,    // 82   locking Caps Lock */
  KC_LOCKING_NUM,     // 83   locking Num Lock */
  KC_LOCKING_SCROLL,  // 84   locking Scroll Lock */
  KC_KP_COMMA,        // 85
  KC_KP_EQUAL_AS400,  // 86   equal sign on AS/400 */
  KC_INT1,            // 87
  KC_INT2,            // 88
  KC_INT3,            // 89
  KC_INT4,            // 8A
  KC_INT5,            // 8B
  KC_INT6,            // 8C
  KC_INT7,            // 8D
  KC_INT8,            // 8E
  KC_INT9,            // 8F
  KC_LANG1,           // 90
  KC_LANG2,           // 91
  KC_LANG3,           // 92
  KC_LANG4,           // 93
  KC_LANG5,           // 94
  KC_LANG6,           // 95
  KC_LANG7,           // 96
  KC_LANG8,           // 97
  KC_LANG9,           // 98
  KC_ALT_ERASE,       // 99
  KC_SYSREQ,          // 9A
  KC_CANCEL,          // 9B
  KC_CLEAR,           // 9C
  KC_PRIOR,           // 9D
  KC_RETURN,          // 9E
  KC_SEPARATOR,       // 9F
  KC_OUT,             // A0
  KC_OPER,            // A1
  KC_CLEAR_AGAIN,     // A2
  KC_CRSEL,           // A3
  KC_EXSEL,           // A4

  /* Modifiers */
  KC_LCTRL = 0xE0,
  KC_LSHIFT,  // E1
  KC_LALT,    // E2
  KC_LGUI,    // E3
  KC_RCTRL,   // E4
  KC_RSHIFT,  // E5
  KC_RALT,    // E6
  KC_RGUI,    // E7
};

/* Special keycodes for 8-bit keymap
   NOTE: 0xA5-DF and 0xE8-FF are used for internal special purpose */
enum internal_special_keycodes {
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
    (key == KC_END   ? KC_P1 : \
    (key == KC_INS   ? KC_P0 : \
    (key == KC_DOWN  ? KC_P2 : \
    (key == KC_PGDN  ? KC_P3 : \
    (key == KC_LEFT  ? KC_P4 : \
    (key == KC_NO    ? KC_P5 : \
    (key == KC_RIGHT ? KC_P6 : \
    (key == KC_HOME  ? KC_P7 : \
    (key == KC_UP    ? KC_P8 : \
    (key == KC_PGUP  ? KC_P9 : \
    (key == KC_DEL   ? KC_PDOT : 0))))))))))))))))))))))


#define MACRO_KEY_CODE(key) \
  (key == KC_B ? KC_BOOT : 0)

// clang-format on
#endif /* HID_KEYCODES_H */
