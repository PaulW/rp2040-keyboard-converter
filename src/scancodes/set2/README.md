# Scancode Set 2 (AT/PS2) - Technical Documentation

## Overview

Scancode Set 2, also known as the **AT scancode set** or **PS/2 default set**, was introduced with the IBM PC/AT in 1984. It became the de facto standard for PS/2 keyboards and remains the most widely used scancode set today. Unlike Set 1, it uses a separate release prefix (F0) rather than embedding release information in the high bit.

## Historical Context

- **Introduced**: 1984 with IBM PC/AT (Model 5170)
- **Protocol**: AT/PS2 protocol (bidirectional, supports host commands)
- **Usage**: Default for all PS/2 keyboards, USB keyboards in BIOS compatibility mode
- **Status**: Current standard for most PC keyboards

## Encoding Scheme

### Basic Encoding

Set 2 uses a more sophisticated encoding:
- **Make codes**: Single byte (0x01-0x83)
- **Break codes**: Two bytes (`F0` + make code)
- No high-bit encoding for release

### Make and Break Codes

| Event | Encoding | Example (A key = 0x1C) |
|-------|----------|------------------------|
| **Key Press (Make)** | `code` | `1C` |
| **Key Release (Break)** | `F0 code` | `F0 1C` |

This scheme provides:
- Clear separation between make and break events
- Full 8-bit keyspace for make codes
- Consistent two-byte break sequences

### Extended Keys (E0 Prefix)

Extended keys use a multi-byte sequence:

| Event | Encoding | Example (Right Control = 0x14) |
|-------|----------|--------------------------------|
| **Extended Make** | `E0 code` | `E0 14` |
| **Extended Break** | `E0 F0 code` | `E0 F0 14` |

Extended keys include:
- Right Alt (AltGr): `E0 11`
- Right Control: `E0 14`
- Arrow keys: `E0 75` (Up), `E0 72` (Down), `E0 6B` (Left), `E0 74` (Right)
- Navigation: Insert, Delete, Home, End, Page Up, Page Down
- Windows keys: `E0 1F` (Left Win), `E0 27` (Right Win)
- Application key: `E0 2F`
- Multimedia keys: Various `E0` sequences
- Keypad `/`: `E0 4A`

### Pause/Break Key (E1 Prefix)

The Pause/Break key uses the most complex sequence in Set 2:

| Event | Sequence |
|-------|----------|
| **Pause** | `E1 14 77 E1 F0 14 F0 77` (8 bytes!) |
| **Break** (Ctrl+Pause) | `E0 7E E0 F0 7E` |
| **Unicomp Pause** | `E0 77 E0 F0 77` (Unicomp New Model M variant) |

**Important**: 
- Pause sends the full 8-byte sequence on press only
- No separate release event for Pause
- Break (Ctrl+Pause) is a different 4-byte sequence (E0 7E)
- Some keyboards (Unicomp) use E0 77 for Pause instead of E1 sequence

**USB HID Mapping:**
- All Pause variants map to **interface code 0x48** (USB HID Pause/Break key)
- E1 sequences (regular Pause): `E1 14 77` → 0x48
- E0 7E (Ctrl+Pause): `E0 7E` → 0x48 (with Ctrl modifier)
- E0 77 (Unicomp): `E0 77` → 0x48
- See Issue #21 for historical context on E0 mapping fixes
- Note: Some very old or non-compliant keyboards may not emit the E1 prefix for Pause
  (they instead send a bare sequence that looks like Ctrl+NumLock). Such keyboards cannot
  be reliably distinguished from intentional Ctrl+key combinations in firmware without
  introducing fragile heuristics that break other combinations. Recommended workaround:
  remap an unused physical key (for example Scroll Lock) to PAUS in your keyboard
  configuration if you need a dedicated Pause key for a non-compliant keyboard.

### Fake Shift Codes

Set 2 includes fake shift codes for Print Screen compatibility:

| Sequence | Description |
|----------|-------------|
| `E0 12 E0 7C` | Print Screen press (fake left shift + actual) |
| `E0 F0 7C E0 F0 12` | Print Screen release |

**Fake shift bytes to ignore:**
- `E0 12` - Fake Left Shift press
- `E0 F0 12` - Fake Left Shift release
- `E0 59` - Fake Right Shift press (rare)
- `E0 F0 59` - Fake Right Shift release (rare)

## State Machine

Set 2 requires a **9-state** state machine, the most complex of the three sets:

```
┌─────────────┐
│    INIT     │ ◄─────────────────────────────┐
└─────┬───────┘                               │
      │                                       │
      ├─[F0]──► F0 ──[code]──────────────────┤
      │                                       │
      ├─[E0]──► E0 ──┬─[F0]──► E0_F0 ──[code]┤
      │              │                        │
      │              └─[code]─────────────────┤
      │                                       │
      ├─[E1]──► E1 ──[14]──► E1_14 ──[77]──► E1_14_77 ──[E1]──► E1_PAUSE ──[F0]──► E1_F0 ──[14]──► E1_F0_14 ──[F0]──► E1_F0_14_F0 ──[77]───┐
      │                                                                                                                                          │
      └─[code]────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┘
```

### State Descriptions

| State | Description | Next Action |
|-------|-------------|-------------|
| **INIT** | Waiting for scancode | Process code or enter prefix state |
| **F0** | F0 (break) prefix received | Process key release |
| **E0** | E0 (extended) prefix received | Extended key or break |
| **E0_F0** | E0 F0 sequence received | Extended key release |
| **E1** | E1 (Pause) prefix received | Begin Pause sequence |
| **E1_14** | E1 14 received | Expect 77 |
| **E1_14_77** | E1 14 77 received | Expect E1 |
| **E1_PAUSE** | E1 14 77 E1 received | Expect F0 |
| **E1_F0** | E1 14 77 E1 F0 received | Expect 14 |
| **E1_F0_14** | E1 14 77 E1 F0 14 received | Expect F0 |
| **E1_F0_14_F0** | E1 14 77 E1 F0 14 F0 received | Expect 77, complete |

## Key Characteristics

### Advantages

✅ **Large keyspace**: Full 8-bit make codes (256 keys possible)  
✅ **Consistent structure**: All breaks are `F0 + make`  
✅ **Clear semantics**: Separate prefix bytes for extended/break  
✅ **Industry standard**: Supported by virtually all PC keyboards  
✅ **Bidirectional**: Supports host commands (LEDs, typematic, etc.)  

### Disadvantages

❌ **Complex state machine**: 9 states required (most complex)  
❌ **Verbose**: Minimum 2 bytes per key (make + break)  
❌ **Fake shifts**: Still includes compatibility codes  
❌ **Pause complexity**: 8-byte sequence for one key  
❌ **Slightly larger**: ~300 bytes for translation tables  

## Code Range

| Range | Usage |
|-------|-------|
| `0x01-0x07` | F keys (F7-F12, partial) |
| `0x08-0x0E` | F keys and special |
| `0x0F-0x7E` | Main keyboard keys |
| `0x7F-0x83` | Extended keys, special codes |
| `0x84+` | Reserved |

## Special Codes

| Code | Description |
|------|-------------|
| `0x00` | Error/Overrun |
| `0x83` | F7 key |
| `0x84` | Alt Print Screen (some keyboards) |
| `0xAA` | Self-test passed (BAT) |
| `0xE0` | Extended key prefix |
| `0xE1` | Pause/Break prefix |
| `0xEE` | Echo response |
| `0xF0` | Break (release) prefix |
| `0xFA` | Acknowledge (ACK) |
| `0xFC` | Self-test failed |
| `0xFD` | Internal failure |
| `0xFE` | Resend request |
| `0xFF` | Error/Buffer overflow |

## Example Sequences

### Regular Key (A)
```
Press:   1C
Release: F0 1C
```

### Extended Key (Right Arrow)
```
Press:   E0 74
Release: E0 F0 74
```

### Print Screen (with fake shifts)
```
Press:   E0 12 E0 7C  (filter E0 12, process E0 7C)
Release: E0 F0 7C E0 F0 12  (process E0 F0 7C, filter E0 F0 12)
```

### Pause/Break
```
Pause:   E1 14 77 E1 F0 14 F0 77  (8 bytes, press only, no release)
Break:   E0 7E E0 F0 7E            (Ctrl+Pause, separate 4-byte sequence)
```

### Multimedia Key (Volume Up, example)
```
Press:   E0 32
Release: E0 F0 32
```

## Implementation Notes

### Processing Algorithm

```c
// Simplified Set 2 processing
switch (state) {
    case INIT:
        if (code == 0xF0) {
            state = F0;
        } else if (code == 0xE0) {
            state = E0;
        } else if (code == 0xE1) {
            state = E1;
        } else {
            handle_key(code, true);  // Make
        }
        break;
    
    case F0:
        handle_key(code, false);  // Break
        state = INIT;
        break;
    
    case E0:
        if (code == 0xF0) {
            state = E0_F0;
        } else if (code != 0x12 && code != 0x59) {
            // Filter fake shifts
            uint8_t key = translate_e0_key(code);
            handle_key(key, true);  // Extended make
            state = INIT;
        } else {
            state = INIT;  // Filtered fake shift
        }
        break;
    
    case E0_F0:
        if (code != 0x12 && code != 0x59) {
            uint8_t key = translate_e0_key(code);
            handle_key(key, false);  // Extended break
        }
        state = INIT;
        break;
    
    case E1:
        // Pause sequence handling...
        break;
}
```

### Translation Requirements

E0-prefixed keys require translation:
- Map extended scancodes to logical key codes
- Distinguish numeric keypad from navigation cluster
- Handle multimedia and browser control keys
- Support ACPI power keys (some keyboards)

### Host Commands

Set 2 supports bidirectional communication:

| Command | Value | Description |
|---------|-------|-------------|
| **Set LEDs** | `0xED` | Control Caps/Num/Scroll Lock LEDs |
| **Echo** | `0xEE` | Echo test (keyboard responds with `0xEE`) |
| **Get/Set Scancode Set** | `0xF0` | Query or change scancode set |
| **Identify** | `0xF2` | Request keyboard ID (2 bytes) |
| **Set Typematic** | `0xF3` | Set repeat rate and delay |
| **Enable** | `0xF4` | Enable keyboard scanning |
| **Disable** | `0xF5` | Disable keyboard scanning |
| **Set Default** | `0xF6` | Reset to default parameters |
| **Reset** | `0xFF` | Keyboard self-test and reset |

### Typematic (Auto-Repeat)

Set 2 keyboards support configurable typematic:
- **Repeat rate**: 2-30 characters per second
- **Delay**: 250ms, 500ms, 750ms, or 1000ms
- Configured via `0xF3` command + parameter byte

## Performance Characteristics

- **Average bytes per keystroke**: 2.5 (1 make + 2 break on average)
- **State machine complexity**: High (9 states)
- **Processing overhead**: Medium (more state transitions)
- **Memory requirements**: ~300 bytes for E0 translation table

## Common Keyboards Using Set 2

- All standard PS/2 keyboards (1987-present)
- IBM Model M keyboards with AT/PS2 interface
- IBM Model F PC/AT keyboards (with timeout, default to Set 2)
- Modern USB keyboards in PS/2 compatibility mode
- Cherry MX keyboards with PS/2 interface
- Most mechanical keyboards with PS/2 option

## Keyboard Identification

Set 2 keyboards respond to `0xF2` (Identify) command with 2-byte ID.

**Source**: [tmk Keyboard Wiki - AT Keyboard Protocol](https://github.com/tmk/tmk_keyboard/wiki/IBM-PC-AT-Keyboard-Protocol#keyboard-id)

### Set 2 Keyboard IDs

| ID (hex) | Description | Models | Notes |
|----------|-------------|--------|-------|
| None | IBM PC/AT 84-key (Model F) | IBM 6450225 | Doesn't respond to 0xF2 |
| `AB 41` | MF2 keyboard (PS/2 standard) | Standard PS/2 keyboards | |
| `AB 83` | MF2 keyboard with translator | Most common PS/2 ID | Cherry G80-3600, IBM 101, Topre Realforce, etc. |
| `AB 84` | Short keyboard (no numpad) | Tenkeyless, Spacesaver | ThinkPad keyboards, space-saving models |
| `AB 85` | Mixed models | NCD N-97, IBM 122-key Model M | NCD N-97 uses Set 3, IBM 122-key varies |

### Terminal Keyboard IDs (Support Set 3)

These keyboards support Set 3, but many default to Set 2 or Set 1:

| ID (hex) | Description | Default Set | Supports |
|----------|-------------|-------------|----------|
| `AB 86` | Terminal keyboards | Set 2 | Set 3 |
|  | - Cherry G80-2551 126-key | | |
|  | - IBM 1397000 | | |
|  | - Unicomp UB40856 | | |
|  | - Other 122-key terminals | | |
| `AB 90` | IBM 5576-002 (Japanese) | Set 2 | Set 3 |
| `AB 91` | IBM 5576-003 (Japanese) | Set 1 | Sets 2, 3 |
|  | Televideo 990/995 DEC style | Set 1 | Sets 2, 3 |
| `AB 92` | IBM 5576-001 (Japanese) | Set 2 | Set 3, 82h |
| `BF BF` | IBM Terminal 122-key | Set 3 | - |
|  | - IBM 1390876, 6110344 | | DIP switch configurable ID |
| `BF B0` | IBM RT keyboard (US) | Set 3 | - |
| `BF B1` | IBM RT keyboard (ISO) | Set 3 | - |
| `7F 7F` | IBM Terminal 101-key | Set 3 | - |
|  | - IBM 1394204 | | Always Set 3, doesn't support F0 |

**Important**: This converter DETECTS which scancode set these keyboards are currently using based on their ID. It does NOT send `F0 03` commands to switch scancode sets. For keyboards detected as Set 3, the converter only sends the `0xF8` command to configure make/break mode.


## Protocol Notes

### LED Control

Host can control LEDs via `0xED` command:
```
Host sends: ED
Keyboard ACKs: FA
Host sends: LED bitmap (bit 0=Scroll, bit 1=Num, bit 2=Caps)
Keyboard ACKs: FA
```

### Scancode Set Switching (Manual Commands)

Keyboards that support multiple scancode sets can be switched manually by the host:
```
Host sends: F0 01  (switch to Set 1)
Host sends: F0 02  (switch to Set 2)
Host sends: F0 03  (switch to Set 3)
```

**Note**: These are commands available to the host system. This converter does NOT send these commands automatically - it detects the keyboard's current scancode set based on the keyboard ID.


## References

- [IBM Personal Computer AT Technical Reference (1986)](http://bitsavers.org/pdf/ibm/pc/at/6183355_PC_AT_Technical_Reference_Mar86.pdf)
- [PS/2 Hardware Technical Reference (1990)](http://www.mcamafia.de/pdf/pdfref.htm)
- [tmk Keyboard Wiki - AT Keyboard Protocol](https://github.com/tmk/tmk_keyboard/wiki/IBM-PC-AT-Keyboard-Protocol)
- [Microsoft Keyboard Scan Code Specification](https://download.microsoft.com/download/1/6/1/161ba512-40e2-4cc9-843a-923143f3456c/scancode.pdf)
- [IBM Model F AT - VintagePC](https://www.seasip.info/VintagePC/ibm_6450225.html)