# Scancode Set 3 (Terminal)

Scancode Set 3 is what IBM designed for terminal keyboards—a clean-sheet design that represents what happens when you build a scancode set from scratch without worrying about backward compatibility. It's logical and consistent—every key has a single-byte make code, break codes are just F0 followed by the make code, and there are no extended prefixes cluttering things up.

The state machine is refreshingly simple: just two states versus Set 2's nine. The Pause key is finally treated like a normal key instead of requiring a bizarre multi-byte sequence. No fake shifts to filter out, no special cases to handle. It's genuinely elegant.

However, Set 3 sees limited use. It's found on 122-key terminal keyboards and some industrial equipment. Standard desktop keyboards continue to use Set 2, so Set 3 remains a niche option despite being well-designed.

## Encoding Scheme

### Basic Encoding

Set 3 uses the cleanest encoding of all three sets:
- **Make codes**: Single byte (0x01-0x84)
- **Break codes**: Two bytes (`F0` + make code)
- **No prefixes**: No E0 or E1 extended sequences
- **One-to-one mapping**: Each physical key has exactly one scancode

### Make and Break Codes

| Event | Encoding | Example (A key = 0x1C) |
|-------|----------|------------------------|
| **Key Press (Make)** | `code` | `1C` |
| **Key Release (Break)** | `F0 code` | `F0 1C` |

This simple and consistent scheme:
- Eliminates all special cases
- No extended key prefixes needed
- No fake shift sequences
- Pure make/break encoding

### No Extended Keys

Set 3's design philosophy eliminated the need for extended key prefixes entirely. Rather than using E0 and E1 prefixes to create separate namespaces like Sets 1 and 2, Set 3 assigns unique single-byte codes to all keys from the outset. This means keys that require multi-byte sequences in other sets—Right Alt, Right Control, arrow keys, navigation keys—all receive their own dedicated scancodes in the base set.

**Key Feature**: Set 3 has no E0 or E1 prefixes!

All keys, including those that require E0 in Sets 1 and 2, have direct single-byte codes:
- Right Alt: `39` (not `E0 11`)
- Right Control: `58` (not `E0 14`)
- Arrow keys: Dedicated single-byte codes
- All keys: Single unique scancode

### Pause/Break Key

Set 3's treatment of Pause/Break demonstrates its cleaner design philosophy. Where Sets 1 and 2 require special multi-byte sequences (6 and 8 bytes respectively), Set 3 treats Pause/Break as an ordinary key with standard make and break codes. This eliminates the special-case logic required in other scancode sets.

Even Pause/Break is simplified:

| Event | Sequence |
|-------|----------|
| **Pause** | `62` (press), `F0 62` (release) |

Unlike Sets 1 and 2, Pause is finally just a regular key with normal make/break codes—no 6-byte or 8-byte sequences required.

### No Fake Shifts

**Set 3 eliminates fake shift codes entirely.**

Print Screen is a regular key:
```
Press:   57
Release: F0 57
```

No compatibility sequences, no filtering required.

## State Machine

Set 3 requires only a **2-state** state machine, the simplest of all sets:

```
┌─────────────┐
│    INIT     │ ◄───────────┐
└─────┬───────┘             │
      │                     │
      ├─[F0]──► F0 ──[code]─┤
      │                     │
      └─[code]──────────────┘
```

### State Descriptions

| State | Description | Next Action |
|-------|-------------|-------------|
| **INIT** | Waiting for scancode | Process make or enter F0 state |
| **F0** | F0 (break) prefix received | Process key release |

That's it. No complex sequences, no special cases.

## Key Characteristics

**What works brilliantly:**

The state machine is about as simple as it gets—just two states. The keyspace is the largest of the three sets, with room for 256 possible codes (though only 0x01-0x84 are currently defined). Every key uses a single-byte make code—no extended prefixes needed. There's no compatibility baggage like fake shifts or special sequences. All keys follow the same make/break pattern consistently. The scancodes are organised systematically rather than historically. It's a clean design that wasn't constrained by backward compatibility requirements.

**Where it falls short:**

Set 3 usage is limited to terminal keyboards and industrial equipment, which means support is inconsistent. BIOS implementations vary in Set 3 support. You can't use it with the XT protocol—it requires the bidirectional AT/PS2 protocol. Terminal keyboards need to be explicitly configured for make/break mode (command 0xF8), which adds an initialisation step.  

## Code Range

| Range | Usage |
|-------|-------|
| `0x01-0x07` | Function keys F1-F7 |
| `0x08-0x0E` | Main keyboard special keys |
| `0x0F-0x6F` | Main keyboard alphanumeric and symbols |
| `0x70-0x7E` | Numeric keypad |
| `0x7F-0x84` | Special keys (F11, F12, extras) |
| `0x85+` | Reserved for future use |

## Special Codes

| Code | Description |
|------|-------------|
| `0x00` | Reserved/Error |
| `0x7C` | Keypad Comma (122-key terminals) |
| `0x83` | F7 key |
| `0x84` | Keypad Plus (122-key terminals) / SysRq on early 84-key AT |
| `0xAA` | Self-test passed (BAT) |
| `0xF0` | Break (release) prefix |
| `0xFA` | Acknowledge (ACK) |
| `0xFC` | Self-test failed |
| `0xFD` | Internal failure |
| `0xFE` | Resend request |
| `0xFF` | Error/Buffer overflow |

**Note**: Set 3 has no `E0` or `E1` codes. Those bytes are not used.

**Self-Test Codes (`0xAA`, `0xFC`):** These keyboard initialisation codes are filtered by the protocol layer during initialisation. The scancode processor does not filter these codes, so post‑initialisation filtering relies on the protocol layer. Unlike Set 1, Set 3 has no collision issue since break codes use the F0 prefix. See [Set 1 Self-Test Code Collision](set1.md#self-test-code-collision) for comparison.

## Example Sequences

### Regular Key (A)
```
Press:   1C
Release: F0 1C
```

### Previously Extended Key (Right Arrow)
```
Press:   6A    (single byte, not E0 6A!)
Release: F0 6A
```

### Print Screen (no fake shifts!)
```
Press:   57
Release: F0 57
```

### Pause/Break (finally simple!)
```
Pause Press:     62
Pause Release:   F0 62
Break Press:     62    (Ctrl+Pause, same key)
Break Release:   F0 62
```

## Implementation Notes

### Processing Algorithm

```c
// Set 3 processing - remarkably simple!
switch (state) {
    case INIT:
        if (code == 0xF0) {
            state = F0;
        } else {
            handle_key(code, true);  // Make
        }
        break;
    
    case F0:
        handle_key(code, false);  // Break
        state = INIT;
        break;
}
```

That's the complete state machine. There are no special cases to handle, no filtering required, and no extended keys to deal with.

### Translation Requirements

Set 3 requires a translation table to map scancodes to HID usage codes, but:
- No E0 table needed (all keys are direct)
- No fake shift filtering needed
- No special Pause/Break handling needed
- Simple one-to-one mapping

### Make/Break Mode Configuration

**Critical**: Terminal keyboards must be explicitly set to make/break mode!

By default, terminal keyboards operate in "make-only" mode (only send key press events). The host must send command `0xF8` to enable full make/break operation:

```
Host sends: F8  (Set All Keys to Make/Break)
Keyboard ACKs: FA
```

Without this, key releases won't be reported, and all keys will appear stuck.

### Host Commands

Set 3 supports all AT/PS2 commands:

| Command | Value | Description |
|---------|-------|-------------|
| **Set LEDs** | `0xED` | Control Caps/Num/Scroll Lock LEDs |
| **Echo** | `0xEE` | Echo test |
| **Get/Set Scancode Set** | `0xF0` | Query or change scancode set |
| **Identify** | `0xF2` | Request keyboard ID |
| **Set Typematic** | `0xF3` | Set repeat rate and delay |
| **Enable** | `0xF4` | Enable scanning |
| **Disable** | `0xF5` | Disable scanning |
| **Set Default** | `0xF6` | Reset to defaults |
| **Set All Keys Make/Break** | `0xF8` | **Essential for Set 3!** |
| **Reset** | `0xFF` | Self-test and reset |

## Key Layout

Set 3 groups scancodes by key ranges, as shown below:

### Function Keys (0x01-0x0E)
```
F1: 07    F2: 0F    F3: 17    F4: 1F    F5: 27    F6: 2F
F7: 37    F8: 3F    F9: 47    F10: 4F   F11: 56   F12: 5E
```

### Main Keyboard
```
Esc: 08    1: 16    2: 1E    3: 26    ...
Tab: 0D    Q: 15    W: 1D    E: 24    ...
Caps: 14   A: 1C    S: 1B    D: 23    ...
LShift: 12 Z: 1A    X: 22    C: 21    ...
```

### Navigation (No E0 prefix!)
```
Insert: 67   Home: 6E   PgUp: 6F
Delete: 64   End:  65   PgDn: 6D
Up: 63       Down: 60   Left: 61   Right: 6A
```

### Numeric Keypad
```
NumLock: 76   /: 77    *: 7E    ,: 84
7: 6C         8: 75    9: 7D    -: 7C
4: 6B         5: 73    6: 74    +: 7B
1: 69         2: 72    3: 7A    Enter: 79
0: 70                  .: 71
```

**Note**: Keypad layout may vary by keyboard model. The mappings above reflect terminal keyboard configurations.

## Performance Characteristics

- **Average bytes per keystroke**: 2.0 (1 make + 1 break, consistent)
- **State machine complexity**: Lowest (2 states)
- **Processing overhead**: Minimal (simple state machine)
- **Memory requirements**: ~256 bytes for translation table (no E0 table)
- **Code efficiency**: Highest (no special case handling)

## Source Code

- **[`scancode.h`](../../src/scancodes/set3/scancode.h)** - Set 3 processor interface
- **[`scancode.c`](../../src/scancodes/set3/scancode.c)** - State machine implementation
- **[Unified Processor](../../src/scancodes/set123/)** - Multi-set processor including Set 3

## Keyboards Using Set 3

### Terminal Keyboards
- IBM Model M 122-key (Part 1395660)
- IBM Model M 122-key with TrackPoint (Part 42H1292)
- MicroSwitch 122ST13 (Terminal keyboard)
- Unicomp Ultra Classic 122-key

### Detection Method

Terminal keyboards identify with specific ID ranges (based on tmk reference):
```
0xAB86-0xAB92: Known terminal keyboards (Cherry G80-2551, IBM 1397000, IBM 5576)
0xBF00-0xBFFF: Type 2 terminal keyboards (IBM 1390876, 6110344, RT keyboards)
0x7F00-0x7FFF: Type 1 terminal keyboards (IBM 1394204)
```

**Not terminal keyboards:**
```
0xAB84: Short keyboards (ThinkPad, Spacesaver) - use Set 2
0xAB85: Mixed models (some Set 2, some Set 3)
```

Keyboards with terminal IDs should:
1. Be configured for Set 3
2. Receive `0xF8` command (Set All Keys Make/Break)
3. Process single-byte scancodes without E0/E1 logic

## Keyboard Identification

Set 3 keyboards respond to `0xF2` (Identify) command with 2-byte ID.

**Source**: [tmk Keyboard Wiki - AT Keyboard Protocol](https://github.com/tmk/tmk_keyboard/wiki/IBM-PC-AT-Keyboard-Protocol#keyboard-id)

### Keyboards That Support Set 3

| ID (hex) | Description | Default Set | Notes |
|----------|-------------|-------------|-------|
| `AB 86` | Terminal keyboards | **Set 2** | Supports switching to Set 3 via F0 03 |
|  | - Cherry G80-2551 126-key | | Starts in Set 2, switchable to Set 3 |
|  | - IBM 1397000 | | |
|  | - Affirmative 1227T | | |
|  | - Unicomp UB40856 | | |
|  | - Other 122-key terminals | | |
| `AB 90` | IBM 5576-002 KEYBOARD-2 | **Set 2** | Supports switching to Set 3 |
|  | - Part number 94X1110 | | Japanese keyboard |
| `AB 91` | IBM 5576-003 | **Set 1** | Supports switching to Set 3 |
|  | Televideo 990/995 DEC style | **Set 1** | Supports Sets 1, 2, 3 |
| `AB 92` | IBM 5576-001 | **Set 2** | Supports switching to Set 3 |
|  |  | | Supports Set 3 and Code Set 82h |
| `BF BF` | IBM Terminal 122-key | **Set 3** | Default is Set 3 |
|  | - IBM 1390876, 6110344 | | ID configurable via DIP switches/jumpers |
| `BF B0` | IBM RT keyboard (US layout) | **Set 3** | Default is Set 3 |
|  |  | | Num Lock and Caps Lock LEDs swapped |
| `BF B1` | IBM RT keyboard (ISO layout) | **Set 3** | Default is Set 3 |
| `7F 7F` | IBM Terminal 101-key | **Set 3** | Always Set 3 |
|  | - IBM 1394204 | | ID not configurable |
|  |  | | Doesn't support F0 command |

### Important Notes

- **Keyboards starting with 0xAB8x**: Default to Set 2, but support switching to Set 3
- **Keyboards starting with 0xBFxx or 0x7Fxx**: Default to Set 3
- **This converter**: DETECTS the current scancode set based on keyboard ID. It does NOT send `F0 03` commands to switch scancode sets.
- **Make/Break Mode**: For keyboards detected as Set 3, the converter sends `0xF8` (Set All Keys Make/Break) command

### Non-Set 3 Keyboards (For Reference)

| ID (hex) | Description | Default Set | Notes |
|----------|-------------|-------------|-------|
| `AB 83` | Standard PS/2 keyboards | Set 2 | Standard ID, stays in Set 2 |
| `AB 84` | Short keyboards (tenkeyless) | Set 2 | ThinkPad, Spacesaver models |
| `AB 85` | Mixed models | Set 2 or 3 | NCD N-97 speaks Set 3, IBM 122-key M varies |

## Protocol Notes

### Initialisation Sequence

Generic Set 3 initialisation procedure:
```
1. Wait for self-test (0xAA)
2. Send 0xF2 (Identify)
3. Read 2-byte ID
4. If ID is terminal (0xAB86-0xAB92, 0xBFxx, or 0x7Fxx):
   - [Optional] Send 0xF0 0x03 (switch to Set 3)
   - [Optional] Wait for ACK (0xFA)
   - Send 0xF8 (Set All Keys Make/Break)
   - Wait for ACK (0xFA)
5. Keyboard is ready
```

**Note about this converter**: This implementation **auto-detects** the keyboard's current scancode set based on its ID and does NOT send the `F0 03` command to force-switch scancode sets. Keyboards with IDs 0xBFxx and 0x7Fxx default to Set 3 at power-up, whilst 0xAB86-0xAB92 keyboards boot in Set 2. The converter configures itself to match the keyboard's native set, eliminating the need for set-switching commands.

### Typematic Control

Like Set 2, Set 3 supports typematic configuration:
- Repeat rate: 2-30 CPS
- Delay: 250ms-1000ms
- Configured via `0xF3` + parameter byte

### LED Control

Standard LED control via `0xED` command:
- Bit 0: Scroll Lock
- Bit 1: Num Lock
- Bit 2: Caps Lock

## Why Set 3 Wasn't Widely Adopted

Despite being the cleanest design:

1. **Late introduction**: Sets 1 and 2 were already entrenched by 1987
2. **BIOS support**: BIOS implementations vary in support for Set 3 (Sets 1 and 2 have broader support)
3. **Limited hardware**: Only terminal and specialised keyboards used it
4. **Backward compatibility**: Standard keyboards stayed with Set 2
5. **Cost**: No compelling reason to switch for consumer keyboards

## Modern Usage

Set 3 is ideal for:
- Custom keyboard converters (this project!)
- Embedded systems with keyboard interfaces
- Clean-slate implementations without legacy baggage
- Educational purposes (easiest to teach and understand)

## Comparison with Other Sets

| Feature | Set 1 | Set 2 | Set 3 |
|---------|-------|-------|-------|
| **State Machine** | 5 states | 9 states | **2 states** ✅ |
| **Extended Keys** | E0 prefix | E0 prefix | **None** ✅ |
| **Pause Complexity** | 6 bytes | 8 bytes | **2 bytes** ✅ |
| **Fake Shifts** | Yes (4 codes) | Yes (4 codes) | **None** ✅ |
| **Break Encoding** | High bit | F0 prefix | F0 prefix ✅ |
| **Keyspace** | 7-bit (128) | 8-bit (256) | **8-bit (256)** ✅ |
| **Consistency** | Medium | Medium | **High** ✅ |
| **Adoption** | XT keyboards | **Universal** ✅ | Terminal only |

## References

- [tmk Keyboard Wiki - AT Keyboard Protocol](https://github.com/tmk/tmk_keyboard/wiki/IBM-PC-AT-Keyboard-Protocol)
- [IBM Terminal Keyboard Technical Reference](http://www.mcamafia.de/pdf/ibm_hitrc11.pdf)
- [IBM 6110344 Terminal 122-key - VintagePC](https://www.seasip.info/VintagePC/ibm_6110344.html)
- [IBM 1390876 Terminal 122-key - VintagePC](https://www.seasip.info/VintagePC/ibm_1390876.html)
- [IBM 1397000 Keyboard - VintagePC](http://www.seasip.info/VintagePC/ibm_1397000.html)
- [Cherry G80-2551 Terminal Keyboard](https://deskthority.net/wiki/Cherry_G80-2551)
---

## Related Documentation

- [Scancode Set 2](set2.md) - AT/PS2 default scancode set
- [Protocols Overview](../protocols/README.md) - All supported protocols
- [Architecture](../advanced/README.md) - System architecture and scancode processing

---

**Questions or stuck on something?**  
Pop into [GitHub Discussions](https://github.com/PaulW/rp2040-keyboard-converter/discussions) or [report a bug](https://github.com/PaulW/rp2040-keyboard-converter/issues) if you've found an issue.
