#! /usr/bin/env sh

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

# Purpose: check Python files for potential programming errors or maintenance
# hurdles. Run pylint to detect some potential mistakes and enforce PEP8
# coding standards. Run mypy to perform static type checking.

# We'll keep going on errors and report the status at the end.
ret=0

if type python3 >/dev/null 2>/dev/null; then
    PYTHON=python3
else
    PYTHON=python
fi

check_version () {
    $PYTHON - "$2" <<EOF
import packaging.version
import sys
import $1 as package
actual = package.__version__
wanted = sys.argv[1]
if packaging.version.parse(actual) < packaging.version.parse(wanted):
    sys.stderr.write("$1: version %s is too old (want %s)\n" % (actual, wanted))
    exit(1)
EOF
}

can_pylint () {
    # Pylint 1.5.2 from Ubuntu 16.04 is too old:
    #     E: 34, 0: Unable to import 'mbedtls_dev' (import-error)
    # Pylint 1.8.3 from Ubuntu 18.04 passed on the first commit containing this line.
    check_version pylint 1.8.3
}

can_mypy () {
    # mypy 0.770 is too old:
    #     tests/scripts/test_psa_constant_names.py:34: error: Cannot find implementation or library stub for module named 'mbedtls_dev'
    # mypy 0.780 from pip passed on the first commit containing this line.
    check_version mypy.version 0.780
}

# With just a --can-xxx option, check whether the tool for xxx is available
# with an acceptable version, and exit without running any checks. The exit
# status is true if the tool is available and acceptable and false otherwise.
if [ "$1" = "--can-pylint" ]; then
    can_pylint
    exit
elif [ "$1" = "--can-mypy" ]; then
    can_mypy
    exit
fi

echo 'Running pylint ...'
$PYTHON -m pylint -j 2 scripts/mbedtls_dev/*.py scripts/*.py tests/scripts/*.py || {
    echo >&2 "pylint reported errors"
    ret=1
}

echo
echo 'Running mypy ...'
$PYTHON -m mypy scripts/*.py tests/scripts/*.py ||
  ret=1

exit $ret
