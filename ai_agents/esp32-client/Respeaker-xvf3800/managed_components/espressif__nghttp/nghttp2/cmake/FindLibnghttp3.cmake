# - Try to find libnghttp3
# Once done this will define
#  LIBNGHTTP3_FOUND        - System has libnghttp3
#  LIBNGHTTP3_INCLUDE_DIRS - The libnghttp3 include directories
#  LIBNGHTTP3_LIBRARIES    - The libraries needed to use libnghttp3

find_package(PkgConfig QUIET)
pkg_check_modules(PC_LIBNGHTTP3 QUIET libnghttp3)

find_path(LIBNGHTTP3_INCLUDE_DIR
  NAMES nghttp3/nghttp3.h
  HINTS ${PC_LIBNGHTTP3_INCLUDE_DIRS}
)
find_library(LIBNGHTTP3_LIBRARY
  NAMES nghttp3
  HINTS ${PC_LIBNGHTTP3_LIBRARY_DIRS}
)

if(LIBNGHTTP3_INCLUDE_DIR)
  set(_version_regex "^#define[ \t]+NGHTTP3_VERSION[ \t]+\"([^\"]+)\".*")
  file(STRINGS "${LIBNGHTTP3_INCLUDE_DIR}/nghttp3/version.h"
    LIBNGHTTP3_VERSION REGEX "${_version_regex}")
  string(REGEX REPLACE "${_version_regex}" "\\1"
    LIBNGHTTP3_VERSION "${LIBNGHTTP3_VERSION}")
  unset(_version_regex)
endif()

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set LIBNGHTTP3_FOUND
# to TRUE if all listed variables are TRUE and the requested version
# matches.
find_package_handle_standard_args(Libnghttp3 REQUIRED_VARS
                                  LIBNGHTTP3_LIBRARY LIBNGHTTP3_INCLUDE_DIR
                                  VERSION_VAR LIBNGHTTP3_VERSION)

if(LIBNGHTTP3_FOUND)
  set(LIBNGHTTP3_LIBRARIES     ${LIBNGHTTP3_LIBRARY})
  set(LIBNGHTTP3_INCLUDE_DIRS  ${LIBNGHTTP3_INCLUDE_DIR})
endif()

mark_as_advanced(LIBNGHTTP3_INCLUDE_DIR LIBNGHTTP3_LIBRARY)
