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

string llvm_type_to_string(const Type *type)
{
  string type_str;
  llvm::raw_string_ostream rso(type_str);
  type->print(rso);
  return rso.str();
}

static Function *define_for_function(Module *module, const string &function_name, list<GeneratorArgumentInfo> &target_arg_list)
{
  IRBuilder<> Builder(getGlobalContext());

  vector<Type*> call_arg_types;

  for (list<GeneratorArgumentInfo>::iterator args_iter = target_arg_list.begin();
       args_iter != target_arg_list.end();
       ++args_iter)
    {
      if(!args_iter->getIsAlias())
        call_arg_types.push_back(args_iter->getLLVMType());
    }

  /* Add the count parameter */
  call_arg_types.push_back(Builder.getInt32Ty());

  FunctionType *func_type = FunctionType::get(Builder.getVoidTy(), call_arg_types, false);

  Function *func = Function::Create(func_type, Function::ExternalLinkage, function_name + ".iteration", module);

  return func;
}

llvm::Function *llvm_void_def_for(Module *module,
                                  const string &function_name,
                                  list<GeneratorArgumentInfo> &target_arg_list)
{
  IRBuilder<> Builder(getGlobalContext());

  Function *func = define_for_function(module, function_name, target_arg_list);

  BasicBlock *func_body_block = BasicBlock::Create(getGlobalContext(), "entry", func);
  Builder.SetInsertPoint(func_body_block);
  Builder.CreateRetVoid();

  return func;
}

static void validate_arguments(Function *target_func, list<GeneratorArgumentInfo> &target_arg_list)
{
  if (target_arg_list.begin()->getAggregation() != GeneratorArgumentInfo::ARG_AGG_ARRAY)
    throw GeneratorException("Return type must be an array");

  /* validate that the arguments match the target function */
  {
    const Function::ArgumentListType &target_func_args = target_func->getArgumentList();

    list<GeneratorArgumentInfo>::iterator args_iter = target_arg_list.begin();
    Type *return_type = target_arg_list.begin()->getLLVMBaseType();

    if (target_func->getReturnType() != return_type)
    {
      throw GeneratorException("Function \"" + target_func->getName().str() +
                               "\" returns \"" + llvm_type_to_string(target_func->getReturnType()) +
                               "\" but iteration requested \"" + llvm_type_to_string(return_type) + "\"");
    }

    if (target_arg_list.begin()->getIsAlias())
      {
        int alias_value = target_arg_list.begin()->getAlias();
        if ((alias_value < 1) || (alias_value >= target_arg_list.size()))
          throw GeneratorException("Alias out of range");
      }

    if (target_func_args.size() != target_arg_list.size() - 1)
    {
      string error_str;
      llvm::raw_string_ostream rso(error_str);
      rso << "Function \"" << target_func->getName().str() << "\" takes "
          << target_func_args.size() << " arguments but iteration gave "
          << (target_arg_list.size() - 1);
      
      throw GeneratorException(rso.str());
    }

    Function::ArgumentListType::const_iterator target_func_args_iter = target_func_args.begin();
    args_iter++;

    while (args_iter != target_arg_list.end())
    {
      const Type *in_type = args_iter->getLLVMBaseType();
      const Type *out_type = target_func_args_iter->getType();
      if (in_type != out_type)
        {
          throw GeneratorException("Function \"" + target_func->getName().str() +
                                   "\" takes \"" + llvm_type_to_string(target_func_args_iter->getType()) +
                                   "\" but iteration requested \"" + llvm_type_to_string(args_iter->getLLVMBaseType()) + "\"");
        }
      if (args_iter->getIsAlias())
        {
          throw GeneratorException("Alias requested for argument");
        }
      
      args_iter++;
      target_func_args_iter++;
    }
  }
}

/* Move the arguments of *func into alloca variables */
static vector<Value*> load_arguments_to_variables(IRBuilder<> &builder, Function *func)
{
  vector<Value*> loop_variables(func->getArgumentList().size());

  vector<Value*>::iterator loop_variables_iter = loop_variables.begin();
  Function::ArgumentListType::iterator func_args_iter = func->getArgumentList().begin();

  while (func_args_iter != func->getArgumentList().end())
    {
      AllocaInst *alloca = builder.CreateAlloca(func_args_iter->getType());
      alloca->setAlignment(16);
      *loop_variables_iter = alloca;

      builder.CreateAlignedStore(func_args_iter, *loop_variables_iter, 16);

      ++loop_variables_iter;
      ++func_args_iter;
    }

  return loop_variables;
}

static void load_referance_arguments(IRBuilder<> &builder,
                                     vector<Value*> &loop_variables,
                                     const vector<GeneratorArgumentInfo> &target_arg_list)
{
  /* If an arg is a pointer, load it's value to the stack */
  vector<GeneratorArgumentInfo>::const_iterator args_iter = target_arg_list.begin();
  vector<Value*>::iterator loop_variables_iter = loop_variables.begin();

  while (args_iter != target_arg_list.end())
    {
      if (args_iter->getAggregation() == GeneratorArgumentInfo::ARG_AGG_REF)
        {
          /* Load the pointer off of the stack */
          Value *ptr_value = builder.CreateLoad(*loop_variables_iter);
          SequentialType *ptr_type = dyn_cast<SequentialType>(args_iter->getLLVMType());
          Type *base_type = ptr_type->getElementType();
          *loop_variables_iter = builder.CreateAlloca(base_type);

          /* Load the real value */
          Value *value = builder.CreateLoad(ptr_value); /* FIXME: Check alignment */
          builder.CreateAlignedStore(value, *loop_variables_iter, 16);
        }

      ++args_iter;
      ++loop_variables_iter;
    }
}

static void increment_array_variables(IRBuilder<> &builder,
                                      vector<Value*> &loop_variables,
                                      const vector<GeneratorArgumentInfo> &target_arg_list)
{
  vector<GeneratorArgumentInfo>::const_iterator args_iter = target_arg_list.begin();
  vector<Value*>::iterator loop_variables_iter = loop_variables.begin();

  while(args_iter != target_arg_list.end())
    {
      /* If an arg is an array, increment it */
      if (args_iter->getAggregation() == GeneratorArgumentInfo::ARG_AGG_ARRAY)
        {
          Value *load_var = builder.CreateLoad(*loop_variables_iter);
          Value *gep_var  = builder.CreateGEP(load_var, builder.getInt32(1));
          builder.CreateStore(gep_var, *loop_variables_iter);
        }
      ++loop_variables_iter;
      ++args_iter;
    }

}

/* Load values indicated by target_arg_list out of
   loop_variables and into a vector sutable for a
   function call.
 */
static vector<Value*> pack_call_parameters(IRBuilder<> &builder,
                                           vector<Value*> &loop_variables,
                                           const vector<GeneratorArgumentInfo> &target_arg_list)
{
  vector<Value*> call_parameters;
  vector<GeneratorArgumentInfo>::const_iterator args_iter = target_arg_list.begin();
  vector<Value*>::iterator loop_variables_iter = loop_variables.begin();

  while(args_iter != target_arg_list.end())
    {
      Value *call_parameter = *loop_variables_iter;

      if (args_iter->getAggregation() == GeneratorArgumentInfo::ARG_AGG_ARRAY)
        {
          /* If our argument is an array deref to the current value */
          call_parameter = builder.CreateLoad(call_parameter);

          if (args_iter->getAligned())
            call_parameter = builder.CreateAlignedLoad(call_parameter, 16);
          else
            call_parameter = builder.CreateAlignedLoad(call_parameter, 1);
        }
      else
        {
          /* If the argument is not an array it must be on our stack and is therefor aligned */
          call_parameter = builder.CreateAlignedLoad(call_parameter, 16);
        }

      call_parameters.push_back(call_parameter);

      ++loop_variables_iter;
      ++args_iter;
    }

  return call_parameters;
}

llvm::Function *llvm_def_for(Module *module,
                             const string &function_name,
                             list<GeneratorArgumentInfo> &target_arg_list)
{
  /* find our target function */
  Function *target_func = module->getFunction(function_name);
  if (!target_func)
  {
    throw GeneratorException("Module has no function \"" + function_name + "\"");
  }

  validate_arguments(target_func, target_arg_list);

  /* generate wrapper function */
  IRBuilder<> builder(getGlobalContext());

  Function *func = define_for_function(module, function_name, target_arg_list);

  bool alias_return_value = target_arg_list.begin()->getIsAlias();

  /* Build iteration */
  BasicBlock *func_body_block = BasicBlock::Create(getGlobalContext(), "entry", func);
  builder.SetInsertPoint(func_body_block);

  vector<Value*> loop_variables = load_arguments_to_variables(builder, func);

  vector<GeneratorArgumentInfo> active_arg_list;

  if (!alias_return_value)
    active_arg_list = vector<GeneratorArgumentInfo>(target_arg_list.begin(), target_arg_list.end());
  else
    active_arg_list = vector<GeneratorArgumentInfo>(++target_arg_list.begin(), target_arg_list.end());

  load_referance_arguments(builder, loop_variables, active_arg_list);

  Value *return_location = NULL;
  if (!alias_return_value)
    return_location = loop_variables[0];
  else
    return_location = loop_variables[target_arg_list.begin()->getAlias() - 1];

  Value *remaining_count_variable = *(loop_variables.end() - 1);

  /* Begin loop */
  BasicBlock *BodyBB = BasicBlock::Create(getGlobalContext(), "loop_body", func);
  BasicBlock *LoopCondBB = BasicBlock::Create(getGlobalContext(), "loop_cond", func);
  BasicBlock *ExitBB = BasicBlock::Create(getGlobalContext(), "exit", func);
  builder.CreateBr(LoopCondBB);

  builder.SetInsertPoint(BodyBB);
  {
    /* Call our target */
    vector<Value *> call_values;
    vector<GeneratorArgumentInfo> call_arginfo;

    if (!alias_return_value)
      {
        call_values = vector<Value *>(loop_variables.begin() + 1, loop_variables.end() - 1);
        call_arginfo = vector<GeneratorArgumentInfo>(active_arg_list.begin() + 1, active_arg_list.end());
      }
    else
      {
        call_values = vector<Value *>(loop_variables.begin(), loop_variables.end() - 1);
        call_arginfo = vector<GeneratorArgumentInfo>(active_arg_list.begin(), active_arg_list.end());
      }


    vector<Value*> call_parameters = pack_call_parameters(builder,
                                                          call_values,
                                                          call_arginfo);

    Value *call_result = builder.CreateCall(target_func, call_parameters);

    if (target_arg_list.begin()->getAligned())
      builder.CreateAlignedStore(call_result, builder.CreateLoad(return_location), 16);
    else
      builder.CreateAlignedStore(call_result, builder.CreateLoad(return_location), 1);

    increment_array_variables(builder, loop_variables, active_arg_list);

    builder.CreateBr(LoopCondBB);
  }

  builder.SetInsertPoint(LoopCondBB);
  {
    Value *val = builder.CreateLoad(remaining_count_variable);
    Value *comparison = builder.CreateICmpUGT(val, builder.getInt32(0));
    val = builder.CreateNUWSub(val, builder.getInt32(1));
    builder.CreateStore(val, remaining_count_variable);
    builder.CreateCondBr(comparison, BodyBB, ExitBB);
  }

  /* End loop */
  builder.SetInsertPoint(ExitBB);
  builder.CreateRetVoid();

  return func;
}
