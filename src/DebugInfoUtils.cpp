#include "DebugInfoUtils.hpp"

using namespace llvm;

std::string pdg::DIUtils::getArgName(Argument& arg, std::vector<DbgDeclareInst*> dbgInstList)
{
  Function *F = arg.getParent();
  SmallVector<std::pair<unsigned, MDNode *>, 20> func_MDs;
  for (auto I : dbgInstList)
  {
    if (auto dbi = dyn_cast<DbgDeclareInst>(I))
      if (auto DLV = dyn_cast<DILocalVariable>(dbi->getVariable()))
        if (DLV->getArg() == arg.getArgNo() + 1 && !DLV->getName().empty() && DLV->getScope()->getSubprogram() == F->getSubprogram())
          return DLV->getName().str();
  }

  return "no_name";
}

DIType *pdg::DIUtils::getLowestDIType(DIType *Ty) 
{
  if (Ty->getTag() == dwarf::DW_TAG_pointer_type ||
      Ty->getTag() == dwarf::DW_TAG_member ||
      Ty->getTag() == dwarf::DW_TAG_typedef ||
      Ty->getTag() == dwarf::DW_TAG_const_type)
  {
    DIType *baseTy = dyn_cast<DIDerivedType>(Ty)->getBaseType();
    if (!baseTy)
      return nullptr;

    //Skip all the DINodes with DW_TAG_typedef tag
    while ((baseTy->getTag() == dwarf::DW_TAG_typedef ||
            baseTy->getTag() == dwarf::DW_TAG_const_type ||
            baseTy->getTag() == dwarf::DW_TAG_pointer_type))
    {
      if (DIType *temp = dyn_cast<DIDerivedType>(baseTy)->getBaseType())
        return temp;
      else
        break;
    }
    return baseTy;
  }
  return Ty;
}

DIType *pdg::DIUtils::getArgDIType(Argument &arg)
{
  Function &F = *arg.getParent();
  SmallVector<std::pair<unsigned, MDNode *>, 4> MDs;
  F.getAllMetadata(MDs);
  for (auto &MD : MDs)
  {
    MDNode *N = MD.second;
    if (DISubprogram *subprogram = dyn_cast<DISubprogram>(N))
    {
      auto *subRoutine = subprogram->getType();
      const auto &TypeRef = subRoutine->getTypeArray();
      if (F.arg_size() >= TypeRef.size())
        break;
      int metaDataLoc = 0;
      if (arg.getArgNo() != 100)
        metaDataLoc = arg.getArgNo() + 1;
      const auto &ArgTypeRef = TypeRef[metaDataLoc]; // + 1 to skip return type
      DIType *Ty = ArgTypeRef;
      return Ty;
    }
  }
  return nullptr;
  // throw ArgHasNoDITypeException("Argument doesn't has DIType needed to extract field name...");
}

DIType *pdg::DIUtils::getFuncRetDIType(Function &F)
{
  SmallVector<std::pair<unsigned, MDNode *>, 4> MDs;
  F.getAllMetadata(MDs);
  for (auto &MD : MDs)
  {
    MDNode *N = MD.second;
    if (DISubprogram *subprogram = dyn_cast<DISubprogram>(N))
    {
      auto *subRoutine = subprogram->getType();
      const auto &TypeRef = subRoutine->getTypeArray();
      if (F.arg_size() >= TypeRef.size())
        break;
      const auto &ArgTypeRef = TypeRef[0];
      DIType *Ty = ArgTypeRef;
      return Ty;
    }
  }
  return nullptr;
  // throw ArgHasNoDITypeException("Argument doesn't has DIType needed to extract field name...");
}

DIType *pdg::DIUtils::getBaseDIType(DIType *Ty) {
  if (Ty == nullptr)
    throw DITypeIsNullPtr("DIType is nullptr, cannot get base type");

  if (Ty->getTag() == dwarf::DW_TAG_pointer_type ||
      Ty->getTag() == dwarf::DW_TAG_member ||
      Ty->getTag() == dwarf::DW_TAG_typedef)
  {
    DIType *baseTy = dyn_cast<DIDerivedType>(Ty)->getBaseType();
    if (!baseTy)
    {
      return nullptr;
    }
    return baseTy;
  }
  return Ty;
}

std::string pdg::DIUtils::getDIFieldName(DIType *ty)
{
  if (ty == nullptr)
    return "void";
  switch (ty->getTag())
  {
  case dwarf::DW_TAG_member:
  {
    return ty->getName().str();
  }
  case dwarf::DW_TAG_array_type:
  {
    ty = dyn_cast<DICompositeType>(ty)->getBaseType();
    return "arr_" + ty->getName().str();
  }
  case dwarf::DW_TAG_pointer_type:
  {
    std::string s = getDIFieldName(dyn_cast<DIDerivedType>(ty)->getBaseType());
    return s;
  }
  case dwarf::DW_TAG_subroutine_type:
    return "func ptr";
  case dwarf::DW_TAG_const_type:
  {
    std::string s = getDIFieldName(dyn_cast<DIDerivedType>(ty)->getBaseType());
    return s;
  }
  default:
  {
    if (!ty->getName().str().empty())
      return ty->getName().str();
    return "no name";
  }
  }
}

std::string pdg::DIUtils::getFuncSigName(DIType *ty, std::string funcPtrName, std::string funcName, bool callFromDev)
{
  std::string func_type_str = "";
  if (DISubroutineType *subRoutine = dyn_cast<DISubroutineType>(ty))
  {
    const auto &typeRefArr = subRoutine->getTypeArray();
    // generate name string for return value
    DIType *retType = typeRefArr[0];
    if (retType == nullptr)
      func_type_str += "void ";
    else
      func_type_str += getDITypeName(retType);

    // generate name string for function pointer 
    func_type_str += " (";
    if (!funcPtrName.empty())
      func_type_str += "*";
    func_type_str += funcPtrName;
    if (!funcName.empty())
      func_type_str = func_type_str + "_" + funcName;
    func_type_str += ")";
    // generate name string for arguments in fucntion pointer signature
    func_type_str += "(";
    for (int i = 1; i < typeRefArr.size(); ++i)
    {
      DIType *d = typeRefArr[i];
      if (d == nullptr) // void type
        func_type_str += "void ";
      else // normal types
      {
        if (DIDerivedType *dit = dyn_cast<DIDerivedType>(d))
        {
          auto baseType = dit->getBaseType();
          if (!baseType)
            continue;
          else if (baseType->getTag() == dwarf::DW_TAG_structure_type)
          {
            std::string argTyName = getDITypeName(d);
            if (argTyName.back() == '*')
            {
              argTyName.pop_back();
              argTyName = argTyName + "_" + funcPtrName + "*";
            }
            else
            {
              argTyName = argTyName + "_" + funcPtrName;
            }

            std::string structName = argTyName + " " + getDIFieldName(d);
            if (structName != " ")
              func_type_str = func_type_str + "projection " + structName;
          }
          else
            func_type_str = func_type_str + getDITypeName(d) + " " + getDIFieldName(d);
        }
        else
          func_type_str = func_type_str + getDITypeName(d) + " " + getDIFieldName(d);
      }

      if (i < typeRefArr.size() - 1 && !getDITypeName(d).empty())
        func_type_str += ", ";
    }
    func_type_str += ")";
    return func_type_str;
  }
  return "void";
}

// std::string pdg::DIUtils::getFuncDITypeName(DIType *ty, std::string funcName)
// {
//   if (ty == nullptr)
//     return "void";

//   if (!ty->getTag() || ty == NULL) // process function type, which has not tag
//     return getFuncSigName(ty);

//   try
//   {
//     switch (ty->getTag())
//     {
//     case dwarf::DW_TAG_typedef:
//       return getFuncDITypeName(getBaseDIType(ty), funcName);
//       // return ty->getName().str();
//     case dwarf::DW_TAG_member:
//       return getFuncDITypeName(getBaseDIType(ty), funcName);
//     case dwarf::DW_TAG_array_type:
//     {
//       if (DIType *arrTy = dyn_cast<DICompositeType>(ty)->getBaseType()) {
//         return arrTy->getName().str() + "[" + std::to_string(arrTy->getSizeInBits()) + "]";
//       }
//     }
//     case dwarf::DW_TAG_pointer_type:
//     {
//       std::string s = getFuncDITypeName(dyn_cast<DIDerivedType>(ty)->getBaseType(), funcName);
//       return (s + "_" + funcName + "*");
//     }
//     case dwarf::DW_TAG_subroutine_type:
//       return getFuncSigName(ty);
//     case dwarf::DW_TAG_union_type:
//       return "union";
//     case dwarf::DW_TAG_structure_type:
//     {
//       std::string st_name = ty->getName().str();
//       if (!st_name.empty())
//         return  st_name;
//         // return "struct " + st_name;
//       return "struct";
//     }
//     case dwarf::DW_TAG_const_type:
//       return getFuncDITypeName(dyn_cast<DIDerivedType>(ty)->getBaseType(), funcName);
//       // return "const " + getFuncDITypeName(dyn_cast<DIDerivedType>(ty)->getBaseType().resolve(), funcName);
//     default:
//     {
//       if (!ty->getName().str().empty())
//         return ty->getName().str();
//       return "unknow";
//     }
//     }
//   }
//   catch (std::exception &e)
//   {
//     errs() << e.what();
//     exit(0);
//   }
// }

std::string pdg::DIUtils::getDITypeName(DIType *ty)
{
  if (ty == nullptr)
    return "void";

  if (!ty->getTag() || ty == NULL) // process function type, which has not tag
    return getFuncSigName(ty);

  try
  {
    switch (ty->getTag())
    {
    case dwarf::DW_TAG_typedef:
      if (getBaseDIType(ty)->getTag() == dwarf::DW_TAG_structure_type) {
        return "struct " + ty->getName().str();
      } else if (getBaseDIType(ty)->getTag() == dwarf::DW_TAG_enumeration_type) {
        return "enum " + ty->getName().str();
      } else if (getBaseDIType(ty)->getTag() == dwarf::DW_TAG_union_type) {
        return "union " + ty->getName().str();
      }
        return ty->getName();
      //return getDITypeName(getBaseDIType(ty));
    case dwarf::DW_TAG_member:
      return getDITypeName(getBaseDIType(ty));
    case dwarf::DW_TAG_array_type:
    {
      if (DIType *arrTy = dyn_cast<DICompositeType>(ty)->getBaseType())
      {
        auto arrName = arrTy->getName().str();
        if (arrTy->getSizeInBits() != 0)
          return arrName + "[" + std::to_string(ty->getSizeInBits() / arrTy->getSizeInBits()) + "]";
        else
          return arrName + "[]";
      }
    }
    case dwarf::DW_TAG_pointer_type:
    {
      std::string s = getDITypeName(dyn_cast<DIDerivedType>(ty)->getBaseType());
      return s + "*";
    }
    case dwarf::DW_TAG_subroutine_type:
      return getFuncSigName(ty);
    case dwarf::DW_TAG_union_type:
      return "union " + ty->getName().str();
    case dwarf::DW_TAG_structure_type:
      return "struct " + ty->getName().str();
    case dwarf::DW_TAG_const_type:
      return "const " + getDITypeName(dyn_cast<DIDerivedType>(ty)->getBaseType());
    case dwarf::DW_TAG_enumeration_type:
      return "enum " + ty->getName().str();
    default:
    {
      if (!ty->getName().str().empty())
        return ty->getName().str();
      return "unknow";
    }
    }
  }
  catch (std::exception &e)
  {
    errs() << e.what();
    exit(0);
  }
}

std::string pdg::DIUtils::getArgTypeName(Argument &arg)
{
  return getDITypeName(getArgDIType(arg));
}

std::string pdg::DIUtils::insertStructDefinition(
    DIType *ty, std::map<std::string, std::string> &userDefinedTypes) {
  DICompositeType *diComp = dyn_cast<DICompositeType>(getLowestDIType(ty));
  if (diComp->getFile()->getFilename().str().find("/usr/include") == 0)
    return "";
  std::string res = "\tstruct ";
  std::string structName = diComp->getName().str();
  if (structName == "") {
    DIType *baseTy = dyn_cast<DIDerivedType>(ty)->getBaseType();
    structName = baseTy->getName().str();
  }
  res += structName + " {\n";
  for (auto el : diComp->getElements()) {
    DIType *elType = dyn_cast<DIType>(el);
    res +=
        "\t\t" + getDITypeName(elType) + " " + elType->getName().str() + ";\n";
  }
  res += "\t};\n";
  if (userDefinedTypes.find(structName) ==
      userDefinedTypes.end()) {  // Insert to map if not found
    userDefinedTypes.insert({structName, res});
  }
  return res;
}

std::string pdg::DIUtils::insertEnumDefinition(
    DIType *ty, std::map<std::string, std::string> &userDefinedTypes) {
  DICompositeType *diComp = dyn_cast<DICompositeType>(getLowestDIType(ty));
  if (diComp->getFile()->getFilename().str().find("/usr/include") == 0)
    return "";
  std::string res = "\tenum ";
  res += diComp->getName().str() + " { ";
  for (auto el : diComp->getElements()) {
    DIEnumerator *enumEl = dyn_cast<DIEnumerator>(el);
    res += enumEl->getName().str() + ", ";
  }
  res = res.substr(0, res.length() - 2);
  res += "\t};\n";
  if (userDefinedTypes.find(diComp->getName().str()) ==
      userDefinedTypes.end()) {  // Insert to map if not found
    userDefinedTypes.insert({diComp->getName().str(), res});
  }
  return res;
}

std::string pdg::DIUtils::insertUnionDefinition(
    DIType *ty, std::map<std::string, std::string> &userDefinedTypes) {
  DICompositeType *diComp = dyn_cast<DICompositeType>(getLowestDIType(ty));
  if (diComp->getFile()->getFilename().str().find("/usr/include") == 0)
    return "";
  std::string res = "\tunion ";
  std::string unionName = diComp->getName().str();
  if (unionName == "") {
    DIType *baseTy = dyn_cast<DIDerivedType>(ty)->getBaseType();
    unionName = baseTy->getName().str();
  }
  if (unionName == "") {  // For typedefed non-pointer unions name is in the ty
    unionName = ty->getName().str();
  }
  res += unionName + " {\n";
  for (auto el : diComp->getElements()) {
    DIType *elType = dyn_cast<DIType>(el);
    res +=
        "\t\t" + getDITypeName(elType) + " " + elType->getName().str() + ";\n";
  }
  res += "\t};\n";
  if (userDefinedTypes.find(unionName) ==
      userDefinedTypes.end()) {  // Insert to map if not found
    userDefinedTypes.insert({unionName, res});
  }
  return res;
}

void pdg::DIUtils::printStructFieldNames(DINodeArray DINodeArr)
{
  for (auto DINode : DINodeArr)
    errs() << dyn_cast<DIType>(DINode)->getName() << "\n";
}

bool pdg::DIUtils::isPointerType(DIType *dt)
{
  if (dt == nullptr)
    return false;
  dt = stripMemberTag(dt);
  if (dt != nullptr)
    return (dt->getTag() == dwarf::DW_TAG_pointer_type);
  return false;
}

bool pdg::DIUtils::isStructPointerTy(DIType *dt) {
  if (dt == nullptr) return false;

  dt = stripMemberTag(dt);
  if (dt->getTag() == dwarf::DW_TAG_pointer_type) {
    auto baseTy = getLowestDIType(dt);
    if (baseTy != nullptr)
      return (baseTy->getTag() == dwarf::DW_TAG_structure_type);
  }
  return false;
}

bool pdg::DIUtils::isUnionPointerTy(DIType *dt) {
  if (dt == nullptr) return false;

  dt = stripMemberTag(dt);
  if (dt->getTag() == dwarf::DW_TAG_pointer_type) {
    auto baseTy = getLowestDIType(dt);
    if (baseTy != nullptr)
      return (baseTy->getTag() == dwarf::DW_TAG_union_type);
  }
  return false;
}

bool pdg::DIUtils::isStructTy(DIType *dt) {
  if (dt == nullptr) return false;
  if (dt->getTag() == dwarf::DW_TAG_pointer_type) return false;
  auto baseTy = getLowestDIType(dt);
  if (baseTy != nullptr)
    return (baseTy->getTag() == dwarf::DW_TAG_structure_type);
  return false;
}

bool pdg::DIUtils::isEnumTy(llvm::DIType *dt) {
  if (dt == nullptr) return false;
  if (dt->getTag() == dwarf::DW_TAG_pointer_type) return false;
  auto baseTy = getLowestDIType(dt);
  if (baseTy != nullptr)
    return (baseTy->getTag() == dwarf::DW_TAG_enumeration_type);
  return false;
}

bool pdg::DIUtils::isUnionTy(llvm::DIType *dt) {
  if (dt == nullptr) return false;
  if (dt->getTag() == dwarf::DW_TAG_pointer_type) return false;
  auto baseTy = getLowestDIType(dt);
  if (baseTy != nullptr)
    return (baseTy->getTag() == dwarf::DW_TAG_union_type);
  return false;
}

bool pdg::DIUtils::isFuncPointerTy(DIType *dt)
{
  if (dt == nullptr)
    return false;
  if (dt->getTag() == dwarf::DW_TAG_subroutine_type)
    return true;
  dt = stripMemberTag(dt);
  if (dt->getTag() == dwarf::DW_TAG_pointer_type) {
    auto baseTy = getBaseDIType(dt);
    if (baseTy != nullptr)
      return (getBaseDIType(dt)->getTag() == dwarf::DW_TAG_subroutine_type);
  }
  return false;
}

bool pdg::DIUtils::isTypeDefTy(llvm::DIType *ty) {
  if (ty->getTag() == dwarf::DW_TAG_pointer_type) {
    ty = getBaseDIType(ty);
    if (ty != nullptr && ty->getTag() == dwarf::DW_TAG_typedef) return true;
  } else if (ty != nullptr && ty->getTag() == dwarf::DW_TAG_typedef) {
    return true;
  }
  return false;
}

bool pdg::DIUtils::isTypeDefPtrTy(llvm::Argument &arg) {
  DIType * ty = getArgDIType(arg);
  if (ty->getTag()== dwarf::DW_TAG_typedef) {
    ty = getBaseDIType(ty);
    if (ty != nullptr && ty->getTag() == dwarf::DW_TAG_pointer_type)
      return true;
  }
  return isTypeDefConstPtrTy(arg);
}

bool pdg::DIUtils::isTypeDefConstPtrTy(llvm::Argument &arg) {
  DIType *ty = getArgDIType(arg);
  if (ty->getTag() == dwarf::DW_TAG_typedef) {
    ty = getBaseDIType(ty);
    if (ty != nullptr && ty->getTag() == dwarf::DW_TAG_pointer_type) {
      ty = getBaseDIType(ty);
      if (ty != nullptr && ty->getTag() == dwarf::DW_TAG_const_type) return true;
    }
  }
  return false;
}

bool pdg::DIUtils::isVoidPointerTy(llvm::Argument &arg) {
  DIType *ty = getArgDIType(arg);
  if (ty->getTag() == dwarf::DW_TAG_pointer_type) {
    if (getDITypeName(dyn_cast<DIDerivedType>(ty)->getBaseType()) == "void")
      return true;
  }
  return false;
}

bool pdg::DIUtils::isCharPointerTy(llvm::Argument &arg) {
  DIType *ty = getArgDIType(arg);
  if (ty->getTag() == dwarf::DW_TAG_pointer_type) {
    std::string typeName =
        getDITypeName(dyn_cast<DIDerivedType>(ty)->getBaseType());
    if (typeName == "char" || typeName == "const char") return true;
  }
  return false;
}

DIType *pdg::DIUtils::stripMemberTag(DIType *dt)
{
  if (dt == nullptr)
    return dt;
  if (dt->getTag() == dwarf::DW_TAG_member)
    return getBaseDIType(dt);
  return dt;
}

DIType *pdg::DIUtils::getFuncDIType(Function* func)
{
  auto subProgram = func->getSubprogram();
  return subProgram->getType();
}

std::vector<Function *> pdg::DIUtils::collectIndirectCallCandidatesWithDI(DIType *funcDIType, Module *module, std::map<std::string, std::string> funcptrTargetMap)
{
  std::vector<Function *> indirectCallList;
  for (auto &F : *module)
  {
    std::string funcName = F.getName().str();
    // get Function type
    if (funcName == "main" || !F.getSubprogram() || F.isDeclaration())
      continue;
    // compare the indirect call function type with each function, filter out certian functions that should not be considered as call targets
    if (funcptrTargetMap[DIUtils::getDIFieldName(funcDIType)] == F.getName())
    {
      indirectCallList.push_back(&F);
    }
  }

  return indirectCallList;
}