#include "llvm/Config/config.h"
#if ((LLVM_VERSION_MAJOR > 3) || ((LLVM_VERSION_MAJOR == 3) && (LLVM_VERSION_MINOR >= 3)))
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#else
#include "llvm/DataLayout.h"
#include "llvm/DerivedTypes.h"
#include "llvm/IRBuilder.h"
#include "llvm/LLVMContext.h"
#include "llvm/Module.h"
#endif

#include "typeinfo.hpp"

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>

using namespace std;
using namespace llvm;
using namespace nanjit;

typedef struct {
  const char *name;
  TypeInfo::BaseTypeEnum id;
} TypeMapping;

static const TypeMapping BASE_TYPE_MAP[] = {
  {"float", TypeInfo::TYPE_FLOAT},
  {"int", TypeInfo::TYPE_INT},
  {"uint", TypeInfo::TYPE_UINT},
  {"short", TypeInfo::TYPE_SHORT},
  {"ushort", TypeInfo::TYPE_USHORT},
  {"char", TypeInfo::TYPE_CHAR},
  {"uchar", TypeInfo::TYPE_UCHAR},
  {"bool", TypeInfo::TYPE_BOOL},
  {NULL, TypeInfo::TYPE_VOID},
};

TypeInfo::TypeInfo(std::string typestr)
{
  base_type = TYPE_VOID;
  width = 1;

  int index = 0;
  int offset = 0;

  std::string namestr;

  while (offset < typestr.size() && isalpha(typestr[offset]))
    namestr.push_back(typestr[offset++]);

  while (BASE_TYPE_MAP[index].name && namestr != BASE_TYPE_MAP[index].name)
    index++;
  base_type = BASE_TYPE_MAP[index].id;

  std::string widthstr;

  while (offset < typestr.size() && isdigit(typestr[offset]))
    widthstr.push_back(typestr[offset++]);

  if (widthstr.empty())
    width = 1;
  else
    width = atoi(widthstr.c_str());

  while (offset < typestr.size() && isspace(typestr[offset]))
    offset++;
}

TypeInfo::TypeInfo(BaseTypeEnum bt, unsigned int w)
  : base_type(bt), width(w)
{
}

TypeInfo::BaseTypeEnum TypeInfo::getBaseType() const
{
  return base_type;
}

unsigned int TypeInfo::getWidth() const
{
  return width;
}

llvm::Type *TypeInfo::getLLVMType() const
{
  Type *llvm_type = NULL;

  switch (base_type)
    {
      case TypeInfo::TYPE_FLOAT:
        llvm_type = Type::getFloatTy(getGlobalContext());
        break;
      case TypeInfo::TYPE_INT:
      case TypeInfo::TYPE_UINT:
        llvm_type = Type::getInt32Ty(getGlobalContext());
        break;
      case TypeInfo::TYPE_SHORT:
      case TypeInfo::TYPE_USHORT:
        llvm_type = Type::getInt16Ty(getGlobalContext());
        break;
      case TypeInfo::TYPE_CHAR:
      case TypeInfo::TYPE_UCHAR:
        llvm_type = Type::getInt8Ty(getGlobalContext());
        break;
      case TypeInfo::TYPE_BOOL:
        llvm_type = Type::getInt1Ty(getGlobalContext());
        break;
      case TypeInfo::TYPE_VOID:
        llvm_type = Type::getVoidTy(getGlobalContext());
        break;
    }

  if (width > 1 && base_type != TypeInfo::TYPE_VOID)
    llvm_type = VectorType::get(llvm_type, width);

  return llvm_type;
}

std::string TypeInfo::toStr() const
{
  if (base_type == TypeInfo::TYPE_VOID)
    return std::string("void");

  std::stringstream result;

  if (base_type == TypeInfo::TYPE_FLOAT)
    result << "float";
  else if (base_type == TypeInfo::TYPE_INT)
    result << "int";
  else if (base_type == TypeInfo::TYPE_UINT)
    result << "uint";
  else if (base_type == TypeInfo::TYPE_SHORT)
    result << "short";
  else if (base_type == TypeInfo::TYPE_USHORT)
    result << "ushort";
  else if (base_type == TypeInfo::TYPE_CHAR)
    result << "char";
  else if (base_type == TypeInfo::TYPE_UCHAR)
    result << "uchar";
  else if (base_type == TypeInfo::TYPE_BOOL)
    result << "bool";

  if (width != 1)
    result << width;

  return result.str();
}

bool TypeInfo::isFloatType() const
{
  if (base_type == TypeInfo::TYPE_FLOAT)
    return true;
  return false;
}

bool TypeInfo::isIntegerType() const
{
  switch (base_type)
    {
      case TypeInfo::TYPE_FLOAT:
      case TypeInfo::TYPE_VOID:
        return false;
      case TypeInfo::TYPE_INT:
      case TypeInfo::TYPE_UINT:
      case TypeInfo::TYPE_SHORT:
      case TypeInfo::TYPE_USHORT:
      case TypeInfo::TYPE_CHAR:
      case TypeInfo::TYPE_UCHAR:
      case TypeInfo::TYPE_BOOL:
        return true;
    }
}

bool TypeInfo::isUnsignedType() const
{
  switch (base_type)
    {
      case TypeInfo::TYPE_FLOAT:
      case TypeInfo::TYPE_VOID:
        return false;
      case TypeInfo::TYPE_INT:
      case TypeInfo::TYPE_SHORT:
      case TypeInfo::TYPE_CHAR:
        return false;
      case TypeInfo::TYPE_UINT:
      case TypeInfo::TYPE_USHORT:
      case TypeInfo::TYPE_UCHAR:
      case TypeInfo::TYPE_BOOL:
        return true;
    }
}

bool TypeInfo::operator == (const TypeInfo &rhs) const
{
  if (base_type == rhs.base_type &&
      width == rhs.width)
    return true;
  return false;
}

bool TypeInfo::operator != (const TypeInfo &rhs) const
{
  return !(*this == rhs);
}
