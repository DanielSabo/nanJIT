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
      jitfunc = nanjit.jit_module_get_range_iteration(jitmod, funcname, "float4[]", "float4[]", "float4[]", None)

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

class TestRange(BaseNanjitTestFloatFunc):
  def test_range(self):
    shaderstr = \
"""float4 process(float4 in, float4 aux, int __x, int __y)
{
  return (float4)(__x, __x, __y, __y);
}
"""
    in_values  = [0.0, 0.0, 0.0, 0.0] * 3
    aux_values = [0.0, 0.0, 0.0, 0.0] * 3
    out_values = [0.0, 0.0, 0.0, 0.0] * 3
    expected_out = []

    in_buf  = buffer_from_list(ctypes.c_float, in_values)
    aux_buf = buffer_from_list(ctypes.c_float, aux_values)
    out_buf = buffer_from_list(ctypes.c_float, out_values)

    expected_out_0 = [0.0, 0.0, 0.0, 0.0] + \
                     [0.0, 0.0, 0.0, 0.0] + \
                     [0.0, 0.0, 0.0, 0.0]
    
    expected_out_1 = [0.0, 0.0, 8.0, 8.0] + \
                     [0.0, 0.0, 0.0, 0.0] + \
                     [0.0, 0.0, 0.0, 0.0]
    
    expected_out_2 = [5.0, 5.0, 2.0, 2.0] + \
                     [6.0, 6.0, 2.0, 2.0] + \
                     [7.0, 7.0, 2.0, 2.0]

    jitmod = None
    try:
      print ""
      jitmod = nanjit.jit_module_for_src(shaderstr, 0)
      jitfunc = nanjit.jit_module_get_range_iteration(jitmod, "process", "float4[]", "float4[]", "float4[]", "int", None)

      jitfunc(out_buf, in_buf, aux_buf, 4, 0, 0)
      self.compare_buffers(in_buf, in_values)
      self.compare_buffers(aux_buf, aux_values)
      self.compare_buffers(out_buf, expected_out_0)

      jitfunc(out_buf, in_buf, aux_buf, 8, 0, 1)
      self.compare_buffers(in_buf, in_values)
      self.compare_buffers(aux_buf, aux_values)
      self.compare_buffers(out_buf, expected_out_1)
      
      jitfunc(out_buf, in_buf, aux_buf, 2, 5, 8)
      self.compare_buffers(in_buf, in_values)
      self.compare_buffers(aux_buf, aux_values)
      self.compare_buffers(out_buf, expected_out_2)
    finally:
      if jitmod:
        nanjit.jit_module_destroy(jitmod)


if __name__ == '__main__':
    unittest.main(verbosity=10)