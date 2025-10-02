# Unified Scancode Processor (Sets 1, 2, 3)

This folder contains a unified, configuration-driven scancode processor that handles all three XT/AT protocol scancode sets (1, 2, and 3).

## Purpose

The unified processor enables **flexible keyboard hot-swapping** on AT/PS2 protocol converters. Rather than rebuilding firmware when changing keyboards, the converter can automatically detect and adapt to different scancode sets at runtime.

**Key Use Case:** Swap between standard PS/2 keyboards (Set 2) and terminal keyboards (Set 3) without firmware changes.

## Overview

**Files:**
- `scancode.h` - API, configuration structures, and constants
- `scancode.c` - Unified state machine implementation
- `scancode_config.h` - Compile-time wrapper for XT protocol (Set 1 only)
- `scancode_runtime.h` - Runtime detection for AT/PS2 protocol (Set 2/3)

**Coexists With:**
- `set1/scancode.c` - Original Set 1 implementation (still functional)
- `set2/scancode.c` - Original Set 2 implementation (still functional)
- `set3/scancode.c` - Original Set 3 implementation (still functional)

The unified processor is an **adaptation for flexibility**, not a replacement. The original per-set implementations remain available and are perfectly valid choices for single-keyboard configurations.

## Quick Start

```c
#include "scancodes/set123/scancode.h"

void keyboard_interface_task(void) {
    uint8_t code;
    
    if (get_scancode_from_buffer(&code)) {
        // Choose your scancode set:
        process_scancode(code, &SCANCODE_CONFIG_SET2);
    }
}
```

## Configuration Constants

Three pre-configured constants are provided:

- **`SCANCODE_CONFIG_SET1`** - XT/Set 1 (break = code | 0x80)
- **`SCANCODE_CONFIG_SET2`** - PS/2 Set 2 (break = F0 + code)
- **`SCANCODE_CONFIG_SET3`** - Terminal Set 3 (break = F0 + code, no E0/E1)

## Features

✅ **Single implementation** for all three scancode sets  
✅ **Configuration-driven** behavior (set-specific logic via config struct)  
✅ **E0/E1 prefix handling** (extended keys, Pause/Break)  
✅ **Fake shift detection** (set-specific)  
✅ **Special code mapping** (F7, SysReq, Keypad Comma, etc.)  
✅ **No performance penalty** (lookup tables, inline functions)  

## Documentation

### Technical Scancode Set References

For detailed technical information about each scancode set:

- **[Set 1 (XT) Technical Documentation](../set1/README.md)** - Encoding, state machine, E0/E1 prefixes, fake shifts
- **[Set 2 (AT/PS2) Technical Documentation](../set2/README.md)** - F0 breaks, extended keys, host commands, LED control
- **[Set 3 (Terminal) Technical Documentation](../set3/README.md)** - Simplified design, no prefixes, make/break mode

## Scancode Set Comparison

For comprehensive technical details on each set's encoding, state machines, and special sequences, see the individual set documentation linked above.

| Feature | Set 1 | Set 2 | Set 3 |
|---------|-------|-------|-------|
| **Break Encoding** | code \| 0x80 | F0 + code | F0 + code |
| **E0 Prefix** | Yes (23 mappings) | Yes (38 mappings) | No |
| **E1 Prefix** | Yes (Pause) | Yes (Pause) | No |
| **Fake Shifts** | 4 codes | 2 codes | None |
| **State Complexity** | Medium (5 states) | High (9 states) | Low (2 states) |
| **Common Usage** | XT protocol | PS/2 default | Terminal KBs |
| **Documentation** | [Set 1 README](../set1/README.md) | [Set 2 README](../set2/README.md) | [Set 3 README](../set3/README.md) |

## Implementation Details

### State Machine

The unified processor uses 11 states to handle all three sets. For detailed state machine diagrams and explanations specific to each set, see:
- [Set 1 state machine](../set1/README.md#state-machine) (5 states)
- [Set 2 state machine](../set2/README.md#state-machine) (9 states)  
- [Set 3 state machine](../set3/README.md#state-machine) (2 states)

**Unified states:**
```
INIT            - Initial state / ready for next code
F0              - Break prefix (Set 2/3)
E0              - E0 prefix (Set 1/2)
E0_F0           - E0 F0 sequence (Set 2)
E1              - E1 prefix (Set 1/2)
E1_1D           - E1 1D (Set 1 Pause make)
E1_9D           - E1 9D (Set 1 Pause break)
E1_14           - E1 14 (Set 2 Pause make)
E1_F0           - E1 F0 (Set 2 Pause break)
E1_F0_14        - E1 F0 14 (Set 2 Pause break)
E1_F0_14_F0     - E1 F0 14 F0 (Set 2 Pause break)
```

### E0 Translation Tables

For complete E0 scancode mappings and extended key details:
- [Set 1 E0 codes](../set1/README.md#extended-keys-e0-prefix) - 23 mappings
- [Set 2 E0 codes](../set2/README.md#extended-keys-e0-prefix) - 38 mappings
- [Set 3](../set3/README.md#no-extended-keys) - No E0 prefix (all direct codes)

### Fake Shift Handling

For the history and rationale behind fake shift codes:
- [Set 1 fake shifts](../set1/README.md#fake-shift-codes) - E0 2A, AA, 36, B6 (Print Screen compatibility)
- [Set 2 fake shifts](../set2/README.md#fake-shift-codes) - E0 12, E0 F0 12, E0 59 (rare)
- [Set 3](../set3/README.md#no-fake-shifts) - None (eliminated entirely)

### Special Codes

Each set has unique special codes - see individual documentation:
- [Set 1 special codes](../set1/README.md#special-codes)
- [Set 2 special codes](../set2/README.md#special-codes) - 0x83 (F7), 0x84 (SysReq)
- [Set 3 special codes](../set3/README.md#special-codes) - 0x7C (Keypad Comma), 0x83 (F7), 0x84 (Keypad Plus)

## Benefits of Unified Processor

**Primary Advantage: Hot-Swappable Keyboards**

The unified processor enables AT/PS2 converters to automatically adapt to different keyboard types:
- Swap between standard PS/2 keyboards (Set 2) and terminal keyboards (Set 3)
- No firmware rebuild required
- Runtime scancode set detection and configuration
- Single binary supports multiple keyboard types

**Additional Benefits:**
- Bug fixes and improvements in one place (rather than three)
- Consistent behavior across all scancode sets
- Reduced code duplication (669 lines → 535 lines)
- Configuration-driven design simplifies testing

**Code Consolidation:**
- Set 1 original: 212 lines
- Set 2 original: 330 lines  
- Set 3 original: 127 lines
- **Total original: 669 lines**
- **Unified: ~535 lines** (includes all three sets + configuration structures)

## Choosing Between Unified and Individual Processors

### Use the Unified Processor (set123) When:

✅ Building a universal AT/PS2 converter that supports multiple keyboard types  
✅ You want runtime keyboard detection and hot-swapping capability  
✅ Supporting both standard PS/2 and terminal keyboards  
✅ Prefer single codebase for maintenance  

### Use Individual Processors (set1/set2/set3) When:

✅ Building a converter for a single, specific keyboard  
✅ Optimizing for absolute minimal code size  
✅ Prefer simpler, focused implementation  
✅ Working with legacy code that uses original processors  

**Both approaches are valid** - choose based on your use case.

## Fallback to Individual Set

If you encounter issues with the unified processor, you can easily revert to individual set implementations:

1. **Edit your keyboard's config file** (`keyboards/<keyboard>/keyboard.config`):
   ```
   # Change from:
   CODESET=set123
   
   # To one of:
   CODESET=set1   # For XT keyboards
   CODESET=set2   # For standard PS/2 keyboards
   CODESET=set3   # For terminal keyboards
   ```

2. **Rebuild** - The build system will automatically use the original implementation

3. **Test** - Verify keyboard functionality with the individual processor

The original per-set implementations are fully maintained and remain the fallback option if needed.

## Performance

**No overhead compared to original implementations:**
- Same O(1) lookup tables
- Inline helper functions
- Static const configuration (~4 bytes stack overhead)
- No dynamic allocation
- No runtime branching overhead

## Why Not Include Apple M0110?

Apple M0110 uses a completely different protocol family:
- No E0/E1 prefixes
- Different encoding scheme
- Different timing requirements
- Separate protocol family (not XT/AT/PS2)

**Philosophy:** Consolidate within protocol families (XT/AT/PS2), not across different protocol families. See [Apple M0110 protocol documentation](../../protocols/apple-m0110/README.md) for details.

## Technical Encoding References

For in-depth understanding of each scancode set's encoding, sequences, and special cases:

- **[Set 1 Technical Details](../set1/README.md)** - XT encoding, high-bit breaks, E0/E1 sequences, 6-byte Pause
- **[Set 2 Technical Details](../set2/README.md)** - F0 breaks, E0 extended keys, 8-byte Pause, host commands
- **[Set 3 Technical Details](../set3/README.md)** - Simplified design, no prefixes, 2-state machine

These documents provide comprehensive technical specifications independent of this keyboard converter implementation.