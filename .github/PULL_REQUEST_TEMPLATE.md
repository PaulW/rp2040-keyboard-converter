## Summary
<!-- Brief description of what this PR does -->

## Type of Change
<!-- Mark relevant items with [x] -->
- [ ] Bug fix (non-breaking change fixing an issue)
- [ ] New feature (non-breaking change adding functionality)
- [ ] Breaking change (fix or feature causing existing functionality to change)
- [ ] Documentation update
- [ ] Refactoring (no functional changes)
- [ ] Build/CI changes

## Architecture Compliance Checklist
<!-- ALL items must be checked before merge -->

### 🔴 Critical Rules (MUST be verified)
- [ ] **No blocking operations** (`sleep_ms`, `sleep_us`, `busy_wait_us`) in any code path
- [ ] **No multicore APIs** (`multicore_launch_core1`, `multicore_fifo_*`, Core 1 usage)
- [ ] **Single-core only** - all code runs on Core 0
- [ ] **No `printf()` in IRQ context** (use `LOG_*` macros instead, or defer to main loop)
- [ ] **Ring buffer pattern maintained** - IRQ writes only, main loop reads only

### 🟡 Important Rules (Should be verified)
- [ ] **IRQ-shared variables use `volatile`** keyword
- [ ] **Memory barriers (`__dmb()`)** after volatile reads/writes in IRQ/main communication
- [ ] **`tud_hid_ready()` checked** before sending HID reports
- [ ] **`ringbuf_reset()` only called** with IRQs disabled (init/reset paths only)
- [ ] **No direct Flash execution** - code runs from RAM (`pico_set_binary_type(copy_to_ram)`)

## Testing Checklist
<!-- Mark completed items with [x] -->
- [ ] **Build tested** with at least one keyboard configuration
- [ ] **Lint script passed** (`./tools/lint.sh`)
- [ ] **No new compiler warnings** introduced
- [ ] **Hardware tested** (if protocol/hardware changes)
- [ ] **Memory usage checked** (Flash < 230KB, RAM < 150KB ideally)

## Build Configuration Tested
<!-- Which configurations were built/tested? -->
```bash
# Example:
docker compose run --rm -e KEYBOARD="modelm/enhanced" builder
docker compose run --rm -e KEYBOARD="modelm/enhanced" -e MOUSE="at-ps2" builder
```

**Configurations tested:**
- [ ] Keyboard-only: ________________
- [ ] Mouse-only: ________________
- [ ] Keyboard + Mouse: ________________

## Documentation
<!-- Mark relevant items with [x] -->
- [ ] **Public documentation updated** (README.md, protocol READMEs, etc.)
- [ ] **Code comments added/updated** for complex logic
- [ ] **No references to `docs-internal/`** in commit messages or documentation
- [ ] **No `docs-internal/` files staged** for commit

## Performance Impact
<!-- If applicable, describe any performance implications -->
- **CPU usage**: <!-- No change / Increased / Decreased -->
- **Memory usage**: <!-- No change / Increased / Decreased -->
- **Latency impact**: <!-- No change / Improved / Regressed -->

## Breaking Changes
<!-- If this is a breaking change, describe what breaks and migration path -->

## Additional Notes
<!-- Any other context, concerns, or follow-up items -->

---

## Reviewer Checklist
<!-- For maintainers reviewing this PR -->
- [ ] Architecture rules verified against code changes
- [ ] No docs-internal files or references in PR
- [ ] Build matrix passed in CI
- [ ] Lint checks passed
- [ ] Code style consistent with existing codebase
- [ ] Changes align with project goals and copilot-instructions.md

**Review policy**: See `.github/copilot-instructions.md` for complete architecture rules and anti-patterns.
