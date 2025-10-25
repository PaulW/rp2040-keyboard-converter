# Testing Enforcement Tools

This guide explains how to test all enforcement mechanisms for the RP2040 Keyboard Converter project.

## Quick Test: Automated Test Suite

Run the automated test suite:

```bash
./tools/test-enforcement.sh
```

This tests:
- ✅ Lint script detection of violations
- ✅ Git hooks (pre-commit and commit-msg)
- ✅ CI checks simulation
- ✅ Multiple violation scenarios

**Expected Results**: All tests should pass, confirming tools work correctly.

---

## Manual Testing Guide

### 1. Test Lint Script

#### Test 1.1: Clean Run
```bash
./tools/lint.sh
```

**Current Expected Result**: 
- ❌ 1 error (sleep_us in uart.c - under review)
- ⚠️ 2 warnings (printf in IRQ, ringbuf_reset usage - both are correct/intentional)

#### Test 1.2: Strict Mode
```bash
./tools/lint.sh --strict
```

**Expected Result**: Fails due to warnings (strict mode treats warnings as errors)

#### Test 1.3: Create Violation and Test
```bash
# Create a test file with blocking operation
cat > src/test_violation.c << 'EOF'
#include "pico/stdlib.h"
void bad_function(void) {
    sleep_ms(100);  // Violation!
}
EOF

# Run lint (should detect violation)
./tools/lint.sh

# Clean up
rm src/test_violation.c
```

**Expected Result**: Lint script detects `sleep_ms()` and reports error

---

### 2. Test Git Hooks

#### Test 2.1: Commit Message Hook (docs-internal reference)
```bash
# This should FAIL (hook blocks it)
git commit --allow-empty -m "Add feature (see docs-internal/notes.md)"
```

**Expected Result**: ❌ Commit blocked with error message

#### Test 2.2: Commit Message Hook (clean message)
```bash
# This should SUCCEED
git commit --allow-empty -m "Add test feature"

# Clean up (undo commit)
git reset --soft HEAD~1
```

**Expected Result**: ✅ Commit allowed

#### Test 2.3: Pre-commit Hook (docs-internal files)
```bash
# Create test file
mkdir -p docs-internal
echo "test" > docs-internal/test.md

# Try to force add (note: .gitignore prevents normal staging)
git add -f docs-internal/test.md

# Try to commit (should FAIL at pre-commit)
git commit -m "test commit"

# Clean up
git reset HEAD docs-internal/test.md
rm docs-internal/test.md
```

**Expected Result**: ❌ Pre-commit hook blocks the commit

#### Test 2.4: Bypass Hooks (--no-verify)
```bash
# This SUCCEEDS but is discouraged
git commit --allow-empty -m "Test bypass" --no-verify

# Clean up
git reset --soft HEAD~1
```

**Expected Result**: ✅ Commit succeeds (bypass works) - but you should NEVER do this except for testing

---

### 3. Test Commit Template

#### Test 3.1: View Template
```bash
# Check if template is configured
git config commit.template

# View template contents
cat .gitmessage
```

**Expected Result**: Shows `.gitmessage` with docs-internal reminder

#### Test 3.2: Use Template
```bash
# Create commit using template (editor will open)
git commit --allow-empty

# You should see the template with reminders
# Cancel with :q! (Vim) or Ctrl+C
```

**Expected Result**: Commit message editor shows template with checklist

---

### 4. Test CI Checks (Simulate Locally)

#### Test 4.1: Check for docs-internal Files in Repo
```bash
# This simulates the CI check
if git ls-files | grep -q "^docs-internal/"; then
    echo "ERROR: docs-internal/ files found in repository!"
    git ls-files | grep "^docs-internal/"
    exit 1
else
    echo "✓ No docs-internal/ files in repository"
fi
```

**Expected Result**: ✅ No files found

#### Test 4.2: Build Matrix (Docker Required)
```bash
# Test one configuration
docker compose run --rm -e KEYBOARD="modelm/enhanced" builder

# Check build artifacts
ls -lh build/rp2040-converter.*
```

**Expected Result**: ✅ Build succeeds, artifacts created

---

### 5. Test PR Template

#### Test 5.1: View Template
```bash
cat .github/PULL_REQUEST_TEMPLATE.md
```

**Expected Result**: Shows comprehensive checklist

#### Test 5.2: Create Test PR (GitHub Web UI)
1. Create a new branch with changes
2. Push to GitHub
3. Open PR - template should auto-populate

**Expected Result**: PR description contains full checklist

---

## Test Scenarios by Violation Type

### Scenario 1: Blocking Operation Added

**Violation**:
```c
// In any source file
void my_function(void) {
    sleep_ms(50);  // BAD
}
```

**Detection Points**:
1. ❌ Lint script: `./tools/lint.sh` (detects immediately)
2. ❌ CI: Fails lint check in GitHub Actions
3. ⚠️ PR Review: Checklist reminds reviewer

---

### Scenario 2: Multicore API Used

**Violation**:
```c
#include "pico/multicore.h"

void bad_code(void) {
    multicore_launch_core1(core1_entry);  // BAD
}
```

**Detection Points**:
1. ❌ Lint script: `./tools/lint.sh` (detects immediately)
2. ❌ CI: Fails lint check
3. ⚠️ PR Review: Checklist item #2

---

### Scenario 3: docs-internal Reference in Commit

**Violation**:
```bash
git commit -m "Fix bug (see docs-internal/analysis.md)"
```

**Detection Points**:
1. ❌ Git hook: commit-msg blocks immediately
2. ❌ CI: Checks commit messages in PR
3. ⚠️ Commit template: Reminder visible

---

### Scenario 4: docs-internal File Committed

**Violation**:
```bash
git add -f docs-internal/notes.md  # Force add
git commit -m "Add notes"
```

**Detection Points**:
1. ❌ Git hook: pre-commit blocks before commit
2. ❌ CI: Checks for docs-internal/ files in repo
3. 🛡️ .gitignore: Prevents accidental staging

---

## Verification Checklist

Before considering enforcement tools working:

- [ ] Lint script detects all 7 violation types
- [ ] Lint script --strict fails on warnings
- [ ] Pre-commit hook blocks docs-internal files
- [ ] Commit-msg hook blocks docs-internal references
- [ ] Commit-msg hook allows clean messages
- [ ] Commit template shows reminders
- [ ] CI workflow file exists (`.github/workflows/ci.yml`)
- [ ] PR template exists (`.github/PULL_REQUEST_TEMPLATE.md`)
- [ ] Hooks are executable (`ls -la .githooks/`)
- [ ] Hooks path configured (`git config core.hooksPath` = `.githooks`)

Run full verification:
```bash
# Check all at once
[ -x .githooks/pre-commit ] && \
[ -x .githooks/commit-msg ] && \
[ -x tools/lint.sh ] && \
[ -f .github/workflows/ci.yml ] && \
[ -f .github/PULL_REQUEST_TEMPLATE.md ] && \
[ "$(git config core.hooksPath)" = ".githooks" ] && \
echo "✅ All enforcement tools configured correctly" || \
echo "❌ Some tools not configured"
```

---

## Troubleshooting

### Hooks Not Running

**Problem**: Commits go through without checks

**Solutions**:
```bash
# 1. Check hooks path
git config core.hooksPath
# Should output: .githooks

# 2. Set if not configured
git config core.hooksPath .githooks

# 3. Check permissions
ls -la .githooks/
# Should show -rwxr-xr-x (executable)

# 4. Make executable if needed
chmod +x .githooks/*
```

### Lint Script Fails on Known Issues

**Problem**: Current codebase has `sleep_us()` in uart.c

**Solution**: This is under review. For now:
- Local development: Run without `--strict`
- CI: Configured to handle known exceptions
- Issue tracked in todo list (#9)

### Can't Test PR Template

**Problem**: Need real PR to test template

**Solution**:
1. Create test branch locally
2. Push to GitHub
3. Open PR to see template
4. Close PR without merging
5. Delete branch

---

## Continuous Testing

### Daily Development

```bash
# Before each commit
./tools/lint.sh

# After adding new code
./tools/lint.sh --strict
```

### Before PR

```bash
# Full check
./tools/test-enforcement.sh

# Manual verification
git log --oneline -5  # Check no docs-internal in recent commits
git status           # Check no docs-internal staged
./tools/lint.sh      # Check no new violations
```

### After Merging

```bash
# On main branch
./tools/lint.sh --strict
# Should pass or have only known issues
```

---

## Expected Test Results Summary

| Test | Expected Result | Current Status |
|------|----------------|----------------|
| Lint clean code | Pass with known warnings | ✅ Working |
| Lint detect blocking ops | Fail (detects violation) | ✅ Working |
| Lint detect multicore | Fail (detects violation) | ✅ Working |
| Lint strict mode | Fail on warnings | ✅ Working |
| Pre-commit blocks docs-internal | Fail (blocks commit) | ✅ Working |
| Commit-msg blocks docs-internal ref | Fail (blocks commit) | ✅ Working |
| Commit-msg allows clean message | Pass (allows commit) | ✅ Working |
| CI docs-internal check | Pass (no files found) | ✅ Working |
| Build matrix | Pass (all configs build) | 🔄 Needs GitHub Actions |
| Memory guards | Pass (within limits) | 🔄 Needs GitHub Actions |

## Notes

- ✅ = Tested and working locally
- 🔄 = Requires GitHub Actions to test fully
- ⚠️ = Known issue being tracked

---

## Adding New Tests

When adding enforcement rules:

1. Add check to `tools/lint.sh`
2. Add test case to `tools/test-enforcement.sh`
3. Update this testing guide
4. Update `.github/PULL_REQUEST_TEMPLATE.md`
5. Document in `.github/copilot-instructions.md`
