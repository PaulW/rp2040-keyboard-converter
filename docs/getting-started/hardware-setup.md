# Hardware Setup

**Time Required**: 30-60 minutes  
**Difficulty**: Beginner-friendly  
**Last Updated**: 29 October 2025

This guide walks you through connecting your vintage keyboard or mouse to an RP2040 board using a breadboard prototype. By the end, you'll have a fully wired converter ready for firmware installation.

---

## What You'll Build

A simple breadboard circuit that safely connects your vintage device (5V logic) to the RP2040 (3.3V logic) using a level shifter for voltage translation. No soldering required—everything uses jumper wires and pre-assembled modules.

![Breadboard Schematic](../images/hardware/breadboard-schematic.png)

*The breadboard schematic above shows: WaveShare RP2040-Zero, BSS138 level shifter, and a 5-pin 180° DIN socket for keyboard connection. This example demonstrates keyboard-only setup (no mouse attached).*

**Your completed setup will have:**
- Vintage device connected via its original connector
- Level shifter translating between 5V and 3.3V
- RP2040 board ready to run converter firmware
- USB connection to your computer for power and data

---

## Parts List

Before starting, gather these components:

### Essential Components

| Component | Quantity | Example | Price |
|-----------|----------|---------|-------|
| **RP2040 Board** | 1 | Raspberry Pi Pico or WaveShare RP2040-Zero | £4-8 |
| **Level Shifter Module** | 1 | BSS138-based 4-channel bidirectional | £2-5 |
| **Device Connector** | 1 | PS/2, DIN-5, or protocol-specific | £2-10 |
| **USB Cable** | 1 | USB-A to Micro-B (Pico) or USB-C (RP2040-Zero) | £1-5 |
| **Breadboard** | 1 | Half-size (400 tie-points) | £3-5 |
| **Jumper Wires** | 1 set | Male-to-male and male-to-female assortment | £5-10 |

**Total Cost**: Approximately £15-40 depending on board and connector choices

### Where to Buy

- **RP2040 Boards**: [Raspberry Pi Store](https://www.raspberrypi.com/products/), [Pimoroni](https://shop.pimoroni.com/), [Adafruit](https://www.adafruit.com/)
- **Level Shifters**: [Adafruit](https://www.adafruit.com/product/757), [SparkFun](https://www.sparkfun.com/products/12009), Amazon (search "BSS138 level shifter")
- **Connectors**: eBay (search "[protocol] extension cable" or "breakout board"), electronics distributors
- **Other Components**: Any electronics supplier (Amazon, eBay, local electronics shops)

---

## Safety First: Why Level Shifters Matter

⚠️ **Critical**: Never connect your vintage device directly to the RP2040!

- **Vintage devices use 5V logic** - Their CLK and DATA lines output 5V when HIGH
- **RP2040 uses 3.3V logic** - GPIO pins expect maximum 3.3V input
- **RP2040 GPIOs are NOT 5V tolerant** - Connecting 5V directly will **permanently destroy the chip**

The level shifter sits between them, automatically translating voltages in both directions:
- Device → RP2040: Converts 5V HIGH to 3.3V HIGH (safe)
- RP2040 → Device: Converts 3.3V HIGH to 5V HIGH (ensures device sees valid logic)

This isn't optional—it's essential for protecting your RP2040.

---

## Step 1: Identify Your Device's Connector

Find out which connector type your device uses. This determines the wiring in later steps.

### Common Connector Types

**PS/2 (Mini-DIN-6)**: Most common for AT/PS2 keyboards and mice
- 6-pin round connector
- Female on device cable, male on motherboard
- Used from late 1980s through 2000s

**DIN-5 (180°)**: Older AT and XT keyboards
- 5-pin circular connector (larger than PS/2)
- 180° pin arrangement (not 240° audio DIN-5)
- Used in 1980s IBM PC/XT and PC/AT

**RJ-10/RJ-11 (4P4C)**: Amiga 1000 and Apple M0110 keyboards
- Telephone-style modular jack
- 4 conductors (4P4C)
- Looks like small phone connector

**8-pin PCB Header**: Amiga 500/600/1200 internal keyboards
- Straight pin header on keyboard PCB
- Requires custom cable or adapter

### Connector Pinouts

Your device's pinout determines which wires carry CLK, DATA, power, and ground. Check your specific protocol documentation:

- **[AT/PS2 Protocol](../protocols/at-ps2.md#connectors-and-pinouts)** - PS/2 and DIN-5 pinouts
- **[XT Protocol](../protocols/xt.md#connector-and-pinout)** - DIN-5 pinout
- **[Amiga Protocol](../protocols/amiga.md#connectors-and-pinouts)** - RJ-10 and 8-pin header pinouts
- **[M0110 Protocol](../protocols/m0110.md#connector-and-pinout)** - RJ-10 pinout

**Tip**: If you have an extension cable or breakout board, use a multimeter to verify which wire corresponds to each pin before connecting.

---

## Step 2: Set Up the Breadboard

Insert your components into the breadboard, leaving space for wiring.

### Component Placement

```
Breadboard Layout (Top View):

     [===========================================]
     | + Rail (Red)                              |  ← Connect to +5V
     | - Rail (Blue/Black)                       |  ← Connect to GND
     |                                           |
     |    [RP2040 Board]                         |  ← Left side of breadboard
     |    (spanning center)                      |
     |                                           |
     |                   [Level Shifter Module]  |  ← Right side of breadboard
     |                   (4-channel BSS138)      |
     |                                           |
     | + Rail (Red)                              |  ← Connect to +5V
     | - Rail (Blue/Black)                       |  ← Connect to GND
     [===========================================]
```

**Placement Guidelines:**
1. Insert RP2040 board on left side, spanning the center gap (pins on both sides)
2. Insert level shifter module on right side, leaving several rows between it and RP2040
3. Leave the top and bottom power rails accessible for connections
4. Orient modules so pin labels are visible

**Tip**: Take a photo of your layout for reference when troubleshooting later.

---

## Step 3: Connect Power Rails

Establish power distribution across the breadboard.

### Power Connections

**Connect these wires:**

| From | To | Wire Color | Purpose |
|------|-----|------------|---------|
| RP2040 **3V3** pin | Top **+ rail** (one section) | Red | 3.3V for level shifter LV side |
| RP2040 **VSYS** pin | Top **+ rail** (different section) | Red | 5V for level shifter HV side and device |
| RP2040 **GND** pin | Top **- rail** | Black | Common ground |
| Top **- rail** | Bottom **- rail** | Black | Extend ground to both rails |

**Important**: Most breadboards have power rails split in the middle. Connect the 3.3V and 5V to **different rail sections** to avoid shorts. Use jumper wires to bridge rail sections if needed.

### Verify Power

Before proceeding:
1. Connect USB cable from RP2040 to your computer
2. RP2040's onboard LED should light up (usually green or red)
3. If no LED lights, check USB cable quality (data cables, not charge-only)
4. Disconnect USB after verification

---

## Step 4: Wire the Level Shifter

Connect the level shifter to power and ground, preparing it for signal translation.

### Level Shifter Pinout

Most BSS138 modules have this pinout:

```
   [Level Shifter Module]
   
                     LV ──┐────┌── HV
   Low Voltage Side LV1 ──┤    |── HV1   High Voltage Side
   (3.3V - RP2040)  LV2 ──┤    |── HV2   (5V - Device side)
                    LV3 ──┤    |── HV3
                    LV4 ──┤    |── HV4
                    GND ──┘────└── GND
```

### Power and Ground Connections

| Level Shifter Pin | Connect To | Wire Color | Purpose |
|-------------------|------------|------------|---------|
| **LV** | 3.3V power rail | Red | Low voltage reference |
| **LV GND** | Ground rail | Black | Common ground |
| **HV** | 5V power rail | Red | High voltage reference |
| **HV GND** | Ground rail | Black | Common ground |

**Note**: Both GND pins connect to the same common ground rail. All grounds must be connected together.

---

## Step 5: Connect Signal Lines

Wire the CLK and DATA lines through the level shifter.

### GPIO Pin Configuration

The breadboard schematic above uses GPIO 6 and 7 as an example configuration:

| Signal | RP2040 GPIO | Level Shifter LV | Level Shifter HV | Device Connection |
|--------|-------------|------------------|------------------|-------------------|
| **Data** | GPIO 6 | LV1 | HV1 | Device DATA |
| **Clock** | GPIO 7 | LV2 | HV2 | Device CLK |

**Important**: You can wire to **any available GPIO pins** on the RP2040—the pins shown above are just one example. Before building firmware, check and update the pin definitions in [`src/config.h`](../../src/config.h) to match your actual wiring:

```c
#define KEYBOARD_DATA_PIN 2  // Change to match your DATA pin
#define MOUSE_DATA_PIN 6     // Change to match your mouse DATA pin (if using mouse)
#define LED_PIN 29           // Change to match your LED data pin (if using LEDs)
```

**Hardware constraint**: The firmware expects CLOCK pin to be DATA pin + 1 for PIO efficiency (e.g., if DATA=2, then CLK=3). This means you can use GPIO pairs like 2/3, 6/7, 10/11, etc. If you follow the breadboard schematic above with GPIO 6/7, update `KEYBOARD_DATA_PIN` to `6` before building.

### Wiring Steps

**1. RP2040 to Level Shifter (LV side):**

| From | To | Wire Color (Suggested) |
|------|-----|------------------------|
| RP2040 **GPIO 6** | Level Shifter **LV1** | Green |
| RP2040 **GPIO 7** | Level Shifter **LV2** | Yellow |

**2. Level Shifter (HV side) to Device Connector:**

| From | To | Notes |
|------|-----|-------|
| Level Shifter **HV1** | Device **DATA** pin | Check pinout for your connector |
| Level Shifter **HV2** | Device **CLK** pin | Check pinout for your connector |

**3. Device Power and Ground:**

| Device Pin | Connect To | Notes |
|------------|-----------|-------|
| **VCC** or **+5V** | 5V power rail | Powers the device |
| **GND** | Ground rail | Common ground |

### Example: AT/PS2 Keyboard (PS/2 Connector)

PS/2 pinout (female connector on cable, looking at pins):

![PS/2 Connector Pinout](../images/connectors/kbd_connector_ps2.png)

**Connections:**
- Pin 1 (DATA) → Level Shifter HV1
- Pin 3 (GND) → Ground rail
- Pin 4 (VCC) → 5V power rail
- Pin 5 (CLK) → Level Shifter HV2

### Protocol-Specific Differences

**For Amiga keyboards**: You'll also need to connect the RESET line (GPIO 4 by default, which is DATA pin + 2) through the level shifter to the device's RESET pin. See [Amiga Protocol Guide](../protocols/amiga.md).

**For Mouse support**: If building with keyboard + mouse, you'll need a second set of signal connections using GPIO 6 (mouse DATA) and GPIO 7 (mouse CLK). Wire these through unused level shifter channels (LV3/HV3 and LV4/HV4).

---

## Step 6: Add Status LED (Optional but Recommended)

The converter uses an addressable WS2812B RGB LED to show its operational status. This is completely optional, but highly recommended—it provides instant visual feedback about the converter's state without needing to check your computer.

### Why Add an LED?

The status LED shows different colors for different states (these are the most common ones you may see):
- **🟢 Green**: Converter ready (device initialized and working)
- **🟠 Orange**: Not ready (waiting for device initialization)
- **🟣 Magenta**: Bootloader mode (ready for firmware flashing when triggered from the Command Mode option)
- **💚💙 Flashing Green/Blue**: Command Mode active (waiting for command selection)
- **💚💖 Flashing Green/Pink**: Log level selection mode (choose debug verbosity)
- **🌈 Rainbow**: Brightness adjustment mode (use +/- keys to adjust LED brightness)

Additionally, if you use 4 LEDs instead of 1, the extra 3 LEDs can show lock key states (Num Lock, Caps Lock, Scroll Lock).

### What You Need

| Component | Quantity | Notes |
|-----------|----------|-------|
| **WS2812B LED** | 1-4 | Addressable RGB LED (also called "NeoPixel") |
| **Jumper wires** | 3 | For VCC, GND, and Data connections |

**Where to buy**: Search for "WS2812B" or "NeoPixel" on Adafruit, SparkFun, Amazon, or eBay. They come in various forms:
- Individual LEDs with leads (easiest for breadboard)
- LED strips (can use just the first 1-4 LEDs)
- Breakout boards with built-in LEDs

**Cost**: £2-8 depending on format

### Wiring the LED

The WS2812B is 3.3V tolerant, so you can connect it directly to the RP2040 without level shifting.

**LED Pinout** (typical WS2812B):
```
     ┌───────────┐
     │  WS2812B  │
     │    LED    │
     │           │
VCC ─┤   ●   ●   ├─ GND
     │    ╲ ╱    │
DIN ─┤     X     ├─ DOUT
     │    ╱ ╲    │
     └───────────┘
```

**Connections:**

| WS2812B Pin | Connect To | Wire Color | Notes |
|-------------|------------|------------|-------|
| **VCC** | 3.3V power rail | Red | Use 3.3V, not 5V |
| **GND** | Ground rail | Black | Common ground |
| **DIN** (Data In) | RP2040 GPIO 29 | Any color | Default data pin |

**Important**: 
- Use the **DIN** (Data In) pin, not DOUT (Data Out)
- The default GPIO is pin 29 (defined in [`src/config.h`](../../src/config.h))
- If using a LED strip, connect to the first LED's DIN pin

### Multiple LEDs (Optional)

If you want lock indicator LEDs (Num Lock, Caps Lock, Scroll Lock), use 4 WS2812B LEDs:

**Chain them together:**
1. First LED: Connect as shown above (GPIO 29 → LED1 DIN)
2. Connect LED1 DOUT → LED2 DIN
3. Connect LED2 DOUT → LED3 DIN
4. Connect LED3 DOUT → LED4 DIN
5. All LEDs share the same VCC and GND (parallel connection)

**LED Assignment:**
- **LED 1**: Status (ready/not ready/bootloader)
- **LED 2**: Num Lock indicator
- **LED 3**: Caps Lock indicator
- **LED 4**: Scroll Lock indicator

**Note**: You need to enable lock LEDs in the build configuration. This is covered in the [Building Firmware](building-firmware.md) guide.

### Alternative: Using LED Strips

If you have a WS2812B LED strip:
1. Cut the strip after the 4th LED (if you want 4 total)
2. Identify the **input end** (usually has arrows showing data direction)
3. Connect the input end's VCC, GND, and DIN to your RP2040
4. The strip already has LEDs chained internally—just wire the first connection

**Tip**: LED strips often have JST connectors. You can buy a JST connector breakout or solder jumper wires directly to the strip's pads.

### Customizing the GPIO Pin

If GPIO 29 conflicts with your setup, you can change it before building firmware:

1. Edit [`src/config.h`](../../src/config.h)
2. Find `#define LED_PIN 29`
3. Change to your preferred GPIO (must be a free pin)
4. Save and rebuild firmware

---

## Step 7: Double-Check Your Wiring

Before powering on, verify all connections carefully.

### Verification Checklist

- [ ] **Power connections secure**: 3.3V and 5V rails properly connected to RP2040
- [ ] **Ground common**: All grounds connected together (RP2040, level shifter, device)
- [ ] **No voltage shorts**: 3.3V and 5V rails are separate, not touching
- [ ] **Level shifter powered**: LV connected to 3.3V, HV connected to 5V
- [ ] **Signal paths correct**: GPIO → LV pins → HV pins → Device
- [ ] **GPIO pins match config.h**: Verify your wiring matches the pin definitions in [`src/config.h`](../../src/config.h) (or update config.h to match your wiring)
- [ ] **Device connector correct**: CLK and DATA wires match your connector's pinout
- [ ] **No loose wires**: All connections firmly inserted into breadboard
- [ ] **LED connected (if using)**: WS2812B VCC to 3.3V, GND to ground, DIN to correct GPIO (check LED_PIN in config.h)

### Visual Inspection

Compare your breadboard to the wiring diagram above. Take a photo and trace each connection with a different colored marker if helpful.

**Common Mistakes to Avoid:**
- ❌ Connecting device CLK/DATA directly to RP2040 (bypassing level shifter)
- ❌ Swapping CLK and DATA lines
- ❌ Forgetting to connect level shifter power references (LV and HV pins)
- ❌ Using separate grounds (all grounds must be common)
- ❌ Poor breadboard connections (wiggle wires to ensure they're seated)

---

## Step 8: Test Your Hardware

Power up and verify connections before flashing firmware.

### Initial Power-Up

1. **Connect USB cable** from RP2040 to your computer
2. **RP2040 LED should light** (indicates board is powered)
3. **Device should power on** (keyboard might flash LEDs briefly during self-test)
4. **WS2812B LED should light up** (if installed—may show orange or off until firmware is loaded)
5. **No smoke, no burning smell** (if you smell anything, disconnect immediately)

### Expected Behavior (Before Firmware)

Without firmware, the RP2040 won't respond to the device yet. This is normal! You should see:

✅ **RP2040 board LED lit** (power indicator)  
✅ **Device powered** (LEDs may illuminate if keyboard has always-on indicators)  
✅ **WS2812B LED powered** (if installed—may be off or dim until firmware loaded)  
✅ **RP2040 appears as USB device** (computer may show "RP2 Boot" or similar)  

❌ **Keyboard does nothing when you type** (no firmware loaded yet—this is expected!)  
❌ **Computer doesn't recognize it as a keyboard** (normal—you haven't flashed firmware)

### Troubleshooting Power Issues

**RP2040 LED not lit?**
- Check USB cable (try a different cable known to work for data)
- Verify RP2040 board isn't damaged
- Check USB port on computer (try different port)

**Device not powering on?**
- Verify 5V connection from VSYS to device VCC
- Check device connector pinout (VCC and GND correct?)
- Test continuity with multimeter if available

**Device LEDs flickering or behaving strangely?**
- Normal during self-test on some keyboards
- If continuous flickering, may indicate power issue (check 5V connection)

---

## Step 9: What's Next?

Your hardware is complete! Now you'll build and flash firmware.

### Ready to Continue

With working hardware, proceed to:

1. **[Building Firmware](building-firmware.md)** - Compile firmware for your specific device
2. **[Flashing Firmware](flashing-firmware.md)** - Install firmware on the RP2040
3. **Testing and verification** - Confirm everything works together

### If Something's Not Right

Hardware issues are usually wiring mistakes. Common fixes:

- **Review the verification checklist** - Did you miss a connection?
- **Check pinout diagrams** - Are CLK and DATA swapped?
- **Verify level shifter orientation** - Are HV and LV sides correct?
- **Test continuity** - Use multimeter to check connections
- **Check for shorts** - Make sure no adjacent breadboard holes are bridged
- **WS2812B not lighting?** - Verify 3.3V power, check DIN connection to GPIO 29

**Need more help?**
- 📖 [Hardware Deep Dive](../hardware/README.md) - Detailed component information
- 📖 [Troubleshooting Guide](../advanced/troubleshooting.md) - Common issues and solutions
- 💬 [Ask on GitHub Discussions](https://github.com/PaulW/rp2040-keyboard-converter/discussions) - Community support
- 🐛 [Report Issues](https://github.com/PaulW/rp2040-keyboard-converter/issues) - If you found a bug

---

## Complete Wiring Diagram

Here's a complete reference showing all connections:

```
                     Vintage Device (e.g., PS/2 Keyboard)
                              ┌──────────┐
                              │  Pins:   │
                              │  CLK ────┼───────┐
                              │  DATA────┼─────┐ │
                              │  VCC ────┼───┐ │ │
                              │  GND ────┼─┐ │ │ │
                              └──────────┘ │ │ │ │
                                           │ │ │ │
                                           │ │ │ │
                    ┌──────────────────────┘ │ │ │
                    │  ┌─────────────────────┘ │ │
                    │  │  ┌────────────────────┘ │
                    │  │  │  ┌───────────────────┘
                    │  │  │  │
    Breadboard:     │  │  │  │
                    │  │  │  │
    [GND Rail]──────┴──┴──┴──┴─────────────────────[GND]
                    │  │  │                           │
    [5V Rail]───────┴──┴──┴───────────────────────[VSYS]
                       │  │                           │
                       │  │    Level Shifter          │
                       │  │   ┌──────────┐            │
                       │  │   │ HV   LV  │            │
                       │  │   │ HV1  LV1 │────────[GPIO X] CLK*
                       │  └───│ HV2  LV2 │────────[GPIO X+1] DATA*
                       │      │ GND  GND │            │
                       │      └──────────┘            │
                       │          │    │              │
    [GND Rail]─────────┴──────────┴────┴───────────[GND]
                                       │              │
    [3.3V Rail]────────────────────────┘           [3V3]
                                                      │
                                              ┌───────┴───────┐
                                              │   RP2040      │
                                              │   Board       │
                                              │               │
                                              │   [USB Port]  │
                                              └───────┬───────┘
                                                      │
                                                      └─── To Computer
```

**Connection Summary:**
- Device VCC → 5V Rail → RP2040 VSYS
- Device GND → GND Rail (common to all)
- Device DATA → Level Shifter HV1 → Level Shifter LV1 → RP2040 GPIO (your DATA pin)*
- Device CLK → Level Shifter HV2 → Level Shifter LV2 → RP2040 GPIO (your CLK pin)*
- Level Shifter HV → 5V Rail
- Level Shifter LV → 3.3V Rail → RP2040 3V3
- Level Shifter GND → GND Rail (both sides)

**\* GPIO pin assignment**: The diagram shows example GPIO connections. Your actual pin numbers will match what you defined in [`src/config.h`](../../src/config.h) (KEYBOARD_DATA_PIN and KEYBOARD_DATA_PIN+1). The hardware requires CLK pin to be DATA pin + 1.

---

**Next**: [Building Firmware →](building-firmware.md)
