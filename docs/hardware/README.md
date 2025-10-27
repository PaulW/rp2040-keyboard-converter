# Hardware Documentation

**Status**: 🔄 In Progress | **Last Updated**: 27 October 2025

This section covers everything you need to know about building hardware for the RP2040 Keyboard Converter. Whether you're prototyping on a breadboard, designing a custom PCB, or planning an internal keyboard installation, you'll find detailed information about components, connections, and practical considerations.

## Available Documentation

### Hardware Guides

- **[Breadboard Prototype](breadboard-prototype.md)** - Simple breadboard setup for testing
- **[Custom PCB](custom-pcb.md)** - Custom PCB design for IBM Model F PC/AT
- **[Pin Configurations](pin-configurations.md)** - GPIO pin assignments by protocol
- **[Level Shifters](level-shifters.md)** - 5V ↔ 3.3V level shifting requirements

---

## Hardware Options

### 1. Breadboard Prototype

Best for: Testing, development, multiple keyboards

**Pros:**
- ✅ Quick to set up
- ✅ Easy to modify
- ✅ Low cost
- ✅ Reusable components

**Cons:**
- ❌ Not permanent
- ❌ External box needed
- ❌ More wiring

**See:** [Breadboard Prototype Guide](breadboard-prototype.md)

---

### 2. Custom PCB

Best for: Permanent installation, specific keyboard

**Pros:**
- ✅ Professional appearance
- ✅ Internal installation (no external box)
- ✅ Reliable connections
- ✅ Compact design

**Cons:**
- ❌ Higher initial cost
- ❌ Requires PCB fabrication
- ❌ Less flexible

**See:** [Custom PCB Guide](custom-pcb.md)

---

## Component Requirements

### Essential Components

| Component | Purpose | Notes |
|-----------|---------|-------|
| **RP2040 Board** | Main microcontroller | Pico, WaveShare RP2040-Zero, or compatible |
| **Level Shifter** | 5V ↔ 3.3V conversion | Bi-directional (e.g., BSS138-based) |
| **Connector** | Keyboard/mouse connection | Depends on protocol (PS/2, DIN-5, SDL, etc.) |
| **USB Cable** | Power and data | Type-A to Micro-USB or Type-C |

### Optional Components

| Component | Purpose | Notes |
|-----------|---------|-------|
| **LED** | Status indicator | Can use built-in RP2040 LED |
| **WS2812 RGB LED** | Enhanced status | Optional, configurable |
| **Enclosure** | Protection | 3D printed or commercial project box |

---

## Supported RP2040 Boards

### Recommended Boards

| Board | Features | Notes |
|-------|----------|-------|
| **Raspberry Pi Pico** | Standard, widely available | Most common choice |
| **WaveShare RP2040-Zero** | Smaller form factor, USB-C | Compact design |
| **Adafruit Feather RP2040** | Battery support, additional features | Good for portable projects |
| **Pimoroni Tiny 2040** | Very small, RGB LED | Compact with built-in LED |

All boards with RP2040 microcontroller are compatible. Choose based on your size, connector, and feature requirements.

---

## Connection Overview

```
Keyboard (5V) <---> Level Shifter <---> RP2040 (3.3V) <---> USB <---> Computer
    CLK/DAT             HV/LV            GPIO Pins
```

**Key Points:**
- RP2040 GPIO pins are 3.3V (NOT 5V tolerant)
- Most vintage keyboards output 5V signals
- Level shifters are REQUIRED for 5V keyboards
- Both CLK and DATA lines need level shifting

**See:** [Level Shifters Guide](level-shifters.md) for detailed information.

---

## GPIO Pin Assignments

Pin assignments vary by protocol. Each keyboard configuration file defines its GPIO pins.

**Example (AT/PS2 protocol):**
- Keyboard CLK: GPIO 17
- Keyboard DATA: GPIO 16

**See:** [Pin Configurations](pin-configurations.md) for complete pin mapping.

---

## Related Documentation

**In This Documentation:**
- [Getting Started](../getting-started/README.md) - Setup guide
- [Protocols](../protocols/README.md) - Protocol details
- [Keyboards](../keyboards/README.md) - Supported keyboards

**Source Code:**
- Keyboard configurations: [`src/keyboards/`](../../src/keyboards/)
- Protocol implementations: [`src/protocols/`](../../src/protocols/)

---

## Need Help?

- 📖 [Troubleshooting](../advanced/troubleshooting.md)
- 💬 [Ask Questions](https://github.com/PaulW/rp2040-keyboard-converter/discussions)
- 🐛 [Report Hardware Issues](https://github.com/PaulW/rp2040-keyboard-converter/issues)

---

**Status**: Documentation in progress. More guides coming soon!
