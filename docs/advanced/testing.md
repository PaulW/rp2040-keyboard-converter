# Testing and Validation

Testing the converter properly means more than just checking that keys produce the right characters. You want to verify it handles real keyboards reliably, catches issues early, and maintains code quality throughout.

---

## Hardware Testing

Verifying the converter with actual keyboards goes beyond confirming correct typing. Software testing alone won't catch marginal signal integrity issues that work with one keyboard but fail with another, timing violations that only show up under specific conditions, electrical problems like insufficient voltage levels, or protocol edge cases where real keyboards behave differently than the specifications suggest.

Hardware testing catches these problems during development, before they appear as intermittent failures in production use.

### Why Hardware Testing Matters

The timing-sensitive nature of keyboard protocols means issues often hide until you test with actual hardware. A keyboard that works perfectly on the bench might fail when connected through a longer cable, or a protocol implementation that passes all software tests might miss edge cases that only specific keyboard models trigger.

---

## Essential Test Equipment

### Logic Analyzer

A logic analyzer captures the actual clock and data signals between keyboard and converter, showing precisely what's happening on the wire.

**Recommended models:**
- **Saleae Logic 8** - Professional-grade, excellent software, 8 channels
- **Open-source compatible** - PulseView + compatible hardware (Sigrok)
- **Budget options** - 8-channel USB logic analyzers (£15-30)

**What to verify:**
- Clock frequencies match protocol specifications (10-16.7 kHz for AT/PS2, ~10 kHz for XT)
- Data transitions occur at correct clock edges (rising vs falling)
- Setup and hold times meet protocol requirements
- Converter's signal timing meets keyboard requirements (for bidirectional protocols)

**Protocol decoders:** Logic analyzer software commonly includes AT/PS2 and UART decoders. These interpret captured waveforms automatically, showing scancodes and protocol errors without manual bit counting.

### USB-UART Adapter

A USB-UART adapter connects to the RP2040's UART pins (GPIO 0/1) to display diagnostic logging output.

**Requirements:**
- **3.3V logic levels** (5V adapters may damage RP2040)
- **TX, RX, GND connections** (no need for power pins)
- **115200 baud rate** (default UART configuration)

**What you'll see:**
- Protocol state transitions (initialization, LED commands, etc.)
- Error conditions (`[ERR]` messages for protocol violations)
- Received scancodes in hexadecimal
- Timing measurements and performance data
- Command Mode activation and menu responses

Enable verbose logging during testing to maximize visibility. See the [Logging guide](../features/logging.md) for configuration options.

### Multimeter

A basic multimeter verifies voltage levels and continuity.

**Signal level verification:**
- Measure keyboard's CLK and DATA lines: Should be 5V (or 3.3V for some modern keyboards)
- Measure level shifter output: Should be 3.3V when interfacing with RP2040
- Verify clean high and low levels: No intermediate voltages indicating poor connections

**Connection testing:**
- Check continuity between keyboard connector and level shifter
- Verify level shifter connections to RP2040 GPIO pins
- Test ground connections (keyboard GND to level shifter to RP2040)

### Oscilloscope (Optional)

An oscilloscope reveals signal quality issues that logic analyzers might miss.

**What to observe:**
- **Rise and fall times:** Should be clean transitions without excessive ringing
- **Signal overshoot:** Excessive overshoot indicates impedance mismatches
- **Noise:** Random voltage fluctuations on clock or data lines
- **Voltage sag:** Power supply issues or insufficient current capacity

Oscilloscopes are optional for basic testing but valuable when debugging timing issues or signal integrity problems.

---

## Testing Scenarios

### Basic Functionality Test

Start with the fundamentals—test that all keys work correctly under normal use.

Connect your keyboard to the converter, plug the converter into your computer, and verify it enumerates as a USB HID device. Type through the alphabet (a-z), numbers (0-9), and punctuation to make sure the basics work. Test all the modifier keys—Shift, Ctrl, Alt, and the GUI key (Windows/Command depending on your OS). Go through the function keys, both F1-F12 and F13-F24 if your keyboard has them. Check the navigation cluster: arrows, Home, End, PgUp, PgDown, Insert, and Delete. Test the numpad with Num Lock both on and off. Finally, verify special keys like Escape, Tab, Caps Lock, Backspace, and Enter all behave correctly.

Every key should produce the correct character or action. No missed keypresses, no duplicate characters, no wrong characters appearing.

### Fast Typing Test

Stress-test the pipeline with rapid typing to see if the buffering and throughput hold up.

Type rapidly at 30 or more characters per second. Try burst typing—hold a key briefly, release it, then immediately repeat with different keys. Test simultaneous keypresses by holding multiple keys down at once. Watch for any dropped keypresses during fast input, and check the UART output for buffer full warnings that would indicate the system's struggling to keep pace.

All keypresses should register correctly. You shouldn't see any `[ERR]` messages about ring buffer overflow in the UART logs, and the converter should maintain pace with human typing speeds.

**Why 30 CPS?** This represents fast typing that tests the system under load without being unrealistic. Professional typists rarely exceed 20 CPS sustained, but burst typing can briefly reach 30+ CPS, making it a useful test threshold.

### Command Mode Test

Verify Command Mode activates and responds correctly to commands.

Hold both Shift keys simultaneously for 3 seconds and check for activation feedback (you'll see it in the UART logging, or via LED indicator if your keyboard supports that). Once activated, press the command keys to test each function: **B** enters bootloader mode (the device should disconnect and present the RP2040 drive for firmware updates), **D** cycles through log levels, **F** does a factory reset (clearing configuration and returning to defaults), and **L** adjusts LED brightness if your protocol supports it. Make sure each command executes correctly, then test exiting Command Mode by pressing Escape or just waiting for the timeout.

Command Mode should activate after the 3-second hold, all commands should execute as documented, and normal typing should resume properly after you exit.

**Note:** If your keyboard has a non-standard modifier layout (like the Apple M0110A with its single Shift key), verify the override keys work correctly. See [`keyboard.h`](../../src/keyboards/apple/m0110a/keyboard.h) for examples of how to configure alternative activation keys.

### LED Indicator Test

Test Caps Lock, Num Lock, and Scroll Lock indicators (if protocol supports LEDs).

**Test procedure:**
1. Press Caps Lock - LED should toggle on/off
2. Press Num Lock - LED should toggle on/off
3. Press Scroll Lock - LED should toggle on/off
4. Verify LED state persists after key release
5. Check UART output for LED command sequences

**Expected results:** LEDs update immediately when lock keys are pressed. No delay or flickering. LED state matches lock key state.

**Protocol support:**
- **AT/PS2:** Full LED support (Caps, Num, Scroll)
- **XT:** No LED support (unidirectional protocol)
- **Amiga:** Caps Lock only (pulse-duration based)
- **M0110:** No built-in LED support

### Protocol Error Recovery Test

Verify the converter recovers gracefully from protocol errors.

**Test procedure:**
1. Start with converter operating normally
2. Power-cycle keyboard whilst converter is running:
   - Unplug keyboard connector
   - Wait 2-3 seconds
   - Reconnect keyboard
3. Verify converter detects disconnection (UART shows protocol errors)
4. Verify converter reinitializes protocol automatically
5. Test keyboard functions normally after reconnection

**Expected results:** Converter detects keyboard disconnection, logs `[ERR]` messages, attempts reinitialization, resumes normal operation when keyboard returns. No manual reset required.

**Common scenarios:**
- Keyboard unplugged during use
- Keyboard power-cycled (USB hub reset)
- Protocol timing violations during initialization
- Transient electrical issues causing garbage scancodes

### Multi-OS Compatibility Test

Test the converter with multiple host operating systems to ensure HID compatibility.

**Test procedure:**
1. Test on **Windows** (7, 10, 11) - Verify HID enumeration and keystrokes
2. Test on **macOS** (10.15+) - Check modifier key mapping and special keys
3. Test on **Linux** (Ubuntu, Fedora, etc.) - Verify keyboard works in console and X11
4. Test special scenarios:
   - BIOS/UEFI firmware (boot protocol fallback)
   - USB hubs (some hubs have higher polling latency)
   - KVM switches (some pause USB polling during switching)

**Expected results:** Converter works identically across all OS platforms. Boot protocol ensures compatibility even with BIOS/firmware.

### Extended Key Test

Test keys that use multi-byte scancode sequences.

**Test procedure:**
1. **Extended keys (E0 prefix):**
   - Right Control, Right Alt, Right GUI
   - Navigation cluster (arrows, Home, End, PgUp, PgDown, Insert, Delete)
   - Numpad Enter and Numpad Slash
2. **Pause/Break key:** Press and verify correct handling (11-byte sequence in Set 2)
3. **Print Screen:** Press with and without modifiers
4. Check UART output to verify scancode sequences are decoded correctly

**Expected results:** All extended keys register correctly. Multi-byte sequences are assembled properly. No spurious keypresses from sequence fragments.

---

## Code Quality Testing

### Lint Script

The [`tools/lint.sh`](../../tools/lint.sh) script scans the entire codebase for critical rule violations.

**What it detects:**
1. **Blocking operations:** `sleep_ms()`, `sleep_us()`, `busy_wait_ms()`, `busy_wait_us()`
2. **Multicore APIs:** `multicore_*`, `core1_*` functions (forbidden)
3. **printf in IRQ:** `printf()` calls in files containing `__isr` (use `LOG_*` macros)
4. **Ring buffer reset annotation:** `ringbuf_reset()` calls without required `// LINT:ALLOW ringbuf_reset` annotation (linter verifies annotation presence, not runtime IRQ protection)
5. **docs-internal exposure:** Ensures private docs never leak into repository
6. **Flash execution:** Missing `copy_to_ram` configuration in CMakeLists.txt
7. **IRQ variable safety:** Missing `volatile` or `__dmb()` barriers for IRQ-shared variables
8. **Tab characters:** Enforces spaces-only indentation (4 spaces)
9. **Header guards:** Missing `#ifndef`/`#define` guards in .h files
10. **File headers:** Missing GPL or MIT license headers
11. **Naming conventions:** Detects camelCase (expects snake_case)
12. **Include order:** Validates include directive organization
13. **IRQ handler attributes:** Missing `__isr` attribute on interrupt handlers
14. **Compile-time validation:** Advisory check for `_Static_assert` and `#error`
15. **Protocol ring buffer setup:** Missing `ringbuf_reset()` in keyboard protocol initialization
16. **Protocol PIO IRQ dispatcher:** Missing centralized `pio_irq_dispatcher_init()` or deprecated direct `irq_set_priority()` usage
17. **Indentation consistency:** Enforces 4-space indentation, detects 2-space violations

**Run before every commit:**
```bash
./tools/lint.sh
```

**Expected result:** 0 errors, 0 warnings. Script exits with code 0.

**CI enforcement:** Pull requests automatically run lint checks in strict mode. Violations block merging.

**Detailed documentation:** See [`tools/README.md`](../../tools/README.md) for comprehensive check descriptions and fix examples.

### Static Analysis

The converter doesn't currently use additional static analysis tools (like Clang-Tidy or Cppcheck), but these could be added for deeper analysis:

**Potential additions:**
- **Clang-Tidy:** Catches modern C++ style issues and potential bugs
- **Cppcheck:** Lightweight static analyzer for C code
- **scan-build:** Clang Static Analyzer for deeper bug detection

These tools can detect issues like:
- Use-after-free bugs
- Memory leaks (though converter uses minimal dynamic allocation)
- Null pointer dereferences
- Integer overflow/underflow
- Dead code and unreachable branches

### Memory Safety

The converter avoids dynamic allocation where possible, preferring stack-based allocation:

**Memory safety patterns:**
- **Stack allocation:** Local variables, function parameters, small buffers
- **Static allocation:** Protocol state, ring buffer, HID reports
- **No malloc/free:** Eliminates heap fragmentation and memory leaks
- **Bounds checking:** Array accesses validated before use
- **Const correctness:** Read-only data marked const

**Testing approach:**
- Build with address sanitizer (requires native build, not ARM cross-compile)
- Review .map file for unexpected heap usage
- Check stack usage doesn't exceed safe limits
- Verify no global variable overflow

---

## Continuous Integration

The CI pipeline (see [`.github/workflows/ci.yml`](../../.github/workflows/ci.yml)) builds every configuration on every commit:

**Build matrix:**
- All keyboards in [`src/keyboards/`](../../src/keyboards/)
- With and without mouse support
- Debug and release builds

**Automated checks:**
1. **Lint enforcement:** `tools/lint.sh` must pass
2. **Build verification:** All configurations must compile
3. **Memory limits:** Flash < 230KB, RAM < 150KB
4. **PIO compilation:** All .pio files must compile without errors
5. **Link verification:** No undefined references or multiple definitions

**When CI fails:**
- Check build log for specific error
- Reproduce locally: `docker compose run --rm -e KEYBOARD="..." builder`
- Fix issues and push updated code
- CI reruns automatically on push

---

## Regression Testing

Whilst the converter doesn't have automated unit tests (integration testing on hardware is more valuable for this project), manual regression testing catches issues:

**Regression test checklist:**
1. **Existing keyboards still work** - Test representative keyboards from each protocol
2. **Command Mode still functions** - Verify all commands work
3. **LEDs still update** - Test lock key indicators
4. **Protocol error recovery** - Power-cycle test
5. **Multi-byte sequences** - Test extended keys and Pause
6. **Fast typing handling** - Burst typing test
7. **Build system works** - All configurations compile
8. **Memory usage acceptable** - No significant size increases

Run this checklist before releasing new firmware or merging significant changes.

---

## Test Documentation

When adding new keyboards or protocols, document your testing:

**What to document:**
- Which keyboard models were tested
- Any quirks or edge cases discovered
- Protocol timing characteristics observed
- Signal quality issues encountered
- Special configuration required

**Example test documentation:**
```markdown
# Testing Notes - IBM Model M Enhanced

**Hardware:** IBM Model M Enhanced (1391401), manufactured 1994
**Protocol:** AT/PS2, Scancode Set 2
**Connection:** 5-pin DIN → level shifter → RP2040 GPIO 2/3

**Testing performed:**
- Basic functionality: ✓ All keys work correctly
- Fast typing: ✓ No dropped keys at 30+ CPS
- LED indicators: ✓ Caps/Num/Scroll Lock all function
- Protocol timing: ✓ Clock ~11 kHz, stable

**Quirks discovered:**
- Sends garbage scancode 0xAA on power-up (handled by protocol init)
- Caps Lock LED slightly dimmer than Num Lock (cosmetic, works fine)

**Signal quality:**
- Clean 5V logic levels on CLK/DATA
- No noise or ringing observed
- Rise/fall times < 100ns
```

This documentation helps future contributors understand what's been tested and what to watch for.

---

## Performance Testing

Whilst the converter hasn't been extensively benchmarked with timing equipment, you can measure key performance characteristics:

### Latency Measurement

**Method 1: Logic analyzer**
1. Connect logic analyzer to keyboard CLK/DATA and USB D+/D-
2. Press a key and capture signals
3. Measure time from keyboard clock edge to USB D+ transition
4. Repeat 10-20 times to get average and variance

**Method 2: Software timing**
1. Enable DEBUG logging level
2. Add timestamps to key pipeline stages
3. Log time at: PIO IRQ, ring buffer read, HID report ready, USB transmission
4. Capture UART output and analyze timing

**Expected latency:**
- Protocol transmission: 1-2ms (inherent to protocol)
- Processing pipeline: <100μs (PIO→IRQ→ring buffer→scancode→HID)
- USB polling wait: 0-8ms (average 4ms)
- Total: 1-10ms depending on USB timing

### Throughput Measurement

**Test maximum throughput:**
1. Write script to simulate rapid keypresses
2. Monitor UART for dropped scancodes or buffer overflow warnings
3. Measure maximum sustained keypress rate without errors

**Expected throughput:**
- Theoretical max: 125 reports/second (8ms USB polling)
- Human typing: 10-15 CPS typical, 20-30 CPS fast
- Gaming inputs: 20-30 inputs/second
- Converter headroom: 4-6x above typical human input

### Resource Monitoring

**Monitor memory usage:**
1. Check build output for Flash/RAM consumption
2. Verify usage stays well below limits (230KB Flash, 150KB RAM)
3. Review .map file for large functions or data structures

**Monitor CPU usage:**
- No direct CPU monitoring in current implementation
- PIO handles protocol timing autonomously (zero CPU during bit reception)
- Main loop operates in non-blocking architecture
- USB polling dominates timing (8ms intervals)

---

## Debugging Techniques

### UART Logging Levels

The converter provides configurable logging levels (see [Logging guide](../features/logging.md)):

- **ERROR:** Critical failures only
- **WARN:** Protocol errors, unexpected conditions
- **INFO:** Initialization messages, state changes
- **DEBUG:** Detailed scancode traces, timing information

**For testing:** Use DEBUG level to see everything. For production: INFO or WARN.

### Logic Analyzer Protocol Decoding

Logic analyzer software commonly includes protocol decoders:

1. **AT/PS2 decoder:** Shows scancodes, parity errors, timing violations
2. **UART decoder:** Displays UART output alongside protocol signals
3. **USB decoder:** Some analyzers can decode USB HID reports

**Debugging workflow:**
1. Capture signals during problematic behaviour
2. Review protocol decoder output for errors
3. Cross-reference with UART logging
4. Identify timing violations or unexpected scancodes

### GDB Debugging

The .elf file includes debug symbols for GDB debugging:

```bash
# Start OpenOCD (connects to RP2040 via SWD)
openocd -f interface/cmsis-dap.cfg -f target/rp2040.cfg

# In another terminal, start GDB
arm-none-eabi-gdb build/rp2040-converter.elf
(gdb) target remote localhost:3333
(gdb) monitor reset halt
(gdb) break main
(gdb) continue
```

**Use GDB for:**
- Setting breakpoints in protocol handlers
- Inspecting ring buffer state
- Stepping through scancode processing
- Analyzing crash dumps

---

## Test Reports

When submitting bug reports or requesting support, include:

1. **Hardware details:**
   - Keyboard make/model
   - Protocol type (AT/PS2, XT, Amiga, M0110)
   - Converter board type (RP2040 variant)

2. **Software version:**
   - Git commit hash or release version
   - Build configuration (keyboard + mouse)
   - Enabled features

3. **Test results:**
   - Which tests passed/failed
   - UART log output (if available)
   - Logic analyzer captures (if available)
   - Steps to reproduce issue

4. **Environment:**
   - Host operating system
   - USB hub/KVM switch (if used)
   - Power supply details

This information helps maintainers reproduce and diagnose issues efficiently.

---

## Related Documentation

- [Architecture](architecture.md) - Understanding the system for effective testing
- [Performance](performance.md) - Expected performance characteristics
- [Build System](build-system.md) - Building test firmware
- [Code Standards](../development/code-standards.md) - Architecture rules and patterns
- [Logging](../features/logging.md) - Configuring debug output
- [Hardware Setup](../getting-started/hardware-setup.md) - Physical connections

---

**Questions about testing or found an issue?**  
Pop into [GitHub Discussions](https://github.com/PaulW/rp2040-keyboard-converter/discussions) or [report a bug](https://github.com/PaulW/rp2040-keyboard-converter/issues).
