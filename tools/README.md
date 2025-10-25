# Development Tools

This directory contains development and maintenance tools for the RP2040 Keyboard Converter project.

## lint.sh

Architectural lint script that enforces critical rules from `.github/copilot-instructions.md`.

### Usage

```bash
# Run locally (warnings don't fail)
./tools/lint.sh

# Run in strict mode (warnings cause failure, used in CI)
./tools/lint.sh --strict
```

### Checks Performed

1. **Blocking Operations** - Detects `sleep_ms()`, `sleep_us()`, `busy_wait_us()`, `busy_wait_ms()`
   - ❌ **Fails**: Any blocking call found in src/
   - 💡 **Fix**: Use time-based state machines with `to_ms_since_boot(get_absolute_time())`
   - 💡 **Suppress error**: Add `// LINT:ALLOW blocking` comment for debug-only code with IRQ protection

2. **Multicore API Usage** - Detects `multicore_launch_core1`, `multicore_fifo_*`
   - ❌ **Fails**: Any multicore API usage
   - 💡 **Fix**: This project is single-core only (Core 0) by design

3. **printf in IRQ Context** - Heuristic check for `printf` in `__isr` functions
   - ⚠️ **Warns**: Possible printf/fprintf/sprintf in interrupt handlers
   - 💡 **Fix**: Use `LOG_*` macros (allowed - designed for IRQ use) or defer logging to main loop

4. **ringbuf_reset() Usage** - Detects calls to `ringbuf_reset()`
   - ⚠️ **Warns**: Must only be called with IRQs disabled
   - 💡 **Fix**: Only use during initialization or state machine resets
   - 💡 **Suppress warning**: Add `// LINT:ALLOW ringbuf_reset` comment on same line

5. **docs-internal/ in Repository** - Checks for committed internal docs
   - ❌ **Fails**: Any docs-internal/ files tracked by git
   - 💡 **Fix**: These are local-only, never commit them

6. **copy_to_ram Configuration** - Verifies code runs from RAM
   - ⚠️ **Warns**: `pico_set_binary_type(copy_to_ram)` not found
   - 💡 **Fix**: Add to CMakeLists.txt for timing-critical code
   - 🔧 **Runtime check**: Debug builds include `ram_check_verify()` - panics if executing from Flash

7. **IRQ-Shared Variables** - Reminder for manual review
   - ⚠️ **Manual check**: Volatile keyword and memory barriers
   - 💡 **Fix**: Use `volatile` + `__dmb()` for IRQ/main shared data

### Exit Codes

- **0**: All checks passed (or warnings only in non-strict mode)
- **1**: Errors found (or warnings in strict mode)

### Integration

- **Pre-commit**: Recommended to run before committing
- **CI**: Automatically runs in strict mode on all PRs
- **Local development**: Run periodically during development

### Example Output

```
========================================
RP2040 Architecture Lint Checks
========================================

[1/7] Checking for blocking operations...
✓ No blocking operations found

[2/7] Checking for multicore API usage...
✓ No multicore API usage found

...

========================================
Lint Summary
========================================
✓ All checks passed!
```

### Common Issues and Fixes

#### Blocking Operations Detected

**Problem**: Code uses `sleep_ms(100)` for timing

**Fix**: Use time-based state machine
```c
// Before (blocking)
sleep_ms(100);
do_something();

// After (non-blocking)
static uint32_t start_time = 0;
static bool waiting = false;

if (!waiting) {
    start_time = to_ms_since_boot(get_absolute_time());
    waiting = true;
    return;
}

if (to_ms_since_boot(get_absolute_time()) - start_time >= 100) {
    do_something();
    waiting = false;
}
```

**Exception**: Debug-only code with explicit justification
```c
// Acceptable use case: UART debug logging with IRQ protection
if (in_irq()) {
    return !queue_full();  // Never block in IRQ
}
sleep_us(delay_us);  // LINT:ALLOW blocking - UART debug logging only, IRQ-protected, yields CPU for PIO/interrupts
```

#### printf in IRQ Context

**Problem**: `printf()` called in interrupt handler

**Fix**: Use LOG macros or defer
```c
// Before (unsafe)
void __isr keyboard_irq_handler(void) {
    printf("Key pressed\n");  // DMA conflict!
}

// After (safe)
void __isr keyboard_irq_handler(void) {
    LOG_DEBUG("Key pressed");  // IRQ-safe
}

// Or defer to main loop
static volatile bool key_pressed = false;

void __isr keyboard_irq_handler(void) {
    key_pressed = true;
    __dmb();
}

void main_loop(void) {
    if (key_pressed) {
        __dmb();
        printf("Key pressed\n");
        key_pressed = false;
    }
}
```

#### Suppressing ringbuf_reset Warnings

**Problem**: Legitimate `ringbuf_reset()` call during initialization triggers warning

**Fix**: Add annotation comment
```c
// Legitimate use - IRQs not yet enabled during init
void keyboard_interface_init(void) {
    // ... setup code ...
    ringbuf_reset();  // LINT:ALLOW ringbuf_reset
}
```

The lint script will skip warnings for lines with the `// LINT:ALLOW ringbuf_reset` annotation.

### Runtime RAM Execution Check

In debug builds (`cmake -DCMAKE_BUILD_TYPE=Debug`), the firmware includes a runtime check that verifies code is executing from SRAM rather than Flash:

```c
#include "ram_check.h"

int main(void) {
    ram_check_verify();  // Panics if executing from Flash (0x10000000)
                          // Passes if executing from SRAM (0x20000000)
    // ... rest of initialization
}
```

This provides defense-in-depth verification beyond the lint script's text search for `pico_set_binary_type(copy_to_ram)`.

## Testing the Tools

See comprehensive testing documentation:
- **[TESTING.md](TESTING.md)** - Complete testing guide with all scenarios
- **[QUICKTEST.md](QUICKTEST.md)** - Quick reference commands

Run automated tests:
```bash
./tools/test-enforcement.sh
```

Quick verification:
```bash
./tools/lint.sh && echo "✅ Ready to commit"
```

## Future Tools

Additional tools planned:
- **Memory profiler** - Detailed RAM/Flash usage analysis
- **Latency analyzer** - Automated pipeline timing measurements
- **Config validator** - Validate keyboard.config files
- **Hardware test harness** - Automated hardware-in-loop tests

## Contributing

When adding new tools:
1. Add executable shell script or Python script
2. Make it executable: `chmod +x tools/new-tool.sh`
3. Document usage in this README
4. Consider integrating with CI pipeline if appropriate
5. Follow existing tool patterns (color output, exit codes, etc.)
