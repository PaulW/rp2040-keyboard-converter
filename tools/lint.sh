#!/bin/bash
#
# Architectural Lint Script for RP2040 Keyboard Converter
# Enforces critical architecture rules from .github/copilot-instructions.md
#
# Usage: ./tools/lint.sh [--strict]
#   --strict: Fail on warnings (use in CI)
#

set -e

# Color codes
RED='\033[0;31m'
YELLOW='\033[1;33m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Base grep exclude patterns (avoid scanning tools/hooks/workflow files)
EXCLUDE_DIRS=(
    --exclude-dir=".git"
    --exclude-dir="build"
    --exclude-dir="external"
    --exclude-dir="docs-internal"
    --exclude-dir="tools"
    --exclude-dir=".githooks"
    --exclude-dir=".github"
)

# Options
STRICT_MODE=0
if [[ "$1" == "--strict" ]]; then
    STRICT_MODE=1
fi

# Counters
ERRORS=0
WARNINGS=0

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}RP2040 Architecture Lint Checks${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Check 1: Blocking Operations
echo -e "${BLUE}[1/7] Checking for blocking operations...${NC}"
BLOCKING_CALLS=$(grep -R --line-number "${EXCLUDE_DIRS[@]}" --exclude="*.md" -E "sleep_ms\(|sleep_us\(|busy_wait_us\(|busy_wait_ms\(" src/ 2>/dev/null || true)

if [ -n "$BLOCKING_CALLS" ]; then
    echo -e "${RED}ERROR: Blocking operations detected!${NC}"
    echo ""
    echo "$BLOCKING_CALLS"
    echo ""
    echo -e "${YELLOW}Architecture rule: NEVER use blocking operations${NC}"
    echo "  - Use to_ms_since_boot(get_absolute_time()) for timing"
    echo "  - Implement time-based state machines"
    echo ""
    ERRORS=$((ERRORS + 1))
else
    echo -e "${GREEN}✓ No blocking operations found${NC}"
fi
echo ""

# Check 2: Multicore API Usage
echo -e "${BLUE}[2/7] Checking for multicore API usage...${NC}"
MULTICORE_USAGE=$(grep -R --line-number "${EXCLUDE_DIRS[@]}" --exclude="*.md" -E "multicore_launch_core1|multicore_fifo_push_blocking|multicore_fifo_pop_blocking|multicore_fifo_drain|multicore_reset_core1|multicore_lockout" src/ 2>/dev/null || true)

if [ -n "$MULTICORE_USAGE" ]; then
    echo -e "${RED}ERROR: Multicore API usage detected!${NC}"
    echo ""
    echo "$MULTICORE_USAGE"
    echo ""
    echo -e "${YELLOW}Architecture rule: Single-core only (Core 0)${NC}"
    echo "  - Multicore adds complexity without performance benefit"
    echo "  - All processing runs on Core 0"
    echo ""
    ERRORS=$((ERRORS + 1))
else
    echo -e "${GREEN}✓ No multicore API usage found${NC}"
fi
echo ""

# Check 3: printf in IRQ Context
echo -e "${BLUE}[3/7] Checking for printf in IRQ context...${NC}"
# This is a heuristic check - finds functions with __isr attribute and checks for printf
ISR_FUNCTIONS=$(grep -R --files-with-matches "${EXCLUDE_DIRS[@]}" --exclude="*.md" "__isr" src/ 2>/dev/null || true)

PRINTF_IN_ISR=""
if [ -n "$ISR_FUNCTIONS" ]; then
    for file in $ISR_FUNCTIONS; do
        # Find all __isr function names
        ISR_NAMES=$(grep -o "__isr [a-zA-Z_][a-zA-Z0-9_]*" "$file" | awk '{print $2}' || true)
        
        for isr_name in $ISR_NAMES; do
            # Check if printf/fprintf/sprintf appears in same function
            # Look ahead max 30 lines and stop at closing brace at column 1 (end of function)
            # Note: LOG_* macros are allowed - they're designed for IRQ use
            FUNCTION_BODY=$(grep -A 30 "$isr_name" "$file" | awk '/^}/ {exit} {print}')
            if echo "$FUNCTION_BODY" | grep -E "\bprintf\(|\bfprintf\(|\bsprintf\(" >/dev/null 2>&1; then
                PRINTF_IN_ISR="${PRINTF_IN_ISR}${file}:${isr_name}\n"
            fi
        done
    done
fi

if [ -n "$PRINTF_IN_ISR" ]; then
    echo -e "${YELLOW}WARNING: Possible printf in IRQ context:${NC}"
    echo ""
    echo -e "$PRINTF_IN_ISR"
    echo ""
    echo -e "${YELLOW}Architecture rule: No printf() in IRQ context${NC}"
    echo "  - printf/fprintf/sprintf use DMA which isn't IRQ-safe"
    echo "  - LOG_* macros are allowed - designed for IRQ use"
    echo "  - Defer logging to main loop if unsure"
    echo ""
    WARNINGS=$((WARNINGS + 1))
else
    echo -e "${GREEN}✓ No obvious printf in IRQ detected${NC}"
fi
echo ""

# Check 4: ringbuf_reset() Usage
echo -e "${BLUE}[4/7] Checking ringbuf_reset() usage...${NC}"
# Exclude the ringbuf library itself (contains function definition)
RINGBUF_RESET=$(grep -R --line-number "${EXCLUDE_DIRS[@]}" --exclude="*.md" --exclude-dir="ringbuf" "ringbuf_reset" src/ 2>/dev/null | grep -v "^src/common/lib/ringbuf\.[ch]:" || true)

if [ -n "$RINGBUF_RESET" ]; then
    # Check for LINT:ALLOW annotation
    RINGBUF_VIOLATIONS=""
    while IFS= read -r line; do
        file=$(echo "$line" | cut -d':' -f1)
        linenum=$(echo "$line" | cut -d':' -f2)
        
        # Check if the specific line contains LINT:ALLOW annotation
        # Use sed to extract the exact line and check for the annotation
        if ! sed -n "${linenum}p" "$file" | grep -q "// LINT:ALLOW ringbuf_reset"; then
            RINGBUF_VIOLATIONS="${RINGBUF_VIOLATIONS}${line}\n"
        fi
    done <<< "$RINGBUF_RESET"
    
    if [ -n "$RINGBUF_VIOLATIONS" ]; then
        echo -e "${YELLOW}WARNING: ringbuf_reset() usage detected:${NC}"
        echo ""
        echo -e "$RINGBUF_VIOLATIONS"
        echo ""
        echo -e "${YELLOW}Architecture rule: Only call ringbuf_reset() with IRQs disabled${NC}"
        echo "  - Never call during normal operation"
        echo "  - Only during initialization or state machine resets"
        echo "  - Must verify IRQs are disabled first"
        echo "  - Add '// LINT:ALLOW ringbuf_reset' comment to suppress this warning"
        echo ""
        WARNINGS=$((WARNINGS + 1))
    else
        echo -e "${GREEN}✓ All ringbuf_reset() calls have LINT:ALLOW annotation${NC}"
    fi
else
    echo -e "${GREEN}✓ No ringbuf_reset() calls found${NC}"
fi
echo ""

# Check 5: docs-internal in Repository
echo -e "${BLUE}[5/7] Checking for docs-internal/ files in repository...${NC}"
DOCS_INTERNAL=$(git ls-files | grep "^docs-internal/" 2>/dev/null || true)

if [ -n "$DOCS_INTERNAL" ]; then
    echo -e "${RED}ERROR: docs-internal/ files found in repository!${NC}"
    echo ""
    echo "$DOCS_INTERNAL"
    echo ""
    echo -e "${YELLOW}Documentation policy: NEVER commit docs-internal/ files${NC}"
    echo "  - These are local-only documentation"
    echo "  - Should be in .gitignore"
    echo ""
    ERRORS=$((ERRORS + 1))
else
    echo -e "${GREEN}✓ No docs-internal/ files in repository${NC}"
fi
echo ""

# Check 6: Flash Execution (check for missing copy_to_ram)
echo -e "${BLUE}[6/7] Checking for copy_to_ram configuration...${NC}"
COPY_TO_RAM=$(grep -R --exclude-dir=".git" --exclude-dir="build" --exclude-dir="external" --exclude-dir="docs-internal" "pico_set_binary_type.*copy_to_ram" . 2>/dev/null || true)

if [ -z "$COPY_TO_RAM" ]; then
    echo -e "${YELLOW}WARNING: pico_set_binary_type(copy_to_ram) not found in CMakeLists.txt${NC}"
    echo ""
    echo -e "${YELLOW}Architecture rule: All code must run from RAM${NC}"
    echo "  - Add: pico_set_binary_type(target copy_to_ram)"
    echo "  - Critical for timing-sensitive operations"
    echo ""
    WARNINGS=$((WARNINGS + 1))
else
    echo -e "${GREEN}✓ copy_to_ram configuration found${NC}"
fi
echo ""

# Check 7: Volatile Usage on IRQ-Shared Variables
echo -e "${BLUE}[7/7] Checking for IRQ-shared variables...${NC}"
# This is a heuristic - looks for static variables in files with ISR functions
echo -e "${YELLOW}⚠ Manual review required for IRQ-shared variables${NC}"
echo "  - Check that shared variables use 'volatile' keyword"
echo "  - Check for __dmb() memory barriers after volatile writes/reads"
echo "  - See copilot-instructions.md for examples"
echo ""

# Summary
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Lint Summary${NC}"
echo -e "${BLUE}========================================${NC}"

if [ $ERRORS -eq 0 ] && [ $WARNINGS -eq 0 ]; then
    echo -e "${GREEN}✓ All checks passed!${NC}"
    exit 0
elif [ $ERRORS -eq 0 ]; then
    echo -e "${YELLOW}⚠ $WARNINGS warning(s) found${NC}"
    if [ $STRICT_MODE -eq 1 ]; then
        echo -e "${RED}Strict mode: Failing due to warnings${NC}"
        exit 1
    else
        echo -e "${YELLOW}Warnings present but not failing (use --strict to fail on warnings)${NC}"
        exit 0
    fi
else
    echo -e "${RED}✗ $ERRORS error(s), $WARNINGS warning(s) found${NC}"
    exit 1
fi
