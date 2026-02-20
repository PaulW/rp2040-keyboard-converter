# Scancode Set 1 (XT)

Scancode Set 1 is the scancode design used by IBM PC and PC/XT keyboards. The designation "XT scancode set" derives from its use in the PC/XT keyboard. Set 1 continues to be used by keyboards operating in XT compatibility mode or implementing XT protocol support.

The encoding scheme: each key generates a make code when pressed and a break code when released. Break codes use the make code with bit 7 set (adding 0x80). Extended keys require multi-byte sequences using E0 and E1 prefixes to encode keys not present on the original XT keyboard layout. Set 1 has been in use for over 40 years across multiple keyboard generations.

## Encoding Scheme

### Basic Encoding

Set 1 uses an 8-bit encoding where:
- **Bit 7 (0x80)**: Release flag
  - `0` = Key press (make code)
  - `1` = Key release (break code)
- **Bits 6-0**: Key identity (0x01-0x7F)

### Make and Break Codes

| Event | Encoding | Example (ESC key = 0x01) |
|-------|----------|--------------------------|
| **Key Press (Make)** | `code` | `0x01` |
| **Key Release (Break)** | `code \| 0x80` | `0x81` |

This simple scheme means:
- Make code: `0x1C` (Return key pressed)
- Break code: `0x9C` (Return key released)

### Extended Keys (E0 Prefix)

The IBM Enhanced Keyboard (introduced with the IBM AT) added keys not present on the original XT keyboard: arrow keys, separate cursor movement keys (Home, End, Page Up, Page Down), and additional modifiers (Right Alt, Right Control). IBM used the E0 prefix byte to encode these "extended keys" in a separate namespace from the existing single-byte scancodes.

Extended keys use a two-byte sequence:

| Sequence | Description |
|----------|-------------|
| `E0 xx` | Extended key press |
| `E0 (xx \| 0x80)` | Extended key release |

**Example: Right Control**
- Press: `E0 1D`
- Release: `E0 9D`

Extended keys include:
- Right Alt, Right Control
- Arrow keys (Up, Down, Left, Right)
- Navigation cluster (Home, End, Page Up, Page Down, Insert, Delete)
- Windows keys (E0 5B, E0 5C)
- Application key (E0 5D)
- Multimedia keys

### Pause/Break Key (E1 Prefix)

The Pause/Break key uses a special 6-byte sequence:

| Event | Sequence |
|-------|----------|
| **Pause** | `E1 1D 45 E1 9D C5` |
| **Break** (Ctrl+Pause) | `E0 46 E0 C6` |
| **Alternate Pause** | `E0 45` (some keyboards) |

**Important**: 
- Pause sends the entire sequence on press only, with no separate release event
- Break (Ctrl+Pause) uses E0 46 sequence
- Some keyboards may send E0 45 as an alternate Pause encoding

**USB HID Mapping:**
- All Pause variants map to **interface code 0x48** (USB HID Pause/Break key)
- E1 sequences (regular Pause): `E1 1D 45` → 0x48
- E0 46 (Ctrl+Pause): `E0 46` → 0x48 (with Ctrl modifier)
- E0 45 (alternate): `E0 45` → 0x48
- See Issue #21 for historical context on E0 mapping fixes
- Note: Some very old or non-compliant keyboards may not emit the E1 prefix for Pause
  (they instead send a bare sequence that looks like Ctrl+NumLock). Such keyboards cannot
  be reliably distinguished from intentional Ctrl+key combinations in firmware without
  introducing fragile heuristics that break other combinations. Recommended workaround:
  remap an unused physical key (for example Scroll Lock) to PAUS in your keyboard
  configuration if you need a dedicated Pause key for a non-compliant keyboard.

### Fake Shift Codes

Set 1 Print Screen scancodes include "fake shift" codes. The IBM PC XT Technical Reference documents Print Screen as involving the Shift key. The AT keyboard's Print Screen implementation uses extended key encoding (E0 prefix) and includes shift codes in the scancode sequence that do not correspond to physical Shift key presses.

Implementations must filter these fake shift codes to prevent unintended Shift key events in the output.

Set 1 Print Screen sequences include "fake shift" codes:

| Sequence | Description |
|----------|-------------|
| `E0 2A E0 37` | Print Screen press (fake left shift + actual code) |
| `E0 B7 E0 AA` | Print Screen release (actual release + fake left shift release) |

**Fake shift bytes to ignore:**
- `E0 2A` - Fake Left Shift press
- `E0 AA` - Fake Left Shift release
- `E0 36` - Fake Right Shift press
- `E0 B6` - Fake Right Shift release

Modern converters should **filter out** these fake shift codes to prevent spurious Shift events.

### Self-Test Code Collision

Set 1 has a collision between self-test codes and valid scancodes. The self-test passed code `0xAA` is identical to the Left Shift break code (`0x2A | 0x80 = 0xAA`). This creates a potential for misinterpreting keyboard reset events as key releases.

**Protocol Layer Filtering:**

The protocol layer (XT, AT/PS2) filters self-test codes (`0xAA`, `0xFC`) during keyboard initialisation:
- **XT Protocol**: Filters `0xAA` whilst in `UNINITIALISED` state
- **AT/PS2 Protocol**: Filters `0xAA` and `0xFC` whilst in `UNINITIALISED` and `INIT_AWAIT_SELFTEST` states
- Once `INITIALISED`, all codes pass through to the scancode layer

**Defense-in-Depth at Scancode Layer:**

Whilst protocol filtering handles normal initialisation, keyboards can send self-test codes post-initialisation during hot-plug events or unstable connections. The scancode processor provides defense-in-depth by explicitly filtering these codes:

```c
// Set 1 scancode processor (simplified)
if (code == 0xAA || code == 0xFC) {
    // Filter self-test codes (0xAA overlaps with Left Shift break 0x2A | 0x80)
    LOG_DEBUG("!INIT! (0x%02X)\n", code);
} else if (code <= 0xD3) {
    // Valid make (0x01-0x53) or break (0x81-0xD3) codes
    handle_keyboard_report(code & 0x7F, code < 0x80);
} else {
    // Invalid codes above 0xD3
    LOG_DEBUG("!INIT! (0x%02X)\n", code);
}
```

This follows the IBM BIOS strategy of "throwing away invalid codes" (IBM PC AT BIOS listing, keyboard interrupt handler K2S routine, lines 132-164).

**Why This Matters:**

Without scancode-layer filtering, hot-plugging a keyboard would send `0xAA`, which the scancode processor would interpret as Left Shift break (`0x2A & 0x7F = 0x2A`, with bit 7 indicating release). This would create a spurious Shift key release event.

TMK's implementation handles Set 2/3 self-test codes with intentional fall-through (comment: "replug or unstable connection probably"), but Set 1 requires explicit blacklisting due to the collision.

## State Machine

Set 1 requires a **5-state** state machine to handle all sequences:

```
┌─────────────┐
│    INIT     │ ◄─────────────────┐
└─────┬───────┘                   │
      │                           │
      ├─[E0]──► E0 ──[code]───────┤
      │                           │
      ├─[E1]──► E1 ──[1D]──► E1_1D ──[45]──► E1_1D_45 ──[E1]──► E1_PAUSE ──[9D]──► E1_PAUSE_9D ──[C5]───┐
      │                                                                                                      │
      └─[code]─────────────────────────────────────────────────────────────────────────────────────────────┘
```

### State Descriptions

| State | Description | Next Action |
|-------|-------------|-------------|
| **INIT** | Waiting for scancode | Process code or enter prefix state |
| **E0** | E0 prefix received | Process extended key, filter fake shifts |
| **E1** | E1 prefix received | Begin Pause sequence parsing |
| **E1_1D** | E1 1D received | Expect 45 |
| **E1_1D_45** | E1 1D 45 received | Expect E1 |
| **E1_PAUSE** | E1 1D 45 E1 received | Expect 9D |
| **E1_PAUSE_9D** | E1 1D 45 E1 9D received | Expect C5, complete Pause |

## Key Characteristics

**What works well:**

The encoding is quite simple—release codes are just make codes with 0x80 added, which makes the state machine fairly compact at only 5 states. Single-byte scancodes are the norm, which keeps things efficient. Each byte has semantic meaning, so the protocol is self-synchronising. BIOS implementations support Set 1 widely, so compatibility's rarely an issue.

**Where it gets awkward:**

The keyspace is limited to 128 make codes (7-bit), which becomes a problem for modern keyboards with extra keys. Print Screen requires filtering out "fake shift" codes that were added for compatibility. The Pause/Break key uses an irregular sequence—6 bytes for Pause, and the break code doesn't follow the usual pattern. If you're using the XT protocol, there's no way for the host to talk back to the keyboard—it's strictly one-way communication.  

## Code Range

| Range | Usage |
|-------|-------|
| `0x01-0x53` | Standard key make codes |
| `0x54-0x7F` | Reserved/extended |
| `0x81-0xD3` | Standard key break codes (make + 0x80) |
| `0xD4-0xFF` | Reserved/extended break codes |

## Special Codes

| Code | Description |
|------|-------------|
| `0xAA` | Self-test passed (BAT - Basic Assurance Test) [⚠️ Collision](#self-test-code-collision) |
| `0xE0` | Extended key prefix |
| `0xE1` | Pause/Break prefix |
| `0xEE` | Echo response (not used in XT) |
| `0xF0` | Reserved |
| `0xFA` | ACK (not used in XT) |
| `0xFC` | Self-test failed [⚠️ Collision](#self-test-code-collision) |
| `0xFE` | Resend (not used in XT) |
| `0xFF` | Error/Buffer overflow |

## Example Sequences

### Regular Key (ESC)
```
Press:   01
Release: 81
```

### Extended Key (Right Arrow)
```
Press:   E0 4D
Release: E0 CD
```

### Print Screen (with fake shifts)
```
Press:   E0 2A E0 37  (filter E0 2A, process E0 37)
Release: E0 B7 E0 AA  (process E0 B7, filter E0 AA)
```

### Pause/Break
```
Pause:   E1 1D 45 E1 9D C5  (press only, no release)
Break:   E0 46 E0 C6        (Ctrl+Pause, separate sequence)
```

## Implementation Notes

### Processing Algorithm

```c
// Simplified Set 1 processing
switch (state) {
    case INIT:
        if (code == 0xE0) {
            state = E0;
        } else if (code == 0xE1) {
            state = E1;
        } else {
            bool is_release = (code & 0x80) != 0;
            uint8_t key = code & 0x7F;
            handle_key(key, !is_release);
        }
        break;
    
    case E0:
        // Filter fake shifts: 2A, AA, 36, B6
        if (code != 0x2A && code != 0xAA && 
            code != 0x36 && code != 0xB6) {
            bool is_release = (code & 0x80) != 0;
            uint8_t key = translate_e0_key(code & 0x7F);
            handle_key(key, !is_release);
        }
        state = INIT;
        break;
    
    case E1:
        // Pause sequence handling...
        break;
}
```

### Translation Requirements

E0-prefixed keys require translation to their logical key codes:
- Map extended scancodes to appropriate HID usage codes
- Handle duplicate keys (e.g., both Left and Right Control)
- Distinguish between main keyboard and numeric keypad keys

### Compatibility Considerations

1. **XT Protocol**: Unidirectional, no host-to-keyboard commands
2. **No LED Control**: XT keyboards have built-in LED logic, not host-controlled
3. **No Typematic Control**: Key repeat rate is keyboard-internal
4. **Self-Test Code Handling**: Defense-in-depth filtering at both protocol and scancode layers (see [Self-Test Code Collision](#self-test-code-collision))

## Performance Characteristics

- **Average bytes per keystroke**: 1.5 (single-byte scancodes, with multi-byte sequences for extended keys)
- **State machine complexity**: Medium (5 states)
- **Processing overhead**: Low (simple bit manipulation)
- **Memory requirements**: ~256 bytes for E0 translation table

## Keyboards Using Set 1

- IBM PC/XT keyboards (1981-1987)
- IBM Model F XT (Part 1501100, 1501105)
- Some early IBM Model M keyboards with XT interface
- Many modern mechanical keyboards in "XT mode"
- Cherry G80 series with XT interface

**Note**: XT keyboards don't respond to keyboard ID commands (0xF2) because the protocol is unidirectional (keyboard to host only).

## Source Code

- **[`scancode.h`](../../src/scancodes/set1/scancode.h)** - Set 1 processor interface
- **[`scancode.c`](../../src/scancodes/set1/scancode.c)** - State machine implementation
- **[Unified Processor](../../src/scancodes/set123/)** - Multi-set processor including Set 1

## References

- [IBM PC/XT Technical Reference Manual (1983)](http://bitsavers.org/pdf/ibm/pc/xt/)
- [IBM Personal Computer AT Technical Reference (1984)](http://bitsavers.org/pdf/ibm/pc/at/1502494_PC_AT_Technical_Reference_Mar84.pdf)
- [tmk Keyboard Wiki - XT Keyboard Protocol](https://github.com/tmk/tmk_keyboard/wiki/IBM-PC-XT-Keyboard-Protocol)
- [IBM Model F XT - VintagePC](https://www.seasip.info/VintagePC/ibm_1501105.html)
---

## Related Documentation

- [XT Protocol](../protocols/xt.md) - Protocol specification using Scancode Set 1
- [Protocols Overview](../protocols/README.md) - All supported protocols
- [Architecture](../advanced/README.md) - System architecture and scancode processing

---

**Questions or stuck on something?**  
Pop into [GitHub Discussions](https://github.com/PaulW/rp2040-keyboard-converter/discussions) or [report a bug](https://github.com/PaulW/rp2040-keyboard-converter/issues) if you've found an issue.
