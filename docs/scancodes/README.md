# Scancode Documentation

When you press a key on your keyboard, it doesn't send the letter 'A' or the number '5'—it sends a scancode, which is just a number that identifies which physical key was pressed. Different keyboard protocols use different scancode sets, and understanding which one your keyboard uses helps you work out what's going on when things don't behave quite as expected.

The converter handles all the scancode translation automatically, but if you're adding support for a new keyboard or debugging why a particular key isn't mapping correctly, these documents explain how each scancode set works and what makes them different.

---

## Scancode Sets

### PC/AT Scancode Sets

| Scancode Set | Used By | Documentation |
|--------------|---------|---------------|
| **Set 1** | IBM XT keyboards, Some AT keyboards | **[→ Full Guide](set1.md)** |
| **Set 2** | AT/PS2 keyboards (default) | **[→ Full Guide](set2.md)** |
| **Set 3** | Terminal keyboards (122-key), Some programmable keyboards | **[→ Full Guide](set3.md)** |
| **Set 1/2/3** | Auto-detecting processor (recommended) | **[→ Full Guide](set123.md)** |

### Other Scancode Sets

| Scancode Set | Used By | Documentation |
|--------------|---------|---------------|
| **Amiga** | Commodore Amiga keyboards | **[→ Full Guide](amiga.md)** |

**Note**: Apple M0110 keyboards use a unique scancode set that's integrated directly into the [M0110 protocol implementation](../protocols/m0110.md)—there's no separate scancode processor for it.

## Source Code Organisation

Scancode processors are located in [`src/scancodes/`](../../src/scancodes/):

- **[`set1/`](../../src/scancodes/set1/)** - XT scancode set processor
- **[`set2/`](../../src/scancodes/set2/)** - AT/PS2 scancode set processor
- **[`set3/`](../../src/scancodes/set3/)** - Terminal scancode set processor
- **[`set123/`](../../src/scancodes/set123/)** - Unified multi-set processor
- **Amiga** - Integrated with [Amiga protocol](../protocols/amiga.md) (documentation only)

---

## Quick Reference

If you're not sure which scancode set your keyboard uses, this should help:

| Your Keyboard | Scancode Set | Processor |
|---------------|--------------|-----------|
| IBM Model M (any variant) | Set 2 | `set123` |
| IBM Model F PC/AT | Set 2 | `set123` |
| IBM Model F XT | Set 1 | `set1` |
| IBM Terminal keyboards (122-key) | Set 3 | `set123` |
| PS/2 keyboards | Set 2 | `set123` |
| Commodore Amiga keyboards | Amiga | `amiga` |
| Apple M0110/M0110A | M0110-specific | Built into protocol |

**Recommended approach:** Use `set123` for any PC/AT-compatible keyboard—it auto-detects which set the keyboard is sending and handles the translation automatically. You don't need to know which set your keyboard uses; the processor works it out for you.

---

## What Makes Them Different?

The three PC scancode sets evolved over time as keyboards got more sophisticated. Here's how they compare:

| Feature | Set 1 | Set 2 | Set 3 |
|---------|-------|-------|-------|
| **Era** | 1981 (XT) | 1984 (AT) | 1987 (Terminal) |
| **Break Encoding** | High bit set | F0 prefix | F0 prefix |
| **Extended Keys** | E0 prefix | E0 prefix | None (clean design) |
| **Special Cases** | 6-byte Pause | 8-byte Pause | No Pause key |
| **Complexity** | Medium | High | Low |
| **Usage** | XT keyboards | AT/PS2 default | Terminal keyboards |

**Set 1** uses the high bit to indicate key release—when you press 'A' it sends `0x1E`, when you release it sends `0x9E` (0x1E with bit 7 set). Extended keys like the arrow keys send an `E0` prefix first. The Pause key is particularly odd—it sends a 6-byte sequence that includes both make and break codes in one go.

**Set 2** separates make and break codes more cleanly—pressing 'A' sends `0x1C`, releasing it sends `F0 1C`. Extended keys still use `E0` prefixes, so Right Control is `E0 14` for make and `E0 F0 14` for break. The Pause key is even more elaborate here—8 bytes with an `E1` prefix. This is the default scancode set for AT/PS2 keyboards.

**Set 3** simplifies things considerably. No prefixes for extended keys, consistent two-byte break codes (`F0` + scancode), and no special Pause key handling. It's a cleaner design, used primarily on terminal keyboards.

**Amiga** scancodes are completely different—they're 7-bit codes with bit 7 indicating make (0) or break (1). No multibyte sequences, no prefixes, just simple single-byte scancodes with the release bit included. Much simpler than the PC scancode sets, though obviously only used on Amiga keyboards.

---

## Auto-Detection (set123)

The `set123` processor detects which scancode set your keyboard is sending and handles the translation automatically. It recognises Set 1 by the high-bit encoding for breaks, Set 2 by the `F0` prefix pattern, and Set 3 by the absence of extended key prefixes.

This is the recommended approach for any PC/AT-compatible keyboard—you don't need to know which set your keyboard uses, and if the keyboard supports multiple sets (which some do), the processor adapts to whichever one it's currently sending.

See the [Set 1/2/3 processor documentation](set123.md) for technical details on how detection works.

---

## When You Need Specific Scancode Sets

Whilst `set123` handles the general case, there are times when you might want to use a specific scancode set processor:

**Use `set1` when:**
- You're working with an XT keyboard that only supports Set 1
- You want to avoid the auto-detection overhead (minimal, but it exists)
- You're debugging Set 1-specific timing or encoding issues

**Use `set2` when:**
- You have a keyboard that only sends Set 2 (AT/PS2 keyboards)
- You're optimising for code size and know you'll never encounter other sets
- You're debugging Set 2-specific sequences like the `E1` Pause key

**Use `amiga` when:**
- You're connecting an Amiga keyboard (obviously)
- You need the Amiga-specific Caps Lock handling

---

## Technical Details

Each scancode documentation includes:

- **Encoding schemes** - How make and break codes are formed
- **Extended key sequences** - Multibyte scancode patterns (E0, E1 prefixes)
- **Special cases** - Pause key, Print Screen, and other unusual scancodes
- **Timing requirements** - Protocol-specific timing constraints
- **Complete scancode tables** - Every key and its corresponding scancode(s)

These documents are quite detailed—they're reference material for when you need to understand exactly what's happening at the protocol level or when you're adding support for unusual keyboards.

---

## Related Documentation

**Protocol Specifications:**
- [AT/PS2 Protocol](../protocols/at-ps2.md) - Uses Scancode Set 2 (can support Set 1/3)
- [XT Protocol](../protocols/xt.md) - Uses Scancode Set 1
- [Amiga Protocol](../protocols/amiga.md) - Uses Amiga scancode set
- [Apple M0110 Protocol](../protocols/m0110.md) - M0110-specific scancodes

**Implementation:**
- [Architecture](../advanced/README.md) - How scancode processing fits into the system
- [Adding Keyboards](../development/adding-keyboards.md) - Which scancode set to specify in keyboard.config

**Hardware:**
- [Keyboards](../keyboards/README.md) - Supported keyboard models and their scancode sets

---

**Questions or stuck on something?**  
Pop into [GitHub Discussions](https://github.com/PaulW/rp2040-keyboard-converter/discussions) or [report a bug](https://github.com/PaulW/rp2040-keyboard-converter/issues) if you've found an issue.
