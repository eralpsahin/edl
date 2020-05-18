#include "SGXGenerator.hpp"

using namespace llvm;

char pdg::SGXGenerator::ID = 0;

static llvm::cl::opt<std::string> defined_t(
    "defined_t",
    llvm::cl::desc("Specify trusted side defined functions file path"),
    llvm::cl::value_desc("defined_t"));

// Add ECALLs to definedFuncsT set
bool pdg::SGXGenerator::doInitialization(llvm::Module &M) {
  std::ifstream defined_func(defined_t);
  std::string func;
  while (getline(defined_func, func)) {
    definedFuncsT.insert(func);
  }
  return false;
}

bool pdg::SGXGenerator::runOnModule(Module &M) {
  // According to the location of main func, learn where to define eid
  // and where to initialize enclave
  for (Function &function : M) {
    if (function.getName() == "main") {
      mainLine =
          function.getSubprogram()->getScopeLine() + 1;  // After this line
      DIFile *mainDI =
          dyn_cast<llvm::DIScope>(function.getMetadata(0))->getFile();
      mainFile =
          mainDI->getDirectory().str() + "/" + mainDI->getFilename().str();
    }

    // Find every ECALL call instruction
    for (auto &basicBlock : function) {
      for (auto &inst : basicBlock) {
        if (CallInst *callInst = dyn_cast<CallInst>(&inst)) {
          // Skip intrinsic and non-ecall functions
          if (Function *called = callInst->getCalledFunction()) {
            if (!called->isIntrinsic() &&
                definedFuncsT.find(called->getName()) != definedFuncsT.end()) {
              ECALLLoc ecall = getECALLInfo(callInst);
              insertInMap(ecall.filepath, ecall);
            }
          }
        }
      }
    }
  }

  // TODO: Inject initializer to main
  for (auto fileECALL : fileECALLMap) {
    generateSGX(fileECALL.first, fileECALL.second);
  }

  // generateEnclaveFile();
  // generateAppFile(eidLine, initLine);
  return false;
}
void pdg::SGXGenerator::generateSGX(std::string file,
                                    std::vector<ECALLLoc> &ecallVec) {
  std::ifstream untrustedFile(file);
  if (untrustedFile.fail()) {
    errs() << file << " cannot be opened\n";
    return;
  }
  std::string line;
  unsigned currLine = 1;
  unsigned ecallIdx = 0;
  std::ofstream sgxUntrusted(getFilename(file));
  if (sgxUntrusted.fail()) {
    errs() << "File cannot be opened\n";
    return;
  }
  sgxUntrusted << includeSt;
  while (getline(untrustedFile, line)) {
    if (mainFile == file && currLine == mainLine) {
      sgxUntrusted << this->initializer << "\n";
    }
    int ecallIdx = searchECALL(file, currLine);
    if (ecallIdx != -1) {
      line.insert(fileECALLMap[file][ecallIdx].column - 1, "_ECALL");
    }
    sgxUntrusted << line << "\n";
    currLine++;
  }
  sgxUntrusted.close();
}

pdg::ECALLLoc pdg::SGXGenerator::getECALLInfo(CallInst *callInst) {
  Function *enclosingFunc = callInst->getFunction();
  Function *called = callInst->getCalledFunction();
  MDNode *node = enclosingFunc->getMetadata(0);
  auto file = dyn_cast<llvm::DIScope>(node)->getFile();

  // Get info about the ECALL
  unsigned line = callInst->getDebugLoc()->getLine();
  unsigned column = callInst->getDebugLoc()->getColumn() +
                    callInst->getCalledFunction()->getName().str().length();
  std::string filename = file->getFilename().str();
  std::string filepath = file->getDirectory().str() + "/" + filename;
  bool hasParam = callInst->getNumArgOperands();
  bool isVoid = called->getReturnType()->isVoidTy();
  // Create ECALLLoc struct and return
  return ECALLLoc(filepath, filename, line, column, hasParam, isVoid);
}

void pdg::SGXGenerator::insertInMap(std::string file, ECALLLoc ecall) {
  if (fileECALLMap.find(file) == fileECALLMap.end()) {
    fileECALLMap.insert(make_pair(file, std::vector<ECALLLoc>()));
    fileECALLMap[file].push_back(ecall);
  } else {
    fileECALLMap[file].push_back(ecall);
  }
}

int pdg::SGXGenerator::searchECALL(std::string filepath, int line) {
  if (fileECALLMap.find(filepath) == fileECALLMap.end()) return -1;
  for (unsigned idx = 0; idx < fileECALLMap[filepath].size(); idx += 1) {
    if (fileECALLMap[filepath][idx].line == line) return idx;
  }
  return -1;
}

std::string pdg::SGXGenerator::getFilename(std::string filepath) {
  int i = filepath.rfind("/");
  if (i == std::string::npos)
    return filepath;
  return filepath.substr(i+1);
}

static RegisterPass<pdg::SGXGenerator> sgxGenerator("sgx",
                                                    "Generate SGX project",
                                                    false, true);
