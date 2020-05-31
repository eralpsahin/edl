#include "SGXGenerator.hpp"

using namespace llvm;

char pdg::SGXGenerator::ID = 0;

static llvm::cl::opt<std::string> defined(
    "defined", llvm::cl::desc("Specify defined functions file path"),
    llvm::cl::value_desc("defined"));

static llvm::cl::opt<std::string> suffix(
    "suffix", llvm::cl::desc("Specify suffix for code injection"),
    llvm::cl::value_desc("suffix"));

// Add CALLs to definedFuncs set
bool pdg::SGXGenerator::doInitialization(llvm::Module &M) {
  std::ifstream defined_func(defined);
  std::string func;
  while (getline(defined_func, func)) {
    definedFuncs.insert(func);
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

    // Find every CALL call instruction
    for (auto &basicBlock : function) {
      for (auto &inst : basicBlock) {
        if (CallInst *callInst = dyn_cast<CallInst>(&inst)) {
          // Skip intrinsic and non-call functions
          if (Function *called = callInst->getCalledFunction()) {
            if (!called->isIntrinsic() &&
                definedFuncs.find(called->getName()) != definedFuncs.end()) {
              CALLLoc call = getCALLInfo(callInst);
              insertInMap(call.filepath, call);
            }
          }
        }
      }
    }
  }

  // TODO: Inject initializer to main
  for (auto fileCALL : fileCALLMap) {
    generateSGX(fileCALL.first, fileCALL.second);
  }
  return false;
}
void pdg::SGXGenerator::generateSGX(std::string file,
                                    std::vector<CALLLoc> &callVec) {
  std::ifstream untrustedFile(file);
  if (untrustedFile.fail()) {
    errs() << file << " cannot be opened\n";
    return;
  }
  std::string line;
  unsigned currLine = 1;
  std::ofstream sgxFile(getFilename(file));
  if (sgxFile.fail()) {
    errs() << "File cannot be opened\n";
    return;
  }
  if (suffix == "_ECALL")
    sgxFile << untrustedInc;
  else
    sgxFile << trustedInc;
  while (getline(untrustedFile, line)) {
    if (mainFile == file && currLine == mainLine) {
      sgxFile << this->initializer << "\n";
    }
    int callIdx = searchCALL(file, currLine);
    if (callIdx != -1) {
      line.insert(fileCALLMap[file][callIdx].column - 1, suffix);
    }
    sgxFile << line << "\n";
    currLine++;
  }
  sgxFile.close();
}

pdg::CALLLoc pdg::SGXGenerator::getCALLInfo(CallInst *callInst) {
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
  return CALLLoc(filepath, filename, line, column, hasParam, isVoid);
}

void pdg::SGXGenerator::insertInMap(std::string file, CALLLoc ecall) {
  if (fileCALLMap.find(file) == fileCALLMap.end()) {
    fileCALLMap.insert(make_pair(file, std::vector<CALLLoc>()));
    fileCALLMap[file].push_back(ecall);
  } else {
    fileCALLMap[file].push_back(ecall);
  }
}

int pdg::SGXGenerator::searchCALL(std::string filepath, int line) {
  if (fileCALLMap.find(filepath) == fileCALLMap.end()) return -1;
  for (unsigned idx = 0; idx < fileCALLMap[filepath].size(); idx += 1) {
    if (fileCALLMap[filepath][idx].line == line) return idx;
  }
  return -1;
}

std::string pdg::SGXGenerator::getFilename(std::string filepath) {
  int i = filepath.rfind("/");
  if (i == std::string::npos) return filepath;
  return filepath.substr(i + 1);
}

static RegisterPass<pdg::SGXGenerator> sgxGenerator("sgx",
                                                    "Generate SGX project",
                                                    false, true);
