#ifndef SGX_GENERATOR_H_

#define SGX_GENERATOR_H_

#include <fstream>
#include <set>  // for set operations
#include <sstream>
#include <string>
#include <vector>

#include "DebugInfoUtils.hpp"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

namespace pdg {
struct ECALLLoc {
  unsigned line;
  unsigned column;
  bool hasParam;
  bool isVoid;
  ECALLLoc() {}
  ECALLLoc(unsigned line, unsigned column, bool hasParam = false,
           bool isVoid = true)
      : line(line), column(column), hasParam(hasParam), isVoid(isVoid) {}
};

class SGXGenerator : public llvm::ModulePass {
 public:
  SGXGenerator() : llvm::ModulePass(ID) {
    initialize =
        "  if (initialize_enclave(&global_eid, \"enclave.token\" , "
        "\"enclave.signed.so\") < 0)\n"
        "  {\n"
        "    printf(\"Fail to initialize enclave.\\n\");\n"
        "    return 1;\n"
        "  }\n";
  }
  static char ID;
  bool runOnModule(llvm::Module &M);
  virtual bool doInitialization(llvm::Module &M);
  virtual bool doFinalization(llvm::Module &M);
  

 private:
  std::set<std::string> definedFuncsT;  // Defined Functions of the Trusted Side
  std::vector<ECALLLoc> ecallVec;
  std::string initialize;

  void generateAppFile(unsigned eidLine, unsigned initLine);
  void generateEnclaveFile();

  int searchECALL(int line);
};

}  // namespace pdg
#endif