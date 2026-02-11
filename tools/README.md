# Development Tools

The tools in this directory handle enforcement and testing for the RP2040 Keyboard Converter project. These catch architectural violations before they make it into the codebase—things like blocking operations or multicore API usage that would break the converter's timing requirements.

## lint.sh

This script checks source code against the architectural rules defined in `.github/copilot-instructions.md`.

### Usage

```bash
# Run locally (warnings don't fail)
./tools/lint.sh

# Run in strict mode (warnings cause failure, used in CI)
./tools/lint.sh --strict
```

### What It Checks

The script runs through fourteen different checks, each targeting a specific architectural rule:

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

7. **IRQ-Shared Variables** - Comprehensive volatile and memory barrier analysis
   - ⚠️ **Warns**: Static variables accessed in `__isr` functions without `volatile` keyword
   - ⚠️ **Warns**: Volatile reads/writes without surrounding `__dmb()` memory barriers
   - 💡 **Fix**: Use `volatile` + `__dmb()` for IRQ/main shared data
   - 💡 **Suppress warning**: Add `// LINT:ALLOW non-volatile` or `// LINT:ALLOW barrier` with justification

8. **Tab Characters** - Enforces spaces-only indentation
   - ❌ **Fails**: Tab characters found in source files
   - 💡 **Fix**: Use 4 spaces, never tabs (run `expand -t 4 <file>` to convert)

9. **Header Guards** - Verifies `#ifndef`/`#define` guards in .h files
   - ⚠️ **Warns**: Header files missing include guards
   - 💡 **Fix**: Add `#ifndef FILENAME_H` / `#define FILENAME_H` / `#endif` pattern

10. **File Headers** - Checks for license headers
    - ⚠️ **Warns**: Source files missing GPL or MIT license headers
    - 💡 **Fix**: Add appropriate license header (see code-standards.md for template)

11. **Naming Conventions** - Detects camelCase (expects snake_case)
    - ⚠️ **Warns**: Possible camelCase function/variable names
    - 💡 **Fix**: Use snake_case (e.g., `keyboard_interface_task`, not `keyboardInterfaceTask`)

12. **Include Order** - Validates include directive organization
    - ⚠️ **Warns**: Own header not first, or SDK headers after project headers
    - 💡 **Fix**: Order: own header → blank line → stdlib → Pico SDK → external libs → project headers

13. **Missing __isr Attribute** - Checks interrupt handler declarations
    - ⚠️ **Warns**: Functions named `*_irq_handler` without `__isr` attribute
    - 💡 **Fix**: Use `void __isr keyboard_irq_handler(void)` for interrupt handlers

14. **Compile-Time Validation** - Advisory check for `_Static_assert` and `#error`
    - ℹ️ **Info**: Reports presence of compile-time validation directives
    - 💡 **Suggestion**: Consider adding compile-time checks for constants (see code-standards.md)
    - 📝 **Note**: This is advisory-only and doesn't affect pass/fail status

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

Debug builds (`cmake -DCMAKE_BUILD_TYPE=Debug`) include a runtime verification that code is actually executing from SRAM:

```c
#include "ram_check.h"

int main(void) {
    ram_check_verify();  // Panics if executing from Flash (0x10000000)
                          // Passes if executing from SRAM (0x20000000)
    // ... rest of initialization
}
```

This catches Flash execution at runtime, which complements the lint script's static check for `pico_set_binary_type(copy_to_ram)` in CMakeLists.txt.

## Testing the Tools

If you're modifying the enforcement tools themselves (not regular development), run the meta-testing suite:

```bash
./tools/test-enforcement.sh
```

This validates that lint checks detect violations correctly. See [TESTING.md](TESTING.md) for detailed scenarios and [QUICKTEST.md](QUICKTEST.md) for quick reference commands.

For regular development, just run the lint script before committing:

```bash
./tools/lint.sh && echo "✅ Ready to commit"
```

## Contributing

If you're adding new enforcement checks, start by adding the check logic to `lint.sh`. Then add corresponding test cases to `test-enforcement.sh` that verify the check detects violations correctly. Document the new check in this README and update `.github/PULL_REQUEST_TEMPLATE.md` with any relevant checklist items.

For new standalone tools, create an executable shell script (`chmod +x tools/new-tool.sh`) and document its usage here. Follow the patterns used by existing tools—colour-coded output, meaningful exit codes (0 for success, 1 for failure), and clear error messages. If the tool should run in CI, update `.github/workflows/ci.yml` accordingly.
