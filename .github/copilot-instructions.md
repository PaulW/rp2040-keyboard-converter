# RP2040 Keyboard Converter

Embedded firmware that converts vintage keyboard/mouse protocols (AT/PS2, XT, Apple M0110, Amiga) to USB HID using RP2040's PIO hardware for protocol handling. **Single-core only** (Core 0), runs from SRAM for performance.

**Critical Architecture Rules:**
- ⚠️ **NEVER use blocking operations** - main loop runs at ~100kHz, protocols require microsecond timing
- ⚠️ **All code runs from RAM** (`pico_set_binary_type(copy_to_ram)`) - never assume Flash execution
- ⚠️ **Single-core only** - do not use Core 1 (multicore adds complexity without performance benefit for this use case)
- ⚠️ **Thread safety**: IRQ (producer) → Ring Buffer → Main Loop (consumer). Use volatile + memory barriers (`__dmb()`)

## Quick Reference

**Performance Characteristics:**
- Ring Buffer Size: 32 bytes
- Maximum Latency: 350μs (PIO IRQ → USB)
- CPU Usage: ~2% at 30 CPS throughput (97.9% idle)
- Maximum Throughput: 30 CPS reliable (360 WPM), saturates at 32+ CPS

**Key Rules:**
- Non-blocking: Use `to_ms_since_boot(get_absolute_time())`, NEVER `sleep_*()`
- IRQ Safety: Use `volatile` + `__dmb()` for shared variables
- USB Check: Always `tud_hid_ready()` before sending HID reports

**Supported Hardware:**
- Protocols: AT/PS2, XT, Apple M0110, Amiga
- Mice: AT/PS2 only
- Platform: RP2040 (ARM Cortex-M0+, 125MHz default)

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
docker compose run --rm -e KEYBOARD="xt/ibm5150" -e MOUSE="at-ps2" builder
```

**Architecture:**
- `src/protocols/at-ps2/mouse_interface.h/c` - Mouse-specific protocol handling
- **Separate hardware lines**: Mouse uses dedicated CLK/DATA pins (independent from keyboard)
- **Separate PIO state machines**: Helper functions dynamically load protocol-specific PIO code into available state machines
- Standard 3-byte packets: [Status, X-delta, Y-delta]
- Scroll wheel support: 4-byte extended packets (IntelliMouse protocol)

**Flexibility:**
- Mouse can be used with **any keyboard protocol** (AT/PS2, XT, Amiga, M0110)
- Keyboard-only, mouse-only, or both configurations supported
- Each protocol uses dedicated PIO state machine with sufficient space

## Build System (Docker-based, CMake configuration)

**Development Environment:**
- Docker-based builds (recommended for consistency)
- Native builds possible with Pico SDK submodule or local SDK setup
- Pico SDK: Auto-detected from `external/pico-sdk` submodule, or via `PICO_SDK_PATH` environment variable
- Pico SDK Version: 2.2.0 (pinned via git submodule and Docker)

**Pico SDK Setup:**
- **Submodule Location:** `external/pico-sdk` (Raspberry Pi Pico SDK 2.2.0)
- **Auto-detection:** CMakeLists.txt checks `PICO_SDK_PATH` env var first, then falls back to submodule
- **Nested Submodules:** TinyUSB, lwIP, btstack, mbedtls, cyw43-driver (automatically initialized)
- **Clone with submodules:** `git clone --recurse-submodules https://github.com/PaulW/rp2040-keyboard-converter.git`
- **Initialize after clone:** `git submodule update --init --recursive` (if not cloned with `--recurse-submodules`)
- **Docker builds:** Self-contained, build SDK 2.2.0 in image layer (no submodule needed)

**Building firmware:**
```bash
# Build Docker container first (one-time)
docker compose build

# Build for specific keyboard (required)
docker compose run --rm -e KEYBOARD="modelm/enhanced" builder

# Build with keyboard + mouse support
docker compose run --rm -e KEYBOARD="modelf/pcat" -e MOUSE="at-ps2" builder
```

**Build system flow:**
1. `KEYBOARD` env var → `src/cmake_includes/keyboard.cmake` reads `src/keyboards/<name>/keyboard.config`
2. Config defines: `KEYBOARD_PROTOCOL` (at-ps2/xt/apple-m0110/amiga), `KEYBOARD_CODESET` (set1/set2/set3/set123)
3. CMake automatically includes: protocol handler, scancode processor, PIO files, keyboard layout
4. Output: `build/rp2040-converter.uf2` (also `.elf` and `.elf.map` for debugging)

**Pico SDK Detection (CMakeLists.txt):**
```cmake
# Priority 1: User-specified environment variable
if(DEFINED ENV{PICO_SDK_PATH})
  set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
  
# Priority 2: Git submodule fallback (automatic for contributors)
elseif(EXISTS "${CMAKE_CURRENT_LIST_DIR}/../external/pico-sdk/pico_sdk_init.cmake")
  set(PICO_SDK_PATH ${CMAKE_CURRENT_LIST_DIR}/../external/pico-sdk)
  
# Error: Neither found
else()
  message(FATAL_ERROR "Pico SDK not found. Clone with --recurse-submodules or set PICO_SDK_PATH")
endif()
```

**Adding new keyboards:** Create `src/keyboards/<name>/` with `keyboard.config`, `keyboard.h`, `keyboard.c`. Use `KEYMAP_<LAYOUT>()` macro patterns from existing keyboards.

## Data Flow Architecture

```
PIO Hardware (CLK/DATA) → IRQ Handler → Ring Buffer (32-byte FIFO)
                                              ↓
                                        Main Loop reads
                                              ↓
Protocol Handler → Scancode Decoder → Keymap → HID Interface → USB
```

**Key points:**
- **PIO State Machines**: Handle low-level protocol timing in hardware (`.pio` files compiled to headers)
- **Ring Buffer**: Lock-free FIFO (`src/common/lib/ringbuf.[ch]`) - IRQ writes head, main reads tail
- **Scancode Processor**: State machine for multi-byte sequences (E0/F0 prefixes, Pause key, etc.)
- **Thread contexts**: IRQ (keyboard events) → Main Loop (USB/HID) → USB Callbacks (TinyUSB)

## Critical Patterns

### Non-Blocking Requirement
**Wrong:** `sleep_ms()`, `busy_wait_us()`, `sleep_us()` in any context (breaks PIO timing)  
**Right:** Time-based state machines using `to_ms_since_boot(get_absolute_time())`

Example from `command_mode_task()`:
```c
void command_mode_task(void) {
  if (cmd_mode.state == CMD_MODE_IDLE) return;  // Early exit - most common case
  uint32_t now_ms = to_ms_since_boot(get_absolute_time());
  if (now_ms - cmd_mode.state_start_time_ms >= CMD_MODE_HOLD_TIME_MS) {
    // Transition after 3 seconds without blocking
  }
}
```

### Ring Buffer Usage (IRQ-safe)
```c
// IRQ context (producer):
if (!ringbuf_is_full()) ringbuf_put(scancode);

// Main loop (consumer):
if (!ringbuf_is_empty() && tud_hid_ready()) {  // Check USB ready to avoid overflow
  uint8_t scancode = ringbuf_get();
  process_scancode(scancode);
}
```

**Never** call `ringbuf_reset()` while IRQs enabled - only during initialization or state machine resets.

### PIO Protocol Implementation
PIO programs (`.pio` files) handle hardware protocols. Key configuration:
```c
// AT/PS2: bidirectional, 11-bit frames (Start + 8 data + Parity + Stop)
sm_config_set_in_shift(&c, true, true, 11);   // LSB-first, autopush
sm_config_set_jmp_pin(&c, pin + 1);           // CLOCK pin for timing

// XT: unidirectional, 9-bit frames (Start + 8 data)
sm_config_set_in_shift(&c, true, true, 9);    // Simple receive-only
```

Clock divider calculation: `calculate_clock_divider(min_pulse_width_us)` - based on protocol specs (30µs for AT/PS2, 160µs for M0110).

## ⚠️ Common Gotchas

### 1. Forgetting to check `tud_hid_ready()` before sending HID reports
**Symptom:** Dropped key events under high throughput  
**Why it happens:** USB host polls at 10ms intervals - sending faster causes buffer overflow  
**Fix:** Always check USB readiness before calling `send_keyboard_report()`:
```c
if (!ringbuf_is_empty() && tud_hid_ready()) {  // Both conditions required
  process_scancode(ringbuf_get());
}
```

### 2. Not using volatile for IRQ-shared variables
**Symptom:** Compiler optimizations cause stale data reads, race conditions  
**Why it happens:** Compiler doesn't know IRQ modifies variables  
**Fix:** Declare with `volatile` and add memory barriers:
```c
static volatile uint8_t irq_flag = 0;

void __isr my_irq(void) {
  irq_flag = 1;
  __dmb();  // Memory barrier
}

void main_task(void) {
  if (irq_flag) {
    __dmb();  // Memory barrier
    irq_flag = 0;
    // Handle event
  }
}
```

### 3. Calling `printf()` in IRQ context
**Symptom:** UART DMA conflicts, garbled output, hangs  
**Why it happens:** `printf()` uses DMA which isn't IRQ-safe  
**Fix:** Use LOG_* macros from IRQ, or defer logging to main loop

### 4. Assuming immediate USB transmission
**Symptom:** USB saturation at 32+ CPS (>10ms per character)  
**Why it happens:** USB host polling is 10ms, not instant  
**Fix:** Design for 30 CPS maximum (360 WPM) reliable throughput. This is NOT a firmware limitation - it's USB HID spec.

### 5. Modifying ring buffer from both IRQ and main
**Symptom:** Data corruption, lost scancodes  
**Why it happens:** Violates single-producer/single-consumer pattern  
**Fix:** IRQ only calls `ringbuf_put()`, main loop only calls `ringbuf_get()`

## Keyboard-Specific Overrides (Preprocessor Pattern)

**Command Mode keys** (bootloader entry): Default is Left Shift + Right Shift. Override in `keyboard.h` **before includes**:

```c
// keyboards/apple/m0110a/keyboard.h - single shift key issue
#define CMD_MODE_KEY1 KC_LSHIFT
#define CMD_MODE_KEY2 KC_LALT     // Override: Shift + Alt instead
#include "hid_keycodes.h"         // Include AFTER defines
```

**Why "before includes":** `#ifndef` guards in `command_mode.c` implement "first definition wins". Keyboard definition takes precedence over defaults. Validated at compile time with `_Static_assert()`.

## Common Tasks

### Adding Protocol Support
1. Create `src/protocols/<name>/` with `.pio`, `keyboard_interface.[ch]`, `common_interface.c`
2. PIO handles timing, IRQ extracts frame, queues to ring buffer
3. Protocol state machine in `keyboard_interface_task()`: initialization, error recovery, LED commands
4. Reference: `at-ps2/` (complex bidirectional), `xt/` (simple unidirectional)

### File Organization

**Protocol Implementation** (`src/protocols/<name>/`):
- `<name>.pio` - PIO assembly for hardware timing
- `keyboard_interface.h/c` - Protocol state machine, initialization, LED commands
- `common_interface.c` - Standardized init/task/led functions that call keyboard_interface
- `README.md` - Protocol specification with timing diagrams

**Keyboard Configuration** (`src/keyboards/<brand>/<model>/`):
- `keyboard.config` - CMake variables: `KEYBOARD_PROTOCOL`, `KEYBOARD_CODESET`, `KEYBOARD_LAYOUT`
- `keyboard.h` - Layout-specific defines (override command mode keys here)
- `keyboard.c` - Keymap using `KEYMAP_<LAYOUT>()` macros
- `README.md` - Keyboard specifications and quirks

**Common Libraries** (`src/common/lib/`):
- `ringbuf.c/h` - Ring buffer implementation
- `ws2812/` - WS2812 LED control
- `hid_interface.c/h` - USB HID interface and report building
- `command_mode.c/h` - Bootloader entry and command mode logic
- `pio_helper.c/h` - PIO utility functions (clock divider calculation, etc.)
- `uart.c/h` - UART/logging utilities
- Other utility files (config_storage, led_helper, keymaps, etc.)

**Scancode Processors** (`src/scancodes/<set>/`):
- `scancode_processor.h/c` - State machine for multi-byte sequences
- `README.md` - Scancode tables and sequences

### Scancode Translation
- **SET123 (unified)**: `src/scancodes/set123/` handles Sets 1/2/3 via runtime configuration
- **Legacy**: Separate `set1/`, `set2/`, `set3/` for simple cases
- State machine tracks prefixes: `E0`, `F0`, `E1` sequences - see `process_scancode()` switch statement

### Code Examples

#### Time-Based State Machine (Non-Blocking)
```c
static uint32_t state_start_time = 0;
static enum { IDLE, WAITING, READY } state = IDLE;

void my_task(void) {
  uint32_t now = to_ms_since_boot(get_absolute_time());
  
  switch (state) {
    case IDLE:
      // Start waiting period
      state = WAITING;
      state_start_time = now;
      break;
      
    case WAITING:
      // Non-blocking timeout check
      if (now - state_start_time >= TIMEOUT_MS) {
        state = READY;
        state_start_time = now;  // Reset for next state
      }
      break;
      
    case READY:
      // Do work
      state = IDLE;
      break;
  }
}
```

#### Safe IRQ-to-Main Communication
```c
// Shared variables (with proper synchronization)
static volatile uint8_t irq_flag = 0;
static volatile uint32_t irq_data = 0;

// IRQ context (producer)
void __isr my_irq_handler(void) {
  irq_data = read_hardware_register();  // Get data
  irq_flag = 1;                          // Signal main loop
  __dmb();                               // Memory barrier - ensure writes complete
}

// Main loop (consumer)
void my_task(void) {
  if (irq_flag) {
    __dmb();                      // Memory barrier - ensure reads are fresh
    uint32_t data = irq_data;     // Copy data
    irq_flag = 0;                 // Clear flag
    
    // Process data (not in IRQ context)
    process_data(data);
  }
}
```

#### Ring Buffer Producer/Consumer Pattern
```c
// Producer (IRQ context) - writes only
void __isr keyboard_irq_handler(void) {
  uint8_t scancode = pio_sm_get(keyboard_pio, keyboard_sm) >> 21;
  
  if (!ringbuf_is_full()) {
    ringbuf_put(scancode);  // Only IRQ writes to ring buffer
  } else {
    // Log error (ringbuf full = USB can't keep up)
    error_count++;
  }
}

// Consumer (main loop) - reads only
void keyboard_task(void) {
  if (!ringbuf_is_empty() && tud_hid_ready()) {
    uint8_t scancode = ringbuf_get();  // Only main loop reads from ring buffer
    process_scancode(scancode);
  }
}
```

### Debugging
- **UART output**: `printf()` uses DMA-driven UART (non-blocking) - connect GPIO pins to USB-UART adapter
- **Build with debug**: All `printf()` statements active in normal builds
- **Error patterns**: `[ERR]` prefix, `[DBG]` for development, `[INFO]` for initialization
- **State machine issues**: Look for `!INIT!` messages indicating unexpected scancode sequences

### Troubleshooting Decision Tree

**Keys not registering?**
```
├─ Check ring buffer: Is it full?
│  ├─ Yes → USB saturated or main loop blocked
│  │        Check: tud_hid_ready() being called?
│  │        Check: No blocking operations in main loop?
│  └─ No → Check protocol initialization
│           Look for: [INFO] messages showing state transitions
│
├─ Check UART output: Any [ERR] or !INIT! messages?
│  ├─ Yes → Protocol state machine issue
│  │        Common: Keyboard sent unexpected byte during init
│  │        Fix: Power cycle keyboard, check wiring
│  └─ No → Check keymap configuration
│           Verify: Correct keyboard model selected
│           Verify: KEYBOARD_CODESET matches keyboard
│
└─ Check LED status (if CONVERTER_LEDS enabled)
   ├─ Red → Protocol error (check UART)
   ├─ Yellow → Not initialized (keyboard not responding)
   └─ Green → Should be working
              Check: USB cable connected?
              Check: Host seeing keyboard in device list?
```

**Build fails?**
```
├─ Check KEYBOARD env var set?
│  ├─ No → Set with: docker compose run --rm -e KEYBOARD="modelm/enhanced" builder
│  └─ Yes → Verify keyboard.config exists
│           Path: src/keyboards/<brand>/<model>/keyboard.config
│
├─ Check Docker running?
│  ├─ No → Start Docker, then: docker compose build
│  └─ Yes → Check for CMake errors
│           Common: Missing protocol or scancode files
│           Fix: Verify KEYBOARD_PROTOCOL in keyboard.config
│
└─ Linker errors?
   └─ Missing function: Verify all required files in CMakeLists.txt
      Missing symbol: Check for typos in function names
```

**High CPU usage or latency spikes?**
```
└─ Check for blocking operations
   ├─ Look for: sleep_ms(), busy_wait_us(), sleep_us()
   ├─ Look for: Long loops without yield
   └─ Use: to_ms_since_boot(get_absolute_time()) for timing
```

## Documentation Policy

### Public Documentation (Committed to Git)
- `README.md` - Project overview and build instructions
- `src/protocols/<name>/README.md` - Protocol specifications and timing diagrams  
- `src/keyboards/README.md` - Keyboard configuration guide
- `src/scancodes/<set>/README.md` - Scancode set tables and sequences

### Private Documentation (Local Only - NEVER Commit)
- `docs-internal/` - Architecture decisions, performance analysis, investigation notes
  - ✅ **DO**: Create and update for your own reference and analysis
  - ✅ **DO**: Use when making technical decisions or understanding architecture
  - ✅ **DO**: Keep up to date with findings and discoveries
  - ❌ **NEVER**: Stage, commit, or push to git
  - ❌ **NEVER**: Reference in public documentation
  - 📍 **Purpose**: Local-only knowledge base for deep dives and investigations

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

**Measurement Method:** Instrumented timing points in code capturing timestamps at each pipeline stage.

### Throughput Limits
- **Maximum Reliable:** 30 CPS (360 WPM) - 0% waits, perfect transmission
- **Saturation Point:** 32 CPS - 1.3% waits, observable data loss
- **Bottleneck:** USB HID polling (10ms host-controlled intervals)
- **Processing Headroom:** 350 μs processing ≪ 10 ms USB interval (97% idle time)

### Resource Usage
- **CPU Utilization:** 2.1% at 30 CPS maximum throughput (97.9% idle)
- **RAM Usage:** ~50% of 264KB SRAM (132KB used for code + data + stack)
- **Flash Usage:** ~200KB for program + const data
- **Ring Buffer:** 32 bytes (adequate for burst handling)

### Protocol-Specific Timing
- **AT/PS2:** 1.854 ms between bytes (keyboard transmission, not converter delay)
- **XT:** Similar timing characteristics
- **Amiga:** ~480 μs per byte (8 bits × 60μs) + bidirectional handshake
- **Apple M0110:** 330 μs clock cycle period (3.03 kHz)

**Key Finding:** USB polling (10ms) is the only bottleneck. Processing (350 μs) is NOT a limiting factor.

## Anti-Patterns to Avoid

### 🔴 CRITICAL (Breaks Functionality/Hardware)
- ❌ Using Core 1 (multicore adds complexity without performance benefit - investigated and rejected)
- ❌ Blocking operations anywhere (breaks PIO timing - microsecond precision required)
- ❌ Direct Flash execution (code must run from RAM via `pico_set_binary_type(copy_to_ram)`)
- ❌ Calling `ringbuf_reset()` with IRQs enabled (causes data corruption)

### 🟡 IMPORTANT (Impacts Correctness/Safety)
- ❌ Global variables without volatile (thread safety issues between IRQ/Main contexts)
- ❌ Reading ring buffer from IRQ context (violates single consumer pattern)
- ❌ Hot-swapping keyboards (protocol state machines don't support mid-operation changes)

### Safety Checklist for Critical Changes

**Before modifying Protocol Handlers:**
- [ ] Understand PIO timing requirements (check protocol README.md)
- [ ] Verify IRQ handler remains non-blocking (no loops, no printf)
- [ ] Test with actual hardware (timing-sensitive, simulator insufficient)
- [ ] Confirm ring buffer write path uses `ringbuf_put()` only
- [ ] Check for memory barriers (`__dmb()`) after volatile writes

**Before modifying Ring Buffer:**
- [ ] Maintain single-producer/single-consumer pattern
- [ ] Never call `ringbuf_reset()` with IRQs enabled
- [ ] Verify head/tail updates are atomic
- [ ] Test overflow/underflow conditions
- [ ] Confirm buffer size adequate for burst handling (32 bytes minimum)

**Before modifying HID Interface:**
- [ ] Always check `tud_hid_ready()` before sending reports
- [ ] Verify USB descriptor matches report structure
- [ ] Test with multiple host operating systems (Windows, macOS, Linux)
- [ ] Confirm modifier key handling preserves state correctly
- [ ] Validate NKRO behavior under high throughput (30 CPS test)

## 🧹 Code Quality and Maintenance

### Code Quality Audit
**When to run:** Before commits, after significant changes, or when requested by maintainer

Run this checklist periodically:
- [ ] Code adheres to style guidelines (indentation, naming conventions, etc.)
- [ ] No unused variables or functions
- [ ] Functions are modular and single-purpose
- [ ] Comments are clear and relevant
- [ ] No hardcoded values where possible (use constants or enums)
- [ ] Error handling is implemented where necessary
- [ ] No memory leaks or buffer overflows
- [ ] All TODOs are addressed or documented
- [ ] Code is optimized for performance where necessary
- [ ] 🔴 We adhere to STRICTLY no blocking operations at any time

### Performance Analysis
**When to run:** After adding features, when debugging performance issues, or before releases

Run this checklist periodically:
- [ ] CPU usage is within acceptable limits (e.g., < 50% on average)
- [ ] Memory usage is within acceptable limits (e.g., < 75% of available RAM)
- [ ] No memory fragmentation issues
- [ ] 🔴 No blocking operations at any time
- [ ] USB polling rate is stable and meets requirements
- [ ] No dropped or missed key events

### Repository Structure Quick Reference
```
.
├── .github/                     # GitHub-specific files (workflows, issue templates, etc.)
├── .vscode.example/             # Example VS Code configuration (optional local development)
├── docs-internal/               # Local-only documentation (never commit to git)
├── src/                         # Source code
│   ├── common/                  # Common libraries and utilities
│   ├── keyboards/               # Keyboard configurations and layouts
│   ├── protocols/               # Protocol implementations (USB, PS/2, etc.)
│   ├── scancodes/               # Scancode mappings
│   └── main.c                   # Main application entry point
├── build/                       # Build output (firmware files)
├── Dockerfile                   # Dockerfile for build environment
├── docker-compose.yml           # Docker Compose configuration
├── CMakeLists.txt               # CMake build configuration
└── README.md                    # Project overview and instructions
```

### Local Development (Optional)

**VS Code Setup:**
- Example configurations in `.vscode.example/` directory
- Copy to `.vscode/` for local use (gitignored)
- Requires: CMake, ARM GCC toolchain, `compile_commands.json`
- See `.vscode.example/README.md` for setup instructions

**What's Portable:**
- All source code uses relative paths
- CMakeLists.txt auto-detects SDK (env var → submodule)
- Docker builds completely self-contained
- No hardcoded user paths anywhere

**What's User-Specific:**
- `.vscode/` directory (gitignored)
- `compile_commands.json` (gitignored, generated by CMake)
- `.build-cache/` directory (gitignored, CMake build artifacts)
- Local SDK location (if not using submodule)

### Debugging Build Issues
If you encounter build issues, consider the following steps:
1. **Check Environment Variables**: Ensure that the `KEYBOARD` and `MOUSE` (if required for testing) environment variables are set correctly.
2. **Clean Build**: Sometimes, a clean build can resolve issues. You can delete the `build/` directory and try building again.
3. **Check Docker Setup**: Ensure that Docker is installed and running correctly on your system.
4. **Rebuild Docker Image**: If you suspect the Docker image might be outdated or corrupted, rebuild it using `docker compose build`.
5. **Review Error Messages**: Carefully read any error messages during the build process. They often provide clues about what went wrong.
6. **Check Local Notes**: Review your local documentation in `docs-internal/` if you've documented similar issues or architecture decisions.
7. **Seek Help**: If you're still stuck, consider reaching out to the project maintainers or community for assistance.

### General Coding Standards
- Use consistent indentation (4 spaces per level, no tabs).
- Use descriptive variable and function names.
- Keep functions short and focused on a single task.
- Comment complex logic and decisions.
- Use snake_case for variables/functions
- Use UPPER_CASE for macros and constants
- Follow existing variable naming patterns in files you're modifying
- Always include full Header comments in `.c` and `.h` files
- Use Doxygen-style comments for functions and complex structures
- Avoid global variables where possible; prefer passing parameters or using static variables within functions.
- PicoSDK uses C11 standard, so ensure compatibility.
- Adhere to the coding conventions used in the PicoSDK and TinyUSB libraries for consistency.

### Sync & Checklist
- **Update all relevant parts of the system when making a change.**
- **Run the Code Quality Audit checklist periodically.**
- **Checklist for Features/Bug Fixes**:
  - [ ] Code changes are complete and tested
  - [ ] Code Quality Audit checklist completed
  - [ ] Performance Analysis checklist completed
  - [ ] Documentation (docstrings, READMEs, etc) all updated if necessary (both internal and external)
  - [ ] Build process verified (clean build)
  - [ ] Changes committed with clear messages

### AI Rules

- **No hallucinations:** Confirm files/modules exist before referencing them. The full codebase is in your context.
- **Validate from multiple sources:**
  - **Source code**: The ultimate source of truth for implementation details
  - **Pico SDK documentation**: For RP2040 hardware capabilities and SDK APIs
  - **docs-internal/**: Contains performance measurements, architecture decisions, and investigation findings
  - Cross-reference all three when making technical claims
- **Be fully aware of Hardware Platform** Ensure all functions, code, and features use the RP2040 microcontroller and its capabilities.
- **Understand the Architecture**: Single-core (Core 0 only), non-blocking operations required.
- **Ask if the task is unclear.**
- **Do not overwrite existing logic unless explicitly requested.** Refactor or add new functions instead.
- **Plan before coding; reflect on the plan after.**
- **Maintain architectural purity:** All USB/HID logic runs on Core 0. Ensure we do not introduce blocking operations.
- **Don't forget PIO Files**: Protocols (and some other common libraries such as WS2812) use PIO assembly files. Ensure these are correctly referenced and used.

### Documentation Rules
- **⚠️ NEVER commit `docs-internal/` files**: These are local-only documentation. Never stage, commit, or push them to git.
- **⚠️ NEVER reference `docs-internal/` files in any Public documentation**: These files are for internal use only and should not be exposed externally.

### Copilot Instructions Policy
- **📖 `.github/copilot-instructions.md` is READ-ONLY**: This file contains project guidelines and architecture rules
  - ✅ Read and follow all instructions carefully
  - ✅ Reference when making decisions
  - ❌ Do not modify unless explicitly requested by the project maintainer
  - ❌ Do not commit changes without maintainer approval

Remember: This RP2040 Keyboard Converter is designed SPECIFICALLY for the RP2040 - builds are fast, tests are focused, and the codebase is optimized for resource-constrained environments.