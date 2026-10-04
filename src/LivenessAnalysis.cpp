//=============================================================================
// FILE:
//    LivenessAnalysis.cpp
//
// DESCRIPTION:
//    An LLVM analysis pass that performs liveness analysis on functions.
//    For each basic block, it computes:
//    - UEVAR: upward-exposed variables (used before killed)
//    - VARKILL: variables killed (defined) in the block
//    - LIVEIN: variables live at the entry of the block
//    - LIVEOUT: variables live at the exit of the block
//
//    The pass uses an iterative algorithm to handle back edges and loops.
//
// USAGE:
//    New PM
//      opt -load-pass-plugin=libLivenessAnalysis.dylib -passes="liveness-analysis" `\`
//        -disable-output <input-llvm-file>
//
//
// License: MIT
//=============================================================================
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

#include <algorithm>
#include <deque>
#include <set>
#include <map>
#include <string>
#include <vector>

using namespace llvm;

//-----------------------------------------------------------------------------
// LivenessAnalysis implementation
//-----------------------------------------------------------------------------
// No need to expose the internals of the pass to the outside world - keep
// everything in an anonymous namespace.
namespace {

// Info struct to store summary sets for each basic block
struct Info {
  std::set<Value*> ueVar;
  std::set<Value*> varKill;
  std::set<Value*> liveIn;
  std::set<Value*> liveOut;
};

// Set function to check if a value is in a set
bool setContains(std::set<Value*>& s, Value* v) {
  return s.find(v) != s.end();
}

// Set function to calculate the union of two sets
void setUnion(std::set<Value*>& s1, std::set<Value*>& s2) {
  for (Value* v : s2) {
    s1.insert(v);
  }
}

// Set function to calculate the difference of two sets
void setDifference(std::set<Value*>& s1, std::set<Value*>& s2) {
  for (Value* v : s2) {
    s1.erase(v);
  }
}

// Print function to display a summary set in a stable order.
// The sets are keyed by Value* (heap addresses), so iterating them directly
// prints in allocator order -- which is not guaranteed to be the same across
// runs, platforms, or LLVM versions. Sorting by name makes the output
// deterministic, which the golden-file tests in tests/ rely on.
void printSet(const char* label, std::set<Value*>& s) {
  std::vector<std::string> names;
  for (Value* v : s) {
    if (v->hasName()) {
      names.push_back(v->getName().str());
    }
  }
  std::sort(names.begin(), names.end());
  errs() << label << ": ";
  for (std::string& name : names) {
    errs() << name << " ";
  }
  errs() << "\n";
}

/*
  Implement the basic liveness analysis as an LLVM pass, 
  with the following specifications:
    - given a C function, your implementation finds the LiveOut sets
      for basic blocks in its CFG;
    - your implementation should be able to handle back edges, that is,
      an iterative algorithm or a
      worklist-based one should be used.
    - your implementation only needs to handle a basic scenario, where
      - all the variables are local variables;
      - all the variables are of primitive data types;
      - the operators in assignments only include +, -, *, /
      - the IR may include comparing and branching instructions, 
        like icmp and br, which may
        also ``use'' variables (and ``define'' variables)
*/
void visitor(Function &fn) {
  // Declare variables
  std::map<BasicBlock*, Info> blockInfo;

  // Collect summary sets (UEVAR, VARKILL) basic blocks => O(N*k)
  for (BasicBlock& block : fn) {
    for (Instruction& inst : block) {
      if (auto *storeInst = dyn_cast<StoreInst>(&inst)) {
        Value* op = storeInst->getPointerOperand();
        blockInfo[&block].varKill.insert(op);
      }
      else if (auto *loadInst = dyn_cast<LoadInst>(&inst)) {
        Value *op = loadInst->getPointerOperand();
        if (!setContains(blockInfo[&block].varKill, op)) {
          blockInfo[&block].ueVar.insert(op);
        }
      }
      else if (auto *binaryOp = dyn_cast<BinaryOperator>(&inst)) {
        Value* op0 = binaryOp->getOperand(0);
        Value* op1 = binaryOp->getOperand(1);
        if (!setContains(blockInfo[&block].varKill, op0)) {
          blockInfo[&block].ueVar.insert(op0);
        }
        if (!setContains(blockInfo[&block].varKill, op1)) {
          blockInfo[&block].ueVar.insert(op1);
        }    
      }
    }
  }

  // Perform basic iterative algorithm to get live out sets (LIVEIN, LIVEOUT)
  bool changed = true;
  while (changed) {
    changed = false;
    for (BasicBlock &block : fn) {
      // Calculate new liveOut based on successors liveIn
      std::set<Value*> newLiveOut;
      for (BasicBlock *succ : successors(&block)) {
        setUnion(newLiveOut, blockInfo[succ].liveIn);
      }

      // Update the new liveOut if it is different from the old liveOut
      if (newLiveOut != blockInfo[&block].liveOut) {
        blockInfo[&block].liveOut = newLiveOut;
        changed = true;
      }

      // Calculate liveIn = (LIVEOUT(x) - VARKILL(x)) U UEVar(x)
      std::set<Value*> newLiveIn = blockInfo[&block].ueVar;
      std::set<Value*> liveOutMinusVarKill = blockInfo[&block].liveOut;
      setDifference(liveOutMinusVarKill, blockInfo[&block].varKill);
      setUnion(newLiveIn, liveOutMinusVarKill);

      // Update the new liveIn if it is different from the old liveIn
      if (newLiveIn != blockInfo[&block].liveIn) {
        blockInfo[&block].liveIn = newLiveIn;
        changed = true;
      }
    }
  }

  // Display the summary sets and liveness results for each basic block
  for (BasicBlock& block : fn) {
    errs() << "----- " << block.getName() << " -----\n";
    printSet("UEVAR", blockInfo[&block].ueVar);
    printSet("VARKILL", blockInfo[&block].varKill);
    printSet("LIVEIN", blockInfo[&block].liveIn);
    printSet("LIVEOUT", blockInfo[&block].liveOut);
  }
}

// New PM implementation
struct LivenessAnalysis : PassInfoMixin<LivenessAnalysis> {
  // Main entry point, takes IR unit to run the pass on (&F) and the
  // corresponding pass manager (to be queried if need be)
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
    visitor(F);
    return PreservedAnalyses::all();
  }

  // Without isRequired returning true, this pass will be skipped for functions
  // decorated with the optnone LLVM attribute. Note that clang -O0 decorates
  // all functions with optnone.
  static bool isRequired() { return true; }
};
} // namespace

//-----------------------------------------------------------------------------
// New PM Registration
//-----------------------------------------------------------------------------
llvm::PassPluginLibraryInfo getLivenessAnalysisPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "LivenessAnalysis", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "liveness-analysis") {
                    FPM.addPass(LivenessAnalysis());
                    return true;
                  }
                  return false;
                });
          }};
}

// This is the core interface for pass plugins. It guarantees that 'opt' will
// be able to recognize LivenessAnalysis when added to the pass pipeline on the
// command line, i.e. via '-passes=liveness-analysis'
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getLivenessAnalysisPluginInfo();
}
