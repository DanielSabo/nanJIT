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
#include "llvm/Target/TargetFrameLowering.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Utils/Cloning.h"

#include "llvm/Assembly/PrintModulePass.h"

#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/raw_os_ostream.h>
using namespace llvm;

#include <cstdio>
#include <cstdarg>
#include <map>
#include <iostream>
#include <fstream>
#include <sstream>
using namespace std;
 
#include "ast.h"
#include "util.h"
#include "varg.h"

#include "jitmodule.h"
#include "jitparser.h"

JitModule *jit_module_for_src(const char *src, unsigned int flags)
{
  try
  {
    return new JitModule(src, flags);
  }
  catch (std::exception& e)
  {
    cout << e.what() << endl;
    return NULL;
  }
}

void *jit_module_get_iteration(JitModule *jm, const char *function_name, const char *return_type, ...)
{
  va_list vargs;
  va_start(vargs, return_type);

  std::list<std::string> argstrs;

  argstrs.push_back(std::string(return_type));

  const char *arg_type = va_arg(vargs, char *);
  while (arg_type)
  {
    argstrs.push_back(std::string(arg_type));
    arg_type = va_arg(vargs, char *);
  }
  va_end(vargs);

  return jm->getIteration(function_name, argstrs);
}

unsigned int jit_module_is_fallback_function(JitModule *jm, void *func)
{
  return jm->isFallbackFunction(func);
}

void jit_module_destroy(JitModule *jm)
{
  delete jm;
}

class JitModuleState
{
public:
  ExecutionEngine *execution_engine;
  TargetMachine *target_machine;

  void optimizeModule(Module *module);

  JitModuleState() : execution_engine(NULL), target_machine(NULL) {};
  ~JitModuleState();
};

void JitModuleState::optimizeModule(Module *module)
{
  auto_ptr<FunctionPassManager> function_optimizer_passes(new FunctionPassManager(module));
  auto_ptr<PassManager> module_optimizer_passes(new PassManager());

  function_optimizer_passes->add(createVerifierPass());

  function_optimizer_passes->add(new DataLayout(*execution_engine->getDataLayout()));
#if ((LLVM_VERSION_MAJOR >= 3) && (LLVM_VERSION_MINOR >= 3))
  target_machine->addAnalysisPasses(*function_optimizer_passes);
#else
  function_optimizer_passes->add(new TargetTransformInfo(target_machine->getScalarTargetTransformInfo(),
                                                         target_machine->getVectorTargetTransformInfo()));
#endif

  {
    PassManagerBuilder pass_builder;
    pass_builder.OptLevel = CodeGenOpt::Default; // -O2, CodeGenOpt::Aggressive is -O3
    pass_builder.Inliner = createFunctionInliningPass();

    pass_builder.populateFunctionPassManager(*function_optimizer_passes);
    pass_builder.populateModulePassManager(*module_optimizer_passes);

    /* Taken from the Intel SIMD compiler -O0 */
    /*
    module_optimizer_passes->add(llvm::createIndVarSimplifyPass());
    module_optimizer_passes->add(llvm::createFunctionInliningPass());
    module_optimizer_passes->add(llvm::createCFGSimplificationPass());
    module_optimizer_passes->add(llvm::createGlobalDCEPass());
    */
  }

  function_optimizer_passes->doInitialization();

  Module::FunctionListType &function_list = module->getFunctionList();
  //cout << "Generated module \"" << module->getModuleIdentifier();
  //cout << "\" contains " << function_list.size() << " functions." << endl;

  for (Module::FunctionListType::iterator it = function_list.begin(); it != function_list.end(); ++it)
    {
      //cout << "Pre-optimizing \"" << it->getName().str() << "()\"" << endl;
      function_optimizer_passes->run(*it);
    }
  function_optimizer_passes->doFinalization();

  module_optimizer_passes->run(*module);
}

JitModuleState::~JitModuleState()
{
  if (execution_engine)
    delete execution_engine;
  if (target_machine)
    delete target_machine;
}

JitModule::JitModule(const char *sourcecode, unsigned int flags)
{
  module = new Module("nanJIT Dummy Module", getGlobalContext());
  internal = new JitModuleState();

  InitializeNativeTarget();

  std::string errStr;

  EngineBuilder engine_builder(module);
  internal->target_machine = engine_builder.selectTarget();
  internal->execution_engine = engine_builder.setErrorStr(&errStr).create();
  if (!internal->execution_engine) {
    delete module;
    throw JitModuleException("Could not create ExecutionEngine: " + errStr);
  }

  cout << "JIT Target:  " << internal->target_machine->getTargetTriple().str() << endl;
  //cout << "stack align: " << internal->target_machine->getFrameLowering()->getStackAlignment() << endl;

  /* Parse the source string */
  std::stringstream source_code_stream(sourcecode);
  ModuleAST *ast = nanjit::parse(source_code_stream);

  if (ast)
  {
    if (flags & JIT_MODULE_DEBUG_AST)
        ast->print(cout) << endl;

    Module *maybe_module = new Module("nanJIT Module", getGlobalContext());

    try
      {
        ast->codegen(maybe_module);
        module = maybe_module;
      }
    catch (std::exception& e)
      {
        cout << "Codegen error: " << e.what() << endl;
        delete maybe_module;
      }
  }

  internal->optimizeModule(module);

  delete ast;

  if (flags & JIT_MODULE_DEBUG_LLVM)
    module->dump();
}

void *JitModule::getIteration(const char *function_name, const char *return_type, ...)
{
  va_list vargs;
  va_start(vargs, return_type);

  std::list<std::string> argstrs;

  argstrs.push_back(std::string(return_type));

  const char *arg_type = va_arg(vargs, char *);
  while (arg_type)
  {
    argstrs.push_back(std::string(arg_type));
    arg_type = va_arg(vargs, char *);
  }
  va_end(vargs);

  return getIteration(function_name, argstrs);
}

void *JitModule::getIteration(const char *function_name, const std::list<std::string> &argstrs)
{
  std::list<GeneratorArgumentInfo> arginfos;

  stringstream function_description;

  try
  {
    for(std::list<std::string>::const_iterator iter = argstrs.begin(); iter != argstrs.end(); ++iter)
    {
      arginfos.push_back(GeneratorArgumentInfo(*iter));
    }

    {
      std::list<GeneratorArgumentInfo>::iterator iter = arginfos.begin();
      GeneratorArgumentInfo return_info = *iter++; /* Skip the return value when printing args */

      function_description << function_name << "(";
      while(iter != arginfos.end())
      {
        function_description << iter->toStr();

        if (++iter != arginfos.end())
          function_description << ", ";
      }
      function_description << ") -> " << return_info.toStr();
    }

    if (liveFunctions.find(function_description.str()) != liveFunctions.end())
    {
      cout << "Existing function for " << function_description.str() << endl;
      return liveFunctions[function_description.str()].compiledFunciton;
    }
  }
  catch (std::exception& e)
  {
    printf("Error in getIteration(%s): %s\n", function_name, e.what());
    return NULL;
  }

  Module *cloned_module = CloneModule(module);

  try
  {
    cout << "Will generate " << function_description.str() << endl;

    Function *iter_func = llvm_def_for(cloned_module, std::string(function_name), arginfos);

    internal->optimizeModule(cloned_module);

    JitModuleIterationData iter_data;

    iter_data.module = cloned_module;
    iter_data.function = iter_func;
    iter_data.voidFunction = false;

    {
      MachineCodeInfo machine_code_info;
      internal->execution_engine->runJITOnFunction(iter_func, &machine_code_info);
      printf("jit result %lld bytes @ %p\n", (long long)machine_code_info.size(), machine_code_info.address());
      iter_data.compiledFunciton = internal->execution_engine->getPointerToFunction(iter_func);
    }

    liveFunctions[function_description.str()] = iter_data;

    return iter_data.compiledFunciton;
  }
  catch (std::exception& e)
  {
    printf("Error in function_for(%s): %s\n", function_name, e.what());

    Function *iter_func = llvm_void_def_for(cloned_module, std::string(function_name), arginfos);

    internal->optimizeModule(cloned_module);

    JitModuleIterationData iter_data;

    iter_data.module = cloned_module;
    iter_data.function = iter_func;
    iter_data.voidFunction = true;

    {
      MachineCodeInfo machine_code_info;
      internal->execution_engine->runJITOnFunction(iter_func, &machine_code_info);
      printf("jit result %lld bytes @ %p\n", (long long)machine_code_info.size(), machine_code_info.address());
      iter_data.compiledFunciton = internal->execution_engine->getPointerToFunction(iter_func);
    }

    liveFunctions[function_description.str()] = iter_data;

    return iter_data.compiledFunciton;
  }
}

bool JitModule::isFallbackFunction(void *function)
{
  std::map<std::string, JitModuleIterationData>::iterator iter;

  for(iter = liveFunctions.begin();
      iter != liveFunctions.end();
      ++iter)
  {
    if (iter->second.compiledFunciton == function)
      return iter->second.voidFunction;
  }

  return false;
}

std::string JitModule::getLLVMCode()
{
  std::string result;
  llvm::raw_string_ostream rso(result);

  PassManager PM;
  PM.add(createPrintModulePass(&rso));
  PM.run(*module);
  
  return result;
}

JitModule::~JitModule()
{
  delete internal;
}
