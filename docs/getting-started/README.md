# Getting Started

**Status**: 🔄 In Progress | **Last Updated**: 27 October 2025

Welcome! This guide will help you get your RP2040 Keyboard Converter up and running.

## Quick Start

Choose your path:

1. **[Hardware Setup](hardware-setup.md)** - Connect your keyboard to the RP2040
2. **[Building Firmware](building-firmware.md)** - Compile the firmware from source
3. **[Flashing Firmware](flashing-firmware.md)** - Install firmware on your RP2040

---

## What You'll Need

### Hardware
- RP2040 board (Raspberry Pi Pico, WaveShare RP2040-Zero, or similar)
- Vintage keyboard or mouse
- Bi-directional level shifter (for 5V keyboards)
- Appropriate connector for your keyboard
- USB cable (Type-A to Micro-USB or Type-C, depending on your RP2040 board)

### Software
- Docker (recommended) or native build tools
- Git
- Text editor or IDE

---

## Overview

The RP2040 Keyboard Converter allows you to use vintage keyboards and mice on modern computers via USB. The converter handles protocol translation and provides USB HID functionality.

**Key Features:**
- Multiple protocol support (AT/PS2, XT, Amiga, Apple M0110)
- Simultaneous keyboard and mouse conversion
- USB HID Boot Protocol for BIOS compatibility
- Persistent configuration storage
- Command mode for testing and configuration

---

## Next Steps

1. **[Set up your hardware](hardware-setup.md)** - Physical connections
2. **[Build the firmware](building-firmware.md)** - Compile for your keyboard
3. **[Flash your RP2040](flashing-firmware.md)** - Install the firmware

---

## Documentation Structure

- **[Hardware Setup](hardware-setup.md)** - Wiring and connections
- **[Building Firmware](building-firmware.md)** - Docker and native builds
- **[Flashing Firmware](flashing-firmware.md)** - UF2 bootloader mode

---

## Need Help?

- 📖 [Troubleshooting Guide](../advanced/troubleshooting.md)
- 💬 [GitHub Discussions](https://github.com/PaulW/rp2040-keyboard-converter/discussions)
- 🐛 [Report an Issue](https://github.com/PaulW/rp2040-keyboard-converter/issues)

---

**Status**: Documentation in progress. Check back for updates!
