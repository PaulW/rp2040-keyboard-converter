# Advanced Topics

Advanced documentation for developers and power users who want to understand the converter's internals, optimise performance, or contribute to the project.

---

## Documentation Structure

The advanced topics are organised into specialised guides, each focusing on a specific aspect of the converter:

### [System Architecture](architecture.md)

Comprehensive overview of how the converter works internally. If you want to understand data flow, execution contexts, or the critical design principles, start here.

**Covers:**
- The big picture: how a keypress becomes a USB HID report
- Main components: PIO state machines, interrupt handlers, ring buffer, scancode processor, keymap translation, HID interface
- Memory layout and execution contexts
- Critical design principles: single-core architecture, non-blocking operations, SRAM execution, ring buffer safety
- Thread safety patterns and synchronisation

**Read this if you're:**
- Adding a new keyboard protocol
- Debugging timing issues
- Understanding why certain architecture decisions were made
- Contributing code changes

### [Performance Characteristics](performance.md)

Analysis of the converter's performance based on RP2040 specifications and design characteristics. No unverified measurements—only facts from source code and hardware documentation.

**Covers:**
- Processing pipeline stages and timing
- CPU and PIO clock rates
- USB polling intervals and throughput limits
- Resource utilisation (memory, CPU)
- Protocol timing requirements
- Latency analysis and deterministic behaviour
- Benchmarking methodology

**Read this if you're:**
- Optimsing performance
- Understanding latency sources
- Troubleshooting throughput issues
- Comparing converter performance to specifications

### [Build System](build-system.md)

Complete guide to the Docker-based build system, CMake configuration, and firmware compilation process.

**Covers:**
- Docker environment setup
- Configuration files (keyboard.config)
- CMake structure and dependency tracking
- PIO program compilation
- Memory management and build optimisation
- Build configurations (keyboard-only, keyboard+mouse)
- CI/CD pipeline
- Troubleshooting build errors

**Read this if you're:**
- Building custom firmware
- Adding new keyboard configurations
- Debugging build errors
- Understanding memory limits
- Contributing to the build system

### [Testing and Validation](testing.md)

Hardware testing procedures, code quality enforcement, and validation techniques for ensuring reliable operation.

**Covers:**
- Essential test equipment (logic analyzer, UART adapter, multimeter, oscilloscope)
- Testing scenarios (basic functionality, fast typing, Command Mode, LED indicators, error recovery)
- Code quality tools (lint script, static analysis)
- Continuous integration checks
- Performance testing methods
- Debugging techniques

**Read this if you're:**
- Verifying converter functionality
- Testing new keyboards or protocols
- Debugging hardware issues
- Contributing code changes
- Setting up test equipment

---

## Quick Reference

### Common Troubleshooting

**Keys not registering?**
- Check ring buffer status in UART logs (buffer full indicates USB saturation)
- Verify protocol initialisation completed (look for `!INIT!` markers)
- Confirm keymap matches keyboard's scancode set

**Build failures?**
- Verify `KEYBOARD` environment variable matches directory structure
- Check `keyboard.config` exists and has valid `PROTOCOL` value
- Ensure Docker image is up-to-date: `docker compose build`

**High latency or missed keys?**
- Search code for blocking operations (`sleep_ms`, `busy_wait_us`)
- Run `./tools/lint.sh` to detect architecture violations
- Check UART logs for protocol timing errors

**Protocol errors on power-up?**
- Normal for some keyboards—they send garbage during initialisation
- Converter should auto-recover within 1-2 seconds
- Persistent errors indicate timing or signal integrity issues

---

## Architecture at a Glance

The converter uses a **single-core, non-blocking architecture** running entirely from SRAM:

**Data flow:** Keyboard → PIO Hardware → IRQ Handler → Ring Buffer → Main Loop (Scancode Processor → Keymap → HID Interface) → TinyUSB → USB Host

**Key characteristics:**
- **PIO state machines** handle protocol timing in hardware (zero CPU overhead)
- **32-byte ring buffer** bridges interrupt context and main loop (lock-free FIFO)
- **Non-blocking main loop** uses time-based state machines (no sleep calls)
- **SRAM execution** ensures deterministic timing (no flash cache misses)
- **USB polling** at 8ms intervals (125 reports/second maximum)

See [Architecture](architecture.md) for complete details.

---

## Design Principles

Four non-negotiable principles ensure reliability:

1. **Single-Core Only** - Core 1 disabled, eliminates multicore synchronisation complexity
2. **Non-Blocking Operations** - No `sleep_ms()`, `busy_wait_us()`, or blocking loops
3. **SRAM Execution** - Code runs from RAM for deterministic timing
4. **Ring Buffer Safety** - Lock-free single-producer/single-consumer design

Violations of these principles will cause `./tools/lint.sh` to fail. See [Architecture - Critical Design Principles](architecture.md#critical-design-principles) for rationale.

---

## Source Code Organisation

**Core libraries:** [`src/common/lib/`](../../src/common/lib/)
- `ringbuf.c/h` - Lock-free FIFO queue
- `hid_interface.c/h` - USB HID report assembly
- `pio_helper.c/h` - PIO program loading and management
- `command_mode.c/h` - Firmware management interface
- `usb_descriptors.c` - USB device descriptors

**Protocol handlers:** [`src/protocols/`](../../src/protocols/)
- `at-ps2/` - AT/PS2 protocol (bidirectional, LED support)
- `xt/` - XT protocol (unidirectional, simpler timing)
- `amiga/` - Amiga protocol (synchronous handshake)
- `apple-m0110/` - Apple M0110 protocol (variable timing)

**Keyboard configurations:** [`src/keyboards/`](../../src/keyboards/)
- `<brand>/<model>/keyboard.config` - Build configuration
- `<brand>/<model>/keyboard.c` - Keymap definitions
- `<brand>/<model>/keyboard.h` - Layout-specific overrides

**Scancode processors:** [`src/scancodes/`](../../src/scancodes/)
- `set123/` - Universal Set 1/2/3 processor
- Protocol-specific processors for other scancode sets

---

## Development Workflow

1. **Read the documentation** - Start with [Architecture](architecture.md) and [Code Standards](../development/code-standards.md)
2. **Set up environment** - Install Docker, clone repository with submodules
3. **Build firmware** - `docker compose run --rm -e KEYBOARD="..." builder`
4. **Test on hardware** - Flash UF2, connect UART adapter, verify functionality
5. **Run lint checks** - `./tools/lint.sh` must pass before committing
6. **Submit changes** - Follow [Contributing Guide](../development/contributing.md)

See [Testing](testing.md) for comprehensive testing procedures.

---

## Related Documentation

**Getting Started:**
- [Hardware Setup](../getting-started/hardware-setup.md) - Physical connections and wiring
- [Building Firmware](../getting-started/building-firmware.md) - Step-by-step build guide
- [Flashing Firmware](../getting-started/flashing-firmware.md) - Installing firmware on RP2040

**Features:**
- [Command Mode](../features/command-mode.md) - Bootloader, log levels, factory reset
- [Configuration Storage](../features/config-storage.md) - Persistent settings
- [LED Support](../features/led-support.md) - Keyboard indicator lights
- [Logging](../features/logging.md) - Debug output configuration
- [Mouse Support](../features/mouse-support.md) - AT/PS2 mouse protocol
- [USB HID](../features/usb-hid.md) - USB interface details

**Development:**
- [Contributing](../development/contributing.md) - Pull request process and commit format
- [Code Standards](../development/code-standards.md) - Coding conventions and patterns
- [Adding Keyboards](../development/adding-keyboards.md) - Creating new keyboard configurations
- [Custom Keymaps](../development/custom-keymaps.md) - Remapping keys

**Protocols and Hardware:**
- [Protocols Overview](../protocols/README.md) - Protocol specifications and timing diagrams
- [Keyboards Overview](../keyboards/README.md) - Supported keyboards and configurations
- [Scancode Sets](../scancodes/README.md) - Scancode decoding reference

---

**Questions or stuck on something?**  
Pop into [GitHub Discussions](https://github.com/PaulW/rp2040-keyboard-converter/discussions) or [report a bug](https://github.com/PaulW/rp2040-keyboard-converter/issues) if you've found an issue.

