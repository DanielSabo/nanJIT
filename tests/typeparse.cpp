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
using namespace std;

#include <list>
#include <string>
#include <memory>
#include <iostream>

#include <stdarg.h>
#include <stdio.h>

#include "typeinfo.h"

int main(int argc, char **argv)
{
  int fail_count = 0;
  int pass_count = 0;

  {
    nanjit::TypeInfo float_type("float");
    nanjit::TypeInfo float4_type("float4");
    nanjit::TypeInfo int_type("int");
    nanjit::TypeInfo int4_type("int4");
    nanjit::TypeInfo uint_type("uint");
    nanjit::TypeInfo uint4_type("uint4");
  
    cout << float_type.toStr() << endl;
    cout << float4_type.toStr() << endl;
    cout << int_type.toStr() << endl;
    cout << int4_type.toStr() << endl;
    cout << uint_type.toStr() << endl;
    cout << uint4_type.toStr() << endl;

    if (!(float_type.getBaseType() == nanjit::TypeInfo::TYPE_FLOAT &&
          float_type.getWidth() == 1))
      fail_count += 1;
    else
      pass_count += 1;

    if (!(float4_type.getBaseType() == nanjit::TypeInfo::TYPE_FLOAT &&
          float4_type.getWidth() == 4))
      fail_count += 1;
    else
      pass_count += 1;

    if (!(int_type.getBaseType() == nanjit::TypeInfo::TYPE_INT &&
          int_type.getWidth() == 1))
      fail_count += 1;
    else
      pass_count += 1;

    if (!(int4_type.getBaseType() == nanjit::TypeInfo::TYPE_INT &&
          int4_type.getWidth() == 4))
      fail_count += 1;
    else
      pass_count += 1;

    if (!(uint_type.getBaseType() == nanjit::TypeInfo::TYPE_UINT &&
          uint_type.getWidth() == 1))
      fail_count += 1;
    else
      pass_count += 1;

    if (!(uint4_type.getBaseType() == nanjit::TypeInfo::TYPE_UINT &&
          uint4_type.getWidth() == 4))
      fail_count += 1;
    else
      pass_count += 1;
  }
  {
    if (!(nanjit::TypeInfo("float").getBaseType() == nanjit::TypeInfo::TYPE_FLOAT))
      fail_count += 1;
    else
      pass_count += 1;
    if (!(nanjit::TypeInfo("int").getBaseType() == nanjit::TypeInfo::TYPE_INT))
      fail_count += 1;
    else
      pass_count += 1;
    if (!(nanjit::TypeInfo("uint").getBaseType() == nanjit::TypeInfo::TYPE_UINT))
      fail_count += 1;
    else
      pass_count += 1;
    if (!(nanjit::TypeInfo("short").getBaseType() == nanjit::TypeInfo::TYPE_SHORT))
      fail_count += 1;
    else
      pass_count += 1;
    if (!(nanjit::TypeInfo("ushort").getBaseType() == nanjit::TypeInfo::TYPE_USHORT))
      fail_count += 1;
    else
      pass_count += 1;
    if (!(nanjit::TypeInfo("char").getBaseType() == nanjit::TypeInfo::TYPE_CHAR))
      fail_count += 1;
    else
      pass_count += 1;
    if (!(nanjit::TypeInfo("uchar").getBaseType() == nanjit::TypeInfo::TYPE_UCHAR))
      fail_count += 1;
    else
      pass_count += 1;
    if (!(nanjit::TypeInfo("bool").getBaseType() == nanjit::TypeInfo::TYPE_BOOL))
      fail_count += 1;
    else
      pass_count += 1;
  }
  {
    if (nanjit::TypeInfo("float") == nanjit::TypeInfo("float"))
      pass_count += 1;
    else
      fail_count += 1;

    if (nanjit::TypeInfo("int4") == nanjit::TypeInfo("int4"))
      pass_count += 1;
    else
      fail_count += 1;

    if (nanjit::TypeInfo("float") != nanjit::TypeInfo("int"))
      pass_count += 1;
    else
      fail_count += 1;

    if (nanjit::TypeInfo("float") != nanjit::TypeInfo("float4"))
      pass_count += 1;
    else
      fail_count += 1;
  }

  cout << "ran " << (pass_count + fail_count) << " tests: " << pass_count << " ok, " << fail_count << " failures" << endl;

  if (!fail_count)
    cout << "OK" << endl;
  else
    cout << "FAIL" << endl;

  return fail_count;
}
