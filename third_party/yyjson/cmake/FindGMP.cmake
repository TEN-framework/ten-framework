# Try to find the GNU Multiple Precision Arithmetic Library (GMP)
# GMP_FOUND - system has GMP lib
# GMP_INCLUDE_DIR - the GMP include directory
# GMP_LIBRARIES - Libraries needed to use GMP


if (GMP_INCLUDES AND GMP_LIBRARIES)
  set(GMP_FIND_QUIETLY TRUE)
endif (GMP_INCLUDES AND GMP_LIBRARIES)

find_path(GMP_INCLUDES
  NAMES
  gmp.h
  PATHS
  $ENV{GMPDIR}
  ${INCLUDE_INSTALL_DIR}
)

find_library(GMP_LIBRARIES gmp PATHS $ENV{GMPDIR} ${LIB_INSTALL_DIR})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GMP DEFAULT_MSG
                                  GMP_INCLUDES GMP_LIBRARIES)
mark_as_advanced(GMP_INCLUDES GMP_LIBRARIES)