# get current directory
LOCAL_PATH := $(call my-dir)

# libz.a
#
include $(CLEAR_VARS)
LOCAL_MODULE := libz
LOCAL_SRC_FILES := $(TARGET_ARCH_ABI)/lib/libz.a
include $(PREBUILT_STATIC_LIBRARY)

# libssl.a
#
include $(CLEAR_VARS)
LOCAL_MODULE := libssl
LOCAL_SRC_FILES := $(TARGET_ARCH_ABI)/lib/libssl.a
include $(PREBUILT_STATIC_LIBRARY)

# libcrypto.a
#
include $(CLEAR_VARS)
LOCAL_MODULE := libcrypto
LOCAL_SRC_FILES := $(TARGET_ARCH_ABI)/lib/libcrypto.a
include $(PREBUILT_STATIC_LIBRARY)

# libwebsockets.a
#
include $(CLEAR_VARS)
LOCAL_MODULE := libwebsockets
LOCAL_SRC_FILES := $(TARGET_ARCH_ABI)/lib/libwebsockets.a
include $(PREBUILT_STATIC_LIBRARY)

# liblwsservice.so
#
include $(CLEAR_VARS)
LOCAL_DISABLE_FATAL_LINKER_WARNINGS := true
LOCAL_MODULE := lwsservice
LOCAL_SRC_FILES := LwsService.cpp
LOCAL_C_INCLUDES := $(LOCAL_PATH) $(TARGET_ARCH_ABI)/include
LOCAL_STATIC_LIBRARIES := websockets z ssl crypto
LOCAL_LDLIBS := -llog
include $(BUILD_SHARED_LIBRARY)
