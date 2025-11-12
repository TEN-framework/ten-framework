#!/bin/sh

# Generate doxygen documentation with a full mbedtls_config.h (this ensures that every
# available flag is documented, and avoids warnings about documentation
# without a corresponding #define).
#
# /!\ This must not be a Makefile target, as it would create a race condition
# when multiple targets are invoked in the same parallel build.
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

CONFIG_BAK=${CONFIG_H}.bak
cp -p $CONFIG_H $CONFIG_BAK

scripts/config.py realfull
make apidoc

mv $CONFIG_BAK $CONFIG_H
