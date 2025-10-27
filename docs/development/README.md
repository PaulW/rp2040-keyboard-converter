# Development Documentation

**Status**: 🔄 In Progress | **Last Updated**: 27 October 2025

Documentation for developers contributing to the project.

## Getting Started with Development

### Contributing Guide

**[Contributing Guide](contributing.md)**

How to contribute to the project:

**Before You Start:**
1. Read [copilot-instructions.md](../../.github/copilot-instructions.md) - Critical architecture rules
2. Understand [Code Standards](code-standards.md) - Coding conventions
3. Review [Architecture](../advanced/architecture.md) - System design

**Contribution Process:**
1. **Fork & Clone**: Fork repository, clone locally
2. **Branch**: Create feature branch (`git checkout -b feat/my-feature`)
3. **Develop**: Make changes following code standards
4. **Test**: Run `./tools/lint.sh` and build tests
5. **Commit**: Follow conventional commit format
6. **PR**: Create pull request with description

**Conventional Commits:**
```
feat: Add support for new keyboard protocol
fix: Correct timing in AT/PS2 host-to-device
docs: Update protocol documentation
refactor: Simplify ring buffer logic
test: Add unit tests for scancode processor
```

**See:** [Contributing Guide](contributing.md)

---

### Code Standards

**[Code Standards](code-standards.md)**

Coding conventions and quality guidelines:

**Style:**
- 4 spaces indentation (no tabs)
- Snake_case for variables/functions
- UPPER_CASE for macros/constants
- Descriptive names (no abbreviations)

**Architecture Rules (CRITICAL):**
- ❌ No blocking operations (`sleep_ms`, `busy_wait_us`)
- ❌ No multicore APIs (`multicore_*`, `core1_*`)
- ❌ No `printf` in IRQ context (use `LOG_*` macros)
- ❌ No `ringbuf_reset()` with IRQs enabled
- ✅ All code runs from SRAM (`copy_to_ram`)
- ✅ Non-blocking state machines for timing
- ✅ Volatile + memory barriers for IRQ communication

**Example (Non-blocking timeout):**
```c
static uint32_t timer = 0;
static enum { IDLE, WAITING, DONE } state = IDLE;

void my_task(void) {
  uint32_t now = to_ms_since_boot(get_absolute_time());
  
  switch (state) {
    case IDLE:
      timer = now;
      state = WAITING;
      break;
    case WAITING:
      if (now - timer >= TIMEOUT_MS) {
        state = DONE;
      }
      break;
    case DONE:
      // Process
      state = IDLE;
      break;
  }
}
```

**See:** [Code Standards Guide](code-standards.md)

---

### Testing

**[Testing Guide](testing.md)**

How to test your changes:

**Lint Script:**
```bash
./tools/lint.sh  # Must pass (0 errors, 0 warnings)
```

**Checks:**
- No blocking operations
- No multicore usage
- No printf in IRQ
- No docs-internal references
- Proper volatile usage
- Memory barrier placement

**Build Testing:**
```bash
# Test single configuration
docker compose run --rm -e KEYBOARD="modelm/enhanced" builder

# Test multiple configurations
docker compose run --rm -e KEYBOARD="modelf/pcat" builder
docker compose run --rm -e KEYBOARD="modelf/xt" builder
docker compose run --rm -e KEYBOARD="amiga/standard" builder
```

**Hardware Testing:**
- Test with real keyboard/mouse
- Monitor UART logs (`115200 baud`)
- Use Keytest Mode for key verification
- Verify LED synchronization
- Check latency with logic analyzer

**See:** [Testing Guide](testing.md)

---

## Development Guides

### Adding Protocol Support

**[Adding Protocols](adding-protocols.md)**

How to add support for new keyboard protocols:

**Steps:**
1. **Research**: Understand timing, framing, communication flow
2. **Create Directory**: `src/protocols/<name>/`
3. **PIO Program**: Write `.pio` for bit timing and framing
4. **Interface**: Implement `keyboard_interface.h/c`
5. **Common Interface**: Implement `common_interface.c`
6. **Test**: Verify with real hardware

**Required Files:**
- `<name>.pio` - PIO assembly for hardware timing
- `keyboard_interface.h` - Protocol state machine header
- `keyboard_interface.c` - Protocol implementation
- `common_interface.c` - Standardized init/task/led functions
- `README.md` - Protocol specification with timing diagrams

**Reference Implementations:**
- **Simple**: `src/protocols/xt/` (unidirectional)
- **Complex**: `src/protocols/at-ps2/` (bidirectional)
- **Unique**: `src/protocols/amiga/` (handshake protocol)

**See:** [Adding Protocols Guide](adding-protocols.md)

---

### Adding Keyboard Support

**[Adding Keyboards](adding-keyboards.md)**

How to add support for new keyboards:

**Steps:**
1. **Identify**: Protocol, codeset, connector type
2. **Create Directory**: `src/keyboards/<brand>/<model>/`
3. **Configuration**: Add `keyboard.config`
4. **Keymap**: Define layout in `keyboard.c`
5. **Override** (optional): Custom command mode keys in `keyboard.h`
6. **Document**: Create `README.md`
7. **Test**: Build and verify with hardware

**Configuration Example:**
```cmake
# src/keyboards/modelm/enhanced/keyboard.config
set(KEYBOARD_PROTOCOL "at-ps2")
set(KEYBOARD_CODESET "set2")
set(KEYBOARD_LAYOUT "us_ansi")
```

**Keymap Example:**
```c
// src/keyboards/modelm/enhanced/keyboard.c
#include "keyboard.h"

const uint8_t KEYMAP[256] = KEYMAP_US_ANSI(
  KC_ESC, KC_F1, KC_F2, ...
);
```

**See:** [Adding Keyboards Guide](adding-keyboards.md)

---

### Adding Scancode Sets

**[Adding Scancode Sets](adding-scancodes.md)**

How to add support for new scancode mappings:

**Steps:**
1. **Research**: Understand scancode format (make/break, prefixes)
2. **Create Directory**: `src/scancodes/<set>/`
3. **State Machine**: Implement multi-byte sequence handling
4. **Mapping**: Define scancode → HID keycode translation
5. **Document**: Create `README.md` with scancode tables
6. **Test**: Verify all key combinations

**Components:**
- `scancode_processor.h/c` - State machine for multi-byte sequences
- `scancode_map.h` - Scancode → HID keycode mapping
- `README.md` - Complete scancode tables

**Reference:**
- `src/scancodes/set2/` - AT/PS2 Scancode Set 2
- `src/scancodes/xt/` - XT Scancode Set
- `src/scancodes/amiga/` - Amiga Scancode Set

**See:** [Adding Scancodes Guide](adding-scancodes.md)

---

### Build System

**[Build System](../advanced/build-system.md)**

Understanding the build system:

**Docker Environment:**
```bash
# Build Docker image (one-time)
docker compose build

# Build firmware
docker compose run --rm -e KEYBOARD="modelm/enhanced" builder
```

**CMake Flow:**
1. `KEYBOARD` env var → `keyboard.config`
2. Config defines protocol, codeset, layout
3. CMake includes protocol, scancode, keymap
4. Build output: `build/rp2040-converter.uf2`

**Local Development:**
```bash
# Setup Pico SDK submodule
git submodule update --init --recursive

# Configure
cmake -B build

# Build
cmake --build build
```

**See:** [Build System Guide](../advanced/build-system.md)

---

## Architecture Deep Dives

### Critical Architecture Rules

⚠️ **Read copilot-instructions.md COMPLETELY before developing**

**Absolute Prohibitions:**
1. ❌ No blocking operations
2. ❌ No multicore APIs (Core 1 disabled)
3. ❌ No printf in IRQ
4. ❌ No ringbuf_reset() with IRQs enabled
5. ❌ No Flash execution

**Why These Rules Exist:**
- **Non-blocking**: Main loop ~100kHz, protocols need μs timing
- **Single-core**: Eliminates sync complexity, predictable latency
- **IRQ safety**: printf uses DMA, conflicts in IRQ context
- **Ring buffer**: SPSC lock-free design requires strict access patterns
- **SRAM execution**: Flash latency breaks timing guarantees

**See:** 
- [Architecture Guide](../advanced/architecture.md)
- [copilot-instructions.md](../../.github/copilot-instructions.md)

---

### Ring Buffer Implementation

**Design**: Lock-free single-producer/single-consumer (SPSC)

**Properties:**
- 32 bytes capacity
- IRQ writes head (producer)
- Main loop reads tail (consumer)
- No locks or mutexes
- Atomic operations only

**Usage Pattern:**
```c
// IRQ context (producer)
void __isr keyboard_irq(void) {
  uint8_t data = read_hardware();
  if (!ringbuf_is_full()) {
    ringbuf_put(data);
  }
}

// Main loop (consumer)
void keyboard_task(void) {
  if (!ringbuf_is_empty()) {
    uint8_t data = ringbuf_get();
    process(data);
  }
}
```

**NEVER:**
- ❌ Read from IRQ context
- ❌ Write from main loop
- ❌ Call `ringbuf_reset()` with IRQs enabled

**See:** [`src/common/lib/ringbuf.[ch]`](../../src/common/lib/ringbuf.c)

---

### PIO Programming

**What is PIO?**
- Programmable I/O subsystem
- 8 state machines (4 per PIO block)
- Custom instruction set
- Hardware-driven timing

**Use Cases:**
- Protocol bit timing
- Frame synchronization
- Clock generation
- Edge detection

**Example** (Simple RX):
```pio
.program simple_rx
.wrap_target
    wait 0 pin 0    ; Wait for CLK low
    in pins, 1      ; Read DATA bit
    wait 1 pin 0    ; Wait for CLK high
.wrap
```

**See:** 
- [PIO Programming Guide](../advanced/pio-programming.md)
- [RP2040 Datasheet](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf)

---

## Tools & Enforcement

### Lint Script

**Location**: `./tools/lint.sh`

**What it checks:**
- Blocking operations (sleep_ms, busy_wait_us)
- Multicore APIs (multicore_*, core1_*)
- printf in IRQ context
- docs-internal references in public files
- Ring buffer safety (ringbuf_reset warnings)

**Usage:**
```bash
./tools/lint.sh           # Check all files
./tools/lint.sh src/main.c  # Check specific file
```

**Exit Codes:**
- 0: Pass
- 1: Errors found
- 2: Warnings found

**CI Integration**: All PRs must pass lint check

---

### CodeRabbit

**Automated PR Reviews:**
- Embedded systems rules
- Path-specific instructions
- Custom pre-merge checks
- Knowledge base from copilot-instructions.md

**Configuration**: `.coderabbitai.yaml`

**Custom Checks:**
1. No blocking operations
2. No multicore usage
3. IRQ safety
4. RAM execution
5. Protocol timing compliance

**See:** [.coderabbitai.yaml](../../.coderabbitai.yaml)

---

## Documentation

### Documentation Structure

**Public** (committed):
- `docs/` - User and developer documentation
- `README.md` - Project overview
- Source code comments

**Private** (local only, NEVER commit):
- `docs-internal/` - Development notes, investigations
- Local analysis, performance data
- Architecture decisions

**Rules:**
- ✅ Use docs-internal/ as reference during development
- ❌ Never commit docs-internal/ files
- ❌ Never reference docs-internal/ in public docs
- ✅ Extract relevant information to public docs

**See:** [Documentation Template](../template.md)

---

## Git Workflow

### Branch Naming

Follow conventional commit prefixes:

```
feat/add-new-protocol      # New features
fix/at-ps2-timing          # Bug fixes
docs/protocol-guide        # Documentation
refactor/simplify-ringbuf  # Code refactoring
test/add-unit-tests        # Testing
```

---

### Commit Messages

**Format:**
```
<type>: <description>

[optional body]

[optional footer]
```

**Types:**
- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation
- `refactor`: Code refactoring
- `test`: Testing
- `chore`: Maintenance

**Examples:**
```
feat: Add support for Amiga protocol

Implements Amiga keyboard protocol with handshake timing and
Caps Lock synchronization via pulse.

Closes #42

---

fix: Correct AT/PS2 host-to-device timing

Device was not responding correctly to LED commands. Fixed
timing to match specification (10-16.7 kHz clock).

Fixes #38
```

---

### Pull Request Template

**Location**: `.github/PULL_REQUEST_TEMPLATE.md`

**Sections:**
- Change description
- Architecture checklist
- Testing performed
- Breaking changes
- Documentation updated

**Requirements:**
- Lint script passes
- Build tests pass
- Hardware tested (if applicable)
- Documentation updated

---

## Related Documentation

**Architecture:**
- [Architecture Overview](../advanced/architecture.md)
- [Performance Analysis](../advanced/performance.md)
- [PIO Programming](../advanced/pio-programming.md)

**Implementation:**
- [Protocol Implementation](../protocols/README.md)
- [Keyboard Configuration](../keyboards/README.md)
- [Features](../features/README.md)

**Project:**
- [Main README](../../README.md)
- [Copilot Instructions](../../.github/copilot-instructions.md)
- [License](../../LICENSE)

---

## Need Help?

- 📖 [Architecture Guide](../advanced/architecture.md)
- 📖 [Troubleshooting](../advanced/troubleshooting.md)
- 💬 [Ask Questions](https://github.com/PaulW/rp2040-keyboard-converter/discussions)
- 🐛 [Report Issues](https://github.com/PaulW/rp2040-keyboard-converter/issues)
- 🤝 [Contributing](contributing.md)

---

**Status**: Documentation in progress. Development guides coming soon!
