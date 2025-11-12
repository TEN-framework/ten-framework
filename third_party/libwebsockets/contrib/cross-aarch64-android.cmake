#
# CMake Toolchain file for crosscompiling Android / aarch64
#
# This can be used when running cmake in the following way:
#  cd build/
#  cmake .. -DCMAKE_TOOLCHAIN_FILE=contrib/cross-aarch64-android.cmake
#


set(ANDROID_API_VER 24)
set(ABARCH1 arm64)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(NDK /opt/android/ndk/21.1.6352462/)
set(CROSS_SYSROOT "${NDK}/platforms/android-${ANDROID_API_VER}/arch-${ABARCH1}")
set(BUILD_ARCH linux-x86_64)

#
# Rest should be computed from the above
#
set(TC_PATH ${NDK}/toolchains/llvm/prebuilt/${BUILD_ARCH})
set(TC_BASE ${TC_PATH}/bin/${CMAKE_SYSTEM_PROCESSOR}-linux-android)
set(PLATFORM android)
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_C_COMPILER "${TC_BASE}${ANDROID_API_VER}-clang")
set(CMAKE_CXX_COMPILER "${TC_BASE}${ANDROID_API_VER}-clang++")
set(CMAKE_STAGING_PREFIX "${CROSS_SYSROOT}")

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

#-nostdlib
SET(CMAKE_C_FLAGS "-DGCC_VER=\"\\\"$(GCC_VER)\\\"\" -DARM64=1 -D__LP64__=1 -Os -g3 -fpie -mstrict-align -fPIC -ffunction-sections -fdata-sections -D__ANDROID_API__=${ANDROID_API_VER} -Wno-pointer-sign" CACHE STRING "" FORCE)


set(CMAKE_FIND_ROOT_PATH "${CROSS_SYSROOT}")

# Adjust the default behavior of the FIND_XXX() commands:
# search programs in the host environment only.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

# Search headers and libraries in the target environment only.
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)


