nanJIT
======
nanJIT is a LLVM based runtime compiler for a subset of OpenCL. Functions are compiled into simple C function pointers that can be called with zero additional overhead compared to normal C functions.

Installation
============
Compile with `scons` install with `scons install`, to install in a prefix use `scons install --prefix=/your/path/here`. If your distribution provides a shared library for LLVM you probably want run scons with the `--llvm-shared` option. Run `scons --help` to see all options. All options are sticky and can be changed when running `scons` or `scons install`, scons will figure out what they affect and rebuild as needed.

Requirements
============
SCons (2.1.0 or newer recomended)
LLVM development libraries (version 3.2 or 3.3)