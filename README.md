# LLVM Liveness Analysis Pass

[![CI](https://github.com/Antheagao/compiler-analysis-pass/actions/workflows/ci.yml/badge.svg)](https://github.com/Antheagao/compiler-analysis-pass/actions/workflows/ci.yml)

An LLVM compiler analysis pass that performs liveness analysis on functions. This pass computes liveness information for each basic block in a function's control flow graph (CFG), identifying which variables are live at different points in the program.

## Overview

Liveness analysis is a fundamental data-flow analysis technique used in compiler optimization. A variable is considered **live** at a point in the program if its value may be used before it is redefined. This pass computes:

- **UEVAR**: Upward-exposed variables (variables used before being killed in a basic block)
- **VARKILL**: Variables killed (defined) in a basic block
- **LIVEIN**: Variables live at the entry of a basic block
- **LIVEOUT**: Variables live at the exit of a basic block

## Features

- ✅ Handles all basic control flow structures (if/else, loops, branches)
- ✅ Uses iterative algorithm to handle back edges and loops
- ✅ Supports local variables of primitive data types
- ✅ Handles arithmetic operations (+, -, *, /)
- ✅ Processes comparison and branching instructions (icmp, br)

## Requirements

- **LLVM**: Version 17 or higher
- **CMake**: Version 3.20 or higher
- **C++ Compiler**: Supporting C++17 standard
- **Clang**: For compiling test files to LLVM IR

## Building

1. **Set up the build directory:**
   ```bash
   mkdir -p build
   cd build
   ```

2. **Configure CMake:**
   ```bash
   cmake .. -DLT_LLVM_INSTALL_DIR=<path-to-llvm-installation>
   ```
   
   For example, if LLVM is installed at `/usr/local/llvm`:
   ```bash
   cmake .. -DLT_LLVM_INSTALL_DIR=/usr/local/llvm
   ```

3. **Build the pass:**
   ```bash
   make
   ```

   This will create a shared library:
   - Linux: `build/lib/libLivenessAnalysis.so`
   - macOS: `build/lib/libLivenessAnalysis.dylib`
   - Windows: `build/lib/libLivenessAnalysis.dll`

## Usage

### Using with `opt`

Once built, you can use the pass with LLVM's `opt` tool:

```bash
opt -load-pass-plugin=./build/lib/libLivenessAnalysis.so \
    -passes="liveness-analysis" \
    -disable-output \
    <input-llvm-file>
```

### Example

1. **Compile a C file to LLVM IR:**
   ```bash
   clang -S -fno-discard-value-names -emit-llvm -c test.c -o test.ll
   ```

2. **Run the analysis pass:**
   ```bash
   opt -load-pass-plugin=./build/lib/libLivenessAnalysis.so \
       -passes="liveness-analysis" \
       -disable-output \
       test.ll
   ```

3. **View the output:**
   The pass will print liveness information for each basic block:
   ```
   ----- entry -----
   UEVAR: a b c 
   VARKILL: e 
   LIVEIN: a b c 
   LIVEOUT: a c e 
   ----- if.then -----
   UEVAR: a 
   VARKILL: e 
   LIVEIN: a c 
   LIVEOUT: a c e 
   ...
   ```

## Running Tests

The repository includes golden-file regression tests in the `tests/` directory: each
committed `.ll` input has a committed expected output in `tests/expected/`, and the
suite diffs the pass's actual output against it — any mismatch fails the run with a
nonzero exit code (this is what CI runs on every push).

```bash
cd tests
chmod +x run_tests.sh
./run_tests.sh
```

If your `opt` binary isn't on `PATH` under that name, point at it explicitly:

```bash
OPT=/usr/lib/llvm-17/bin/opt ./run_tests.sh
```

Development flags:

- `./run_tests.sh --bless` — overwrite the expected outputs with the current pass
  output (after an intentional output change)
- `./run_tests.sh --regen-ir` — recompile the `.ll` inputs from the `.c` sources
  (a different clang version may change block/value names, so a `--bless` usually
  follows)

The pass prints each set's variables sorted by name, so the output is deterministic
across runs, platforms, and LLVM versions — that's what makes byte-exact golden
files viable.

### Test Cases

- **test1.c**: Simple if/else branching
- **test2.c**: Do-while loop with nested if/else
- **test3.c**: For loop with conditional branching
- **test4.c**: Nested loops (for and while)

## Project Structure

```
compiler-analysis-pass/
├── .github/workflows/ci.yml # Build + golden-file tests on every push
├── CMakeLists.txt           # Build configuration
├── LICENSE                  # MIT
├── README.md                # This file
├── src/
│   └── LivenessAnalysis.cpp # Main pass implementation
└── tests/
    ├── run_tests.sh         # Golden-file test runner (nonzero exit on failure)
    ├── expected/            # Committed expected outputs (one per test)
    ├── test1.c / test1.ll   # Simple if/else branching
    ├── test2.c / test2.ll   # Do-while loop with nested if/else
    ├── test3.c / test3.ll   # For loop with conditional branching
    └── test4.c / test4.ll   # Nested loops (for and while)
```

## Algorithm

The pass implements a standard iterative liveness analysis algorithm:

1. **Initialization**: For each basic block, compute:
   - `UEVAR`: Variables used before being killed
   - `VARKILL`: Variables defined (killed) in the block

2. **Iterative Fixpoint Computation**:
   - `LIVEOUT[B] = ∪ LIVEIN[S]` for all successors S of B
   - `LIVEIN[B] = UEVAR[B] ∪ (LIVEOUT[B] - VARKILL[B])`
   - Repeat until no changes occur (fixpoint reached)

3. **Output**: Display the computed sets for each basic block

The algorithm handles back edges naturally through the iterative approach, ensuring correct results even for complex control flow graphs with loops.

## Limitations

The current implementation handles:
- ✅ Local variables of primitive types
- ✅ Basic arithmetic operations (+, -, *, /)
- ✅ Comparison and branching instructions
- ✅ All control flow structures (if/else, loops)

It does **not** currently handle:
- ❌ Global variables
- ❌ Function calls
- ❌ Pointers and memory operations beyond basic load/store
- ❌ Complex data types (arrays, structs)

## License

MIT License

## Contributing

Contributions are welcome! Please feel free to submit issues or pull requests.

## References

- [LLVM Pass Infrastructure](https://llvm.org/docs/WritingAnLLVMNewPMPass.html)
- [Liveness Analysis](https://en.wikipedia.org/wiki/Live-variable_analysis)
- [Data Flow Analysis](https://en.wikipedia.org/wiki/Data-flow_analysis)
