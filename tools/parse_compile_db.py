#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
#
# parse_compile_db.py — Extract build context from a compile_commands.json.
#
# Parses every compile command in the database and writes three output files
# consumed by clang-tidy and cppcheck in the static-analysis pipeline:
#
#   files_out    — one source file path per line (for --file-list / mapfile)
#   includes_out — one include directory per line (for --includes-file)
#   defines_out  — one preprocessor define per line (for -D flags)
#
# All -I, -isystem, and -D flags are extracted from the real compile commands,
# so analysis tools see exactly the same context as the actual build.
#
# Usage:
#   python3 parse_compile_db.py <project_db> <files_out> <includes_out> <defines_out>
#
# Arguments:
#   project_db    — path to the filtered compile_commands.json
#   files_out     — output file: one source path per line
#   includes_out  — output file: one include directory per line
#   defines_out   — output file: one define per line

import json
import re
import sys

# Cppcheck evaluates define values as expressions, so only defines whose values
# are safe to pass as-is are included.  Defines with string or date-like values
# (e.g. _BUILD_TIME=2026-02-25) would be misinterpreted as arithmetic and cause
# an internalError bail-out.  We accept:
#   - bare defines with no value  (e.g. DEBUG)
#   - defines with a pure identifier or integer value  (e.g. FOO=1, BAR=rp2040)
# Everything else is silently dropped — cppcheck doesn't need string macros for
# its control-flow or portability checks.
_SAFE_VALUE_RE = re.compile(r"^[A-Za-z0-9_]*$")


def _is_safe_define(define: str) -> bool:
    """Return True if define is safe to pass to cppcheck as a -D flag."""
    if "=" not in define:
        return True  # bare flag, no value
    _, value = define.split("=", 1)
    # A value starting with a quote is a string literal that got truncated by
    # the \S+ regex (spaces inside the quoted string are not captured).  Passing
    # a partial string like -D_KEYBOARD_DESCRIPTION="IBM to cppcheck causes a
    # bad-macro-syntax internalError.  Reject anything that starts with a quote.
    if value.startswith('"') or value.startswith("'"):
        return False
    return bool(_SAFE_VALUE_RE.match(value))


def main() -> None:
    if len(sys.argv) != 5:
        print(
            "Usage: parse_compile_db.py <project_db> <files_out> <includes_out> <defines_out>",
            file=sys.stderr,
        )
        sys.exit(1)

    project_db, files_out, includes_out, defines_out = sys.argv[1:]

    with open(project_db) as f:
        db = json.load(f)

    include_dirs: set[str] = set()
    defines: set[str] = set()
    files: set[str] = set()
    total_defines_seen = 0

    for entry in db:
        files.add(entry["file"])
        # compile_commands.json entries may use either 'command' (a single
        # shell string) or 'arguments' (a pre-split list).
        cmd = entry.get("command", "") or " ".join(entry.get("arguments", []))
        # Strip escaped quotes that cmake sometimes emits around string values.
        cmd = cmd.replace('\\"', "")
        for m in re.finditer(r"-I\s*(\S+)", cmd):
            include_dirs.add(m.group(1))
        for m in re.finditer(r"-isystem\s+(\S+)", cmd):
            include_dirs.add(m.group(1))
        for m in re.finditer(r"-D\s*(\S+)", cmd):
            total_defines_seen += 1
            d = m.group(1)
            if _is_safe_define(d):
                defines.add(d)

    with open(files_out, "w") as f:
        for fp in sorted(files):
            f.write(fp + "\n")

    with open(includes_out, "w") as f:
        for d in sorted(include_dirs):
            f.write(d + "\n")

    with open(defines_out, "w") as f:
        for d in sorted(defines):
            f.write(d + "\n")

    skipped = total_defines_seen - len(defines)

    print(
        f"  {len(include_dirs)} include dirs, "
        f"{len(defines)} defines ({skipped} skipped \u2014 complex values), "
        f"{len(files)} files"
    )


if __name__ == "__main__":
    main()
