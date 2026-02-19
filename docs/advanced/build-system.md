# Build System

The build system transforms keyboard configuration into protocol-specific firmware through Docker and CMake. Set `KEYBOARD=modelm/enhanced` and the system locates the configuration, includes the protocol handler, compiles PIO programs, and links the keymap automatically.

---

## Overview

When you specify `KEYBOARD=modelm/enhanced` during build, the system locates your keyboard configuration, pulls in the appropriate protocol handler and scancode processor, compiles the matching PIO program, and links your keyboard-specific keymap. It's all automatic—you just need to tell it which keyboard you're building for.

The build produces three artifacts:
- **rp2040-converter.uf2** - Drag-and-drop firmware file for flashing
- **rp2040-converter.elf** - Debugger-compatible binary with symbols
- **rp2040-converter.elf.map** - Memory layout analysis file

---

## Build Process

### Docker Environment

The Docker-based build environment ensures reproducible builds by including a pinned version of **Pico SDK 2.2.0** with all necessary toolchain components. This produces bit-identical builds on macOS, Linux, or Windows without requiring manual ARM cross-compiler installation or SDK path configuration.

**Build the Docker image** (one-time setup):
```bash
docker compose build
```

**Build firmware for a specific keyboard:**
```bash
docker compose run --rm -e KEYBOARD="modelm/enhanced" builder
```

**Build with mouse support:**
```bash
docker compose run --rm -e KEYBOARD="modelm/enhanced" -e MOUSE="at-ps2" builder
```

The Docker container includes:
- ARM GCC cross-compiler toolchain
- CMake 3.13+ with Pico SDK integration
- Pico SDK 2.2.0 (pinned version via git submodule)
- Python 3 for build scripts
- All dependencies for parallel builds

Builds complete in seconds even for full rebuilds due to parallel compilation support.

---

## Configuration Files

### keyboard.config

Each keyboard has a [`keyboard.config`](../../src/keyboards/modelm/enhanced/keyboard.config) file in its directory: [`src/keyboards/`](../../src/keyboards/)`<brand>/<model>/keyboard.config`

**Format** (simple key=value pairs):
```bash
# src/keyboards/modelm/enhanced/keyboard.config
MAKE=IBM
MODEL=Model M Enhanced PC Keyboard
DESCRIPTION=IBM Personal Computer AT Enhanced Keyboard
PROTOCOL=at-ps2
CODESET=set123
```

**Configuration variables:**
- **MAKE:** Manufacturer name (appears in boot messages)
- **MODEL:** Keyboard model designation
- **DESCRIPTION:** Full description (appears in boot messages)
- **PROTOCOL:** Protocol handler to include (at-ps2, xt, amiga, apple-m0110)
- **CODESET:** Scancode processor to use (set123, set1, set2, etc.)

The build system reads these values and automatically includes the appropriate components.

---

## CMake Structure

### Top-Level CMakeLists.txt

The top-level [`CMakeLists.txt`](../../src/CMakeLists.txt) orchestrates the build:

1. **Read keyboard.config** - Parses the configuration file for the specified `KEYBOARD` environment variable
2. **Include protocol handler** - Adds the appropriate protocol implementation from [`src/protocols/`](../../src/protocols/)
3. **Include scancode processor** - Adds the scancode decoder from [`src/scancodes/`](../../src/scancodes/)
4. **Compile PIO programs** - Generates C headers from `.pio` assembly files
5. **Include keyboard keymap** - Adds the keyboard-specific layout from [`src/keyboards/`](../../src/keyboards/)
6. **Configure binary type** - Sets SRAM execution mode: `pico_set_binary_type(copy_to_ram)`
7. **Link TinyUSB** - Includes USB stack and HID interface
8. **Generate outputs** - Produces UF2, ELF, and MAP files

**Key CMake configuration (line 93):**
```cmake
pico_set_binary_type(${PROJECT_NAME} copy_to_ram)
```

This instructs the linker to copy the entire program from flash into SRAM during boot, ensuring deterministic execution timing. See [Architecture - SRAM Execution](architecture.md#sram-execution) for rationale.

### Dependency Tracking

CMake tracks all dependencies automatically:
- Changing a protocol's `.pio` file triggers PIO recompilation and relinking
- Modifying a keyboard's keymap triggers recompilation of that keyboard configuration
- Updating common libraries triggers recompilation of all files that include them
- Header file changes trigger recompilation of dependent source files

This dependency tracking ensures incremental builds work correctly—you only recompile what's changed.

---

## PIO Program Compilation

PIO assembly files (`.pio`) contain the timing logic that runs on the RP2040's PIO hardware state machines. The build system compiles these into C headers during the build process.

**PIO compilation process:**
1. **pioasm tool** reads `.pio` files from protocol directories
2. Assembles PIO instructions into binary format
3. Generates C header with program constants and helper functions
4. Header includes `pio_program_t` structure for loading into hardware

**Example:** [`src/protocols/at-ps2/interface.pio`](../../src/protocols/at-ps2/interface.pio) compiles to `interface.pio.h`, which provides:
- `interface_program` - PIO instruction array
- `interface_program_init()` - Helper to configure state machine
- `interface_wrap_target` / `interface_wrap` - Program loop bounds
- Instruction count and origin information

The build system automatically discovers `.pio` files in protocol directories and compiles them. No manual configuration needed.

---

## Memory Management

### Build Output Analysis

The linker reports memory usage during build:

```
Memory region         Used Size  Region Size  %age Used
           FLASH:      95284 B         2 MB      4.55%
             RAM:      71432 B       264 KB     26.42%
```

**Flash usage:** Program code and constant data stored persistently
**RAM usage:** Code execution space (copied from flash) + variables + stack

The converter executes from RAM (copy_to_ram), so the program consumes space in both flash (storage) and RAM (execution). This is intentional for deterministic timing.

### Memory Limits

CI enforcement in [`.github/workflows/ci.yml`](../../.github/workflows/ci.yml) fails builds exceeding:
- **Flash:** 230KB (11.5% of 2MB capacity)
- **RAM:** 150KB (56.8% of 264KB capacity)

These limits provide comfortable margins whilst preventing configurations that approach resource boundaries.

### Memory Map Analysis

The `.elf.map` file shows detailed memory layout:

```
.text           0x20000000    0x15abc  # Code section in SRAM
.rodata         0x20015abc    0x2f40   # Read-only data
.data           0x200189fc    0x1d0    # Initialised data
.bss            0x20018bcc    0x3e8    # Uninitialised data
```

Use this file to:
- Verify code executes from SRAM addresses (0x20000000-0x20041000)
- Identify large functions or data structures
- Analyze stack usage and placement
- Debug linker errors about overlapping sections

---

## Build Configurations

### Keyboard-Only Build

Standard build with keyboard support only:

```bash
docker compose run --rm -e KEYBOARD="modelm/enhanced" builder
```

Produces firmware with:
- Selected keyboard protocol (AT/PS2, XT, Amiga, or M0110)
- Selected scancode processor
- Keyboard keymap
- Command Mode, logging, configuration storage
- No mouse support

### Keyboard + Mouse Build

Build with both keyboard and AT/PS2 mouse support:

```bash
docker compose run --rm -e KEYBOARD="modelm/enhanced" -e MOUSE="at-ps2" builder
```

Adds mouse protocol handler with:
- Separate PIO state machine for mouse protocol
- Independent CLK/DATA lines (dedicated GPIO pins)
- 3-byte or 4-byte packet decoding (standard and scroll wheel)
- Mouse HID interface alongside keyboard interface

Mouse support works with **any keyboard protocol**—the mouse uses its own AT/PS2 implementation on separate hardware lines.

### Mouse-Only Build

Build with only mouse support (no keyboard):

```bash
docker compose run --rm -e MOUSE="at-ps2" builder
```

Produces minimal firmware for mouse-only operation.

---

## Build Optimization

### Compiler Flags

The build uses optimization flags appropriate for embedded systems:

**Default optimization:** `-O2` (optimize for speed with size consideration)
- Balances performance and code size
- Enables most optimizations without excessive code growth
- Suitable for production use

**Debug builds:** `-Og` (optimize for debugging)
- Preserves debug information quality
- Minimal optimizations to keep code structure recognizable
- Used for GDB debugging sessions

**Size optimization:** `-Os` (optimize for size)
- Prioritizes small code size over speed
- Useful for configurations approaching memory limits
- Not currently used—plenty of headroom with `-O2`

### Link-Time Optimization (LTO)

LTO optimizes across translation units during linking:
- Inlines functions across file boundaries
- Eliminates dead code more aggressively
- Reduces binary size by eliminating unused code

Currently **not enabled** to keep build times fast and debugging straightforward. Could be enabled if memory becomes constrained.

---

## Parallel Builds

The Docker container supports parallel compilation:

```bash
# Use all available CPU cores
docker compose run --rm -e KEYBOARD="modelm/enhanced" builder

# Limit to specific number of jobs
docker compose run --rm -e KEYBOARD="modelm/enhanced" -e CMAKE_BUILD_PARALLEL_LEVEL=4 builder
```

Parallel builds complete in seconds even for full rebuilds. Incremental builds (only changed files) complete in under a second.

---

## Build Troubleshooting

### Common Build Errors

**Error: keyboard.config not found**
```
CMake Error: keyboard.config missing for KEYBOARD=modelm/enhanced
```

**Solution:** Verify the `KEYBOARD` environment variable matches a directory under [`src/keyboards/`](../../src/keyboards/). Check spelling and path structure.

**Error: Undefined reference to protocol_*_init**
```
undefined reference to `at_ps2_keyboard_interface_init'
```

**Solution:** The `PROTOCOL` value in keyboard.config doesn't match any implemented protocol. Check [`src/protocols/`](../../src/protocols/) for available options.

**Error: PIO program too large**
```
PIO program exceeds available instruction memory
```

**Solution:** PIO blocks have 32 instructions each. Some complex protocols require optimization or splitting across multiple state machines.

**Error: Region RAM overflowed**
```
region `RAM' overflowed by XXXX bytes
```

**Solution:** Configuration exceeds 264KB SRAM. Either:
- Reduce feature set (disable mouse, logging, etc.)
- Optimize code (smaller functions, fewer globals)
- Request CI limit increase if usage is reasonable

### Build Verification

After building, verify the firmware:

1. **Check memory usage:** Look for "Memory region" output showing Flash/RAM consumption
2. **Verify UF2 exists:** `build/rp2040-converter.uf2` should be present and >100KB
3. **Check MAP file:** `build/rp2040-converter.elf.map` shows memory layout
4. **Test on hardware:** Flash to RP2040 and verify boot messages via UART

---

## CI/CD Pipeline

### Continuous Integration

The CI pipeline (see [`.github/workflows/ci.yml`](../../.github/workflows/ci.yml)) builds every configuration in the keyboard matrix on every commit:

**Build matrix:**
- All keyboards in [`src/keyboards/`](../../src/keyboards/) (8+ configurations)
- With and without mouse support
- Debug and release builds

**Automated checks:**
- Lint script must pass ([`tools/lint.sh`](../../tools/lint.sh))
- All configurations must compile successfully
- Memory limits enforced (230KB Flash, 150KB RAM)
- PIO programs must compile without errors
- No blocking operations or multicore APIs detected

Builds run on Ubuntu latest with the same Docker container used for local builds, ensuring consistency between development and CI environments.

### Memory Limit Enforcement

CI fails builds exceeding memory thresholds:

```yaml
- name: Check memory limits
  run: |
    FLASH=$(grep "FLASH:" build_output.txt | awk '{print $2}')
    RAM=$(grep "RAM:" build_output.txt | awk '{print $2}')
    [ $FLASH -lt 235520 ] || exit 1  # 230KB
    [ $RAM -lt 153600 ] || exit 1    # 150KB
```

This prevents accidental resource overruns and ensures all configurations remain within comfortable margins.

---

## Development Workflow

### Typical Development Cycle

1. **Modify source code** - Edit protocol handlers, keymaps, or core libraries
2. **Build locally** - Test compilation with Docker
3. **Flash to hardware** - Copy UF2 to RP2040 bootloader
4. **Test functionality** - Verify changes work as expected
5. **Check UART output** - Monitor debug logs for errors
6. **Run lint script** - Ensure no architecture violations
7. **Commit changes** - Push to repository
8. **CI verification** - Automated builds test all configurations

### Iterative Testing

For rapid iteration during development:

```bash
# Build, flash, test loop
while true; do
    docker compose run --rm -e KEYBOARD="modelm/enhanced" builder
    cp build/rp2040-converter.uf2 /Volumes/RPI-RP2/
    # Wait for device to reboot and test
    sleep 5
done
```

The RP2040 bootloader accepts UF2 files via drag-and-drop or command-line copy. Device reboots automatically after flashing.

---

## Related Documentation

- [Architecture](architecture.md) - SRAM execution rationale and memory layout
- [Performance](performance.md) - Resource utilization and optimization
- [Hardware Setup](../getting-started/hardware-setup.md) - Physical connections
- [Building Firmware](../getting-started/building-firmware.md) - Step-by-step build guide
- [Flashing Firmware](../getting-started/flashing-firmware.md) - UF2 installation
- [Adding Keyboards](../development/adding-keyboards.md) - Creating new configurations
- [Code Standards](../development/code-standards.md) - Build system conventions

---

**Questions or build issues?**  
Pop into [GitHub Discussions](https://github.com/PaulW/rp2040-keyboard-converter/discussions) or [report a bug](https://github.com/PaulW/rp2040-keyboard-converter/issues).
