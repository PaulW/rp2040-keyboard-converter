# Keyboard Configuration and Layout Definitions

Within this folder you will find all supported Keyboards for this converter.  This includes Layout configurations and also protocol-specific changes.

Keyboards are selected at compilation time, and to use a different keyboard or layout, the firmware will need to be built with that specific keyboard specified.

Each keyboard consists of (at a minumum) 3 files:

| File | Description |
|---|---|
| keyboard.config | Configuration options for this Keyboard. |
| keyboard.c | Layout definitions for this Keyboard. |
| keyboard.h | Header file for the above |

## keyboard.config

This file defines specific Configuration options for this keyboard.  All of the following values are required, and the build will fail if they are missing or reference invalid items:

| Option | Description |
|---|---|
| MAKE | (string) Defines Manufacturer of this Keyboard |
| MODEL | (string) Specific Model Number for this Keyboard |
| DESCRIPTION | (string) Used to describe this keyboard |
| PROTOCOL | (string) Defines which Protocol this keyboard uses* |
| CODESET | (string) Defines which Codeset is in use with this Keyboard* |

* Please refer to [Protocols](/src/protocols/) for a complete list of currently supported Protocols.
* Please refer to [Scancodes](/src/scancodes/) for a complete list of currently supported Scancodes.

*Any extra options specified here are not yet supported.*

## Command Mode Configuration (Optional)

By default, Command Mode is activated by holding **Left Shift + Right Shift** for 3 seconds. This provides access to special functions like bootloader entry without requiring 3-key rollover support.

For keyboards that need different activation keys (e.g., keyboards with only one shift key), you can override the default keys in the keyboard's `keyboard.h` file.

### How to Override Command Mode Keys

Add these defines at the **top** of your `keyboard.h` file, **before** any `#include` statements:

```c
// Override command mode activation keys (optional)
// Both keys must be HID modifier keys (0xE0-0xE7)
#define CMD_MODE_KEY1 KC_LSHIFT  // First activation key
#define CMD_MODE_KEY2 KC_LALT    // Second activation key (example: Alt instead of Right Shift)
```

### Valid Modifier Keys

Command mode keys must be HID modifier keys:

| Constant | Description |
|----------|-------------|
| `KC_LCTRL` | Left Control |
| `KC_LSHIFT` | Left Shift |
| `KC_LALT` | Left Alt |
| `KC_LGUI` | Left GUI/Windows/Command |
| `KC_RCTRL` | Right Control |
| `KC_RSHIFT` | Right Shift (default) |
| `KC_RALT` | Right Alt |
| `KC_RGUI` | Right GUI/Windows/Command |

### Common Override Scenarios

**Single Shift Key Keyboards:**
Some keyboards (like the Apple M0110A) have only one physical shift key or both keys return the same scancode. Use Shift + another modifier:

```c
#define CMD_MODE_KEY1 KC_LSHIFT
#define CMD_MODE_KEY2 KC_LALT    // Shift + Alt
```

**Compact Layouts:**
60% or smaller keyboards where both shifts exist but you want a different combination:

```c
#define CMD_MODE_KEY1 KC_LCTRL
#define CMD_MODE_KEY2 KC_LALT    // Control + Alt
```

**Terminal Keyboards:**
Keyboards with non-standard modifier layouts:

```c
#define CMD_MODE_KEY1 KC_LCTRL
#define CMD_MODE_KEY2 KC_RCTRL   // Both control keys
```

### Validation

The build system validates that both keys are HID modifiers at compile time. If you define invalid keys, you'll get a clear build error:

```
error: static assertion failed: "CMD_MODE_KEY1 must be a HID modifier key (0xE0-0xE7)"
```

### When to Use

Only define these keys if your keyboard:
- Has only one shift key
- Has a layout where both shifts are not easily accessible
- Would benefit from a different key combination

If you don't define these keys, the default (Left Shift + Right Shift) will be used automatically.