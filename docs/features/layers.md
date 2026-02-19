# Keyboard Layers

Layers let you access different sets of key functions without adding physical keys to your keyboard. Think of them as transparent overlays—you activate a layer and suddenly the same physical keys do different things. Press a key on your numpad and it might be a '7', or it might be HOME, depending on which layer is active.

This isn't a new concept. Keyboards already use a form of layering—the Shift key is one example. Hold Shift and the '2' key sends '@' instead of '2'. You've got two layers there: the base layer with numbers, and the shifted layer with symbols. The converter just extends this concept further, giving you access to additional layers for functions, media controls, or alternative layouts.

---

## What Layers Are For

Vintage keyboards often lack modern conveniences. Many don't have dedicated media keys for volume control or playback. Some are missing function keys beyond F10, and F13-F24 are particularly rare. If you're using a compact keyboard, you might not even have a full navigation cluster. Layers solve these problems by letting you access those functions without modifying the physical keyboard.

One approach is to keep a base layer (Layer 0) for normal typing, then add Layer 1 for media controls, extra function keys, or navigation that the base layout doesn't include.

The beauty of this approach is that it's entirely optional. If your keyboard already has everything you need, you don't need layers. But if you're converting a compact keyboard or want to add modern conveniences to vintage hardware, layers give you that flexibility without requiring hardware modifications.

---

## How Layers Work

The converter maintains a stack of active layers. Layer 0—the base layer—is always active. When you activate an upper layer, it overlays on top. Each key position checks the highest active layer first. If that layer has a transparent key (`TRNS`) at that position, the lookup falls through to the next lower layer, continuing until it finds an actual keycode or reaches Layer 0.

This means you don't need to redefine your entire keyboard layout for each layer. Most keys in upper layers are transparent, passing through to the base layer. You only define the keys that actually change—media controls on certain keys, function keys on the number row, navigation on the numpad. Everything else remains exactly as it is in the base layer.

**Important:** Layer 0 (the base layer) must never contain `TRNS` entries. If `TRNS` is encountered in Layer 0 (either through fall-through from an upper layer or when Layer 0 is active directly), the converter logs an error and defaults to `NO` (no key). Always use `NO` explicitly for unmapped keys in Layer 0 rather than `TRNS`.

The system supports up to 8 layers (numbered 0-7). Keep the layer count as low as practical to reduce keymap complexity. Finding the right balance depends on what you're trying to achieve.

### How Transparent Fallthrough Actually Works

Right, this is important—the converter maintains a bitmap of active layers (8 bits, one per layer). When you press a key, the lookup starts at the **highest active layer** based on that bitmap. Layer 0 is always active, so it's always the final fallback.

Here's the key bit: if the starting layer has `TRNS` at that key position, the lookup falls through to the **next lower active layer** in the bitmap. It skips any layers that aren't currently active. This continues until it finds a non-transparent keycode or reaches Layer 0.

What does this mean practically? Say you've activated Layer 1 and Layer 3 via toggle (both in the bitmap). You press a key that has `TRNS` in Layer 3. The lookup checks Layer 3 (highest) → skips Layer 2 (not active) → checks Layer 1 (active, finds keycode) → stops. If Layer 1 also had `TRNS`, it'd fall through to Layer 0.

Compare this to exclusive mode with `TO_3`: bitmap shows only Layer 0 and Layer 3 active. Same key with `TRNS` in Layer 3 falls through, but now it skips Layers 2 and 1 (both inactive) and goes straight to Layer 0. Different behaviour, different result.

The bitmap determines both the starting point (highest active layer) and the fallthrough path (only active layers get checked).

**Why this matters:**

- **TG (Toggle)** lets you stack layers. Toggle Layer 1 on, then Layer 3 on—you've got both active. Layer 3 is highest, but `TRNS` keys can fall through to Layer 1. Toggle Layer 3 off and Layer 1 becomes the highest active layer—different starting point, different fallthrough path.

- **TO (Switch To)** clears all layers except Layer 0, then activates only the target. You get exactly one upper layer active. `TRNS` keys skip straight to Layer 0 because nothing else is in the bitmap. No ambiguity about what's active.

The practical takeaway: layer stacking via `TG` gives you composable behaviour—activate multiple layers, they respect each other during fallthrough. Exclusive switching via `TO` gives you isolated modes—one layer at a time, no unexpected interactions from leftover active layers.

---

## Layer Activation Methods

There are several ways to activate layers, each suited to different use cases. You might want a layer that's only active whilst you hold a key (like a traditional Fn key), or one that toggles on and stays on until you toggle it off again. The converter supports four different activation methods to cover these scenarios.

| Keycode | Type | Behaviour | Use Case |
|---------|------|-----------|----------|
| MO_1, MO_2, MO_3 | Momentary | Active whilst held | Fn key for media controls or navigation |
| TG_1, TG_2, TG_3 | Toggle | Stays on until pressed again | Gaming layer, alternative layout |
| TO_1, TO_2, TO_3 | Permanent | Switches to layer, deactivates others | Force specific layout mode |
| OSL_1, OSL_2, OSL_3 | One-shot | Active for next keypress only | Symbols or special characters |

**Momentary layers** activate whilst you hold a specific key and deactivate when you release it. This is the most common type—it's how Fn keys typically work on laptops and compact keyboards. Hold the Fn key, press another key to access its upper-layer function, release Fn and you're back to normal typing. The IBM Model M Enhanced uses this pattern with MO_1 positioned where Right Alt normally sits.

**Toggle layers** stay active until you press the toggle key again. Press once to turn the layer on, press again to turn it off. Toggle is **additive**—you can have multiple layers toggled on simultaneously. Each toggle flips that specific layer's bit in the bitmap on or off, leaving other layers alone. This lets you stack layers: activate Layer 1 with `TG_1`, then activate Layer 3 with `TG_3`, and Layer 3 becomes the starting point (highest active) whilst Layer 1 remains underneath. Toggle Layer 3 off and you're back to starting from Layer 1. Useful for switching between different layer "modes" without explicitly selecting each one.

**Toggle layers persist across reboots.** If you toggle on Dvorak (TG_2) and power off the keyboard, Layer 2 will still be active when you power back on. The system validates saved layer state against the current keymap configuration—if you flash firmware with different layer definitions or a different keyboard, the layer state automatically resets to Layer 0 for safety.

**Permanent layer switching** deactivates **all** current layers except Layer 0, then switches to the specified target layer exclusively. Unlike toggle (which is additive), permanent switching clears the slate—it doesn't matter what was active before, you're now on exactly one upper layer. The bitmap gets reset to just Layer 0 and your target layer. This is useful for forcing a specific mode (like switching to a Dvorak layout layer) where you don't want any layering ambiguity. You'll need another layer switch key in the target layer to switch back—there's no automatic return. Like toggle layers, TO layers also persist across reboots.

**One-shot layers** activate for exactly one keypress, then automatically deactivate. Press the one-shot key, then press another key—that second key uses the upper layer, but the third key is back on the base layer. This is handy for accessing symbols or special characters without holding a modifier. If you don't press another key within the configured timeout, the one-shot layer deactivates automatically to prevent you getting stuck.

Each method has trade-offs. Momentary layers require holding a key, which might be awkward for complex shortcuts. Toggle layers risk getting stuck in the wrong layer if you lose track—though you can always toggle back or switch to a known layer with `TO`. Permanent switching forces explicit layer changes and clears whatever was active before—useful for ensuring consistent state, but less flexible than toggle. One-shot layers work brilliantly for occasional access but can be confusing if you're not used to the behaviour. Pick the method that matches how you'll actually use the layer.

**Quick comparison of TG vs TO:**

- **TG (Toggle)**: Additive—can activate multiple layers, they stack, the highest one is used as the starting point. Toggling layers on/off changes the stack order.
- **TO (Switch to)**: Exclusive—clears all layers except Layer 0, activates only the target. Forces single-layer mode, no ambiguity about what's active.

---

## Practical Examples

Right, enough theory. What does this look like in practice?

### Adding Media Controls

Say you've got a keyboard without dedicated media keys (volume, playback controls). You could create a function layer that maps media controls to convenient positions—perhaps function keys or the number row.

**Layer 0 (Base)**: Your normal typing layout. Place an MO_1 keycode somewhere convenient (perhaps Right Alt or an unused key position).

**Layer 1 (Function)**: Define media controls at specific positions:

```c
KEYMAP( /* Layer 1: Function Layer (activated by MO_1) */
    TRNS,  VOLD,  VOLU,  MPRV,  MPLY,  MNXT,  TRNS, ...  // F1-F6 become media controls
    TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS, ...  // Numbers stay transparent
    TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS, ...  // Letters stay transparent
    APP,   TRNS,  TRNS,  TRNS,  TRNS,  TRNS,  TRNS, ...  // Caps Lock becomes APP key
)
```

Most positions show TRNS (transparent)—they fall through to Layer 0. Only the keys that change get actual keycodes. Hold your MO_1 key, press F1, and you get Volume Down instead of F1. Release MO_1 and everything returns to normal.

### Numpad Navigation for macOS

Compact keyboards often have numpads that double as navigation (HOME, END, PGUP, PGDN). Windows and Linux handle this through NumLock automatically, but macOS doesn't support NumLock. Layers provide an alternative.

**Layer 0 (Base)**: Numpad positions send their standard codes (P7, P8, P9, etc.). These are always numeric on macOS, but toggle on Windows/Linux via NumLock.

**Layer 1 (Navigation)**: Map navigation keys to numpad positions:

```c
KEYMAP( /* Layer 1: Navigation Layer */
    F11,   F12,   TRNS,  ...  // Extra function keys if needed
    TRNS,  TRNS,  TRNS,  ...  // Main alphabet stays transparent
    TRNS,  HOME,  UP,    PGUP,  TRNS,  ...  // Numpad 7/8/9 → HOME/UP/PGUP
    TRNS,  LEFT,  NO,    RIGHT, TRNS,  ...  // Numpad 4/5/6 → LEFT/nochange/RIGHT
    TRNS,  END,   DOWN,  PGDN,  TRNS,  ...  // Numpad 1/2/3 → END/DOWN/PGDN
    TRNS,  INS,   DEL,   ...                 // Numpad 0/. → INS/DEL
)
```

Hold your function key and the numpad becomes a navigation cluster. Windows/Linux users don't strictly need this (they have NumLock), but it provides consistent behaviour across operating systems.

### Alternative Layouts

Toggle layers work well for switching between keyboard layouts—QWERTY to Dvorak, for instance:

**Layer 0 (QWERTY)**: Standard QWERTY layout. Include a TG_1 key to toggle Layer 1.

**Layer 1 (Dvorak)**: Complete Dvorak layout with a TG_1 key to toggle back. Every letter position gets remapped:

```c
KEYMAP( /* Layer 1: Dvorak Layout */
    TRNS,  TRNS,  QUOT,  COMM,  DOT,   P,     Y,     F,    G,    C,    R,  ...
    TRNS,  TRNS,  A,     O,     E,     U,     I,     D,    H,    T,    N,  ...
    TRNS,  TRNS,  SCLN,  Q,     J,     K,     X,     B,    M,    W,    V,  ...
)
```

Press the toggle key once and you're typing Dvorak. Press it again and you're back to QWERTY. **The setting persists across reboots**, so you don't need to retoggle every time you power on. If you later flash firmware with different layer definitions (for example, removing Layer 1), the system automatically resets to Layer 0 for safety.

### When You Might Not Need Layers

Full-size keyboards with all the keys you need typically don't require layers. If you've got dedicated media keys, F13-F24, and a full navigation cluster, a single-layer keymap suffices. Layers add flexibility but also complexity—there's no point including them if you won't use them.

Several keyboards in the project use single-layer configurations because their physical layouts already provide all necessary functions. Check [`docs/keyboards/`](../keyboards/) to see which keyboards use layers and which don't.

---

## Layer State Persistence

Toggle layers (TG) and permanent layer switches (TO) persist across power cycles. If you toggle Dvorak on and reboot your keyboard, it'll still be in Dvorak mode when it powers back up. Momentary layers (MO) and one-shot layers (OSL) are temporary and always become inactive at boot.

**How persistence works:**

The converter stores active layer state in flash memory alongside other configuration settings (LED brightness, shift-override enable, etc.). Every time you press TG or TO to change layers, the new state is saved immediately. On boot, the system:

1. Loads the saved layer state
2. Validates it against the current keymap configuration
3. Restores the state if valid, resets to Layer 0 if invalid

**Validation ensures safety.** If you flash firmware with:
- A different keyboard configuration
- Modified layer definitions (added/removed layers)
- Changed keymap dimensions

...the system detects this and resets to Layer 0 instead of restoring potentially invalid state. This prevents crashes from trying to access non-existent layers.

**Why MO and OSL don't persist:**

Momentary and one-shot layers are designed for temporary access—you hold a key or press it once. Persisting these would be confusing: keyboard boots with "Fn" layer active even though you're not holding the Fn key. Toggle and TO layers are explicit state changes that make sense to preserve.

**Flash wear:** The config storage uses wear-leveling and dual-copy redundancy. Layer state changes write to flash, but typical usage patterns (toggling Dvorak once per session, occasional layout switches) generate minimal wear. Flash wear is mitigated by the dual‑copy scheme; avoid excessive toggling if you are concerned about endurance.

---

## When You Don't Need Layers

Most keyboards don't need them. If you're converting a full-size keyboard with all the keys you need, and you're not planning to remap the layout or add modern conveniences, a single-layer keymap is perfectly adequate. The implementation adds minimal overhead, but there's no point adding complexity if you won't use it.

Even keyboards that could benefit from layers don't require them. You can always decide later that you want to add a function layer for media controls. The keymap is just source code—edit it, rebuild, flash, and you're done. There's no need to design the perfect layer system from the start. Build what you need now and extend it if requirements change.

---

## Implementation Details

The layer system documentation here covers the concept and practical usage. If you want to actually implement layers in your keyboard's configuration—editing the keymap, defining which keys activate which layers, setting up transparent keys—that's all covered in the [Custom Keymaps](../development/custom-keymaps.md) guide.

That guide walks through the keymap structure, explains how to add layers to your keyboard.c file, shows code examples for different layer types, and covers the keycodes you'll use (MO, TG, TO, OSL, and TRNS). It's implementation documentation rather than conceptual, so it gets into the details of array structures, macro usage, and how the lookup actually works.

The keyboard-specific documentation in [`docs/keyboards/`](../keyboards/) shows which keyboards have layer configurations and what they're used for. Some keyboards define multiple layers out of the box, others stick to a single base layer. Check the documentation for your specific keyboard to see what's already configured before you start adding your own.

---

## Related Documentation

- [Custom Keymaps](../development/custom-keymaps.md) - Keymap implementation guide with layer setup details
- [Adding Keyboards](../development/adding-keyboards.md) - Includes layer configuration for new keyboards
- [Keyboards Overview](../keyboards/README.md) - Supported keyboards and their layer configurations
- [Command Mode](command-mode.md) - Special function access without layers

---

**Questions or stuck on something?**  
Pop into [GitHub Discussions](https://github.com/PaulW/rp2040-keyboard-converter/discussions) or [report a bug](https://github.com/PaulW/rp2040-keyboard-converter/issues) if you've found an issue.
