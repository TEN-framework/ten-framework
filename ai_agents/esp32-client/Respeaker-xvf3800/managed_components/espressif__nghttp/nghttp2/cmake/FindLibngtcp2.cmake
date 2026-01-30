# - Try to find libngtcp2
# Once done this will define
#  LIBNGTCP2_FOUND        - System has libngtcp2
#  LIBNGTCP2_INCLUDE_DIRS - The libngtcp2 include directories
#  LIBNGTCP2_LIBRARIES    - The libraries needed to use libngtcp2

find_package(PkgConfig QUIET)
pkg_check_modules(PC_LIBNGTCP2 QUIET libngtcp2)

find_path(LIBNGTCP2_INCLUDE_DIR
  NAMES ngtcp2/ngtcp2.h
  HINTS ${PC_LIBNGTCP2_INCLUDE_DIRS}
)
find_library(LIBNGTCP2_LIBRARY
  NAMES ngtcp2
  HINTS ${PC_LIBNGTCP2_LIBRARY_DIRS}
)

if(LIBNGTCP2_INCLUDE_DIR)
  set(_version_regex "^#define[ \t]+NGTCP2_VERSION[ \t]+\"([^\"]+)\".*")
  file(STRINGS "${LIBNGTCP2_INCLUDE_DIR}/ngtcp2/version.h"
    LIBNGTCP2_VERSION REGEX "${_version_regex}")
  string(REGEX REPLACE "${_version_regex}" "\\1"
    LIBNGTCP2_VERSION "${LIBNGTCP2_VERSION}")
  unset(_version_regex)
endif()

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set LIBNGTCP2_FOUND
# to TRUE if all listed variables are TRUE and the requested version
# matches.
find_package_handle_standard_args(Libngtcp2 REQUIRED_VARS
                                  LIBNGTCP2_LIBRARY LIBNGTCP2_INCLUDE_DIR
                                  VERSION_VAR LIBNGTCP2_VERSION)

if(LIBNGTCP2_FOUND)
  set(LIBNGTCP2_LIBRARIES     ${LIBNGTCP2_LIBRARY})
  set(LIBNGTCP2_INCLUDE_DIRS  ${LIBNGTCP2_INCLUDE_DIR})
endif()

mark_as_advanced(LIBNGTCP2_INCLUDE_DIR LIBNGTCP2_LIBRARY)
