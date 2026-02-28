#!/bin/bash
#
# Static Analysis Script for RP2040 Keyboard Converter
# Runs Clang-Tidy (distro-specific version) and Cppcheck 2.19.0 against our
# project source files.
#
# Produces output from Clang-Tidy 19.1.7 and Cppcheck 2.19.0, allowing
# reproduction of issues locally before they appear in code review.
#
# Preferred usage — build + analyse in one step (no linking failures):
#   docker compose run --rm -e KEYBOARD="modelf/pcat" analyser
#
# This invokes entrypoint.sh (full firmware build) followed by this script.
# Because the build is already complete, all PIO headers are present and
# clang-tidy/cppcheck see the exact same compile context as the real build.
#
# Standalone usage (separate cmake configure + partial build):
#   EXISTING_BUILD_DIR="" docker compose run --rm analyser
#   (Requires a custom entrypoint override; the analyser service always sets
#    ANALYSE=1 so entrypoint.sh drives it via EXISTING_BUILD_DIR.)
#
# KEYBOARD env var selects the keyboard config for cmake.
# Defaults to modelf/pcat, which exercises all common code paths.
#

set -e

# ─── Colours (consistent with lint.sh) ───────────────────────────────────────
RED='\033[0;31m'
YELLOW='\033[1;33m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m'

CLANG_ERRORS=0
CLANG_WARNINGS=0
CPPCHECK_ERRORS=0
CPPCHECK_WARNINGS=0

echo ""
echo "========================================"
echo "Static Analysis"
echo "========================================"
echo ""

# ─── Build directory / compile_commands.json ─────────────────────────────────
# When EXISTING_BUILD_DIR is set the caller has already run a full build in that
# directory (via ANALYSE=1 in entrypoint.sh).  compile_commands.json and all
# PIO headers are already present — no cmake step required.
#
# When EXISTING_BUILD_DIR is not set a fresh cmake configure and build are run
# in analysis-build/ so the script can be used standalone.
if [ -n "${EXISTING_BUILD_DIR}" ]; then
  BUILD_DIR="${EXISTING_BUILD_DIR}"
  echo -e "${BLUE}[INFO]${NC} Using existing build directory: ${BUILD_DIR}"
  echo -e "${BLUE}[INFO]${NC}   (PIO headers and compile_commands.json already present)"

  if [ ! -f "${BUILD_DIR}/compile_commands.json" ]; then
    echo -e "${RED}[FAIL]${NC} compile_commands.json not found in ${BUILD_DIR}"
    echo -e "${RED}[FAIL]${NC} Rebuild with -DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
    exit 1
  fi

  echo -e "${GREEN}[ OK ]${NC} compile_commands.json found"
  echo ""
else
  ANALYSIS_KEYBOARD=${KEYBOARD:-modelf/pcat}
  BUILD_DIR="${APP_DIR}/analysis-build"
  echo -e "${BLUE}[INFO]${NC} Generating compile_commands.json (KEYBOARD=${ANALYSIS_KEYBOARD})..."

  mkdir -p "${BUILD_DIR}"

  # Remove cached cmake state so paths are always fresh for the mounted source.
  rm -f "${BUILD_DIR}/CMakeCache.txt"

  KEYBOARD="${ANALYSIS_KEYBOARD}" cmake \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DCMAKE_BUILD_TYPE=Debug \
    -B "${BUILD_DIR}" \
    "${APP_DIR}/src" > /dev/null 2>&1

  if [ ! -f "${BUILD_DIR}/compile_commands.json" ]; then
    echo -e "${RED}[FAIL]${NC} cmake did not produce compile_commands.json"
    exit 1
  fi

  echo -e "${GREEN}[ OK ]${NC} compile_commands.json generated"

  # Build the firmware to generate PIO headers (.pio → .pio.h via pioasm).
  # These headers must exist before clang-tidy attempts to analyse files that
  # include them.
  echo -e "${BLUE}[INFO]${NC} Building to generate PIO headers..."
  cmake --build "${BUILD_DIR}" --parallel "$(nproc)" > /dev/null 2>&1
  echo -e "${GREEN}[ OK ]${NC} Build complete (PIO headers generated)"
  echo ""
fi

# Filter compile_commands.json to project source files only.
# compile_commands.json contains entries for every file cmake knows about,
# including thousands of pico-sdk files.  Both clang-tidy and cppcheck would
# follow those entries without this filter.
PROJECT_DB="${BUILD_DIR}/compile_commands_project.json"
python3 "${APP_DIR}/filter_compile_db.py" \
  "${BUILD_DIR}/compile_commands.json" "${PROJECT_DB}" "${APP_DIR}/src/"
echo ""

# Parse the filtered database once: extract file list, include paths and
# defines.  Outputs are reused by both clang-tidy and cppcheck below.
DB_FILES=$(mktemp)
DB_INCLUDES=$(mktemp)
DB_DEFINES=$(mktemp)

echo -e "${BLUE}[INFO]${NC} Parsing compilation database (files, includes, defines)..."
python3 "${APP_DIR}/parse_compile_db.py" \
  "${PROJECT_DB}" "${DB_FILES}" "${DB_INCLUDES}" "${DB_DEFINES}"
echo ""

# ─── Clang-Tidy ---───────────────────────────────────────────────────────────
echo "----------------------------------------"
echo "Clang-Tidy"
echo "----------------------------------------"
echo ""
echo -e "${BLUE}[INFO]${NC} $(clang-tidy --version 2>&1 | head -1)"
echo ""

# Detect ARM GCC system include paths dynamically
# This is necessary because clang-tidy uses its own frontend but needs to find
# the ARM newlib headers that the actual ARM GCC toolchain uses.
ARM_GCC_INCLUDES=""
if command -v arm-none-eabi-gcc >/dev/null 2>&1; then
  echo -e "${BLUE}[INFO]${NC} Detecting ARM GCC system include paths..."
  ARM_INCLUDE_PATHS=$(echo '#include <stdio.h>' | arm-none-eabi-gcc -E -v -x c - 2>&1 \
    | sed -n '/#include <...> search starts here:/,/End of search list./p' \
    | grep '^ ' \
    | sed 's/^ //' \
    | while read -r path; do
        # Resolve relative paths like ../../../arm-none-eabi/include
        if [ -d "$path" ]; then
          realpath "$path" 2>/dev/null || echo "$path"
        fi
      done \
    | sort -u)
  
  for inc_path in $ARM_INCLUDE_PATHS; do
    if [ -d "$inc_path" ]; then
      ARM_GCC_INCLUDES="${ARM_GCC_INCLUDES} --extra-arg=-isystem${inc_path}"
    fi
  done
  
  if [ -n "$ARM_GCC_INCLUDES" ]; then
    echo -e "${GREEN}[ OK ]${NC} ARM GCC system includes detected"
  else
    echo -e "${YELLOW}[WARN]${NC} Could not detect ARM GCC system includes"
  fi
fi
echo ""

CLANG_OUTPUT=$(mktemp)

mapfile -t PROJECT_FILES < "${DB_FILES}"

if [ "${#PROJECT_FILES[@]}" -eq 0 ]; then
  echo -e "${RED}[FAIL]${NC} No source files found in compilation database"
  exit 1
else
  echo -e "${BLUE}[INFO]${NC} Analysing ${#PROJECT_FILES[@]} source files from compilation database..."
  for src_file in "${PROJECT_FILES[@]}"; do
    echo -e "${BLUE}[FILE]${NC} ${src_file}"
    clang-tidy \
      --config-file="${APP_DIR}/.clang-tidy" \
      --header-filter="${APP_DIR}/src/.*" \
      -p "${BUILD_DIR}" \
      ${ARM_GCC_INCLUDES} \
      "${src_file}" 2>&1 | tee -a "${CLANG_OUTPUT}" || true
  done
fi

CLANG_ERRORS=$(grep -c ': error:' "${CLANG_OUTPUT}" 2>/dev/null || true)
CLANG_WARNINGS=$(grep -c ': warning:' "${CLANG_OUTPUT}" 2>/dev/null || true)

if [ "${CLANG_ERRORS}" -gt 0 ]; then
  echo -e "${RED}[FAIL]${NC} clang-tidy: ${CLANG_ERRORS} error(s), ${CLANG_WARNINGS} warning(s)"
elif [ "${CLANG_WARNINGS}" -gt 0 ]; then
  echo -e "${YELLOW}[WARN]${NC} clang-tidy: 0 errors, ${CLANG_WARNINGS} warning(s)"
else
  echo -e "${GREEN}[ OK ]${NC} clang-tidy: no issues found"
fi

rm -f "${CLANG_OUTPUT}"
echo ""

# ─── Cppcheck 2.19.0 ─────────────────────────────────────────────────────────
echo "----------------------------------------"
echo "Cppcheck 2.19.0"
echo "----------------------------------------"
echo ""
echo -e "${BLUE}[INFO]${NC} $(cppcheck --version)"
echo ""

# Build defines array from the extracted defines file (one define per line).
CPPCHECK_DEFINES_ARGS=()
while IFS= read -r define; do
  [ -n "${define}" ] && CPPCHECK_DEFINES_ARGS+=("-D${define}")
done < "${DB_DEFINES}"

CPPCHECK_OUTPUT=$(mktemp)

# --file-list      : explicit list of project source files (no SDK files)
# --includes-file  : every -I/-isystem path from the real compile commands
# -D flags         : every define from the real compile commands
# --suppress=missingIncludeSystem : silence system header lookup noise
# --suppress=preprocessorErrorDirective : silence #error guards (e.g. missing
#   CFG_TUSB_MCU) that are satisfied at build time but not resolvable by cppcheck
# --inline-suppr   : respect // cppcheck-suppress comments in source
cppcheck \
  --file-list="${DB_FILES}" \
  --includes-file="${DB_INCLUDES}" \
  "${CPPCHECK_DEFINES_ARGS[@]}" \
  --enable=performance,portability,missingInclude \
  --disable=warning,style,information \
  --inline-suppr \
  --suppress=missingIncludeSystem \
  --suppress=preprocessorErrorDirective \
  --suppress=checkersReport \
  --error-exitcode=0 \
  --template='{severity}: [{id}] {message} ({file}:{line})' \
  2>&1 | tee "${CPPCHECK_OUTPUT}" || true

rm -f "${DB_FILES}" "${DB_INCLUDES}" "${DB_DEFINES}"

CPPCHECK_ERRORS=$(grep -c '^error:' "${CPPCHECK_OUTPUT}" 2>/dev/null || true)
CPPCHECK_WARNINGS=$(grep -Ec '^(warning|performance|portability):' "${CPPCHECK_OUTPUT}" 2>/dev/null || true)

if [ "${CPPCHECK_ERRORS}" -gt 0 ]; then
  echo -e "${RED}[FAIL]${NC} cppcheck: ${CPPCHECK_ERRORS} error(s), ${CPPCHECK_WARNINGS} warning(s)"
elif [ "${CPPCHECK_WARNINGS}" -gt 0 ]; then
  echo -e "${YELLOW}[WARN]${NC} cppcheck: 0 errors, ${CPPCHECK_WARNINGS} warning(s)"
else
  echo -e "${GREEN}[ OK ]${NC} cppcheck: no issues found"
fi

rm -f "${CPPCHECK_OUTPUT}"
echo ""

# ─── Summary ─────────────────────────────────────────────────────────────────
TOTAL_ERRORS=$((CLANG_ERRORS + CPPCHECK_ERRORS))
TOTAL_WARNINGS=$((CLANG_WARNINGS + CPPCHECK_WARNINGS))

echo "========================================"
echo "Analysis Summary"
echo "========================================"
echo ""
printf "  %-20s errors: %d   warnings: %d\n" "clang-tidy" "${CLANG_ERRORS}" "${CLANG_WARNINGS}"
printf "  %-20s errors: %d   warnings: %d\n" "cppcheck"   "${CPPCHECK_ERRORS}" "${CPPCHECK_WARNINGS}"
echo ""

if [ "${TOTAL_ERRORS}" -gt 0 ]; then
  echo -e "${RED}RESULT: FAILED — ${TOTAL_ERRORS} error(s) found${NC}"
  exit 1
elif [ "${TOTAL_WARNINGS}" -gt 0 ]; then
  echo -e "${YELLOW}RESULT: PASSED WITH WARNINGS — ${TOTAL_WARNINGS} warning(s)${NC}"
  exit 0
else
  echo -e "${GREEN}RESULT: PASSED — no issues found${NC}"
  exit 0
fi
