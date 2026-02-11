# Contributing

If you'd like to contribute to the converter project—whether it's adding support for a new keyboard, fixing a bug, improving documentation, or anything else—that's brilliant! This guide should help you get started and explain how the contribution process works.

I'm always happy to see contributions, and I'll do my best to review and merge them quickly. Don't worry if you're not sure about something—just ask, and I'll try to help out.

---

## Before You Start

Before diving in, it's worth reading through a few bits of documentation to understand how the project's put together:

- **[System Architecture](../advanced/architecture.md):** Gives you an overview of how data flows through the system and how the main components fit together.

- **[Code Standards](code-standards.md):** Contains the critical architecture rules and design principles. It's quite detailed, but it'll save you time by explaining why things are done certain ways. Particularly important if you're working on protocol handlers or modifying the core processing pipeline.

- **[Protocol Documentation](../protocols/README.md):** If you're adding a new keyboard or working with an existing protocol, read through the relevant protocol documentation. It contains timing diagrams, command formats, and implementation notes.

The project has some fairly strict rules about blocking operations, multicore usage, and memory barriers—these aren't arbitrary, they're there to ensure the converter stays responsive and doesn't drop scancodes. Have a read through the architecture documentation to understand why.

---

## Getting Set Up

### Fork and Clone

First, fork the repository on GitHub to your own account. Then clone your fork locally:

```bash
git clone --recurse-submodules https://github.com/your-username/rp2040-keyboard-converter.git
cd rp2040-keyboard-converter
```

The `--recurse-submodules` is important—it pulls in the Pico SDK submodule which is required for building.

If you've already cloned without submodules, you can initialize them afterwards:

```bash
git submodule update --init --recursive
```

### Docker Build Environment

The project uses Docker for builds to ensure everyone's using the same toolchain and SDK version. You'll need Docker installed on your system.

Build the Docker image (only needed once):

```bash
docker compose build
```

Now you can build firmware configurations:

```bash
# Build for a specific keyboard
docker compose run --rm -e KEYBOARD="modelm/enhanced" builder

# Build with keyboard and mouse support
docker compose run --rm -e KEYBOARD="modelm/enhanced" -e MOUSE="at-ps2" builder
```

The output UF2 file will be in the `build/` directory.

---

## Making Changes

### Create a Branch

Always create a new branch for your changes. Branch names should be descriptive and follow this format:

```bash
# For new features
git checkout -b feature/amiga-keyboard-support

# For bug fixes
git checkout -b fix/ringbuf-overflow

# For documentation
git checkout -b docs/improve-hardware-setup
```

Use lowercase with hyphens, and start with `feature/`, `fix/`, `docs/`, `refactor/`, or whatever best describes what you're doing.

### Follow the Code Standards

The project has some coding standards—mostly around non-blocking operation, memory barriers, and comment style. Have a read through [Code Standards](code-standards.md) before making changes.

**Critical rules:**
- No `sleep_ms()`, `sleep_us()`, `busy_wait_ms()`, or `busy_wait_us()` in the main processing path
- No multicore APIs (Core 1 is unused, everything runs on Core 0)
- No `printf()` in interrupt handlers (use `LOG_*` macros instead)
- Use `volatile` and `__dmb()` memory barriers for IRQ-shared variables

Before committing, run the lint script:

```bash
./tools/lint.sh
```

This checks for blocking operations, multicore usage, and documentation policy violations. It must pass with 0 errors and 0 warnings before you can submit a pull request.

### Testing

If you're modifying firmware code, test your changes on real hardware if possible. Timing-sensitive protocols can behave quite differently on actual hardware compared to simulation—microsecond timing matters here, and you'll only catch certain issues with the real thing.

Start with basic functionality—does it work as expected? Then stress-test the pipeline with fast typing, around 30+ characters per second. That'll show up any buffering or timing issues. Test Command Mode by holding both shifts for 3 seconds to verify the mode activates properly. If your protocol supports LEDs, make sure they update correctly when you toggle Caps Lock or Num Lock. Finally, test protocol error recovery by power cycling the keyboard mid-operation—the converter should detect the keyboard disappearing and recover when it comes back.

If you can't test on hardware, mention that in your pull request and explain what testing you did do. It's not a blocker, but it helps me understand what additional verification might be needed.

### Testing Enforcement Tools

If you're modifying enforcement mechanisms (lint.sh, git hooks, or CI checks), test them with:

```bash
./tools/test-enforcement.sh
```

This meta-testing suite validates that enforcement tools correctly detect violations. It creates intentional violations and verifies they're caught properly. Regular contributors working on keyboard support, bug fixes, or features don't need to run this—it's only for those modifying the enforcement tools themselves.

---

## Commit Message Format

The project uses conventional commit message format—it helps with automatic changelog generation and makes the git history easier to navigate when you're trying to figure out when something changed.

The format looks like this:

```
<type>(<scope>): <subject>

<body>

<footer>
```

The type describes what kind of change it is: `feat` for new features (like new keyboard support or a new protocol), `fix` for bug fixes, `docs` for documentation changes, `refactor` for code refactoring without functional changes, `test` for adding or updating tests, and `chore` for build system stuff, dependencies, or other non-source changes.

The scope's optional but helpful—it indicates what area's affected. So you might use `at-ps2`, `amiga`, `ringbuf`, `docs`, or whatever's relevant to your change.

**Examples:**

```
feat(amiga): add Amiga 1000 keyboard support

Implements Amiga protocol handler with proper handshake timing
and Caps Lock LED control via pulse duration.

Fixes #42
```

```
fix(at-ps2): correct parity calculation for host-to-device

The parity bit wasn't being calculated correctly for the 0xED
LED command, causing some keyboards to NAK. Now using the
interface_parity_table lookup for consistency.
```

```
docs(hardware): clarify level shifter requirements

Added notes about voltage requirements and bidirectional operation.
Included wiring diagram for Adafruit BSS138 module.
```

**Keep commits atomic:** Each commit should represent a single logical change. If you're adding a new keyboard and fixing an unrelated bug, make them separate commits.

---

## Pull Request Process

### Submitting Your PR

When you're ready, push your branch to your fork:

```bash
git push origin feature/your-branch-name
```

Then open a pull request on GitHub from your branch to the main repository's `main` branch.

In your PR description, explain what you've changed and why you've changed it. Describe what testing you've done—if you tested on hardware, mention which keyboard or converter board you used. If there are any known limitations or follow-up work that's needed, mention those too. Screenshots or UART logs can be quite helpful if you're fixing a timing issue or demonstrating new functionality.

The PR template will guide you through what information's needed, so don't worry if you're not sure what to include.

### PR Checklist

Before submitting, run through these checks: Make sure `./tools/lint.sh` passes with 0 errors and 0 warnings—this is mandatory. Verify the code builds successfully using the Docker build with your configuration. If you've modified firmware, test it on real hardware if at all possible, and mention which hardware you used in the PR. Update any documentation if you've changed functionality—don't leave that for later, it's easy to forget. Check that your commit messages follow the conventional commit format. Finally, make sure you haven't included any unrelated changes—keep PRs focused on one thing.

### Code Review

Your PR will be reviewed. There's also an AI-powered reviewer called CodeRabbit that automatically checks for issues and suggests improvements. It's a GitHub app that looks for things like adherence to project coding standards, potential bugs or logic errors, documentation gaps, and best practice violations.

CodeRabbit's reviews are suggestions, not blocking—if it flags something you disagree with, just explain your reasoning in a reply and we'll work it out. Its pattern matching isn't perfect, and sometimes it'll suggest changes that don't make sense in context.

I'll review the code as well and might ask for changes or clarifications. This is completely normal—it's not a criticism, just a way to ensure the code stays maintainable and follows the project's patterns. Sometimes I'll suggest a different approach or ask questions to understand why you did something a certain way.

### Merging

Once the review's done and any requested changes are made, I'll merge your PR. The changes will go into the main branch and be included in the next release.

If your contribution adds new functionality, I'll make sure it's documented and credit you in the release notes.

---

## Types of Contributions

Here are some common types of contributions and where to start:

### Adding a New Keyboard

If you're adding support for a specific keyboard model using an existing protocol, you'll create a new directory under [`src/keyboards/`](../../src/keyboards/)`<brand>/<model>/` and add four files: `keyboard.config`, `keyboard.h`, `keyboard.c`, and `README.md`. Define the keymap using the appropriate `KEYMAP_*()` macro for your layout, then test with your actual keyboard to make sure everything works.

See [Adding Keyboards](adding-keyboards.md) for a detailed guide that walks through the whole process.

### Adding a New Protocol

If you're adding support for a new keyboard protocol, this is more involved. You'll create a new directory under [`src/protocols/`](../../src/protocols/)`<name>/` and write a PIO program (`.pio` file) that handles the protocol timing. Then implement the interrupt handler to extract scancodes and write them to the ring buffer. You'll also need to implement the protocol state machine for things like initialisation, LED commands, and error recovery. If the protocol uses multi-byte scancode sequences, you'll need to add a scancode processor for that. Finally, document the protocol with timing diagrams and implementation notes so others can understand how it works.

Have a look at [`at-ps2/`](../../src/protocols/at-ps2/) or [`xt/`](../../src/protocols/xt/) for examples of how protocols are structured—they'll give you a sense of what's involved.

### Fixing Bugs

If you've found a bug and want to fix it, first check the GitHub issues to see if it's already been reported. If not, open an issue describing the bug and how to reproduce it—that helps track what's being worked on. Then make your fix, being careful not to introduce blocking operations or break the threading model. Test the fix on hardware if possible, since timing issues often only show up on real devices. Finally, submit a PR referencing the issue number.

### Improving Documentation

Documentation improvements are always welcome! Whether it's fixing typos, clarifying confusing sections, or adding missing information, it all helps. Make your changes using Markdown format with British English spelling, check that internal links work (use relative paths), and follow the existing documentation style—conversational but informative. Submit a PR with a clear description of what you've improved.

One important note: Don't commit or reference files in `docs-internal/`—that directory is for local development notes only and isn't tracked in git.

---

## Getting Help

If you're stuck or not sure about something, just ask. Open an issue on GitHub with your question—I'm happy to help figure things out. It's worth checking existing issues first to see if someone's already asked something similar, and having a read through the documentation in `docs/` and the source code comments, which are usually fairly detailed.

But don't be afraid to ask questions even if you think it might be obvious. I'd much rather answer a question and help you contribute than have you struggle in silence or give up because something wasn't clear.

---

## Project Goals and Philosophy

A few notes about the project's direction that might help when you're making decisions about how to implement something.

The architecture prioritizes performance over convenience. Low latency and deterministic behaviour matter more than code simplicity here. If a change makes the code simpler but adds latency or introduces blocking operations, it's probably not the right approach for this project—even if it would be fine in most other contexts.

Hardware testing really matters with timing-sensitive embedded firmware. It's genuinely hard to test without real hardware because microsecond timing differences can change behaviour. If you can test on the actual device, please do. Simulators and logic analysers are useful, but nothing quite replaces testing with the real keyboard.

Documentation is part of the code, not an afterthought. Good documentation makes the project accessible to others who want to understand how things work or add their own keyboard. If you're adding a feature, document it. If you're fixing a bug, explain why it happened—that helps prevent the same bug creeping back in later.

Finally, the converter's single-core and non-blocking. Core 1 stays unused, and nothing in the critical path blocks. This isn't negotiable—it's fundamental to how the converter achieves its latency characteristics. If you're tempted to add a `sleep_ms()` or use Core 1, there's probably a better approach that maintains the non-blocking architecture.

---

Thanks for considering contributing! Even if you're just fixing a typo in documentation, it's appreciated. If you have questions or want to discuss a larger change before starting work, feel free to open an issue and we can chat about it.

---

## Related Documentation

- [Code Standards](code-standards.md) - Coding conventions and testing scenarios
- [Adding Keyboards](adding-keyboards.md) - Step-by-step keyboard support guide
- [Custom Keymaps](custom-keymaps.md) - Keymap modification guide
- [Development Guide](README.md) - Overview and getting started
- [Advanced Topics](../advanced/README.md) - Architecture and performance analysis
- [Lint Tool](../../tools/lint.sh) - Code quality enforcement

---

**Questions or stuck on something?**  
Pop into [GitHub Discussions](https://github.com/PaulW/rp2040-keyboard-converter/discussions) or [report a bug](https://github.com/PaulW/rp2040-keyboard-converter/issues) if you've found an issue.
