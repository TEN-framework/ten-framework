# Dockerfile
#
# Purpose
# -------
# Defines a Docker container suitable to build and run all tests (all.sh),
# except for those that use a proprietary toolchain.

# Copyright The Mbed TLS Contributors
# SPDX-License-Identifier: Apache-2.0
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may
# not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#  http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
ARG MAKEFLAGS_PARALLEL=""
ARG MY_REGISTRY=

FROM ${MY_REGISTRY}ubuntu:bionic


ENV DEBIAN_FRONTEND noninteractive

RUN apt-get update \
    && apt-get -y install software-properties-common \
    && rm -rf /var/lib/apt/lists

RUN add-apt-repository -y ppa:team-gcc-arm-embedded/ppa

RUN apt-get update \
    && apt-get -y install \
    # mbedtls build/test dependencies
    build-essential \
    clang \
    cmake \
    doxygen \
    gcc-arm-none-eabi \
    gcc-mingw-w64-i686 \
    gcc-multilib \
    g++-multilib \
    gdb \
    git \
    graphviz \
    lsof \
    python \
    python3-pip \
    python3 \
    pylint3 \
    valgrind \
    wget \
    # libnettle build dependencies
    libgmp-dev \
    m4 \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

# Jinja2 is required for driver dispatch code generation.
RUN python3 -m pip install \
    jinja2==2.10.1 types-jinja2

# Build a static, legacy openssl from sources with sslv3 enabled
# Based on https://gist.github.com/bmaupin/8caca3a1e8c3c5686141 (build-openssl.sh)
# Note: openssl-1.0.2 and earlier has known build issues with parallel make.
RUN cd /tmp \
    && wget https://www.openssl.org/source/old/1.0.1/openssl-1.0.1j.tar.gz -qO- | tar xz \
    && cd openssl-1.0.1j \
    && ./config --openssldir=/usr/local/openssl-1.0.1j no-shared \
    && (make ${MAKEFLAGS_PARALLEL} || make -j 1) \
    && make install_sw \
    && rm -rf /tmp/openssl*
ENV OPENSSL_LEGACY=/usr/local/openssl-1.0.1j/bin/openssl

# Build OPENSSL as 1.0.2g
RUN cd /tmp \
    && wget https://www.openssl.org/source/old/1.0.2/openssl-1.0.2g.tar.gz -qO- | tar xz \
    && cd openssl-1.0.2g \
    && ./config --openssldir=/usr/local/openssl-1.0.2g no-shared \
    && (make ${MAKEFLAGS_PARALLEL} || make -j 1) \
    && make install_sw \
    && rm -rf /tmp/openssl*
ENV OPENSSL=/usr/local/openssl-1.0.2g/bin/openssl

# Build a new openssl binary for ARIA/CHACHA20 support
# Based on https://gist.github.com/bmaupin/8caca3a1e8c3c5686141 (build-openssl.sh)
RUN cd /tmp \
    && wget https://www.openssl.org/source/openssl-1.1.1a.tar.gz -qO- | tar xz \
    && cd openssl-1.1.1a \
    && ./config --prefix=/usr/local/openssl-1.1.1a -Wl,--enable-new-dtags,-rpath,'${LIBRPATH}' no-shared \
    && make ${MAKEFLAGS_PARALLEL} \
    && make install_sw \
    && rm -rf /tmp/openssl*
ENV OPENSSL_NEXT=/usr/local/openssl-1.1.1a/bin/openssl

# Build libnettle 2.7.1 (needed by legacy gnutls)
RUN cd /tmp \
    && wget https://ftp.gnu.org/gnu/nettle/nettle-2.7.1.tar.gz -qO- | tar xz \
    && cd nettle-2.7.1 \
    && ./configure --disable-documentation \
    && make ${MAKEFLAGS_PARALLEL} \
    && make install \
    && /sbin/ldconfig \
    && rm -rf /tmp/nettle*

# Build legacy gnutls (3.3.8)
RUN cd /tmp \
    && wget https://www.gnupg.org/ftp/gcrypt/gnutls/v3.3/gnutls-3.3.8.tar.xz -qO- | tar xJ \
    && cd gnutls-3.3.8 \
    && ./configure --prefix=/usr/local/gnutls-3.3.8 --exec_prefix=/usr/local/gnutls-3.3.8 --disable-shared --disable-guile --disable-doc \
    && make ${MAKEFLAGS_PARALLEL} \
    && make install \
    && rm -rf /tmp/gnutls*
ENV GNUTLS_LEGACY_CLI=/usr/local/gnutls-3.3.8/bin/gnutls-cli
ENV GNUTLS_LEGACY_SERV=/usr/local/gnutls-3.3.8/bin/gnutls-serv

# Build libnettle 3.1 (needed by gnutls)
RUN cd /tmp \
    && wget https://ftp.gnu.org/gnu/nettle/nettle-3.1.tar.gz -qO- | tar xz \
    && cd nettle-3.1 \
    && ./configure --disable-documentation \
    && make ${MAKEFLAGS_PARALLEL} \
    && make install \
    && /sbin/ldconfig \
    && rm -rf /tmp/nettle*

# Build gnutls (3.4.10)
RUN cd /tmp \
    && wget https://www.gnupg.org/ftp/gcrypt/gnutls/v3.4/gnutls-3.4.10.tar.xz -qO- | tar xJ \
    && cd gnutls-3.4.10 \
    && ./configure --prefix=/usr/local/gnutls-3.4.10 --exec_prefix=/usr/local/gnutls-3.4.10 \
        --with-included-libtasn1 --without-p11-kit \
        --disable-shared --disable-guile --disable-doc \
    && make ${MAKEFLAGS_PARALLEL} \
    && make install \
    && rm -rf /tmp/gnutls*
ENV GNUTLS_CLI=/usr/local/gnutls-3.4.10/bin/gnutls-cli
ENV GNUTLS_SERV=/usr/local/gnutls-3.4.10/bin/gnutls-serv

# Build libnettle 3.7.3 (needed by gnutls next)
RUN cd /tmp \
    && wget https://ftp.gnu.org/gnu/nettle/nettle-3.7.3.tar.gz -qO- | tar xz \
    && cd nettle-3.7.3 \
    && ./configure --disable-documentation \
    && make ${MAKEFLAGS_PARALLEL} \
    && make install \
    && /sbin/ldconfig \
    && rm -rf /tmp/nettle*

# Build gnutls next (3.7.2)
RUN cd /tmp \
    && wget https://www.gnupg.org/ftp/gcrypt/gnutls/v3.7/gnutls-3.7.2.tar.xz -qO- | tar xJ \
    && cd gnutls-3.7.2 \
    && ./configure --prefix=/usr/local/gnutls-3.7.2 --exec_prefix=/usr/local/gnutls-3.7.2 \
        --with-included-libtasn1 --with-included-unistring --without-p11-kit \
        --disable-shared --disable-guile --disable-doc \
    && make ${MAKEFLAGS_PARALLEL} \
    && make install \
    && rm -rf /tmp/gnutls*

ENV GNUTLS_NEXT_CLI=/usr/local/gnutls-3.7.2/bin/gnutls-cli
ENV GNUTLS_NEXT_SERV=/usr/local/gnutls-3.7.2/bin/gnutls-serv
