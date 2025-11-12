#!/bin/sh
#
# Copyright The Mbed TLS Contributors
# SPDX-License-Identifier: Apache-2.0
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may
# not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Purpose
#
# Show symbols in the X.509 and TLS libraries that are defined in another
# libmbedtlsXXX.a library. This is usually done to list Crypto dependencies.
#
# Usage:
# - build the library with debug symbols and the config you're interested in
#   (default, full minus MBEDTLS_USE_PSA_CRYPTO, full, etc.)
# - run this script with the name of your config as the only argument

set -eu

# list mbedtls_ symbols of a given type in a static library
syms() {
    TYPE="$1"
    FILE="$2"

    nm "$FILE" | sed -n "s/[0-9a-f ]*${TYPE} \(mbedtls_.*\)/\1/p" | sort -u
}

# create listings for the given library
list() {
    NAME="$1"
    FILE="library/libmbed${NAME}.a"
    PREF="${CONFIG}-$NAME"

    syms '[TRrD]' $FILE > ${PREF}-defined
    syms U $FILE > ${PREF}-unresolved

    diff ${PREF}-defined ${PREF}-unresolved \
        | sed -n 's/^> //p' > ${PREF}-external
    sed 's/mbedtls_\([^_]*\).*/\1/' ${PREF}-external \
        | uniq -c | sort -rn > ${PREF}-modules

    rm ${PREF}-defined ${PREF}-unresolved
}

CONFIG="${1:-unknown}"

list x509
list tls
