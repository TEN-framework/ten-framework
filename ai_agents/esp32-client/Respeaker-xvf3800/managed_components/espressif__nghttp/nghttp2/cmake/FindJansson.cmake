# - Try to find jansson
# Once done this will define
#  JANSSON_FOUND        - System has jansson
#  JANSSON_INCLUDE_DIRS - The jansson include directories
#  JANSSON_LIBRARIES    - The libraries needed to use jansson

find_package(PkgConfig QUIET)
pkg_check_modules(PC_JANSSON QUIET jansson)

find_path(JANSSON_INCLUDE_DIR
  NAMES jansson.h
  HINTS ${PC_JANSSON_INCLUDE_DIRS}
)
find_library(JANSSON_LIBRARY
  NAMES jansson
  HINTS ${PC_JANSSON_LIBRARY_DIRS}
)

if(JANSSON_INCLUDE_DIR)
  set(_version_regex "^#define[ \t]+JANSSON_VERSION[ \t]+\"([^\"]+)\".*")
  file(STRINGS "${JANSSON_INCLUDE_DIR}/jansson.h"
    JANSSON_VERSION REGEX "${_version_regex}")
  string(REGEX REPLACE "${_version_regex}" "\\1"
    JANSSON_VERSION "${JANSSON_VERSION}")
  unset(_version_regex)
endif()

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set JANSSON_FOUND to TRUE
# if all listed variables are TRUE and the requested version matches.
find_package_handle_standard_args(Jansson REQUIRED_VARS
                                  JANSSON_LIBRARY JANSSON_INCLUDE_DIR
                                  VERSION_VAR JANSSON_VERSION)

if(JANSSON_FOUND)
  set(JANSSON_LIBRARIES     ${JANSSON_LIBRARY})
  set(JANSSON_INCLUDE_DIRS  ${JANSSON_INCLUDE_DIR})
endif()

mark_as_advanced(JANSSON_INCLUDE_DIR JANSSON_LIBRARY)
