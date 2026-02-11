# Testing Enforcement Tools

This guide covers testing the enforcement mechanisms themselves—you'll need this when modifying lint.sh, git hooks, or CI checks. For regular development, just run `./tools/lint.sh` before committing (see [QUICKTEST.md](QUICKTEST.md)).

## Automated Test Suite

The meta-testing suite validates that enforcement tools detect violations correctly:

```bash
./tools/test-enforcement.sh
```

This runs through multiple scenarios checking that the lint script catches violations, git hooks block problematic commits, and CI checks function correctly. You'll only need this when modifying the enforcement tools—not for regular keyboard development or bug fixes.

All tests should pass, confirming the enforcement tools work as expected.

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

## Common Violation Scenarios

These scenarios show how different violations get caught at various stages:

**Adding blocking operations:**

If you add `sleep_ms(50)` to any source file, the lint script will catch it immediately when you run `./tools/lint.sh`. CI will also fail the lint check in GitHub Actions, and the PR checklist reminds reviewers to watch for blocking operations.

**Using multicore APIs:**

Code like `multicore_launch_core1(core1_entry)` gets detected by the same mechanisms—lint script locally, CI check remotely, and PR checklist review.

**Referencing docs-internal in commits:**

A commit message like "Fix bug (see docs-internal/analysis.md)" won't make it past the commit-msg hook—it blocks immediately. CI also checks commit messages in PRs, and the commit template shows a reminder about this.

**Committing docs-internal files:**

If you force-add a docs-internal file with `git add -f docs-internal/notes.md`, the pre-commit hook blocks the commit before it happens. CI also checks for docs-internal files in the repository, and .gitignore prevents accidental staging in the first place.

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

**Hooks not running:**

If commits go through without any checks, the hooks path probably isn't configured. Check with `git config core.hooksPath`—it should output `.githooks`. If it doesn't, set it with `git config core.hooksPath .githooks`. Also check the hooks are executable with `ls -la .githooks/`—you should see `-rwxr-xr-x` permissions. If not, run `chmod +x .githooks/*`.

**Lint script fails on known issues:**

The current codebase has a known `sleep_us()` call in uart.c that's under review. For local development, run without `--strict`. CI is configured to handle this exception. This is tracked as issue #9.

**Can't test PR template:**

You need an actual PR to see the template in action. Create a test branch locally, push to GitHub, open a PR to see the template, then close without merging and delete the branch.

---

## Daily Usage

Before each commit, run the lint script to catch violations early:

```bash
./tools/lint.sh
```

Before submitting a PR, run strict mode to match what CI will check:

```bash
./tools/lint.sh --strict
```

Also verify recent commits don't reference docs-internal and nothing's staged from that directory:

```bash
git log --oneline -5  # Check recent commit messages
git status           # Check staged files
```

---

## Adding New Enforcement Rules

When you add a new architectural rule that needs enforcement:

1. Add the check logic to `lint.sh`
2. Add test cases to `test-enforcement.sh` that verify the check catches violations
3. Update this testing guide with the new scenarios
4. Update `.github/PULL_REQUEST_TEMPLATE.md` with relevant checklist items
5. Document the rule in `.github/copilot-instructions.md`
