# Commodore Amiga Scancode Implementation

## Overview

This directory contains the scancode processor for Commodore Amiga keyboards (A1000, A500, A2000, A3000, A4000). The Amiga protocol uses a simple make/break encoding with one unique exception: the CAPS LOCK key.

## Protocol Characteristics

### Standard Key Encoding

All keys (except CAPS LOCK) use simple make/break encoding:

```
Bit 7: Key state
  0 = Key pressed (make code)
  1 = Key released (break code)

Bits 6-0: Key code (0x00-0x67)
  104 possible key positions
```

**Examples**:
- Space (0x40) pressed: `0x40` (bit 7 = 0)
- Space (0x40) released: `0xC0` (bit 7 = 1)
- Left Shift (0x60) pressed: `0x60`
- Left Shift (0x60) released: `0xE0`

### CAPS LOCK Special Behavior

The Amiga CAPS LOCK key (scancode 0x62) has unique behavior that differs from all other keyboard protocols:

**Keyboard Behavior**:
1. **Only sends on press** (never on release)
2. Keyboard maintains internal LED state
3. LED toggles on each press
4. **Bit 7 indicates NEW LED state** after toggle (not press/release):
   - `0x62` (bit 7 = 0): LED now **ON** (was OFF, pressed → turned ON)
   - `0xE2` (bit 7 = 1): LED now **OFF** (was ON, pressed → turned OFF)

**Our Implementation**:
- **CAPS LOCK handling is done in the protocol layer** (`keyboard_interface.c`), NOT in this scancode processor
- Protocol layer detects CAPS LOCK by key code (0x62)
- **Smart synchronisation**: Compares keyboard LED state (from bit 7) with USB HID CAPS LOCK state
- **Only toggles if states differ**: Prevents unnecessary HID events and handles desync gracefully
- Generates press+release pair with configurable hold time (default 125ms, set via `CAPS_LOCK_TOGGLE_TIME_MS` in `config.h`)
- This scancode processor receives normal make/break codes and processes them uniformly

**Why This Design?**:

The Amiga keyboard maintains its own CAPS LOCK LED state internally. The computer cannot control Amiga keyboard LEDs (unlike PC keyboards with LED control commands). By sending the LED state instead of press/release, the keyboard ensures the computer knows the current LED state after each toggle.

USB HID CAPS LOCK is a **toggle key**: each press+release cycle toggles the state. The protocol layer implements smart synchronisation that only sends toggle events when keyboard LED and USB HID states differ, handling scenarios like:
- Normal operation (both states match)
- Desync after converter reboot (keyboard powered, USB reset)
- Rapid presses before USB completes toggle

**Example Sequence**:
```
Initial state: LED OFF, USB CAPS OFF

User presses CAPS LOCK (first time):
  Keyboard: Toggles LED OFF→ON, sends 0x62 (bit 7=0, "LED now ON")
  Protocol: Check states - LED=ON, USB=OFF → Different, send toggle
  Scancode: Receives 0x62 (press) and 0xE2 (release after hold time)
  Result: Both keyboard LED and USB CAPS are ON ✓

User presses CAPS LOCK (second time):
  Keyboard: Toggles LED ON→OFF, sends 0xE2 (bit 7=1, "LED now OFF")
  Protocol: Check states - LED=OFF, USB=ON → Different, send toggle
  Scancode: Receives 0x62 (press) and 0xE2 (release after hold time)
  Result: Both keyboard LED and USB CAPS are OFF ✓

Reboot scenario (keyboard stays powered):
  Before: LED ON, USB ON (synchronised)
  After reboot: LED ON (unchanged), USB OFF (reset)
  User presses CAPS LOCK:
    Keyboard: Toggles LED ON→OFF, sends 0xE2 (bit 7=1, "LED now OFF")
    Protocol: Check states - LED=OFF, USB=OFF → Same, skip toggle!
    Result: Both OFF, automatically resynchronised ✓
```

## Key Code Range

Valid Amiga key codes:
- **0x00-0x67**: Valid key positions (104 keys total)
- **0x68-0x77**: Reserved/invalid
- **0x78**: Reset warning (handled by protocol layer)
- **0x79-0xF8**: Reserved/invalid
- **0xF9-0xFE**: Special protocol codes (handled by protocol layer)

## Special Protocol Codes

These codes are **NOT** processed by the scancode layer (filtered by `keyboard_interface.c`):

| Code | Name | Description |
|------|------|-------------|
| `0x78` | Reset Warning | CTRL + both Amiga keys pressed |
| `0xF9` | Lost Sync | Previous transmission failed, retransmitting |
| `0xFA` | Buffer Overflow | Keyboard buffer full |
| `0xFC` | Self-Test Fail | Keyboard self-test failed on power-up |
| `0xFD` | Powerup Start | Beginning of power-up key stream |
| `0xFE` | Powerup End | End of power-up key stream |

## Data Flow

```
Keyboard → Protocol Layer → Scancode Layer → HID Layer
   ↓            ↓                 ↓              ↓
Raw bits    De-rotate        Extract         Keymap
            Invert          key code        lookup
            Filter          Handle          Generate
            special         CAPS LOCK       USB HID
            codes                           report
```

## Implementation Details

### Function: `process_scancode(uint8_t code)`

**Input**: 8-bit scancode (after de-rotation and active-low inversion)
- Bit 7: State flag (0=press, 1=release) OR LED state for CAPS LOCK
- Bits 6-0: Key code (0x00-0x67)

**Processing**:
1. Extract key code (bits 6-0)
2. Extract make/break flag (bit 7)
3. Validate key code range (0x00-0x67)
4. Special case: CAPS LOCK (0x62)
   - Generate press+release pair (ignore bit 7)
   - USB HID toggles on each press+release cycle
   - Keeps keyboard LED and USB CAPS LOCK synchronised
5. Normal keys: Pass to HID layer with make/break flag

**Output**: Calls `handle_keyboard_report(key_code, make)` for HID processing

### Validation

Key codes > 0x67 are logged as errors and ignored (defensive programming):
```c
if (key_code > AMIGA_MAX_KEYCODE) {
  LOG_ERROR("Invalid Amiga key code: 0x%02X (raw scancode: 0x%02X)\n", key_code, code);
  return;
}
```

## Modifier Keys

Amiga keyboards have independent matrix positions for all modifiers:

| Modifier | Make Code | Break Code | Notes |
|----------|-----------|------------|-------|
| Left Shift | `0x60` | `0xE0` | Independent matrix position |
| Right Shift | `0x61` | `0xE1` | Independent matrix position |
| Control | `0x63` | `0xE3` | Only one Control key |
| Left Alt | `0x64` | `0xE4` | Independent matrix position |
| Right Alt | `0x65` | `0xE5` | Independent matrix position |
| Left Amiga | `0x66` | `0xE6` | Maps to Left Windows/Command |
| Right Amiga | `0x67` | `0xE7` | Maps to Right Windows/Command |
| CAPS LOCK | `0x62` or `0xE2` | None | Special behavior (see above) |

## Comparison with Other Protocols

### vs. PC/XT (Set 1)
- **Simpler**: No E0/E1 prefix codes
- **Simpler**: No fake shifts
- **Different**: CAPS LOCK special behavior
- **Similar**: Basic make/break with bit 7 flag

### vs. Apple M0110
- **Similar**: Simple make/break encoding
- **Similar**: No multi-byte sequences
- **Different**: M0110 uses bits 6-1 for keycode, bit 0 always 1
- **Different**: CAPS LOCK behavior (M0110 is normal key)

### vs. PS/2 (Set 2)
- **Simpler**: No multi-byte sequences
- **Simpler**: No typematic control
- **Different**: Make code = base, break code = base + 0x80 (vs. PS/2 F0 prefix)
- **Different**: CAPS LOCK special behavior

## Testing Considerations

### Normal Keys
- Test all 104 key positions
- Verify make codes (0x00-0x67)
- Verify break codes (0x80-0xE7)
- Test rapid press/release sequences

### CAPS LOCK Testing
- **Critical**: Test multiple presses in sequence
- Verify LED state starts OFF on power-up
- Verify LED toggles on each press
- Verify computer CAPS LOCK state syncs with keyboard LED
- Test: Press 10 times rapidly - should end at same state it started

### Special Cases
- Test all modifier keys independently
- Test modifier combinations (Shift+Key, Ctrl+Alt+Del equivalent)
- Test rapid typing (verify no buffer overflows)
- Test key rollover (multiple simultaneous keys)

### Error Cases
- Invalid key codes (> 0x67) should be logged and ignored
- Buffer overflow protection
- Special protocol codes should not reach scancode layer

## Source Code

- **Protocol Implementation**: [`src/protocols/amiga/keyboard_interface.c`](../../src/protocols/amiga/keyboard_interface.c)
- **Protocol Header**: [`src/protocols/amiga/keyboard_interface.h`](../../src/protocols/amiga/keyboard_interface.h)
- **Keyboard Layout**: [`src/keyboards/amiga/a500/keyboard.c`](../../src/keyboards/amiga/a500/keyboard.c)
- **HID Interface**: [`src/common/lib/hid_interface.c`](../../src/common/lib/hid_interface.c)
- **HID Keycodes**: [`src/common/lib/hid_keycodes.h`](../../src/common/lib/hid_keycodes.h)

## References

- **Amiga Hardware Reference Manual**: Appendix H (Keyboard protocol)
- **[Amiga Protocol Documentation](../protocols/amiga.md)** - Complete protocol specification

## Notes

- **No LED Control**: Computer cannot control Amiga keyboard LEDs
- **No Typematic**: Amiga keyboards don't support programmable repeat rates
- **No Configuration**: No keyboard configuration commands (unlike PS/2)
- **Hot-Swap**: Not supported (requires power cycle)
- **CAPS LOCK LED**: Maintained internally by keyboard, informational only

---

## Related Documentation

- [Amiga Protocol](../protocols/amiga.md) - Protocol specification using Amiga scancodes
- [Protocols Overview](../protocols/README.md) - All supported protocols  
- [Architecture](../advanced/README.md) - System architecture and scancode processing

---

**Questions or stuck on something?**  
Pop into [GitHub Discussions](https://github.com/PaulW/rp2040-keyboard-converter/discussions) or [report a bug](https://github.com/PaulW/rp2040-keyboard-converter/issues) if you've found an issue.
