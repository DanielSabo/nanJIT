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

static void validate_arguments(Function *target_func,
                               list<GeneratorArgumentInfo> &target_arg_list,
                               list<string> &magic_arguments)
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

    /* FIXME: Check the type of magic values */
    unsigned int num_args = 0;

    for (Function::ArgumentListType::iterator func_args_iter = target_func->getArgumentList().begin();
         func_args_iter != target_func->getArgumentList().end();
         ++func_args_iter)
      {
        string arg_name = func_args_iter->getName();

        if (0 == count(magic_arguments.begin(), magic_arguments.end(), arg_name))
          num_args++;
      }

    if (num_args != target_arg_list.size() - 1)
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
          Value *value = NULL;
          if (args_iter->getAligned())
            value = builder.CreateAlignedLoad(ptr_value, 16);
          else
            value = builder.CreateAlignedLoad(ptr_value, 1);
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

typedef struct
{
  GeneratorArgumentInfo arg_info;
  Value *value;
} LocalVariablePair;

/* Build the arguments vector (part 1):
 * For each argument the function requires:
 *   If it's name is in magic_arguments, use matching pair from magic_arguments
 *   Otherwise take the next value from (loop_variables, target_arg_list)
 */
static vector<LocalVariablePair> inject_magic_arguments(Function *target_func,
                                                        const vector<Value*> &loop_variables,
                                                        const vector<GeneratorArgumentInfo> &target_arg_list,
                                                        map<string, LocalVariablePair> &magic_arguments)
{
  Function::ArgumentListType::iterator func_args_iter = target_func->getArgumentList().begin();

  vector<Value*>::const_iterator loop_variables_iter = loop_variables.begin();
  vector<GeneratorArgumentInfo>::const_iterator target_arg_iter = target_arg_list.begin();

  vector<LocalVariablePair> result;

  for (Function::ArgumentListType::iterator func_args_iter = target_func->getArgumentList().begin();
       func_args_iter != target_func->getArgumentList().end();
       ++func_args_iter)
    {
      string arg_name = func_args_iter->getName();

      map<string, LocalVariablePair>::iterator found_magic = magic_arguments.find(arg_name);

      if (found_magic != magic_arguments.end())
        {
          result.push_back(found_magic->second);
        }
      else
        {
          LocalVariablePair pair;
          pair.arg_info = *target_arg_iter;
          pair.value = *loop_variables_iter;
          result.push_back(pair);

          ++target_arg_iter;
          ++loop_variables_iter;
        }
    }

  return result;
}

/* Load values indicated by target_arg_list out of
   loop_variables and into a vector sutable for a
   function call.
 */
static vector<Value*> pack_call_parameters(IRBuilder<> &builder,
                                           vector<LocalVariablePair> &argument_pairs)
{
  vector<Value*> call_parameters;
  vector<LocalVariablePair>::const_iterator args_iter = argument_pairs.begin();

  while(args_iter != argument_pairs.end())
    {
      Value *call_parameter = args_iter->value;

      if (args_iter->arg_info.getAggregation() == GeneratorArgumentInfo::ARG_AGG_ARRAY)
        {
          /* If our argument is an array deref to the current value */
          call_parameter = builder.CreateLoad(call_parameter);

          if (args_iter->arg_info.getAligned())
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

  list<string> magic_arguments;
  validate_arguments(target_func, target_arg_list, magic_arguments);

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

    map<string, LocalVariablePair> magic_arguments_map;
    vector<LocalVariablePair> call_pairs = inject_magic_arguments(target_func, call_values, call_arginfo, magic_arguments_map);
    vector<Value*> call_parameters = pack_call_parameters(builder, call_pairs);

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

static Function *define_for_range_function(Module *module, const string &function_name, list<GeneratorArgumentInfo> &target_arg_list)
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

  /* Add the x.from and x.to parameters */
  call_arg_types.push_back(Builder.getInt32Ty());
  call_arg_types.push_back(Builder.getInt32Ty());

  FunctionType *func_type = FunctionType::get(Builder.getVoidTy(), call_arg_types, false);

  Function *func = Function::Create(func_type, Function::ExternalLinkage, function_name + ".iteration.range1D", module);

  return func;
}

llvm::Function *llvm_void_def_for_range(Module *module,
                                        const string &function_name,
                                        list<GeneratorArgumentInfo> &target_arg_list)
{
  IRBuilder<> Builder(getGlobalContext());

  Function *func = define_for_range_function(module, function_name, target_arg_list);

  BasicBlock *func_body_block = BasicBlock::Create(getGlobalContext(), "entry", func);
  Builder.SetInsertPoint(func_body_block);
  Builder.CreateRetVoid();

  return func;
}


llvm::Function *llvm_def_for_range(Module *module,
                                   const string &function_name,
                                   list<GeneratorArgumentInfo> &target_arg_list)
{

  /* find our target function */
  Function *target_func = module->getFunction(function_name);
  if (!target_func)
  {
    throw GeneratorException("Module has no function \"" + function_name + "\"");
  }

  list<string> magic_arguments;
  magic_arguments.push_back("__x");

  validate_arguments(target_func, target_arg_list, magic_arguments);

  IRBuilder<> builder(getGlobalContext());

  Function *func = define_for_range_function(module, function_name, target_arg_list);

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

  /* FIXME: Align alloca */
  Value *x_start = *(loop_variables.end() - 2);
  Value *x_end   = *(loop_variables.end() - 1);
  Value *x_variable = builder.CreateAlloca(builder.getInt32Ty());
  builder.CreateStore(builder.CreateLoad(x_start), x_variable);

  /* Begin loop */
  BasicBlock *loop_body_block = BasicBlock::Create(getGlobalContext(), "loop_body", func);
  BasicBlock *loop_cond_block = BasicBlock::Create(getGlobalContext(), "loop_cond", func);
  BasicBlock *exit_block = BasicBlock::Create(getGlobalContext(), "exit", func);
  builder.CreateBr(loop_cond_block);

  builder.SetInsertPoint(loop_body_block);
  {
    /* Call our target */
    vector<Value *> call_values;
    vector<GeneratorArgumentInfo> call_arginfo;

    if (!alias_return_value)
      {
        call_values = vector<Value *>(loop_variables.begin() + 1, loop_variables.end() - 2);
        call_arginfo = vector<GeneratorArgumentInfo>(active_arg_list.begin() + 1, active_arg_list.end());
      }
    else
      {
        call_values = vector<Value *>(loop_variables.begin(), loop_variables.end() - 2);
        call_arginfo = vector<GeneratorArgumentInfo>(active_arg_list.begin(), active_arg_list.end());
      }

    map<string, LocalVariablePair> magic_arguments_map;
    magic_arguments_map["__x"] = (LocalVariablePair){GeneratorArgumentInfo("uint"), x_variable};

    vector<LocalVariablePair> call_pairs = inject_magic_arguments(target_func, call_values, call_arginfo, magic_arguments_map);
    vector<Value*> call_parameters = pack_call_parameters(builder, call_pairs);

    Value *call_result = builder.CreateCall(target_func, call_parameters);

    if (target_arg_list.begin()->getAligned())
      builder.CreateAlignedStore(call_result, builder.CreateLoad(return_location), 16);
    else
      builder.CreateAlignedStore(call_result, builder.CreateLoad(return_location), 1);

    increment_array_variables(builder, loop_variables, active_arg_list);

    /* Increment the index value */
    {
      Value *val = builder.CreateLoad(x_variable);
      val = builder.CreateAdd(val, builder.getInt32(1));
      builder.CreateStore(val, x_variable);
    }

    builder.CreateBr(loop_cond_block);
  }

  builder.SetInsertPoint(loop_cond_block);
  {
    Value *val = builder.CreateLoad(x_variable);
    Value *max_val = builder.CreateLoad(x_end);
    Value *comparison = builder.CreateICmpSLT(val, max_val);
    builder.CreateStore(val, x_variable);
    builder.CreateCondBr(comparison, loop_body_block, exit_block);
  }

  /* End loop */
  builder.SetInsertPoint(exit_block);
  builder.CreateRetVoid();

  return func;
}