# - Try to find jemalloc
# Once done this will define
#  JEMALLOC_FOUND        - System has jemalloc
#  JEMALLOC_INCLUDE_DIRS - The jemalloc include directories
#  JEMALLOC_LIBRARIES    - The libraries needed to use jemalloc

find_package(PkgConfig QUIET)
pkg_check_modules(PC_JEMALLOC QUIET jemalloc)

find_path(JEMALLOC_INCLUDE_DIR
  NAMES jemalloc/jemalloc.h
  HINTS ${PC_JEMALLOC_INCLUDE_DIRS}
)
find_library(JEMALLOC_LIBRARY
  NAMES jemalloc
  HINTS ${PC_JEMALLOC_LIBRARY_DIRS}
)

if(JEMALLOC_INCLUDE_DIR)
  set(_version_regex "^#define[ \t]+JEMALLOC_VERSION[ \t]+\"([^\"]+)\".*")
  file(STRINGS "${JEMALLOC_INCLUDE_DIR}/jemalloc/jemalloc.h"
    JEMALLOC_VERSION REGEX "${_version_regex}")
  string(REGEX REPLACE "${_version_regex}" "\\1"
    JEMALLOC_VERSION "${JEMALLOC_VERSION}")
  unset(_version_regex)
endif()

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set JEMALLOC_FOUND to TRUE
# if all listed variables are TRUE and the requested version matches.
find_package_handle_standard_args(Jemalloc REQUIRED_VARS
                                  JEMALLOC_LIBRARY JEMALLOC_INCLUDE_DIR
                                  VERSION_VAR JEMALLOC_VERSION)

if(JEMALLOC_FOUND)
  set(JEMALLOC_LIBRARIES    ${JEMALLOC_LIBRARY})
  set(JEMALLOC_INCLUDE_DIRS ${JEMALLOC_INCLUDE_DIR})
endif()

mark_as_advanced(JEMALLOC_INCLUDE_DIR JEMALLOC_LIBRARY)
