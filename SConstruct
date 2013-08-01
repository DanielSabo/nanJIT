import os, sys, subprocess
import distutils.version
import SCons.Errors

# print ARGUMENTS
# print BUILD_TARGETS
# print COMMAND_LINE_TARGETS
# print DEFAULT_TARGETS

AddOption("--verbose",
          help="print all build commands",
          action="store_true")

AddOption("--prefix",
          dest="prefix",
          type="string",
          nargs=1,
          action="store",
          metavar="DIR",
          help="installation prefix")

AddOption("--clang-build",
          dest="use_clang",
          help="compile with clang/clang++ instead of the default compiler",
          action="store_true")
AddOption("--no-clang-build",
          dest="use_clang",
          action="store_false")

AddOption("--with-llvm-config",
          dest="llvm_config",
          type="string",
          nargs=1,
          action="store",
          metavar="PATH",
          help="path to the llvm-config binary")

AddOption("--llvm-shared",
          dest="llvm_shared",
          help="link to LLVM dynamically",
          action="store_true")
AddOption("--no-llvm-shared",
          dest="llvm_shared",
          help="link to LLVM statically",
          action="store_false")

# ENV=os.environ will clone the current enviroment variables (like
# PKG_CONFIG_PATH), buy default scons doesn't propagate this.
env = Environment(ENV=os.environ)

# Enumerate the persistent configuration variables
CONFIG_FILE_NAME = "scons.conf"
variables = Variables(CONFIG_FILE_NAME)
variables.AddVariables(
    (PathVariable("PREFIX", "installation prefix", "/usr")),
    (BoolVariable("USE_CLANG", "always build with clang", False)),
    ("LLVM_CONFIG", "llvm-config binary to use", "llvm-config"),
    (BoolVariable("LLVM_SHARED", "link to LLVM dynamically", False)),
    ("CCFLAGS", "C/C++ compiler arguments", ["-g", "-O2"]),
  )

# Load default values into env
variables.Update(env)

# Parse any changes the user made to the variables
if GetOption("use_clang") is not None:
  if GetOption("use_clang"):
    env["USE_CLANG"] = True
  else:
    env["USE_CLANG"] = False

if GetOption("llvm_config") is not None:
  env["LLVM_CONFIG"] = GetOption("llvm_config")

if GetOption("llvm_shared") is not None:
  if GetOption("llvm_shared"):
    env["LLVM_SHARED"] = True
  else:
    env["LLVM_SHARED"] = False

if GetOption("prefix"):
  env["PREFIX"] = GetOption("prefix")

if env["USE_CLANG"]:
  env["CC"] = "clang"
  env["CXX"] = "clang++"

# Save the updated config before we do anything else to the environment
variables.Save(CONFIG_FILE_NAME, env)

if not GetOption("verbose"):
  if sys.stdout.isatty() and "TERM" in os.environ:
    COLOR_BOLD  = "\033[01m"
    COLOR_RESET = "\033[00m"
    env.Replace(CCCOMSTR     = COLOR_BOLD + "CC" + COLOR_RESET + "   $SOURCES ==> $TARGET",
                SHCCCOMSTR   = COLOR_BOLD + "CC" + COLOR_RESET + "   $SOURCES ==> $TARGET",
                CXXCOMSTR    = COLOR_BOLD + "CXX" + COLOR_RESET + "  $SOURCES ==> $TARGET",
                SHCXXCOMSTR  = COLOR_BOLD + "CXX" + COLOR_RESET + "  $SOURCES ==> $TARGET",
                LINKCOMSTR   = COLOR_BOLD + "CCLD" + COLOR_RESET + " $SOURCES ==> $TARGET",
                SHLINKCOMSTR = COLOR_BOLD + "CCLD" + COLOR_RESET + " $SOURCES ==> $TARGET",
                YACCCOMSTR   = COLOR_BOLD + "YACC" + COLOR_RESET + " $SOURCES ==> $TARGET",
                LEXCOMSTR    = COLOR_BOLD + "LEX" + COLOR_RESET + "  $SOURCES ==> $TARGET")
  else:
    env.Replace(CCCOMSTR     = "CC   $SOURCES ==> $TARGET",
                SHCCCOMSTR   = "CC   $SOURCES ==> $TARGET",
                CXXCOMSTR    = "CXX  $SOURCES ==> $TARGET",
                SHCXXCOMSTR  = "CXX  $SOURCES ==> $TARGET",
                LINKCOMSTR   = "CCLD $SOURCES ==> $TARGET",
                SHLINKCOMSTR = "CCLD $SOURCES ==> $TARGET",
                YACCCOMSTR   = "YACC $SOURCES ==> $TARGET",
                LEXCOMSTR    = "LEX  $SOURCES ==> $TARGET")

for vk in variables.keys() + ["CC", "CXX"]:
  print "%s:" % vk, env[vk]

llvm_required_version = distutils.version.LooseVersion("3.2")
llvm_version = subprocess.Popen([env["LLVM_CONFIG"], "--version"], stdout=subprocess.PIPE).communicate()[0].rstrip()
llvm_version = distutils.version.LooseVersion(llvm_version)
if not (llvm_version >= llvm_required_version):
  raise SCons.Errors.EnvironmentError("Need LLVM " + str(llvm_required_version) + " or newer")

parser_env = env.Clone()

parser_env.CXXFile("lexer.cpp", "lexer.ll")
bison_outputs = parser_env.CXXFile("parser.tab.cpp", "parser.yy", YACCFLAGS="-d")
Clean(bison_outputs, "location.hh")
Clean(bison_outputs, "position.hh")
Clean(bison_outputs, "stack.hh")
parser_env.Append(LIBS = ["l"]) # Link to flex

llvm_env = parser_env.Clone()

def filter_llvm_flags(env, flags):
  # Remove some problematic flags added by llvm-config
  flags = env.ParseFlags(flags)
  flags["CCFLAGS"] = [i for i in flags["CCFLAGS"] if not i in ["-O3", "-fno-exceptions"]]
  env.MergeFlags(flags)

llvm_env.ParseConfig(env["LLVM_CONFIG"] + " --cxxflags", function=filter_llvm_flags)
if env["LLVM_SHARED"]:
  llvm_env.Append(LIBS = ["LLVM-" + str(llvm_version)] )
else:
  llvm_env.ParseConfig(env["LLVM_CONFIG"] + " --libs engine ipo")
# This must come after "--libs" or GCC will get confused
llvm_env.ParseConfig(env["LLVM_CONFIG"] + " --ldflags")

parser_objects = env.SharedObject("lexer.cpp") + env.SharedObject("parser.tab.cpp")
nanjit_lib_inputs = ["util.cpp", "ast.cpp", "jitparser.cpp", "jitmodule.cpp", "typeinfo.cpp", "varg.cpp"] + parser_objects
nanjit_lib = llvm_env.SharedLibrary("nanjit", nanjit_lib_inputs)

env.Command("nanjit.pc", "nanjit.pc.py", "python $SOURCE $PREFIX > $TARGET")

SConscript(["tests/SConscript"], exports=["env", "llvm_env"])
SConscript(["tools/SConscript"], exports=["env", "llvm_env"])

# Install targets
install_prefix = env["PREFIX"]

install_headers = ["jitmodule.h"]

install_env = env.Clone()
install_lib = install_env.Install(os.path.join(install_prefix, "lib"), nanjit_lib)
install_env.Install(os.path.join(install_prefix, "include", "nanjit"), install_headers)
install_env.Install(os.path.join(install_prefix, "lib", "pkgconfig"), "nanjit.pc")

# Fix library link paths on OSX
if sys.platform == "darwin":
  install_env.AddPostAction(install_lib, "install_name_tool -id $TARGET $TARGET")


env.Alias('install', install_prefix)
