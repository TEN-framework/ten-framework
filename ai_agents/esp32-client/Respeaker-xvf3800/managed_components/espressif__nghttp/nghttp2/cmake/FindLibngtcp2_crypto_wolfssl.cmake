# - Try to find libngtcp2_crypto_wolfssl
# Once done this will define
#  LIBNGTCP2_CRYPTO_WOLFSSL_FOUND        - System has libngtcp2_crypto_wolfssl
#  LIBNGTCP2_CRYPTO_WOLFSSL_INCLUDE_DIRS - The libngtcp2_crypto_wolfssl include directories
#  LIBNGTCP2_CRYPTO_WOLFSSL_LIBRARIES    - The libraries needed to use libngtcp2_crypto_wolfssl

find_package(PkgConfig QUIET)
pkg_check_modules(PC_LIBNGTCP2_CRYPTO_WOLFSSL QUIET libngtcp2_crypto_wolfssl)

find_path(LIBNGTCP2_CRYPTO_WOLFSSL_INCLUDE_DIR
  NAMES ngtcp2/ngtcp2_crypto_wolfssl.h
  HINTS ${PC_LIBNGTCP2_CRYPTO_WOLFSSL_INCLUDE_DIRS}
)
find_library(LIBNGTCP2_CRYPTO_WOLFSSL_LIBRARY
  NAMES ngtcp2_crypto_wolfssl
  HINTS ${PC_LIBNGTCP2_CRYPTO_WOLFSSL_LIBRARY_DIRS}
)

if(LIBNGTCP2_CRYPTO_WOLFSSL_INCLUDE_DIR)
  set(_version_regex "^#define[ \t]+NGTCP2_VERSION[ \t]+\"([^\"]+)\".*")
  file(STRINGS "${LIBNGTCP2_CRYPTO_WOLFSSL_INCLUDE_DIR}/ngtcp2/version.h"
    LIBNGTCP2_CRYPTO_WOLFSSL_VERSION REGEX "${_version_regex}")
  string(REGEX REPLACE "${_version_regex}" "\\1"
    LIBNGTCP2_CRYPTO_WOLFSSL_VERSION "${LIBNGTCP2_CRYPTO_WOLFSSL_VERSION}")
  unset(_version_regex)
endif()

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set
# LIBNGTCP2_CRYPTO_WOLFSSL_FOUND to TRUE if all listed variables are
# TRUE and the requested version matches.
find_package_handle_standard_args(Libngtcp2_crypto_wolfssl REQUIRED_VARS
                                  LIBNGTCP2_CRYPTO_WOLFSSL_LIBRARY
                                  LIBNGTCP2_CRYPTO_WOLFSSL_INCLUDE_DIR
                                  VERSION_VAR LIBNGTCP2_CRYPTO_WOLFSSL_VERSION)

if(LIBNGTCP2_CRYPTO_WOLFSSL_FOUND)
  set(LIBNGTCP2_CRYPTO_WOLFSSL_LIBRARIES ${LIBNGTCP2_CRYPTO_WOLFSSL_LIBRARY})
  set(LIBNGTCP2_CRYPTO_WOLFSSL_INCLUDE_DIRS ${LIBNGTCP2_CRYPTO_WOLFSSL_INCLUDE_DIR})
endif()

mark_as_advanced(LIBNGTCP2_CRYPTO_WOLFSSL_INCLUDE_DIR
                 LIBNGTCP2_CRYPTO_WOLFSSL_LIBRARY)
