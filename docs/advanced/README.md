# Advanced Topics

**Status**: 🔄 In Progress | **Last Updated**: 27 October 2025

Advanced documentation for developers and power users.

## Architecture & Implementation

### System Architecture

**[Architecture Overview](architecture.md)**

Deep dive into the converter's internal design:

**Key Components:**
- **PIO State Machines**: Hardware-driven protocol timing
- **Ring Buffer**: Lock-free FIFO for IRQ-to-main communication
- **Scancode Processor**: State machine for multi-byte sequences
- **HID Interface**: USB HID boot protocol implementation
- **Command Mode**: Special firmware management mode

**Data Flow:**
```
PIO Hardware → IRQ Handler → Ring Buffer (32-byte FIFO)
                                  ↓
                            Main Loop reads
                                  ↓
Protocol Handler → Scancode Decoder → Keymap → HID → USB
```

**Thread Contexts:**
- IRQ: Keyboard events (producer)
- Main Loop: USB/HID handling (consumer)
- USB Callbacks: TinyUSB stack

**See:** [Architecture Guide](architecture.md)

---

### Performance Analysis

**[Performance Metrics](performance.md)**

Real hardware measurements and optimization:

**Pipeline Latency** (October 2024 - IBM Model M):

| Stage | Time (μs) | Percentage |
|-------|-----------|------------|
| PIO IRQ → Ring Buffer | 44 | 12.6% |
| Ring Buffer → Scancode | 48 | 13.7% |
| Scancode → HID | 69 | 19.7% |
| HID → USB | 141 | 40.3% |
| **Total** | **350** | **100%** |

**Resource Usage:**
- CPU: ~2% at 30 CPS (97.9% idle)
- RAM: 132KB used (50% of 264KB SRAM)
- Flash: ~200KB for program + data
- Ring Buffer: 32 bytes

**Bottleneck**: USB polling (10ms host-driven), NOT processing

**See:** [Performance Guide](performance.md)

---

### PIO Programming

**[PIO State Machines](pio-programming.md)**

Understanding RP2040's Programmable I/O:

**What is PIO?**
- Hardware-driven I/O with custom instruction set
- 8 state machines total (4 per PIO block)
- Microsecond-precision timing
- CPU-independent operation

**Protocol Implementation:**
- Bit timing and framing in PIO assembly
- IRQ triggers when frame complete
- Automatic clock generation/synchronization
- Zero CPU overhead during reception

**Example** (AT/PS2 protocol):
```pio
.program keyboard_rx
    wait 0 pin 0        ; Wait for CLK low
    in pins, 1          ; Read DATA bit
    wait 1 pin 0        ; Wait for CLK high
    jmp !osre, loop     ; Repeat until 11 bits
```

**See:** [PIO Programming Guide](pio-programming.md)

---

### Build System

**[Build System Details](build-system.md)**

CMake-based build system with Docker support:

**Build Flow:**
1. `KEYBOARD` env var → `keyboard.config` file
2. Config defines protocol, codeset, layout
3. CMake includes protocol handler, scancode processor, PIO files
4. Output: `build/rp2040-converter.uf2` (also `.elf`, `.elf.map`)

**Docker Environment:**
- Self-contained Pico SDK 2.2.0
- Consistent builds across platforms
- Parallel builds supported
- Memory analysis tools included

**Configuration System:**
```cmake
# src/keyboards/modelm/enhanced/keyboard.config
set(KEYBOARD_PROTOCOL "at-ps2")
set(KEYBOARD_CODESET "set2")
set(KEYBOARD_LAYOUT "us_ansi")
```

**See:** [Build System Guide](build-system.md)

---

### Troubleshooting

**[Troubleshooting Guide](troubleshooting.md)**

Common issues and solutions:

**Keys not registering?**
```
Ring buffer full? → USB saturated (check tud_hid_ready())
Protocol errors? → Check UART logs ([ERR] messages)
Wrong keymap? → Verify KEYBOARD_CODESET matches keyboard
```

**Build fails?**
```
KEYBOARD missing? → Set with docker compose run -e KEYBOARD="..."
keyboard.config missing? → Check path: src/keyboards/<brand>/<model>/
Linker errors? → Verify KEYBOARD_PROTOCOL in keyboard.config
```

**High latency?**
```
Check for: sleep_ms(), busy_wait_us(), long loops
Use: to_ms_since_boot(get_absolute_time())
```

**See:** [Troubleshooting Guide](troubleshooting.md)

---

## Critical Design Principles

### Single-Core Architecture

⚠️ **CRITICAL**: Core 1 is disabled. All code runs on Core 0.

**Why single-core?**
- Eliminates synchronization complexity
- Predictable timing and latency
- Simpler debugging
- Lower power consumption
- Sufficient performance (2% CPU at max load)

**Never use:**
- ❌ `multicore_*` functions
- ❌ `core1_*` functions
- ❌ Any Core 1 initialization

**See:** [Architecture Guide](architecture.md)

---

### Non-Blocking Operations

⚠️ **CRITICAL**: No blocking operations allowed (except debug with annotations).

**Why non-blocking?**
- PIO timing requires microsecond precision
- USB polling every 10ms
- Main loop must remain responsive

**Forbidden:**
- ❌ `sleep_ms()`, `sleep_us()`
- ❌ `busy_wait_ms()`, `busy_wait_us()`
- ❌ Long loops without yield

**Alternative:**
```c
// State machine with timeout
static uint32_t timer = 0;
if (to_ms_since_boot(get_absolute_time()) - timer > TIMEOUT) {
  // Timeout reached
}
```

**Exception**: Debug-only with `LINT:ALLOW blocking` + IRQ guard:
```c
if (in_irq()) return;  // Never block in IRQ
sleep_us(delay);  // LINT:ALLOW blocking - [justification]
```

**See:** [Architecture Guide](architecture.md)

---

### SRAM Execution

⚠️ **CRITICAL**: All code must run from SRAM (not Flash).

**Why SRAM?**
- Flash execution adds latency
- Flash cache misses unpredictable
- SRAM guarantees consistent timing
- Required for 350μs latency target

**CMake Configuration:**
```cmake
pico_set_binary_type(rp2040-converter copy_to_ram)
```

**See:** [Build System Guide](build-system.md)

---

### Ring Buffer Safety

⚠️ **CRITICAL**: Lock-free single-producer/single-consumer (SPSC) ring buffer.

**Design:**
- IRQ writes head (producer)
- Main loop reads tail (consumer)
- 32-byte capacity
- No locks or mutexes

**Rules:**
- ✅ IRQ calls `ringbuf_put()` only
- ✅ Main loop calls `ringbuf_get()` only
- ✅ Check `ringbuf_is_full()` before putting
- ✅ Check `ringbuf_is_empty()` before getting
- ❌ **NEVER** call `ringbuf_reset()` with IRQs enabled

**See:** [Architecture Guide](architecture.md)

---

## Testing & Validation

### Hardware Testing

**[Hardware Testing Guide](../development/hardware-testing.md)**

Best practices for hardware validation:

**Equipment:**
- Logic analyzer (Saleae, PulseView)
- USB-UART adapter (3.3V)
- Oscilloscope (optional)
- Multimeter

**Test Procedure:**
1. Verify signal levels (5V keyboard → 3.3V RP2040)
2. Check timing (CLK frequency, bit timing)
3. Monitor UART logs for errors
4. Test all keys (Keytest Mode)
5. Measure latency (logic analyzer)

**See:** [Hardware Testing Guide](../development/hardware-testing.md)

---

### Code Quality

**[Code Quality Standards](../development/code-standards.md)**

Coding standards and enforcement:

**Automated Tools:**
- `./tools/lint.sh` - Enforces all critical rules
- CI Pipeline - Build matrix, memory limits
- CodeRabbit - PR review automation

**Checks:**
1. No blocking operations
2. No multicore usage
3. IRQ safety (no printf, volatile usage)
4. SRAM execution
5. Protocol timing compliance

**See:** [Code Standards Guide](../development/code-standards.md)

---

## Memory Management

### Memory Layout

**RP2040 Memory:**
- **Flash**: 2MB (program storage, read-only)
- **SRAM**: 264KB (code execution, data)

**Usage:**
- Code: ~130KB SRAM (copy_to_ram)
- Data: ~2KB (globals, statics)
- Stack: ~16KB
- Heap: Minimal (mostly stack-based allocation)

**Limits (CI enforced):**
- Flash: <230KB
- RAM: <150KB

**See:** [Build System Guide](build-system.md)

---

## Protocol Timing

### Timing Requirements

Each protocol has strict timing constraints:

| Protocol | Clock | Bit Time | Frame Time |
|----------|-------|----------|------------|
| AT/PS2 | 10-16.7 kHz | 60-100 μs | ~1 ms |
| XT | ~20 kHz | ~50 μs | ~550 μs |
| Amiga | Variable | Variable | ~1 ms |
| M0110 | Variable | Variable | ~1 ms |

**Implementation:**
- PIO handles all timing automatically
- IRQ latency: <50 μs
- Ring buffer ensures no data loss
- Main loop processes within USB polling interval

**See:** [Protocol Guides](../protocols/README.md)

---

## Related Documentation

**In This Documentation:**
- [Architecture](architecture.md) - System design
- [Performance](performance.md) - Metrics and optimization
- [PIO Programming](pio-programming.md) - PIO state machines
- [Build System](build-system.md) - Build configuration
- [Troubleshooting](troubleshooting.md) - Common issues

**Development:**
- [Contributing](../development/contributing.md) - How to contribute
- [Code Standards](../development/code-standards.md) - Coding guidelines
- [Hardware Testing](../development/hardware-testing.md) - Testing procedures

**Source Code:**
- Main loop: [`src/main.c`](../../src/main.c)
- Ring buffer: [`src/common/lib/ringbuf.[ch]`](../../src/common/lib/ringbuf.c)
- PIO helper: [`src/common/lib/pio_helper.[ch]`](../../src/common/lib/pio_helper.c)

---

## Need Help?

- 📖 [Troubleshooting](troubleshooting.md)
- 💬 [Ask Questions](https://github.com/PaulW/rp2040-keyboard-converter/discussions)
- 🐛 [Report Issues](https://github.com/PaulW/rp2040-keyboard-converter/issues)
- 🔬 [Deep Dives](https://github.com/PaulW/rp2040-keyboard-converter/discussions/categories/deep-dives)

---

**Status**: Documentation in progress. Advanced guides coming soon!
