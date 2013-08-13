#!/usr/bin/env python

import ctypes, os, sys
import unittest
import nanjit

class BaseNanjitTestErrorFunc(unittest.TestCase):
  def doTest(self, shaderstr, funcname):
    jitmod = None
    try:
      print ""
      jitmod = nanjit.jit_module_for_src(shaderstr, 0)
      jitfunc = nanjit.jit_module_get_iteration(jitmod, funcname, "float4[]", "float4[]", "float4[]", None)

      self.assertTrue(nanjit.jit_module_is_fallback_function(jitmod, jitfunc))
    finally:
      if jitmod:
        nanjit.jit_module_destroy(jitmod)

class ErrorTests(BaseNanjitTestErrorFunc):
  def test_001_check_fallback(self):
    shaderstr = \
"""float4 process(float4 in, float4 aux)
{
  return aux + in;
}
"""
    jitmod = None
    try:
      print ""
      jitmod = nanjit.jit_module_for_src(shaderstr, 0)
      jitfunc = nanjit.jit_module_get_iteration(jitmod, "process", "float4[]", "float4[]", "float4[]", None)
      self.assertFalse(nanjit.jit_module_is_fallback_function(jitmod, jitfunc))

      jitfunc = nanjit.jit_module_get_iteration(jitmod, "notprocess", "float4[]", "float4[]", "float4[]", None)
      self.assertTrue(nanjit.jit_module_is_fallback_function(jitmod, jitfunc))
    finally:
      if jitmod:
        nanjit.jit_module_destroy(jitmod)

  def test_002_invalid_identifier(self):
    shaderstr = \
"""float4 process(float4 in, float4 aux)
{
  float4 out = aux + in + one;
  return out;
}
"""
    self.doTest(shaderstr, "process")

  def test_002_mixed_widths(self):
    shaderstr = \
"""float4 process(float4 in, float4 aux)
{
  float4 out = aux + in + 1.0f;
  return out;
}
"""
    self.doTest(shaderstr, "process")

  def test_003_missing_paren(self):
    shaderstr = \
"""float4 process(float4 in, float4 aux)
{
  return shuffle2(out, in, (uint4)(0, 1, 2, 7);;
}
"""
    self.doTest(shaderstr, "process")

  def test_004_wrong_iterator_type(self):
    shaderstr = \
"""float process(float in, float aux)
{
  return 1.0f + 2.0f;
}
"""
    self.doTest(shaderstr, "process")

  def test_005_variable_redefinition(self):
    shaderstr = \
"""float4 process(float4 in, float4 aux)
{
  float4 out = in;
  float4 out = aux;
  return out;
}
"""
    self.doTest(shaderstr, "process")

  def test_006_variable_redefinition2(self):
    shaderstr = \
"""float4 process(float4 in, float4 aux)
{
  float4 out = in;
  int4   out = (int4)(0, 0, 0, 0);
  return out;
}
"""
    self.doTest(shaderstr, "process")

  def test_007_variable_out_of_scope(self):
    shaderstr = \
"""float4 process(float4 in, float4 aux)
{
  if (1.0f < 2.0f)
    {
      float4 out = in;
    }
  return out;
}
"""
    self.doTest(shaderstr, "process")

if __name__ == '__main__':
    unittest.main(verbosity=10)