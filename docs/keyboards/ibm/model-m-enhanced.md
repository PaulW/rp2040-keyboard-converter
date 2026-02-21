# IBM Model M Enhanced Keyboard

**Configuration**: `modelm/enhanced`

![IBM Model M Enhanced (1390131)](../../images/keyboards/ibm/IBM-1390131.jpeg)

---

## Overview

The IBM Model M Enhanced Keyboard uses membrane-based buckling spring switches. This is the standard 101/102-key IBM Enhanced Keyboard—the most common Model M variant.

**Part Numbers**: 1390131 (PC/AT US), 1391401 (PS/2 ANSI), 1391403 (PS/2 ISO)

---

## Specifications

| Specification | Details |
|---------------|---------|
| **Make** | IBM |
| **Model** | Model M Enhanced PC Keyboard |
| **Keys** | 101 (ANSI) / 102 (ISO) |
| **Protocol** | AT/PS2 |
| **Codeset** | Scancode Set 2 |
| **Connector** | 6-pin mini-DIN (SDL) or 5-pin DIN (180° arrangement, SDL models) |
| **Voltage** | 5V |
| **Switch Type** | Buckling spring (membrane-based) |
| **Key Rollover** | 2-key rollover (2KRO) |

---

## Features

The buckling spring switches give tactile click and audible feedback. The keyboard has a steel backplate with thick ABS plastic case and dye-sublimated PBT keycaps (later models switched to pad-printed). The layout's either 101-key ANSI or 102-key ISO depending on region, with LEDs for Caps Lock, Num Lock, and Scroll Lock.

SDL (Shielded Data Link) models come with a removable cable—either 6-pin mini-DIN or 5-pin DIN connector. Later production models moved to fixed cables.

### Key Rollover

The Model M's limited to 2-key rollover (2KRO)—only two non-modifier keys can be reliably detected at the same time. This comes from the membrane-based matrix design, quite different from the earlier Model F keyboards which used capacitive sensing and supported full NKRO. In normal typing you won't notice, but if you're pressing multiple keys simultaneously the keyboard may miss anything beyond the second key.

There's a quirk though—depending on which row and column the keys are on, you can sometimes get more than two keys detected. That's just how the membrane matrix is wired, not a design feature.

---

## Building Firmware

Build firmware specifically for this keyboard:

```bash
# Model M Enhanced only
docker compose run --rm -e KEYBOARD="modelm/enhanced" builder

# Model M Enhanced + AT/PS2 Mouse
docker compose run --rm -e KEYBOARD="modelm/enhanced" -e MOUSE="at-ps2" builder
```

**Output**: `build/rp2040-converter.uf2`

See: [Building Firmware Guide](../../getting-started/building-firmware.md)

---

## Key Mapping

The default keymap matches the standard IBM Model M layout with the following custom modifications:

### Base Layer Mapping (Layer 0 - NumLock On)

From [`keyboard.c`](../../../src/keyboards/modelm/enhanced/keyboard.c):

```
IBM Model M Enhanced Keyboard
,---.   ,---------------. ,---------------. ,---------------. ,-----------.
|Esc|   |F1 |F2 |F3 |F4 | |F5 |F6 |F7 |F8 | |F9 |F10|F11|F12| |PrS|ScL|Pau|
`---'   `---------------' `---------------' `---------------' `-----------'
,-----------------------------------------------------------. ,-----------. ,---------------.
|  `|  1|  2|  3|  4|  5|  6|  7|  8|  9|  0|  -|  =|     BS| |Ins|Hom|PgU| |NmL|  /|  *|  -|
|-----------------------------------------------------------| |-----------| |---------------|
|Tab  |  Q|  W|  E|  R|  T|  Y|  U|  I|  O|  P|  [|  ]|    \| |Del|End|PgD| |  7|  8|  9|  +|
|-----------------------------------------------------------| `-----------' |-----------|   |
|CapsL |  A|  S|  D|  F|  G|  H|  J|  K|  L|  ;|  '|  ±| Ret|               |  4|  5|  6|   |
|-----------------------------------------------------------|     ,---.     |---------------|
|Shft|  <|  Z|  X|  C|  V|  B|  N|  M|  ,|  ,|  /|     Shift|     |Up |     |  1|  2|  3|   |
|-----------------------------------------------------------| ,-----------. |-----------|Ent|
|Ctrl|    |Alt |          Space              |Alt |    |Ctrl| |Lef|Dow|Rig| |      0|  .|   |
`----'    `---------------------------------------'    `----' `-----------' `---------------'

±: ISO Hash Key uses same code as ANSI Backslash
```

### Raw Scancode Map

From [`keyboard.h`](../../../src/keyboards/modelm/enhanced/keyboard.h):

```
IBM Model M Enhanced Keyboard - Scancode Set 2
,---.   ,---------------. ,---------------. ,---------------. ,-----------.
| 76|   | 05| 06| 04| 0C| | 03| 0B|~83| 0A| | 01| 09| 78| 07| |+7C| 7E|+77|
`---'   `---------------' `---------------' `---------------' `-----------'
,-----------------------------------------------------------. ,-----------. ,---------------.
| 0E| 16| 1E| 26| 25| 2E| 36| 3D| 3E| 46| 45| 4E| 55|     66| |*70|*6C|*7D| | 77|*4A| 7C| 7B|
|-----------------------------------------------------------| |-----------| |---------------|
| 0D  | 15| 1D| 24| 2D| 2C| 35| 3C| 43| 44| 4D| 54| 5B|  5D | |*71|*69|*7A| | 6C| 75| 7D| 79|
|-----------------------------------------------------------| `-----------' |-----------|   |
| 58   | 1C| 1B| 23| 2B| 34| 33| 3B| 42| 4B| 4C| 52|±5D| 5A |               | 6B| 73| 74|   |
|-----------------------------------------------------------|     ,---.     |---------------|
| 12 | 61| 1A| 22| 21| 2A| 32| 31| 3A| 41| 49| 4A|      59  |     |*75|     | 69| 72| 7A|*5A|
|-----------------------------------------------------------| ,-----------. |-----------|   |
| 14 |    | 11 |          29                 | *11|     |*14| |*6B|*72|*74| |     70| 71|   |
`----'    `---------------------------------------'     `---' `-----------' `---------------'

*: E0-prefixed codes
+: Special codes sequence
~: Remaps to alternate code (83-02)
±: ISO Hash Key uses same code as ANSI Backslash

61 is ISO 102-key Backslash Key
```

### Key Assignments

| Physical Key | Function | Notes |
|--------------|----------|-------|
| **Caps Lock** | Menu key | Remapped for modern OS compatibility |
| **Right Ctrl** | Left GUI (Windows/Command key) | Adds GUI key to standard layout |
| **Right Alt** | Fn modifier | Enables function layer access |

### Function Layer

When holding Right Alt (Fn modifier), additional functions are available for media control and navigation:

| Key Combination | Function | Notes |
|----------------|----------|-------|
| **Fn + F1** | Volume Down | Media control |
| **Fn + F2** | Volume Up | Media control |
| **Fn + F3** | Brightness Down | Display control |
| **Fn + F4** | Brightness Up | Display control |
| **Fn + Caps Lock** | Menu (App) key | Alternative access |
| **Fn + ` (Grave)** | Non-US Backslash | Additional key mapping |

### Fn Modifier Key

**Default Fn Key**: Right Alt (can be customised in [`keyboard.c`](../../../src/keyboards/modelm/enhanced/keyboard.c))

---

## Customisation

### Modifying Key Layout

To customise the key layout, edit the keymap in [`keyboard.c`](../../../src/keyboards/modelm/enhanced/keyboard.c):

```c
// Example: Remap Caps Lock to Control
// Find the CAPS_LOCK entry and change KC_MENU to KC_LCTRL
```

Available keycodes are defined in [`hid_keycodes.h`](../../../src/common/lib/hid_keycodes.h).

### Command Mode Keys

This keyboard uses the default command mode keys: **Left Shift + Right Shift**

If you need to change this (not typical for Model M), edit [`keyboard.h`](../../../src/keyboards/modelm/enhanced/keyboard.h) before any includes:

```c
#define CMD_MODE_KEY1 KC_LSHIFT
#define CMD_MODE_KEY2 KC_RSHIFT  // Default
```

---

## Hardware Connection

### Connector and Pinout

Model M Enhanced uses either a **6-pin mini-DIN (SDL)** connector or **5-pin DIN (180° arrangement)** on SDL models with removable/interchangeable cables. Later Model M keyboards used hardwired 6-pin mini-DIN connectors which could not be interchanged.

**Pinout details and diagrams**: See [AT/PS2 Protocol - Physical Interface](../../protocols/at-ps2.md#physical-interface) for complete connector pinout diagrams and specifications.

**Notes**:
- SDL models support removable/interchangeable cables with either connector type
- Later non-SDL models have hardwired 6-pin mini-DIN cables
- Both connector types use the same electrical pinout (CLOCK, DATA, GND, +5V)

### Wiring to RP2040

Connect the keyboard to your Raspberry Pi Pico:

**For 6-pin mini-DIN:**

| Mini-DIN Pin | Function | RP2040 GPIO | Notes |
|--------------|----------|-------------|-------|
| 1 | DATA | GPIO 2 (default) | Configurable in [`config.h`](../../../src/config.h) |
| 3 | GND | GND | Any ground pin |
| 4 | VCC | VBUS (5V) | External 5V recommended for reliability |
| 5 | CLOCK | GPIO 3 (DATA+1) | Must be DATA pin + 1 |

**For 5-pin DIN (SDL models):**

| DIN Pin | Function | RP2040 GPIO | Notes |
|---------|----------|-------------|-------|
| 1 | CLOCK | GPIO 3 (DATA+1) | Must be DATA pin + 1 |
| 2 | DATA | GPIO 2 (default) | Configurable in [`config.h`](../../../src/config.h) |
| 4 | GND | GND | Any ground pin |
| 5 | VCC | VBUS (5V) | External 5V recommended for reliability |

**⚠️ Important**: CLOCK pin must be DATA pin + 1 (hardware constraint). If you change DATA to GPIO 10, CLOCK becomes GPIO 11.

See: [Hardware Setup Guide](../../getting-started/hardware-setup.md)

---

## Protocol Details

The Model M Enhanced uses the AT/PS2 protocol with bidirectional communication. The keyboard sends scancodes to the host, and the converter can send LED commands back. The keyboard uses Scancode Set 2.

See [AT/PS2 Protocol Documentation](../../protocols/at-ps2.md) for complete technical details.

---

## History & Variants

### Production Timeline

The Model M Enhanced was introduced as the PC/AT Enhanced Keyboard (1390xxx series) followed by the PS/2 Enhanced variant (1391xxx series). The "M" designation refers to the membrane-based sensing mechanism—buckling springs still provide the tactile feedback, just a different sensing method underneath.

The keyboards came in either 101-key ANSI or 102-key ISO layouts.

### Variants

Example part numbers for this keyboard:

| Part Number | Description | Keys | Layout |
|-------------|-------------|------|--------|
| **1390131** | PC/AT Enhanced (US) | 101 | ANSI |
| **1391401** | PS/2 Enhanced (ANSI) | 101 | ANSI |
| **1391403** | PS/2 Enhanced (ISO) | 102 | ISO |

**Note**: This is not a comprehensive list. See [Admiral Shark's Keyboards Model M Enhanced](https://sharktastica.co.uk/wiki/model-m-enhanced) for complete part number directory.

---

## Troubleshooting

### Keyboard Not Detected

1. **Check wiring**: Verify DATA/CLOCK pins match [`config.h`](../../../src/config.h)
2. **Check power**: Ensure 5V is stable (use external power, not Pico VBUS directly)
3. **Mini-DIN connector**: PS/2 connectors can have poor contacts - clean with contact cleaner
4. **Power-on reset**: Some Model M keyboards require power cycle to initialise

### Keys Not Registering

1. **Cleaning needed**: Buckling springs can accumulate dirt - disassemble and clean
2. **Membrane damage**: Keyboard membrane may be torn or deteriorated
3. **Spring issues**: Individual buckling springs may be deformed or missing
4. **Barrel plate issues**: Broken rivets can cause registration problems - may require screw or bolt modding to fix
5. **Barrel plate damage**: Barrel plates can split over time, especially in older keyboards

### LEDs Not Working

1. **Normal behaviour**: LEDs should respond to Caps/Num/Scroll Lock
2. **LED failure**: Individual LEDs can fail over time
3. **Controller issue**: LED control comes from keyboard controller, not converter

---

## Source Files

- **Configuration**: [`src/keyboards/modelm/enhanced/keyboard.config`](../../../src/keyboards/modelm/enhanced/keyboard.config)
- **Keymap**: [`src/keyboards/modelm/enhanced/keyboard.c`](../../../src/keyboards/modelm/enhanced/keyboard.c)
- **Header**: [`src/keyboards/modelm/enhanced/keyboard.h`](../../../src/keyboards/modelm/enhanced/keyboard.h)

---

## Related Documentation

- [Supported Keyboards](../README.md) - All supported keyboards
- [AT/PS2 Protocol](../../protocols/at-ps2.md) - Protocol details
- [Hardware Setup](../../getting-started/hardware-setup.md) - Wiring guide
- [Building Firmware](../../getting-started/building-firmware.md) - Build instructions
- [Command Mode](../../features/README.md) - Command mode features

---

## External Resources

### Technical Documentation
- **[Admiral Shark's Keyboards - IBM Model M Enhanced](https://sharktastica.co.uk/wiki/model-m-enhanced)** - Complete Model M Enhanced documentation with part number directory
- **[Admiral Shark's Keyboards - IBM Personal System/2 Enhanced Keyboard](https://sharktastica.co.uk/keyboard-directory/riGuZHSw)** - PS/2 Enhanced variant with comprehensive part number directory
- **[Admiral Shark's Keyboards - IBM Model M Wiki](https://sharktastica.co.uk/wiki/ibm-model-m)** - Model M family history, design details, and variants
- [Deskthority Wiki: IBM Model M](https://deskthority.net/wiki/IBM_Model_M) - Community-maintained technical information

### Historical Context
- [Wikipedia: Model M Keyboard](https://en.wikipedia.org/wiki/Model_M_keyboard) - Production timeline and manufacturer transitions

### Community Resources
- [ClickyKeyboards.com Model M Guide](https://clickykeyboards.com/) - Restoration and maintenance guides
- [Unicomp](https://www.pckeyboard.com/) - Current manufacturer continuing Model M production

