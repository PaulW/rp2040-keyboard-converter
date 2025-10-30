# Getting Started

**Status**: 🔄 In Progress | **Last Updated**: 29 October 2025

Welcome to the RP2040 Keyboard Converter! This project brings vintage keyboards and mice back to life by converting their original protocols to USB. Whether you're a retrocomputing enthusiast, a mechanical keyboard collector, or simply curious about vintage peripherals, this guide will help you get your classic hardware working with modern computers.

## Why Convert Vintage Peripherals?

Vintage peripherals from the 1980s and early 1990s are prized by enthusiasts for good reasons. Keyboards like the IBM Model M and Model F feature buckling spring switches that provide exceptional tactile feedback and durability—many units from the 1980s are still fully functional today, outlasting modern keyboards by decades. The mechanical switches, key travel, and acoustic properties create a typing experience that many users prefer over modern alternatives. Similarly, classic mice like those from the Amiga era offer unique ergonomics and feel.

These devices also represent important milestones in computing history. Using original hardware provides an authentic connection to the early PC era and helps preserve computing heritage.

However, there's a practical challenge: vintage peripherals use communication protocols and connectors that modern computers no longer support. These older protocols were replaced by USB in the late 1990s, leaving classic hardware incompatible with contemporary systems.

This project provides a non-invasive bridge between vintage protocols and modern USB. The converter handles all protocol translation automatically, preserving your original hardware while enabling full functionality on today's computers. Your vintage peripherals appear to the computer as standard USB devices that work with any operating system.

## What Makes This Project Different?

While other converters exist (like TMK, QMK, and Soarer's Converter), this project offers some unique advantages by leveraging the RP2040 microcontroller's capabilities.

The RP2040's PIO (Programmable I/O) state machines handle protocol timing and bit-level operations in dedicated hardware, providing microsecond-precision timing without consuming CPU cycles. This ensures reliable operation even during intensive USB traffic. The flexible PIO architecture also makes adding new protocols straightforward—no hardware changes are needed, just flash different firmware.

You can convert both a keyboard and mouse simultaneously using independent hardware state machines. Each protocol runs in parallel without interference, and you can even mix protocols if needed (such as using different protocols for keyboard and mouse).

The project uses the official Raspberry Pi Pico SDK and TinyUSB, benefiting from active maintenance and excellent documentation. Docker-based builds ensure consistency across platforms without complex toolchain setup. And with RP2040 boards costing just £4-8 and being widely available from multiple manufacturers, getting started is both affordable and accessible.

The well-documented code and comprehensive architecture guides also make this project valuable for learning about embedded systems, hardware protocols, state machine design, and USB HID implementation.

## Quick Start

Choose your starting point based on what you want to accomplish:

### If you want to connect your device to the RP2040...
Start with the **[Hardware Setup Guide](hardware-setup.md)**.

**You'll need:**
- A vintage keyboard or mouse
- An RP2040 board (Raspberry Pi Pico, WaveShare RP2040-Zero, or similar)
- Basic electronics knowledge
- A level shifter module (5V ↔ 3.3V conversion)

This guide provides a step-by-step walkthrough for wiring your device to the RP2040 board. You'll get a clear parts list, see exactly how to connect everything on a breadboard, and verify your setup works before moving on to firmware. For detailed technical explanations about voltage levels and component selection, see the [Hardware Documentation](../hardware/README.md).

### If you want to compile firmware for your device...
Follow the **[Building Firmware Guide](building-firmware.md)**.

**You'll need:**
- Docker installed on your computer
- Git to clone the repository
- 15-30 minutes for first build (<5 minutes for subsequent builds)

This guide explains the Docker-based build process, which provides a consistent environment regardless of your operating system. You'll learn how to specify your device configuration, enable optional features like simultaneous keyboard and mouse support, and understand the build output. The Docker approach eliminates toolchain configuration headaches—no need to manually install ARM compilers, CMake, or manage SDK versions.

### If you want to install firmware on your RP2040...
Complete the **[Flashing Firmware Guide](flashing-firmware.md)**.

**You'll need:**
- Compiled firmware (.uf2 file)
- USB cable to connect RP2040 to your computer
- 5 minutes

This guide covers the UF2 drag-and-drop flashing process, using Command Mode for convenient firmware updates without physical access to the board, and troubleshooting steps if something doesn't work as expected.

---

## What You'll Need

Setting up a device converter requires some hardware components and basic technical knowledge. The process is straightforward, and we'll guide you through each step.

### Hardware Requirements

#### Essential Components

You'll need four essential components to build your converter.

First, you need an **RP2040 microcontroller board** (£4-8), which is the brain of the converter. Any RP2040-based board will work—the Raspberry Pi Pico is the official option and widely available, while the WaveShare RP2040-Zero offers a more compact design with USB-C. Other options like the Adafruit Feather RP2040 or Pimoroni Tiny 2040 provide additional features like battery support or built-in RGB LEDs. All boards provide the same core functionality, so choose based on your size, connector, and feature preferences.

Second, a **bi-directional level shifter** (£2-5) is absolutely essential for safe operation. The RP2040's GPIO pins operate at 3.3V and connecting them directly to vintage devices (which output 5V logic levels) will permanently damage the microcontroller. BSS138-based 4-channel modules from Adafruit or SparkFun are reliable and support the bi-directional communication that many protocols require. Avoid the 74LVC245 chip—it's a unidirectional bus transceiver and won't work with bidirectional protocols.

Third, you need a **connector** (£2-10) that matches your device. Most keyboards use PS/2 (6-pin mini-DIN) connectors, though older keyboards might use DIN-5 connectors. Mice typically use PS/2 connectors as well. Some devices like Amiga keyboards or Apple M0110 use proprietary connectors that may require custom cables. Consider purchasing pre-made cables if available—they eliminate tedious connector soldering.

Finally, a **USB cable** (£1-5) connects the RP2040 to your computer. Match your board's connector type (USB Micro-B for Raspberry Pi Pico, USB-C for WaveShare RP2040-Zero). Choose a reliable cable—poor-quality cables can cause intermittent connection issues that are frustrating to debug.

#### Optional Components

For prototyping and testing, a breadboard and jumper wires (£5-10) make it easy to experiment before building a permanent solution. A half-size breadboard with 400 tie-points is sufficient.

An enclosure (£5-20) protects your converter and provides a professional appearance. You can use 3D printed cases (many designs available online), commercial project boxes from manufacturers like Hammond or Takachi, or design custom enclosures for internal device mounting.

The firmware supports WS2812B addressable RGB LEDs (£1-2) for visual status indication, useful for debugging or visual feedback about Command Mode. This is entirely optional and can be disabled in firmware.

### Software Requirements

The project uses **Docker** to provide a consistent build environment across all platforms. This eliminates toolchain configuration headaches—you don't need to manually install ARM compilers, CMake, or manage SDK versions. 

- **Installation**: [Docker Desktop](https://www.docker.com/products/docker-desktop) (macOS, Windows) or Docker Engine (Linux)
- **Requirements**: ~2GB disk space for the Docker image, at least 2GB RAM available

You'll also need **Git** to clone the repository and manage updates. Installation is straightforward through platform package managers (`brew install git`, `apt install git`, `winget install Git.Git`) or the [official Git website](https://git-scm.com/downloads).

A text editor or IDE is optional for basic firmware builds, but helpful if you plan to modify configurations or create custom keymaps. Visual Studio Code is a good free option with excellent C/C++ support.

**Operating System Compatibility:**
- ✅ macOS (Apple Silicon and Intel)
- ✅ Linux (all major distributions)
- ✅ Windows 10/11 (WSL2 recommended for best Docker performance)

### Technical Skills

You should have a basic understanding of electronics—knowing the difference between 3.3V and 5V logic, how to identify power and ground pins, and basic breadboard usage. If this sounds intimidating, don't worry. Many online tutorials cover these basics, and the converter circuit is quite simple with only 4-6 connections total.

Soldering skills are only required if you're building a permanent installation. For breadboard prototypes, everything uses jumper wires and pre-assembled modules.

Basic command line familiarity is helpful—being able to navigate directories, run commands, and copy files. All necessary commands are provided in the guides with explanations, so you can copy and paste them directly.

---

## What This Converter Does

Once set up, the converter provides functionality that goes beyond simple protocol translation.

It supports 6-key rollover (6KRO) via USB HID Boot Protocol, reporting up to 6 simultaneous key presses plus modifiers without ghosting. This is sufficient for fast typing and most keyboard shortcuts. The Boot Protocol ensures maximum compatibility with all systems, including BIOS/UEFI firmware and operating systems—no drivers required, it just works immediately on any system.

For protocols that support it, keyboard LEDs automatically synchronise with the host computer. Your Caps Lock, Num Lock, and Scroll Lock LEDs will accurately reflect your computer's state without any manual intervention.

Configuration settings like log level and LED brightness are stored in flash memory, surviving power cycles and firmware updates until you explicitly change them.

The converter also includes Command Mode—a special firmware management mode accessible via a keyboard combination during boot. This provides convenient access to features like bootloader entry for reflashing firmware without physical access to the board, log level control for troubleshooting, LED brightness adjustment, and factory reset.

Built-in UART logging provides detailed diagnostic information when you need it. Connect a USB-UART adapter to see protocol state, scancode sequences, and error conditions—invaluable for troubleshooting hardware issues or understanding converter behaviour.

---

## How It All Connects

Here's a simplified view of how everything fits together:

```
┌─────────────────┐        ┌───────────────┐        ┌────────────┐
│ Vintage Device  │◄──────►│ Level Shifter │◄──────►│   RP2040   │
│  (5V Protocol)  │  CLK   │  (5V ↔ 3.3V)  │  CLK   │   Board    │
│                 │  DATA  │               │  DATA  │            │
│                 │───────►│               │───────►│            │
│      GND ───────┼────────┼───────────────┼────────┤   GND      │
│      VCC (+5V)  │        │   VCC (+5V)   │        │   VSYS     │
└─────────────────┘        └───────────────┘        └──────┬─────┘
                                                           │ USB
                                                           │
                                                    ┌──────▼──────┐
                                                    │   Modern    │
                                                    │  Computer   │
                                                    │ (USB Host)  │
                                                    └─────────────┘
```

The device sends 5V logic signals on CLK and DATA lines to the level shifter, which translates them down to 3.3V for the RP2040's GPIO pins. Communication can flow in both directions—the level shifter handles translation automatically in both ways.

The RP2040 connects to your computer via USB, which also provides power. The board receives 5V on its VSYS pin and has an internal 3.3V regulator for its logic. Your device receives 5V from the RP2040's VSYS or an external supply.

The RP2040's PIO state machines handle all timing-critical protocol operations in dedicated hardware. The main CPU processes received data, translates scancodes (or mouse movements) to USB HID format, and sends reports to the computer. This division of labour ensures microsecond-precision protocol timing while maintaining responsive USB communication.

---

## Next Steps

Ready to begin? Follow the guides in order:

1. **[Hardware Setup](hardware-setup.md)** - Physical connections and wiring (30-60 minutes)
2. **[Building Firmware](building-firmware.md)** - Compile firmware for your device (15-30 minutes first time, <5 minutes subsequently)
3. **[Flashing Firmware](flashing-firmware.md)** - Install firmware on RP2040 (5 minutes)

After initial setup, you can update firmware or change device configurations without rewiring—just rebuild with a different configuration and reflash.

### Documentation Structure

Once you have a working converter, explore these sections to deepen your understanding:

- **[Hardware](../hardware/README.md)** - Detailed component information, PCB designs, internal installation examples
- **[Protocols](../protocols/README.md)** - How vintage device protocols work, timing diagrams, technical specifications
- **[Keyboards](../keyboards/README.md)** - Supported keyboards, configuration details, adding new keyboards
- **[Features](../features/README.md)** - Command Mode, USB HID, LED support, configuration storage
- **[Advanced](../advanced/README.md)** - Architecture, performance metrics, PIO programming, build system internals
- **[Development](../development/README.md)** - Contributing, code standards, adding protocols/keyboards

---

## Need Help?

If you run into issues or have questions, we're here to help:

- 📖 **[Troubleshooting Guide](../advanced/troubleshooting.md)** - Common issues and solutions
- 💬 **[GitHub Discussions](https://github.com/PaulW/rp2040-keyboard-converter/discussions)** - Ask questions, share your build, discuss features
- 🐛 **[GitHub Issues](https://github.com/PaulW/rp2040-keyboard-converter/issues)** - Report bugs or hardware problems

**Before asking for help:**
1. Check the troubleshooting guide—many common issues are already documented
2. Search existing discussions and issues—your question may already be answered
3. Provide details: keyboard model, RP2040 board type, exact error messages, and photos of your wiring if relevant
