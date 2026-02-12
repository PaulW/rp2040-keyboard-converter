# Quick Test Commands

## One-Line Tests

```bash
# Test everything
./tools/test-enforcement.sh

# Test lint only
./tools/lint.sh

# Test lint strictly (fail on warnings)
./tools/lint.sh --strict

# Test commit message hook
git commit --allow-empty -m "Test commit"

# Verify hooks configured
git config core.hooksPath && ls -la .githooks/*.sh 2>/dev/null || ls -la .githooks/pre-commit .githooks/commit-msg

# Check for docs-internal in repo
git ls-files | grep "^docs-internal/" && echo "ERROR" || echo "OK"

# Verify all tools present
[ -x .githooks/pre-commit ] && [ -x .githooks/commit-msg ] && [ -x tools/lint.sh ] && echo "✅ All tools present" || echo "❌ Missing tools"
```

## Test Violation Detection

```bash
# Create test file with violations
cat > src/test.c << 'EOF'
#include "pico/stdlib.h"
void test(void) { sleep_ms(100); }
EOF

# Run lint (should fail)
./tools/lint.sh

# Clean up
rm src/test.c
```

## Test docs-internal Protection

```bash
# Try to commit reference (should fail)
git commit --allow-empty -m "See docs-internal/notes"

# Try to stage file (should fail)
echo "test" > docs-internal/test.md
git add -f docs-internal/test.md
git commit -m "test"
git reset HEAD docs-internal/test.md
rm docs-internal/test.md
```

## Before Every Commit

```bash
./tools/lint.sh && echo "✅ Ready to commit" || echo "❌ Fix violations first"
```

## Before Every PR

```bash
# Run the same checks as CI
./tools/lint.sh --strict && echo "✅ Ready for PR" || echo "❌ Fix violations first"
```

## Testing the Enforcement Tools (Optional)

Only run this if you're modifying lint.sh, git hooks, or other enforcement tools:

```bash
./tools/test-enforcement.sh
```

This meta-tests the enforcement tools themselves—not needed for regular development.

## See Full Testing Guide

```bash
cat tools/TESTING.md
# or
less tools/TESTING.md
```
