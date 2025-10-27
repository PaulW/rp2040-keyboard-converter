# Hardware Documentation

**Status**: 🔄 In Progress | **Last Updated**: 27 October 2025

This section covers everything you need to know about building hardware for the RP2040 Keyboard Converter. Whether you're prototyping on a breadboard, designing a custom PCB, or planning an internal keyboard installation, you'll find detailed information about components, connections, and practical considerations.

## Available Documentation

Complete hardware guides covering different build approaches:

- **[Breadboard Prototype](breadboard-prototype.md)** - Quick prototyping setup for testing and development
- **[Custom PCB](custom-pcb.md)** - Professional PCB design for internal keyboard installation
- **[Pin Configurations](pin-configurations.md)** - GPIO pin assignments for each protocol
- **[Level Shifters](level-shifters.md)** - Understanding voltage level translation requirements

---

## Choosing Your Build Approach

The converter's hardware can be built in several ways, each suited to different needs and skill levels. Understanding the trade-offs helps you choose the right approach for your project.

### Breadboard Prototype

**Best for**: First-time builds, testing, development, supporting multiple keyboards

A breadboard-based converter is the ideal starting point for most users. It requires no soldering, uses readily available components, and can be assembled in 30-60 minutes. The temporary nature of breadboard connections makes it perfect for experimentation—you can easily swap keyboards, try different RP2040 boards, or modify connections during troubleshooting.

**Advantages:**
- **Quick Setup**: Assemble a working converter in under an hour without any permanent modifications
- **No Soldering Required**: Everything connects via jumper wires and pre-assembled modules
- **Easy Modifications**: Rewire connections, test different pin assignments, or swap components instantly
- **Low Initial Cost**: ~£15-25 for all components (RP2040 board, level shifter, breadboard, wires)
- **Reusable Components**: Disassemble and reuse parts for other projects or different keyboards
- **Learning-Friendly**: See all connections clearly, understand signal flow, debug issues visually

**Trade-offs:**
- **External Enclosure**: Requires a separate project box since components can't fit inside most keyboards
- **Less Reliable Connections**: Breadboard contacts can loosen over time or with movement/vibration
- **More Wiring**: Jumper wires create a less tidy appearance compared to PCB traces
- **Not Travel-Friendly**: Loose connections make transportation risky—wires can disconnect

**When to choose this approach:**
- You're new to electronics and want a forgiving, reversible build
- You plan to test multiple keyboards or protocols
- You're developing new features or debugging issues
- You want minimal commitment before deciding on a permanent solution
- Budget is a primary concern

**See:** [Breadboard Prototype Guide](breadboard-prototype.md) for detailed assembly instructions, connection diagrams, and troubleshooting tips.

---

### Custom PCB

**Best for**: Permanent installations, internal keyboard mounting, clean professional appearance

A custom PCB provides the ultimate in reliability and aesthetics. By consolidating all components onto a single board, you eliminate loose wiring, improve signal integrity, and create an installation that looks factory-original. The example PCB design for the IBM Model F PC/AT demonstrates what's possible—a tiny board that mounts inside the keyboard case, completely invisible from the outside.

**Advantages:**
- **Professional Appearance**: Clean, compact design that rivals commercial products
- **Internal Installation**: Mount inside keyboard case—no external box, no visible converter
- **Reliable Connections**: Soldered joints and PCB traces eliminate connection problems
- **Compact Form Factor**: Custom boards can be designed to fit specific keyboard internal geometry
- **Integrated Level Shifting**: On-board level shifter circuitry eliminates external modules
- **Strain Relief**: Proper mounting prevents stress on connections from cable movement

**Trade-offs:**
- **Higher Initial Cost**: PCB fabrication (~£20-40 for 5 boards) plus assembly time
- **Requires PCB Skills**: Design knowledge (KiCad, EasyEDA) and soldering ability needed
- **Less Flexible**: Changing protocols or pin assignments requires new board design
- **Keyboard-Specific**: Most designs optimised for particular keyboard models
- **Longer Development**: Design, order, receive, assemble cycle takes 2-4 weeks

**When to choose this approach:**
- You have a valuable or collectible keyboard deserving of a clean installation
- You're comfortable with PCB design and soldering (or willing to learn)
- You want the converter to be invisible or minimally invasive
- You have a specific keyboard in mind for permanent conversion
- You plan to convert multiple units of the same keyboard model

**Real-World Example**: The custom PCB for IBM Model F PC/AT (documented in this section) measures just 40×30mm and mounts on the keyboard's internal metal frame using existing screw holes. It includes integrated level shifting, USB-C connector, and RP2040 chip with all necessary components—everything needed for a complete, invisible conversion. Total cost: ~£25 for 5 fully assembled boards from JLCPCB including SMT assembly service (£5 per board).

**See:** [Custom PCB Guide](custom-pcb.md) for complete design files, fabrication instructions, assembly guide, and installation photos.

---

## Understanding Component Requirements

Every converter build requires the same essential components, regardless of whether you choose breadboard or PCB. Understanding why each component is necessary and how to select the right one ensures a successful build.

### Essential Components

#### RP2040 Microcontroller Board

The RP2040 is the heart of the converter, handling all protocol translation, USB communication, and feature implementation. Raspberry Pi's RP2040 chip offers unique capabilities that make it ideal for this application:

**Key Features:**
- **Dual ARM Cortex-M0+ Cores**: Two 125MHz processors (though this project uses only one for simplicity)
- **264KB SRAM**: Plenty of memory for code and data, allowing execution from RAM for optimal performance
- **PIO State Machines**: Eight independent hardware state machines for microsecond-precision I/O—the "secret sauce" that enables reliable protocol handling
- **Flexible GPIO**: 30 multipurpose pins that can be assigned to any function
- **USB 1.1 Device**: Native USB support via TinyUSB stack
- **Low Power**: ~2% CPU usage at maximum typing speed leaves headroom for future features

**Why PIO Matters:**
Traditional microcontrollers handle protocols using software bit-banging or hardware peripherals (UART, SPI). Software approaches struggle with timing precision, while hardware peripherals lack flexibility. The RP2040's PIO (Programmable I/O) provides the best of both worlds—hardware-level timing precision with software-level flexibility. Each PIO state machine runs independently of the CPU, monitoring signals and extracting bits with microsecond accuracy. This architecture is why the converter achieves 350µs latency while using only 2% CPU time.

**Board Selection Guide:**

| Board | Size | USB | Price | Best For |
|-------|------|-----|-------|----------|
| **Raspberry Pi Pico** | 51×21mm | Micro-B | £4 | Breadboard prototyping, learning, most common choice |
| **WaveShare RP2040-Zero** | 23.5×18mm | USB-C | £6 | Internal installations, compact builds, modern connector |
| **Pimoroni Tiny 2040** | 22×18mm | USB-C | £8 | Space-constrained builds, built-in RGB LED |
| **Adafruit Feather RP2040** | 51×23mm | USB-C | £12 | Battery power, portables, extensive I/O |

All boards provide identical functionality—choose based on your physical constraints, connector preference, and budget. The Raspberry Pi Pico is recommended for first builds due to widespread availability, extensive documentation, and lowest cost.

#### Bi-Directional Level Shifter

**Critical Safety Component** - The level shifter prevents permanent damage to your RP2040 by translating between different voltage domains. Understanding why this component is essential helps avoid costly mistakes.

**The Problem:**
- Vintage keyboards operate at **5V logic levels** (0V = LOW, 5V = HIGH)
- RP2040 GPIO pins operate at **3.3V logic levels** (0V = LOW, 3.3V = HIGH)
- RP2040 GPIOs are **NOT 5V tolerant**—connecting them directly to 5V signals **will destroy the chip**
- This isn't a gradual degradation—5V on a 3.3V pin causes immediate, permanent damage

**The Solution:**
A bi-directional level shifter sits between the keyboard and RP2040, automatically translating signals in both directions:
- Keyboard → RP2040: Converts 5V HIGH signals down to 3.3V (safe for RP2040 input)
- RP2040 → Keyboard: Converts 3.3V HIGH signals up to 5V (ensures keyboard sees valid logic levels)

**Why Bi-Directional?**
Protocols like AT/PS2 and Amiga require two-way communication on the same wires (open-drain signalling). The keyboard can pull lines LOW, and the RP2040 can also pull lines LOW. A bi-directional level shifter handles this automatically without needing direction control signals.

**Recommended Modules:**

**BSS138-Based 4-Channel Level Shifter** (Most Common)
- **Chip**: BSS138 N-channel MOSFET (4 independent channels)
- **Speed**: Fast enough for MHz signals (more than adequate for kHz keyboard protocols)
- **Price**: £2-5 from Adafruit, SparkFun, or generic suppliers
- **Advantages**: Simple, reliable, well-documented, widely available
- **Usage**: Connect 2 channels (one for CLK, one for DATA). Two channels remain unused.

**TXB0104 4-Bit Level Shifter** (Alternative)
- **Chip**: TXB0104 auto-direction sensing level translator
- **Speed**: 100 Mbps data rate
- **Price**: £3-6
- **Advantages**: Slightly more compact, automatic direction detection
- **Considerations**: Less common than BSS138 modules, similar performance

**Connection Example (BSS138 Module):**
```
Level Shifter Module:
LV (Low Voltage)  ─── Connect to RP2040 3V3 pin
LV1 (Channel 1)   ─── RP2040 GPIO (e.g., GPIO 17 for CLK)
LV2 (Channel 2)   ─── RP2040 GPIO (e.g., GPIO 16 for DATA)
LV3, LV4          ─── Unused (available for future expansion)
GND               ─── Common ground (RP2040 GND)

HV (High Voltage) ─── Connect to keyboard +5V or RP2040 VSYS
HV1 (Channel 1)   ─── Keyboard CLK line
HV2 (Channel 2)   ─── Keyboard DATA line
HV3, HV4          ─── Unused
GND               ─── Common ground (keyboard GND)
```

**Important Notes:**
- Both LV and HV sides need their respective voltage reference (3.3V and 5V)
- Ground must be common between all components
- Never connect keyboard signals directly to RP2040—always through level shifter
- Check module pinout diagram—manufacturers vary in pin arrangement

**See:** [Level Shifters Guide](level-shifters.md) for detailed wiring diagrams, troubleshooting, and alternatives.

#### Keyboard Connector

The connector type depends entirely on your keyboard's original interface. Using the correct connector ensures reliable signal transmission and proper mechanical connection.

**Common Connector Types:**

**PS/2 Connector (6-pin mini-DIN)**
- **Used By**: Most AT/PS2 protocol keyboards from late 1980s onward, PS/2 mice
- **Physical**: 6-pin mini-DIN, ~13mm diameter, keyed to prevent incorrect insertion
- **Pinout** (looking at female socket on keyboard cable, numbers marked on connector):
  ```
       ___
    6 |   | 5
   4  | o |  1
    3 |___| 2
  
  Pin 1: DATA
  Pin 2: N/C (not connected)
  Pin 3: GND
  Pin 4: VCC (+5V)
  Pin 5: CLK
  Pin 6: N/C (not connected)
  ```
- **Availability**: Very common, inexpensive panel-mount sockets or cables available
- **Note**: Mechanically identical to SDL connector but may have different pinout—verify!

**DIN-5 Connector (180° or 270°)**
- **Used By**: Older AT keyboards, IBM Model F keyboards
- **Physical**: 5-pin DIN, ~13.2mm diameter, two variants:
  - **180° DIN**: Pins arranged in semicircle (most common)
  - **270° DIN**: Pins arranged in 270° arc (less common, IBM keyboards)
- **Pinout** (180° DIN, looking at female socket):
  ```
      ___
    3| o |2
   4 |o o| 1
      |___|5
  
  Pin 1: CLK
  Pin 2: DATA
  Pin 3: N/C
  Pin 4: GND
  Pin 5: VCC (+5V)
  ```
- **Warning**: 180° and 270° DIN are NOT interchangeable—connector won't physically fit
- **Availability**: More difficult to source than PS/2, check vintage electronics suppliers

**SDL Connector (also called 6-pin mini-DIN)**
- **Used By**: Some IBM Model M variants (e.g., 1391401 Enhanced Keyboard)
- **Physical**: Identical to PS/2 connector (6-pin mini-DIN)
- **Pinout**: May differ from PS/2 standard—check keyboard documentation
- **Confusion Factor**: Looks exactly like PS/2 but pinout can be different
- **Advice**: Measure voltages and identify signals before assuming PS/2 pinout

**Amiga Keyboard Connector (Proprietary 5-pin)**
- **Used By**: Commodore Amiga computers (A500, A1000, A2000, etc.)
- **Physical**: Proprietary rectangular connector, ~8mm width
- **Pinout**: 
  ```
  Pin 1: CLK
  Pin 2: DATA
  Pin 3: RESET
  Pin 4: GND
  Pin 5: VCC (+5V)
  ```
- **Availability**: Difficult to find original connectors, custom cables or breakout boards often needed
- **Alternative**: Cut and splice original keyboard cable (not ideal for collectible keyboards)

**Apple M0110 Connector (Proprietary 4-pin)**
- **Used By**: Apple Macintosh M0110, M0110A keyboards
- **Physical**: Proprietary 4-pin coiled cable connector
- **Pinout**: Unique to Apple keyboards
- **Availability**: Very rare, usually requires cable modification
- **Advice**: Consider non-destructive adapter cable if keyboard is collectible

**Sourcing Connectors:**
- **Pre-made Cables**: Search for "[protocol] extension cable" or "[protocol] breakout board"
- **Panel Mount Sockets**: Available from electronics distributors (Mouser, Digi-Key, Farnell)
- **Salvage**: Desoldering from damaged motherboards or broken keyboards (free, requires skill)
- **Custom PCBs**: Design connector footprint directly into custom converter boards

**See:** Protocol-specific documentation for exact pinouts and wiring details.

#### USB Cable

Connects your RP2040 board to the computer, providing both power and data communication.

**Cable Selection:**
- **Raspberry Pi Pico**: USB-A to Micro-B
- **WaveShare RP2040-Zero, Pimoroni Tiny 2040**: USB-A to USB-C
- **Modern computers**: May need USB-C to USB-C

**Quality Matters:**
Cheap cables (often bundled with smartphones for charging only) may lack data wires or have poor shielding. This causes mysterious connection problems—the converter appears to work intermittently or not at all. Invest in a quality cable (£3-5) with good reviews specifically mentioning data transfer capability.

**Length Considerations:**
- **Short cables (30-50cm)**: Less voltage drop, better for breadboard prototypes on desk
- **Longer cables (1-2m)**: More flexibility for permanent installations, ensure rated for data

**Verify Your Cable:**
After assembly, if the converter doesn't enumerate (computer doesn't recognize USB device), try a different cable before troubleshooting hardware. Cable issues are surprisingly common and easily misdiagnosed as converter problems.

---

### Optional Components

These components enhance functionality or improve the user experience but aren't required for basic operation.

#### Breadboard and Jumper Wires

**For Prototyping:**
- **Half-Size Breadboard**: 400 tie-points (30 rows), ~£3-5. Sufficient for all converter components.
- **Jumper Wire Kit**: Assorted lengths in multiple colours, ~£5-10. Colour coding helps track signals:
  - Red: Power (VCC, 5V, 3.3V)
  - Black: Ground (GND)
  - Yellow: Clock (CLK)
  - Green: Data (DATA)
  - Other colours: Optional signals, status LEDs, etc.

**Wire Types:**
- **Male-to-Male**: Breadboard to breadboard connections
- **Male-to-Female**: Breadboard to GPIO pin headers (RP2040, level shifter modules)
- **Female-to-Female**: Between modules with pin headers (less common for this project)

**Quality Tip**: Pre-formed wire kits with consistent quality are worth the small premium over loose wire assortments. Bad connections cause intermittent failures that are maddening to debug.

#### Enclosure

**Breadboard Builds:**
Protect your converter and prevent accidental short circuits:
- **Project Boxes**: Hammond 1591 series, Takachi cases, generic ABS boxes (£5-15)
- **Size Guide**: ~100×60×25mm minimum for RP2040 + breadboard + level shifter
- **Considerations**: Access to USB port, strain relief for keyboard cable, ventilation (not critical—converter runs cool)

**3D Printed Cases:**
Custom designs for specific RP2040 boards available on Thingiverse, Printables, and other repositories. Search for "[board name] case" (e.g., "Raspberry Pi Pico case"). Many designs include mounting for breadboards or custom PCBs.

**Design Your Own:**
FreeCAD, Fusion 360, OnShape, and other CAD tools enable custom enclosure design. Measure your components, add 2-3mm clearance, design mounting features (standoffs, clips, screw posts), and export STL files for printing or ordering.

#### Status LED (WS2812B RGB)

**Optional Visual Feedback:**
The firmware supports WS2812B "NeoPixel" individually addressable RGB LEDs for status indication:
- **Command Mode**: Rapid flashing (Green/Blue, Green/Pink, or Rainbow depending on submode)
- **Keytest Mode**: Blink on each keypress (visual confirmation without computer)
- **Error States**: Red flash patterns for protocol errors
- **Power On**: Boot animation (can be disabled)

**Implementation:**
- **Cost**: £1-2 per LED, available as individual modules or strips
- **Connection**: Single data wire to RP2040 GPIO, plus power and ground
- **Configuration**: Enabled/disabled in firmware config, brightness adjustable via Command Mode
- **Power**: Each LED draws ~60mA at full white brightness—use dimmer colours or lower brightness to reduce consumption

**Note**: Most RP2040 boards include a built-in LED (usually connected to GPIO 25) that can be used for basic status indication without external components. The WS2812B provides more sophisticated visual feedback with colour coding.

#### Connectors and Headers

**For Permanent Builds:**
- **2.54mm Pin Headers**: Standard spacing, compatible with breadboards and DuPont connectors
- **Screw Terminals**: Tool-free connections, good for thick keyboard cables (16-22 AWG)
- **JST Connectors**: Small, positive-locking connectors for internal wiring (1.25mm, 2.0mm, or 2.54mm pitch)
- **Heat Shrink Tubing**: Insulate exposed connections, provide strain relief, improve appearance

**Crimping Tools** (if using custom cables):
- **DuPont Crimping Tool**: For 2.54mm connectors (~£15-30)
- **JST Crimping Tool**: For JST connectors (~£20-40)
- **Alternative**: Pre-crimped wires available from many suppliers (easier, slightly more expensive)

---

## Supported RP2040 Boards

The converter firmware runs on any RP2040-based board. Here's a detailed comparison to help you choose:

### Raspberry Pi Pico

**The Reference Platform**

The official Raspberry Pi Pico is the most widely used and best-documented RP2040 board, making it the ideal choice for first-time builders.

**Specifications:**
- **Dimensions**: 51×21mm (2.0×0.8 inches)
- **Mounting**: 2.54mm pin headers (20 pins per side, breadboard-compatible)
- **USB**: Micro-B connector
- **Power**: 1.8-5.5V input on VSYS or 5V via USB
- **LEDs**: Single green LED on GPIO 25
- **Debug**: 3-pin SWD header for advanced debugging
- **Cost**: ~£4 (official), ~£2-3 (compatible clones)

**Advantages:**
- **Universal Compatibility**: Works with all tutorials, examples, and development tools
- **Breadboard-Friendly**: Pin headers fit standard breadboards perfectly
- **Extensive Documentation**: Official Pico SDK examples, community resources, troubleshooting guides
- **Wide Availability**: Sold by virtually every electronics distributor worldwide
- **Cost-Effective**: Lowest price point for RP2040 boards

**Considerations:**
- Larger form factor may not fit inside compact keyboards
- Micro-B USB is becoming less common (adapters widely available)
- Through-hole pin headers add height (can use low-profile socket if needed)

**Best For**: Learning, breadboard prototypes, development, most builds where space isn't critically constrained.

### WaveShare RP2040-Zero

**Compact Powerhouse**

WaveShare's ultra-compact board packs the same RP2040 power into a tiny footprint, perfect for internal keyboard installations.

**Specifications:**
- **Dimensions**: 23.5×18.4mm (0.9×0.7 inches)—less than half the Pico's area
- **Mounting**: Castellated pads for direct PCB soldering, optional pin headers
- **USB**: USB-C connector (modern, reversible)
- **Power**: 5V via USB or external power on pads
- **LEDs**: WS2812B RGB LED built-in (fully programmable)
- **Reset**: Physical reset button
- **Cost**: ~£6

**Advantages:**
- **Tiny Form Factor**: Fits inside most keyboards with minimal space requirements
- **USB-C**: Modern connector, more durable than Micro-B, no orientation worries
- **Built-in RGB LED**: Eliminates need for external status LED, full colour programming
- **Castellated Pads**: Enable direct PCB mounting for ultra-compact designs
- **Pin-Compatible**: Can use pin headers for breadboard prototyping like Pico

**Considerations:**
- Less common than Pico (may have longer shipping times)
- Castellated pads require soldering skills for direct mounting
- Smaller size makes hand-wiring more fiddly

**Best For**: Internal keyboard installations, space-constrained builds, projects requiring built-in RGB LED, modern USB-C preference.

### Pimoroni Tiny 2040

**Smallest with Style**

Pimoroni's contribution to the RP2040 ecosystem emphasizes minimal size and elegant design with quality components.

**Specifications:**
- **Dimensions**: 22×18.3mm (similar to WaveShare but narrower)
- **Mounting**: Castellated edges, optional pin headers via castellations
- **USB**: USB-C connector with ESD protection
- **Power**: 5V via USB, 3.3V and 5V output pads
- **LEDs**: WS2812B RGB LED (high-quality, bright)
- **Additional**: Reset button, BOOTSEL button accessible
- **Cost**: ~£8

**Advantages:**
- **Premium Components**: High-quality USB connector, improved ESD protection
- **Bright RGB LED**: Excellent visibility for status indication
- **Good Documentation**: Pimoroni provides clear guides and Python/C++ examples
- **Additional GPIO Access**: Pads expose additional pins not on pin headers
- **Aesthetics**: Purple PCB with clean silkscreen (if appearance matters)

**Considerations:**
- Higher cost than WaveShare or Pico
- Less common in stock (Pimoroni production runs can sell out)
- Castellated mounting requires soldering skills

**Best For**: Premium builds, users who value aesthetics, projects requiring maximum GPIO access, support for Pimoroni's ecosystem.

### Adafruit Feather RP2040

**Feature-Rich Platform**

Adafruit's Feather form factor adds battery management, extensive I/O, and accessory compatibility for portable or expandable projects.

**Specifications:**
- **Dimensions**: 51×23mm (Feather form factor standard)
- **Mounting**: Pin headers, Feather-compatible mounting holes
- **USB**: USB-C connector
- **Power**: LiPo battery support with on-board charger, voltage regulator
- **LEDs**: NeoPixel RGB LED, red charge indicator
- **Additional**: STEMMA QT connector (I2C), extra GPIO, voltage output selection
- **Cost**: ~£12

**Advantages:**
- **Battery Operation**: Built-in LiPo charger enables portable operation without external power
- **Expandable**: Compatible with Feather accessory ecosystem (displays, sensors, LoRa, etc.)
- **More GPIO**: 21 GPIO pins vs Pico's 26 (more than adequate for converter)
- **STEMMA QT**: Easy connection to I2C devices without soldering
- **Versatile Power**: Choose 3.3V or 5V output for peripherals

**Considerations:**
- Significantly more expensive than other options
- Larger form factor (standard Feather size)
- Extra features unnecessary for basic converter function
- Higher power consumption (battery charging circuitry always active)

**Best For**: Portable builds (keyboard + battery), projects requiring expansion, integration with existing Feather-based systems, learning platform for future IoT projects.

---

## Connection Overview

Understanding signal flow and voltage levels ensures safe, reliable operation.

### Signal Path Diagram

```
┌─────────────────────────┐
│   Vintage Keyboard       │
│   (5V Logic Levels)      │
│                          │
│   CLK  ──────┐          │
│   DATA ──────┼──────┐   │
│   GND  ──────┼──────┼─┐ │
│   VCC  ──────┼──────┼─┼─│─── +5V (from RP2040 VSYS or external)
└──────────────┼──────┼─┼─┘
               │      │ │
               ▼      ▼ ▼
       ┌──────────────────────┐
       │  Level Shifter        │
       │                       │
       │  HV Side (5V)         │
       │  HV1 (CLK)  ◄────────┼──── LV1 ◄──── RP2040 GPIO 17
       │  HV2 (DATA) ◄────────┼──── LV2 ◄──── RP2040 GPIO 16
       │  HV (5V ref) ◄───────┼──── LV (3.3V) ◄─── RP2040 3V3 pin
       │  GND         ◄───────┼──── GND ◄──────── RP2040 GND
       └──────────────────────┘
               │      │
               │      │  Voltage Translation
               │      │  Bi-directional
               ▼      ▼
       ┌──────────────────────┐
       │   RP2040 Board        │
       │   (3.3V Logic)        │
       │                       │
       │   GPIO 17 (CLK)       │─┐
       │   GPIO 16 (DATA)      │ ├─── PIO State Machine 0
       │   3V3 (Power Out)     │─┘     (Hardware Protocol)
       │   GND (Ground)        │
       │   VSYS (5V In)        │◄──── USB 5V Power
       │                       │
       │   USB Port            │
       └───────────┬───────────┘
                   │
                   │ USB Data + Power
                   │
          ┌────────▼────────┐
          │  Modern Computer │
          │  (USB Host)      │
          └─────────────────┘
```

**Signal Explanation:**

1. **Keyboard Output (5V)**:
   - Keyboard generates CLK and DATA signals at 5V logic levels
   - Open-drain signalling: either device can pull lines LOW, pull-ups bring HIGH
   - Keyboard receives power: +5V on VCC, Ground on GND

2. **Level Translation**:
   - **HV Side** (High Voltage): Connected to keyboard, 5V reference
   - **LV Side** (Low Voltage): Connected to RP2040, 3.3V reference
   - Automatic bi-directional translation: 
     - 5V HIGH → 3.3V HIGH (keyboard to RP2040)
     - 3.3V HIGH → 5V HIGH (RP2040 to keyboard)
     - Either side can pull LOW (0V)

3. **RP2040 Processing**:
   - GPIO pins receive safe 3.3V signals
   - PIO state machine monitors CLK/DATA, extracts protocol frames
   - Main CPU processes scancodes, generates USB HID reports
   - USB stack communicates with host computer

4. **USB Connection**:
   - Computer provides 5V power on USB VBUS
   - RP2040 distributes power: internal 3.3V regulator, VSYS provides 5V
   - USB data lines (D+/D−) carry HID reports to computer

**Critical Points:**
- ⚠️ **Never** connect 5V keyboard signals directly to RP2040 GPIO
- ✓ **Always** use level shifter between different voltage domains
- ✓ **Common ground** required: keyboard GND = level shifter GND = RP2040 GND
- ✓ **Both voltage references** needed: level shifter HV=5V, LV=3.3V

---

## GPIO Pin Assignments

Pin assignments vary by protocol and are defined in each keyboard's configuration file. Understanding the assignment system helps with troubleshooting and custom builds.

### Default Pin Assignments

**AT/PS2 Protocol (Most Common):**
```c
// Keyboard
#define KEYBOARD_CLOCK_PIN  17
#define KEYBOARD_DATA_PIN   16

// Mouse (if enabled)
#define MOUSE_CLOCK_PIN     19
#define MOUSE_DATA_PIN      18
```

**Amiga Protocol:**
```c
#define KEYBOARD_CLOCK_PIN  17
#define KEYBOARD_DATA_PIN   16
#define KEYBOARD_RESET_PIN  15  // Amiga-specific reset line
```

**XT Protocol:**
```c
#define KEYBOARD_CLOCK_PIN  17
#define KEYBOARD_DATA_PIN   16
// XT is unidirectional (keyboard → host only)
```

**Apple M0110 Protocol:**
```c
#define KEYBOARD_CLOCK_PIN  17
#define KEYBOARD_DATA_PIN   16
// Poll-based protocol (host requests, keyboard responds)
```

### Why These Pins?

GPIO pins 16 and 17 are chosen as defaults for several reasons:
- **Physically adjacent**: Simplifies wiring, reduces chance of mistakes
- **PIO-capable**: All RP2040 GPIO pins can be assigned to PIO, no restrictions
- **Not used by built-in peripherals**: Avoids conflicts with UART, I2C, SPI on their default pins
- **Accessible on all boards**: Present on pin headers of Pico, WaveShare, Pimoroni, and Feather

### Custom Pin Assignment

You can change pin assignments in keyboard configuration files if needed:

**When to change pins:**
- Physical layout constraints in custom PCB designs
- Avoiding conflicts with additional peripherals (UART logging, I2C displays, SPI sensors)
- Personal preference or existing wiring

**How to change:**
1. Open `src/keyboards/<your_keyboard>/keyboard.h`
2. Modify `#define KEYBOARD_CLOCK_PIN` and `#define KEYBOARD_DATA_PIN`
3. Rebuild firmware with your keyboard configuration
4. Rewire hardware to match new pin assignments

**Limitations:**
- PIO programs may assume specific pin ordering (e.g., DATA = CLK - 1)
- Check PIO program requirements before changing pins
- Some protocols use multiple consecutive pins for efficiency

**See:** [Pin Configurations Guide](pin-configurations.md) for complete pin tables, protocol-specific requirements, and custom assignment examples.

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
