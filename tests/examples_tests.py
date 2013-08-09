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

class ExampleTests(BaseNanjitTestFloatFunc):
  def test_example_001(self):
    shaderstr = \
"""float4 process(float4 in, float4 aux)
{
  float4 one = (float4)(1.0f, 1.0f, 1.0f, 1.0f);
  float4 out = aux + in * (one - aux.s3333);
         out = shuffle2(out, in, (uint4)(0, 1, 2, 7));
  return out;
}
"""
    self.doTest(shaderstr,
      "process",
      [0.5, 0.5, 0.5, 0.5],
      [0.0, 0.5, 0.0, 0.5],
      [0.25, 0.75, 0.25, 0.5])

  def test_example_002(self):
    shaderstr = \
"""/* multi-line
 * comment before
 */

float4 process (float4 in, float4 aux)
{
  // One line comment
  float n = (5.5f + 3.4f + 0.2f - 5f + 1.0f) * (in.s0 / 0.5f);
  return (float4)(n, -n, /* inline * block * / comment */ n, n);
}

"""
    self.doTest(shaderstr,
      "process",
      [0.5, 0.5, 0.5, 0.5],
      [0.0, 0.5, 0.0, 0.5],
      [5.0999994278, -5.0999994278, 5.0999994278, 5.0999994278])

  def test_example_003(self):
    shaderstr = \
"""
float4 process(float4 in, float4 aux)
{
  float4 one = (float4)(1.0f, 1.0f, 1.0f, 1.0f);
  float4 aaaa = aux.s3333;
  if (aaaa.s0 == 0.0f)
  {
    return in;
  }
  float4 out = aux + in * (one - aaaa);
         out = shuffle2(out, in, (int4)(0, 1, 2, 7));
  return out;
}
"""
    self.doTest(shaderstr,
      "process",
      [0.5, 0.5, 0.5, 0.5],
      [0.0, 0.5, 0.0, 0.5],
      [0.25, 0.75, 0.25, 0.5])
    
    self.doTest(shaderstr,
      "process",
      [0.5, 0.5, 0.5, 0.5],
      [0.0, 0.5, 0.0, 0.0],
      [0.5, 0.5, 0.5, 0.5])

  def test_example_004(self):
    shaderstr = \
"""
float4 process(float4 in, float4 aux)
{
  float4 three = (float4)(3.0f, 3.0f, 3.0f, 3.0f);
  return shuffle2(in, three, (int4)(0, 1, 2, 7) );
}
"""
    self.doTest(shaderstr,
      "process",
      [0.5, 0.5, 0.5, 0.5],
      [0.0, 0.5, 0.0, 0.5],
      [0.5, 0.5, 0.5, 3.0])

  def test_example_006(self):
    shaderstr = \
"""
/* SVG Overlay mode */
float4 process(float4 in, float4 aux)
{
  float4 cB = in;
  float4 aB = in.s3333;
  float4 cA = aux;
  float4 aA = aux.s3333;
  float4 aD = aA + aB - aA * aB;
  float4 zero = (float4)(0.0f, 0.0f, 0.0f, 0.0f);
  float4 one = (float4)(1.0f, 1.0f, 1.0f, 1.0f);
  float4 two = (float4)(2.0f, 2.0f, 2.0f, 2.0f);

  float4 common_path = cA * (one - aB) + cB * (one - aA);

  float4 out_path_if = two * cA * cB + common_path;
  float4 out_path_else = aA * aB - two * (aB - cB) * (aA - cA) + common_path;

  bool4  select_mask = (two * cB) > aB;
  float4 blended_paths = select (out_path_if, out_path_else, select_mask);
         blended_paths = shuffle2 (blended_paths, aD, (int4)(0, 1, 2, 7));

  return clamp(blended_paths, zero, aD);
}
"""
    self.doTest(shaderstr,
      "process",
      [0.5, 0.5, 0.5, 0.5],
      [0.0, 0.5, 0.0, 0.5],
      [0.2500, 0.7500, 0.2500, 0.7500])
    
    self.doTest(shaderstr,
      "process",
      [0.25, 0.25, 0.25, 0.5],
      [0.0, 0.5, 0.0, 0.5],
      [0.125, 0.625, 0.125, 0.75])

    self.doTest(shaderstr,
      "process",
      [0.25, 0.25, 0.25, 0.5],
      [0.0, 0.25, 0.0, 0.5],
      [0.125, 0.375, 0.125, 0.75])

  def test_example_007(self):
    shaderstr = \
"""
float4 function_one(float4 in, float4 aux)
{
  return (float4)(10.0f, 10.0f, 10.0f, 10.0f);
}

float4 function_two(float4 in, float4 aux)
{
  return (float4)(20.0f, 20.0f, 20.0f, 20.0f);
}
"""
    self.doTest(shaderstr,
      "function_one",
      [0.5, 0.5, 0.5, 0.5],
      [0.0, 0.5, 0.0, 0.5],
      [10.0, 10.0, 10.0, 10.0])

    self.doTest(shaderstr,
      "function_two",
      [0.5, 0.5, 0.5, 0.5],
      [0.0, 0.5, 0.0, 0.5],
      [20.0, 20.0, 20.0, 20.0])

  def test_example_008_casts(self):
    shaderstr = \
"""
float4 process(float4 in, float4 aux)
{
  float4 egFloat4 = (float4)(1.0f, 1.0f, 1.0f, 1.0f);
  uint4 egUInt4 = (uint4)(1, 1, 1, 1);
  int4 egInt4 = (int4)(1, 1, 1, 1);
  ushort4 egUShort4 = (ushort4)(1, 1, 1, 1);
  short4 egShort4 = (short4)(1, 1, 1, 1);

  float egFloat = 5.0f;
  uint egUInt = 4;
  int egInt = 3;
  ushort egUShort = 1;
  short egShort = 2;

  float4 ffff = (egUInt4 + egInt4) + egFloat4;
  uint4 uiiii = ffff;
  int4 iiii = ffff;
  iiii = uiiii;
  uiiii = iiii;

  int4 shufi = (int4)(egUInt4.s0, egInt4.s1, egUShort4.s2, egShort4.s3);

  int   ia = egInt + egUInt;
        ia = ia + egFloat;
  uint  ib = egInt + egUInt;
        ib = ib + egFloat;
  float x = egFloat + egInt;
  float y = egFloat + egUInt;
  float u = ia;
  float v = ib;
  return (float4)(x, y, u, v) + (egUInt4 + egInt4 + egUShort4 + egShort4) + shufi;
}

"""
    self.doTest(shaderstr,
      "process",
      [0.5, 0.5, 0.5, 0.5],
      [0.0, 0.5, 0.0, 0.5],
      [13.0000, 14.0000, 17.0000, 17.0000])

  def test_example_009_signs(self):
    shaderstr = \
"""
float4 process(float4 in, float4 aux)
{
  uint4 egUInt4 = (uint4)(4, 4, 4, 4);
  int4 egInt4 = (int4)(-3, -3, -3, -3);
  ushort4 egUShort4 = (ushort4)(1, 1, 1, 1);
  short4 egShort4 = (short4)(-2, -2, -2, -2);

  uint egUInt = 4;
  int egInt = -3;
  ushort egUShort = 1;
  short egShort = -2;

  int x = egUShort + egShort;
  int y = egUInt + egInt;

  int4 u = egUShort4 + egShort4;
  int4 v = egUInt4 + egInt4;

  return (float4)(x, y, u.s0, v.s0);
}

"""
    self.doTest(shaderstr,
      "process",
      [0.5, 0.5, 0.5, 0.5],
      [0.0, 0.5, 0.0, 0.5],
      [-1.0000, 1.0000, -1.0000, 1.0000])

  def test_example_010_assignment_casts(self):
    shaderstr = \
"""
float4 process(float4 in, float4 aux)
{
  float4 a = (uint4)(4, 4, 4, 4);
  float4 b = (int4)(-3, -3, -3, -3);
  float4 c = (ushort4)(1, 1, 1, 1);
  float4 d = (short4)(-2, -2, -2, -2);

  return (float4)(a.s0, b.s0, c.s0, d.s0);
}

"""
    self.doTest(shaderstr,
      "process",
      [0.5, 0.5, 0.5, 0.5],
      [0.0, 0.5, 0.0, 0.5],
      [4.0, -3.0, 1.0, -2.0])

  def test_example_011_assignment_casts2(self):
    shaderstr = \
"""
float4 process(float4 in, float4 aux)
{
  float4 a = (float4)(0, 0, 0, 0);
  float4 b = (float4)(0, 0, 0, 0);
  float4 c = (float4)(0, 0, 0, 0);
  float4 d = (float4)(0, 0, 0, 0);
  a = (uint4)(4, 4, 4, 4);
  b = (int4)(-3, -3, -3, -3);
  c = (ushort4)(1, 1, 1, 1);
  d = (short4)(-2, -2, -2, -2);

  return (float4)(a.s0, b.s0, c.s0, d.s0);
}

"""
    self.doTest(shaderstr,
      "process",
      [0.5, 0.5, 0.5, 0.5],
      [0.0, 0.5, 0.0, 0.5],
      [4.0, -3.0, 1.0, -2.0])


if __name__ == '__main__':
    unittest.main(verbosity=10)