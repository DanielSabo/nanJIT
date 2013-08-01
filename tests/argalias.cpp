#include <cstdio>
#include <iostream>
#include <fstream>
#include <sstream>
#include <memory>
#include <stdint.h>
using namespace std;

#include "jitmodule.h"


typedef void (*AliasedFunction)(int *a, int *b, uint32_t count);

bool test_alias()
{
  static const char *shader_src = \
    "int4 process(int4 a, int4 b) "
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

  void *jitfunc = jitmod->getIteration("process", "alias(1) int4[]", "int4[]", "int4[]", NULL);

  if (NULL == jitfunc || jitmod->isFallbackFunction(jitfunc))
  {
    cout << "Function is fallback" << endl;
    return false;
  }

  int a_array[8] = { 15,  15,  15,  15,  15,  15,  15,  15};
  int b_array[8] = { 60,  60,  60,  60,  60,  60,  60,  60};
  int expected[8] = { 15,  15,  75,  75,  75,  75,  15,  15};

  ((AliasedFunction)jitfunc)(a_array + 2, b_array + 2, 1);

  for (int i = 0; i < 8; ++i)
    {
      if (a_array[i] != expected[i])
        return false;

      //cout << a_array[i] << " : " << b_array[i] << endl;
    }

  return true;
}

bool test_alias2()
{
  static const char *shader_src = \
    "int4 process(int4 a, int4 b) "
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

  void *jitfunc = jitmod->getIteration("process", "alias(2) int4[]", "int4[]", "int4[]", NULL);

  if (NULL == jitfunc || jitmod->isFallbackFunction(jitfunc))
  {
    cout << "Function is fallback" << endl;
    return false;
  }

  int a_array[8] = { 15,  15,  15,  15,  15,  15,  15,  15};
  int b_array[8] = { 60,  60,  60,  60,  60,  60,  60,  60};
  int expected_a[8] = { 15,  15,  15,  15,  15,  15,  15,  15};
  int expected_b[8] = { 60,  60,  75,  75,  75,  75,  60,  60};

  ((AliasedFunction)jitfunc)(a_array + 2, b_array + 2, 1);

  for (int i = 0; i < 8; ++i)
    {
      if (a_array[i] != expected_a[i])
        return false;
      if (b_array[i] != expected_b[i])
        return false;

      //cout << a_array[i] << " : " << b_array[i] << endl;
    }

  return true;
}

bool test_bad_alias_index()
{
  static const char *shader_src = \
    "int4 process(int4 a, int4 b) "
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

  void *jitfunc = jitmod->getIteration("process", "alias(0) int4[]", "int4[]", "int4[]", NULL);

  if (!jitmod->isFallbackFunction(jitfunc))
    {
      cout << "Function generated with alias value of 0" << endl;
      return false;
    }

  return true;
}


bool test_bad_alias_index2()
{
  static const char *shader_src = \
    "int4 process(int4 a, int4 b) "
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

  void *jitfunc = jitmod->getIteration("process", "alias(3) int4[]", "int4[]", "int4[]", NULL);

  if (!jitmod->isFallbackFunction(jitfunc))
    {
      cout << "Function generated with out of range alias value" << endl;
      return false;
    }

  return true;
}

int main(int argc, char **argv) {
  int pass_count = 0;
  int fail_count = 0;

  test_alias() ? pass_count++ : fail_count++;
  test_alias2() ? pass_count++ : fail_count++;
  test_bad_alias_index() ? pass_count++ : fail_count++;
  test_bad_alias_index2() ? pass_count++ : fail_count++;

  cout << "ran " << (pass_count + fail_count) << " tests: " << pass_count << " ok, " << fail_count << " failures" << endl;

  if (!fail_count)
    cout << "OK" << endl;
  else
    cout << "FAIL" << endl;

  return fail_count;
}
