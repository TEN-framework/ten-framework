#
# CMake Toolchain file for crosscompiling on ARM.
#
# This can be used when running cmake in the following way:
#  cd build/
#  cmake .. -DCMAKE_TOOLCHAIN_FILE=../cross-arm-linux-gnueabihf.cmake
#

# Target operating system name.
set(CMAKE_SYSTEM_NAME Linux)

# assumed these are set up on the $PATH
set(TC xtensa-esp32-elf)

set(CMAKE_C_COMPILER	"${TC}-gcc${EXECUTABLE_EXT}")
set(CMAKE_AR		"${TC}-ar${EXECUTABLE_EXT}")
set(CMAKE_RANLIB	"${TC}-ranlib${EXECUTABLE_EXT}")
set(CMAKE_LINKER	"${TC}-ld${EXECUTABLE_EXT}")

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

SET(CMAKE_C_FLAGS "-nostdlib -Wall -Werror \
	-I${BUILD_DIR_BASE}/include \
	-I${IDF_PATH}/components/newlib/platform_include \
	-I${IDF_PATH}/components/mdns/include \
	-I${IDF_PATH}/components/heap/include \
	-I${IDF_PATH}/components/driver/include \
	-I${IDF_PATH}/components/spi_flash/include \
	-I${IDF_PATH}/components/nvs_flash/include \
	-I${IDF_PATH}/components/tcpip_adapter/include \
	-I${IDF_PATH}/components/lwip/include/lwip/posix \
	-I${IDF_PATH}/components/lwip/include/lwip \
	-I${IDF_PATH}/components/lwip/include/lwip/port \
	-I${IDF_PATH}/components/esp32/include/ \
	-I${IDF_PATH}/components/bootloader_support/include/ \
	-I${IDF_PATH}/components/app_update/include/ \
	-I$(IDF_PATH)/components/soc/esp32/include/ \
	-I$(IDF_PATH)/components/soc/include/ \
	-I$(IDF_PATH)/components/vfs/include/ \
	${LWS_C_FLAGS} -Os \
	-I${IDF_PATH}/components/nvs_flash/test_nvs_host \
	-I${IDF_PATH}/components/freertos/include" CACHE STRING "" FORCE)

# Where to look for the target environment. (More paths can be added here)
set(CMAKE_FIND_ROOT_PATH "${CROSS_PATH}")

# Adjust the default behavior of the FIND_XXX() commands:
# search programs in the host environment only.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

# Search headers and libraries in the target environment only.
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

