# Protocol Documentation

Early keyboards used different communication protocols. With no standardised interface in the early computer marketplace, each manufacturer developed their own approach. As I've collected different keyboards over time, I've implemented support for multiple protocols to get them working with modern systems, and I hope to add more as and when I get hold of anything new.

Below you'll find details on each protocol, plus quick reference tables if you're not sure which one your keyboard uses.

---

## Supported Protocols

### Keyboard Protocols

| Protocol | Typical Keyboards | Documentation |
|----------|-------------------|---------------|
| **AT/PS2** | IBM Model M, Most PC keyboards | **[→ Full Guide](at-ps2.md)** |
| **XT** | IBM Model F (XT), Early PC keyboards | **[→ Full Guide](xt.md)** |
| **Amiga** | Commodore Amiga A500/A1000/A2000/A3000 | **[→ Full Guide](amiga.md)** |
| **Apple M0110** | Macintosh M0110, M0110A, M0110B | **[→ Full Guide](m0110.md)** |

### Mouse Protocols

| Protocol | Documentation |
|----------|---------------|
| **AT/PS2 Mouse** | **[→ Full Guide](at-ps2.md#mouse-implementation)** |

**Note**: Mouse protocol documentation is included in the AT/PS2 guide, as both keyboard and mouse use the same underlying protocol with different data formats.

---

## Quick Protocol Selection

If you're not sure which protocol your keyboard uses, this table should help:

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

## Feature Comparison

Here's how the protocols stack up against each other:

| Feature | AT/PS2 | XT | Amiga | M0110 |
|---------|--------|-----|-------|-------|
| **Direction** | Bidirectional | Unidirectional | Bidirectional | Poll-based |
| **LED Control** | ✅ Yes | ❌ No | ✅ Yes (special) | ❌ No |
| **Error Detection** | ✅ Parity | ❌ None | ✅ Handshake | ❌ Minimal |
| **Host Commands** | ✅ Yes | ❌ No | ✅ Limited | ✅ Yes |
| **Complexity** | High | Low | High | Medium |

**For detailed technical specifications, timing diagrams, and implementation notes, see each protocol's dedicated documentation.**

**Hardware note:** Most vintage keyboards and mice use 5V logic, but the RP2040's GPIO pins are 3.3V and not 5V tolerant. You'll need bidirectional level shifters (BSS138-based modules work well) on both the CLOCK and DATA lines. See the [Hardware Setup](../hardware/README.md) guide for wiring details.

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
- **[Advanced Topics](../advanced/README.md)** - Implementation details and PIO programs
- **[Scancode Sets](../scancodes/README.md)** - Scancode translation and encoding

**Source Code:**
- Protocol implementations: [`src/protocols/`](../../src/protocols/)
- PIO programs: [`src/protocols/*/keyboard.pio`](../../src/protocols/)

---

**Questions or stuck on something?**  
Pop into [GitHub Discussions](https://github.com/PaulW/rp2040-keyboard-converter/discussions) or [report a bug](https://github.com/PaulW/rp2040-keyboard-converter/issues) if you've found an issue.
