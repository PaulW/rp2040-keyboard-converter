# Hardware Guide

This guide covers everything you need to know about the hardware side of building the converter—from choosing the right components to understanding why certain design decisions were made. Whether you're planning a simple breadboard prototype or designing a custom PCB to fit inside your keyboard, you'll find what you need here. That said, the documentation may not cover every possible scenario or component combination, so if you're working with something unusual, feel free to ask in the discussions.

---

## What's in This Guide

- **[Build Approaches](#choosing-your-build-approach)** - Breadboard vs PCB: which is right for you?
- **[Component Deep Dive](#understanding-components)** - Everything about RP2040 boards and level shifters
- **[Connectors and Cables](#connectors-and-cables)** - Finding the right connectors and where to get them
- **[Status LEDs](#status-leds)** - Adding visual feedback with WS2812B RGB LEDs
- **[Breadboards and Prototyping Supplies](#breadboards-and-prototyping-supplies)** - What you need for breadboard builds
- **[Custom PCB](#custom-pcb)** - Professional PCB design for internal installations

---

## Choosing Your Build Approach

There are a few different ways you can build this converter, and each has its trade-offs. I'd say it's worth taking a few minutes to think about what you're trying to achieve before you start buying components or wiring things up.

### Breadboard Prototype

**Best for**: First-time builds, testing, experimentation, or if you want to support multiple keyboards

A breadboard build is where I'd recommend starting. There's no soldering involved, you can put it together in under an hour, and if something's not working you can easily swap things around. It's forgiving, which is great if you're new to this or just want to test whether your keyboard actually works before you commit to anything permanent.

**What makes breadboards great:**
- **Quick to set up**: 30-60 minutes and you're done
- **No soldering needed**: Just jumper wires and pre-made modules
- **Easy to modify**: Want to try different pins? Just pull it out and move it
- **Low cost**: About £15-25 for everything
- **Reusable**: Take it apart and use the bits for something else
- **Great for learning**: You can actually see all the connections

**The trade-offs:**
- **Needs an external enclosure**: Breadboards require external housing
- **Connections can work loose**: Breadboards are fine for testing, but they're not great for long-term reliability
- **More wiring**: It's not as tidy as a PCB
- **Not ideal for portability**: If you're taking your keyboard places, loose connections aren't brilliant

**When this makes sense:**
- You're new to electronics and want something forgiving
- You're testing multiple keyboards or protocols
- You're developing new features or troubleshooting issues
- You want to try it out before committing to something permanent
- Budget's a concern

For the step-by-step breadboard build, check out the **[Getting Started Guide](../getting-started/hardware-setup.md)**.

---

### Custom PCB

**Best for**: Permanent installations, internal keyboard mounting, or if you want it to look professional

If you have a keyboard you really care about—maybe it's a collectible, or just something you want to use long-term—a custom PCB is worth considering. Everything's on one neat board, no loose wiring, and if you design it right you can mount it inside the keyboard so it's completely hidden. I built one for my IBM Model F PC/AT, and you'd never know it was there from the outside.

**What makes PCBs great:**
- **Looks professional**: Clean and compact
- **Internal installation**: Hide it inside the keyboard case—no external box
- **Solid connections**: Soldered joints don't work loose
- **Compact**: You can design boards to fit specific spaces
- **Integrated**: Everything on one board—level shifter, RP2040, connectors
- **Strain relief**: Proper mounting means cables aren't going to pull things apart

**The trade-offs though:**
- **Higher upfront cost**: PCB fabrication is about £20-40 for a batch of 5 boards
- **Needs PCB skills**: You'll need to know your way around KiCad or similar, and be comfortable soldering
- **Less flexible**: If you want to change protocols or pins, you're designing a new board
- **Keyboard-specific**: Designs are optimised for particular keyboards
- **Takes longer**: Design, order, wait for delivery, assemble—you're looking at 2-4 weeks

**When this makes sense:**
- You have a valuable or collectible keyboard that deserves a proper installation
- You're comfortable with PCB design and soldering (or keen to learn)
- You want the converter to be invisible or minimally invasive
- You have a specific keyboard in mind for permanent conversion
- You're planning to convert multiple units of the same keyboard

**Real example**: My custom PCB for the IBM Model F PC/AT is only 40×30mm and mounts on the keyboard's internal frame using existing screw holes. It's got the level shifter, USB-C, RP2040, everything—completely invisible from outside. Total cost works out to about £5 per board when ordering 5 from JLCPCB with their SMT assembly service.

Check out the **[Custom PCB Guide](custom-pcb.md)** for the full design, fabrication files, and installation photos.

---

## Understanding Components

Every converter needs the same core components, whether you're building on breadboard or designing a PCB. Let me walk you through what each one does and why it matters.

### RP2040 Microcontroller Board

This is the brain of the whole operation. The RP2040 handles all the protocol translation, USB communication, and features like Command Mode and Keytest.

**Why the RP2040 works so well for this:**

Other converters use Atmel AVR chips (like in Arduino Pro Micro boards), and they work fine. But the RP2040 has something really useful: **PIO state machines**. These are like little independent hardware processors that can monitor signals with microsecond precision without the main CPU needing to do anything. This means the converter can handle keyboard timing without using much CPU at all.

Traditional microcontrollers either bit-bang protocols in software (struggles with timing) or use fixed hardware peripherals like UART or SPI (not flexible enough). PIO gives you hardware-level timing with software-level flexibility—each state machine runs independently, watching the CLOCK and DATA lines.

**Key specs:**
- **Dual ARM Cortex-M0+ cores** running at 125MHz (we only use one)
- **264KB SRAM** - plenty of room, we run the code from RAM for performance
- **Eight PIO state machines** - hardware-level I/O with microsecond precision
- **30 GPIO pins** - flexible, can be assigned to any function
- **Native USB 1.1** support via TinyUSB
- **Low power** - minimal CPU usage even when typing

**Which board should you get?**

They're all based on the same RP2040 chip, so they're functionally identical. It comes down to size, USB connector, and what you're planning to do with it.

| Board | Size | USB | Price | When to choose it |
|-------|------|-----|-------|-------------------|
| **Raspberry Pi Pico** | 51×21mm | Micro-B | ~£4 | Best starting point—cheap, widely available, tons of documentation |
| **WaveShare RP2040-Zero** | 23.5×18mm | USB-C | ~£6 | Great for internal installs, tiny, has built-in RGB LED |
| **Pimoroni Tiny 2040** | 22×18mm | USB-C | ~£8 | Similar to WaveShare but premium components, nice if you value quality |
| **Adafruit Feather RP2040** | 51×23mm | USB-C | ~£12 | Premium option with battery power and expansion capabilities |

**My recommendation**: Start with a Raspberry Pi Pico if you're breadboarding. It's the cheapest, you can get them anywhere, and there's loads of help available if you get stuck. If you're planning an internal installation, the Waveshare RP2040-Zero is what I use—it's tiny, has USB-C, and even comes with an RGB LED built in.

### Bi-Directional Level Shifter

**This is the most important safety component** - don't skip it!

Here's the thing: your keyboard speaks 5V logic, but the RP2040 speaks 3.3V logic. The RP2040's GPIO pins are **NOT 5V tolerant**. If you connect 5V directly to them, you'll damage the chip. There's no "it might survive"—it just dies.

**What it does:**

It sits between your keyboard and the RP2040, translating voltages in both directions:
- Keyboard → RP2040: Converts 5V down to 3.3V (safe for the RP2040)
- RP2040 → Keyboard: Converts 3.3V up to 5V (proper logic levels for the keyboard)

**Why bi-directional?**

The AT/PS2 protocol uses open-drain signaling on both CLOCK and DATA lines—both the keyboard and host need to be able to pull the lines LOW. A bi-directional level shifter handles this automatically without needing any direction control signals.

**Which level shifter should you get?**

**BSS138-Based 4-Channel Level Shifter**

This is what I'd recommend. It uses BSS138 N-channel MOSFETs and is simple to use:
- **Price**: £2-5 from Adafruit, SparkFun, or generic suppliers on Amazon
- **Speed**: Way faster than we need (MHz capable, we're only doing kHz)
- **Channels**: 4 independent channels (you'll use 2 for CLOCK/DATA, leaving 2 spare)
- **Widely available**: You can get these pretty much anywhere
- **Well documented**: Loads of tutorials and examples online

**TXB0104 4-Bit Level Shifter** (Alternative)

This is another good option if you can find one:
- **Price**: £3-6
- **Chip**: TXB0104 with automatic direction sensing
- **Speed**: 100 Mbps data rate
- **Slightly more compact** than BSS138 modules
- **Availability**: More specialised, works just as well

**Important notes:**
- Both sides need their voltage references (LV=3.3V, HV=5V)
- All grounds must be tied together (keyboard, level shifter, RP2040)
- Never connect keyboard signals directly to RP2040—always through the level shifter
- GPIO pins are configurable in firmware (no fixed assignments)

**How to wire it:**

```
Level Shifter Module:
LV (Low Voltage)  ─── RP2040 3V3 pin
LV1 (Channel 1)   ─── RP2040 GPIO (you choose which one)
LV2 (Channel 2)   ─── RP2040 GPIO (you choose which one)  
LV3, LV4          ─── Unused (available for mouse or future expansion)
GND               ─── Common ground

HV (High Voltage) ─── Keyboard +5V or RP2040 VSYS
HV1 (Channel 1)   ─── Keyboard CLOCK line
HV2 (Channel 2)   ─── Keyboard DATA line
HV3, HV4          ─── Unused
GND               ─── Common ground
```

---

## Connectors and Cables

The connector you need depends on your keyboard. IBM-compatible connectors are widely used, but other keyboards use different types entirely.

### Most Common Connectors

**PS/2 (Mini-DIN-6)**

This is what you'll find on AT/PS2 keyboards and mice from the late 1980s onwards. It's a 6-pin round connector that was widespread through the '90s and 2000s.

- **Used by**: AT/PS2 keyboards and mice
- **Availability**: Pretty easy to find—eBay, electronics distributors, or just buy a PS/2 extension cable and cut it in half

**DIN-5 (180°)**

The older, larger connector you'll find on IBM PC/XT and early AT keyboards. It's got a 180° pin arrangement (not the 240° arrangement used for audio equipment—don't mix them up).

- **Used by**: IBM PC/XT and early AT keyboards
- **Availability**: eBay, or salvage from old motherboards

**Other Keyboards**

Some keyboards use completely different connectors—telephone-style modular jacks, internal PCB headers, or proprietary designs. You'll need to identify what your specific keyboard uses and find the pinout.

### Where to Get Connectors

**Easiest option**: Buy a keyboard extension cable on eBay and cut it in half. You get a connector with wires already attached.

**Other options:**
- **Breakout boards**: Search for "PS/2 breakout Arduino"—these give you screw terminals or pin headers
- **Salvage**: Desolder connectors from damaged motherboards or old keyboards (free, but needs desoldering skills)
- **Distributors**: Mouser, Digi-Key, or Farnell stock panel mount sockets if you want something proper

### Finding Your Keyboard's Pinout

The documentation might help—check if there's a specific page for your keyboard model in the **[protocol documentation](../protocols/README.md)**. If not, you'll need to trace the connections yourself or search online for pinout diagrams.

### USB Cables

Don't forget you'll need a USB cable to connect the RP2040 to your computer!

**What you need depends on your board:**
- **Raspberry Pi Pico**: USB-A to Micro-B
- **WaveShare RP2040-Zero / Pimoroni Tiny 2040**: USB-A to USB-C (or USB-C to USB-C for modern computers)

**Quality matters**: Cheap charging-only cables won't work—make sure it's rated for data transfer. If your converter isn't being recognised by your computer, try a different cable before you start troubleshooting everything else. Cable issues can be easy to miss.

---

## Status LEDs

The converter supports WS2812B addressable RGB LEDs for visual feedback. This is completely optional, but it's useful—you can see at a glance what state the converter's in without needing to check your computer.

### What the LED Shows

**Status Colours:**
- **🟢 Green**: Everything's working, converter's ready
- **🟠 Orange**: Waiting for keyboard initialisation
- **🟣 Magenta**: Bootloader mode (ready for firmware flashing)
- **💚💙 Flashing Green/Blue**: Command Mode active
- **💚💖 Flashing Green/Pink**: Log level selection mode  
- **🌈 Rainbow**: Brightness adjustment mode

**Lock Indicators:**
If you use 4 LEDs instead of 1, the extra 3 show lock key states:
- **LED 2**: Num Lock
- **LED 3**: Caps Lock
- **LED 4**: Scroll Lock

### What You Need

**WS2812B LEDs** (also called "NeoPixels"):
- **Individual LEDs**: Around £1-2 each, easiest for breadboard
- **LED strips**: £5-8, useful if you want multiple LEDs (just use the first 1-4)
- **Breakout boards**: £2-5, pre-mounted LEDs with easier connections

**Where to buy**: Adafruit, SparkFun, Amazon, eBay—just search for "WS2812B" or "NeoPixel"

### Wiring

The WS2812B is 3.3V tolerant, so you can connect it straight to the RP2040:

```
WS2812B LED:
3.3V  ──→ RP2040 3V3 (not 5V!)
GND   ──→ RP2040 GND
DIN   ──→ RP2040 GPIO 29 (default, can be changed)
```

**Important**: Use DIN (Data In), not DOUT (Data Out).

### Multiple LEDs (For Lock Indicators)

Chain them together by connecting DOUT of one LED to DIN of the next:

```
GPIO 29 ──→ LED1 DIN
            LED1 DOUT ──→ LED2 DIN
                         LED2 DOUT ──→ LED3 DIN
                                      LED3 DOUT ──→ LED4 DIN

All LEDs share 3.3V and GND in parallel
```

**LED assignments:**
1. Status LED
2. Num Lock indicator
3. Caps Lock indicator
4. Scroll Lock indicator

### Using LED Strips

For WS2812B LED strips:
1. Cut after the 4th LED (if you want 4 total)
2. Find the input end (arrows show data direction)
3. Connect 3.3V, GND, and DIN to the first LED
4. The strip's already chained internally

**Tip**: LED strips often use JST connectors—you can either get a JST breakout adapter or solder wires directly to the pads.

### Changing the GPIO Pin

Default is GPIO 29 (defined in [`src/config.h`](../../src/config.h)). To change it:

1. Edit [`src/config.h`](../../src/config.h)
2. Find `#define LED_PIN 29`
3. Change to your preferred GPIO
4. Rebuild firmware

---

## Breadboards and Prototyping Supplies

If you're building a breadboard prototype (which I'd recommend for your first build), here's what you'll want to get.

### Breadboards

**Half-size breadboard** (400 tie-points, ~30 rows): About £3-5, and it's plenty big enough for the converter plus level shifter and RP2040 board. Full-size breadboards work too, but they're overkill unless you're planning to add loads of extra stuff.

### Jumper Wires

Get yourself an assorted kit with different lengths—you're looking at £5-10 for a decent set. Colour coding really helps keep track of what's what:
- **Red**: Power (VCC, 5V, 3.3V)
- **Black**: Ground
- **Yellow**: Clock signals
- **Green**: Data signals
- **Other colours**: Whatever else you need

**Wire types you'll want:**
- **Male-to-Male**: Breadboard-to-breadboard connections
- **Male-to-Female**: Breadboard to pin headers (on the RP2040 or level shifter)
- **Female-to-Female**: Less common for this project, but sometimes handy

**Tip**: Pre-formed wire kits are worth the small extra cost over loose assortments. Poor connections cause intermittent faults that can be difficult to debug.

### Enclosures (Optional but Recommended)

Once you have everything working, you'll want to protect it and stop things from shorting out:

**Project boxes**: Hammond 1591 series, Takachi cases, or generic ABS boxes run about £5-15. You'll want something around 100×60×25mm minimum to fit the RP2040, breadboard, and level shifter comfortably. Make sure you can access the USB port and route the keyboard cable properly.

**3D printed cases**: Loads of designs on Thingiverse and Printables for specific RP2040 boards. Search for "[your board name] case" and you'll find plenty of options. Some even include mounting for breadboards.

**DIY**: If you have access to CAD software (FreeCAD, Fusion 360, OnShape), you can design your own. Just measure your components, add a few mm clearance, design some mounting features, and export the STL.

---

## Custom PCB

For detailed information about designing and building custom PCBs, including a complete worked example for the IBM Model F PC/AT, see the dedicated **[Custom PCB Guide](custom-pcb.md)**.

That guide covers:
- Complete PCB design process (schematic, layout, fabrication)
- Bill of materials and component selection
- Fabrication options (JLCPCB SMT assembly, DIY soldering)
- Installation guide with photos
- Cost breakdown (about £5 per board for quantities of 5)

---

## Related Documentation

**For building your first converter:**
- **[Getting Started Guide](../getting-started/README.md)** - Step-by-step build instructions
- **[Hardware Setup](../getting-started/hardware-setup.md)** - Detailed breadboard wiring guide

**Protocol-specific information:**
- **[Protocols Overview](../protocols/README.md)** - All supported protocols

**Keyboard configurations:**
- **[Supported Keyboards](../keyboards/README.md)** - Keyboard-specific configs and quirks

**Source code:**
- Keyboard configurations: [`src/keyboards/`](../../src/keyboards/)
- Protocol implementations: [`src/protocols/`](../../src/protocols/)

---

**Questions or stuck on something?**  
Pop into [GitHub Discussions](https://github.com/PaulW/rp2040-keyboard-converter/discussions) or [report a bug](https://github.com/PaulW/rp2040-keyboard-converter/issues) if you've found an issue.

