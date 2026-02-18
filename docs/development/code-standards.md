# Code Standards

This project has some specific coding standards, mostly focused on keeping things readable and ensuring the non-blocking architecture stays intact. If you're contributing code, following these guidelines will make the review process smoother and help maintain consistency across the codebase.

These aren't arbitrary rules—they exist because this is an embedded system with tight timing requirements. Things like blocking operations or missing memory barriers can cause subtle bugs that are genuinely difficult to track down, especially when they only show up under specific timing conditions.

---

## General Principles

### Write Code That's Easy to Understand

Favour readability over cleverness. If there's a choice between compact code and clear code, pick clear code every time. Someone else (or future you) will need to understand what's happening, especially when debugging timing issues or protocol errors.

Use descriptive variable and function names. `keyboard_state` is better than `ks`. `scancode_processor_task()` is better than `process()`.

### Be Explicit About What You're Doing

Don't rely on implicit behaviour or side effects. If a function modifies state, make it obvious in the function name. If a variable's shared between IRQ and main loop, mark it `volatile` so the compiler knows not to optimise accesses away.

This is particularly important for memory ordering and synchronisation—the ARM Cortex-M0+ can reorder memory accesses unless you're explicit about barriers. What works in testing might break under different timing conditions.

### Defensive Programming

Check return values. Validate inputs. Use assertions for things that should never happen but might if there's a bug elsewhere in the system.

The firmware runs in a constrained environment with tight timing requirements. Catching problems early (even if it's just an assertion failure that resets the board) is much better than silent corruption or missed scancodes that are impossible to debug.

---

## Code Formatting

### Automated Formatting with clang-format

The project uses clang-format based on Google style with customisations for readability and consistency. The formatting configuration is:

```text
{ BasedOnStyle: Google, ReflowComments: true, UseTab: Never, AllowShortIfStatementsOnASingleLine: false, AllowShortFunctionsOnASingleLine: false, AlwaysBreakBeforeMultilineStrings: true, AllowAllParametersOfDeclarationOnNextLine: true, AlignConsecutiveAssignments: true, AlignConsecutiveBitFields: true, AlignConsecutiveDeclarations: true, AlignConsecutiveMacros: true, ColumnLimit: 100, IndentWidth: 4 }
```

**Key formatting rules:**
- **IndentWidth: 4** - Four spaces per indentation level
- **UseTab: Never** - Spaces only, no tab characters
- **ColumnLimit: 100** - Target line length (can exceed for alignment)
- **AlignConsecutive* options** - Align assignments, declarations, macros, and bitfields for readability
- **AllowShortIfStatementsOnASingleLine: false** - Always use braces and multiple lines
- **AllowShortFunctionsOnASingleLine: false** - Function bodies always on separate lines

If using VSCode, copy `.vscode.example/settings.json` to `.vscode/settings.json` to enable automatic formatting on save.

### Indentation and Spacing

Use **4 spaces per indentation level**. No tabs. This keeps the code looking consistent regardless of editor settings. The lint script (Check 8 and Check 17) enforces this—files with tab characters or 2-space indentation will fail the check.

**Note:** Code within `// clang-format off` and `// clang-format on` blocks is excluded from indentation validation, allowing for hand-formatted tables or aligned data structures.

**Braces:** Use K&R style—opening brace on the same line as the control statement, closing brace on its own line:

```c
if (condition) {
    do_something();
} else {
    do_something_else();
}

void my_function(void) {
    // Function body
}
```

**Line length:** Try to keep lines under 100 characters where reasonable. It's not a hard limit, but overly long lines are hard to read. Break them at natural points (after commas, before operators).

**Whitespace:** Use blank lines to separate logical sections within a function. Put a space after commas, around operators, and after keywords like `if`, `while`, `for`:

```c
uint32_t result = (value * 2) + offset;

if (result > threshold) {
    process_result(result, threshold, mode);
}
```

### Naming Conventions

**Variables and functions:** Use `snake_case` (lowercase with underscores):

```c
uint8_t scancode_buffer[32];
void keyboard_interface_setup(uint pin);
static bool is_initialization_complete(void);
```

**Constants and macros:** Use `UPPER_CASE`:

```c
#define RINGBUF_SIZE 32
#define KEYBOARD_DATA_PIN 2
```

**Struct and enum types:** Use `snake_case` with `_t` suffix:

```c
typedef struct {
    uint8_t head;
    uint8_t tail;
} ring_buffer_t;
```

**Be descriptive:** Function names should describe what they do. Variable names should describe what they contain. Avoid abbreviations unless they're widely recognized (e.g., `pio`, `irq`, `usb`).

**Interrupt handlers:** Use `__isr` attribute and descriptive names:

```c
void __isr keyboard_irq_handler(void);
void __isr keyboard_input_event_handler(void);
```

**Static variables:** Prefix with `static` and use descriptive module-scoped names:

```c
static uint8_t scancode_buffer[32];
static volatile uint8_t irq_flag = 0;  // Note: volatile for IRQ-shared
static command_mode_context_t cmd_mode;
```

**State machines:** Use descriptive enum names with state suffix:

```c
typedef enum {
    CMD_MODE_IDLE,
    CMD_MODE_SHIFT_HOLD_WAIT,
    CMD_MODE_COMMAND_ACTIVE,
} command_mode_state_t;
```

---

## Comments and Documentation

### File Headers

Every source file should have a header comment with license information:

```c
/*
 * This file is part of RP2040 Keyboard Converter.
 *
 * Copyright 2023 Paul Bramhall (paulwamp@gmail.com)
 *
 * RP2040 Keyboard Converter is free software: you can redistribute it
 * and/or modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * [... rest of GPL header ...]
 */
```

### Function Documentation

Use Doxygen-style comments for functions, especially in header files:

```c
/**
 * @brief Processes a single scancode from the ring buffer.
 * 
 * Handles multi-byte sequences (E0/F0 prefixes) and assembles complete
 * scancode events. Updates internal state machine for sequence tracking.
 * 
 * @param scancode Raw scancode byte from ring buffer
 * @return true if a complete scancode event is ready, false otherwise
 * 
 * @note Must be called from main loop context only
 * @note Non-blocking—returns immediately regardless of scancode state
 */
bool scancode_processor_task(uint8_t scancode);
```

Include details about what the function does, what the parameters mean, what the return value indicates, any threading or context requirements (IRQ vs main loop), and whether it blocks or not. These details help anyone reading the code understand not just what the function does, but how and when to use it safely.

### Inline Comments

Add comments for non-obvious code or important decisions. Don't comment the obvious:

```c
// Bad: stating the obvious
i++;  // Increment i

// Good: explaining why
// Skip extended scancode prefix—actual scancode follows
if (scancode == 0xE0) {
    extended_prefix = true;
    return false;
}
```

Use comments to explain **why**, not **what**. The code shows what you're doing; comments should explain why you're doing it that way.

### Enhanced Documentation Standards

Beyond basic commenting, include these details in function and module documentation:

**Threading Model Documentation:**

Always document the threading context for functions and data structures:

```c
/**
 * @brief Command Mode State Structure
 * 
 * Threading Model:
 * - Only accessed from main task context (via command_mode_process)
 * - No interrupt access, no synchronization needed
 */
typedef struct {
    command_mode_state_t state;
    uint32_t state_start_time_ms;
} command_mode_context_t;
```

**Performance Characteristics:**

Document measured timing and performance where relevant:

```c
/**
 * Performance Characteristics:
 * - LOG_LEVEL < DEBUG + success: Only TinyUSB send (~1.6µs, near-zero overhead)
 * - LOG_LEVEL >= DEBUG + success: TinyUSB send + hex dump + UART (~16µs)
 * - Any level + failure: TinyUSB send attempt + hex dump + UART (~16µs)
 */
static inline bool hid_send_report(uint8_t instance, uint8_t report_id, 
                                   void const* report, uint16_t len);
```

**State Machine Documentation:**

For state machines, document transitions clearly:

```c
/**
 * State Transitions:
 * 
 *   IDLE ──┬─> SHIFT_HOLD_WAIT (only both shifts pressed, no other keys)
 *          └─> IDLE (normal operation)
 * 
 *   SHIFT_HOLD_WAIT ──┬─> COMMAND_ACTIVE (held 3 seconds with only shifts)
 *                      ├─> IDLE (shifts released before 3s)
 *                      └─> IDLE (any other key pressed - abort)
 */
```

**Timing Requirements:**

For protocol handlers, document critical timing characteristics:

```c
/**
 * Timing Characteristics:
 * - Clock frequency: 10-16.7 kHz (typical 10-15 kHz)
 * - Clock pulse width: minimum 30µs low, 30µs high
 * - Data setup time: minimum 5µs before clock falling edge
 * - Inhibit time: 96µs CLOCK low (exceeds max bit period)
 */
```

---

## Critical Architecture Rules

These are the non-negotiable rules that keep the converter working reliably. Violating these will cause the lint script to fail and block your PR.

### No Blocking Operations

**Never** use `sleep_ms()`, `sleep_us()`, `busy_wait_ms()`, or `busy_wait_us()` in the main processing path. These block the entire main loop, which prevents scancode processing and can cause the ring buffer to overflow.

Use time-based state machines instead:

```c
// Bad: blocks for 100ms
sleep_ms(100);
do_something();

// Good: non-blocking state machine
static uint32_t start_time = 0;
static enum { IDLE, WAITING, READY } state = IDLE;

void my_task(void) {
    uint32_t now = to_ms_since_boot(get_absolute_time());
    
    switch (state) {
        case IDLE:
            start_time = now;
            state = WAITING;
            break;
            
        case WAITING:
            if (now - start_time >= 100) {
                state = READY;
            }
            break;
            
        case READY:
            do_something();
            state = IDLE;
            break;
    }
}
```

The exception to this is debug-only code, which can use blocking operations if absolutely necessary. But it must add a `// LINT:ALLOW blocking` annotation with justification, guard with `if (in_irq()) return;` to prevent IRQ blocking, and be clearly marked as debug or development code. Even then, think twice before adding blocking operations—there's usually a better way.

### No Multicore APIs

The converter uses only Core 0. Core 1 is unused. Don't use any `multicore_*` or `core1_*` functions. This simplifies the architecture and avoids inter-core synchronization overhead.

```c
// Bad: trying to use Core 1
multicore_launch_core1(my_function);

// Good: everything runs on Core 0
my_function();
```

### No printf() in Interrupt Handlers

Interrupt handlers must be fast and non-blocking. `printf()` can block whilst waiting for UART DMA, which violates the non-blocking requirement.

Use `LOG_*` macros instead—these are DMA-safe and designed for use in interrupt context:

```c
// Bad: printf in IRQ
void __isr keyboard_irq_handler(void) {
    uint8_t scancode = pio_sm_get(pio, sm) >> 21;
    printf("Scancode: 0x%02X\n", scancode);  // DON'T DO THIS
}

// Good: LOG macro
void __isr keyboard_irq_handler(void) {
    uint8_t scancode = pio_sm_get(pio, sm) >> 21;
    LOG_DEBUG("Scancode: 0x%02X\n", scancode);  // OK
}
```

The `LOG_*` macros are defined in `log.h` and handle buffering safely.

### Ring Buffer Safety

The ring buffer uses a single-producer/single-consumer model. The IRQ handler writes (producer), the main loop reads (consumer).

**Never** call `ringbuf_reset()` whilst interrupts are enabled—this violates the single-writer invariant. Only reset during initialization or protocol state resets where interrupts are already disabled.

```c
// Bad: resetting with IRQs enabled
ringbuf_reset();

// Good: disable IRQs first (and document why)
uint32_t irq_state = save_and_disable_interrupts();
ringbuf_reset();  // LINT:ALLOW ringbuf_reset - protocol state machine reset
restore_interrupts(irq_state);
```

### SRAM Execution

All code must run from SRAM, not Flash. This is enforced by the build system and checked at compile time.

The CMakeLists.txt sets `pico_set_binary_type(copy_to_ram)`, and `main.c` includes:

```c
#if !PICO_NO_FLASH && !PICO_COPY_TO_RAM
#error "This must be built to run from SRAM!"
#endif
```

Don't change these settings. Flash execution adds latency that breaks protocol timing.

### Volatile and Memory Barriers

Variables shared between IRQ and main loop must be `volatile` to prevent compiler optimizations that assume single-threaded access.

Use memory barriers (`__dmb()`) after writing volatile variables in IRQ context, and before reading them in main loop context:

```c
static volatile uint8_t irq_flag = 0;
static volatile uint32_t irq_data = 0;

void __isr my_irq_handler(void) {
    irq_data = read_hardware();
    irq_flag = 1;
    __dmb();  // Ensure writes complete before IRQ returns
}

void my_task(void) {
    if (irq_flag) {
        __dmb();  // Ensure reads see latest values
        uint32_t data = irq_data;
        irq_flag = 0;
        process_data(data);
    }
}
```

The barriers ensure memory ordering is correct and the main loop sees updates from the IRQ handler.

---

## Error Handling

### Check Return Values

If a function can fail, check the return value:

```c
// Bad: ignoring return value that indicates success/failure
pio_claim_sm_mask(pio, sm_mask);

// Good: checking for errors
if (!pio_can_add_program(pio, program)) {
    LOG_ERROR("Not enough PIO instruction memory\n");
    return false;
}
```

### Validate Inputs

For public functions, validate inputs before using them:

```c
bool keyboard_interface_setup(uint data_pin) {
    if (data_pin >= NUM_BANK0_GPIOS) {
        LOG_ERROR("Invalid GPIO pin: %u\n", data_pin);
        return false;
    }
    
    // Setup keyboard interface...
}
```

#### Guard Patterns and Validation Delegation

Helper functions often centralize validation logic for code reuse and consistency. Before adding validation guards, trace the call chain to ensure you're not duplicating checks that already exist upstream.

**Validation Delegation Pattern:**

Helper functions that return NULL or error codes typically perform validation internally. Callers delegate validation by checking the return value:

```c
// Helper centralizes validation
PIO find_available_pio(const pio_program_t* program) {
    if (!pio_can_add_program(pio0, program)) {
        if (!pio_can_add_program(pio1, program)) {
            return NULL;  // Both PIOs full
        }
        return pio1;
    }
    return pio0;
}

// Caller delegates validation to helper
keyboard_pio = find_available_pio(&keyboard_interface_program);
if (keyboard_pio == NULL) {  // Validation already done by helper
    LOG_ERROR("No PIO available\n");
    return;
}

// ❌ WRONG: Redundant check - helper already validated this
if (!pio_can_add_program(keyboard_pio, &keyboard_interface_program)) {
    // This can never trigger - helper already checked
}
```

**Single Validation Point Pattern:**

When a public function validates inputs before calling private helpers, the helpers don't need to duplicate validation:

```c
// Public function validates once
void keylayers_process_key(uint8_t code, bool make) {
    const uint8_t target_layer = GET_LAYER_TARGET(code);
    
    // Single validation point for all downstream helpers
    if (target_layer >= max_layers) {
        return;
    }
    
    // Helpers rely on caller's validation
    keylayers_handle_mo(target_layer, code, make);
}

// Private helper - no redundant validation needed
static void keylayers_handle_mo(uint8_t target_layer, uint8_t code, bool make) {
    // Bounds already validated by caller - see function documentation
    layer_state.momentary_keys[target_layer - 1] = code;
}
```

**When to add guards:**
- ✅ Public API entry points (callers may not validate)
- ✅ Genuinely missing from entire call chain
- ✅ Race conditions between check and use

**When NOT to add guards:**
- ❌ Helper already validates and returns NULL/error
- ❌ Caller validates before calling helper function
- ❌ Would duplicate established pattern across codebase

**Tracing validation**: Use `grep -rn "function_name" src/` to find where functions are called, then read the calling function and any helper functions to understand the validation chain.

### Use Assertions for Invariants

Use `assert()` for conditions that should never happen but indicate a serious bug if they do:

```c
#include <assert.h>

void ring_buffer_put(uint8_t data) {
    assert(!ringbuf_is_full());  // Caller should have checked
    buffer[head] = data;
    head = (head + 1) % RINGBUF_SIZE;
}
```

Assertions are enabled in debug builds and stripped in release builds (though for this project, we generally keep them enabled).

### Compile-Time Validation

Use `_Static_assert` or `#error` for compile-time checks:

```c
// Verify buffer size is power of 2 (required for efficient masking)
_Static_assert((RINGBUF_SIZE & (RINGBUF_SIZE - 1)) == 0, 
               "RINGBUF_SIZE must be power of 2");

// Enforce SRAM execution requirement
#if !PICO_NO_FLASH && !PICO_COPY_TO_RAM
#error "This must be built to run from SRAM!"
#endif

// Validate configuration constants
_Static_assert(KEYBOARD_DATA_PIN < 30, "Invalid GPIO pin number");
```

This catches configuration errors at compile time rather than runtime.

---

## Code Organization

### File Structure

Keep related functionality together. Each protocol should have its own directory under [`src/protocols/`](../../src/protocols/), each keyboard should have its own directory under [`src/keyboards/`](../../src/keyboards/). Don't scatter related code across multiple unrelated files—it makes things harder to find when you're debugging.

Within a source file, organise functions in a logical order that reads well from top to bottom. Start with includes and defines, then type definitions, followed by static variables. If you need forward declarations, put them next. Then come private (static) functions, and finally public functions. This ordering means someone reading the file encounters definitions before they're used, which makes things easier to follow.

### Header Guards

Use include guards in all header files:

```c
#ifndef KEYBOARD_INTERFACE_H
#define KEYBOARD_INTERFACE_H

// Header contents...

#endif /* KEYBOARD_INTERFACE_H */
```

### Include Order

Include headers in a specific order with blank lines between groups. This isn't arbitrary—it serves a purpose.

First comes your own header if you're in a `.c` file. This ensures the header is self-contained and doesn't rely on implicit includes from other files—if the header's missing an include, you'll find out immediately rather than discovering it later when someone includes it in a different order.

Then come standard library headers like `<stdio.h>`, `<stdint.h>`, and `<stdbool.h>`. After that, Pico SDK headers like `pico/stdlib.h` and `hardware/pio.h`, followed by external library headers like `tusb.h` and `bsp/board.h`. Finally, project headers like `config.h`, `ringbuf.h`, and `log.h`.

Within each group, sort alphabetically unless there are specific dependencies that require a different order.

```c
// Example from keyboard_interface.c
#include "keyboard_interface.h"  // Own header first

#include <math.h>                 // Standard library

#include "bsp/board.h"            // External libraries
#include "hardware/clocks.h"      // Pico SDK
#include "interface.pio.h"        // Generated PIO headers

#include "config.h"               // Project headers (alphabetical)
#include "led_helper.h"
#include "log.h"
#include "pio_helper.h"
#include "ringbuf.h"
```

### Function Grouping

Group related functions together and separate groups with comment headers:

```c
/* ============================================================================
 * Ring Buffer Operations
 * ============================================================================ */

static inline bool ringbuf_is_empty(void) {
    return head == tail;
}

static inline bool ringbuf_is_full(void) {
    return ((head + 1) % RINGBUF_SIZE) == tail;
}

/* ============================================================================
 * Scancode Processing
 * ============================================================================ */

static bool process_make_code(uint8_t scancode) {
    // ...
}
```

### Specialized Formatting

**Keyboard Layout Definitions:**

Keyboard layout macros may exceed 100 characters for visual alignment. Use `clang-format off/on` to preserve intentional formatting:

```c
// clang-format off
const uint8_t keymap_map[][KEYMAP_ROWS][KEYMAP_COLS] = {
  KEYMAP( \
    ESC,          F1,    F2,    F3,    F4,       F5,    F6,    F7,    F8,     \
    GRV,   1,     2,     3,     4,     5,     6,     7,     8,     9,     0,  \
    TAB,          Q,     W,     E,     R,     T,     Y,     U,     I,     O,  \
    // Visual alignment is critical for understanding physical layout
  )
};
// clang-format on
```

**Long Lines - Acceptable Cases:**

There are a few cases where exceeding the 100-character guideline is actually better than breaking the line:

URLs in comments should stay intact so they're clickable in most editors:
```c
// Reference: https://archive.org/details/bitsavers_ibmpcps284erfaceTechnicalReferenceCommonInterfaces_39004874/page/n229/mode/2up?q=keyboard
```

LOG messages should stay on one line so you can grep for them easily. If you split them across lines, searching for the message becomes a pain:
```c
LOG_INFO("Command mode active! Press 'B'=bootloader, 'D'=log level, 'F'=factory reset, 'L'=LED brightness\n");
```

Keyboard layout arrays get special treatment too—visual alignment matters more than line length there, since you're trying to represent the physical keyboard layout.

**Translation Tables:**

Sparse lookup tables use bracket notation for clarity:

```c
static const uint8_t e0_code_translation[256] = {
    [0x10] = 0x08,  // WWW Search -> F13
    [0x11] = 0x0F,  // Right Alt
    [0x14] = 0x19,  // Right Ctrl
    // Sparse entries make code changes less error-prone
};
```

---

## PIO-Specific Standards

The RP2040's Programmable I/O (PIO) requires special consideration:

### PIO Program Organization

**File structure:**
- PIO assembly: `protocol_name.pio` or `interface.pio`
- Generated header: Auto-generated during build, don't modify
- Interface code: `keyboard_interface.c` wraps PIO interactions

**PIO state machine lifecycle:**

Setting up a PIO state machine follows a specific sequence. First, find an available PIO instance—there are only two (PIO0 and PIO1), and program space is limited:

```c
// 1. Find available PIO instance
PIO pio = find_available_pio(&program);
if (!pio) {
    LOG_ERROR("No PIO space available\n");
    return false;
}

// 2. Load program and claim state machine
offset = pio_add_program(pio, &program);
sm = pio_claim_unused_sm(pio, true);

// 3. Configure and start
program_init(pio, sm, offset, data_pin);

// 4. Setup IRQ handler (if needed)
pio_set_irq0_source_enabled(pio, pis_interrupt0, true);
irq_set_exclusive_handler(pio_irq, &handler);
irq_set_priority(pio_irq, 0x00);        // Set priority before enabling
irq_set_enabled(pio_irq, true);
```

**PIO error recovery:**

Always provide a way to reset or restart PIO state machines when things go wrong. Protocol errors happen—keyboards get power cycled, cables come loose, timing glitches occur. You need a clean way to recover:

```c
void pio_restart(PIO pio, uint sm, uint offset) {
    // 1. Drain TX FIFO (prevents stale data)
    pio_sm_drain_tx_fifo(pio, sm);
    
    // 2. Clear both FIFOs
    pio_sm_clear_fifos(pio, sm);
    
    // 3. Restart state machine
    pio_sm_restart(pio, sm);
    pio_sm_exec(pio, sm, pio_encode_jmp(offset));
}
```

### PIO Resource Management

Always check before allocating PIO resources. There's limited space—only 32 instructions per PIO instance, and only 4 state machines per instance. If you just assume space is available, you'll hit problems when running configurations that use multiple protocols:

```c
if (!pio_can_add_program(pio0, &program)) {
    LOG_WARN("PIO0 full, trying PIO1\n");
    pio = pio1;  // Fall back to PIO1
}
```

Document PIO resource usage in your protocol handler so others know what space they're consuming:

```c
/**
 * PIO Resource Usage:
 * - Program size: 12 instructions
 * - State machine: 1 SM required
 * - DMA: Not used
 * - IRQ: Uses IRQ0 for RX FIFO not empty
 */
```

---

## Testing and Validation

### Run the Lint Script

Before committing, always run the lint script:

```bash
./tools/lint.sh
```

It checks for blocking operations, multicore API usage, `printf()` in potential IRQ context, ring buffer resets without IRQ guards, and references to `docs-internal/` in public files. The script must pass with 0 errors and 0 warnings—no exceptions. If it fails, fix the issues before committing.

### Build and Test

Build your changes with Docker to ensure you're using the same toolchain as everyone else:

```bash
docker compose run --rm -e KEYBOARD="your-keyboard" builder
```

If at all possible, test on real hardware. Timing-sensitive protocols can behave quite differently in simulation or on the bench compared to actual use. What works perfectly in a test harness might fail when connected to a real keyboard that has slightly different timing characteristics or electrical properties.

### Test Scenarios

When testing firmware changes, run through these scenarios:

Start with normal typing to verify basic functionality works as expected. Then stress-test the pipeline with fast typing—this'll show up any buffering or timing issues that only appear under load.

Test Command Mode by holding both shifts for 3 seconds to verify it activates properly and responds to commands. If your protocol supports LEDs, test Caps Lock, Num Lock, and Scroll Lock to make sure they update correctly—some protocols have quirky LED behaviour that only shows up in real use.

Finally, test error recovery by power cycling the keyboard mid-operation. The converter should detect the keyboard disappearing and recover gracefully when it comes back. This catches issues with state machine initialization and cleanup.

These aren't exhaustive tests, but they'll catch most issues before you submit a PR. If you find something that breaks, add a test case for it so it doesn't regress later.

---

## Documentation Policy

### Public Documentation

Files in `docs/` are committed to git and form part of the public documentation—things like README files, protocol documentation, hardware setup guides, and keyboard-specific notes.

### Private Documentation

This is important: Never commit or reference files in `docs-internal/`. That directory contains local development notes, performance analysis, and investigation findings. It's in `.gitignore` and must stay local-only—it's not for public consumption. Use this area to keep notes on things you might be working on! It can help you to keep context if you're managing multiple tasks.

If you need to reference information from `docs-internal/` in your code or commits, rephrase it so it doesn't mention the directory. Use the findings, but don't cite the source. This keeps the internal development notes separate from the public documentation.

---

## Style Consistency

When in doubt, match the existing code style. If you're modifying an existing file, follow the conventions already in that file rather than reformatting to your preference.

Consistency matters more than personal style preferences—it makes the codebase easier to navigate and understand.

---

That covers the main coding standards. The critical ones are the architecture rules (no blocking, no multicore, volatile + barriers) and running the lint script before committing. Everything else is mostly about keeping the code readable and consistent.

If you're not sure about something, have a look at existing protocol implementations (AT/PS2, XT, Amiga, etc.) to see how they handle similar situations. Or ask in an issue on GitHub—I'm happy to clarify.

---

## Related Documentation

- [Contributing Guide](contributing.md) - Pull request process and commit format
- [Protocol Implementation Guide](protocol-implementation.md) - Standard protocol setup pattern
- [Adding Keyboards](adding-keyboards.md) - Step-by-step keyboard support guide
- [Custom Keymaps](custom-keymaps.md) - Keymap modification guide
- [Development Guide](README.md) - Overview and getting started
- [Advanced Topics](../advanced/README.md) - Architecture and performance analysis
- [Lint Tool](../../tools/lint.sh) - Code quality enforcement

---

**Questions or stuck on something?**  
Pop into [GitHub Discussions](https://github.com/PaulW/rp2040-keyboard-converter/discussions) or [report a bug](https://github.com/PaulW/rp2040-keyboard-converter/issues) if you've found an issue.
