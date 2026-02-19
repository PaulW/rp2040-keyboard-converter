# Mouse Support

The converter supports connecting a mouse alongside your keyboard, giving you complete period-accurate input hardware on modern computers. This page explains how to physically connect mouse hardware and enable mouse support in your firmware build.

---

## Overview

Mouse support operates completely independently of your keyboard protocol selection. You can use a compatible mouse with **any** supported keyboard—AT/PS2, XT, Amiga, or M0110. The mouse uses its own dedicated GPIO pins and PIO state machine, so it works in parallel with the keyboard without interference.

**Currently Supported**: AT/PS2 protocol mice (the most common mouse protocol from the mid-1980s through early 2000s)

**Not Currently Supported**: Serial mice (Microsoft/Mouse Systems protocol), bus mice, Apple ADB mice, or USB-only mice

For detailed information about the AT/PS2 mouse protocol, packet formats, and command sequences, see the [AT/PS2 Protocol Documentation](../protocols/at-ps2.md#mouse-implementation).

---

## Hardware Requirements

### What You Need

To add mouse support, you need a compatible mouse, a way to connect it to the RP2040, and pull-up resistors for the signal lines.

Currently, only AT/PS2 protocol mice are supported. This includes mice with PS/2 connectors, AT connectors, and USB mice that support passive PS/2 adapters. Mice with scroll wheels are automatically detected and work without special configuration.

For the physical connection, you'll need either a breakout board with the appropriate mouse connector, or you can wire directly to a mouse cable. Breakout boards are the easiest approach—they provide a clean connector on one side and header pins for breadboard or direct wiring on the other.

Pull-up resistors are required on the CLOCK and DATA signal lines. The typical value is 4.7kΩ connected between each signal and 3.3V. Many breakout boards include these resistors already, so check your board's specifications before adding external resistors. If your board already has pull-ups, adding more external ones is harmless.

For protocol-specific details including connector types, pinout diagrams, wire colors, and electrical requirements, see the [AT/PS2 Protocol Documentation](../protocols/at-ps2.md#physical-interface).

---

## Wiring

Mouse connections are completely separate from keyboard connections. The mouse uses its own dedicated GPIO pins for CLOCK and DATA, plus shared power and ground. This means you can add a mouse even if you already have a keyboard connected—they operate independently.

The mouse data pin is configured in [`config.h`](../../src/config.h) through `MOUSE_DATA_PIN`. The clock pin is automatically calculated as `MOUSE_DATA_PIN + 1`, so you only need to configure the data pin. These pins must be consecutive due to how the PIO hardware handles the protocol timing. See the [Configuration](#configuration) section for instructions on changing GPIO assignments.

Power comes from the VSYS pin (+5V from USB), and ground connects to any GND pin on the RP2040. The total USB power budget is 500mA shared between keyboard, mouse, LEDs, and the RP2040. If you experience power issues, try reducing LED brightness first.

For complete wiring diagrams, connector pinouts, and protocol-specific electrical requirements, see the [AT/PS2 Protocol Documentation](../protocols/at-ps2.md#physical-interface).

---

## Building with Mouse Support

Mouse support is optional and controlled at build time via environment variable.

### Build Commands

```bash
# Keyboard only (no mouse)
docker compose run --rm -e KEYBOARD="modelm/enhanced" builder

# Mouse only (no keyboard)
docker compose run --rm -e MOUSE="at-ps2" builder

# Both keyboard and mouse
docker compose run --rm -e KEYBOARD="modelm/enhanced" -e MOUSE="at-ps2" builder
```

**Note**: The `MOUSE` variable currently only accepts `at-ps2`. If you don't specify `MOUSE`, mouse support is excluded to save flash space (~3-4KB).

See [Building Firmware](../getting-started/building-firmware.md) for complete build instructions.

---

## Architecture

### How It Works

The mouse operates completely independently of the keyboard using dedicated hardware resources.

The RP2040's PIO (Programmable I/O) hardware handles the precise protocol timing (see [`interface.pio`](../../src/protocols/at-ps2/interface.pio)). The converter uses one PIO state machine for the keyboard and a separate state machine for the mouse. These run in parallel without coordination—each monitors its own GPIO pins, handles its own protocol timing, and operates independently.

When mouse data arrives, the PIO hardware triggers an interrupt. The interrupt handler (see [`mouse_interface.c`](../../src/protocols/at-ps2/mouse_interface.c)) processes incoming data, assembling complete packets before sending reports directly to the USB HID interface. This direct processing architecture prevents any cross-contamination with keyboard data.

Mouse initialisation happens separately from keyboard initialisation on dedicated GPIO pins, so adding mouse support doesn't affect keyboard operation. Both keyboard and mouse reports travel over the same USB connection, with the host operating system automatically distinguishing report types based on format.

The practical result: you can type and move the mouse simultaneously without coordination or timing concerns. Press keys while moving the cursor, scroll while holding modifier keys, click while typing—everything works as expected.

For protocol-specific technical details about packet formats, initialisation sequences, and timing specifications, see [AT/PS2 Protocol Documentation](../protocols/at-ps2.md#mouse-implementation).

---

## Troubleshooting

### Mouse Not Detected or Initialisation Fails

If your mouse isn't working at all, start by verifying the wiring. Check that GPIO pin connections match the definitions in [`config.h`](../../src/config.h), with DATA connecting to `MOUSE_DATA_PIN` and CLOCK to `MOUSE_DATA_PIN + 1`. Verify power and ground are properly connected.

Confirm pull-up resistors are present on both CLOCK and DATA lines. If you're using a breakout board, check whether it has pull-ups built in. If wiring directly to a connector, ensure external pull-up resistors are installed.

Try testing with a different mouse to isolate hardware-specific initialisation issues. Some mice have quirks that prevent standard initialisation.

Connect debug UART (see [Logging documentation](logging.md)) to watch the initialisation sequence during power-on. Error messages will indicate where initialisation fails.

If you're running many bright LEDs with both keyboard and mouse, you might be approaching the USB 500mA power limit. Try reducing LED brightness or temporarily disconnecting LEDs to test whether power is the issue.

### Mouse Movement Problems

Erratic or jittery cursor movement is usually a mechanical issue with the mouse itself—dirty rollers on ball mice or faulty sensors on optical mice. Clean the mouse hardware and test on different surfaces.

Electrical noise can cause erratic behavior. Keep wiring short (under 30cm) and avoid routing mouse wires parallel to USB cables or power supplies.

If cursor axes are reversed, adjust the axis direction in your operating system's mouse settings—this is an OS configuration issue, not a converter problem.

Complete lack of movement despite successful initialisation usually indicates incorrect GPIO wiring. Verify physical wiring matches the pin numbers in [`config.h`](../../src/config.h).

### Scroll Wheel Not Working

Connect debug UART during power-on to see the detected mouse type. If scroll wheel isn't detected during initialisation, the mouse may not support the required protocol extensions.

Try a different mouse to rule out mouse-specific protocol compatibility issues.

Verify scroll functionality works with a native USB mouse on your computer to rule out OS or application issues.

### Both Keyboard and Mouse Not Working

If both keyboard and mouse fail together, you've likely exceeded the USB power budget. Disconnect the mouse and test keyboard alone. If keyboard works by itself, reduce LED brightness or use external power.

Check for GPIO pin conflicts. The mouse uses `MOUSE_DATA_PIN` and `MOUSE_DATA_PIN + 1`—ensure these don't overlap with keyboard pins, UART, LEDs, or other hardware. Review all pin assignments in [`config.h`](../../src/config.h).

Verify you built firmware with `MOUSE=at-ps2` environment variable set. See [Building Firmware](../getting-started/building-firmware.md) for correct build commands.

Check for wiring shorts between signal lines or between signals and power rails using a multimeter in continuity mode.

---

## Configuration

### Customising GPIO Pins

Mouse GPIO pins are defined in [`config.h`](../../src/config.h):

```c
// Mouse protocol GPIO pin (CLOCK is automatically DATA + 1)
#define MOUSE_DATA_PIN 6   // Set to your desired GPIO pin for mouse data
```

**To use different pins**:
1. Edit `MOUSE_DATA_PIN` in config.h to your desired GPIO number
2. The clock pin automatically becomes DATA + 1
3. Rebuild firmware with your changes

**Requirements when selecting pins**:
- Avoid conflicts with keyboard pins, LEDs, or UART TX
- CLOCK and DATA must be consecutive (CLOCK = DATA + 1)
- Must be routable to an available PIO block
- Consider physical board layout for easier wiring

**Example configurations**:
```c
#define MOUSE_DATA_PIN 6    // Mouse DATA on GPIO 6, CLOCK on GPIO 7
#define MOUSE_DATA_PIN 10   // Mouse DATA on GPIO 10, CLOCK on GPIO 11
#define MOUSE_DATA_PIN 14   // Mouse DATA on GPIO 14, CLOCK on GPIO 15
```

See [`config.h`](../../src/config.h) for complete hardware configuration options and other pin assignments to avoid conflicts.

---

## Related Documentation

- [AT/PS2 Protocol](../protocols/at-ps2.md) - Mouse protocol specification and timing
- [USB HID Guide](usb-hid.md) - How mouse reports work
- [Hardware Setup](../getting-started/hardware-setup.md) - Wiring and connections
- [Building Firmware](../getting-started/building-firmware.md) - Enabling mouse support in build

---

**Questions or stuck on something?**  
Pop into [GitHub Discussions](https://github.com/PaulW/rp2040-keyboard-converter/discussions) or [report a bug](https://github.com/PaulW/rp2040-keyboard-converter/issues) if you've found an issue.
