#ifndef ACCESSINFO_TRACKER_H_

#define ACCESSINFO_TRACKER_H_
#include <fstream>
#include <map>
#include <sstream>

#include "DebugInfoUtils.hpp"
#include "Heuristics.hpp"
#include "ProgramDependencyGraph.hpp"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/IR/Module.h"
#include "llvm/PassAnalysisSupport.h"

namespace pdg {
class AccessInfoTracker : public llvm::ModulePass {
 public:
  AccessInfoTracker() : llvm::ModulePass(ID) {}
  static char ID;
  bool runOnModule(llvm::Module &M);
  void createTrusted(std::string prefix, llvm::Module &M);
  void createUntrusted(std::string prefix, llvm::Module &M);
  void writeCALLWrapper(llvm::Function &F, std::ofstream &header, std::ofstream &cpp,
                        std::string suffix);
  void populateLists(std::string prefix);
  void getAnalysisUsage(llvm::AnalysisUsage &AU) const;
  void getIntraFuncReadWriteInfoForArg(ArgumentWrapper *argW, TreeType treeTy);
  void getIntraFuncReadWriteInfoForFunc(llvm::Function &F);
  int getCallParamIdx(const InstructionWrapper *instW,
                      const InstructionWrapper *callInstW);  //? Unused
  ArgumentMatchType getArgMatchType(llvm::Argument *arg1,
                                    llvm::Argument *arg2);  //? Unused
  AccessType getAccessTypeForInstW(const InstructionWrapper *instW,
                                   ArgumentWrapper *argW);
  void generateIDLforFunc(llvm::Function &F, bool root);
  void generateRpcForFunc(llvm::Function &F, bool root);
  void generateIDLforArg(ArgumentWrapper *argW, TreeType ty,
                         std::string funcName = "", bool handleFuncPtr = false);
  std::vector<llvm::Function *> getTransitiveClosure(llvm::Function &F);

 private:
  ProgramDependencyGraph *PDG;
  llvm::CallGraph *CG;
  std::ofstream edl_file;
  std::ofstream ecallsH;
  std::ofstream ecallsC;
  std::ofstream ocallsH;
  std::ofstream ocallsC;
  std::set<std::string> deviceObjStore;  // Unused
  std::set<std::string> kernelObjStore;
  std::set<std::string> importedFuncList;
  std::set<std::string> kernelFuncList;
  std::set<std::string> definedFuncList;
  std::set<std::string> blackFuncList;
  std::set<std::string> staticFuncptrList;
  std::set<std::string> staticFuncList;
  std::set<std::string> lockFuncList;
  std::map<std::string, std::string> userDefinedTypes;
  std::set<std::string> processedFuncPtrNames;
  std::string curImportedTransFuncName;
  bool seenFuncOps;
  bool crossBoundary;  // indicate whether transitive closure cross two domains
};

std::string getAccessAttributeName(tree<InstructionWrapper *>::iterator treeI);
std::string getAccessAttributeName(unsigned accessIdx);

}  // namespace pdg
#endif