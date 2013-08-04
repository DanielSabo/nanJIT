#ifndef __JITMODULE_HPP__
#define __JITMODULE_HPP__
#ifndef __cplusplus
typedef struct _JitModule JitModule;
#else
class JitModule;
#endif

#ifdef __cplusplus
extern "C" {
#endif
  typedef enum
  {
    JIT_MODULE_DEFAULT_FLAGS = 0x0,
    JIT_MODULE_DEBUG_AST  = 0x0100,
    JIT_MODULE_DEBUG_LLVM = 0x0200
  } JitModuleFlags;

  JitModule *jit_module_for_src(const char *src, unsigned int module_flags);
  void *jit_module_get_iteration(JitModule *jm, const char *function_name, const char *return_type, ...);
  unsigned int jit_module_is_fallback_function(JitModule *jm, void *func);
  void jit_module_destroy(JitModule *jm);
#ifdef __cplusplus
};
#endif

#ifdef __cplusplus
#include <map>
#include <list>

namespace llvm {
  class Module;
  class Function;
};

class JitModuleException : public std::exception
{
  std::string error_string;
public:
  JitModuleException (const char *e) : error_string(e) {}
  JitModuleException (std::string e) : error_string(e) {}
  ~JitModuleException() throw() {}
  const char* what() const throw() { return error_string.c_str(); }
};

class JitModuleIterationData
{
public:
  llvm::Module *module;
  llvm::Function *function;
  void *compiledFunciton;
  bool voidFunction;

  JitModuleIterationData() : module(NULL), function(NULL), compiledFunciton(NULL), voidFunction(false) {};
};

class JitModuleState;

class JitModule
{
  llvm::Module *module;
  JitModuleState *internal;
  unsigned int flags;

  std::map<std::string, JitModuleIterationData> liveFunctions;

public:
  JitModule(const char *sourcecode, unsigned int module_flags);
  void *getIteration(const char *function_name, const char *return_type, ...) __attribute__ ((sentinel));
  void *getIteration(const char *function_name, const std::list<std::string> &argstrs);
  bool isFallbackFunction(void *function);

  ~JitModule();
  
  std::string getLLVMCode();
};
#endif /* __cplusplus */
#endif /* __JITMODULE_HPP__ */
