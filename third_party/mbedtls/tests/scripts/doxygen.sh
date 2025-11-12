#!/bin/sh

# Make sure the doxygen documentation builds without warnings
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

# Abort on errors (and uninitiliased variables)
set -eu

if [ -d library -a -d include -a -d tests ]; then :; else
    echo "Must be run from mbed TLS root" >&2
    exit 1
fi

if scripts/apidoc_full.sh > doc.out 2>doc.err; then :; else
    cat doc.err
    echo "FAIL" >&2
    exit 1;
fi

cat doc.out doc.err | \
    grep -v "warning: ignoring unsupported tag" \
    > doc.filtered

if egrep "(warning|error):" doc.filtered; then
    echo "FAIL" >&2
    exit 1;
fi

make apidoc_clean
rm -f doc.out doc.err doc.filtered
