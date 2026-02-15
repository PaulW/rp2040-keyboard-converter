# Protocol Implementation Guide

If you're adding a new keyboard or mouse protocol to the converter, there's a standard pattern that all protocol implementations follow. This isn't enforced by the build system—it's more of a convention that's emerged from the existing implementations. Following this pattern makes the code easier to understand and maintains consistency across the codebase.

This guide covers the setup sequence for protocol implementations. If you're adding keyboard support for an existing protocol, you probably want the [Adding Keyboards Guide](adding-keyboards.md) instead.

---

## Understanding Protocol Setup

Each protocol has its own `keyboard_interface_setup()` function in `src/protocols/<protocol>/keyboard_interface.c`. If the protocol supports mice, there will also be a `mouse_interface_setup()` function in `mouse_interface.c`. Whilst the specific details vary according to what each protocol needs, they all follow the same basic pattern. This isn't by accident—the pattern ensures each protocol initialises its resources in the correct order and handles errors consistently.

The pattern matters because:
- PIO resources are limited (2 PIOs, 4 state machines each)
- Ring buffer must be clean before use
- IRQ handlers need proper priority configuration
- LED state should reflect the actual hardware state
- Error conditions need consistent handling

Things can go wrong if you deviate from this pattern. For instance, if you set up the IRQ handler before clearing the ring buffer, you might process stale data from a previous session. If you don't initialise the LED state, the indicator might show ready when the keyboard isn't actually initialised yet.

---

## Standard Setup Sequence

Here's the standard pattern all protocols follow, in order:

### 1. LED Initialisation

First up, set the converter's keyboard ready state to false and update the status LED. This happens before anything else because you want the LED to show the actual state—the keyboard isn't ready yet, so don't indicate that it is.

```c
#ifdef CONVERTER_LEDS
  converter.state.kb_ready = 0;
  update_converter_status();
#endif
```

The `#ifdef` is there because LED support is optional (controlled by `CONVERTER_LEDS` define). If LEDs aren't configured, the code doesn't include the LED library at all, which saves a bit of memory.

### 2. Ring Buffer Reset

Next, clear any stale data from the ring buffer. The buffer might contain leftover scancodes from a previous session (if the converter was reset but the keyboard wasn't power-cycled), so you need a clean slate before starting.

```c
ringbuf_reset();  // LINT:ALLOW ringbuf_reset
```

The `LINT:ALLOW` annotation tells the lint checker this is intentional. Normally, calling `ringbuf_reset()` whilst IRQs are enabled would be flagged as an error (it's not safe—the IRQ handler might be writing to the buffer). Here it's fine because we're in the setup function before IRQs are enabled.

### 3. PIO Instance Allocation

Now find an available PIO instance that has enough instruction memory for your protocol's program. The RP2040 has two PIO blocks (PIO0 and PIO1), each with 32 instruction memory slots. The helper function searches both and returns the first one with enough space.

```c
keyboard_pio = find_available_pio(&pio_interface_program);
if (keyboard_pio == NULL) {
  LOG_ERROR("No PIO available for AT/PS2 Keyboard Interface Program\n");
  return;
}
```

If allocation fails, log an error and return early. There's no point continuing—without a PIO instance, the protocol can't function. The error message should identify which protocol failed (helpful when debugging multi-protocol builds or when system resources are exhausted).

### 4. State Machine Claiming

After getting a PIO instance, claim an unused state machine. Each PIO has 4 state machines (numbered 0-3), and you need one for the keyboard interface.

Most protocols use the panic-on-failure pattern:

```c
keyboard_sm = (uint)pio_claim_unused_sm(keyboard_pio, true);
```

The `true` parameter means "panic if no state machine is available". This is reasonable for most protocols—if you can't get a state machine, the system is misconfigured or out of resources, and there's not much you can do about it.

However, the Apple M0110 protocol takes a different approach with manual error handling:

```c
int sm = pio_claim_unused_sm(keyboard_pio, false);
if (sm < 0) {
  LOG_ERROR("No PIO state machine available for M0110 Keyboard Interface\n");
  return;
}
keyboard_sm = (unsigned int)sm;
```

This is actually the better pattern if you're writing a new protocol. It gives you control over error handling and allows for cleaner error messages. The panic approach is simpler but less graceful.

### 5. Program Loading

Load your protocol's PIO program into the instruction memory. The function returns an offset indicating where the program was loaded (because multiple programs can coexist in the same PIO's instruction memory).

```c
keyboard_offset = pio_add_program(keyboard_pio, &pio_interface_program);
```

This can't fail—the helper in step 3 already verified there's enough space. The offset is used later when initialising the state machine (it needs to know where your program starts).

### 6. Pin Assignment

Store the GPIO pin number(s) for the protocol. This varies by protocol needs:

```c
// Most protocols: single data pin
keyboard_data_pin = data_pin;

// Amiga protocol: separate clock and data pins (must be adjacent)
keyboard_data_pin = data_pin;
keyboard_clock_pin = data_pin + 1;
```

The Amiga protocol has a specific requirement where the clock pin must be immediately after the data pin (a constraint of how the PIO program is written). Other protocols might have different requirements—document them clearly if your protocol has specific pin relationships.

### 7. Clock Divider Calculation

Calculate the PIO clock divider based on your protocol's timing requirements. The helper function converts a microsecond timing value into the divider ratio needed by the PIO hardware.

```c
float clock_div = calculate_clock_divider(ATPS2_TIMING_CLOCK_MIN_US);
```

The timing constant should be defined in your protocol's header file, not hardcoded here. This makes it easier to adjust timing if needed and documents what the value represents.

### 8. PIO Program Initialisation

Initialise the state machine with your program. This sets up the PIO hardware: configures pins, sets the clock divider, loads the initial state, and prepares the state machine to run.

```c
pio_interface_program_init(keyboard_pio, keyboard_sm, keyboard_offset, data_pin, clock_div);
```

The init function is generated by `pioasm` (the PIO assembler) and is specific to your protocol's `.pio` file. It knows how to configure the state machine for your particular program's requirements.

### 9. IRQ Handler Registration

Register your interrupt handler for this PIO instance. The handler will be called when the PIO raises an interrupt (typically when it receives a complete scancode).

```c
uint pio_irq = keyboard_pio == pio0 ? PIO0_IRQ_0 : PIO1_IRQ_0;
irq_set_exclusive_handler(pio_irq, &keyboard_input_event_handler);
```

The IRQ number depends on which PIO instance you got (PIO0 or PIO1). Each has its own IRQ line. The handler must be marked `__isr` and must never block (no `sleep_ms()`, no `printf()`, no loops without exit conditions).

### 10. IRQ Priority Configuration

Set the interrupt priority before enabling the IRQ. All keyboard protocols use priority 0x00 (highest) to ensure keyboard events are processed before other interrupts.

```c
irq_set_priority(pio_irq, 0x00);
```

The Pico SDK defaults to priority 0x80 (medium priority). Setting priority 0x00 elevates the keyboard interrupt above the default, ensuring minimal latency for keyboard events. The RP2040 only uses the top 2 bits of the priority value, so the four effective levels are 0x00 (highest), 0x40, 0x80, and 0xC0 (lowest).

**Important:** Set priority *before* enabling the IRQ to avoid a race condition window where the handler could execute at default priority if a PIO event fires between enable and priority set.

### 11. IRQ Enable

Enable the interrupt so your handler will actually be called. Without this, the PIO can raise interrupts all day and nothing will happen.

```c
irq_set_enabled(pio_irq, true);
```

Enable the IRQ *after* setting the handler and priority, never before. Otherwise you might get interrupts before the handler is configured (hard fault) or at the wrong priority (timing issues).

### 12. Setup Complete - Update LED State

After everything is initialised successfully, update the keyboard ready state and refresh the LED status.

```c
#ifdef CONVERTER_LEDS
  converter.state.kb_ready = 1;
  update_keyboard_ready_led();
#endif
```

This provides visual feedback that the protocol setup completed successfully. The LED now accurately reflects the hardware state.

### 13. Logging

Finally, log confirmation that setup completed. Include relevant details like which PIO instance and state machine were allocated, and any protocol-specific parameters.

```c
LOG_INFO("PIO%d SM%d AT/PS2 Interface program loaded at offset %d with clock divider of %.2f\n",
         (keyboard_pio == pio0 ? 0 : 1), keyboard_sm, keyboard_offset, clock_div);
```

This appears in the UART debug output and helps verify the protocol initialised correctly. It's particularly useful when debugging resource allocation issues or when multiple protocols are loaded simultaneously.

---

## Pattern Variations

Whilst all keyboard protocols follow this basic structure, there are legitimate reasons for variations:

### Mouse vs. Keyboard Setup

The AT/PS2 protocol supports both keyboards and mice on separate hardware lines. The mouse setup in `mouse_interface_setup()` follows a similar pattern with one critical difference:

**Mouse does NOT use the ring buffer.** Unlike keyboards which queue scancodes through the ring buffer for processing in the main loop, mice process data packets directly in the IRQ context via `mouse_event_processor()`. This means:

```c
void mouse_interface_setup(uint data_pin) {
  // 1. LED initialization
  #ifdef CONVERTER_LEDS
    converter.state.mouse_ready = 0;
    update_converter_status();
  #endif

  // 2. NO ringbuf_reset() - mouse doesn't use ring buffer

  // 3-13. Standard PIO/IRQ setup (same as keyboard)
  mouse_pio = find_available_pio(&pio_interface_program);
  // ... rest of setup follows keyboard pattern
}
```

**Why this difference?**
- Keyboard: Multi-byte scancode sequences need buffering (E0 prefixes, Pause key sequences)
- Mouse: Fixed packet format (3-4 bytes), processed as complete packets in IRQ handler
- Mouse: HID reports sent immediately after packet validation

The mouse still follows the same resource allocation pattern (PIO → SM → Program → Clock → IRQ → Priority), just without the ring buffer reset step.

### Error Handling Approach

**Panic on failure:**
```c
keyboard_sm = (uint)pio_claim_unused_sm(keyboard_pio, true);
```

Used by: AT/PS2, XT, Amiga

**Manual error handling:**
```c
int sm = pio_claim_unused_sm(keyboard_pio, false);
if (sm < 0) {
  LOG_ERROR("No state machine available\n");
  return;
}
keyboard_sm = (unsigned int)sm;
```

Used by: M0110

The manual approach is preferable for new protocols—it's more explicit about error conditions and gives better error messages. The panic approach is simpler but less informative when things go wrong.

### Pin Requirements

**Single pin:**
```c
keyboard_data_pin = data_pin;
```

**Adjacent pins:**
```c
keyboard_data_pin = data_pin;
keyboard_clock_pin = data_pin + 1;  // Must be adjacent
```

Document any pin relationship requirements clearly in your protocol's README. If pins must be adjacent, state why (usually it's a PIO program requirement). If there's flexibility, mention that too.

### Timing Constants

Always use named constants rather than magic numbers:

```c
// GOOD: Named constant
float clock_div = calculate_clock_divider(XT_TIMING_SAMPLE_US);

// BAD: Magic number
float clock_div = calculate_clock_divider(10);
```

Define timing constants in your protocol's header file with clear names that keyboard protocols follow key aspects of this pattern:
- LED initialisation present
- Ring buffer reset before IRQ setup (keyboard only)
- IRQ priority configured (not left at SDK default)

The lint checks specifically target `keyboard_interface.c` files and run automatically in CI. Run locally before committing: `./tools/lint.sh`

**Note:** Mouse implementations are intentionally excluded from the ring buffer check as they use direct event processing instead
After setup completes, the protocol's task function (`keyboard_interface_task()`) handles the runtime state machine. This varies significantly between protocols depending on their complexity:

- **Unidirectional protocols** (XT): Just process incoming scancodes
- **Bidirectional protocols** (AT/PS2, Amiga, M0110): Also handle sending commands, LED updates, and error recovery

The setup function only deals with initialisation. The complexity of your protocol's state machine (initialisation sequences, command acknowledgements, error recovery) lives in the task function.

---

## Using This Pattern

When implementing a new protocol, use the existing implementations as reference:
- **AT/PS2 Keyboard** ([src/protocols/at-ps2/keyboard_interface.c](../../src/protocols/at-ps2/keyboard_interface.c)): Most complex, full bidirectional with error recovery
- **AT/PS2 Mouse** ([src/protocols/at-ps2/mouse_interface.c](../../src/protocols/at-ps2/mouse_interface.c)): Mouse protocol example, direct event processing without ring buffer
- **XT** ([src/protocols/xt/keyboard_interface.c](../../src/protocols/xt/keyboard_interface.c)): Simplest, unidirectional only
- **Amiga** ([src/protocols/amiga/keyboard_interface.c](../../src/protocols/amiga/keyboard_interface.c)): Bidirectional with adjacent pin requirement
- **M0110** ([src/protocols/apple-m0110/keyboard_interface.c](../../src/protocols/apple-m0110/keyboard_interface.c)): Manual error handling example

Each protocol's README in `src/protocols/<protocol>/` contains timing diagrams and protocol-specific details. Start with those to understand the protocol requirements, then follow this setup pattern for consistency.

**For mouse implementations:** Check the AT/PS2 mouse interface to see how mouse-specific event processing works without the ring buffer. The setup pattern is nearly identical, just without the ring buffer reset step.

---

## Validation

The automated lint script ([tools/lint.sh](../../tools/lint.sh)) checks that protocols follow key aspects of this pattern:
- LED initialisation present
- Ring buffer reset before IRQ setup
- IRQ priority configured (not left at SDK default)

The lint checks run automatically in CI and should be run locally before committing (`./tools/lint.sh`).

---

## Why This Pattern Matters

This pattern isn't arbitrary—each step builds on the previous ones:

1. LEDs first → visual feedback of actual state
2. Ring buffer reset → clean slate for new data
3. PIO allocation → get hardware resources needed
4. SM claim → reserve state machine before loading program
5. Program load → install code into instruction memory
6. Pin assignment → define hardware interface
7. Clock divider → configure timing
8. PIO init → set up state machine with all parameters
9. IRQ handler → define what happens on keyboard events
10. IRQ enable → start receiving events
11. IRQ priority → ensure minimal latency
12. LED update → show successful initialisation
13. Logging → confirm setup completed

If you skip steps or reorder them, you risk initialisation failures, race conditions, or undefined behaviour. The pattern exists because these steps have dependencies on each other and must happen in this specific order for reliable operation.

---

## Common Mistakes (Keyboard)
**Problem:** Stale data from previous session gets processed  
**Solution:** Always `ringbuf_reset()` before `irq_set_enabled()` in keyboard setup  
**Note:** Mice don't use ring buffer, so this doesn't apply to mouse implementations

### Forgetting LED initialisation (Keyboard/Mouse)et()` before `irq_set_enabled()`

### Forgetting LED initialisation
**Problem:** LED shows ready state before protocol is actually ready  
**Solution:** Set `kb_ready = 0` at start, `kb_ready = 1` at end

### Hardcoded timing values
**Problem:** Unclear what the value represents, hard to tune  
**Solution:** Use named constants defined in protocol header

### Missing IRQ priority
**Problem:** Keyboard events handled at default priority (0x80) instead of high priority (0x00), or handler executes at default priority during race condition window  
**Solution:** Always call `irq_set_priority(pio_irq, 0x00)` *before* `irq_set_enabled()` to avoid timing window

### Wrong error message protocol name
**Problem:** Confusing error messages when debugging  
**Solution:** Error messages should clearly identify which protocol failed

---

## Further Reading

- [Code Standards](code-standards.md) - General coding guidelines and architecture rules
- [Adding Keyboards](adding-keyboards.md) - How to add keyboards using existing protocols
- [Protocol Documentation](../protocols/README.md) - Details on implemented protocols
- [PIO Helper Functions](../../src/common/lib/pio_helper.h) - Utility functions for PIO resource management
