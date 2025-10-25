# Git Hooks for RP2040 Keyboard Converter

This directory contains custom git hooks to enforce project policies.

## Setup (Required for Contributors)

**One-time setup** - Configure git to use these hooks:

```bash
# Set hooks directory (run from repository root)
git config core.hooksPath .githooks

# Make hooks executable (Linux/macOS)
chmod +x .githooks/*
```

## Available Hooks

### `pre-commit`
Runs before `git commit` completes. Checks for:
- ✅ No `docs-internal/` files staged for commit
- ✅ Warning if `docs-internal/` not in `.gitignore`
- ⚠️ Early warning about `docs-internal` references (if commit message prepared)

**Blocks commit** if docs-internal files are staged.

### `commit-msg`
Runs after commit message is written but before commit is created. Checks for:
- ✅ No `docs-internal` references in commit message (case-insensitive)

**Blocks commit** if commit message contains "docs-internal".

## Policy Enforcement

These hooks enforce the documentation policy from `.github/copilot-instructions.md`:

> ⚠️ **NEVER commit `docs-internal/` files**: These are local-only documentation. Never stage, commit, or push them to git.
> 
> ⚠️ **NEVER reference `docs-internal/` files in any Public documentation**: These files are for internal use only and should not be exposed externally.

**Commit messages are PUBLIC documentation** - they are visible in:
- Git history (`git log`)
- GitHub web interface
- Pull requests
- Release notes

## Troubleshooting

### Hook not running?
```bash
# Check hooks directory configuration
git config core.hooksPath
# Should output: .githooks

# Check hook is executable
ls -la .githooks/
# Should show: -rwxr-xr-x (executable permission)
```

### Need to bypass hook temporarily?
```bash
# Use --no-verify flag (use with caution!)
git commit --no-verify -m "..."
```

**Warning**: Only bypass hooks if you're absolutely certain the commit is safe. The hooks exist to prevent policy violations.

## Commit Message Template

A commit message template (`.gitmessage`) is available with built-in reminders:

```bash
# Set commit message template (optional)
git config commit.template .gitmessage
```

This template includes:
- Reminder about docs-internal policy
- Commit message format guidelines
- Pre-commit checklist

## Testing the Hooks

```bash
# Test 1: Try staging a docs-internal file (should fail)
touch docs-internal/test.md
git add docs-internal/test.md
git commit -m "test"
# Expected: ERROR - pre-commit hook blocks

# Test 2: Try referencing docs-internal in message (should fail)
git commit -m "Add feature (documented in docs-internal/)"
# Expected: ERROR - commit-msg hook blocks

# Clean up test
git reset HEAD docs-internal/test.md
rm docs-internal/test.md
```

## For AI Assistants

When helping with commits:
1. **Never** include "docs-internal" in commit messages
2. **Never** stage files from `docs-internal/` directory
3. **Always** check that hooks are configured before committing
4. If a hook blocks a commit, fix the issue - don't suggest bypassing

The hooks are there to enforce project policies automatically.
