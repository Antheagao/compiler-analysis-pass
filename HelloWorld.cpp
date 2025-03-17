//=============================================================================
// FILE:
//    HelloWorld.cpp
//
// DESCRIPTION:
//    Visits all functions in a module, prints their names and the number of
//    arguments via stderr. Strictly speaking, this is an analysis pass (i.e.
//    the functions are not modified). However, in order to keep things simple
//    there's no 'print' method here (every analysis pass should implement it).
//
// USAGE:
//    New PM
//      opt -load-pass-plugin=libHelloWorld.dylib -passes="hello-world" `\`
//        -disable-output <input-llvm-file>
//
//
// License: MIT
//=============================================================================
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

#include <deque>
#include <set>
#include <map>

using namespace llvm;

//-----------------------------------------------------------------------------
// HelloWorld implementation
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
        also ``use’’ variables (and ``define’’ variables)
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

  // Display the summary sets and liveOut for each basic block
  for (BasicBlock& block : fn) {
    errs() << "----- " << block.getName() << " -----\n";
    errs() << "UEVAR: ";
    for (Value* v : blockInfo[&block].ueVar) {
      if (v->hasName()) {
        errs() << v->getName() << " ";
      }
    }
    errs() << "\n";
    errs() << "VARKILL: ";
    for (Value* v : blockInfo[&block].varKill) {
      if (v->hasName()) {
        errs() << v->getName() << " ";
      }
    }
    errs() << "\n";
    errs() << "LIVEOUT: ";
    for (Value* v : blockInfo[&block].liveOut) {
      if (v->hasName()) {
        errs() << v->getName() << " ";
      }
    }
    errs() << "\n";
  }
}

// New PM implementation
struct HelloWorld : PassInfoMixin<HelloWorld> {
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
llvm::PassPluginLibraryInfo getHelloWorldPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "HelloWorld", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "hello-world") {
                    FPM.addPass(HelloWorld());
                    return true;
                  }
                  return false;
                });
          }};
}

// This is the core interface for pass plugins. It guarantees that 'opt' will
// be able to recognize HelloWorld when added to the pass pipeline on the
// command line, i.e. via '-passes=hello-world'
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getHelloWorldPluginInfo();
}
