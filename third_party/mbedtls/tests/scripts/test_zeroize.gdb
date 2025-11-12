# test_zeroize.gdb
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
# Run a test using the debugger to check that the mbedtls_platform_zeroize()
# function in platform_util.h is not being optimized out by the compiler. To do
# so, the script loads the test program at programs/test/zeroize.c and sets a
# breakpoint at the last return statement in main(). When the breakpoint is
# hit, the debugger manually checks the contents to be zeroized and checks that
# it is actually cleared.
#
# The mbedtls_platform_zeroize() test is debugger driven because there does not
# seem to be a mechanism to reliably check whether the zeroize calls are being
# eliminated by compiler optimizations from within the compiled program. The
# problem is that a compiler would typically remove what it considers to be
# "unnecessary" assignments as part of redundant code elimination. To identify
# such code, the compilar will create some form dependency graph between
# reads and writes to variables (among other situations). It will then use this
# data structure to remove redundant code that does not have an impact on the
# program's observable behavior. In the case of mbedtls_platform_zeroize(), an
# intelligent compiler could determine that this function clears a block of
# memory that is not accessed later in the program, so removing the call to
# mbedtls_platform_zeroize() does not have an observable behavior. However,
# inserting a test after a call to mbedtls_platform_zeroize() to check whether
# the block of memory was correctly zeroed would force the compiler to not
# eliminate the mbedtls_platform_zeroize() call. If this does not occur, then
# the compiler potentially has a bug.
#
# Note: This test requires that the test program is compiled with -g3.

set confirm off

file ./programs/test/zeroize

search GDB_BREAK_HERE
break $_

set args ./programs/test/zeroize.c
run

set $i = 0
set $len = sizeof(buf)
set $buf = buf

while $i < $len
    if $buf[$i++] != 0
        echo The buffer at was not zeroized\n
        quit 1
    end
end

echo The buffer was correctly zeroized\n

continue

if $_exitcode != 0
    echo The program did not terminate correctly\n
    quit 1
end

quit 0
