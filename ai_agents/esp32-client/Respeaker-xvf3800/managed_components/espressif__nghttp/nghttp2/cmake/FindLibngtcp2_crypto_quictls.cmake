# - Try to find libngtcp2_crypto_quictls
# Once done this will define
#  LIBNGTCP2_CRYPTO_QUICTLS_FOUND        - System has libngtcp2_crypto_quictls
#  LIBNGTCP2_CRYPTO_QUICTLS_INCLUDE_DIRS - The libngtcp2_crypto_quictls include directories
#  LIBNGTCP2_CRYPTO_QUICTLS_LIBRARIES    - The libraries needed to use libngtcp2_crypto_quictls

find_package(PkgConfig QUIET)
pkg_check_modules(PC_LIBNGTCP2_CRYPTO_QUICTLS QUIET libngtcp2_crypto_quictls)

find_path(LIBNGTCP2_CRYPTO_QUICTLS_INCLUDE_DIR
  NAMES ngtcp2/ngtcp2_crypto_quictls.h
  HINTS ${PC_LIBNGTCP2_CRYPTO_QUICTLS_INCLUDE_DIRS}
)
find_library(LIBNGTCP2_CRYPTO_QUICTLS_LIBRARY
  NAMES ngtcp2_crypto_quictls
  HINTS ${PC_LIBNGTCP2_CRYPTO_QUICTLS_LIBRARY_DIRS}
)

if(LIBNGTCP2_CRYPTO_QUICTLS_INCLUDE_DIR)
  set(_version_regex "^#define[ \t]+NGTCP2_VERSION[ \t]+\"([^\"]+)\".*")
  file(STRINGS "${LIBNGTCP2_CRYPTO_QUICTLS_INCLUDE_DIR}/ngtcp2/version.h"
    LIBNGTCP2_CRYPTO_QUICTLS_VERSION REGEX "${_version_regex}")
  string(REGEX REPLACE "${_version_regex}" "\\1"
    LIBNGTCP2_CRYPTO_QUICTLS_VERSION "${LIBNGTCP2_CRYPTO_QUICTLS_VERSION}")
  unset(_version_regex)
endif()

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set
# LIBNGTCP2_CRYPTO_QUICTLS_FOUND to TRUE if all listed variables are
# TRUE and the requested version matches.
find_package_handle_standard_args(Libngtcp2_crypto_quictls REQUIRED_VARS
                                  LIBNGTCP2_CRYPTO_QUICTLS_LIBRARY
                                  LIBNGTCP2_CRYPTO_QUICTLS_INCLUDE_DIR
                                  VERSION_VAR LIBNGTCP2_CRYPTO_QUICTLS_VERSION)

if(LIBNGTCP2_CRYPTO_QUICTLS_FOUND)
  set(LIBNGTCP2_CRYPTO_QUICTLS_LIBRARIES ${LIBNGTCP2_CRYPTO_QUICTLS_LIBRARY})
  set(LIBNGTCP2_CRYPTO_QUICTLS_INCLUDE_DIRS ${LIBNGTCP2_CRYPTO_QUICTLS_INCLUDE_DIR})
endif()

mark_as_advanced(LIBNGTCP2_CRYPTO_QUICTLS_INCLUDE_DIR
                 LIBNGTCP2_CRYPTO_QUICTLS_LIBRARY)
