#!/bin/sh

# Measure heap usage (and performance) of ECC operations with various values of
# the relevant tunable compile-time parameters.
#
# Usage (preferably on a 32-bit platform):
# cmake -D CMAKE_BUILD_TYPE=Release .
# scripts/ecc-heap.sh | tee ecc-heap.log
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

set -eu

CONFIG_H='include/mbedtls/mbedtls_config.h'

if [ -r $CONFIG_H ]; then :; else
    echo "$CONFIG_H not found" >&2
    exit 1
fi

if grep -i cmake Makefile >/dev/null; then :; else
    echo "Needs Cmake" >&2
    exit 1
fi

if git status | grep -F $CONFIG_H >/dev/null 2>&1; then
    echo "mbedtls_config.h not clean" >&2
    exit 1
fi

CONFIG_BAK=${CONFIG_H}.bak
cp $CONFIG_H $CONFIG_BAK

cat << EOF >$CONFIG_H
#define MBEDTLS_PLATFORM_C
#define MBEDTLS_PLATFORM_MEMORY
#define MBEDTLS_MEMORY_BUFFER_ALLOC_C
#define MBEDTLS_MEMORY_DEBUG

#define MBEDTLS_TIMING_C

#define MBEDTLS_BIGNUM_C
#define MBEDTLS_ECP_C
#define MBEDTLS_ASN1_PARSE_C
#define MBEDTLS_ASN1_WRITE_C
#define MBEDTLS_ECDSA_C
#define MBEDTLS_SHA256_C // ECDSA benchmark needs it
#define MBEDTLS_SHA224_C // SHA256 requires this for now
#define MBEDTLS_ECDH_C

// NIST curves >= 256 bits
#define MBEDTLS_ECP_DP_SECP256R1_ENABLED
#define MBEDTLS_ECP_DP_SECP384R1_ENABLED
#define MBEDTLS_ECP_DP_SECP521R1_ENABLED
// SECP "koblitz-like" curve >= 256 bits
#define MBEDTLS_ECP_DP_SECP256K1_ENABLED
// Brainpool curves (no specialised "mod p" routine)
#define MBEDTLS_ECP_DP_BP256R1_ENABLED
#define MBEDTLS_ECP_DP_BP384R1_ENABLED
#define MBEDTLS_ECP_DP_BP512R1_ENABLED
// Montgomery curves
#define MBEDTLS_ECP_DP_CURVE25519_ENABLED
#define MBEDTLS_ECP_DP_CURVE448_ENABLED

#define MBEDTLS_HAVE_ASM // just make things a bit faster
#define MBEDTLS_ECP_NIST_OPTIM // faster and less allocations

//#define MBEDTLS_ECP_WINDOW_SIZE            4
//#define MBEDTLS_ECP_FIXED_POINT_OPTIM      1
EOF

for F in 0 1; do
    for W in 2 3 4; do
        scripts/config.py set MBEDTLS_ECP_WINDOW_SIZE $W
        scripts/config.py set MBEDTLS_ECP_FIXED_POINT_OPTIM $F
        make benchmark >/dev/null 2>&1
        echo "fixed point optim = $F, max window size = $W"
        echo "--------------------------------------------"
        programs/test/benchmark ecdh ecdsa
    done
done

# cleanup

mv $CONFIG_BAK $CONFIG_H
make clean
