# Debug Logging

The converter provides detailed UART-based logging to help you understand what it's doing, diagnose problems, and verify correct operation. This page explains how to connect to the converter's debug output, what information the logs contain, and how to adjust logging verbosity.

---

## Why UART Logging?

USB is the converter's primary communication channel for sending keystrokes and mouse movements to your computer. Using USB for debug output would interfere with normal operation and complicate the firmware.

Instead, the converter uses UART (Universal Asynchronous Receiver/Transmitter), a simple serial protocol that sends text data over GPIO pins. You connect these pins to a USB-to-UART adapter (sometimes called an FTDI adapter or USB serial adapter), which lets you view the debug output on your computer using a terminal program.

UART is ideal for debugging embedded systems because it operates completely separately from the USB connection—debug output never interferes with keyboard or mouse operation. The hardware requirements are minimal: just two wires connecting TX and GND. The output format is human-readable standard text, viewable in any terminal program without special software.

The converter's logging implementation uses DMA (Direct Memory Access), which means log messages transmit in the background without blocking keyboard processing. Even at the highest debug verbosity, logging never adds latency to your keystrokes. And because UART is independent of USB, it keeps working even if the USB connection fails or has problems—exactly when you need debug output most.

---

## Hardware Setup

To receive debug logs, you need a USB-to-UART adapter and two wire connections.

### Required Hardware

You need a USB-to-UART adapter—a small device that converts UART signals to USB. Common types include FTDI-based adapters (using FT232RL or FT234X chips), Silicon Labs CP2102/CP2104 adapters, Prolific PL2303 adapters, and CH340/CH341 adapters. These are widely available from electronics suppliers for $5-15. Most come as small breakout boards with pins for TX, RX, GND, and power.

The key requirement is 3.3V logic level support. Many adapters have a jumper or switch to select between 3.3V and 5V voltage levels. Make sure yours is set to 3.3V to match the RP2040's GPIO voltage. Applying 5V to the RP2040's GPIO pins can damage the chip.

### Wiring

You only need two connections:

| RP2040 Pin | USB-UART Adapter Pin | Purpose |
|------------|----------------------|---------|
| **GPIO 0** (UART TX) | **RX** | Transmit from RP2040 to adapter |
| **GND** | **GND** | Ground reference |

**Important**: Connect the RP2040's TX to the adapter's RX. TX (transmit) sends data, RX (receive) accepts data. The RP2040 transmits log messages, the adapter receives them.

You don't need to connect the adapter's TX pin unless you're sending commands to the converter (which this firmware doesn't currently support).

The adapter's power pins (3.3V, 5V) are not needed—the RP2040 is already powered via USB. Only connect TX→RX and GND.

**Visual Reference**:
```
RP2040 Board               USB-UART Adapter
┌──────────────┐          ┌─────────────────┐
│              │          │                 │
│  GPIO 0 ─────┼──────────┤ RX              │
│  (UART TX)   │          │                 │
│              │          │                 │
│  GND ────────┼──────────┤ GND             │
│              │          │                 │
└──────────────┘          └─────────────────┘
                                 │
                                USB to Computer
```

### Physical Connection Tips

**Keep wires short** - UART is tolerant of wire length, but shorter is better. 30cm or less is ideal to minimize noise.

**Avoid parallel routing** - Don't bundle UART wires with high-speed signals (like USB data lines) to reduce electromagnetic interference.

**Solid connections** - Use proper connectors or solder joints rather than loose jumper wires. Poor connections cause intermittent data corruption.

**Verify voltage** - Double-check your adapter is set to 3.3V before connecting. 5V on the RP2040's 3.3V GPIO pins can damage the chip.

---

## Software Setup

Once you've wired the adapter, you need terminal software to view the log output.

### Serial Terminal Programs

**macOS:**
- **screen** (built-in): `screen /dev/tty.usbserial-* 115200`
- **minicom**: `brew install minicom`, then `minicom -D /dev/tty.usbserial-* -b 115200`
- **CoolTerm** (GUI): Free download, user-friendly interface

**Linux:**
- **screen**: `screen /dev/ttyUSB0 115200`
- **minicom**: `minicom -D /dev/ttyUSB0 -b 115200`
- **picocom**: `picocom -b 115200 /dev/ttyUSB0`

**Windows:**
- **PuTTY**: Free, supports serial connections (select "Serial" and enter COM port)
- **TeraTerm**: Free, simple serial terminal
- **RealTerm**: Advanced features for debugging

### Connection Settings

All terminal programs need these settings to match the converter's UART configuration:

| Parameter | Value |
|-----------|-------|
| **Baud Rate** | 115200 |
| **Data Bits** | 8 |
| **Parity** | None |
| **Stop Bits** | 1 |
| **Flow Control** | None |

These are the default settings for most serial terminals, but verify them if you're not seeing output.

### Finding the Serial Port

When you plug in your USB-UART adapter, the operating system creates a serial port device:

**macOS**: `/dev/tty.usbserial-XXXXXXXX` or `/dev/tty.SLAB_USBtoUART` (the X's vary by adapter)  
**Linux**: `/dev/ttyUSB0`, `/dev/ttyUSB1`, etc. (number increments for each adapter)  
**Windows**: `COM3`, `COM4`, etc. (check Device Manager under "Ports (COM & LPT)")

To find the device name:
- **macOS/Linux**: Run `ls /dev/tty.*` or `ls /dev/ttyUSB*` before and after plugging in the adapter to see which device appears
- **Windows**: Open Device Manager, expand "Ports (COM & LPT)", and look for "USB Serial Port (COMX)"

### Example: Using screen on macOS

```bash
# Find the device
ls /dev/tty.usbserial-*

# Connect (replace with your actual device name)
screen /dev/tty.usbserial-A50285BI 115200

# To exit screen later: Press Ctrl+A, then K, then Y
```

Once connected, you should immediately see log output scrolling by (assuming the converter is powered and running).

---

## Understanding Log Output

The converter logs initialization steps, protocol events, errors, and debug information (see [`log.h`](../../src/common/lib/log.h) for implementation). Understanding the log format helps you diagnose issues quickly.

### Log Format

Each log message follows a consistent format:

```
[LEVEL] Message text here
```

**LEVEL** indicates message importance:
- `[INFO]` - Informational messages about normal operation
- `[DBG]` - Debug details useful for development and troubleshooting
- `[ERR]` - Error conditions that prevent proper operation

Examples:
```
[INFO] Build Time: Oct 28 2025 14:23:17
[INFO] Keyboard Make: IBM
[INFO] Keyboard Protocol: at-ps2
[DBG] Processing scancode: 0x1C
[ERR] Start Bit Validation Failed: start_bit=1
```

### Startup Sequence

When the converter powers on, you'll see initialization messages:

```
--------------------------------
[INFO] RP2040 Device Converter
[INFO] RP2040 Serial ID: E6605889DB6B8D2E
[INFO] Build Time: Oct 28 2025 14:23:17
--------------------------------
[INFO] Keyboard Support Enabled
[INFO] Keyboard Make: IBM
[INFO] Keyboard Model: Model M Enhanced PC Keyboard
[INFO] Keyboard Description: IBM Personal Computer AT Enhanced Keyboard
[INFO] Keyboard Protocol: at-ps2
[INFO] Keyboard Scancode Set: set123
--------------------------------
```

This startup sequence confirms:
1. Device serial ID (unique to your RP2040)
2. Firmware build timestamp
3. Keyboard support status
4. Keyboard make, model, and description from configuration
5. Active protocol and scancode set

If something goes wrong during initialization, you'll see `[ERR]` messages explaining the failure.

### Normal Operation

During normal use, logs show key events at various verbosity levels:

**Log Level 0 (Errors Only)** - Only shows problems:
```
[ERR] Start Bit Validation Failed: start_bit=1
[ERR] Parity Bit Validation Failed: expected=1, actual=0
[ERR] Keyboard Self-Test Failed: 0xAA
```

**Log Level 1 (Info)** - Shows significant events without overwhelming detail:
```
[INFO] Command mode active! Press 'B'=bootloader, 'D'=log level, 'F'=factory reset, 'L'=LED brightness, or wait 3s to cancel
[INFO] Log level set to INFO (1)
[INFO] Factory reset complete - rebooting device...
```

**Log Level 2 (Debug)** - Shows detailed protocol activity:
```
[DBG] Processing scancode: 0x1C
[DBG] Invalid M0110 scancode (bit 0 not set): 0x7F
[DBG] Log level set to DEBUG (2)
```

Debug level is most useful when troubleshooting specific key presses or protocol issues. It generates significant output, so use it only when needed.

### Error Messages

When things go wrong, error messages indicate what failed:

**Protocol Errors:**
```
[ERR] Start Bit Validation Failed: start_bit=1
```
The protocol expects a start bit of 0, but received 1. This indicates timing issues or electrical noise on the data line.

```
[ERR] Parity Bit Validation Failed: expected=1, actual=0
```
AT/PS2 protocol includes a parity bit for error detection. This means electrical noise corrupted the data or there's a timing problem. Check wiring and pull-up resistors.

```
[ERR] Keyboard Self-Test Failed: 0xAA
```
The keyboard failed its power-on self-test. Expected 0xAA (success), but received a different value. Keyboard may be faulty or incompatible with the selected protocol.

**Amiga Protocol Errors:**
```
[ERR] Amiga: Keyboard self-test FAILED (0xFC)
```
Amiga keyboards send 0xFD/0xFE on successful self-test. Receiving 0xFC indicates a hardware fault.

---

## Log Levels

The converter supports three log levels that control how much information appears in the output.

### Log Level 0: Errors Only

**What you see**: Only `[ERR]` and `[WARN]` messages indicating problems.

**When to use**: Normal daily use. You don't need to see every keystroke or initialization step—you only want to know if something breaks.

**Impact on performance**: Minimal. Error logging is extremely lightweight since errors are rare.

**Example output**:
```
[ERR] Start Bit Validation Failed: start_bit=1
[ERR] Parity Bit Validation Failed: expected=1, actual=0
[WARN] Factory reset requested - restoring default configuration
```

### Log Level 1: Info (Default)

**What you see**: `[ERR]` and `[WARN]` messages plus `[INFO]` messages about significant events (initialization, Command Mode activation, settings changes).

**When to use**: Default setting. Provides enough visibility to understand what the converter is doing without overwhelming detail.

**Impact on performance**: Negligible. Info messages are infrequent (mostly at startup or during configuration changes).

**Example output**:
```
[INFO] RP2040 Device Converter
[INFO] Build Time: Oct 28 2025 14:23:17
[INFO] Keyboard Make: IBM
[INFO] Keyboard Protocol: at-ps2
[ERR] Start Bit Validation Failed: start_bit=1
```

### Log Level 2: Debug

**What you see**: Everything—`[ERR]`, `[WARN]`, `[INFO]`, and `[DBG]` messages including every scancode, key press, USB report, and protocol detail.

**When to use**: Troubleshooting specific issues, verifying protocol operation, understanding timing problems, or developing new features.

**Impact on performance**: Moderate. Debug logging generates significant output (potentially dozens of messages per second during active typing). The DMA-driven UART ensures this doesn't block keyboard processing, but serial terminals can struggle to keep up with the output rate.

**Example output**:
```
[DBG] !E0! (0x12)
[DBG] !F0! (0x1C)
[DBG] Keyboard Self Test OK!
[DBG] Waiting for Keyboard ID...
[DBG] ACK Keyboard ID Request
```

The `!PREFIX!` markers indicate scancode sequence states (E0/F0/E1 prefixes), helping troubleshoot multi-byte sequences.

### Changing Log Level

You can adjust the log level through Command Mode (see [`command_mode.c`](../../src/common/lib/command_mode.c)):

1. Hold both shift keys for 3 seconds (or your keyboard's configured Command Mode keys)
2. Press 'D' (Debug)
3. Press '1' (Errors), '2' (Info), or '3' (Debug) to select log level
4. The converter immediately switches to the new level and saves it to flash

The new log level persists across reboots, so you only need to set it once.

**Technical Note**: The key numbers map to log levels as: Press '1' → ERROR (level 0), Press '2' → INFO (level 1), Press '3' → DEBUG (level 2). The numeric value shown in log messages (e.g., "Log level set to INFO (1)") indicates the internal level constant, not the key pressed.

---

## DMA-Based Logging

Understanding how logging works internally explains why it doesn't impact keyboard performance.

### Why DMA?

Traditional UART transmission involves the CPU writing each character to a hardware register, waiting for transmission to complete, then writing the next character. This "blocking" approach would freeze the converter during log output—disastrous for time-sensitive keyboard protocol handling.

DMA (Direct Memory Access) solves this, implemented in [`uart.c`](../../src/common/lib/uart.c). When the firmware wants to log a message:

1. The message string is prepared in a RAM buffer
2. The DMA controller is told "transmit this buffer over UART"
3. The DMA controller handles transmission independently, byte by byte
4. The CPU immediately continues processing keyboard data

Result: Logging appears instantaneous from the CPU's perspective. No blocking, no delays, no impact on keyboard timing.

### Ring Buffer Architecture

To handle multiple concurrent log messages (e.g., protocol events happening while initialization logs are still transmitting), logging uses a ring buffer:

**Main code calls LOG_INFO()** → Message formatted and written to ring buffer → DMA reads from ring buffer → Bytes transmitted over UART

The ring buffer is 16,384 bytes (16KB: 64 message slots × 256 bytes per message), implemented in [`uart.c`](../../src/common/lib/uart.c). If log messages accumulate faster than UART can transmit them (rare, only at debug level during intense activity), new messages overwrite the oldest unread messages. This prevents memory exhaustion while ensuring recent messages are preserved.

### Performance Impact

Logging overhead is designed to be negligible:
- **Error/Info levels**: Minimal CPU usage (messages are infrequent - typically only at startup or during error conditions)
- **Debug level**: Moderate CPU usage during active typing (generates many messages per keystroke)

These characteristics are achieved through the non-blocking DMA design. UART transmission rate at 115200 baud limits throughput to approximately 11,520 bytes per second. Debug logging during fast typing can generate 5000-8000 bytes per second, well within UART capacity.

The non-blocking DMA design ensures that even at debug level, keyboard latency remains unaffected by logging activity.

---

## Troubleshooting Logging

### No Output on Serial Terminal

If you're not seeing any log output:

**Verify wiring** - Check that RP2040 TX connects to adapter RX, and GND connects to GND.

**Check voltage** - Ensure your USB-UART adapter is set to 3.3V, not 5V.

**Confirm port** - Make sure your terminal program is connected to the correct serial port device.

**Check baud rate** - Verify 115200 baud in your terminal settings.

**Test the adapter** - Connect the adapter's TX to its RX (loopback test). Type in the terminal—you should see characters echoed back. This confirms the adapter works.

**Power cycle** - Disconnect and reconnect the converter's USB power. You should see startup messages immediately.

### Garbled or Corrupted Output

If you see random characters or missing text:

**Baud rate mismatch** - Double-check that your terminal is set to 115200 baud.

**Poor connection** - Wiggle the wires. If output improves temporarily, you have a loose connection. Re-seat or solder connections.

**Electrical noise** - Long wires or proximity to noisy signals (USB data lines, switching power supplies) can corrupt UART data. Shorten wires, add shielding, or route away from noise sources.

**Wrong voltage** - If your adapter is set to 5V but the RP2040 outputs 3.3V, the voltage difference can cause signal integrity issues. Set adapter to 3.3V.

**Ground loop** - Ensure only one ground connection between RP2040 and adapter. Multiple ground paths can create current loops that induce noise.

### Missing Messages

If some log messages don't appear:

**Ring buffer overflow** - At debug level during intense activity, messages can accumulate faster than UART transmits them. The ring buffer wraps, discarding oldest messages. This is intentional to prevent blocking. Solutions:
  - Reduce log level to 1 (Info) to reduce message volume
  - Increase UART baud rate in firmware (requires code change and rebuild)
  - Slow down your typing during debug observation

**Terminal buffer limits** - Some terminal programs have limited scrollback buffers. Increase your terminal's buffer size in settings, or log output to a file for later review.

### Output Stops Mid-Operation

If logging works initially then stops:

**USB-UART adapter disconnected** - Check USB connection on adapter.

**Terminal program closed** - Verify your terminal window is still open and connected.

**Firmware crash** - If logging stops abruptly and the converter becomes unresponsive, the firmware may have crashed. Power cycle the converter. Check for consistent crash behavior and file a bug report.

**Watchdog reset** - The RP2040 has a watchdog timer. If the main loop blocks for too long (firmware bug), the watchdog resets the chip. You'll see startup messages appear again after the reset.

---

## Practical Usage Tips

### Capturing Logs to File

For detailed analysis or bug reports, save log output to a file:

**macOS/Linux with screen**:
```bash
# Start screen with logging
screen -L -Logfile keyboard-log.txt /dev/tty.usbserial-* 115200
```

**macOS/Linux with picocom**:
```bash
picocom -b 115200 /dev/ttyUSB0 | tee keyboard-log.txt
```

**Windows with PuTTY**: Session → Logging → "All session output" → Specify filename

Having a log file lets you review events after they happen, share with others for troubleshooting, or compare behavior across multiple tests.

### Filtering Output

If debug level generates too much output, filter to specific messages:

**macOS/Linux**:
```bash
# Show only errors
screen /dev/tty.usbserial-* 115200 | grep "\[ERR\]"

# Show errors and info
screen /dev/tty.usbserial-* 115200 | grep -E "\[ERR\]|\[INFO\]"
```

This keeps your terminal cleaner while still capturing relevant information.

### Correlating Logs with Actions

When troubleshooting, note what you're doing as you review logs:

1. Set log level to 2 (Debug)
2. Press a specific key
3. Look for the corresponding scancode and HID report in logs
4. Verify the scancode matches your keymap expectations

This technique helps identify wrong keymaps, protocol issues, or timing problems.

### Remote Logging

For headless setups (converter embedded in a project without easy access), you can extend UART over longer distances:

**RS-232 level shifters** - Convert 3.3V UART to RS-232 voltage levels (±12V), which can travel many meters over cable.

**UART-to-Ethernet adapters** - Send log output over network to a remote terminal.

These are advanced configurations beyond the scope of this document, but they're useful for installations where physical access is inconvenient.

---

## Related Documentation

**Command Mode**:
- [Command Mode Guide](command-mode.md) - How to access log level adjustment (D command)

**Implementation details**:
- [`uart.c`](../../src/common/lib/uart.c) - DMA-based UART implementation with ring buffer
- [`uart.h`](../../src/common/lib/uart.h) - UART DMA initialization and configuration
- [`log.h`](../../src/common/lib/log.h) - Log level filtering and LOG_* macros (LOG_ERROR, LOG_WARN, LOG_INFO, LOG_DEBUG)
- [`log.c`](../../src/common/lib/log.c) - Log level runtime management
- [`config.h`](../../src/config.h) - UART configuration (GPIO pins, baud rate, buffer sizes, log level defaults)

**Hardware setup**:
- [Hardware Setup Guide](../getting-started/hardware-setup.md) - GPIO pinout and wiring diagrams

---

**Questions or stuck on something?**  
Pop into [GitHub Discussions](https://github.com/PaulW/rp2040-keyboard-converter/discussions) or [report a bug](https://github.com/PaulW/rp2040-keyboard-converter/issues) if you've found an issue.
