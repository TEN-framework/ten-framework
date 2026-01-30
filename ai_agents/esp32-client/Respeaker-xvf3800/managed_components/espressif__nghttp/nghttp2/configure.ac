dnl nghttp2 - HTTP/2 C Library

dnl Copyright (c) 2012, 2013, 2014, 2015 Tatsuhiro Tsujikawa

dnl Permission is hereby granted, free of charge, to any person obtaining
dnl a copy of this software and associated documentation files (the
dnl "Software"), to deal in the Software without restriction, including
dnl without limitation the rights to use, copy, modify, merge, publish,
dnl distribute, sublicense, and/or sell copies of the Software, and to
dnl permit persons to whom the Software is furnished to do so, subject to
dnl the following conditions:

dnl The above copyright notice and this permission notice shall be
dnl included in all copies or substantial portions of the Software.

dnl THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
dnl EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
dnl MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
dnl NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
dnl LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
dnl OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
dnl WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

dnl Do not change user variables!
dnl https://www.gnu.org/software/automake/manual/html_node/Flag-Variables-Ordering.html

AC_PREREQ(2.61)
AC_INIT([nghttp2], [1.65.0], [t-tujikawa@users.sourceforge.net])
AC_CONFIG_AUX_DIR([.])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])
AC_USE_SYSTEM_EXTENSIONS

LT_PREREQ([2.2.6])
LT_INIT()

AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_CANONICAL_TARGET

AM_INIT_AUTOMAKE([subdir-objects tar-pax])

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

dnl See versioning rule:
dnl  https://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html
AC_SUBST(LT_CURRENT, 42)
AC_SUBST(LT_REVISION, 4)
AC_SUBST(LT_AGE, 28)

major=`echo $PACKAGE_VERSION |cut -d. -f1 | sed -e "s/[^0-9]//g"`
minor=`echo $PACKAGE_VERSION |cut -d. -f2 | sed -e "s/[^0-9]//g"`
patch=`echo $PACKAGE_VERSION |cut -d. -f3 | cut -d- -f1 | sed -e "s/[^0-9]//g"`

PACKAGE_VERSION_NUM=`printf "0x%02x%02x%02x" "$major" "$minor" "$patch"`

AC_SUBST(PACKAGE_VERSION_NUM)

dnl Checks for command-line options
AC_ARG_ENABLE([werror],
    [AS_HELP_STRING([--enable-werror],
                    [Turn on compile time warnings])],
    [werror=$enableval], [werror=no])

AC_ARG_ENABLE([debug],
    [AS_HELP_STRING([--enable-debug],
                    [Turn on debug output])],
    [debug=$enableval], [debug=no])

AC_ARG_ENABLE([threads],
    [AS_HELP_STRING([--disable-threads],
                    [Turn off threading in apps])],
    [threads=$enableval], [threads=yes])

AC_ARG_ENABLE([app],
    [AS_HELP_STRING([--enable-app],
                    [Build applications (nghttp, nghttpd, nghttpx and h2load) [default=check]])],
    [request_app=$enableval], [request_app=check])

AC_ARG_ENABLE([hpack-tools],
    [AS_HELP_STRING([--enable-hpack-tools],
                    [Build HPACK tools [default=check]])],
    [request_hpack_tools=$enableval], [request_hpack_tools=check])

AC_ARG_ENABLE([examples],
    [AS_HELP_STRING([--enable-examples],
                    [Build examples [default=check]])],
    [request_examples=$enableval], [request_examples=check])

AC_ARG_ENABLE([failmalloc],
    [AS_HELP_STRING([--disable-failmalloc],
                    [Do not build failmalloc test program])],
    [request_failmalloc=$enableval], [request_failmalloc=yes])

AC_ARG_ENABLE([lib-only],
    [AS_HELP_STRING([--enable-lib-only],
                    [Build libnghttp2 only.  This is a short hand for --disable-app --disable-examples --disable-hpack-tools])],
    [request_lib_only=$enableval], [request_lib_only=no])

AC_ARG_ENABLE([http3],
    [AS_HELP_STRING([--enable-http3],
                    [(EXPERIMENTAL) Enable HTTP/3.  This requires ngtcp2, nghttp3, and a custom OpenSSL.])],
    [request_http3=$enableval], [request_http3=no])

AC_ARG_WITH([libxml2],
    [AS_HELP_STRING([--with-libxml2],
                    [Use libxml2 [default=check]])],
    [request_libxml2=$withval], [request_libxml2=check])

AC_ARG_WITH([jansson],
    [AS_HELP_STRING([--with-jansson],
                    [Use jansson [default=check]])],
    [request_jansson=$withval], [request_jansson=check])

AC_ARG_WITH([zlib],
    [AS_HELP_STRING([--with-zlib],
                    [Use zlib [default=check]])],
    [request_zlib=$withval], [request_zlib=check])

AC_ARG_WITH([libevent-openssl],
    [AS_HELP_STRING([--with-libevent-openssl],
                    [Use libevent_openssl [default=check]])],
    [request_libevent_openssl=$withval], [request_libevent_openssl=check])

AC_ARG_WITH([libcares],
    [AS_HELP_STRING([--with-libcares],
                    [Use libc-ares [default=check]])],
    [request_libcares=$withval], [request_libcares=check])

AC_ARG_WITH([wolfssl],
    [AS_HELP_STRING([--with-wolfssl],
                    [Use wolfSSL [default=check]])],
    [request_wolfssl=$withval], [request_wolfssl=check])

AC_ARG_WITH([openssl],
    [AS_HELP_STRING([--with-openssl],
                    [Use openssl [default=check]])],
    [request_openssl=$withval], [request_openssl=check])

AC_ARG_WITH([libev],
    [AS_HELP_STRING([--with-libev],
                    [Use libev [default=check]])],
    [request_libev=$withval], [request_libev=check])

AC_ARG_WITH([jemalloc],
    [AS_HELP_STRING([--with-jemalloc],
                    [Use jemalloc [default=check]])],
    [request_jemalloc=$withval], [request_jemalloc=check])

AC_ARG_WITH([systemd],
    [AS_HELP_STRING([--with-systemd],
                    [Enable systemd support in nghttpx [default=check]])],
    [request_systemd=$withval], [request_systemd=check])

AC_ARG_WITH([mruby],
    [AS_HELP_STRING([--with-mruby],
                    [Use mruby [default=no]])],
    [request_mruby=$withval], [request_mruby=no])

AC_ARG_WITH([neverbleed],
    [AS_HELP_STRING([--with-neverbleed],
                    [Use neverbleed [default=no]])],
    [request_neverbleed=$withval], [request_neverbleed=no])

AC_ARG_WITH([libngtcp2],
    [AS_HELP_STRING([--with-libngtcp2],
                    [Use libngtcp2 [default=check]])],
    [request_libngtcp2=$withval], [request_libngtcp2=check])

AC_ARG_WITH([libnghttp3],
    [AS_HELP_STRING([--with-libnghttp3],
                    [Use libnghttp3 [default=check]])],
    [request_libnghttp3=$withval], [request_libnghttp3=check])

AC_ARG_WITH([libbpf],
    [AS_HELP_STRING([--with-libbpf],
                    [Use libbpf [default=no]])],
    [request_libbpf=$withval], [request_libbpf=no])

AC_ARG_WITH([libbrotlienc],
    [AS_HELP_STRING([--with-libbrotlienc],
                    [Use libbrotlienc [default=no]])],
    [request_libbrotlienc=$withval], [request_libbrotlienc=no])

AC_ARG_WITH([libbrotlidec],
    [AS_HELP_STRING([--with-libbrotlidec],
                    [Use libbrotlidec [default=no]])],
    [request_libbrotlidec=$withval], [request_libbrotlidec=no])

dnl Define variables
AC_ARG_VAR([LIBEV_CFLAGS], [C compiler flags for libev, skipping any checks])
AC_ARG_VAR([LIBEV_LIBS], [linker flags for libev, skipping any checks])

AC_ARG_VAR([JEMALLOC_CFLAGS],
           [C compiler flags for jemalloc, skipping any checks])
AC_ARG_VAR([JEMALLOC_LIBS], [linker flags for jemalloc, skipping any checks])

AC_ARG_VAR([LIBTOOL_LDFLAGS],
           [libtool specific flags (e.g., -static-libtool-libs)])

AC_ARG_VAR([BPFCFLAGS], [C compiler flags for bpf program])

dnl Checks for programs
AC_PROG_CC
AC_PROG_CXX
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_MKDIR_P

PKG_PROG_PKG_CONFIG([0.20])

AM_PATH_PYTHON([3.8],, [:])

if [test "x$request_lib_only" = "xyes"]; then
  request_app=no
  request_hpack_tools=no
  request_examples=no
  request_http3=no
  request_libxml2=no
  request_jansson=no
  request_zlib=no
  request_libevent_openssl=no
  request_libcares=no
  request_openssl=no
  request_libev=no
  request_jemalloc=no
  request_systemd=no
  request_mruby=no
  request_neverbleed=no
  request_libngtcp2=no
  request_libnghttp3=no
  request_libbpf=no
fi

if test "x$GCC" = "xyes" -o "x$CC" = "xclang" ; then
  AC_DEFINE([NGHTTP2_NORETURN], [__attribute__((noreturn))], [Hint to the compiler that a function never return])
else
  AC_DEFINE([NGHTTP2_NORETURN], , [Hint to the compiler that a function never return])
fi

save_CXXFLAGS="$CXXFLAGS"
CXXFLAGS=

AX_CXX_COMPILE_STDCXX([20], [], [optional])

CXX1XCXXFLAGS="$CXXFLAGS"
CXXFLAGS="$save_CXXFLAGS"
AC_SUBST([CXX1XCXXFLAGS])

AC_LANG_PUSH(C++)

save_CXXFLAGS="$CXXFLAGS"
CXXFLAGS="$CXXFLAGS $CXX1XCXXFLAGS"

# Check that std::future is available.
AC_MSG_CHECKING([whether std::future is available])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
[[
#include <vector>
#include <future>
]],
[[
std::vector<std::future<int>> v;
(void)v;
]])],
    [AC_DEFINE([HAVE_STD_FUTURE], [1],
               [Define to 1 if you have the `std::future`.])
     have_std_future=yes
     AC_MSG_RESULT([yes])],
    [have_std_future=no
     AC_MSG_RESULT([no])])

# Check that std::map::emplace is available for g++-4.7.
AC_MSG_CHECKING([whether std::map::emplace is available])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
[[
#include <map>
]],
[[
std::map<int, int>().emplace(1, 2);
]])],
    [AC_DEFINE([HAVE_STD_MAP_EMPLACE], [1],
               [Define to 1 if you have the `std::map::emplace`.])
     have_std_map_emplace=yes
     AC_MSG_RESULT([yes])],
    [have_std_map_emplace=no
     AC_MSG_RESULT([no])])

# Check that std::atomic<std::shared_ptr<T>> is supported.
AC_MSG_CHECKING([whether std::atomic<std::shared_ptr<T>> is supported])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
[[
#include <memory>
]],
[[
auto a = std::atomic<std::shared_ptr<int>>(std::make_shared<int>(1000000007));
auto p = a.load();
++*p;
a.store(p);
]])],
    [AC_DEFINE([HAVE_ATOMIC_STD_SHARED_PTR], [1],
               [Define to 1 if you have the std::atomic<std::shared_ptr<T>> is supported.])
     have_atomic_std_shared_ptr=yes
     AC_MSG_RESULT([yes])],
    [have_atomic_std_shared_ptr=no
     AC_MSG_RESULT([no])])

# Check that thread_local storage specifier is available
AC_MSG_CHECKING([whether thread_local storage class specifier is available.])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
,
[[
thread_local int a = 0;
(void)a;
]])],
    [AC_DEFINE([HAVE_THREAD_LOCAL], [1],
               [Define to 1 if you have thread_local storage specifier.])
     have_thread_local=yes
     AC_MSG_RESULT([yes])],
    [have_Thread_local=no
     AC_MSG_RESULT([no])])

CXXFLAGS=$save_CXXFLAGS

AC_LANG_POP()

# Checks for libraries.

# Additional libraries required for tests.
TESTLDADD=

# Additional libraries required for programs under src directory.
APPLDFLAGS=

case "$host_os" in
  *android*)
    android_build=yes
    # android does not need -pthread, but needs following 2 libs for C++
    APPLDFLAGS="$APPLDFLAGS -latomic"
    ;;
  *)
    PTHREAD_LDFLAGS="-pthread"
    APPLDFLAGS="$APPLDFLAGS $PTHREAD_LDFLAGS"
    ;;
esac

case "$host_os" in
  *solaris*)
    APPLDFLAGS="$APPLDFLAGS -lsocket -lnsl"
    ;;
esac

case "${build}" in
  *-apple-darwin*)
    EXTRA_DEFS="-D__APPLE_USE_RFC_3542"
    AC_SUBST([EXTRA_DEFS])
    ;;
esac

# zlib
have_zlib=no
if test "x${request_zlib}" != "xno"; then
  PKG_CHECK_MODULES([ZLIB], [zlib >= 1.2.3], [have_zlib=yes], [have_zlib=no])

  if test "x${have_zlib}" = "xno"; then
    AC_MSG_NOTICE($ZLIB_PKG_ERRORS)
  fi
fi

if test "x${request_zlib}" = "xyes" &&
   test "x${have_zlib}" != "xyes"; then
  AC_MSG_ERROR([zlib was requested (--with-zlib) but not found])
fi

# dl: openssl requires libdl when it is statically linked.
case "${host_os}" in
  *bsd*)
    # dlopen is in libc on *BSD
    ;;
  *)
    save_LIBS=$LIBS
    AC_SEARCH_LIBS([dlopen], [dl], [APPLDFLAGS="-ldl $APPLDFLAGS"], [], [])
    LIBS=$save_LIBS
    ;;
esac

# libev (for src)
have_libev=no
if test "x${request_libev}" != "xno"; then
  if test "x${LIBEV_LIBS}" = "x" && test "x${LIBEV_CFLAGS}" = "x"; then
    # libev does not have pkg-config file.  Check it in an old way.
    save_LIBS=$LIBS
    # android requires -lm for floor
   AC_CHECK_LIB([ev], [ev_time], [have_libev=yes], [have_libev=no], [-lm])
   if test "x${have_libev}" = "xyes"; then
      AC_CHECK_HEADER([ev.h], [have_libev=yes], [have_libev=no])
      if test "x${have_libev}" = "xyes"; then
        LIBEV_LIBS=-lev
        LIBEV_CFLAGS=
      fi
    fi
    LIBS=$save_LIBS
  else
    have_libev=yes
  fi

  if test "x${have_libev}" = "xyes"; then
    AC_DEFINE([HAVE_LIBEV], [1], [Define to 1 if you have `libev` library.])
  fi
fi

if test "x${request_libev}" = "xyes" &&
   test "x${have_libev}" != "xyes"; then
  AC_MSG_ERROR([libev was requested (--with-libev) but not found])
fi

if test "x${request_openssl}" = "xyes" &&
   test "x${request_wolfssl}" = "xyes"; then
  AC_MSG_ERROR([Requesting both OpenSSL and wolfSSL is not allowed])
fi

# openssl (for src)
have_openssl=no
if test "x${request_openssl}" != "xno" &&
   test "x${request_wolfssl}" != "xyes"; then
  PKG_CHECK_MODULES([OPENSSL], [openssl >= 1.1.1],
                    [have_openssl=yes], [have_openssl=no])
  if test "x${have_openssl}" = "xno"; then
    AC_MSG_NOTICE($OPENSSL_PKG_ERRORS)
  else
    # Use C++ compiler because boringssl needs C++ runtime.
    AC_LANG_PUSH(C++)

    save_CXXFLAGS="$CXXFLAGS"
    save_LIBS="$LIBS"
    CXXFLAGS="$OPENSSL_CFLAGS $CXXFLAGS"
    LIBS="$OPENSSL_LIBS $LIBS"

    # quictls/openssl has SSL_provide_quic_data.  boringssl also has
    # it.  We will deal with it later.
    have_ssl_provide_quic_data=no
    AC_MSG_CHECKING([for SSL_provide_quic_data])
    AC_LINK_IFELSE([AC_LANG_PROGRAM([[
      #include <openssl/ssl.h>
    ]], [[
      SSL_provide_quic_data(NULL, (ssl_encryption_level_t)0, NULL, 0);
    ]])],
    [AC_MSG_RESULT([yes]); have_ssl_provide_quic_data=yes],
    [AC_MSG_RESULT([no]); have_ssl_provide_quic_data=no])

    # boringssl has SSL_set_quic_early_data_context.
    AC_MSG_CHECKING([for SSL_set_quic_early_data_context])
    AC_LINK_IFELSE([AC_LANG_PROGRAM([[
      #include <openssl/ssl.h>
    ]], [[
      SSL *ssl = NULL;
      SSL_set_quic_early_data_context(ssl, NULL, 0);
    ]])],
    [AC_MSG_RESULT([yes]); have_boringssl_quic=yes],
    [AC_MSG_RESULT([no]); have_boringssl_quic=no])

    CXXFLAGS="$save_CXXFLAGS"
    LIBS="$save_LIBS"

    AC_LANG_POP()
  fi
fi

if test "x${request_openssl}" = "xyes" &&
   test "x${have_openssl}" != "xyes"; then
  AC_MSG_ERROR([openssl was requested (--with-openssl) but not found])
fi

# wolfSSL (for src)
have_wolfssl=no
if test "x${request_wolfssl}" != "xno" &&
   test "x${request_openssl}" != "xyes" &&
   test "x${have_openssl}" != "xyes"; then
  PKG_CHECK_MODULES([WOLFSSL], [wolfssl >= 5.7.0],
                    [have_wolfssl=yes], [have_wolfssl=no])
  if test "x${have_wolfssl}" = "xno"; then
    AC_MSG_NOTICE($WOLFSSL_PKG_ERRORS)
  else
    AC_DEFINE([HAVE_WOLFSSL], [1],
              [Define to 1 if you have 'wolfssl' library.])

    save_CFLAGS="$CFLAGS"
    save_LIBS="$LIBS"
    CFLAGS="$WOLFSSL_CFLAGS $CFLAGS"
    LIBS="$WOLFSSL_LIBS $LIBS"

    have_wolfssl_quic=no
    AC_MSG_CHECKING([for wolfSSL QUIC])
    AC_LINK_IFELSE([AC_LANG_PROGRAM([[
      #include <wolfssl/options.h>
      #include <wolfssl/openssl/ssl.h>
    ]], [[
      SSL_provide_quic_data(NULL, 0, NULL, 0);
    ]])],
    [AC_MSG_RESULT([yes]); have_wolfssl_quic=yes],
    [AC_MSG_RESULT([no]); have_wolfssl_quic=no])

    CFLAGS="$save_CFLAGS"
    LIBS="$save_LIBS"
  fi
fi

if test "x${request_wolfssl}" = "xyes" &&
   test "x${have_wolfssl}" != "xyes"; then
  AC_MSG_ERROR([wolfSSL was requested (--with-wolfssl) but not found])
fi

# c-ares (for src)
have_libcares=no
if test "x${request_libcares}" != "xno"; then
  PKG_CHECK_MODULES([LIBCARES], [libcares >= 1.16.0], [have_libcares=yes],
                    [have_libcares=no])
  if test "x${have_libcares}" = "xno"; then
    AC_MSG_NOTICE($LIBCARES_PKG_ERRORS)
  fi
fi

if test "x${request_libcares}" = "xyes" &&
   test "x${have_libcares}" != "xyes"; then
  AC_MSG_ERROR([libcares was requested (--with-libcares) but not found])
fi

# ngtcp2 (for src)
have_libngtcp2=no
if test "x${request_libngtcp2}" != "xno"; then
  PKG_CHECK_MODULES([LIBNGTCP2], [libngtcp2 >= 1.4.0], [have_libngtcp2=yes],
                    [have_libngtcp2=no])
  if test "x${have_libngtcp2}" = "xno"; then
    AC_MSG_NOTICE($LIBNGTCP2_PKG_ERRORS)
  fi
fi

if test "x${request_libngtcp2}" = "xyes" &&
   test "x${have_libngtcp2}" != "xyes"; then
  AC_MSG_ERROR([libngtcp2 was requested (--with-libngtcp2) but not found])
fi

# ngtcp2_crypto_wolfssl (for src)
have_libngtcp2_crypto_wolfssl=no
if test "x${have_wolfssl_quic}" = "xyes" &&
   test "x${request_libngtcp2}" != "xno"; then
  PKG_CHECK_MODULES([LIBNGTCP2_CRYPTO_WOLFSSL],
                    [libngtcp2_crypto_wolfssl >= 1.0.0],
                    [have_libngtcp2_crypto_wolfssl=yes],
                    [have_libngtcp2_crypto_wolfssl=no])
  if test "x${have_libngtcp2_crypto_wolfssl}" = "xno"; then
    AC_MSG_NOTICE($LIBNGTCP2_CRYPTO_WOLFSSL_PKG_ERRORS)
  else
    AC_DEFINE([HAVE_LIBNGTCP2_CRYPTO_WOLFSSL], [1],
              [Define to 1 if you have `libngtcp2_crypto_wolfssl` library.])
  fi
fi

if test "x${have_wolfssl_quic}" = "xyes" &&
   test "x${request_libngtcp2}" = "xyes" &&
   test "x${have_libngtcp2_crypto_wolfssl}" != "xyes"; then
  AC_MSG_ERROR([libngtcp2_crypto_wolfssl was requested (--with-libngtcp2) but not found])
fi

# ngtcp2_crypto_quictls (for src)
have_libngtcp2_crypto_quictls=no
if test "x${have_ssl_provide_quic_data}" = "xyes" &&
   test "x${have_boringssl_quic}" != "xyes" &&
   test "x${request_libngtcp2}" != "xno"; then
  PKG_CHECK_MODULES([LIBNGTCP2_CRYPTO_QUICTLS],
                    [libngtcp2_crypto_quictls >= 1.0.0],
                    [have_libngtcp2_crypto_quictls=yes],
                    [have_libngtcp2_crypto_quictls=no])
  if test "x${have_libngtcp2_crypto_quictls}" = "xno"; then
    AC_MSG_NOTICE($LIBNGTCP2_CRYPTO_QUICTLS_PKG_ERRORS)
  else
    AC_DEFINE([HAVE_LIBNGTCP2_CRYPTO_QUICTLS], [1],
              [Define to 1 if you have `libngtcp2_crypto_quictls` library.])
  fi
fi

if test "x${have_ssl_provide_quic_data}" = "xyes" &&
   test "x${have_boringssl_quic}" != "xyes" &&
   test "x${request_libngtcp2}" = "xyes" &&
   test "x${have_libngtcp2_crypto_quictls}" != "xyes"; then
  AC_MSG_ERROR([libngtcp2_crypto_quictls was requested (--with-libngtcp2) but not found])
fi

# ngtcp2_crypto_boringssl (for src)
have_libngtcp2_crypto_boringssl=no
if test "x${have_boringssl_quic}" = "xyes" &&
   test "x${request_libngtcp2}" != "xno"; then
  PKG_CHECK_MODULES([LIBNGTCP2_CRYPTO_BORINGSSL],
                    [libngtcp2_crypto_boringssl >= 0.0.0],
                    [have_libngtcp2_crypto_boringssl=yes],
                    [have_libngtcp2_crypto_boringssl=no])
  if test "x${have_libngtcp2_crypto_boringssl}" = "xno"; then
    AC_MSG_NOTICE($LIBNGTCP2_CRYPTO_BORINGSSL_PKG_ERRORS)
  else
    AC_DEFINE([HAVE_LIBNGTCP2_CRYPTO_BORINGSSL], [1],
              [Define to 1 if you have `libngtcp2_crypto_boringssl` library.])
  fi
fi

if test "x${have_boringssl_quic}" = "xyes" &&
   test "x${request_libngtcp2}" = "xyes" &&
   test "x${have_libngtcp2_crypto_boringssl}" != "xyes"; then
  AC_MSG_ERROR([libngtcp2_crypto_boringssl was requested (--with-libngtcp2) but not found])
fi

# nghttp3 (for src)
have_libnghttp3=no
if test "x${request_libnghttp3}" != "xno"; then
  PKG_CHECK_MODULES([LIBNGHTTP3], [libnghttp3 >= 1.1.0], [have_libnghttp3=yes],
                    [have_libnghttp3=no])
  if test "x${have_libnghttp3}" = "xno"; then
    AC_MSG_NOTICE($LIBNGHTTP3_PKG_ERRORS)
  fi
fi

if test "x${request_libnghttp3}" = "xyes" &&
   test "x${have_libnghttp3}" != "xyes"; then
  AC_MSG_ERROR([libnghttp3 was requested (--with-libnghttp3) but not found])
fi

# libbpf (for src)
have_libbpf=no
if test "x${request_libbpf}" != "xno"; then
  PKG_CHECK_MODULES([LIBBPF], [libbpf >= 0.7.0], [have_libbpf=yes],
                    [have_libbpf=no])
  if test "x${have_libbpf}" = "xyes"; then
    AC_DEFINE([HAVE_LIBBPF], [1], [Define to 1 if you have `libbpf` library.])
    if test "x${BPFCFLAGS}" = "x"; then
      BPFCFLAGS="-Wall -O2 -g"
    fi
    # Add the include path for Debian
    EXTRABPFCFLAGS="-I/usr/include/$host_cpu-$host_os"
    AC_SUBST([EXTRABPFCFLAGS])

    AC_MSG_CHECKING([whether enum bpf_stats_type is defined in linux/bpf.h])
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
    [[
    #include <linux/bpf.h>
    ]],
    [[
    enum bpf_stats_type foo;
    (void)foo;
    ]])],
    [have_bpf_stats_type=yes],
    [have_bpf_stats_type=no])

    if test "x${have_bpf_stats_type}" = "xyes"; then
      AC_MSG_RESULT([yes])
      AC_DEFINE([HAVE_BPF_STATS_TYPE], [1],
                [Define to 1 if you have enum bpf_stats_type in linux/bpf.h.])
    else
      AC_MSG_RESULT([no])
    fi
  else
    AC_MSG_NOTICE($LIBBPF_PKG_ERRORS)
  fi
fi

if test "x${request_libbpf}" = "xyes" &&
   test "x${have_libbpf}" != "xyes"; then
  AC_MSG_ERROR([libbpf was requested (--with-libbpf) but not found])
fi

AM_CONDITIONAL([HAVE_LIBBPF], [ test "x${have_libbpf}" = "xyes" ])

# libbrotlienc (for src)
have_libbrotlienc=no
if test "x${request_libbrotlienc}" != "xno"; then
  PKG_CHECK_MODULES([LIBBROTLIENC], [libbrotlienc >= 1.0.9],
                    [have_libbrotlienc=yes],
                    [have_libbrotlienc=no])
  if test "x${have_libbrotlienc}" = "xno"; then
    AC_MSG_NOTICE($LIBBROTLIENC_PKG_ERRORS)
  fi
fi

if test "x${request_libbrotlienc}" = "xyes" &&
   test "x${have_libbrotlienc}" != "xyes"; then
  AC_MSG_ERROR([libbrotlienc was requested (--with-libbrotlienc) but not found])
fi

# libbrotlidec (for src)
have_libbrotlidec=no
if test "x${request_libbrotlidec}" != "xno"; then
  PKG_CHECK_MODULES([LIBBROTLIDEC], [libbrotlidec >= 1.0.9],
                    [have_libbrotlidec=yes],
                    [have_libbrotlidec=no])
  if test "x${have_libbrotlidec}" = "xno"; then
    AC_MSG_NOTICE($LIBBROTLIDEC_PKG_ERRORS)
  fi
fi

if test "x${request_libbrotlidec}" = "xyes" &&
   test "x${have_libbrotlidec}" != "xyes"; then
  AC_MSG_ERROR([libbrotlidec was requested (--with-libbrotlidec) but not found])
fi

have_libbrotli=no
if test "x${have_libbrotlienc}" = "xyes" &&
   test "x${have_libbrotlidec}" = "xyes"; then
  have_libbrotli=yes

  AC_DEFINE([HAVE_LIBBROTLI], [1],
            [Define to 1 if you have `libbrotlienc` and `libbrotlidec` libraries.])
fi

# libevent_openssl (for examples)
# 2.0.8 is required because we use evconnlistener_set_error_cb()
have_libevent_openssl=no
if test "x${request_libevent_openssl}" != "xno"; then
  PKG_CHECK_MODULES([LIBEVENT_OPENSSL], [libevent_openssl >= 2.0.8],
                    [have_libevent_openssl=yes], [have_libevent_openssl=no])
  if test "x${have_libevent_openssl}" = "xno"; then
    AC_MSG_NOTICE($LIBEVENT_OPENSSL_PKG_ERRORS)
  fi
fi

if test "x${request_libevent_openssl}" = "xyes" &&
   test "x${have_libevent_openssl}" != "xyes"; then
  AC_MSG_ERROR([libevent_openssl was requested (--with-libevent) but not found])
fi

# jansson (for src/nghttp, src/deflatehd and src/inflatehd)
have_jansson=no
if test "x${request_jansson}" != "xno"; then
  PKG_CHECK_MODULES([JANSSON], [jansson >= 2.5],
                    [have_jansson=yes], [have_jansson=no])
  if test "x${have_jansson}" = "xyes"; then
    AC_DEFINE([HAVE_JANSSON], [1],
              [Define to 1 if you have `libjansson` library.])
  else
    AC_MSG_NOTICE($JANSSON_PKG_ERRORS)
  fi
fi

if test "x${request_jansson}" = "xyes" &&
   test "x${have_jansson}" != "xyes"; then
  AC_MSG_ERROR([jansson was requested (--with-jansson) but not found])
fi

#  libsystemd (for src/nghttpx)
have_libsystemd=no
if test "x${request_systemd}" != "xno"; then
  PKG_CHECK_MODULES([SYSTEMD], [libsystemd >= 209], [have_libsystemd=yes],
                    [have_libsystemd=no])
  if test "x${have_libsystemd}" = "xyes"; then
    AC_DEFINE([HAVE_LIBSYSTEMD], [1],
              [Define to 1 if you have `libsystemd` library.])
  else
    AC_MSG_NOTICE($SYSTEMD_PKG_ERRORS)
  fi
fi

if test "x${request_systemd}" = "xyes" &&
   test "x${have_libsystemd}" != "xyes"; then
  AC_MSG_ERROR([systemd was requested (--with-systemd) but not found])
fi

# libxml2 (for src/nghttp)
have_libxml2=no
if test "x${request_libxml2}" != "xno"; then
  PKG_CHECK_MODULES([LIBXML2], [libxml-2.0 >= 2.6.26],
                    [have_libxml2=yes], [have_libxml2=no])
  if test "x${have_libxml2}" = "xyes"; then
    AC_DEFINE([HAVE_LIBXML2], [1], [Define to 1 if you have `libxml2` library.])
  else
    AC_MSG_NOTICE($LIBXML2_PKG_ERRORS)
  fi
fi

if test "x${request_libxml2}" = "xyes" &&
   test "x${have_libxml2}" != "xyes"; then
  AC_MSG_ERROR([libxml2 was requested (--with-libxml2) but not found])
fi

AM_CONDITIONAL([HAVE_LIBXML2], [ test "x${have_libxml2}" = "xyes" ])

# jemalloc
have_jemalloc=no
if test "x${request_jemalloc}" != "xno"; then
  if test "x${JEMALLOC_LIBS}" = "x" && test "x${JEMALLOC_CFLAGS}" = "x"; then
    save_LIBS=$LIBS
    AC_SEARCH_LIBS([malloc_stats_print], [jemalloc], [have_jemalloc=yes], [],
                   [$PTHREAD_LDFLAGS])

    if test "x${have_jemalloc}" = "xyes"; then
      jemalloc_libs=${ac_cv_search_malloc_stats_print}
    else
      # On Darwin, malloc_stats_print is je_malloc_stats_print
      AC_SEARCH_LIBS([je_malloc_stats_print], [jemalloc], [have_jemalloc=yes], [],
                     [$PTHREAD_LDFLAGS])

      if test "x${have_jemalloc}" = "xyes"; then
        jemalloc_libs=${ac_cv_search_je_malloc_stats_print}
      fi
    fi

    LIBS=$save_LIBS

    if test "x${have_jemalloc}" = "xyes" &&
       test "x${jemalloc_libs}" != "xnone required"; then
      JEMALLOC_LIBS=${jemalloc_libs}
    fi
  else
    have_jemalloc=yes
  fi
fi

if test "x${request_jemalloc}" = "xyes" &&
   test "x${have_jemalloc}" != "xyes"; then
  AC_MSG_ERROR([jemalloc was requested (--with-jemalloc) but not found])
fi

# The nghttp, nghttpd and nghttpx under src depend on zlib, OpenSSL,
# libev, and libc-ares.
enable_app=no
if test "x${request_app}" != "xno" &&
   test "x${have_zlib}" = "xyes" &&
   (test "x${have_openssl}" = "xyes" || test "x${have_wolfssl}" = "xyes") &&
   test "x${have_libev}" = "xyes" &&
   test "x${have_libcares}" = "xyes"; then
  enable_app=yes
fi

if test "x${request_app}" = "xyes" &&
   test "x${enable_app}" != "xyes"; then
  AC_MSG_ERROR([applications were requested (--enable-app) but dependencies are not met.])
fi

AM_CONDITIONAL([ENABLE_APP], [ test "x${enable_app}" = "xyes" ])

# Check HTTP/3 support
enable_http3=no
if test "x${request_http3}" != "xno" &&
   test "x${have_libngtcp2}" = "xyes" &&
   (test "x${have_libngtcp2_crypto_wolfssl}" = "xyes" ||
    test "x${have_libngtcp2_crypto_quictls}" = "xyes" ||
    test "x${have_libngtcp2_crypto_boringssl}" = "xyes") &&
   test "x${have_libnghttp3}" = "xyes"; then
  enable_http3=yes
  AC_DEFINE([ENABLE_HTTP3], [1], [Define to 1 if HTTP/3 is enabled.])
fi

if test "x${request_http3}" = "xyes" &&
   test "x${enable_http3}" != "xyes"; then
  AC_MSG_ERROR([HTTP/3 was requested (--enable-http3) but dependencies are not met.])
fi

AM_CONDITIONAL([ENABLE_HTTP3], [ test "x${enable_http3}" = "xyes" ])

enable_hpack_tools=no
# HPACK tools requires jansson
if test "x${request_hpack_tools}" != "xno" &&
   test "x${have_jansson}" = "xyes"; then
  enable_hpack_tools=yes
fi

if test "x${request_hpack_tools}" = "xyes" &&
   test "x${enable_hpack_tools}" != "xyes"; then
  AC_MSG_ERROR([HPACK tools were requested (--enable-hpack-tools) but dependencies are not met.])
fi

AM_CONDITIONAL([ENABLE_HPACK_TOOLS], [ test "x${enable_hpack_tools}" = "xyes" ])

# The example programs depend on OpenSSL and libevent_openssl
enable_examples=no
if test "x${request_examples}" != "xno" &&
   test "x${have_openssl}" = "xyes" &&
   test "x${have_libevent_openssl}" = "xyes"; then
  enable_examples=yes
fi

if test "x${request_examples}" = "xyes" &&
   test "x${enable_examples}" != "xyes"; then
  AC_MSG_ERROR([examples were requested (--enable-examples) but dependencies are not met.])
fi

AM_CONDITIONAL([ENABLE_EXAMPLES], [ test "x${enable_examples}" = "xyes" ])

# third-party only be built when needed

enable_third_party=no
have_mruby=no
have_neverbleed=no
if test "x${enable_examples}" = "xyes" ||
   test "x${enable_app}" = "xyes" ||
   test "x${enable_hpack_tools}" = "xyes"; then
  enable_third_party=yes

  # mruby (for src/nghttpx)
  if test "x${request_mruby}" = "xyes"; then
    # We are going to build mruby
    have_mruby=yes
    AC_DEFINE([HAVE_MRUBY], [1], [Define to 1 if you have `mruby` library.])
    LIBMRUBY_LIBS="-lmruby -lm"
    LIBMRUBY_CFLAGS=
    AC_SUBST([LIBMRUBY_LIBS])
    AC_SUBST([LIBMRUBY_CFLAGS])
  fi

  # neverbleed (for src/nghttpx)
  if test "x${request_neverbleed}" = "xyes"; then
    have_neverbleed=yes
    AC_DEFINE([HAVE_NEVERBLEED], [1], [Define to 1 if you have `neverbleed` library.])
  fi
fi

AM_CONDITIONAL([ENABLE_THIRD_PARTY], [ test "x${enable_third_party}" = "xyes" ])
AM_CONDITIONAL([HAVE_MRUBY], [test "x${have_mruby}" = "xyes"])
AM_CONDITIONAL([HAVE_NEVERBLEED], [test "x${have_neverbleed}" = "xyes"])

# failmalloc tests
enable_failmalloc=no
if test "x${request_failmalloc}" = "xyes"; then
  enable_failmalloc=yes
fi

AM_CONDITIONAL([ENABLE_FAILMALLOC], [ test "x${enable_failmalloc}" = "xyes" ])

# Checks for header files.
AC_HEADER_ASSERT
AC_CHECK_HEADERS([ \
  arpa/inet.h \
  fcntl.h \
  inttypes.h \
  limits.h \
  netdb.h \
  netinet/in.h \
  netinet/ip.h \
  pwd.h \
  stddef.h \
  stdint.h \
  stdlib.h \
  string.h \
  sys/socket.h \
  sys/time.h \
  syslog.h \
  unistd.h \
  windows.h \
])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT8_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_INT8_T
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_UID_T
AC_CHECK_TYPES([ptrdiff_t])
AC_C_BIGENDIAN
AC_C_INLINE
AC_SYS_LARGEFILE

AC_CHECK_MEMBER([struct tm.tm_gmtoff], [have_struct_tm_tm_gmtoff=yes],
                [have_struct_tm_tm_gmtoff=no], [[#include <time.h>]])

AC_CHECK_MEMBER([struct sockaddr_in.sin_len],
                [AC_DEFINE([HAVE_SOCKADDR_IN_SIN_LEN],[1],
                  [Define to 1 if struct sockaddr_in has sin_len member.])],
                [],
                [[
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
]])

AC_CHECK_MEMBER([struct sockaddr_in6.sin6_len],
                [AC_DEFINE([HAVE_SOCKADDR_IN6_SIN6_LEN],[1],
                  [Define to 1 if struct sockaddr_in6 has sin6_len member.])],
                [],
                [[
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
]])

if test "x$have_struct_tm_tm_gmtoff" = "xyes"; then
  AC_DEFINE([HAVE_STRUCT_TM_TM_GMTOFF], [1],
            [Define to 1 if you have `struct tm.tm_gmtoff` member.])
fi

# Checks for library functions.

# Don't check malloc, since it does not play nicely with C++ stdlib
# AC_FUNC_MALLOC

AC_FUNC_CHOWN
AC_FUNC_ERROR_AT_LINE
AC_FUNC_FORK
# Don't check realloc, since LeakSanitizer detects memory leak during check
# AC_FUNC_REALLOC
AC_FUNC_STRERROR_R
AC_FUNC_STRNLEN

AC_CHECK_FUNCS([ \
  _Exit \
  accept4 \
  clock_gettime \
  dup2 \
  getcwd \
  getpwnam \
  localtime_r \
  memchr \
  memmove \
  memset \
  mkostemp \
  pipe2 \
  socket \
  sqrt \
  strchr \
  strdup \
  strerror \
  strndup \
  strstr \
  strtol \
  strtoul \
  timegm \
])

# timerfd_create was added in linux kernel 2.6.25

AC_CHECK_FUNC([timerfd_create],
              [have_timerfd_create=yes], [have_timerfd_create=no])

AC_MSG_CHECKING([checking for GetTickCount64])
AC_LINK_IFELSE([AC_LANG_PROGRAM(
[[
#include <windows.h>
]],
[[
GetTickCount64();
]])],
[have_gettickcount64=yes],
[have_gettickcount64=no])

if test "x${have_gettickcount64}" = "xyes"; then
  AC_MSG_RESULT([yes])
  AC_DEFINE([HAVE_GETTICKCOUNT64], [1],
            [Define to 1 if you have `GetTickCount64` function.])
else
  AC_MSG_RESULT([no])
fi

# For cygwin: we can link initgroups, so AC_CHECK_FUNCS succeeds, but
# cygwin disables initgroups due to feature test macro magic with our
# configuration.  FreeBSD declares initgroups() in unistd.h.
AC_CHECK_DECLS([initgroups], [], [], [[
  #ifdef HAVE_UNISTD_H
  # include <unistd.h>
  #endif
  #include <grp.h>
]])

AC_CHECK_DECLS([CLOCK_MONOTONIC], [], [], [[
#include <time.h>
]])

save_CFLAGS=$CFLAGS
save_CXXFLAGS=$CXXFLAGS

CFLAGS=
CXXFLAGS=

if test "x$werror" != "xno"; then
    # For C compiler
    AX_CHECK_COMPILE_FLAG([-Wall], [CFLAGS="$CFLAGS -Wall"])
    AX_CHECK_COMPILE_FLAG([-Wextra], [CFLAGS="$CFLAGS -Wextra"])
    AX_CHECK_COMPILE_FLAG([-Werror], [CFLAGS="$CFLAGS -Werror"])
    AX_CHECK_COMPILE_FLAG([-Wmissing-prototypes], [CFLAGS="$CFLAGS -Wmissing-prototypes"])
    AX_CHECK_COMPILE_FLAG([-Wstrict-prototypes], [CFLAGS="$CFLAGS -Wstrict-prototypes"])
    AX_CHECK_COMPILE_FLAG([-Wmissing-declarations], [CFLAGS="$CFLAGS -Wmissing-declarations"])
    AX_CHECK_COMPILE_FLAG([-Wpointer-arith], [CFLAGS="$CFLAGS -Wpointer-arith"])
    AX_CHECK_COMPILE_FLAG([-Wdeclaration-after-statement], [CFLAGS="$CFLAGS -Wdeclaration-after-statement"])
    AX_CHECK_COMPILE_FLAG([-Wformat-security], [CFLAGS="$CFLAGS -Wformat-security"])
    AX_CHECK_COMPILE_FLAG([-Wwrite-strings], [CFLAGS="$CFLAGS -Wwrite-strings"])
    AX_CHECK_COMPILE_FLAG([-Wshadow], [CFLAGS="$CFLAGS -Wshadow"])
    AX_CHECK_COMPILE_FLAG([-Winline], [CFLAGS="$CFLAGS -Winline"])
    AX_CHECK_COMPILE_FLAG([-Wnested-externs], [CFLAGS="$CFLAGS -Wnested-externs"])
    AX_CHECK_COMPILE_FLAG([-Wfloat-equal], [CFLAGS="$CFLAGS -Wfloat-equal"])
    AX_CHECK_COMPILE_FLAG([-Wundef], [CFLAGS="$CFLAGS -Wundef"])
    AX_CHECK_COMPILE_FLAG([-Wendif-labels], [CFLAGS="$CFLAGS -Wendif-labels"])
    AX_CHECK_COMPILE_FLAG([-Wempty-body], [CFLAGS="$CFLAGS -Wempty-body"])
    AX_CHECK_COMPILE_FLAG([-Wcast-align], [CFLAGS="$CFLAGS -Wcast-align"])
    AX_CHECK_COMPILE_FLAG([-Wclobbered], [CFLAGS="$CFLAGS -Wclobbered"])
    AX_CHECK_COMPILE_FLAG([-Wvla], [CFLAGS="$CFLAGS -Wvla"])
    AX_CHECK_COMPILE_FLAG([-Wpragmas], [CFLAGS="$CFLAGS -Wpragmas"])
    AX_CHECK_COMPILE_FLAG([-Wunreachable-code], [CFLAGS="$CFLAGS -Wunreachable-code"])
    AX_CHECK_COMPILE_FLAG([-Waddress], [CFLAGS="$CFLAGS -Waddress"])
    AX_CHECK_COMPILE_FLAG([-Wattributes], [CFLAGS="$CFLAGS -Wattributes"])
    AX_CHECK_COMPILE_FLAG([-Wdiv-by-zero], [CFLAGS="$CFLAGS -Wdiv-by-zero"])
    AX_CHECK_COMPILE_FLAG([-Wshorten-64-to-32], [CFLAGS="$CFLAGS -Wshorten-64-to-32"])

    AX_CHECK_COMPILE_FLAG([-Wconversion], [CFLAGS="$CFLAGS -Wconversion"])
    AX_CHECK_COMPILE_FLAG([-Wextended-offsetof], [CFLAGS="$CFLAGS -Wextended-offsetof"])
    AX_CHECK_COMPILE_FLAG([-Wformat-nonliteral], [CFLAGS="$CFLAGS -Wformat-nonliteral"])
    AX_CHECK_COMPILE_FLAG([-Wlanguage-extension-token], [CFLAGS="$CFLAGS -Wlanguage-extension-token"])
    AX_CHECK_COMPILE_FLAG([-Wmissing-field-initializers], [CFLAGS="$CFLAGS -Wmissing-field-initializers"])
    AX_CHECK_COMPILE_FLAG([-Wmissing-noreturn], [CFLAGS="$CFLAGS -Wmissing-noreturn"])
    AX_CHECK_COMPILE_FLAG([-Wmissing-variable-declarations], [CFLAGS="$CFLAGS -Wmissing-variable-declarations"])
    # Not used because we cannot change public structs
    # AX_CHECK_COMPILE_FLAG([-Wpadded], [CFLAGS="$CFLAGS -Wpadded"])
    AX_CHECK_COMPILE_FLAG([-Wsign-conversion], [CFLAGS="$CFLAGS -Wsign-conversion"])
    # Not used because this basically disallows default case
    # AX_CHECK_COMPILE_FLAG([-Wswitch-enum], [CFLAGS="$CFLAGS -Wswitch-enum"])
    AX_CHECK_COMPILE_FLAG([-Wunreachable-code-break], [CFLAGS="$CFLAGS -Wunreachable-code-break"])
    AX_CHECK_COMPILE_FLAG([-Wunused-macros], [CFLAGS="$CFLAGS -Wunused-macros"])
    AX_CHECK_COMPILE_FLAG([-Wunused-parameter], [CFLAGS="$CFLAGS -Wunused-parameter"])
    AX_CHECK_COMPILE_FLAG([-Wredundant-decls], [CFLAGS="$CFLAGS -Wredundant-decls"])
    # Only work with Clang for the moment
    AX_CHECK_COMPILE_FLAG([-Wheader-guard], [CFLAGS="$CFLAGS -Wheader-guard"])
    AX_CHECK_COMPILE_FLAG([-Wsometimes-uninitialized], [CFLAGS="$CFLAGS -Wsometimes-uninitialized"])
    AX_CHECK_COMPILE_FLAG([-Wextra-semi], [CFLAGS="$CFLAGS -Wextra-semi"])

    # This is required because we pass format string as "const char*.
    AX_CHECK_COMPILE_FLAG([-Wno-format-nonliteral], [CFLAGS="$CFLAGS -Wno-format-nonliteral"])

    # For C++ compiler
    AC_LANG_PUSH(C++)
    AX_CHECK_COMPILE_FLAG([-Wall], [CXXFLAGS="$CXXFLAGS -Wall"])
    AX_CHECK_COMPILE_FLAG([-Werror], [CXXFLAGS="$CXXFLAGS -Werror"])
    AX_CHECK_COMPILE_FLAG([-Wformat-security], [CXXFLAGS="$CXXFLAGS -Wformat-security"])
    AX_CHECK_COMPILE_FLAG([-Wsometimes-uninitialized], [CXXFLAGS="$CXXFLAGS -Wsometimes-uninitialized"])
    AX_CHECK_COMPILE_FLAG([-Wextra-semi], [CXXFLAGS="$CXXFLAGS -Wextra-semi"])
    # Disable noexcept-type warning of g++-7.  This is not harmful as
    # long as all source files are compiled with the same compiler.
    AX_CHECK_COMPILE_FLAG([-Wno-noexcept-type], [CXXFLAGS="$CXXFLAGS -Wno-noexcept-type"])
    # clang++-18 warns this when building with wolfSSL >= v5.7.6-stable.
    AX_CHECK_COMPILE_FLAG([-Wno-extern-c-compat], [CXXFLAGS="$CXXFLAGS -Wno-extern-c-compat"])
    AC_LANG_POP()
fi

WARNCFLAGS=$CFLAGS
WARNCXXFLAGS=$CXXFLAGS

CFLAGS=$save_CFLAGS
CXXFLAGS=$save_CXXFLAGS

AC_SUBST([WARNCFLAGS])
AC_SUBST([WARNCXXFLAGS])

EXTRACFLAG=
AX_CHECK_COMPILE_FLAG([-fvisibility=hidden], [EXTRACFLAG="-fvisibility=hidden"])

AC_SUBST([EXTRACFLAG])

if test "x$debug" != "xno"; then
    AC_DEFINE([DEBUGBUILD], [1], [Define to 1 to enable debug output.])
fi

enable_threads=yes
# Some platform does not have working std::future.  We disable
# threading for those platforms.
if test "x$threads" != "xyes" ||
   test "x$have_std_future" != "xyes"; then
    enable_threads=no
    AC_DEFINE([NOTHREADS], [1], [Define to 1 if you want to disable threads.])
fi

# propagate $enable_static to tests/Makefile.am
AM_CONDITIONAL([ENABLE_STATIC], [test "x$enable_static" = "xyes"])

AC_SUBST([TESTLDADD])
AC_SUBST([APPLDFLAGS])

AC_CONFIG_FILES([
  Makefile
  lib/Makefile
  lib/libnghttp2.pc
  lib/includes/Makefile
  lib/includes/nghttp2/nghttp2ver.h
  tests/Makefile
  tests/testdata/Makefile
  third-party/Makefile
  src/Makefile
  src/testdata/Makefile
  bpf/Makefile
  examples/Makefile
  integration-tests/Makefile
  integration-tests/config.go
  integration-tests/setenv
  doc/Makefile
  doc/conf.py
  doc/index.rst
  doc/package_README.rst
  doc/tutorial-client.rst
  doc/tutorial-server.rst
  doc/tutorial-hpack.rst
  doc/nghttpx-howto.rst
  doc/h2load-howto.rst
  doc/building-android-binary.rst
  doc/nghttp2.h.rst
  doc/nghttp2ver.h.rst
  doc/contribute.rst
  contrib/Makefile
  script/Makefile
])
AC_OUTPUT

AC_MSG_NOTICE([summary of build options:

    Package version: ${VERSION}
    Library version: $LT_CURRENT:$LT_REVISION:$LT_AGE
    Install prefix:  ${prefix}
    System types:
      Build:         ${build}
      Host:          ${host}
      Target:        ${target}
    Compiler:
      C compiler:     ${CC}
      CFLAGS:         ${CFLAGS}
      LDFLAGS:        ${LDFLAGS}
      C++ compiler:   ${CXX}
      CXXFLAGS:       ${CXXFLAGS}
      CXXCPP:         ${CXXCPP}
      C preprocessor: ${CPP}
      CPPFLAGS:       ${CPPFLAGS}
      WARNCFLAGS:     ${WARNCFLAGS}
      WARNCXXFLAGS:   ${WARNCXXFLAGS}
      CXX1XCXXFLAGS:  ${CXX1XCXXFLAGS}
      EXTRACFLAG:     ${EXTRACFLAG}
      BPFCFLAGS:      ${BPFCFLAGS}
      EXTRABPFCFLAGS: ${EXTRABPFCFLAGS}
      LIBS:           ${LIBS}
      DEFS:           ${DEFS}
      EXTRA_DEFS:     ${EXTRA_DEFS}
    Library:
      Shared:         ${enable_shared}
      Static:         ${enable_static}
    Libtool:
      LIBTOOL_LDFLAGS: ${LIBTOOL_LDFLAGS}
    Python:
      Python:         ${PYTHON}
      PYTHON_VERSION: ${PYTHON_VERSION}
    Test:
      Failmalloc:     ${enable_failmalloc}
    Libs:
      wolfSSL:        ${have_wolfssl} (CFLAGS='${WOLFSSL_CFLAGS}' LIBS='${WOLFSSL_LIBS}')
      OpenSSL:        ${have_openssl} (CFLAGS='${OPENSSL_CFLAGS}' LIBS='${OPENSSL_LIBS}')
      Libxml2:        ${have_libxml2} (CFLAGS='${LIBXML2_CFLAGS}' LIBS='${LIBXML2_LIBS}')
      Libev:          ${have_libev} (CFLAGS='${LIBEV_CFLAGS}' LIBS='${LIBEV_LIBS}')
      Libc-ares:      ${have_libcares} (CFLAGS='${LIBCARES_CFLAGS}' LIBS='${LIBCARES_LIBS}')
      libngtcp2:      ${have_libngtcp2} (CFLAGS='${LIBNGTCP2_CFLAGS}' LIBS='${LIBNGTCP2_LIBS}')
      libngtcp2_crypto_quictls: ${have_libngtcp2_crypto_quictls} (CFLAGS='${LIBNGTCP2_CRYPTO_QUICTLS_CFLAGS}' LIBS='${LIBNGTCP2_CRYPTO_QUICTLS_LIBS}')
      libngtcp2_crypto_boringssl: ${have_libngtcp2_crypto_boringssl} (CFLAGS='${LIBNGTCP2_CRYPTO_BORINGSSL_CFLAGS}' LIBS='${LIBNGTCP2_CRYPTO_BORINGSSL_LIBS}')
      libnghttp3:     ${have_libnghttp3} (CFLAGS='${LIBNGHTTP3_CFLAGS}' LIBS='${LIBNGHTTP3_LIBS}')
      libbpf:         ${have_libbpf} (CFLAGS='${LIBBPF_CFLAGS}' LIBS='${LIBBPF_LIBS}')
      Libevent(SSL):  ${have_libevent_openssl} (CFLAGS='${LIBEVENT_OPENSSL_CFLAGS}' LIBS='${LIBEVENT_OPENSSL_LIBS}')
      Jansson:        ${have_jansson} (CFLAGS='${JANSSON_CFLAGS}' LIBS='${JANSSON_LIBS}')
      Jemalloc:       ${have_jemalloc} (CFLAGS='${JEMALLOC_CFLAGS}' LIBS='${JEMALLOC_LIBS}')
      Zlib:           ${have_zlib} (CFLAGS='${ZLIB_CFLAGS}' LIBS='${ZLIB_LIBS}')
      Systemd:        ${have_libsystemd} (CFLAGS='${SYSTEMD_CFLAGS}' LIBS='${SYSTEMD_LIBS}')
      Libbrotlienc:   ${have_libbrotlienc} (CFLAGS="${LIBBROTLIENC_CFLAGS}' LIBS='${LIBBROTLIENC_LIBS}')
      Libbrotlidec:   ${have_libbrotlidec} (CFLAGS="${LIBBROTLIDEC_CFLAGS}' LIBS='${LIBBROTLIDEC_LIBS}')
    Third-party:
      http-parser:    ${enable_third_party}
      MRuby:          ${have_mruby} (CFLAGS='${LIBMRUBY_CFLAGS}' LIBS='${LIBMRUBY_LIBS}')
      Neverbleed:     ${have_neverbleed}
    Features:
      Applications:   ${enable_app}
      HPACK tools:    ${enable_hpack_tools}
      Examples:       ${enable_examples}
      Threading:      ${enable_threads}
      HTTP/3 (EXPERIMENTAL): ${enable_http3}
])
