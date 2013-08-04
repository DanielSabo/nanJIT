nanJIT
======
nanJIT is a LLVM based runtime compiler for a (small) subset of OpenCL.
Functions are compiled into simple C function pointers that can be called
with zero additional overhead compared to normal C functions.

Installation
============
Compile with `scons` install with `scons install`, to install in a prefix
use `scons install --prefix=/your/path/here`.

If your distribution provides a shared library for LLVM you probably want
run scons with the `--llvm-shared` option. Options are sticky and can be
changed when running `scons` or `scons install`, scons will figure out
what they affect and rebuild as needed.

Examples
============
A nanJIT function:

    float4 process(float4 in, float4 aux)
    {
      float4 one = (float4)(1.0f, 1.0f, 1.0f, 1.0f);
      float4 aaaa = aux.s3333;
      return aux + in * (one - aaaa);
    }

To call it you would use code like the following:

    #include "nanjit/jitmodule.h"

    ...

    typedef void (*JitProcessFunction)(float *output, const float *input, const float *aux, uint32_t count);
    JitModule *jm;
    JitProcessFunction aligned_function;
    JitProcessFunction unaligned_function;

    ...

    static const char *shader_src = \
    "float4 process(float4 in, float4 aux)\
    {\
      float4 one = (float4)(1.0f, 1.0f, 1.0f, 1.0f);\
      float4 aaaa = aux.s3333;\
      float4 out = aux + in * (one - aaaa);\
      return out;\
    }";

    jm = jit_module_for_src(shader_src, 0);

    aligned_function = jit_module_get_iteration(jm, "process", "aligned float4[]", "aligned float4[]", "aligned float4[]", NULL);
    unaligned_function = jit_module_get_iteration(jm, "process", "float4[]", "float4[]", "float4[]", NULL);

    ...

    unaligned_function(out_buf, in_buf, aux_buf, n_pixels);


The `aligned` means the data will be aligned to the size of the type,
16 bytes in the case of float4. This allows LLVM to use faster SSE
loads on most processors. `float4[]` means the data is an array who's
length will be specified by the last parameter of the generated function,
supplied as `n_pixels` here. Other valid types would be `float4` to specify
a single value passed on the stack, or `*float4` to specify a single value
passed by reference.

Requirements
============
   - SCons (2.1.0 or newer recommended)
   - Flex (2.5.35)
   - Bison (2.5 recommended, up to 3.0 should also work)
   - LLVM development libraries (version 3.2 or 3.3)
