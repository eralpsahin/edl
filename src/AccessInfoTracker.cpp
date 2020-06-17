#include "AccessInfoTracker.hpp"

#include <iostream>
#include <sstream>

using namespace llvm;

char pdg::AccessInfoTracker::ID = 0;

bool pdg::AccessInfoTracker::runOnModule(Module &M) {
  if (!USEDEBUGINFO) {
    errs() << "[WARNING] No debug information avaliable... \nUse [-debug 1] in "
              "the pass to generate debug information\n";
    exit(0);
  }

  auto &pdgUtils = PDGUtils::getInstance();
  PDG = &getAnalysis<pdg::ProgramDependencyGraph>();
  CG = &getAnalysis<CallGraphWrapperPass>().getCallGraph();

  // Populate function name sets for [string, size, count] attributes
  Heuristics::populateStringFuncs();
  Heuristics::populateMemFuncs();
  Heuristics::populateprintfFuncs();

  std::string enclaveFile = "Enclave.edl";
  edl_file.open(enclaveFile);

  // Read the untrusted domain information to create trusted side and wrappers
  createTrusted(UPREFIX, M);

  // Clear function lists
  lockFuncList.clear();
  blackFuncList.clear();
  staticFuncList.clear();
  importedFuncList.clear();
  definedFuncList.clear();
  kernelFuncList.clear();

  // Read the trusted domain information to create untrusted side
  createUntrusted(TPREFIX, M);

  edl_file.close();

  std::ifstream temp(enclaveFile);
  std::stringstream buffer;
  buffer << temp.rdbuf();
  edl_file.open(enclaveFile);
  edl_file << "enclave"
           << " {\n\n";

  for (auto el : userDefinedTypes) {
    edl_file << el.second;
  }
  edl_file << buffer.str();

  edl_file.close();
  return false;
}

void pdg::AccessInfoTracker::createTrusted(std::string prefix, Module &M) {
  populateLists(prefix);
  // Get the main Functions closure for root ECALLs
  auto main = M.getFunction(StringRef("main"));
  auto mainClosure = getTransitiveClosure(*main);
  PDG->buildPDGForFunc(main);

  edl_file << "\ttrusted {\n";

  // Open file for ecall wrapper functions
  ecallsH.open("Ecalls.h");
  ecallsH << "#pragma once\n";
  ecallsH << "#include \"Enclave_u.h\"\n#include \"sgx_urts.h\"\n#include "
             "\"sgx_utils.h\"\n";
  ecallsH << "extern sgx_enclave_id_t global_eid;\n";

  ecallsC.open("Ecalls.cpp");
  ecallsC << "#include \"Ecalls.h\"\n";
  ecallsC << "sgx_enclave_id_t global_eid = 0;\n";
  for (auto func : mainClosure) {
    getIntraFuncReadWriteInfoForFunc(*func);
  }

  for (auto funcName : importedFuncList) {
    crossBoundary = false;
    curImportedTransFuncName = funcName;
    auto func = M.getFunction(StringRef(funcName));
    if (func->isDeclaration()) continue;
    auto transClosure = getTransitiveClosure(*func);
    for (std::string staticFuncName : staticFuncList) {
      Function *staticFunc = M.getFunction(StringRef(staticFuncName));
      if (staticFunc && !staticFunc->isDeclaration())
        transClosure.push_back(staticFunc);
    }
    for (auto iter = transClosure.rbegin(); iter != transClosure.rend();
         iter++) {
      auto transFunc = *iter;
      if (transFunc->isDeclaration()) continue;
      if (definedFuncList.find(transFunc->getName()) != definedFuncList.end() ||
          staticFuncList.find(transFunc->getName()) != staticFuncList.end())
        crossBoundary = true;
      getIntraFuncReadWriteInfoForFunc(*transFunc);
    }
    writeCALLWrapper(*func, ecallsH, ecallsC, "_ECALL");
    if (std::find(mainClosure.begin(), mainClosure.end(), func) !=
        mainClosure.end())  // This function is in main Funcs closure
      generateIDLforFunc(*func, true);  // It is possibly a root ECALL
    else
      generateIDLforFunc(*func, false);
    edl_file << ";\n\n";
  }

  edl_file << "\t};\n";
  ecallsH.close();
  ecallsC.close();
}

void pdg::AccessInfoTracker::createUntrusted(std::string prefix, Module &M) {
  populateLists(prefix);
  edl_file << "\n\tfrom \"Unsupported.edl\" import *;\n\tuntrusted {\n";

  ocallsH.open("Ocalls.h");
  ocallsH << "#pragma once\n";
  ocallsH << "#include \"Enclave_t.h\"\n";

  ocallsC.open("Ocalls.cpp");
  ocallsC << "#include \"Ocalls.h\"\n";
  for (auto funcName : importedFuncList) {
    crossBoundary = false;
    curImportedTransFuncName = funcName;
    auto func = M.getFunction(StringRef(funcName));
    if (func->isDeclaration()) continue;
    auto transClosure = getTransitiveClosure(*func);
    for (std::string staticFuncName : staticFuncList) {
      Function *staticFunc = M.getFunction(StringRef(staticFuncName));
      if (staticFunc && !staticFunc->isDeclaration())
        transClosure.push_back(staticFunc);
    }
    // Start populating stringstream for allow() syntax of OCALL
    std::ostringstream allow;
    allow << "allow(";
    for (auto iter = transClosure.rbegin(); iter != transClosure.rend();
         iter++) {
      auto transFunc = *iter;
      if (transFunc->isDeclaration()) continue;
      if (definedFuncList.find(transFunc->getName()) != definedFuncList.end() ||
          staticFuncList.find(transFunc->getName()) != staticFuncList.end()) {
        if (allow.str().length() > 6) allow << ", ";
        allow << transFunc->getName().str();
        crossBoundary = true;
      }
      getIntraFuncReadWriteInfoForFunc(*transFunc);
    }
    allow << ");";
    generateIDLforFunc(*func, false);
    writeCALLWrapper(*func, ocallsH, ocallsC, "_OCALL");
    // Write the allow syntax if there is an ECALL in this OCALL
    if (allow.str().length() > 8)
      edl_file << allow.str() << "\n\n";
    else
      edl_file << ";\n\n";
  }

  edl_file << "\t};\n\n};";
}

void pdg::AccessInfoTracker::writeCALLWrapper(Function &F,
                                              std::ofstream &header,
                                              std::ofstream &cpp,
                                              std::string suffix) {
  auto &pdgUtils = PDGUtils::getInstance();
  DIType *funcRetType = DIUtils::getFuncRetDIType(F);
  std::string retTypeName;
  if (funcRetType == nullptr)
    retTypeName = "void";
  else
    retTypeName = DIUtils::getDITypeName(funcRetType);

  header << retTypeName << " " << F.getName().str() << suffix << "( ";
  cpp << retTypeName << " " << F.getName().str() << suffix << "( ";
  std::vector<std::pair<std::string, std::string> > argVec;
  for (auto argW : pdgUtils.getFuncMap()[&F]->getArgWList()) {
    Argument &arg = *argW->getArg();
    Type *argType = arg.getType();
    auto &dbgInstList = pdgUtils.getFuncMap()[&F]->getDbgDeclareInstList();
    std::string argName = DIUtils::getArgName(arg, dbgInstList);
    if (PDG->isStructPointer(argType)) {
      header << " " << DIUtils::getArgTypeName(arg) << " " << argName;
      cpp << " " << DIUtils::getArgTypeName(arg) << " " << argName;
      argVec.push_back(make_pair(DIUtils::getArgTypeName(arg), argName));
    } else {
      if (argType->getTypeID() == 15) {
        header << DIUtils::getArgTypeName(arg) << " " << argName;
        cpp << DIUtils::getArgTypeName(arg) << " " << argName;
        argVec.push_back(make_pair(DIUtils::getArgTypeName(arg), argName));

      } else {
        header << DIUtils::getArgTypeName(arg) << " " << argName;
        cpp << DIUtils::getArgTypeName(arg) << " " << argName;
        argVec.push_back(make_pair(DIUtils::getArgTypeName(arg), argName));
      }
    }

    if (argW->getArg()->getArgNo() < F.arg_size() - 1 && !argName.empty()) {
      header << ", ";
      cpp << ", ";
    }
  }
  header << ");\n";
  cpp << ") {\n";
  if (retTypeName != "void") {
    cpp << "\t" << retTypeName << " res;\n";
  }
  if (suffix == "_ECALL") {
    cpp << "\t" << F.getName().str() << "(global_eid";
    if (retTypeName != "void") {
      cpp << ", &res";
    }
    for (auto arg : argVec) {
      cpp << ", " << arg.second;
    }
    cpp << ");\n";
  } else {
    cpp << "\t" << F.getName().str() << "(";
    if (retTypeName != "void") {
      cpp << "&res";
    }
    for (unsigned i = 0; i < argVec.size(); i += 1) {
      if (i == 0 && retTypeName == "void") {
        cpp << argVec[i].second;
      } else
        cpp << ", " << argVec[i].second;
    }
    cpp << ");\n";
  }

  if (retTypeName != "void") {
    cpp << "\treturn res;\n";
  }
  cpp << "}\n";
}

void pdg::AccessInfoTracker::populateLists(std::string prefix) {
  std::ifstream importedFuncs(prefix + "imported_func.txt");
  std::ifstream definedFuncs(prefix + "defined_func.txt");
  std::ifstream blackFuncs(prefix + "blacklist.txt");
  std::ifstream static_funcptr(prefix + "static_funcptr.txt");
  std::ifstream static_func(prefix + "static_func.txt");
  std::ifstream lock_funcs(prefix + "lock_func.txt");
  // process global shared lock
  for (std::string line; std::getline(lock_funcs, line);)
    lockFuncList.insert(line);
  for (std::string line; std::getline(blackFuncs, line);)
    blackFuncList.insert(line);
  for (std::string staticFuncLine, funcPtrLine;
       std::getline(static_func, staticFuncLine),
       std::getline(static_funcptr, funcPtrLine);) {
    staticFuncList.insert(staticFuncLine);
    staticFuncptrList.insert(funcPtrLine);
  }
  for (std::string line; std::getline(importedFuncs, line);)
    if (blackFuncList.find(line) == blackFuncList.end())
      importedFuncList.insert(line);
  for (std::string line; std::getline(definedFuncs, line);)
    definedFuncList.insert(line);

  // importedFuncList.insert(staticFuncptrList.begin(),
  // staticFuncptrList.end());
  seenFuncOps = false;
  kernelFuncList = importedFuncList;

  importedFuncs.close();
  definedFuncs.close();
  blackFuncs.close();
  static_funcptr.close();
  static_func.close();
  lock_funcs.close();
}

void pdg::AccessInfoTracker::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<pdg::ProgramDependencyGraph>();
  AU.addRequired<CallGraphWrapperPass>();
  AU.setPreservesAll();
}

std::vector<Function *> pdg::AccessInfoTracker::getTransitiveClosure(
    Function &F) {
  std::vector<Function *> transClosure;
  transClosure.push_back(&F);
  std::queue<CallGraphNode *> funcNodeQ;
  std::set<Function *> seen;
  funcNodeQ.push(CG->getOrInsertFunction(&F));
  while (!funcNodeQ.empty()) {
    if (transClosure.size() > 100) return transClosure;

    auto callNode = funcNodeQ.front();
    funcNodeQ.pop();
    if (!callNode) continue;

    for (auto calleeNodeI = callNode->begin(); calleeNodeI != callNode->end();
         calleeNodeI++) {
      if (!calleeNodeI->second->getFunction()) continue;
      auto funcName = calleeNodeI->second->getFunction()->getName();
      if (blackFuncList.find(funcName) != blackFuncList.end()) continue;
      Function *calleeFunc = calleeNodeI->second->getFunction();
      if (calleeFunc->isDeclaration()) continue;
      if (seen.find(calleeFunc) != seen.end()) continue;
      funcNodeQ.push(calleeNodeI->second);
      transClosure.push_back(calleeFunc);
      seen.insert(calleeFunc);
    }
  }
  return transClosure;
}

AccessType pdg::AccessInfoTracker::getAccessTypeForInstW(
    const InstructionWrapper *instW, ArgumentWrapper *argW) {
  auto &pdgUtils = PDGUtils::getInstance();
  auto dataDList = PDG->getNodeDepList(instW->getInstruction());
  AccessType accessType = AccessType::NOACCESS;
  for (auto depPair : dataDList) {
    InstructionWrapper *depInstW =
        const_cast<InstructionWrapper *>(depPair.first->getData());
    DependencyType depType = depPair.second;
    // check for read
    if (!depInstW->getInstruction() || depType != DependencyType::DATA_DEF_USE)
      continue;

    if (isa<LoadInst>(depInstW->getInstruction()) ||
        isa<GetElementPtrInst>(depInstW->getInstruction()))
      accessType = AccessType::READ;

    // check for store instruction.

    if (StoreInst *st = dyn_cast<StoreInst>(depInstW->getInstruction())) {
      // if a value is used in a store instruction and is the store destination
      if (dyn_cast<Instruction>(st->getPointerOperand()) ==
          instW->getInstruction()) {
        if (isa<Argument>(st->getValueOperand()))  // ignore the store inst that
                                                   // store arg to stack mem
          break;
        accessType = AccessType::WRITE;
        break;
      }
    }
    // Heuristic checks for string-only functions
    if (CallInst *callInst = dyn_cast<CallInst>(depInstW->getInstruction())) {
      // Get the funcName this argument is used as parameter
      std::string funcName;
      if (callInst->getCalledFunction()) {
        funcName = callInst->getCalledFunction()->getName().str();
      } else {  // continue if function is inaccessible
        continue;
      }
      int argNum = -1;
      int curr = 0;
      for (auto arg = callInst->arg_begin(); arg != callInst->arg_end();
           ++arg) {
        if (instW->getInstruction() == *arg) {
          argNum = curr;
          break;
        }
        curr += 1;
      }
      assert(argNum != -1 &&
             "argument is not found in the callInst's arguments");
      if (DIUtils::isCharPointerTy(*argW->getArg())) {
        // Check if it is in the string funcs list
        Heuristics::addStringAttribute(funcName, argNum, argW);
        Heuristics::checkPrintf(callInst, argNum, argW);
        // Check whether the called function has string attribute for the arg
        if (pdgUtils.getFuncMap().find(callInst->getCalledFunction()) !=
            pdgUtils.getFuncMap().end()) {
          auto funcW = pdgUtils.getFuncMap()[callInst->getCalledFunction()];
          if (argNum < funcW->getArgWList().size()) {
            ArgumentWrapper *innerArgW = funcW->getArgWList()[argNum];
            if (innerArgW->getAttribute().isString())
              argW->getAttribute().setString();
            else if (argW->getAttribute().isString())
              innerArgW->getAttribute().setString();
          }
        }
      }
      if (DIUtils::isPointerType(DIUtils::getArgDIType(*argW->getArg())))
        Heuristics::addSizeAttribute(funcName, argNum, callInst, argW, PDG);
    }
  }
  return accessType;
}

void pdg::AccessInfoTracker::getIntraFuncReadWriteInfoForArg(
    ArgumentWrapper *argW, TreeType treeTy) {
  auto argTree = argW->getTree(treeTy);
  if (argTree.size() == 0) return;
  // throw new ArgParameterTreeSizeIsZero("Argment tree is empty... Every param
  // should have at least one node...\n");

  auto func = argW->getArg()->getParent();
  auto treeI = argW->getTree(treeTy).begin();
  // if (!(*treeI)->getTreeNodeType()->isPointerTy())

  if ((*treeI)->getDIType() == nullptr) {
    errs() << "Empty debugging info for " << func->getName() << " - "
           << argW->getArg()->getArgNo() << "\n";
    return;
  }
  if ((*treeI)->getDIType()->getTag() != dwarf::DW_TAG_pointer_type &&
      !DIUtils::isTypeDefPtrTy(*argW->getArg())) {
    // errs() << func->getName() << " - " << argW->getArg()->getArgNo()
    //        << " Find non-pointer type parameter, do not track...\n";
    return;
  }

  AccessType accessType = AccessType::NOACCESS;
  auto &pdgUtils = PDGUtils::getInstance();
  int count = -1;
  if (DIUtils::isTypeDefPtrTy(*argW->getArg())) {
    argW->getAttribute().setIsPtr();
    // it can also be readonly
    if (DIUtils::isTypeDefConstPtrTy(*argW->getArg())) {
      argW->getAttribute().setReadOnly();
    }
  }
  for (auto treeI = argW->tree_begin(TreeType::FORMAL_IN_TREE);
       treeI != argW->tree_end(TreeType::FORMAL_IN_TREE); ++treeI) {
    count += 1;
    auto valDepPairList =
        PDG->getNodesWithDepType(*treeI, DependencyType::VAL_DEP);
    for (auto valDepPair : valDepPairList) {
      auto dataW = valDepPair.first->getData();
      for (auto castInst :
           pdgUtils.getFuncMap()[argW->getFunc()]->getCastInstList()) {
        if (castInst->getOperand(0) == dataW->getInstruction()) {
          auto instW = pdgUtils.getInstMap()[castInst];
          getAccessTypeForInstW(instW, argW);
        }
      }

      AccessType accType = getAccessTypeForInstW(dataW, argW);
      if (static_cast<int>(accType) >
          static_cast<int>((*treeI)->getAccessType())) {
        auto &dbgInstList =
            pdgUtils.getFuncMap()[func]->getDbgDeclareInstList();
        std::string argName =
            DIUtils::getArgName(*(argW->getArg()), dbgInstList);

        if (accType == AccessType::WRITE) {
          argW->getAttribute().setOut();
        }
        if (count == 1 && accType == AccessType::READ) {
          argW->getAttribute().setIn();
        }
        // errs() << argName << " n" << count << "-"
        //        << getAccessAttributeName(treeI) << " => "
        //        << getAccessAttributeName((unsigned)accType) << "\n";

        (*treeI)->setAccessType(accType);
      }
    }
  }

  auto main = pdgUtils.getFuncMap()[func]
                  ->getDbgDeclareInstList()[0]
                  ->getModule()
                  ->getFunction(StringRef("main"));
  for (auto callinst : pdgUtils.getFuncMap()[main]->getCallInstList()) {
    if (callinst->getCalledFunction() != argW->getFunc()) continue;
    if (callinst->getNumArgOperands() < argW->getArg()->getArgNo()) continue;
    Value *v = callinst->getOperand(argW->getArg()->getArgNo());
    if (isa<Instruction>(v) || isa<Argument>(v)) {
      // V is used in inst
      if (dyn_cast<Instruction>(v)) {
        if (GetElementPtrInst *getEl =
                dyn_cast<GetElementPtrInst>(dyn_cast<Instruction>(v))) {
          Type *T = dyn_cast<PointerType>(getEl->getPointerOperandType())
                        ->getElementType();
          if (isa<ArrayType>(T)) {
            argW->getAttribute().setCount(
                std::to_string(T->getArrayNumElements()));
          }
        }
      }
    }
  }
}

void pdg::AccessInfoTracker::getIntraFuncReadWriteInfoForFunc(Function &F) {
  auto &pdgUtils = PDGUtils::getInstance();
  FunctionWrapper *funcW = pdgUtils.getFuncMap()[&F];
  // for arguments
  if (!pdgUtils.getFuncMap()[&F]->hasTrees()) {
    PDG->buildPDGForFunc(&F);
  }
  for (auto argW : funcW->getArgWList())
    getIntraFuncReadWriteInfoForArg(argW, TreeType::FORMAL_IN_TREE);

  // for return value
  ArgumentWrapper *retW = funcW->getRetW();
  getIntraFuncReadWriteInfoForArg(retW, TreeType::FORMAL_IN_TREE);
}

pdg::ArgumentMatchType pdg::AccessInfoTracker::getArgMatchType(Argument *arg1,
                                                               Argument *arg2) {
  Type *arg1_type = arg1->getType();
  Type *arg2_type = arg2->getType();

  if (arg1_type == arg2_type) return pdg::ArgumentMatchType::EQUAL;

  if (arg1_type->isPointerTy())
    arg1_type = (dyn_cast<PointerType>(arg1_type))->getElementType();

  if (arg1_type->isStructTy()) {
    StructType *arg1_st_type = dyn_cast<StructType>(arg1_type);
    for (unsigned i = 0; i < arg1_st_type->getNumElements(); ++i) {
      Type *arg1_element_type = arg1_st_type->getElementType(i);
      bool type_match = (arg1_element_type == arg2_type);

      if (arg2_type->isPointerTy()) {
        bool pointed_type_match =
            ((dyn_cast<PointerType>(arg2_type))->getElementType() ==
             arg1_element_type);
        type_match = type_match || pointed_type_match;
      }

      if (type_match) return pdg::ArgumentMatchType::CONTAINED;
    }
  }

  return pdg::ArgumentMatchType::NOTCONTAINED;
}

int pdg::AccessInfoTracker::getCallParamIdx(
    const InstructionWrapper *instW, const InstructionWrapper *callInstW) {
  Instruction *inst = instW->getInstruction();
  Instruction *callInst = callInstW->getInstruction();
  if (inst == nullptr || callInst == nullptr) return -1;

  if (CallInst *CI = dyn_cast<CallInst>(callInst)) {
    int paraIdx = 0;
    for (auto arg_iter = CI->arg_begin(); arg_iter != CI->arg_end();
         ++arg_iter) {
      if (Instruction *tmpInst = dyn_cast<Instruction>(&*arg_iter)) {
        if (tmpInst == inst) return paraIdx;
      }
      paraIdx++;
    }
  }
  return -1;
}

void pdg::AccessInfoTracker::generateRpcForFunc(Function &F, bool root) {
  auto &pdgUtils = PDGUtils::getInstance();
  DIType *funcRetType = DIUtils::getFuncRetDIType(F);
  if (DIUtils::isStructPointerTy(funcRetType) ||
      DIUtils::isStructTy(funcRetType)) {
    DIUtils::insertStructDefinition(funcRetType, userDefinedTypes);
  } else if (DIUtils::isEnumTy(funcRetType)) {
    DIUtils::insertEnumDefinition(funcRetType, userDefinedTypes);
  } else if (DIUtils::isUnionTy(funcRetType) ||
             DIUtils::isUnionPointerTy(funcRetType)) {
    DIUtils::insertUnionDefinition(funcRetType, userDefinedTypes);
  }
  std::string retTypeName;
  if (funcRetType == nullptr)
    retTypeName = "void";
  else
    retTypeName = DIUtils::getDITypeName(funcRetType);

  edl_file << "\t\t";
  if (root) edl_file << "public ";
  edl_file << retTypeName << " " << F.getName().str() << "( ";
  // handle parameters
  for (auto argW : pdgUtils.getFuncMap()[&F]->getArgWList()) {
    Argument &arg = *argW->getArg();
    Type *argType = arg.getType();
    auto &dbgInstList = pdgUtils.getFuncMap()[&F]->getDbgDeclareInstList();
    std::string argName = DIUtils::getArgName(arg, dbgInstList);
    if (argType->getTypeID() == 15 &&
        !(DIUtils::isUnionTy(DIUtils::getArgDIType(
            arg))))  // Reject non pointer unions explicitly
      edl_file << argW->getAttribute().dump() << " "
               << DIUtils::getArgTypeName(arg) << " " << argName;
    else
      edl_file << DIUtils::getArgTypeName(arg) << " " << argName;

    if (argW->getArg()->getArgNo() < F.arg_size() - 1 && !argName.empty())
      edl_file << ", ";
  }
  edl_file << " )";
}

void pdg::AccessInfoTracker::generateIDLforFunc(Function &F, bool root) {
  // if a function is defined on the same side, no need to generate IDL rpc for
  // this function.
  auto &pdgUtils = PDGUtils::getInstance();
  FunctionWrapper *funcW = pdgUtils.getFuncMap()[&F];
  for (auto argW : funcW->getArgWList()) {
    generateIDLforArg(argW, TreeType::FORMAL_IN_TREE);
  }
  generateIDLforArg(funcW->getRetW(), TreeType::FORMAL_IN_TREE);
  generateRpcForFunc(F, root);
}

void pdg::AccessInfoTracker::generateIDLforArg(ArgumentWrapper *argW,
                                               TreeType treeTy,
                                               std::string funcName,
                                               bool handleFuncPtr) {
  auto &pdgUtils = PDGUtils::getInstance();
  if (argW->getTree(TreeType::FORMAL_IN_TREE).size() == 0 ||
      argW->getArg()->getArgNo() == 100)  // No need to handle return args
    return;

  Function &F = *argW->getArg()->getParent();
  auto check = argW->tree_begin(treeTy);

  if (funcName.empty()) funcName = F.getName().str();

  auto &dbgInstList = pdgUtils.getFuncMap()[&F]->getDbgDeclareInstList();
  std::string argName = DIUtils::getArgName(*(argW->getArg()), dbgInstList);
  std::string sharedFieldPrefix = curImportedTransFuncName + argName;
  auto curDIType = (*check)->getDIType();
  if (DIUtils::isStructPointerTy(curDIType) || DIUtils::isStructTy(curDIType)) {
    DIUtils::insertStructDefinition(DIUtils::getArgDIType(*argW->getArg()),
                                    userDefinedTypes);
  } else if (DIUtils::isEnumTy(curDIType)) {
    DIUtils::insertEnumDefinition(DIUtils::getArgDIType(*argW->getArg()),
                                  userDefinedTypes);
  } else if (DIUtils::isUnionTy(curDIType) ||
             DIUtils::isUnionPointerTy(curDIType)) {
    DIUtils::insertUnionDefinition(DIUtils::getArgDIType(*argW->getArg()),
                                   userDefinedTypes);
  } else if (DIUtils::isTypeDefTy(curDIType)) {
    auto base = dyn_cast<DIDerivedType>(curDIType)->getBaseType();
    if (DISubroutineType *diSub =
            dyn_cast<DISubroutineType>(DIUtils::getLowestDIType(base))) {
      llvm::errs() << "Function pointer typedef found for " << *base << "\n";
    } else {
      DIFile *file = curDIType->getFile();
      if (file == nullptr) file = base->getFile();
      if (file != nullptr &&
          file->getFilename().str().find("/usr/include") != 0) {
        userDefinedTypes.insert({file->getFilename().str(),
                                 "include \"" + file->getFilename().str() +
                                     "\" // For param: " + argName + "\n"});
      }
    }
  }
}

std::string pdg::getAccessAttributeName(
    tree<InstructionWrapper *>::iterator treeI) {
  int accessIdx = static_cast<int>((*treeI)->getAccessType());
  return getAccessAttributeName(accessIdx);
}

std::string pdg::getAccessAttributeName(unsigned accessIdx) {
  std::vector<std::string> access_attribute = {"[-]", "[in]", "[out]"};
  return access_attribute[accessIdx];
}

static RegisterPass<pdg::AccessInfoTracker> AccessInfoTracker(
    "accinfo-track", "Argument access information tracking Pass", false, true);