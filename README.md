# RP2040 Multi-Protocol Keyboard & Mouse Converter
[![Test Building of Converter Firmware](https://github.com/PaulW/rp2040-keyboard-converter/actions/workflows/ci.yml/badge.svg)](https://github.com/PaulW/rp2040-keyboard-converter/actions/workflows/ci.yml)
![CodeRabbit Pull Request Reviews](https://img.shields.io/coderabbit/prs/github/PaulW/rp2040-keyboard-converter?style=flat)

A USB HID converter for keyboards and mice, using the RP2040's PIO (Programmable I/O) hardware to support multiple protocols.

**Features:**
- Multi-protocol support (AT/PS2, XT, Amiga, Apple M0110)
- Simultaneous keyboard and mouse conversion
- USB HID Boot Protocol for BIOS/UEFI compatibility
- Command Mode for firmware updates without physical access
- Persistent configuration storage

**Quick Links:** [📖 Full Documentation](docs/README.md) | [🚀 Getting Started](docs/getting-started/README.md) | [🔧 Hardware Setup](docs/hardware/README.md) | [💬 Discussions](https://github.com/PaulW/rp2040-keyboard-converter/discussions)

---

## 📚 Documentation

Complete documentation is available in the [`docs/`](docs/) directory. Here's what's covered:

**For Users:**
- [Getting Started](docs/getting-started/README.md) - Setup, building, and flashing firmware
- [Hardware Setup](docs/hardware/README.md) - Wiring diagrams and connector information
- [Supported Protocols](docs/protocols/README.md) - AT/PS2, XT, Amiga, M0110 specifications
- [Supported Keyboards](docs/keyboards/README.md) - Compatible keyboards and configurations
- [Features Guide](docs/features/README.md) - Command Mode, LED support, configuration

**For Developers:**
- [Development Guide](docs/development/README.md) - Contributing, code standards, adding keyboards/protocols
- [Advanced Topics](docs/advanced/README.md) - Architecture, performance analysis, troubleshooting

**Quick Reference:**

| I want to... | See... |
|--------------|--------|
| Build and flash firmware | [Getting Started](docs/getting-started/README.md) |
| Connect my keyboard | [Hardware Setup](docs/hardware/README.md) |
| Check compatibility | [Supported Keyboards](docs/keyboards/README.md) |
| Use Command Mode | [Features Guide](docs/features/README.md) |
| Contribute code | [Development Guide](docs/development/README.md) |

---

## ⚡ Quick Start

### 1. Clone Repository

```bash
git clone --recurse-submodules https://github.com/PaulW/rp2040-keyboard-converter.git
cd rp2040-keyboard-converter
```

### 2. Build Firmware

```bash
# Build Docker container (one-time setup)
docker compose build

# Compile firmware for your keyboard
docker compose run --rm -e KEYBOARD="modelm/enhanced" builder

# Optional: Add mouse support
docker compose run --rm -e KEYBOARD="modelm/enhanced" -e MOUSE="at-ps2" builder
```

### 3. Flash to RP2040

1. Hold **BOOT** button, press **RESET**, release **BOOT**
2. Copy `build/rp2040-converter.uf2` to the `RPI-RP2` drive
3. Device reboots automatically with new firmware

**📖 Detailed Instructions:** See [Getting Started Guide](docs/getting-started/README.md)

---

## 🔌 Supported Hardware

### Protocols

| Protocol | Type | Status | Documentation |
|----------|------|--------|---------------|
| **AT/PS2** | Keyboard & Mouse | ✅ Full Support | [Specification](docs/protocols/at-ps2.md) |
| **XT** | Keyboard | ✅ Full Support | [Specification](docs/protocols/xt.md) |
| **Amiga** | Keyboard | ✅ Full Support | [Specification](docs/protocols/amiga.md) |
| **M0110** | Keyboard | ⚠️ Partial Support | [Specification](docs/protocols/m0110.md) |

### Example Keyboards

| Manufacturer | Models | Protocol |
|--------------|--------|----------|
| **IBM** | Model F (PC/AT), Model M (Enhanced/122-key) | AT/PS2 |
| **Cherry** | G80-0614H, G80-1104H | XT |
| **Commodore** | Amiga A500/A2000 | Amiga |
| **Apple** | M0110/M0110A | M0110 |

**📋 Complete List:** See [Supported Keyboards](docs/keyboards/README.md)

---

## 🛠️ Available Build Configurations

Configure firmware for your specific keyboard when building:

```bash
# IBM keyboards
docker compose run --rm -e KEYBOARD="modelf/pcat" builder
docker compose run --rm -e KEYBOARD="modelm/enhanced" builder
docker compose run --rm -e KEYBOARD="modelm/m122" builder

# Other keyboards
docker compose run --rm -e KEYBOARD="cherry/G80-0614H" builder
docker compose run --rm -e KEYBOARD="amiga/a500" builder
docker compose run --rm -e KEYBOARD="apple/m0110a" builder
```

**🔍 See all configurations:** [`src/keyboards/`](src/keyboards/)

---

## ⚙️ Key Features

### Command Mode
Access firmware functions without physical board access:
- **Bootloader Mode** - Update firmware via USB (press B)
- **Debug Logging** - Adjust UART output level (press D)
- **Factory Reset** - Restore default settings (press F)
- **LED Brightness** - Adjust status LED (press L)

**Activation:** Hold both Shift keys for 3 seconds

**📖 Details:** [Command Mode Documentation](docs/features/command-mode.md)

### Configuration Storage
Settings persist across reboots and firmware updates:
- Log level preferences
- LED brightness
- Future: Macros and key remapping

**📖 Details:** [Configuration Storage](docs/features/README.md#configuration-storage)

### USB HID Boot Protocol
6-key rollover for maximum compatibility:
- Works in BIOS/UEFI
- Compatible with all operating systems
- No driver installation required

**📖 Details:** [USB HID Documentation](docs/features/usb-hid.md)

---

## 🔍 Debugging & Development

For troubleshooting and development information:
- **UART Debug Output** - Connect USB-UART adapter to GPIO 0/1 for diagnostics
- **Command Mode** - Adjust log levels without reflashing (press D in Command Mode)
- **Architecture Details** - See [Advanced Topics](docs/advanced/README.md) for system architecture
- **Development Tools** - See [Development Guide](docs/development/README.md) for lint scripts and testing

---

## 🤝 Contributing

Contributions are welcome! The project welcomes:
- New keyboard/protocol support
- Feature development (NKRO, dynamic keymaps, etc.)
- Hardware designs (PCBs, adapters, cases)
- Documentation improvements

**Before submitting PRs:**
1. Run `./tools/lint.sh` (must pass with zero errors/warnings)
2. Test your configuration builds successfully
3. Review [Development Guide](docs/development/README.md) for coding standards

---

## 📜 License

The project is licensed under **GPLv3** or later. Third-party libraries and code used in this project have their own licenses as follows:

* **Pico-SDK (https://github.com/raspberrypi/pico-sdk)**: License information can be found in the Pico-SDK repository.

* **TinyUSB (https://github.com/hathach/tinyusb)**: License information can be found in the TinyUSB repository. These licenses remain intact in any included portions of code from those shared resources.

* **Ringbuffer implementation (source files: ringbuf.c, etc.)**:
  * Based on code originally created by Hasu@tmk for the TMK Keyboard Firmware project.
  * Potential source references:
    * https://github.com/tmk/tinyusb_ps2/blob/main/ringbuf.h
    * https://github.com/tmk/tmk_keyboard/blob/master/tmk_core/ring_buffer.h
  * License: Likely GPLv2 or later (same as TMK) due to its inclusion in the TMK repository, but the exact origin is unclear.
