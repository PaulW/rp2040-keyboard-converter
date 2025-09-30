# IBM XT Keyboard Protocol

## Overview

The **IBM XT keyboard protocol** is the **simplest and earliest** standardized PC keyboard communication protocol, introduced with the original IBM PC (1981) and formalized with the IBM PC/XT (1983). This protocol represents the foundation upon which all subsequent PC keyboard protocols were built, establishing fundamental concepts like scan codes and asynchronous keyboard-to-host communication that remain influential today.

Unlike modern keyboard protocols, the XT protocol is **strictly unidirectional**—only the keyboard transmits data to the host computer. There are no host commands, acknowledgments, LED control, or bidirectional communication of any kind. This extreme simplicity makes the protocol remarkably robust and easy to implement, though it lacks the advanced features of later protocols like AT/PS2.

-----

## Historical Context

  - **1981**: IBM PC introduces basic keyboard interface with asynchronous serial communication
  - **1983**: IBM PC/XT formalizes the protocol specification and establishes scan code standards
  - **1984**: IBM AT introduces enhanced bidirectional protocol (AT/PS2), superseding XT
  - **1980s**: Wide adoption across IBM-compatible PC manufacturers as the de facto standard
  - **Legacy**: Foundation for all subsequent PC keyboard protocols and scan code conventions

The XT protocol's influence extends far beyond its direct use period. The scan code set it established (now known as "Scan Code Set 1") became the default for PC keyboards and is still supported by modern systems for backwards compatibility. The protocol's simplicity made it ideal for early microcontrollers and remains popular in embedded systems and retro computing projects.

-----

## Technical Specifications

### Physical Interface

The XT keyboard uses a simple, single-wire data interface:

  - **Connector**: 5-pin DIN connector with 180° orientation (180° DIN-5)
  - **Signals**: Single DATA line with embedded clock timing + power
  - **Logic Levels**: 5V TTL (0V = logic low, 5V = logic high)
  - **Power**: +5V and GND supplied by host computer
  - **Pull-up**: Host provides pull-up resistor on DATA line (typically 1-10kΩ)

Unlike later protocols, the XT protocol does not use a separate CLOCK line. Instead, the keyboard generates its own timing and embeds clock information in the DATA signal transitions.

### Protocol Characteristics

  - **Type**: Asynchronous serial communication with self-clocked transmission
  - **Direction**: Strictly unidirectional (keyboard → host only, no host commands)
  - **Clock Generation**: Keyboard generates its own internal timing
  - **Data Format**: Pure 8-bit scan codes with no additional framing
  - **Bit Order**: LSB-first (bit 0 transmitted first, then bit 1, ..., bit 7)
  - **Framing**: No start bits, stop bits, or parity—just raw 8-bit data
  - **Error Detection**: None (no checksums, parity, or acknowledgments)
  - **Flow Control**: None (host must be capable of keeping up with keyboard transmission)

### Data Format

The XT protocol transmits pure 8-bit scan codes with no additional framing:

- **Bits 0-7**: Scan code data (LSB transmitted first)
- **Make Codes**: 0x01-0x53 (key press events)
- **Break Codes**: Make code + 0x80 (key release events)
- **No Multi-byte Sequences**: Each key action = single byte

## Signal Diagrams

### Data Transmission (Keyboard → Host Only)

#### Genuine IBM XT Keyboards (Two Start Conditions)
```
    ____       _ 1 _ 2 _ 3 _ 4 _ 5 _ 6 _ 7 _ 8 _ 9 _______
CLK     \_____/ \_/ \_/ \_/ \_/ \_/ \_/ \_/ \_/ \_/   
             _____ ___ ___ ___ ___ ___ ___ ___ ___
DAT ________/     \___X___X___X___X___X___X___X___\_______
        ^   ^  S    0   1   2   3   4   5   6   7
        RTS CTS
```

#### XT Clone Keyboards (Single Start Condition)
```
    ____________ 1 _ 2 _ 3 _ 4 _ 5 _ 6 _ 7 _ 8 _ 9 _______
CLK             \_/ \_/ \_/ \_/ \_/ \_/ \_/ \_/ \_/     
    ______________ ___ ___ ___ ___ ___ ___ ___ ___ _______
DAT               \___X___X___X___X___X___X___X___/
               S    0   1   2   3   4   5   6   7
```

**Key Points:**
- **RTS/CTS**: Genuine IBM keyboards use two start conditions (Request-to-Send/Clear-to-Send)
- **Single Start**: Clone keyboards typically use simpler single start condition
- **Timing**: ~10 kHz clock rate, ~100µs per bit
- **Edge**: Data read on clock rising edge by host

### Power-On and Reset Sequence

```
Power-On Sequence:
1. CLOCK = LOW  (keyboard in reset)
2. CLOCK = HIGH (keyboard ready)  
3. DATA = HIGH for ~500ms (power-on indication)
4. DATA = LOW  (ready for soft reset)

Soft Reset (Host-Initiated):
1. Host inhibits CLOCK (pulls low) for ~20ms
2. Keyboard acknowledges by pulling DATA HIGH
3. Host releases CLOCK (returns to input mode)
4. Keyboard begins self-test sequence
5. Keyboard sends 0xAA if BAT (Basic Assurance Test) passes
```

## Scan Code Set

The XT protocol uses a **fixed scan code set** (equivalent to modern "Set 1"):

### Make Codes (Key Press)
| Key | Scan Code | Key | Scan Code | Key | Scan Code |
|-----|-----------|-----|-----------|-----|-----------|
| **ESC** | `0x01` | **1** | `0x02` | **2** | `0x03` |
| **3** | `0x04` | **4** | `0x05` | **5** | `0x06` |
| **Q** | `0x10` | **W** | `0x11` | **E** | `0x12` |
| **A** | `0x1E` | **S** | `0x1F` | **D** | `0x20` |
| **Space** | `0x39` | **Enter** | `0x1C` | **Backspace** | `0x0E` |
| **Left Shift** | `0x2A` | **Right Shift** | `0x36` | **Ctrl** | `0x1D` |

### Break Codes (Key Release)
Break codes are generated by adding `0x80` to the make code:

```
Make Code:  0x1E (A key pressed)
Break Code: 0x9E (A key released) = 0x1E + 0x80
```

### Special Codes
| Code | Value | Description |
|------|-------|-------------|
| **BAT Passed** | `0xAA` | Self-test successful (power-on only) |
| **BAT Failed** | `0xFC` | Self-test failed (rare) |

## Protocol Operation

### Initialization Process

1. **Power-On**: Keyboard performs internal self-test
2. **Clock Setup**: CLOCK brought HIGH after internal initialization  
3. **Data Ready**: DATA HIGH for ~500ms, then LOW when ready
4. **Soft Reset**: Host pulls CLOCK low for ~20ms to request reset
5. **Reset ACK**: Keyboard pulls DATA HIGH to acknowledge reset
6. **Self-Test**: Keyboard performs Basic Assurance Test (BAT)
7. **BAT Result**: Keyboard sends `0xAA` if successful
8. **Ready**: Keyboard begins normal scan code transmission

### Normal Operation

```
Continuous Operation Loop:
1. Keyboard scans key matrix
2. On key state change:
   a. Generate make code (0x01-0x53) for key press
   b. Generate break code (make + 0x80) for key release
3. Transmit scan code using embedded clock protocol
4. Return to step 1 (no host acknowledgment needed)
```

### No Host Commands

Unlike later protocols, XT keyboards do not accept commands from the host:
- No LED control commands
- No scan code set selection
- No disable/enable commands
- No configuration options
- No bidirectional communication

## Timing Specifications

The XT protocol operates with keyboard-generated timing:

### Clock Timing
```
Keyboard-Generated Clock:
- Frequency: ~10 kHz (approximately, not precisely specified)
- Period: ~100µs per bit transmission
- Low Phase: ~40µs (clock low period)
- High Phase: ~60µs (clock high period)
- Tolerance: ±20% (keyboards vary significantly between manufacturers)
```

### Data Timing Requirements
```
Data Setup and Hold:
- Setup Time: Data must be valid before clock rising edge
- Hold Time: Data must remain stable after clock rising edge
- Inter-byte Gap: Variable (typically 1-10ms between consecutive scan codes)
```

### Reset and Initialization Timing
```
Soft Reset Sequence:
- Host CLOCK Low: 20ms minimum to signal reset request
- Keyboard DATA High Response: Within 1ms of CLOCK release (acknowledgment)
- BAT Duration: 200-500ms typical for Basic Assurance Test
- BAT Response: 0xAA sent immediately after successful self-test completion
```

-----

## Implementation Details

### Host-Side Implementation

Since the protocol is unidirectional, host implementation is straightforward:

```c
// Pseudo-code for XT protocol receiver
void xt_keyboard_handler(void) {
    static uint8_t bit_count = 0;
    static uint8_t scan_code = 0;
    
    if (clock_rising_edge_detected()) {
        if (data_line_high()) {
            scan_code |= (1 << bit_count);
        }
        
        bit_count++;
        
        if (bit_count == 8) {
            // Complete scan code received
            process_scan_code(scan_code);
            bit_count = 0;
            scan_code = 0;
        }
    }
}
```

### PIO State Machine Configuration

For RP2040 implementation using Programmable I/O:

```c
// LSB-first, 9-bit reception (8 data bits + timing/framing detection)
sm_config_set_in_shift(&c, true, true, 9);

// Pin assignments for unidirectional receive-only operation
sm_config_set_in_pins(&c, pin);         // DATA pin for input
sm_config_set_jmp_pin(&c, pin + 1);     // CLOCK pin for timing detection and state decisions
```

### Clock Divider Calculation

The PIO clock divider is calculated for ~10µs minimum pulse detection:

```c
float clock_div = calculate_clock_divider(10);
// 10µs minimum timing for reliable signal detection of ~10 kHz keyboard clock
```

-----

### Soft Reset Implementation

```c
void xt_soft_reset(uint data_pin, uint clock_pin) {
    // Pull CLOCK low for soft reset
    gpio_set_dir(clock_pin, GPIO_OUT);
    gpio_put(clock_pin, 0);
    sleep_ms(20);
    
    // Release CLOCK and wait for DATA high acknowledgment
    gpio_set_dir(clock_pin, GPIO_IN);
    
    // Wait for keyboard to pull DATA high (reset acknowledgment)
    while (!gpio_get(data_pin)) {
        sleep_us(10);
    }
    
    // Wait for DATA to go low again (ready for normal operation)
    while (gpio_get(data_pin)) {
        sleep_us(10);
    }
}
```

## Advantages and Limitations

### Advantages
- **Simplicity**: Minimal complexity, easy to implement
- **Reliability**: Few failure modes due to simple design
- **Low Latency**: Direct transmission, no command/response delays
- **Hardware Efficiency**: Minimal host-side hardware requirements
- **Deterministic**: Predictable behavior and timing

### Limitations
- **No Flow Control**: Host must keep up with keyboard transmission
- **No Error Detection**: No parity, checksums, or acknowledgments
- **No Configuration**: Fixed scan code set, no customization
- **No LED Control**: Cannot control Caps Lock, Num Lock, etc.
- **Timing Variations**: Different keyboards have different timing characteristics
- **No Hotplug**: Cannot detect keyboard connection/disconnection

## Modern Applications

### Legacy Support
- **Retro Computing**: Authentic restoration of vintage PC systems
- **Embedded Systems**: Simple, robust keyboard interface
- **Industrial Control**: Environments requiring minimal complexity
- **BIOS/Boot Loaders**: Simple keyboard input during system startup

### Conversion Projects
- **USB Converters**: Convert XT keyboards for modern systems
- **Protocol Bridges**: Interface XT keyboards with AT/PS2 systems
- **Microcontroller Projects**: Simple keyboard input for embedded applications

## Troubleshooting Guide

### Common Issues

1. **No Keyboard Response**
   - Check power connections (+5V, GND)
   - Verify DATA line pull-up resistor
   - Ensure keyboard is not in continuous reset state
   - Check for proper soft reset sequence

2. **Garbled Scan Codes**
   - Verify LSB-first bit order
   - Check clock edge detection (should read on rising edge)
   - Look for timing synchronization issues
   - Ensure proper debouncing of clock signal

3. **Missing Key Events**
   - Check for interrupt latency issues
   - Verify host can keep up with transmission rate
   - Look for electrical noise affecting clock signal
   - Ensure proper ground connections

4. **Initialization Problems**
   - Increase soft reset timing (some keyboards need longer)
   - Check for proper BAT sequence completion
   - Verify power-on sequence timing
   - Look for keyboard-specific timing requirements

### Debug Techniques

- **Logic Analyzer**: Capture DATA and timing signals
- **Oscilloscope**: Check signal quality and noise
- **LED Indicators**: Visual feedback for protocol states
- **Serial Output**: Log received scan codes for analysis

### Signal Quality Checklist

- Clean 5V power supply with minimal ripple
- Proper grounding between keyboard and host
- Pull-up resistor on DATA line (1-10kΩ)
- Short, shielded cable connections
- Protection against ESD and electrical noise

## Hardware Variants and Compatibility

### Genuine IBM Keyboards
- **Model F (XT)**: Capacitive switches, high quality
- **Model M (Early)**: Buckling spring switches  
- **Timing**: Most consistent with specification
- **Features**: Two-start-bit sequence, precise timing

### Clone and Compatible Keyboards
- **Various Manufacturers**: Different switch technologies
- **Timing Variations**: May deviate from IBM specification
- **Simplified Protocols**: Often use single-start-bit sequence
- **Quality Range**: From excellent to barely functional

### Modern Reproductions
- **Mechanical Switch**: Cherry MX and similar switches
- **Microcontroller-Based**: Arduino, Pi Pico implementations
- **Enhanced Features**: May add USB connectivity
- **Protocol Accuracy**: Usually follows XT specification closely

## References and Sources

1. **IBM PC Technical Reference Manual** (1981): Original protocol specification
2. **IBM PC/XT Technical Reference Manual** (1983): Formal XT protocol definition  
3. **IBM Hardware Maintenance Service Manual**: Keyboard diagnostic procedures
4. **Microcontroller Application Notes**: Modern implementation examples
5. **TMK Keyboard Firmware**: Reference converter implementations

---
