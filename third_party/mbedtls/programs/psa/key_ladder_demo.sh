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

set -e -u

program="${0%/*}"/key_ladder_demo
files_to_clean=

run () {
    echo
    echo "# $1"
    shift
    echo "+ $*"
    "$@"
}

if [ -e master.key ]; then
    echo "# Reusing the existing master.key file."
else
    files_to_clean="$files_to_clean master.key"
    run "Generate a master key." \
        "$program" generate master=master.key
fi

files_to_clean="$files_to_clean input.txt hello_world.wrap"
echo "Here is some input. See it wrapped." >input.txt
run "Derive a key and wrap some data with it." \
    "$program" wrap master=master.key label=hello label=world \
               input=input.txt output=hello_world.wrap

files_to_clean="$files_to_clean hello_world.txt"
run "Derive the same key again and unwrap the data." \
    "$program" unwrap master=master.key label=hello label=world \
               input=hello_world.wrap output=hello_world.txt
run "Compare the unwrapped data with the original input." \
    cmp input.txt hello_world.txt

files_to_clean="$files_to_clean hellow_orld.txt"
! run "Derive a different key and attempt to unwrap the data. This must fail." \
  "$program" unwrap master=master.key input=hello_world.wrap output=hellow_orld.txt label=hellow label=orld

files_to_clean="$files_to_clean hello.key"
run "Save the first step of the key ladder, then load it as a master key and construct the rest of the ladder." \
    "$program" save master=master.key label=hello \
               input=hello_world.wrap output=hello.key
run "Check that we get the same key by unwrapping data made by the other key." \
    "$program" unwrap master=hello.key label=world \
               input=hello_world.wrap output=hello_world.txt

# Cleanup
rm -f $files_to_clean
