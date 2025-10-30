# Protocol Documentation

**Status**: ✅ Complete | **Last Updated**: 27 October 2025

This converter supports a variety of keyboard and mouse protocols from the 1980s and early 1990s. Each protocol has its own detailed documentation with historical context, technical specifications, timing diagrams, and implementation notes.

---

## Supported Protocols

### Keyboard Protocols

| Protocol | Era | Direction | Typical Keyboards | Documentation |
|----------|-----|-----------|-------------------|---------------|
| **AT/PS2** | 1984-2000s | Bidirectional | IBM Model M, Most PC keyboards | **[→ Full Guide](at-ps2.md)** |
| **XT** | 1981-1987 | Unidirectional | IBM Model F (XT), Early PC keyboards | **[→ Full Guide](xt.md)** |
| **Amiga** | 1985-1996 | Bidirectional | Commodore Amiga A500/A1000/A2000/A3000/A4000 | **[→ Full Guide](amiga.md)** |
| **Apple M0110** | 1984-1987 | Poll-based | Macintosh M0110, M0110A, M0110B | **[→ Full Guide](m0110.md)** |

### Mouse Protocols

| Protocol | Era | Description | Documentation |
|----------|-----|-------------|---------------|
| **AT/PS2 Mouse** | 1987-2000s | 3/4-byte packets, IntelliMouse support | **[→ Full Guide](at-ps2.md#mouse-implementation)** |

**Note**: Mouse protocol documentation is included in the AT/PS2 guide, as both keyboard and mouse use the same underlying protocol with different data formats.

---

## Quick Protocol Selection

**Don't know which protocol your keyboard uses?** Use this quick reference:

| Your Keyboard | Protocol |
|---------------|----------|
| IBM Model M Enhanced (1391401) | AT/PS2 |
| IBM Model M (101/102-key) | AT/PS2 |
| IBM Model F PC/AT | AT/PS2 |
| IBM Model F XT | XT |
| Commodore Amiga keyboard (any model) | Amiga |
| Apple Macintosh M0110/M0110A | M0110 |
| Most keyboards with 5-pin DIN connector | XT or AT/PS2 |
| Most keyboards with 6-pin mini-DIN connector | AT/PS2 |

**Still unsure?** See the [Keyboards Documentation](../keyboards/README.md) for detailed keyboard identification.

---

## Why Multiple Protocols?

Vintage keyboards use different communication protocols depending on when and by whom they were designed. Unlike modern USB, the early computing era (1980s-1990s) had no universal standard—each manufacturer designed protocols optimized for their specific hardware and design philosophy:

- **IBM XT (1981)**: Simplest protocol—keyboard sends scancodes, no host commands, no LEDs
- **IBM AT/PS2 (1984)**: Bidirectional—host can send commands, control LEDs, configure keyboard
- **Commodore Amiga (1985)**: Custom handshake and bit rotation for safety and reliability
- **Apple M0110 (1984)**: Poll-based—host requests keys, keyboard responds on demand

Each protocol reflects different engineering priorities: cost, simplicity, features, or integration with specific computer architectures.

---

## Protocol Comparison

---


### Feature Comparison

| Feature | AT/PS2 | XT | Amiga | M0110 |
|---------|--------|-----|-------|-------|
| **Direction** | Bidirectional | Unidirectional | Bidirectional | Poll-based |
| **LED Control** | ✅ Yes | ❌ No | ✅ Yes (special) | ❌ No |
| **Error Detection** | ✅ Parity | ❌ None | ✅ Handshake | ❌ Minimal |
| **Host Commands** | ✅ Yes | ❌ No | ✅ Limited | ✅ Yes |
| **Clock Source** | Device/Host | Device only | Device only | Host only |
| **Complexity** | High | Low | High | Medium |

**For detailed technical specifications, timing diagrams, and implementation notes, see each protocol's dedicated documentation.**

---

## Implementation Notes

All protocols are implemented using the RP2040's **PIO (Programmable I/O)** hardware for precise timing:

- **Microsecond accuracy**: Hardware-driven timing independent of CPU load
- **Non-blocking**: Protocols run in parallel with main firmware
- **IRQ-driven**: Scancodes trigger interrupts for processing
- **Low CPU overhead**: PIO handles bit-level timing automatically

⚠️ **CRITICAL: Level Shifting Required**
- Keyboard/Mouse signals: **5V**
- RP2040 GPIO: **3.3V** (NOT 5V tolerant)
- **Solution**: Bidirectional level shifter (e.g., BSS138-based) on both CLK and DATA lines

See **[Hardware Setup](../hardware/README.md)** for wiring and level shifter details.

---

## Related Documentation

**Detailed Guides:**
- **[AT/PS2 Protocol](at-ps2.md)** - Comprehensive guide with keyboard and mouse implementation
- **[XT Protocol](xt.md)** - Full specifications and genuine vs clone detection
- **[Amiga Protocol](amiga.md)** - Handshake timing and special features
- **[Apple M0110 Protocol](m0110.md)** - Poll-based protocol and command reference

**Hardware and Setup:**
- **[Hardware Setup](../hardware/README.md)** - Physical connections and wiring
- **[Keyboards](../keyboards/README.md)** - Supported keyboard models
- **[Getting Started](../getting-started/README.md)** - Quick start guide

**Advanced Topics:**
- **[Architecture](../advanced/architecture.md)** - Implementation details and PIO programs
- **[Troubleshooting](../advanced/troubleshooting.md)** - Protocol-specific debugging

**Source Code:**
- Protocol implementations: [`src/protocols/`](../../src/protocols/)
- PIO programs: [`src/protocols/*/keyboard.pio`](../../src/protocols/)

---

## Need Help?

- 📖 **[Troubleshooting Guide](../advanced/troubleshooting.md)** - Common issues and solutions
- 💬 **[Discussions](https://github.com/PaulW/rp2040-keyboard-converter/discussions)** - Ask questions
- 🐛 **[Issue Tracker](https://github.com/PaulW/rp2040-keyboard-converter/issues)** - Report bugs
