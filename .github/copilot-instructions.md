# RP2040 Keyboard Converter - AI Instructions

Embedded firmware converting vintage keyboard/mouse protocols (AT/PS2, XT, Apple M0110, Amiga) to USB HID using RP2040's PIO hardware. **Single-core only** (Core 0), runs from SRAM for performance.

---

## 🚨 CRITICAL RULES - READ FIRST, EVERY TIME

### Absolute Prohibitions (NEVER violate):
1. ❌ **NO blocking operations** (`sleep_ms`, `sleep_us`, `busy_wait_ms`, `busy_wait_us`) - except with `LINT:ALLOW blocking` + IRQ guard
2. ❌ **NO multicore APIs** (`multicore_*`, `core1_*`) - Core 1 is disabled, single-core architecture only
3. ❌ **NO `printf`/`fprintf`/`sprintf` in IRQ context** - use `LOG_*` macros instead (DMA-safe)
4. ❌ **NO `ringbuf_reset()` with IRQs enabled** - except with `LINT:ALLOW ringbuf_reset` annotation
5. ❌ **NO Flash execution** - code must run from RAM (`pico_set_binary_type(copy_to_ram)`)
6. ❌ **NO committing docs-internal/ files** - local-only documentation, never stage/commit/push
7. ❌ **NO references to docs-internal/** in public documentation or commit messages

### Architecture Summary:
- **Non-blocking**: Main loop runs at ~100kHz, protocols need microsecond timing → use `to_ms_since_boot(get_absolute_time())`
- **IRQ Safety**: IRQ (producer) → Ring Buffer → Main Loop (consumer). Use `volatile` + `__dmb()` memory barriers
- **USB**: Always check `tud_hid_ready()` before sending HID reports (10ms host polling)
- **Platform**: RP2040 (ARM Cortex-M0+, 125MHz), 32-byte ring buffer, 350μs max latency

### Self-Verification Protocol (Run Before EVERY Response):
1. **"Did I read the full relevant section of copilot-instructions.md?"**
2. **"Am I about to violate any Critical Rules?"**
3. **"Should I verify this with the user before proceeding?"**
4. **"Did I check enforcement tool output (`./tools/lint.sh`)?"**
5. **"Am I making assumptions, or do I have verified context?"**

### ⛔ STOP - Operations Requiring Explicit User Confirmation:
**Always ask user before:**
- Adding any `sleep_ms()`, `sleep_us()`, `busy_wait_ms()`, or `busy_wait_us()` call
- Modifying ring buffer logic or calling `ringbuf_reset()`
- Changing PIO programs (`.pio` files)
- Modifying protocol IRQ handlers
- Changing memory barriers or volatile declarations

**Immediately reject (do not ask):**
- Any use of Core 1 or multicore APIs
- Direct Flash execution
- Committing or staging docs-internal/ files
- Referencing docs-internal/ in public documentation or commit messages

---

## 🛑 MANDATORY CHECKLISTS

### ⛔ Before Modifying Protocol Handlers
- [ ] Read protocol README.md completely (timing diagrams, state machine)
- [ ] Understand PIO timing requirements (microsecond precision)
- [ ] Verify IRQ handler remains non-blocking (no loops, no printf, no sleep)
- [ ] Confirm ring buffer write path uses `ringbuf_put()` only
- [ ] Check memory barriers (`__dmb()`) after volatile writes
- [ ] Test with actual hardware (timing-sensitive)
- [ ] Run `./tools/lint.sh`

### ⛔ Before Modifying Ring Buffer Logic
- [ ] Maintain single-producer/single-consumer (IRQ writes, main reads)
- [ ] Never call `ringbuf_reset()` with IRQs enabled (unless LINT:ALLOW)
- [ ] Verify head/tail updates are atomic
- [ ] Test overflow/underflow conditions
- [ ] Confirm 32-byte buffer adequate for burst handling

### ⛔ Before Modifying HID Interface
- [ ] Always check `tud_hid_ready()` before sending reports
- [ ] Verify USB descriptor matches report structure
- [ ] Test with multiple host OSes (Windows, macOS, Linux)
- [ ] Validate NKRO behavior under high throughput (30 CPS test)

### ⛔ Before Adding/Modifying Time-Based Code
- [ ] Using `to_ms_since_boot(get_absolute_time())` (non-blocking)
- [ ] NOT using `sleep_ms()`, `sleep_us()`, `busy_wait_ms()`, `busy_wait_us()`
- [ ] If blocking truly necessary (debug only):
  - [ ] Added `LINT:ALLOW blocking` annotation with justification
  - [ ] Added IRQ protection: `if (in_irq()) return;`
  - [ ] Documented why blocking acceptable

### ⛔ Before Committing ANY Changes
- [ ] Run `./tools/lint.sh` (must pass 0 errors, 0 warnings)
- [ ] No blocking operations introduced (or properly annotated)
- [ ] No multicore APIs used
- [ ] No docs-internal/ files staged or referenced
- [ ] Build tested: `docker compose run --rm -e KEYBOARD="<config>" builder`
- [ ] If firmware modified: Test on real hardware if possible

---

## Quick Reference

**Performance:**
- Ring Buffer: 32 bytes
- Max Latency: 350μs (PIO IRQ → USB)
- CPU Usage: ~2% at 30 CPS (97.9% idle)
- Max Throughput: 30 CPS reliable (360 WPM), saturates at 32+ CPS

**Protocols:** AT/PS2, XT, Apple M0110, Amiga | **Mice:** AT/PS2 only | **Platform:** RP2040 (125MHz)

**Build:**
```bash
docker compose build  # One-time
docker compose run --rm -e KEYBOARD="modelm/enhanced" builder
docker compose run --rm -e KEYBOARD="modelf/pcat" -e MOUSE="at-ps2" builder
```

### Mouse Support

**Protocol:** AT/PS2 mouse protocol only (independent of keyboard protocol)

**Enabling mouse support:**
```bash
# Build with keyboard only
docker compose run --rm -e KEYBOARD="modelm/enhanced" builder

# Build with mouse only
docker compose run --rm -e MOUSE="at-ps2" builder

# Build with both keyboard and mouse (any keyboard + AT/PS2 mouse)
docker compose run --rm -e KEYBOARD="modelm/enhanced" -e MOUSE="at-ps2" builder
```

**Architecture:**
- `src/protocols/at-ps2/mouse_interface.h/c` - Mouse-specific protocol handling
- **Separate hardware lines**: Mouse uses dedicated CLK/DATA pins (independent from keyboard)
- **Separate PIO state machines**: Helper functions dynamically load protocol-specific PIO code
- Standard 3-byte packets: [Status, X-delta, Y-delta]
- Scroll wheel support: 4-byte extended packets (IntelliMouse protocol)
- Mouse can be used with **any keyboard protocol** (AT/PS2, XT, Amiga, M0110)

---

## Build System

**Development Environment:**
- Docker-based builds (recommended for consistency)
- Pico SDK: Auto-detected from `external/pico-sdk` submodule or `PICO_SDK_PATH` env var
- Pico SDK Version: 2.2.0 (pinned via git submodule and Docker)

**Pico SDK Setup:**
- **Submodule Location:** `external/pico-sdk` (Raspberry Pi Pico SDK 2.2.0)
- **Auto-detection:** CMakeLists.txt checks `PICO_SDK_PATH` env var first, then falls back to submodule
- **Clone with submodules:** `git clone --recurse-submodules <url>`
- **Initialize after clone:** `git submodule update --init --recursive`
- **Docker builds:** Self-contained, SDK 2.2.0 in image layer

**Build system flow:**
1. `KEYBOARD` env var → `src/cmake_includes/keyboard.cmake` reads `src/keyboards/<name>/keyboard.config`
2. Config defines: `KEYBOARD_PROTOCOL`, `KEYBOARD_CODESET`, `KEYBOARD_LAYOUT`
3. CMake includes: protocol handler, scancode processor, PIO files, keyboard layout
4. Output: `build/rp2040-converter.uf2` (also `.elf` and `.elf.map` for debugging)

---

## Data Flow Architecture

```
PIO Hardware (CLK/DATA) → IRQ Handler → Ring Buffer (32-byte FIFO)
                                              ↓
                                        Main Loop reads
                                              ↓
Protocol Handler → Scancode Decoder → Keymap → HID Interface → USB
```

**Key Components:**
- **PIO State Machines**: Hardware protocol timing (`.pio` files → headers)
- **Ring Buffer**: Lock-free FIFO (`src/common/lib/ringbuf.[ch]`) - IRQ writes head, main reads tail
- **Scancode Processor**: State machine for multi-byte sequences (E0/F0 prefixes, Pause key)
- **Thread Contexts**: IRQ (keyboard events) → Main Loop (USB/HID) → USB Callbacks (TinyUSB)

---

## Critical Patterns

### Non-Blocking Time-Based State Machines
```c
static uint32_t state_start_time = 0;
static enum { IDLE, WAITING, READY } state = IDLE;

void my_task(void) {
  uint32_t now = to_ms_since_boot(get_absolute_time());
  
  switch (state) {
    case IDLE:
      state = WAITING;
      state_start_time = now;
      break;
    case WAITING:
      if (now - state_start_time >= TIMEOUT_MS) {
        state = READY;
      }
      break;
    case READY:
      // Do work
      state = IDLE;
      break;
  }
}
```

**Exception:** Debug-only with `LINT:ALLOW blocking` + IRQ guard:
```c
if (in_irq()) return;  // Never block in IRQ
sleep_us(delay);  // LINT:ALLOW blocking - [justification]
```

### Ring Buffer Producer/Consumer
```c
// IRQ context (producer) - writes only
void __isr keyboard_irq_handler(void) {
  uint8_t scancode = pio_sm_get(keyboard_pio, keyboard_sm) >> 21;
  if (!ringbuf_is_full()) {
    ringbuf_put(scancode);
  }
}

// Main loop (consumer) - reads only
void keyboard_task(void) {
  if (!ringbuf_is_empty() && tud_hid_ready()) {
    uint8_t scancode = ringbuf_get();
    process_scancode(scancode);
  }
}
```

**Never** call `ringbuf_reset()` with IRQs enabled - only during init or state resets.

### IRQ-to-Main Communication
```c
static volatile uint8_t irq_flag = 0;
static volatile uint32_t irq_data = 0;

void __isr my_irq_handler(void) {
  irq_data = read_hardware_register();
  irq_flag = 1;
  __dmb();  // Memory barrier
}

void my_task(void) {
  if (irq_flag) {
    __dmb();  // Memory barrier
    uint32_t data = irq_data;
    irq_flag = 0;
    process_data(data);
  }
}
```

---

## Common Anti-Patterns (What NOT to Do)

### 🔴 CRITICAL (Breaks Functionality):
- ❌ Using Core 1 (single-core architecture)
- ❌ Blocking operations (breaks PIO timing)
- ❌ Flash execution (must run from RAM)
- ❌ `ringbuf_reset()` with IRQs enabled

### 🟡 IMPORTANT (Impacts Correctness):
- ❌ Forgetting `tud_hid_ready()` check → USB buffer overflow, dropped keys
- ❌ Missing `volatile` for IRQ-shared variables → stale data, race conditions
- ❌ `printf()` in IRQ context → UART DMA conflicts, hangs
- ❌ Assuming immediate USB transmission → saturation at 32+ CPS (10ms polling limit)
- ❌ Ring buffer access from both IRQ and main → data corruption

---

## File Organization

**Protocols** (`src/protocols/<name>/`):
- `<name>.pio` - PIO assembly for timing
- `keyboard_interface.h/c` - State machine, init, LED commands
- `common_interface.c` - Standardized init/task/led
- `README.md` - Specification with timing diagrams

**Keyboards** (`src/keyboards/<brand>/<model>/`):
- `keyboard.config` - CMake vars: KEYBOARD_PROTOCOL, KEYBOARD_CODESET, KEYBOARD_LAYOUT
- `keyboard.h` - Layout defines (override command mode keys)
- `keyboard.c` - Keymap using KEYMAP_<LAYOUT>() macros
- `README.md` - Specifications and quirks

**Common Libraries** (`src/common/lib/`):
- `ringbuf.c/h`, `hid_interface.c/h`, `command_mode.c/h`, `pio_helper.c/h`, `uart.c/h`

**Scancodes** (`src/scancodes/<set>/`):
- `scancode_processor.h/c` - Multi-byte sequence state machine
- `README.md` - Scancode tables

---

## Common Tasks

### Adding Protocol Support
1. Create `src/protocols/<name>/` with `.pio`, `keyboard_interface.[ch]`, `common_interface.c`
2. PIO handles timing, IRQ extracts frame, queues to ring buffer
3. Protocol state machine in `keyboard_interface_task()`: initialization, error recovery, LED commands
4. Reference: `at-ps2/` (complex bidirectional), `xt/` (simple unidirectional)

### Keyboard-Specific Overrides
Command Mode keys (bootloader entry): Default is Left Shift + Right Shift. Override in `keyboard.h` **before includes**:

```c
// keyboards/apple/m0110a/keyboard.h - single shift key issue
#define CMD_MODE_KEY1 KC_LSHIFT
#define CMD_MODE_KEY2 KC_LALT     // Override: Shift + Alt instead
#include "hid_keycodes.h"         // Include AFTER defines
```

**Why "before includes":** `#ifndef` guards in `command_mode.c` implement "first definition wins". Keyboard definition takes precedence over defaults.

### Debugging
- **UART output**: `printf()` uses DMA-driven UART (non-blocking) - connect GPIO pins to USB-UART adapter
- **Error patterns**: `[ERR]` prefix, `[DBG]` for development, `[INFO]` for initialization
- **State machine issues**: Look for `!INIT!` messages indicating unexpected scancode sequences
- **Build with debug**: All `printf()` statements active in normal builds

---

## Performance Metrics (Measured)

### Pipeline Latency (October 2024 - Real Hardware)
Measured with IBM Model M Enhanced keyboard (AT/PS2 protocol):

| Stage | Time (μs) | Percentage | Description |
|-------|-----------|------------|-------------|
| PIO_IRQ → RINGBUF_PUT | 44 | 12.6% | IRQ handler overhead |
| RINGBUF_PUT → RINGBUF_GET | 48 | 13.7% | Main loop polling |
| RINGBUF_GET → SCANCODE_DONE | 47 | 13.4% | Scancode state machine |
| SCANCODE_DONE → HID_START | 69 | 19.7% | Keymap lookup + command mode |
| HID_START → HID_END | 141 | 40.3% | HID report + USB stack |
| **Total End-to-End** | **350** | **100%** | **PIO IRQ → USB transmission** |

**Timing Consistency:** ±0 μs variance (deterministic, non-blocking architecture proven)

### Resource Usage
- **CPU Utilization:** 2.1% at 30 CPS maximum throughput (97.9% idle)
- **RAM Usage:** ~50% of 264KB SRAM (132KB used for code + data + stack)
- **Flash Usage:** ~200KB for program + const data
- **Ring Buffer:** 32 bytes (adequate for burst handling)

**Key Finding:** USB polling (10ms) is the only bottleneck. Processing (350 μs) is NOT a limiting factor.

---

## Troubleshooting Decision Tree

**Keys not registering?**
```
Ring buffer full? → USB saturated or main loop blocked (check tud_hid_ready())
Protocol errors? → Check UART for [ERR]/!INIT! (power cycle keyboard)
Wrong keymap? → Verify KEYBOARD_CODESET matches keyboard
```

**Build fails?**
```
KEYBOARD env var missing? → Set with docker compose run -e KEYBOARD="..."
keyboard.config missing? → Path: src/keyboards/<brand>/<model>/keyboard.config
Linker errors? → Verify KEYBOARD_PROTOCOL in keyboard.config
```

**High CPU/latency?**
```
Check for: sleep_ms(), busy_wait_us(), long loops without yield
Use: to_ms_since_boot(get_absolute_time()) for timing
```

---

## Enforcement Tools

**Before every commit:**
```bash
./tools/lint.sh  # Must pass (0 errors, 0 warnings)
```

**Automated checks:**
1. **Lint Script** - Detects blocking ops, multicore APIs, printf in IRQ, docs-internal violations
2. **Runtime RAM Check** - Debug builds verify SRAM execution (not Flash)
3. **Git Hooks** - Blocks docs-internal staging/references
4. **CI Pipeline** - Lint strict mode, build matrix (8 configs), memory limits (Flash<230KB, RAM<150KB)
5. **PR Template** - Mandatory architecture checklist

---

## Documentation Policy

**Public (committed to git):** 
- README.md, src/protocols/*/README.md, src/keyboards/README.md, src/scancodes/*/README.md
- All source code, CMake files, configuration files
- Public-facing documentation that describes the project

**Private (LOCAL ONLY - NEVER commit):** 
- `docs-internal/` - Local development notes, performance analysis, investigations, architecture decisions
- ✅ **DO**: Create, modify, and update docs-internal/ files for development reference
- ✅ **DO**: Use docs-internal/ as information source when developing (historical context, findings, decisions)
- ✅ **DO**: Reference docs-internal/ content when making technical decisions (along with source code, Pico SDK docs, READMEs)
- ❌ **NEVER**: Stage, commit, or push docs-internal/ files to git
- ❌ **NEVER**: Reference docs-internal/ in public documentation, commit messages, or PR descriptions
- 📍 **Purpose**: Local-only knowledge base for deep dives, investigations, and development context

---

## Code Quality and Maintenance

### General Coding Standards
- Use consistent indentation (4 spaces per level, no tabs)
- Use descriptive variable and function names
- Keep functions short and focused on a single task
- Comment complex logic and decisions
- Use snake_case for variables/functions, UPPER_CASE for macros/constants
- Follow existing patterns in files you're modifying
- Always include header comments in `.c` and `.h` files
- Use Doxygen-style comments for functions and complex structures
- Avoid global variables where possible
- PicoSDK uses C11 standard, ensure compatibility
- Adhere to coding conventions used in PicoSDK and TinyUSB libraries

### Code Quality Audit Checklist
**When to run:** Before commits, after significant changes

- [ ] Code adheres to style guidelines (indentation, naming conventions)
- [ ] No unused variables or functions
- [ ] Functions are modular and single-purpose
- [ ] Comments are clear and relevant
- [ ] No hardcoded values (use constants or enums)
- [ ] Error handling implemented where necessary
- [ ] No memory leaks or buffer overflows
- [ ] All TODOs addressed or documented
- [ ] 🔴 **STRICTLY no blocking operations at any time**
- [ ] Run `./tools/lint.sh` before committing

### Repository Structure Quick Reference
```
.github/                    # GitHub workflows, issue templates, copilot instructions
docs-internal/              # Local-only documentation (never commit)
src/
  ├── common/              # Common libraries and utilities
  ├── keyboards/           # Keyboard configurations and layouts
  ├── protocols/           # Protocol implementations (AT/PS2, XT, etc.)
  ├── scancodes/           # Scancode mappings
  └── main.c               # Main application entry point
build/                      # Build output (firmware files)
tools/                      # Enforcement tools (lint.sh, test scripts)
external/pico-sdk/         # Pico SDK submodule
```

---

## AI Self-Check (Run Before EVERY Response)

1. ✅ Read relevant copilot-instructions.md section completely?
2. ✅ Will this violate Critical Rules (blocking, multicore, Flash)?
3. ✅ Should I ask user before proceeding?
4. ✅ Did I run/verify ./tools/lint.sh?
5. ✅ Am I citing facts, or making assumptions?

**High-risk operations:**
- Adding blocking call → **ASK USER FIRST**
- Modifying ring buffer → **ASK USER FIRST**
- Changing PIO programs → **ASK USER FIRST**
- Modifying protocol IRQ handlers → **ASK USER FIRST**
- Touching multicore APIs → **REJECT IMMEDIATELY**

**Validation sources:**
- **Source code**: Ultimate truth for implementation
- **Pico SDK docs**: RP2040 hardware capabilities
- **docs-internal/**: Performance data, architecture decisions, investigation findings (use freely for context, never commit)
- **Protocol/Keyboard READMEs**: Specifications and implementation details

---

**Remember:** This RP2040 firmware is single-core, non-blocking, RAM-only execution. Builds are fast, tests are focused, codebase optimized for resource-constrained embedded systems. Use docs-internal/ freely as your development knowledge base!
