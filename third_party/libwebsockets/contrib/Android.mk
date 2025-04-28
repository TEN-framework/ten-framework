# example Android Native Library makefile
# contributed by Gregory Junker <ggjunker@gmail.com>

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := libwebsockets
LOCAL_CFLAGS    := -DLWS_BUILTIN_GETIFADDRS
LWS_LIB_PATH	:= ../../../shared/libwebsockets/lib
LOCAL_C_INCLUDES:= $(LOCAL_PATH)/$(LWS_LIB_PATH)
LOCAL_SRC_FILES := \
	$(LWS_LIB_PATH)/base64-decode.c \
	$(LWS_LIB_PATH)/client.c \
	$(LWS_LIB_PATH)/client-handshake.c \
	$(LWS_LIB_PATH)/client-parser.c \
	$(LWS_LIB_PATH)/daemonize.c \
	$(LWS_LIB_PATH)/extension.c \
	$(LWS_LIB_PATH)/extension-deflate-frame.c \
	$(LWS_LIB_PATH)/extension-deflate-stream.c \
	$(LWS_LIB_PATH)/getifaddrs.c \
	$(LWS_LIB_PATH)/handshake.c \
	$(LWS_LIB_PATH)/libwebsockets.c \
	$(LWS_LIB_PATH)/md5.c \
	$(LWS_LIB_PATH)/output.c \
	$(LWS_LIB_PATH)/parsers.c \
	$(LWS_LIB_PATH)/sha-1.c

include $(BUILD_STATIC_LIBRARY)
