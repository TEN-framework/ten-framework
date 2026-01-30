# - Try to find libbpf
# Once done this will define
#  LIBBPF_FOUND        - System has libbpf
#  LIBBPF_INCLUDE_DIRS - The libbpf include directories
#  LIBBPF_LIBRARIES    - The libraries needed to use libbpf

find_package(PkgConfig QUIET)
pkg_check_modules(PC_LIBBPF QUIET libbpf)

find_path(LIBBPF_INCLUDE_DIR
  NAMES bpf/bpf.h
  HINTS ${PC_LIBBPF_INCLUDE_DIRS}
)
find_library(LIBBPF_LIBRARY
  NAMES bpf
  HINTS ${PC_LIBBPF_LIBRARY_DIRS}
)

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set LIBBPF_FOUND
# to TRUE if all listed variables are TRUE and the requested version
# matches.
find_package_handle_standard_args(Libbpf REQUIRED_VARS
                                  LIBBPF_LIBRARY LIBBPF_INCLUDE_DIR
                                  VERSION_VAR LIBBPF_VERSION)

if(LIBBPF_FOUND)
  set(LIBBPF_LIBRARIES     ${LIBBPF_LIBRARY})
  set(LIBBPF_INCLUDE_DIRS  ${LIBBPF_INCLUDE_DIR})
endif()

mark_as_advanced(LIBBPF_INCLUDE_DIR LIBBPF_LIBRARY)
