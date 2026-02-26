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
3. Ask user for: blocking calls, ring buffer changes, PIO edits, IRQ handler changes
4. Run `./tools/lint.sh` locally for code changes (only tool not in Docker)
5. Verify facts, don't assume - never claim unverified performance metrics
6. British English throughout project (code, comments, docs)
7. Docs: conversational human-written style (see `docs/getting-started/`), no idioms/marketing/assumptions

### Pre-Commit Checklist:

- [ ] Run `./tools/lint.sh` (0 errors, 0 warnings)
- [ ] No blocking ops (or `LINT:ALLOW blocking` + IRQ guard + justification)
- [ ] Protocol changes: Read README.md, verify IRQ non-blocking, test hardware
- [ ] Ring buffer: Single producer/consumer maintained, no reset with IRQs enabled
- [ ] HID: Check `tud_hid_ready()` before sending
- [ ] Time-based: Use `to_ms_since_boot(get_absolute_time())`, never sleep
- [ ] Build test: `docker compose run --rm -e KEYBOARD="<config>" builder`
- [ ] No docs-internal/ staged/referenced

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

## File Organization

**Protocols:** `src/protocols/<name>/` → `.pio`, `keyboard_interface.[ch]`, `common_interface.c`, `README.md`
**Keyboards:** `src/keyboards/<brand>/<model>/` → `keyboard.config`, `keyboard.h`, `keyboard.c`, `README.md`
**Scancodes:** `src/scancodes/<set>/` → `scancode_processor.[ch]`, `README.md`
**Common:** `src/common/lib/` → `ringbuf`, `hid_interface`, `command_mode`, `pio_helper`, `uart`

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

**Docker Workflow:** ALL builds and compilation via Docker. Only `./tools/lint.sh` runs locally.

**Tools:** `./tools/lint.sh` (local pre-commit), Git hooks (docs-internal block), CI (lint + build matrix)

**Documentation:**

- **Public:** All in git (README, docs/, src/)
- **Private:** docs-internal/ — use freely for development context, NEVER commit/reference publicly

**Documentation Style (match existing docs in `docs/getting-started/`):**

- **Conversational, human-written**: First-person narrative ("I've set this up...", "I know it might seem...", "Have a look at...")
- **Explain WHY, not just WHAT**: Context before instructions ("Why Docker? It packages all the complex build tools...")
- **British English naturally**: whilst, colour, initialise, behaviour, lying about, mucking about
- **Direct, helpful tone**: "That's it!", "This is important:", parenthetical clarifications
- **100% factual**: Verify all technical claims against codebase, point to directories (don't enumerate protocols/features)
- ❌ **NEVER use**: Marketing language ("pleased", "convenient", "powerful"), idioms, assumptions, suggestive remarks, excessive bullets without explanation
- ✅ **Example refs**: "Check `src/protocols/` for available implementations" (NOT "supports AT/PS2, XT, Amiga...")

---

## Code Standards

- **C11 standard** - strict compliance required
- **British English** - code, comments, docs use British spelling (colour, initialise, behaviour)
- Indentation: 4 spaces, no tabs; line length target 100 chars
- Naming: `snake_case` (vars/funcs), `UPPER_CASE` (macros/consts), `snake_case_t` (structs/enums)
- IRQ handlers: must use `__isr` attribute — `void __isr keyboard_irq_handler(void)`
- Every new `.c`/`.h` file: must include the GPL license header (see existing files for template)
- Doxygen for all functions: include `@brief`, `@param`, `@return`, threading context (`@note IRQ-safe` / `@note Main loop only`), blocking behaviour
- `.clang-format` present in repo root (Google style, 4-space indent, 100 col limit) — use for editor formatting; not run by CI
- Follow PicoSDK/TinyUSB patterns
- Run `./tools/lint.sh` locally before commits (everything else via Docker)

**Repo Structure:**

```text
src/
  ├── common/      # Libraries, utilities
  ├── keyboards/   # Configurations, layouts
  ├── protocols/   # Protocol implementations
  ├── scancodes/   # Scancode mappings
  └── main.c
```

---

## AI Guidelines

**Validation sources:**

- Source code (ultimate truth)
- Pico SDK docs (hardware)
- docs-internal/ (decisions, context - use freely, never commit)
- Protocol/keyboard READMEs (specs)

**Behavioural rules:**

- Verify files exist before referencing; ask if unclear, don't assume
- Preserve existing logic unless explicitly asked to change it
- Plan approach before implementing
- Never claim unverified performance metrics
