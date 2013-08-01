import os, sys

def create_pkgconfig_file(prefix, project_name, version_string):
  pkg_string = """prefix=%(prefix)s
exec_prefix=${prefix}
libdir=%(libdir)s
includedir=%(includedir)s

Name: %(project_name)s
Description: %(description)s
Version: %(version)s
Cflags: -I%(includedir)s
Libs: -L%(libdir)s -l%(project_name)s"""

  pc_str = pkg_string % {
    "prefix":os.path.normpath(prefix),
    "includedir":os.path.join("${prefix}", "include"),
    "libdir":os.path.join("${prefix}", "lib"),
    "project_name":project_name,
    "description":"A JIT compiler for a subset of OpenCL",
    "version":version_string,
  }

  print (pc_str)

if len(sys.argv) > 1:
  install_prefix = sys.argv[1]
else:
  install_prefix = "/usr"

create_pkgconfig_file(install_prefix, "nanjit", "0.1.0")
