# Performance Characteristics

These performance characteristics are based on the RP2040 hardware specifications and what's implemented in the source code. No estimated measurements or unverified claims—everything here comes from the datasheet or the actual implementation.

---

## Design Characteristics

The design prioritizes deterministic, low-latency operation through hardware acceleration and non-blocking architecture. Deterministic timing means the converter behaves identically across executions—no variable delays from caching, no race conditions from multicore synchronization.

---

## Processing Pipeline

Each key press flows through distinct stages that operate without blocking each other:

1. **PIO Hardware Capture** - The RP2040's PIO state machines monitor keyboard protocol signals (clock and data lines) and assemble bits into complete scancodes. This happens entirely in hardware without CPU involvement.

2. **Interrupt Handler** - When a complete byte arrives, the PIO triggers an interrupt. The handler extracts the scancode and writes it to the ring buffer—a minimal operation taking only a few CPU cycles.

3. **Ring Buffer** - The [32-byte FIFO](../../src/common/lib/ringbuf.c) (defined as `RINGBUF_SIZE` in [ringbuf.h](../../src/common/lib/ringbuf.h)) bridges interrupt context and main loop. Its size provides sufficient capacity for multi-byte scancode sequences arriving back-to-back whilst remaining small enough to stay in L1 cache.

4. **Scancode Decoder** - The main loop retrieves scancodes from the buffer and reconstructs complete key events. Some protocols use multi-byte sequences (E0 prefixes, F0 break codes, the eleven-byte Pause key), so the decoder maintains state across multiple reads.

5. **Keymap Translation** - Complete scancodes translate to USB HID keycodes via keyboard-specific lookup tables. Command Mode detection also happens here, intercepting special key combinations before they reach USB.

6. **HID Report Assembly** - The HID interface collects keycodes into USB boot protocol reports (six-key rollover format). Reports only send when `tud_hid_ready()` confirms the USB host is ready to receive.

7. **USB Transmission** - TinyUSB handles the low-level USB communication, transmitting reports during the host's polling interval.

This pipeline design keeps interrupt handlers minimal (microseconds), maintains non-blocking operations throughout, and leverages PIO hardware for protocol timing precision.

---

## System Timing

The RP2040 and USB host impose specific timing constraints that shape the converter's performance:

### CPU and PIO

The RP2040 runs at **125 MHz** by default (configurable, but the converter uses the default). This provides **8 nanosecond instruction cycles**—fast enough that protocol timing is never CPU-limited.

PIO state machines execute independently at the same clock rate, handling protocol timing autonomously:
- **AT/PS2 keyboards:** 10-16.7 kHz clock frequency (60-100 microseconds per bit)
- **XT keyboards:** ~10 kHz clock frequency (approximately 100 microseconds per bit)
- **Amiga keyboards:** ~16.7 kHz synchronous protocol
- **Apple M0110:** Variable timing, adapts to signal conditions

The PIO hardware samples data lines at precise clock edges, shifts bits into registers, and triggers interrupts only when complete bytes arrive. Zero CPU overhead during bit reception.

### USB Polling

USB polling operates at **8 millisecond intervals** (configured via `bInterval = 8` in [usb_descriptors.c](../../src/common/lib/usb_descriptors.c)). This is the USB specification's full-speed polling rate for HID devices.

**Theoretical maximum throughput:** 125 reports per second (1000ms ÷ 8ms). In practice, human typing rarely exceeds 10-15 characters per second, leaving substantial headroom. Boot protocol reports support six simultaneous keys (6KRO), which accommodates even fast gaming inputs.

The USB polling interval represents the primary throughput bottleneck—not the processing pipeline. Even if the converter processed scancodes instantaneously, it could only send data as fast as the USB host polls.

---

## Resource Utilization

The converter's resource usage is deliberately conservative to ensure reliable operation and leave headroom for future features:

### Memory

The RP2040 provides **264KB of SRAM** and **2MB of flash storage**. The converter executes entirely from SRAM (see [SRAM Execution](architecture.md#sram-execution) for why), consuming space in both flash (for persistent storage) and SRAM (for execution).

**Build limits** (enforced by CI in [`.github/workflows/ci.yml`](../../.github/workflows/ci.yml)):
- **Flash usage:** < 230KB (11.5% of 2MB capacity)
- **SRAM usage:** < 150KB (56.8% of 264KB capacity)

These limits provide comfortable margins whilst preventing configurations that approach resource boundaries. Actual usage varies by configuration—AT/PS2 with a standard keyboard uses roughly 80-120KB flash and 50-80KB SRAM.

The 32-byte ring buffer stays in cache throughout operation. Global variables store protocol state, HID report buffers, and configuration values. The stack handles function call depth and local variables. The heap remains minimal—the code favours stack allocation to avoid fragmentation and memory leaks.

### CPU Utilization

The non-blocking architecture keeps the CPU idle between keyboard events. PIO hardware handles protocol timing autonomously, so the CPU only wakes for complete scancodes.

**Estimated CPU usage patterns:**
- Idle (waiting for scancodes): ~99% of time
- Processing scancodes: Sub-millisecond bursts
- USB communication: Coordinated with host polling (8ms intervals)

The converter doesn't perform continuous polling or busy-waiting—everything operates on interrupts or time-based state machines that check timestamps without blocking.

---

## Protocol Timing

Each keyboard protocol operates at different speeds with distinct timing characteristics. The PIO hardware handles these requirements automatically, but understanding the timing helps when debugging protocol issues or adding new protocol support.

### AT/PS2 Protocol

**Clock frequency:** 10-16.7 kHz (typical 10-15 kHz)
**Frame format:** 11 bits (start, 8 data, parity, stop)
**Frame duration:** ~1 millisecond at 10 kHz
**Bidirectional:** Host can send commands by pulling clock low

The PIO program samples data on clock falling edges, assembles bytes, and validates parity. When sending commands to the keyboard (LED updates, configuration), the PIO generates clock pulses at the same frequency.

### XT Protocol

**Clock frequency:** ~10 kHz
**Frame format:** 8 bits plus stop bit
**Unidirectional:** Keyboard to host only
**Simpler timing:** No bidirectional clock coordination required

The XT PIO program is simpler than AT/PS2 because it only needs to monitor signals, not generate them.

### Amiga Protocol

**Clock frequency:** ~16.7 kHz (derived from master clock)
**Frame format:** Synchronous handshake + 7-bit key codes
**Bidirectional:** Host provides reset pulse, keyboard responds

Timing is stricter than AT/PS2 or XT because both sides must coordinate around specific intervals. The PIO manages handshake sequences with precise timing.

### Apple M0110 Protocol

**Variable timing:** Adapts to signal conditions
**Complex state machine:** Multiple timing phases

The PIO implements state machine transitions required by this more complex protocol.

---

## Latency Analysis

Whilst the converter hasn't been measured with hardware timing equipment, the architecture's design characteristics provide insight into expected latency:

### Sources of Latency

1. **Keyboard protocol transmission:** Inherent to the protocol (1-2 milliseconds for AT/PS2 11-bit frame at 10 kHz)
2. **PIO to IRQ:** Hardware interrupt latency on RP2040
3. **Ring buffer transfer:** Interrupt handler writes, main loop reads (sub-microsecond operation)
4. **Scancode processing:** State machine handles multi-byte sequences (microseconds)
5. **Keymap lookup:** Array index operation (nanoseconds)
6. **HID report assembly:** Struct manipulation (nanoseconds)
7. **USB polling wait:** Host queries device every 8ms (average 4ms wait)

The protocol transmission and USB polling dominate total latency. Processing overhead (steps 2-6) totals well under 100 microseconds—negligible compared to protocol and USB timing.

### Deterministic Timing

The converter's architecture ensures **deterministic timing**—latency remains consistent across executions:

- **SRAM execution:** Every instruction takes the same time, regardless of cache state
- **Non-blocking operations:** No variable delays from sleep calls or busy-waiting
- **PIO hardware timing:** Protocol handling happens at fixed clock rates
- **Single-core design:** No inter-core synchronization overhead or race conditions

This determinism makes the converter's behaviour predictable and repeatable, which is valuable for debugging and optimization.

---

## Throughput Limits

The USB polling interval (8ms) imposes a hard limit on throughput:

**Maximum theoretical:** 125 reports/second (6 keys per report = 750 keypresses/second)
**Human typing speed:** 10-15 characters/second typical, 20-30 characters/second for fast typists
**Gaming inputs:** Even rapid gaming rarely exceeds 30 inputs/second

The converter provides substantial headroom above human input speeds. The 6KRO (six-key rollover) boot protocol format accommodates modifier combinations and multi-key gaming inputs without conflicts.

**Burst handling:** The 32-byte ring buffer accommodates short bursts where multiple scancodes arrive faster than USB can transmit them. The buffer drains during the next available USB polling window.

---

## Optimization Opportunities

The current implementation prioritizes correctness and determinism over raw performance. Several optimization opportunities exist for future development:

### Potential Optimizations

1. **NKRO (N-Key Rollover):** Switch from boot protocol (6KRO) to NKRO HID descriptor for unlimited simultaneous keys. Requires host driver support.

2. **USB polling interval:** Could negotiate faster polling (1ms minimum for full-speed USB) with compatible hosts. Requires USB descriptor changes and host compatibility testing.

3. **PIO program efficiency:** Some PIO programs could be optimized for fewer instructions or faster execution. Current implementations prioritize readability and maintainability.

4. **Scancode processor:** Could use lookup tables instead of state machines for protocols with simpler multi-byte sequences. Trade-off between code size and execution speed.

5. **Ring buffer size tuning:** Could reduce to 16 bytes (still ample for burst handling) to improve cache utilization. Requires testing with worst-case multi-byte sequences.

These optimizations aren't currently implemented because the converter already meets performance requirements with comfortable margins. Premature optimization would add complexity without practical benefit for keyboard conversion use cases.

---

## Benchmarking Methodology

For those wanting to measure actual hardware performance:

### Recommended Tools

1. **Logic analyzer:** Capture keyboard protocol signals and USB traffic simultaneously. Measure time from keyboard clock edge to USB D+ transition.

2. **USB analyzer:** Monitor USB packets to verify polling intervals and report timing. Beagle USB analyzers or software capture with Wireshark.

3. **Oscilloscope:** Observe signal quality, measure rise/fall times, identify noise or ringing that affects timing.

4. **UART logging:** Enable DEBUG logging level and capture timestamps. The converter logs timing information for protocol events.

### Measurement Points

- **Protocol latency:** Time from keyboard clock edge to ring buffer write
- **Processing latency:** Time from ring buffer read to HID report ready
- **USB latency:** Time from HID report ready to USB transmission complete
- **End-to-end latency:** Time from keyboard clock edge to USB D+ transition

### Considerations

- Test with multiple keyboard models—protocol timing varies between manufacturers
- Test with different USB hosts—some hosts have higher polling latency than others
- Test under load—measure latency during fast typing, not just single keypresses
- Test multi-byte sequences—extended keys (E0 prefix) and Pause key (11 bytes) have different timing

---

## Related Documentation

- [Architecture](architecture.md) - System architecture and design principles
- [Build System](build-system.md) - Build configuration and optimization
- [Testing](testing.md) - Hardware testing and validation procedures
- [Protocol Documentation](../protocols/README.md) - Protocol timing specifications
- [Code Standards](../development/code-standards.md) - Non-blocking requirements

---

**Questions or need clarification?**  
Pop into [GitHub Discussions](https://github.com/PaulW/rp2040-keyboard-converter/discussions) or [report a bug](https://github.com/PaulW/rp2040-keyboard-converter/issues).
