#!/bin/bash
# Golden-file test suite for the Liveness Analysis LLVM pass.
#
# Default mode runs the pass on the committed .ll files and diffs the output
# against the expected outputs in tests/expected/. Any mismatch fails the
# suite (nonzero exit), so CI can catch regressions.
#
# Flags (development only):
#   --bless      overwrite the expected outputs with the current pass output
#   --regen-ir   recompile the .ll files from the .c sources with clang
#                (the committed .ll files are the test inputs; regenerating
#                them with a different clang version may change block/value
#                names and require a --bless to follow)

set -u

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Configuration
BUILD_DIR="${BUILD_DIR:-../build}"
PLUGIN_NAME="libLivenessAnalysis"
OPT="${OPT:-opt}"
CLANG="${CLANG:-clang}"
TEST_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$TEST_DIR"

# Determine plugin extension based on OS
if [[ "$OSTYPE" == "darwin"* ]]; then
    PLUGIN_EXT=".dylib"
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
    PLUGIN_EXT=".so"
else
    PLUGIN_EXT=".dll"
fi

PLUGIN_PATH="${BUILD_DIR}/lib/${PLUGIN_NAME}${PLUGIN_EXT}"

BLESS=0
REGEN_IR=0
for arg in "$@"; do
    case "$arg" in
        --bless) BLESS=1 ;;
        --regen-ir) REGEN_IR=1 ;;
        *) echo "Unknown flag: $arg"; exit 2 ;;
    esac
done

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}  Liveness Analysis Test Suite${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""

# Check if plugin exists
if [ ! -f "$PLUGIN_PATH" ]; then
    echo -e "${RED}Error: Plugin not found at ${PLUGIN_PATH}${NC}"
    echo -e "${YELLOW}Please build the project first:${NC}"
    echo "  mkdir -p build && cd build"
    echo "  cmake .. -DLT_LLVM_INSTALL_DIR=<path-to-llvm>"
    echo "  make"
    exit 1
fi

# Optionally recompile test files to LLVM IR (development only; see header)
if [ "$REGEN_IR" -eq 1 ]; then
    echo -e "${YELLOW}Recompiling test files to LLVM IR...${NC}"
    for test_file in test*.c; do
        echo "  Compiling $test_file..."
        "$CLANG" -S -fno-discard-value-names -emit-llvm -c "$test_file" -o "${test_file%.c}.ll"
    done
    echo ""
fi

mkdir -p expected

# Run tests
failures=0
total=0
for test_ll in test*.ll; do
    test_name="${test_ll%.ll}"
    expected_file="expected/${test_name}.txt"
    actual_file="$(mktemp)"
    total=$((total + 1))

    # The pass prints to stderr; -disable-output suppresses the IR itself
    "$OPT" -load-pass-plugin "$PLUGIN_PATH" \
        -passes="liveness-analysis" \
        -disable-output \
        "$test_ll" 2> "$actual_file"
    opt_status=$?

    if [ "$opt_status" -ne 0 ]; then
        echo -e "${RED}FAIL${NC} ${test_name} (opt exited ${opt_status})"
        failures=$((failures + 1))
        rm -f "$actual_file"
        continue
    fi

    if [ "$BLESS" -eq 1 ]; then
        cp "$actual_file" "$expected_file"
        echo -e "${YELLOW}BLESSED${NC} ${test_name} -> ${expected_file}"
    elif [ ! -f "$expected_file" ]; then
        echo -e "${RED}FAIL${NC} ${test_name} (missing ${expected_file} -- run with --bless to create)"
        failures=$((failures + 1))
    elif diff -u "$expected_file" "$actual_file" > /tmp/diff_${test_name}.txt 2>&1; then
        echo -e "${GREEN}PASS${NC} ${test_name}"
    else
        echo -e "${RED}FAIL${NC} ${test_name} (output differs from ${expected_file}):"
        cat "/tmp/diff_${test_name}.txt"
        failures=$((failures + 1))
    fi
    rm -f "$actual_file"
done

echo ""
echo -e "${GREEN}========================================${NC}"
if [ "$BLESS" -eq 1 ]; then
    echo -e "${YELLOW}  Blessed ${total} expected outputs${NC}"
elif [ "$failures" -eq 0 ]; then
    echo -e "${GREEN}  All ${total} tests passed${NC}"
else
    echo -e "${RED}  ${failures}/${total} tests FAILED${NC}"
fi
echo -e "${GREEN}========================================${NC}"

exit $((failures > 0 ? 1 : 0))
