# RP2040 Keyboard Converter - AI Instructions

Embedded firmware converting keyboard/mouse protocols to USB HID using RP2040's PIO. **Single-core only** (Core 0), runs from SRAM.

**Documentation:** Check `docs/` for detailed guides (development, features, protocols, keyboards) when needed.

---

## 🚨 CRITICAL RULES

### Never Do:

1. ❌ Blocking operations (`sleep_ms`, `sleep_us`, `busy_wait_*`) - except `LINT:ALLOW blocking` + IRQ guard
2. ❌ Multicore APIs (`multicore_*`, `core1_*`) - single-core only
3. ❌ `printf`/`fprintf`/`sprintf` in IRQ - use `LOG_*` macros
4. ❌ `ringbuf_reset()` with IRQs enabled - except `LINT:ALLOW ringbuf_reset`
5. ❌ Flash execution - RAM only (`pico_set_binary_type(copy_to_ram)`)
6. ❌ Commit/reference docs-internal/ files

### Architecture:

- **Non-blocking**: Use `to_ms_since_boot(get_absolute_time())`, never sleep
- **IRQ Safety**: IRQ writes ring buffer, main loop reads. Use `volatile` + `__dmb()`
- **USB**: Check `tud_hid_ready()` before sending (8ms polling)
- **Platform**: RP2040 125MHz, 32-byte ring buffer

### Before Every Response:

1. Read relevant sections completely
2. Check for Critical Rule violations
3. Check for Code Standards violations — refer to `docs/development/code-standards.md`
4. Ask user for: blocking calls, ring buffer changes, PIO edits, IRQ handler changes
5. Run `./tools/lint.sh` locally for code changes (only tool not in Docker)
6. Verify facts, don't assume - never claim unverified performance metrics
7. British English throughout project (code, comments, docs)
8. Docs: conversational human-written style (see `docs/getting-started/`), no idioms/marketing/assumptions

### Pre-Commit Checklist:

- [ ] Run `./tools/lint.sh` (0 errors, 0 warnings)
- [ ] No blocking ops (or `LINT:ALLOW blocking` + IRQ guard + justification)
- [ ] Protocol changes: Read README.md, verify IRQ non-blocking, test hardware
- [ ] Ring buffer: Single producer/consumer maintained, no reset with IRQs enabled
- [ ] HID: Check `tud_hid_ready()` before sending
- [ ] Time-based: Use `to_ms_since_boot(get_absolute_time())`, never sleep
- [ ] Build test: `docker compose run --rm -e KEYBOARD="<config>" builder`
- [ ] No docs-internal/ staged/referenced
- [ ] Code Standards: verify against `docs/development/code-standards.md`

### Guard Analysis:

Before suggesting guards, trace complete call chain. Helper functions (`find_*()`, `claim_*()`) often centralize validation. Check return values and existing patterns across protocols.

---

## Quick Reference

**Platform:** RP2040 125MHz, 32B ring buffer | **Build:** Docker-based (all compilation via Docker)

**Build:**

```bash
docker compose run --rm -e KEYBOARD="modelm/enhanced" builder
docker compose run --rm -e KEYBOARD="modelf/pcat" -e MOUSE="at-ps2" builder  # Mouse optional
```

**Note:** Build process outputs RAM utilisation metrics on completion. Monitor these against documented limits.

**Mouse:** AT/PS2 only, separate CLK/DATA pins, works with any keyboard protocol

---

## Data Flow

```text
PIO (CLK/DATA) → IRQ → Ring Buffer (32B) → Main Loop → Protocol → Scancode → Keymap → HID → USB
```

---

## Critical Patterns

**Non-Blocking State Machine:**

```c
static uint32_t state_time = 0;
static enum { IDLE, WAITING, READY } state = IDLE;

void task(void) {
  uint32_t now = to_ms_since_boot(get_absolute_time());
  if (state == WAITING && now - state_time >= TIMEOUT_MS) state = READY;
}
```

**Ring Buffer (Producer/Consumer):**

```c
void __isr irq_handler(void) { if (!ringbuf_is_full()) ringbuf_put(data); }  // IRQ writes
void task(void) { if (!ringbuf_is_empty() && tud_hid_ready()) process(ringbuf_get()); }  // Main reads
```

**IRQ Communication:**

```c
static volatile uint8_t flag = 0;
void __isr irq(void) { flag = 1; __dmb(); }
void task(void) { if (flag) { __dmb(); flag = 0; process(); } }
```

---

## Keyboard-Specific Traps

**Command Mode Keys:** Override in `keyboard.h` before includes (defaults: L/R Shift):

```c
#define CMD_MODE_KEY1 KC_LSHIFT
#define CMD_MODE_KEY2 KC_LALT  // Override for single-shift keyboards
#include "hid_keycodes.h"
```

**Shift-Override:** For non-standard shift legends, define `keymap_shift_override_layers[]` in `keyboard.c`:

```c
const uint8_t * const keymap_shift_override_layers[KEYMAP_MAX_LAYERS] = {
    [0] = (const uint8_t[SHIFT_OVERRIDE_ARRAY_SIZE]){
        [KC_2] = SUPPRESS_SHIFT | KC_DQUO,  // Shift+2 → " (suppress shift)
    },
};
```

- `SUPPRESS_SHIFT` (0x80): Removes shift modifier
- Only map non-standard keys, rest default to 0
- Per-layer support, NULL layers use standard behaviour
- Disabled by default, enable via Command Mode
- See IBM M122 for example

**NumLock:** Don't create NumLock layers. Numpad keys (KC_P0-P9) are HID dual-function.

**Layer Persistence:** TG/TO states persist via config storage v3 (dual-hash validation). MO/OSL are temporary. Override `keymap_layer_count` to match layer count for bounds checking.

**Debugging:**

- UART (GPIO 0/1): `printf()` uses DMA (non-blocking). Look for `[ERR]`, `[DBG]`, `[INFO]`, `!INIT!` patterns
- Build output: Check RAM utilisation metrics (documented limits exist)

---

## Troubleshooting

**Keys not registering?** Ring buffer full (check `tud_hid_ready()`), protocol errors (UART `[ERR]`/`!INIT!`), wrong keymap (verify KEYBOARD_CODESET)

**Build fails?** Missing KEYBOARD env var, missing keyboard.config, wrong KEYBOARD_PROTOCOL

**High CPU/latency?** Blocking operations (sleep/busy_wait), long loops without yield

---

## Build & Enforcement

**Docker Workflow:** ALL builds via Docker. `./tools/lint.sh` local pre-commit only. Git hooks block docs-internal commits. CI runs lint + build matrix.

**Documentation style** (match `docs/getting-started/`):

- **Conversational, human-written**: First-person narrative ("I've set this up...", "I know it might seem...", "Have a look at...")
- **Explain WHY, not just WHAT**: Context before instructions ("Why Docker? It packages all the complex build tools...")
- **British English naturally**: whilst, colour, initialise, behaviour, lying about, mucking about
- **Direct, helpful tone**: "That's it!", "This is important:", parenthetical clarifications
- **100% factual**: Verify all technical claims against codebase, point to directories (don't enumerate protocols/features)
- ❌ **NEVER use**: Marketing language ("pleased", "convenient", "powerful"), idioms, assumptions, suggestive remarks, excessive bullets without explanation
- ✅ **Example refs**: "Check `src/protocols/` for available implementations" (NOT "supports AT/PS2, XT, Amiga...")

---

## Code Standards

- **C11 standard** — strict compliance required
- **British English** — code, comments, docs use British spelling (colour, initialise, behaviour)
- Indentation: 4 spaces, no tabs; line length target 100 chars
- Naming: `snake_case` (vars/funcs), `UPPER_CASE` (macros/consts), `snake_case_t` (structs/enums)
- IRQ handlers: must use `__isr` attribute — `void __isr keyboard_irq_handler(void)`
- Every new `.c`/`.h` file: must include the GPL license header (see existing files for template)
- Doxygen for all functions: include `@brief`, `@param`, `@return`, threading context (`@note IRQ-safe` / `@note Main loop only`), blocking behaviour
- `.clang-format` present in repo root (Google style, 4-space indent, 100 col limit) — use for editor formatting; not run by CI
- Follow PicoSDK/TinyUSB patterns

`./tools/lint.sh` enforces 19 checks. Zero tolerance for errors or warnings.

For thorough compliance reference — naming, formatting, Doxygen, IRQ safety, include ordering, file structure, LINT:ALLOW annotations — see `docs/development/code-standards.md`.

**Repo Structure:**

```text
src/
  ├── board_config/    # Hardware board header (keyboard_converter_board.h)
  ├── cmake_includes/  # CMake build fragments (common, keyboard, mouse, flags)
  ├── common/
  │   └── lib/         # Shared libraries: ringbuf, hid_interface, command_mode,
  │                    #   keylayers, keymaps, config_storage, pio_helper, uart,
  │                    #   led_helper, log, flow_tracker, ram_check, ws2812, types
  ├── keyboards/       # Per-keyboard configs and keymaps (<brand>/<model>/)
  ├── protocols/       # Protocol implementations (at-ps2, xt, amiga, apple-m0110)
  ├── scancodes/       # Scancode sets (set1, set2, set3, set123, amiga, apple-m0110)
  ├── config.h         # Top-level build configuration
  └── main.c
```

---

## AI Guidelines

**Validation sources (in priority order):**

1. Source code — ultimate truth for this project
2. Pico SDK / TinyUSB / hardware datasheets — verifiable online docs
3. Protocol/keyboard READMEs — specs and timings
4. docs-internal/ — working notes, spike findings, and development context only; treat as potentially incomplete or out of date, not authoritative; never commit or reference publicly
5. Training data — **last resort only**; if used, explicitly tell the user and ask them to confirm before acting

**Never use training data as a primary source.** Always verify technical claims (hardware behaviour, timings, API signatures, protocol specs) against 100% verifiable online sources. If no verifiable source can be found, say so and ask the user before proceeding.

**Behavioural rules:**

- Verify files exist before referencing; ask if unclear, don't assume
- Preserve existing logic unless explicitly asked to change it
- Plan approach before implementing
- Never claim unverified performance metrics
