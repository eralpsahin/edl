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
struct CALLLoc {
  unsigned line;
  unsigned column;
  bool hasParam;
  bool isVoid;
  std::string filepath;
  std::string filename;
  CALLLoc() {}
  CALLLoc(std::string filepath, std::string filename, unsigned line,
          unsigned column, bool hasParam = false, bool isVoid = true)
      : filepath(filepath),
        filename(filename),
        line(line),
        column(column),
        hasParam(hasParam),
        isVoid(isVoid) {}
};

class SGXGenerator : public llvm::ModulePass {
 public:
  SGXGenerator() : llvm::ModulePass(ID) {
    initializer =
        "if (initialize_enclave(&global_eid, \"enclave.token\", "
        "\"enclave.signed.so\") < 0)\n"
        "{\n"
        "printf(\"Failed to initialize enclave.\\n\");\n"
        "return 1;\n"
        "}\n";
    untrustedInc = "#include \"Ecalls.h\"\n#include \"Unsupported.h\"\n";
    trustedInc = "#include \"Ocalls.h\"\n#include \"Unsupported.h\"\n";
  }
  static char ID;
  bool runOnModule(llvm::Module &M);
  virtual bool doInitialization(llvm::Module &M);

 private:
  std::set<std::string> definedFuncs;  // Defined Functions of the Trusted Side
  std::map<std::string, std::vector<CALLLoc>>
      fileCALLMap;           // CALLLocs in each file
  std::string initializer;   // initializer for SGX enclave
  std::string untrustedInc;  // include statements necessary
  std::string trustedInc;
  int mainLine;
  std::string mainFile;

  int searchCALL(std::string filepath, int line);
  CALLLoc getCALLInfo(llvm::CallInst *callInst);
  void insertInMap(std::string file, CALLLoc call);
  void generateSGX(std::string file);
  std::string getFilename(std::string filepath);
};

}  // namespace pdg
#endif