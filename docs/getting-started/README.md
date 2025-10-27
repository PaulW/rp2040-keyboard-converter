# Getting Started

**Status**: 🔄 In Progress | **Last Updated**: 27 October 2025

Welcome to the RP2040 Keyboard Converter! This project brings vintage keyboards and mice back to life, allowing you to use classic hardware with modern computers via USB. Whether you're a retrocomputing enthusiast, a mechanical keyboard collector, or just curious about vintage peripherals, this guide will help you get started.

## Why Convert Vintage Keyboards?

Vintage keyboards, particularly those from the 1980s and early 1990s, are prized by enthusiasts for several compelling reasons:

**Superior Build Quality**: Keyboards like the IBM Model M and Model F feature buckling spring switches that provide exceptional tactile feedback and durability. Many units from the 1980s are still fully functional today, outlasting modern keyboards by decades.

**Unique Typing Experience**: The mechanical switches, key travel, and acoustic properties of vintage keyboards create a typing experience that many users prefer over modern alternatives. The distinctive "click-clack" sound and satisfying key feel can significantly improve typing comfort and accuracy.

**Historical Significance**: These keyboards represent important milestones in computing history. Using original hardware provides an authentic connection to the early PC era and preserves computing heritage.

**Practical Challenges**: Despite their desirability, vintage keyboards face a significant obstacle—they use communication protocols and connectors that modern computers no longer support. AT/PS2, XT, and other vintage protocols were replaced by USB in the late 1990s, leaving these keyboards incompatible with contemporary systems.

**The Solution**: This project provides a non-invasive bridge between vintage protocols and modern USB, preserving original hardware while enabling full functionality on today's computers. The converter handles all protocol translation automatically, presenting your vintage keyboard to the computer as a standard USB HID device that works with any operating system.

## What Makes This Project Different?

While several keyboard converters exist (TMK, QMK, Soarer's Converter), this project offers unique advantages by leveraging the RP2040 microcontroller's capabilities:

**Hardware-Accelerated Protocol Handling**: The RP2040's PIO (Programmable I/O) state machines handle protocol timing and bit-level operations in dedicated hardware. This provides microsecond-precision timing without consuming CPU cycles, ensuring reliable operation even during intensive USB traffic.

**True Multi-Protocol Support**: The same hardware can support AT/PS2, XT, Amiga, and Apple M0110 protocols by simply flashing different firmware. The flexible PIO architecture makes adding new protocols straightforward—no hardware changes needed.

**Simultaneous Conversion**: Convert both a keyboard and mouse at the same time using independent hardware state machines. Each protocol runs in parallel without interference, and you can mix protocols (e.g., AT/PS2 keyboard with AT/PS2 mouse).

**Modern Development Environment**: Built with the official Raspberry Pi Pico SDK and TinyUSB, the project benefits from active maintenance and excellent documentation. Docker-based builds ensure consistency across platforms—no complex toolchain setup required.

**Low Cost and Accessibility**: RP2040 boards are inexpensive (£4-8) and widely available from multiple manufacturers. The standardised hardware means you can use any RP2040 board—Raspberry Pi Pico, WaveShare RP2040-Zero, or compatible alternatives.

**Educational Value**: The project serves as a practical learning platform for embedded systems, hardware protocols, state machine design, and USB HID implementation. Well-documented code and comprehensive architecture guides make it accessible for learning.

## Quick Start

Choose your path based on what you want to accomplish:

### 1. Hardware Setup
**[Hardware Setup Guide](hardware-setup.md)** - Connect your keyboard to the RP2040

**Start here if you have:**
- A vintage keyboard you want to use with a modern computer
- An RP2040 board (Raspberry Pi Pico, WaveShare RP2040-Zero, or similar)
- Basic electronics knowledge (soldering, using a breadboard)
- A level shifter module (5V ↔ 3.3V conversion)

This guide covers physical connections, level shifting requirements, and connector types. You'll learn why level shifters are essential (the RP2040's 3.3V GPIO pins aren't 5V tolerant, but most vintage keyboards output 5V signals), how to wire them correctly, and which pins to use for your specific protocol.

### 2. Building Firmware
**[Building Firmware Guide](building-firmware.md)** - Compile firmware for your keyboard

**Follow this if you want to:**
- Build firmware customised for your specific keyboard model and protocol
- Add mouse support alongside keyboard conversion
- Understand the build system and configuration options
- Prepare firmware for flashing to your RP2040

This guide explains the Docker-based build process, which provides a consistent environment regardless of your host operating system (macOS, Linux, or Windows). You'll learn how to specify your keyboard configuration, enable optional features, and understand the build output.

### 3. Flashing Firmware
**[Flashing Firmware Guide](flashing-firmware.md)** - Flash firmware to your RP2040

**Complete this to:**
- Install compiled firmware on your RP2040 board
- Update existing firmware to a newer version
- Enter bootloader mode for reflashing
- Verify successful installation

This guide covers the UF2 drag-and-drop flashing process, Command Mode for convenient firmware updates without physical access to the board, and troubleshooting steps if something doesn't work as expected.

---


---

## What You'll Need

Setting up a keyboard converter requires some hardware components and basic technical knowledge. Don't worry if you're new to electronics—the process is straightforward, and we'll guide you through each step.

### Hardware Requirements

#### Essential Components

**RP2040 Microcontroller Board** (£4-8)
The brain of the converter. Any RP2040-based board will work, giving you flexibility in form factor and features:
- **Raspberry Pi Pico**: The official board, widely available, well-documented, and affordable. Features standard 2.54mm pin headers for breadboarding. USB Micro-B connector.
- **WaveShare RP2040-Zero**: Compact design (23.5×18.4mm) with USB-C connector. Ideal for internal keyboard installations where space is limited. Castellated pads for direct PCB mounting.
- **Adafruit Feather RP2040**: Larger form factor with battery support, LiPo charger, and extra features. Good for portable projects or prototypes requiring power flexibility.
- **Pimoroni Tiny 2040**: Smallest option (22×18.3mm) with built-in RGB LED. Similar to WaveShare but with additional GPIO accessible via pads.

All RP2040 boards provide the same core functionality—dual ARM Cortex-M0+ cores at 125MHz, 264KB RAM, 2MB Flash, and the critical PIO state machines that make hardware-accelerated protocol handling possible. Choose based on your size, connector, and feature preferences.

**Bi-Directional Level Shifter** (£2-5)
Absolutely essential for safe operation. The RP2040's GPIO pins operate at 3.3V and are NOT 5V tolerant—connecting them directly to vintage keyboards (which output 5V logic levels) will permanently damage the microcontroller. A level shifter safely translates between voltage domains:
- **BSS138-based 4-channel modules** (Adafruit, SparkFun): Reliable, fast switching (supports kHz-MHz frequencies), bi-directional (important for protocols like AT/PS2 where data flows both ways). Use 2 channels for CLK and DATA lines.
- **TXB0104 4-bit modules**: Auto-direction sensing, slightly more compact than BSS138 designs. Good choice for permanent installations.
- **74LVC245 or similar buffer ICs**: Lower-level solution requiring more circuit knowledge. Useful if designing custom PCBs.

**Important**: Cheap uni-directional level shifters (often sold for I2C) may not work reliably with keyboard protocols. Always use bi-directional shifters that support open-drain signalling.

**Keyboard Connector** (£2-10)
The connector type depends on your keyboard's protocol and original cable:
- **PS/2 (6-pin mini-DIN)**: Most AT/PS2 keyboards. Standard female PS/2 panel mount connector or cable with socket. Pin order: 1=DATA, 2=N/C, 3=GND, 4=VCC, 5=CLK, 6=N/C.
- **DIN-5 (180° or 270°)**: Older AT keyboards and IBM Model F. Check pin angle (some keyboards use 180° DIN, others 270° DIN—they're NOT interchangeable).
- **SDL connector (also called 6-pin mini-DIN)**: Some IBM keyboards (Model M variants). Physically identical to PS/2 but may have different pin assignments—always verify with keyboard documentation.
- **Custom connectors**: Amiga keyboards (proprietary 5-pin), Apple M0110 (4-pin), or other unique protocols may require custom cables or adapter boards.

Consider purchasing pre-made cables if available—many vendors sell PS/2 extension cables or breakout boards that eliminate tedious connector soldering.

**USB Cable** (£1-5)
Connects the RP2040 to your computer. Match your board's connector:
- **USB Micro-B to USB-A**: Raspberry Pi Pico, most older boards
- **USB-C to USB-A or USB-C**: WaveShare RP2040-Zero, Pimoroni Tiny 2040, modern boards

Choose a reliable cable (not the cheapest one)—poor-quality cables can cause intermittent connection issues that are frustrating to debug.

#### Optional Components

**Breadboard and Jumper Wires** (£5-10)
Essential for prototyping and testing. A half-size breadboard (400 tie-points) is sufficient. Use male-to-male jumper wires for breadboard connections and male-to-female for connecting to the RP2040. Pre-cut wire sets in various colours help keep your wiring organised.

**Enclosure** (£5-20)
Protects your converter and provides a professional appearance:
- **3D Printed Cases**: Many designs available online (Thingiverse, Printables) for specific RP2040 boards. You can print your own or order from online services.
- **Commercial Project Boxes**: Hammond, Takachi, and others offer small ABS or aluminium enclosures. Choose one with enough internal space for the RP2040, level shifter, and wiring.
- **Custom Designs**: For internal keyboard mounting, design enclosures around your specific keyboard's internal geometry. The custom PCB for IBM Model F PC/AT (see Hardware docs) is an example of this approach.

**Status LED (WS2812B RGB)** (Optional, £1-2)
The firmware supports WS2812B addressable RGB LEDs for visual status indication. Useful for debugging or visual feedback about Command Mode, Keytest Mode, and other states. Can be disabled in firmware to save power.

**Connectors and Headers** (£2-5)
- **2.54mm pin headers**: For making breadboard-friendly connections
- **Screw terminals**: For permanent, tool-free wire connections
- **JST or similar connectors**: For detachable connections between converter and keyboard

### Software Requirements

**Docker** (Free)
The project uses Docker to provide a consistent build environment across all platforms. This eliminates toolchain configuration headaches—no need to install ARM compilers, CMake, or manage SDK versions manually.
- **Installation**: [Docker Desktop](https://www.docker.com/products/docker-desktop) (macOS, Windows) or Docker Engine (Linux)
- **Disk Space**: ~2GB for Docker image (includes Pico SDK, compilers, and all dependencies)
- **Memory**: At least 2GB RAM available for Docker container

**Git** (Free)
Required to clone the repository and manage updates:
- **Installation**: [Git Downloads](https://git-scm.com/downloads) or platform package managers (`brew install git`, `apt install git`, `winget install Git.Git`)
- **Basic Knowledge**: Familiarity with `git clone`, `git pull`, and `git submodule` commands helpful but not essential—all commands are provided in documentation

**Text Editor or IDE** (Free options available)
While not strictly necessary for basic firmware builds, you'll want a text editor if you plan to:
- Modify keyboard configurations
- Create custom keymaps
- Contribute to the project
- Browse source code

**Recommended Options**:
- **Visual Studio Code**: Excellent support for C/C++, CMake, and embedded development. Free, cross-platform, extensive plugin ecosystem.
- **Sublime Text, Vim, Emacs**: Lightweight alternatives if you prefer simpler editors.

**Operating System Compatibility**:
- ✅ macOS (Apple Silicon and Intel)
- ✅ Linux (Ubuntu, Debian, Fedora, Arch, and others)
- ✅ Windows 10/11 (with WSL2 recommended for Docker performance)

### Technical Skills

**Basic Electronics Understanding** (Beginner-Friendly)
You should understand:
- **Voltage and Logic Levels**: Difference between 3.3V and 5V logic, why level shifting is necessary
- **Pin Connections**: How to identify power (VCC), ground (GND), and signal pins
- **Breadboard Usage**: How tie-points are connected internally, how to build temporary circuits
- **Polarity**: Recognising positive (+) and negative (-) connections to avoid reverse polarity

If this sounds intimidating, don't worry—many online tutorials cover these basics, and the converter circuit is quite simple (only 4-6 connections total).

**Soldering Skills** (Optional)
Only required if you're building a permanent installation or custom PCB:
- **Through-Hole Soldering**: Connecting wires to headers, attaching connectors (easiest type of soldering)
- **Safety**: Proper ventilation, temperature control, avoiding burns
- **Tools**: Soldering iron (temperature-controlled, 30-60W), solder (60/40 or 63/37 rosin-core), helping hands or PCB holder

For breadboard prototypes, no soldering is needed—everything uses jumper wires and pre-assembled modules.

**Command Line Basics** (Helpful)
Ability to:
- Navigate directories (`cd`, `ls` / `dir`)
- Run commands in terminal/command prompt
- Copy files (`cp` / `copy`)
- Understand basic command structure and parameters

All necessary commands are provided in the guides with explanations—you can copy and paste them directly.

---

## What This Converter Does

Once set up, the converter provides sophisticated functionality that goes beyond simple protocol translation:

**Full N-Key Rollover (NKRO)**: Reports every key press simultaneously without ghosting or blocking. Press as many keys as you like—all will register. This is particularly valuable for fast typing, gaming, or using complex keyboard shortcuts where multiple keys need to be held simultaneously.

**USB HID Boot Protocol Compatibility**: Works at the BIOS/UEFI level for pre-boot environments (firmware setup, boot menus, disk encryption password entry). No operating system drivers required—the converter appears as a standard USB keyboard that works immediately on any system (Windows, macOS, Linux, BSD, and others).

**LED Synchronisation**: For protocols that support it (AT/PS2, Amiga), keyboard LEDs (Caps Lock, Num Lock, Scroll Lock) automatically synchronise with host state. No manual toggling needed—the LEDs accurately reflect your computer's lock key states.

**Simultaneous Keyboard + Mouse**: Convert both devices at the same time using independent hardware channels. The keyboard and mouse operate completely independently—each has dedicated PIO state machines, ring buffers, and USB endpoints. Mix protocols if needed (e.g., AT/PS2 keyboard with AT/PS2 mouse, or even different protocol combinations if supported).

**Persistent Configuration**: Settings like log level, LED brightness, and (in the future) custom key mappings are stored in flash memory. Survives power cycles and firmware updates—your preferences persist until you explicitly change them.

**Command Mode**: Special firmware management mode accessible via keyboard combination (Left Shift + Right Shift during boot). Provides convenient access to:
- **Bootloader Entry**: Reflash firmware without physical access to the RP2040's BOOT button
- **Keytest Mode**: Verify all keys work correctly without needing additional software
- **Log Level Control**: Adjust UART debugging output verbosity for troubleshooting
- **LED Brightness**: Adjust status LED brightness or turn it off completely
- **Factory Reset**: Restore all settings to defaults

**Field Debugging**: Built-in UART logging provides detailed diagnostic information. Connect a USB-UART adapter to see protocol state, scancode sequences, error conditions, and performance metrics. Invaluable for troubleshooting hardware issues, protocol problems, or understanding converter behaviour.

---

## Overview

Here's a simplified view of how everything connects:

```
┌─────────────────┐        ┌───────────────┐        ┌────────────┐
│ Vintage Keyboard│◄──────►│ Level Shifter │◄──────►│   RP2040   │
│  (5V Protocol)  │  CLK   │  (5V ↔ 3.3V) │  CLK   │   Board    │
│                 │  DATA  │               │  DATA  │            │
│                 │───────►│               │───────►│            │
│      GND ───────┼────────┼───────────────┼────────┤   GND      │
│      VCC (+5V)  │        │   VCC (+5V)   │        │   VSYS     │
└─────────────────┘        └───────────────┘        └──────┬─────┘
                                                            │ USB
                                                            │
                                                    ┌───────▼─────┐
                                                    │   Modern    │
                                                    │  Computer   │
                                                    │ (USB Host)  │
                                                    └─────────────┘
```

**Signal Path Explanation**:
1. **Keyboard → Level Shifter**: Keyboard sends 5V logic signals on CLK and DATA lines. Open-drain signalling allows bidirectional communication on the same wires.

2. **Level Shifter → RP2040**: Level shifter translates 5V signals down to 3.3V for the RP2040's GPIOs, and vice versa (RP2040's 3.3V outputs become 5V for the keyboard). Translation happens in both directions automatically.

3. **RP2040 → Computer**: USB connection provides power (VSYS) and data. RP2040 appears as a standard USB HID keyboard to the computer—no drivers needed.

**Power Distribution**:
- Computer provides 5V power via USB
- RP2040 receives 5V on VSYS pin, has internal 3.3V regulator for logic
- Keyboard receives 5V from RP2040's VSYS or external supply
- Level shifter receives both 5V (HV) and 3.3V (LV) for translation reference

**Protocol Translation**:
The RP2040's PIO state machines handle all timing-critical protocol operations in dedicated hardware. The main CPU processes received data, performs scancode-to-USB-HID translation, and sends HID reports to the computer. This division of labour ensures microsecond-precision protocol timing while maintaining responsive USB communication.

---

## Next Steps

Ready to begin? Follow the guides in order:

1. **[Hardware Setup](hardware-setup.md)** - Physical connections and wiring (30-60 minutes)
2. **[Building Firmware](building-firmware.md)** - Compile firmware for your keyboard (15-30 minutes first time, <5 minutes subsequently)
3. **[Flashing Firmware](flashing-firmware.md)** - Install firmware on RP2040 (5 minutes)

**After initial setup**, you can update firmware or change keyboards without rewiring—just rebuild with a different `KEYBOARD` setting and reflash.

### Documentation Structure

Once you have a working converter, explore these sections to deepen your understanding:

- **[Hardware](../hardware/README.md)** - Detailed component information, PCB designs, internal installation examples
- **[Protocols](../protocols/README.md)** - How vintage keyboard protocols work, timing diagrams, technical specifications
- **[Keyboards](../keyboards/README.md)** - Supported keyboards, configuration details, adding new keyboards
- **[Features](../features/README.md)** - Command Mode, Keytest, USB HID, LED support, configuration storage
- **[Advanced](../advanced/README.md)** - Architecture, performance metrics, PIO programming, build system internals
- **[Development](../development/README.md)** - Contributing, code standards, adding protocols/keyboards

---

## Need Help?

**Questions or Problems?** We're here to help:

- 📖 **[Troubleshooting Guide](../advanced/troubleshooting.md)** - Common issues and solutions
- 💬 **[GitHub Discussions](https://github.com/PaulW/rp2040-keyboard-converter/discussions)** - Ask questions, share your build, discuss features
- 🐛 **[GitHub Issues](https://github.com/PaulW/rp2040-keyboard-converter/issues)** - Report bugs or hardware problems
- 📧 **Community** - Other users can offer advice based on their experiences

**Before Asking**:
1. Check the troubleshooting guide—many common issues are already documented
2. Search existing discussions and issues—your question may already be answered
3. Provide details: keyboard model, RP2040 board type, exact error messages, photos of wiring if relevant

The community is friendly and welcoming to beginners. Don't hesitate to ask if you're stuck!

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
