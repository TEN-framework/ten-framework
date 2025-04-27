#
# GNU Make makefile for building static libraries for use with the Android NDK
# Copyright (C) 2016, Alexander Bruines <alexander.bruines@gmail.com>
#
# This file is made available under the Creative Commons CC0 1.0
# Universal Public Domain Dedication.
#
# The person who associated a work with this deed has dedicated
# the work to the public domain by waiving all of his or her rights
# to the work worldwide under copyright law, including all related
# and neighboring rights, to the extent allowed by law. You can copy,
# modify, distribute and perform the work, even for commercial purposes,
# all without asking permission.
#
# The test apps are intended to be adapted for use in your code, which
# may be proprietary.  So unlike the library itself, they are licensed
# Public Domain.
#

#
# This makefile is fully intergrated with this Android Studio project and
# it will be called automaticaly when you build the project with Gradle.
#
# The source packages for the libraries will be automaticaly downloaded.
# Alternativly you can provide your own sources by placing the following
# files in the 'jni' directory:
#
#  zlib-1.2.8.tar.gz
#  openssl-1.0.2g.tar.gz
#  libwebsockets.tar.gz
#
# This makefile was tested with the latest NDK/SDK and Android Studio at the
# time of this writing. As these software packages evolve changes to this
# makefile may be needed or it may become obselete...
#
# This makefile was made for use in Linux but you may be able to edit it
# and make it work under Windows.
#
# At least on Debian, building openssl requires package xutils-dev
# for makedepend. Ofcourse the standard development packages must also be
# installed, but xutils-dev is not that obvious in this case...
#
# Makedepend will most likely print a lot of warnings during the 'make depend'
# stage of building openssl. In this case these warnings can be safely ignored.
#

# Include Application.mk but do not complain if it is not found
#
ifeq ($(MAKE_NO_INCLUDES),)
-include Application.mk
endif

# Location of the NDK.
#
ifeq ($(NDK_ROOT),)
NDK_ROOT := /opt/Android/SDK/ndk-bundle
endif

# Select the ABIs to compile for
#
NDK_APP_ABI = $(APP_ABI)
ifeq ($(NDK_APP_ABI),)
# Set to 'all' if APP_ABI is undefined
NDK_APP_ABI = all
endif
ifeq ($(NDK_APP_ABI),all)
# Translate 'all' to the individual targets
NDK_APP_ABI = armeabi armeabi-v7a arm64-v8a mips mips64 x86 x86_64
else
# Use the targets from APP_ABI
NDK_APP_ABI = $(APP_ABI)
endif

# Select the Android platform to compile for
#
ifeq ($(APP_PLATFORM),)
# use a level that supports all specified ABIs if none was specified
APP_PLATFORM = android-21
endif

NDK_MAKE_TOOLCHAIN := $(NDK_ROOT)/build/tools/make_standalone_toolchain.py

#
# The source packages we want/need
# Zlib and openssl should be defined in Application.mk, libwebsockets is
# cloned from github
#

ifeq ($(ZLIB_VERSION),)
ZLIB_VERSION := 1.2.8
endif
ifeq ($(OPENSSL_VERSION),)
OPENSSL_VERSION := 1.0.2g
endif

ifeq ($(ZLIB_TGZ_SOURCE),)
ZLIB_TGZ_SOURCE := zlib-$(ZLIB_VERSION).tar.gz
endif
ifeq ($(OPENSSL_TGZ_SOURCE),)
OPENSSL_TGZ_SOURCE := openssl-$(OPENSSL_VERSION).tar.gz
endif
LIBWEBSOCKETS_TGZ_SOURCE := libwebsockets.tar.gz

# The names of the directories in the source tgz files
ZLIB_DIR := $(basename $(basename $(ZLIB_TGZ_SOURCE)))
OPENSSL_DIR := $(basename $(basename $(OPENSSL_TGZ_SOURCE)))
LIBWEBSOCKETS_DIR := $(basename $(basename $(LIBWEBSOCKETS_TGZ_SOURCE)))

# The URLs used to fetch the source tgz files
ZLIB_TGZ_URL := http://prdownloads.sourceforge.net/libpng/$(ZLIB_TGZ_SOURCE)
OPENSSL_TGZ_URL := https://openssl.org/source/$(OPENSSL_TGZ_SOURCE)
ifeq ($(LIBWEBSOCKETS_GIT_URL),)
LIBWEBSOCKETS_GIT_URL := https://github.com/warmcat/libwebsockets.git
endif

# These values are the same as the values for $TARGET_ARCH_ABI in Android.mk
# This way 'make $TARGET_ARCH_ABI' builds libraries for that ABI.
# This is also the name for the directory where the libraries are installed to.
#
TARGET_X86 := x86
TARGET_X86_64 := x86_64
TARGET_ARM := armeabi
TARGET_ARM_V7A := armeabi-v7a
TARGET_ARM_V7A_HARD := armeabi-v7a-hard
TARGET_ARM64_V8A := arm64-v8a
TARGET_MIPS := mips
TARGET_MIPS64 := mips64

# The Android NDK API version to build the libraries with.
#
#  android-9 ... android-19 support arm mips and x86
#  android-21 and higher also support arm64 mips64 and x86_64
#
# These should be set to the same value as APP_PLATFORM (Application.mk)
#
# http://developer.android.com/ndk/guides/stable_apis.html
#
# If you change these or APP_PLATFORM you must do a 'make clean'
#
# Note:
# libraries compiled for android-21 and upwards are incompatible with devices below that version!
# http://stackoverflow.com/questions/28740315/android-ndk-getting-java-lang-unsatisfiedlinkerror-dlopen-failed-cannot-loca
#
TARGET_X86_NDK_API := $(subst android-,,$(APP_PLATFORM))
TARGET_X86_64_NDK_API := $(subst android-,,$(APP_PLATFORM))
TARGET_ARM_NDK_API := $(subst android-,,$(APP_PLATFORM))
TARGET_ARM_V7A_NDK_API := $(subst android-,,$(APP_PLATFORM))
TARGET_ARM_V7A_HARD_NDK_API := $(subst android-,,$(APP_PLATFORM))
TARGET_ARM64_V8A_NDK_API := $(subst android-,,$(APP_PLATFORM))
TARGET_MIPS_NDK_API := $(subst android-,,$(APP_PLATFORM))
TARGET_MIPS64_NDK_API := $(subst android-,,$(APP_PLATFORM))

# The configure arguments to pass to the OpenSSL Configure script
# (--prefix and --openssldir are added automaticaly).
# (note: use no-asm on x86 and x86_64 to generate fully position independent code)
#
# x86
TARGET_X86_OPENSSL_CONFIG_TARGET := android-x86
TARGET_X86_OPENSSL_CONFIG := no-asm no-shared no-idea no-mdc2 no-rc5 no-zlib no-zlib-dynamic enable-tlsext no-ssl2 no-ssl3 enable-ec enable-ecdh enable-ecp
# x86_64
TARGET_X86_64_OPENSSL_CONFIG_TARGET := linux-x86_64
TARGET_X86_64_OPENSSL_CONFIG := no-asm no-shared no-idea no-mdc2 no-rc5 no-zlib no-zlib-dynamic enable-tlsext no-ssl2 no-ssl3 enable-ec enable-ecdh enable-ecp enable-ec_nistp_64_gcc_128
# armeabi
TARGET_ARM_OPENSSL_CONFIG_TARGET := android
TARGET_ARM_OPENSSL_CONFIG := no-shared no-idea no-mdc2 no-rc5 no-zlib no-zlib-dynamic enable-tlsext no-ssl2 no-ssl3 enable-ec enable-ecdh enable-ecp
# armeabi-v7a
TARGET_ARM_V7A_OPENSSL_CONFIG_TARGET := android-armv7
TARGET_ARM_V7A_OPENSSL_CONFIG := no-shared no-idea no-mdc2 no-rc5 no-zlib no-zlib-dynamic enable-tlsext no-ssl2 no-ssl3 enable-ec enable-ecdh enable-ecp
# armeabi-v7a-hard
TARGET_ARM_V7A_HARD_OPENSSL_CONFIG_TARGET := android-armv7
TARGET_ARM_V7A_HARD_OPENSSL_CONFIG := no-shared no-idea no-mdc2 no-rc5 no-zlib no-zlib-dynamic enable-tlsext no-ssl2 no-ssl3 enable-ec enable-ecdh enable-ecp
# arm64-v8a
TARGET_ARM64_V8A_OPENSSL_CONFIG_TARGET := android
TARGET_ARM64_V8A_OPENSSL_CONFIG := no-shared no-idea no-mdc2 no-rc5 no-zlib no-zlib-dynamic enable-tlsext no-ssl2 no-ssl3 enable-ec enable-ecdh enable-ecp
# mips
TARGET_MIPS_OPENSSL_CONFIG_TARGET := android-mips
TARGET_MIPS_OPENSSL_CONFIG := no-shared no-idea no-mdc2 no-rc5 no-zlib no-zlib-dynamic enable-tlsext no-ssl2 no-ssl3 enable-ec enable-ecdh enable-ecp
# mips64
TARGET_MIPS64_OPENSSL_CONFIG_TARGET := android
TARGET_MIPS64_OPENSSL_CONFIG := no-shared no-idea no-mdc2 no-rc5 no-zlib no-zlib-dynamic enable-tlsext no-ssl2 no-ssl3 enable-ec enable-ecdh enable-ecp

# The cmake configuration options for libwebsockets per target ABI,
# --prefix and openssl library/header paths are set automaticaly and
# the location of zlib should be picked up by CMake
# x86
TARGET_X86_LWS_OPTIONS = \
 -DCMAKE_C_COMPILER=$(shell pwd)/$(TOOLCHAIN_X86)/bin/$(TOOLCHAIN_X86_PREFIX)-gcc \
 -DCMAKE_AR=$(shell pwd)/$(TOOLCHAIN_X86)/bin/$(TOOLCHAIN_X86_PREFIX)-ar \
 -DCMAKE_RANLIB=$(shell pwd)/$(TOOLCHAIN_X86)/bin/$(TOOLCHAIN_X86_PREFIX)-ranlib \
 -DCMAKE_C_FLAGS="$$CFLAGS" \
 -DLWS_WITH_SHARED=OFF \
 -DLWS_WITH_STATIC=ON \
 -DLWS_WITHOUT_DAEMONIZE=ON \
 -DLWS_WITHOUT_TESTAPPS=ON \
 -DLWS_IPV6=OFF \
 -DLWS_WITH_BUNDLED_ZLIB=OFF \
 -DLWS_WITH_SSL=ON  \
 -DLWS_WITH_HTTP2=ON \
 -DCMAKE_BUILD_TYPE=Release
# x86_64
TARGET_X86_64_LWS_OPTIONS = \
 -DCMAKE_C_COMPILER=$(shell pwd)/$(TOOLCHAIN_X86_64)/bin/$(TOOLCHAIN_X86_64_PREFIX)-gcc \
 -DCMAKE_AR=$(shell pwd)/$(TOOLCHAIN_X86_64)/bin/$(TOOLCHAIN_X86_64_PREFIX)-ar \
 -DCMAKE_RANLIB=$(shell pwd)/$(TOOLCHAIN_X86_64)/bin/$(TOOLCHAIN_X86_64_PREFIX)-ranlib \
 -DCMAKE_C_FLAGS="$$CFLAGS" \
 -DLWS_WITH_SHARED=OFF \
 -DLWS_WITH_STATIC=ON \
 -DLWS_WITHOUT_DAEMONIZE=ON \
 -DLWS_WITHOUT_TESTAPPS=ON \
 -DLWS_IPV6=OFF \
 -DLWS_WITH_BUNDLED_ZLIB=OFF \
 -DLWS_WITH_SSL=ON  \
 -DLWS_WITH_HTTP2=ON \
 -DCMAKE_BUILD_TYPE=Release
# armeabi
TARGET_ARM_LWS_OPTIONS = \
 -DCMAKE_C_COMPILER=$(shell pwd)/$(TOOLCHAIN_ARM)/bin/$(TOOLCHAIN_ARM_PREFIX)-gcc \
 -DCMAKE_AR=$(shell pwd)/$(TOOLCHAIN_ARM)/bin/$(TOOLCHAIN_ARM_PREFIX)-ar \
 -DCMAKE_RANLIB=$(shell pwd)/$(TOOLCHAIN_ARM)/bin/$(TOOLCHAIN_ARM_PREFIX)-ranlib \
 -DCMAKE_C_FLAGS="$$CFLAGS" \
 -DLWS_WITH_SHARED=OFF \
 -DLWS_WITH_STATIC=ON \
 -DLWS_WITHOUT_DAEMONIZE=ON \
 -DLWS_WITHOUT_TESTAPPS=ON \
 -DLWS_IPV6=OFF \
 -DLWS_WITH_BUNDLED_ZLIB=OFF \
 -DLWS_WITH_SSL=ON  \
 -DLWS_WITH_HTTP2=ON \
 -DCMAKE_BUILD_TYPE=Release
# armeabi-v7a
TARGET_ARM_V7A_LWS_OPTIONS = \
 -DCMAKE_C_COMPILER=$(shell pwd)/$(TOOLCHAIN_ARM_V7A)/bin/$(TOOLCHAIN_ARM_V7A_PREFIX)-gcc \
 -DCMAKE_AR=$(shell pwd)/$(TOOLCHAIN_ARM_V7A)/bin/$(TOOLCHAIN_ARM_V7A_PREFIX)-ar \
 -DCMAKE_RANLIB=$(shell pwd)/$(TOOLCHAIN_ARM_V7A)/bin/$(TOOLCHAIN_ARM_V7A_PREFIX)-ranlib \
 -DCMAKE_C_FLAGS="$$CFLAGS" \
 -DLWS_WITH_SHARED=OFF \
 -DLWS_WITH_STATIC=ON \
 -DLWS_WITHOUT_DAEMONIZE=ON \
 -DLWS_WITHOUT_TESTAPPS=ON \
 -DLWS_IPV6=OFF \
 -DLWS_WITH_BUNDLED_ZLIB=OFF \
 -DLWS_WITH_SSL=ON  \
 -DLWS_WITH_HTTP2=ON \
 -DCMAKE_BUILD_TYPE=Release
# armeabi-v7a-hard
TARGET_ARM_V7A_HARD_LWS_OPTIONS = \
 -DCMAKE_C_COMPILER=$(shell pwd)/$(TOOLCHAIN_ARM_V7A_HARD)/bin/$(TOOLCHAIN_ARM_V7A_HARD_PREFIX)-gcc \
 -DCMAKE_AR=$(shell pwd)/$(TOOLCHAIN_ARM_V7A_HARD)/bin/$(TOOLCHAIN_ARM_V7A_HARD_PREFIX)-ar \
 -DCMAKE_RANLIB=$(shell pwd)/$(TOOLCHAIN_ARM_V7A_HARD)/bin/$(TOOLCHAIN_ARM_V7A_HARD_PREFIX)-ranlib \
 -DCMAKE_C_FLAGS="$$CFLAGS" \
 -DLWS_WITH_SHARED=OFF \
 -DLWS_WITH_STATIC=ON \
 -DLWS_WITHOUT_DAEMONIZE=ON \
 -DLWS_WITHOUT_TESTAPPS=ON \
 -DLWS_IPV6=OFF \
 -DLWS_WITH_BUNDLED_ZLIB=OFF \
 -DLWS_WITH_SSL=ON  \
 -DLWS_WITH_HTTP2=ON \
 -DCMAKE_BUILD_TYPE=Release
# arm64-v8a
TARGET_ARM64_V8A_LWS_OPTIONS = \
 -DCMAKE_C_COMPILER=$(shell pwd)/$(TOOLCHAIN_ARM64_V8A)/bin/$(TOOLCHAIN_ARM64_V8A_PREFIX)-gcc \
 -DCMAKE_AR=$(shell pwd)/$(TOOLCHAIN_ARM64_V8A)/bin/$(TOOLCHAIN_ARM64_V8A_PREFIX)-ar \
 -DCMAKE_RANLIB=$(shell pwd)/$(TOOLCHAIN_ARM64_V8A)/bin/$(TOOLCHAIN_ARM64_V8A_PREFIX)-ranlib \
 -DCMAKE_C_FLAGS="$$CFLAGS" \
 -DLWS_WITH_SHARED=OFF \
 -DLWS_WITH_STATIC=ON \
 -DLWS_WITHOUT_DAEMONIZE=ON \
 -DLWS_WITHOUT_TESTAPPS=ON \
 -DLWS_IPV6=OFF \
 -DLWS_WITH_BUNDLED_ZLIB=OFF \
 -DLWS_WITH_SSL=ON  \
 -DLWS_WITH_HTTP2=ON \
 -DCMAKE_BUILD_TYPE=Release
# mips
TARGET_MIPS_LWS_OPTIONS = \
 -DCMAKE_C_COMPILER=$(shell pwd)/$(TOOLCHAIN_MIPS)/bin/$(TOOLCHAIN_MIPS_PREFIX)-gcc \
 -DCMAKE_AR=$(shell pwd)/$(TOOLCHAIN_MIPS)/bin/$(TOOLCHAIN_MIPS_PREFIX)-ar \
 -DCMAKE_RANLIB=$(shell pwd)/$(TOOLCHAIN_MIPS)/bin/$(TOOLCHAIN_MIPS_PREFIX)-ranlib \
 -DCMAKE_C_FLAGS="$$CFLAGS" \
 -DLWS_WITH_SHARED=OFF \
 -DLWS_WITH_STATIC=ON \
 -DLWS_WITHOUT_DAEMONIZE=ON \
 -DLWS_WITHOUT_TESTAPPS=ON \
 -DLWS_IPV6=OFF \
 -DLWS_WITH_BUNDLED_ZLIB=OFF \
 -DLWS_WITH_SSL=ON  \
 -DLWS_WITH_HTTP2=ON \
 -DCMAKE_BUILD_TYPE=Release
# mips64
TARGET_MIPS64_LWS_OPTIONS = \
 -DCMAKE_C_COMPILER=$(shell pwd)/$(TOOLCHAIN_MIPS64)/bin/$(TOOLCHAIN_MIPS64_PREFIX)-gcc \
 -DCMAKE_AR=$(shell pwd)/$(TOOLCHAIN_MIPS64)/bin/$(TOOLCHAIN_MIPS64_PREFIX)-ar \
 -DCMAKE_RANLIB=$(shell pwd)/$(TOOLCHAIN_MIPS64)/bin/$(TOOLCHAIN_MIPS64_PREFIX)-ranlib \
 -DCMAKE_C_FLAGS="$$CFLAGS" \
 -DLWS_WITH_SHARED=OFF \
 -DLWS_WITH_STATIC=ON \
 -DLWS_WITHOUT_DAEMONIZE=ON \
 -DLWS_WITHOUT_TESTAPPS=ON \
 -DLWS_IPV6=OFF \
 -DLWS_WITH_BUNDLED_ZLIB=OFF \
 -DLWS_WITH_SSL=ON  \
 -DLWS_WITH_HTTP2=ON \
 -DCMAKE_BUILD_TYPE=Release

#
# Toolchain configuration
#

# The directory names for the different toolchains
TOOLCHAIN_X86 := toolchains/x86
TOOLCHAIN_X86_64 := toolchains/x86_64
TOOLCHAIN_ARM := toolchains/arm
TOOLCHAIN_ARM_V7A := toolchains/arm-v7a
TOOLCHAIN_ARM_V7A_HARD := toolchains/arm-v7a-hard
TOOLCHAIN_ARM64_V8A := toolchains/arm64-v8a
TOOLCHAIN_MIPS := toolchains/mips
TOOLCHAIN_MIPS64 := toolchains/mips64

# Use APP_STL to determine what STL to use.
#
ifeq ($(APP_STL),stlport_static)
TOOLCHAIN_STL := stlport
else ifeq ($(APP_STL),stlport_shared)
TOOLCHAIN_STL := stlport
else ifeq ($(APP_STL),gnustl_static)
TOOLCHAIN_STL := gnustl
else ifeq ($(APP_STL),gnustl_shared)
TOOLCHAIN_STL := gnustl
else ifeq ($(APP_STL),c++_static)
TOOLCHAIN_STL := libc++
else ifeq ($(APP_STL),c++_shared)
TOOLCHAIN_STL := libc++
endif

# The settings to use for the individual toolchains:
# x86
TOOLCHAIN_X86_API := $(TARGET_X86_NDK_API)
TOOLCHAIN_X86_PREFIX := i686-linux-android
TOOLCHAIN_X86_FLAGS := -march=i686 -msse3 -mstackrealign -mfpmath=sse
TOOLCHAIN_X86_LINK :=
TOOLCHAIN_X86_PLATFORM_HEADERS := $(shell pwd)/$(TOOLCHAIN_X86)/sysroot/usr/include
TOOLCHAIN_X86_PLATFORM_LIBS := $(shell pwd)/$(TOOLCHAIN_X86)/sysroot/usr/lib
# x86_64
TOOLCHAIN_X86_64_API := $(TARGET_X86_64_NDK_API)
TOOLCHAIN_X86_64_PREFIX := x86_64-linux-android
TOOLCHAIN_X86_64_FLAGS :=
TOOLCHAIN_X86_64_LINK :=
TOOLCHAIN_X86_64_PLATFORM_HEADERS := $(shell pwd)/$(TOOLCHAIN_X86_64)/sysroot/usr/include
TOOLCHAIN_X86_64_PLATFORM_LIBS := $(shell pwd)/$(TOOLCHAIN_X86_64)/sysroot/usr/lib
# arm
TOOLCHAIN_ARM_API := $(TARGET_ARM_NDK_API)
TOOLCHAIN_ARM_PREFIX := arm-linux-androideabi
TOOLCHAIN_ARM_FLAGS := -mthumb
TOOLCHAIN_ARM_LINK :=
TOOLCHAIN_ARM_PLATFORM_HEADERS := $(shell pwd)/$(TOOLCHAIN_ARM)/sysroot/usr/include
TOOLCHAIN_ARM_PLATFORM_LIBS := $(shell pwd)/$(TOOLCHAIN_ARM)/sysroot/usr/lib
# arm-v7a
TOOLCHAIN_ARM_V7A_API := $(TARGET_ARM_V7A_NDK_API)
TOOLCHAIN_ARM_V7A_PREFIX := arm-linux-androideabi
TOOLCHAIN_ARM_V7A_FLAGS := -march=armv7-a -mfloat-abi=softfp -mfpu=vfpv3-d16
TOOLCHAIN_ARM_V7A_LINK := -march=armv7-a -Wl,--fix-cortex-a8
TOOLCHAIN_ARM_V7A_PLATFORM_HEADERS :=  $(shell pwd)/$(TOOLCHAIN_ARM_V7A)/sysroot/usr/include
TOOLCHAIN_ARM_V7A_PLATFORM_LIBS := $(shell pwd)/$(TOOLCHAIN_ARM_V7A)/sysroot/usr/lib
# arm-v7a-hard
TOOLCHAIN_ARM_V7A_HARD_API := $(TARGET_ARM_V7A_HARD_NDK_API)
TOOLCHAIN_ARM_V7A_HARD_PREFIX := arm-linux-androideabi
TOOLCHAIN_ARM_V7A_HARD_FLAGS := -march=armv7-a -mfpu=vfpv3-d16 -mhard-float -mfloat-abi=hard -D_NDK_MATH_NO_SOFTFP=1
TOOLCHAIN_ARM_V7A_HARD_LINK := -march=armv7-a -Wl,--fix-cortex-a8 -Wl,--no-warn-mismatch -lm_hard
TOOLCHAIN_ARM_V7A_HARD_PLATFORM_HEADERS :=  $(shell pwd)/$(TOOLCHAIN_ARM_V7A_HARD)/sysroot/usr/include
TOOLCHAIN_ARM_V7A_HARD_PLATFORM_LIBS := $(shell pwd)/$(TOOLCHAIN_ARM_V7A_HARD)/sysroot/usr/lib
# arm64-v8a
TOOLCHAIN_ARM64_V8A_API := $(TARGET_ARM64_V8A_NDK_API)
TOOLCHAIN_ARM64_V8A_PREFIX := aarch64-linux-android
TOOLCHAIN_ARM64_V8A_FLAGS :=
TOOLCHAIN_ARM64_V8A_LINK :=
TOOLCHAIN_ARM64_V8A_PLATFORM_HEADERS := $(shell pwd)/$(TOOLCHAIN_ARM64_V8A)/sysroot/usr/include
TOOLCHAIN_ARM64_V8A_PLATFORM_LIBS := $(shell pwd)/$(TOOLCHAIN_ARM64_V8A)/sysroot/usr/lib
# mips
TOOLCHAIN_MIPS_API := $(TARGET_MIPS_NDK_API)
TOOLCHAIN_MIPS_PREFIX := mipsel-linux-android
TOOLCHAIN_MIPS_FLAGS :=
TOOLCHAIN_MIPS_LINK :=
TOOLCHAIN_MIPS_PLATFORM_HEADERS := $(shell pwd)/$(TOOLCHAIN_MIPS)/sysroot/usr/include
TOOLCHAIN_MIPS_PLATFORM_LIBS := $(shell pwd)/$(TOOLCHAIN_MIPS)/sysroot/usr/lib
# mips64
TOOLCHAIN_MIPS64_API := $(TARGET_MIPS64_NDK_API)
TOOLCHAIN_MIPS64_PREFIX := mips64el-linux-android
TOOLCHAIN_MIPS64_FLAGS :=
TOOLCHAIN_MIPS64_LINK :=
TOOLCHAIN_MIPS64_PLATFORM_HEADERS := $(shell pwd)/$(TOOLCHAIN_MIPS64)/sysroot/usr/include
TOOLCHAIN_MIPS64_PLATFORM_LIBS := $(shell pwd)/$(TOOLCHAIN_MIPS64)/sysroot/usr/lib

# Environment variables to set while compiling for each ABI
# x86
TOOLCHAIN_X86_ENV = \
 ANDROID_DEV="$(shell pwd)/$(TOOLCHAIN_X86)/bin" \
 CC=$(TOOLCHAIN_X86_PREFIX)-gcc \
 CXX=$(TOOLCHAIN_X86_PREFIX)-g++ \
 LINK=$(TOOLCHAIN_X86_PREFIX)-g++ \
 LD=$(TOOLCHAIN_X86_PREFIX)-ld \
 AR=$(TOOLCHAIN_X86_PREFIX)-ar \
 RANLIB=$(TOOLCHAIN_X86_PREFIX)-ranlib \
 STRIP=$(TOOLCHAIN_X86_PREFIX)-strip \
 ARCH_FLAGS="$(TOOLCHAIN_X86_FLAGS)" \
 ARCH_LINK="$(TOOLCHAIN_X86_LINK)" \
 CPPFLAGS="-I. $(TOOLCHAIN_X86_FLAGS) -I$(TOOLCHAIN_X86_PLATFORM_HEADERS) -fpic -ffunction-sections -funwind-tables -fstack-protector -fno-strict-aliasing -finline-limit=64" \
 CXXFLAGS="-I. $(TOOLCHAIN_X86_FLAGS) -I$(TOOLCHAIN_X86_PLATFORM_HEADERS) -fpic -ffunction-sections -funwind-tables -fstack-protector -fno-strict-aliasing -finline-limit=64 -frtti -fexceptions" \
 CFLAGS="-I. $(TOOLCHAIN_X86_FLAGS) -I$(TOOLCHAIN_X86_PLATFORM_HEADERS) -fpic -ffunction-sections -funwind-tables -fstack-protector -fno-strict-aliasing -finline-limit=64" \
 LDFLAGS="$(TOOLCHAIN_X86_LINK)" \
 PATH="$(shell pwd)/$(TOOLCHAIN_X86)/bin:$$PATH"
# x86_64
TOOLCHAIN_X86_64_ENV = \
 ANDROID_DEV="$(shell pwd)/$(TOOLCHAIN_X86_64)/bin" \
 CC=$(TOOLCHAIN_X86_64_PREFIX)-gcc \
 CXX=$(TOOLCHAIN_X86_64_PREFIX)-g++ \
 LINK=$(TOOLCHAIN_X86_64_PREFIX)-g++ \
 LD=$(TOOLCHAIN_X86_64_PREFIX)-ld \
 AR=$(TOOLCHAIN_X86_64_PREFIX)-ar \
 RANLIB=$(TOOLCHAIN_X86_64_PREFIX)-ranlib \
 STRIP=$(TOOLCHAIN_X86_64_PREFIX)-strip \
 ARCH_FLAGS="$(TOOLCHAIN_X86_64_FLAGS)" \
 ARCH_LINK="$(TOOLCHAIN_X86_64_LINK)" \
 CPPFLAGS="-I. $(TOOLCHAIN_X86_64_FLAGS) -I$(TOOLCHAIN_X86_64_PLATFORM_HEADERS) -fpic -ffunction-sections -funwind-tables -fstack-protector -fno-strict-aliasing -finline-limit=64" \
 CXXFLAGS="-I. $(TOOLCHAIN_X86_64_FLAGS) -I$(TOOLCHAIN_X86_64_PLATFORM_HEADERS) -fpic -ffunction-sections -funwind-tables -fstack-protector -fno-strict-aliasing -finline-limit=64 -frtti -fexceptions" \
 CFLAGS="-I. $(TOOLCHAIN_X86_64_FLAGS) -I$(TOOLCHAIN_X86_64_PLATFORM_HEADERS) -fpic -ffunction-sections -funwind-tables -fstack-protector -fno-strict-aliasing -finline-limit=64" \
 LDFLAGS="$(TOOLCHAIN_X86_64_LINK)" \
 PATH="$(shell pwd)/$(TOOLCHAIN_X86_64)/bin:$$PATH"
# arm
TOOLCHAIN_ARM_ENV = \
 ANDROID_DEV="$(shell pwd)/$(TOOLCHAIN_ARM)/bin" \
 CC=$(TOOLCHAIN_ARM_PREFIX)-gcc \
 CXX=$(TOOLCHAIN_ARM_PREFIX)-g++ \
 LINK=$(TOOLCHAIN_ARM_PREFIX)-g++ \
 LD=$(TOOLCHAIN_ARM_PREFIX)-ld \
 AR=$(TOOLCHAIN_ARM_PREFIX)-ar \
 RANLIB=$(TOOLCHAIN_ARM_PREFIX)-ranlib \
 STRIP=$(TOOLCHAIN_ARM_PREFIX)-strip \
 ARCH_FLAGS="$(TOOLCHAIN_ARM_FLAGS)" \
 ARCH_LINK="$(TOOLCHAIN_ARM_LINK)" \
 CPPFLAGS="-I. $(TOOLCHAIN_ARM_FLAGS) -I$(TOOLCHAIN_ARM_PLATFORM_HEADERS) -fpic -ffunction-sections -funwind-tables -fstack-protector -fno-strict-aliasing -finline-limit=64" \
 CXXFLAGS="-I. $(TOOLCHAIN_ARM_FLAGS) -I$(TOOLCHAIN_ARM_PLATFORM_HEADERS) -fpic -ffunction-sections -funwind-tables -fstack-protector -fno-strict-aliasing -finline-limit=64 -frtti -fexceptions" \
 CFLAGS="-I. $(TOOLCHAIN_ARM_FLAGS) -I$(TOOLCHAIN_ARM_PLATFORM_HEADERS) -fpic -ffunction-sections -funwind-tables -fstack-protector -fno-strict-aliasing -finline-limit=64" \
 LDFLAGS="$(TOOLCHAIN_ARM_LINK)" \
 PATH="$(shell pwd)/$(TOOLCHAIN_ARM)/bin:$$PATH"
# arm-v7a
TOOLCHAIN_ARM_V7A_ENV = \
 ANDROID_DEV="$(shell pwd)/$(TOOLCHAIN_ARM_V7A)/bin" \
 CC=$(TOOLCHAIN_ARM_V7A_PREFIX)-gcc \
 CXX=$(TOOLCHAIN_ARM_V7A_PREFIX)-g++ \
 LINK=$(TOOLCHAIN_ARM_V7A_PREFIX)-g++ \
 LD=$(TOOLCHAIN_ARM_V7A_PREFIX)-ld \
 AR=$(TOOLCHAIN_ARM_V7A_PREFIX)-ar \
 RANLIB=$(TOOLCHAIN_ARM_V7A_PREFIX)-ranlib \
 STRIP=$(TOOLCHAIN_ARM_V7A_PREFIX)-strip \
 ARCH_FLAGS="$(TOOLCHAIN_ARM_V7A_FLAGS)" \
 ARCH_LINK="$(TOOLCHAIN_ARM_V7A_LINK)" \
 CPPFLAGS="-I. $(TOOLCHAIN_ARM_V7A_FLAGS) -I$(TOOLCHAIN_ARM_V7A_PLATFORM_HEADERS) -fpic -ffunction-sections -funwind-tables -fstack-protector -fno-strict-aliasing -finline-limit=64" \
 CXXFLAGS="-I. $(TOOLCHAIN_ARM_V7A_FLAGS) -I$(TOOLCHAIN_ARM_V7A_PLATFORM_HEADERS) -fpic -ffunction-sections -funwind-tables -fstack-protector -fno-strict-aliasing -finline-limit=64 -frtti -fexceptions" \
 CFLAGS="-I. $(TOOLCHAIN_ARM_V7A_FLAGS) -I$(TOOLCHAIN_ARM_V7A_PLATFORM_HEADERS) -fpic -ffunction-sections -funwind-tables -fstack-protector -fno-strict-aliasing -finline-limit=64" \
 LDFLAGS="$(TOOLCHAIN_ARM_V7A_LINK)" \
 PATH="$(shell pwd)/$(TOOLCHAIN_ARM_V7A)/bin:$$PATH"
# arm-v7a-hard
TOOLCHAIN_ARM_V7A_HARD_ENV = \
 ANDROID_DEV="$(shell pwd)/$(TOOLCHAIN_ARM_V7A_HARD)/bin" \
 CC=$(TOOLCHAIN_ARM_V7A_HARD_PREFIX)-gcc \
 CXX=$(TOOLCHAIN_ARM_V7A_HARD_PREFIX)-g++ \
 LINK=$(TOOLCHAIN_ARM_V7A_HARD_PREFIX)-g++ \
 LD=$(TOOLCHAIN_ARM_V7A_HARD_PREFIX)-ld \
 AR=$(TOOLCHAIN_ARM_V7A_HARD_PREFIX)-ar \
 RANLIB=$(TOOLCHAIN_ARM_V7A_HARD_PREFIX)-ranlib \
 STRIP=$(TOOLCHAIN_ARM_V7A_HARD_PREFIX)-strip \
 ARCH_FLAGS="$(TOOLCHAIN_ARM_V7A_HARD_FLAGS)" \
 ARCH_LINK="$(TOOLCHAIN_ARM_V7A_HARD_LINK)" \
 CPPFLAGS="-I. $(TOOLCHAIN_ARM_V7A_HARD_FLAGS) -I$(TOOLCHAIN_ARM_V7A_HARD_PLATFORM_HEADERS) -fpic -ffunction-sections -funwind-tables -fstack-protector -fno-strict-aliasing -finline-limit=64" \
 CXXFLAGS="-I. $(TOOLCHAIN_ARM_V7A_HARD_FLAGS) -I$(TOOLCHAIN_ARM_V7A_HARD_PLATFORM_HEADERS) -fpic -ffunction-sections -funwind-tables -fstack-protector -fno-strict-aliasing -finline-limit=64 -frtti -fexceptions" \
 CFLAGS="-I. $(TOOLCHAIN_ARM_V7A_HARD_FLAGS) -I$(TOOLCHAIN_ARM_V7A_HARD_PLATFORM_HEADERS) -fpic -ffunction-sections -funwind-tables -fstack-protector -fno-strict-aliasing -finline-limit=64" \
 LDFLAGS="$(TOOLCHAIN_ARM_V7A_HARD_LINK)" \
 PATH="$(shell pwd)/$(TOOLCHAIN_ARM_V7A_HARD)/bin:$$PATH"
# arm64-v8a
TOOLCHAIN_ARM64_V8A_ENV = \
 ANDROID_DEV="$(shell pwd)/$(TOOLCHAIN_ARM64_V8A)/bin" \
 CC=$(TOOLCHAIN_ARM64_V8A_PREFIX)-gcc \
 CXX=$(TOOLCHAIN_ARM64_V8A_PREFIX)-g++ \
 LINK=$(TOOLCHAIN_ARM64_V8A_PREFIX)-g++ \
 LD=$(TOOLCHAIN_ARM64_V8A_PREFIX)-ld \
 AR=$(TOOLCHAIN_ARM64_V8A_PREFIX)-ar \
 RANLIB=$(TOOLCHAIN_ARM64_V8A_PREFIX)-ranlib \
 STRIP=$(TOOLCHAIN_ARM64_V8A_PREFIX)-strip \
 ARCH_FLAGS="$(TOOLCHAIN_ARM64_V8A_FLAGS)" \
 ARCH_LINK="$(TOOLCHAIN_ARM64_V8A_LINK)" \
 CPPFLAGS="-I. $(TOOLCHAIN_ARM64_V8A_FLAGS) -I$(TOOLCHAIN_ARM64_V8A_PLATFORM_HEADERS) -fpic -ffunction-sections -funwind-tables -fstack-protector -fno-strict-aliasing -finline-limit=64" \
 CXXFLAGS="-I. $(TOOLCHAIN_ARM64_V8A_FLAGS) -I$(TOOLCHAIN_ARM64_V8A_PLATFORM_HEADERS) -fpic -ffunction-sections -funwind-tables -fstack-protector -fno-strict-aliasing -finline-limit=64 -frtti -fexceptions" \
 CFLAGS="-I. $(TOOLCHAIN_ARM64_V8A_FLAGS) -I$(TOOLCHAIN_ARM64_V8A_PLATFORM_HEADERS) -fpic -ffunction-sections -funwind-tables -fstack-protector -fno-strict-aliasing -finline-limit=64" \
 LDFLAGS="$(TOOLCHAIN_ARM64_V8A_LINK)" \
 PATH="$(shell pwd)/$(TOOLCHAIN_ARM64_V8A)/bin:$$PATH"
# mips
TOOLCHAIN_MIPS_ENV = \
 ANDROID_DEV="$(shell pwd)/$(TOOLCHAIN_MIPS)/bin" \
 CC=$(TOOLCHAIN_MIPS_PREFIX)-gcc \
 CXX=$(TOOLCHAIN_MIPS_PREFIX)-g++ \
 LINK=$(TOOLCHAIN_MIPS_PREFIX)-g++ \
 LD=$(TOOLCHAIN_MIPS_PREFIX)-ld \
 AR=$(TOOLCHAIN_MIPS_PREFIX)-ar \
 RANLIB=$(TOOLCHAIN_MIPS_PREFIX)-ranlib \
 STRIP=$(TOOLCHAIN_MIPS_PREFIX)-strip \
 ARCH_FLAGS="$(TOOLCHAIN_MIPS_FLAGS)" \
 ARCH_LINK="$(TOOLCHAIN_MIPS_LINK)" \
 CPPFLAGS="-I. $(TOOLCHAIN_MIPS_FLAGS) -I$(TOOLCHAIN_MIPS_PLATFORM_HEADERS) -fpic -ffunction-sections -funwind-tables -fstack-protector -fno-strict-aliasing -finline-limit=64" \
 CXXFLAGS="-I. $(TOOLCHAIN_MIPS_FLAGS) -I$(TOOLCHAIN_MIPS_PLATFORM_HEADERS) -fpic -ffunction-sections -funwind-tables -fstack-protector -fno-strict-aliasing -finline-limit=64 -frtti -fexceptions" \
 CFLAGS="-I. $(TOOLCHAIN_MIPS_FLAGS) -I$(TOOLCHAIN_MIPS_PLATFORM_HEADERS) -fpic -ffunction-sections -funwind-tables -fstack-protector -fno-strict-aliasing -finline-limit=64" \
 LDFLAGS="$(TOOLCHAIN_MIPS_LINK)" \
 PATH="$(shell pwd)/$(TOOLCHAIN_MIPS)/bin:$$PATH"
# mips64
TOOLCHAIN_MIPS64_ENV = \
 ANDROID_DEV="$(shell pwd)/$(TOOLCHAIN_MIPS64)/bin" \
 CC=$(TOOLCHAIN_MIPS64_PREFIX)-gcc \
 CXX=$(TOOLCHAIN_MIPS64_PREFIX)-g++ \
 LINK=$(TOOLCHAIN_MIPS64_PREFIX)-g++ \
 LD=$(TOOLCHAIN_MIPS64_PREFIX)-ld \
 AR=$(TOOLCHAIN_MIPS64_PREFIX)-ar \
 RANLIB=$(TOOLCHAIN_MIPS64_PREFIX)-ranlib \
 STRIP=$(TOOLCHAIN_MIPS64_PREFIX)-strip \
 ARCH_FLAGS="$(TOOLCHAIN_MIPS64_FLAGS)" \
 ARCH_LINK="$(TOOLCHAIN_MIPS64_LINK)" \
 CPPFLAGS="-I. $(TOOLCHAIN_MIPS64_FLAGS) -I$(TOOLCHAIN_MIPS64_PLATFORM_HEADERS) -fpic -ffunction-sections -funwind-tables -fstack-protector -fno-strict-aliasing -finline-limit=64" \
 CXXFLAGS="-I. $(TOOLCHAIN_MIPS64_FLAGS) -I$(TOOLCHAIN_MIPS64_PLATFORM_HEADERS) -fpic -ffunction-sections -funwind-tables -fstack-protector -fno-strict-aliasing -finline-limit=64 -frtti -fexceptions" \
 CFLAGS="-I. $(TOOLCHAIN_MIPS64_FLAGS) -I$(TOOLCHAIN_MIPS64_PLATFORM_HEADERS) -fpic -ffunction-sections -funwind-tables -fstack-protector -fno-strict-aliasing -finline-limit=64" \
 LDFLAGS="$(TOOLCHAIN_MIPS64_LINK)" \
 PATH="$(shell pwd)/$(TOOLCHAIN_MIPS64)/bin:$$PATH"

#
# All the external tools we use in this Makefile
#

AWK := awk
CD := cd
CMAKE := cmake
ECHO := echo
EGREP := egrep
GIT := git
LN := ln
MKDIR := mkdir
RM := rm
SORT := sort
TAR := tar
WGET := wget

#
# End of user configurable options.
#

.PHONY: \
 all \
 all-x86 \
 all-x86_64 \
 all-armeabi \
 all-armeabi-v7a \
 all-armeabi-v7a-hard \
 all-arm64-v8a \
 all-mips \
 all-mips64 \
 common \
 sources \
 toolchains \
 toolchain-x86 \
 toolchain-x86_64 \
 toolchain-armeabi \
 toolchain-armeabi-v7a \
 toolchain-armeabi-v7a-hard \
 toolchain-arm64-v8a \
 toolchain-mips \
 toolchain-mips64 \
 zlib \
 zlib-x86 \
 zlib-x86_64 \
 zlib-armeabi \
 zlib-armeabi-v7a \
 zlib-armeabi-v7a-hard \
 zlib-arm64-v8a \
 zlib-mips \
 zlib-mips64 \
 openssl \
 openssl-x86 \
 openssl-x86_64 \
 openssl-armeabi \
 openssl-armeabi-v7a \
 openssl-armeabi-v7a-hard \
 openssl-arm64-v8a \
 openssl-mips \
 openssl-mips64 \
 libwebsockets \
 libwebsockets-x86 \
 libwebsockets-x86_64 \
 libwebsockets-armeabi \
 libwebsockets-armeabi-v7a \
 libwebsockets-armeabi-v7a-hard \
 libwebsockets-arm64-v8a \
 libwebsockets-mips \
 libwebsockets-mips64 \
 clean-ndk \
 clean \
 dist-clean \
 clean-targets \
 clean-target-x86 \
 clean-target-x86_64 \
 clean-target-armeabi \
 clean-target-armeabi-v7a \
 clean-target-armeabi-v7a-hard \
 clean-target-arm64-v8a \
 clean-target-mips \
 clean-target-mips64 \
 clean-sources \
 clean-source-zlib \
 clean-source-openssl \
 clean-source-libwebsockets \
 clean-toolchains \
 clean-toolchain-x86 \
 clean-toolchain-x86_64 \
 clean-toolchain-armeabi \
 clean-toolchain-armeabi-v7a \
 clean-toolchain-armeabi-v7a-hard \
 clean-toolchain-arm64-v8a \
 clean-toolchain-mips \
 clean-toolchain-mips64 \
 list-targets

# Default rule: build the libraries for all ABIs defined in NDK_APP_ABI then run ndk-build
all: $(NDK_APP_ABI)
	$(NDK_ROOT)/ndk-build clean
	$(NDK_ROOT)/ndk-build

# Libraries may also be build per ABI
all-x86: $(TARGET_X86)
all-x86_64: $(TARGET_X86_64)
all-armeabi: $(TARGET_ARM)
all-armeabi-v7a: $(TARGET_ARM_V7A)
all-armeabi-v7a-hard: $(TARGET_ARM_V7A_HARD)
all-arm64-v8a: $(TARGET_ARM64_V8A)
all-mips: $(TARGET_MIPS)
all-mips64: $(TARGET_MIPS64)

# Common rule all targets depend on
common: ../jniLibs

# These rules are called from Android.mk when executing ndk-build
$(TARGET_X86): common zlib-x86 openssl-x86 libwebsockets-x86
$(TARGET_X86_64): common zlib-x86_64 openssl-x86_64 libwebsockets-x86_64
$(TARGET_ARM): common zlib-armeabi openssl-armeabi libwebsockets-armeabi
$(TARGET_ARM_V7A): common zlib-armeabi-v7a openssl-armeabi-v7a libwebsockets-armeabi-v7a
$(TARGET_ARM_V7A_HARD): common zlib-armeabi-v7a-hard openssl-armeabi-v7a-hard libwebsockets-armeabi-v7a-hard
$(TARGET_ARM64_V8A): common zlib-arm64-v8a openssl-arm64-v8a libwebsockets-arm64-v8a
$(TARGET_MIPS): common zlib-mips openssl-mips libwebsockets-mips
$(TARGET_MIPS64): common zlib-mips64 openssl-mips64 libwebsockets-mips64

#
# A rule to ensure ../jniLibs points to ../libs
# (ndk-build creates ../libs but Gradle looks for ../jniLibs)
#

../libs:
	$(MKDIR) ../libs

../jniLibs: ../libs
	$(CD) .. && $(LN) -s libs jniLibs

#
# Some rules to download the sources
#

sources: $(ZLIB_TGZ_SOURCE) $(OPENSSL_TGZ_SOURCE) $(LIBWEBSOCKETS_TGZ_SOURCE)

$(ZLIB_TGZ_SOURCE):
	$(WGET) -q $(ZLIB_TGZ_URL)

$(OPENSSL_TGZ_SOURCE):
	$(WGET) -q $(OPENSSL_TGZ_URL)

$(LIBWEBSOCKETS_TGZ_SOURCE):
	if [ -d $(LIBWEBSOCKETS_DIR) ]; then $(RM) -fr $(LIBWEBSOCKETS_DIR); fi
	$(GIT) clone $(LIBWEBSOCKETS_GIT_URL)
	$(TAR) caf $(LIBWEBSOCKETS_TGZ_SOURCE) $(LIBWEBSOCKETS_DIR)
	$(RM) -fR $(LIBWEBSOCKETS_DIR)

#
# Some rules to install the required toolchains
#

toolchains: \
 toolchain-x86 \
 toolchain-x86_64 \
 toolchain-armeabi \
 toolchain-armeabi-v7a \
 toolchain-armeabi-v7a-hard \
 toolchain-arm64-v8a \
 toolchain-mips \
 toolchain-mips64

toolchain-x86: $(TOOLCHAIN_X86)
toolchain-x86_64: $(TOOLCHAIN_X86_64)
toolchain-armeabi: $(TOOLCHAIN_ARM)
toolchain-armeabi-v7a: $(TOOLCHAIN_ARM_V7A)
toolchain-armeabi-v7a-hard: $(TOOLCHAIN_ARM_V7A_HARD)
toolchain-arm64-v8a: $(TOOLCHAIN_ARM64_V8A)
toolchain-mips: $(TOOLCHAIN_MIPS)
toolchain-mips64: $(TOOLCHAIN_MIPS64)

$(TOOLCHAIN_X86):
ifneq ($(TOOLCHAIN_STL),)
	$(NDK_MAKE_TOOLCHAIN) \
	  --stl $(TOOLCHAIN_STL) \
	  --api $(TOOLCHAIN_X86_API) \
	  --arch x86 \
	  --install-dir $(shell pwd)/$(TOOLCHAIN_X86)
else
	$(NDK_MAKE_TOOLCHAIN) \
	  --api $(TOOLCHAIN_X86_API) \
	  --arch x86 \
	  --install-dir $(shell pwd)/$(TOOLCHAIN_X86)
endif

$(TOOLCHAIN_X86_64):
ifneq ($(TOOLCHAIN_STL),)
	$(NDK_MAKE_TOOLCHAIN) \
	  --stl $(TOOLCHAIN_STL) \
	  --api $(TOOLCHAIN_X86_64_API) \
	  --arch x86_64 \
	  --install-dir $(shell pwd)/$(TOOLCHAIN_X86_64)
else
	$(NDK_MAKE_TOOLCHAIN) \
	  --api $(TOOLCHAIN_X86_64_API) \
	  --arch x86_64 \
	  --install-dir $(shell pwd)/$(TOOLCHAIN_X86_64)
endif

$(TOOLCHAIN_ARM):
ifneq ($(TOOLCHAIN_STL),)
	$(NDK_MAKE_TOOLCHAIN) \
	  --stl $(TOOLCHAIN_STL) \
	  --api $(TOOLCHAIN_ARM_API) \
	  --arch arm \
	  --install-dir $(shell pwd)/$(TOOLCHAIN_ARM)
else
	$(NDK_MAKE_TOOLCHAIN) \
	  --api $(TOOLCHAIN_ARM_API) \
	  --arch arm \
	  --install-dir $(shell pwd)/$(TOOLCHAIN_ARM)
endif

$(TOOLCHAIN_ARM_V7A):
ifneq ($(TOOLCHAIN_STL),)
	$(NDK_MAKE_TOOLCHAIN) \
	  --stl $(TOOLCHAIN_STL) \
	  --api $(TOOLCHAIN_ARM_V7A_API) \
	  --arch arm \
	  --install-dir $(shell pwd)/$(TOOLCHAIN_ARM_V7A)
else
	$(NDK_MAKE_TOOLCHAIN) \
	  --api $(TOOLCHAIN_ARM_V7A_API) \
	  --arch arm \
	  --install-dir $(shell pwd)/$(TOOLCHAIN_ARM_V7A)
endif

$(TOOLCHAIN_ARM_V7A_HARD):
ifneq ($(TOOLCHAIN_STL),)
	$(NDK_MAKE_TOOLCHAIN) \
	  --stl $(TOOLCHAIN_STL) \
	  --api $(TOOLCHAIN_ARM_V7A_HARD_API) \
	  --arch arm \
	  --install-dir $(shell pwd)/$(TOOLCHAIN_ARM_V7A_HARD)
else
	$(NDK_MAKE_TOOLCHAIN) \
	  --api $(TOOLCHAIN_ARM_V7A_HARD_API) \
	  --arch arm \
	  --install-dir $(shell pwd)/$(TOOLCHAIN_ARM_V7A_HARD)
endif

$(TOOLCHAIN_ARM64_V8A):
ifneq ($(TOOLCHAIN_STL),)
	$(NDK_MAKE_TOOLCHAIN) \
	  --stl $(TOOLCHAIN_STL) \
	  --api $(TOOLCHAIN_ARM64_V8A_API) \
	  --arch arm64 \
	  --install-dir $(shell pwd)/$(TOOLCHAIN_ARM64_V8A)
else
	$(NDK_MAKE_TOOLCHAIN) \
	  --api $(TOOLCHAIN_ARM64_V8A_API) \
	  --arch arm64 \
	  --install-dir $(shell pwd)/$(TOOLCHAIN_ARM64_V8A)
endif

$(TOOLCHAIN_MIPS):
ifneq ($(TOOLCHAIN_STL),)
	$(NDK_MAKE_TOOLCHAIN) \
	  --stl $(TOOLCHAIN_STL) \
	  --api $(TOOLCHAIN_MIPS_API) \
	  --arch mips \
	  --install-dir $(shell pwd)/$(TOOLCHAIN_MIPS)
else
	$(NDK_MAKE_TOOLCHAIN) \
	  --api $(TOOLCHAIN_MIPS_API) \
	  --arch mips \
	  --install-dir $(shell pwd)/$(TOOLCHAIN_MIPS)
endif

$(TOOLCHAIN_MIPS64):
ifneq ($(TOOLCHAIN_STL),)
	$(NDK_MAKE_TOOLCHAIN) \
	  --stl $(TOOLCHAIN_STL) \
	  --api $(TOOLCHAIN_MIPS64_API) \
	  --arch mips64 \
	  --install-dir $(shell pwd)/$(TOOLCHAIN_MIPS64)
else
	$(NDK_MAKE_TOOLCHAIN) \
	  --api $(TOOLCHAIN_MIPS64_API) \
	  --arch mips64 \
	  --install-dir $(shell pwd)/$(TOOLCHAIN_MIPS64)
endif

#
# Rules to build zlib
#

zlib: \
 zlib-x86 \
 zlib-x86_64 \
 zlib-armeabi \
 zlib-armeabi-v7a \
 zlib-armeabi-v7a-hard \
 zlib-arm64-v8a \
 zlib-mips \
 zlib-mips64

zlib-x86: $(TARGET_X86)/lib/libz.a
zlib-x86_64: $(TARGET_X86_64)/lib/libz.a
zlib-armeabi: $(TARGET_ARM)/lib/libz.a
zlib-armeabi-v7a: $(TARGET_ARM_V7A)/lib/libz.a
zlib-armeabi-v7a-hard: $(TARGET_ARM_V7A_HARD)/lib/libz.a
zlib-arm64-v8a: $(TARGET_ARM64_V8A)/lib/libz.a
zlib-mips: $(TARGET_MIPS)/lib/libz.a
zlib-mips64: $(TARGET_MIPS64)/lib/libz.a

# Extracting/configuring sources

$(TARGET_X86)/src/$(ZLIB_DIR): $(ZLIB_TGZ_SOURCE) $(TOOLCHAIN_X86)
	-$(MKDIR) -p $(TARGET_X86)/src
	$(TAR) xf $(ZLIB_TGZ_SOURCE) -C $(TARGET_X86)/src
	$(CD) $(TARGET_X86)/src/$(ZLIB_DIR) && $(TOOLCHAIN_X86_ENV) \
	  ./configure --static --prefix=$(shell pwd)/$(TARGET_X86)

$(TARGET_X86_64)/src/$(ZLIB_DIR): $(ZLIB_TGZ_SOURCE) $(TOOLCHAIN_X86_64)
	-$(MKDIR) -p $(TARGET_X86_64)/src
	$(TAR) xf $(ZLIB_TGZ_SOURCE) -C $(TARGET_X86_64)/src
	$(CD) $(TARGET_X86_64)/src/$(ZLIB_DIR) && $(TOOLCHAIN_X86_64_ENV) \
	  ./configure --static --prefix=$(shell pwd)/$(TARGET_X86_64)

$(TARGET_ARM)/src/$(ZLIB_DIR): $(ZLIB_TGZ_SOURCE) $(TOOLCHAIN_ARM)
	-$(MKDIR) -p $(TARGET_ARM)/src
	$(TAR) xf $(ZLIB_TGZ_SOURCE) -C $(TARGET_ARM)/src
	$(CD) $(TARGET_ARM)/src/$(ZLIB_DIR) && $(TOOLCHAIN_ARM_ENV) \
	  ./configure --static --prefix=$(shell pwd)/$(TARGET_ARM)

$(TARGET_ARM_V7A)/src/$(ZLIB_DIR): $(ZLIB_TGZ_SOURCE) $(TOOLCHAIN_ARM_V7A)
	-$(MKDIR) -p $(TARGET_ARM_V7A)/src
	$(TAR) xf $(ZLIB_TGZ_SOURCE) -C $(TARGET_ARM_V7A)/src
	$(CD) $(TARGET_ARM_V7A)/src/$(ZLIB_DIR) && $(TOOLCHAIN_ARM_V7A_ENV) \
	  ./configure --static --prefix=$(shell pwd)/$(TARGET_ARM_V7A)

$(TARGET_ARM_V7A_HARD)/src/$(ZLIB_DIR): $(ZLIB_TGZ_SOURCE) $(TOOLCHAIN_ARM_V7A_HARD)
	-$(MKDIR) -p $(TARGET_ARM_V7A_HARD)/src
	$(TAR) xf $(ZLIB_TGZ_SOURCE) -C $(TARGET_ARM_V7A_HARD)/src
	$(CD) $(TARGET_ARM_V7A_HARD)/src/$(ZLIB_DIR) && $(TOOLCHAIN_ARM_V7A_HARD_ENV) \
	  ./configure --static --prefix=$(shell pwd)/$(TARGET_ARM_V7A_HARD)

$(TARGET_ARM64_V8A)/src/$(ZLIB_DIR): $(ZLIB_TGZ_SOURCE) $(TOOLCHAIN_ARM64_V8A)
	-$(MKDIR) -p $(TARGET_ARM64_V8A)/src
	$(TAR) xf $(ZLIB_TGZ_SOURCE) -C $(TARGET_ARM64_V8A)/src
	$(CD) $(TARGET_ARM64_V8A)/src/$(ZLIB_DIR) && $(TOOLCHAIN_ARM64_V8A_ENV) \
	  ./configure --static --prefix=$(shell pwd)/$(TARGET_ARM64_V8A)

$(TARGET_MIPS)/src/$(ZLIB_DIR): $(ZLIB_TGZ_SOURCE) $(TOOLCHAIN_MIPS)
	-$(MKDIR) -p $(TARGET_MIPS)/src
	$(TAR) xf $(ZLIB_TGZ_SOURCE) -C $(TARGET_MIPS)/src
	$(CD) $(TARGET_MIPS)/src/$(ZLIB_DIR) && $(TOOLCHAIN_MIPS_ENV) \
	  ./configure --static --prefix=$(shell pwd)/$(TARGET_MIPS)

$(TARGET_MIPS64)/src/$(ZLIB_DIR): $(ZLIB_TGZ_SOURCE) $(TOOLCHAIN_MIPS64)
	-$(MKDIR) -p $(TARGET_MIPS64)/src
	$(TAR) xf $(ZLIB_TGZ_SOURCE) -C $(TARGET_MIPS64)/src
	$(CD) $(TARGET_MIPS64)/src/$(ZLIB_DIR) && $(TOOLCHAIN_MIPS64_ENV) \
	  ./configure --static --prefix=$(shell pwd)/$(TARGET_MIPS64)

# Build/install library

$(TARGET_X86)/lib/libz.a: $(TARGET_X86)/src/$(ZLIB_DIR)
	$(CD) $(TARGET_X86)/src/$(ZLIB_DIR) && $(TOOLCHAIN_X86_ENV) $(MAKE) libz.a
	$(CD) $(TARGET_X86)/src/$(ZLIB_DIR) && $(TOOLCHAIN_X86_ENV) $(MAKE) install

$(TARGET_X86_64)/lib/libz.a: $(TARGET_X86_64)/src/$(ZLIB_DIR)
	$(CD) $(TARGET_X86_64)/src/$(ZLIB_DIR) && $(TOOLCHAIN_X86_64_ENV) $(MAKE) libz.a
	$(CD) $(TARGET_X86_64)/src/$(ZLIB_DIR) && $(TOOLCHAIN_X86_64_ENV) $(MAKE) install

$(TARGET_ARM)/lib/libz.a: $(TARGET_ARM)/src/$(ZLIB_DIR)
	$(CD) $(TARGET_ARM)/src/$(ZLIB_DIR) && $(TOOLCHAIN_ARM_ENV) $(MAKE) libz.a
	$(CD) $(TARGET_ARM)/src/$(ZLIB_DIR) && $(TOOLCHAIN_ARM_ENV) $(MAKE) install

$(TARGET_ARM_V7A)/lib/libz.a: $(TARGET_ARM_V7A)/src/$(ZLIB_DIR)
	$(CD) $(TARGET_ARM_V7A)/src/$(ZLIB_DIR) && $(TOOLCHAIN_ARM_V7A_ENV) $(MAKE) libz.a
	$(CD) $(TARGET_ARM_V7A)/src/$(ZLIB_DIR) && $(TOOLCHAIN_ARM_V7A_ENV) $(MAKE) install

$(TARGET_ARM_V7A_HARD)/lib/libz.a: $(TARGET_ARM_V7A_HARD)/src/$(ZLIB_DIR)
	$(CD) $(TARGET_ARM_V7A_HARD)/src/$(ZLIB_DIR) && $(TOOLCHAIN_ARM_V7A_HARD_ENV) $(MAKE) libz.a
	$(CD) $(TARGET_ARM_V7A_HARD)/src/$(ZLIB_DIR) && $(TOOLCHAIN_ARM_V7A_HARD_ENV) $(MAKE) install

$(TARGET_ARM64_V8A)/lib/libz.a: $(TARGET_ARM64_V8A)/src/$(ZLIB_DIR)
	$(CD) $(TARGET_ARM64_V8A)/src/$(ZLIB_DIR) && $(TOOLCHAIN_ARM64_V8A_ENV) $(MAKE) libz.a
	$(CD) $(TARGET_ARM64_V8A)/src/$(ZLIB_DIR) && $(TOOLCHAIN_ARM64_V8A_ENV) $(MAKE) install

$(TARGET_MIPS)/lib/libz.a: $(TARGET_MIPS)/src/$(ZLIB_DIR)
	$(CD) $(TARGET_MIPS)/src/$(ZLIB_DIR) && $(TOOLCHAIN_MIPS_ENV) $(MAKE) libz.a
	$(CD) $(TARGET_MIPS)/src/$(ZLIB_DIR) && $(TOOLCHAIN_MIPS_ENV) $(MAKE) install

$(TARGET_MIPS64)/lib/libz.a: $(TARGET_MIPS64)/src/$(ZLIB_DIR)
	$(CD) $(TARGET_MIPS64)/src/$(ZLIB_DIR) && $(TOOLCHAIN_MIPS64_ENV) $(MAKE) libz.a
	$(CD) $(TARGET_MIPS64)/src/$(ZLIB_DIR) && $(TOOLCHAIN_MIPS64_ENV) $(MAKE) install

#
# Rules to build OpenSSL
#

openssl: \
 openssl-x86 \
 openssl-x86_64 \
 openssl-armeabi \
 openssl-armeabi-v7a \
 openssl-armeabi-v7a-hard \
 openssl-arm64-v8a \
 openssl-mips \
 openssl-mips64

openssl-x86: $(TARGET_X86)/lib/libssl.a
openssl-x86_64: $(TARGET_X86_64)/lib/libssl.a
openssl-armeabi: $(TARGET_ARM)/lib/libssl.a
openssl-armeabi-v7a: $(TARGET_ARM_V7A)/lib/libssl.a
openssl-armeabi-v7a-hard: $(TARGET_ARM_V7A_HARD)/lib/libssl.a
openssl-arm64-v8a: $(TARGET_ARM64_V8A)/lib/libssl.a
openssl-mips: $(TARGET_MIPS)/lib/libssl.a
openssl-mips64: $(TARGET_MIPS64)/lib/libssl.a

# Extracting/configuring sources

$(TARGET_X86)/src/$(OPENSSL_DIR): $(OPENSSL_TGZ_SOURCE) $(TOOLCHAIN_X86)
	-$(MKDIR) -p $(TARGET_X86)/src
	$(TAR) xf $(OPENSSL_TGZ_SOURCE) -C $(TARGET_X86)/src
	$(CD) $(TARGET_X86)/src/$(OPENSSL_DIR) && $(TOOLCHAIN_X86_ENV) \
	  ./Configure $(TARGET_X86_OPENSSL_CONFIG_TARGET) \
	    --prefix=$(shell pwd)/$(TARGET_X86) \
	    --openssldir=$(shell pwd)/$(TARGET_X86)/lib/ssl \
	    $(TARGET_X86_OPENSSL_CONFIG)

$(TARGET_X86_64)/src/$(OPENSSL_DIR): $(OPENSSL_TGZ_SOURCE) $(TOOLCHAIN_X86_64)
	-$(MKDIR) -p $(TARGET_X86_64)/src
	$(TAR) xf $(OPENSSL_TGZ_SOURCE) -C $(TARGET_X86_64)/src
	$(CD) $(TARGET_X86_64)/src/$(OPENSSL_DIR) && $(TOOLCHAIN_X86_64_ENV) \
	  ./Configure $(TARGET_X86_64_OPENSSL_CONFIG_TARGET) \
	    --prefix=$(shell pwd)/$(TARGET_X86_64) \
	    --openssldir=$(shell pwd)/$(TARGET_X86_64)/lib/ssl \
	    $(TARGET_X86_64_OPENSSL_CONFIG)

$(TARGET_ARM)/src/$(OPENSSL_DIR): $(OPENSSL_TGZ_SOURCE) $(TOOLCHAIN_ARM)
	-$(MKDIR) -p $(TARGET_ARM)/src
	$(TAR) xf $(OPENSSL_TGZ_SOURCE) -C $(TARGET_ARM)/src
	$(CD) $(TARGET_ARM)/src/$(OPENSSL_DIR) && $(TOOLCHAIN_ARM_ENV) \
	  ./Configure $(TARGET_ARM_OPENSSL_CONFIG_TARGET) \
	    --prefix=$(shell pwd)/$(TARGET_ARM) \
	    --openssldir=$(shell pwd)/$(TARGET_ARM)/lib/ssl \
	    $(TARGET_ARM_OPENSSL_CONFIG)

$(TARGET_ARM_V7A)/src/$(OPENSSL_DIR): $(OPENSSL_TGZ_SOURCE) $(TOOLCHAIN_ARM_V7A)
	-$(MKDIR) -p $(TARGET_ARM_V7A)/src
	$(TAR) xf $(OPENSSL_TGZ_SOURCE) -C $(TARGET_ARM_V7A)/src
	$(CD) $(TARGET_ARM_V7A)/src/$(OPENSSL_DIR) && $(TOOLCHAIN_ARM_V7A_ENV) \
	  ./Configure $(TARGET_ARM_V7A_OPENSSL_CONFIG_TARGET) \
	    --prefix=$(shell pwd)/$(TARGET_ARM_V7A) \
	    --openssldir=$(shell pwd)/$(TARGET_ARM_V7A)/lib/ssl \
	    $(TARGET_ARM_V7A_OPENSSL_CONFIG)

$(TARGET_ARM_V7A_HARD)/src/$(OPENSSL_DIR): $(OPENSSL_TGZ_SOURCE) $(TOOLCHAIN_ARM_V7A_HARD)
	-$(MKDIR) -p $(TARGET_ARM_V7A_HARD)/src
	$(TAR) xf $(OPENSSL_TGZ_SOURCE) -C $(TARGET_ARM_V7A_HARD)/src
	$(CD) $(TARGET_ARM_V7A_HARD)/src/$(OPENSSL_DIR) && $(TOOLCHAIN_ARM_V7A_HARD_ENV) \
	  ./Configure $(TARGET_ARM_V7A_HARD_OPENSSL_CONFIG_TARGET) \
	    --prefix=$(shell pwd)/$(TARGET_ARM_V7A_HARD) \
	    --openssldir=$(shell pwd)/$(TARGET_ARM_V7A_HARD)/lib/ssl \
	    $(TARGET_ARM_V7A_HARD_OPENSSL_CONFIG)

$(TARGET_ARM64_V8A)/src/$(OPENSSL_DIR): $(OPENSSL_TGZ_SOURCE) $(TOOLCHAIN_ARM64_V8A)
	-$(MKDIR) -p $(TARGET_ARM64_V8A)/src
	$(TAR) xf $(OPENSSL_TGZ_SOURCE) -C $(TARGET_ARM64_V8A)/src
	$(CD) $(TARGET_ARM64_V8A)/src/$(OPENSSL_DIR) && $(TOOLCHAIN_ARM64_V8A_ENV) \
	  ./Configure $(TARGET_ARM64_V8A_OPENSSL_CONFIG_TARGET) \
	    --prefix=$(shell pwd)/$(TARGET_ARM64_V8A) \
	    --openssldir=$(shell pwd)/$(TARGET_ARM64_V8A)/lib/ssl \
	    $(TARGET_ARM64_V8A_OPENSSL_CONFIG)

$(TARGET_MIPS)/src/$(OPENSSL_DIR): $(OPENSSL_TGZ_SOURCE) $(TOOLCHAIN_MIPS)
	-$(MKDIR) -p $(TARGET_MIPS)/src
	$(TAR) xf $(OPENSSL_TGZ_SOURCE) -C $(TARGET_MIPS)/src
	$(CD) $(TARGET_MIPS)/src/$(OPENSSL_DIR) && $(TOOLCHAIN_MIPS_ENV) \
	  ./Configure $(TARGET_MIPS_OPENSSL_CONFIG_TARGET) \
	    --prefix=$(shell pwd)/$(TARGET_MIPS) \
	    --openssldir=$(shell pwd)/$(TARGET_MIPS)/lib/ssl \
	    $(TARGET_MIPS_OPENSSL_CONFIG)

$(TARGET_MIPS64)/src/$(OPENSSL_DIR): $(OPENSSL_TGZ_SOURCE) $(TOOLCHAIN_MIPS64)
	-$(MKDIR) -p $(TARGET_MIPS64)/src
	$(TAR) xf $(OPENSSL_TGZ_SOURCE) -C $(TARGET_MIPS64)/src
	$(CD) $(TARGET_MIPS64)/src/$(OPENSSL_DIR) && $(TOOLCHAIN_MIPS64_ENV) \
	  ./Configure $(TARGET_MIPS64_OPENSSL_CONFIG_TARGET) \
	    --prefix=$(shell pwd)/$(TARGET_MIPS64) \
	    --openssldir=$(shell pwd)/$(TARGET_MIPS64)/lib/ssl \
	    $(TARGET_MIPS64_OPENSSL_CONFIG)

# Build/install library

$(TARGET_X86)/lib/libssl.a: $(TARGET_X86)/src/$(OPENSSL_DIR)
	$(CD) $(TARGET_X86)/src/$(OPENSSL_DIR) && $(TOOLCHAIN_X86_ENV) $(MAKE) depend
	$(CD) $(TARGET_X86)/src/$(OPENSSL_DIR) && $(TOOLCHAIN_X86_ENV) $(MAKE) build_libs
	$(CD) $(TARGET_X86)/src/$(OPENSSL_DIR) && $(ECHO) '#!/bin/sh\n\nfalse\n' > apps/openssl
	$(CD) $(TARGET_X86)/src/$(OPENSSL_DIR) && $(TOOLCHAIN_X86_ENV) $(MAKE) install_sw

$(TARGET_X86_64)/lib/libssl.a: $(TARGET_X86_64)/src/$(OPENSSL_DIR)
	$(CD) $(TARGET_X86_64)/src/$(OPENSSL_DIR) && $(TOOLCHAIN_X86_64_ENV) $(MAKE) depend
	$(CD) $(TARGET_X86_64)/src/$(OPENSSL_DIR) && $(TOOLCHAIN_X86_64_ENV) $(MAKE) build_libs
	$(CD) $(TARGET_X86_64)/src/$(OPENSSL_DIR) && $(ECHO) '#!/bin/sh\n\nfalse\n' > apps/openssl
	$(CD) $(TARGET_X86_64)/src/$(OPENSSL_DIR) && $(TOOLCHAIN_X86_64_ENV) $(MAKE) install_sw

$(TARGET_ARM)/lib/libssl.a: $(TARGET_ARM)/src/$(OPENSSL_DIR)
	$(CD) $(TARGET_ARM)/src/$(OPENSSL_DIR) && $(TOOLCHAIN_ARM_ENV) $(MAKE) depend
	$(CD) $(TARGET_ARM)/src/$(OPENSSL_DIR) && $(TOOLCHAIN_ARM_ENV) $(MAKE) build_libs
	$(CD) $(TARGET_ARM)/src/$(OPENSSL_DIR) && $(ECHO) '#!/bin/sh\n\nfalse\n' > apps/openssl
	$(CD) $(TARGET_ARM)/src/$(OPENSSL_DIR) && $(TOOLCHAIN_ARM_ENV) $(MAKE) install_sw

$(TARGET_ARM_V7A)/lib/libssl.a: $(TARGET_ARM_V7A)/src/$(OPENSSL_DIR)
	$(CD) $(TARGET_ARM_V7A)/src/$(OPENSSL_DIR) && $(TOOLCHAIN_ARM_V7A_ENV) $(MAKE) depend
	$(CD) $(TARGET_ARM_V7A)/src/$(OPENSSL_DIR) && $(TOOLCHAIN_ARM_V7A_ENV) $(MAKE) build_libs
	$(CD) $(TARGET_ARM_V7A)/src/$(OPENSSL_DIR) && $(ECHO) '#!/bin/sh\n\nfalse\n' > apps/openssl
	$(CD) $(TARGET_ARM_V7A)/src/$(OPENSSL_DIR) && $(TOOLCHAIN_ARM_V7A_ENV) $(MAKE) install_sw

$(TARGET_ARM_V7A_HARD)/lib/libssl.a: $(TARGET_ARM_V7A_HARD)/src/$(OPENSSL_DIR)
	$(CD) $(TARGET_ARM_V7A_HARD)/src/$(OPENSSL_DIR) && $(TOOLCHAIN_ARM_V7A_HARD_ENV) $(MAKE) depend
	$(CD) $(TARGET_ARM_V7A_HARD)/src/$(OPENSSL_DIR) && $(TOOLCHAIN_ARM_V7A_HARD_ENV) $(MAKE) build_libs
	$(CD) $(TARGET_ARM_V7A_HARD)/src/$(OPENSSL_DIR) && $(ECHO) '#!/bin/sh\n\nfalse\n' > apps/openssl
	$(CD) $(TARGET_ARM_V7A_HARD)/src/$(OPENSSL_DIR) && $(TOOLCHAIN_ARM_V7A_HARD_ENV) $(MAKE) install_sw

$(TARGET_ARM64_V8A)/lib/libssl.a: $(TARGET_ARM64_V8A)/src/$(OPENSSL_DIR)
	$(CD) $(TARGET_ARM64_V8A)/src/$(OPENSSL_DIR) && $(TOOLCHAIN_ARM64_V8A_ENV) $(MAKE) depend
	$(CD) $(TARGET_ARM64_V8A)/src/$(OPENSSL_DIR) && $(TOOLCHAIN_ARM64_V8A_ENV) $(MAKE) build_libs
	$(CD) $(TARGET_ARM64_V8A)/src/$(OPENSSL_DIR) && $(ECHO) '#!/bin/sh\n\nfalse\n' > apps/openssl
	$(CD) $(TARGET_ARM64_V8A)/src/$(OPENSSL_DIR) && $(TOOLCHAIN_ARM64_V8A_ENV) $(MAKE) install_sw

$(TARGET_MIPS)/lib/libssl.a: $(TARGET_MIPS)/src/$(OPENSSL_DIR)
	$(CD) $(TARGET_MIPS)/src/$(OPENSSL_DIR) && $(TOOLCHAIN_MIPS_ENV) $(MAKE) depend
	$(CD) $(TARGET_MIPS)/src/$(OPENSSL_DIR) && $(TOOLCHAIN_MIPS_ENV) $(MAKE) build_libs
	$(CD) $(TARGET_MIPS)/src/$(OPENSSL_DIR) && $(ECHO) '#!/bin/sh\n\nfalse\n' > apps/openssl
	$(CD) $(TARGET_MIPS)/src/$(OPENSSL_DIR) && $(TOOLCHAIN_MIPS_ENV) $(MAKE) install_sw

$(TARGET_MIPS64)/lib/libssl.a: $(TARGET_MIPS64)/src/$(OPENSSL_DIR)
	$(CD) $(TARGET_MIPS64)/src/$(OPENSSL_DIR) && $(TOOLCHAIN_MIPS64_ENV) $(MAKE) depend
	$(CD) $(TARGET_MIPS64)/src/$(OPENSSL_DIR) && $(TOOLCHAIN_MIPS64_ENV) $(MAKE) build_libs
	$(CD) $(TARGET_MIPS64)/src/$(OPENSSL_DIR) && $(ECHO) '#!/bin/sh\n\nfalse\n' > apps/openssl
	$(CD) $(TARGET_MIPS64)/src/$(OPENSSL_DIR) && $(TOOLCHAIN_MIPS64_ENV) $(MAKE) install_sw

#
# Rules to build libwebsockets
#

libwebsockets: \
 libwebsockets-x86 \
 libwebsockets-x86_64 \
 libwebsockets-armeabi \
 libwebsockets-armeabi-v7a \
 libwebsockets-armeabi-v7a-hard \
 libwebsockets-arm64-v8a \
 libwebsockets-mips \
 libwebsockets-mips64 \

libwebsockets-x86: $(TARGET_X86)/lib/libwebsockets.a
libwebsockets-x86_64: $(TARGET_X86_64)/lib/libwebsockets.a
libwebsockets-armeabi: $(TARGET_ARM)/lib/libwebsockets.a
libwebsockets-armeabi-v7a: $(TARGET_ARM_V7A)/lib/libwebsockets.a
libwebsockets-armeabi-v7a-hard: $(TARGET_ARM_V7A_HARD)/lib/libwebsockets.a
libwebsockets-arm64-v8a: $(TARGET_ARM64_V8A)/lib/libwebsockets.a
libwebsockets-mips: $(TARGET_MIPS)/lib/libwebsockets.a
libwebsockets-mips64: $(TARGET_MIPS64)/lib/libwebsockets.a

# Extracting/configuring sources

$(TARGET_X86)/src/$(LIBWEBSOCKETS_DIR): $(LIBWEBSOCKETS_TGZ_SOURCE) $(TOOLCHAIN_X86) $(TARGET_X86)/lib/libssl.a $(TARGET_X86)/lib/libz.a
	-$(MKDIR) -p $(TARGET_X86)/src
	$(TAR) xf $(LIBWEBSOCKETS_TGZ_SOURCE) -C $(TARGET_X86)/src
	-$(MKDIR) -p $(TARGET_X86)/src/$(LIBWEBSOCKETS_DIR)/build
	$(CD) $(TARGET_X86)/src/$(LIBWEBSOCKETS_DIR)/build && $(TOOLCHAIN_X86_ENV) \
	  $(CMAKE) $(TARGET_X86_LWS_OPTIONS) \
	    -DCMAKE_INSTALL_PREFIX=$(shell pwd)/$(TARGET_X86) \
	    -DLWS_OPENSSL_LIBRARIES="$(shell pwd)/$(TARGET_X86)/lib/libssl.a;$(shell pwd)/$(TARGET_X86)/lib/libcrypto.a" \
	    -DLWS_OPENSSL_INCLUDE_DIRS="$(shell pwd)/$(TARGET_X86)/include" \
	    ..

$(TARGET_X86_64)/src/$(LIBWEBSOCKETS_DIR): $(LIBWEBSOCKETS_TGZ_SOURCE) $(TOOLCHAIN_X86_64) $(TARGET_X86_64)/lib/libssl.a $(TARGET_X86_64)/lib/libz.a
	-$(MKDIR) -p $(TARGET_X86_64)/src
	$(TAR) xf $(LIBWEBSOCKETS_TGZ_SOURCE) -C $(TARGET_X86_64)/src
	-$(MKDIR) -p $(TARGET_X86_64)/src/$(LIBWEBSOCKETS_DIR)/build
	$(CD) $(TARGET_X86_64)/src/$(LIBWEBSOCKETS_DIR)/build && $(TOOLCHAIN_X86_64_ENV) \
	  $(CMAKE) $(TARGET_X86_64_LWS_OPTIONS) \
	    -DCMAKE_INSTALL_PREFIX=$(shell pwd)/$(TARGET_X86_64) \
	    -DLWS_OPENSSL_LIBRARIES="$(shell pwd)/$(TARGET_X86_64)/lib/libssl.a;$(shell pwd)/$(TARGET_X86_64)/lib/libcrypto.a" \
	    -DLWS_OPENSSL_INCLUDE_DIRS="$(shell pwd)/$(TARGET_X86_64)/include" \
	    ..

$(TARGET_ARM)/src/$(LIBWEBSOCKETS_DIR): $(LIBWEBSOCKETS_TGZ_SOURCE) $(TOOLCHAIN_ARM) $(TARGET_ARM)/lib/libssl.a $(TARGET_ARM)/lib/libz.a
	-$(MKDIR) -p $(TARGET_ARM)/src
	$(TAR) xf $(LIBWEBSOCKETS_TGZ_SOURCE) -C $(TARGET_ARM)/src
	-$(MKDIR) -p $(TARGET_ARM)/src/$(LIBWEBSOCKETS_DIR)/build
	$(CD) $(TARGET_ARM)/src/$(LIBWEBSOCKETS_DIR)/build && $(TOOLCHAIN_ARM_ENV) \
	  $(CMAKE) $(TARGET_ARM_LWS_OPTIONS) \
	    -DCMAKE_INSTALL_PREFIX=$(shell pwd)/$(TARGET_ARM) \
	    -DLWS_OPENSSL_LIBRARIES="$(shell pwd)/$(TARGET_ARM)/lib/libssl.a;$(shell pwd)/$(TARGET_ARM)/lib/libcrypto.a" \
	    -DLWS_OPENSSL_INCLUDE_DIRS="$(shell pwd)/$(TARGET_ARM)/include" \
	    ..

$(TARGET_ARM_V7A)/src/$(LIBWEBSOCKETS_DIR): $(LIBWEBSOCKETS_TGZ_SOURCE) $(TOOLCHAIN_ARM_V7A) $(TARGET_ARM_V7A)/lib/libssl.a $(TARGET_ARM_V7A)/lib/libz.a
	-$(MKDIR) -p $(TARGET_ARM_V7A)/src
	$(TAR) xf $(LIBWEBSOCKETS_TGZ_SOURCE) -C $(TARGET_ARM_V7A)/src
	-$(MKDIR) -p $(TARGET_ARM_V7A)/src/$(LIBWEBSOCKETS_DIR)/build
	$(CD) $(TARGET_ARM_V7A)/src/$(LIBWEBSOCKETS_DIR)/build && $(TOOLCHAIN_ARM_V7A_ENV) \
	  $(CMAKE) $(TARGET_ARM_V7A_LWS_OPTIONS) \
	    -DCMAKE_INSTALL_PREFIX=$(shell pwd)/$(TARGET_ARM_V7A) \
	    -DLWS_OPENSSL_LIBRARIES="$(shell pwd)/$(TARGET_ARM_V7A)/lib/libssl.a;$(shell pwd)/$(TARGET_ARM_V7A)/lib/libcrypto.a" \
	    -DLWS_OPENSSL_INCLUDE_DIRS="$(shell pwd)/$(TARGET_ARM_V7A)/include" \
	    ..

$(TARGET_ARM_V7A_HARD)/src/$(LIBWEBSOCKETS_DIR): $(LIBWEBSOCKETS_TGZ_SOURCE) $(TOOLCHAIN_ARM_V7A_HARD) $(TARGET_ARM_V7A_HARD)/lib/libssl.a $(TARGET_ARM_V7A_HARD)/lib/libz.a
	-$(MKDIR) -p $(TARGET_ARM_V7A_HARD)/src
	$(TAR) xf $(LIBWEBSOCKETS_TGZ_SOURCE) -C $(TARGET_ARM_V7A_HARD)/src
	-$(MKDIR) -p $(TARGET_ARM_V7A_HARD)/src/$(LIBWEBSOCKETS_DIR)/build
	$(CD) $(TARGET_ARM_V7A_HARD)/src/$(LIBWEBSOCKETS_DIR)/build && $(TOOLCHAIN_ARM_V7A_HARD_ENV) \
	  $(CMAKE) $(TARGET_ARM_V7A_HARD_LWS_OPTIONS) \
	    -DCMAKE_INSTALL_PREFIX=$(shell pwd)/$(TARGET_ARM_V7A_HARD) \
	    -DLWS_OPENSSL_LIBRARIES="$(shell pwd)/$(TARGET_ARM_V7A_HARD)/lib/libssl.a;$(shell pwd)/$(TARGET_ARM_V7A_HARD)/lib/libcrypto.a" \
	    -DLWS_OPENSSL_INCLUDE_DIRS="$(shell pwd)/$(TARGET_ARM_V7A_HARD)/include" \
	    ..

$(TARGET_ARM64_V8A)/src/$(LIBWEBSOCKETS_DIR): $(LIBWEBSOCKETS_TGZ_SOURCE) $(TOOLCHAIN_ARM64_V8A) $(TARGET_ARM64_V8A)/lib/libssl.a $(TARGET_ARM64_V8A)/lib/libz.a
	-$(MKDIR) -p $(TARGET_ARM64_V8A)/src
	$(TAR) xf $(LIBWEBSOCKETS_TGZ_SOURCE) -C $(TARGET_ARM64_V8A)/src
	-$(MKDIR) -p $(TARGET_ARM64_V8A)/src/$(LIBWEBSOCKETS_DIR)/build
	$(CD) $(TARGET_ARM64_V8A)/src/$(LIBWEBSOCKETS_DIR)/build && $(TOOLCHAIN_ARM64_V8A_ENV) \
	  $(CMAKE) $(TARGET_ARM64_V8A_LWS_OPTIONS) \
	    -DCMAKE_INSTALL_PREFIX=$(shell pwd)/$(TARGET_ARM64_V8A) \
	    -DLWS_OPENSSL_LIBRARIES="$(shell pwd)/$(TARGET_ARM64_V8A)/lib/libssl.a;$(shell pwd)/$(TARGET_ARM64_V8A)/lib/libcrypto.a" \
	    -DLWS_OPENSSL_INCLUDE_DIRS="$(shell pwd)/$(TARGET_ARM64_V8A)/include" \
	    ..

$(TARGET_MIPS)/src/$(LIBWEBSOCKETS_DIR): $(LIBWEBSOCKETS_TGZ_SOURCE) $(TOOLCHAIN_MIPS) $(TARGET_MIPS)/lib/libssl.a $(TARGET_MIPS)/lib/libz.a
	-$(MKDIR) -p $(TARGET_MIPS)/src
	$(TAR) xf $(LIBWEBSOCKETS_TGZ_SOURCE) -C $(TARGET_MIPS)/src
	-$(MKDIR) -p $(TARGET_MIPS)/src/$(LIBWEBSOCKETS_DIR)/build
	$(CD) $(TARGET_MIPS)/src/$(LIBWEBSOCKETS_DIR)/build && $(TOOLCHAIN_MIPS_ENV) \
	  $(CMAKE) $(TARGET_MIPS_LWS_OPTIONS) \
	    -DCMAKE_INSTALL_PREFIX=$(shell pwd)/$(TARGET_MIPS) \
	    -DLWS_OPENSSL_LIBRARIES="$(shell pwd)/$(TARGET_MIPS)/lib/libssl.a;$(shell pwd)/$(TARGET_MIPS)/lib/libcrypto.a" \
	    -DLWS_OPENSSL_INCLUDE_DIRS="$(shell pwd)/$(TARGET_MIPS)/include" \
	    ..

$(TARGET_MIPS64)/src/$(LIBWEBSOCKETS_DIR): $(LIBWEBSOCKETS_TGZ_SOURCE) $(TOOLCHAIN_MIPS64) $(TARGET_MIPS64)/lib/libssl.a $(TARGET_MIPS64)/lib/libz.a
	-$(MKDIR) -p $(TARGET_MIPS64)/src
	$(TAR) xf $(LIBWEBSOCKETS_TGZ_SOURCE) -C $(TARGET_MIPS64)/src
	-$(MKDIR) -p $(TARGET_MIPS64)/src/$(LIBWEBSOCKETS_DIR)/build
	$(CD) $(TARGET_MIPS64)/src/$(LIBWEBSOCKETS_DIR)/build && $(TOOLCHAIN_MIPS64_ENV) \
	  $(CMAKE) $(TARGET_MIPS64_LWS_OPTIONS) \
	    -DCMAKE_INSTALL_PREFIX=$(shell pwd)/$(TARGET_MIPS64) \
	    -DLWS_OPENSSL_LIBRARIES="$(shell pwd)/$(TARGET_MIPS64)/lib/libssl.a;$(shell pwd)/$(TARGET_MIPS64)/lib/libcrypto.a" \
	    -DLWS_OPENSSL_INCLUDE_DIRS="$(shell pwd)/$(TARGET_MIPS64)/include" \
	    ..

# Build/install library

$(TARGET_X86)/lib/libwebsockets.a: $(TARGET_X86)/src/$(LIBWEBSOCKETS_DIR)
	$(CD) $(TARGET_X86)/src/$(LIBWEBSOCKETS_DIR)/build && $(TOOLCHAIN_X86_ENV) $(MAKE)
	$(CD) $(TARGET_X86)/src/$(LIBWEBSOCKETS_DIR)/build && $(TOOLCHAIN_X86_ENV) $(MAKE) install

$(TARGET_X86_64)/lib/libwebsockets.a: $(TARGET_X86_64)/src/$(LIBWEBSOCKETS_DIR)
	$(CD) $(TARGET_X86_64)/src/$(LIBWEBSOCKETS_DIR)/build && $(TOOLCHAIN_X86_64_ENV) $(MAKE)
	$(CD) $(TARGET_X86_64)/src/$(LIBWEBSOCKETS_DIR)/build && $(TOOLCHAIN_X86_64_ENV) $(MAKE) install

$(TARGET_ARM)/lib/libwebsockets.a: $(TARGET_ARM)/src/$(LIBWEBSOCKETS_DIR)
	$(CD) $(TARGET_ARM)/src/$(LIBWEBSOCKETS_DIR)/build && $(TOOLCHAIN_ARM_ENV) $(MAKE)
	$(CD) $(TARGET_ARM)/src/$(LIBWEBSOCKETS_DIR)/build && $(TOOLCHAIN_ARM_ENV) $(MAKE) install

$(TARGET_ARM_V7A)/lib/libwebsockets.a: $(TARGET_ARM_V7A)/src/$(LIBWEBSOCKETS_DIR)
	$(CD) $(TARGET_ARM_V7A)/src/$(LIBWEBSOCKETS_DIR)/build && $(TOOLCHAIN_ARM_V7A_ENV) $(MAKE)
	$(CD) $(TARGET_ARM_V7A)/src/$(LIBWEBSOCKETS_DIR)/build && $(TOOLCHAIN_ARM_V7A_ENV) $(MAKE) install

$(TARGET_ARM_V7A_HARD)/lib/libwebsockets.a: $(TARGET_ARM_V7A_HARD)/src/$(LIBWEBSOCKETS_DIR)
	$(CD) $(TARGET_ARM_V7A_HARD)/src/$(LIBWEBSOCKETS_DIR)/build && $(TOOLCHAIN_ARM_V7A_HARD_ENV) $(MAKE)
	$(CD) $(TARGET_ARM_V7A_HARD)/src/$(LIBWEBSOCKETS_DIR)/build && $(TOOLCHAIN_ARM_V7A_HARD_ENV) $(MAKE) install

$(TARGET_ARM64_V8A)/lib/libwebsockets.a: $(TARGET_ARM64_V8A)/src/$(LIBWEBSOCKETS_DIR)
	$(CD) $(TARGET_ARM64_V8A)/src/$(LIBWEBSOCKETS_DIR)/build && $(TOOLCHAIN_ARM64_V8A_ENV) $(MAKE)
	$(CD) $(TARGET_ARM64_V8A)/src/$(LIBWEBSOCKETS_DIR)/build && $(TOOLCHAIN_ARM64_V8A_ENV) $(MAKE) install

$(TARGET_MIPS)/lib/libwebsockets.a: $(TARGET_MIPS)/src/$(LIBWEBSOCKETS_DIR)
	$(CD) $(TARGET_MIPS)/src/$(LIBWEBSOCKETS_DIR)/build && $(TOOLCHAIN_MIPS_ENV) $(MAKE)
	$(CD) $(TARGET_MIPS)/src/$(LIBWEBSOCKETS_DIR)/build && $(TOOLCHAIN_MIPS_ENV) $(MAKE) install

$(TARGET_MIPS64)/lib/libwebsockets.a: $(TARGET_MIPS64)/src/$(LIBWEBSOCKETS_DIR)
	$(CD) $(TARGET_MIPS64)/src/$(LIBWEBSOCKETS_DIR)/build && $(TOOLCHAIN_MIPS64_ENV) $(MAKE)
	$(CD) $(TARGET_MIPS64)/src/$(LIBWEBSOCKETS_DIR)/build && $(TOOLCHAIN_MIPS64_ENV) $(MAKE) install

#
# Some rules for housekeeping
#

clean-ndk:
	$(NDK_ROOT)/ndk-build clean

clean: clean-targets clean-toolchains

dist-clean: clean clean-sources

clean-targets: \
 clean-target-x86 \
 clean-target-x86_64 \
 clean-target-armeabi \
 clean-target-armeabi-v7a \
 clean-target-armeabi-v7a-hard \
 clean-target-arm64-v8a \
 clean-target-mips \
 clean-target-mips64

clean-target-x86:
	-$(RM) -fr $(TARGET_X86)

clean-target-x86_64:
	-$(RM) -fr $(TARGET_X86_64)

clean-target-armeabi:
	-$(RM) -fr $(TARGET_ARM)

clean-target-armeabi-v7a:
	-$(RM) -fr $(TARGET_ARM_V7A)

clean-target-armeabi-v7a-hard:
	-$(RM) -fr $(TARGET_ARM_V7A_HARD)

clean-target-arm64-v8a:
	-$(RM) -fr $(TARGET_ARM64_V8A)

clean-target-mips:
	-$(RM) -fr $(TARGET_MIPS)

clean-target-mips64:
	-$(RM) -fr $(TARGET_MIPS64)

clean-sources: \
 clean-source-zlib \
 clean-source-openssl \
 clean-source-libwebsockets

clean-source-zlib:
	-$(RM) $(ZLIB_TGZ_SOURCE)

clean-source-openssl:
	-$(RM) $(OPENSSL_TGZ_SOURCE)

clean-source-libwebsockets:
	-$(RM) $(LIBWEBSOCKETS_TGZ_SOURCE)

clean-toolchains: \
 clean-toolchain-x86 \
 clean-toolchain-x86_64 \
 clean-toolchain-armeabi \
 clean-toolchain-armeabi-v7a \
 clean-toolchain-armeabi-v7a-hard \
 clean-toolchain-arm64-v8a \
 clean-toolchain-mips \
 clean-toolchain-mips64
	-$(RM) -fr toolchains

clean-toolchain-x86:
	-$(RM) -fr $(TOOLCHAIN_X86)

clean-toolchain-x86_64:
	-$(RM) -fr $(TOOLCHAIN_X86_64)

clean-toolchain-armeabi:
	-$(RM) -fr $(TOOLCHAIN_ARM)

clean-toolchain-armeabi-v7a:
	-$(RM) -fr $(TOOLCHAIN_ARM_V7A)

clean-toolchain-armeabi-v7a-hard:
	-$(RM) -fr $(TOOLCHAIN_ARM_V7A_HARD)

clean-toolchain-arm64-v8a:
	-$(RM) -fr $(TOOLCHAIN_ARM64_V8A)

clean-toolchain-mips:
	-$(RM) -fr $(TOOLCHAIN_MIPS)

clean-toolchain-mips64:
	-$(RM) -fr $(TOOLCHAIN_MIPS64)

# 'make list-targets' prints a list of all targets.
# Thanks to: http://stackoverflow.com/questions/4219255/how-do-you-get-the-list-of-targets-in-a-makefile
# Modified to allow us to include files in this Makefile.
list-targets: MAKE_NO_INCLUDES := 1
export MAKE_NO_INCLUDES
list-targets:
	@$(MAKE) -s list-targets-no-includes
list-targets-no-includes:
	@$(MAKE) -pRrq -f $(lastword $(MAKEFILE_LIST)) : 2>/dev/null | $(AWK) -v RS= -F: '/^# File/,/^# Finished Make data base/ {if ($$1 !~ "^[#.]") {print $$1}}' | $(SORT) | $(EGREP) -v -e '^[^[:alnum:]]' -e '^$@$$'

