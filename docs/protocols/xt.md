# IBM XT Protocol

**Status**: ✅ Production | **Last Updated**: 28 October 2025

The IBM XT keyboard protocol represents the **foundation of PC keyboard communication**, introduced with the original IBM PC in 1981 and formalized with the IBM PC/XT in 1983. This protocol established fundamental concepts—scan codes, asynchronous communication, and keyboard-to-host data flow—that influenced every subsequent PC keyboard design, including modern USB keyboards.

What makes the XT protocol remarkable is its **extreme simplicity**: strictly unidirectional communication with no host commands, no acknowledgments, no LED control, and no error detection. While later protocols added sophisticated bidirectional features, the XT protocol's elegant minimalism made it extraordinarily robust and easy to implement—qualities that explain its longevity in embedded systems and retro computing projects.

---

## Table of Contents

- [Historical Context](#historical-context)
- [Protocol Overview](#protocol-overview)
- [Physical Interface](#physical-interface)
- [Communication Protocol](#communication-protocol)
- [Scan Code Set](#scan-code-set)
- [Protocol Operation](#protocol-operation)
- [Implementation Notes](#implementation-notes)
- [Troubleshooting](#troubleshooting)

---

## Historical Context

### The Birth of PC Keyboard Standards

**1981 - IBM PC**: When IBM released the original Personal Computer, they needed a simple, reliable way for keyboards to communicate with the system. The solution was an asynchronous serial protocol where the keyboard transmitted scan codes to the host computer. This design prioritized simplicity and cost-effectiveness—critical factors for a mass-market product.

**1983 - IBM PC/XT**: The PC/XT formalized the protocol specification and established the scan code standards that would become known as "Scan Code Set 1." While the protocol itself changed little from the original IBM PC, the XT's popularity cemented these conventions as the de facto standard for PC-compatible computers.

**1984 - The AT Revolution**: IBM introduced the PC/AT with a revolutionary bidirectional protocol, superseding XT. The new AT protocol (later standardized as PS/2) added host-to-keyboard commands, LED control, and error recovery. However, the XT protocol didn't disappear—its scan code set remained the default for compatibility, and countless XT keyboards continued in use throughout the 1980s.

**1980s - Wide Adoption**: As IBM PC-compatible computers proliferated, XT keyboards became ubiquitous. Every clone manufacturer implemented the XT protocol, and millions of keyboards used this simple, reliable communication method. Its robustness made it ideal for industrial environments where more complex protocols might fail.

**Legacy and Influence**: The XT protocol's influence extends far beyond its active use period. "Scan Code Set 1" remains supported by modern systems for backwards compatibility. The protocol's simplicity makes it popular in embedded systems, microcontroller projects, and retro computing. When learning keyboard protocols, the XT protocol is often the starting point precisely because it strips away all complexity to reveal the fundamental concepts.

---

## Protocol Overview

### Key Characteristics

The XT protocol is fundamentally different from both its predecessors (typewriter-style direct wiring) and successors (bidirectional protocols):

**Strictly Unidirectional**: Only the keyboard transmits data to the host computer. There is no mechanism for the host to send commands back to the keyboard. This means:
- No LED control (Caps Lock, Num Lock indicators don't exist on XT keyboards)
- No configuration commands (scan code set is fixed)
- No error recovery requests (no way to ask for retransmission)
- No acknowledgments (keyboard doesn't know if host received the data)

**Event-Driven Protocol with Synchronous Bit Transmission**: The keyboard sends scan codes asynchronously (when keys are pressed) but each bit is transmitted synchronously with a dedicated clock signal:
- **Clock Signal**: Keyboard provides CLOCK line for bit-level synchronization
- **Data Signal**: Separate DATA line carries scan codes
- **Protocol Level**: Asynchronous - keyboard transmits unsolicited events at any time
- **Bit Level**: Synchronous - each bit clocked by dedicated CLK line
- **Start Bit Detection**: 1 or 2 start bits depending on genuine IBM vs clone keyboards
- **LSB-First**: Data bits transmitted least-significant-bit first
- **No Parity/Stop**: No error detection or stop bits

**Frame Structure**: Each transmission consists of:
- **Start bit(s)**: 1 bit (clones) or 2 bits (genuine IBM XT)
- **Data bits**: 8 bits containing scan code
- **Total**: 9 bits per frame (8 data + 1 start bit in ISR)

**Timing Characteristics**:
- **Clock Rate**: ~10 kHz (approximately 100µs per bit)
- **Variable Timing**: Different keyboards may vary within ±20% tolerance
- **Self-Contained**: Keyboard generates timing, host must adapt

---

## Physical Interface

### Connector and Pinout

The XT keyboard uses a **5-pin DIN connector** with 180° orientation:

![IBM PC/XT/AT DIN-5 Connector Pinout](../images/connectors/kbd_connector_ibmpc.png)

*Image credit: [KbdBabel Vintage Keyboard Documentation](http://kbdbabel.org/conn/index.html)*

**Pinout** (looking at female socket on keyboard):
- Pin 1: CLK - Clock line (keyboard generates timing reference)
- Pin 2: DATA - Data line (keyboard transmits scan codes)
- Pin 3: N/C - Not connected or +5V reset on some keyboards
- Pin 4: GND - Ground (0V reference)
- Pin 5: VCC - Power supply (+5V from host computer)

**Important Connector Notes:**

The 5-pin DIN comes in two variants—**180° and 270°**—referring to the pin layout. They are **NOT mechanically interchangeable**:
- **180° DIN**: Pin 3 at top center (most common for XT keyboards)
- **270° DIN**: Pin arrangement rotated 90° (used by some AT keyboards and IBM terminals)

Always verify which type your keyboard uses before ordering connectors or cables. The plug will not physically fit the wrong socket orientation.

**Availability**: More difficult to source than PS/2 connectors, check vintage electronics suppliers or salvage from old motherboards.

---

**IBM Terminal 270° DIN-5 Connector** - IBM 3179, 318x, 319x Terminals

![IBM Terminal 270° DIN-5 Connector Pinout](../images/connectors/kbd_connector_ibm3179_318x_319x.png)

*Image credit: [KbdBabel Vintage Keyboard Documentation](http://kbdbabel.org/conn/index.html)*

Some IBM terminal keyboards use a 270° DIN-5 connector with different pinout:

**Pinout**:
- Pin 1: VCC - Power supply (+5V)
- Pin 2: GND - Ground
- Pin 3: PE - Protective Earth (not used by converter)
- Pin 4: DATA - Data line
- Pin 5: CLK - Clock line

**Availability**: Rare connector, may require custom cables or adapters from terminal keyboard cables.

### Electrical Characteristics and Pin Functions

**Pin Assignments** (looking at female socket on keyboard):
- **Pin 1 - CLK**: Clock signal generated by keyboard for synchronization (~10 kHz square wave during transmission)
- **Pin 2 - DATA**: Scan code transmission line (keyboard → host), open-drain output with host pull-up resistor (1-10kΩ typical)
- **Pin 3 - N/C**: Not connected (or internal reset on some keyboards)
- **Pin 4 - GND**: Ground reference (0V)
- **Pin 5 - VCC**: +5V power supply from host

**Logic Levels:**
- **HIGH (logic 1)**: 2.0V to 5.5V (typically 5V with pull-up resistor)
- **LOW (logic 0)**: 0V to 0.8V
- **Signal Levels**: TTL compatible (0-0.8V low, 2.0-5.5V high)

**Signal Characteristics:**
- **Open-Drain**: DATA line uses open-drain (or open-collector) output
- **Pull-Up**: Host provides pull-up resistor on DATA line (typically 1-10kΩ, 4.7kΩ typical)
- **Pull-Up Location**: Resistor on host motherboard, not in keyboard
- **Idle State**: DATA line HIGH when keyboard not transmitting

**Power Requirements:**
- **Voltage**: +5V ±5% (4.75V to 5.25V)
- **Current**: 50-150mA typical (varies by keyboard model)
- **Startup Peak**: Up to 275mA during power-on self-test

**Signal Integrity:**
- **Cable Length**: Typically works well up to 1-2 meters (source: hardware testing)
- **Noise Immunity**: Reasonable for office environments
- **EMI**: Unshielded cables acceptable for most installations

---

## Communication Protocol

### Frame Structure

The XT protocol uses **9-bit frames** consisting of start bit(s) and data bits:

```
Frame Format (9 bits total):
- Start bit(s): 1 bit (clone) or 2 bits (genuine IBM)
- Data bits: 8 bits containing scan code (LSB-first)

PIO Configuration:
- ISR shift: 9 bits (captures start + data)
- Autopush: Enabled at 9-bit boundary
- Direction: Right-shift (LSB-first reception)
```

**Bit Order - LSB First**:

To transmit scancode 0x1C (00011100 binary), the keyboard sends:
```
Order: Bit 0 → Bit 1 → Bit 2 → Bit 3 → Bit 4 → Bit 5 → Bit 6 → Bit 7
Value:   0   →   0   →   1   →   1   →   1   →   0   →   0   →   0
```

This is the opposite of "MSB-first" protocols and must be handled correctly in the receiver.

### Keyboard-to-Host Transmission

The transmission method differs between genuine IBM keyboards and clone keyboards:

#### Genuine IBM XT Keyboards (Two Start Conditions)

Authentic IBM keyboards use a sophisticated two-start-bit sequence (RTS/CTS):

```
Timing Diagram:
    ____       _ 1 _ 2 _ 3 _ 4 _ 5 _ 6 _ 7 _ 8 _ 9 _______
CLK     \_____/ \_/ \_/ \_/ \_/ \_/ \_/ \_/ \_/ \_/   
             _____ ___ ___ ___ ___ ___ ___ ___ ___
DAT ________/     \___X___X___X___X___X___X___X___\_______
        ^   ^  S    0   1   2   3   4   5   6   7
        RTS CTS

Legend:
RTS = Request-to-Send (first start condition, CLK LOW + DATA LOW)
CTS = Clear-to-Send (second start condition, CLK transitions)
S   = Start bit (DATA goes HIGH momentarily)
0-7 = Data bits transmitted LSB-first
```

**Transmission Sequence:**

1. **Request-to-Send (RTS)**: Keyboard pulls both CLK and DATA LOW (~40µs)
2. **Clear-to-Send (CTS)**: Keyboard releases CLK (goes HIGH)
3. **Start Bit**: DATA briefly goes HIGH then LOW
4. **Data Bits**: 8 bits transmitted with CLK pulses, DATA sampled on falling CLK edge
5. **Return to Idle**: Both CLK and DATA return HIGH

**Why Two Start Bits?**: This handshake ensures the host is ready to receive data and provides synchronization between keyboard and host timing domains.

#### Clone XT Keyboards (Single Start Condition)

Most clone keyboards simplified the protocol to use a single start condition:

```
Timing Diagram:
    ____________ 1 _ 2 _ 3 _ 4 _ 5 _ 6 _ 7 _ 8 _ 9 _______
CLK             \_/ \_/ \_/ \_/ \_/ \_/ \_/ \_/ \_/     
    ______________ ___ ___ ___ ___ ___ ___ ___ ___ _______
DAT               \___X___X___X___X___X___X___X___/
               S    0   1   2   3   4   5   6   7

Legend:
S   = Start condition (CLK falls, DATA transitions)
0-7 = Data bits transmitted LSB-first
```

**Simplified Sequence:**

1. **Start Condition**: CLK falls while DATA transitions
2. **Data Bits**: 8 bits transmitted with CLK pulses
3. **Return to Idle**: Both lines return HIGH

**Why Simplify?**: Clone manufacturers reduced component count and simplified firmware, accepting slightly reduced robustness in exchange for lower costs.

### Timing Specifications

**Clock Timing (Keyboard-Generated)**:

```
Frequency: ~10 kHz (approximately, not precisely specified)
Period: ~100µs per bit transmission
Low Phase: ~40µs (clock low period)
High Phase: ~60µs (clock high period)
Tolerance: ±20% (keyboards vary significantly)
```

**Data Timing**:

```
Setup Time: Data valid before CLK rising edge (minimum ~5µs before edge)
Hold Time: Data stable after CLK rising edge (minimum ~5µs after edge)
Inter-byte Gap: 1-10ms typical between consecutive scan codes
```

**Reset and Initialization Timing**:

```
Soft Reset Sequence:
- Host CLK Low: 20ms minimum to signal reset request
- Keyboard DATA High Response: Within 1ms of CLK release (acknowledgment)
- BAT Duration: 200-500ms typical for Basic Assurance Test
- BAT Response: 0xAA sent immediately after successful self-test
```

**Critical Timing Notes**:

The ~10 kHz clock rate is **not tightly specified**. Different keyboards operate at different frequencies within a wide tolerance band. Host implementations must accommodate this variation by:
- Using oversampling (sampling at much higher frequency than bit rate)
- Not making assumptions about exact timing
- Being tolerant of clock rate variations between keyboards

### Detection Challenge: Genuine vs Clone

The XT protocol has a unique implementation challenge: distinguishing between genuine IBM keyboards (two start bits) and clone keyboards (single start bit).

**The Problem**:

- Start bit pulse width: ~40µs (both genuine and clone)
- Typical bit period: ~100µs
- If sampling at bit period rate (~100µs), you miss the second start bit
- Need to detect presence/absence of second start bit within first 40µs window

**The Solution - Fast Sampling**:

Our implementation uses 10µs sampling (see [Why 10µs Sampling is Critical](#why-10µs-sampling-is-critical) below for detailed explanation):

```
Minimum Pulse Detection: 10µs (not 100µs typical bit period)
RP2040 System Clock: 125 MHz
Target Sampling: (1000 / 10µs) × 5 samples = 500 kHz
Clock Divider: 125,000 kHz / 500 kHz = 250
PIO Cycle Time: 2µs per cycle

Detection Window:
- Start bit width: ~40µs
- Sampling interval: 10µs  
- Samples per start bit: 4 samples (40µs / 10µs)
```

**Detection Logic**:

```
On CLK falling edge:
  Sample DATA immediately (within 10µs window)
  
  If DATA = LOW:
    → Genuine IBM XT detected (two start bits)
    → Discard first start bit (RTS)
    → Wait for CLK rising edge
    → Wait for CLK falling edge again
    → Read second start bit (CTS)
    → Proceed with 8 data bits
    
  If DATA = HIGH:
    → Clone XT detected (single start bit)
    → Use this sample as start bit
    → Proceed with 8 data bits
```

This elegant solution allows a single receiver implementation to work with both genuine and clone keyboards automatically.

---

## Scan Code Set

The XT protocol uses a **fixed scan code set** that became the foundation for all PC keyboard scan codes.

**Scancode Structure:**

```
Make Code (Key Press):   0x01 - 0x53
Break Code (Key Release): Make Code + 0x80

Examples:
0x1E (A key pressed)  →  0x9E (A key released)  = 0x1E + 0x80
0x2A (L Shift press)  →  0xAA (L Shift release) = 0x2A + 0x80
0x1C (Enter press)    →  0x9C (Enter release)   = 0x1C + 0x80
```

**Simple Algorithm**:

```c
bool is_break_code(uint8_t scancode) {
    return (scancode & 0x80) != 0;  // Bit 7 set = break code
}

uint8_t get_base_key(uint8_t scancode) {
    return scancode & 0x7F;  // Clear bit 7 to get base key code
}
```

**Special Codes:**

| Code | Value | Description |
|------|-------|-------------|
| **BAT Passed** | 0xAA | Self-test successful (sent once at power-on) |
| **BAT Failed** | 0xFC | Self-test failed (rare, indicates hardware problem) |

**BAT (Basic Assurance Test)**:
- Performed during keyboard power-on
- Tests ROM checksum, RAM, key matrix
- 0xAA: Everything OK, keyboard ready
- 0xFC: Hardware failure detected

**Note**: For complete scancode tables with all key mappings, see the **[Scancode Set 1](../../src/scancodes/set1/README.md)** documentation.

---

## Protocol Operation

### Power-On Sequence

```
Power-On Initialization:
1. Keyboard applies power from Pin 5 (VCC)
2. Internal microcontroller boots and runs self-test (~200-500ms)
3. Tests performed:
   - ROM checksum verification
   - RAM read/write test  
   - Key matrix continuity check
4. If all tests pass:
   - Keyboard sends 0xAA (BAT Passed)
   - Enters normal operation mode
5. If any test fails:
   - Keyboard sends 0xFC (BAT Failed)
   - May blink LED indicators (if present)
   - May enter degraded mode or halt
```

### Soft Reset Protocol

The XT keyboard protocol **does support soft reset** via the CLK line, as documented in the IBM PC XT Technical Reference (April 1984, p5-98):

**Reset Sequence:**

1. **Host Request**: Pull CLK LOW for ~20ms (host-initiated reset signal)
2. **Keyboard ACK**: Keyboard responds by pulling DATA HIGH (acknowledgment)
3. **Host Release**: Host releases CLK (returns to input mode, keyboard controls timing)
4. **Keyboard Ready**: Keyboard pulls DATA LOW (ready for transmission)
5. **Self-Test**: Keyboard performs BAT (Basic Assurance Test)
6. **BAT Response**: Keyboard transmits 0xAA (passed) or 0xFC (failed)

**PIO Implementation:**

Our converter implements this protocol in the PIO state machine ([`keyboard_interface.pio`](../../src/protocols/xt/keyboard_interface.pio)):

```pio
softReset:
    set pindirs 2 [1]       ; Set CLK pin to output mode
    set pins, 0             ; Pull CLK LOW (soft reset request, ~20ms hold)
    
    wait 1 pin 0 [1]        ; Wait for DATA HIGH (keyboard ACK)
    
    set pindirs 1  [1]      ; Set CLK to input (release to keyboard)
    set pindirs, 0          ; Set DATA to input (allow keyboard control)
    
    wait 0 pin 0 [1]        ; Wait for DATA LOW (ready for transmission)
```

**When Soft Reset Occurs:**

- **Power-on initialization**: After keyboard powers up and brings CLK/DATA HIGH
- **BAT failure recovery**: When keyboard returns invalid BAT response
- **Communication error**: When invalid start bit detected (triggers `pio_restart()`)
- **State machine reset**: After protocol errors or frame synchronization loss

### Normal Operation

```
Continuous Scan Loop:
1. Keyboard continuously scans key matrix
2. On key state change:
   a. Key Press: Generate make code (0x01-0x53)
   b. Key Release: Generate break code (make code + 0x80)
3. Transmit scan code using self-clocked protocol
4. Return to step 1 (no waiting for host acknowledgment)

Key Points:
- Keyboard operates independently of host
- No flow control or acknowledgments
- Host must keep up with transmission rate
- Typical key repeat rate: 10-15 characters per second
- Burst rate limited by mechanical key switch debounce
```

---

## Implementation Notes

### Why 10µs Sampling is Critical {#why-10µs-sampling-is-critical}

The 10µs sampling rate is essential for distinguishing genuine IBM XT keyboards (two start bits) from clone keyboards (single start bit):

- **Start bit width**: ~40µs on genuine IBM keyboards
- **Sampling interval**: 10µs provides 4 samples within the 40µs window
- **Detection capability**: Fast enough to catch DATA LOW state of first start bit
- **Oversampling factor**: 10× faster than bit period (10µs vs 100µs)

Without this fast sampling, both genuine and clone keyboards would appear identical when sampled at the bit period rate (~100µs), making automatic detection impossible. This is why slower sampling (e.g., at the bit period rate of ~100µs) fails:

**At 100µs sampling (bit period rate)**:
- First sample: Misses the ~40µs RTS pulse entirely
- Second sample: Sees DATA HIGH (after RTS ends)
- Result: Genuine and clone keyboards look identical

**At 10µs sampling (our implementation)**:
- First sample (at ~10µs): Catches DATA LOW on genuine XT (RTS pulse)
- First sample (at ~10µs): Sees DATA HIGH on clone XT (no RTS pulse)
- Result: Can distinguish keyboard types and align frame correctly

### Host-Side Implementation

The converter uses RP2040's PIO hardware to handle the XT protocol timing automatically:

**PIO State Machine Configuration:**

```c
// From keyboard_interface.c
// See: ../../src/protocols/xt/keyboard_interface.c
pio_sm_config c = keyboard_interface_program_get_default_config(offset);

// Configure 9-bit reception (8 data + 1 start bit)
sm_config_set_in_shift(&c, true, true, 9);  // Right-shift, autopush, 9 bits

// Pin assignments
sm_config_set_in_pins(&c, keyboard_data_pin);  // DATA on base pin
sm_config_set_jmp_pin(&c, keyboard_clock_pin); // CLK on base pin + 1

// Clock divider for 10µs sampling (genuine vs clone detection)
float clock_div = calculate_clock_divider(10);
sm_config_set_clkdiv(&c, clock_div);
```

**IRQ Handler:**

```c
static void __isr keyboard_input_event_handler() {
    // Read 9-bit frame from PIO FIFO
    io_ro_32 data_cast = keyboard_pio->rxf[keyboard_sm] >> 23;
    uint16_t data = (uint16_t)data_cast;

    // Extract start bit and data byte
    uint8_t start_bit = data & 0x1;
    uint8_t data_byte = (uint8_t)((data_cast >> 1) & 0xFF);

    // Validate start bit (must be 1 for XT protocol)
    if (start_bit != 1) {
        // Invalid frame - restart PIO
        keyboard_state = UNINITIALISED;
        pio_restart(keyboard_pio, keyboard_sm, keyboard_offset);
        return;
    }

    // Process valid scan code
    keyboard_event_processor(data_byte);
}
```

**Key Implementation Details:**

- PIO handles all timing automatically (no software delays)
- 10µs sampling rate enables genuine vs clone keyboard detection
- Start bit validation ensures frame synchronization
- Ring buffer queues scan codes for main loop processing
- Non-blocking architecture (IRQ → ring buffer → main task)

### RP2040 PIO Configuration

```c
// Clock divider calculation for 10µs minimum pulse detection
float calculate_clock_divider(uint32_t target_us) {
    // RP2040 system clock: 125 MHz = 125,000 kHz
    // Target: 10µs minimum pulse detection
    // Desired frequency: (1000 / 10µs) × 5 samples = 500 kHz
    // Divider: 125,000 / 500 = 250
    
    const uint32_t sys_clock_khz = clock_get_hz(clk_sys) / 1000;
    const uint32_t target_freq_khz = (1000 / target_us) * 5;
    
    return (float)sys_clock_khz / (float)target_freq_khz;
}
```

See [Why 10µs Sampling is Critical](#why-10µs-sampling-is-critical) above for the technical rationale behind this timing requirement.

---

## Troubleshooting

### Common Issues

**1. No Keyboard Response**

Symptoms: No scan codes received, keyboard appears dead

Causes and Solutions:
- **Power Issue**: Check +5V on Pin 5, verify current capacity (150mA minimum)
- **Ground Issue**: Verify solid ground connection on Pin 4
- **Pull-Up Missing**: Ensure DATA line has pull-up resistor (1-10kΩ)
- **Reset State**: Keyboard may be in continuous reset—check for stuck CLK line
- **Failed BAT**: Listen for error beeps or watch for LED error patterns

Debug Steps:
```
1. Measure VCC (Pin 5): Should be 5V ±5%
2. Measure GND (Pin 4): Should be 0V
3. Measure DATA idle: Should be ~5V (pulled up)
4. Check for any signals on CLK: Activity indicates keyboard trying to communicate
5. Power cycle keyboard and watch for 0xAA (BAT passed)
```

**2. Garbled Scan Codes**

Symptoms: Random or incorrect key codes, garbage data

Causes and Solutions:
- **Wrong Bit Order**: Verify LSB-first reception, not MSB-first
- **Clock Edge**: Ensure sampling on **rising edge**, not falling edge  
- **Timing Issues**: Check clock divider calculation, may be sampling too fast/slow
- **Electrical Noise**: Add decoupling capacitor near keyboard connector (0.1µF)
- **Cable Length**: Try shorter cable (< 1 meter)

Debug Steps:
```
1. Capture signal with logic analyzer
2. Verify clock frequency (~10 kHz, ±20%)
3. Check data transitions align with clock edges
4. Measure signal rise/fall times (should be clean, < 1µs)
5. Add 0.1µF capacitor across VCC/GND at connector
```

**3. Missing Key Events**

Symptoms: Some key presses don't register, intermittent operation

Causes and Solutions:
- **Interrupt Latency**: Host can't keep up—reduce other interrupt loads
- **Ring Buffer Overflow**: Increase ring buffer size (current: 32 bytes)
- **Clock Glitches**: Poor signal integrity causing false triggers
- **Bad Connections**: Intermittent contact in cable or connector

Debug Steps:
```
1. Monitor ring buffer fill level—if frequently full, increase size
2. Check interrupt response time with oscilloscope
3. Try different cable (worn cables common problem)
4. Check connector pins for corrosion or damage
5. Test with different keyboard to isolate hardware vs firmware issue
```

**4. Initialization Problems**

Symptoms: Keyboard doesn't send BAT (0xAA), no initial response

Causes and Solutions:
- **Insufficient Delay**: Some keyboards need > 500ms to complete BAT
- **Soft Reset Required**: XT keyboards require soft reset (CLK LOW for 20ms) after power-on to trigger BAT transmission
- **Failed Self-Test**: Keyboard hardware problem (ROM, RAM, matrix failure)

Debug Steps:
```
1. Increase power-on delay to 1000ms
2. Verify soft reset sequence executes (CLK pulled LOW, DATA goes HIGH)
3. Listen for audible error beeps
4. Watch for LED blink patterns indicating error code
5. Try keyboard on known-good system to verify hardware OK
```

### Signal Quality Checklist

Essential signal quality requirements:

- ✅ **Clean 5V power** with low ripple (<50mV peak-to-peak)
- ✅ **Solid ground** between keyboard and host (check continuity)
- ✅ **Pull-up resistor** on DATA line (1kΩ to 10kΩ, 4.7kΩ typical)
- ✅ **Short cable** (< 2 meters, shorter better)
- ✅ **Clean signals** with fast rise/fall times (< 1µs)
- ✅ **Decoupling capacitor** near connector (0.1µF ceramic)
- ✅ **ESD protection** on signal lines (optional but recommended)

### Debug Techniques

**Logic Analyzer Capture**:

```
Trigger: Falling edge on CLK
Capture: CLK + DATA simultaneously
Duration: 10ms (captures several scan codes)

Analysis:
- Clock frequency: Should be ~10 kHz (100µs period)
- Clock duty cycle: ~40% low, ~60% high (not critical, but typical)
- Data transitions: Should be clean, no glitches
- Data alignment: DATA stable before CLK rising edge
- Inter-byte gap: 1-10ms between scan codes
```

**Oscilloscope Measurements**:

```
Voltage Levels:
- VCC: 5.0V ±0.25V
- DATA idle: 4.0V to 5.0V (pulled up)
- DATA low: < 0.8V
- DATA high: > 2.0V

Signal Quality:
- Rise time: < 1µs (10% to 90%)
- Fall time: < 1µs (90% to 10%)
- Overshoot: < 10% of VCC
- Ringing: < 0.5V amplitude, damped within 2 cycles
```

---

## Related Documentation

- **[Hardware Setup](../hardware/README.md)** - Physical connections and wiring
- **[Keyboards](../keyboards/README.md)** - Supported XT keyboards  
- **[Scancode Set 1](../../src/scancodes/set1/README.md)** - XT scancode tables
- **[Architecture](../advanced/architecture.md)** - Implementation details

---

## References

### Official Documentation

1. **[IBM PC XT Technical Reference (April 1984)](https://bitsavers.org/pdf/ibm/pc/xt/6361459_PC_XT_Technical_Reference_Apr84.pdf)** - Official XT specification, keyboard protocol on p5-32 and p5-98
2. **[IBM PC Technical Reference (April 1983)](https://bitsavers.org/pdf/ibm/pc/xt/1502237_PC_XT_Technical_Reference_Apr83.pdf)** - Original PC/XT specification (25MB PDF)
3. **[IBM PC Technical Reference Manual (1981)](https://bitsavers.org/pdf/ibm/pc/)** - Original IBM PC documentation

### Implementation References

4. **[8048 XT Keyboard Firmware](https://github.com/Halicery/8042/blob/main/8048_XT_INTERN.TEXT)** - Reverse-engineered 8048 microcontroller firmware from genuine IBM XT keyboard
5. **[TMK Keyboard Firmware](https://github.com/tmk/tmk_keyboard)** - XT protocol reference implementation
6. **[kbdbabel Project](https://github.com/tmk/kbdbabel)** - XT to PS/2 converter with timing specifications

### Our Implementation

7. **[`keyboard_interface.pio`](../../src/protocols/xt/keyboard_interface.pio)** - RP2040 PIO state machine for XT protocol
8. **[`keyboard_interface.c`](../../src/protocols/xt/keyboard_interface.c)** - C implementation, IRQ handlers, and state machine

---

**Status**: This protocol is fully implemented and tested. The converter reliably detects and handles both genuine IBM XT keyboards (with two start bits) and clone keyboards (with single start bit), providing automatic compatibility without user configuration.
