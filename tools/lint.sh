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
    # Check for LINT:ALLOW annotations
    BLOCKING_VIOLATIONS=""
    while IFS= read -r line; do
        file=$(echo "$line" | cut -d':' -f1)
        linenum=$(echo "$line" | cut -d':' -f2)
        
        # Check if the specific line contains LINT:ALLOW annotation
        if ! sed -n "${linenum}p" "$file" | grep -q "// LINT:ALLOW blocking"; then
            BLOCKING_VIOLATIONS="${BLOCKING_VIOLATIONS}${line}\n"
        fi
    done <<< "$BLOCKING_CALLS"
    
    if [ -n "$BLOCKING_VIOLATIONS" ]; then
        echo -e "${RED}ERROR: Blocking operations detected!${NC}"
        echo ""
        echo -e "$BLOCKING_VIOLATIONS"
        echo ""
        echo -e "${YELLOW}Architecture rule: NEVER use blocking operations${NC}"
        echo "  - Use to_ms_since_boot(get_absolute_time()) for timing"
        echo "  - Implement time-based state machines"
        echo "  - Add '// LINT:ALLOW blocking' comment to suppress this error for exceptional cases"
        echo ""
        ERRORS=$((ERRORS + 1))
    else
        echo -e "${GREEN}✓ All blocking operations have LINT:ALLOW annotation${NC}"
    fi
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
echo -e "${BLUE}[7/14] Checking for IRQ-shared variables...${NC}"

# Find files with __isr functions
ISR_FILES=$(grep -R --files-with-matches "${EXCLUDE_DIRS[@]}" --exclude="*.md" "__isr" src/ 2>/dev/null || true)

VOLATILE_ISSUES=""
BARRIER_ISSUES=""

if [ -n "$ISR_FILES" ]; then
    for file in $ISR_FILES; do
        # Look for static variables that aren't volatile (potential IRQ-shared variables)
        # Pattern: "static" followed by type, then variable name, but NOT containing "volatile"
        NON_VOLATILE_STATIC=$(grep -n "^static " "$file" | grep -v "volatile" | grep -v "const" | grep -v "inline" | grep -E 'static (uint|int|bool|unsigned|_Atomic)' || true)
        
        if [ -n "$NON_VOLATILE_STATIC" ]; then
            # Check if these variables might be accessed in ISR (heuristic: same name appears in file)
            while IFS= read -r line; do
                # Extract variable name (last word before = or ;)
                var_name=$(echo "$line" | awk -F'[=;]' '{print $1}' | awk '{print $NF}' | sed 's/[*]//g')
                line_num=$(echo "$line" | cut -d: -f1)
                
                # Check for LINT:ALLOW annotation on this line
                if sed -n "${line_num}p" "$file" | grep -q "// LINT:ALLOW non-volatile"; then
                    continue  # Skip this variable - it's annotated
                fi
                
                # Check if variable name appears in an __isr function
                # Get all __isr function names
                ISR_FUNCTION_NAMES=$(grep -o "__isr [a-zA-Z_][a-zA-Z0-9_]*" "$file" | awk '{print $2}')
                
                for isr_func in $ISR_FUNCTION_NAMES; do
                    # Check if variable is referenced in this ISR function
                    # Use grep -n to find function start line (avoids regex metacharacter issues)
                    func_start=$(grep -n "^void __isr ${isr_func}" "$file" | head -1 | cut -d: -f1)
                    
                    if [ -n "$func_start" ]; then
                        # Extract from function start to first closing brace at column 1
                        ISR_BODY=$(sed -n "${func_start},/^}/p" "$file")
                        if echo "$ISR_BODY" | grep -qw "$var_name"; then
                            VOLATILE_ISSUES="${VOLATILE_ISSUES}${file}:${line_num}: static ${var_name} (accessed in ${isr_func})\n"
                            break
                        fi
                    fi
                done
            done <<< "$NON_VOLATILE_STATIC"
        fi
        
        # Check for volatile variables without __dmb() in the file
        HAS_VOLATILE=$(grep -q "volatile" "$file" && echo "yes" || echo "no")
        HAS_BARRIER=$(grep -q "__dmb()" "$file" && echo "yes" || echo "no")
        
        if [ "$HAS_VOLATILE" = "yes" ] && [ "$HAS_BARRIER" = "no" ]; then
            BARRIER_ISSUES="${BARRIER_ISSUES}${file}: has volatile variables but no __dmb() barriers\n"
        elif [ "$HAS_VOLATILE" = "yes" ] && [ "$HAS_BARRIER" = "yes" ]; then
            # File has volatile and barriers, but check if EVERY volatile write has a nearby barrier
            # Get volatile variable declarations (extract names)
            VOLATILE_VAR_NAMES=$(grep -o "volatile[[:space:]]\+[a-zA-Z_][a-zA-Z0-9_]*[[:space:]]\+\*\?[a-zA-Z_][a-zA-Z0-9_]*" "$file" | awk '{print $NF}' | sed 's/\*//g' | sort -u)
            
            # For each volatile variable, find writes and check for nearby __dmb()
            while IFS= read -r var; do
                [ -z "$var" ] && continue
                
                # Find lines where this variable is written (assigned, not declared)
                # Pattern: variable = something (exclude comparisons ==, !=, <=, >=)
                # Exclude lines with "volatile" keyword (declarations) and LINT:ALLOW annotations
                WRITE_LINES=$(grep -n "[^a-zA-Z_!<>=]${var}[[:space:]]*=[^=]" "$file" | grep -v "volatile" | grep -v "LINT:ALLOW" | cut -d: -f1)
                
                for write_line in $WRITE_LINES; do
                    # Check if line has LINT:ALLOW annotation (using awk for single file read)
                    lint_allow=$(awk "BEGIN {found=0} NR==${write_line} && /LINT:ALLOW/ {found=1} END {print found}" "$file")
                    if [ "$lint_allow" = "1" ]; then
                        continue
                    fi
                    
                    # Check if __dmb() appears within next 3 lines (using awk to read once)
                    has_barrier_after=$(awk "BEGIN {found=0} NR>=${write_line} && NR<=$((write_line + 3)) && /__dmb\(\)/ {found=1} END {print found}" "$file")
                    
                    if [ "$has_barrier_after" = "0" ]; then
                        # One more check: see if this file has barriers before reads (read-side protection)
                        # Look for patterns like: __dmb(); ... var_name (reading the variable)
                        # If found, the write-side doesn't strictly need barriers (read-side protects)
                        has_read_barrier=false
                        
                        # Search for __dmb() followed within 5 lines by a read of this variable
                        # This is a heuristic - if we find it, assume read-side protection exists
                        dmb_lines=$(grep -n "__dmb()" "$file" | cut -d: -f1)
                        for dmb_line in $dmb_lines; do
                            # Check next 5 lines after __dmb() for variable read (using awk)
                            # Use flag variable to avoid exit/END block issues
                            read_found=$(awk "BEGIN {found=0} NR>$dmb_line && NR<=$((dmb_line + 5)) && /([^a-zA-Z0-9_]|^)${var}([^a-zA-Z0-9_]|$)/ {found=1} END {print found}" "$file")
                            if [ "$read_found" = "1" ]; then
                                has_read_barrier=true
                                break
                            fi
                        done
                        
                        if [ "$has_read_barrier" = false ]; then
                            BARRIER_ISSUES="${BARRIER_ISSUES}${file}:${write_line}: volatile '${var}' write missing __dmb() barrier\n"
                        fi
                    fi
                done
            done <<< "$VOLATILE_VAR_NAMES"
        fi
    done
fi

ISSUES_FOUND=false

if [ -n "$VOLATILE_ISSUES" ]; then
    echo -e "${YELLOW}⚠ Potential missing 'volatile' on IRQ-shared variables:${NC}"
    echo ""
    echo -e "$VOLATILE_ISSUES" | head -10
    [ "$(echo -e "$VOLATILE_ISSUES" | wc -l)" -gt 10 ] && echo "... and more"
    echo ""
    ISSUES_FOUND=true
fi

if [ -n "$BARRIER_ISSUES" ]; then
    echo -e "${YELLOW}⚠ Missing memory barriers (__dmb()):${NC}"
    echo ""
    echo -e "$BARRIER_ISSUES"
    echo ""
    ISSUES_FOUND=true
fi

if [ "$ISSUES_FOUND" = true ]; then
    echo -e "${YELLOW}IRQ safety rules:${NC}"
    echo "  - Static variables accessed in __isr functions must be 'volatile'"
    echo "  - Use __dmb() after volatile writes (in ISR) and before reads (in main)"
    echo "  - Add '// LINT:ALLOW non-volatile' for write-once variables"
    echo "  - See code-standards.md for examples"
    echo ""
    WARNINGS=$((WARNINGS + 1))
else
    echo -e "${GREEN}✓ No obvious IRQ variable safety issues detected${NC}"
fi
echo ""

# Check 8: Tab Characters
echo -e "${BLUE}[8/14] Checking for tab characters...${NC}"
TAB_USAGE=$(grep -R -E --line-number "${EXCLUDE_DIRS[@]}" --exclude="*.md" $'^\t|[^\t]\t' src/ 2>/dev/null || true)

if [ -n "$TAB_USAGE" ]; then
    echo -e "${RED}ERROR: Tab characters found!${NC}"
    echo ""
    echo "$TAB_USAGE" | head -20
    [ "$(echo "$TAB_USAGE" | wc -l)" -gt 20 ] && echo "... and more"
    echo ""
    echo -e "${YELLOW}Code formatting rule: Use 4 spaces, never tabs${NC}"
    echo "  - Maintains consistent indentation across editors"
    echo "  - Run: expand -t 4 <file> to convert tabs to spaces"
    echo ""
    ERRORS=$((ERRORS + 1))
else
    echo -e "${GREEN}✓ No tab characters found${NC}"
fi
echo ""

# Check 9: Header Guards in .h Files
echo -e "${BLUE}[9/14] Checking header guards...${NC}"
MISSING_GUARDS=""
HEADER_FILES=$(find src/ -name "*.h" -not -path "*/external/*" -not -path "*/build/*" 2>/dev/null)

for header in $HEADER_FILES; do
    # Skip very small files or generated files
    [ ! -s "$header" ] && continue
    grep -q "\.pio\.h$" <<< "$header" && continue
    
    # Check for #ifndef and #define guard pattern
    if ! grep -q "^#ifndef.*_H" "$header" || ! grep -q "^#define.*_H" "$header"; then
        MISSING_GUARDS="${MISSING_GUARDS}${header}\n"
    fi
done

if [ -n "$MISSING_GUARDS" ]; then
    echo -e "${YELLOW}WARNING: Header files missing include guards:${NC}"
    echo ""
    echo -e "$MISSING_GUARDS"
    echo ""
    echo -e "${YELLOW}Code organization rule: All .h files need include guards${NC}"
    echo "  - Format: #ifndef FILENAME_H / #define FILENAME_H"
    echo "  - Prevents multiple inclusion errors"
    echo ""
    WARNINGS=$((WARNINGS + 1))
else
    echo -e "${GREEN}✓ All header files have include guards${NC}"
fi
echo ""

# Check 10: File Headers (GPL License)
echo -e "${BLUE}[10/14] Checking file headers...${NC}"
MISSING_HEADERS=""
MIT_LICENSED_FILES=(
    "src/common/lib/usb_descriptors.c"
    "src/common/lib/usb_descriptors.h"
    "src/common/lib/tusb_config.h"
)

SOURCE_FILES=$(find src/ \( -name "*.c" -o -name "*.h" \) -not -path "*/external/*" -not -path "*/build/*" 2>/dev/null | grep -v "\.pio\.h$")

for file in $SOURCE_FILES; do
    # Skip known MIT-licensed files (TinyUSB-derived)
    skip=false
    for mit_file in "${MIT_LICENSED_FILES[@]}"; do
        if [[ "$file" == "$mit_file" ]]; then
            # Verify MIT license is present
            if ! head -25 "$file" | grep -q "The MIT License (MIT)"; then
                MISSING_HEADERS="${MISSING_HEADERS}${file} (expected MIT license)\n"
            fi
            skip=true
            break
        fi
    done
    
    # Check for GPL header on non-MIT files
    if [ "$skip" = false ]; then
        if ! head -25 "$file" | grep -q "Copyright.*Paul Bramhall"; then
            MISSING_HEADERS="${MISSING_HEADERS}${file}\n"
        fi
    fi
done

if [ -n "$MISSING_HEADERS" ]; then
    echo -e "${YELLOW}WARNING: Files missing proper license header:${NC}"
    echo ""
    echo -e "$MISSING_HEADERS"
    echo ""
    echo -e "${YELLOW}Documentation rule: All source files need appropriate license header${NC}"
    echo "  - GPL files: See code-standards.md for template"
    echo "  - MIT files (TinyUSB): usb_descriptors.[ch], tusb_config.h"
    echo ""
    WARNINGS=$((WARNINGS + 1))
else
    echo -e "${GREEN}✓ All source files have proper headers${NC}"
fi
echo ""

# Check 11: Naming Conventions (camelCase detection)
echo -e "${BLUE}[11/14] Checking naming conventions...${NC}"
CAMEL_CASE=$(grep -R --line-number "${EXCLUDE_DIRS[@]}" --exclude="*.md" -E "^[a-z]+[A-Z][a-zA-Z]*\s*\(|^static\s+[a-z]+[A-Z][a-zA-Z]*\s+[a-z]" src/ 2>/dev/null | grep -v "typedef" | head -20 || true)

if [ -n "$CAMEL_CASE" ]; then
    echo -e "${YELLOW}⚠ Possible camelCase usage detected (expecting snake_case):${NC}"
    echo ""
    echo "$CAMEL_CASE"
    echo ""
    echo -e "${YELLOW}Naming convention: Use snake_case for functions/variables${NC}"
    echo "  - Functions: keyboard_interface_task()"
    echo "  - Variables: scancode_buffer"
    echo "  - Manual review recommended"
    echo ""
    # Don't increment warnings - this is informational only (too many false positives)
else
    echo -e "${GREEN}✓ No obvious camelCase violations${NC}"
fi
echo ""

# Check 12: Include Order (comprehensive check)
echo -e "${BLUE}[12/14] Checking include order...${NC}"
WRONG_INCLUDE_ORDER=""
GROUPING_ISSUES=""
C_FILES=$(find src/ -name "*.c" -not -path "*/external/*" -not -path "*/build/*" 2>/dev/null)

for cfile in $C_FILES; do
    # Get the expected header name (e.g., foo.c -> foo.h)
    header_name=$(basename "$cfile" .c).h
    
    # Check if corresponding .h file exists
    expected_header=$(dirname "$cfile")/$header_name
    if [ -f "$expected_header" ]; then
        # Get first #include line
        first_include=$(grep -n "^#include" "$cfile" | head -1)
        
        # Check if it includes its own header
        if ! echo "$first_include" | grep -q "\"$header_name\""; then
            WRONG_INCLUDE_ORDER="${WRONG_INCLUDE_ORDER}${cfile}: First include should be \"${header_name}\"\n"
        else
            # Check for blank line after own header
            first_line_num=$(echo "$first_include" | cut -d: -f1)
            next_line_num=$((first_line_num + 1))
            next_line=$(sed -n "${next_line_num}p" "$cfile")
            
            # Next line should be blank or a comment, not immediately another include
            if echo "$next_line" | grep -q "^#include"; then
                WRONG_INCLUDE_ORDER="${WRONG_INCLUDE_ORDER}${cfile}: Missing blank line after own header include (line ${first_line_num})\n"
            fi
        fi
    fi
    
    # Check group ordering: Look for project headers before SDK headers (common mistake)
    # Note: This heuristic lumps standard library (<stdio.h>) and Pico SDK (pico/stdlib.h)
    # together as "SDK/standard headers", so it cannot distinguish violations between
    # groups 3 and 4 of the documented order. This is acceptable as a first-pass check.
    # Extract all includes (skip own header if present)
    includes=$(grep "^#include" "$cfile" 2>/dev/null || true)
    
    # Flag if project headers (quoted, not .pio.h) appear before pico/ or hardware/ includes
    found_project_header=false
    found_sdk_after_project=false
    
    while IFS= read -r line; do
        # Skip own header
        if [ -f "$expected_header" ] && echo "$line" | grep -q "\"$header_name\""; then
            continue
        fi
        
        # Check if this is a project header (quoted, but not .pio.h, bsp/, hardware/, or pico/)
        if echo "$line" | grep -q '^#include "' && ! echo "$line" | grep -q '\.pio\.h' && ! echo "$line" | grep -q 'bsp/' && ! echo "$line" | grep -q 'hardware/' && ! echo "$line" | grep -q 'pico/'; then
            found_project_header=true
        fi
        
        # Check if SDK header appears after we've seen project headers
        if [ "$found_project_header" = true ]; then
            if echo "$line" | grep -Eq '#include.*(pico/|hardware/|<.*\.h>)'; then
                found_sdk_after_project=true
                break
            fi
        fi
    done <<< "$includes"
    
    if [ "$found_sdk_after_project" = true ]; then
        GROUPING_ISSUES="${GROUPING_ISSUES}${cfile}: SDK/standard headers appear after project headers\n"
    fi
done

ISSUES_FOUND=false

if [ -n "$WRONG_INCLUDE_ORDER" ]; then
    echo -e "${YELLOW}⚠ Include order violations:${NC}"
    echo ""
    echo -e "$WRONG_INCLUDE_ORDER" | head -10
    [ "$(echo -e "$WRONG_INCLUDE_ORDER" | wc -l)" -gt 10 ] && echo "... and more"
    echo ""
    ISSUES_FOUND=true
fi

if [ -n "$GROUPING_ISSUES" ]; then
    echo -e "${YELLOW}⚠ Include grouping violations:${NC}"
    echo ""
    echo -e "$GROUPING_ISSUES" | head -10
    [ "$(echo -e "$GROUPING_ISSUES" | wc -l)" -gt 10 ] && echo "... and more"
    echo ""
    ISSUES_FOUND=true
fi

if [ "$ISSUES_FOUND" = true ]; then
    echo -e "${YELLOW}Include order standard (code-standards.md):${NC}"
    echo "  1. Own header first (for .c files)"
    echo "  2. Blank line"
    echo "  3. Standard library headers (<stdio.h>, <stdint.h>, etc.)"
    echo "  4. Pico SDK headers (pico/stdlib.h, hardware/*, etc.)"
    echo "  5. External library headers (tusb.h, bsp/*, etc.)"
    echo "  6. Project headers (config.h, ringbuf.h, etc.)"
    echo "  - Blank lines between groups"
    echo "  - Alphabetical within groups"
    echo ""
    WARNINGS=$((WARNINGS + 1))
else
    echo -e "${GREEN}✓ Include order looks correct${NC}"
fi
echo ""

# Check 13: Missing __isr Attribute
echo -e "${BLUE}[13/14] Checking interrupt handler attributes...${NC}"
MISSING_ISR=$(grep -R --line-number "${EXCLUDE_DIRS[@]}" --exclude="*.md" -E "void.*_irq_handler\s*\(|void.*_interrupt_handler\s*\(|void.*_isr\s*\(" src/ 2>/dev/null | grep -v "__isr" | grep -v "\.h:" || true)

if [ -n "$MISSING_ISR" ]; then
    echo -e "${YELLOW}WARNING: IRQ handlers possibly missing __isr attribute:${NC}"
    echo ""
    echo "$MISSING_ISR"
    echo ""
    echo -e "${YELLOW}Naming convention: IRQ handlers should use __isr attribute${NC}"
    echo "  - void __isr keyboard_irq_handler(void);"
    echo "  - Helps compiler optimize for interrupt context"
    echo ""
    WARNINGS=$((WARNINGS + 1))
else
    echo -e "${GREEN}✓ IRQ handlers properly attributed${NC}"
fi
echo ""

# Check 14: _Static_assert for Compile-Time Validation
# NOTE: This is an advisory-only check that doesn't affect pass/fail status
echo -e "${BLUE}[14/14] Checking for compile-time validation...${NC}"
STATIC_ASSERT_COUNT=$(grep -R "${EXCLUDE_DIRS[@]}" --exclude="*.md" -c "_Static_assert\|#error" src/ 2>/dev/null | grep -vc ":0$")

if [ "$STATIC_ASSERT_COUNT" -gt 0 ]; then
    echo -e "${GREEN}✓ Found $STATIC_ASSERT_COUNT files with compile-time checks${NC}"
else
    echo -e "${YELLOW}⚠ No _Static_assert or #error directives found${NC}"
    echo "  - Consider adding compile-time validation for constants"
    echo "  - See code-standards.md for examples"
fi
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
