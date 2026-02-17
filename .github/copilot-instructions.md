# RP2040 Keyboard Converter - AI Instructions

Embedded firmware converting keyboard/mouse protocols (AT/PS2, XT, Apple M0110, Amiga) to USB HID using RP2040's PIO hardware. **Single-core only** (Core 0), runs from SRAM for performance.

**Key Documentation:**

- [Contributing Guide](../docs/development/contributing.md) - PR process, commit format, contribution types
- [Code Standards](../docs/development/code-standards.md) - Coding conventions, testing scenarios, PIO standards
- [Adding Keyboards](../docs/development/adding-keyboards.md) - Step-by-step keyboard support guide
- [Custom Keymaps](../docs/development/custom-keymaps.md) - Keymap modification guide

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
- **USB**: Always check `tud_hid_ready()` before sending HID reports (8ms host polling - bInterval=8)
- **Platform**: RP2040 (ARM Cortex-M0+, 125MHz), 32-byte ring buffer, 350μs max latency

### Self-Verification Protocol (Run Before EVERY Response):

1. **"Did I read the full relevant section of copilot-instructions.md?"**
2. **"Am I about to violate any Critical Rules?"**
3. **"Should I verify this with the user before proceeding?"**
4. **"Did I check enforcement tool output (if modifying code)?"** - Run `./tools/lint.sh` for code changes only, not documentation
5. **"Am I making assumptions, or do I have verified context?"**
6. **"For documentation work: Is content fact-driven with conversational British English flow?"**

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
- [ ] Validate 6KRO (Boot Protocol) behavior under high throughput (30 CPS test)

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
- `keyboard.c` - Keymap using KEYMAP\_<LAYOUT>() macros
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

**See also:** Each protocol's README.md contains timing diagrams and implementation details.

### Adding Keyboard Support

For adding a new keyboard using an existing protocol, see the comprehensive guide in `docs/development/adding-keyboards.md`. Quick overview:

1. Create `src/keyboards/<brand>/<model>/` with four files: `keyboard.config`, `keyboard.h`, `keyboard.c`, `README.md`
2. Define KEYBOARD_PROTOCOL, KEYBOARD_CODESET, KEYBOARD_LAYOUT in keyboard.config
3. Create keymap using KEYMAP\_\*() macros matching physical layout
4. Test with actual hardware

### Modifying Keymaps

For remapping keys or creating alternative layouts, see `docs/development/custom-keymaps.md`. The keymap is just an array - change the HID keycode at any position to remap that physical key.

### Keyboard-Specific Overrides

**Command Mode Keys:** Default to Left Shift + Right Shift. For keyboards with unusual layouts (e.g., single shift key), override in `keyboard.h` **before includes**:

```c
// keyboards/apple/m0110a/keyboard.h - single shift key issue
#define CMD_MODE_KEY1 KC_LSHIFT
#define CMD_MODE_KEY2 KC_LALT     // Override: Shift + Alt instead
#include "hid_keycodes.h"         // Include AFTER defines
```

**Why "before includes":** `#ifndef` guards in `command_mode.c` implement "first definition wins". Keyboard definition takes precedence over defaults.

**Shift-Override (Non-Standard Shift Legends):** For keyboards with non-standard shift legends (terminal keyboards, vintage keyboards, international layouts, etc.), define `keymap_shift_override_layers[KEYMAP_MAX_LAYERS]` array of pointers in keyboard.c:

```c
// Per-layer shift-override for keyboards with non-standard shift legends
const uint8_t * const keymap_shift_override_layers[KEYMAP_MAX_LAYERS] = {
    [0] = (const uint8_t[SHIFT_OVERRIDE_ARRAY_SIZE]){
        [KC_1] = SUPPRESS_SHIFT | KC_EXLM,  // Shift+1 → ! (suppress shift)
        [KC_2] = SUPPRESS_SHIFT | KC_DQUO,  // Shift+2 → " (suppress shift)
        [KC_7] = SUPPRESS_SHIFT | KC_QUOT,  // Shift+7 → ' (suppress shift)
        // Only non-standard keys need entries, rest default to 0
    },
    // Optional: Define shift-override for additional layers
    // [1] = (const uint8_t[SHIFT_OVERRIDE_ARRAY_SIZE]){ ... },
};
```

- **SUPPRESS_SHIFT flag (0x80):** Removes shift modifier for final keycode
- **Without SUPPRESS_SHIFT:** Keeps shift held (standard behaviour)
- **Only map non-standard keys:** Empty entries (0) use default shift behaviour
- **Per-layer support:** Each layer can have different shift behaviour (e.g., Layer 0 modern, Layer 1 terminal)
- **NULL layers:** Layers without shift-override use standard behaviour
- **Examples:** IBM M122 (`modelm/m122/keyboard.c`) has complete terminal keyboard implementation
- **Disabled by default:** User must enable via Command Mode (Shift+Shift hold 3s, then 'S' key)
- **Runtime validation:** Automatically detects and logs errors if shift-override defined for non-existent layers

**NumLock Layers NOT Needed:** Numpad keys (KC_P0-P9) are dual-function by HID spec—host OS interprets as numeric (NumLock ON) or navigation (NumLock OFF). Don't create separate NumLock layers; send numpad codes directly. For macOS (no NumLock support), provide navigation in Fn layer.

**Layer State Persistence:** TG (toggle) and TO (switch to) layer states persist across reboots via config storage v3. System validates saved state using dual-hash (keyboard_id + layers_hash) to detect firmware/keymap changes. If validation fails (different keyboard, layer definitions changed), resets to Layer 0 automatically. MO (momentary) and OSL (one-shot) layers are temporary and never persist. Each keyboard MUST override `keymap_layer_count` to match the number of layers in `keymap_map` (e.g., `const uint8_t keymap_layer_count = 2;` before keymap_map definition) for correct hash validation and bounds checking. Without this override, the weak default of 8 could allow out-of-bounds access if layer-switch keys like MO_2+ are used beyond defined layers.

**See also:** `docs/development/adding-keyboards.md` covers all keyboard-specific customization options.

### Debugging and Testing

**UART Debug Output:**

- `printf()` uses DMA-driven UART (non-blocking) - connect GPIO 0/1 to USB-UART adapter
- Error patterns: `[ERR]` prefix, `[DBG]` for development, `[INFO]` for initialization
- State machine issues: Look for `!INIT!` messages indicating unexpected scancode sequences
- All `printf()` statements active in normal builds

**Testing Scenarios** (see `docs/development/code-standards.md` for details):

1. **Basic functionality** - Normal typing, verify keys work as expected
2. **Stress test** - Fast typing at 30+ CPS to test pipeline and buffering
3. **Command Mode** - Hold both shifts for 3 seconds, verify activation
4. **LEDs** - Test Caps/Num/Scroll Lock updates (if protocol supports)
5. **Error recovery** - Power cycle keyboard mid-operation, verify recovery
6. **Multi-OS testing** - Test on Windows, macOS, Linux for HID compatibility

---

## Performance Metrics (Measured)

### Pipeline Latency (October 2024 - Real Hardware)

Measured with IBM Model M Enhanced keyboard (AT/PS2 protocol):

| Stage                       | Time (μs) | Percentage | Description                    |
| --------------------------- | --------- | ---------- | ------------------------------ |
| PIO_IRQ → RINGBUF_PUT       | 44        | 12.6%      | IRQ handler overhead           |
| RINGBUF_PUT → RINGBUF_GET   | 48        | 13.7%      | Main loop polling              |
| RINGBUF_GET → SCANCODE_DONE | 47        | 13.4%      | Scancode state machine         |
| SCANCODE_DONE → HID_START   | 69        | 19.7%      | Keymap lookup + command mode   |
| HID_START → HID_END         | 141       | 40.3%      | HID report + USB stack         |
| **Total End-to-End**        | **350**   | **100%**   | **PIO IRQ → USB transmission** |

**Timing Consistency:** ±0 μs variance (deterministic, non-blocking architecture proven)

### Resource Usage

- **CPU Utilization:** 2.1% at 30 CPS maximum throughput (97.9% idle)
- **RAM Usage:** ~50% of 264KB SRAM (132KB used for code + data + stack)
- **Flash Usage:** ~80-120KB for program + const data (varies by configuration)
- **Ring Buffer:** 32 bytes (adequate for burst handling)

**Key Finding:** USB polling (8ms - bInterval=8) is the only bottleneck. Processing (350 μs) is NOT a limiting factor.

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

- README.md, src/protocols/_/README.md, src/keyboards/README.md, src/scancodes/_/README.md
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

### Documentation Writing Style

**Critical Guidelines for Public Documentation:**

1. **100% Fact-Driven Content**
   - NO assumptions about what protocols/keyboards are supported—point to directories (`src/protocols/`, `src/keyboards/`)
   - NO marketing language: Remove "pleased to know", "straightforward", "simply", "convenient", superlatives
   - NO protocol/feature enumeration in opening paragraphs—readers can check directories
   - Verify ALL technical details against codebase (file paths, function names, configuration options)

2. **Natural Conversational Flow**
   - Write flowing prose, NOT bullet-point lists (unless listing actual options/commands)
   - Use natural transitions: "First up", "Next", "Right, let's", "Now", "Finally"
   - Explain WHY not just WHAT: "This is important because...", "The reason for this..."
   - Self-aware about difficulties: "It's tedious work, I'll grant you", "trust me, you'll forget"
   - Conversational asides: "I know it might seem", "you'd be surprised"

3. **British English Throughout**
   - Use: "whilst", "colour", "initialise", "behaviour", "favour", "analyse"
   - NOT: "while", "color", "initialize", "behavior", "favor", "analyze"
   - Natural British phrasing: "have a look", "quite often", "at odd hours", "a bit of"

4. **Reference Style Examples**
   - Good: "Check `src/protocols/` for available protocol implementations"
   - Bad: "The project supports AT/PS2, XT, Amiga, and Apple M0110 protocols"
   - Good: "You'll create a new directory under `src/keyboards/<brand>/<model>/` and add four files"
   - Bad: "Create the following files: 1. keyboard.config, 2. keyboard.h, 3. keyboard.c, 4. README.md"

5. **Avoid These Patterns**
   - ❌ "You'll be pleased to know..."
   - ❌ "It's fairly straightforward..."
   - ❌ "Simply do X and Y..."
   - ❌ Numbered step lists when narrative flow works better
   - ❌ Bold headers for every subsection (use sparingly)
   - ❌ Listing all project features/protocols in introductions

6. **When to Use Bullet Lists**
   - ✅ Reference material (keycode tables, command options)
   - ✅ Actual configuration options from files
   - ✅ Quick checklists for verification
   - ❌ Explaining processes or workflows (use flowing prose)
   - ❌ Describing what to do step-by-step (use narrative)

**Style References:**

- docs/getting-started/building-firmware.md - Conversational, explains context
- docs/getting-started/hardware-setup.md - Natural flow with "Right, let's get"
- docs/development/contributing.md - British English, self-aware tone
- docs/development/code-standards.md - Explains why, not just what

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
4. ✅ Did I run/verify ./tools/lint.sh (for code changes only, NOT documentation)?
5. ✅ Am I citing facts, or making assumptions?
6. ✅ For documentation: Is this 100% fact-driven with natural conversational flow in British English?

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

### AI Behavioral Guidelines

- **No hallucinations**: Confirm files/modules exist before referencing them. The full codebase is in your context.
- **Validate from multiple sources**: Cross-reference source code, Pico SDK documentation, and docs-internal when making technical claims
- **Hardware Platform Awareness**: Ensure all functions, code, and features use the RP2040 microcontroller and its capabilities
- **Understand the Architecture**: Single-core (Core 0 only), non-blocking operations required
- **Ask if task unclear**: Don't make assumptions about requirements
- **Preserve existing logic**: Don't overwrite unless explicitly requested - refactor or add new functions instead
- **Plan before coding**: Think through approach, then reflect on the plan after implementation
- **Maintain architectural purity**: All USB/HID logic runs on Core 0 - ensure no blocking operations introduced
- **Remember PIO files**: Protocols (and libraries like WS2812) use PIO assembly files - ensure correctly referenced
- **Documentation style matters**: Public docs must be fact-driven, naturally flowing, British English—never list protocols/features, verify all technical claims against codebase

---

**Remember:** This RP2040 firmware is single-core, non-blocking, RAM-only execution. Builds are fast, tests are focused, codebase optimized for resource-constrained embedded systems. Use docs-internal/ freely as your development knowledge base!
