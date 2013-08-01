import sys, os
import ctypes
import re

if sys.platform == "darwin":
  _clibpath = os.path.join(os.path.dirname(__file__), "..", "libnanjit.dylib")
elif sys.platform == "win32":
  _clibpath = os.path.join(os.path.dirname(__file__), "..", "nanjit.dll")
else:
  _clibpath = os.path.join(os.path.dirname(__file__), "..", "libnanjit.so")

_libnanjit = ctypes.CDLL(_clibpath)

_libnanjit.jit_module_for_src.restype = ctypes.c_void_p
_libnanjit.jit_module_for_src.argtypes = [ctypes.c_char_p, ctypes.c_uint]

_libnanjit.jit_module_get_iteration.restype = ctypes.c_void_p

_libnanjit.jit_module_is_fallback_function.restype = ctypes.c_void_p
_libnanjit.jit_module_is_fallback_function.argtypes = [ctypes.c_void_p, ctypes.c_void_p]

_libnanjit.jit_module_destroy.restype = None
_libnanjit.jit_module_destroy.argtypes = [ctypes.c_void_p]


def parse_argtype(argtype):
  splitarg = re.match("\A(\*)?([a-z]+)\d*(\[\])?\Z", argtype).groups()
  if splitarg[1] is None:
    raise Exception("Couldn't parse argument type")

  type_map = {
    "float": ctypes.c_float,
    "int": ctypes.c_int32,
    "uint": ctypes.c_uint32,
    "short": ctypes.c_int16,
    "ushort": ctypes.c_uint16,
    "char": ctypes.c_int8,
    "uchar": ctypes.c_uint8,
  }

  base_type = type_map[splitarg[1]]

  if splitarg[0] or splitarg[2]:
    return ctypes.POINTER(base_type)
  return base_type

def _call_get_iteration(jm, name, return_type, *args):
  if args[-1] is not None:
    raise Exception("Args list must end in None")

  args = [name, return_type] + list(args)

  # Convert the arguments to ctypes pointers, we need to do this explicitly because we can't set
  # the argument types for a varargs function.
  arg_chars = [ctypes.c_char_p(n) for n in args]

  # Parse the nanjit typestrings into ctype types for the returned function's prototype
  arg_ctypes = [None] + [parse_argtype(n) for n in args[1:-1]] + [ctypes.c_int32]

  # Build a ctype function prototype that matches the expected arguments of the iteration
  proto = ctypes.CFUNCTYPE(*arg_ctypes)

  # Get the jit function
  funcptr = _libnanjit.jit_module_get_iteration(ctypes.c_void_p(jm), *arg_chars)

  # Wrap the function pointer in the prototype
  return proto(funcptr)

jit_module_for_src = _libnanjit.jit_module_for_src
jit_module_get_iteration = _call_get_iteration
jit_module_is_fallback_function = _libnanjit.jit_module_is_fallback_function
jit_module_destroy = _libnanjit.jit_module_destroy
