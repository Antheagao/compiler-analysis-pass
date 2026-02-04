#!/bin/bash
# Test script for Liveness Analysis LLVM Pass
# This script compiles test files and runs the analysis pass on them

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Configuration
BUILD_DIR="${BUILD_DIR:-../build}"
PLUGIN_NAME="libLivenessAnalysis"
TEST_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Determine plugin extension based on OS
if [[ "$OSTYPE" == "darwin"* ]]; then
    PLUGIN_EXT=".dylib"
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
    PLUGIN_EXT=".so"
else
    PLUGIN_EXT=".dll"
fi

PLUGIN_PATH="${BUILD_DIR}/lib/${PLUGIN_NAME}${PLUGIN_EXT}"

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

# Recompile test files to LLVM IR
echo -e "${YELLOW}Compiling test files to LLVM IR...${NC}"
for test_file in test*.c; do
    if [ -f "$test_file" ]; then
        echo "  Compiling $test_file..."
        clang -S -fno-discard-value-names -emit-llvm -c "$test_file" -o "${test_file%.c}.ll"
    fi
done
echo ""

# Run tests
test_num=1
for test_ll in test*.ll; do
    if [ -f "$test_ll" ]; then
        test_name="${test_ll%.ll}"
        echo -e "${GREEN}***********************************************************${NC}"
        echo -e "${GREEN}*                      Test ${test_num} Output                      *${NC}"
        echo -e "${GREEN}*                      (${test_name})                      *${NC}"
        echo -e "${GREEN}***********************************************************${NC}"
        echo ""
        
        opt -load-pass-plugin "$PLUGIN_PATH" \
            -passes="liveness-analysis" \
            -disable-output \
            "$test_ll"
        
        echo ""
        test_num=$((test_num + 1))
    fi
done

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}  All tests completed!${NC}"
echo -e "${GREEN}========================================${NC}"
