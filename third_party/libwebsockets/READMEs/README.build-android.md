# Building for Android NDK

If you have the ndk and prebuilt toolchains with that, you can simply build
lws library for your android app from one cmake and one make command.

However if you want a tls lib, you have to take care of building and pointing
to that first.  But if it's a cmake project like mbedtls, that also is just a
matter of one cmake and one make.

## Installing NDK pieces

There's probably a more direct way but the official way is install the whole
Android Studio and then run `sdkmanager` to install a recent NDK.

I installed the sdk and ndk pieces into /opt/android/ and that's how the
`./contrib/cross-aarch64-android.cmake` toolchain file is shipped.  You can
adapt some settings at the top of that file including the path if needed.

## Fetching lws (needed first for cross toolchain file)

It doesn't care where you put these projects, but for simplicity they should
be in the same parent dir, like

```
 - /home/someone
  - /home/someone/libwebsockets
  - /home/someone/mbedtls
```

The reason is that building mbedtls need the cross toolchain file from
libwebsockets, that's also why we have to get libwebsockets first now but
build it later.

```
$ git clone https://libwebsockets.org/repo/libwebsockets
```

## Building mbedtls

```
$ git clone https://github.com/ARMmbed/mbedtls.git
$ cd mbedtls
$ mkdir build
$ cd build
$ rm -f CMakeCache.txt && \
  cmake .. -DCMAKE_TOOLCHAIN_FILE=../libwebsockets/contrib/cross-aarch64-android.cmake \
  -DUSE_SHARED_MBEDTLS_LIBRARY=1 \
  -DENABLE_PROGRAMS=0 \
  -Wno-dev && \
  make -j && \
  cmake --install .
```

The lws toolchain file sets the path to install into as the cross root path, so
despite it looks like the destination dir is missing for the install, it will
go into, eg `/opt/android/ndk/21.1.6352462/platforms/android-24/arch-arm64/lib/libmbedcrypto.a`
where lws will look for it

## Building lws

You don't need to explain where mbedtls can be found... lws will build with the
same toolchain file that sets the cross root to the same place as mbedtls, it
will easily find them there without any further hints.

```
$ mkdir build
$ cd build
$ rm -f CMakeCache.txt && \
  cmake .. -DCMAKE_TOOLCHAIN_FILE=../libwebsockets/contrib/cross-aarch64-android.cmake \
  -DLWS_WITH_MBEDTLS=1 \
  -DLWS_WITHOUT_TESTAPPS=1 && \
  make && \
  cmake --install .
```

That's it, both mbedtls and lws library and header files are installed into the
ndk cross root.
