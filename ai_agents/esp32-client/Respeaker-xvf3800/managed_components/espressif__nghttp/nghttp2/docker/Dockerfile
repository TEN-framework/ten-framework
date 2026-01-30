FROM debian:12 as build

ARG NGHTTP2_BRANCH=master

RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        git clang make binutils autoconf automake autotools-dev libtool \
        pkg-config cmake cmake-data \
        zlib1g-dev libev-dev libjemalloc-dev ruby-dev libc-ares-dev bison \
        libelf-dev libbrotli-dev

RUN git clone --recursive --shallow-submodules --depth 1 -b v1.46.1 https://github.com/aws/aws-lc && \
    cd aws-lc && \
    cmake -B build -DDISABLE_GO=ON && \
    make -j$(nproc) -C build && \
    cmake --install build && \
    cd .. && \
    rm -rf aws-lc

RUN git clone --recursive --shallow-submodules --depth 1 -b v1.8.0 https://github.com/ngtcp2/nghttp3 && \
    cd nghttp3 && \
    autoreconf -i && \
    ./configure --disable-dependency-tracking --enable-lib-only && \
    make -j$(nproc) && \
    make install-strip && \
    cd .. && \
    rm -rf nghttp3

RUN git clone --recursive --shallow-submodules --depth 1 -b v1.11.0 https://github.com/ngtcp2/ngtcp2 && \
    cd ngtcp2 && \
    autoreconf -i && \
    ./configure --disable-dependency-tracking --enable-lib-only \
        --with-boringssl \
        LIBTOOL_LDFLAGS="-static-libtool-libs" \
        BORINGSSL_LIBS="-l:libssl.a -l:libcrypto.a" \
        PKG_CONFIG_PATH="/usr/local/lib64/pkgconfig" && \
    make -j$(nproc) && \
    make install-strip && \
    cd .. && \
    rm -rf ngtcp2

RUN git clone --depth 1 -b v1.5.0 https://github.com/libbpf/libbpf && \
    cd libbpf && \
    PREFIX=/usr/local make -C src install && \
    cd .. && \
    rm -rf libbpf

RUN git clone --recursive --shallow-submodules --depth 1 -b $NGHTTP2_BRANCH https://github.com/nghttp2/nghttp2 && \
    cd nghttp2 && \
    autoreconf -i && \
    ./configure --disable-dependency-tracking --disable-examples \
        --disable-hpack-tools \
        --with-mruby \
        --enable-http3 --with-libbpf \
        --with-libbrotlienc --with-libbrotlidec \
        CC=clang CXX=clang++ \
        LIBTOOL_LDFLAGS="-static-libtool-libs" \
        OPENSSL_LIBS="-l:libssl.a -l:libcrypto.a" \
        LIBEV_LIBS="-l:libev.a" \
        JEMALLOC_LIBS="-l:libjemalloc.a" \
        LIBCARES_LIBS="-l:libcares.a" \
        ZLIB_LIBS="-l:libz.a" \
        LIBBPF_LIBS="-L/usr/local/lib64 -l:libbpf.a -l:libelf.a" \
        LIBBROTLIENC_LIBS="-l:libbrotlienc.a -l:libbrotlicommon.a" \
        LIBBROTLIDEC_LIBS="-l:libbrotlidec.a -l:libbrotlicommon.a" \
        LDFLAGS="-static-libgcc -static-libstdc++" \
        PKG_CONFIG_PATH="/usr/local/lib64/pkgconfig" && \
    make -j$(nproc) install-strip && \
    cd .. && \
    rm -rf nghttp2

FROM gcr.io/distroless/base-nossl-debian12

COPY --from=build --link \
    /usr/local/share/nghttp2/ \
    /usr/local/share/nghttp2/
COPY --from=build --link \
    /usr/local/bin/h2load \
    /usr/local/bin/nghttpx \
    /usr/local/bin/nghttp \
    /usr/local/bin/nghttpd \
    /usr/local/bin/
COPY --from=build --link /usr/local/lib/nghttp2/reuseport_kern.o \
    /usr/local/lib/nghttp2/
