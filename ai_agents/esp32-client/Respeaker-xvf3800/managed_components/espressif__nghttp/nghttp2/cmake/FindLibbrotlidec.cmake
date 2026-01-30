# - Try to find libbrotlidec
# Once done this will define
#  LIBBROTLIDEC_FOUND        - System has libbrotlidec
#  LIBBROTLIDEC_INCLUDE_DIRS - The libbrotlidec include directories
#  LIBBROTLIDEC_LIBRARIES    - The libraries needed to use libbrotlidec

find_package(PkgConfig QUIET)
pkg_check_modules(PC_LIBBROTLIDEC QUIET libbrotlidec)

find_path(LIBBROTLIDEC_INCLUDE_DIR
  NAMES brotli/decode.h
  HINTS ${PC_LIBBROTLIDEC_INCLUDE_DIRS}
)
find_library(LIBBROTLIDEC_LIBRARY
  NAMES brotlidec
  HINTS ${PC_LIBBROTLIDEC_LIBRARY_DIRS}
)

if(PC_LIBBROTLIDEC_FOUND)
  set(LIBBROTLIDEC_VERSION ${PC_LIBBROTLIDEC_VERSION})
endif()

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set LIBBROTLIDEC_FOUND
# to TRUE if all listed variables are TRUE and the requested version
# matches.
find_package_handle_standard_args(Libbrotlidec REQUIRED_VARS
                                  LIBBROTLIDEC_LIBRARY LIBBROTLIDEC_INCLUDE_DIR
                                  VERSION_VAR LIBBROTLIDEC_VERSION)

if(LIBBROTLIDEC_FOUND)
  set(LIBBROTLIDEC_LIBRARIES     ${LIBBROTLIDEC_LIBRARY})
  set(LIBBROTLIDEC_INCLUDE_DIRS  ${LIBBROTLIDEC_INCLUDE_DIR})
endif()

mark_as_advanced(LIBBROTLIDEC_INCLUDE_DIR LIBBROTLIDEC_LIBRARY)
