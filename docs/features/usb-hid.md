# USB HID Interface

This page explains how the converter communicates with your computer over USB. Understanding the USB side helps explain why the converter works everywhere—from BIOS screens to modern operating systems—without requiring any drivers or special software.

---

## What Your Computer Sees

When you plug the converter into a USB port, your computer performs a process called "enumeration" where it asks "what are you?" The converter responds by describing itself as a USB HID (Human Interface Device) composite device with multiple interfaces (defined in [`usb_descriptors.c`](../../src/common/lib/usb_descriptors.c)).

Depending on your build configuration, the converter presents itself with up to three separate interfaces, each handling a different type of input:

**Interface 1: Keyboard**  
Sends keystroke reports using the standard Boot Protocol format. This interface handles all regular typing, including modifier keys (Shift, Ctrl, Alt, GUI/Windows) and up to 6 simultaneously pressed regular keys.

**Interface 2: Consumer Control**  
Sends media key and system control reports. This separate interface handles volume controls, play/pause, track navigation, and other multimedia functions independently from regular typing.

**Interface 3: Mouse** (optional, if enabled at build time)  
Sends mouse movement and button reports. When you've connected an AT/PS2 mouse alongside your keyboard, this interface provides standard pointer input with scroll wheel support.

Each interface has its own dedicated endpoint and operates independently—pressing a media key won't interfere with typing, and moving the mouse won't block keyboard input.

---

## Boot Protocol: Universal Compatibility

The converter uses USB HID Boot Protocol for the keyboard and mouse interfaces. This design choice provides universal compatibility at the cost of some advanced features.

### Why Boot Protocol?

Boot Protocol is a simplified HID format specifically designed to work in environments where full HID drivers aren't available. When your computer is first powering on and showing BIOS/UEFI screens, it needs basic keyboard input before any operating system has loaded. Boot Protocol makes this possible.

The format is intentionally simple and standardised. Every USB-capable system knows how to interpret Boot Protocol reports without requiring custom drivers or configuration. This means the converter works identically on Windows, macOS, Linux, FreeBSD, Chrome OS, and even in firmware setup screens.

### Trade-offs

Boot Protocol limits keyboards to reporting 6 simultaneously pressed regular keys plus 8 modifiers. Modern gaming keyboards often use Report Protocol instead, which allows unlimited simultaneous keys (called NKRO or N-Key Rollover).

For keyboard conversion, this limitation doesn't matter. Most keyboards have hardware constraints—their key matrix design uses diodes and multiplexing that can only detect 2-3 simultaneous regular keys before "ghosting" or "blocking" occurs. Offering NKRO would be pointless when the source keyboard can't provide that many simultaneous inputs.

The universal compatibility of Boot Protocol far outweighs the unused NKRO capability for this use case.

---

## Keyboard Reports: How Keystrokes Travel

Every time a key state changes (pressed or released), the converter sends an 8-byte report to the host (see [`hid_interface.c`](../../src/common/lib/hid_interface.c)). This report represents the current state of all keys—not just what changed, but everything currently pressed.

### Report Structure

The 8-byte keyboard report follows the standard Boot Protocol format defined in the USB HID specification:

```
Byte 0: Modifier keys bitmask
Byte 1: Reserved (always 0x00)
Bytes 2-7: Keycodes for up to 6 pressed keys
```

### Modifier Byte Explained

Byte 0 contains 8 bits, each representing one modifier key. When a bit is set to 1, that modifier is currently pressed. When 0, it's released.

```
Bit 0: Left Control       Bit 4: Right Control
Bit 1: Left Shift         Bit 5: Right Shift
Bit 2: Left Alt           Bit 6: Right Alt
Bit 3: Left GUI (Win)     Bit 7: Right GUI (Win)
```

The host operating system tracks left vs right modifiers separately, even though most software treats them identically. This distinction matters for some applications—for example, some games bind different actions to left vs right Shift.

### Regular Key Slots

Bytes 2 through 7 contain USB HID keycodes for regular keys (see [`hid_keycodes.h`](../../src/common/lib/hid_keycodes.h) for complete definitions). Each keycode is a number from 0x00 to 0xFF that identifies a specific key according to the USB HID Usage Tables specification.

Common examples:
- `0x04` = A
- `0x05` = B
- `0x1C` = Y
- `0x1D` = Z
- `0x00` = No key (empty slot)

When fewer than 6 keys are pressed, the remaining slots contain 0x00. When you press a 7th regular key whilst 6 are already held, the converter keeps the existing report and ignores the additional key until a slot is freed.

### Example Report

Here's what the report looks like when pressing A+B whilst holding Left Shift:

```
Byte:    0    1    2    3    4    5    6    7
Value: 0x02 0x00 0x04 0x05 0x00 0x00 0x00 0x00
        |         |    |
        |         |    └── B key (0x05)
        |         └── A key (0x04)
        └── Left Shift (bit 1 set)
```

When you release the Shift key, byte 0 changes to 0x00 (no modifiers pressed). When you release A, its slot changes to 0x00 and subsequent keys shift positions if needed.

### Stateful Design

This "stateful" approach differs from some other input systems. Instead of sending "key down" and "key up" events, USB HID sends complete state snapshots. Each report tells the host exactly what's pressed right now.

This design has advantages: if a report gets lost (rare but possible), the next report still contains correct information. There's no risk of the host and converter getting out of sync about which keys are held.

---

## Timing and Performance

The converter processes keystrokes through several stages, from receiving the original protocol signal to transmitting the USB report (implementation in [`hid_interface.c`](../../src/common/lib/hid_interface.c)):

| Stage | What Happens |
|-------|--------------|  
| **Protocol reception** | PIO hardware captures signal, interrupt fires, byte goes into ring buffer |
| **Main loop pickup** | Main loop polls ring buffer and retrieves byte |
| **Scancode processing** | State machine handles multi-byte sequences (E0/F0 prefixes, special keys) |
| **Keymap translation** | Convert protocol-specific scancode to USB HID keycode |
| **USB transmission** | Build HID report, call TinyUSB stack, transmit to host |

The non-blocking architecture ensures low-latency processing at each stage.

### USB Polling Intervals

The host computer polls HID devices at regular intervals. The converter's USB descriptor specifies an 8 millisecond polling interval for keyboard and mouse reports, which is standard for USB Full Speed HID devices.

### Throughput Characteristics

Each character requires two USB reports—one with the key pressed, one with it released. The USB polling interval limits how quickly sequential reports can be transmitted to the host.

---

## Consumer Control: Media Keys

Modern keyboards include dedicated keys for controlling media playback and system functions—volume up/down, play/pause, mute, and so on. These use a separate USB interface with a different report format.

### Why a Separate Interface?

Consumer Control commands use different HID usage codes than regular keyboard keys (defined in [`hid_keycodes.h`](../../src/common/lib/hid_keycodes.h)). By putting them on a separate interface (Interface 2), they operate independently from typing. This means pressing a media key doesn't consume one of the six regular key slots in the keyboard report.

It also prevents conflicts. If media keys shared the keyboard interface, the host might interpret a volume control command as a keyboard shortcut, or vice versa.

### Report Format

Consumer Control reports are simpler than keyboard reports—just 2 bytes:

```
Bytes 0-1: 16-bit usage code (little-endian)
```

When a media key is pressed, the converter sends the corresponding usage code. When released, it sends 0x0000 (nothing pressed).

Common usage codes include:
- `0x00E9` = Volume Increment
- `0x00EA` = Volume Decrement  
- `0x00E2` = Mute
- `0x00CD` = Play/Pause
- `0x00B5` = Scan Next Track
- `0x00B6` = Scan Previous Track

### Mapping Media Keys

Most keyboards don't have dedicated media keys—they predate that concept. However, if your keyboard has extra keys you rarely use (SysReq, Scroll Lock, Pause), you can remap them to consumer control functions in your keyboard's layout configuration file.

For example, the IBM Model M Enhanced has a SysReq key that most people never press. Mapping this to Play/Pause gives you media control without sacrificing any commonly-used keys.

---

## Mouse Support

When you enable mouse support at compile time and connect an AT/PS2 mouse, the converter adds Interface 3 for mouse input (see [`handle_mouse_report()`](../../src/common/lib/hid_interface.c) for implementation). This interface uses Boot Protocol like the keyboard, ensuring universal compatibility.

### Mouse Report Format

Mouse reports are 5 bytes:

```
Byte 0: Button states (bit flags)
Byte 1: X movement (signed, -127 to +127)
Byte 2: Y movement (signed, -127 to +127)
Byte 3: Wheel movement (signed, -127 to +127)
Byte 4: Pan movement (signed, -127 to +127)
```

### Button States

Byte 0 uses individual bits for each button:

```
Bit 0: Left button
Bit 1: Right button
Bit 2: Middle button
Bit 3: Back button (4th button, if IntelliMouse Explorer)
Bit 4: Forward button (5th button, if IntelliMouse Explorer)
Bits 5-7: Reserved
```

Standard 3-button mice only use bits 0-2. IntelliMouse-compatible mice with scroll wheels work with the first 3 bits plus byte 3 (wheel). IntelliMouse Explorer mice with 5 buttons use all fields.

### Movement Values

Bytes 1 and 2 contain signed 8-bit values representing relative movement since the last report. Positive X moves right, negative X moves left. Positive Y moves down (standard computer graphics coordinates), negative Y moves up.

The PS/2 mouse protocol actually allows values from -255 to +255, but USB HID Boot Protocol only supports -127 to +127. The converter clamps larger values to fit this range. In practice, this rarely matters—moving the mouse fast enough to exceed ±127 in 8ms is difficult.

### Scroll Wheel

Byte 3 contains signed 8-bit scroll wheel movement. Positive values scroll up (away from you), negative values scroll down (toward you). Most mice send ±1 or ±2 per detent (notch) when scrolling.

### Polling Rate

The converter sends mouse reports at the same 125 Hz rate as keyboard reports (every 8ms USB poll). This matches or exceeds most mice, which typically sample at 40-100 Hz. The result is smooth cursor movement without perceptible lag.

---

## LED Control: Host-to-Device Communication

USB HID isn't just one-way. Whilst most communication flows from the converter to the host (sending keystroke and mouse reports), the host can also send reports back to control keyboard LEDs (handled by [`tud_hid_set_report_cb()`](../../src/common/lib/hid_interface.c)).

### LED Status Reports

When you press Caps Lock, Num Lock, or Scroll Lock, your operating system updates its internal keyboard state and sends a 1-byte LED report back to the keyboard:

```
Bit 0: Num Lock LED (1 = on, 0 = off)
Bit 1: Caps Lock LED (1 = on, 0 = off)
Bit 2: Scroll Lock LED (1 = on, 0 = off)
Bits 3-7: Reserved
```

The converter receives this report and translates it into whatever LED control commands your specific keyboard protocol supports. This all happens automatically and typically completes within 10-20 milliseconds, fast enough to feel instant.

### Protocol Limitations

Not all keyboard protocols support host-controlled LEDs:

**AT/PS2 keyboards**: Full support for all three LEDs. The converter sends LED control commands and the keyboard responds.

**XT keyboards**: No LED support in the protocol. XT keyboards predate host-controlled lock indicators. The keyboard either has no LEDs or manages them internally.

**Amiga keyboards**: Caps Lock only, and it's managed by the keyboard's own firmware. The host can't directly control it. The Amiga keyboard watches Caps Lock key presses and toggles its own LED.

**M0110 keyboards**: No LED support. Early Macintosh keyboards relied on the Mac's screen to show Caps Lock status.

If your keyboard's protocol doesn't support a particular LED, the converter simply ignores that LED control bit. Everything else continues to work normally.

---

## Data Flow Architecture

Here's how data flows through the entire system (implemented across multiple modules, with HID handling in [`hid_interface.c`](../../src/common/lib/hid_interface.c)), from the keyboard to your computer and back:

```
[Keyboard]
        ↓ Native protocol (AT/PS2, XT, Amiga, M0110)
   [PIO Hardware]
        ↓ Captures signal with microsecond precision
   [Ring Buffer]
        ↓ Temporary storage (32 bytes)
   [Scancode Processor]
        ↓ Handle multi-byte sequences and state machine
   [Keymap Translation]
        ↓ Convert scancode to USB HID keycode
   [HID Report Builder]
        ↓ Package into 8-byte USB report
   [TinyUSB Stack]
        ↓ USB protocol handling
   [Host Computer]

   [Host Computer]
        ↓ LED state change (Caps/Num/Scroll Lock)
   [TinyUSB Stack]
        ↓ Receive 1-byte LED report
   [LED Handler]
        ↓ Translate to protocol-specific command
   [Protocol Interface]
        ↓ Send LED command in native protocol
   [Keyboard]
        ↓ Update LED(s)
```

The entire pipeline is non-blocking and asynchronous. PIO hardware captures protocol signals via interrupts regardless of what the CPU is doing. The main loop processes data from the ring buffer whenever it's available. USB transmission happens through TinyUSB's internal state machine. Nothing ever waits or blocks.

This architecture allows the converter to handle burst input (rapid keypresses) without dropping data, and respond to LED updates without interfering with keystroke processing.

---

## Operating System Compatibility

Because the converter uses standard USB HID Boot Protocol, it works with effectively every operating system that supports USB keyboards. No drivers, no configuration files, no software installation—just plug it in and type.

### Tested Platforms

The converter has been verified to work correctly on:

- **Windows**: All versions from XP through 11
- **macOS**: All Intel and Apple Silicon versions
- **Linux**: All major distributions (Ubuntu, Debian, Fedora, Arch, etc.)
- **Chrome OS**: Chromebooks and Chromeboxes
- **FreeBSD and OpenBSD**: Full support through kernel HID subsystem
- **BIOS/UEFI**: Pre-boot environments for firmware configuration

### Untested But Should Work

These platforms use standard USB HID drivers and should work without issues:

- **Haiku OS**: Uses standard HID stack
- **DOS with USB drivers**: If your DOS system has USB keyboard support loaded
- **Game consoles**: PlayStation and Xbox consoles that accept USB keyboards
- **Embedded systems**: Any device with USB host capability and HID driver

The converter doesn't do anything platform-specific or proprietary. If a system can use a standard USB keyboard, it can use this converter.

---

## Troubleshooting

### Computer Doesn't Recognise the Converter

If your computer shows no response when you plug in the converter, check these possibilities:

**USB cable**: Some USB cables only have power wires and lack data connections. These "charge-only" cables won't work. Try a cable you know works for data (one you've used with another USB device successfully).

**USB port**: Some older USB hubs or ports have compatibility issues. Try connecting directly to a USB port on your computer rather than through a hub. Try different USB ports on your computer.

**Firmware**: If the RP2040's LED lights up but the computer still doesn't detect it, verify that you've flashed the converter firmware. A blank RP2040 won't enumerate as a USB HID device.

**Power**: Check that the RP2040's power LED illuminates when connected. No LED means no power, indicating a cable or port problem.

### Converter Recognised But Keys Don't Work

If the converter appears in your system's device list (Device Manager on Windows, System Information on macOS, `lsusb` on Linux) but typing produces no output, this indicates a wiring or configuration issue rather than a USB problem.

**Firmware match**: Confirm you flashed the correct firmware for your keyboard's protocol. Flashing AT/PS2 firmware when you have an XT keyboard won't work—the protocols are different.

**Wiring**: Double-check your CLOCK and DATA connections. These two wires are easy to accidentally reverse. Refer to your protocol's pinout documentation and verify which wire goes where.

**Level shifter**: Verify the level shifter has power on both sides (LV and HV). Check that its ground connections are solid. A level shifter without proper power won't translate signals correctly.

**Keyboard health**: If possible, test your keyboard on its original hardware to confirm it works. A faulty keyboard obviously won't produce correct output even with perfect wiring.

**Debug output**: Connect a USB-UART adapter to the converter's UART pins (GPIO 0 and 1) and watch the log output. You should see scancode messages when you press keys. If you see scancodes, the problem is in keymap translation. If you don't see scancodes, the problem is hardware (wiring/level shifter/keyboard).

### Some Keys Work Incorrectly

If most keys produce the correct output but specific keys are wrong or non-functional, this typically indicates a keymap problem rather than a hardware issue.

**Variant keyboards**: Some keyboard models that look identical have different internal controllers with different scancode mappings. Verify you have the exact right firmware variant for your specific keyboard.

**Protocol documentation**: Check the protocol documentation for your keyboard to see if it has any known quirks or special handling requirements. Some keyboards require special initialisation sequences or have non-standard key assignments.

**Scancode logs**: Connect via UART and watch what scancodes your keyboard actually sends when you press the problem keys. Compare these to the expected scancodes documented for your keyboard model.

### Electrical Noise Issues

If keys randomly repeat, stick, or produce spurious inputs, this usually indicates electrical noise on the signal lines.

**Wire length**: Keep CLOCK and DATA wires as short as practical. Longer wires act as antennas that pick up electromagnetic interference from other components.

**Capacitors**: Add 0.1μF ceramic capacitors between each signal line and ground, positioned close to the level shifter. These capacitors filter out high-frequency noise.

**Wire routing**: Don't run signal wires parallel to power wires. Keep them separated or cross at right angles to minimise coupling.

**Grounding**: Verify all ground connections are solid. Poor grounding is a common source of noise issues. The RP2040, level shifter, and keyboard must all share a common ground.

### LED Control Doesn't Work

If typing works correctly but pressing Caps Lock or Num Lock doesn't update the keyboard's LEDs, first check whether your keyboard's protocol supports host-controlled LEDs at all.

Many protocols don't support LED control. This is normal and doesn't indicate a problem. Refer to your protocol's documentation to confirm whether LED support exists.

If your protocol should support LEDs but they don't work, check the UART debug logs for error messages when you press lock keys. The converter will log any communication failures with the keyboard.

---

## Related Documentation

This page focused on the USB side of the converter. For a complete understanding, explore these related topics:

**Protocol implementations** explain how the converter receives data from specific keyboard types:
- [AT/PS2 Protocol](../protocols/at-ps2.md) - Most common protocol
- [XT Protocol](../protocols/xt.md) - Original IBM PC keyboards
- [Amiga Protocol](../protocols/amiga.md) - Commodore Amiga keyboards
- [M0110 Protocol](../protocols/m0110.md) - Early Macintosh keyboards

**Other features** extend the converter's functionality:
- [Command Mode](command-mode.md) - Access bootloader and runtime configuration via keyboard
- [Mouse Support](mouse-support.md) - Detailed explanation of simultaneous mouse conversion
- [LED Support](led-support.md) - Status indicators and RGB lighting options
- [Configuration Storage](config-storage.md) - How settings persist across reboots
- [Logging](logging.md) - UART debug output for troubleshooting

**Implementation details** for developers interested in the code:
- [`hid_interface.c`](../../src/common/lib/hid_interface.c) - HID report generation and interface management
- [`usb_descriptors.c`](../../src/common/lib/usb_descriptors.c) - USB device descriptors and configuration
- [TinyUSB Documentation](https://docs.tinyusb.org/) - The USB stack library used by the converter

---

**Questions or stuck on something?**  
Pop into [GitHub Discussions](https://github.com/PaulW/rp2040-keyboard-converter/discussions) or [report a bug](https://github.com/PaulW/rp2040-keyboard-converter/issues) if you've found an issue.
