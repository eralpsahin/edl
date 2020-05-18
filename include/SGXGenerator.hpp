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
  std::string filepath;
  std::string filename;
  ECALLLoc() {}
  ECALLLoc(std::string filepath, std::string filename, unsigned line,
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
    includeSt =
        "#include \"Ecalls.h\"\n";
  }
  static char ID;
  bool runOnModule(llvm::Module &M);
  virtual bool doInitialization(llvm::Module &M);

 private:
  std::set<std::string> definedFuncsT;  // Defined Functions of the Trusted Side
  std::map<std::string, std::vector<ECALLLoc>>
      fileECALLMap;         // ECALLLocs in each file
  std::string initializer;  // initializer for SGX enclave
  std::string includeSt;   // include statements necessary
  int mainLine;
  std::string mainFile;

  int searchECALL(std::string filepath, int line);
  ECALLLoc getECALLInfo(llvm::CallInst *callInst);
  void insertInMap(std::string file, ECALLLoc ecall);
  void generateSGX(std::string file, std::vector<ECALLLoc> &ecallVec);
  std::string getFilename(std::string filepath);
};

}  // namespace pdg
#endif