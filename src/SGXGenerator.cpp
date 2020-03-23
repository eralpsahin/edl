#include "SGXGenerator.hpp"

using namespace llvm;

char pdg::SGXGenerator::ID = 0;

static llvm::cl::opt<std::string> app_c(
    "app", llvm::cl::desc("Specify untrusted file path"),
    llvm::cl::value_desc("app_c"));

static llvm::cl::opt<std::string> enclave_c(
    "enclave", llvm::cl::desc("Specify trusted file path"),
    llvm::cl::value_desc("enclave_c"));

static llvm::cl::opt<std::string> defined_t(
    "defined_t", llvm::cl::desc("Specify trusted side defined functions file"),
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
  unsigned initLine = -1;
  unsigned eidLine = -1;

  // According to the location of main func, learn where to define eid
  // and where to initialize enclave
  for (Function &function : M) {
    if (function.getName() == "main") {
      initLine =
          function.getSubprogram()->getScopeLine() + 1;  // After this line
    }

    if (function.getSubprogram() != nullptr) {
      if (eidLine > function.getSubprogram()->getLine())
        eidLine = function.getSubprogram()->getLine();  // Before this Line;
    }
    // For every ECALL, add insert global_eid variable as the first parameter
    for (auto &basicBlock : function) {
      for (auto &inst : basicBlock) {
        if (CallInst *callInst = dyn_cast<CallInst>(&inst)) {
          Function *called = callInst->getCalledFunction();
          if (!called->isIntrinsic()) {
            if (definedFuncsT.find(called->getName()) != definedFuncsT.end()) {
              // Get info about the ECALL
              unsigned line = callInst->getDebugLoc()->getLine();
              unsigned column =
                  callInst->getDebugLoc()->getColumn() +
                  callInst->getCalledFunction()->getName().str().length();
              bool hasParam = callInst->getNumArgOperands();
              bool isVoid = called->getReturnType()->isVoidTy();
              // Push to vector
              ecallVec.push_back(ECALLLoc(line, column, hasParam, isVoid));
            }
          }
        }
      }
    }
  }

  generateEnclaveFile();
  generateAppFile(eidLine, initLine);
  return false;
}

/**
 * Generating SGX Enclave file on a basic level
 * is relatively easy. We just need the _t header for it.
 */

void pdg::SGXGenerator::generateEnclaveFile() {
  std::ifstream encFile(enclave_c);
  std::string line;
  std::ofstream sgxEnc("Enclave.cpp");
  sgxEnc << "#include \"Enclave_t.h\"\n";
  while (getline(encFile, line)) {
    sgxEnc << line << "\n";
  }
  sgxEnc.close();
}

/**
 * To generate SGX App (untrusted) file we need to define an eid and
 * for each ECALL we need to send that variable as the first parameter.
 * TODO: ECALLs with return types have a second parameter for the return value.
 */
void pdg::SGXGenerator::generateAppFile(unsigned eidLine, unsigned initLine) {
  std::ifstream appFile(app_c);
  std::string line;
  unsigned currLine = 1;
  unsigned ecallCount = 0;
  std::ofstream sgxApp("App.cpp");
  sgxApp << "#include \"Enclave_u.h\"\n#include \"sgx_urts.h\"\n#include "
            "\"sgx_utils.h\"\n";
  while (getline(appFile, line)) {
    if (currLine == eidLine) {
      sgxApp << "sgx_enclave_id_t global_eid = 0;\n";
    } else if (currLine == initLine) {
      sgxApp << this->initialize << "\n";
    }
    if (currLine == ecallVec[ecallCount].line) {
      std::string eidParam = "global_eid";
      if (ecallVec[ecallCount].hasParam) eidParam += ", ";
      line.insert(ecallVec[ecallCount].column, eidParam);
      ecallCount++;
    }
    sgxApp << line << "\n";
    currLine++;
  }
  sgxApp.close();
}

bool pdg::SGXGenerator::doFinalization(llvm::Module &M) { return false; }

static RegisterPass<pdg::SGXGenerator> sgxGenerator("sgx",
                                                    "Generate SGX project",
                                                    false, true);
