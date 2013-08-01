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

#include <stdarg.h>
#include <stdio.h>

#include "varg.hpp"
using namespace nanjit;

GeneratorArgumentInfo::GeneratorArgumentInfo()
{
  aggregation = ARG_AGG_SINGLE;
  type = TypeInfo(TypeInfo::TYPE_FLOAT);
  aligned = false;
}

GeneratorArgumentInfo::GeneratorArgumentInfo(std::string str)
{
  aggregation = ARG_AGG_SINGLE;
  type = TypeInfo(TypeInfo::TYPE_FLOAT);
  aligned = false;

  parse(str);
}

void GeneratorArgumentInfo::setAligned(bool is_aligned)
{
  aligned = is_aligned;
}

bool GeneratorArgumentInfo::getAligned()
{
  return aligned;
}

void GeneratorArgumentInfo::setAggregation(ArgAggEnum agg)
{
  aggregation = agg;
}

GeneratorArgumentInfo::ArgAggEnum GeneratorArgumentInfo::getAggregation()
{
  return aggregation;
}

nanjit::TypeInfo GeneratorArgumentInfo::getType()
{
  return type;
}

std::string GeneratorArgumentInfo::toStr()
{
  std::stringstream result;

  if(aggregation == GeneratorArgumentInfo::ARG_AGG_SINGLE)
    result << "single(";
  else if(aggregation == GeneratorArgumentInfo::ARG_AGG_REF)
    result << "pointer(";
  else if(aggregation == GeneratorArgumentInfo::ARG_AGG_ARRAY)
    result << "array(";

  if (aligned)
    result << "aligned ";

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

llvm::Type *GeneratorArgumentInfo::getLLVMBaseType()
{
  return type.getLLVMType();
}

llvm::Type *GeneratorArgumentInfo::getLLVMType()
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

void GeneratorArgumentInfo::parse(std::string in_str)
{
  int argument_aggregation = GeneratorArgumentInfo::ARG_AGG_SINGLE;
  bool argument_aligned = false;
  int offset = 0;

  if(0 == in_str.compare(offset, 8, "aligned "))
    {
      argument_aligned = true;
      offset += 8;
    }
  setAligned(argument_aligned);

  if(in_str[0] == '*')
    {
      argument_aggregation = GeneratorArgumentInfo::ARG_AGG_REF;
      setAggregation(GeneratorArgumentInfo::ARG_AGG_REF);
      offset += 1;
    }

  std::string typestr;

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


#if 0
void create_null_function(llvm::Function *target_func)
{
  IRBuilder<> Builder(getGlobalContext());

  Type *out_type = target_func->getArgumentList().front().getType();

  BasicBlock *func_body_block = BasicBlock::Create(getGlobalContext(), "entry", func);
  Builder.SetInsertPoint(func_body_block);

  ConstantAggregateZero 
}
#endif

std::string llvm_type_to_string(const Type *type)
{
  std::string type_str;
  llvm::raw_string_ostream rso(type_str);
  type->print(rso);
  return rso.str();
}


llvm::Function *llvm_void_def_for(Module *module, const std::string &function_name, std::list<GeneratorArgumentInfo> &target_arg_list)
{
  IRBuilder<> Builder(getGlobalContext());

  std::vector<Type*> call_arg_types(target_arg_list.size() + 1);

  {
    std::list<GeneratorArgumentInfo>::iterator args_iter;
    std::vector<Type*>::iterator types_iter;

    for (args_iter = target_arg_list.begin(), types_iter = call_arg_types.begin();
         args_iter != target_arg_list.end();
         ++args_iter, ++types_iter)
      {
        *types_iter = args_iter->getLLVMType();
      }

    /* Add the count parameter */
    /* FIXME: Check the native int size */
    *types_iter = Builder.getInt32Ty();
  }

  FunctionType *func_type = FunctionType::get(Type::getVoidTy(getGlobalContext()), call_arg_types, false);

  Function *func = Function::Create(func_type, Function::ExternalLinkage, function_name + ".iteration", module);

  BasicBlock *func_body_block = BasicBlock::Create(getGlobalContext(), "entry", func);
  Builder.SetInsertPoint(func_body_block);
  Builder.CreateRetVoid();

  return func;
}

llvm::Function *llvm_def_for(Module *module, const std::string &function_name, std::list<GeneratorArgumentInfo> &target_arg_list)
{
  /* find our target function */
  Function *target_func = module->getFunction(function_name);
  if (!target_func)
  {
    throw GeneratorException("Module has no function \"" + function_name + "\"");
  }

  if (target_arg_list.begin()->getAggregation() != GeneratorArgumentInfo::ARG_AGG_ARRAY)
    throw GeneratorException("Return type must be an array");

  /* validate that the arguments match the target function */
  {
    const Function::ArgumentListType &target_func_args = target_func->getArgumentList();

    std::list<GeneratorArgumentInfo>::iterator args_iter = target_arg_list.begin();
    Type *return_type = target_arg_list.begin()->getLLVMBaseType();

    if (target_func->getReturnType() != return_type)
    {
      throw GeneratorException("Function \"" + function_name +
                               "\" returns \"" + llvm_type_to_string(target_func->getReturnType()) +
                               "\" but iteration requested \"" + llvm_type_to_string(return_type) + "\"");
    }


    if (target_func_args.size() != target_arg_list.size() - 1)
    {
      std::string error_str;
      llvm::raw_string_ostream rso(error_str);
      rso << "Function \"" << function_name << "\" takes "
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
          throw GeneratorException("Function \"" + function_name +
                                   "\" takes \"" + llvm_type_to_string(target_func_args_iter->getType()) +
                                   "\" but iteration requested \"" + llvm_type_to_string(args_iter->getLLVMBaseType()) + "\"");
        }
      
      args_iter++;
      target_func_args_iter++;
    }
  }

  /* generate wrapper function */
  IRBuilder<> Builder(getGlobalContext());

  std::vector<Type*> call_arg_types(target_arg_list.size() + 1);

  {
    std::list<GeneratorArgumentInfo>::iterator args_iter;
    std::vector<Type*>::iterator types_iter;
    
    for (args_iter = target_arg_list.begin(), types_iter = call_arg_types.begin();
         args_iter != target_arg_list.end();
         ++args_iter, ++types_iter)
      {
        *types_iter = args_iter->getLLVMType();
      }

    /* Add the count parameter */
    /* FIXME: Check the native int size */
    *types_iter = Builder.getInt32Ty();
  }

  FunctionType *func_type = FunctionType::get(Type::getVoidTy(getGlobalContext()), call_arg_types, false);

  Function *func = Function::Create(func_type, Function::ExternalLinkage, function_name + ".iteration", module);

  /* Build iteration */
  BasicBlock *func_body_block = BasicBlock::Create(getGlobalContext(), "entry", func);
  Builder.SetInsertPoint(func_body_block);

  /* Allocate stack space for all our args */
  std::vector<Value*> loop_variables(target_arg_list.size() + 1);
  Value *remaining_count_variable = NULL;

  {
    std::list<GeneratorArgumentInfo>::iterator args_iter;
    std::vector<Value*>::iterator loop_variables_iter;
    
    for (args_iter = target_arg_list.begin(), loop_variables_iter = loop_variables.begin();
         args_iter != target_arg_list.end();
         ++args_iter, ++loop_variables_iter)
      {
        if (args_iter->getAggregation() == GeneratorArgumentInfo::ARG_AGG_REF)
          {
            SequentialType *ptr_type = dyn_cast<SequentialType>(args_iter->getLLVMType());
            Type *base_type = ptr_type->getElementType();
            *loop_variables_iter = Builder.CreateAlloca(base_type);
          }
        else
          {
            *loop_variables_iter = Builder.CreateAlloca(args_iter->getLLVMType());
          }
      }

    /* FIXME: Check the native int size */
    *loop_variables_iter = Builder.CreateAlloca(Builder.getInt32Ty());
    remaining_count_variable = *loop_variables_iter;
  }

  /* If an arg is a single, copy it to the stack */
  /* If an arg is an array, copy it to the stack */
  /* If an arg is a pointer, load it's value to the stack */
  {
    Function::ArgumentListType::iterator func_args_iter;
    std::list<GeneratorArgumentInfo>::iterator args_iter;
    std::vector<Value*>::iterator loop_variables_iter;
    
    for (args_iter = target_arg_list.begin(),
          loop_variables_iter = loop_variables.begin(),
          func_args_iter = func->getArgumentList().begin();
         args_iter != target_arg_list.end();
         ++args_iter, ++loop_variables_iter, ++func_args_iter)
      { 
        if (args_iter->getAggregation() == GeneratorArgumentInfo::ARG_AGG_REF)
          {
            Builder.CreateAlignedStore(Builder.CreateLoad(func_args_iter), *loop_variables_iter, 16);
          }
        else
          {
            Builder.CreateAlignedStore(func_args_iter, *loop_variables_iter, 16);
          }
      }

    Builder.CreateStore(func_args_iter, *loop_variables_iter);
  }

  /* Begin loop */
  BasicBlock *BodyBB = BasicBlock::Create(getGlobalContext(), "loop_body", func);
  BasicBlock *LoopCondBB = BasicBlock::Create(getGlobalContext(), "loop_cond", func);
  BasicBlock *ExitBB = BasicBlock::Create(getGlobalContext(), "exit", func);
  Builder.CreateBr(LoopCondBB);
  {

    Builder.SetInsertPoint(BodyBB);
  /*   Call our target */
    {
      std::list<GeneratorArgumentInfo>::iterator args_info_iter = ++target_arg_list.begin();
      std::vector<Value*> call_parameters(loop_variables.begin() + 1, loop_variables.end() - 1);

      for (std::vector<Value*>::iterator call_parameters_iter = call_parameters.begin();
           call_parameters_iter != call_parameters.end();
           ++call_parameters_iter, ++args_info_iter)
      {
        Value *arg_value = *call_parameters_iter;

        if (args_info_iter->getAggregation() == GeneratorArgumentInfo::ARG_AGG_ARRAY)
          {
            /* If our argument is an array deref to the current value */
            *call_parameters_iter = Builder.CreateLoad(*call_parameters_iter);
            if (args_info_iter->getAligned())
            {
              *call_parameters_iter = Builder.CreateAlignedLoad(*call_parameters_iter, 16);
            }
            else
            {
              *call_parameters_iter = Builder.CreateAlignedLoad(*call_parameters_iter, 1);
            }
          }
        else
          {
            /* If the argument is not an array it must be on our stack and is therefor aligned */
            *call_parameters_iter = Builder.CreateAlignedLoad(*call_parameters_iter, 16);
          }
      }

      Value *call_result = Builder.CreateCall(target_func, call_parameters);

      /* FIXME: This assumes the return is always an array */
      if (target_arg_list.begin()->getAligned())
      {
        Builder.CreateAlignedStore(call_result, Builder.CreateLoad(loop_variables[0]), 16);
      }
      else
      {
        Builder.CreateAlignedStore(call_result, Builder.CreateLoad(loop_variables[0]), 1);
      }
    }


    {
      /* If an arg is an array, increment it */
      std::list<GeneratorArgumentInfo>::iterator args_iter;
      std::vector<Value*>::iterator loop_variables_iter;
      
      for (args_iter = target_arg_list.begin(), loop_variables_iter = loop_variables.begin();
           args_iter != target_arg_list.end();
           ++args_iter, ++loop_variables_iter)
        {
          if (args_iter->getAggregation() == GeneratorArgumentInfo::ARG_AGG_ARRAY)
            {
              Builder.CreateStore(Builder.CreateGEP(Builder.CreateLoad(*loop_variables_iter), Builder.getInt32(1)), *loop_variables_iter);
            }
        }
    }

    Builder.CreateBr(LoopCondBB);
  }


  Builder.SetInsertPoint(LoopCondBB);
  {
    Value *val = Builder.CreateLoad(remaining_count_variable);
    Value *comparison = Builder.CreateICmpUGT(val, Builder.getInt32(0));
    val = Builder.CreateNUWSub(val, Builder.getInt32(1));
    Builder.CreateStore(val, remaining_count_variable);
    Builder.CreateCondBr(comparison, BodyBB, ExitBB);
  }
  
  /* End loop */
  Builder.SetInsertPoint(ExitBB);
  Builder.CreateRetVoid();

#if 0
  /* Create a null function, this is inteded for the error case */
  {
    /* FIXME: Assert that the return argument is a pointer */
    SequentialType *result_ptr_type = dyn_cast<SequentialType>(target_arg_list.front().getLLVMType());
    Value *null_result = ConstantAggregateZero::get(result_ptr_type->getElementType());

    BasicBlock *func_body_block = BasicBlock::Create(getGlobalContext(), "entry", func);
    Builder.SetInsertPoint(func_body_block);

    Function::arg_iterator func_arg_iter = func->arg_begin();
    Builder.CreateStore(null_result, func_arg_iter);
    Builder.CreateRetVoid();
  }
#endif

  return func;
}
