# Flow Tracking

Flow tracking is an optional developer feature that timestamps the keyboard processing pipeline so you can see how long the converter spends at each stage between a key being pressed and the USB report going out. When it's compiled out (which is the default), there's no overhead at all—the compiler sees only empty inline stubs and discards them entirely at `-O2`. When it's compiled in, it starts dormant and you enable it at runtime through Command Mode.

I've written this as an advanced guide, so I'll assume you're comfortable reading UART output and have the build tools set up. See [Logging](../features/logging.md) and [Build System](build-system.md) if you need the basics first.

---

## What It Actually Measures

When a key is pressed, the converter works through several stages before the USB host receives anything—the PIO hardware fires an interrupt, the ISR writes to the ring buffer, the main loop picks it up, the scancode processor decodes it, the keymap translates it, the HID interface assembles a report, and TinyUSB sends it. Flow tracking puts a timestamp at each of those handoff points.

The output is a sequence of `LOG_DEBUG` lines in your UART terminal, one per stage, each with a microsecond timestamp from the RP2040 hardware timer. Subtracting the first timestamp from the last gives you the end-to-end firmware latency—the time from ISR entry to the USB report being handed off to TinyUSB.

All the instrumented stages are located in `src/common/lib/flow_tracker.[ch]`. The call sites are found in the protocol implementations under `src/protocols/`, the scancode processors under `src/scancodes/`, and the files `src/common/lib/hid_interface.c` and `src/common/lib/keymaps.c`.

The mouse path is not instrumented—it uses a streaming protocol that does not go through the ring buffer, so the token queue model does not apply there.

---

## Getting It Running

There are four things to do in order.

**First, enable the compile-time feature.** In `src/config.h`, find the developer features section and change:

```c
#define FLOW_TRACKING_ENABLED 0
```

to:

```c
#define FLOW_TRACKING_ENABLED 1
```

**Then rebuild.** Replace `modelm/enhanced` with whatever keyboard you're testing:

```bash
docker compose run --rm -e KEYBOARD="modelm/enhanced" builder
```

**Set the log level to DEBUG.** All trace output goes through `LOG_DEBUG`, so if the log level is lower than DEBUG none of it will appear. Use Command Mode ('D' then '3') to switch to DEBUG, or check the startup UART output to confirm the current level.

**Enable flow tracking at runtime.** From Command Mode, press 'T'. The UART will show `"Flow tracking enabled"`. Press 'T' again to turn it off. That's it—a trace will now appear for every keypress.

One thing worth knowing: if you try to enable tracking whilst the log level is lower than DEBUG, the converter won't allow it and shows `"Flow tracking: set log level to DEBUG first (D→3)"` instead. This stops you from enabling it silently with no output and wondering why nothing's appearing.

---

## Reading the Output

Each keypress produces a block of `LOG_DEBUG` lines:

```text
[FLOW] run=42
  [0] time=1234567  func=keyboard_irq_handler     data=0x1C
  [1] time=1234569  func=keyboard_interface_task  data=0x1C
  [2] time=1234570  func=process_scancode         data=0x1C
  [3] time=1234572  func=handle_keyboard_report   data=0x04
  [4] time=1234573  func=keymap_get_key_val       data=0x04
  [5] time=1234574  func=hid_keyboard_add_key     data=0x04
  [6] time=1234575  func=hid_send_report          data=0x01
```

`run=42` is a monotonic counter—it increments with every keypress so you can match make and break events up in the log. `time` is microseconds since boot from `timer_hw->timerawl`, the RP2040's 1MHz free-running hardware timer. `data` is whatever scancode, keycode, or report ID is in flight at that stage.

The end-to-end latency in this example is `1234575 − 1234567 = 8µs`. The Command Mode evaluation path is a bit longer—around 24µs (at the time of testing)—because `evaluate_command_mode()` calls `to_ms_since_boot()` (a 64-bit division) and emits a `LOG_INFO` message. That's expected behaviour, not a problem; 24µs is still well within the 8ms USB polling budget.

Break codes—the byte the keyboard sends when you *release* a key—don't always produce a HID report, so some keypresses won't have a complete trace. That's normal.

---

## Compile-Time vs Runtime Control

Flow tracking has two layers of control, and it's worth understanding why.

The compile-time flag (`FLOW_TRACKING_ENABLED` in `config.h`) controls whether the instrumentation code exists at all. When it's `0`, every call site expands to an empty inline stub or a `(void)` cast—the compiler generates no code at `-O2`. When it's `1`, the token queue, the step array, and all the timestamps are compiled in.

The runtime flag (toggled with Command Mode 'T') controls whether the ISR actually pushes tokens. This exists so you can build a debug firmware and leave it running normally until you want to investigate something—at which point you enable it from the keyboard without reflashing.

This state is not saved to flash. It always starts disabled after every power-up, regardless of what it was set to previously. The reason is deliberate: if tracking were enabled persistently and you powered the converter on without a serial terminal connected, the UART DMA queue would start filling immediately with trace output that nothing is reading. Keeping it session-only means you have to consciously turn it on each time, which is the right behaviour for a diagnostic tool.

---

## How It Works Internally

I'll keep this fairly brief, but it's worth understanding the concurrency model if you're adding instrumentation to a new protocol.

The ISR and the main loop run at different priority levels and can't safely share state without synchronisation. The ARM Cortex-M0+ doesn't have hardware locks, and disabling interrupts in the main processing path isn't an option.

Flow tracking uses a lock-free single-producer / single-consumer ring buffer—a token queue—that mirrors the existing keyboard ring buffer. The ISR is the sole producer and writes to `token_head`. The main loop is the sole consumer and reads from `token_tail`. Because they operate on opposite ends of the queue, no locking is needed; `__dmb()` memory barriers ensure the writes are visible when the other side reads.

This queue has to stay in strict lockstep with the keyboard ring buffer: a token is pushed only inside the `!ringbuf_is_full()` guard on the same code path as `ringbuf_put()`, and consumed only immediately after `ringbuf_get()`. If those two get out of sync, the timestamps end up attached to the wrong scancodes.

Timestamps come from `timer_hw->timerawl`, which is the RP2040's 1MHz free-running hardware timer. It's a single register read, independent of the CPU clock, which makes it suitable for this kind of lightweight instrumentation.

The runtime enable flag (`flow_tracking_runtime_enabled`) is `volatile bool`. The ISR checks it before pushing any token, so turning tracking off takes effect at the next interrupt. A `__dmb()` barrier in `flow_tracker_set_enabled()` ensures the updated flag is visible to the ISR before the function returns.

---

## Adding Instrumentation to a New Protocol

If you're implementing a new keyboard protocol and want it to participate in flow tracking, have a look at `src/protocols/xt/keyboard_interface.c` for a working example. The pattern is the same for every protocol:

In the ISR, immediately after a successful `ringbuf_put()`:

```c
if (!ringbuf_is_full()) {
    ringbuf_put(data_byte);
    isr_push_flow_token(__func__, data_byte);
} else {
    LOG_WARN("Ring buffer full, dropping scancode\n");
}
```

In `keyboard_interface_task()`, immediately after `ringbuf_get()`:

```c
uint8_t scancode = ringbuf_get();
flow_token_t flow_tok;
if (main_pop_flow_token(&flow_tok)) {
    flow_start(&flow_tok);
}
process_scancode(scancode);
```

In your scancode processor's `process_scancode()`, as the first statement:

```c
void process_scancode(uint8_t code) {
    FLOW_STEP(code);
    // ... rest of function
}
```

The `isr_push_flow_token()` call must be inside the `!ringbuf_is_full()` guard. If the ring buffer was full and you skipped the `ringbuf_put()`, you must also skip the push—otherwise the two queues diverge. Don't forget `#include "flow_tracker.h"` in both files.

---

## Troubleshooting

**No `[FLOW]` output at all?** Check that `FLOW_TRACKING_ENABLED` is `1` in `config.h` and that you've rebuilt. Confirm the log level is DEBUG—if you're not seeing any `[DBG]`-prefixed messages in the UART at all, the level is too low. Then use Command Mode 'T' to enable tracking.

**Output stops after a few keypresses?** The UART DMA queue is dropping messages. Check that your terminal baud rate matches `UART_BAUD` in `config.h`.

**`step_count` stops at 8?** `FLOW_MAX_STEPS` in `flow_tracker.h` caps the number of recorded stages. If you've added extra instrumentation points and are hitting the limit, increase it. Keep `FLOW_TOKEN_QUEUE_SIZE` a power of 2 to match the queue sizing.
