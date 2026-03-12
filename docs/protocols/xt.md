# IBM XT Protocol

The IBM XT keyboard protocol was used in the original IBM PC and PC/XT systems. The protocol uses scan codes, synchronous serial communication with a dedicated clock signal, and unidirectional keyboard-to-host data flow.

The XT protocol uses strictly unidirectional communication with no host commands, no acknowledgments, no LED control, and no error detection.

---

## Historical Context

The protocol is event-driven: key press generates a "make code", key release generates a "break code" (make code with bit 7 set, equivalent to adding 0x80). These scancodes were later designated "Scancode Set 1" when AT keyboards introduced support for multiple scancode sets.

**Type 1 vs Type 2 XT Keyboards**

IBM produced two XT keyboard variants with different reset mechanisms:

- **Type 1 (Early PC)**: Uses pin 3 of the 5-pin DIN connector for hardware reset
- **Type 2 (Later PC/XT)**: Uses a soft reset sequence on CLOCK and DATA lines—no dedicated reset pin

Data transmission, timing, frame structure, and scancodes are identical between variants. The reset mechanism is the only difference.

This converter implements the Type 2 soft reset method (pulling CLOCK and DATA LOW during initialisation). Type 1 keyboards have not been tested—compatibility with Type 1 reset mechanism cannot be confirmed.

**Note**: The Type 1/Type 2 distinction is documented only for IBM keyboards. Whether other manufacturers (clones, third-party keyboards) implemented Type 1, Type 2, or some other variation is not known. The Type 2 soft reset mechanism appears to be the most common implementation based on available documentation.

**Successor Protocol**

The IBM PC/AT introduced a bidirectional protocol with command support, LED control, and error detection. AT keyboards can switch scancode sets and respond to host requests. See **[AT/PS2 Protocol](at-ps2.md)** for the bidirectional specification.

---

## Protocol Overview

### Key Characteristics

The XT protocol sits somewhere between the simple typewriter-style direct wiring that came before it and the more sophisticated bidirectional protocols that followed.

**Strictly Unidirectional**

Data flows one way only—keyboard to host. There's no mechanism for the host to send commands back to the keyboard. This means:

- No host LED control — Caps Lock and Num Lock indicators, if present, are locally managed by the keyboard controller and cannot be controlled by the host
- No configuration commands (scan code set is fixed, no changing it)
- No error recovery (no way to ask for retransmission if something goes wrong)
- No acknowledgments (keyboard has no idea if the host actually received the data)

**Event-Driven Synchronous Serial Communication**

The protocol is synchronous—it uses a dedicated clock signal for data transmission. The keyboard sends scancodes whenever keys are pressed (event-driven), but all data transmission is synchronised to the clock signal:

- **Clock Signal**: Keyboard provides a CLOCK line for bit-level synchronisation
- **Data Signal**: Separate DATA line carries the actual scancodes
- **Synchronous Transmission**: Each bit is clocked out on a dedicated CLOCK line (not a predetermined baud rate)
- **Event-Driven Protocol**: Keyboard transmits whenever key events occur (no fixed timing between scancodes)
- **Start Bit Detection**: 1 or 2 start bits depending on whether it's a genuine IBM or clone keyboard
- **LSB-First**: Data bits transmitted least-significant-bit first
- **No Parity/Stop**: No error detection or stop bits

**Frame Structure**

Frame length depends on keyboard type:

- **Genuine IBM XT**: start(0) + start(1) + 8 data bits = 10 bits total. The PIO discards start(0) (DATA LOW at the RTS clock edge) and produces a normalised 9-bit frame (start(1) + 8 data bits) delivered to the interrupt handler.
- **Clone XT**: 1 start bit + 8 data bits = 9 bits total. The PIO passes these through as the normalised 9-bit frame.

In either case, the interrupt handler receives a normalised 9-bit PIO frame: 1 start bit followed by 8 data bits.

**Timing Characteristics**

- **Clock Rate**: ~10 kHz (roughly 100µs per bit)
- **Variable Timing**: Different keyboards can vary within ±20% tolerance
- **Self-Contained**: Keyboard generates its own timing, host needs to keep up

---

## Physical Interface

### Connector and Pinout

XT keyboards use a **5-pin DIN connector** with 180° orientation:

![IBM PC/XT/AT DIN-5 Connector Pinout](../images/connectors/kbd_connector_ibmpc.png)

**Pinout** (looking at the female socket on the keyboard):

- Pin 1: CLOCK - Clock line (keyboard generates timing reference)
- Pin 2: DATA - Data line (keyboard transmits scancodes)
- Pin 3: N/C - Not connected or +5V reset on some keyboards
- Pin 4: GND - Ground (0V reference)
- Pin 5: VCC - Power supply (+5V from host computer)

**Important Connector Notes:**

The 5-pin DIN comes in two variants—**180° and 240°**—and they're **NOT mechanically interchangeable**:

- **180° DIN**: Pin 3 at top center (standard for PC/XT/AT keyboards)
- **240° DIN**: Pin arrangement rotated 60° from the 180° layout

Always verify which type your keyboard uses before ordering connectors or cables. The plug simply won't fit the wrong socket orientation.

### Electrical Characteristics

**Logic Levels:**

- **HIGH (logic 1)**: 2.0V to 5.5V (typically 5V with pull-up resistor)
- **LOW (logic 0)**: 0V to 0.8V
- **Signal Levels**: TTL compatible (0-0.8V low, 2.0-5.5V high)

**Signal Characteristics:**

- **Open-Drain**: DATA line uses open-drain (or open-collector) output
- **Pull-Up**: Host has pull-up resistor on DATA line (typically 1-10kΩ, 4.7kΩ typical)
- **Pull-Up Location**: Resistor on host motherboard, not in keyboard
- **Idle State**: DATA line LOW for genuine IBM XT keyboards (keyboard actively drives low between transmissions); DATA line HIGH via pull-up for clone keyboards

**Power Requirements:**

- **Voltage**: +5V ±5% (4.75V to 5.25V)
- **Current**: 50-150mA typical (varies by keyboard model)
- **Startup Peak**: Up to 275mA during power-on self-test

**Signal Integrity:**

- **Cable Length**: Keep cables as short as practical; longer cables increase susceptibility to noise
- **Noise Immunity**: Use shielded cables where possible to reduce interference
- **EMI**: Avoid routing cables near high-frequency or power sources

---

## Communication Protocol

### Frame Structure

Genuine IBM XT keyboards use a **10-bit** frame; clone keyboards use a **9-bit** frame. The PIO normalises both to a 9-bit frame, so the interrupt handler always receives the same format regardless of keyboard type.

Throughout this document, the two genuine IBM XT start bits are named by their sampled DATA values: **start(0)** is the first bit (DATA LOW = 0, the RTS framing bit, discarded by the PIO) and **start(1)** is the second (DATA HIGH = 1, the actual start bit that opens the 9-bit normalised frame). Clone keyboards have only a single start bit, equivalent to start(1).

```text
Frame format:
  Genuine IBM XT:  10 bits — start(0) [discarded by PIO] + start(1) + 8 data bits
  Clone XT:         9 bits — 1 start bit + 8 data bits

Normalised PIO frame (both types, 9 bits):
  Bit 0:   Start bit (DATA HIGH, always 1)
  Bits 1–8: Scancode, LSB-first

PIO Configuration:
  ISR shift: 9 bits (captures start + data)
  Autopush: Enabled at 9-bit boundary
  Direction: Right-shift (LSB-first reception)
```

**Bit Order - LSB First**:

To transmit scancode 0x1C (00011100 binary), the keyboard sends:

```yaml
Order: Bit 0 → Bit 1 → Bit 2 → Bit 3 → Bit 4 → Bit 5 → Bit 6 → Bit 7
Value: 0   →   0   →   1   →   1   →   1   →   0   →   0   →   0
```

This is the opposite of "MSB-first" protocols and must be handled correctly in the receiver.

### Keyboard-to-Host Transmission

The transmission method differs between genuine IBM keyboards and clone keyboards:

#### Genuine IBM XT Keyboards (Two Start Bits)

Authentic IBM keyboards use a two-start-bit framing sequence described in the IBM XT Technical Reference. The terms **RTS** (request-to-send) and **CTS** (clear-to-send) are IBM's own terms for the way the keyboard signals it is about to transmit — they describe events on the CLOCK and DATA lines only. They are not RS-232 hardware flow-control signals; there is no separate RTS/CTS pin on the keyboard connector.

**RTS**: The keyboard pulls CLOCK LOW to signal intent to transmit. DATA is already LOW in idle state, so this first CLOCK falling edge samples DATA LOW — this is start(0). **CTS**: The keyboard immediately releases DATA; the pull-up brings DATA HIGH, confirming the framing sequence has begun. The next CLOCK falling edge samples DATA HIGH — this is start(1), the actual start bit that opens the 9-bit normalised frame.

```text
Timing Diagram:
      ____       _ 1 _ 2 _ 3 _ 4 _ 5 _ 6 _ 7 _ 8 _ 9 _______
CLOCK     \_____/ \_/ \_/ \_/ \_/ \_/ \_/ \_/ \_/ \_/
               _____ ___ ___ ___ ___ ___ ___ ___ ___
DATA  ________/     \___X___X___X___X___X___X___X___\_______
          ^   ^   S    0   1   2   3   4   5   6   7
          RTS CTS

Legend:
RTS = Request-to-send: keyboard pulls CLOCK LOW; DATA is already LOW (idle state), sample reads start(0) = 0
CTS = Clear-to-send: keyboard releases DATA, pull-up brings DATA HIGH
S   = Start bit, start(1): DATA HIGH at first clock edge; PIO captures this to begin the 9-bit normalised frame
0-7 = Data bits transmitted LSB-first
```

**Transmission Sequence:**

1. **RTS**: Keyboard pulls CLOCK LOW. DATA is already LOW in idle state (start(0) = 0). PIO samples DATA LOW and detects genuine IBM XT framing.
2. **CTS**: Keyboard immediately releases DATA; pull-up brings DATA HIGH.
3. **Start bit**: CLOCK pulses again with DATA HIGH (start(1) = 1). PIO begins the 9-bit normalised frame here.
4. **Data Bits**: 8 bits transmitted LSB-first, with DATA sampled on each falling CLOCK edge.
5. **Return to Idle**: CLOCK returns HIGH; DATA returns to the genuine IBM XT idle level (LOW).

**Note:** XT is strictly unidirectional — data flows keyboard to host only. The RTS/CTS framing sequence is entirely keyboard-initiated; the host has no mechanism to send data back.

#### Clone XT Keyboards (Single Start Condition)

Clone keyboards simplified the protocol to use a single start condition:

```text
Timing Diagram:
      ____________ 1 _ 2 _ 3 _ 4 _ 5 _ 6 _ 7 _ 8 _ 9 _______
CLOCK             \_/ \_/ \_/ \_/ \_/ \_/ \_/ \_/ \_/
      ______________ ___ ___ ___ ___ ___ ___ ___ ___ _______
DATA                \___X___X___X___X___X___X___X___/
                 S    0   1   2   3   4   5   6   7

Legend:
S   = Start condition (CLOCK falls, DATA transitions)
0-7 = Data bits transmitted LSB-first
```

**Simplified Sequence:**

1. **Start Condition**: CLOCK falls whilst DATA transitions
2. **Data Bits**: 8 bits transmitted with CLOCK pulses
3. **Return to Idle**: Both lines return HIGH

### Timing Specifications

**Clock Timing (Keyboard-Generated)**:

The keyboard generates its own clock at roughly 10 kHz:

```text
Frequency: ~10 kHz (approximately, not precisely specified)
Period: ~100µs per bit transmission
Low Phase: ~40µs (clock low period)
High Phase: ~60µs (clock high period)
Tolerance: ±20% (keyboards vary significantly)
```

**Data Timing**:

```text
Setup Time: Data valid before CLOCK falling edge (minimum ~5µs before edge)
Hold Time: Data stable after CLOCK falling edge (minimum ~5µs after edge)
Inter-byte Gap: 1-10ms typical between consecutive scan codes
```

**Reset and Initialisation Timing**:

```text
Soft Reset Sequence:
- Host CLOCK Low: 20ms minimum to signal reset request
- Keyboard DATA High Response: Within 1ms of CLOCK release (acknowledgment)
- BAT Duration: 200-500ms typical for Basic Assurance Test
- BAT Response: 0xAA sent immediately after successful self-test
```

### Detection Challenge: Genuine vs Clone

The implementation needs to distinguish between genuine IBM keyboards (which use two start bits) and clone keyboards (which use just one). Both types look identical if you're not careful about how you sample the signal.

**The Problem**:

The IBM XT Technical Reference documents that the genuine IBM keyboard holds DATA LOW in idle state. When the keyboard wants to transmit, it issues RTS by pulling CLOCK LOW and immediately releases DATA — the pull-up brings DATA HIGH (CTS). The constraint: the keyboard's 8048 microcontroller releases DATA in the very next instruction after pulling CLOCK LOW. At ~2.5µs per 8048 machine cycle, the DATA is only LOW for roughly **~5µs** at the first CLOCK falling edge before it goes HIGH. This is the start(0) window.

Clone keyboards hold DATA HIGH in idle, so DATA is already HIGH when CLOCK falls — no RTS/CTS, just a single start bit.

The detection is therefore: read DATA as fast as possible after the first CLOCK falling edge. DATA LOW = genuine IBM XT (start(0) confirmed, RTS/CTS sequence), DATA HIGH = clone (single start bit). The difficulty is that the ~5µs window is tight enough that the IBM Technical Reference notes it requires specially written code even on a 16MHz AVR.

**The Solution — Sample Immediately on the CLOCK Edge**:

The PIO `check` loop polls CLOCK every 2µs (clock divider 250 at 125MHz = 2µs per PIO cycle). When CLOCK goes LOW, `jmp pin, check` falls through and the very next instruction — `in pins, 1` — reads DATA. Total latency from CLOCK falling edge to sample: 2–4µs (1–2 PIO cycles). This is within the ~5µs window:

```text
PIO setup:
  System clock:     125 MHz
  Clock divider:    250
  PIO cycle time:   2µs

start(0) timing (genuine IBM XT):
  DATA LOW window:  ~5µs (one 8048 instruction cycle after CLOCK falls)
  PIO sample delay: 2–4µs (1–2 PIO cycles from CLOCK edge to in pins, 1)
  Margin:           ~1–3µs
```

Once the first sample is taken:

```text
At first CLOCK falling edge (check → in pins, 1):

  DATA = LOW → genuine IBM XT
    start(0) detected (RTS confirmed)
    Discard this bit (clear ISR)
    Wait for CLOCK rising edge
    Wait for CLOCK falling edge (start(1), DATA HIGH)
    Proceed with 8 data bits

  DATA = HIGH → clone XT
    Single start bit (no RTS/CTS)
    Use this sample as start bit
    Proceed with 8 data bits
```

This allows a single PIO program to handle both genuine and clone keyboards without any software intervention.

---

## Scan Code Set

The XT protocol uses a fixed scancode set. It's simple—no multibyte sequences, no mode switching, just simple make and break codes.

**Scancode Structure:**

```text
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

| Code           | Value | Description                                         |
| -------------- | ----- | --------------------------------------------------- |
| **BAT Passed** | 0xAA  | Self-test successful (sent once at power-on)        |
| **BAT Failed** | 0xFC  | Self-test failed (rare, indicates hardware problem) |

**BAT (Basic Assurance Test)**:

- Performed during keyboard power-on
- Tests ROM checksum, RAM, key matrix
- 0xAA: Everything OK, keyboard ready
- 0xFC: Hardware failure detected

**Protocol Layer Filtering:**
The XT protocol implementation filters 0xAA during initialisation (UNINITIALISED state) to prevent it from being processed as a scancode. Once INITIALISED, codes pass through to the scancode layer, which provides defense-in-depth filtering for post-initialisation scenarios. See [Scancode Set 1 Self-Test Code Collision](../scancodes/set1.md#self-test-code-collision) for details on why this matters.

**Note**: For complete scancode tables with all key mappings, see the **[Scancode Set 1](../scancodes/set1.md)** documentation.

---

## Protocol Operation

### Power-On Sequence

Power on an XT keyboard and it runs through a predictable initialisation sequence:

```text
Power-On Initialisation:
1. Keyboard receives power from Pin 5 (VCC)
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

XT keyboards support soft reset via the CLOCK and DATA lines. This is the **Type 2 reset method** used by IBM PC/XT keyboards, documented in the IBM PC XT Technical Reference (April 1984, p5-98).

**Note**: Type 1 XT keyboards (original IBM PC) used pin 3 for hardware reset instead. This implementation uses Type 2 soft reset and hasn't been tested with Type 1 keyboards.

**Reset Sequence:**

1. **Host Request**: Pull CLOCK LOW for ~20ms (host-initiated reset signal)
2. **Keyboard ACK**: Keyboard releases DATA, which returns HIGH via pull-up (open-drain acknowledgment)
3. **Host Release**: Host releases CLOCK (returns to input mode, keyboard controls timing)
4. **Keyboard Ready**: Keyboard pulls DATA LOW (ready for transmission)
5. **Self-Test**: Keyboard performs BAT (Basic Assurance Test)
6. **BAT Response**: Keyboard transmits 0xAA (passed) or 0xFC (failed)

**PIO Implementation:**

The converter implements this protocol in the PIO state machine ([`keyboard_interface.pio`](../../src/protocols/xt/keyboard_interface.pio)):

```pio
softReset:
    set pindirs 2 [1]       ; Set CLOCK pin to output mode
    set pins, 0             ; Pull CLOCK LOW (soft reset request, ~20ms hold)

    wait 1 pin 0 [1]        ; Wait for DATA released to HIGH by pull-up (keyboard ACK)

    set pindirs 1  [1]      ; Set CLOCK to input (release to keyboard)
    set pindirs, 0          ; Set DATA to input (allow keyboard control)

    wait 0 pin 0 [1]        ; Wait for DATA LOW (ready for transmission)
```

**When Soft Reset Occurs:**

- **Power-on initialisation**: After keyboard powers up and brings CLOCK/DATA HIGH
- **BAT failure recovery**: When keyboard returns invalid BAT response
- **Communication error**: When invalid start bit detected (triggers `pio_restart()`)
- **State machine reset**: After protocol errors or frame synchronisation loss

### Normal Operation

Once initialised, the keyboard runs a continuous scan loop:

```text
Continuous Scan Loop:
1. Keyboard continuously scans its key matrix
2. On key state change:
   a. Key Press: Generate make code (0x01-0x53)
   b. Key Release: Generate break code (make code + 0x80)
3. Transmit scancode using self-clocked protocol
4. Return to step 1 (no waiting for host acknowledgment)

Key Points:
- Keyboard operates independently of the host
- No flow control or acknowledgments
- Host needs to keep up with the transmission rate
- Burst rate is limited by mechanical key switch debounce
```

---

## Implementation Notes

### PIO Clock Divider and the ~5µs Detection Window

The `XT_TIMING_SAMPLE_US` constant (value 10) sets the PIO clock divider. The 10µs figure is the **data-bit sampling period**: the `bitLoopIn` loop takes 5 PIO cycles per iteration (wait 2 cycles + in 1 cycle + wait 1 cycle + jmp 1 cycle), and at 2µs per cycle that gives exactly 10µs per data bit — matching the ~100µs bit period at ~10kHz.

The start(0) detection is a different matter. It relies on the **2µs PIO cycle time**, not the 10µs loop period. The `check` loop detects the CLOCK falling edge within one 2µs cycle, and `in pins, 1` samples DATA in the very next cycle. Here's what happens at different PIO speeds:

**At 2µs PIO cycles (this implementation — divider 250)**:

- CLOCK falls detected; DATA sampled ~2–4µs later
- ~5µs DATA LOW window on genuine XT → caught reliably
- Result: correct genuine vs clone framing every time

**At 10µs PIO cycles (5× slower)**:

- CLOCK falls; DATA sampled ~10µs later
- DATA LOW window already expired by then (~5µs)
- Result: genuine IBM XT looks like a clone — framing misaligned, scancodes garbled

**At 100µs PIO cycles (bit period rate)**:

- CLOCK falls; DATA sampled ~100µs later — mid next bit
- Result: completely wrong framing

The clock divider of 250 is therefore set to achieve two things simultaneously: 2µs PIO cycles to catch the ~5µs start(0) window, and a natural 10µs data-bit sampling period (5 cycles × 2µs) for the rest of the frame.

### Host-Side Implementation

The converter uses the RP2040's PIO hardware to handle all the XT protocol timing automatically:

**PIO State Machine Configuration:**

```c
// From keyboard_interface.c
// See: ../../src/protocols/xt/keyboard_interface.c
pio_sm_config c = keyboard_interface_program_get_default_config(offset);

// Configure 9-bit reception (8 data + 1 start bit)
sm_config_set_in_shift(&c, true, true, 9);  // Right-shift, autopush, 9 bits

// Pin assignments
sm_config_set_in_pins(&c, keyboard_data_pin);  // DATA on base pin
sm_config_set_jmp_pin(&c, keyboard_clock_pin); // CLOCK on base pin + 1

// Clock divider for genuine vs clone detection
float clock_div = calculate_clock_divider(XT_TIMING_SAMPLE_US);  // 10µs sampling period
sm_config_set_clkdiv(&c, clock_div);
```

**IRQ Handler:**

```c
static void __isr keyboard_input_event_handler() {
    // Read 9-bit frame from PIO FIFO
    io_ro_32 data_cast = pio_engine.pio->rxf[pio_engine.sm] >> 23;
    uint16_t data = (uint16_t)data_cast;

    // Extract start bit and data byte
    uint8_t start_bit = data & 0x1;
    uint8_t data_byte = (uint8_t)((data_cast >> 1) & 0xFF);

    // Validate start bit (must be 1 for XT protocol)
    if (start_bit != 1) {
        // Invalid frame - restart PIO
        keyboard_state = UNINITIALISED;
        pio_restart(pio_engine.pio, pio_engine.sm, pio_engine.offset);
        return;
    }

    // Process valid scan code
    keyboard_event_processor(data_byte);
}
```

**Key Implementation Details:**

- PIO handles all timing automatically (no software delays)
- 10µs sampling rate allows genuine vs clone keyboard detection
- Start bit validation confirms frame synchronisation
- Ring buffer queues scancodes for main loop processing
- Non-blocking architecture (IRQ → ring buffer → main task)
- Standard protocol setup pattern (see [Protocol Implementation Guide](../development/protocol-implementation.md))

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

See [PIO Clock Divider and the ~5µs Detection Window](#pio-clock-divider-and-the-5µs-detection-window) above for the technical rationale behind this timing requirement.

---

## Troubleshooting

If you're running into issues with your XT keyboard, the problems usually fall into a few common categories. Protocol-specific troubleshooting is covered here, but for general hardware setup issues (voltage levels, wiring, that sort of thing), see the **[Hardware Setup Guide](../getting-started/hardware-setup.md)**.

**Basic voltage checks with a multimeter:**

- VCC (Pin 5): 5.0V ±5%
- GND (Pin 4): 0V
- Idle DATA (genuine IBM XT): ~0V (keyboard actively drives LOW)
- Idle DATA (clone XT): ~5V (pulled HIGH)

### Common Issues

| Symptom                | Likely Cause                   | What to Check                                                |
| ---------------------- | ------------------------------ | ------------------------------------------------------------ |
| No response            | Power issue, bad connections   | Check VCC, GND, verify pull-up resistor on DATA              |
| Initialisation fails   | Missing soft reset, too early  | Execute CLOCK LOW soft reset, wait 500ms after power-on      |
| Garbled scancodes      | Wrong bit order, sampling edge | Verify LSB-first, sample on CLOCK falling edge               |
| Missing key events     | Buffer overflow, timing        | Check ring buffer size, verify interrupt latency             |
| Intermittent operation | Cable issues, signal quality   | Use a shorter cable, check for cable integrity and shielding |
| No BAT (0xAA)          | Failed self-test, timing       | Wait longer (1000ms), verify soft reset sequence             |
| Clone detection issues | Start bit mismatch             | Implementation auto-detects 1 or 2 start bits                |

---

## Related Documentation

- **[Hardware Setup](../hardware/README.md)** - Physical connections and wiring
- **[Keyboards](../keyboards/README.md)** - Supported XT keyboards
- **[Scancode Set 1](../scancodes/set1.md)** - XT scancode tables

## References

### Official Documentation

1. **[IBM PC XT Technical Reference (April 1984)](https://bitsavers.org/pdf/ibm/pc/xt/6361459_PC_XT_Technical_Reference_Apr84.pdf)** - Official XT specification, keyboard protocol on p5-32 and p5-98
2. **[IBM PC Technical Reference (April 1983)](https://bitsavers.org/pdf/ibm/pc/xt/1502237_PC_XT_Technical_Reference_Apr83.pdf)** - Original PC/XT specification (25MB PDF)
3. **[IBM PC Technical Reference Manual (1981)](https://bitsavers.org/pdf/ibm/pc/)** - Original IBM PC documentation

### Implementation References

1. **[8048 XT Keyboard Firmware](https://github.com/Halicery/8042/blob/main/8048_XT_INTERN.TEXT)** - Reverse-engineered 8048 microcontroller firmware from genuine IBM XT keyboard
2. **[TMK Keyboard Firmware](https://github.com/tmk/tmk_keyboard)** - XT protocol reference implementation
3. **[KbdBabel Vintage Keyboard Documentation](http://kbdbabel.org/conn/index.html)** - Connector diagrams used in this document

### Our Implementation

1. **[`keyboard_interface.pio`](../../src/protocols/xt/keyboard_interface.pio)** - RP2040 PIO state machine for XT protocol
2. **[`keyboard_interface.c`](../../src/protocols/xt/keyboard_interface.c)** - C implementation, IRQ handlers, and state machine

---

**Questions or stuck on something?**
Use [GitHub Discussions](https://github.com/PaulW/rp2040-keyboard-converter/discussions) or [open an issue](https://github.com/PaulW/rp2040-keyboard-converter/issues) if you've found a problem.
