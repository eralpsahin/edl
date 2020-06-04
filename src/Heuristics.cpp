#include "Heuristics.hpp"

#include "json.hpp"

// Initialize static data members
std::set<std::string> Heuristics::inStr;
std::set<std::string> Heuristics::outStr;
std::map<std::string, unsigned> Heuristics::printfFuncs;
std::set<std::string> Heuristics::inMem;
std::set<std::string> Heuristics::outMem;

void Heuristics::populateStringFuncs() {
  json::JSON strJSON;
  std::ifstream t("../heuristics/str.json");
  std::stringstream buffer;
  buffer << t.rdbuf();
  strJSON = json::JSON::Load(buffer.str());  // Load the json
  unsigned length = strJSON["in"].length();

  // Add function names to in and out vectors
  for (unsigned i = 0; i < length; i += 1) {
    inStr.insert(strJSON["in"][i].ToString());
  }

  length = strJSON["out"].length();
  for (unsigned i = 0; i < length; i += 1) {
    outStr.insert(strJSON["out"][i].ToString());
  }
}

void Heuristics::populateMemFuncs() {
  json::JSON memJSON;
  std::ifstream t("../heuristics/mem.json");
  std::stringstream buffer;
  buffer << t.rdbuf();
  memJSON = json::JSON::Load(buffer.str());  // Load the json

  unsigned length = memJSON["in"].length();

  // Add function names to in and out vectors
  for (unsigned i = 0; i < length; i += 1) {
    inMem.insert(memJSON["in"][i].ToString());
  }

  length = memJSON["out"].length();
  for (unsigned i = 0; i < length; i += 1) {
    outMem.insert(memJSON["out"][i].ToString());
  }
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
    auto arg = callInst->getOperand(2);
    if (llvm::ConstantInt* CI = llvm::dyn_cast<llvm::ConstantInt>(arg)) {
      // Size is a literal
      argW->getAttribute().setSize(std::to_string(CI->getZExtValue()));
    } else {
      // Size is not a literal
      // TODO: Handle local vars and arguments
      auto dataDList =
          PDG->getNodeDepList(llvm::dyn_cast<llvm::Instruction>(arg));
      for (auto depPair : dataDList) {
        InstructionWrapper* depInstW =
            const_cast<InstructionWrapper*>(depPair.first->getData());
        // TODO: try to get the argument
      }
    }
    if (inMem.find(funcName) != inMem.end() ||
        (outMem.find(funcName) != outMem.end() && argNum == 1))
      argW->getAttribute().setIn();
    else if (outMem.find(funcName) != outMem.end() && argNum == 0)
      argW->getAttribute().setOut();
  }
}

/**
 * String attribute is required to have in attribute.
 *
 */
void Heuristics::addStringAttribute(std::string funcName, int argNum,
                                    ArgumentWrapper* argW) {
  if (inStr.find(funcName) != inStr.end() ||
      outStr.find(funcName) !=
          outStr.end()) {  // argument of a string functions
    argW->getAttribute().setString();
  }
  if (outStr.find(funcName) != outStr.end() &&
      argNum == 0) {  // First argument means modification
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
          llvm::dyn_cast<llvm::Constant>(callInst->getArgOperand(0))) {
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