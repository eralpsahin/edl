#include "PDGUtils.hpp" 
#include "llvm/IR/InstIterator.h"

using namespace llvm;
/**
 * All instructions of the function are wrapped as a graph node.
 * G_instMap is a map from instruction to its wrapper.
 * funcInstWMap is a map from function to a list of its instructions' wrapper. 
 */ 
void pdg::PDGUtils::constructInstMap(Function &F)
{
  for (inst_iterator I = inst_begin(F); I != inst_end(F); ++I)
  {
    if (G_instMap.find(&*I) == G_instMap.end())
    {
      InstructionWrapper *instW = new InstructionWrapper(&*I, GraphNodeType::INST);
      G_instMap[&*I] = instW;
      G_funcInstWMap[&F].insert(instW); 
    }
  }
}

/**
 * All functions of the module are analyzed and a wrapper for
 * instructions and arguments created.
 * funcMap is a map from function to its wrapper.
 * FunctionWrapper constructor creates wrapper for arguments too.
 */
void pdg::PDGUtils::constructFuncMap(Module &M)
{
  for (Module::iterator FI = M.begin(); FI != M.end(); ++FI)
  {
    if (FI->isDeclaration())
      continue;

    constructInstMap(*FI);
    if (G_funcMap.find(&*FI) == G_funcMap.end())
    {
      FunctionWrapper *funcW = new FunctionWrapper(&*FI);
      G_funcMap[&*FI] = funcW;
    }
  }
}

void pdg::PDGUtils::collectGlobalInsts(Module &M)
{
  for (Module::global_iterator globalIt = M.global_begin(); globalIt != M.global_end(); ++globalIt)
  {
    InstructionWrapper *globalW = new InstructionWrapper(dyn_cast<Value>(&(*globalIt)), GraphNodeType::GLOBAL_VALUE);
    G_globalInstsSet.insert(globalW);
  }
}

void pdg::PDGUtils::categorizeInstInFunc(Function &F)
{
  // sort store/load/return/CallInst in function
  for (inst_iterator I = inst_begin(F), IE = inst_end(F); I != IE; ++I)
  {
    Instruction *inst = dyn_cast<Instruction>(&*I);

    if (isa<StoreInst>(inst))
      G_funcMap[&F]->addStoreInst(inst);

    if (isa<LoadInst>(inst))
      G_funcMap[&F]->addLoadInst(inst);

    if (isa<ReturnInst>(inst))
      G_funcMap[&F]->addReturnInst(inst);

    if (CallInst *ci = dyn_cast<CallInst>(inst))
    {
      if (isa<DbgDeclareInst>(ci))
        G_funcMap[&F]->addDbgInst(inst);
      else
        G_funcMap[&F]->addCallInst(inst);
    }

    if (CastInst *bci = dyn_cast<CastInst>(inst))
      G_funcMap[&F]->addCastInst(inst);
    
    if (IntrinsicInst *ii = dyn_cast<IntrinsicInst>(inst))
      G_funcMap[&F]->addIntrinsicInst(inst);
  }
}
