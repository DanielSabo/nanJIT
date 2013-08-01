#include <cstdio>
#include <iostream>
#include <fstream>
#include <sstream>
#include <memory>
#include <stdint.h>
using namespace std;

#include "jitmodule.h"

bool test_float4()
{
  static const char *shader_src = \
    "float4 process(float4 a, float4 b) "
    "{ "
    "return (float4)(a.s0, b.s0, a.s2 + b.s2, a.s3 + b.s3); "
    "}";

  auto_ptr<JitModule> jitmod;

  try
    {
      jitmod.reset(new JitModule(shader_src, 0));
    }
  catch (JitModuleException &e)
    {
      cout << e.what() << endl;
      return false;
    }

  static const char *typestr = "float4[]";

  void *jitfunc = jitmod->getIteration("process", typestr, typestr, typestr, NULL);

  if (NULL == jitfunc || jitmod->isFallbackFunction(jitfunc))
    {
      cout << "Function for " << typestr << " is fallback" << endl;
      return false;
    }

  return true;
}

bool test_float3()
{
  static const char *shader_src = \
    "float3 process(float3 a, float3 b) "
    "{ "
    "return a + b; "
    "}";

  auto_ptr<JitModule> jitmod;

  try
    {
      jitmod.reset(new JitModule(shader_src, 0));
    }
  catch (JitModuleException &e)
    {
      cout << e.what() << endl;
      return false;
    }

  static const char *typestr = "float3[]";

  void *jitfunc = jitmod->getIteration("process", typestr, typestr, typestr, NULL);

  if (NULL == jitfunc || jitmod->isFallbackFunction(jitfunc))
    {
      cout << "Function for " << typestr << " is fallback" << endl;
      return false;
    }

  return true;
}

bool test_float()
{
  static const char *shader_src = \
    "float process(float a, float b) "
    "{ "
    "return a + b; "
    "}";

  auto_ptr<JitModule> jitmod;

  try
    {
      jitmod.reset(new JitModule(shader_src, 0));
    }
  catch (JitModuleException &e)
    {
      cout << e.what() << endl;
      return false;
    }

  static const char *typestr = "float[]";

  void *jitfunc = jitmod->getIteration("process", typestr, typestr, typestr, NULL);

  if (NULL == jitfunc || jitmod->isFallbackFunction(jitfunc))
    {
      cout << "Function for " << typestr << " is fallback" << endl;
      return false;
    }

  return true;
}

bool test_int4()
{
  static const char *shader_src = \
    "int4 process(int4 a, int4 b) "
    "{ "
    "return (int4)(a.s0, b.s0, a.s2 + b.s2, a.s3 + b.s3); "
    "}";

  auto_ptr<JitModule> jitmod;

  try
    {
      jitmod.reset(new JitModule(shader_src, 0));
    }
  catch (JitModuleException &e)
    {
      cout << e.what() << endl;
      return false;
    }

  static const char *typestr = "int4[]";

  void *jitfunc = jitmod->getIteration("process", typestr, typestr, typestr, NULL);

  if (NULL == jitfunc || jitmod->isFallbackFunction(jitfunc))
    {
      cout << "Function for " << typestr << " is fallback" << endl;
      return false;
    }

  return true;
}

bool test_uint4()
{
  static const char *shader_src = \
    "uint4 process(uint4 a, uint4 b) "
    "{ "
    "return (uint4)(a.s0, b.s0, a.s2 + b.s2, a.s3 + b.s3); "
    "}";

  auto_ptr<JitModule> jitmod;

  try
    {
      jitmod.reset(new JitModule(shader_src, 0));
    }
  catch (JitModuleException &e)
    {
      cout << e.what() << endl;
      return false;
    }

  static const char *typestr = "uint4[]";

  void *jitfunc = jitmod->getIteration("process", typestr, typestr, typestr, NULL);

  if (NULL == jitfunc || jitmod->isFallbackFunction(jitfunc))
    {
      cout << "Function for " << typestr << " is fallback" << endl;
      return false;
    }

  return true;
}

bool test_short4()
{
  static const char *shader_src = \
    "short4 process(short4 a, short4 b) "
    "{ "
    "return (short4)(a.s0, b.s0, a.s2 + b.s2, a.s3 + b.s3); "
    "}";

  auto_ptr<JitModule> jitmod;

  try
    {
      jitmod.reset(new JitModule(shader_src, 0));
    }
  catch (JitModuleException &e)
    {
      cout << e.what() << endl;
      return false;
    }

  static const char *typestr = "short4[]";

  void *jitfunc = jitmod->getIteration("process", typestr, typestr, typestr, NULL);

  if (NULL == jitfunc || jitmod->isFallbackFunction(jitfunc))
    {
      cout << "Function for " << typestr << " is fallback" << endl;
      return false;
    }

  return true;
}

bool test_ushort4()
{
  static const char *shader_src = \
    "ushort4 process(ushort4 a, ushort4 b) "
    "{ "
    "return (ushort4)(a.s0, b.s0, a.s2 + b.s2, a.s3 + b.s3); "
    "}";

  auto_ptr<JitModule> jitmod;

  try
    {
      jitmod.reset(new JitModule(shader_src, 0));
    }
  catch (JitModuleException &e)
    {
      cout << e.what() << endl;
      return false;
    }

  static const char *typestr = "ushort4[]";

  void *jitfunc = jitmod->getIteration("process", typestr, typestr, typestr, NULL);

  if (NULL == jitfunc || jitmod->isFallbackFunction(jitfunc))
    {
      cout << "Function for " << typestr << " is fallback" << endl;
      return false;
    }

  return true;
}

bool test_uchar4()
{
  static const char *shader_src = \
    "uchar4 process(uchar4 a, uchar4 b) "
    "{ "
    "return (uchar4)(a.s0, b.s0, a.s2 + b.s2, a.s3 + b.s3); "
    "}";

  auto_ptr<JitModule> jitmod;

  try
    {
      jitmod.reset(new JitModule(shader_src, 0));
    }
  catch (JitModuleException &e)
    {
      cout << e.what() << endl;
      return false;
    }

  static const char *typestr = "uchar4[]";

  void *jitfunc = jitmod->getIteration("process", typestr, typestr, typestr, NULL);

  if (NULL == jitfunc || jitmod->isFallbackFunction(jitfunc))
    {
      cout << "Function for " << typestr << " is fallback" << endl;
      return false;
    }

  return true;
}

int main(int argc, char **argv) {
  int pass_count = 0;
  int fail_count = 0;

  test_float4() ? pass_count++ : fail_count++;
  test_float3() ? pass_count++ : fail_count++;
  test_float()  ? pass_count++ : fail_count++;
  test_int4() ? pass_count++ : fail_count++;
  test_uint4() ? pass_count++ : fail_count++;
  test_short4() ? pass_count++ : fail_count++;
  test_ushort4() ? pass_count++ : fail_count++;
  //test_char4() ? pass_count++ : fail_count++;
  //test_uchar4() ? pass_count++ : fail_count++;

  cout << "ran " << (pass_count + fail_count) << " tests: " << pass_count << " ok, " << fail_count << " failures" << endl;

  if (!fail_count)
    cout << "OK" << endl;
  else
    cout << "FAIL" << endl;

  return fail_count;
}
