# Scancode Set 1 (XT) - Technical Documentation

## Overview

Scancode Set 1, also known as the **XT scancode set**, was introduced with the original IBM PC/XT in 1981. It is the oldest and most straightforward of the three major scancode sets, using a simple 7-bit encoding scheme with the high bit indicating key release.

## Historical Context

- **Introduced**: 1981 with IBM PC/XT (Model 5150)
- **Protocol**: XT protocol (unidirectional, keyboard to host only)
- **Usage**: IBM PC/XT keyboards, some early AT keyboards, modern mechanical keyboards
- **Legacy**: Still commonly used in BIOS implementations and some embedded systems

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

Extended keys (added with IBM AT keyboard) use a two-byte sequence:

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

**Important**: Pause sends the entire sequence on press only, with no separate release event.

### Fake Shift Codes

To maintain compatibility with XT hardware that expected Print Screen to involve the Shift key, Set 1 includes "fake shift" codes around Print Screen:

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

### Advantages

✅ **Simple decoding**: Break = Make + 0x80  
✅ **Efficient**: Most keys are single-byte  
✅ **Self-synchronizing**: Each byte has semantic meaning  
✅ **Wide compatibility**: Supported by all BIOS implementations  

### Disadvantages

❌ **Limited keyspace**: Only 128 unique keys (7-bit)  
❌ **Fake shifts**: Requires filtering of compatibility codes  
❌ **Complex special keys**: Pause/Break has irregular sequence  
❌ **No bidirectional**: XT protocol is keyboard-to-host only  

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
| `0xAA` | Self-test passed (BAT - Basic Assurance Test) |
| `0xE0` | Extended key prefix |
| `0xE1` | Pause/Break prefix |
| `0xEE` | Echo response (not used in XT) |
| `0xF0` | Reserved |
| `0xFA` | ACK (not used in XT) |
| `0xFC` | Self-test failed |
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
4. **Self-Test Only**: `0xAA` is sent on power-up, `0xFC` on failure

## Performance Characteristics

- **Average bytes per keystroke**: 1.5 (most keys are 1 byte, some are 2)
- **State machine complexity**: Medium (5 states)
- **Processing overhead**: Low (simple bit manipulation)
- **Memory requirements**: ~256 bytes for E0 translation table

## Common Keyboards Using Set 1

- IBM PC/XT keyboards (1981-1987)
- IBM Model F XT (Part 1501100, 1501105)
- Some early IBM Model M keyboards with XT interface
- Many modern mechanical keyboards in "XT mode"
- Cherry G80 series with XT interface

**Note**: XT keyboards don't respond to keyboard ID commands (0xF2) because the protocol is unidirectional (keyboard to host only).

## References

- [IBM PC/XT Technical Reference Manual (1983)](http://bitsavers.org/pdf/ibm/pc/xt/)
- [IBM Personal Computer AT Technical Reference (1984)](http://bitsavers.org/pdf/ibm/pc/at/1502494_PC_AT_Technical_Reference_Mar84.pdf)
- [tmk Keyboard Wiki - XT Keyboard Protocol](https://github.com/tmk/tmk_keyboard/wiki/IBM-PC-XT-Keyboard-Protocol)
- [IBM Model F XT - VintagePC](https://www.seasip.info/VintagePC/ibm_1501105.html)