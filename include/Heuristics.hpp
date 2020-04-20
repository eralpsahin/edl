#ifndef _HEURISTICS_H_
#define _HEURISTICS_H_
#include <fstream>
#include <iostream>
#include <map>
#include <string>

#include "ArgumentWrapper.hpp"
#include "ProgramDependencyGraph.hpp"
#include "json.hpp"

namespace pdg {

class Heuristics {
 public:
  static void populateStringFuncs() {
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

  static void populateMemFuncs() {
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

  /**
   *
   * TODO: void pointers should have size attribute only.
   * TODO: Other pointer types should have count attribute only..
   *
   */

  static void addSizeAttribute(std::string funcName, int argNum,
                               llvm::CallInst* callInst,
                               ArgumentWrapper* argW, ProgramDependencyGraph * PDG) {
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
        auto dataDList = PDG->getNodeDepList(llvm::dyn_cast<llvm::Instruction>(arg));
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
  static void addStringAttribute(std::string funcName, int argNum,
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

 private:
  static std::set<std::string> inStr;
  static std::set<std::string> outStr;
  static std::set<std::string> inMem;
  static std::set<std::string> outMem;
};
// Initialize static data members
std::set<std::string> Heuristics::inStr;
std::set<std::string> Heuristics::outStr;
std::set<std::string> Heuristics::inMem;
std::set<std::string> Heuristics::outMem;

}  // namespace pdg

#endif