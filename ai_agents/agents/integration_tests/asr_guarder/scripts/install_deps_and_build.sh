#!/usr/bin/env bash

# mac, linux
OS="linux"

# x64, arm64
CPU="x64"

# debug, release
BUILD_TYPE="release"

PIP_INSTALL_CMD=${PIP_INSTALL_CMD:-"uv pip install --system"}

install_python_requirements() {
  local app_dir=$1

  if [[ -f "requirements.txt" ]]; then
    ${PIP_INSTALL_CMD} install -r requirements.txt
  fi

  # traverse the ten_packages/extension directory to find the requirements.txt
  if [[ -d "ten_packages/extension" ]]; then
    for extension in ten_packages/extension/*; do
      if [[ -f "$extension/requirements.txt" ]]; then
        ${PIP_INSTALL_CMD} -r $extension/requirements.txt
      fi
    done
  fi

  # traverse the ten_packages/system directory to find the requirements.txt
  if [[ -d "ten_packages/system" ]]; then
    for extension in ten_packages/system/*; do
      if [[ -f "$extension/requirements.txt" ]]; then
        ${PIP_INSTALL_CMD} -r $extension/requirements.txt
      fi
    done
  fi
}

main() {
  APP_HOME=$(
    cd $(dirname $0)/..
    pwd
  )

  if [[ $1 == "-clean" ]]; then
    clean $APP_HOME
    exit 0
  fi

  if [[ $# -ne 2 ]]; then
    echo "Usage: $0 <os> <cpu>"
    exit 1
  fi

  OS=$1
  CPU=$2

  echo -e "#include <stdio.h>\n#include <immintrin.h>\nint main() { __m256 a = _mm256_setzero_ps(); return 0; }" > /tmp/test.c
  if gcc -mavx2 /tmp/test.c -o /tmp/test && ! /tmp/test; then
    echo "FATAL: unsupported platform."
    echo "       Please UNCHECK the 'Use Rosetta for x86_64/amd64 emulation on Apple Silicon' Docker Desktop setting if you're running on mac."

    exit 1
  fi

  if [[ ! -f $APP_HOME/manifest.json ]]; then
    echo "FATAL: manifest.json is required."
    exit 1
  fi

  # Install all dependencies specified in manifest.json.
  echo "install dependencies..."
  tman -y install

  # install python requirements
  echo "install_python_requirements..."
  install_python_requirements $APP_HOME
}

main "$@"
