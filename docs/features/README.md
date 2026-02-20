# Features Documentation

The converter includes several features that make it practical for daily use whilst keeping the architecture clean and non-blocking. This page gives you an overview of what's available—each feature has its own detailed guide linked below if you want to dive deeper.

## Core Features

### USB HID Interface

**[USB HID Guide](usb-hid.md)**

The converter uses standard USB HID Boot Protocol, which means it works everywhere without drivers—BIOS screens, UEFI setup, Windows, macOS, Linux, BSD, and other operating systems. The Boot Protocol is intentionally simple and universally supported, which is exactly what you want when converting keyboards.

The interface reports up to 6 simultaneous regular keys plus all 8 modifiers (both Shift, Ctrl, Alt, and GUI keys). This is called 6-Key Rollover (6KRO), and whilst modern gaming keyboards often tout N-Key Rollover, it's not actually relevant for these keyboards. Most keyboards have hardware limitations—their key matrices can only detect 2-3 simultaneous keys before ghosting occurs anyway. The 6KRO limit never comes up in practice.

The converter provides low-latency keyboard conversion with minimal processing overhead. The non-blocking architecture ensures responsive keystroke handling without dropped inputs.

The converter can handle simultaneous keyboard and mouse conversion if you have an AT/PS2 mouse to connect. LED indicators (Caps Lock, Num Lock, Scroll Lock) synchronise properly with the host OS, so the keyboard's lock LEDs update when you press the lock keys—assuming your keyboard protocol supports LED control.

**See:** [USB HID Guide](usb-hid.md) for complete technical details about report formats, timing, and USB polling

---

### Command Mode

**[Command Mode Guide](command-mode.md)**

Command Mode provides access to special functions without requiring firmware reflashing or device disconnection. This allows you to adjust settings and perform system operations during normal use.

**Activation:**
- Hold both shift keys for 3 seconds
- LED begins alternating between green and blue
- Mode auto-exits after 3 seconds of inactivity or immediately after executing a command

**Available Commands:**
- 🔄 **'B'**: Enter bootloader (BOOTSEL mode for firmware updates)
- 🐛 **'D'**: Change log level (then press 1/2/3 for ERROR/INFO/DEBUG)
- ⚙️ **'F'**: Factory reset (restore default configuration and reboot)
- 💡 **'L'**: LED brightness (then press +/- to adjust, 0=off to 10=max)

**LED Feedback:**
- Alternating green/blue - Command Mode active
- Solid green - Normal operation resumed

Command Mode provides a handy method for adjusting settings during testing or troubleshooting without interrupting workflow.

**See:** [Command Mode Guide](command-mode.md) for complete activation instructions and command reference

---

### Configuration Storage

**[Configuration Storage Guide](config-storage.md)**

Configuration settings persist across reboots using the flash storage system. LED brightness, log level, and protocol settings are saved to the last 4KB sector of internal flash memory, eliminating the need for reconfiguration after power cycling.

The implementation uses a dual-copy redundancy system for data integrity. Two copies (A and B) are stored, each with CRC validation to detect corruption. Writes alternate between the copies for wear leveling, which extends flash life to about 10,000 write cycles per location before degradation. That's plenty for configuration updates that happen occasionally rather than continuously.

Currently stored settings include your log level (ERROR, INFO, or DEBUG), LED brightness level (0-10 scale), protocol initialisation parameters, and timing thresholds. The storage system's extensible too, so future features like custom key mappings, macros, or debounce timing adjustments can slot right in without architectural changes.

**See:** [Configuration Storage Guide](config-storage.md) for technical details about flash allocation and redundancy

---

### LED Indicators

**[LED Indicators Guide](led-indicators.md)**

LED indicators provide visual feedback about converter status, including operational state, Command Mode activation, and error conditions. Every converter requires at least one status LED, with optional WS2812 RGB LEDs available for enhanced visibility.

**Converter Status Indicators:**
- Status LED (RGB): Shows operational state and command mode
- Power indication
- Error states and protocol issues

The status LED shows different colours depending on what's happening. Solid green means everything's working normally. Solid orange appears briefly during startup whilst firmware initialises. Alternating green and blue indicates Command Mode is active and waiting for you to press a command key. Solid magenta means bootloader mode (firmware flash mode). These patterns make it easy to tell what's going on without needing to connect debug tools.

**Keyboard Lock Indicators:**

For keyboard lock indicators (Caps Lock, Num Lock, Scroll Lock), the converter handles synchronisation automatically. When a lock key is pressed, the host OS sends LED states back through the HID protocol. The converter forwards these states to the keyboard using the appropriate LED command format for the protocol—AT/PS2 keyboards receive the 0xED command, Amiga keyboards use handshake timing variations, whilst XT and M0110 keyboards don't support LED control (unidirectional protocols).

**Optional WS2812 RGB LED:**
- Status indicator (Command Mode)
- Configurable brightness

WS2812 RGB LEDs (NeoPixels) can be added for enhanced visual feedback. These connect to GPIO 29 by default and support configurable brightness through Command Mode. WS2812 support is optional and disabled by default to conserve power.

**See:** [LED Indicators Guide](led-indicators.md) for wiring diagrams, colour patterns, and technical details

---

### Mouse Support

**[Mouse Support Guide](mouse-support.md)**

Mouse support allows connection of a mouse alongside the keyboard, providing complete period-accurate input hardware. The mouse operates independently from the keyboard using separate GPIO pins and a separate PIO state machine, preventing any signal interference.

AT/PS2 protocol mice are currently supported, covering mid-1980s through early 2000s devices that follow the standard packet formats. The converter handles standard 3-byte packets (buttons, X movement, Y movement) and extended 4-byte packets for mice with scroll wheels (IntelliMouse protocol). Scroll wheel detection occurs automatically during initialisation.

Mouse support is protocol-agnostic and works with any keyboard protocol. An AT/PS2 mouse can be paired with XT, Amiga, or M0110 keyboards because the mouse uses dedicated hardware lines (CLOCK and DATA separate from keyboard lines), allowing parallel operation without coordination. Press keys whilst moving the mouse, scroll whilst holding modifier keys, click whilst typing—everything works as expected because the signals never cross paths.

To enable mouse support, add the MOUSE environment variable when building firmware:

**Example:**
```bash
# Build with keyboard + mouse
docker compose run --rm -e KEYBOARD="modelm/enhanced" -e MOUSE="at-ps2" builder
```

**Hardware Requirements:**
- Separate CLOCK and DATA lines for mouse (independent from keyboard)
- Level shifters required (5V → 3.3V)
- GPIO pins configured in [`config.h`](../../src/config.h)

**See:** [Mouse Support Guide](mouse-support.md) for complete wiring instructions and protocol details

---

### Keyboard Layers

**[Keyboard Layers Guide](layers.md)**

Keyboard layers provide a way to access multiple sets of key functions from the same physical keys. Think of them as transparent overlays—activate a layer and the keys do different things. This is particularly useful for compact keyboards or when adding modern conveniences like media controls to vintage hardware that lacks dedicated keys.

The converter supports up to 8 layers (numbered 0-7). Layer 0 is always active as the base layer, and upper layers overlay on top. Keys can be transparent in upper layers, falling through to lower active layers until a keycode is found, so you only need to define the keys that actually change.

**Layer activation methods:**
- **Momentary (MO)**: Active whilst held, like a traditional Fn key
- **Toggle (TG)**: Stays on until toggled off again (persists across reboots)
- **Switch To (TO)**: Permanently switches to that layer (persists across reboots)
- **One-Shot (OSL)**: Activates for exactly one keypress

Practical use cases include adding media controls (volume, playback) to keyboards that lack dedicated keys, accessing function keys F13-F24 on compact keyboards, providing numpad navigation for macOS (which doesn't use NumLock), or switching between QWERTY and alternative layouts like Dvorak or Colemak.

The layer system is entirely optional. If your keyboard has all the keys you need, and you're not planning to remap anything, a single-layer keymap is perfectly adequate. Layers add flexibility without requiring hardware modifications.

**See:** [Keyboard Layers Guide](layers.md) for concepts and practical examples, or [Custom Keymaps Guide](../development/custom-keymaps.md) for implementation details

---

### Logging

**[Logging Guide](logging.md)**

The converter provides detailed UART-based logging to help you understand what it's doing and diagnose problems. This is particularly useful when you're adding support for a new keyboard or troubleshooting protocol issues—you can watch the raw scancodes come in, see how they're processed, and spot where things go wrong.

The logging implementation uses DMA-driven UART, which means log messages transmit in the background without blocking keyboard processing. Even at DEBUG log level (which outputs every scancode), logging never adds latency to your keystrokes. The UART operates completely independently from USB, so debug output keeps working even if the USB connection has problems—exactly when you need it most.

You can adjust the log level through Command Mode without reflashing firmware. Press D in Command Mode, then 1 for ERROR (just critical errors), 2 for INFO (initialisation and state changes), or 3 for DEBUG (everything including individual scancodes). The setting persists across reboots, so you only need to set it once.

Log output looks like this:

```
[INFO] Keyboard interface initialised (AT/PS2)
[DBG] Scancode: 0x1C (Make)
[ERR] Parity error detected
[INFO] Command mode activated
```

To view logs, connect a USB-to-UART adapter to GPIO 0 (TX) and GPIO 1 (RX, optional) with a shared ground connection. Set your terminal program to 115200 baud, 8 data bits, no parity, 1 stop bit. Any standard terminal program works—screen, minicom, PuTTY, or CoolTerm.

**See:** [Logging Guide](logging.md) for complete wiring instructions, terminal setup, and log format reference

---

## Architectural Principles

All these features follow the same architectural principles that keep the converter responsive and reliable.

### Non-Blocking Architecture

Nothing in the converter ever blocks. No `sleep_ms()`, no `busy_wait_us()`, no polling loops that halt execution. This is critical for maintaining the microsecond-level timing that keyboard protocols require. Even a brief blocking operation can cause the ring buffer to overflow and drop scancodes.

Instead, everything uses non-blocking state machines with timeouts:

```c
// ❌ BAD: Blocking
sleep_ms(100);

// ✅ GOOD: State machine with timeout
static uint32_t state_start = 0;
if (to_ms_since_boot(get_absolute_time()) - state_start > 100) {
  // Timeout reached, proceed
}
```

This pattern appears throughout the codebase—protocol handlers, Command Mode, LED updates, configuration storage. The main loop runs continuously without blocking, checking state machines and processing events as they occur. It's a bit more code than just calling `sleep()`, but it's what keeps the converter working reliably under all conditions.

**See:** [Architecture Guide](../advanced/architecture.md) for complete details about the non-blocking design

---

### Performance Optimisation

The converter runs on modest hardware—an RP2040 microcontroller with 264KB SRAM and a 125MHz ARM Cortex-M0+ core. Resource constraints matter here, so the implementation is designed for efficiency.

The 32-byte ring buffer provides adequate burst handling—it can queue up several scancodes before the main loop processes them. The non-blocking architecture ensures the converter remains responsive even under continuous input.

Memory usage is about 132KB of the available 264KB SRAM, which leaves plenty of headroom for future features. Flash usage varies by configuration (which protocols and features you enable), but typically sits around 80-120KB out of 2MB available.

**See:** [Advanced Topics](../advanced/README.md) for detailed performance analysis and measurements

---

## Related Documentation

If you want to dive deeper into how these features work or understand the implementation details, have a look at these guides:

**Getting Started:**
- [Getting Started Guide](../getting-started/README.md) - Initial setup and first firmware build
- [Hardware Setup](../getting-started/hardware-setup.md) - Wiring diagrams and connections

**Technical Details:**
- [Protocols Overview](../protocols/README.md) - How keyboard protocols work
- [Advanced Topics](../advanced/README.md) - System architecture and troubleshooting
- [Development Guide](../development/README.md) - Contributing and code standards

**Source Code References:**
- Command Mode: [`src/common/lib/command_mode.[ch]`](../../src/common/lib/command_mode.c)
- HID Interface: [`src/common/lib/hid_interface.[ch]`](../../src/common/lib/hid_interface.c)
- Config Storage: [`src/common/lib/config_storage.[ch]`](../../src/common/lib/config_storage.c)