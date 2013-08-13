#!/usr/bin/env python

import ctypes, os, sys
import unittest
import nanjit

def buffer_from_list(base_type, values):
  buffer_type = base_type * len(values)
  return buffer_type(*values)

class BaseNanjitTestFloatFunc(unittest.TestCase):
  def compare_buffers(self, buffer_a, buffer_b):
    buffer_a = map(float, buffer_a)
    buffer_b = map(float, buffer_b)
    for a, b in zip(buffer_a, buffer_b):
      self.assertAlmostEqual(a, b)

  def doTest(self, shaderstr, funcname, in_pixel, aux_pixel, out_pixel):
    zero_out_pixel = [0.0] * len(out_pixel)

    in_values  = in_pixel * 2
    aux_values = aux_pixel * 2
    out_values = zero_out_pixel * 2

    in_buf  = buffer_from_list(ctypes.c_float, in_values)
    aux_buf = buffer_from_list(ctypes.c_float, aux_values)
    out_buf = buffer_from_list(ctypes.c_float, out_values)

    expected_out_0 = zero_out_pixel + zero_out_pixel
    expected_out_1 = out_pixel + zero_out_pixel
    expected_out_2 = out_pixel + out_pixel

    jitmod = None
    try:
      #print ""
      jitmod = nanjit.jit_module_for_src(shaderstr, 0)
      jitfunc = nanjit.jit_module_get_iteration(jitmod, funcname, "float4[]", "float4[]", "float4[]", None)

      jitfunc(out_buf, in_buf, aux_buf, 0)
      self.compare_buffers(in_buf, in_values)
      self.compare_buffers(aux_buf, aux_values)
      self.compare_buffers(out_buf, expected_out_0)

      jitfunc(out_buf, in_buf, aux_buf, 1)
      self.compare_buffers(in_buf, in_values)
      self.compare_buffers(aux_buf, aux_values)
      self.compare_buffers(out_buf, expected_out_1)

      jitfunc(out_buf, in_buf, aux_buf, 2)
      self.compare_buffers(in_buf, in_values)
      self.compare_buffers(aux_buf, aux_values)
      self.compare_buffers(out_buf, expected_out_2)
    finally:
      if jitmod:
        nanjit.jit_module_destroy(jitmod)

class Tests(BaseNanjitTestFloatFunc):
  def test_001_if_statement(self):
    shaderstr = \
"""float4 process(float4 in, float4 aux)
{
  float4 out = in;
  if (in.s0 > 0.0f)
    {
      out = (float4)(5, out.s1, out.s2, out.s3);
    }
  if (in.s1 > 0.0f)
    {
      out = (float4)(out.s0, 5, out.s2, out.s3);
    }
  return out;
}
"""
    self.doTest(shaderstr,
      "process",
      [0.5, -0.5, 0.0, 0.0],
      [0.0, 0.0, 0.0, 0.0],
      [5.0, -0.5, 0.0, 0.0])
  
  def test_002_if_return(self):
    shaderstr = \
"""float4 process(float4 in, float4 aux)
{
  if (in.s0 > 0.0f)
    {
      return (float4)(5, 5, 5, 5);
    }
  return in;
}
"""
    self.doTest(shaderstr,
      "process",
      [0.5, -0.5, 0.0, 0.0],
      [0.0, 0.0, 0.0, 0.0],
      [5.0, 5.0, 5.0, 5.0])
    
    self.doTest(shaderstr,
      "process",
      [-0.5, -0.5, 0.0, 0.0],
      [0.0, 0.0, 0.0, 0.0],
      [-0.5, -0.5, 0.0, 0.0])

  def test_003_if_nested_pre(self):
    shaderstr = \
"""float4 process(float4 in, float4 aux)
{
  float a = 0;
  if (in.s0 > 0.0f)
    {
      a = a + 1;
      if (in.s1 > 0.0f)
        {
          a = a + 1;
          if (in.s2 > 0.0f)
            {
              a = a + 1;
              if (in.s3 > 0.0f)
                {
                  a = a + 1;
                }
            }
        }
    }
  return (float4)(a, 0, 0, 0);
}
"""

    self.doTest(shaderstr,
      "process",
      [-0.5, -0.5, 0.0, 0.0],
      [0.0, 0.0, 0.0, 0.0],
      [0.0, 0.0, 0.0, 0.0])

    self.doTest(shaderstr,
      "process",
      [0.5, -0.5, 0.0, 0.0],
      [0.0, 0.0, 0.0, 0.0],
      [1.0, 0.0, 0.0, 0.0])
    
    self.doTest(shaderstr,
      "process",
      [0.5, 0.5, -0.5, 0.0],
      [0.0, 0.0, 0.0, 0.0],
      [2.0, 0.0, 0.0, 0.0])
    
    self.doTest(shaderstr,
      "process",
      [0.5, 0.5, 0.5, -0.5],
      [0.0, 0.0, 0.0, 0.0],
      [3.0, 0.0, 0.0, 0.0])
    
    self.doTest(shaderstr,
      "process",
      [0.5, 0.5, 0.5, 0.5],
      [0.0, 0.0, 0.0, 0.0],
      [4.0, 0.0, 0.0, 0.0])

  def test_004_if_nested_post(self):
    shaderstr = \
"""float4 process(float4 in, float4 aux)
{
  float a = 0;
  if (in.s0 > 0.0f)
    {
      if (in.s1 > 0.0f)
        {
          if (in.s2 > 0.0f)
            {
              if (in.s3 > 0.0f)
                {
                  a = a + 1;
                }
              a = a + 1;
            }
          a = a + 1;
        }
      a = a + 1;
    }
  return (float4)(a, 0, 0, 0);
}
"""

    self.doTest(shaderstr,
      "process",
      [-0.5, -0.5, 0.0, 0.0],
      [0.0, 0.0, 0.0, 0.0],
      [0.0, 0.0, 0.0, 0.0])

    self.doTest(shaderstr,
      "process",
      [0.5, -0.5, 0.0, 0.0],
      [0.0, 0.0, 0.0, 0.0],
      [1.0, 0.0, 0.0, 0.0])
    
    self.doTest(shaderstr,
      "process",
      [0.5, 0.5, -0.5, 0.0],
      [0.0, 0.0, 0.0, 0.0],
      [2.0, 0.0, 0.0, 0.0])
    
    self.doTest(shaderstr,
      "process",
      [0.5, 0.5, 0.5, -0.5],
      [0.0, 0.0, 0.0, 0.0],
      [3.0, 0.0, 0.0, 0.0])
    
    self.doTest(shaderstr,
      "process",
      [0.5, 0.5, 0.5, 0.5],
      [0.0, 0.0, 0.0, 0.0],
      [4.0, 0.0, 0.0, 0.0])

  def test_005_if_nested_return(self):
    shaderstr = \
"""float4 process(float4 in, float4 aux)
{
  if (in.s0 > 0.0f)
    {
      if (in.s1 > 0.0f)
        {
          if (in.s2 > 0.0f)
            {
              if (in.s3 > 0.0f)
                {
                  return (float4)(4, 0, 0, 0);
                }
              return (float4)(3, 0, 0, 0);
            }
          return (float4)(2, 0, 0, 0);
        }
      return (float4)(1, 0, 0, 0);
    }
  return (float4)(0, 0, 0, 0);
}
"""

    self.doTest(shaderstr,
      "process",
      [-0.5, -0.5, 0.0, 0.0],
      [0.0, 0.0, 0.0, 0.0],
      [0.0, 0.0, 0.0, 0.0])

    self.doTest(shaderstr,
      "process",
      [0.5, -0.5, 0.0, 0.0],
      [0.0, 0.0, 0.0, 0.0],
      [1.0, 0.0, 0.0, 0.0])
    
    self.doTest(shaderstr,
      "process",
      [0.5, 0.5, -0.5, 0.0],
      [0.0, 0.0, 0.0, 0.0],
      [2.0, 0.0, 0.0, 0.0])
    
    self.doTest(shaderstr,
      "process",
      [0.5, 0.5, 0.5, -0.5],
      [0.0, 0.0, 0.0, 0.0],
      [3.0, 0.0, 0.0, 0.0])
    
    self.doTest(shaderstr,
      "process",
      [0.5, 0.5, 0.5, 0.5],
      [0.0, 0.0, 0.0, 0.0],
      [4.0, 0.0, 0.0, 0.0])

if __name__ == '__main__':
    unittest.main(verbosity=10)