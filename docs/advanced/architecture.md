# System Architecture

If you're wanting to understand how everything fits together inside the converter—whether you're debugging timing issues, adding a new protocol, or just curious—this should give you a decent overview of what's going on under the hood.

I won't dive into every single implementation detail here (that's what the source code's for), but I'll cover the key pieces and how they all connect together.

---

## The Big Picture

The converter's built around a single-core architecture running entirely from SRAM. Everything happens on Core 0 of the RP2040, with Core 1 sitting idle. This keeps things simple and avoids all the complexity of synchronizing between cores.

The whole design is focused on keeping latency as low as possible whilst making sure nothing blocks. There's no `sleep_ms()` calls or busy-waiting anywhere in the main pipeline—everything uses state machines that check timestamps to see if enough time's passed. This means the main loop runs continuously without ever stalling.

**Why SRAM execution?** The RP2040's Flash memory has variable access latency due to caching, which isn't ideal when you need deterministic timing for protocol handling. Running the entire program from SRAM eliminates that overhead and gives us predictable execution timing. It does mean we're more constrained on code size, but for what we're doing it's absolutely worth the trade-off.

---

## How Data Flows Through the System

When you press a key on your keyboard, here's an example journey it takes before it appears on your computer, obviously this might differ slightly depending on the protocol and keyboard, but this is a general flow:

```
┌──────────────┐
│ Physical Key │  You press a key
│    Press     │
└──────┬───────┘
       │
       ▼
┌──────────────────────────┐
│   PIO State Machine      │  Hardware captures clock/data signals
│  (Protocol Timing)       │  Assembles bits into scancode bytes
└──────┬───────────────────┘
       │
       ▼
┌──────────────────────────┐
│   Interrupt Handler      │  PIO fires an interrupt when byte's ready
│   (IRQ Context)          │  Extracts scancode, writes to ring buffer
└──────┬───────────────────┘
       │
       ▼
┌──────────────────────────┐
│   Ring Buffer (32 bytes) │  Lock-free FIFO queue
│   (Shared Memory)        │  IRQ writes, main loop reads
└──────┬───────────────────┘
       │
       ▼
┌──────────────────────────┐
│  Scancode Processor      │  Handles multi-byte sequences
│  (Main Loop - CPU)       │  Deals with E0/F0 prefixes, Pause key
└──────┬───────────────────┘
       │
       ▼
┌──────────────────────────┐
│  Keymap Translation      │  Maps scancode → HID keycode
│  Command Mode Handler    │  Detects command key combinations
└──────┬───────────────────┘
       │
       ▼
┌──────────────────────────┐
│    HID Interface         │  Assembles HID report
│    (Main Loop)           │  Waits for tud_hid_ready()
└──────┬───────────────────┘
       │
       ▼
┌──────────────────────────┐
│   TinyUSB Stack          │  USB communication with host
│   (USB Callbacks)        │  10ms polling interval
└──────┬───────────────────┘
       │
       ▼
┌──────────────┐
│  Host OS     │  Key appears on your computer
│  Receives    │
└──────────────┘
```

The pipeline's deliberately non-blocking throughout—everything's either hardware-driven (PIO handles protocol timing) or poll-based in the main loop. The USB host polls every 8ms (configured by `bInterval` in usb_descriptors.c), which becomes the main bottleneck for maximum throughput. The actual processing (scancodes, keymap translation, HID report assembly) happens quickly enough that USB polling is the limiting factor.

---

## The Main Components

### PIO State Machines

The RP2040's Programmable I/O blocks are doing the heavy lifting for protocol timing. These are dedicated hardware state machines that can bit-bang protocols with precise timing, completely independently of what the CPU's doing.

Each protocol has its own PIO program—effectively a tiny assembly program that runs on the PIO hardware. These programs handle the clock and data line timing, shifting in bits, and assembling them into scancode bytes. When a complete byte's ready, the PIO fires an interrupt to let the CPU know there's data waiting.

**Why use PIO rather than GPIO interrupts?** Timing. These protocols need precise sampling of data lines relative to clock edges. Doing that reliably in software on the CPU would be difficult—you'd need to handle other interrupts, and interrupt latency can vary. The PIO hardware handles this deterministically, freeing the CPU to focus on higher-level processing.

The PIO programs are written in `.pio` files (PIO assembly), which get compiled into header files during the build. If you're adding a new protocol, you'll likely need to write a PIO program that understands that protocol's timing.

### Interrupt Handlers

When the PIO completes receiving a scancode byte, it fires an interrupt. The interrupt handler's job is pretty simple: grab the byte from the PIO, do any pre-processing or validation steps of the data before writing it to the ring buffer, and get out as fast as possible.

**Critical rule:** Interrupt handlers must be non-blocking and fast. No `printf()`, no `sleep_ms()`, no loops. Just read the data, write it to the buffer, and return.

The ring buffer write is lock-free—the IRQ is the only producer (writes to `head`), whilst the main loop is the only consumer (writes to `tail`). This means no mutexes or critical sections needed. We do use `volatile` qualifiers and memory barriers (`__dmb()`) to ensure visibility between contexts, but that's it.

### Ring Buffer

The ring buffer's a 32-byte FIFO queue that sits between the interrupt handler and the main loop. It's sized to handle bursts of scancodes from fast typing without overflowing.

**Why 32 bytes?** It's a power of two (which makes the modulo arithmetic cheap), and it's large enough to buffer several scancodes during worst-case scenarios. In practice, the main loop processes scancodes fast enough that the buffer rarely gets more than a few bytes deep.

The buffer uses a single-producer/single-consumer model:
- **Producer (IRQ):** Writes new scancodes, increments `head` pointer
- **Consumer (Main loop):** Reads scancodes, increments `tail` pointer

When `head == tail`, the buffer's empty. When `(head + 1) % 32 == tail`, it's full. The implementation's lock-free because only one context writes each pointer.

One thing to watch out for: you can't call `ringbuf_reset()` whilst interrupts are enabled. That would break the single-writer invariant. Resets only happen during initialization or state machine resets where the protocol handler's already disabled interrupts.

### Scancode Processor

Not all scancodes are single bytes. Some protocols (like AT/PS2 Scancode Set 2) use multi-byte sequences—escape prefixes like `E0` and `F0` (for extended keys and key releases), and even longer sequences like the Pause key which sends `E1 14 77 E1 F0 14 F0 77`.

The scancode processor runs in the main loop on the CPU (not in the PIO—the PIO only handles bit-level protocol timing). It reads bytes from the ring buffer and assembles them into complete scancode events. Once it's got a complete sequence, it knows whether it's a make (key press) or break (key release), and what the actual scancode is.

Each protocol can have its own scancode processor implementation, since the rules differ between Scancode Set 1, Set 2, Set 3, and the various other protocols. The processor's responsible for maintaining state across bytes and figuring out when a complete event's ready. Documentation for these is in [docs/scancodes/](../scancodes/README.md), with source implementations in [`src/scancodes/`](../../src/scancodes/).

### Keymap Translation

Once we've got a complete scancode (make or break), we need to translate it into a USB HID keycode. That's where the keyboard-specific keymaps come in.

Each keyboard configuration has a keymap—a lookup table that maps scancodes to HID keycodes. The keymap's defined in the keyboard's `keyboard.c` file using macros like `KEYMAP_ANSI()` or `KEYMAP_ISO()` that match the physical layout of the keyboard.

This is also where Command Mode detection happens. Before we send a key event to the HID interface, Command Mode checks if you're holding down the activation keys (default: both shifts). If you are, it starts monitoring for command key presses instead of sending them as normal keystrokes.

### HID Interface

The HID interface takes HID keycodes from the keymap translation and assembles them into USB HID reports. These reports describe which keys are currently pressed, which modifiers are active, and so on.

**Important quirk:** We can only send a HID report when `tud_hid_ready()` returns true. This function checks whether the USB host is ready to accept more data. The host polls USB devices every 8ms (configured by `bInterval` in usb_descriptors.c), so there's a natural rate limit here—we're often waiting for the USB host to be ready before we can send the next report.

The HID interface maintains the current keystroke state (which keys are pressed) and only sends reports when something changes. This reduces unnecessary USB traffic and keeps things efficient.

### TinyUSB Stack

TinyUSB handles all the low-level USB communication—enumeration, descriptor handling, endpoint management, and so on. It's a well-tested, mature USB stack that runs on lots of different microcontrollers, including the RP2040.

We're using it in device mode, where the RP2040 acts as a USB HID keyboard and mouse. TinyUSB handles all the USB protocol details, and we just interface with it through a few callbacks and functions like `tud_hid_keyboard_report()` and `tud_hid_mouse_report()`.

---

## Memory Layout

The RP2040 has 264KB of SRAM. Actual usage depends on build configuration (which protocols and keyboards are compiled in), leaving plenty of headroom. The exact usage varies based on which features you've enabled. The linker reports precise memory consumption during build.

**Flash usage** also depends on configuration. The RP2040's got 2MB of Flash, so there's no shortage of space. CI limits enforce maximum 230KB flash and 150KB RAM (see [.github/workflows/ci.yml](../../.github/workflows/ci.yml)).

**Critical detail:** Even though the code's stored in Flash, it's copied to SRAM at boot time and executed from there. This is enforced by the build system (`pico_set_binary_type(copy_to_ram)` in CMakeLists.txt) and verified at compile time by a `#error` check in `main.c` if you accidentally build for Flash execution. SRAM execution's faster and more reliable for this kind of real-time work.

---

## Execution Contexts

There are three main execution contexts in the converter:

### Interrupt Context (IRQ Handlers)

This is where the PIO interrupt handlers run when the hardware has data ready. IRQ context has the highest priority—it can preempt the main loop at any time.

**Rules for IRQ context:**
- Must be fast
- No blocking operations (`sleep_ms`, `busy_wait_us`, etc.)
- No `printf()` or other UART operations (use `LOG_*` macros which are DMA-safe)
- No complex processing—just queue data and return
- Use `volatile` and memory barriers for shared data

The interrupt handlers in this project follow these rules strictly. They extract scancodes from the PIO, write them to the ring buffer, and return. That's it.

### Main Loop Context

The main loop runs continuously, processing queued scancodes, managing protocol state, and sending USB HID reports.

The main loop never blocks. All time-based operations use `to_ms_since_boot(get_absolute_time())` to check if enough time's passed, rather than sleeping. This keeps the loop responsive and ensures we can handle bursts of scancodes without delay.

**Typical main loop iteration:**
1. Check ring buffer for new scancodes
2. Process any scancodes through the scancode processor
3. Run protocol state machine tasks (initialization, LED commands, etc.)
4. Check Command Mode state
5. If there's a HID event and USB is ready, send report
6. Handle mouse events if mouse support's enabled
7. Run TinyUSB stack tasks
8. Repeat

### USB Callback Context

TinyUSB calls back into our code from USB interrupt context when USB events happen (enumeration, configuration, control requests, etc.). These callbacks need to be fast and non-blocking, similar to the protocol interrupt handlers.

USB callbacks in this implementation set flags or copy small amounts of data. The bulk of USB processing happens in the main loop when we call `tud_task()`.

---

## Thread Safety and Synchronization

We use a few different patterns to ensure data stays consistent across execution contexts:

### Volatile Variables with Memory Barriers

When IRQ handlers need to communicate state to the main loop (beyond just the ring buffer), we use `volatile` variables with explicit memory barriers:

```c
static volatile uint8_t irq_flag = 0;
static volatile uint32_t irq_data = 0;

void __isr my_irq_handler(void) {
    irq_data = read_hardware_register();
    irq_flag = 1;
    __dmb();  // Ensure writes complete before returning
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

This pattern's used sparingly—most communication goes through the ring buffer—but it's useful for signaling state changes or errors.

### Single-Writer Principle

Wherever possible, we ensure only one context writes to any given variable. The IRQ writes the ring buffer's `head` pointer, the main loop writes `tail`. The IRQ sets flags, the main loop clears them. This eliminates most race conditions by design.

---

## Critical Design Principles

The converter's architecture relies on several non-negotiable principles that ensure reliability and deterministic behaviour. These aren't arbitrary choices—they're fundamental to how the converter achieves low latency without dropping scancodes.

### Single-Core Architecture

Everything runs on Core 0 of the RP2040. Core 1 sits completely idle, disabled from boot. This might seem inefficient on a dual-core processor, but it eliminates entire categories of bugs whilst delivering more than sufficient performance.

**Why single-core?** Multicore architectures require complex synchronization—mutexes to protect shared data, spinlocks for critical sections, atomic operations for state coordination, memory barriers to enforce ordering, and careful analysis to prevent deadlocks. Each mechanism adds code complexity and execution overhead. Worse, multicore bugs frequently show up as intermittent, timing-dependent failures that are genuinely difficult to reproduce and debug.

A single-core approach sidesteps all of this. Interrupt handlers share data with the main loop using simple volatile variables and memory barriers—no locks needed. Timing becomes deterministic, latency measurements stay consistent across executions, and debugging follows a single thread of control. Power consumption drops with Core 1 in deep sleep.

The converter's processing requirements are modest. Even if parallelization were possible, the bottleneck remains USB polling (8ms intervals per `bInterval` in [usb_descriptors.c](../../src/common/lib/usb_descriptors.c)), not the processing pipeline.

**Rule:** Never use `multicore_*` or `core1_*` functions in this codebase. The [`tools/lint.sh`](../../tools/lint.sh) enforcement script detects these violations and blocks commits that attempt to enable Core 1. If you find yourself wanting multicore, there's likely a blocking operation somewhere that needs refactoring into a non-blocking state machine.

### Non-Blocking Operations

Every function in the main loop and interrupt handlers must complete within microseconds without blocking. This requirement stems from PIO hardware timing precision and the need for responsive USB communication.

**Why non-blocking matters:** PIO state machines react to keyboard signals rapidly. If the main loop blocks, scancodes accumulate in the ring buffer and risk overflow. USB polling occurs every 8 milliseconds, so any blocking operation delays response to host queries. Blocking in an interrupt handler prevents the PIO from delivering new scancodes and can completely wedge the converter.

Four functions are **strictly forbidden** in production code: `sleep_ms()`, `sleep_us()`, `busy_wait_ms()`, and `busy_wait_us()`. These halt execution for a specified duration, preventing all other work. Long loops without early exit conditions also constitute blocking operations and require refactoring if they iterate thousands of times.

**Exception for debug code:** Debug diagnostics may require blocking for specific purposes like bit-banging protocols or introducing delays to observe timing behaviour. In these cases, add a `LINT:ALLOW blocking` comment explaining why the blocking operation is acceptable, and protect it with an `if (in_irq()) return;` guard to ensure it never executes in interrupt context:

```c
if (in_irq()) return;  // Never block in IRQ
sleep_us(delay);  // LINT:ALLOW blocking - timing diagnostic for protocol debug
```

**Non-blocking pattern:** Implement time-based state machines using timestamps. Store a timestamp when entering a state, then check elapsed time with `to_ms_since_boot(get_absolute_time())` on each loop iteration. When the timeout expires, transition to the next state. This pattern allows the main loop to continue servicing the ring buffer, USB stack, and other tasks whilst waiting for time-based conditions.

### SRAM Execution

The RP2040 includes 264KB of SRAM and 2MB of flash storage. Flash provides persistent storage but introduces variable access latency due to caching and timing characteristics. SRAM delivers deterministic, single-cycle access regardless of cache state. For precise protocol timing, this consistency is critical.

**How it works:** The CMake configuration specifies `pico_set_binary_type(rp2040-converter copy_to_ram)`, which instructs the linker to copy the entire program from flash into SRAM during boot. After initialization completes, all code executes from RAM with predictable timing. This trades memory efficiency (the binary must fit in both flash for storage and SRAM for execution) for performance consistency. The RP2040's memory capacity accommodates this approach comfortably.

**Why it matters:** Flash cache misses introduce unpredictable latency. When code executes from flash, the first execution of a function may require dozens of microseconds to load the instruction stream into cache, whilst subsequent calls complete quickly. This variability makes latency measurements unreliable and creates difficult-to-characterize worst-case scenarios. SRAM execution eliminates this problem—every instruction takes the same time whether it executes for the first time or the millionth time.

**Verification:** Check the SRAM execution configuration in [`CMakeLists.txt`](../../src/CMakeLists.txt) at line 93 where the binary type is set. The compiled binary's `.elf.map` file shows memory layout, confirming that code sections reside in SRAM regions (addresses starting with 0x20000000) rather than flash addresses (0x10000000).

### Ring Buffer Safety

The ring buffer implements the critical data handoff between interrupt context (where keyboard events arrive) and the main loop (where processing occurs). Incorrect implementation leads to dropped keystrokes, duplicate characters, or complete system failure. The design follows a lock-free single-producer/single-consumer (SPSC) pattern that guarantees safety without locks or atomic operations.

**Single-writer principle:** The interrupt handler acts as the sole producer, calling `ringbuf_put()` to add scancodes as they arrive from PIO hardware. The main loop acts as the sole consumer, calling `ringbuf_get()` to retrieve scancodes for processing. The head pointer (write position) is modified only by the interrupt handler, whilst the tail pointer (read position) is modified only by the main loop. This strict separation eliminates any possibility of concurrent modification.

**Overflow protection:** The interrupt handler checks `ringbuf_is_full()` before writing data to prevent overflow. If the buffer is full, the scancode is dropped and an error is logged (when logging is configured). This situation indicates the main loop cannot maintain pace. The main loop checks `ringbuf_is_empty()` before reading to avoid spurious reads. These guard checks prevent data corruption without requiring expensive synchronization primitives.

**Buffer sizing:** The capacity of 32 bytes (defined as [`RINGBUF_SIZE`](../../src/common/lib/ringbuf.h)) is deliberately small. The main loop polls frequently enough that scancodes move through the buffer within microseconds. Even multi-byte sequences rarely accumulate more than a few bytes before consumption. The small size keeps the buffer in cache and makes overflow immediately apparent rather than allowing problems to hide in deep queues.

**Critical rule:** Never call `ringbuf_reset()` with interrupts enabled. This function resets both head and tail pointers to zero, which corrupts buffer state if an interrupt arrives during the reset operation. Only call this function during initialization before enabling interrupts, or after explicitly disabling interrupts for error recovery. The implementation appears in [`src/common/lib/ringbuf.c`](../../src/common/lib/ringbuf.c) and [`ringbuf.h`](../../src/common/lib/ringbuf.h).

---

## Related Documentation

**Advanced Topics:**
- [Performance Characteristics](performance.md) - Timing, throughput, and resource utilization
- [Build System](build-system.md) - CMake configuration and Docker builds
- [Testing and Validation](testing.md) - Hardware testing and code quality

**Protocol Details:**
- [Protocols Overview](../protocols/README.md) - Protocol specifications and timing
- [Scancode Sets](../scancodes/README.md) - Scancode decoding and multi-byte sequences

**Development:**
- [Code Standards](../development/code-standards.md) - Coding conventions and architecture rules
- [Contributing Guide](../development/contributing.md) - How to contribute changes
- [Adding Keyboards](../development/adding-keyboards.md) - Creating new keyboard configurations

**Features:**
- [Command Mode](../features/command-mode.md) - Firmware management interface
- [Logging](../features/logging.md) - Debug output configuration
- [USB HID](../features/usb-hid.md) - USB interface details

**Source Code:**
- Main loop: [`src/main.c`](../../src/main.c)
- Ring buffer: [`src/common/lib/ringbuf.c`](../../src/common/lib/ringbuf.c)
- PIO helper: [`src/common/lib/pio_helper.c`](../../src/common/lib/pio_helper.c)
- HID interface: [`src/common/lib/hid_interface.c`](../../src/common/lib/hid_interface.c)
- Protocols: [`src/protocols/`](../../src/protocols/)
- Keyboards: [`src/keyboards/`](../../src/keyboards/)

---

**Questions or stuck on something?**  
Pop into [GitHub Discussions](https://github.com/PaulW/rp2040-keyboard-converter/discussions) or [report a bug](https://github.com/PaulW/rp2040-keyboard-converter/issues).
