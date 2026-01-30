#!/bin/sh -e

libdir="@abs_top_builddir@/lib"
if [ -d "$libdir/.libs" ]; then
    libdir="$libdir/.libs"
fi

export CGO_CFLAGS="-I@abs_top_srcdir@/lib/includes -I@abs_top_builddir@/lib/includes @CFLAGS@"
export CGO_CPPFLAGS="@CPPFLAGS@"
export CGO_LDFLAGS="-L$libdir @LDFLAGS@"
export LD_LIBRARY_PATH="$libdir"
export DYLD_LIBRARY_PATH="$libdir"
export GODEBUG=cgocheck=0
"$@"
