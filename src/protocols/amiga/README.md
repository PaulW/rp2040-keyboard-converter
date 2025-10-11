# Commodore Amiga Keyboard Protocol

## Overview

The **Commodore Amiga keyboard protocol** is a synchronous serial protocol used by Amiga computers (A1000, A500, A2000, A3000, A4000) from 1985 onwards. Unlike simpler unidirectional protocols, the Amiga protocol features **bidirectional communication** with a sophisticated handshake mechanism, synchronization recovery, and special command codes for system management including reset capabilities.

The protocol is notable for its **bit rotation scheme** (transmitting bits in order 6-5-4-3-2-1-0-7) and **mandatory handshake** requirement where the computer must acknowledge every byte received. This design ensures reliable communication but requires careful timing implementation on the host side.

-----

## Historical Context

  - **1985**: Amiga 1000 introduces the protocol with innovative keyboard-to-computer interface
  - **1987**: Amiga 500 and 2000 adopt the same protocol with slight keyboard variations
  - **1990**: Amiga 3000 maintains protocol compatibility
  - **1992**: Amiga 4000 continues using the established protocol
  - **Legacy**: One of the most sophisticated 8-bit keyboard protocols of the era

The Amiga keyboard protocol was designed to be reliable and feature-rich, supporting advanced capabilities like three-key reset sequences (CTRL + both Amiga keys) and keyboard-initiated system resets with grace periods for emergency shutdowns.

-----

## Technical Specifications

### Physical Interface

The Amiga keyboard uses a **four-wire interface** with bidirectional data capabilities:

  - **Connector**: Custom 4-pin connector (varies by Amiga model)
  - **Signals**: 
    - **KCLK** (Keyboard Clock): Unidirectional, always driven by keyboard
    - **KDAT** (Keyboard Data): Bidirectional, driven by both keyboard and computer
    - **+5V**: Power supply from computer to keyboard
    - **GND**: Ground reference
  - **Logic Levels**: 5V TTL, active-low signaling (high = 0, low = 1)
  - **Pull-ups**: Open-collector design with pull-up resistors in both keyboard and computer
  - **Bidirectional**: KDAT line requires bidirectional capability for handshake

### Protocol Characteristics

  - **Type**: Synchronous serial communication with keyboard-generated clock
  - **Direction**: Primarily keyboard → computer, with computer acknowledgment via KDAT handshake
  - **Clock Generation**: Keyboard generates KCLK signal, computer never drives clock
  - **Data Format**: 8-bit bytes with special bit rotation (6-5-4-3-2-1-0-7 transmission order)
  - **Bit Order**: Rotated (bit 6 first, bit 7 last) to ensure key-up codes on sync loss
  - **Framing**: No start/stop bits, pure 8-bit data transmission
  - **Handshake**: **Mandatory** - Computer must pulse KDAT low for 85µs after each byte
  - **Error Recovery**: Automatic resynchronization by clocking out 1-bits until handshake
  - **Flow Control**: Handshake timeout (143ms) triggers resync mode

### Data Format

The Amiga protocol transmits 8-bit values with a unique **bit rotation**:

**Bit Transmission Order**: 6-5-4-3-2-1-0-7 (bit 6 sent first, bit 7 sent last)

**Reason for Rotation**: 
- Bit 7 = key up/down flag
- Transmitted last to ensure sync loss produces key-up codes (safer than key-down)
- During resync, keyboard clocks out 1-bits → creates 0xFF pattern → appears as key release

**Active-Low Logic**:
- Logic HIGH (+5V) = 0
- Logic LOW (0V) = 1
- Inverted from standard TTL convention

**Keycodes**:
- **Bits 0-6**: Key identification (0x00-0x67)
- **Bit 7**: Up/Down flag (0 = key pressed, 1 = key released)

**Special Codes** (8-bit, no up/down flag):
- `0x78`: Reset warning (CTRL + both Amiga keys held)
- `0xF9`: Lost sync / Last key code bad
- `0xFA`: Keyboard buffer overflow
- `0xFC`: Keyboard self-test failed
- `0xFD`: Initiate power-up key stream
- `0xFE`: Terminate power-up key stream

## Signal Diagrams

### Data Transmission (Keyboard → Computer)

```
Normal 8-Bit Transmission:
    ___   ___   ___   ___   ___   ___   ___   ___   _______
KCLK   \_/   \_/   \_/   \_/   \_/   \_/   \_/   \_/

    _______________________________________________________
KDAT       \_____x_____x_____x_____x_____x_____x_____x_____/
             (6)   (5)   (4)   (3)   (2)   (1)   (0)   (7)

          First                                      Last
          sent                                       sent
```

**Timing Details**:
- **Bit Setup**: KDAT set ~20µs before KCLK falls
- **Clock Low**: KCLK stays low ~20µs
- **Clock High**: KCLK goes high, stays high ~20µs
- **Bit Rate**: ~60µs per bit = ~17 kbit/s
- **Total Byte**: 8 bits × 60µs = ~480µs per complete byte transmission

### Handshake (Computer → Keyboard)

```
Handshake Pulse (After 8th Bit Received):
    ___________________________________________________
KCLK                                                   

    _______________________             _______________
KDAT                       \___________/
                           ^           ^
                           |           |
                        Start        End
                    (within 1µs)  (85µs minimum)
                    
Required Timing:
- Pulse KDAT low within 1µs of last KCLK rising edge
- Hold KDAT low for minimum 85µs (software MUST use 85µs for compatibility)
- Hardware latch detects pulses ≥1µs
- Release KDAT (returns high via pull-up)
```

### Power-Up Synchronization

```
Power-On Sequence:
    ___     ___     ___     ___     ___     ___
KCLK   \___/   \___/   \___/   \___/   \___/   \___  (1-bits)

    _______     _______     _______     _______
KDAT       \___/       \___/       \___/       \___  (waiting for handshake)

After Handshake Received:
    __________________________________________________
KCLK

    _____________________             ________________
KDAT                     \___________/
                         ^           ^
                      Handshake   Sync Achieved
```

**Synchronization Process**:
1. Keyboard powers up and performs self-test
2. Keyboard clocks out 1-bits slowly (~60µs per bit)
3. Computer may be booting (could take minutes)
4. Keyboard continues clocking until it receives handshake
5. After handshake: keyboard and computer are synchronized
6. No more than 8 clocks needed if keyboard plugged into running system

## Timing Specifications

### Clock Timing (Keyboard-Generated)
```
KCLK Timing:
- Pre-clock setup: ~20µs (KDAT stable before KCLK falls)
- Clock low phase: ~20µs (KCLK held low)
- Clock high phase: ~20µs (KCLK held high)
- Total bit period: ~60µs (complete clock cycle)
- Bit rate: ~17 kbit/sec (16.67 kHz approximate)
- Tolerance: Typical ±10% variance across keyboards
```

### Handshake Timing Requirements
```
Computer Handshake Response:
- Setup time: Within 1µs of KCLK 8th rising edge
- Minimum pulse: 85µs (software MUST use this for compatibility)
- Hardware detection: Latches pulses ≥1µs
- Maximum delay: 143ms before keyboard assumes lost sync

Critical:
- 1µs minimum for hardware detection
- 85µs required for all keyboard models (some need longer)
- Shorter pulses may not be detected by all keyboards
```

### Timeout and Recovery
```
Handshake Timeout:
- Expected: Handshake within 1µs-1ms (typical operation)
- Timeout: 143ms without handshake triggers resync mode
- Recovery: Keyboard clocks out 1-bits until handshake received

Resync Mode Timing:
- Clock out one 1-bit (~60µs)
- Wait 143ms for handshake
- If no handshake, clock another 1-bit
- Repeat until handshake received
- Send 0xF9 (lost sync code) after recovery
- Retransmit the failed byte
```

-----

## Protocol Operation

### Power-Up and Initialization

```
1. Power-On:
   - Keyboard performs internal self-test (ROM checksum, RAM, watchdog)
   - Duration: Varies by keyboard model

2. Synchronization:
   - Keyboard clocks out 1-bits slowly
   - Waits for computer to handshake
   - May continue for minutes if computer still booting
   - Maximum 8 clocks if plugged into running system

3. Self-Test Result:
   - Success: Continue to power-up key stream
   - Failure: Send 0xFC, then blink CAPS LOCK LED in coded pattern
     * 1 blink: ROM checksum failure
     * 2 blinks: RAM test failure
     * 3 blinks: Watchdog timer failure
     * 4 blinks: Short between row lines

4. Power-Up Key Stream:
   - Send 0xFD (initiate power-up key stream)
   - Send codes for any keys currently held down
   - Send 0xFE (terminate key stream)
   - Turn off CAPS LOCK LED
   - Enter normal operation mode
```

### Normal Operation

```
Continuous Scan Loop:
1. Keyboard scans key matrix
2. On key state change:
   a. For press: Generate key code with bit 7 = 0
   b. For release: Generate key code with bit 7 = 1
   c. Exception: CAPS LOCK only sends on press, bit 7 reflects LED state
3. Rotate bits: Original bits [7:0] → transmitted as [6-5-4-3-2-1-0-7]
4. Transmit 8 bits using KCLK/KDAT protocol
5. Wait for handshake (KDAT low pulse from computer)
6. If handshake timeout (143ms):
   a. Enter resync mode
   b. Clock out 1-bits until handshake received
   c. Send 0xF9 (lost sync code)
   d. Retransmit the failed key code
7. Return to step 1
```

### CAPS LOCK Behavior

**Unique Handling**:
- Only generates code on **key press** (never on release)
- Bit 7 indicates **LED state** (not press/release):
  - Bit 7 = 0: CAPS LOCK LED is now ON
  - Bit 7 = 1: CAPS LOCK LED is now OFF
- Computer must track LED state and toggle on each press

### Reset Warning Sequence

**Three-Key Combination**: CTRL + Left Amiga + Right Amiga

```
Reset Warning Protocol:
1. User presses CTRL + both Amiga keys
2. Keyboard syncs pending transmissions
3. Keyboard sends first 0x78 (reset warning)
4. Computer must handshake normally (or keyboard does hard reset)
5. Keyboard sends second 0x78
6. Computer must pull KDAT low within 250ms (or keyboard does hard reset)
7. Computer has 10 seconds for emergency processing
8. When computer pulls KDAT high, keyboard asserts hard reset
9. If KDAT not pulled high in 10s, keyboard asserts hard reset anyway
```

**Hard Reset** (post-warning):
- Keyboard pulls KCLK low
- Starts 500ms timer
- When keys released AND 500ms elapsed: releases KCLK
- Keyboard performs internal reset (jumps to startup code)
- Motherboard detects 500ms KCLK pulse and resets system

## Scan Code Set

The Amiga uses a **custom scan code set** based on physical keyboard matrix layout:

### Matrix Organization

- **6 rows** × **16 columns** = 96 possible keys
- Physical matrix encoded as: `Column (bits 7-4) | Row (bits 3-2)`
- Actual keyboard implementations vary by model (A1000 vs A500/A2000)

### Common Key Codes (Examples)

| Key | Code | Key | Code | Key | Code |
|-----|------|-----|------|-----|------|
| **ESC** | `0x45` | **1** | `0x01` | **2** | `0x02` |
| **Q** | `0x10` | **W** | `0x11` | **E** | `0x12` |
| **A** | `0x20` | **S** | `0x21` | **D** | `0x22` |
| **Z** | `0x31` | **X** | `0x32` | **C** | `0x33` |
| **Space** | `0x40` | **Return** | `0x44` | **Backspace** | `0x41` |
| **Tab** | `0x42` | **Caps Lock** | `0x62` | **Left Shift** | `0x60` |
| **Right Shift** | `0x61` | **Ctrl** | `0x63` | **Left Alt** | `0x64` |
| **Right Alt** | `0x65` | **Left Amiga** | `0x66` | **Right Amiga** | `0x67` |

### Special Keys (Independently Readable)

These keys never generate ghosts or phantoms in the matrix:

| Key | Code | Description |
|-----|------|-------------|
| **Left Shift** | `0x60` | Independent row/column |
| **Right Shift** | `0x61` | Independent row/column |
| **Ctrl** | `0x63` | Independent row/column |
| **Left Alt** | `0x64` | Independent row/column |
| **Right Alt** | `0x65` | Independent row/column |
| **Left Amiga** | `0x66` | Independent row/column |
| **Right Amiga** | `0x67` | Independent row/column |

### Keycode Format

```
Standard Key Press:
    Bits [7:0] = [0|Col|Row]
    Where: Bit 7 = 0 (pressed)
          Bits 6-2 = Key identification
          
Standard Key Release:
    Bits [7:0] = [1|Col|Row]
    Where: Bit 7 = 1 (released)
          Bits 6-2 = Key identification

Transmitted Order After Rotation:
    Original: [7  6  5  4  3  2  1  0]
    Sent as:  [6  5  4  3  2  1  0  7]
```

## Implementation Details

### Host-Side Handshake (Critical)

```c
// Pseudo-code for Amiga handshake
void amiga_handshake(uint data_pin) {
    // CRITICAL: Must be called immediately after 8th bit received
    // Timing: Within 1µs of last KCLK rising edge
    
    // Pull KDAT low for handshake
    gpio_set_dir(data_pin, GPIO_OUT);  // Set to output
    gpio_put(data_pin, 0);              // Pull low
    
    // Hold for 85µs (compatibility requirement)
    sleep_us(85);
    
    // Release KDAT (returns high via pull-up)
    gpio_set_dir(data_pin, GPIO_IN);   // Set to input (high-Z)
}
```

**Critical Timing Notes**:
- Handshake must start within 1µs of byte completion
- 85µs duration is **mandatory** for all keyboard models
- Shorter durations may work on some keyboards but fail on others
- Hardware latch in keyboard requires minimum pulse width detection

### PIO State Machine Configuration

```c
// Configuration for bidirectional KDAT with unidirectional KCLK
// Pin assignments: pin = KDAT, pin+1 = KCLK

// Receive data (LSB-first after rotation)
sm_config_set_in_shift(&c, true, true, 8);  // Right shift, autopush, 8 bits

// KCLK used for timing detection
sm_config_set_jmp_pin(&c, pin + 1);  // KCLK for state decisions

// KDAT for data input/output
sm_config_set_in_pins(&c, pin);      // KDAT input
sm_config_set_set_pins(&c, pin, 1);  // KDAT output control
```

### Clock Divider Calculation

```c
// Amiga timing: ~20µs per phase = ~60µs per bit
// PIO needs to sample at appropriate intervals for 20µs detection
float clock_div = calculate_clock_divider(20);  // 20µs minimum pulse width
```

### Bit Rotation Handling

```c
// De-rotate received byte (transmitted as 6-5-4-3-2-1-0-7)
uint8_t amiga_derotate_byte(uint8_t rotated) {
    // Received bit order: 6-5-4-3-2-1-0-7
    // Target bit order:   7-6-5-4-3-2-1-0
    
    uint8_t original = 0;
    original |= (rotated & 0x01) << 7;  // Bit 0 → Bit 7
    original |= (rotated & 0xFE) >> 1;  // Bits 7-1 → Bits 6-0
    
    return original;
}
```

## Advantages and Limitations

### Advantages
- **Bidirectional**: Handshake provides reliable acknowledgment
- **Synchronization**: Automatic recovery from sync loss
- **Error Detection**: Lost sync codes inform software of issues
- **Reset Capability**: Keyboard-initiated system reset with grace period
- **Robust Design**: Open-collector with pull-ups tolerates electrical noise
- **Hot-Plug Capable**: Handles keyboard connection to running system

### Limitations
- **Complex Timing**: Requires precise handshake pulse (85µs)
- **Mandatory Handshake**: Computer must respond to every byte
- **Bit Rotation**: Requires software de-rotation of received bytes
- **No LED Control**: Computer cannot control keyboard LEDs (except CAPS LOCK via key)
- **No Configuration**: Fixed timing and behavior, no host commands
- **Reset Risk**: Three-key combo can trigger reset (may be unintentional)

## Modern Applications

### Retro Computing
- **Amiga Restoration**: Authentic keyboard support for vintage Amigas
- **Emulation**: Accurate keyboard behavior in Amiga emulators
- **Preservation**: Maintaining original hardware functionality

### Conversion Projects
- **USB Adapters**: Convert Amiga keyboards for modern PCs
- **Modern Keyboard to Amiga**: Allow modern keyboards on vintage Amigas
- **Protocol Bridges**: Interface with other vintage computer protocols

### Embedded Systems
- **Microcontroller Projects**: Sophisticated keyboard input handling
- **Educational**: Learn about bidirectional serial protocols
- **Inspiration**: Study robust synchronization techniques

## Troubleshooting Guide

### Common Issues

1. **No Keyboard Response**
   - Check +5V power supply and ground connections
   - Verify KCLK and KDAT pull-up resistors (in both keyboard and computer)
   - Ensure KDAT is configured as bidirectional (open-collector or high-Z output)
   - Check that handshake is being sent (85µs pulse on KDAT)
   - Look for KCLK pulses during power-on sync (keyboard clocking out 1s)

2. **Garbled or Missing Characters**
   - Verify handshake timing (must be 85µs, not shorter)
   - Check handshake latency (must start within 1µs of byte end)
   - Confirm bit de-rotation is correct (6-5-4-3-2-1-0-7 → 7-6-5-4-3-2-1-0)
   - Look for KCLK signal integrity (clean edges, no bounce)
   - Ensure KDAT isn't held low by faulty hardware

3. **Keyboard Repeatedly Losing Sync**
   - Measure handshake pulse width (should be ~85µs)
   - Check for electrical noise on KCLK or KDAT lines
   - Verify handshake response time (<1µs from byte end)
   - Look for interrupt latency issues in host software
   - Ensure proper grounding between keyboard and computer

4. **CAPS LOCK LED Behavior Issues**
   - Remember: CAPS LOCK only sends on press, not release
   - Bit 7 reflects LED state, not key state
   - Computer must toggle LED state on each CAPS LOCK press
   - Some keyboards don't support LED control at all

5. **Unexpected Resets**
   - Check if CTRL + both Amiga keys being accidentally pressed
   - Verify keyboard isn't entering reset warning mode spuriously
   - Look for KCLK being pulled low by external hardware
   - Confirm keyboard self-test is passing (no LED blink codes)

### Debug Techniques

- **Logic Analyzer**: Capture KCLK, KDAT, and handshake timing
  - Trigger on KCLK falling edge
  - Measure bit periods (~60µs)
  - Verify handshake pulse width (should be 85µs)
  - Look for 143ms timeout gaps (indicates lost sync)

- **Oscilloscope**: Check signal quality and noise
  - KCLK and KDAT should have clean edges
  - No ringing or overshoot on transitions
  - Pull-ups should bring lines to 5V when idle
  - Handshake pulse should be clean 0V for 85µs

- **LED Indicators**: Visual feedback for protocol states
  - Power-on sync: Blink during 1-bit transmission
  - Byte received: Pulse on each complete byte
  - Handshake sent: Pulse when handshake executed
  - Lost sync: Distinct pattern when 0xF9 received

- **Serial Debug Output**: Log protocol events
  - Bytes received (before and after de-rotation)
  - Special codes (0xF9, 0xFD, 0xFE, 0x78)
  - Handshake timing measurements
  - Sync loss events

### Signal Quality Checklist

- ✅ Clean 5V power with low ripple (<50mV)
- ✅ Solid ground connection between keyboard and computer
- ✅ Pull-up resistors on both KCLK and KDAT (1-10kΩ typical)
- ✅ Open-collector or high-Z output capability on KDAT
- ✅ Short cable (<2 meters recommended)
- ✅ Shielded cable if in noisy environment
- ✅ ESD protection on signal lines
- ✅ Proper connector mating (no bent pins)

## Hardware Variants

### Amiga 1000 Keyboard
- **Earliest version**: First Amiga keyboard design
- **Connector**: Custom 4-pin connector
- **Features**: All standard keys, separate numeric pad
- **Reset**: Supports reset warning sequence
- **Compatibility**: Fully compatible with protocol specification

### Amiga 500 Keyboard
- **Integrated**: Built into A500 case
- **Connector**: Internal 4-pin connection
- **Features**: Compact layout, integrated numeric pad
- **Differences**: Some key codes differ from A1000
- **Reset**: Full reset warning support
- **Note**: Numeric pad keys have dedicated codes

### Amiga 2000 Keyboard
- **Separate keyboard**: Detached from computer
- **Connector**: External 4-pin connector
- **Features**: Full-size layout, separate numeric pad
- **Enhanced**: Additional keys compared to A1000
- **Reset**: Complete reset functionality
- **Professional**: Designed for professional use

### Amiga 3000/4000 Keyboards
- **Later models**: Enhanced layouts
- **Connector**: Same 4-pin standard maintained
- **Compatibility**: Backward compatible with earlier Amigas
- **Features**: Additional function keys and control keys
- **Protocol**: Identical protocol to earlier models

## References and Sources

1. **Amiga Hardware Reference Manual** (Commodore): Official protocol specification
2. **Amiga Hardware Programmer's Guide**: Detailed keyboard implementation
3. **Amiga ROM Kernel Reference Manual**: Low-level keyboard handling
4. **Community Documentation**: [amigarealm.com](https://www.amigarealm.com/computing/knowledge/hardref/aph.htm)
5. **Preservation Projects**: Amiga Forever, WinUAE documentation

## Special Notes

### Why Bit Rotation?

The unusual bit rotation (6-5-4-3-2-1-0-7) serves a critical purpose:

**Problem**: During sync loss, keyboard clocks out 1-bits to resynchronize.

**Without Rotation**: 
- Eight 1-bits → 0xFF byte → could be interpreted as random key press with up flag
- Dangerous if interpreted as modifier key

**With Rotation**:
- Bit 7 (up/down flag) transmitted last
- Sync recovery sends 1-bits → creates key-up code
- Key release is safer than key press (doesn't trigger actions)
- Software can safely ignore garbage key-up codes

This elegant design choice demonstrates the thoughtful engineering in the Amiga keyboard protocol.

### Reset Warning Philosophy

The reset warning sequence provides a **grace period** for emergency processing:

1. **User Intent**: Three-key combo clearly signals deliberate reset
2. **Software Warning**: 0x78 codes alert OS of impending reset
3. **Grace Period**: 10 seconds for disk writes, cache flush, etc.
4. **Fail-Safe**: Reset occurs even if software hangs

This design balances user control with system reliability—a reset can always complete even if software is unresponsive.
