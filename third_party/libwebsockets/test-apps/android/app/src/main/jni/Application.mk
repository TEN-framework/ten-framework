#
# Zlib, OpenSSL and libwebsockets will be downloaded automatically unless you place
# their source .tar.gz files in the jni directory...
#

# The location of the NDK
#
NDK_ROOT := /opt/Android/Sdk/ndk-bundle

# Update these to the latest versions before building
#
ZLIB_VERSION := 1.2.8
OPENSSL_VERSION := 1.0.2h

# This will be executed as 'git clone $(LIBWEBSOCKETS_GIT_URL)'
#
LIBWEBSOCKETS_GIT_URL := --branch master https://github.com/warmcat/libwebsockets.git

#
# Note: If you build for API level 21 or higher in APP_PLATFORM,
#       the resulting application will only run on API 21+ devices.
#       Even if minSdkVersion has been set to a lower level!
#       This is the result of API changes for the native signal() function.
#       The recommended solution is to build two packages, one for API 17+ and the other for API 21+ devices.
#       http://stackoverflow.com/questions/28740315/android-ndk-getting-java-lang-unsatisfiedlinkerror-dlopen-failed-cannot-loca
#
# Note: If you change the API level the JNI code must be rebuild completely.
#       (Run 'make clean' from the app/src/main/jni directory.)
#
APP_PLATFORM := android-23

# Builds for armeabi armeabi-v7a x86 mips arm64-v8a x86_64 mips64
#
#APP_ABI := all

# The same as above.
#
#APP_ABI := armeabi armeabi-v7a x86 mips arm64-v8a x86_64 mips64

# Good enough for most current devices + x86 AVD
#
APP_ABI := armeabi-v7a x86

# Enable (GNU) c++11 extentions
APP_CPPFLAGS += -std=gnu++11

# Use the GNU standard template library
APP_STL := gnustl_shared

