# LED Indicators

Keyboards with lock indicator LEDs use them for Caps Lock, Num Lock, and Scroll Lock. When you toggle one of these lock keys on your computer, the OS tells the keyboard to update its LEDs accordingly. This documentation covers how the converter handles these LED updates and passes them through to your keyboard.

The short version: it works transparently for protocols that support LEDs (AT/PS2), but protocols that don't have LED commands (XT, M0110) can't control the keyboard's physical LEDs. Amiga keyboards are a bit special—they've only got Caps Lock, and it's controlled through the handshake timing rather than a dedicated command.

---

## How Lock Indicator LEDs Work

When you press Caps Lock (or Num Lock, or Scroll Lock) on your keyboard, the OS receives a HID keycode for that key. The OS then updates its internal lock state and sends a HID output report back through USB telling the keyboard which LEDs should be lit.

Here's the flow:

```
┌──────────────────┐
│   You Press      │  Press Caps Lock on keyboard
│   Caps Lock      │
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│    Converter     │  Sends HID keycode to OS via USB
│  (HID Interface) │
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│    Operating     │  OS updates lock state,
│      System      │  sends HID output report with LED states
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│    Converter     │  Receives HID output report,
│   (TinyUSB)      │  extracts LED states
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│ HID Interface    │  Calls protocol LED function
│                  │
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│ Protocol Handler │  Protocol-specific LED command
│  (AT/PS2, etc.)  │  (if supported)
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│    Keyboard      │  LEDs update to match OS state
│  Physical LEDs   │
└──────────────────┘
```

The converter sits in the middle, receiving USB HID output reports from the OS and translating them into whatever LED command format the keyboard protocol understands.

---

## Protocol Support

Different keyboard protocols have different levels of LED support. Here's what works with each protocol:

| Protocol | Caps Lock | Num Lock | Scroll Lock | Implementation |
|----------|-----------|----------|-------------|----------------|
| AT/PS2   | ✓ Yes     | ✓ Yes    | ✓ Yes       | `0xED` command + state byte |
| XT       | ✗ No      | ✗ No     | ✗ No        | Protocol doesn't support LEDs |
| Amiga    | ✓ Yes     | ✗ No     | ✗ No        | Handshake timing (short=ON, long=OFF) |
| M0110    | ✗ No      | ✗ No     | ✗ No        | Protocol doesn't support LEDs |

### AT/PS2 LED Control

AT/PS2 keyboards support full LED control through the `0xED` command. This is a two-byte sequence: first send `0xED` (Set LED States), then send a bitmap byte indicating which LEDs should be on.

The LED state byte is structured like this:

```
Bit 0: Scroll Lock LED (1 = ON, 0 = OFF)
Bit 1: Num Lock LED    (1 = ON, 0 = OFF)
Bit 2: Caps Lock LED   (1 = ON, 0 = OFF)
Bits 3-7: Reserved (must be 0)
```

When the OS sends a HID output report, the converter extracts the lock states, packs them into this format, and sends the `0xED` command followed by the state byte to the keyboard. The keyboard acknowledges with `0xFA` (ACK) and updates its LEDs accordingly.

**Timing:** The LED command's sent as part of the main loop when the keyboard's in the INITIALISED state. There's no blocking—the protocol handler queues the command through the PIO hardware, which transmits it asynchronously whilst the main loop continues processing.

### XT LED Control

XT keyboards don't have any mechanism for the host to control LEDs. The protocol's unidirectional (keyboard-to-host only), so there's no way to send commands back to the keyboard.

If your XT keyboard has lock indicator LEDs, they won't update when you press the lock keys. The OS will still register the lock state internally (so Caps Lock behaviour works in software), but the physical LEDs won't reflect it.

### Amiga LED Control

Amiga keyboards have a Caps Lock LED, but no Num Lock or Scroll Lock LEDs. The LED's controlled through the handshake timing rather than a dedicated command.

After the converter receives a scancode from the keyboard, it sends a handshake pulse back to acknowledge receipt. The duration of this pulse controls the Caps Lock LED:

- **Short pulse (85μs LOW):** Turn Caps Lock LED on
- **Long pulse (85μs LOW + 1ms HIGH + 85μs LOW):** Turn Caps Lock LED off

The handshake timing's handled in the PIO program and protocol state machine. When the OS sends a HID output report with Caps Lock state, the protocol handler updates its internal state, and the next handshake pulse uses the appropriate timing.

**Why this works:** Amiga keyboards expect a handshake after every scancode. Since we're already sending handshakes, we just vary the timing based on whether Caps Lock should be on or off. It's a bit unusual compared to AT/PS2's explicit command, but it works reliably.

### M0110/M0110A LED Control

Apple M0110 and M0110A keyboards don't have lock indicator LEDs at all—there's nothing to control. The protocol doesn't define any LED commands, and the keyboards themselves don't have the hardware.

Caps Lock state is still tracked by the OS (so Caps Lock behaviour works), but there's no visual feedback on the keyboard itself.

---

## WS2812 RGB LEDs (Converter Status)

In addition to the keyboard's lock indicator LEDs, you can optionally configure WS2812 RGB LEDs on the converter itself. These are separate from the keyboard's physical LEDs and show the converter's status rather than lock states.

**Configuration:** Define `CONVERTER_LEDS` and `LED_PIN` in your build configuration ([`src/config.h`](../../src/config.h)) to enable WS2812 support. You can configure up to 4 LEDs.

**Status colours:**
- **Green:** Converter initialised and ready
- **Yellow:** Keyboard initialising
- **Red:** Error state (protocol error, keyboard not responding)
- **Alternating Green/Blue:** Command Mode active (waiting for command key)
- **Magenta:** Firmware update or factory reset in progress

The WS2812 LEDs are driven by a separate PIO program ([ws2812.pio](../../src/common/lib/ws2812/ws2812.pio)) and don't interfere with keyboard protocol timing. They provide status feedback and don't reflect lock indicator states.

**Brightness control:** You can adjust LED brightness through Command Mode (press both shifts for 3 seconds, then press 'L' to enter brightness adjustment). The brightness setting's saved to flash memory and persists across reboots.

For more details on the WS2812 implementation and configuration, have a look at the [LED Support documentation](led-support.md).

---

## Troubleshooting LED Issues

### LEDs Don't Update at All

If the keyboard's lock indicator LEDs aren't updating when you press lock keys:

**Check protocol support:** Make sure your keyboard protocol actually supports LED control. XT and M0110 protocols don't have LED commands, so the LEDs won't update no matter what.

**Verify wiring:** For bidirectional protocols (AT/PS2, Amiga), check that both the DATA and CLOCK lines are connected correctly through the level shifter. If the converter can send data to the keyboard, the LEDs should work.

**Check UART output:** Connect a UART adapter to GPIO 0/1 and look for LED-related messages. You should see `[INFO] LED update:` messages when the OS changes lock states. If you don't see these, the HID output reports aren't being received.

**Test with different OS:** Try toggling Caps Lock on a different computer or OS. Some systems send LED updates more reliably than others.

### AT/PS2 LEDs Update Slowly or Inconsistently

If the LEDs update but there's a noticeable delay:

**This is normal for busy typing:** The LED command's queued through the same protocol handler that's processing scancodes. During fast typing, there may be a brief delay whilst the protocol finishes processing key events before sending the LED command.

**Check for protocol errors:** If you see `[ERR]` messages on UART related to the keyboard not acknowledging commands, the LED command might be failing. The keyboard should send `0xFA` (ACK) after receiving the `0xED` command.

**Verify power supply:** Weak power (undervoltage on the 5V rail) can cause keyboards to behave unreliably. Measure the voltage at the keyboard connector—it should be stable at 5V.

### Amiga Caps Lock LED Doesn't Match OS State

If the Amiga keyboard's Caps Lock LED doesn't reflect the OS lock state:

**Check handshake timing:** The LED's controlled by the handshake pulse duration. Use a logic analyser or oscilloscope to verify the DATA line pulse width matches the expected timing (85μs for short, ~1.2ms for long).

**Verify protocol initialisation:** The Amiga protocol's fairly sensitive to timing during initialisation. If the handshake gets out of sync during the init sequence, the LED state might not track correctly. Try unplugging and replugging the converter to re-initialise.

**Test with known-good keyboard:** Some Amiga keyboards (particularly older ones) might have LED hardware issues. If possible, test with a different Amiga keyboard to rule out hardware problems.

### WS2812 Status LEDs Don't Work

If you've enabled WS2812 support but the LEDs don't light up:

**Check `CONVERTER_LEDS` definition:** Make sure `CONVERTER_LEDS` is defined before including the WS2812 headers. It should be in [`src/config.h`](../../src/config.h) or passed as a build flag.

**Verify `LED_PIN`:** The `LED_PIN` definition should match where you've connected the WS2812 data line. It defaults to GPIO 16, but you might need to change it for your board.

**Check power and data connections:** WS2812 LEDs need 5V power (VDD) and a data line (DIN) from the RP2040. The data line's 3.3V, which works with WS2812 specifications, but some variants may need a level shifter to 5V logic.

**Test LED brightness:** Try adjusting brightness through Command Mode. If the LEDs are too dim (brightness set to 0 or 1), they might appear off.

**Try a simple test:** Upload the WS2812 example from the Pico SDK to verify the LEDs and wiring work independently of the converter firmware.

---

That covers LED indicators. For AT/PS2 keyboards, LED control works through the protocol. For other protocols, LED support depends on what the protocol provides. The WS2812 status LEDs are optional and purely for showing converter status, not lock states.
