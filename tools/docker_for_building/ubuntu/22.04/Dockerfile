FROM ubuntu:22.04

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get clean && apt-get update && apt-get install -y --no-install-recommends \
  apt-utils \
  software-properties-common \
  lsb-release \
  ca-certificates \
  wget \
  curl \
  build-essential \
  vim \
  cmake \
  pkg-config \
  locate \
  iwyu \
  libssl-dev \
  p7zip-full \
  p7zip-rar \
  git \
  tree \
  zip \
  unzip \
  jq \
  cpulimit \
  util-linux \
  libasan5 \
  autoconf \
  libtool \
  uuid-dev \
  libmsgpack-dev \
  libmysqlclient-dev \
  libmysqlcppconn-dev \
  libcrypto++-dev \
  libexpat1-dev \
  libcurl4-gnutls-dev \
  gpg-agent \
  make \
  libavformat-dev \
  libavfilter-dev \
  libx264-dev \
  libxdamage1 \
  libxcomposite-dev \
  libdrm-dev \
  zlib1g-dev \
  libncurses5-dev \
  libgdbm-dev \
  libnss3-dev \
  libreadline-dev \
  libffi-dev \
  libunwind-dev \
  libasound2 \
  python3 \
  python3-dev \
  python3-pip \
  python3-venv

RUN export ARCH=$(dpkg --print-architecture) && \
  if [ ${ARCH} = "amd64" ]; then \
  apt-get install -y --no-install-recommends g++-multilib; \
  fi

# =======================================
# Installing Python deps

RUN pip3 install --upgrade setuptools pip && \
  pip3 install requests oss2 && \
  pip3 install --use-pep517 python-dotenv jinja2 && \
  ln -sf /usr/bin/python3.10-config /usr/bin/python3-config && \
  ln -sf /usr/bin/python3 /usr/bin/python

# =======================================
# Installing clang18

RUN wget --no-check-certificate -O - https://apt.llvm.org/llvm-snapshot.gpg.key | apt-key add - && \
  add-apt-repository "deb http://apt.llvm.org/$(lsb_release -cs)/   llvm-toolchain-$(lsb_release -cs)-18  main" && \
  apt-get update && \
  apt-get install -y clang-18 \
  clang++-18 \
  clang-tools-18 \
  clang-tidy-18 \
  clang-format-18 \
  libc++-18-dev \
  libc++1-18 \
  libc++abi-18-dev \
  libc++abi1-18 \
  clangd-18 && \
  update-alternatives --install /usr/bin/clang clang /usr/bin/clang-18 100 && \
  update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-18 100 && \
  update-alternatives --install /usr/bin/clang-format clang-format /usr/bin/clang-format-18 100 && \
  update-alternatives --install /usr/bin/clang-tidy clang-tidy /usr/bin/clang-tidy-18 100 && \
  update-alternatives --install /usr/bin/clang-check clang-check /usr/bin/clang-check-18 100 && \
  update-alternatives --install /usr/bin/clangd clangd /usr/bin/clangd-18 100

# =======================================
# Install golang

# /root/go is GOPATH.
ENV PATH="$PATH:/usr/local/go/bin:/root/go/bin"

# TEN go binding needs to be compatible with GO 1.20, so we need to install GO
# 1.20 to check the compatibility.
RUN export ARCH=$(dpkg --print-architecture) && curl -OL https://go.dev/dl/go1.22.3.linux-${ARCH}.tar.gz && \
  rm -rf /usr/local/go && tar -C /usr/local -xvf go1.22.3.linux-${ARCH}.tar.gz && rm go1.22.3.linux-${ARCH}.tar.gz && \
  go install golang.org/dl/go1.20.12@latest && go1.20.12 download

# =======================================
# Install cargo for rust.

RUN curl https://sh.rustup.rs -sSf | sh -s -- -y && \
  /root/.cargo/bin/rustup install nightly && \
  /root/.cargo/bin/rustup default nightly && \
  /root/.cargo/bin/cargo install --force cbindgen

ENV PATH="/root/.cargo/bin:$PATH"

# =======================================
# Cleanup

RUN apt-get clean && rm -rf /var/lib/apt/lists/* && rm -rf /tmp/*
