# Adding Keyboard Support

If you have a keyboard that isn't already supported, adding it is fairly simple—provided the protocol implementation already exists. Check [`src/protocols/`](../../src/protocols/) to see which protocols are implemented. If your keyboard uses one of those, you're mostly just defining the keymap and providing some configuration metadata.

This guide walks through the process of adding a new keyboard configuration, assuming the protocol is already implemented. If you need to add a completely new protocol, that's more involved—have a look at the existing implementations in [`src/protocols/`](../../src/protocols/) to see what's required.

**What you'll need to know:**
- Your keyboard's protocol
- The scancode set it sends
- Physical layout (affects how you define the keymap matrix)
- Any quirks or non-standard behaviour

This is implementation documentation for adding new keyboard support. If you're looking to configure or customise an already-supported keyboard, have a look at the [Keyboard Configuration Guide](../keyboards/README.md) instead.

---

## Gathering Information

Before you start creating files, you'll want to gather some information about your keyboard. Some of this might be in documentation, some you might need to work out yourself.

First up, you'll need to know what protocol your keyboard uses. Check [`src/protocols/`](../../src/protocols/) to see which ones are implemented—there's AT/PS2, XT, Amiga, and Apple M0110 at the moment. The [Protocol Documentation](../protocols/README.md) describes each protocol's characteristics and how to identify them. If your keyboard uses one of these, you're in luck. If not, you'll need to implement the protocol first, which is rather more involved.

Next, work out which scancode set your keyboard sends. Check the [Scancode Sets documentation](../scancodes/README.md) to see the available processors. The `set123` processor handles multiple scancode sets dynamically, so if you're not certain which set your keyboard uses, that processor can detect the set automatically.

You'll also need to understand the physical layout. Is it a standard ANSI (US) layout, ISO (UK/EU), or something more unusual like a 122-key terminal layout? This determines how you'll structure your keymap matrix and which macro variants you might need. The layout affects which parameters the KEYMAP macro expects and how they map to the actual scancode matrix.

Finally, note down any quirks or oddities. Does your keyboard have non-standard scancodes? Missing keys compared to a standard layout? Special function keys that don't map to standard HID codes? These are useful to document now—trust me, you'll forget them otherwise, and future you will appreciate having it written down when you come back to it months later wondering why you did something a particular way.

If you're not sure about scancodes, the easiest way to find out is to connect a UART adapter to GPIO 0/1 and watch the debug output. Press keys one at a time and note what codes appear. It's a bit tedious, I'll grant you, but you'll have accurate data rather than relying on documentation that might not match your specific revision.

---

## Directory Structure

Each keyboard gets its own directory under [`src/keyboards/`](../../src/keyboards/)`<brand>/<model>/`. For example:

```
src/keyboards/
├── modelm/
│   ├── enhanced/
│   │   ├── keyboard.config
│   │   ├── keyboard.h
│   │   ├── keyboard.c
│   │   └── README.md
│   └── m122/
│       ├── keyboard.config
│       ├── keyboard.h
│       ├── keyboard.c
│       └── README.md
├── apple/
│   └── m0110a/
│       ├── keyboard.config
│       ├── keyboard.h
│       ├── keyboard.c
│       └── README.md
└── ... other keyboards ...
```

The directory name should be lowercase, use hyphens for spaces (e.g., `model-m-enhanced` rather than `ModelMEnhanced`), and be descriptive enough to identify the specific model.

---

## keyboard.config

This file tells the build system which protocol and scancode set to use. It's quite simple—just a simple key=value format:

```plaintext
# This is the Config File for this Keyboard.
MAKE=IBM
MODEL=Model M Enhanced PC Keyboard
DESCRIPTION=IBM Personal Computer AT Enhanced Keyboard
PROTOCOL=at-ps2
CODESET=set123
```

The `MAKE` and `MODEL` fields are fairly self-explanatory—just the manufacturer name and model designation. The `DESCRIPTION` is a longer description that appears in boot messages, so you'll see it when the converter starts up.

The important bits are `PROTOCOL` and `CODESET`. The `PROTOCOL` field tells the build system which protocol implementation to use—check [`src/protocols/`](../../src/protocols/) for what's available. The `CODESET` field specifies which scancode processor to use—see the [Scancode Sets documentation](../scancodes/README.md) for the options.

One thing worth mentioning about `CODESET`: the `set123` processor handles Scancode Sets 1, 2, and 3 dynamically. If you're working with a keyboard and aren't certain which set it uses, `set123` can detect the set automatically. For examples of CODESET usage with different protocols, see the existing keyboard configurations in [`src/keyboards/`](../../src/keyboards/)—they show the patterns for each protocol type.

---

## keyboard.h

This header defines any keyboard-specific overrides or constants. The simplest implementation just includes the HID keycodes header. You'll need to add content here if you need to override the Command Mode activation keys.

**Minimal example:**

```c
#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "hid_keycodes.h"

#endif  // KEYBOARD_H
```

**With Command Mode override** (for keyboards with unusual modifier keys):

```c
#ifndef KEYBOARD_H
#define KEYBOARD_H

// Apple M0110A only has one shift key—both physical Shift keys send the same 
// scancode (0x71). The default Command Mode activation uses Left Shift + Right 
// Shift, which obviously won't work here. So we override it to use Shift + 
// Option (Alt) instead.
#define CMD_MODE_KEY1 KC_LSHIFT
#define CMD_MODE_KEY2 KC_LALT

#include "hid_keycodes.h"

#endif  // KEYBOARD_H
```

**Important:** The `#define` statements for Command Mode keys must come **before** the `#include "hid_keycodes.h"` line. The Command Mode implementation uses `#ifndef` guards, so the first definition wins. If you don't override the keys here, the defaults (Left Shift + Right Shift) are used automatically.

---

## keyboard.c

This is where you define the actual keymap—the lookup table that maps scancodes to HID keycodes. The structure depends on your keyboard's physical layout, and whilst the array looks a bit daunting initially, it's quite logical once you see how it works.

**Basic structure:**

```c
#include "keyboard.h"
#include "keymaps.h"

const uint8_t keymap_map[][KEYMAP_ROWS][KEYMAP_COLS] = {
    KEYMAP(
        // Row by row key definitions...
    ),
};
```

The `KEYMAP()` macro (and its variants) depends on your keyboard's layout:

- **KEYMAP():** Standard layout macro
- **KEYMAP_ISO():** For keyboards with ISO-specific differences
- **KEYMAP_PC122():** For 122-key terminal keyboards
- **Custom macros:** For unusual layouts, define your own in keyboard.h

Check existing keyboard implementations for examples of the variants.

**Example (IBM Model F PC/AT):**

```c
#include "keyboard.h"
#include "keymaps.h"

const uint8_t keymap_map[][KEYMAP_ROWS][KEYMAP_COLS] = {
    KEYMAP(
        ESC,      F1,   F2,   F3,   F4,   F5,   F6,   F7,   F8,   F9,   F10,  F11,   F12,   PSCR, SLCK, PAUS,
        GRV,      1,    2,    3,    4,    5,    6,    7,    8,    9,    0,    MINS,  EQL,   BSPC, INS,  HOME, PGUP, NLCK, PSLS, PAST, PMNS,
        TAB,      Q,    W,    E,    R,    T,    Y,    U,    I,    O,    P,    LBRC,  RBRC,  BSLS, DEL,  END,  PGDN, P7,   P8,   P9,   PPLS,
        CAPS,     A,    S,    D,    F,    G,    H,    J,    K,    L,    SCLN, QUOT,  ENT,   P4,   P5,   P6,
        LSFT,     Z,    X,    C,    V,    B,    N,    M,    COMM, DOT,  SLSH, RSFT,  UP,    P1,   P2,   P3,   PENT,
        LCTL,     LGUI, LALT, SPC,  RALT, RGUI, APP,  RCTL,  LEFT, DOWN, RIGHT, P0,   PDOT
    ),
};
```

The keycodes are laid out in the same physical order as the keys on the keyboard. This makes it easier to verify the mapping and spot mistakes.

**Available keycodes:** See [`src/common/lib/hid_keycodes.h`](../../src/common/lib/hid_keycodes.h) for the complete list. You use the shorthand names in your keymap—the KEYMAP macro adds the `KC_` prefix automatically via token concatenation (`KC_##`).

Common shorthand keycodes:

- **Letters:** A through Z
- **Numbers:** 1 through 0
- **Function keys:** F1 through F24
- **Modifiers:** LSFT, RSFT, LCTL, RCTL, LALT, RALT, LGUI, RGUI
- **Navigation:** UP, DOWN, LEFT, RIGHT, HOME, END, PGUP, PGDN
- **Editing:** INS, DEL, BSPC, ENT, TAB, ESC
- **Locks:** CAPS, NLCK, SLCK
- **Numpad:** P0 through P9, PAST, PPLS, PMNS, PSLS, PENT, PDOT

**Unmapped keys:** Use `NO` for keys that don't have a mapping or shouldn't send anything.

### Multi-Layer Keymaps

Most keyboards only need a single base layer (Layer 0). However, you might want additional layers for function keys, navigation, or gaming modes. Layer switching is covered in detail in the [Custom Keymaps](custom-keymaps.md) guide, but here's the basic structure:

```c
const uint8_t keymap_map[][KEYMAP_ROWS][KEYMAP_COLS] = {
    [0] = KEYMAP(  // Base layer
        // Your default key mappings
    ),
    [1] = KEYMAP(  // Function layer (activated by MO_1)
        // Function key mappings, media controls, etc.
        // Use TRNS for keys that should pass through to base layer
    ),
};
```

Layer keycodes:
- **MO_1, MO_2, MO_3**: Momentary layer switch whilst held
- **TG_1, TG_2, TG_3**: Toggle layer on/off
- **TO_1, TO_2, TO_3**: Switch to layer permanently
- **OSL_1, OSL_2, OSL_3**: One-shot layer (next key only)
- **TRNS**: Transparent—passes through to lower layer

**NumLock Handling:** You don't need separate NumLock layers. The numpad keys (KC_P0-KC_P9, KC_P7=7/HOME, KC_P1=1/END, etc.) are dual-function by HID specification—the host OS interprets them as either numeric (NumLock ON) or navigation (NumLock OFF). Windows and Linux handle this natively. On macOS (which doesn't support NumLock), provide navigation keys in a function layer for users who need them.

**Example—Model F PC/AT with navigation in Fn layer:**

```c
const uint8_t keymap_map[][KEYMAP_ROWS][KEYMAP_COLS] = {
    [0] = KEYMAP(  // Base layer
        // ... ESC, numbers, letters, etc.
        // Numpad: P7, P8, P9 send as-is (host interprets)
    ),
    [1] = KEYMAP(  // Fn layer for macOS navigation
        TRNS, TRNS, /* ... */
        HOME, UP,   PGUP,   // Fn+7/8/9 → HOME/UP/PGUP
        LEFT, TRNS, RIGHT,  // Fn+4/5/6 → LEFT/nochange/RIGHT  
        END,  DOWN, PGDN,   // Fn+1/2/3 → END/DOWN/PGDN
    ),
};
```

### Shift-Override (Non-Standard Shift Legends)

Some keyboards have non-standard shift legends that don't match the HID keycodes sent by standard keys. This includes terminal keyboards, vintage keyboards, and some international layouts. For example, the IBM Model M Type 2 (M122) has:
- Physical key labelled `2` and `"` (double quote)
- Standard shift behaviour: Shift+2 = `@`
- But key physically shows `"`

For these keyboards, you can define per-layer shift-override tables that remap shifted keys to match the physical legends. This is completely optional—only use it for keyboards where the physical legends genuinely differ from standard.

**Implementation:**

Add a `keymap_shift_override_layers` array to your keyboard.c:

```c
// Per-layer shift-override table (sparse array of pointers)
// Array index = layer number, NULL = no shift-override for that layer
const uint8_t * const keymap_shift_override_layers[KEYMAP_MAX_LAYERS] = {
    [0] = (const uint8_t[SHIFT_OVERRIDE_ARRAY_SIZE]){
        // Suppress shift and send different key
        [KC_7] = SUPPRESS_SHIFT | KC_QUOT,   // Shift+7 → ' (apostrophe, suppress shift)
        [KC_0] = SUPPRESS_SHIFT | KC_NUHS,   // Shift+0 → # (hash/pound, suppress shift)
        [KC_MINS] = SUPPRESS_SHIFT | KC_EQL, // Shift+- → = (equals, suppress shift)
        
        // Keep shift for standard behaviour
        [KC_6] = KC_7,  // Shift+6 → Shift+7 produces & (ampersand, keep shift held)
        
        // Most entries are 0 (use default shift behaviour)
    },
    // Other layers: NULL or define additional shift-override arrays as needed
    // [1] = (const uint8_t[SHIFT_OVERRIDE_ARRAY_SIZE]){ ... },  // Optional: Layer 1 shift-override
};
```

**How it works:**
- When shift is held and a key pressed, the system checks the shift-override array for the source layer
- Source layer = layer where key was found (accounting for KC_TRNS fallthrough)
- If entry exists: send that keycode instead of base key
- If entry has `SUPPRESS_SHIFT` flag (bit 7): remove shift modifier for final key
- If entry is 0 or layer has no shift-override array (NULL): use default behaviour (send base key with shift)

**SUPPRESS_SHIFT flag (0x80):**
- `SUPPRESS_SHIFT | KC_QUOT`: Sends `'` (apostrophe) **without** shift modifier
- `KC_7`: Sends `7` **with** shift modifier (produces shifted character like `&`)

**Per-layer shift-override:**
Different layers can have different shift behaviours. For example, Layer 0 might use modern PC shift legends while Layer 1 uses terminal shift legends. Simply define multiple entries in the `keymap_shift_override_layers` array:

```c
const uint8_t * const keymap_shift_override_layers[KEYMAP_MAX_LAYERS] = {
    [0] = (const uint8_t[SHIFT_OVERRIDE_ARRAY_SIZE]){ /* Standard PC legends */ },
    [1] = (const uint8_t[SHIFT_OVERRIDE_ARRAY_SIZE]){ /* Terminal legends */ },
    // Layers 2-7 default to NULL (no shift-override)
};
```

**Runtime validation:**
The converter automatically detects configuration errors at runtime. If you define a shift-override array for a layer that doesn't exist in your keymap, an error is logged to UART when that layer is accessed:

```text
[ERR] Shift-override array entry [1] exists but keyboard only defines Layer 0!
[ERR] Fix keyboard.c: Either add Layer 1 to keymap_map or remove shift-override entry [1]
```

This prevents undefined behaviour from stale or incorrect shift-override definitions.

**When to use shift-override:**
- Terminal keyboards with non-standard legends (IBM 327x, 3270, 522x series)
- International keyboards where physical labels don't match HID standard
- NOT for standard keyboards—adds complexity without benefit

**Example keyboards with shift-override:**
- IBM Model M Type 2 (M122): `src/keyboards/modelm/m122/keyboard.c`
- Microswitch Type 2 (predicted): `src/keyboards/microswitch/122st13/keyboard.c` (needs hardware testing)

**Enabling shift-override:**

Shift-override is **disabled by default** even if your keyboard defines the array. Users must explicitly enable it through Command Mode:

1. Hold both shifts for 3 seconds (enter Command Mode)
2. Press 'S' to toggle shift-override on/off
3. If the keyboard doesn't define the array, a warning is displayed and the setting is not changed
4. State persists across reboots in flash memory
5. Automatically resets to disabled if different keyboard firmware flashed
6. Automatically disables if the shift-override array is removed from the keyboard but keyboard ID stays the same

The converter validates shift-override availability on every boot, ensuring the config state always matches the actual keyboard capabilities. This prevents stale configuration from causing undefined behaviour.

---

## README.md

Document your keyboard properly. Future you will appreciate this when you come back to it months later wondering why you made certain decisions. Include the practical stuff that someone else would need to know if they wanted to use or modify your configuration.

Start with the specifications—the full model name and number exactly as it appears on the label, manufacturer, year if you can determine it, protocol and scancode set, physical layout, and connector type. These are the basics that help identify exactly what keyboard you're dealing with.

Then document the quirks and special features. This is where you note down anything unusual—non-standard scancodes that differ from typical protocol behaviour, missing keys compared to a standard layout, special function keys that required custom handling, LED support (or lack thereof), and any timing peculiarities you discovered whilst testing. These details are gold dust when troubleshooting later.

Make sure to include the build command too. Show the exact command needed to build firmware for this keyboard—saves people (including future you) having to work it out themselves.

Here's an example from the IBM Model F configuration to show what I mean:

```markdown
# IBM Model F PC/AT

**Specifications:**
- Model: IBM Model F PC/AT (1503100, 1503104, etc.)
- Manufacturer: IBM
- Year: 1984-1987
- Protocol: AT/PS2
- Scancode Set: Set 2
- Layout: ANSI (US)
- Connector: 5-pin DIN (AT keyboard connector)

**Features:**
- Full 101-key layout with F1-F12 function keys
- Buckling spring switches
- LED indicators: Caps Lock, Num Lock, Scroll Lock

**Build:**
```bash
docker compose run --rm -e KEYBOARD="modelf/pcat" builder
```

**Notes:**
This is the "modern" IBM Model F with 101 keys.
```

---

## Testing

Right, once you've created all the configuration files, it's time to build the firmware and see what happens:

```bash
docker compose run --rm -e KEYBOARD="your-brand/your-model" builder
```

If the build succeeds, you'll get a `build/rp2040-converter.uf2` file. Flash it to your RP2040 board, connect your keyboard, and test everything systematically.

I mean everything too—every single key. I know it seems tedious, but you'd be surprised what you might miss otherwise. Test modifier keys in combination (Shift+letter, Ctrl+C, Alt+Tab, that sort of thing), all the function keys (F1-F12, and F13-F24 if your keyboard has them), the navigation cluster (arrows, Home, End, PgUp, PgDown, Insert, Delete), and the numpad with Num Lock both on and off. If your keyboard has lock indicators, check those update correctly when you press Caps Lock, Num Lock, and Scroll Lock. Also verify Command Mode activates properly—hold your configured keys for 3 seconds and you should get visual feedback.

**If keys produce the wrong characters,** check your keymap for mismatches. Double-check that the physical positions match the HID keycodes in your keyboard.c file. Verify the scancode set in keyboard.config actually matches what your keyboard sends. The best way to debug this is to connect a UART adapter to GPIO 0/1 and enable debug logging. Press keys one at a time and watch what scancodes appear versus what you expected—it's tedious work, but it'll show you exactly where the problem is.

**If Command Mode doesn't activate,** check you've overridden CMD_MODE_KEY1 and CMD_MODE_KEY2 in keyboard.h if your keyboard needs it (remember, keyboards with unusual modifier keys might need this). Both keys must be HID modifiers (0xE0-0xE7)—regular keys won't work for Command Mode activation. Also make sure you're holding them for the full 3 seconds.

---

## Common Issues

### Build Fails with "keyboard.config not found"

The build system's looking for your configuration file and can't find it. Check that your directory structure matches what's expected: [`src/keyboards/`](../../src/keyboards/)`<brand>/<model>/keyboard.config`. Also verify the `KEYBOARD` environment variable matches your directory path—for example, if your keyboard config is at [`src/keyboards/modelm/enhanced/`](../../src/keyboards/modelm/enhanced/), you'd use:

```bash
docker compose run --rm -e KEYBOARD="modelm/enhanced" builder
```

Just to clarify the structure: it uses `<brand>/<model>` under [`src/keyboards/`](../../src/keyboards/), not `<brand>/<manufacturer>/<model>`. So Model M keyboards live at `modelm/enhanced`, not `ibm/modelm/enhanced`.

### Build Succeeds but Wrong Keys Register

Verify your keymap matches the physical layout. The keycodes in `keyboard.c` need to be in the same order as the physical keys on your keyboard, following the KEYMAP macro's parameter order.

The best way to debug this is to connect a UART adapter to GPIO 0/1 and enable debug output. Press keys one at a time—the debug output shows which scancode was received and which HID keycode it mapped to. Compare this against what you expected to happen. It's tedious work, I'll grant you, but it'll show exactly where the mismatch is.

### Some Keys Don't Work at All

A few possible causes here. Your keyboard might send scancodes that aren't in the scancode processor's lookup table—check the UART debug output and if you see `[WARN]` messages about unrecognised scancodes, you'll need to add them to the appropriate scancode processor in [`src/scancodes/`](../../src/scancodes/).

It could also be a scancode set mismatch. You might have specified the wrong scancode set in keyboard.config. Check the [Scancode Sets documentation](../scancodes/README.md) for available processors and verify which one matches your keyboard's behaviour.

Some keyboards are also pickier about timing than others. If only certain keys fail consistently (especially extended keys like arrows or numpad), there might be a protocol timing issue. Check the UART output for protocol errors—the protocol handler will log warnings if it's detecting timing problems.

### LED Indicators Don't Update

This one can have several causes. The protocol implementation might not include LED commands. Some keyboards control their own LEDs directly rather than accepting host commands. The LEDs themselves might be faulty. There could be a wiring issue between the keyboard and converter. Even if the protocol supports LED control, the specific keyboard might not implement it. And some keyboards require specific command sequences or timing that might not be quite right.

Check the UART output for LED command sequences and responses to see what's actually happening—this shows whether the protocol is sending LED commands and whether the keyboard acknowledges them.

---

## Example: Adding a New Keyboard

Let's walk through adding support for a hypothetical "Acme Terminal Keyboard". This shows the complete process from start to finish.

Create the directory structure:

```bash
mkdir -p src/keyboards/acme/terminal
```

Create `keyboard.config`:

```plaintext
# This is the Config File for this Keyboard.
MAKE=Acme Corporation
MODEL=Terminal Keyboard
DESCRIPTION=Acme Terminal Keyboard Model TK-100
PROTOCOL=at-ps2
CODESET=set123
```

Create `keyboard.h` (no special overrides needed for this one):

```c
#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "hid_keycodes.h"

#endif  // KEYBOARD_H
```

Create `keyboard.c` with your keymap:

```c
#include "keyboard.h"
#include "keymaps.h"

const uint8_t keymap_map[][KEYMAP_ROWS][KEYMAP_COLS] = {
    KEYMAP(
        // Fill in your key mappings here, row by row
        ESC, F1, F2, /* ... and so on */
    ),
};
```

Create `README.md` with specifications, quirks, and build instructions.

Build and test:

```bash
docker compose run --rm -e KEYBOARD="acme/terminal" builder
```

Flash the resulting UF2 file and test every key systematically. If something's not right, the UART debug output will show what's happening.

---

That covers the keyboard support process. If you run into issues that aren't covered here, have a look at the existing keyboard implementations in [`src/keyboards/`](../../src/keyboards/)—they're all documented with their own quirks and solutions. The Model M Enhanced and Apple M0110A configurations are particularly well-documented if you need more detailed examples.

And if you get properly stuck, open an issue on GitHub.

---

## Related Documentation

- [Keyboards Overview](../keyboards/README.md) - Supported keyboards and configurations
- [Custom Keymaps](custom-keymaps.md) - Keymap modification guide
- [Contributing Guide](contributing.md) - Pull request process and commit format
- [Code Standards](code-standards.md) - Coding conventions and testing
- [Protocols Overview](../protocols/README.md) - Protocol specifications
- [Building Firmware](../getting-started/building-firmware.md) - Docker build process

---

**Questions or stuck on something?**  
Pop into [GitHub Discussions](https://github.com/PaulW/rp2040-keyboard-converter/discussions) or [report a bug](https://github.com/PaulW/rp2040-keyboard-converter/issues) if you've found an issue.
