# Custom Keymaps

Want to swap Caps Lock with Control? Remap the Windows key? Create a Dvorak or Colemak layout? This guide shows you how to modify your keyboard's key mappings—it's just editing a source file, rebuilding, and flashing.

The converter translates scancodes (what your keyboard sends) into HID keycodes (what USB expects). The keymap is the lookup table between the two. Change the keymap, and any physical key can send any keycode you want.

---

## How Keymaps Work

When you press a key on your keyboard, there's a process that happens before your computer sees it. The keyboard's internal controller detects the keypress and sends a scancode over its protocol—AT/PS2, XT, or whatever it uses. Different keyboards use different scancode sets, which is one reason why we need the converter in the first place.

Now, some scancodes are multi-byte sequences. Extended keys like the arrow keys get prefixed with `E0`, and some special keys like Pause send even longer sequences. The scancode processor handles all this complexity and produces complete make/break events—effectively "key down" and "key up" notifications.

That's where the keymap comes in. It's a lookup table that translates the scancode into an HID keycode—the USB standard code for that key. So if scancode 0x1C comes in (which is usually the 'A' key on most keyboards), the keymap looks that up and returns the HID keycode for 'A'. The HID interface then packages this into a USB HID report and sends it to your computer, where the OS interprets it.

The beauty of this is that the keymap is just an array. Want the 'A' key to send 'B' instead? Change the keymap entry for scancode 0x1C from `A` to `B`. That's all there is to it.

---

## Finding Your Keyboard's Keymap

Each keyboard configuration has its own keymap file. It's in [`src/keyboards/`](../../src/keyboards/)`<brand>/<model>/keyboard.c`.

For example:
- **IBM Model M Enhanced:** [`src/keyboards/modelm/enhanced/keyboard.c`](../../src/keyboards/modelm/enhanced/keyboard.c)
- **Apple M0110A:** [`src/keyboards/apple/m0110a/keyboard.c`](../../src/keyboards/apple/m0110a/keyboard.c)
- **Amiga 500:** [`src/keyboards/amiga/a500/keyboard.c`](../../src/keyboards/amiga/a500/keyboard.c)

Open the file and you'll see something like this:

```c
#include "keyboard.h"
#include "keymaps.h"

const uint8_t keymap_map[][KEYMAP_ROWS][KEYMAP_COLS] = {
    KEYMAP(
        ESC,     F1,   F2,   F3,   /* ... more keys ... */
        GRV,     1,    2,    3,    /* ... */
        TAB,     Q,    W,    E,    /* ... */
        CAPS,    A,    S,    D,    /* ... */
        LSFT,    Z,    X,    C,    /* ... */
        LCTL,    LGUI, LALT, SPC,  /* ... */
    ),
};
```

The `KEYMAP()` macro (or `KEYMAP_ISO()`, `KEYMAP_PC122()`, etc. depending on your layout) defines the keys in physical order—same order they appear on the keyboard. This makes it easier to find which key is which.

Note that keycodes are written without the `KC_` prefix in the KEYMAP macro—the build system adds it automatically through preprocessor token concatenation.

---

## Simple Remapping

Let's say you want to swap Caps Lock and Control. Find Caps Lock in the keymap:

```c
CAPS, A, S, D, F, /* ... */
```

Change `CAPS` to `LCTL`:

```c
LCTL, A, S, D, F, /* ... */
```

Now find Left Control (usually on the bottom row):

```c
LCTL, LGUI, LALT, SPC, /* ... */
```

Change `LCTL` to `CAPS`:

```c
CAPS, LGUI, LALT, SPC, /* ... */
```

That's it. Rebuild the firmware:

```bash
docker compose run --rm -e KEYBOARD="your-keyboard" builder
```

Flash the UF2 file to your converter, and Caps Lock will act as Control, whilst the physical Control key will now be Caps Lock.

---

## Common Modifications

Here are several keymap modifications you might find useful:

### Disable a Key

Sometimes you just want a key to do nothing at all. Maybe you keep hitting it by accident, or perhaps it's broken and you want to prevent spurious keypresses. Use `NO` to disable it:

```c
// Disable the Windows/GUI key
NO, LALT, SPC, /* ... */
```

### Swap Modifiers

Swapping modifier keys around is straightforward. Here are a couple of examples:

**Swap Alt and GUI (Windows/Command key)** - Some people prefer this arrangement, especially if they're switching between different operating systems:

```c
// Before: LGUI, LALT, SPC
// After:
LALT, LGUI, SPC
```

**Make both Shift keys identical** - Useful if one of your Shift keys is broken or just feels dodgy:

Both should already send `LSFT` and `RSFT` respectively. If you want both to send the same code (say, both send `LSFT`), just change one to match the other.

### Remap Function Keys

If your keyboard doesn't have F13-F24 keys but you want access to them, you can remap the existing function keys:

```c
// Before: F1, F2, F3, F4, /* ... */
// After:
F13, F2, F3, F4, /* ... */
```

Now F1 will send F13 to your computer. This can be handy for application shortcuts that use extended function keys.

---

## Creating Alternative Layouts

If you want to create an entirely different layout (like Dvorak or Colemak), you're just remapping all the letter keys. It's more work than swapping a couple of modifiers, but the process is the same—just find the keys in your keymap and change them.

Dvorak rearranges letters for typing efficiency. The home row becomes `aoeuidhtns` instead of `asdfghjkl`. Here's how the letter key rows look in QWERTY and Dvorak:

```c
// QWERTY (standard):
TAB,  Q, W, E, R, T, Y, U, I, O, P, /* ... */
CAPS, A, S, D, F, G, H, J, K, L, SCLN, /* ... */
LSFT, Z, X, C, V, B, N, M, COMM, DOT, SLSH, /* ... */
```

```c
// Dvorak:
TAB,  QUOT, COMM, DOT, P, Y, F, G, C, R, L, /* ... */
CAPS, A, O, E, U, I, D, H, T, N, S, /* ... */
LSFT, SCLN, Q, J, K, X, B, M, W, V, Z, /* ... */
```

Colemak's another alternative layout designed for efficiency. It changes fewer keys than Dvorak, which makes it easier to learn if you're coming from QWERTY. Some people find this a less drastic transition:

```c
// Colemak:
TAB,  Q, W, F, P, G, J, L, U, Y, SCLN, /* ... */
CAPS, A, R, S, T, D, H, N, E, I, O, /* ... */
LSFT, Z, X, C, V, B, K, M, COMM, DOT, SLSH, /* ... */
```

One thing to remember: these are physical key positions. When you press the physical 'Q' key on your keyboard, it sends whatever keycode you've mapped to that position in the array. So if you map that position to `QUOT`, pressing the key with 'Q' printed on it will send a quote character to your computer. The keycap legends don't change what's sent—only the keymap does.

---

## Understanding Layout Macros

The `KEYMAP()` macro (and its variants like `KEYMAP_ISO()`, `KEYMAP_PC122()`) is defined in each keyboard's `keyboard.h` file. It maps the physical grid of keys to the actual lookup array indexed by scancode.

For standard layouts, the macros are already defined. But if you're working with an unusual keyboard layout, you might need to create your own macro.

**Basic structure:**

```c
#define KEYMAP( \
    K00, K01, K02, K03, /* ... row 0 keys */ \
    K10, K11, K12, K13, /* ... row 1 keys */ \
    K20, K21, K22, K23, /* ... row 2 keys */ \
    /* ... more rows ... */ \
) \
{ \
    { KC_##K00, KC_##K01, KC_##K02, KC_##K03, /* ... */ }, \
    { KC_##K10, KC_##K11, KC_##K12, KC_##K13, /* ... */ }, \
    { KC_##K20, KC_##K21, KC_##K22, KC_##K23, /* ... */ }, \
    /* ... */ \
}
```

The parameters (`K00`, `K01`, etc.) represent each key position. The macro uses `KC_##` token concatenation to build the actual keycode references—when you pass `ESC`, it expands to `KC_ESC` automatically.

You probably won't need to modify these unless you're adding support for a keyboard with a completely non-standard layout.

---

## Complete Keycode Reference

Here's a quick reference of available HID keycodes. For the complete list, see [`src/common/lib/hid_keycodes.h`](../../src/common/lib/hid_keycodes.h).

### Letters and Numbers

```
A through Z       - Letters
1 through 0       - Number row
ENT               - Enter/Return
ESC               - Escape
BSPC              - Backspace
TAB               - Tab
SPC               - Spacebar
```

### Function Keys

```
F1 through F12    - Standard function keys
F13 through F24   - Extended function keys
```

### Modifiers

```
LCTL, RCTL        - Left/Right Control
LSFT, RSFT        - Left/Right Shift
LALT, RALT        - Left/Right Alt
LGUI, RGUI        - Left/Right GUI (Windows/Command key)
```

### Navigation and Editing

```
UP, DOWN, LEFT, RIGHT   - Arrow keys
HOME, END               - Home/End
PGUP, PGDN              - Page Up/Down
INS, DEL                - Insert/Delete
```

### Lock Keys

```
CAPS                - Caps Lock
NLCK                - Num Lock
SLCK                - Scroll Lock
```

### Punctuation and Symbols

```
MINS                - Minus/Underscore (-)
EQL                 - Equal/Plus (=)
LBRC                - Left Bracket ([)
RBRC                - Right Bracket (])
BSLS                - Backslash (\)
SCLN                - Semicolon/Colon (;)
QUOT                - Quote/Double-quote (')
GRV                 - Grave/Tilde (`)
COMM                - Comma/Less-than (,)
DOT                 - Dot/Greater-than (.)
SLSH                - Slash/Question (/)
```

### Numpad

```
P0 through P9           - Numpad numbers
PSLS                    - Numpad /
PAST                    - Numpad *
PMNS                    - Numpad -
PPLS                    - Numpad +
PENT                    - Numpad Enter
PDOT                    - Numpad . (decimal point)
```

### Media and System Keys

```
PSCR                - Print Screen
PAUS                - Pause/Break
APP                 - Application/Menu key
PWR                 - Power
MUTE                - Mute
VOLU                - Volume Up
VOLD                - Volume Down
MNXT                - Next Track
MPRV                - Previous Track
MPLY                - Play/Pause
MSTP                - Stop
```

### Special

```
NO                  - No action (key disabled)
TRNS                - Transparent (pass through to lower layer)
```

---

## Testing Your Keymap

Right, after rebuilding and flashing, you'll want to test things systematically. Go through row by row and verify each key sends what you expect. A key tester website like `keyboardchecker.com` or `keyboard-test.space` can be useful here—they'll visualise which keys are being detected, making it easier to spot any issues.

Don't forget to test modifier combinations too. Make sure Shift+letter, Ctrl+letter, and all that work as expected. Test the lock keys—Caps Lock, Num Lock, Scroll Lock should toggle correctly, and if your protocol supports LEDs, they should update accordingly.

Once you've done the systematic testing, try using it in your actual workflow. Type out a paragraph, use your editor shortcuts, test any application-specific key bindings you rely on. This'll catch issues you might've missed in the systematic testing—there's nothing quite like real-world use for finding the odd quirk.

If a key doesn't work as expected, the UART debug output is your friend. Enable DEBUG logging (Command Mode → D → 3) and connect a UART adapter to GPIO 0/1. The debug output shows which scancode was received and which HID keycode it mapped to—that'll tell you exactly where things are going wrong.

---

## Multi-Layer Keymaps

The converter supports multi-layer keymaps (similar to QMK firmware), but most keyboards don't need this—single-layer keymaps are usually sufficient.

If you do want multiple layers (e.g., a function layer accessed via a modifier key), you can define additional layers in the keymap array:

```c
const uint8_t keymap_map[][KEYMAP_ROWS][KEYMAP_COLS] = {
    // Layer 0: Default
    KEYMAP(
        ESC, F1, F2, /* ... */
    ),
    
    // Layer 1: Function layer
    KEYMAP(
        TRNS, F13, F14, /* ... */
    ),
};
```

You'd then need to add layer switching logic in the `keymap_actions` array (via a function key or modifier). This is more advanced—single-layer keymaps are sufficient for standard keyboard remapping.

---

## Troubleshooting

### Key Sends Wrong Character

Double-check your keymap—make sure the keycode at that position is what you actually intended. Remember that the keymap represents physical key positions, not the legends printed on the keycaps. That 'Q' keycap might be sending something entirely different if you've remapped it.

If you've customised the layout and things are getting confusing, try resetting to the default keymap and rebuilding. That'll verify the converter itself works correctly, and then you can reapply your changes more carefully.

### Modifier Keys Don't Work

Make sure you're using the correct modifier keycodes. You need `LCTL` or `RCTL` for Control, `LSFT` or `RSFT` for Shift, `LALT` or `RALT` for Alt, and `LGUI` or `RGUI` for Windows/Command. Don't use `CTRL` or `SHIFT` without the L/R prefix—you need to specify left or right explicitly.

### Build Fails After Editing Keymap

Syntax errors are usually the culprit here. Check for missing commas between keycodes, mismatched parentheses or braces, or typos in keycode names. The keycode names need to match exactly what's in `hid_keycodes.h`. The compiler error message should point you to the problematic line, which helps narrow it down.

### Layout Doesn't Feel Right

If you've switched to Dvorak or Colemak and it feels strange, that's completely normal—alternative layouts take time to learn. Most people take a few weeks to become proficient, and it can feel quite frustrating initially when you're slower than you were with QWERTY.

If you decide it's not for you and want to bail out, just revert your changes and rebuild. No shame in that—not every layout suits everyone.

---

That should cover most keymap customisation scenarios. Whether you're just swapping Caps Lock with Control or creating a completely custom layout, the process is the same: edit the keymap, rebuild, flash, test.

And remember, you can always undo your changes and rebuild if something doesn't work out. The original keymap's in git history, so you're never stuck with a broken configuration.

---

## Related Documentation

- [Adding Keyboards](adding-keyboards.md) - Step-by-step keyboard support guide
- [Keyboards Overview](../keyboards/README.md) - Supported keyboards and configurations
- [Building Firmware](../getting-started/building-firmware.md) - Docker build process
- [Flashing Firmware](../getting-started/flashing-firmware.md) - Installing firmware updates
- [HID Keycodes](../../src/common/lib/hid_keycodes.h) - Complete keycode reference
- [Contributing Guide](contributing.md) - Pull request process if sharing custom layouts

---

**Questions or stuck on something?**  
Pop into [GitHub Discussions](https://github.com/PaulW/rp2040-keyboard-converter/discussions) or [report a bug](https://github.com/PaulW/rp2040-keyboard-converter/issues) if you've found an issue.
