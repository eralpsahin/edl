#include "AccessInfoTracker.hpp"
#include <iostream>
#include <sstream>

using namespace llvm;

char pdg::AccessInfoTracker::ID = 0;

void pdg::AccessInfoTracker::createTrusted(std:: string prefix, Module &M) {
  std::ifstream importedFuncs(prefix+"imported_func.txt");
  std::ifstream definedFuncs(prefix+"defined_func.txt");
  std::ifstream blackFuncs(prefix+"blacklist.txt");
  std::ifstream static_funcptr(prefix+"static_funcptr.txt");
  std::ifstream static_func(prefix+"static_func.txt");
  std::ifstream lock_funcs(prefix+"lock_func.txt");
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
    driverFuncPtrCallTargetMap[funcPtrLine] =
        staticFuncLine;  // map each function ptr name with corresponding
                         // defined func in isolated device
  }
  for (std::string line; std::getline(importedFuncs, line);)
    if (blackFuncList.find(line) == blackFuncList.end())
      importedFuncList.insert(line);
  for (std::string line; std::getline(definedFuncs, line);)
    definedFuncList.insert(line);

  // importedFuncList.insert(staticFuncptrList.begin(), staticFuncptrList.end());
  seenFuncOps = false;
  kernelFuncList = importedFuncList;

  auto &pdgUtils = PDGUtils::getInstance();
  PDG = &getAnalysis<pdg::ProgramDependencyGraph>();

  if (!USEDEBUGINFO)
  {
    errs() << "[WARNING] No debug information avaliable... \nUse [-debug 1] in the pass to generate debug information\n";
    exit(0);
  }

  CG = &getAnalysis<CallGraphWrapperPass>().getCallGraph();

  // Get the main Functions closure for root ECALLs
  auto main = M.getFunction(StringRef("main"));
  auto mainClosure = getTransitiveClosure(*main);

  std::string file_name = "Enclave.edl";
  idl_file.open(file_name);
  idl_file << "enclave" << " {\n\n\ttrusted {\n";

  for (auto funcName : importedFuncList)
  {
    sharedFieldMap.clear();
    crossBoundary = false;
    curImportedTransFuncName = funcName;
    auto func = M.getFunction(StringRef(funcName));
    if (func->isDeclaration())
      continue;
    auto transClosure = getTransitiveClosure(*func);
    for (std::string staticFuncName : staticFuncList)
    {
      Function* staticFunc = M.getFunction(StringRef(staticFuncName));
      if (staticFunc && !staticFunc->isDeclaration())
        transClosure.push_back(staticFunc);
    }
    for (auto iter = transClosure.rbegin(); iter != transClosure.rend(); iter++)
    {
      auto transFunc = *iter;
      if (transFunc->isDeclaration())
        continue; 
      if (definedFuncList.find(transFunc->getName()) != definedFuncList.end() || staticFuncList.find(transFunc->getName()) != staticFuncList.end())
        crossBoundary = true;
      getIntraFuncReadWriteInfoForFunc(*transFunc);
      getInterFuncReadWriteInfo(*transFunc);
    }
    if (std::find(mainClosure.begin(), mainClosure.end(), func) != mainClosure.end()) // This function is in main Funcs closure
      generateIDLforFunc(*func, true); // It is possibly a root ECALL
    else generateIDLforFunc(*func, false);
    idl_file <<";\n\n";
  }

  idl_file << "\t};\n";

  // printGlobalLockWarningFunc();

  idl_file.close();
  importedFuncs.close();
  definedFuncs.close();
  blackFuncs.close();
  static_funcptr.close();
  static_func.close();
  lock_funcs.close();
  // errs() << "\n\n";
  // for (auto &F : M)
  // {
  //   if (F.isDeclaration() || ((importedFuncList.find(F.getName()) == importedFuncList.end()) && (staticFuncList.find(F.getName().str()) == staticFuncList.end())) || F.isIntrinsic())
  //     continue;
  //   printFuncArgAccessInfo(F);
  // }
}

void pdg::AccessInfoTracker::createUntrusted(std::string prefix, Module &M) {
  std::ifstream importedFuncs(prefix+ "imported_func.txt");
  std::ifstream definedFuncs(prefix+"defined_func.txt");
  std::ifstream blackFuncs(prefix+"blacklist.txt");
  std::ifstream static_funcptr(prefix+"static_funcptr.txt");
  std::ifstream static_func(prefix+"static_func.txt");
  std::ifstream lock_funcs(prefix+"lock_func.txt");
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
    driverFuncPtrCallTargetMap[funcPtrLine] =
        staticFuncLine;  // map each function ptr name with corresponding
                         // defined func in isolated device
  }
  for (std::string line; std::getline(importedFuncs, line);)
    if (blackFuncList.find(line) == blackFuncList.end())
      importedFuncList.insert(line);
  for (std::string line; std::getline(definedFuncs, line);)
    definedFuncList.insert(line);

  // importedFuncList.insert(staticFuncptrList.begin(), staticFuncptrList.end());
  seenFuncOps = false;
  kernelFuncList = importedFuncList;

  auto &pdgUtils = PDGUtils::getInstance();
  PDG = &getAnalysis<pdg::ProgramDependencyGraph>();
  if (!USEDEBUGINFO)
  {
    errs() << "[WARNING] No debug information avaliable... \nUse [-debug 1] in the pass to generate debug information\n";
    exit(0);
  }

  CG = &getAnalysis<CallGraphWrapperPass>().getCallGraph();

  std::string file_name = "enclave";
  file_name += ".edl";
  idl_file.open(file_name, std::fstream::in | std::fstream::out | std::fstream::app);
  idl_file << "\n\tuntrusted {\n";

  for (auto funcName : importedFuncList)
  {
    sharedFieldMap.clear();
    crossBoundary = false;
    curImportedTransFuncName = funcName;
    auto func = M.getFunction(StringRef(funcName));
    if (func->isDeclaration())
      continue;
    auto transClosure = getTransitiveClosure(*func);
    for (std::string staticFuncName : staticFuncList)
    {
      Function* staticFunc = M.getFunction(StringRef(staticFuncName));
      if (staticFunc && !staticFunc->isDeclaration())
        transClosure.push_back(staticFunc);
    }
    // Start populating stringstream for allow() syntax of OCALL
    std::ostringstream allow;
    allow << "allow(";
    for (auto iter = transClosure.rbegin(); iter != transClosure.rend(); iter++)
    {
      auto transFunc = *iter;
      if (transFunc->isDeclaration())
        continue;
      if (definedFuncList.find(transFunc->getName()) != definedFuncList.end() || staticFuncList.find(transFunc->getName()) != staticFuncList.end()) {
        if (allow.str().length() > 6)  allow << ", ";
        allow << transFunc->getName().str();
        crossBoundary = true;
      }
      getIntraFuncReadWriteInfoForFunc(*transFunc);
      getInterFuncReadWriteInfo(*transFunc);
    }
    allow << ");";
    generateIDLforFunc(*func, false);
    // Write the allow syntax if there is an ECALL in this OCALL
    if (allow.str().length() > 8)
      idl_file << allow.str() <<"\n\n";
    else idl_file << ";\n\n";
  }

  idl_file << "\t};\n\n};";

  // printGlobalLockWarningFunc();

  idl_file.close();
  importedFuncs.close();
  definedFuncs.close();
  blackFuncs.close();
  static_funcptr.close();
  static_func.close();
  lock_funcs.close();
  // errs() << "\n\n";
  // for (auto &F : M)
  // {
  //   if (F.isDeclaration() || ((importedFuncList.find(F.getName()) == importedFuncList.end()) && (staticFuncList.find(F.getName().str()) == staticFuncList.end())) || F.isIntrinsic())
  //     continue;
  //   printFuncArgAccessInfo(F);
  // }
}

bool pdg::AccessInfoTracker::runOnModule(Module &M)
{
  // Populate function name sets for [string, size, count] attributes
  Heuristics::populateStringFuncs();
  Heuristics::populateMemFuncs();

  // Read the untrusted domain information to create trusted side
  createTrusted(UPREFIX, M);

  // Clear function lists
  lockFuncList.clear();
  blackFuncList.clear();
  staticFuncList.clear();
  importedFuncList.clear();
  definedFuncList.clear();
  kernelFuncList.clear();
  driverFuncPtrCallTargetMap.clear();

  // Read the trusted domain information to create untrusted side
  createUntrusted(TPREFIX, M);
  return false;
}

void pdg::AccessInfoTracker::getAnalysisUsage(AnalysisUsage &AU) const
{
  AU.addRequired<pdg::ProgramDependencyGraph>();
  AU.addRequired<CallGraphWrapperPass>();
  AU.setPreservesAll();
}

std::vector<Function *> pdg::AccessInfoTracker::getTransitiveClosure(Function &F)
{
  std::vector<Function*> transClosure;
  transClosure.push_back(&F);
  std::queue<CallGraphNode*> funcNodeQ;
  std::set<Function*> seen;
  funcNodeQ.push(CG->getOrInsertFunction(&F));
  while (!funcNodeQ.empty())
  {
    if (transClosure.size() > 100) 
      return transClosure; 

    auto callNode = funcNodeQ.front();
    funcNodeQ.pop();
    if (!callNode)
      continue;

    for (auto calleeNodeI = callNode->begin(); calleeNodeI != callNode->end(); calleeNodeI++)
    {
      if (!calleeNodeI->second->getFunction())
        continue;
      auto funcName = calleeNodeI->second->getFunction()->getName();
      if (blackFuncList.find(funcName) != blackFuncList.end())
        continue;
      Function *calleeFunc = calleeNodeI->second->getFunction();
      if (calleeFunc->isDeclaration())
        continue;
      if (seen.find(calleeFunc) != seen.end())
        continue;
      funcNodeQ.push(calleeNodeI->second);
      transClosure.push_back(calleeFunc);
      seen.insert(calleeFunc);
    }
  }
  return transClosure;
}

AccessType pdg::AccessInfoTracker::getAccessTypeForInstW(
    const InstructionWrapper *instW, ArgumentWrapper *argW) {
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
      } else { // continue if function is inaccessible
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
      assert(argNum != -1 && "argument is not found in the callInst's arguments");
      Heuristics::addStringAttribute(funcName, argNum, argW);
      if (DIUtils::isVoidPointerTy(*argW->getArg()))
        Heuristics::addSizeAttribute(funcName, argNum, callInst, argW, PDG);
    }
  }
  return accessType;
}

void pdg::AccessInfoTracker::printRetValueAccessInfo(Function &Func)
{
  auto &pdgUtils = PDGUtils::getInstance();
  for (CallInst *CI : pdgUtils.getFuncMap()[&Func]->getCallInstList())
  {
    CallWrapper *CW = pdgUtils.getCallMap()[CI];
    errs() << "Ret Value Acc Info.." << "\n";
    printArgAccessInfo(CW->getRetW(), TreeType::ACTUAL_IN_TREE);
    errs() << "......... [ END " << Func.getName() << " ] .........\n\n";
  }
}

// intra function
void pdg::AccessInfoTracker::getIntraFuncReadWriteInfoForArg(ArgumentWrapper *argW, TreeType treeTy)
{
  auto argTree = argW->getTree(treeTy);
  if (argTree.size() == 0)
    return;
  // throw new ArgParameterTreeSizeIsZero("Argment tree is empty... Every param should have at least one node...\n");

  auto func = argW->getArg()->getParent();
  auto treeI = argW->getTree(treeTy).begin();
  // if (!(*treeI)->getTreeNodeType()->isPointerTy())

  if ((*treeI)->getDIType() == nullptr)
  {
    errs() << "Empty debugging info for " << func->getName() << " - " << argW->getArg()->getArgNo() << "\n";
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
  for (auto treeI = argW->tree_begin(TreeType::FORMAL_IN_TREE); treeI != argW->tree_end(TreeType::FORMAL_IN_TREE); ++treeI)
  {
    count += 1;
      auto valDepPairList =
          PDG->getNodesWithDepType(*treeI, DependencyType::VAL_DEP);
      for (auto valDepPair : valDepPairList) {
        auto dataW = valDepPair.first->getData();
        AccessType accType = getAccessTypeForInstW(dataW, argW);
        if (static_cast<int>(accType) >
            static_cast<int>((*treeI)->getAccessType())) {
          auto &dbgInstList =
              pdgUtils.getFuncMap()[func]->getDbgDeclareInstList();
          std::string argName =
              DIUtils::getArgName(*(argW->getArg()), dbgInstList);

          if ((unsigned)accType == 2) {
            argW->getAttribute().setOut();
          }
          if (count == 1 && (unsigned)accType == 1) {
            argW->getAttribute().setIn();
          }
          errs() << argName << " n" << count << "-"
                 << getAccessAttributeName(treeI) << " => "
                 << getAccessAttributeName((unsigned)accType) << "\n";

          (*treeI)->setAccessType(accType);
        }
        if (accType != AccessType::NOACCESS) {
          // update shared field map
          auto &dbgInstList =
              pdgUtils.getFuncMap()[func]->getDbgDeclareInstList();
          std::string argName =
              DIUtils::getArgName(*(argW->getArg()), dbgInstList);
          bool isKernel =
              (definedFuncList.find(func->getName()) == definedFuncList.end());
          std::string fieldName =
              argName + DIUtils::getDIFieldName((*treeI)->getDIType());
          updateSharedFieldMap(fieldName, isKernel);
        }
      }
  }
}

void pdg::AccessInfoTracker::updateSharedFieldMap(std::string fieldName, bool isKernel)
{
  auto fname = curImportedTransFuncName + fieldName;
  if (sharedFieldMap.find(fname) == sharedFieldMap.end())
    sharedFieldMap[fname] = std::array<int, 2>();

  if (isKernel)
    sharedFieldMap[fname][0] = 1;
  else
    sharedFieldMap[fname][1] = 1;
}

void pdg::AccessInfoTracker::getIntraFuncReadWriteInfoForFunc(Function &F)
{
  auto &pdgUtils = PDGUtils::getInstance();
  FunctionWrapper *funcW = pdgUtils.getFuncMap()[&F];
  // for arguments
  for (auto argW : funcW->getArgWList())
    getIntraFuncReadWriteInfoForArg(argW, TreeType::FORMAL_IN_TREE);

  // for return value
  ArgumentWrapper *retW = funcW->getRetW();
  getIntraFuncReadWriteInfoForArg(retW, TreeType::FORMAL_IN_TREE);
}

pdg::ArgumentMatchType pdg::AccessInfoTracker::getArgMatchType(Argument *arg1, Argument *arg2)
{
  Type *arg1_type = arg1->getType();
  Type *arg2_type = arg2->getType();

  if (arg1_type == arg2_type)
    return pdg::ArgumentMatchType::EQUAL;

  if (arg1_type->isPointerTy())
    arg1_type = (dyn_cast<PointerType>(arg1_type))->getElementType();

  if (arg1_type->isStructTy())
  {
    StructType *arg1_st_type = dyn_cast<StructType>(arg1_type);
    for (unsigned i = 0; i < arg1_st_type->getNumElements(); ++i)
    {
      Type *arg1_element_type = arg1_st_type->getElementType(i);
      bool type_match = (arg1_element_type == arg2_type);

      if (arg2_type->isPointerTy())
      {
        bool pointed_type_match = ((dyn_cast<PointerType>(arg2_type))->getElementType() == arg1_element_type);
        type_match = type_match || pointed_type_match;
      }

      if (type_match)
        return pdg::ArgumentMatchType::CONTAINED;
    }
  }

  return pdg::ArgumentMatchType::NOTCONTAINED;
}

void pdg::AccessInfoTracker::mergeArgAccessInfo(ArgumentWrapper *callerArgW, ArgumentWrapper *calleeArgW, tree<InstructionWrapper *>::iterator callerTreeI)
{
  if (callerArgW == nullptr || calleeArgW == nullptr)
    return;
  auto callerFunc = callerArgW->getArg()->getParent();
  auto calleeFunc = calleeArgW->getArg()->getParent();
  if (callerFunc == nullptr || calleeFunc == nullptr)
    return;

  unsigned callerParamTreeSize = callerArgW->getTree(TreeType::FORMAL_IN_TREE).size(callerTreeI);
  unsigned calleeParamTreeSize = calleeArgW->getTree(TreeType::FORMAL_IN_TREE).size();

  if (callerParamTreeSize != calleeParamTreeSize)
    return;

  auto calleeTreeI = calleeArgW->tree_begin(TreeType::FORMAL_IN_TREE);
  for (; callerTreeI != callerArgW->tree_end(TreeType::FORMAL_IN_TREE) && calleeTreeI != calleeArgW->tree_end(TreeType::FORMAL_IN_TREE); ++callerTreeI, ++calleeTreeI)
  {
    if (callerTreeI == 0 || calleeTreeI == 0)
      return;
    if (static_cast<int>((*callerTreeI)->getAccessType()) < static_cast<int>((*calleeTreeI)->getAccessType())) {
      (*callerTreeI)->setAccessType((*calleeTreeI)->getAccessType());
    }
  }
}

bool pdg::AccessInfoTracker::getInterFuncReadWriteInfo(Function &F)
{
  auto &pdgUtils = PDGUtils::getInstance();
  bool reachFixPoint = true;

  if (pdgUtils.getFuncMap()[&F] == nullptr)
    return true;

  for (auto argW : pdgUtils.getFuncMap()[&F]->getArgWList())
  {
    if (!PDG->isStructPointer(argW->getArg()->getType()))
      continue;

    for (auto treeI = argW->tree_begin(TreeType::FORMAL_IN_TREE); treeI != argW->tree_end(TreeType::FORMAL_IN_TREE); ++treeI)
    {
      // find related instruction nodes
      auto treeNodeDepVals = PDG->getNodesWithDepType(*treeI, DependencyType::VAL_DEP);
      // for each dependency value, check whether it is passed through function call
      for (auto pair : treeNodeDepVals)
      {
        auto depInstW = pair.first->getData();
        auto depCallInsts = PDG->getNodesWithDepType(depInstW, DependencyType::DATA_CALL_PARA);
        // find instructions uses the dep value
        for (auto depCallPair : depCallInsts)
        {
          auto depCallInstW = depCallPair.first->getData();
          int paraIdx = getCallParamIdx(depInstW, depCallInstW);
          auto callInst = dyn_cast<CallInst>(depCallInstW->getInstruction());
          if (PDG->isIndirectCallOrInlineAsm(callInst))
          {
            if (callInst->isInlineAsm())
              continue;
            // indirect call
            auto indirect_call_candidates = PDG->collectIndirectCallCandidates(callInst->getFunctionType(), F, definedFuncList);
            for (auto indirect_call : indirect_call_candidates)
            {
              if (indirect_call == nullptr) continue;
              if (pdgUtils.getFuncMap().find(indirect_call) == pdgUtils.getFuncMap().end()) continue;
              auto calleeFuncW = pdgUtils.getFuncMap()[indirect_call];
              auto calleeArgW = calleeFuncW->getArgWByIdx(paraIdx);
              if (!calleeFuncW->hasTrees()) continue;
              
              mergeArgAccessInfo(argW, calleeArgW, tree<InstructionWrapper *>::parent(treeI));
              // importedFuncList.insert(indirect_call->getName().str()); // put new discovered function into imported list for analysis
              reachFixPoint = false;
            }
          }
          else
          {
            auto f = callInst->getCalledFunction();
            if (f == nullptr)
              f = dyn_cast<Function>(callInst->getCalledValue()->stripPointerCasts());
            if (f == nullptr) continue;
            if (f->isDeclaration() || pdgUtils.getFuncMap().find(f) == pdgUtils.getFuncMap().end()) continue;
            if (f->isVarArg()) continue;// do not handle arity function

            FunctionWrapper *calleeFuncW = pdgUtils.getFuncMap()[f];
            ArgumentWrapper *calleeArgW = calleeFuncW->getArgWByIdx(paraIdx);
            if (!calleeFuncW->hasTrees())
              continue;

            mergeArgAccessInfo(argW, calleeArgW, tree<InstructionWrapper *>::parent(treeI));
          }
        }
      }
    }
  }
  return reachFixPoint;
}

int pdg::AccessInfoTracker::getCallParamIdx(const InstructionWrapper *instW, const InstructionWrapper *callInstW)
{
  Instruction *inst = instW->getInstruction();
  Instruction *callInst = callInstW->getInstruction();
  if (inst == nullptr || callInst == nullptr)
    return -1;

  if (CallInst *CI = dyn_cast<CallInst>(callInst))
  {
    int paraIdx = 0;
    for (auto arg_iter = CI->arg_begin(); arg_iter != CI->arg_end(); ++arg_iter)
    {
      if (Instruction *tmpInst = dyn_cast<Instruction>(&*arg_iter))
      {
        if (tmpInst == inst)
          return paraIdx;
      }
      paraIdx++;
    }
  }
  return -1;
}

void pdg::AccessInfoTracker::printFuncArgAccessInfo(Function &F)
{
  auto &pdgUtils = PDGUtils::getInstance();
  errs() << "For function: " << F.getName() << "\n";
  FunctionWrapper *funcW = pdgUtils.getFuncMap()[&F];
  for (auto argW : funcW->getArgWList())
  {
    printArgAccessInfo(argW, TreeType::FORMAL_IN_TREE);
  }
  printArgAccessInfo(funcW->getRetW(), TreeType::FORMAL_IN_TREE);
  errs() << "......... [ END " << F.getName() << " ] .........\n\n";
}

void pdg::AccessInfoTracker::printArgAccessInfo(ArgumentWrapper *argW, TreeType ty)
{
  std::vector<std::string> access_name = {
      "No Access",
      "Read",
      "Write"};

  errs() << argW->getArg()->getParent()->getName() << " Arg use information for arg no: " << argW->getArg()->getArgNo() << "\n";
  errs() << "Size of argW: " << argW->getTree(ty).size() << "\n";

  for (auto treeI = argW->tree_begin(ty);
       treeI != argW->tree_end(ty);
       ++treeI)
  {
    if ((argW->getTree(ty).depth(treeI) > EXPAND_LEVEL))
      return;
    InstructionWrapper *curTyNode = *treeI;
    if (curTyNode->getDIType() == nullptr)
      return;

    Type *parentTy = curTyNode->getParentTreeNodeType();
    Type *curType = curTyNode->getTreeNodeType();

    errs() << "Num of child: " << tree<InstructionWrapper *>::number_of_children(treeI) << "\n";

    if (parentTy == nullptr)
    {
      errs() << "** Root type node **" << "\n";
      errs() << "Field name: " << DIUtils::getDIFieldName(curTyNode->getDIType()) << "\n";
      errs() << "Access Type: " << access_name[static_cast<int>(curTyNode->getAccessType())] << "\n";
      errs() << dwarf::TagString(curTyNode->getDIType()->getTag()) << "\n";
      errs() << ".............................................\n";
      continue;
    }

    errs() << "sub field name: " << DIUtils::getDIFieldName(curTyNode->getDIType()) << "\n";
    errs() << "Access Type: " << access_name[static_cast<int>(curTyNode->getAccessType())] << "\n";
    errs() << dwarf::TagString(curTyNode->getDIType()->getTag()) << "\n";
    errs() << "..............................................\n";
  }
}

void pdg::AccessInfoTracker::generateRpcForFunc(Function &F, bool root)
{
  auto &pdgUtils = PDGUtils::getInstance();
  DIType *funcRetType = DIUtils::getFuncRetDIType(F);
  std::string retTypeName;
  if (funcRetType == nullptr)
    retTypeName = "void";
  else
    retTypeName = DIUtils::getDITypeName(funcRetType);

  // handle return type, concate with function name
  if (PDG->isStructPointer(F.getReturnType())) // generate alloc(caller) as return struct pointer is from the other side
  {
    // auto funcName = F.getName().str();
    // if (retTypeName.back() == '*')
    // {
    //   retTypeName.pop_back();
    //   funcName.push_back('*');
    // }
    retTypeName = "struct " + retTypeName;
  }
  
  idl_file << "\t\t";
  if (root) idl_file << "public ";
  idl_file << retTypeName << " " << F.getName().str() << "( ";
  // handle parameters
  for (auto argW : pdgUtils.getFuncMap()[&F]->getArgWList())
  {
    Argument &arg = *argW->getArg();
    Type *argType = arg.getType();
    auto &dbgInstList = pdgUtils.getFuncMap()[&F]->getDbgDeclareInstList();
    std::string argName = DIUtils::getArgName(arg, dbgInstList);
    if (PDG->isStructPointer(argType))
    {
      idl_file <<argW->getAttribute().dump() << " struct " << DIUtils::getArgTypeName(arg) <<" "<< argName;
    }
    // else if (PDG->isFuncPointer(argType)) {
    //   idl_file << DIUtils::getFuncSigName(DIUtils::getLowestDIType(DIUtils::getArgDIType(arg)), argName, "");
    // }
    else {
      if (argType->getTypeID() == 15)
        idl_file << argW->getAttribute().dump() << " " << DIUtils::getArgTypeName(arg) << " " << argName;
      else
        idl_file << DIUtils::getArgTypeName(arg) << " " << argName;
    }

    if (argW->getArg()->getArgNo() < F.arg_size() - 1 && !argName.empty())
      idl_file << ", ";
  }
  idl_file << " )";//\n\n";
}

void pdg::AccessInfoTracker::generateIDLforFunc(Function &F, bool root)
{
  // if a function is defined on the same side, no need to generate IDL rpc for this function.
  auto &pdgUtils = PDGUtils::getInstance();
  FunctionWrapper *funcW = pdgUtils.getFuncMap()[&F];
  for (auto argW : funcW->getArgWList())
  {
    generateIDLforArg(argW, TreeType::FORMAL_IN_TREE);
  }
  generateIDLforArg(funcW->getRetW(), TreeType::FORMAL_IN_TREE);
  generateRpcForFunc(F, root);
}

void pdg::AccessInfoTracker::generateIDLforFuncPtrWithDI(DIType *funcDIType, Module *module, std::string funcPtrName)
{
  if (!DIUtils::isFuncPointerTy(funcDIType))
    return;
  std::vector<Function *> indirectCallTargets;
  if (driverFuncPtrCallTargetMap.find(funcPtrName) != driverFuncPtrCallTargetMap.end())
    indirectCallTargets.push_back(module->getFunction(StringRef(driverFuncPtrCallTargetMap[funcPtrName])));
  else
    indirectCallTargets = DIUtils::collectIndirectCallCandidatesWithDI(funcDIType, module, driverFuncPtrCallTargetMap);

  assert(indirectCallTargets.size() >= 1 && "indirect call target size < 1. Cannot generate IDL for such indirect call function");

  auto &pdgUtils = PDGUtils::getInstance();
  auto mergeToFunc = indirectCallTargets[0];
  auto mergeToFuncW = pdgUtils.getFuncMap()[mergeToFunc];

  for (auto f : indirectCallTargets)
  {
    if (!pdgUtils.getFuncMap()[f]->hasTrees())
      PDG->buildPDGForFunc(f);
    getIntraFuncReadWriteInfoForFunc(*f);
    getInterFuncReadWriteInfo(*f);
  }

  for (int i = 0; i < mergeToFunc->arg_size(); ++i)
  {
    auto mergeToArgW = mergeToFuncW->getArgWByIdx(i);
    auto mergeToArg = mergeToArgW->getArg();
    if (!PDG->isStructPointer(mergeToArg->getType()))
      continue;
    auto oldMergeToArgTree = tree<InstructionWrapper *>(mergeToArgW->getTree(TreeType::FORMAL_IN_TREE));
    for (int j = 1; j < indirectCallTargets.size(); ++j)
    {
      auto mergeFromArgW = pdgUtils.getFuncMap()[indirectCallTargets[j]]->getArgWByIdx(i);
      mergeArgAccessInfo(mergeToArgW, mergeFromArgW, mergeToArgW->tree_begin(TreeType::FORMAL_IN_TREE));
    }
    // after merging, start generating projection
    auto funcName = DIUtils::getDIFieldName(funcDIType);
    generateIDLforArg(mergeToArgW, TreeType::FORMAL_IN_TREE, funcPtrName, true);
    // printArgAccessInfo(mergeToArgW, TreeType::FORMAL_IN_TREE);
    mergeToArgW->setTree(oldMergeToArgTree, TreeType::FORMAL_IN_TREE);
  }
}

void pdg::AccessInfoTracker::generateIDLforArg(ArgumentWrapper *argW, TreeType treeTy, std::string funcName, bool handleFuncPtr)
{
  auto &pdgUtils = PDGUtils::getInstance();
  if (argW->getTree(TreeType::FORMAL_IN_TREE).size() == 0 || argW->getArg()->getArgNo() == 100) // No need to handle return args
    return;

  Function &F = *argW->getArg()->getParent();
  auto check = argW->tree_begin(treeTy);
  
  if (funcName.empty())
    funcName = F.getName().str();
  

  auto &dbgInstList = pdgUtils.getFuncMap()[&F]->getDbgDeclareInstList();
  std::string argName = DIUtils::getArgName(*(argW->getArg()), dbgInstList);
  std::string sharedFieldPrefix = curImportedTransFuncName + argName;
  auto curDIType = (*check)->getDIType();
   if (DIUtils::isStructPointerTy(curDIType)) {
     // TODO: Put struct definition on top
     idl_file << DIUtils::getStructDefinition(*argW->getArg());
   }
  // TODO: Fix header include statements
  DIFile *file = curDIType->getFile();
  if (file != nullptr)
          idl_file
      << "\t\tinclude \"" << file->getFilename().str()
      << "\" // For param: " << argName << "\n\n ";

  // std::queue<tree<InstructionWrapper *>::iterator> treeNodeQ;
  // treeNodeQ.push(argW->tree_begin(treeTy));

  // while (!treeNodeQ.empty()) {
  //   errs() << "Node\n";
  //   auto treeI = treeNodeQ.front();
  //   treeNodeQ.pop();
  //   auto curDIType = (*treeI)->getDIType();

  //   if (curDIType == nullptr) continue;

  //   std::stringstream projection_str;
  //   bool accessed = false;

  //   // only process sturct pointer and function pointer, these are the only
  //   // types that we should generate projection for
  //   if (!DIUtils::isStructPointerTy(curDIType))
  //     continue;
  //   // std::string ptrStarNum = "";
  //   // std::string structPtrName = DIUtils::getDITypeName(curDIType);
  //   // while (structPtrName.back() == '*')
  //   // {
  //   //   structPtrName.pop_back();
  //   //   ptrStarNum.push_back('*');
  //   // }

  //   curDIType = DIUtils::getLowestDIType(curDIType);

  //   if (!DIUtils::isStructTy(curDIType)) continue;

  //   errs() << "Arg " << argName << "\n";

  //   treeI++;
  //   for (int i = 0; i < tree<InstructionWrapper *>::number_of_children(treeI);
  //        ++i) {
  //     auto childT = tree<InstructionWrapper *>::child(treeI, i);
  //     auto childDIType = (*childT)->getDIType();
  //     if ((*childT)->getAccessType() == AccessType::NOACCESS) continue;
  //     std::string sharedFieldName =
  //         sharedFieldPrefix + DIUtils::getDIFieldName(childDIType);

  //     // if (!handleFuncPtr && !DIUtils::isFuncPointerTy(childDIType) &&
  //     //     sharedFieldMap.find(sharedFieldName) == sharedFieldMap.end())
  //     //   continue;

  //     auto accessStat = sharedFieldMap[sharedFieldName];
  //     // only check access status under cross boundary case. If not cross, we do
  //     // not check and simply perform normal field finding analysis.
  //     crossBoundary = false;
  //     if (crossBoundary && (accessStat[0] == 0 || accessStat[1] == 0) && !handleFuncPtr)
  //       continue;

  //     accessed = true;
  //     if (DIUtils::isStructPointerTy(childDIType)) {
  //       // for struct pointer, generate a projection
  //       auto tmpName = DIUtils::getDITypeName(childDIType);
  //       auto tmpFuncName = funcName;
  //       // formatting functions
  //       while (tmpName.back() == '*') {
  //         tmpName.pop_back();
  //         tmpFuncName.push_back('*');
  //       }

  //       std::string fieldTypeName = tmpName;
  //       if (fieldTypeName.find("device_ops") ==
  //           std::string::npos)  // specific handling for function ops
  //         fieldTypeName = tmpName + "_" + tmpFuncName;

  //       projection_str << "\t\t"
  //                      << "projection " << fieldTypeName << " "
  //                      << " " << DIUtils::getDIFieldName(childDIType) << ";\n";
  //       treeNodeQ.push(childT);
  //     } else if (DIUtils::isStructTy(childDIType)) {
  //       continue;
  //     } else {
  //       std::string fieldName = DIUtils::getDIFieldName(childDIType);
  //       if (!fieldName.empty())
  //         projection_str << "\t\t" + DIUtils::getDITypeName(childDIType) << " "
  //                        << getAccessAttributeName(childT) << " "
  //                        << DIUtils::getDIFieldName(childDIType) << ";\n";
  //     }
  //   }

  //   // if any child field is accessed, we need to print out the projection for
  //   // the current struct.
  //   if (DIUtils::isStructTy(curDIType)) {
  //     std::stringstream temp;
  //     std::string structName = DIUtils::getDIFieldName(curDIType);

  //     // a very specific handling for generating IDL for function ops
  //     if (structName.find("device_ops") == std::string::npos) {
  //       temp << "\tprojection "
  //            << "< struct " << structName << " > " << structName << "_"
  //            << funcName << " "
  //            << " {\n"
  //            << projection_str.str() << " \t}\n\n";
  //     } else {
  //       if (!seenFuncOps) {
  //         temp << "\tprojection "
  //              << "< struct " << structName << " > " << structName << " "
  //              << " {\n"
  //              << projection_str.str() << " \t}\n\n";
  //         seenFuncOps = true;
  //       }
  //     }

  //     projection_str = std::move(temp);
  //   } else
  //     projection_str.str("");

  //   idl_file << projection_str.str();
  //   accessed = false;
  // }
}

std::string pdg::getAccessAttributeName(tree<InstructionWrapper *>::iterator treeI)
{
  std::vector<std::string> access_attribute = {
      "[-]",
      "[in]",
      "[out]"};
  int accessIdx = static_cast<int>((*treeI)->getAccessType());
  return access_attribute[accessIdx];
}

std::string pdg::getAccessAttributeName(unsigned accessIdx) {
  std::vector<std::string> access_attribute = {
      "[-]",
      "[in]",
      "[out]"};
  return access_attribute[accessIdx];
}

static RegisterPass<pdg::AccessInfoTracker>
    AccessInfoTracker("accinfo-track", "Argument access information tracking Pass", false, true);