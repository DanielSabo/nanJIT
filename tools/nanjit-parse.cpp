#include <cstdio>
#include <iostream>
#include <fstream>
#include <sstream>
#include <memory>
#include <stdint.h>
using namespace std;

#include "jitmodule.hpp"

typedef void (*PixelBlendFunction)(float *in, float *aux, float *out);
typedef void (*PixelBlendFunctionIter)(float *in, float *aux, float *out, uint32_t count);

int main(int argc, char **argv) {
  std::stringstream sourcecode_stream;

  if (argc == 2)
    {
      int length = 1024;
      char *buffer = new char[length];
      
      if (std::string("-") == argv[1])
        {
          while (cin.read(buffer, length))
            sourcecode_stream.write(buffer, length);
          sourcecode_stream.write(buffer, cin.gcount());
        }
      else
        {
          int readlen = 0;
          std::ifstream read_file_stream(argv[1], std::ifstream::binary);
          if (!read_file_stream.good())
            {
              cout << "Error reading \"" << argv[1] << "\"" << endl;
              delete[] buffer;
              return 1;
            }

          while (read_file_stream.read(buffer, length))
            sourcecode_stream.write(buffer, length);
          sourcecode_stream.write(buffer, read_file_stream.gcount());
        }
      
      delete[] buffer;
    }
  else
    {
      cout << "Need a source file." << endl;
      return 1;
    }

  std::string sourcecode = sourcecode_stream.str();
  auto_ptr<JitModule> jitmod;

  try
    {
      jitmod.reset(new JitModule(sourcecode.c_str(), JIT_MODULE_DEBUG_AST | JIT_MODULE_DEBUG_LLVM));
    }
  catch (JitModuleException &e)
    {
      cout << e.what() << endl;
      return 1;
    }
    
  {
    PixelBlendFunctionIter FPtrIter = (PixelBlendFunctionIter)jitmod->getIteration("process", "float4[]", "float4[]", "float4[]", NULL);

    printf ("JIT Function is fallback: %s\n", jitmod->isFallbackFunction((void *)FPtrIter) ? "True" : "False");

    float in[8] = { 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5 };
    float aux[8] = { 0.0, 0.5, 0.0, 0.5, 0.0, 0.5, 0.0, 0.5 };
    float out[8] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
    
    FPtrIter(out, in, aux, 1);
    
    printf ("%.4f %.4f %.4f %.4f ", out[0], out[1], out[2], out[3]);
    printf ("%.4f %.4f %.4f %.4f\n", out[4], out[5], out[6], out[7]);
    
    FPtrIter(out, in, aux, 2);
    
    printf ("%.4f %.4f %.4f %.4f ", out[0], out[1], out[2], out[3]);
    printf ("%.4f %.4f %.4f %.4f\n", out[4], out[5], out[6], out[7]);
  }

  return 0;
}
