#
# CMake Toolchain file for crosscompiling on Atmel Arm products
#
# To build without tls
#
#  cd build/
#  cmake .. -DCMAKE_INSTALL_PREFIX:PATH=/opt/atmel/cross-root \
#           -DCMAKE_TOOLCHAIN_FILE=../contrib/cross-atmel.cmake \
#           -DLWS_PLAT_FREERTOS=1 \
#           -DLWS_WITH_ZLIB=0 \
#           -DLWS_WITHOUT_EXTENSIONS=1 \
#           -DLWS_WITH_ZIP_FOPS=0 \
#           -DLWS_WITH_HTTP_STREAM_COMPRESSION=0 \
#           -DLWS_WITH_MBEDTLS=0 \
#           -DLWS_WITH_SSL=0 \
#           -DLWS_WITH_FILE_OPS=0
#

# I had to edit /opt/xdk-asf-3.48.0/thirdparty/lwip/lwip-port-1.4.1-dev/sam/include/arch/cc.h
# to comment out #define LWIP_PROVIDE_ERRNO

# if your sdk lives somewhere else, this is the only place that should need changing

set(CROSS_BASE /opt/arm-none-eabi)
set(SDK_BASE /opt/xdk-asf-3.48.0)
set(CROSS_PATH ${CROSS_BASE}/bin/arm-none-eabi)

set(LWIP_VER 1.4.1-dev)
set(FREERTOS_VER 10.0.0)

#
# Target operating system name.
set(CMAKE_SYSTEM_NAME Generic)

# Name of C compiler.
set(CMAKE_C_COMPILER "${CROSS_PATH}-gcc")

#
# cmake believes we should link a NOP test program OK, but since we're
# baremetal, that's not true in our case.  It tries to build this test
# with the cross compiler, but with no args on it, and it fails.
# So disable this test for this toolchain (we'll find out soon enough
# if we actually can't compile anything)

set(CMAKE_C_COMPILER_WORKS 1)
set(CMAKE_CXX_COMPILER_WORKS 1)

#
# similarly we're building a .a like this, we can't actually build
# complete test programs to probe api availability... so force some
# key ones

set(LWS_HAVE_mbedtls_ssl_conf_alpn_protocols 1)
set(LWS_HAVE_mbedtls_ssl_conf_alpn_protocols 1)
set(LWS_HAVE_mbedtls_ssl_get_alpn_protocol 1)
set(LWS_HAVE_mbedtls_ssl_conf_sni 1)
set(LWS_HAVE_mbedtls_ssl_set_hs_ca_chain 1)
set(LWS_HAVE_mbedtls_ssl_set_hs_own_cert 1)
set(LWS_HAVE_mbedtls_ssl_set_hs_authmode 1)
set(LWS_HAVE_mbedtls_net_init 1)
set(LWS_HAVE_mbedtls_md_setup 1) # not on xenial 2.2
set(LWS_HAVE_mbedtls_rsa_complete 1) # not on xenial 2.2
set(LWS_HAVE_mbedtls_internal_aes_encrypt 1)
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

set(PLAT_ARCH        ARM_CM4F)
set(PLAT_ARCH_CMSIS  sam4e)
set(PLAT_SOC         __SAM4E16E__)
set(PLAT_BOARD       SAM4E_XPLAINED_PRO)

set(CF_LWIP "${CF_LWIP} -I${SDK_BASE}/thirdparty/lwip/lwip-${LWIP_VER}/src/include/lwip")
set(CF_LWIP "${CF_LWIP} -I${SDK_BASE}/thirdparty/lwip/lwip-${LWIP_VER}/src/include/posix")
set(CF_LWIP "${CF_LWIP} -I${SDK_BASE}/thirdparty/lwip/lwip-${LWIP_VER}/src/include")
set(CF_LWIP "${CF_LWIP} -I${SDK_BASE}/thirdparty/lwip/lwip-${LWIP_VER}/src/module_config")
set(CF_LWIP "${CF_LWIP} -I${SDK_BASE}/thirdparty/lwip/lwip-port-${LWIP_VER}/sam/include")
set(CF_LWIP "${CF_LWIP} -I${SDK_BASE}/thirdparty/lwip/lwip-${LWIP_VER}/src/include/ipv4")

set(CF_FREERTOS "${CF_FREERTOS} -I${SDK_BASE}/thirdparty/freertos/freertos-${FREERTOS_VER}/Source/include")
set(CF_FREERTOS "${CF_FREERTOS} -I${SDK_BASE}/thirdparty/freertos/freertos-${FREERTOS_VER}/module_config")
set(CF_FREERTOS "${CF_FREERTOS} -I${SDK_BASE}/thirdparty/freertos/freertos-${FREERTOS_VER}/Source/portable/GCC/${PLAT_ARCH}")

set(CF_SDK_GLUE "${CF_SDK_GLUE} -I${SDK_BASE}/common/boards")
set(CF_SDK_GLUE "${CF_SDK_GLUE} -I${SDK_BASE}/common/utils")
set(CF_SDK_GLUE "${CF_SDK_GLUE} -I${SDK_BASE}/sam/utils/")
set(CF_SDK_GLUE "${CF_SDK_GLUE} -I${SDK_BASE}/sam/utils/preprocessor")
set(CF_SDK_GLUE "${CF_SDK_GLUE} -I${SDK_BASE}/sam/utils/header_files")
set(CF_SDK_GLUE "${CF_SDK_GLUE} -I${SDK_BASE}/sam/boards")
set(CF_SDK_GLUE "${CF_SDK_GLUE} -I${SDK_BASE}/sam/utils/cmsis/${PLAT_ARCH_CMSIS}/source/templates")
set(CF_SDK_GLUE "${CF_SDK_GLUE} -I${SDK_BASE}/sam/utils/cmsis/${PLAT_ARCH_CMSIS}/include")
set(CF_SDK_GLUE "${CF_SDK_GLUE} -I${SDK_BASE}/thirdparty/CMSIS/Include")
set(CF_SDK_GLUE "${CF_SDK_GLUE} -I${SDK_BASE}/common/utils/osprintf")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -lnosys -nostartfiles ${CF_LWIP} ${CF_FREERTOS} ${CF_SDK_GLUE} -DBOARD=${PLAT_BOARD} -D${PLAT_SOC} -DLWIP_TIMEVAL_PRIVATE=0 -DLWS_AMAZON_RTOS=1 -DLWIP_SOCKET_OFFSET=0 -DLWIP_COMPAT_SOCKETS -DLWIP_DNS=1 -DLWIP_SOCKETS=1 "  CACHE STRING "" FORCE)

# Where to look for the target environment. (More paths can be added here)
set(CMAKE_FIND_ROOT_PATH "${CROSS_PATH}")

# Adjust the default behavior of the FIND_XXX() commands:
# search programs in the host environment only.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

# Search headers and libraries in the target environment only.
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

