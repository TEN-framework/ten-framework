#!/bin/sh -e

autoreconf -i
git submodule update --init
./configure --with-mruby --with-neverbleed
make -j8 distcheck DISTCHECK_CONFIGURE_FLAGS="--with-mruby --with-neverbleed --enable-werror"
