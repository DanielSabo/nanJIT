#ifndef __VARG_HPP__
#define __VARG_HPP__

#include "typeinfo.h"

namespace llvm
{
  class Function;
};

class GeneratorException : public std::exception
{
  std::string error_string;
public:
  GeneratorException (const char *e) : error_string(e) {}
  GeneratorException (std::string e) : error_string(e) {}
  ~GeneratorException() throw() {}
  const char* what() const throw() { return error_string.c_str(); }
};


class GeneratorArgumentInfo
{
public:
  typedef enum {
    ARG_AGG_SINGLE,
    ARG_AGG_REF,
    ARG_AGG_ARRAY
  } ArgAggEnum;

private:
  ArgAggEnum aggregation;
  nanjit::TypeInfo type;
  bool aligned;
  int alias_index;
public:

  GeneratorArgumentInfo();
  GeneratorArgumentInfo(std::string str);
  void setAligned(bool is_aligned);
  void setAggregation(ArgAggEnum agg);
  void setAlias(int a);
  void parse(std::string str);
  std::string toStr() const;

  bool getAligned() const;
  ArgAggEnum getAggregation() const;
  bool getIsAlias() const;
  int getAlias() const;
  nanjit::TypeInfo getType() const;
  llvm::Type *getLLVMBaseType() const;
  llvm::Type *getLLVMType() const;
};

llvm::Function *llvm_def_for(Module *module, const std::string &function_name, std::list<GeneratorArgumentInfo> &target_arg_list);
llvm::Function *llvm_void_def_for(Module *module, const std::string &function_name, std::list<GeneratorArgumentInfo> &target_arg_list);

#endif /* __VARG_HPP__ */
