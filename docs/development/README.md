# Development Documentation

This guide helps developers contribute to the RP2040 keyboard converter project, whether you're adding support for a new keyboard, implementing a different protocol, or fixing bugs. The project maintains strict architectural constraints to achieve precise timing and deterministic performance, so understanding these principles before contributing is essential.

## Getting Started with Development

### Contributing Guide

Contributing to an embedded systems project requires understanding both the hardware constraints and the software architecture. Before writing any code, you need to internalize the critical design principles that make this converter work reliably across dozens of keyboard models.

Start by reviewing the [Code Standards](code-standards.md) documentation—this contains the critical architecture rules that must never be violated. The standards explain why blocking operations break protocol timing, why Core 1 stays disabled, and how the ring buffer maintains data integrity without locks. Every rejected pull request traces back to violating principles documented there.

Next, review the [Advanced Topics guide](../advanced/README.md) to understand how PIO state machines handle protocol timing, how the ring buffer coordinates interrupt and main loop contexts, and why code executes from SRAM rather than flash. This architectural overview connects the individual rules to the overall system design, helping you understand the "why" behind each constraint.

Finally, familiarize yourself with the [Protocol Documentation](../protocols/README.md) to see how different keyboard interfaces work—AT/PS2's bidirectional communication, XT's simple unidirectional signaling, Amiga's handshake protocol, and the M0110's variable timing. Understanding these protocols helps you write code that works correctly across the hardware diversity that computing presents.

The contribution process follows standard Git workflows with careful attention to quality. Fork the repository and clone it locally, then create a feature branch using conventional commit prefixes (`git checkout -b feat/my-feature`). Make your changes following the code standards detailed below, ensuring every function has clear comments and variable names explain their purpose. Before committing, run [`./tools/lint.sh`](../../tools/lint.sh) to catch architecture violations—the script must pass with zero errors and zero warnings, or continuous integration will reject your pull request.

Commit messages follow the conventional commits format, using prefixes like `feat:` for new features, `fix:` for bug corrections, `docs:` for documentation updates, `refactor:` for code restructuring, and `test:` for testing additions. Each commit message should explain what changed and why, helping reviewers understand your reasoning without needing to decode the diff. Finally, create a pull request with a clear description of what you've changed, why the change is necessary, and how you've tested it.

The CI pipeline automatically builds all keyboard configurations in the build matrix, runs lint checks, and enforces memory limits (Flash <230KB, RAM <150KB). CodeRabbit provides automated review focusing on embedded systems best practices and architecture compliance. These automated checks catch most issues before human review, but understanding the constraints yourself leads to cleaner initial submissions.

---

### Code Standards

Code quality in embedded systems goes beyond style preferences—it's about writing deterministic, maintainable code that behaves predictably under timing constraints. The project follows C11 standards with conventions optimized for readability and debugging.

Use 4-space indentation throughout (never tabs), which ensures consistent display across editors without configuration. Function and variable names use snake_case for familiarity to C programmers, while macros and constants use UPPER_CASE to make their compile-time nature immediately obvious. Choose descriptive names that explain purpose without needing comments—`keyboard_state_machine_task()` tells you more than `kbd_sm()` ever could.

The critical architecture rules deserve special emphasis because violating them breaks functionality rather than just style. Never use blocking operations like `sleep_ms()`, `sleep_us()`, `busy_wait_ms()`, or `busy_wait_us()` anywhere in production code. The main loop must continuously service the ring buffer and USB stack, so even brief blocking causes scancode loss. If you need time-based behavior, implement non-blocking state machines that check elapsed time on each iteration using `to_ms_since_boot(get_absolute_time())`.

Never call multicore APIs (`multicore_*`, `core1_*`) or attempt to use Core 1. The single-core architecture eliminates synchronization complexity and delivers predictable latency. Adding multicore would introduce bugs without performance benefit. Never use `printf()` or related functions in interrupt context—these functions use DMA-driven UART that conflicts with interrupt handlers. Use the `LOG_*` macros instead, which handle interrupt-safe output buffering.

Never call `ringbuf_reset()` with interrupts enabled. The ring buffer implements lock-free single-producer/single-consumer (SPSC) semantics, which breaks if you reset both pointers while an interrupt might be writing. Only reset during initialization before enabling interrupts, or within an explicit interrupt-disabled critical section. All code must execute from SRAM using the `copy_to_ram` binary type—this ensures deterministic timing without flash cache misses affecting latency measurements.

Use volatile qualifiers and memory barriers (`__dmb()`) when sharing data between interrupt and main contexts. The compiler and CPU both reorder operations for optimization, which breaks interrupt communication without explicit synchronization. Place `volatile` on variables written in IRQ and read in main, then use `__dmb()` after volatile writes and before volatile reads to enforce ordering.

Here's the pattern for non-blocking timeouts:
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

This pattern stores the start time when entering a waiting state, then checks elapsed time on each loop iteration without blocking. The main loop continues servicing other tasks while the timer runs, maintaining system responsiveness. Standards enforcement happens through [`./tools/lint.sh`](../../tools/lint.sh) automated checks and comprehensive rules in [Code Standards](code-standards.md).

---

### Testing

Testing embedded systems requires multiple verification layers—automated checks catch architecture violations, build testing ensures compilation across configurations, and hardware validation confirms real-world functionality. Each layer serves a different purpose in the quality assurance process.

Run the lint script before every commit: `./tools/lint.sh` must pass with zero errors and zero warnings. The script runs 17 comprehensive checks including blocking operations (`sleep_ms`, `busy_wait_us`), multicore API usage (`multicore_*`, `core1_*`), printf calls in interrupt context, indentation consistency (4-space required), volatile variable usage with memory barriers, header guards, include order, file headers, and protocol setup patterns. The script provides detailed error messages showing exactly which line violates which rule, making fixes straightforward. See [`tools/README.md`](../../tools/README.md) for complete check descriptions.

Build testing verifies that your changes compile successfully across different keyboard configurations. Test at least one configuration locally using Docker: `docker compose run --rm -e KEYBOARD="modelm/enhanced" builder`. If your changes affect protocol handling, test multiple protocols: `modelf/pcat` (AT/PS2), `modelf/xt` (XT), and `amiga/standard` (Amiga). The CI pipeline tests all configurations in the build matrix automatically, but catching build failures locally saves iteration time.

Hardware testing with actual keyboards remains essential—timing behavior, signal characteristics, and protocol quirks manifest only with real hardware. Follow the [Hardware Setup guide](../getting-started/hardware-setup.md) for wiring and level shifting. Test all keys systematically, paying special attention to extended keys (E0 prefixes), modifiers, and the Pause key (eleven-byte sequence). Use [Command Mode's keytest function](../features/command-mode.md) to verify HID reports for each key press. Test Command Mode bootloader entry (hold Left Shift + Right Shift for 3 seconds) to ensure the special mode still works. Monitor UART logs using the [Logging guide](../features/logging.md) to observe protocol state transitions and catch errors.

Additional testing resources include [`./tools/lint.sh`](../../tools/lint.sh) for lint checks, [`.github/workflows/ci.yml`](../../.github/workflows/ci.yml) for the complete CI build matrix, and [Building Firmware](../getting-started/building-firmware.md) for Docker build procedures.

---

## Development Guides

### Adding Protocol Support

Adding support for a new keyboard protocol involves implementing hardware timing in PIO assembly, managing protocol state in C, and integrating with the existing scancode and HID infrastructure. The task requires understanding both the target protocol's electrical characteristics and the converter's architectural patterns.

---

## Development Guides

### Adding Protocol Support

How to add support for new keyboard protocols:

**Steps:**
1. **Research**: Understand timing, framing, communication flow
2. **Create Directory**: `src/protocols/<name>/`
3. **PIO Program**: Write `.pio` for bit timing and framing

Start by researching the protocol thoroughly—you need to understand timing (clock frequency, bit duration, frame structure), signaling (voltage levels, idle states, edge polarities), and communication flow (unidirectional vs bidirectional, who generates the clock, how errors get detected). Datasheets, oscilloscope captures, and existing implementations all provide valuable information.

Create a directory structure at `src/protocols/<name>/` containing all the protocol-specific files. The PIO program (`<name>.pio`) implements the low-level bit timing and frame synchronization in PIO assembly—this hardware state machine handles clock edge detection, bit shifting, and interrupt generation without CPU involvement. The protocol interface (`keyboard_interface.h` and `keyboard_interface.c`) implements the state machine for protocol initialization, error recovery, and LED command handling. The common interface (`common_interface.c`) provides standardized `keyboard_init()`, `keyboard_task()`, and `keyboard_set_leds()` functions that the main loop calls, isolating protocol-specific details from the rest of the system.

**Protocol Setup Pattern:** All protocol implementations follow a standard initialization sequence. See the [Protocol Implementation Guide](protocol-implementation.md) for the complete pattern and explanation of why each step matters. Following this pattern ensures consistency, proper resource allocation, and correct error handling across all protocols.

Document the protocol comprehensively in a `README.md` file including timing diagrams, electrical specifications, state machine descriptions, and oscilloscope captures if available. This documentation helps future maintainers understand the protocol without reverse-engineering your code.

Study the reference implementations to understand the patterns. The [`src/protocols/xt/`](../../src/protocols/xt/) implementation demonstrates simple unidirectional protocols (see [XT protocol docs](../protocols/xt.md) for details). The [`src/protocols/at-ps2/`](../../src/protocols/at-ps2/) implementation shows complex bidirectional communication with host-to-device commands (see [AT/PS2 protocol docs](../protocols/at-ps2.md)). The [`src/protocols/amiga/`](../../src/protocols/amiga/) implementation illustrates unique handshake timing (see [Amiga protocol docs](../protocols/amiga.md)). The [`src/protocols/apple-m0110/`](../../src/protocols/apple-m0110/) handles variable timing protocols (see [M0110 protocol docs](../protocols/m0110.md)).

Finally, test with actual hardware—protocol timing issues only manifest with real keyboards, not in simulation. The [Protocols overview](../protocols/README.md) explains the overall protocol architecture, whilst the [PIO helper library](../../src/common/lib/pio_helper.c) provides utilities for loading and managing PIO programs.

---

### Adding Keyboard Support

Adding support for a specific keyboard model mostly involves configuration and keymap definition—the protocol and scancode infrastructure already handle the low-level communication, so you're primarily mapping physical keys to HID keycodes and providing metadata.

Start by identifying which protocol the keyboard speaks (AT/PS2, XT, Amiga, or M0110), which scancode set it uses (Set 1, Set 2, Set 3, or protocol-specific), and what connector it has (5-pin DIN, 6-pin mini-DIN, RJ-11, etc.). The keyboard's manual, online documentation, or community knowledge bases usually provide this information. If uncertain, test with a logic analyzer to observe the actual signals.

Create a directory at [`src/keyboards/`](../../src/keyboards/)`<brand>/<model>/` to hold all keyboard-specific files. The `keyboard.config` file uses simple KEY=value pairs to specify the keyboard's characteristics:

```ini
# src/keyboards/modelm/enhanced/keyboard.config
MAKE=IBM
MODEL=Model M Enhanced PC Keyboard
DESCRIPTION=IBM Personal Computer AT Enhanced Keyboard
PROTOCOL=at-ps2
CODESET=set123
```

See [`src/keyboards/modelm/enhanced/keyboard.config`](../../src/keyboards/modelm/enhanced/keyboard.config) for a complete example. The build system reads this configuration and automatically includes the appropriate protocol handler and scancode processor.

Define the keymap in `keyboard.c` using the layout macros that match the keyboard's physical arrangement. The KEYMAP macros arrange keycodes spatially to mirror the actual key positions, making it easy to verify correctness by visual inspection:

```c
// src/keyboards/modelm/enhanced/keyboard.c
#include "keyboard.h"

const uint8_t KEYMAP[256] = KEYMAP_US_ANSI(
  KC_ESC, KC_F1, KC_F2, ...
);
```

If the keyboard lacks both shift keys (like the Apple M0110 which has only one shift), override the Command Mode activation keys by defining `CMD_MODE_KEY1` and `CMD_MODE_KEY2` in `keyboard.h` before including other headers. The default combination (Left Shift + Right Shift) won't work on single-shift keyboards. See [Command Mode customization](../features/command-mode.md#keyboard-specific-configuration) for details on implementing these overrides.

Create a `README.md` documenting the keyboard's specifications, connector pinout, any quirks or special behavior, and build instructions. This helps users understand what they're building firmware for and assists troubleshooting.

Build and test with the actual hardware—verify all keys produce correct characters, test modifier combinations, confirm LEDs respond to system state, and validate Command Mode entry. Explore the [`src/keyboards/`](../../src/keyboards/) directory for examples, review the [Keyboards documentation](../keyboards/README.md) for patterns, and examine [`CMakeLists.txt`](../../src/CMakeLists.txt) to understand build integration.

---

### Adding Scancode Sets

Supporting a new scancode set requires implementing a state machine that assembles multi-byte sequences and a mapping table that translates scancodes to USB HID keycodes. The challenge lies in handling protocol-specific conventions like prefix bytes, make/break codes, and extended key sequences.

Research the scancode format thoroughly—understand whether the protocol uses separate make and break codes or a single code with a prefix, whether extended keys require special prefixes (like E0 in AT/PS2 Set 2), and whether any keys send multi-byte sequences (like the Pause key's eleven-byte sequence). Manufacturer documentation, community reverse-engineering efforts, and existing converter projects provide this information.

Create a directory at [`src/scancodes/`](../../src/scancodes/)`<set>/` for all scancode-specific files (see existing implementations in [docs/scancodes/](../scancodes/README.md) for examples). Implement the state machine in `scancode_processor.h` and `scancode_processor.c`—this code receives individual bytes from the protocol handler and assembles them into complete scancode events. The state machine tracks whether it's expecting a prefix byte, whether the next code represents a key press or release, and how to handle special cases like Pause or Print Screen.

Define the scancode-to-HID mapping in `scancode_map.h`, creating a lookup table that translates each scancode to its corresponding USB HID keycode. This simple mapping handles the bulk of translation, with the state machine managing sequence assembly complexity.

Document the scancode set completely in `README.md`, including tables showing every scancode's meaning, the prefix conventions, and examples of multi-byte sequences. This documentation becomes invaluable when debugging why a specific key doesn't work correctly.

Test exhaustively with hardware—press every key to verify correct translation, test all modifier combinations to catch mapping errors, validate that extended keys work properly, and confirm that special sequences like Pause and Print Screen generate correct HID codes.

Reference implementations demonstrate the patterns. The [Scancode Set 2](../scancodes/set2.md) implementation handles AT/PS2 with E0 and F0 prefixes. [Scancode Set 1](../scancodes/set1.md) shows simple make/break codes without prefixes. The [Amiga scancode processor](../scancodes/amiga.md) demonstrates 7-bit protocol-specific codes. The [combined Set 1/2/3 processor](../scancodes/set123.md) handles keyboards that can switch sets dynamically.

The [Scancode Sets documentation](../scancodes/README.md) provides detailed documentation for each set, while [HID keycodes](../../src/common/lib/hid_keycodes.h) defines all available USB HID codes for mapping.

---

### Build System

Understanding the build system (see [Advanced Topics - Build System](../advanced/build-system.md) for details):

**Docker Environment:**
```bash
# Build Docker image (one-time)
docker compose build

# Build firmware
docker compose run --rm -e KEYBOARD="modelm/enhanced" builder
```

**CMake Flow:**

### Build System

Understanding the build system helps when adding keyboards or debugging configuration issues (see [Advanced Topics - Build System](../advanced/build-system.md) for architectural details). The build flow transforms a keyboard name into a complete firmware binary tailored for that specific model.

The Docker environment provides: `docker compose run --rm -e KEYBOARD="modelm/enhanced" builder`. This command sets the `KEYBOARD` environment variable, which points to a configuration file at [`src/keyboards/`](../../src/keyboards/)`<brand>/<model>/keyboard.config`. The configuration file defines the protocol (`PROTOCOL=at-ps2`), scancode set (`CODESET=set123`), and other keyboard-specific parameters. CMake reads this configuration and automatically includes the appropriate protocol handler, scancode processor, and keymap files. The build outputs `build/rp2040-converter.uf2` ready for drag-and-drop flashing, along with `.elf` and `.elf.map` files for debugging.

For local development without Docker, initialize the Pico SDK submodule (`git submodule update --init --recursive`), configure the build directory (`cmake -B build`), and compile (`cmake --build build`). The local build path requires the Pico SDK and ARM toolchain installed manually, but provides faster iteration when actively developing.

Build resources include the [Building Firmware guide](../getting-started/building-firmware.md) for step-by-step procedures, [`CMakeLists.txt`](../../src/CMakeLists.txt) for build logic, [Docker setup](../../docker-compose.yml) for environment configuration, and [`keyboard.config` format examples](../../src/keyboards/modelm/enhanced/keyboard.config).

---

## Architecture Deep Dives

### Critical Architecture Rules

Before writing any code, read the [Code Standards](code-standards.md) documentation completely. This isn't a suggestion—it's the authoritative source for architectural constraints that must never be violated. Every architectural decision traces back to achieving precise timing with deterministic latency.

The five absolute prohibitions exist for specific technical reasons. No blocking operations (`sleep_ms`, `sleep_us`, `busy_wait_us`, `busy_wait_ms`) because the main loop must service the ring buffer and USB stack continuously—protocols require precise timing that blocking operations destroy. No multicore APIs (`multicore_*`, `core1_*`) because Core 1 stays disabled—the single-core architecture eliminates synchronization complexity while delivering predictable latency. No `printf` in IRQ context because printf uses DMA-driven UART that conflicts with interrupt handlers—use `LOG_*` macros instead, which implement interrupt-safe buffering. No `ringbuf_reset()` with interrupts enabled because the lock-free SPSC design breaks if both pointers reset while an interrupt might be writing—only reset during initialization or within explicit interrupt-disabled sections. No Flash execution because flash cache misses introduce unpredictable latency—code must run from SRAM using `copy_to_ram` to guarantee deterministic timing.

Understanding the "why" behind each rule helps you write compliant code naturally rather than fighting constraints. See [Advanced Topics](../advanced/README.md) for complete architecture documentation, [Code Standards](code-standards.md) for authoritative rules, and the [main loop implementation](../../src/main.c) for reference patterns.

---

### Ring Buffer Implementation

The ring buffer implements the critical handoff between interrupt context (where scancodes arrive) and the main loop (where they're processed). This lock-free single-producer/single-consumer (SPSC) design achieves thread-safety without locks, atomics, or blocking operations.

The 32-byte capacity (defined as `RINGBUF_SIZE` in [ringbuf.h](../../src/common/lib/ringbuf.h)) handles burst typing without overflow because the main loop polls frequently. The interrupt handler acts as the sole producer, writing to the head pointer. The main loop acts as the sole consumer, reading from the tail pointer. This strict separation means each context owns one pointer exclusively, eliminating concurrent modification possibilities.

Usage follows a clear pattern. In IRQ context (producer): read data from hardware, check if buffer is full using `ringbuf_is_full()`, then add to buffer with `ringbuf_put()` if space exists. In main loop context (consumer): check if buffer has data using `ringbuf_is_empty()`, then retrieve with `ringbuf_get()` if data exists, and process the scancode.

Three operations are strictly forbidden: never read from IRQ context (only write), never write from main loop (only read), and never call `ringbuf_reset()` with interrupts enabled. Violating these rules corrupts buffer state and causes dropped or duplicate scancodes. See [`src/common/lib/ringbuf.c`](../../src/common/lib/ringbuf.c) and [`ringbuf.h`](../../src/common/lib/ringbuf.h) for implementation details.

---

### PIO Programming

The RP2040's Programmable I/O subsystem provides eight independent state machines (four per PIO block) that execute custom assembly programs. These aren't general-purpose processors—they're specialized for GPIO manipulation with a minimal instruction set focused on bit shifting, pin waiting, and conditional jumps. This specialization enables precise protocol timing without CPU involvement.

PIO handles protocol bit timing by monitoring clock edges and sampling data at precisely the right moment, frame synchronization by detecting start/stop conditions and assembling complete bytes, clock generation when protocols require the host to provide timing signals, and edge detection with precision that software interrupt handlers can't achieve.

Rather than studying simplified examples, examine the real implementations to understand production-quality PIO programming. The [AT/PS2 PIO program](../../src/protocols/at-ps2/interface.pio) demonstrates bidirectional communication with clock synchronization. The [XT PIO program](../../src/protocols/xt/keyboard_interface.pio) shows simple unidirectional receive with minimal state. The [Amiga PIO program](../../src/protocols/amiga/keyboard_interface.pio) implements handshake timing for synchronous protocols. The [M0110 PIO program](../../src/protocols/apple-m0110/keyboard_interface.pio) handles variable-timing protocols that adapt to signal conditions.

Additional PIO resources include the [PIO helper library](../../src/common/lib/pio_helper.c) and [header](../../src/common/lib/pio_helper.h) for loading and managing PIO programs, the [Advanced Topics](../advanced/README.md) for architectural context, and the [RP2040 Datasheet](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf) PIO specification for complete instruction set reference.

---

## Tools & Enforcement

### Lint Script

The lint script (`./tools/lint.sh`) provides automated checking for architectural violations before commits reach CI. Run it before every commit—it must pass with zero errors and zero warnings, or the PR will be rejected.

The script scans for blocking operations (`sleep_ms`, `busy_wait_us`, etc.), multicore API usage (`multicore_*`, `core1_*` functions), `printf` calls in interrupt context (which cause DMA conflicts), references to `docs-internal` in public files (which would expose local-only documentation), and ring buffer safety violations (like calling `ringbuf_reset()` without proper guards).

Usage is simple: `./tools/lint.sh` checks all source files, or `./tools/lint.sh src/main.c` checks a specific file. The script returns exit code 0 on pass, 1 when errors are found, and 2 when warnings are present. CI integration ensures all pull requests pass lint checks before merge consideration.

---

### CodeRabbit

Automated pull request reviews through CodeRabbit provide AI-assisted analysis focusing on embedded systems best practices and architectural compliance. The bot reviews every PR, highlighting potential issues with blocking operations, memory safety, protocol timing, and adherence to architectural patterns.

CodeRabbit configuration defines path-specific instructions (different rules for protocols vs keyboards vs common libraries), custom pre-merge checks (must pass lint, must compile all configurations, must respect memory limits), and knowledge base integration ensuring reviews align with project architecture.

**Custom Checks:**
1. No blocking operations
2. No multicore usage
3. IRQ safety
4. RAM execution
5. Protocol timing compliance

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

**Documentation structure**: [Project README](../../README.md), [Getting Started guides](../getting-started/README.md), [Protocol docs](../protocols/README.md), [Features docs](../features/README.md)

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


Custom checks ensure architectural compliance: no blocking operations anywhere in the code, no multicore usage attempts, interrupt safety for all shared data structures, RAM execution requirements met, and protocol timing specifications followed precisely. CodeRabbit combines automated analysis with project-specific knowledge to catch issues that generic linters miss.

---

## Documentation

### Documentation Structure

The project maintains separate public and private documentation with clear boundaries. Public documentation (everything in `docs/`, `README.md` files, and source code comments) gets committed to version control and appears in the repository. This includes user guides, protocol specifications, architecture documentation, and development guides—anything that helps users or contributors understand and work with the project.

Private documentation (`docs-internal/`) stays local and never gets committed. This directory holds development notes, investigation findings, performance analysis, architecture decision records, and other working documents that inform development but don't need public visibility. Git hooks and lint checks actively prevent committing these files, and referencing them in public documentation triggers build failures.

The separation serves an important purpose: docs-internal provides a space for thorough investigation and analysis without worrying about polish or completeness, while public docs maintain high standards for accuracy and clarity. You can freely use docs-internal as a reference source during development—it often contains detailed findings and historical context that inform decisions. Just extract relevant information into properly formatted public documentation rather than linking to the private files.

Documentation resources include the project structure documented across multiple guides, the main [README](../../README.md) providing project overview, and comprehensive guides in [Getting Started](../getting-started/README.md), [Protocols](../protocols/README.md), and [Features](../features/README.md) directories.

---

## Git Workflow

### Branch Naming

Branch names follow conventional commit prefixes to indicate the type of work: `feat/add-new-protocol` for new features, `fix/at-ps2-timing` for bug fixes, `docs/protocol-guide` for documentation updates, `refactor/simplify-ringbuf` for code restructuring, and `test/add-unit-tests` for testing additions. This convention makes the branch purpose immediately clear in the branch list and Pull Request interface.

---

### Commit Messages

Commit messages follow the conventional commits format to maintain a clear, parseable history. The format includes a type prefix (`feat`, `fix`, `docs`, `refactor`, `test`, or `chore`), a colon, and a short description that completes the sentence "This commit will...". The optional body provides additional context about what changed and why. The optional footer references related issues or indicates breaking changes.

Good commit messages explain both what changed and why the change was necessary. "feat: Add support for Amiga protocol" tells you what, while the body explains "Implements Amiga keyboard protocol with handshake timing and Caps Lock synchronization via pulse." Reference related issues in the footer using "Closes #42" for features or "Fixes #38" for bug fixes—this automatically links commits to issues and closes them when merged.

For bug fixes, explain the incorrect behavior and what caused it, not just what code changed: "Device was not responding correctly to LED commands. Fixed timing to match specification (10-16.7 kHz clock). Fixes #38" tells the complete story.

---

### Pull Request Template

The pull request template (`.github/PULL_REQUEST_TEMPLATE.md`) provides structure for describing changes comprehensively. Sections include the change description explaining what you've modified and why it matters, an architecture checklist confirming you've read the code standards and followed all critical rules, testing performed detailing how you've verified the changes work correctly, breaking changes noting any incompatibilities with previous versions, and documentation updated confirming you've revised relevant guides.

Requirements for all PRs include lint script passing with zero errors and warnings, build tests passing for at least one configuration (preferably multiple if your changes affect protocols), hardware testing with actual keyboards when changes affect protocol or keyboard support, and documentation updated to reflect any new behavior or configuration options.

---

## Related Documentation

**Architecture:**
- [Advanced Topics](../advanced/README.md) - Complete system architecture, performance analysis, PIO programming, critical design principles
- [Code Standards](code-standards.md) - Authoritative architectural constraints and rules

**Implementation:**
- [Protocol Implementation](../protocols/README.md) - AT/PS2, XT, Amiga, M0110 protocol specifications
- [Keyboard Configuration](../keyboards/README.md) - Supported keyboards and configuration format
- [Features](../features/README.md) - Command Mode, Config Storage, LED Support, Logging, Mouse Support, USB HID

**Project:**
- [Main README](../../README.md) - Project overview and quick start
- [License](../../LICENSE) - MIT License terms

---

## Related Documentation

- [Code Standards](code-standards.md) - Coding conventions and testing
- [Contributing Guide](contributing.md) - Pull request process and commit format
- [Adding Keyboards](adding-keyboards.md) - Step-by-step keyboard support guide
- [Custom Keymaps](custom-keymaps.md) - Keymap modification guide
- [Advanced Topics](../advanced/README.md) - Architecture and performance
- [Protocols Overview](../protocols/README.md) - Protocol implementations

---

**Questions or stuck on something?**  
Pop into [GitHub Discussions](https://github.com/PaulW/rp2040-keyboard-converter/discussions) or [report a bug](https://github.com/PaulW/rp2040-keyboard-converter/issues) if you've found an issue.

