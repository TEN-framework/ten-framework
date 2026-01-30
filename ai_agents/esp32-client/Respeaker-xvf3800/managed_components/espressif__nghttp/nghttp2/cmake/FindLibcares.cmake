# - Try to find libcares
# Once done this will define
#  LIBCARES_FOUND        - System has libcares
#  LIBCARES_INCLUDE_DIRS - The libcares include directories
#  LIBCARES_LIBRARIES    - The libraries needed to use libcares

find_package(PkgConfig QUIET)
pkg_check_modules(PC_LIBCARES QUIET libcares)

find_path(LIBCARES_INCLUDE_DIR
  NAMES ares.h
  HINTS ${PC_LIBCARES_INCLUDE_DIRS}
)
find_library(LIBCARES_LIBRARY
  NAMES cares
  HINTS ${PC_LIBCARES_LIBRARY_DIRS}
)

if(LIBCARES_INCLUDE_DIR)
  file(READ "${LIBCARES_INCLUDE_DIR}/ares_version.h" _ares_version_h)
  string(REGEX REPLACE ".*#define[ \t]+ARES_VERSION_MAJOR[ \t]+([0-9]+).*" "\\1"
    _ares_version_major ${_ares_version_h})
  string(REGEX REPLACE ".*#define[ \t]+ARES_VERSION_MINOR[ \t]+([0-9]+).*" "\\1"
    _ares_version_minor ${_ares_version_h})
  string(REGEX REPLACE ".*#define[ \t]+ARES_VERSION_PATCH[ \t]+([0-9]+).*" "\\1"
    _ares_version_patch ${_ares_version_h})
  set(LIBCARES_VERSION "${_ares_version_major}.${_ares_version_minor}.${_ares_version_patch}")
  unset(_ares_version_patch)
  unset(_ares_version_minor)
  unset(_ares_version_major)
  unset(_ares_version_h)
endif()

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set LIBCARES_FOUND to TRUE
# if all listed variables are TRUE and the requested version matches.
find_package_handle_standard_args(Libcares REQUIRED_VARS
                                  LIBCARES_LIBRARY LIBCARES_INCLUDE_DIR
                                  VERSION_VAR LIBCARES_VERSION)

if(LIBCARES_FOUND)
  set(LIBCARES_LIBRARIES     ${LIBCARES_LIBRARY})
  set(LIBCARES_INCLUDE_DIRS  ${LIBCARES_INCLUDE_DIR})
endif()

mark_as_advanced(LIBCARES_INCLUDE_DIR LIBCARES_LIBRARY)
