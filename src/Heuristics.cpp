#include "Heuristics.hpp"

// Initialize static data members
std::map<std::string, std::set<unsigned>> Heuristics::inStr;
std::map<std::string, std::set<unsigned>> Heuristics::outStr;
std::map<std::string, unsigned> Heuristics::printfFuncs;
std::map<std::string, std::set<unsigned>> Heuristics::inMem;
std::map<std::string, std::set<unsigned>> Heuristics::outMem;

void Heuristics::populateStringFuncs() {
  json::JSON strJSON;
  std::ifstream t("../heuristics/str.json");
  std::stringstream buffer;
  buffer << t.rdbuf();
  strJSON = json::JSON::Load(buffer.str());  // Load the json
  populateMapFrom(inStr, strJSON["in"]);
  populateMapFrom(outStr, strJSON["out"]);
}

void Heuristics::populateMemFuncs() {
  json::JSON memJSON;
  std::ifstream t("../heuristics/mem.json");
  std::stringstream buffer;
  buffer << t.rdbuf();
  memJSON = json::JSON::Load(buffer.str());  // Load the json

  populateMapFrom(inMem, memJSON["in"]);
  populateMapFrom(outMem, memJSON["out"]);
}

void Heuristics::populateprintfFuncs() {
  json::JSON printfJSON;
  std::ifstream t("../heuristics/printf.json");
  std::stringstream buffer;
  buffer << t.rdbuf();
  printfJSON = json::JSON::Load(buffer.str());

  unsigned length = printfJSON.length();
  for (unsigned i = 0; i < length; i += 1) {
    printfFuncs[printfJSON[i][0].ToString()] = printfJSON[i][1].ToInt();
  }
}

void Heuristics::populateMapFrom(std::map<std::string, std::set<unsigned>>& map,
                                 json::JSON json) {
  unsigned length = json.length();
  for (unsigned i = 0; i < length; i += 1) {
    std::string funcName = json[i]["name"].ToString();
    map.insert(std::pair<std::string, std::set<unsigned>>(funcName, {}));
    unsigned args = json[i]["idx"].length();
    for (unsigned j = 0; j < args; j += 1) {
      map[funcName].insert(json[i]["idx"][j].ToInt());
    }
  }
}

/**
 * Void pointers should have size attributes.
 *
 */

void Heuristics::addSizeAttribute(std::string funcName, int argNum,
                                  llvm::CallInst* callInst,
                                  ArgumentWrapper* argW,
                                  ProgramDependencyGraph* PDG) {
  if (inMem.find(funcName) != inMem.end() ||
      outMem.find(funcName) != outMem.end()) {
    // size argument
    auto val = callInst->getOperand(2);
    if (llvm::ConstantInt* CI = llvm::dyn_cast<llvm::ConstantInt>(val)) {
      // Size is a literal
      argW->getAttribute().setSize(std::to_string(CI->getZExtValue()));
    } else {
      // Size is not a literal get alias instructions
      auto paramDepList =
          PDG->getNodeDepList(llvm::dyn_cast<llvm::Instruction>(val));
      std::set<llvm::Instruction*> insts;  // insert alias insts here
      insts.insert(llvm::dyn_cast<llvm::Instruction>(val));
      for (auto depPair : paramDepList) {
        InstructionWrapper* depInstW =
            const_cast<InstructionWrapper*>(depPair.first->getData());
        if (depPair.second == DependencyType::DATA_ALIAS) {
          insts.insert(depInstW->getInstruction());
        }
      }

      auto& pdgUtils = PDGUtils::getInstance();
      FunctionWrapper* funcW = pdgUtils.getFuncMap()[callInst->getFunction()];
      for (auto argWIt : funcW->getArgWList()) {
        // Get the allocation instruction dependencies
        auto inst = PDG->getArgAllocaInst(*argWIt->getArg());
        auto argAllocDepList = PDG->getNodeDepList(inst);
        for (auto depPair : argAllocDepList) {
          InstructionWrapper* depInstW =
              const_cast<InstructionWrapper*>(depPair.first->getData());
          if (depPair.second == DependencyType::DATA_READ) {
            if (insts.find(depInstW->getInstruction()) != insts.end()) {
              std::string argName = DIUtils::getArgName(
                  *argWIt->getArg(), funcW->getDbgDeclareInstList());
              argW->getAttribute().setSize(argName);
              break;
            }
          }
        }
      }
    }
    if (inMem.find(funcName) != inMem.end() &&
        inMem[funcName].find(argNum) != inMem[funcName].end())
      argW->getAttribute().setIn();
    else if (outMem.find(funcName) != outMem.end() &&
             outMem[funcName].find(argNum) != outMem[funcName].end())
      argW->getAttribute().setOut();
  }
}

/**
 * String attribute is required to have in attribute.
 *
 */
void Heuristics::addStringAttribute(std::string funcName, int argNum,
                                    ArgumentWrapper* argW) {
  if (inStr.find(funcName) != inStr.end() &&
      inStr[funcName].find(argNum) !=
          outStr[funcName].end()) {  // argument of a string functions
    argW->getAttribute().setString();
  }
  if (outStr.find(funcName) != outStr.end() &&
      outStr[funcName].find(argNum) !=
          outStr[funcName].end()) {  // First argument means modification
    argW->getAttribute().setOut();
  }
}

/**
 * Parse string literals in printf calls to check %s specifiers for
 * string attribute.
 *
 */
void Heuristics::checkPrintf(llvm::CallInst* callInst, int argNum,
                             ArgumentWrapper* argW) {
  std::string funcName = callInst->getCalledFunction()->getName().str();
  if (printfFuncs.find(funcName) == printfFuncs.end()) return;
  unsigned startIdx = printfFuncs[funcName];
  if (llvm::Constant* cons =
          llvm::dyn_cast<llvm::Constant>(callInst->getArgOperand(startIdx))) {
    if (llvm::GlobalVariable* var =
            llvm::dyn_cast<llvm::GlobalVariable>(cons->getOperand(0))) {
      if (llvm::ConstantDataArray* arr =
              llvm::dyn_cast<llvm::ConstantDataArray>(var->getInitializer())) {
        std::set<unsigned> stringIdx =
            parseFormatString(arr->getAsString(), startIdx);
        if (stringIdx.find(argNum) != stringIdx.end()) {
          argW->getAttribute().setString();
        }
      }
    }
  }
}

/**
 * This function parses printf formatted string literal and returns arg indices
 * of %s.
 *
 */
std::set<unsigned> Heuristics::parseFormatString(const std::string& format,
                                                 unsigned startIdx) {
  const char printfSpecifier[] = {'%', 'd', 'i', 'u', 'f', 'F', 'e',
                                  'E', 'g', 'G', 'x', 'X', 'a', 'A',
                                  'o', 's', 'c', 'p', 'n'};
  std::set<unsigned> stringIdx;
  unsigned pos = 0;
  pos = format.find_first_of('%', pos);
  unsigned argIndex = startIdx;
  while (pos < format.length()) {
    pos = format.find_first_of(printfSpecifier, ++pos);
    if (pos < format.length()) {
      if (format[pos] != '%') {
        argIndex += 1;  // New specifier other than % means new arg to printf
        if (format[pos] == 's') stringIdx.insert(argIndex);
      }
      pos = format.find_first_of("%", ++pos);
    }
  }
  return stringIdx;
}