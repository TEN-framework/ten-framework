# - Try to find libbrotlienc
# Once done this will define
#  LIBBROTLIENC_FOUND        - System has libbrotlienc
#  LIBBROTLIENC_INCLUDE_DIRS - The libbrotlienc include directories
#  LIBBROTLIENC_LIBRARIES    - The libraries needed to use libbrotlienc

find_package(PkgConfig QUIET)
pkg_check_modules(PC_LIBBROTLIENC QUIET libbrotlienc)

find_path(LIBBROTLIENC_INCLUDE_DIR
  NAMES brotli/encode.h
  HINTS ${PC_LIBBROTLIENC_INCLUDE_DIRS}
)
find_library(LIBBROTLIENC_LIBRARY
  NAMES brotlienc
  HINTS ${PC_LIBBROTLIENC_LIBRARY_DIRS}
)

if(PC_LIBBROTLIENC_FOUND)
  set(LIBBROTLIENC_VERSION ${PC_LIBBROTLIENC_VERSION})
endif()

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set LIBBROTLIENC_FOUND
# to TRUE if all listed variables are TRUE and the requested version
# matches.
find_package_handle_standard_args(Libbrotlienc REQUIRED_VARS
                                  LIBBROTLIENC_LIBRARY LIBBROTLIENC_INCLUDE_DIR
                                  VERSION_VAR LIBBROTLIENC_VERSION)

if(LIBBROTLIENC_FOUND)
  set(LIBBROTLIENC_LIBRARIES     ${LIBBROTLIENC_LIBRARY})
  set(LIBBROTLIENC_INCLUDE_DIRS  ${LIBBROTLIENC_INCLUDE_DIR})
endif()

mark_as_advanced(LIBBROTLIENC_INCLUDE_DIR LIBBROTLIENC_LIBRARY)
