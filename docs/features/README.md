# Features Documentation

**Status**: 🔄 In Progress | **Last Updated**: 27 October 2025

Complete documentation for all converter features.

## Core Features

### USB HID Interface

**[USB HID Guide](usb-hid.md)**

Modern USB HID boot protocol support:

- ✅ **Full NKRO**: All keys reported simultaneously (no ghosting)
- ✅ **Boot Protocol**: Compatible with BIOS/UEFI and legacy systems
- ✅ **Mouse Support**: Simultaneous keyboard + mouse conversion
- ✅ **LED Indicators**: Caps Lock, Num Lock, Scroll Lock synchronization
- ✅ **Multiple OS**: Works with Windows, macOS, Linux, BSD

**Performance:**
- 350μs latency (PIO IRQ → USB transmission)
- 30 CPS reliable throughput (360 WPM)
- ~2% CPU usage at maximum load
- 10ms USB polling interval (host-driven)

**See:** [USB HID Guide](usb-hid.md)

---

### Command Mode

**[Command Mode Guide](command-mode.md)**

Special mode for firmware management and testing:

**Activation**: Hold **Left Shift + Right Shift** during boot

**Features:**
- 🔄 **Bootloader Entry**: Flash new firmware (press 'B')
- 🧪 **Keytest Mode**: Verify all keys work (press 'T')
- 🎨 **LED Test**: Test indicator LEDs (automatic)
- ⚙️ **Configuration**: View/modify persistent settings (planned)

**LED Indicators:**
- All LEDs flash rapidly = Command mode active
- Num Lock LED blinks = Keytest mode active
- Normal operation = Command mode disabled

**See:** [Command Mode Guide](command-mode.md)

---

### Keytest Mode

**[Keytest Mode Guide](keytest-mode.md)**

Interactive key testing interface:

**Purpose**: Verify all keys register correctly without external software

**How to Use:**
1. Enter Command Mode (Left Shift + Right Shift during boot)
2. Press 'T' to activate Keytest Mode
3. Press each key - LED blinks on press
4. Press Escape to exit and reboot

**Indicators:**
- Num Lock LED blinks on each keypress
- Works with any keyboard layout
- Tests make/break events
- No computer required (works standalone)

**See:** [Keytest Mode Guide](keytest-mode.md)

---

### Configuration Storage

**[Configuration Storage Guide](config-storage.md)**

Persistent configuration stored in flash memory:

**Current Settings:**
- Protocol-specific parameters
- LED brightness (planned)
- Custom key mappings (planned)
- Debounce timing (planned)

**Storage:**
- Flash memory (separate from firmware)
- Survives power cycles
- Reset to defaults via Command Mode
- 4KB reserved sector

**Status**: ⚠️ Basic implementation, extended features planned

**See:** [Configuration Storage Guide](config-storage.md)

---

### LED Indicators

**[LED Support Guide](led-support.md)**

Full LED support for keyboard indicators:

**Supported LEDs:**
- ✅ Caps Lock
- ✅ Num Lock
- ✅ Scroll Lock
- ✅ Compose (if keyboard supports)
- ✅ Kana (if keyboard supports)

**Protocol Support:**
- AT/PS2: Full LED control
- XT: No LED control (unidirectional protocol)
- Amiga: Caps Lock sync via pulse
- M0110: No LED control

**Optional WS2812 RGB LED:**
- Status indicator (Command Mode, Keytest)
- Configurable brightness
- Disabled by default (saves power)

**See:** [LED Support Guide](led-support.md)

---

### Mouse Support

**[Mouse Support Guide](mouse-support.md)**

AT/PS2 mouse protocol conversion:

**Features:**
- ✅ Standard 3-byte packets (buttons, X, Y)
- ✅ 4-byte extended packets (scroll wheel - IntelliMouse)
- ✅ Independent hardware lines (separate CLK/DATA)
- ✅ Works with any keyboard protocol
- ✅ Simultaneous keyboard + mouse

**Example:**
```bash
# Build with keyboard + mouse
docker compose run --rm -e KEYBOARD="modelm/enhanced" -e MOUSE="at-ps2" builder
```

**Pin Configuration:**
- Mouse CLK: GPIO 19
- Mouse DATA: GPIO 18
- Level shifters required (5V → 3.3V)

**See:** [Mouse Support Guide](mouse-support.md)

---

### Logging & Debugging

**[Logging Guide](logging.md)**

UART-based logging for development and troubleshooting:

**Features:**
- DMA-driven UART (non-blocking)
- Configurable log levels (INFO, DEBUG, ERROR)
- Protocol state information
- Error tracking
- Performance metrics

**Output Example:**
```
[INFO] Keyboard interface initialized (AT/PS2)
[DBG] Scancode: 0x1C (Make)
[ERR] Parity error detected
[INFO] Command mode activated
```

**Connection:**
- TX: GPIO 0
- RX: GPIO 1 (optional)
- Baud: 115200
- Use USB-UART adapter for monitoring

**See:** [Logging Guide](logging.md)

---

## Feature Comparison

| Feature | AT/PS2 | XT | Amiga | M0110 | Mouse |
|---------|--------|----|----|-------|-------|
| **Full NKRO** | ✅ | ✅ | ✅ | ✅ | N/A |
| **LED Control** | ✅ | ❌ | Caps only | ❌ | N/A |
| **Bidirectional** | ✅ | ❌ | ✅ | ❌ | ✅ |
| **Command Mode** | ✅ | ✅ | ✅ | ✅ | N/A |
| **Keytest Mode** | ✅ | ✅ | ✅ | ✅ | N/A |
| **Config Storage** | ✅ | ✅ | ✅ | ✅ | ✅ |

---

## Planned Features

### Coming Soon

- 🔄 **Extended Config Options** - Custom key mappings, debounce timing, LED brightness
- 🔄 **WebHID Interface** - Browser-based configuration tool (no drivers needed)
- 🔄 **Macro Support** - Custom key macros and sequences
- 🔄 **Multiple Layouts** - Switch between layouts without reflashing
- 🔄 **Advanced Mouse** - Acceleration profiles, button remapping

### Under Consideration

- 📋 **IBM 4704 Protocol** - Support for Model F 4704 keyboards
- 📋 **ADB Protocol** - Apple Desktop Bus keyboards
- 📋 **Serial Mouse** - 9-pin serial mouse support
- 📋 **Bluetooth** - Wireless conversion (hardware dependent)

---

## Feature Request Process

Have an idea for a new feature?

1. **Search**: Check [existing discussions](https://github.com/PaulW/rp2040-keyboard-converter/discussions) and [issues](https://github.com/PaulW/rp2040-keyboard-converter/issues)
2. **Discuss**: Start a [feature request discussion](https://github.com/PaulW/rp2040-keyboard-converter/discussions/new?category=ideas)
3. **Provide Details**:
   - Use case description
   - Expected behavior
   - Hardware requirements (if any)
   - Priority level

**Architecture Constraints:**
- ⚠️ Single-core only (Core 0)
- ⚠️ Non-blocking operations required
- ⚠️ Must run from SRAM (not Flash)
- ⚠️ Ring buffer capacity (32 bytes)
- ⚠️ USB polling interval (10ms)

**See:** [Architecture Guide](../advanced/architecture.md)

---

## Implementation Details

All features are designed with these principles:

### Non-Blocking Architecture

**No** `sleep_ms()`, `busy_wait_us()`, or blocking operations:
```c
// ❌ BAD: Blocking
sleep_ms(100);

// ✅ GOOD: State machine with timeout
static uint32_t state_start = 0;
if (to_ms_since_boot(get_absolute_time()) - state_start > 100) {
  // Timeout reached, proceed
}
```

**See:** [Architecture Guide](../advanced/architecture.md)

---

### Performance Optimization

**Resource constraints:**
- ~2% CPU usage at 30 CPS (97.9% idle)
- 32-byte ring buffer (adequate for burst handling)
- 350μs end-to-end latency
- 264KB SRAM (132KB used)

**See:** [Performance Guide](../advanced/performance.md)

---

## Related Documentation

**In This Documentation:**
- [Getting Started](../getting-started/README.md) - Basic setup
- [Protocols](../protocols/README.md) - Protocol details
- [Architecture](../advanced/architecture.md) - Implementation details
- [Development](../development/README.md) - Contributing guide

**Source Code:**
- Command Mode: [`src/common/lib/command_mode.[ch]`](../../src/common/lib/command_mode.c)
- HID Interface: [`src/common/lib/hid_interface.[ch]`](../../src/common/lib/hid_interface.c)
- Config Storage: [`src/common/lib/config_storage.[ch]`](../../src/common/lib/config_storage.c)

---

## Need Help?

- 📖 [Troubleshooting](../advanced/troubleshooting.md)
- 💬 [Ask Questions](https://github.com/PaulW/rp2040-keyboard-converter/discussions)
- 🐛 [Report Issues](https://github.com/PaulW/rp2040-keyboard-converter/issues)
- 💡 [Feature Requests](https://github.com/PaulW/rp2040-keyboard-converter/discussions/new?category=ideas)

---

**Status**: Documentation in progress. Feature-specific guides coming soon!
