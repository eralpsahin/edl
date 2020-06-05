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
  static void populateStringFuncs();
  static void populateMemFuncs();
  static void populateprintfFuncs();
  static void populateMapFrom(std::map<std::string, std::set<unsigned>>& map,
                              json::JSON json);
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
  static std::map<std::string, std::set<unsigned>> inStr;
  static std::map<std::string, std::set<unsigned>> outStr;
  static std::map<std::string, std::set<unsigned>> inMem;
  static std::map<std::string, std::set<unsigned>> outMem;
  static std::map<std::string, unsigned> printfFuncs;
};

}  // namespace pdg

#endif