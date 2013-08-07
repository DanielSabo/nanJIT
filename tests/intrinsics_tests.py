#!/usr/bin/env python

import ctypes, os, sys
import math
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
      print ""
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

class TestClamp(BaseNanjitTestFloatFunc):
  def test_clamp_const(self):
    shaderstr = \
"""float4 process(float4 in, float4 aux)
{
  float4 min = (float4)(0.1f, 0.1f, 0.1f, 0.1f);
  float4 max = (float4)(0.6f, 0.6f, 0.6f, 0.6f);
  return clamp(in, min, max);
}
"""
    self.doTest(shaderstr,
      "process",
      [-0.5, 0.0, 0.5, 1.0],
      [0.0, 0.0, 0.0, 0.0],
      [0.1, 0.1, 0.5, 0.6])
  
  def test_clamp_vars(self):
    shaderstr = \
"""float4 process(float4 min, float4 max)
{
  float4 values = (float4)(-0.5f, 0.0f, 0.5f, 1.0f);
  return clamp(values, min, max);
}
"""
    self.doTest(shaderstr,
      "process",
      [0.1, 0.1, 0.1, 0.1],
      [0.6, 0.6, 0.6, 0.6],
      [0.1, 0.1, 0.5, 0.6])

class TestMin(BaseNanjitTestFloatFunc):
  def test_min_vars(self):
    shaderstr = \
"""float4 process(float4 a, float4 b)
{
  return min(a, b);
}
"""
    self.doTest(shaderstr,
      "process",
      [-0.5, 0.0, 0.5, 1.0],
      [0.1, 0.1, 0.1, 0.1],
      [-0.5, 0.0, 0.1, 0.1])

class TestMax(BaseNanjitTestFloatFunc):
  def test_max_vars(self):
    shaderstr = \
"""float4 process(float4 a, float4 b)
{
  return max(a, b);
}
"""
    self.doTest(shaderstr,
      "process",
      [-0.5, 0.0, 0.5, 1.0],
      [0.1, 0.1, 0.1, 0.1],
      [0.1, 0.1, 0.5, 1.0])

class TestSqrt(BaseNanjitTestFloatFunc):
  def test_sqrt_vars(self):
    shaderstr = \
"""float4 process(float4 a, float4 b)
{
  return sqrt(a);
}
"""

    a = [2.0, 0.0, 10.0, 625.0]
    out = map(math.sqrt, a)
    self.doTest(shaderstr,
      "process",
      a,
      [0.0, 0.0, 0.0, 0.0],
      out)

if __name__ == '__main__':
    unittest.main(verbosity=10)