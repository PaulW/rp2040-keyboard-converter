# Amiga Protocol

**Status**: ✅ Production | **Last Updated**: 28 October 2025

The Amiga keyboard protocol represents one of the most **sophisticated and elegant 8-bit serial protocols** ever designed for consumer computers. Created for the original Amiga 1000 in 1985, this bidirectional protocol features mandatory handshaking, unique bit rotation for safety, and graceful error recovery—engineering decisions that showcase Commodore's attention to detail and robustness.

What sets the Amiga protocol apart is its **mandatory bidirectional handshake**: every byte the keyboard transmits **must** be acknowledged by the host within 143ms, or the keyboard enters resynchronization mode. Combined with a clever bit rotation scheme (transmitting bits in order 6-5-4-3-2-1-0-7 instead of 0-7), this protocol achieves exceptional reliability while preventing dangerous failure modes where key-down codes could stick after sync loss.

---

## Table of Contents

- [Historical Context](#historical-context)
- [Protocol Overview](#protocol-overview)
- [Physical Interface](#physical-interface)
- [Communication Protocol](#communication-protocol)
- [Keyboard Operation](#keyboard-operation)
- [Special Features](#special-features)
- [Hardware Variants](#hardware-variants)
- [Implementation Notes](#implementation-notes)
- [Troubleshooting](#troubleshooting)

---

## Historical Context

### The Amiga Revolution

**1985 - Amiga 1000**: When Commodore launched the Amiga 1000, they weren't just building another home computer—they were creating a multimedia powerhouse. The keyboard protocol reflected this ambition: instead of settling for simple unidirectional communication (like the IBM XT), Commodore engineers designed a robust bidirectional protocol with error recovery and safety features.

**1987 - Amiga 500/2000**: The Amiga 500 and Amiga 2000 cemented Amiga's position in both home and professional markets. The keyboard protocol remained unchanged, demonstrating its excellent original design. These models popularized the Amiga platform and established the protocol as a de facto standard for Amiga-compatible systems.

**1990 - Amiga 3000**: The high-end Amiga 3000 brought workstation-class performance while maintaining complete protocol compatibility. The ability to use any Amiga keyboard with any Amiga model—from A1000 to A4000—showcased the protocol's forward-thinking design.

**1992 - Amiga 4000**: The final flagship Amiga, the A4000, continued using the same keyboard protocol, now nearly a decade old but still more sophisticated than many contemporary designs. This longevity is testament to the protocol's robustness and well-considered architecture.

**Legacy**: While Amiga production ended in the mid-1990s, the platform maintains a dedicated community. Modern Amiga enthusiasts, FPGA recreations (MiSTer, Minimig), and emulation projects all benefit from the protocol's excellent documentation and predictable behavior. The protocol's influence can be seen in later bidirectional protocols that adopted similar handshaking concepts.

### Design Philosophy

The Amiga protocol embodies several key design principles:

1. **Reliability First**: Mandatory handshaking ensures no data loss
2. **Safe Failure**: Bit rotation means sync loss produces key-up codes, never stuck keys
3. **Clear Communication**: Keyboard explicitly signals error conditions (lost sync, overflow)
4. **User Safety**: Reset warning gives 10 seconds to change mind before hard reset
5. **Deterministic**: Precise timing specifications enable reliable implementations

These weren't theoretical concerns—they were practical solutions to real problems observed in earlier keyboard protocols.

---

## Protocol Overview

### Key Characteristics

**Bidirectional with Mandatory Handshake**:

- **Keyboard → Host**: Transmits scan codes serially (8 bits, ~60µs per bit)
- **Host → Keyboard**: Must respond with 85µs handshake pulse within 143ms
- **Failure Mode**: Missing handshake triggers resynchronization (keyboard sends 1-bits until acknowledged)
- **Recovery**: After handshake received, keyboard sends 0xF9 (lost sync) and retransmits last byte

**Bit Rotation (6-5-4-3-2-1-0-7)**:

Transmitting bit 7 (key up/down flag) **last** prevents stuck-key failures:
- If sync lost mid-byte: Incomplete data interpreted as key-down
- Next sync point likely has bit 7 set (key-up)
- Result: Brief transient, not catastrophic stuck modifier

**Active-Low Logic**: HIGH=0, LOW=1 (open-drain design, noise immunity)

**Self-Clocked**: Keyboard generates timing (~17 kbit/s), host follows

---

## Physical Interface

### Connectors and Pinouts

**Amiga 1000/2000/3000**

![Amiga Connector Pinout](../images/connectors/kbd_connector_amiga.png)

*Image credit: [KbdBabel Vintage Keyboard Documentation](http://kbdbabel.org/conn/index.html)*

Amiga keyboards use different connectors depending on the model:

**Amiga 1000: RJ-10 4P4C Connector**

**Pinout** (looking at female socket):
- Pin 1: VCC - +5V power
- Pin 2: KCLK - Keyboard Clock (keyboard drives, output)
- Pin 3: KDAT - Keyboard Data (bidirectional)
- Pin 4: GND - Ground (0V reference)

**Amiga 2000/3000: DIN-5 Connector (180°)**

**Pinout**:
- Pin 1: KCLK - Keyboard Clock
- Pin 2: KDAT - Keyboard Data
- Pin 3: N/C - Not connected
- Pin 4: GND - Ground
- Pin 5: VCC - +5V power

**Amiga 500/500+: 8-pin PCB Connector**

![Amiga A500 Internal Connector](../images/connectors/kbd_connector_amiga_a500_internal.png)

*Image credit: [English Amiga Board](https://eab.abime.net/showthread.php?t=96725)*

**Pinout** (internal 8-pin header):
- Pin 1: KCLK - Keyboard Clock (keyboard drives, output)
- Pin 2: KDAT - Keyboard Data (bidirectional)
- Pin 3: RESET - Reset line (not used by most keyboards)
- Pin 4: VCC - +5V power
- Pin 5: KEY - Not connected (keying pin)
- Pin 6: GND - Ground (0V reference)
- Pin 7: Power LED - Not used by converter
- Pin 8: Drive LED - Not used by converter

**Connector Availability:**
- **A1000 (RJ-10)**: Readily available - standard 4P4C modular connectors from electronics suppliers
- **A2000/3000 (DIN-5)**: Readily available - same as IBM PC/XT/AT connectors, vintage electronics suppliers
- **A500/500+ (8-pin PCB)**: Standard 2.54mm (0.1") pitch male pin header (keyboard has female socket) - readily available from electronics suppliers

**Alternative for A500/500+**: Use standard 8-pin dupont-style jumper wires for prototyping

**Important**: Verify your specific Amiga keyboard model connector type before wiring. The converter implementation uses the KCLK (Clock) and KDAT (Data) signals, which are consistent across all variants.

### Electrical Characteristics

**Logic Levels (Active-Low)**:

```
HIGH (Logic 0 / Idle):
- Voltage: 4.5V to 5.5V
- Interpretation: No action, idle state

LOW (Logic 1 / Active):
- Voltage: 0V to 0.8V
- Interpretation: Active assertion, transmission

Pull-Up: Both KCLK and KDAT lines have pull-up resistors (4.7kΩ typical)
```

**Signal Characteristics**:

```
Type: Open-Drain (or Open-Collector)
Pull-Up Location: Host motherboard (not in keyboard)
Idle State: Both KCLK and KDAT HIGH (pulled up)
Active State: Keyboard or host pulls line LOW (sinks current)
```

**Power Requirements**:

```
Voltage: +5V ±5% (4.75V to 5.25V)
Current: 30-80mA typical (varies by keyboard model)
Startup: May draw up to 150mA during power-on initialization
```

**Timing Characteristics**:

| Parameter | Value | Notes |
|-----------|-------|-------|
| **Bit Period** | ~60µs | Keyboard-generated (approximately 16.7 kHz) |
| **Setup Phase** | ~20µs | KCLK HIGH, KDAT stable before transition |
| **Clock Low** | ~20µs | KCLK LOW, data active |
| **Clock High** | ~20µs | KCLK HIGH, host samples KDAT |
| **Handshake Duration** | 85µs min, 110µs actual | Our PIO: 100µs calculated, 110µs measured |
| **Handshake Timeout** | 143ms | Keyboard enters resync if no response |
| **PIO Clock Divider** | 500 | 4µs per cycle (125 MHz / 500) |

### Signal Integrity

**Cable Considerations**:

- **Maximum Length**: 1-2 meters recommended (longer cables increase noise)
- **Shielding**: Not required for typical installations, but helps in noisy environments
- **Capacitance**: Keep cable capacitance low (< 100pF) for clean signals

**Noise Immunity**:

The active-low open-drain architecture provides good noise immunity:
- Pull-up resistors provide HIGH default state
- LOW states actively driven (strong signal)
- Open-drain prevents bus conflicts (both sides can drive LOW safely)

**Recommended Protection**:

```
- Series resistors: 100Ω on KCLK and KDAT (limit current, protect against shorts)
- ESD protection: Transient voltage suppressors (TVS diodes) on signal lines
- Power filtering: 0.1µF ceramic + 10µF electrolytic capacitors near keyboard
```

---

## Communication Protocol

### Bit Timing and Frame Structure

Each byte transmission consists of **8 bits transmitted in rotated order** with precise timing defined in the Commodore Amiga Hardware Reference Manual:

> **From Official Specification**:  
> "The keyboard processor sets the KDAT line about **20 microseconds** before it pulls KCLK low. KCLK stays low for about **20 microseconds**, then goes high again. The processor waits another **20 microseconds** before changing KDAT. Therefore, the bit rate during transmission is about **60 microseconds per bit**, or 17 kbits/sec."  
> — *Amiga Hardware Reference Manual, Appendix H*

```
Timing Diagram (One Bit Period ~60µs):

         Setup      Data Low    Data High
       |←20µs→|     |←20µs→|     |←20µs→|
       ________     ________     ________
KCLK           \___/        \___/
       
       _______________________
KDAT   ______________________/     (Example: Transmitting 1-bit)

Phase 1: Setup    - KCLK HIGH, KDAT transitions to value (20µs)
Phase 2: Data Low - KCLK falls, KDAT stable (20µs)
Phase 3: Data High- KCLK rises, host samples KDAT (20µs)
Total bit period: 60µs (as specified in hardware manual)
```

**Bit Transmission Sequence**:

```
For byte value 0x3C (00111100 binary):
Bit positions:    7  6  5  4  3  2  1  0
Bit values:       0  0  1  1  1  1  0  0

Transmission order: 6 → 5 → 4 → 3 → 2 → 1 → 0 → 7
Actual sequence:    0 → 1 → 1 → 1 → 1 → 0 → 0 → 0

Timeline:
Bit 1 (bit 6=0): 60µs
Bit 2 (bit 5=1): 60µs
Bit 3 (bit 4=1): 60µs
Bit 4 (bit 3=1): 60µs
Bit 5 (bit 2=1): 60µs
Bit 6 (bit 1=0): 60µs
Bit 7 (bit 0=0): 60µs
Bit 8 (bit 7=0): 60µs
Total: 480µs per byte
```

### Signal Timing Diagrams

**Complete 8-Bit Transmission**:

```
Normal Byte Transmission (Keyboard → Host):

    ___   ___   ___   ___   ___   ___   ___   ___   _______
KCLK   \_/   \_/   \_/   \_/   \_/   \_/   \_/   \_/

    _______________________________________________________
KDAT      \_____x_____x_____x_____x_____x_____x_____x_____/
            (6)   (5)   (4)   (3)   (2)   (1)   (0)   (7)

         First bit                               Last bit
         transmitted                             transmitted

Timing per bit:
- Bit setup:   ~20µs (KDAT stable before KCLK falls)
- Clock low:   ~20µs (KCLK held low)
- Clock high:  ~20µs (KCLK returns high, data sampled)
- Total:       ~60µs per bit
- Bit rate:    ~16.67 kHz (approximately 17 kbit/sec)
```

**Handshake Response (Host → Keyboard)**:

```
Host Acknowledgment After Receiving Byte:

    __________________________________________________
KCLK                                               ↑
                                             8th rising edge

    _____________________             ________________
KDAT                     \___________/
                         ↑           ↑
                         |           |
                      Start         End
               (within 1µs)         (85µs minimum hold)

Handshake Requirements:
- Start: Within 1µs of 8th KCLK rising edge
- Duration: 85µs minimum (mandatory for all keyboard models)
- Hardware detection: Keyboard latches pulses ≥1µs
- Release: KDAT returns HIGH via pull-up resistor
```

**Power-Up Synchronization Sequence**:

```
Keyboard Power-On (Finding Sync):

    ___     ___     ___     ___     ___     ___
KCLK   \___/   \___/   \___/   \___/   \___/   \___  (Continuous 1-bits)

    _______     _______     _______     _______
KDAT       \___/       \___/       \___/       \___  (Waiting for handshake)

Process:
1. Keyboard clocks out 1-bits slowly (~60µs per bit)
2. Host may be booting (could take seconds or minutes)
3. Keyboard continues clocking until handshake received
4. Maximum 8 clocks if keyboard plugged into running system

After Handshake Received:

    __________________________________________________
KCLK                                                  (Idle)

    _____________________             ________________
KDAT                     \___________/
                         ↑           ↑
                      Handshake   Sync Achieved

Result: Keyboard and host now synchronized, normal operation begins
```

**Complete Transaction Example (Key Press)**:

```
Example: "B" Key Pressed (Scancode 0x35 = 00110101 binary)

Transmitted order after rotation: 01101010

    ___   ___   ___   ___   ___   ___   ___   ___   ______
KCLK   \_/   \_/   \_/   \_/   \_/   \_/   \_/   \_/

    _______            ______     ______      ____________
KDAT       \__________/      \____/      \____/

         0     1     1     0     1     0     1     0

Bit sequence sent: 6→5→4→3→2→1→0→7 = 0,1,1,0,1,0,1,0

After receiving all 8 bits, host sends handshake:
    _________________________________________________
KCLK

    __________________            ___________________
KDAT                  \__________/
                      ← 85µs →
```

### Handshake Protocol

After transmitting all 8 bits, the keyboard **immediately** waits for host acknowledgment:

```
Complete Transaction Timing:

Keyboard transmits 8 bits (480µs)
         ↓
Keyboard waits for handshake (0-143ms timeout)
         ↓
Host pulls KDAT LOW for 85µs minimum
         ↓
Host releases KDAT (goes HIGH)
         ↓
Keyboard ready for next transmission
```

**Handshake Requirements**:

| Parameter | Value | Notes |
|-----------|-------|-------|
| **Protocol Minimum** | 85µs | Mandatory for all keyboard models |
| **Hardware Detection** | ≥1µs | Keyboard latch threshold |
| **Implementation** | ~110µs | PIO: 100µs calculated, 110µs measured |
| **Maximum Delay** | 143ms | Timeout triggers resync mode |

**Why 85µs?** While hardware detects ≥1µs pulses, different keyboard models vary. The 85µs spec ensures universal compatibility (A1000/A500/A2000/A3000/A4000). Our 110µs provides +29% safety margin.

### Error Recovery: Resynchronization Mode

If handshake timeout (143ms) occurs, keyboard enters resync mode:

1. Transmits continuous 1-bits (~60µs each) until handshake received
2. Host sends 85µs handshake
3. Keyboard exits resync, sends 0xF9 (Lost Sync code)
4. Keyboard retransmits original byte
5. Normal operation resumes

The 0xF9 code notifies host to discard partial data and reset receive state.

---

## Keyboard Operation

### Power-On Initialization

**Complete Power-Up Sequence**:

```
Step 1: Power-On Self-Test
- Keyboard receives +5V power from host
- Internal microcontroller boots and initializes
- Self-test executes (duration varies by keyboard model):
  a. ROM checksum verification
  b. RAM read/write test  
  c. Watchdog timer test
  d. Key matrix row line check (detecting shorts)

Step 2: Self-Test Results
SUCCESS:
  → Proceed to synchronization
  
FAILURE:
  → Send 0xFC (self-test failed) code
  → Enter LED blink error indication mode
  → Keyboard will NOT respond to key presses
  
Step 3: Synchronization
- Keyboard clocks out 1-bits slowly (~60µs per bit)
- Waits for host to send handshake
- May continue for seconds/minutes if host still booting
- Maximum 8 clocks needed if plugged into running system

Step 4: Power-Up Key Stream
- Send 0xFD (initiate power-up key stream)
- Send codes for ANY keys currently held down
  (bit 7=0 for each, indicating "pressed")
- Send 0xFE (terminate power-up key stream)
- Turn off CAPS LOCK LED
- Enter normal operation mode

Typical sequence: Power → Self-test → Sync → 0xFD → 0xFE → Ready
```

**Self-Test LED Blink Codes**:

If self-test fails, the keyboard enters an error state and blinks the CAPS LOCK LED:

| Blink Pattern | Meaning | Description |
|---------------|---------|-------------|
| **1 blink** | ROM checksum failure | Firmware corrupted, keyboard non-functional |
| **2 blinks** | RAM test failed | Internal memory defective |
| **3 blinks** | Watchdog timer failure | Microcontroller timing malfunction |
| **4 blinks** | Row line short | Physical short between keyboard matrix rows |

**Blink Rate**: Approximately one burst per second (e.g., 1 blink → pause → 1 blink → pause...)

**Recovery**: Self-test failures are fatal—keyboard will not function until repaired. These indicate hardware problems, not communication issues.

**Important**: Unlike some protocols, Amiga keyboards do **not** send a "BAT Passed" (0xAA) code after successful self-test. Instead, they enter synchronization mode and wait for the host to handshake, confirming bidirectional communication works.

### Scan Code Transmission

```
Normal Operation Loop:

1. Keyboard scans key matrix (continuous polling)
2. Detects key state change:
   - Key pressed: Bit 7 = 0, bits 6-0 = key code
   - Key released: Bit 7 = 1, bits 6-0 = key code
3. Transmits scan code (8 bits in rotated order: 6-5-4-3-2-1-0-7)
4. Waits for handshake (up to 143ms)
5. If handshake received:
   - Acknowledges, ready for next key event
6. If handshake timeout:
   - Enter resync mode (transmit 1-bits)
   - Wait for handshake
   - Send 0xF9 (lost sync)
   - Retransmit original byte
7. Return to step 1
```

### Scan Code Format

```
Bit Layout:
7  6  5  4  3  2  1  0
|  |              |
|  └──────────────┴─ Key code (0x00 to 0x7F)
└─ Key state: 0=pressed, 1=released

Examples:
0x33 = 00110011 binary → Key 0x33 pressed (bit 7=0)
0xB3 = 10110011 binary → Key 0x33 released (bit 7=1)

0x50 = 01010000 binary → Key 0x50 pressed
0xD0 = 11010000 binary → Key 0x50 released
```

**Key Code Range**:
- **0x00 - 0x67**: Standard key codes (104 keys max)
- **0x78**: Reset warning (see Special Features section)
- **0xF9**: Lost synchronization (error recovery)
- **0xFA**: Keyboard buffer overflow (too many simultaneous keys)
- **0xFC**: Self-test failed (hardware error)
- **0xFD**: Initiate power-up key stream (internal use)
- **0xFE**: Terminate key stream (internal use)

### Scan Code Set

The Amiga uses a **custom scan code set** based on the physical keyboard matrix layout.

**Matrix Organization**:

```
Structure: 6 rows × 16 columns = 96 possible key positions
Encoding: Column (bits 6-4) | Row (bits 3-2) | Always 0 (bits 1-0)
Range: 0x00 to 0x67 (104 possible codes, not all used)
```

**Standard Keycode Format**:

```
Make Code (Key Press):
  Bit 7: 0 (pressed)
  Bits 6-0: Key identification

Break Code (Key Release):  
  Bit 7: 1 (released)
  Bits 6-0: Key identification

Example:
  0x35 = B key pressed
  0xB5 = B key released (0x35 | 0x80)
```

**Note**: For complete scancode tables including all keys, modifiers, function keys, and keyboard variants, see the **[Amiga Scancode Set](../../src/scancodes/amiga/README.md)** documentation.

---

## Special Features

### CAPS LOCK LED Control

Amiga keyboards handle CAPS LOCK with unique behavior requiring sophisticated synchronization:

**Keyboard Behavior**:
- CAPS LOCK LED is physically in the keyboard
- Keyboard sends event **only on key press** (never on release)
- Bit 7 indicates **LED state** (not key state):
  - **Bit 7 = 0** (0x62): LED is **ON** (CAPS LOCK active)
  - **Bit 7 = 1** (0xE2): LED is **OFF** (CAPS LOCK inactive)

**USB HID Behavior**:
- Host computer controls CAPS LOCK via HID reports
- Press+release cycle toggles the state
- Host sends LED state back to device

**Synchronization Challenge**:

The converter must keep keyboard LED state and USB HID state synchronized, especially after reboot:

| Keyboard LED | USB HID | Converter Action | Reason |
|--------------|---------|------------------|---------|
| OFF | OFF | **SKIP** toggle | Already synchronized |
| OFF | ON | **SEND** toggle | Need to turn USB OFF |
| ON | OFF | **SEND** toggle | Need to turn USB ON |
| ON | ON | **SKIP** toggle | Already synchronized |

**Implementation Strategy**:

The converter implements smart synchronization by comparing keyboard LED state against USB HID state. Only when states differ does it queue a press+release toggle sequence. A 125ms hold time between press and release ensures MacOS compatibility.

**Synchronization Logic**:
```c
// Extract from keyboard_interface.c (lines 270-295)
bool kbd_led_on = (data_byte & 0x80) == 0;  // Bit 7=0 means LED ON
bool hid_caps_on = lock_leds.keys.capsLock; // Current USB HID state

if (kbd_led_on != hid_caps_on) {
    // States differ - queue toggle event
    ringbuf_put(AMIGA_CAPSLOCK_KEY);        // Press (0x62)
    // Start non-blocking timer for 125ms delay
    caps_lock_timing.state = CAPS_PRESS_SENT;
    caps_lock_timing.press_time_ms = to_ms_since_boot(get_absolute_time());
    // Release sent after delay by timing state machine
} else {
    // States match - discard (already synchronized)
}
```

- **Implementation Note**: See [`keyboard_interface.c`](../../src/protocols/amiga/keyboard_interface.c) lines 270-295 for complete state comparison and non-blocking timing state machine

**Example: Reboot Desync Recovery**

1. Before reboot: Keyboard LED **ON**, USB HID **ON** (synchronized)
2. Converter reboots: Keyboard LED **ON** (still powered), USB HID **OFF** (reset)
3. User presses CAPS LOCK: Keyboard sends 0xE2 (LED turning **OFF**)
4. Converter checks: `kbd_led_on=false`, `hid_caps_on=false` → **States match!**
5. Result: No toggle sent (already synchronized at OFF)

Without this check, the converter would blindly send a toggle, causing USB to go OFF→ON (wrong direction).

### Reset Warning System

The Amiga protocol includes a graceful reset mechanism preventing accidental data loss:

**Trigger**: CTRL + Left Amiga + Right Amiga simultaneously

**Protocol Sequence**:

1. **First 0x78**: Keyboard sends first reset warning → Host **must** handshake normally (85µs)
2. **Second 0x78**: Keyboard sends second warning → Host **must** pull KDAT LOW within 250ms
3. **Grace Period**: Host keeps KDAT LOW for up to 10 seconds (emergency cleanup time)
4. **Completion**:
   - User holds keys 10 seconds → Hard reset (keyboard pulls KCLK LOW for 500ms)
   - User releases keys → Keyboard sends 0xF8 (reset aborted), normal operation resumes
   - Host pulls KDAT HIGH → Immediate hard reset

**Compatibility**: A500 keyboards do **not** support reset warning (no 0x78 codes)

### Special Codes Summary

| Code | Value | Description |
|------|-------|-------------|
| **Reset Warning** | 0x78 | CTRL + both Amiga keys pressed, 10s countdown starts |
| **Reset Abort** | 0xF8 | User released keys during countdown, reset aborted |
| **Lost Sync** | 0xF9 | Resynchronization completed, about to retransmit byte |
| **Buffer Overflow** | 0xFA | Too many simultaneous keys, keyboard buffer full |
| **Self-Test Failed** | 0xFC | Hardware error detected during power-on test |
| **Initiate Stream** | 0xFD | Power-up key stream start (internal protocol use) |
| **Terminate Stream** | 0xFE | Power-up key stream end (internal protocol use) |

---

## Hardware Variants

The Amiga keyboard protocol remained consistent across all Amiga models, but keyboard hardware varied in form factor, layout, and features.

### Amiga 1000 Keyboard

**Type**: External keyboard (first generation)

**Characteristics**:
- Separate keyboard connected via RJ-10/RJ-11 cable
- Full-size layout with separate numeric keypad
- 10 function keys (F1-F10)
- Reset warning feature: ✅ **Supported**
- Unique beige/cream color scheme matching A1000

**Protocol**: Fully compatible with specification

**Connector**: RJ-10 (4P4C) or RJ-11 (6P4C, only 4 pins used)

**Notes**: Original Amiga keyboard design, became template for later models. Some early units may have minor timing variations.

### Amiga 500 / 500+ Keyboard

**Type**: Integrated keyboard (built into computer case)

**Characteristics**:
- Keyboard integrated into A500 case top
- Compact layout with integrated numeric keypad
- 10 function keys (F1-F10)
- Reset warning feature: ❌ **NOT Supported**
- Internal 8-pin PCB connection (not externally accessible)

**Protocol**: Fully compatible (except no 0x78 reset warning)

**Connector**: 8-pin 2.54mm pitch header (internal connection)

**Notes**: Most popular Amiga model. Keyboard cannot be easily separated from computer case. Some numeric keypad keys use different scancodes than A1000.

### Amiga 2000 Keyboard

**Type**: External keyboard (professional model)

**Characteristics**:
- Separate keyboard connected via 5-pin DIN cable
- Full-size professional layout
- Separate numeric keypad with dedicated keys
- 10 function keys (F1-F10)
- Reset warning feature: ✅ **Supported**
- Enhanced build quality for professional use

**Protocol**: Fully compatible with specification

**Connector**: 5-pin DIN (same physical connector as IBM PC/XT/AT)

**Notes**: Designed for professional/business use. High-quality construction. Often considered the best Amiga keyboard.

### Amiga 3000 Keyboard

**Type**: External keyboard (workstation-class)

**Characteristics**:
- Separate keyboard connected via 5-pin DIN cable
- Enhanced layout with additional keys
- Full numeric keypad
- 10 function keys (F1-F10) + Help key
- Reset warning feature: ✅ **Supported**
- Workstation-quality construction

**Protocol**: Fully compatible with specification

**Connector**: 5-pin DIN

**Notes**: Backward compatible with all earlier Amigas. Can be used on A1000, A500, A2000 with appropriate cable adapter.

### Amiga 4000 Keyboard

**Type**: External keyboard (final flagship model)

**Characteristics**:
- Separate keyboard connected via 5-pin DIN cable
- Enhanced layout (similar to A3000)
- Full numeric keypad
- 10 function keys (F1-F10) + Help key
- Reset warning feature: ✅ **Supported**
- High-quality construction

**Protocol**: Fully compatible with specification

**Connector**: 5-pin DIN

**Notes**: Final production Amiga keyboard. Fully backward compatible with all previous models.

### Cross-Compatibility

**Protocol Level**: ✅ All keyboards use identical electrical protocol

**Physical Level**: Requires cable adapters:
- A1000 (RJ-10/RJ-11) ↔ A2000/A3000/A4000 (DIN-5): Adapter available
- A500 (internal 8-pin) ↔ External models: Requires modification
- Any external keyboard works on any external-keyboard Amiga with correct cable

**Scancode Level**: Minor differences:
- Core keys (letters, numbers, modifiers): Identical across all models
- Numeric keypad: A500 uses some different codes than A1000/A2000
- Special keys: A3000/A4000 may have additional keys not present on earlier models

**Best Practice**: Always verify scancode mappings for specific keyboard model when implementing protocol support.

---

## Implementation Notes

### Overview

This implementation handles the Amiga protocol entirely in hardware using the RP2040's PIO (Programmable I/O) peripheral, with minimal software overhead. The design follows these principles:

1. **PIO State Machine**: Receives 8 bits in rotated order, automatically sends 100µs handshake
2. **Interrupt Handler**: De-rotates bits, processes special codes, queues to ring buffer
3. **Main Loop**: Processes scan codes when USB HID ready
4. **2-State Machine**: UNINITIALISED → INITIALISED (special codes handled in event processor)

### Key Implementation Files

- **[`keyboard_interface.pio`](../../src/protocols/amiga/keyboard_interface.pio)**: PIO assembly for bit reception and handshake
- **[`keyboard_interface.h`](../../src/protocols/amiga/keyboard_interface.h)**: Protocol constants and inline helpers
- **[`keyboard_interface.c`](../../src/protocols/amiga/keyboard_interface.c)**: State machine and event processing
- **[`src/scancodes/amiga/`](../../src/scancodes/amiga/)**: Scancode to HID keycode translation

### Bit De-Rotation

The PIO receives bits in the transmitted order (6-5-4-3-2-1-0-7). The interrupt handler de-rotates:

```c
// Inline helper from keyboard_interface.h
static inline uint8_t amiga_derotate_byte(uint8_t rotated) {
    // Received: [6 5 4 3 2 1 0 7]
    // Target:   [7 6 5 4 3 2 1 0]
    uint8_t original = 0;
    original |= (rotated & 0x01) << 7;  // Bit 0 → Bit 7
    original |= (rotated & 0xFE) >> 1;  // Bits 7-1 → Bits 6-0
    return original;
}
```

### PIO Handshake Implementation

The handshake is generated entirely in PIO hardware after receiving 8 bits:

```assembly
; From keyboard_interface.pio
; After 8 bits received, send 100µs handshake pulse

set pins, 0         ; Pull KDAT low
set pindirs, 1      ; Set KDAT as output
set x, 24           ; Loop counter (25 iterations: 24→0)

handshake_loop:
    jmp x--, handshake_loop  ; 1 cycle per iteration

set pindirs, 0      ; Release KDAT (goes high via pull-up)
```

**Timing Calculation**:
- Clock divider: 500 (optimized for 20µs KCLK pulse detection)
- PIO cycle time: 125 MHz / 500 = 4µs per cycle
- Loop iterations: 25 (counts 24→0)
- Handshake duration: 25 × 4µs = 100µs

**Why This Works**:
- Above 85µs minimum ✓
- Well below 143ms timeout ✓
- No blocking operations in software ✓
- Hardware-precise timing ✓

### State Machine

```c
// From keyboard_interface.c
static enum {
  UNINITIALISED,    // Initial state, waiting for first byte
  INITIALISED       // Normal operation
} keyboard_state = UNINITIALISED;
```

### State Machine

```c
static enum {
  UNINITIALISED,    // Initial state, waiting for first byte
  INITIALISED       // Normal operation
} keyboard_state = UNINITIALISED;
```

- **UNINITIALISED** → **INITIALISED** on first byte received
- Special codes (0xF9, 0xFA, 0xFC, 0xFD, 0xFE, 0x78) handled in event processor, not as separate states

### Performance Characteristics

| Metric | Value | Notes |
|--------|-------|-------|
| **Keystroke Latency** | ~590µs | Byte transmission (480µs) + handshake (110µs) |
| **Max Throughput** | ~1,695 keys/sec | Theoretical maximum |
| **CPU Usage** | 0.055% | At typical 5 keys/sec (60 WPM) |
| **Ring Buffer** | 32 bytes | Adequate for burst handling |

---

## Troubleshooting

### Diagnostic Tools

**Logic Analyzer:**
Capture KCLK and KDAT to verify:
- Bit period: ~60µs (±10µs acceptable)
- Bit order: 6→5→4→3→2→1→0→7 (rotated transmission)
- Handshake: 85µs LOW pulse on KDAT after 8 bits
- Active-low: KDAT LOW during data = logic 1

**Oscilloscope:**
Verify signal integrity:
- Power: 5.0V ±0.25V
- Idle levels: 4.5-5.5V (pulled HIGH)
- Active levels: < 0.8V (driven LOW)
- Handshake pulse: ≥85µs width (measure at 50% threshold)

### Common Issues and Solutions

**1. No Keyboard Response**

Symptoms: Keyboard appears dead, no scan codes received

Causes and Solutions:
- **Power**: Verify +5V on keyboard connector (30-80mA draw)
- **Pull-Ups**: Both KCLK and KDAT need 4.7kΩ pull-up resistors to +5V
- **Logic Inversion**: Verify active-low handling (LOW=1, HIGH=0)
- **Initial Sync**: Keyboard may be in sync mode (sending 1-bits), needs handshake to exit
- **Ground**: Check continuity between keyboard GND and converter GND

Debug Steps:
```
1. Multimeter: Measure VCC=5V, KCLK idle ~5V, KDAT idle ~5V
2. Power-cycle keyboard, watch for KCLK pulses (indicates sync mode)
3. If KCLK pulsing: Verify PIO sends 85µs handshake after each byte
4. Check for 0xF9 code after first valid handshake (sync recovered)
```

**2. Garbled Scan Codes**

Symptoms: Random or incorrect key codes, wrong characters

Causes and Solutions:
- **Bit Order Wrong**: Must receive 6-5-4-3-2-1-0-7 (rotated), not standard 0-7
- **De-Rotation Missing**: Verify `amiga_derotate_byte()` called on received data
- **Logic Not Inverted**: Check active-low handling in PIO (LOW=1, HIGH=0)
- **Wrong Clock Edge**: Verify sampling KDAT when KCLK falls (not rises)
- **Timing Drift**: Clock divider misconfigured, sampling off-center

Debug Steps:
```
1. Logic analyzer: Verify bit period 50-70µs (60µs ±17%)
2. Check PIO: First bit should be bit 6, last bit should be bit 7
3. Test: Press known key (e.g., 'A'=0x20), verify code before/after de-rotation
4. Verify active-low: KDAT LOW during transmission = logic 1-bit
```

**3. Keyboard Freezes After First Keys**

Symptoms: Initial keys work correctly, then keyboard stops responding

Causes and Solutions:
- **Missing Handshake**: PIO not sending 85µs KDAT LOW pulse after each byte
- **Pulse Too Short**: Handshake < 85µs not recognized, keyboard times out (143ms)
- **Wrong Pin Direction**: KDAT not configured as output during handshake
- **Handshake Too Long**: Excessive pulse duration (should be 85-200µs range)

Debug Steps:
```
1. Oscilloscope: Measure handshake pulse on KDAT after each byte
2. Verify pulse width: ≥85µs (85-150µs optimal range)
3. Check PIO state machine: Handshake loop executing (25 iterations × 4µs)
4. Test: If no handshake, keyboard enters resync within 143ms (continuous 1-bits)
```

**4. Continuous Resync Loop**

Symptoms: Keyboard sends continuous pulses, never transmits normal data

Causes and Solutions:
- **Handshake Not Detected**: Keyboard doesn't see LOW on KDAT
- **Pin Direction Error**: KDAT stays input during handshake (not driven)
- **Insufficient Pull-Down**: GPIO can't pull KDAT below ~1V threshold
- **Ground Issue**: Poor ground connection prevents voltage swing

Debug Steps:
```
1. Oscilloscope: Verify KDAT goes below 0.8V during handshake
2. Check pin config: KDAT must be OUTPUT during handshake, INPUT otherwise
3. Measure current: KDAT should sink ~1mA when driven LOW (5V / 4.7kΩ)
4. Watch for 0xF9 after valid handshake (indicates sync recovered)
```

**5. CAPS LOCK Not Working**

Symptoms: CAPS LOCK key press doesn't toggle state, LED issues

Causes and Solutions:
- **Protocol Misunderstanding**: Keyboard only sends on press, never release
- **Bit 7 Meaning**: Indicates LED state (0=ON, 1=OFF), not key up/down
- **Missing Synchronization**: Not comparing keyboard LED vs USB HID state
- **Timing Issue**: Release sent too quickly (needs 125ms hold for MacOS)

Debug Steps:
```
1. Capture scancode: Should be 0x62 (LED ON) or 0xE2 (LED OFF)
2. Verify logic: Only send toggle if keyboard state ≠ USB state
3. Test reboot case: Keyboard LED stays ON, USB resets OFF → sync on first press
4. Check timing: Release sent after 125ms (CAPS_LOCK_TOGGLE_TIME_MS)
```

### Signal Quality Checklist

Essential requirements for reliable operation:

- ✅ **Clean 5V power** with low ripple (<100mV p-p)
- ✅ **Solid ground** connection (check continuity: <1Ω)
- ✅ **Pull-up resistors** on KCLK and KDAT (4.7kΩ typical, 1kΩ-10kΩ range)
- ✅ **Short cables** (1-2 meters max, shorter better for signal integrity)
- ✅ **Active-low logic** correctly implemented (LOW=1, HIGH=0)
- ✅ **Handshake timing** verified (≥85µs, <143ms)

### Debug Techniques

**Logic Analyzer Capture**:
```
Configuration:
  Trigger: KCLK falling edge
  Signals: KCLK + KDAT
  Sample rate: 1 MHz minimum (≥16× bit rate)
  Duration: 50-100ms (captures multiple keys)

Analysis:
  - Bit period: 50-70µs (60µs typical)
  - Bit count: 8 bits per frame
  - Bit order: 6→5→4→3→2→1→0→7
  - Handshake: 85-150µs LOW on KDAT after bit 7
  - Inter-byte gap: 1-10ms between frames
```

**Oscilloscope Measurements**:
```
Voltage Levels:
  - VCC: 5.0V ±0.25V (4.75-5.25V acceptable)
  - KDAT idle: 4.5-5.5V (pulled HIGH via resistor)
  - KDAT active: < 0.8V (driven LOW)
  - KCLK idle: 4.5-5.5V (pulled HIGH)
  - KCLK active: < 0.8V (driven LOW)

Signal Quality:
  - Rise time: < 2µs (10% to 90%, faster better)
  - Fall time: < 2µs (90% to 10%, faster better)
  - Overshoot: < 15% of VCC (ringing dampened)
  - Handshake: Clean rectangle, no glitches
```

**Protocol Analyzer**:
```
Frame Decoding:
  1. Detect KCLK pulses (falling edges)
  2. Sample KDAT on each falling edge (8 times)
  3. Invert bits (active-low): LOW=1, HIGH=0
  4. De-rotate: [6 5 4 3 2 1 0 7] → [7 6 5 4 3 2 1 0]
  5. Separate bit 7 (up/down) from bits 6-0 (key code)
  6. Verify handshake: KDAT LOW for ≥85µs after bit 7
  7. Decode special codes: 0x78, 0xF9, 0xFA, 0xFC, 0xFD, 0xFE
```

---

## Related Documentation

- **[Hardware Setup](../hardware/README.md)** - Physical connections and wiring
- **[Keyboards](../keyboards/README.md)** - Supported Amiga keyboards
- **[Amiga Scancode Set](../../src/scancodes/amiga/README.md)** - Complete scancode reference
- **[Architecture](../advanced/architecture.md)** - Implementation details

---

## References

### Official Documentation

1. **Commodore Amiga Hardware Reference Manual** - Appendix H: Keyboard Interface Specification (1985-1992)
2. **[Amiga Hardware Reference - Keyboard Appendix](http://amigadev.elowar.com/read/ADCD_2.1/Hardware_Manual_guide/node0172.html)** - Official Amiga Developer CD 2.1 documentation
   - [Keyboard Communications](http://amigadev.elowar.com/read/ADCD_2.1/Hardware_Manual_guide/node0173.html) - Timing specifications (20µs phases, 60µs bit period)
   - [Out-of-Sync Condition](http://amigadev.elowar.com/read/ADCD_2.1/Hardware_Manual_guide/node0176.html) - Handshake timeout and resynchronization
   - [Special Codes](http://amigadev.elowar.com/read/ADCD_2.1/Hardware_Manual_guide/node017B.html) - Protocol special codes (0x78, 0xF9, etc.)
3. **Amiga Hardware Programmer's Guide** - Detailed keyboard implementation and timing specifications

### Protocol Validation

4. All timing specifications verified against Commodore hardware reference manual
5. Handshake requirements (85µs minimum, 143ms timeout) from official documentation  
6. Special codes (0x78, 0xF9, 0xFA, 0xFC, 0xFD, 0xFE) confirmed in Appendix H
7. Reset warning sequence and hard reset timing verified in hardware specifications

### Our Implementation

8. **[`keyboard_interface.pio`](../../src/protocols/amiga/keyboard_interface.pio)** - RP2040 PIO state machine for bit reception and handshake
9. **[`keyboard_interface.c`](../../src/protocols/amiga/keyboard_interface.c)** - C implementation, state machine, and CAPS LOCK synchronization
10. **[`keyboard_interface.h`](../../src/protocols/amiga/keyboard_interface.h)** - Protocol constants, inline bit de-rotation helper

---

**Status**: This protocol is fully implemented and tested. The converter reliably handles the bidirectional handshake, bit rotation, CAPS LOCK LED encoding, reset warning system, and resynchronization mode, providing complete compatibility with all Amiga keyboard models from A1000 through A4000.

