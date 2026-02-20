# LED Support

The converter uses LEDs to provide visual feedback about its current state—whether it's ready to use, experiencing an error, or waiting for a command. This page explains what the different LED patterns mean, how to add your own RGB LEDs for enhanced visual feedback, and how the LED system works internally.

---

## Status LED: Your Converter's Dashboard

Every converter needs at least one status LED to indicate what it's doing. This LED serves as your primary interface for understanding the converter's state without needing to connect debug tools or check logs.

The status LED shows different colors and patterns depending on what's happening:

**Solid Green** - Everything is working perfectly. The converter has successfully initialised, the keyboard is communicating properly, and USB is connected. This is what you should see during normal operation.

**Solid Orange** - The converter is starting up. You'll see this briefly during power-on whilst firmware initialises hardware, loads configuration, and establishes communication with your keyboard. Should transition to green once initialisation completes.

**Solid Magenta** - Bootloader mode active (firmware flash mode). The converter is in BOOTSEL mode waiting for a firmware upload. This is normal when holding the BOOTSEL button during power-on or after issuing the 'B' command in Command Mode. See [building-firmware.md](../getting-started/building-firmware.md) for firmware installation instructions.

**Alternating Green/Blue** - Command Mode is active. You've successfully held the activation keys for 3 seconds, and the converter is waiting for you to press a command key (B, D, F, or L). See the [Command Mode documentation](command-mode.md) for details.

**Alternating Green/Pink** - You've pressed 'D' in Command Mode and the converter is waiting for you to select a log level (1, 2, or 3).

**Rainbow Cycling** - You've pressed 'L' in Command Mode and the converter is displaying a rainbow cycling pattern whilst you adjust LED brightness using +/- keys. The LED cycles through all colors of the rainbow (red → yellow → green → cyan → blue → magenta → red...) to help you judge the brightness setting. See [Command Mode - 'L' Command](command-mode.md#l---adjust-led-brightness) for complete details.

Understanding these patterns helps you diagnose issues quickly. Solid green means everything is working. Solid orange means the converter is still initialising. Solid magenta means bootloader mode is active (this is normal when flashing firmware). The alternating patterns indicate Command Mode is active and waiting for input.

---

## Adding WS2812 RGB LEDs

The basic status LED provides essential feedback, but you can enhance the converter with WS2812 RGB LEDs (also known as NeoPixels or addressable RGB LEDs) for more visible or decorative indicators.

### What Are WS2812 LEDs?

WS2812 LEDs are "smart" RGB LEDs—each LED contains a tiny controller chip that understands a simple data protocol. You can chain multiple WS2812 LEDs together and control each one independently using a single data line.

Common WS2812 products:
- Individual 5mm or 8mm through-hole LEDs
- LED strips with flexible PCB backing
- LED rings in various sizes
- LED matrices for displays

The converter supports two LED configurations: either 1 LED (status only) or 4 LEDs (status + 3 lock indicators). The number of LEDs is determined by whether `CONVERTER_LOCK_LEDS` is enabled in [`src/config.h`](../../src/config.h).

### Wiring WS2812 LEDs

WS2812 LEDs (also called NeoPixels) are addressable RGB LEDs that need just three connections to work. They're 3.3V tolerant, so you can connect them directly to the RP2040 without level shifting.

#### Understanding WS2812 Pinout

A typical WS2812 LED has four pins (or pads on a breakout board):

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

**Pin functions:**
- **VCC (or 5V)**: Power supply input
- **GND**: Ground connection
- **DIN**: Data Input - receives the control signal
- **DOUT**: Data Output - passes data to the next LED in a chain

The important distinction: **DIN** is where you connect the control signal from the RP2040. **DOUT** is only used when chaining multiple LEDs together.

#### Single LED Connection (Status Only)

For a single status LED, make these three connections:

| WS2812 Pin | Connect To | Wire Suggestion | Notes |
|------------|------------|-----------------|-------|
| **VCC** | 3.3V power rail | Red wire | Use 3.3V, not 5V (safer for mixed voltage setups) |
| **GND** | Ground rail | Black wire | Common ground with RP2040 |
| **DIN** | RP2040 GPIO 29 | Yellow/data wire | Default data pin (configurable in [`config.h`](../../src/config.h)) |

**Power notes**: WS2812 LEDs are specified for 5V operation but work reliably at 3.3V with reduced maximum brightness. Since the converter uses gamma-corrected brightness control (typically running at 10-50% brightness), 3.3V provides plenty of light whilst simplifying the wiring. If you need maximum brightness, connect VCC to VSYS (5V from USB) instead—just ensure your USB power supply can handle it.

**Signal level**: The RP2040's 3.3V GPIO output works with most WS2812 variants. The LED's data input threshold is typically 0.7 × VCC, so at 3.3V that's 2.31V—well within the RP2040's 3.3V HIGH output. If you experience intermittent behavior or color glitches, try adding a 220-470Ω resistor in series with the data line for signal protection.

#### Four LED Configuration (Status + Lock Indicators)

To enable lock indicator LEDs, you need exactly 4 LEDs total and must enable `CONVERTER_LOCK_LEDS` in [`src/config.h`](../../src/config.h). WS2812 LEDs are designed to be chained together—each LED receives data on its DIN pin, extracts the first 24 bits for itself, and passes the remaining data out its DOUT pin to the next LED.

**Wiring 4 chained LEDs:**

1. **First LED (Status)**: Connect as shown in the single LED table above
   - RP2040 GPIO 29 → LED1 DIN
   
2. **Chain the remaining LEDs**:
   - LED1 DOUT → LED2 DIN
   - LED2 DOUT → LED3 DIN  
   - LED3 DOUT → LED4 DIN

3. **Power and ground**: All LEDs share power and ground
   - Connect all VCC pins to 3.3V rail (or 5V VSYS)
   - Connect all GND pins to ground rail
   - Use parallel connections, not daisy-chained

**LED assignment** (from [`led_helper.c`](../../src/common/lib/led_helper.c)):
- **LED 1**: Status LED (ready/not ready/bootloader/command mode)
- **LED 2**: Num Lock indicator
- **LED 3**: Caps Lock indicator  
- **LED 4**: Scroll Lock indicator

**Current draw considerations**: Each WS2812 LED can draw up to 60mA at full white brightness (all three RGB elements at maximum). With gamma correction and typical brightness levels (10-50%), actual current is usually 3-15mA per LED. For the single LED configuration, draw is 3-15mA typical. For four LEDs at moderate brightness, draw is 12-60mA total—well within USB's 500mA budget. Only at maximum brightness (rarely needed) would you approach the theoretical 240mA maximum.

**Important**: The firmware expects either 1 or 4 LEDs depending on the `CONVERTER_LOCK_LEDS` setting. If you wire 4 LEDs but don't enable `CONVERTER_LOCK_LEDS`, only the first LED will function. If you enable `CONVERTER_LOCK_LEDS` but wire only 1 LED, the lock indicators won't display (the firmware will try to send data to LEDs 2-4, but nothing will happen visually).

#### Using LED Strips

WS2812 LED strips (sold as "NeoPixel strips" or "addressable RGB strips") work perfectly for this project. You only need the first LED (status only) or first 4 LEDs (status + lock indicators), but the strip format makes mounting easier.

**Connections:**
- Find the strip's input end (usually marked with arrows showing data direction, or labeled "DIN" / "5V" / "GND")
- Connect the input end's three wires: 5V (or 3.3V), GND, and DIN
- Ignore DOUT—you don't need to connect anything to the rest of the strip
- The converter will only control the configured number of LEDs (1 or 4); remaining LEDs stay off

**Strip advantages**: Pre-soldered connections, built-in spacing, often includes mounting adhesive. Many strips also have built-in capacitors and resistors for signal protection.

#### Wire Length and Reliability

The WS2812 data signal is sensitive to noise and timing. For best reliability:

**Keep data wires short**: Under 30cm (12 inches) ideally. The signal degrades over distance and becomes more susceptible to electromagnetic interference. If your layout requires longer runs (like mounting LEDs in a keyboard case), use shielded wire or add a resistor.

**Signal protection resistor**: Adding a 220-470Ω resistor in series with the data line (between GPIO 29 and LED DIN) protects against voltage spikes and reflections. This is especially helpful with:
- Long wire runs (over 30cm)
- LED strips (higher capacitance)  
- Electrically noisy environments
- Intermittent color glitching

**Capacitor for power stability**: A 100-1000µF capacitor across the power rails (near the LED) smooths voltage fluctuations during LED color changes. This prevents brownouts when LEDs switch to bright white (high current draw). Most LED breakout boards and strips include this capacitor; loose LEDs may benefit from adding one.

#### Troubleshooting Wiring Issues

**LED doesn't light up**:
- Check power connections (VCC and GND must both be connected)
- Verify you're using DIN, not DOUT (easy mistake!)
- Confirm GPIO 29 is set correctly in [`config.h`](../../src/config.h) (or your custom pin)
- Try 5V power instead of 3.3V (some LEDs need higher voltage)

**Wrong colors displayed** (red appears green, etc.):
- Change `CONVERTER_LEDS_TYPE` in [`config.h`](../../src/config.h) to different color order (see Configuration section below)
- Most WS2812B are `LED_GRB`, but variants exist with different orders

**Intermittent behavior or flickering**:
- Shorten data wire length
- Add 220-470Ω resistor in series with data line
- Add 100µF capacitor across power rails near LED
- Check for loose connections or poor solder joints

**First LED works, but chained LEDs don't**:
- Verify DOUT from LED1 connects to DIN of LED2 (not DOUT to DOUT)
- Check that all LEDs share power and ground
- Test each LED individually to isolate faulty units

### Configuration

The firmware needs to know which GPIO pin connects to your WS2812 LEDs and what type of LED chip you're using. This is configured in [`src/config.h`](../../src/config.h):

```c
#define CONVERTER_LEDS               // Enable LED support
#define CONVERTER_LEDS_TYPE LED_GRB  // LED chip type (color order)
#define CONVERTER_LOCK_LEDS          // Enable 3 lock indicator LEDs
#define LED_PIN 29                   // GPIO pin for WS2812 data line
```

**LED Type Selection**: Different WS2812 variants use different color orders. The converter supports 6 types (defined in [`src/common/lib/types.h`](../../src/common/lib/types.h)):
- `LED_GRB` - Most common WS2812/WS2812B chips (Green-Red-Blue order)
- `LED_RGB` - Alternative chips (Red-Green-Blue order)  
- `LED_BGR` - Blue-Green-Red order
- `LED_RBG` - Red-Blue-Green order
- `LED_GBR` - Green-Blue-Red order
- `LED_BRG` - Blue-Red-Green order

If your LEDs show wrong colors (e.g., red appears green), try changing `CONVERTER_LEDS_TYPE` to a different color order. `LED_GRB` works for most standard WS2812/WS2812B LEDs.

Change these values if you're using a different GPIO pin or LED type. Remember to rebuild and flash the firmware after changing configuration. See [`building-firmware.md`](../getting-started/building-firmware.md) for build instructions.

---

## How WS2812 LEDs Work

Understanding the basics of how WS2812 LEDs communicate helps explain why the converter implements them the way it does, and why certain timing constraints exist.

### The Single-Wire Protocol

WS2812 LEDs are "addressable" RGB LEDs—each LED contains a tiny controller chip. Unlike regular LEDs that simply turn on when powered, WS2812 LEDs receive digital data that tells them what color to display.

The clever part: they use timing to encode data. A single data wire carries both 0s and 1s by varying how long the signal stays HIGH versus LOW:

- **'0' bit**: HIGH for 0.4µs, then LOW for 0.85µs
- **'1' bit**: HIGH for 0.8µs, then LOW for 0.45µs

Each LED needs 24 bits (8 for green, 8 for red, 8 for blue). After receiving 24 bits, the LED displays that color and passes any additional bits to the next LED in the chain.

A special "reset" signal (holding the line LOW for at least 50µs) tells all LEDs to simultaneously latch their received colors and display them. This is why you can update 4 LEDs almost instantly—you send the data in sequence, then the reset pulse makes them all change at once.

**Timing breakdown**:
- Transmitting 24 bits at 1.25µs per bit = 30µs per LED
- For 4 LEDs: 4 × 30µs = 120µs data transmission
- Plus 50µs minimum reset pulse (converter uses 60µs for safety)
- Total minimum interval: 120µs + 60µs = 180µs

These microsecond-level timings are too precise for software loops. Even small delays from interrupts or other code would corrupt the signal. That's where hardware assistance comes in.

### PIO: Dedicated Hardware for Precise Timing

The RP2040's PIO (Programmable I/O) is a specialised processor designed for exactly this kind of task. It's like having a tiny dedicated coprocessor whose only job is generating perfectly-timed signals.

**Why PIO matters**:

The PIO runs independently at 125MHz with 8-nanosecond resolution. Once you load a program into it and feed it data, it generates the WS2812 signal without any CPU involvement. Your main code continues processing keyboard events whilst the PIO handles LED timing in parallel.

This is critical for keyboard operation. If updating LEDs required the CPU to carefully time every microsecond for 180µs, that would be 18 main loop iterations where no keyboard processing happens. You'd lose keystrokes. With PIO, LED updates happen "in the background" whilst the CPU keeps working.

**The PIO program** (from [`ws2812.pio`](../../src/common/lib/ws2812/ws2812.pio)):

The actual program is only 6 assembly instructions. It runs at 2.5MHz (125MHz divided by 50), where each cycle takes 0.4µs. The program reads bits from a FIFO buffer and generates HIGH/LOW pulses with exact WS2812 timing.

The CPU's job is simple: compute the RGB color (with brightness and color order), check if the PIO's buffer has space, and write the 24-bit value. The PIO does all the microsecond-precise timing.

### LED Chaining: One Wire, Multiple LEDs

Chaining WS2812 LEDs is simple: wire the output of one LED to the input of the next. Data flows through the chain automatically.

When you send 96 bits (4 × 24) down the wire:
1. First LED grabs the first 24 bits and displays that color
2. Second LED grabs the next 24 bits
3. Third LED grabs the next 24 bits
4. Fourth LED grabs the final 24 bits

Each LED extracts its data and passes the remainder along. The converter calls `ws2812_show()` four times in sequence—once per LED—then waits 50µs for the reset pulse.

**Timing constraint**: The converter enforces a minimum 60µs (50µs + safety margin) between update cycles to ensure the reset pulse completes. For 4 LEDs, the total minimum interval is 180µs: 120µs for data transmission plus 60µs reset time.

### Non-Blocking by Design

If LED updates blocked execution, keyboard processing would freeze and protocols with strict timing requirements would miss clock edges, causing lost keystrokes.

The solution: two-level checking prevents blocking.

**Level 1: FIFO availability**. Before writing to the PIO, check if its buffer has space. If the buffer is full (PIO is still transmitting the previous LED's data), don't wait—just return and try again in 10µs.

**Level 2: Timing interval**. Track when the last LED update finished. If less than 180µs has elapsed, defer the update and try again in 10µs.

If either check fails, the LED system marks the update as "pending" and the main loop retries on the next iteration. Typically, LEDs update within 10µs when conditions are met. The LED system never blocks—it either succeeds immediately or tries again soon.

### Performance Impact

The actual CPU cost of preparing an LED update is tiny: about 300 nanoseconds per LED. This includes looking up the brightness multiplier, extracting RGB components, scaling them, and reordering to GRB format.

At 10µs per main loop iteration, 300ns is 3% CPU utilisation. The PIO handles the actual transmission (30µs per LED) in parallel whilst the CPU processes keyboard data.

The converter's non-blocking LED design ensures status indicators never interfere with keyboard operation, even when updating 4 LEDs simultaneously. You get visual feedback without sacrificing responsiveness.

---

## LED Brightness Control

Both the status LED and WS2812 LEDs support brightness adjustment through Command Mode. The brightness setting ranges from 0 (off) to 10 (maximum).

See [Command Mode - 'L' Command](command-mode.md#l---adjust-led-brightness) for complete instructions on adjusting LED brightness. The command uses rainbow cycling with +/- keys to adjust the brightness level, with a 3-second timeout that automatically saves your selection.

### Gamma Correction

LED brightness control uses gamma correction to make the perceived brightness change linearly as you adjust the setting. Without gamma correction, LEDs appear to jump from "barely visible" to "blindingly bright" with just a small change in the control value.

The human eye perceives brightness logarithmically—doubling the electrical power doesn't make an LED appear twice as bright. Gamma correction compensates for this by using a lookup table (defined in [`src/common/lib/ws2812/ws2812.c`](../../src/common/lib/ws2812/ws2812.c) as `BRIGHTNESS_LUT`) that maps the linear brightness setting (0-10) to perceptually uniform steps using γ ≈ 2.5.

**Brightness levels** (from [`ws2812.c`](../../src/common/lib/ws2812/ws2812.c)):

| Level | Electrical | Perceived | Use Case |
|-------|------------|-----------|----------|
| 1 | 0.8% | ~10% | Very dim (night mode, dark rooms) |
| 2 | 2.0% | ~20% | Quite dim |
| 3 | 3.9% | ~30% | Dim (default for many users) |
| 4 | 7.8% | ~40% | Low brightness |
| 5 | 13.7% | ~50% | Medium-low (balanced) |
| 6 | 23.5% | ~60% | Medium |
| 7 | 35.3% | ~70% | Medium-high |
| 8 | 52.9% | ~80% | Bright |
| 9 | 74.5% | ~90% | Very bright |
| 10 | 100.0% | ~100% | Maximum (full LED capability) |

The practical result: each brightness increment from 1 to 10 produces an approximately equal perceived change in brightness. Without gamma correction, levels 1-5 would barely be visible whilst 9-10 would be a massive jump. The lookup table makes adjustment intuitive—each step feels like the same increase in brightness.

---

## Converter Lock Indicator LEDs

In addition to the status LED, the converter can provide its own dedicated lock indicator LEDs separate from your keyboard's built-in lock LEDs. When `CONVERTER_LOCK_LEDS` is enabled in [`src/config.h`](../../src/config.h), the converter drives 3 additional WS2812 LEDs (Num Lock, Caps Lock, Scroll Lock) that reflect the host computer's lock state.

### Why Converter Lock LEDs?

Some keyboards don't have lock indicator LEDs (like XT keyboards or early Macintosh keyboards), or their LEDs may not function due to protocol limitations. The converter's lock LEDs provide universal lock state indication regardless of your keyboard's capabilities.

### Implementation

When `CONVERTER_LOCK_LEDS` is defined:
- LED 0: Status LED (shows converter state - green/orange/magenta/alternating patterns)
- LED 1: Num Lock indicator (lights up green when Num Lock is on)
- LED 2: Caps Lock indicator (lights up green when Caps Lock is on)  
- LED 3: Scroll Lock indicator (lights up green when Scroll Lock is on)

The converter listens for USB HID LED state reports from the host computer and updates these indicators accordingly. This works independently of your keyboard's protocol or hardware capabilities.

### Configuration

Enable in [`src/config.h`](../../src/config.h):

```c
#define CONVERTER_LOCK_LEDS          // Enable 3 lock indicator LEDs
#define CONVERTER_LOCK_LEDS_COLOR 0x00FF00  // Green when lock active
```

The lock LEDs share the same WS2812 data line as the status LED, chained after it. Wire 4 WS2812 LEDs in series: status LED → Num Lock → Caps Lock → Scroll Lock.

---

## Keyboard Lock LEDs (Protocol-Dependent)

Separately from the converter's lock indicators, your keyboard may have its own built-in lock LEDs. Whether these work depends on the keyboard protocol and hardware capabilities.

### How Keyboard Lock LEDs Work

When you press Caps Lock:

1. The converter sends the keystroke to your computer
2. Your operating system toggles its internal Caps Lock state
3. The OS sends an LED status report back to the converter
4. The converter translates this into the command your keyboard's protocol expects
5. Your keyboard receives the command and lights up (or turns off) the Caps Lock LED

This happens quickly enough to feel instant.

If your protocol doesn't support a particular LED, the converter silently ignores LED commands for that indicator. Everything else continues to work normally. This is where the converter's lock indicator LEDs (when enabled) provide useful feedback regardless of protocol support.

---

## How the LED System Works

Understanding the implementation helps explain why LEDs behave the way they do and what limitations exist.

### Non-Blocking Updates

The LED system never blocks the main loop. Updating LED color or brightness happens asynchronously, allowing the converter to continue processing keystrokes and USB communication without interruption.

For WS2812 LEDs specifically, the converter uses the RP2040's PIO (Programmable I/O) hardware to generate the precise timing signals these LEDs require. The PIO runs independently of the CPU, meaning LED updates happen in hardware whilst the CPU continues processing keyboard data. See [`src/common/lib/ws2812/ws2812.pio`](../../src/common/lib/ws2812/ws2812.pio) for the PIO program implementation.

Before sending new color data to WS2812 LEDs, [`ws2812_show()`](../../src/common/lib/ws2812/ws2812.c) checks if the PIO's FIFO (first-in-first-out buffer) has space. If the FIFO is full, the update is deferred. This prevents blocking—if the PIO is busy, we'll try again on the next main loop iteration.

### Status Tracking

The LED system maintains state information about what the converter is doing, implemented in [`src/common/lib/led_helper.c`](../../src/common/lib/led_helper.c):

- Initialisation in progress (orange - `CONVERTER_LEDS_STATUS_NOT_READY_COLOR`)
- Ready for use (green - `CONVERTER_LEDS_STATUS_READY_COLOR`)
- Bootloader/firmware flash mode (magenta - `CONVERTER_LEDS_STATUS_FWFLASH_COLOR`)
- Command Mode active with specific submenu (various alternating patterns)

This state drives the LED color and pattern. The LED update function in [`led_helper_task()`](../../src/common/lib/led_helper.c) checks the current state on each main loop iteration and updates the LED accordingly.

For alternating patterns (like green/blue in Command Mode), the system uses time-based toggling. It tracks when the LED last changed color and toggles to the other color after a fixed interval (100ms for all command mode states). This timing uses the non-blocking time tracking system—checking elapsed time with `to_ms_since_boot(get_absolute_time())` without sleeping.

### Color Representation

WS2812 LEDs use 24-bit RGB color: 8 bits each for red, green, and blue. Different WS2812 variants expect different byte orders (GRB is most common, but RGB, BGR, RBG, GBR, and BRG are also supported). The converter handles this via `CONVERTER_LEDS_TYPE` in [`src/config.h`](../../src/config.h), and [`ws2812_set_color()`](../../src/common/lib/ws2812/ws2812.c) converts RGB values to the correct byte order.

Colors are defined in [`src/config.h`](../../src/config.h) as RGB hex values:
- Green: 0x00FF00 (ready state, command mode alternation)
- Orange: 0xFF2800 (not ready/initialising)
- Magenta: 0xFF00FF (bootloader mode)
- Blue: 0x0000FF (command mode alternation)
- Pink: 0xFF1493 (log level selection alternation)

When brightness adjustment is applied via [`ws2812_set_brightness()`](../../src/common/lib/ws2812/ws2812.c), these values are scaled using the gamma correction lookup table. For example, brightness level 5 (mid-level) scales Green (0x00FF00) according to `BRIGHTNESS_LUT[5]` to maintain perceptually uniform brightness.

---

## Power Considerations

WS2812 LEDs can consume significant power, especially when using multiple LEDs or showing bright white colors.

Each WS2812 LED draws:
- ~1mA when off or showing dim colors
- ~20mA per primary color at full brightness (20mA red, 20mA green, or 20mA blue)
- ~60mA when showing full white (all three colors at maximum)

If you're powering everything from USB:

**USB 2.0 ports provide 500mA maximum**. The RP2040 itself uses ~50-100mA depending on activity. That leaves about 400mA for LEDs and your keyboard.

With 4 WS2812 LEDs (1 status + 3 lock indicators) showing white at full brightness, that's 240mA—well within USB limits.

For reference, the converter's maximum 4 LEDs at various brightness levels:
- Brightness 10 (maximum) showing white: ~240mA for LEDs
- Brightness 5 (mid-level) showing white: ~120mA for LEDs  
- Brightness 2 (low) showing typical colors: ~30-50mA for LEDs

To reduce power consumption:
- Lower brightness (level 5 instead of 10 uses approximately half the power after gamma correction)
- Avoid full white on all LEDs simultaneously (single-color status indicators use ~1/3 the power)
- Disable lock indicator LEDs if not needed (comment out `CONVERTER_LOCK_LEDS` in [`src/config.h`](../../src/config.h))

---

## Troubleshooting

### Status LED Doesn't Light Up

If the status LED never illuminates:

**Check your wiring** - Verify the LED is connected to the correct GPIO pin with appropriate current-limiting resistor (typically 220-330Ω for standard LEDs).

**Verify polarity** - LEDs only work in one direction. If it doesn't light, try reversing the connection.

**Check GPIO configuration** - Ensure [`src/config.h`](../../src/config.h) defines the correct pin for your status LED.

**Test with WS2812** - If you're using a WS2812 as your status LED, make sure it's wired correctly (5V, GND, and data on the configured pin).

### WS2812 LEDs Don't Work

If WS2812 LEDs don't light up or show incorrect colors:

**Check power** - WS2812s need 5V. Verify your power connections are solid and providing proper voltage.

**Verify data pin** - The data wire must connect to the GPIO pin specified in [`src/config.h`](../../src/config.h) as `LED_PIN` (default GPIO 29).

**Check signal direction** - WS2812 LEDs have a data input and data output. Make sure you're feeding the signal to the input side (usually marked with an arrow or "DIN").

**Check wiring distance** - Long data wires (over 30cm) can cause signal degradation. Try a shorter wire or add a small resistor (220-470Ω) in series with the data line.

**Verify LED type** - Different WS2812 variants use different color orders. Check [`src/config.h`](../../src/config.h) `CONVERTER_LEDS_TYPE` setting. Most WS2812/WS2812B use `LED_GRB`. If colors are wrong (e.g., red appears green), try different color order values (LED_RGB, LED_BGR, etc.).

### Lock State LEDs Don't Update

**Converter Lock LEDs** (when `CONVERTER_LOCK_LEDS` enabled):
- Verify all 4 WS2812 LEDs are wired in series: status → Num Lock → Caps Lock → Scroll Lock
- Check that LEDs 1-3 light up when you toggle lock keys
- See UART logs for USB HID LED state reports from host

**Keyboard Lock LEDs** (built into keyboard):

- **Check protocol support** - Not all protocols support host-controlled LEDs. See the protocol limitations section above.
- **Verify keyboard works** - Test the keyboard on its original hardware if possible. Some keyboards have faulty LED circuits.
- **Check UART logs** - Connect via UART and watch for LED command messages when you press lock keys. If you see commands being sent but the LEDs don't respond, the keyboard might not be receiving them properly.

### LEDs Show Wrong Colors

If WS2812 LEDs display unexpected colors:

**Check LED type configuration** - Different WS2812 variants use different color orders. The converter supports 6 types (LED_GRB, LED_RGB, LED_BGR, LED_RBG, LED_GBR, LED_BRG) configured via `CONVERTER_LEDS_TYPE` in [`src/config.h`](../../src/config.h). Most standard WS2812/WS2812B chips use `LED_GRB`. If your red appears green, try changing to `LED_RGB` or another color order variant.

**Verify power supply** - Insufficient power can cause color shifting, especially at higher brightness.

**Check for interference** - Electrical noise on the data line can corrupt color data. Add a small capacitor (0.1μF) between the LED's power and ground pins, close to the LED.

---

## Related Documentation

**Features that control LEDs**:
- [Command Mode](command-mode.md) - Adjust LED brightness through Command Mode 'L' command
- [Configuration Storage](config-storage.md) - How brightness settings persist in flash

**Implementation details**:
- [`led_helper.c`](../../src/common/lib/led_helper.c) - LED state management and color control
- [`led_helper.h`](../../src/common/lib/led_helper.h) - LED API and configuration
- [`ws2812.c`](../../src/common/lib/ws2812/ws2812.c) - WS2812 driver with gamma correction and color order conversion
- [`ws2812.h`](../../src/common/lib/ws2812/ws2812.h) - WS2812 API
- [`ws2812.pio`](../../src/common/lib/ws2812/ws2812.pio) - PIO assembly for WS2812 timing
- [`types.h`](../../src/common/lib/types.h) - LED type enum definitions (LED_RGB, LED_GRB, etc.)
- [`config.h`](../../src/config.h) - LED configuration constants and color definitions

**Hardware setup**:
- [Hardware Setup Guide](../getting-started/hardware-setup.md) - Physical wiring instructions

**External references**:
- [WS2812B Datasheet](https://cdn-shop.adafruit.com/datasheets/WS2812B.pdf) - Official WS2812B specifications (timing, electrical characteristics)

---

**Questions or stuck on something?**  
Pop into [GitHub Discussions](https://github.com/PaulW/rp2040-keyboard-converter/discussions) or [report a bug](https://github.com/PaulW/rp2040-keyboard-converter/issues) if you've found an issue.
