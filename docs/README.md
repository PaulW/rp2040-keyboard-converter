# Documentation

**RP2040 Multi-Protocol Keyboard & Mouse Converter**

Welcome to the comprehensive documentation for the RP2040 Keyboard Converter project. This documentation covers everything from getting started to advanced technical details.

---

## 📚 Documentation Structure

### 🚀 Getting Started

New to the project? Start here:

- **[Quick Start Guide](getting-started/README.md)** - Get up and running quickly
- **[Hardware Setup](getting-started/hardware-setup.md)** - Physical connections and wiring
- **[Building Firmware](getting-started/building-firmware.md)** - How to compile the firmware
- **[Flashing Firmware](getting-started/flashing-firmware.md)** - How to install firmware on your RP2040

### 🔧 Hardware

Everything about the physical hardware:

- **[Hardware Overview](hardware/README.md)** - Complete hardware guide
- **[Breadboard Prototype](hardware/breadboard-prototype.md)** - Simple breadboard setup
- **[Custom PCB](hardware/custom-pcb.md)** - Custom PCB design for IBM Model F
- **[Pin Configurations](hardware/pin-configurations.md)** - GPIO pin assignments
- **[Level Shifters](hardware/level-shifters.md)** - 5V ↔ 3.3V conversion

### 📡 Protocols

Supported keyboard and mouse protocols:

- **[Protocol Overview](protocols/README.md)** - All supported protocols
- **[AT/PS2 Protocol](protocols/at-ps2.md)** - IBM PC/AT and PS/2 keyboards/mice
- **[XT Protocol](protocols/xt.md)** - IBM PC/XT keyboards
- **[Amiga Protocol](protocols/amiga.md)** - Commodore Amiga keyboards
- **[Apple M0110 Protocol](protocols/apple-m0110.md)** - Apple M0110/M0110A keyboards
- **[Mouse Support](protocols/mouse-support.md)** - Mouse protocol details

### ⌨️ Keyboards

Information about supported keyboards:

- **[Supported Keyboards](keyboards/README.md)** - Complete keyboard list
- **[Keyboard Configuration](keyboards/configuration.md)** - How to configure keyboards
- **[Adding New Keyboards](keyboards/adding-keyboards.md)** - Guide for adding support
- **[Scancode Sets](keyboards/scancode-sets.md)** - Understanding scancode sets

### ✨ Features

Key features and how to use them:

- **[Features Overview](features/README.md)** - All available features
- **[Command Mode](features/command-mode.md)** - Interactive command mode
- **[Keytest Mode](features/keytest-mode.md)** - Keyboard testing utility
- **[Configuration Storage](features/configuration-storage.md)** - Persistent settings
- **[LED Control](features/led-control.md)** - LED brightness and patterns
- **[USB Compatibility](features/usb-compatibility.md)** - USB HID and BIOS mode

### 🎓 Advanced Topics

Deep dives into technical details:

- **[Architecture Overview](advanced/README.md)** - System architecture
- **[Performance Characteristics](advanced/performance.md)** - Speed and latency
- **[PIO Programming](advanced/pio-programming.md)** - PIO state machines explained
- **[Build System](advanced/build-system.md)** - CMake configuration
- **[Troubleshooting](advanced/troubleshooting.md)** - Common problems and solutions

### 👨‍💻 Development

For contributors and developers:

- **[Development Guide](development/README.md)** - Contributing to the project
- **[Contributing Guidelines](development/contributing.md)** - How to contribute
- **[Code Standards](development/code-standards.md)** - Coding conventions
- **[Testing Guide](development/testing.md)** - How to test changes
- **[Architecture Decisions](development/architecture-decisions.md)** - Why we made certain choices

---

## 🔍 Quick Find

### Common Tasks

| I want to... | Go to... |
|--------------|----------|
| **Get started quickly** | [Quick Start Guide](getting-started/README.md) |
| **Build the firmware** | [Building Firmware](getting-started/building-firmware.md) |
| **Connect my keyboard** | [Hardware Setup](getting-started/hardware-setup.md) |
| **Check if my keyboard is supported** | [Supported Keyboards](keyboards/README.md) |
| **Understand performance** | [Performance Characteristics](advanced/performance.md) |
| **Test my keyboard** | [Keytest Mode](features/keytest-mode.md) |
| **Add a new keyboard** | [Adding New Keyboards](keyboards/adding-keyboards.md) |
| **Troubleshoot problems** | [Troubleshooting](advanced/troubleshooting.md) |
| **Contribute code** | [Contributing Guidelines](development/contributing.md) |

### By Protocol

| Protocol | Documentation |
|----------|---------------|
| AT/PS2 | [AT/PS2 Protocol](protocols/at-ps2.md) |
| XT | [XT Protocol](protocols/xt.md) |
| Amiga | [Amiga Protocol](protocols/amiga.md) |
| Apple M0110 | [Apple M0110 Protocol](protocols/apple-m0110.md) |

---

## 📖 Documentation Standards

All documentation in this project follows these guidelines:

- ✅ **Up-to-Date** - Documentation is updated with code changes
- ✅ **Accurate** - All information is tested and verified
- ✅ **Complete** - Covers current implementation (no experimental features)
- ✅ **User-Focused** - Written for users and developers, not just maintainers
- ✅ **Well-Organised** - Clear structure with good navigation

---

## 🔗 External Resources

- **[Main README](../README.md)** - Project overview
- **[Source Code](../src/)** - Firmware source code
- **[Pico SDK Documentation](https://www.raspberrypi.com/documentation/pico-sdk/)** - RP2040 SDK
- **[TinyUSB Documentation](https://docs.tinyusb.org/)** - USB stack
- **[GitHub Issues](https://github.com/PaulW/rp2040-keyboard-converter/issues)** - Report bugs or request features
- **[GitHub Discussions](https://github.com/PaulW/rp2040-keyboard-converter/discussions)** - Ask questions

---

## 📝 About This Documentation

**Status**: 🔄 In Progress  
**Last Updated**: 27 October 2025  
**Maintained By**: Development Team

This documentation is version-controlled alongside the code. If you find errors or have suggestions, please [open an issue](https://github.com/PaulW/rp2040-keyboard-converter/issues) or submit a pull request.

---

**Happy Converting! ⌨️→🔌→💻**
