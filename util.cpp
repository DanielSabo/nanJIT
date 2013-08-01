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

#include "util.h"

using namespace llvm;

/* need to push each input argument onto the stack */
/* load, (call), inc, store */

Function *createIteration(Function *target_func, Module *module)
{
  IRBuilder<> Builder(getGlobalContext());

  Function::ArgumentListType &target_arg_list = target_func->getArgumentList();
  std::vector<Type*>  call_arg_types(target_arg_list.size() + 1);

  {
    Function::ArgumentListType::iterator args_iter;
    std::vector<Type*>::iterator types_iter;
    
    for (args_iter = target_arg_list.begin(), types_iter = call_arg_types.begin();
         args_iter != target_arg_list.end();
         ++args_iter, ++types_iter)
      {
        *types_iter = args_iter->getType();
      }

    /* Add the count parameter */
    /* FIXME: Check the native int size */
    *types_iter = Builder.getInt32Ty();
  }

  /* create the function type */
  FunctionType *func_type = FunctionType::get(Type::getVoidTy(getGlobalContext()), call_arg_types, false);

  Function *func = Function::Create(func_type, Function::ExternalLinkage, "function", module);
  
  BasicBlock *func_body_block = BasicBlock::Create(getGlobalContext(), "entry", func);
  Builder.SetInsertPoint(func_body_block);
  
  /* Stack allocations */
  std::vector<Value*> loop_variables(target_arg_list.size() + 1);
  Value *remaining_count_variable = NULL;
  {
    Function::ArgumentListType::iterator args_iter;
    std::vector<Value*>::iterator loop_variables_iter;
    
    for (args_iter = target_arg_list.begin(), loop_variables_iter = loop_variables.begin();
         args_iter != target_arg_list.end();
         ++args_iter, ++loop_variables_iter)
      {
        *loop_variables_iter = Builder.CreateAlloca(args_iter->getType());
      }

    /* FIXME: Check the native int size */
    *loop_variables_iter = Builder.CreateAlloca(Builder.getInt32Ty());
    remaining_count_variable = *loop_variables_iter;
  }

  {
    Function::ArgumentListType::iterator func_args_iter;
    std::vector<Value*>::iterator loop_variables_iter;
    
    for (func_args_iter = func->getArgumentList().begin(),
          loop_variables_iter = loop_variables.begin();
         func_args_iter != func->getArgumentList().end();
           ++func_args_iter,
           ++loop_variables_iter)
      {
         Builder.CreateStore(func_args_iter, *loop_variables_iter);
      }
  }

  /* Make the call */
  BasicBlock *BodyBB = BasicBlock::Create(getGlobalContext(), "loop_body", func);
  BasicBlock *LoopCondBB = BasicBlock::Create(getGlobalContext(), "loop_cond", func);
  BasicBlock *ExitBB = BasicBlock::Create(getGlobalContext(), "exit", func);
  Builder.CreateBr(LoopCondBB);
  {
    std::vector<Value*> call_arguments(target_arg_list.size());

    Builder.SetInsertPoint(BodyBB);
      
    Function::ArgumentListType::iterator args_iter;
    std::vector<Value*>::iterator call_arguments_iter;
    std::vector<Value*>::iterator loop_variables_iter;
    
    Value *index =  Builder.CreateLoad(remaining_count_variable);
    
    for (args_iter = target_arg_list.begin(),
           loop_variables_iter = loop_variables.begin(),
           call_arguments_iter = call_arguments.begin();
         args_iter != target_arg_list.end();
         ++args_iter,
           ++loop_variables_iter,
           ++call_arguments_iter)
      {
        *call_arguments_iter = Builder.CreateLoad(*loop_variables_iter);
      }
    Builder.CreateCall(target_func, call_arguments);

    for (args_iter = target_arg_list.begin(),
           loop_variables_iter = loop_variables.begin(),
           call_arguments_iter = call_arguments.begin();
         args_iter != target_arg_list.end();
         ++args_iter,
           ++loop_variables_iter,
           ++call_arguments_iter)
      {
        Builder.CreateStore(Builder.CreateGEP(Builder.CreateLoad(*loop_variables_iter), Builder.getInt32(1)), *loop_variables_iter);
      }
  }
  
  Builder.CreateBr(LoopCondBB);
  Builder.SetInsertPoint(LoopCondBB);
  /* Increment variables */
  {
    Value *val = Builder.CreateLoad(remaining_count_variable);
    Value *comparison = Builder.CreateICmpUGT(val, Builder.getInt32(0));
    val = Builder.CreateNUWSub(val, Builder.getInt32(1));
    Builder.CreateStore(val, remaining_count_variable);
    Builder.CreateCondBr(comparison, BodyBB, ExitBB);
  }
  
  //Builder.CreateBr(ExitBB);
  Builder.SetInsertPoint(ExitBB);

  Builder.CreateRetVoid();
  
  return func;
}
