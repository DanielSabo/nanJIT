#ifndef __VARG_HPP__
#define __VARG_HPP__

#include "typeinfo.hpp"

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
  std::string name;
public:

  GeneratorArgumentInfo();
  GeneratorArgumentInfo(std::string str);
  void setAligned(bool is_aligned);
  void setAggregation(ArgAggEnum agg);
  void parse(std::string str);
  std::string toStr();

  bool getAligned();
  ArgAggEnum getAggregation();
  nanjit::TypeInfo getType();
  llvm::Type *getLLVMBaseType();
  llvm::Type *getLLVMType();
};

llvm::Function *llvm_def_for(Module *module, const std::string &function_name, std::list<GeneratorArgumentInfo> &target_arg_list);
llvm::Function *llvm_void_def_for(Module *module, const std::string &function_name, std::list<GeneratorArgumentInfo> &target_arg_list);

#endif /* __VARG_HPP__ */
