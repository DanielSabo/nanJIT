#include "llvm/Analysis/Passes.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/CodeGen/MachineCodeInfo.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/JIT.h"
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
#include "llvm/PassManager.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Transforms/Scalar.h"


#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/raw_os_ostream.h>
using namespace llvm;

#include <list>
#include <string>
#include <memory>
#include <iostream>
#include <sstream>
using namespace std;

#include <stdarg.h>
#include <stdio.h>

#include "varg.h"
using namespace nanjit;

GeneratorArgumentInfo::GeneratorArgumentInfo()
{
  aggregation = ARG_AGG_SINGLE;
  type = TypeInfo(TypeInfo::TYPE_VOID);
  aligned = false;
  alias_index = -1;
}

GeneratorArgumentInfo::GeneratorArgumentInfo(string str)
{
  aggregation = ARG_AGG_SINGLE;
  type = TypeInfo(TypeInfo::TYPE_VOID);
  aligned = false;
  alias_index = -1;

  parse(str);
}

void GeneratorArgumentInfo::setAligned(bool is_aligned)
{
  aligned = is_aligned;
}


bool GeneratorArgumentInfo::getAligned() const
{
  return aligned;
}

void GeneratorArgumentInfo::setAlias(int a)
{
  alias_index = a;
}

int GeneratorArgumentInfo::getAlias() const
{
  return alias_index;
}

bool GeneratorArgumentInfo::getIsAlias() const
{
  return alias_index != -1;
}

void GeneratorArgumentInfo::setAggregation(ArgAggEnum agg)
{
  aggregation = agg;
}

GeneratorArgumentInfo::ArgAggEnum GeneratorArgumentInfo::getAggregation() const
{
  return aggregation;
}

nanjit::TypeInfo GeneratorArgumentInfo::getType() const
{
  return type;
}

string GeneratorArgumentInfo::toStr() const
{
  stringstream result;

  if(aggregation == GeneratorArgumentInfo::ARG_AGG_SINGLE)
    result << "single(";
  else if(aggregation == GeneratorArgumentInfo::ARG_AGG_REF)
    result << "pointer(";
  else if(aggregation == GeneratorArgumentInfo::ARG_AGG_ARRAY)
    result << "array(";

  if (aligned)
    result << "aligned ";

  if (alias_index !=  -1)
    result << "alias(" << alias_index << ") ";

  if (type.getBaseType() == TypeInfo::TYPE_FLOAT)
    result << "float";
  else if (type.getBaseType() == TypeInfo::TYPE_INT)
    result << "int";
  else if (type.getBaseType() == TypeInfo::TYPE_UINT)
    result << "uint";
  else if (type.getBaseType() == TypeInfo::TYPE_SHORT)
    result << "short";
  else if (type.getBaseType() == TypeInfo::TYPE_USHORT)
    result << "ushort";

  if (type.getWidth() > 1)
    result << type.getWidth();
  
  result << ")";

  return result.str();
}

llvm::Type *GeneratorArgumentInfo::getLLVMBaseType() const
{
  return type.getLLVMType();
}

llvm::Type *GeneratorArgumentInfo::getLLVMType() const
{
  llvm::Type *our_type = getLLVMBaseType();

  if(aggregation == GeneratorArgumentInfo::ARG_AGG_SINGLE)
    ;
  else if(aggregation == GeneratorArgumentInfo::ARG_AGG_REF)
    our_type = PointerType::getUnqual(our_type);
  else if(aggregation == GeneratorArgumentInfo::ARG_AGG_ARRAY)
    our_type = PointerType::getUnqual(our_type);

  return our_type;
}

void GeneratorArgumentInfo::parse(string in_str)
{
  aggregation = ARG_AGG_SINGLE;
  type = TypeInfo(TypeInfo::TYPE_VOID);
  aligned = false;
  alias_index = -1;

  int argument_aggregation = GeneratorArgumentInfo::ARG_AGG_SINGLE;
  int attribute_offset = 0;
  int offset = 0;

  bool reading_attributes = true;

  while (reading_attributes)
    {
      string maybe_attribute;

      while (attribute_offset < in_str.size() && !isspace(in_str[attribute_offset]))
        maybe_attribute.push_back(in_str[attribute_offset++]);

      while (attribute_offset < in_str.size() && isspace(in_str[attribute_offset]))
        attribute_offset++;

      int alias_value;

      if (maybe_attribute == "aligned")
        setAligned(true);
      else if (1 == sscanf(maybe_attribute.c_str(), "alias(%d)", &alias_value))
        setAlias(alias_value);
      else
        reading_attributes = false;

      if (reading_attributes)
        offset += attribute_offset; /* Consume this word */
    }

  if(in_str[offset] == '*')
    {
      argument_aggregation = GeneratorArgumentInfo::ARG_AGG_REF;
      setAggregation(GeneratorArgumentInfo::ARG_AGG_REF);
      offset += 1;
    }

  string typestr;

  while (offset < in_str.size() && isspace(in_str[offset]))
    offset++;

  while (offset < in_str.size() && isalpha(in_str[offset]))
    typestr.push_back(in_str[offset++]);

  while (offset < in_str.size() && isdigit(in_str[offset]))
    typestr.push_back(in_str[offset++]);

  type = TypeInfo(typestr);

  /* If there's anything left this might be an array */
  if (offset < in_str.size())
    {
      if (argument_aggregation == GeneratorArgumentInfo::ARG_AGG_REF)
        {
          throw GeneratorException("Couldn't parse argument string \"" + in_str + "\"");
        }

      if (0 == in_str.compare(offset, 2, "[]"))
        {
          argument_aggregation = GeneratorArgumentInfo::ARG_AGG_ARRAY;
          setAggregation(GeneratorArgumentInfo::ARG_AGG_ARRAY);
        }
      else
        {
          throw GeneratorException("Couldn't parse argument string \"" + in_str + "\"");
        }
    }
}