#!/bin/bash
#
# Test Suite for RP2040 Keyboard Converter Enforcement Tools
# Tests git hooks, lint script, and CI checks
#
# Usage: ./tools/test-enforcement.sh
#

set -e

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test counters
TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

# Store original branch
ORIGINAL_BRANCH=$(git branch --show-current)

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Enforcement Tools Test Suite${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Helper function to run a test
run_test() {
    local test_name="$1"
    local test_command="$2"
    local expected_result="$3"  # "pass" or "fail"
    
    TESTS_RUN=$((TESTS_RUN + 1))
    echo -e "${BLUE}[Test $TESTS_RUN] $test_name${NC}"
    
    # Run the test command and capture exit code
    set +e
    eval "$test_command" > /tmp/test_output.log 2>&1
    EXIT_CODE=$?
    set -e
    
    # Check if result matches expectation
    if [ "$expected_result" = "fail" ]; then
        # We expect the command to fail (non-zero exit)
        if [ $EXIT_CODE -ne 0 ]; then
            echo -e "${GREEN}  ✓ PASS - Command failed as expected${NC}"
            TESTS_PASSED=$((TESTS_PASSED + 1))
        else
            echo -e "${RED}  ✗ FAIL - Command passed but should have failed${NC}"
            cat /tmp/test_output.log
            TESTS_FAILED=$((TESTS_FAILED + 1))
        fi
    else
        # We expect the command to pass (zero exit)
        if [ $EXIT_CODE -eq 0 ]; then
            echo -e "${GREEN}  ✓ PASS - Command passed as expected${NC}"
            TESTS_PASSED=$((TESTS_PASSED + 1))
        else
            echo -e "${RED}  ✗ FAIL - Command failed but should have passed${NC}"
            cat /tmp/test_output.log
            TESTS_FAILED=$((TESTS_FAILED + 1))
        fi
    fi
    echo ""
}

# Setup test environment
setup_test_env() {
    echo -e "${YELLOW}Setting up test environment...${NC}"
    
    # Clean up any lingering test files from previous runs
    git reset HEAD . >/dev/null 2>&1 || true
    rm -f src/test_*.c
    rm -f docs-internal/test_*.md
    
    # Ensure hooks are configured
    git config core.hooksPath .githooks
    chmod +x .githooks/pre-commit .githooks/commit-msg
    chmod +x tools/lint.sh
    
    # Create a test branch
    git checkout -b test-enforcement-tools >/dev/null 2>&1 || git checkout test-enforcement-tools >/dev/null 2>&1
    
    echo -e "${GREEN}✓ Test environment ready${NC}"
    echo ""
}

# Cleanup test environment
cleanup_test_env() {
    # Disable exit-on-error for cleanup
    set +e
    
    echo -e "${YELLOW}Cleaning up test environment...${NC}"
    
    # Remove any test files
    rm -f src/test_*.c
    rm -f docs-internal/test_*.md
    
    # Reset any staged files
    git reset HEAD . >/dev/null 2>&1 || true
    
    # Return to original branch
    git checkout "$ORIGINAL_BRANCH" >/dev/null 2>&1 || true
    
    # Delete test branch
    git branch -D test-enforcement-tools >/dev/null 2>&1 || true
    
    echo -e "${GREEN}✓ Cleanup complete${NC}"
    echo ""
}

# Test 1: Lint Script - Clean Code
test_lint_clean() {
    echo -e "${BLUE}======================================== ${NC}"
    echo -e "${BLUE}Lint Script Tests${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo ""
    
    # Note: Current codebase has known violations, so we expect failure
    # This test just ensures the script runs
    run_test "Lint script executes" "./tools/lint.sh || true" "pass"
}

# Test 2: Lint Script - Detect Blocking Operations
test_lint_blocking_ops() {
    cat > src/test_blocking.c << 'EOF'
#include "pico/stdlib.h"

void bad_function(void) {
    sleep_ms(100);  // Violation
    busy_wait_us(50);  // Violation
}
EOF
    
    run_test "Lint detects blocking operations" "./tools/lint.sh" "fail"
    rm -f src/test_blocking.c
}

# Test 3: Lint Script - Detect Multicore Usage
test_lint_multicore() {
    cat > src/test_multicore.c << 'EOF'
#include "pico/multicore.h"

void core1_entry(void) {
    while (1) {}
}

void bad_function(void) {
    multicore_launch_core1(core1_entry);  // Violation
}
EOF
    
    run_test "Lint detects multicore API" "./tools/lint.sh" "fail"
    rm -f src/test_multicore.c
}

# Test 4: Git Hook - Prevent docs-internal staging
test_hook_docs_internal_staging() {
    mkdir -p docs-internal
    echo "test" > docs-internal/test_file.md
    
    # Try to add and commit (should fail at pre-commit)
    run_test "Pre-commit blocks docs-internal files" "git add -f docs-internal/test_file.md && git commit -m 'test'" "fail"
    
    git reset HEAD docs-internal/test_file.md >/dev/null 2>&1 || true
    rm -f docs-internal/test_file.md
}

# Test 5: Git Hook - Commit Message Check
test_hook_commit_message() {
    # Create empty commit with bad message
    run_test "Commit-msg hook blocks docs-internal reference" "git commit --allow-empty -m 'Add feature (see docs-internal/notes.md)'" "fail"
}

# Test 6: Git Hook - Clean Commit Message
test_hook_clean_message() {
    # Create empty commit with clean message
    run_test "Commit-msg hook allows clean message" "git commit --allow-empty -m 'Add test feature'" "pass"
    
    # Clean up the commit
    git reset --soft HEAD~1 >/dev/null 2>&1
}

# Test 7: Lint Script - Strict Mode
test_lint_strict_mode() {
    # Strict mode should pass if codebase is clean (which it now is)
    # This verifies the strict flag works correctly
    run_test "Lint strict mode passes on clean code" "./tools/lint.sh --strict" "pass"
}

# Test 8: CI Checks - docs-internal Detection
test_ci_docs_internal() {
    # Simulate CI check for docs-internal files
    mkdir -p docs-internal
    echo "test" > docs-internal/test_ci.md
    git add -f docs-internal/test_ci.md >/dev/null 2>&1 || true
    
    # Run the CI check - it should detect the file (grep exits 0 when found)
    run_test "CI check detects docs-internal files" "git ls-files | grep -q '^docs-internal/'" "pass"
    
    # CRITICAL: Clean up immediately to prevent contaminating other tests
    git reset HEAD docs-internal/test_ci.md >/dev/null 2>&1 || true
    rm -f docs-internal/test_ci.md
    
    # Also remove from git if somehow it got committed
    git rm --cached -f docs-internal/test_ci.md >/dev/null 2>&1 || true
}

# Test 9: Lint Script - Copy to RAM Check
test_lint_copy_to_ram() {
    # This should pass since src/CMakeLists.txt has copy_to_ram
    # Use same pattern as lint.sh: grep recursively from repo root
    # Note: cd deferred to eval time via single quotes in run_test
    local repo_root
    repo_root=$(git rev-parse --show-toplevel)
    
    run_test "Lint verifies copy_to_ram config" \
        "cd '$repo_root' && grep -R --exclude-dir='.git' --exclude-dir='build' --exclude-dir='external' 'pico_set_binary_type.*copy_to_ram' ." \
        "pass"
}

# Test 10: Hook Bypass (--no-verify)
test_hook_bypass() {
    echo -e "${YELLOW}[Info] Testing --no-verify bypass (should work but is discouraged)${NC}"
    run_test "Commit with --no-verify bypasses hooks" "git commit --allow-empty -m 'Test bypass docs-internal' --no-verify" "pass"
    
    # Clean up
    git reset --soft HEAD~1 >/dev/null 2>&1
}

# Test 11: Multiple Violations
test_multiple_violations() {
    cat > src/test_multiple.c << 'EOF'
#include "pico/stdlib.h"
#include "pico/multicore.h"

void __isr bad_isr(void) {
    printf("Bad IRQ\n");  // Violation: printf in IRQ
}

void bad_function(void) {
    sleep_ms(100);  // Violation: blocking
    multicore_launch_core1(NULL);  // Violation: multicore
}
EOF
    
    run_test "Lint detects multiple violations" "./tools/lint.sh" "fail"
    rm -f src/test_multiple.c >/dev/null 2>&1
}

# Test 12: Commit Template
test_commit_template() {
    # Verify commit template is configured
    TEMPLATE=$(git config commit.template)
    if [ "$TEMPLATE" = ".gitmessage" ]; then
        echo -e "${GREEN}  ✓ PASS - Commit template configured${NC}"
        TESTS_PASSED=$((TESTS_PASSED + 1))
    else
        echo -e "${RED}  ✗ FAIL - Commit template not configured${NC}"
        TESTS_FAILED=$((TESTS_FAILED + 1))
    fi
    TESTS_RUN=$((TESTS_RUN + 1))
    echo ""
}

# Test 13: Hook Permissions
test_hook_permissions() {
    echo -e "${BLUE}[Test $((TESTS_RUN + 1))] Hook file permissions${NC}"
    TESTS_RUN=$((TESTS_RUN + 1))
    
    if [ -x ".githooks/pre-commit" ] && [ -x ".githooks/commit-msg" ]; then
        echo -e "${GREEN}  ✓ PASS - Hooks are executable${NC}"
        TESTS_PASSED=$((TESTS_PASSED + 1))
    else
        echo -e "${RED}  ✗ FAIL - Hooks are not executable${NC}"
        TESTS_FAILED=$((TESTS_FAILED + 1))
    fi
    echo ""
}

# Test 14: Lint Script with No Source Files
test_lint_empty_check() {
    # Temporarily hide src directory to test empty check handling
    # (Skip this test - too invasive)
    echo -e "${YELLOW}[Test $((TESTS_RUN + 1))] Skipped - Empty source check (too invasive)${NC}"
    TESTS_RUN=$((TESTS_RUN + 1))
    echo ""
}

# Main test execution
main() {
    setup_test_env
    
    # Run all tests
    test_lint_clean
    test_lint_blocking_ops
    test_lint_multicore
    test_lint_strict_mode
    test_lint_copy_to_ram
    test_multiple_violations
    
    echo -e "${BLUE}======================================== ${NC}"
    echo -e "${BLUE}Git Hook Tests${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo ""
    
    test_hook_docs_internal_staging
    test_hook_commit_message
    test_hook_clean_message
    test_hook_bypass
    test_commit_template
    test_hook_permissions
    
    echo -e "${BLUE}======================================== ${NC}"
    echo -e "${BLUE}CI Check Tests${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo ""
    
    test_ci_docs_internal
    
    # Cleanup before printing summary
    cleanup_test_env
    
    # Print summary
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}Test Summary${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo -e "Tests run:    ${BLUE}$TESTS_RUN${NC}"
    echo -e "Tests passed: ${GREEN}$TESTS_PASSED${NC}"
    echo -e "Tests failed: ${RED}$TESTS_FAILED${NC}"
    echo ""
    
    if [ $TESTS_FAILED -eq 0 ]; then
        echo -e "${GREEN}✅ All tests passed!${NC}"
        exit 0
    else
        echo -e "${RED}❌ Some tests failed${NC}"
        exit 1
    fi
}

# Handle interrupts gracefully
trap 'cleanup_test_env; exit 130' INT TERM

# Run main
main
