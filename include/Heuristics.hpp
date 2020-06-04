#ifndef _HEURISTICS_H_
#define _HEURISTICS_H_
#include <fstream>
#include <iostream>
#include <map>
#include <string>

#include "ArgumentWrapper.hpp"
#include "ProgramDependencyGraph.hpp"

namespace pdg {

class Heuristics {
 public:
  static void populateStringFuncs();
  static void populateMemFuncs();
  static void populateprintfFuncs();
  static void addSizeAttribute(std::string funcName, int argNum,
                               llvm::CallInst* callInst, ArgumentWrapper* argW,
                               ProgramDependencyGraph* PDG);
  static void addStringAttribute(std::string funcName, int argNum,
                                 ArgumentWrapper* argW);
  static void checkPrintf(llvm::CallInst* callInst, int argNum,
                          ArgumentWrapper* argW);
  static std::set<unsigned> parseFormatString(const std::string& format,
                                              unsigned startIdx);

 private:
  static std::set<std::string> inStr;
  static std::set<std::string> outStr;
  static std::set<std::string> inMem;
  static std::set<std::string> outMem;
  static std::map<std::string, unsigned> printfFuncs;
};

}  // namespace pdg

#endif