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

- **[Hardware Overview](hardware/README.md)** - Complete hardware guide with wiring diagrams
- **[Custom PCB](hardware/custom-pcb.md)** - Custom PCB design for IBM Model F

### 📡 Protocols

Supported keyboard and mouse protocols:

- **[Protocol Overview](protocols/README.md)** - All supported protocols
- **[AT/PS2 Protocol](protocols/at-ps2.md)** - IBM PC/AT and PS/2 keyboards/mice (includes mouse support)
- **[XT Protocol](protocols/xt.md)** - IBM PC/XT keyboards
- **[Amiga Protocol](protocols/amiga.md)** - Commodore Amiga keyboards
- **[Apple M0110 Protocol](protocols/m0110.md)** - Apple M0110/M0110A keyboards

### ⌨️ Keyboards

Information about supported keyboards:

- **[Supported Keyboards](keyboards/README.md)** - Complete keyboard list and configuration guide

### ✨ Features

Key features and how to use them:

- **[Features Overview](features/README.md)** - All available features including Command Mode, LED control, and USB compatibility

### 🎓 Advanced Topics

Deep dives into technical details:

- **[Architecture Overview](advanced/README.md)** - System architecture, performance, PIO programming, build system, and troubleshooting

### 👨‍💻 Development

For contributors and developers:

- **[Development Guide](development/README.md)** - Contributing guidelines, code standards, testing, and architecture decisions

---

## 🔍 Quick Find

### Common Tasks

| I want to... | Go to... |
|--------------|----------|
| **Get started quickly** | [Quick Start Guide](getting-started/README.md) |
| **Build the firmware** | [Building Firmware](getting-started/building-firmware.md) |
| **Connect my keyboard** | [Hardware Setup](getting-started/hardware-setup.md) |
| **Check if my keyboard is supported** | [Supported Keyboards](keyboards/README.md) |
| **Understand performance** | [Architecture Overview](advanced/README.md) |
| **Add a new keyboard** | [Development Guide](development/README.md) |
| **Troubleshoot problems** | [Architecture Overview](advanced/README.md) |
| **Contribute code** | [Development Guide](development/README.md) |

### By Protocol

| Protocol | Documentation |
|----------|---------------|
| AT/PS2 | [AT/PS2 Protocol](protocols/at-ps2.md) |
| XT | [XT Protocol](protocols/xt.md) |
| Amiga | [Amiga Protocol](protocols/amiga.md) |
| Apple M0110 | [Apple M0110 Protocol](protocols/m0110.md) |

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

**Status**: ✅ 60% Complete (Getting Started + Protocols verified)  
**Last Updated**: 30 October 2025  
**Maintained By**: Development Team

This documentation is version-controlled alongside the code. If you find errors or have suggestions, please [open an issue](https://github.com/PaulW/rp2040-keyboard-converter/issues) or submit a pull request.

---

**Happy Converting! ⌨️→🔌→💻**
