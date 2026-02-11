# Keyboard Configuration Guide

I've included configurations for keyboards I own and have personally tested—these serve as working examples you can adapt for your own setup. The configuration system itself is simple: define your keyboard's protocol and layout, map the scancodes to their positions, and the converter handles the rest.

This guide covers the configuration process from start to finish, using the included examples as reference.

---

## How It Works

Each keyboard configuration lives in its own directory under `src/keyboards/<brand>/<model>/` and consists of three files:

**1. keyboard.config** - Protocol and metadata
Tells the build system which protocol your keyboard uses, what scancode set it sends, and some basic info about the keyboard.

**2. keyboard.h** - Layout matrix and scancode mapping  
Defines your keyboard's physical layout as a matrix, mapping each scancode to its position. Includes ASCII-art diagrams showing the layout and scancodes—they're actually useful once you're working with this stuff.

**3. keyboard.c** - Key assignments and layers
Maps physical key positions to HID keycodes—what each key actually does when you press it. You can define multiple layers here too, for things like Function keys or NumLock states.

### How the Build System Uses These Files

When you build firmware with `KEYBOARD="modelm/enhanced"`, the build system:
1. Reads `keyboard.config` to determine protocol and scancode set
2. Includes the correct protocol implementation (AT/PS2, XT, etc.)
3. Links in the scancode processor for that codeset
4. Compiles your keyboard.h/keyboard.c with the layout and mappings

The result is a firmware image specifically configured for your keyboard.

---

## Configuring a New Keyboard

Working through an IBM Model M configuration shows how the system works—it demonstrates the process with a standard 101-key layout.

### Step 1: Gather Information About Your Keyboard

You'll need to know:
- **Protocol**: What communication protocol does it use? Check [`src/protocols/`](../../src/protocols/) for available implementations (AT/PS2, XT, Amiga, Apple M0110)
- **Scancode Set**: Which scancode set does it send? The scancode set varies by keyboard and sometimes by configuration—check the [Scancode Sets documentation](../scancodes/README.md) for details
- **Physical Layout**: How many keys has it got? Standard 101/102-key, or something different like a 122-key terminal layout?
- **Connector**: DIN-5, mini-DIN-6 (PS/2), or something else? Mainly just to make sure you have the right hardware bits sorted
- **Any quirks**: Unusual key behaviour? LED indicators? Special reset requirements? The protocol implementation should handle these aspects, but it's useful to know what you're dealing with

The [Protocol Documentation](../protocols/README.md) can help identify what protocol your keyboard might be using.

### Step 2: Create the Directory Structure

```bash
mkdir -p src/keyboards/<brand>/<model>
cd src/keyboards/<brand>/<model>
```

For example, the Model M Enhanced is at [`src/keyboards/modelm/enhanced/`](../../src/keyboards/modelm/enhanced/).

### Step 3: Create keyboard.config

This is the simplest file—just a few key properties:

```cmake
# This is the Config File for this Keyboard.
MAKE=IBM
MODEL=Model M Enhanced PC Keyboard
DESCRIPTION=IBM Personal Computer AT Enhanced Keyboard
PROTOCOL=at-ps2
CODESET=set123
```

Available protocols:
- `at-ps2` - IBM PC/AT and PS/2 keyboards and mice
- `xt` - IBM PC/XT keyboards  
- `amiga` - Commodore Amiga keyboards
- `apple-m0110` - Apple M0110 and M0110A keyboards

Scancode sets you can use:
- `set123` - Auto-detects between Scancode Set 1, 2, or 3 (handy if you're not sure)
- `set1` - Scancode Set 1 only
- `amiga` - Amiga scancode set

The `MAKE`, `MODEL`, and `DESCRIPTION` fields are just for your reference—they don't actually affect how the firmware works, but they're useful for keeping track of what you're building.

### Step 4: Create keyboard.h - The Layout Matrix

The header file defines the KEYMAP macro that maps your keyboard's physical layout to a matrix organised by scancode values. It looks daunting initially, but the pattern's logical once you see how it fits together.

Here's a simplified example showing how it's structured:

**File: keyboard.h**

```c
#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "hid_keycodes.h"

/* IBM Model M Enhanced Keyboard
 * Scancode Set 2 layout (showing hexadecimal scancodes):
 * ,---.   ,---------------. ,---------------. ,---------------. ,-----------.
 * | 76|   | 05| 06| 04| 0C| | 03| 0B| 83| 0A| | 01| 09| 78| 07| | 7C| 7E| 77|
 * `---'   `---------------' `---------------' `---------------' `-----------'
 * ,-----------------------------------------------------------. ,-----------. ,---------------.
 * | 0E| 16| 1E| 26| 25| 2E| 36| 3D| 3E| 46| 45| 4E| 55|    66| | 70| 6C| 7D| | 77| 7C| 7B| 7E|
 * |-----------------------------------------------------------| |-----------| |---------------|
 * ...
 */

#define KEYMAP( \
    K76,    K05,K06,K04,K0C,  K03,K0B,K02,K0A,  K01,K09,K78,K07,  K7F,K7E,K48, \
    K0E,K16,K1E,K26,K25,K2E,K36,K3D,K3E,K46,K45,K4E,K55,    K66,  K39,K2F,K5E,  K77,K60,K7C,K7B, \
    /* ... more rows matching your keyboard layout ... */ \
) \
{ \
    { KC_NO,    KC_##K01, KC_##K02, KC_##K03, KC_##K04, KC_##K05, KC_##K06, KC_##K07 }, /* 00-07 */ \
    { KC_NO,    KC_##K09, KC_##K0A, KC_##K0B, KC_##K0C, KC_##K0D, KC_##K0E, KC_##K0F }, /* 08-0F */ \
    { KC_NO,    KC_##K11, KC_##K12, KC_NO,    KC_##K14, KC_##K15, KC_##K16, KC_NO    }, /* 10-17 */ \
    /* ... more matrix rows organised by scancode value 0x00 through 0x7F ... */ \
}

#endif // KEYBOARD_H
```

The KEYMAP macro's doing a few things here:
1. Takes scancode position parameters (K76, K05, etc.) that match the physical layout
2. Uses `KC_##` token concatenation to build the actual keycode references
3. Arranges them into a matrix organised by scancode value (0x00-0x7F in groups of 8)

When you pass `ESC` to the KEYMAP macro in keyboard.c, it becomes `KC_##ESC` which expands to `KC_ESC` through the preprocessor's token concatenation.

The scancode values come from your keyboard's documentation, or you can just observe what codes it actually sends. I've documented the scancodes for each example keyboard in their individual docs.

### Step 5: Create keyboard.c - Key Assignments

Now you define what each physical key actually does. The keycodes you pass to the KEYMAP macro get the `KC_` prefix added automatically:

**File: keyboard.c**

```c
#include "keyboard.h"
#include "keymaps.h"

/* IBM Model M Enhanced Keyboard
 * ,---.   ,---------------. ,---------------. ,---------------. ,-----------.
 * |Esc|   |F1 |F2 |F3 |F4 | |F5 |F6 |F7 |F8 | |F9 |F10|F11|F12| |PrS|ScL|Pau|
 * `---'   `---------------' `---------------' `---------------' `-----------'
 * ,-----------------------------------------------------------. ,-----------. ,---------------.
 * |  `|  1|  2|  3|  4|  5|  6|  7|  8|  9|  0|  -|  =|     BS| |Ins|Hom|PgU| |NmL|  /|  *|  -|
 * |-----------------------------------------------------------| |-----------| |---------------|
 * |Tab  |  Q|  W|  E|  R|  T|  Y|  U|  I|  O|  P|  [|  ]|    \| |Del|End|PgD| |  7|  8|  9|  +|
 * |-----------------------------------------------------------| `-----------' |-----------|   |
 * |CapsL |  A|  S|  D|  F|  G|  H|  J|  K|  L|  ;|  '|     Ret|               |  4|  5|  6|   |
 * |-----------------------------------------------------------|     ,---.     |---------------|
 * |Shft|  <|  Z|  X|  C|  V|  B|  N|  M|  ,|  .|  /|     Shift|     |Up |     |  1|  2|  3|   |
 * |-----------------------------------------------------------| ,-----------. |-----------|Ent|
 * |Ctrl|    |Alt |          Space              |Fn  |    |GUI | |Lef|Dow|Rig| |      0|  .|   |
 * `----'    `---------------------------------------'    `----' `-----------' `---------------'
 */

const uint8_t keymap_map[][KEYMAP_ROWS][KEYMAP_COLS] = {
  KEYMAP( \
    ESC,          F1,    F2,    F3,    F4,       F5,    F6,    F7,    F8,        F9,    F10,   F11,   F12,      PSCR,  SLCK,  PAUS, \
    GRV,   1,     2,     3,     4,     5,     6,     7,     8,     9,     0,     MINS,  EQL,          BSPC,     INS,   HOME,  PGUP,     NLCK,  PSLS,  PAST,  PMNS, \
    TAB,          Q,     W,     E,     R,     T,     Y,     U,     I,     O,     P,     LBRC,  RBRC,  BSLS,     DEL,   END,   PGDN,     P7,    P8,    P9,    PPLS, \
    CAPS,         A,     S,     D,     F,     G,     H,     J,     K,     L,     SCLN,  QUOT,         ENT,                              P4,    P5,    P6,          \
    LSFT,  NUBS,  Z,     X,     C,     V,     B,     N,     M,     COMM,  DOT,   SLSH,                RSFT,            UP,              P1,    P2,    P3,    PENT, \
    LCTL,         LALT,                     SPC,                                 FN,                  LGUI,     LEFT,  DOWN,  RIGHT,           P0,    PDOT \
  ),
};
```

The keycodes come from `hid_keycodes.h` and are passed **without** the `KC_` prefix (the KEYMAP macro adds it):
- Letters: `A`, `B`, `C`, etc.
- Numbers: `1`, `2`, `3`, etc.
- Modifiers: `LSFT`, `LCTL`, `LALT`, `LGUI`
- Special: `ESC`, `TAB`, `ENT`, `SPC`
- Function: `F1` through `F24`
- Navigation: `INS`, `DEL`, `HOME`, `END`, `PGUP`, `PGDN`

You can set up multiple layers too—useful for NumLock states or Function key alternatives:

```c
const uint8_t keymap_actions[][KEYMAP_ROWS][KEYMAP_COLS] = {
    KEYMAP( /* Function layer when FN key is held */
        TRNS, VOLD, VOLU, BRTD, BRTI, /* ... rest of layout ... */
    ),
};
```

Use `TRNS` (transparent) for keys that should behave the same as the base layer.

### Step 6: Build and Test

```bash
docker compose run --rm -e KEYBOARD="<brand>/<model>" builder
```

The firmware gets output to `build/rp2040-converter.uf2`. Flash it to your RP2040, wire up your keyboard, and test each key to verify the mapping.

---

## Example Keyboard Configurations

I've included configurations for the keyboards I own and have tested. These are practical, working examples showing different scenarios—various protocols, layouts, and scancode sets. If you have one of these keyboards, you can use the configuration as-is. Otherwise, they're useful references when you're setting up your own.

### IBM Keyboards (AT/PS2 Protocol)

| Make | Model | Path | Protocol | Codeset | Details |
|------|-------|------|----------|---------|---------|
| **IBM** | Model M Enhanced PC Keyboard | `modelm/enhanced` | AT/PS2 | Set 1/2/3 | [View](ibm/model-m-enhanced.md) |
| **IBM** | Model M M122 Terminal Keyboard | `modelm/m122` | AT/PS2 | Set 1/2/3 | [View](ibm/model-m-m122.md) |
| **IBM** | Model F/AT PC Keyboard | `modelf/pcat` | AT/PS2 | Set 1/2/3 | [View](ibm/model-f-pcat.md) |

### Cherry Keyboards (XT Protocol)

| Make | Model | Path | Protocol | Codeset | Details |
|------|-------|------|----------|---------|---------|
| **Cherry** | G80-0614H (Merlin Cheetah Terminal) | `cherry/G80-0614H` | XT | Set 1/2/3 | [View](cherry/g80-0614h.md) |
| **Cherry** | G80-1104H (Merlin Cheetah Terminal) | `cherry/G80-1104H` | XT | Set 1/2/3 | [View](cherry/g80-1104h.md) |

### MicroSwitch Keyboards (AT/PS2 Protocol)

| Make | Model | Path | Protocol | Codeset | Details |
|------|-------|------|----------|---------|---------|
| **MicroSwitch** | 122ST13 (PC122 Terminal) | `microswitch/122st13` | AT/PS2 | Set 1/2/3 | [View](microswitch/122st13.md) |

### Amiga Keyboards (Amiga Protocol)

| Make | Model | Path | Protocol | Codeset | Details |
|------|-------|------|----------|---------|---------|
| **Commodore** | Amiga 500/2000 Keyboard | `amiga/a500` | Amiga | Amiga Set | [View](commodore/amiga-a500.md) |

### Apple Keyboards (Apple M0110 Protocol)

| Make | Model | Path | Protocol | Codeset | Details |
|------|-------|------|----------|---------|---------|
| **Apple** | M0110A Keyboard | `apple/m0110a` | Apple M0110 | Apple M0110 Set | [View](apple/m0110a.md) |

### Building an Example Configuration

To build firmware for any of these keyboards:

```bash
# IBM Model M Enhanced
docker compose run --rm -e KEYBOARD="modelm/enhanced" builder

# IBM Model F PC/AT
docker compose run --rm -e KEYBOARD="modelf/pcat" builder

# Cherry G80-0614H Terminal
docker compose run --rm -e KEYBOARD="cherry/G80-0614H" builder

# Commodore Amiga 500/2000
docker compose run --rm -e KEYBOARD="amiga/a500" builder

# Apple M0110A
docker compose run --rm -e KEYBOARD="apple/m0110a" builder
```

The individual keyboard documentation pages provide more detail on each configuration—specific features, layout quirks, historical context, and any gotchas I discovered whilst testing.

---

## Adding Mouse Support

Mouse support works independently from keyboard configuration. Currently only the AT/PS2 mouse protocol is supported, but you can pair it with any keyboard:

```bash
# Keyboard + AT/PS2 Mouse
docker compose run --rm -e KEYBOARD="modelm/enhanced" -e MOUSE="at-ps2" builder
```

The mouse uses separate hardware lines (dedicated CLOCK/DATA pins), so it doesn't interfere with keyboard operation. More details in the [AT/PS2 Protocol Documentation](../protocols/at-ps2.md).

---

## Command Mode Key Configuration

Command Mode gives you access to special functions like entering the bootloader for firmware updates. By default, you hold **Left Shift + Right Shift** for 3 seconds, then press 'B' within 3 seconds to enter bootloader mode.

### Overriding Default Activation Keys

Some keyboards need different activation keys. The Apple M0110A, for example, only has one Shift key (both physical Shift keys send the same scancode). For these situations, you can override the default in your `keyboard.h` file.

The [Apple M0110A configuration](apple/m0110a.md) demonstrates this—it overrides the default to use Shift + Option (Alt) instead.

Add these defines **at the top** of `keyboard.h`, **before** any `#include` statements:

```c
// Override command mode activation keys (must be HID modifiers 0xE0-0xE7)
#define CMD_MODE_KEY1 KC_LSHIFT
#define CMD_MODE_KEY2 KC_LALT    // Use Shift + Alt instead
```

The modifier keys you can use:

| Constant | Key |
|----------|-----|
| `KC_LCTRL` | Left Control |
| `KC_LSHIFT` | Left Shift |
| `KC_LALT` | Left Alt |
| `KC_LGUI` | Left GUI/Windows/Command |
| `KC_RCTRL` | Right Control |
| `KC_RSHIFT` | Right Shift |
| `KC_RALT` | Right Alt |
| `KC_RGUI` | Right GUI/Windows/Command |

The build system validates these at compile time. If you try to use a non-modifier key, you'll get an error.

More information about Command Mode features is in the [Features Guide](../features/README.md).

---

## A Few Things Worth Knowing

**Start with an example** - Find an example keyboard similar to yours (same protocol, roughly the same layout) and use it as a template. It's far easier to modify something that already works than build from scratch.

**Get the scancodes right** - Use a scancode viewer or [Keytest Mode](../features/README.md) to see what codes your keyboard actually sends. Documentation for older keyboards may not match your specific revision.

**Test everything** - Every key, modifiers in combination, all of it. If your keyboard's got LEDs, make sure they work. Check Command Mode activates properly.

**Document the quirks** - If your keyboard does something unusual, document it in a README.md in your keyboard directory. Future reference matters when you need to revisit the configuration.

**The ASCII art matters** - Those visual layout diagrams in keyboard.h aren't just decoration—they make it easier to understand and maintain the configuration when you need to tweak something.

---

## Related Documentation

### Getting Started
- [Quick Start Guide](../getting-started/README.md) - Initial setup
- [Hardware Setup](../getting-started/hardware-setup.md) - Wiring and connections
- [Building Firmware](../getting-started/building-firmware.md) - Build process details
- [Flashing Firmware](../getting-started/flashing-firmware.md) - Installing firmware

### Technical Details
- [Protocols](../protocols/README.md) - Protocol specifications
- [Hardware](../hardware/README.md) - Connection diagrams
- [Features](../features/README.md) - Command Mode, Keytest, and more
- [Advanced Topics](../advanced/README.md) - Architecture and troubleshooting

### Source Code
- All keyboard configurations: [`src/keyboards/`](../../src/keyboards/)
- Config files: [`src/keyboards/*/keyboard.config`](../../src/keyboards/)
- Layout definitions: [`src/keyboards/*/keyboard.h`](../../src/keyboards/)
- Key mappings: [`src/keyboards/*/keyboard.c`](../../src/keyboards/)
- Protocol implementations: [`src/protocols/`](../../src/protocols/)
- Scancode processors: [docs/scancodes/](../scancodes/README.md) (source: `src/scancodes/`)

---

**Questions or stuck on something?**  
Pop into [GitHub Discussions](https://github.com/PaulW/rp2040-keyboard-converter/discussions) or [report a bug](https://github.com/PaulW/rp2040-keyboard-converter/issues) if you've found an issue.

