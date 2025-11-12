#
# CMake Toolchain file for crosscompiling aarch64 for QNX.
#
# This can be used when running cmake in the following way:
#  cd build/
#  cmake .. -DCMAKE_TOOLCHAIN_FILE=../cross-aarch64-qnx.cmake
#

# adapt to your toolchain path
set(CROSS_PATH /var/toolchain/qnx5.4/host/linux/x86_64/usr)

# Target operating system name.
set(CMAKE_SYSTEM_NAME QNX)
set(BUILD_SHARED_LIBS OFF)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# Name of C compiler.
set(CMAKE_C_COMPILER "${CROSS_PATH}/bin/ntoaarch64-gcc")
set(CMAKE_CXX_COMPILER "${CROSS_PATH}/bin/ntoaarch64-g++")

set(CMAKE_C_FLAGS "-Wno-error")
set(CMAKE_FIND_ROOT_PATH "${CROSS_PATH}")

#
# Different build system distros set release optimization level to different
# things according to their local policy, eg, Fedora is -O2 and Ubuntu is -O3
# here.  Actually the build system's local policy is completely unrelated to
# our desire for cross-build release optimization policy for code built to run
# on a completely different target than the build system itself.
#
# Since this goes last on the compiler commandline we have to override it to a
# sane value for cross-build here.  Notice some gcc versions enable broken
# optimizations with -O3.
#
if (CMAKE_BUILD_TYPE MATCHES RELEASE OR CMAKE_BUILD_TYPE MATCHES Release OR CMAKE_BUILD_TYPE MATCHES release)
	set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2")
endif()

# Where to look for the target environment. (More paths can be added here)
set(CMAKE_FIND_ROOT_PATH "${CROSS_PATH}")

# Adjust the default behavior of the FIND_XXX() commands:
# search programs in the host environment only.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

# Search headers and libraries in the target environment only.
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

