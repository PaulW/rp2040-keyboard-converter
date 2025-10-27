# Supported Keyboards

**Status**: 🔄 In Progress | **Last Updated**: 27 October 2025

Complete list of supported keyboards and configuration guides.

## Supported Keyboards

### IBM Model M (AT/PS2 Protocol)

| Model | Description | Codeset | Layout | Status |
|-------|-------------|---------|--------|--------|
| **[Enhanced 1391401](ibm/model-m-enhanced.md)** | 101-key enhanced keyboard | Set 2 | US ANSI | ✅ Tested |
| **[Terminal 1392595](ibm/model-m-terminal.md)** | 122-key terminal keyboard | Set 2 | US Terminal | ✅ Tested |
| **[Space Saver](ibm/model-m-spacesaver.md)** | 84-key compact keyboard | Set 2 | US Compact | 🔄 In Progress |

### IBM Model F (Multiple Protocols)

| Model | Description | Protocol | Codeset | Status |
|-------|-------------|----------|---------|--------|
| **[PC/AT](ibm/model-f-pcat.md)** | 84-key AT keyboard | AT/PS2 | Set 2 | ✅ Tested |
| **[XT](ibm/model-f-xt.md)** | 83-key XT keyboard | XT | XT Set | ✅ Tested |
| **[4704](ibm/model-f-4704.md)** | Terminal keyboard | Custom | Custom | ⏳ Planned |

### Amiga Keyboards

| Model | Description | Codeset | Status |
|-------|-------------|---------|--------|
| **[Amiga 500/600/1200](amiga/amiga-standard.md)** | Standard Amiga keyboard | Amiga Set | ✅ Tested |
| **[Amiga 2000/3000/4000](amiga/amiga-enhanced.md)** | Enhanced Amiga keyboard | Amiga Set | ✅ Tested |

### Apple Keyboards

| Model | Description | Codeset | Status |
|-------|-------------|---------|--------|
| **[M0110](apple/m0110.md)** | Original Macintosh keyboard | M0110 Set | ✅ Tested |
| **[M0110A](apple/m0110a.md)** | Enhanced Macintosh keyboard | M0110 Set | ✅ Tested |

---

## Quick Configuration Guide

### Building Firmware for Your Keyboard

Use the `KEYBOARD` environment variable to specify your keyboard:

```bash
# IBM Model M Enhanced (101-key)
docker compose run --rm -e KEYBOARD="modelm/enhanced" builder

# IBM Model F PC/AT (84-key)
docker compose run --rm -e KEYBOARD="modelf/pcat" builder

# IBM Model F XT (83-key)
docker compose run --rm -e KEYBOARD="modelf/xt" builder

# Amiga keyboard
docker compose run --rm -e KEYBOARD="amiga/standard" builder

# Apple M0110A keyboard
docker compose run --rm -e KEYBOARD="apple/m0110a" builder
```

**See:** [Building Firmware Guide](../getting-started/building-firmware.md)

---

### Adding Mouse Support

Mouse support is independent of keyboard protocol:

```bash
# Keyboard + Mouse
docker compose run --rm -e KEYBOARD="modelm/enhanced" -e MOUSE="at-ps2" builder
```

**See:** [AT/PS2 Mouse Guide](../protocols/at-ps2-mouse.md)

---

## Keyboard Details

### IBM Model M Enhanced (1391401)

**Specifications:**
- **Keys**: 101 (US ANSI layout)
- **Protocol**: AT/PS2
- **Codeset**: Scancode Set 2
- **Connector**: SDL (6-pin mini-DIN, same as PS/2)
- **Voltage**: 5V
- **Layout**: Standard enhanced keyboard

**Features:**
- Full NKRO support
- LED indicators (Caps Lock, Num Lock, Scroll Lock)
- Command mode: Left Shift + Right Shift

**See:** [Model M Enhanced Guide](ibm/model-m-enhanced.md)

---

### IBM Model F PC/AT

**Specifications:**
- **Keys**: 84
- **Protocol**: AT/PS2
- **Codeset**: Scancode Set 2
- **Connector**: DIN-5
- **Voltage**: 5V
- **Layout**: AT layout

**Features:**
- Capacitive buckling spring switches
- Full NKRO support
- LED indicators
- Custom PCB available

**See:** [Model F PC/AT Guide](ibm/model-f-pcat.md)

---

### IBM Model F XT

**Specifications:**
- **Keys**: 83
- **Protocol**: XT
- **Codeset**: XT Scancode Set
- **Connector**: DIN-5
- **Voltage**: 5V
- **Layout**: XT layout

**Features:**
- Capacitive buckling spring switches
- Unidirectional protocol (no LED control)
- Full NKRO support

**See:** [Model F XT Guide](ibm/model-f-xt.md)

---

### Amiga Keyboards

**Specifications:**
- **Keys**: 91-95 (model-dependent)
- **Protocol**: Amiga
- **Codeset**: Amiga Scancode Set
- **Connector**: Proprietary 5-pin
- **Voltage**: 5V
- **Layout**: Amiga-specific

**Features:**
- Unique handshake protocol
- Caps Lock synchronization
- Reset line
- Full NKRO support

**See:** [Amiga Keyboard Guide](amiga/amiga-standard.md)

---

### Apple M0110/M0110A

**Specifications:**
- **Keys**: 58 (M0110), 78 (M0110A)
- **Protocol**: Apple M0110
- **Codeset**: M0110 Scancode Set
- **Connector**: Proprietary 4-pin
- **Voltage**: 5V
- **Layout**: Compact (M0110), Enhanced (M0110A)

**Features:**
- Poll-based protocol
- No LED indicators
- Compact design
- Command mode override (single shift key)

**See:** [M0110/M0110A Guide](apple/m0110.md)

---

## Keyboard Configuration Files

Each keyboard has a configuration file defining its protocol, codeset, and layout:

**Location**: `src/keyboards/<brand>/<model>/keyboard.config`

**Example** (`src/keyboards/modelm/enhanced/keyboard.config`):
```cmake
set(KEYBOARD_PROTOCOL "at-ps2")
set(KEYBOARD_CODESET "set2")
set(KEYBOARD_LAYOUT "us_ansi")
```

**Components:**
- `KEYBOARD_PROTOCOL`: Communication protocol (at-ps2, xt, amiga, m0110)
- `KEYBOARD_CODESET`: Scancode mapping (set2, xt, amiga, m0110)
- `KEYBOARD_LAYOUT`: Physical key layout (us_ansi, us_terminal, at_84, etc.)

---

## Adding New Keyboards

To add support for a new keyboard:

1. **Identify**: Protocol, codeset, and connector type
2. **Create**: Configuration directory (`src/keyboards/<brand>/<model>/`)
3. **Configure**: Add `keyboard.config` with protocol/codeset/layout
4. **Keymap**: Define key layout in `keyboard.c` using `KEYMAP_*()` macros
5. **Test**: Build and test with real hardware

**See:** [Adding Keyboards Guide](../development/adding-keyboards.md)

---

## Custom Key Overrides

Some keyboards need special configurations (e.g., single shift key):

**Example** (Apple M0110A - single shift key):
```c
// keyboards/apple/m0110a/keyboard.h
#define CMD_MODE_KEY1 KC_LSHIFT
#define CMD_MODE_KEY2 KC_LALT     // Override: Shift + Alt
#include "hid_keycodes.h"
```

**Key Point**: Define overrides **before** including `hid_keycodes.h` (first definition wins).

---

## Command Mode Keys

Default: **Left Shift + Right Shift** (enter bootloader)

**Override if**:
- Keyboard has single shift key
- Custom key combination preferred
- Layout makes default awkward

**See:** [Command Mode Guide](../features/command-mode.md)

---

## Related Documentation

**In This Documentation:**
- [Getting Started](../getting-started/README.md) - Setup guide
- [Protocols](../protocols/README.md) - Protocol details
- [Hardware](../hardware/README.md) - Connection details
- [Building Firmware](../getting-started/building-firmware.md) - Build process

**Source Code:**
- Keyboard configurations: [`src/keyboards/`](../../src/keyboards/)
- Keymap macros: [`src/keyboards/*/keyboard.c`](../../src/keyboards/)

---

## Need Help?

- 📖 [Troubleshooting](../advanced/troubleshooting.md)
- 💬 [Ask Questions](https://github.com/PaulW/rp2040-keyboard-converter/discussions)
- 🐛 [Report Keyboard Issues](https://github.com/PaulW/rp2040-keyboard-converter/issues)
- 🆕 [Request Keyboard Support](https://github.com/PaulW/rp2040-keyboard-converter/discussions/new?category=keyboard-requests)

---

**Status**: Documentation in progress. Keyboard-specific guides coming soon!
