#!/bin/sh

DEFAULT_OUTPUT_FILE=programs/test/cpp_dummy_build.cpp

if [ "$1" = "--help" ]; then
    cat <<EOF
Usage: $0 [OUTPUT]
Generate a C++ dummy build program that includes all the headers.
OUTPUT defaults to "programs/test/cpp_dummy_build.cpp".
Run this program from the root of an Mbed TLS directory tree or from
its "programs" or "programs/test" subdirectory.
EOF
    exit
fi

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

set -e

# Ensure a reproducible order for *.h
export LC_ALL=C

print_cpp () {
    cat <<'EOF'
/* Automatically generated file. Do not edit.
 *
 *  This program is a dummy C++ program to ensure Mbed TLS library header files
 *  can be included and built with a C++ compiler.
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may
 *  not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#include "mbedtls/build_info.h"

EOF

    for header in include/mbedtls/*.h include/psa/*.h; do
        case ${header#include/} in
            mbedtls/mbedtls_config.h) :;; # not meant for direct inclusion
            psa/crypto_config.h) :;; # not meant for direct inclusion
            # Some of the psa/crypto_*.h headers are not meant to be included
            # directly. They do have include guards that make them no-ops if
            # psa/crypto.h has been included before. Since psa/crypto.h comes
            # before psa/crypto_*.h in the wildcard enumeration, we don't need
            # to skip those headers.
            *) echo "#include \"${header#include/}\"";;
        esac
    done

    cat <<'EOF'

int main()
{
    mbedtls_platform_context *ctx = NULL;
    mbedtls_platform_setup(ctx);
    mbedtls_printf("CPP Build test passed\n");
    mbedtls_platform_teardown(ctx);
}
EOF
}

if [ -d include/mbedtls ]; then
    :
elif [ -d ../include/mbedtls ]; then
    cd ..
elif [ -d ../../include/mbedtls ]; then
    cd ../..
else
    echo >&2 "This script must be run from an Mbed TLS source tree."
    exit 3
fi

print_cpp >"${1:-$DEFAULT_OUTPUT_FILE}"
