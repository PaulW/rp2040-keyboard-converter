# Documentation

**RP2040 Multi-Protocol Keyboard & Mouse Converter**

The RP2040 Multi-Protocol Keyboard & Mouse Converter lets you use vintage keyboards and mice with modern computers via USB. It originally started out as a converter for an IBM Model F PC/AT Keyboard I found in my parents' attic, and has since expanded to support a growing list of protocols, so you can connect a variety of vintage keyboards and mice to modern systems.

This documentation aims to cover everything from initial setup and configuration, through to hardware requirements, the different protocols supported, available features, and how to contribute to the project if you're interested. It may not cover absolutely everything, but should help you understand how it all works and get you up and running.

---

## 🚀 Getting Started

If you're new to the project, or just want to get up and running quickly, the [Quick Start Guide](getting-started/README.md) will walk you through everything from basic hardware setup to building and flashing the firmware.

## 🔧 Hardware

I've used a Waveshare RP2040-Zero for this project, as that's what I already had to hand, but any RP2040-based board should work just fine (I think the newer Pico 2 with the RP2350 should work too, though I've not tested that yet!). The Raspberry Pi Foundation's done some great work getting people into electronics and programming over the years, and with the release of the RP2040, I felt this would be a good learning experience. The RP2040's PIO (Programmable I/O) hardware is particularly well-suited for implementing the precise timing requirements of the protocols—that abstraction of how it works is really quite nice.

- **[Hardware Overview](hardware/README.md)** - In-depth hardware guide with wiring diagrams
- **[Custom PCB](hardware/custom-pcb.md)** - Custom PCB design for IBM Model F

## 📡 Protocols

I've been working on adding support for a variety of protocols used by different devices, and I'm planning to expand this further as I get hold of more keyboards or mice to test with. The [Protocol Overview](protocols/README.md) provides details on what's currently supported, along with technical details on how each protocol is implemented.

### ⌨️ Keyboards

I've documented details about specific devices I currently own and have used to develop this project. This isn't just a list of what's supported—it should give you an idea of what's possible and help you create your own configurations for your specific device. Check out [Supported Keyboards](keyboards/README.md) for more information.

### ✨ Features

As the project's evolved, I've added various features to make using the converter a bit more streamlined. This will continue to evolve over time as new features are added, but current documentation can be found in the [Features Documentation](features/README.md).

### 🎓 Advanced Topics

If you're interested in understanding how the converter works, or need more specific information, I've put together some more technical information which covers things like the architecture, performance characteristics, and inner workings:

- **[Architecture Overview](advanced/README.md)** - System architecture, performance, PIO programming, build system, and troubleshooting

### 👨‍💻 Development

If you're interested in contributing to the project, or want to understand the codebase better, the development guide covers everything you need to know about the project's structure, coding standards, and how to add support for new devices or protocols:

- **[Development Guide](development/README.md)** - Contributing guidelines, code standards, testing, and architecture decisions

---

## 🔍 Quick Reference

### Common Tasks

| I want to... | Go to... |
|--------------|----------|
| **Get started quickly** | [Quick Start Guide](getting-started/README.md) |
| **Build the firmware** | [Building Firmware](getting-started/building-firmware.md) |
| **Connect my keyboard** | [Hardware Setup](getting-started/hardware-setup.md) |
| **See what keyboards are already supported** | [Supported Keyboards](keyboards/README.md) |
| **Check supported Protocols** | [Protocol Overview](protocols/README.md) |
| **Understand architecture** | [Advanced Topics](advanced/README.md) |
| **Add a new keyboard** | [Development Guide](development/README.md) |
| **Contribute code** | [Development Guide](development/README.md) |

---

## 🔗 External Resources

- **[Main README](../README.md)** - Project overview
- **[Source Code](../src/)** - Firmware source code
- **[Pico SDK Documentation](https://www.raspberrypi.com/documentation/pico-sdk/)** - RP2040 SDK
- **[TinyUSB Documentation](https://docs.tinyusb.org/)** - USB stack
- **[GitHub Issues](https://github.com/PaulW/rp2040-keyboard-converter/issues)** - Report bugs or request features
- **[GitHub Discussions](https://github.com/PaulW/rp2040-keyboard-converter/discussions)** - Ask questions
