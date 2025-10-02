# IBM AT/PS2 Keyboard and Mouse Protocol

## Overview

The **IBM AT/PS2 protocol** is a **bidirectional, synchronous serial communication protocol** originally developed for the IBM PC/AT (1984) and later standardized with the PS/2 connector (1987). This protocol represents a significant evolution from the earlier XT protocol, adding sophisticated features like error detection, flow control, device identification, and bidirectional communication. It supports both keyboards and mice, making it one of the most versatile and widely-adopted computer peripheral interfaces before the advent of USB.

Unlike the unidirectional XT protocol, AT/PS2 allows the host to send commands to the device and receive acknowledgments, enabling features like LED control, scan code set selection, and device configuration. The protocol's robust design and error-handling capabilities made it the de facto standard for PC peripherals throughout the 1990s and early 2000s.

-----

## Historical Context

  - **1984**: IBM PC/AT introduces enhanced keyboard protocol with bidirectional communication
  - **1987**: PS/2 connector standardized with 6-pin mini-DIN connector
  - **1990s-2000s**: Peak adoption across the PC industry, becoming the universal standard
  - **2000s+**: Gradual replacement by USB, but still widely supported for legacy compatibility
  - **Present**: Remains in use for BIOS/UEFI firmware, embedded systems, and vintage computing

The AT/PS2 protocol's influence extends beyond its direct use—many of its concepts, including scan code sets and device identification, directly influenced the design of USB HID (Human Interface Device) specifications.

-----

## Technical Specifications

### Physical Interface

The AT/PS2 interface uses a compact, standardized connector design:

  - **Connector**: 6-pin mini-DIN (PS/2) or 5-pin DIN (original AT)
  - **Signals**: DATA, CLOCK, VCC (+5V), GND (plus 2 reserved pins on PS/2)
  - **Logic Levels**: Open-drain with pull-up resistors (typically 10kΩ)
  - **Voltage**: 5V TTL levels (0V = logic low, 5V = logic high)
  - **Current**: Low power operation (typically < 100mA per device)

Both the DATA and CLOCK lines use **open-drain outputs**, meaning either the host or device can pull the line low, but neither can actively drive it high. The pull-up resistors (located on the motherboard) ensure the lines return to a high state when released.

### Protocol Characteristics

  - **Type**: Synchronous serial communication with clock-coordinated data transfer
  - **Direction**: Bidirectional—both host and device can initiate communication
  - **Clock Control**: Device (keyboard or mouse) normally controls the CLOCK line
  - **Data Format**: 11-bit frames consisting of Start + 8 Data + Parity + Stop bits
  - **Bit Order**: LSB-first (bit 0 transmitted first, then bit 1, ..., bit 7)
  - **Clock Rate**: 10-16.7 kHz typical (60-100µs per bit)
  - **Error Detection**: Odd parity bit calculated over the 8 data bits
  - **Flow Control**: Hardware-level acknowledgment and request-to-send mechanism
  - **Framing**: Well-defined start and stop bits for frame synchronization

## Signal Diagrams

### Receiving Data (Device → Host)

```
IBM AT Format:
    ____ 1 _ 2 _ 3 _ 4 _ 5 _ 6 _ 7 _ 8 _ 9 _ A _ B _____
CLK     \_/ \_/ \_/ \_/ \_/ \_/ \_/ \_/ \_/ \_/ \_/
    ___     ___ ___ ___ ___ ___ ___ ___ ___ ___ ________
DAT    \___/___X___X___X___X___X___X___X___X___/
         S   0   1   2   3   4   5   6   7   P   s

Z-150/Other AT Variants:
    ____ 1 _ 2 _ 3 _ 4 _ 5 _ 6 _ 7 _ 8 _ 9 _ A _ B _____  
CLK     \_/ \_/ \_/ \_/ \_/ \_/ \_/ \_/ \_/ \_/ \_/  
    ___     ___ ___ ___ ___ ___ ___ ___ ___ ___     ____
DAT    \___/___X___X___X___X___X___X___X___X___\___/
         S   0   1   2   3   4   5   6   7   P   s*

S  = Start bit (always 0)
0-7= Data bits (LSB first)  
P  = Parity bit (odd parity)
s  = Stop bit (1 for IBM AT, 0 for some variants)
```

### Transmitting Data (Host → Device)

```
IBM AT Format:
    __      _ 1 _ 2 _ 3 _ 4 _ 5 _ 6 _ 7 _ 8 _ 9 _ A __ B _____
CLK   \____/ \_/ \_/ \_/ \_/ \_/ \_/ \_/ \_/ \_/ \_/  \_/
    ______     ___ ___ ___ ___ ___ ___ ___ ___ ______     ____
DAT       \___/___X___X___X___X___X___X___X___X___/  \___/
       H    R   0   1   2   3   4   5   6   7   P   s ACK

Z-150/Other AT Variants:
    __      _ 1 _ 2 _ 3 _ 4 _ 5 _ 6 _ 7 _ 8 _ 9 _ A __________
CLK   \____/ \_/ \_/ \_/ \_/ \_/ \_/ \_/ \_/ \_/ \_/ XXXXX
    ______     ___ ___ ___ ___ ___ ___ ___ ___ ______     ____
DAT       \___/___X___X___X___X___X___X___X___X___/  \___/
       H    R   0   1   2   3   4   5   6   7   P   s ACK*

H   = Host pulls DATA low to request transmission
R   = Request-to-send acknowledgment  
0-7 = Data bits (LSB first)
P   = Parity bit (odd parity)
s   = Stop bit (always 1)
ACK = Device acknowledgment (DATA pulled low briefly)
*   = Some variants have irregular clock during ACK
```

## Protocol Data Format

### Frame Structure

Every data transmission uses this 11-bit frame format:

```
| Start | Data[0:7] | Parity | Stop |
|   0   |  LSB...MSB |  Odd   |  1   |
```

- **Start Bit**: Always 0, signals beginning of frame
- **Data Bits**: 8 bits transmitted LSB-first (D0, D1, D2, ... D7)
- **Parity Bit**: Odd parity calculated over 8 data bits
- **Stop Bit**: Always 1, signals end of frame

### Parity Calculation

The parity bit ensures odd parity across the data byte:
```c
uint8_t parity = 0;
for (int i = 0; i < 8; i++) {
    if (data & (1 << i)) parity++;
}
parity_bit = (parity & 1) ^ 1;  // Make total count odd
```

## Common Commands and Responses

### Shared Commands (Keyboard and Mouse)

| Command | Value | Description | Response |
|---------|-------|-------------|----------|
| **Get Device ID** | `0xF2` | Request device identification | Device-specific ID bytes |
| **Resend** | `0xFE` | Request retransmission of last response | Previous response repeated |
| **Reset** | `0xFF` | Reset device to power-on state | `0xAA` (BAT passed) + device ID |

### Shared Responses

| Response | Value | Description | Context |
|----------|-------|-------------|---------|
| **ACK** | `0xFA` | Command acknowledged successfully | After most valid commands |
| **BAT Passed** | `0xAA` | Basic Assurance Test completed successfully | After reset or power-on |

### Keyboard-Specific Commands

| Command | Value | Description | Parameters |
|---------|-------|-------------|------------|
| **Set LEDs** | `0xED` | Control keyboard LEDs | LED state byte follows |
| **Set Scan Code Set** | `0xF0` | Change scan code set | Set number (1, 2, or 3) |
| **Enable** | `0xF4` | Enable key scanning | None |
| **Disable** | `0xF5` | Disable key scanning (except reset) | None |
| **Set Defaults** | `0xF6` | Restore default settings | None |

### Mouse-Specific Commands

| Command | Value | Description | Parameters |
|---------|-------|-------------|------------|
| **Set Sample Rate** | `0xF3` | Set reporting rate | Rate value (10-200 Hz) |
| **Set Resolution** | `0xE8` | Set movement resolution | Resolution code (0-3) |
| **Enable Reporting** | `0xF4` | Start sending movement data | None |
| **Status Request** | `0xE9` | Request mouse status | None |

## Device Identification

### Keyboard Device IDs

| Device Type | ID Bytes | Description |
|-------------|----------|-------------|
| **Standard AT** | `0xAB 0x41` | Basic AT keyboard (83-key) |
| **Enhanced AT** | `0xAB 0x83` | Enhanced AT keyboard (104-key) |
| **PS/2 Keyboard** | `0xAB 0x83` | Standard PS/2 keyboard |

### Mouse Device IDs

| Device Type | ID | Description | Packet Format |
|-------------|-------|-------------|---------------|
| **Standard PS/2** | `0x00` | 3-button mouse | 3-byte packets |
| **IntelliMouse** | `0x03` | Mouse with scroll wheel | 4-byte packets |
| **IntelliMouse Explorer** | `0x04` | 5-button mouse with wheel | 4-byte packets |

## Protocol Operation

### Initialization Sequence

1. **Power-On**: Both DATA and CLOCK high (idle state)
2. **Device Self-Test**: Device performs BAT (Basic Assurance Test)
3. **BAT Result**: Device sends `0xAA` if successful, `0xFC` if failed
4. **Device ID**: Device may send identification bytes after BAT
5. **Host Commands**: Host can now send configuration commands

### Communication Flow

#### Host-Initiated Communication
```
1. Host pulls DATA low (request-to-send)
2. Host pulls CLOCK low
3. Device takes control of CLOCK
4. Host releases DATA, prepares to send
5. Device clocks out host data
6. Device sends ACK by pulling DATA low
7. Both lines return to idle (high)
```

#### Device-Initiated Communication
```
1. Device checks that both lines are high (idle)
2. Device pulls CLOCK low
3. Device places start bit (0) on DATA
4. Device clocks out data frame
5. Host reads data on each clock edge
6. Device releases both lines (high)
```

### Error Handling

- **Parity Error**: Host sends `0xFE` (Resend command)
- **Timeout**: Host may reset communication or retry
- **Frame Error**: Host ignores frame, may request resend
- **Device Error**: Device may send error code or stop responding

### Timing Requirements

The protocol operates with precise timing constraints to ensure reliable communication:

#### Clock Timing
  - **Frequency**: 10-16.7 kHz (typical ~12 kHz for most devices)
  - **Period**: 60-100µs per bit transmission
  - **Duty Cycle**: Approximately 50% (30-50µs low phase, 30-50µs high phase)
  - **Minimum Pulse Width**: 30µs per IBM specification (IBM 84F9735 PS/2 Hardware Interface Technical Reference)

#### Data Setup and Hold Times
  - **Data Setup Time**: Data must be valid at least 5µs before the clock falling edge
  - **Data Hold Time**: Data must remain stable for at least 5µs after the clock falling edge
  - **Frame Spacing**: Minimum 50µs gap between consecutive frames

#### Response and Timeout Values
  - **Host Timeout**: Maximum 15ms wait for device response before considering communication failed
  - **Device Response**: Device should respond within 20ms of receiving a command
  - **Inhibit Time**: Host can hold CLOCK low indefinitely to prevent device transmission

-----

## Implementation Considerations

### Hardware Requirements
- **Pull-up Resistors**: 10kΩ on both DATA and CLOCK lines
- **Open-Drain Drivers**: Both host and device use open-drain outputs
- **ESD Protection**: Important for connector reliability
- **Debouncing**: May be needed for mechanical switch implementations

### Software Implementation
- **Interrupt-Driven**: Use interrupts for clock edge detection
- **State Machine**: Implement robust receive/transmit state machines
- **Buffering**: Queue commands and responses for reliable operation
- **Timeout Handling**: Implement timeouts for error recovery

### PIO State Machine Configuration

For RP2040 implementation using Programmable I/O:

```c
// LSB-first, 11-bit frames with automatic push/pull
sm_config_set_in_shift(&c, true, true, 11);   // Shift right (LSB-first), autopush at 11 bits
sm_config_set_out_shift(&c, true, true, 9);   // Shift right (LSB-first), autopull at 9 bits

// Pin assignments for bidirectional communication
sm_config_set_jmp_pin(&c, pin + 1);     // CLOCK pin (DATA + 1) for conditional jumps
sm_config_set_in_pins(&c, pin);         // DATA pin for input operations
sm_config_set_out_pins(&c, pin, 2);     // Both DATA and CLOCK for output operations
```

### Clock Divider Calculation

The PIO clock divider is calculated to achieve the correct protocol timing based on the minimum clock pulse width specified by IBM:

```c
float clock_div = calculate_clock_divider(ATPS2_TIMING_CLOCK_MIN_US);
// ATPS2_TIMING_CLOCK_MIN_US = 30µs (minimum pulse width per IBM specification)
```

-----

## Modern Applications and Compatibility

### Legacy Support
- **BIOS/UEFI**: Boot-time keyboard support
- **Embedded Systems**: Simple, reliable peripheral interface  
- **Retro Computing**: Authentic vintage computer restoration
- **Industrial**: Environments requiring robust, simple protocols

### USB Compatibility
- **USB-PS/2 Adapters**: Protocol translation between PS/2 and USB
- **BIOS Emulation**: BIOS provides PS/2 emulation for USB keyboards
- **Dual-Mode Devices**: Some devices support both PS/2 and USB protocols

## Troubleshooting Guide

### Common Problems

1. **No Device Response**
   - Check power connections (VCC, GND)
   - Verify pull-up resistors on DATA and CLOCK
   - Ensure proper open-drain configuration
   - Check cable/connector integrity

2. **Communication Errors**
   - Verify timing parameters (clock frequency, pulse widths)
   - Check parity calculation
   - Ensure proper LSB-first bit order
   - Look for electrical noise or signal integrity issues

3. **Intermittent Operation**
   - Check for loose connections
   - Verify timeout handling
   - Look for software timing issues
   - Check for interrupt latency problems

### Debug Techniques

- **Logic Analyzer**: Capture DATA and CLOCK signals
- **Oscilloscope**: Check signal quality and timing
- **Protocol Analyzer**: Decode frames and check parity
- **LED Indicators**: Visual feedback for communication states

## References and Sources

1. **IBM Technical Reference Manual**: Original AT specification (1984)
2. **PS/2 Hardware Technical Reference**: IBM PS/2 documentation (1987)
3. **Microsoft PS/2 Mouse Programmer's Reference**: Mouse protocol details
4. **Intel 8042 Controller Datasheet**: Keyboard controller implementation
5. **Modern Implementations**: USB-PS/2 adapters and converters