# Protocol Documentation

**Status**: 🔄 In Progress | **Last Updated**: 27 October 2025

Complete protocol documentation for supported keyboard and mouse communication protocols.

## Supported Protocols

### Keyboard Protocols

| Protocol | Description | Keyboards | Status |
|----------|-------------|-----------|--------|
| **[AT/PS2](at-ps2.md)** | Bidirectional, clock-synchronized | IBM Model M, Most modern keyboards | ✅ Production |
| **[XT](xt.md)** | Unidirectional, simple protocol | IBM Model F (XT), Early keyboards | ✅ Production |
| **[Amiga](amiga.md)** | Bidirectional, special handshake | Amiga keyboards | ✅ Production |
| **[Apple M0110](m0110.md)** | Unidirectional, Apple-specific | Macintosh M0110, M0110A | ✅ Production |

### Mouse Protocols

| Protocol | Description | Mice | Status |
|----------|-------------|------|--------|
| **[AT/PS2 Mouse](at-ps2-mouse.md)** | 3-byte packets, bidirectional | PS/2 mice, IntelliMouse | ✅ Production |

---

## Protocol Overview

### AT/PS2 Protocol

**Direction**: Bidirectional (host ↔ device)  
**Voltage**: 5V  
**Lines**: CLK (Clock), DATA  
**Communication**: Device generates clock, either can send data

**Key Features:**
- Most common protocol for modern keyboards
- Supports LED control (Caps Lock, Num Lock, Scroll Lock)
- Error detection via parity bit
- Start/stop bits for frame synchronization

**See:** [AT/PS2 Protocol Guide](at-ps2.md)

---

### XT Protocol

**Direction**: Unidirectional (device → host)  
**Voltage**: 5V  
**Lines**: CLK (Clock), DATA  
**Communication**: Device sends scancodes only

**Key Features:**
- Simple make/break scancodes
- No host-to-device communication
- No LED control
- Fixed clock timing (~20 kHz)

**See:** [XT Protocol Guide](xt.md)

---

### Amiga Protocol

**Direction**: Bidirectional (host ↔ device)  
**Voltage**: 5V  
**Lines**: CLK (Clock), DATA, RESET  
**Communication**: Special handshake sequence

**Key Features:**
- Unique handshake-driven timing
- Caps Lock synchronization via pulse
- Reset line for keyboard initialization
- Make/break scancodes with prefix

**See:** [Amiga Protocol Guide](amiga.md)

---

### Apple M0110 Protocol

**Direction**: Unidirectional (device → host)  
**Voltage**: 5V  
**Lines**: CLK (Clock), DATA  
**Communication**: Device responds to host requests

**Key Features:**
- Poll-based communication
- Host requests, keyboard responds
- Simple protocol, minimal error handling
- No LED control

**See:** [Apple M0110 Protocol Guide](m0110.md)

---

### AT/PS2 Mouse Protocol

**Direction**: Bidirectional (host ↔ device)  
**Voltage**: 5V  
**Lines**: CLK (Clock), DATA  
**Communication**: Same as AT/PS2 keyboard

**Key Features:**
- 3-byte standard packets (button status, X-delta, Y-delta)
- 4-byte extended packets (scroll wheel - IntelliMouse protocol)
- Independent from keyboard (separate CLK/DATA lines)
- Can be used with any keyboard protocol

**See:** [AT/PS2 Mouse Protocol Guide](at-ps2-mouse.md)

---

## Protocol Selection

### How to Choose

Your keyboard determines the protocol:

| Keyboard Type | Protocol |
|---------------|----------|
| IBM Model M Enhanced | AT/PS2 |
| IBM Model M 1391401 | AT/PS2 |
| IBM Model F PC/AT | AT/PS2 |
| IBM Model F XT | XT |
| IBM Model F 4704 | Custom (not yet supported) |
| Amiga Keyboards | Amiga |
| Apple M0110/M0110A | M0110 |
| Modern keyboards | Usually AT/PS2 |

**See:** [Keyboards Documentation](../keyboards/README.md) for specific keyboard details.

---

## Technical Details

### PIO State Machines

All protocols are implemented using RP2040's PIO (Programmable I/O) hardware:

- **Precise timing**: Hardware-driven, microsecond accuracy
- **CPU independent**: Protocols run in parallel with main code
- **IRQ-driven**: Scancodes trigger interrupts for processing
- **Non-blocking**: Main loop remains responsive

**See:** [Architecture Guide](../advanced/architecture.md) for implementation details.

---

### Signal Levels

⚠️ **CRITICAL: Level Shifting Required**

- **Keyboard/Mouse**: 5V signals
- **RP2040 GPIO**: 3.3V (NOT 5V tolerant)
- **Solution**: Bi-directional level shifter (e.g., BSS138-based)

Both CLK and DATA lines must be level-shifted.

**See:** [Level Shifters Guide](../hardware/level-shifters.md)

---

### Timing Requirements

Each protocol has strict timing requirements:

| Protocol | Clock Freq | Bit Time | Frame Time |
|----------|-----------|----------|------------|
| AT/PS2 | 10-16.7 kHz | 60-100 μs | ~1 ms |
| XT | ~20 kHz | ~50 μs | ~550 μs |
| Amiga | Variable | Variable | ~1 ms |
| M0110 | Variable | Variable | ~1 ms |

PIO state machines handle all timing automatically.

---

## Adding New Protocols

To add support for a new protocol:

1. **Research**: Understand timing, framing, and communication flow
2. **PIO Program**: Write `.pio` assembly for bit timing and framing
3. **Interface**: Implement `keyboard_interface.h/c` for state management
4. **Test**: Verify with real hardware (timing-critical)

**See:** [Contributing Guide](../development/contributing.md) for implementation details.

---

## Related Documentation

**In This Documentation:**
- [Hardware Setup](../hardware/README.md) - Physical connections
- [Keyboards](../keyboards/README.md) - Supported keyboards
- [Architecture](../advanced/architecture.md) - Implementation details

**Source Code:**
- Protocol implementations: [`src/protocols/`](../../src/protocols/)
- PIO programs: [`src/protocols/*/keyboard.pio`](../../src/protocols/)

---

## Need Help?

- 📖 [Troubleshooting](../advanced/troubleshooting.md)
- 💬 [Ask Questions](https://github.com/PaulW/rp2040-keyboard-converter/discussions)
- 🐛 [Report Protocol Issues](https://github.com/PaulW/rp2040-keyboard-converter/issues)

---

**Status**: Documentation in progress. Protocol-specific guides coming soon!
