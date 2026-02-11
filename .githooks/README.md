# Git Hooks

These hooks enforce the project's documentation policy—specifically preventing docs-internal files from being committed and stopping references to them in commit messages.

## Setup

Before you can commit, configure git to use these hooks:

```bash
# Set hooks directory (run from repository root)
git config core.hooksPath .githooks

# Make hooks executable (Linux/macOS)
chmod +x .githooks/*
```

## What They Do

**pre-commit** runs before the commit completes. It checks whether you've staged any files from docs-internal and blocks the commit if it finds any. It also warns if docs-internal isn't in .gitignore, and gives an early warning if your commit message (when using `git commit -m`) mentions docs-internal.

**commit-msg** runs after you've written the commit message but before the commit is created. It scans the message for any references to "docs-internal" (case-insensitive) and blocks the commit if it finds them.

## Why This Matters

The documentation policy in `.github/copilot-instructions.md` states that docs-internal files must never be committed—they're local-only development notes. Similarly, you can't reference them in public documentation, which includes commit messages.

Commit messages are public. They show up in git history, the GitHub web interface, pull requests, and release notes. References to docs-internal in commit messages would expose information that's meant to stay local.

## Troubleshooting

**Hook not running:**

Check the hooks directory is configured: `git config core.hooksPath` should output `.githooks`. If it doesn't, set it with the setup command above.

Check the hooks are executable: `ls -la .githooks/` should show `-rwxr-xr-x` permissions. If not, run `chmod +x .githooks/*`.

**Need to bypass temporarily:**

You can use `git commit --no-verify` to skip hooks, but you should only do this if you're certain the commit doesn't violate the policy. The hooks exist to catch mistakes automatically.

## Commit Message Template

There's a commit message template in `.gitmessage` with reminders about the docs-internal policy, message format guidelines, and a pre-commit checklist:

```bash
# Set commit message template (optional)
git config commit.template .gitmessage
```

## Testing the Hooks

To verify the hooks work, try staging a docs-internal file:

```bash
touch docs-internal/test.md
git add docs-internal/test.md
git commit -m "test"
# Expected: ERROR - pre-commit hook blocks
```

Or try referencing docs-internal in a commit message:

```bash
git commit --allow-empty -m "Add feature (documented in docs-internal/)"
# Expected: ERROR - commit-msg hook blocks
```

Clean up afterwards:

```bash
git reset HEAD docs-internal/test.md 2>/dev/null || true
rm docs-internal/test.md
```
