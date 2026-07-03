#! /usr/bin/env bash

# all.sh
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



################################################################
#### Documentation
################################################################

# Purpose
# -------
#
# To run all tests possible or available on the platform.
#
# Notes for users
# ---------------
#
# Warning: the test is destructive. It includes various build modes and
# configurations, and can and will arbitrarily change the current CMake
# configuration. The following files must be committed into git:
#    * include/mbedtls/mbedtls_config.h
#    * Makefile, library/Makefile, programs/Makefile, tests/Makefile,
#      programs/fuzz/Makefile
# After running this script, the CMake cache will be lost and CMake
# will no longer be initialised.
#
# The script assumes the presence of a number of tools:
#   * Basic Unix tools (Windows users note: a Unix-style find must be before
#     the Windows find in the PATH)
#   * Perl
#   * GNU Make
#   * CMake
#   * GCC and Clang (recent enough for using ASan with gcc and MemSan with clang, or valgrind)
#   * G++
#   * arm-gcc and mingw-gcc
#   * ArmCC 5 and ArmCC 6, unless invoked with --no-armcc
#   * OpenSSL and GnuTLS command line tools, recent enough for the
#     interoperability tests. If they don't support old features which we want
#     to test, then a legacy version of these tools must be present as well
#     (search for LEGACY below).
# See the invocation of check_tools below for details.
#
# This script must be invoked from the toplevel directory of a git
# working copy of Mbed TLS.
#
# The behavior on an error depends on whether --keep-going (alias -k)
# is in effect.
#  * Without --keep-going: the script stops on the first error without
#    cleaning up. This lets you work in the configuration of the failing
#    component.
#  * With --keep-going: the script runs all requested components and
#    reports failures at the end. In particular the script always cleans
#    up on exit.
#
# Note that the output is not saved. You may want to run
#   script -c tests/scripts/all.sh
# or
#   tests/scripts/all.sh >all.log 2>&1
#
# Notes for maintainers
# ---------------------
#
# The bulk of the code is organized into functions that follow one of the
# following naming conventions:
#  * pre_XXX: things to do before running the tests, in order.
#  * component_XXX: independent components. They can be run in any order.
#      * component_check_XXX: quick tests that aren't worth parallelizing.
#      * component_build_XXX: build things but don't run them.
#      * component_test_XXX: build and test.
#  * support_XXX: if support_XXX exists and returns false then
#    component_XXX is not run by default.
#  * post_XXX: things to do after running the tests.
#  * other: miscellaneous support functions.
#
# Each component must start by invoking `msg` with a short informative message.
#
# Warning: due to the way bash detects errors, the failure of a command
# inside 'if' or '!' is not detected. Use the 'not' function instead of '!'.
#
# Each component is executed in a separate shell process. The component
# fails if any command in it returns a non-zero status.
#
# The framework performs some cleanup tasks after each component. This
# means that components can assume that the working directory is in a
# cleaned-up state, and don't need to perform the cleanup themselves.
# * Run `make clean`.
# * Restore `include/mbedtls/mbedtls_config.h` from a backup made before running
#   the component.
# * Check out `Makefile`, `library/Makefile`, `programs/Makefile`,
#   `tests/Makefile` and `programs/fuzz/Makefile` from git.
#   This cleans up after an in-tree use of CMake.
#
# The tests are roughly in order from fastest to slowest. This doesn't
# have to be exact, but in general you should add slower tests towards
# the end and fast checks near the beginning.



################################################################
#### Initialization and command line parsing
################################################################

# Abort on errors (even on the left-hand side of a pipe).
# Treat uninitialised variables as errors.
set -e -o pipefail -u

pre_check_environment () {
    if [ -d library -a -d include -a -d tests ]; then :; else
        echo "Must be run from mbed TLS root" >&2
        exit 1
    fi
}

pre_initialize_variables () {
    CONFIG_H='include/mbedtls/mbedtls_config.h'
    CRYPTO_CONFIG_H='include/psa/crypto_config.h'

    # Files that are clobbered by some jobs will be backed up. Use a different
    # suffix from auxiliary scripts so that all.sh and auxiliary scripts can
    # independently decide when to remove the backup file.
    backup_suffix='.all.bak'
    # Files clobbered by config.py
    files_to_back_up="$CONFIG_H $CRYPTO_CONFIG_H"
    # Files clobbered by in-tree cmake
    files_to_back_up="$files_to_back_up Makefile library/Makefile programs/Makefile tests/Makefile programs/fuzz/Makefile"

    append_outcome=0
    MEMORY=0
    FORCE=0
    QUIET=0
    KEEP_GOING=0

    # Seed value used with the --release-test option.
    #
    # See also RELEASE_SEED in basic-build-test.sh. Debugging is easier if
    # both values are kept in sync. If you change the value here because it
    # breaks some tests, you'll definitely want to change it in
    # basic-build-test.sh as well.
    RELEASE_SEED=1

    : ${MBEDTLS_TEST_OUTCOME_FILE=}
    : ${MBEDTLS_TEST_PLATFORM="$(uname -s | tr -c \\n0-9A-Za-z _)-$(uname -m | tr -c \\n0-9A-Za-z _)"}
    export MBEDTLS_TEST_OUTCOME_FILE
    export MBEDTLS_TEST_PLATFORM

    # Default commands, can be overridden by the environment
    : ${OPENSSL:="openssl"}
    : ${OPENSSL_LEGACY:="$OPENSSL"}
    : ${OPENSSL_NEXT:="$OPENSSL"}
    : ${GNUTLS_CLI:="gnutls-cli"}
    : ${GNUTLS_SERV:="gnutls-serv"}
    : ${GNUTLS_LEGACY_CLI:="$GNUTLS_CLI"}
    : ${GNUTLS_LEGACY_SERV:="$GNUTLS_SERV"}
    : ${OUT_OF_SOURCE_DIR:=./mbedtls_out_of_source_build}
    : ${ARMC5_BIN_DIR:=/usr/bin}
    : ${ARMC6_BIN_DIR:=/usr/bin}
    : ${ARM_NONE_EABI_GCC_PREFIX:=arm-none-eabi-}
    : ${ARM_LINUX_GNUEABI_GCC_PREFIX:=arm-linux-gnueabi-}

    # if MAKEFLAGS is not set add the -j option to speed up invocations of make
    if [ -z "${MAKEFLAGS+set}" ]; then
        export MAKEFLAGS="-j$(all_sh_nproc)"
    fi

    # Include more verbose output for failing tests run by CMake or make
    export CTEST_OUTPUT_ON_FAILURE=1

    # CFLAGS and LDFLAGS for Asan builds that don't use CMake
    ASAN_CFLAGS='-Werror -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=all'

    # Gather the list of available components. These are the functions
    # defined in this script whose name starts with "component_".
    # Parse the script with sed. This way we get the functions in the order
    # they are defined.
    ALL_COMPONENTS=$(sed -n 's/^ *component_\([0-9A-Z_a-z]*\) *().*/\1/p' <"$0")

    # Exclude components that are not supported on this platform.
    SUPPORTED_COMPONENTS=
    for component in $ALL_COMPONENTS; do
        case $(type "support_$component" 2>&1) in
            *' function'*)
                if ! support_$component; then continue; fi;;
        esac
        SUPPORTED_COMPONENTS="$SUPPORTED_COMPONENTS $component"
    done
}

# Test whether the component $1 is included in the command line patterns.
is_component_included()
{
    # Temporarily disable wildcard expansion so that $COMMAND_LINE_COMPONENTS
    # only does word splitting.
    set -f
    for pattern in $COMMAND_LINE_COMPONENTS; do
        set +f
        case ${1#component_} in $pattern) return 0;; esac
    done
    set +f
    return 1
}

usage()
{
    cat <<EOF
Usage: $0 [OPTION]... [COMPONENT]...
Run mbedtls release validation tests.
By default, run all tests. With one or more COMPONENT, run only those.
COMPONENT can be the name of a component or a shell wildcard pattern.

Examples:
  $0 "check_*"
    Run all sanity checks.
  $0 --no-armcc --except test_memsan
    Run everything except builds that require armcc and MemSan.

Special options:
  -h|--help             Print this help and exit.
  --list-all-components List all available test components and exit.
  --list-components     List components supported on this platform and exit.

General options:
  -q|--quiet            Only output component names, and errors if any.
  -f|--force            Force the tests to overwrite any modified files.
  -k|--keep-going       Run all tests and report errors at the end.
  -m|--memory           Additional optional memory tests.
     --append-outcome   Append to the outcome file (if used).
     --arm-none-eabi-gcc-prefix=<string>
                        Prefix for a cross-compiler for arm-none-eabi
                        (default: "${ARM_NONE_EABI_GCC_PREFIX}")
     --arm-linux-gnueabi-gcc-prefix=<string>
                        Prefix for a cross-compiler for arm-linux-gnueabi
                        (default: "${ARM_LINUX_GNUEABI_GCC_PREFIX}")
     --armcc            Run ARM Compiler builds (on by default).
     --restore          First clean up the build tree, restoring backed up
                        files. Do not run any components unless they are
                        explicitly specified.
     --error-test       Error test mode: run a failing function in addition
                        to any specified component. May be repeated.
     --except           Exclude the COMPONENTs listed on the command line,
                        instead of running only those.
     --no-append-outcome    Write a new outcome file and analyze it (default).
     --no-armcc         Skip ARM Compiler builds.
     --no-force         Refuse to overwrite modified files (default).
     --no-keep-going    Stop at the first error (default).
     --no-memory        No additional memory tests (default).
     --no-quiet         Print full output from components.
     --out-of-source-dir=<path>  Directory used for CMake out-of-source build tests.
     --outcome-file=<path>  File where test outcomes are written (not done if
                            empty; default: \$MBEDTLS_TEST_OUTCOME_FILE).
     --random-seed      Use a random seed value for randomized tests (default).
  -r|--release-test     Run this script in release mode. This fixes the seed value to ${RELEASE_SEED}.
  -s|--seed             Integer seed value to use for this test run.

Tool path options:
     --armc5-bin-dir=<ARMC5_bin_dir_path>       ARM Compiler 5 bin directory.
     --armc6-bin-dir=<ARMC6_bin_dir_path>       ARM Compiler 6 bin directory.
     --gnutls-cli=<GnuTLS_cli_path>             GnuTLS client executable to use for most tests.
     --gnutls-serv=<GnuTLS_serv_path>           GnuTLS server executable to use for most tests.
     --gnutls-legacy-cli=<GnuTLS_cli_path>      GnuTLS client executable to use for legacy tests.
     --gnutls-legacy-serv=<GnuTLS_serv_path>    GnuTLS server executable to use for legacy tests.
     --openssl=<OpenSSL_path>                   OpenSSL executable to use for most tests.
     --openssl-legacy=<OpenSSL_path>            OpenSSL executable to use for legacy tests..
     --openssl-next=<OpenSSL_path>              OpenSSL executable to use for recent things like ARIA
EOF
}

# Cleanup before/after running a component.
# Remove built files as well as the cmake cache/config.
# Does not remove generated source files.
cleanup()
{
    command make clean

    # Remove CMake artefacts
    find . -name .git -prune -o \
           -iname CMakeFiles -exec rm -rf {} \+ -o \
           \( -iname cmake_install.cmake -o \
              -iname CTestTestfile.cmake -o \
              -iname CMakeCache.txt -o \
              -path './cmake/*.cmake' \) -exec rm -f {} \+
    # Recover files overwritten by in-tree CMake builds
    rm -f include/Makefile include/mbedtls/Makefile programs/*/Makefile

    # Remove any artifacts from the component_test_cmake_as_subdirectory test.
    rm -rf programs/test/cmake_subproject/build
    rm -f programs/test/cmake_subproject/Makefile
    rm -f programs/test/cmake_subproject/cmake_subproject

    # Remove any artifacts from the component_test_cmake_as_package test.
    rm -rf programs/test/cmake_package/build
    rm -f programs/test/cmake_package/Makefile
    rm -f programs/test/cmake_package/cmake_package

    # Remove any artifacts from the component_test_cmake_as_installed_package test.
    rm -rf programs/test/cmake_package_install/build
    rm -f programs/test/cmake_package_install/Makefile
    rm -f programs/test/cmake_package_install/cmake_package_install

    # Restore files that may have been clobbered by the job
    for x in $files_to_back_up; do
        cp -p "$x$backup_suffix" "$x"
    done
}

# Final cleanup when this script exits (except when exiting on a failure
# in non-keep-going mode).
final_cleanup () {
    cleanup

    for x in $files_to_back_up; do
        rm -f "$x$backup_suffix"
    done
}

# Executed on exit. May be redefined depending on command line options.
final_report () {
    :
}

fatal_signal () {
    final_cleanup
    final_report $1
    trap - $1
    kill -$1 $$
}

trap 'fatal_signal HUP' HUP
trap 'fatal_signal INT' INT
trap 'fatal_signal TERM' TERM

# Number of processors on this machine. Used as the default setting
# for parallel make.
all_sh_nproc ()
{
    {
        nproc || # Linux
        sysctl -n hw.ncpuonline || # NetBSD, OpenBSD
        sysctl -n hw.ncpu || # FreeBSD
        echo 1
    } 2>/dev/null
}

msg()
{
    if [ -n "${current_component:-}" ]; then
        current_section="${current_component#component_}: $1"
    else
        current_section="$1"
    fi

    if [ $QUIET -eq 1 ]; then
        return
    fi

    echo ""
    echo "******************************************************************"
    echo "* $current_section "
    printf "* "; date
    echo "******************************************************************"
}

armc6_build_test()
{
    FLAGS="$1"

    msg "build: ARM Compiler 6 ($FLAGS)"
    ARM_TOOL_VARIANT="ult" CC="$ARMC6_CC" AR="$ARMC6_AR" CFLAGS="$FLAGS" \
                    WARNING_CFLAGS='-xc -std=c99' make lib

    msg "size: ARM Compiler 6 ($FLAGS)"
    "$ARMC6_FROMELF" -z library/*.o

    make clean
}

err_msg()
{
    echo "$1" >&2
}

check_tools()
{
    for TOOL in "$@"; do
        if ! `type "$TOOL" >/dev/null 2>&1`; then
            err_msg "$TOOL not found!"
            exit 1
        fi
    done
}

pre_parse_command_line () {
    COMMAND_LINE_COMPONENTS=
    all_except=0
    error_test=0
    restore_first=0
    no_armcc=

    # Note that legacy options are ignored instead of being omitted from this
    # list of options, so invocations that worked with previous version of
    # all.sh will still run and work properly.
    while [ $# -gt 0 ]; do
        case "$1" in
            --append-outcome) append_outcome=1;;
            --arm-none-eabi-gcc-prefix) shift; ARM_NONE_EABI_GCC_PREFIX="$1";;
            --arm-linux-gnueabi-gcc-prefix) shift; ARM_LINUX_GNUEABI_GCC_PREFIX="$1";;
            --armcc) no_armcc=;;
            --armc5-bin-dir) shift; ARMC5_BIN_DIR="$1";;
            --armc6-bin-dir) shift; ARMC6_BIN_DIR="$1";;
            --error-test) error_test=$((error_test + 1));;
            --except) all_except=1;;
            --force|-f) FORCE=1;;
            --gnutls-cli) shift; GNUTLS_CLI="$1";;
            --gnutls-legacy-cli) shift; GNUTLS_LEGACY_CLI="$1";;
            --gnutls-legacy-serv) shift; GNUTLS_LEGACY_SERV="$1";;
            --gnutls-serv) shift; GNUTLS_SERV="$1";;
            --help|-h) usage; exit;;
            --keep-going|-k) KEEP_GOING=1;;
            --list-all-components) printf '%s\n' $ALL_COMPONENTS; exit;;
            --list-components) printf '%s\n' $SUPPORTED_COMPONENTS; exit;;
            --memory|-m) MEMORY=1;;
            --no-append-outcome) append_outcome=0;;
            --no-armcc) no_armcc=1;;
            --no-force) FORCE=0;;
            --no-keep-going) KEEP_GOING=0;;
            --no-memory) MEMORY=0;;
            --no-quiet) QUIET=0;;
            --openssl) shift; OPENSSL="$1";;
            --openssl-legacy) shift; OPENSSL_LEGACY="$1";;
            --openssl-next) shift; OPENSSL_NEXT="$1";;
            --outcome-file) shift; MBEDTLS_TEST_OUTCOME_FILE="$1";;
            --out-of-source-dir) shift; OUT_OF_SOURCE_DIR="$1";;
            --quiet|-q) QUIET=1;;
            --random-seed) unset SEED;;
            --release-test|-r) SEED=$RELEASE_SEED;;
            --restore) restore_first=1;;
            --seed|-s) shift; SEED="$1";;
            -*)
                echo >&2 "Unknown option: $1"
                echo >&2 "Run $0 --help for usage."
                exit 120
                ;;
            *) COMMAND_LINE_COMPONENTS="$COMMAND_LINE_COMPONENTS $1";;
        esac
        shift
    done

    # With no list of components, run everything.
    if [ -z "$COMMAND_LINE_COMPONENTS" ] && [ $restore_first -eq 0 ]; then
        all_except=1
    fi

    # --no-armcc is a legacy option. The modern way is --except '*_armcc*'.
    # Ignore it if components are listed explicitly on the command line.
    if [ -n "$no_armcc" ] && [ $all_except -eq 1 ]; then
        COMMAND_LINE_COMPONENTS="$COMMAND_LINE_COMPONENTS *_armcc*"
    fi

    # Error out if an explicitly requested component doesn't exist.
    if [ $all_except -eq 0 ]; then
        unsupported=0
        # Temporarily disable wildcard expansion so that $COMMAND_LINE_COMPONENTS
        # only does word splitting.
        set -f
        for component in $COMMAND_LINE_COMPONENTS; do
            set +f
            # If the requested name includes a wildcard character, don't
            # check it. Accept wildcard patterns that don't match anything.
            case $component in
                *[*?\[]*) continue;;
            esac
            case " $SUPPORTED_COMPONENTS " in
                *" $component "*) :;;
                *)
                    echo >&2 "Component $component was explicitly requested, but is not known or not supported."
                    unsupported=$((unsupported + 1));;
            esac
        done
        set +f
        if [ $unsupported -ne 0 ]; then
            exit 2
        fi
    fi

    # Build the list of components to run.
    RUN_COMPONENTS=
    for component in $SUPPORTED_COMPONENTS; do
        if is_component_included "$component"; [ $? -eq $all_except ]; then
            RUN_COMPONENTS="$RUN_COMPONENTS $component"
        fi
    done

    unset all_except
    unset no_armcc
}

pre_check_git () {
    if [ $FORCE -eq 1 ]; then
        rm -rf "$OUT_OF_SOURCE_DIR"
        git checkout-index -f -q $CONFIG_H
        cleanup
    else

        if [ -d "$OUT_OF_SOURCE_DIR" ]; then
            echo "Warning - there is an existing directory at '$OUT_OF_SOURCE_DIR'" >&2
            echo "You can either delete this directory manually, or force the test by rerunning"
            echo "the script as: $0 --force --out-of-source-dir $OUT_OF_SOURCE_DIR"
            exit 1
        fi

        if ! git diff --quiet include/mbedtls/mbedtls_config.h; then
            err_msg "Warning - the configuration file 'include/mbedtls/mbedtls_config.h' has been edited. "
            echo "You can either delete or preserve your work, or force the test by rerunning the"
            echo "script as: $0 --force"
            exit 1
        fi
    fi
}

pre_restore_files () {
    # If the makefiles have been generated by a framework such as cmake,
    # restore them from git. If the makefiles look like modifications from
    # the ones checked into git, take care not to modify them. Whatever
    # this function leaves behind is what the script will restore before
    # each component.
    case "$(head -n1 Makefile)" in
        *[Gg]enerated*)
            git update-index --no-skip-worktree Makefile library/Makefile programs/Makefile tests/Makefile programs/fuzz/Makefile
            git checkout -- Makefile library/Makefile programs/Makefile tests/Makefile programs/fuzz/Makefile
            ;;
    esac
}

pre_back_up () {
    for x in $files_to_back_up; do
        cp -p "$x" "$x$backup_suffix"
    done
}

pre_setup_keep_going () {
    failure_count=0 # Number of failed components
    last_failure_status=0 # Last failure status in this component

    # See err_trap
    previous_failure_status=0
    previous_failed_command=
    previous_failure_funcall_depth=0
    unset report_failed_command

    start_red=
    end_color=
    if [ -t 1 ]; then
        case "${TERM:-}" in
            *color*|cygwin|linux|rxvt*|screen|[Eex]term*)
                start_red=$(printf '\033[31m')
                end_color=$(printf '\033[0m')
                ;;
        esac
    fi

    # Keep a summary of failures in a file. We'll print it out at the end.
    failure_summary_file=$PWD/all-sh-failures-$$.log
    : >"$failure_summary_file"

    # Whether it makes sense to keep a component going after the specified
    # command fails (test command) or not (configure or build).
    # This function normally receives the failing simple command
    # ($BASH_COMMAND) as an argument, but if $report_failed_command is set,
    # this is passed instead.
    # This doesn't have to be 100% accurate: all failures are recorded anyway.
    # False positives result in running things that can't be expected to
    # work. False negatives result in things not running after something else
    # failed even though they might have given useful feedback.
    can_keep_going_after_failure () {
        case "$1" in
            "msg "*) false;;
            "cd "*) false;;
            *make*[\ /]tests*) false;; # make tests, make CFLAGS=-I../tests, ...
            *test*) true;; # make test, tests/stuff, env V=v tests/stuff, ...
            *make*check*) true;;
            "grep "*) true;;
            "[ "*) true;;
            "! "*) true;;
            *) false;;
        esac
    }

    # This function runs if there is any error in a component.
    # It must either exit with a nonzero status, or set
    # last_failure_status to a nonzero value.
    err_trap () {
        # Save $? (status of the failing command). This must be the very
        # first thing, before $? is overridden.
        last_failure_status=$?
        failed_command=${report_failed_command-$BASH_COMMAND}

        if [[ $last_failure_status -eq $previous_failure_status &&
              "$failed_command" == "$previous_failed_command" &&
              ${#FUNCNAME[@]} == $((previous_failure_funcall_depth - 1)) ]]
        then
            # The same command failed twice in a row, but this time one level
            # less deep in the function call stack. This happens when the last
            # command of a function returns a nonzero status, and the function
            # returns that same status. Ignore the second failure.
            previous_failure_funcall_depth=${#FUNCNAME[@]}
            return
        fi
        previous_failure_status=$last_failure_status
        previous_failed_command=$failed_command
        previous_failure_funcall_depth=${#FUNCNAME[@]}

        text="$current_section: $failed_command -> $last_failure_status"
        echo "${start_red}^^^^$text^^^^${end_color}" >&2
        echo "$text" >>"$failure_summary_file"

        # If the command is fatal (configure or build command), stop this
        # component. Otherwise (test command) keep the component running
        # (run more tests from the same build).
        if ! can_keep_going_after_failure "$failed_command"; then
            exit $last_failure_status
        fi
    }

    final_report () {
        if [ $failure_count -gt 0 ]; then
            echo
            echo "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
            echo "${start_red}FAILED: $failure_count components${end_color}"
            cat "$failure_summary_file"
            echo "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
        elif [ -z "${1-}" ]; then
            echo "SUCCESS :)"
        fi
        if [ -n "${1-}" ]; then
            echo "Killed by SIG$1."
        fi
        rm -f "$failure_summary_file"
        if [ $failure_count -gt 0 ]; then
            exit 1
        fi
    }
}

# record_status() and if_build_succeeded() are kept temporarily for backward
# compatibility. Don't use them in new components.
record_status () {
    "$@"
}
if_build_succeeded () {
    "$@"
}

# '! true' does not trigger the ERR trap. Arrange to trigger it, with
# a reasonably informative error message (not just "$@").
not () {
    if "$@"; then
        report_failed_command="! $*"
        false
        unset report_failed_command
    fi
}

pre_prepare_outcome_file () {
    case "$MBEDTLS_TEST_OUTCOME_FILE" in
      [!/]*) MBEDTLS_TEST_OUTCOME_FILE="$PWD/$MBEDTLS_TEST_OUTCOME_FILE";;
    esac
    if [ -n "$MBEDTLS_TEST_OUTCOME_FILE" ] && [ "$append_outcome" -eq 0 ]; then
        rm -f "$MBEDTLS_TEST_OUTCOME_FILE"
    fi
}

pre_print_configuration () {
    if [ $QUIET -eq 1 ]; then
        return
    fi

    msg "info: $0 configuration"
    echo "MEMORY: $MEMORY"
    echo "FORCE: $FORCE"
    echo "MBEDTLS_TEST_OUTCOME_FILE: ${MBEDTLS_TEST_OUTCOME_FILE:-(none)}"
    echo "SEED: ${SEED-"UNSET"}"
    echo
    echo "OPENSSL: $OPENSSL"
    echo "OPENSSL_LEGACY: $OPENSSL_LEGACY"
    echo "OPENSSL_NEXT: $OPENSSL_NEXT"
    echo "GNUTLS_CLI: $GNUTLS_CLI"
    echo "GNUTLS_SERV: $GNUTLS_SERV"
    echo "GNUTLS_LEGACY_CLI: $GNUTLS_LEGACY_CLI"
    echo "GNUTLS_LEGACY_SERV: $GNUTLS_LEGACY_SERV"
    echo "ARMC5_BIN_DIR: $ARMC5_BIN_DIR"
    echo "ARMC6_BIN_DIR: $ARMC6_BIN_DIR"
}

# Make sure the tools we need are available.
pre_check_tools () {
    # Build the list of variables to pass to output_env.sh.
    set env

    case " $RUN_COMPONENTS " in
        # Require OpenSSL and GnuTLS if running any tests (as opposed to
        # only doing builds). Not all tests run OpenSSL and GnuTLS, but this
        # is a good enough approximation in practice.
        *" test_"*)
            # To avoid setting OpenSSL and GnuTLS for each call to compat.sh
            # and ssl-opt.sh, we just export the variables they require.
            export OPENSSL_CMD="$OPENSSL"
            export GNUTLS_CLI="$GNUTLS_CLI"
            export GNUTLS_SERV="$GNUTLS_SERV"
            # Avoid passing --seed flag in every call to ssl-opt.sh
            if [ -n "${SEED-}" ]; then
                export SEED
            fi
            set "$@" OPENSSL="$OPENSSL" OPENSSL_LEGACY="$OPENSSL_LEGACY"
            set "$@" GNUTLS_CLI="$GNUTLS_CLI" GNUTLS_SERV="$GNUTLS_SERV"
            set "$@" GNUTLS_LEGACY_CLI="$GNUTLS_LEGACY_CLI"
            set "$@" GNUTLS_LEGACY_SERV="$GNUTLS_LEGACY_SERV"
            check_tools "$OPENSSL" "$OPENSSL_LEGACY" "$OPENSSL_NEXT" \
                        "$GNUTLS_CLI" "$GNUTLS_SERV" \
                        "$GNUTLS_LEGACY_CLI" "$GNUTLS_LEGACY_SERV"
            ;;
    esac

    case " $RUN_COMPONENTS " in
        *_doxygen[_\ ]*) check_tools "doxygen" "dot";;
    esac

    case " $RUN_COMPONENTS " in
        *_arm_none_eabi_gcc[_\ ]*) check_tools "${ARM_NONE_EABI_GCC_PREFIX}gcc";;
    esac

    case " $RUN_COMPONENTS " in
        *_mingw[_\ ]*) check_tools "i686-w64-mingw32-gcc";;
    esac

    case " $RUN_COMPONENTS " in
        *" test_zeroize "*) check_tools "gdb";;
    esac

    case " $RUN_COMPONENTS " in
        *_armcc*)
            ARMC5_CC="$ARMC5_BIN_DIR/armcc"
            ARMC5_AR="$ARMC5_BIN_DIR/armar"
            ARMC5_FROMELF="$ARMC5_BIN_DIR/fromelf"
            ARMC6_CC="$ARMC6_BIN_DIR/armclang"
            ARMC6_AR="$ARMC6_BIN_DIR/armar"
            ARMC6_FROMELF="$ARMC6_BIN_DIR/fromelf"
            check_tools "$ARMC5_CC" "$ARMC5_AR" "$ARMC5_FROMELF" \
                        "$ARMC6_CC" "$ARMC6_AR" "$ARMC6_FROMELF";;
    esac

    # past this point, no call to check_tool, only printing output
    if [ $QUIET -eq 1 ]; then
        return
    fi

    msg "info: output_env.sh"
    case $RUN_COMPONENTS in
        *_armcc*)
            set "$@" ARMC5_CC="$ARMC5_CC" ARMC6_CC="$ARMC6_CC" RUN_ARMCC=1;;
        *) set "$@" RUN_ARMCC=0;;
    esac
    "$@" scripts/output_env.sh
}

pre_generate_files() {
    # since make doesn't have proper dependencies, remove any possibly outdate
    # file that might be around before generating fresh ones
    make neat
    if [ $QUIET -eq 1 ]; then
        make generated_files >/dev/null
    else
        make generated_files
    fi
}



################################################################
#### Basic checks
################################################################

#
# Test Suites to be executed
#
# The test ordering tries to optimize for the following criteria:
# 1. Catch possible problems early, by running first tests that run quickly
#    and/or are more likely to fail than others (eg I use Clang most of the
#    time, so start with a GCC build).
# 2. Minimize total running time, by avoiding useless rebuilds
#
# Indicative running times are given for reference.

component_check_recursion () {
    msg "Check: recursion.pl" # < 1s
    tests/scripts/recursion.pl library/*.c
}

component_check_generated_files () {
    msg "Check: check-generated-files, files generated with make" # 2s
    make generated_files
    tests/scripts/check-generated-files.sh

    msg "Check: check-generated-files -u, files present" # 2s
    tests/scripts/check-generated-files.sh -u
    # Check that the generated files are considered up to date.
    tests/scripts/check-generated-files.sh

    msg "Check: check-generated-files -u, files absent" # 2s
    command make neat
    tests/scripts/check-generated-files.sh -u
    # Check that the generated files are considered up to date.
    tests/scripts/check-generated-files.sh

    # This component ends with the generated files present in the source tree.
    # This is necessary for subsequent components!
}

component_check_doxy_blocks () {
    msg "Check: doxygen markup outside doxygen blocks" # < 1s
    tests/scripts/check-doxy-blocks.pl
}

component_check_files () {
    msg "Check: file sanity checks (permissions, encodings)" # < 1s
    tests/scripts/check_files.py
}

component_check_changelog () {
    msg "Check: changelog entries" # < 1s
    rm -f ChangeLog.new
    scripts/assemble_changelog.py -o ChangeLog.new
    if [ -e ChangeLog.new ]; then
        # Show the diff for information. It isn't an error if the diff is
        # non-empty.
        diff -u ChangeLog ChangeLog.new || true
        rm ChangeLog.new
    fi
}

component_check_names () {
    msg "Check: declared and exported names (builds the library)" # < 3s
    tests/scripts/check_names.py -v
}

component_check_test_cases () {
    msg "Check: test case descriptions" # < 1s
    if [ $QUIET -eq 1 ]; then
        opt='--quiet'
    else
        opt=''
    fi
    tests/scripts/check_test_cases.py $opt
    unset opt

    # Check that no tests are explicitely disabled when USE_PSA_CRYPTO is set
    # as a matter of policy to ensure there is no missed testing
    msg "Check: explicitely disabled test with USE_PSA_CRYPTO"  # < 1s
    not grep -n 'depends_on:.*!MBEDTLS_USE_PSA_CRYPTO' tests/suites/*.function tests/suites/*.data
    not grep -n '^ *requires_config_disabled.*MBEDTLS_USE_PSA_CRYPTO' tests/ssl-opt.sh tests/opt-testcases/*.sh
}

component_check_doxygen_warnings () {
    msg "Check: doxygen warnings (builds the documentation)" # ~ 3s
    tests/scripts/doxygen.sh
}



################################################################
#### Build and test many configurations and targets
################################################################

component_test_default_out_of_box () {
    msg "build: make, default config (out-of-box)" # ~1min
    make
    # Disable fancy stuff
    unset MBEDTLS_TEST_OUTCOME_FILE

    msg "test: main suites make, default config (out-of-box)" # ~10s
    make test

    msg "selftest: make, default config (out-of-box)" # ~10s
    programs/test/selftest
}

component_test_default_cmake_gcc_asan () {
    msg "build: cmake, gcc, ASan" # ~ 1 min 50s
    CC=gcc cmake -D CMAKE_BUILD_TYPE:String=Asan .
    make

    msg "test: main suites (inc. selftests) (ASan build)" # ~ 50s
    make test

    msg "test: selftest (ASan build)" # ~ 10s
    programs/test/selftest

    msg "test: ssl-opt.sh (ASan build)" # ~ 1 min
    tests/ssl-opt.sh

    msg "test: compat.sh (ASan build)" # ~ 6 min
    tests/compat.sh

    msg "test: context-info.sh (ASan build)" # ~ 15 sec
    tests/context-info.sh
}

component_test_full_cmake_gcc_asan () {
    msg "build: full config, cmake, gcc, ASan"
    scripts/config.py full
    CC=gcc cmake -D CMAKE_BUILD_TYPE:String=Asan .
    make

    msg "test: main suites (inc. selftests) (full config, ASan build)"
    make test

    msg "test: selftest (ASan build)" # ~ 10s
    programs/test/selftest

    msg "test: ssl-opt.sh (full config, ASan build)"
    tests/ssl-opt.sh

    msg "test: compat.sh (full config, ASan build)"
    tests/compat.sh

    msg "test: context-info.sh (full config, ASan build)" # ~ 15 sec
    tests/context-info.sh
}

component_test_psa_crypto_key_id_encodes_owner () {
    msg "build: full config + PSA_CRYPTO_KEY_ID_ENCODES_OWNER, cmake, gcc, ASan"
    scripts/config.py full
    scripts/config.py set MBEDTLS_PSA_CRYPTO_KEY_ID_ENCODES_OWNER
    CC=gcc cmake -D CMAKE_BUILD_TYPE:String=Asan .
    make

    msg "test: full config - USE_PSA_CRYPTO + PSA_CRYPTO_KEY_ID_ENCODES_OWNER, cmake, gcc, ASan"
    make test
}

# check_renamed_symbols HEADER LIB
# Check that if HEADER contains '#define MACRO ...' then MACRO is not a symbol
# name is LIB.
check_renamed_symbols () {
    ! nm "$2" | sed 's/.* //' |
      grep -x -F "$(sed -n 's/^ *# *define  *\([A-Z_a-z][0-9A-Z_a-z]*\)..*/\1/p' "$1")"
}

component_build_psa_crypto_spm () {
    msg "build: full config + PSA_CRYPTO_KEY_ID_ENCODES_OWNER + PSA_CRYPTO_SPM, make, gcc"
    scripts/config.py full
    scripts/config.py unset MBEDTLS_PSA_CRYPTO_BUILTIN_KEYS
    scripts/config.py set MBEDTLS_PSA_CRYPTO_KEY_ID_ENCODES_OWNER
    scripts/config.py set MBEDTLS_PSA_CRYPTO_SPM
    # We can only compile, not link, since our test and sample programs
    # aren't equipped for the modified names used when MBEDTLS_PSA_CRYPTO_SPM
    # is active.
    make CC=gcc CFLAGS='-Werror -Wall -Wextra -I../tests/include/spe' lib

    # Check that if a symbol is renamed by crypto_spe.h, the non-renamed
    # version is not present.
    echo "Checking for renamed symbols in the library"
    check_renamed_symbols tests/include/spe/crypto_spe.h library/libmbedcrypto.a
}

component_test_psa_crypto_client () {
    msg "build: default config - PSA_CRYPTO_C + PSA_CRYPTO_CLIENT, make"
    scripts/config.py unset MBEDTLS_PSA_CRYPTO_C
    scripts/config.py unset MBEDTLS_PSA_CRYPTO_STORAGE_C
    scripts/config.py set MBEDTLS_PSA_CRYPTO_CLIENT
    make

    msg "test: default config - PSA_CRYPTO_C + PSA_CRYPTO_CLIENT, make"
    make test
}

component_test_psa_crypto_rsa_no_genprime() {
    msg "build: default config minus MBEDTLS_GENPRIME"
    scripts/config.py unset MBEDTLS_GENPRIME
    make

    msg "test: default config minus MBEDTLS_GENPRIME"
    make test
}

component_test_ref_configs () {
    msg "test/build: ref-configs (ASan build)" # ~ 6 min 20s
    # test-ref-configs works by overwriting mbedtls_config.h; this makes cmake
    # want to re-generate generated files that depend on it, quite correctly.
    # However this doesn't work as the generation script expects a specific
    # format for mbedtls_config.h, which the other files don't follow. Also,
    # cmake can't know this, but re-generation is actually not necessary as
    # the generated files only depend on the list of available options, not
    # whether they're on or off. So, disable cmake's (over-sensitive here)
    # dependency resolution for generated files and just rely on them being
    # present (thanks to pre_generate_files) by turning GEN_FILES off.
    CC=gcc cmake -D GEN_FILES=Off -D CMAKE_BUILD_TYPE:String=Asan .
    tests/scripts/test-ref-configs.pl
}

component_test_no_renegotiation () {
    msg "build: Default + !MBEDTLS_SSL_RENEGOTIATION (ASan build)" # ~ 6 min
    scripts/config.py unset MBEDTLS_SSL_RENEGOTIATION
    CC=gcc cmake -D CMAKE_BUILD_TYPE:String=Asan .
    make

    msg "test: !MBEDTLS_SSL_RENEGOTIATION - main suites (inc. selftests) (ASan build)" # ~ 50s
    make test

    msg "test: !MBEDTLS_SSL_RENEGOTIATION - ssl-opt.sh (ASan build)" # ~ 6 min
    tests/ssl-opt.sh
}

component_test_no_pem_no_fs () {
    msg "build: Default + !MBEDTLS_PEM_PARSE_C + !MBEDTLS_FS_IO (ASan build)"
    scripts/config.py unset MBEDTLS_PEM_PARSE_C
    scripts/config.py unset MBEDTLS_FS_IO
    scripts/config.py unset MBEDTLS_PSA_ITS_FILE_C # requires a filesystem
    scripts/config.py unset MBEDTLS_PSA_CRYPTO_STORAGE_C # requires PSA ITS
    CC=gcc cmake -D CMAKE_BUILD_TYPE:String=Asan .
    make

    msg "test: !MBEDTLS_PEM_PARSE_C !MBEDTLS_FS_IO - main suites (inc. selftests) (ASan build)" # ~ 50s
    make test

    msg "test: !MBEDTLS_PEM_PARSE_C !MBEDTLS_FS_IO - ssl-opt.sh (ASan build)" # ~ 6 min
    tests/ssl-opt.sh
}

component_test_rsa_no_crt () {
    msg "build: Default + RSA_NO_CRT (ASan build)" # ~ 6 min
    scripts/config.py set MBEDTLS_RSA_NO_CRT
    CC=gcc cmake -D CMAKE_BUILD_TYPE:String=Asan .
    make

    msg "test: RSA_NO_CRT - main suites (inc. selftests) (ASan build)" # ~ 50s
    make test

    msg "test: RSA_NO_CRT - RSA-related part of ssl-opt.sh (ASan build)" # ~ 5s
    tests/ssl-opt.sh -f RSA

    msg "test: RSA_NO_CRT - RSA-related part of compat.sh (ASan build)" # ~ 3 min
    tests/compat.sh -t RSA

    msg "test: RSA_NO_CRT - RSA-related part of context-info.sh (ASan build)" # ~ 15 sec
    tests/context-info.sh
}

component_test_no_ctr_drbg_classic () {
    msg "build: Full minus CTR_DRBG, classic crypto in TLS"
    scripts/config.py full
    scripts/config.py unset MBEDTLS_CTR_DRBG_C
    scripts/config.py unset MBEDTLS_USE_PSA_CRYPTO
    scripts/config.py unset MBEDTLS_SSL_PROTO_TLS1_3

    CC=gcc cmake -D CMAKE_BUILD_TYPE:String=Asan .
    make

    msg "test: Full minus CTR_DRBG, classic crypto - main suites"
    make test

    # In this configuration, the TLS test programs use HMAC_DRBG.
    # The SSL tests are slow, so run a small subset, just enough to get
    # confidence that the SSL code copes with HMAC_DRBG.
    msg "test: Full minus CTR_DRBG, classic crypto - ssl-opt.sh (subset)"
    tests/ssl-opt.sh -f 'Default\|SSL async private.*delay=\|tickets enabled on server'

    msg "test: Full minus CTR_DRBG, classic crypto - compat.sh (subset)"
    tests/compat.sh -m tls12 -t 'ECDSA PSK' -V NO -p OpenSSL
}

component_test_no_ctr_drbg_use_psa () {
    msg "build: Full minus CTR_DRBG, PSA crypto in TLS"
    scripts/config.py full
    scripts/config.py unset MBEDTLS_CTR_DRBG_C
    scripts/config.py set MBEDTLS_USE_PSA_CRYPTO

    CC=gcc cmake -D CMAKE_BUILD_TYPE:String=Asan .
    make

    msg "test: Full minus CTR_DRBG, USE_PSA_CRYPTO - main suites"
    make test

    # In this configuration, the TLS test programs use HMAC_DRBG.
    # The SSL tests are slow, so run a small subset, just enough to get
    # confidence that the SSL code copes with HMAC_DRBG.
    msg "test: Full minus CTR_DRBG, USE_PSA_CRYPTO - ssl-opt.sh (subset)"
    tests/ssl-opt.sh -f 'Default\|SSL async private.*delay=\|tickets enabled on server'

    msg "test: Full minus CTR_DRBG, USE_PSA_CRYPTO - compat.sh (subset)"
    tests/compat.sh -m tls12 -t 'ECDSA PSK' -V NO -p OpenSSL
}

component_test_no_hmac_drbg_classic () {
    msg "build: Full minus HMAC_DRBG, classic crypto in TLS"
    scripts/config.py full
    scripts/config.py unset MBEDTLS_HMAC_DRBG_C
    scripts/config.py unset MBEDTLS_ECDSA_DETERMINISTIC # requires HMAC_DRBG
    scripts/config.py unset MBEDTLS_USE_PSA_CRYPTO
    scripts/config.py unset MBEDTLS_SSL_PROTO_TLS1_3

    CC=gcc cmake -D CMAKE_BUILD_TYPE:String=Asan .
    make

    msg "test: Full minus HMAC_DRBG, classic crypto - main suites"
    make test

    # Normally our ECDSA implementation uses deterministic ECDSA. But since
    # HMAC_DRBG is disabled in this configuration, randomized ECDSA is used
    # instead.
    # Test SSL with non-deterministic ECDSA. Only test features that
    # might be affected by how ECDSA signature is performed.
    msg "test: Full minus HMAC_DRBG, classic crypto - ssl-opt.sh (subset)"
    tests/ssl-opt.sh -f 'Default\|SSL async private: sign'

    # To save time, only test one protocol version, since this part of
    # the protocol is identical in (D)TLS up to 1.2.
    msg "test: Full minus HMAC_DRBG, classic crypto - compat.sh (ECDSA)"
    tests/compat.sh -m tls12 -t 'ECDSA'
}

component_test_no_hmac_drbg_use_psa () {
    msg "build: Full minus HMAC_DRBG, PSA crypto in TLS"
    scripts/config.py full
    scripts/config.py unset MBEDTLS_HMAC_DRBG_C
    scripts/config.py unset MBEDTLS_ECDSA_DETERMINISTIC # requires HMAC_DRBG
    scripts/config.py set MBEDTLS_USE_PSA_CRYPTO

    CC=gcc cmake -D CMAKE_BUILD_TYPE:String=Asan .
    make

    msg "test: Full minus HMAC_DRBG, USE_PSA_CRYPTO - main suites"
    make test

    # Normally our ECDSA implementation uses deterministic ECDSA. But since
    # HMAC_DRBG is disabled in this configuration, randomized ECDSA is used
    # instead.
    # Test SSL with non-deterministic ECDSA. Only test features that
    # might be affected by how ECDSA signature is performed.
    msg "test: Full minus HMAC_DRBG, USE_PSA_CRYPTO - ssl-opt.sh (subset)"
    tests/ssl-opt.sh -f 'Default\|SSL async private: sign'

    # To save time, only test one protocol version, since this part of
    # the protocol is identical in (D)TLS up to 1.2.
    msg "test: Full minus HMAC_DRBG, USE_PSA_CRYPTO - compat.sh (ECDSA)"
    tests/compat.sh -m tls12 -t 'ECDSA'
}

component_test_psa_external_rng_no_drbg_classic () {
    msg "build: PSA_CRYPTO_EXTERNAL_RNG minus *_DRBG, classic crypto in TLS"
    scripts/config.py full
    scripts/config.py unset MBEDTLS_USE_PSA_CRYPTO
    scripts/config.py unset MBEDTLS_SSL_PROTO_TLS1_3
    scripts/config.py set MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG
    scripts/config.py unset MBEDTLS_ENTROPY_C
    scripts/config.py unset MBEDTLS_ENTROPY_NV_SEED
    scripts/config.py unset MBEDTLS_PLATFORM_NV_SEED_ALT
    scripts/config.py unset MBEDTLS_CTR_DRBG_C
    scripts/config.py unset MBEDTLS_HMAC_DRBG_C
    scripts/config.py unset MBEDTLS_ECDSA_DETERMINISTIC # requires HMAC_DRBG
    # When MBEDTLS_USE_PSA_CRYPTO is disabled and there is no DRBG,
    # the SSL test programs don't have an RNG and can't work. Explicitly
    # make them use the PSA RNG with -DMBEDTLS_TEST_USE_PSA_CRYPTO_RNG.
    make CFLAGS="$ASAN_CFLAGS -O2 -DMBEDTLS_TEST_USE_PSA_CRYPTO_RNG" LDFLAGS="$ASAN_CFLAGS"

    msg "test: PSA_CRYPTO_EXTERNAL_RNG minus *_DRBG, classic crypto - main suites"
    make test

    msg "test: PSA_CRYPTO_EXTERNAL_RNG minus *_DRBG, classic crypto - ssl-opt.sh (subset)"
    tests/ssl-opt.sh -f 'Default'
}

component_test_psa_external_rng_no_drbg_use_psa () {
    msg "build: PSA_CRYPTO_EXTERNAL_RNG minus *_DRBG, PSA crypto in TLS"
    scripts/config.py full
    scripts/config.py set MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG
    scripts/config.py unset MBEDTLS_ENTROPY_C
    scripts/config.py unset MBEDTLS_ENTROPY_NV_SEED
    scripts/config.py unset MBEDTLS_PLATFORM_NV_SEED_ALT
    scripts/config.py unset MBEDTLS_CTR_DRBG_C
    scripts/config.py unset MBEDTLS_HMAC_DRBG_C
    scripts/config.py unset MBEDTLS_ECDSA_DETERMINISTIC # requires HMAC_DRBG
    make CFLAGS="$ASAN_CFLAGS -O2" LDFLAGS="$ASAN_CFLAGS"

    msg "test: PSA_CRYPTO_EXTERNAL_RNG minus *_DRBG, PSA crypto - main suites"
    make test

    msg "test: PSA_CRYPTO_EXTERNAL_RNG minus *_DRBG, PSA crypto - ssl-opt.sh (subset)"
    tests/ssl-opt.sh -f 'Default\|opaque'
}

component_test_crypto_full_no_md () {
    msg "build: crypto_full minus MD"
    scripts/config.py crypto_full
    scripts/config.py unset MBEDTLS_MD_C
    # Direct dependencies
    scripts/config.py unset MBEDTLS_ECJPAKE_C
    scripts/config.py unset MBEDTLS_HKDF_C
    scripts/config.py unset MBEDTLS_HMAC_DRBG_C
    scripts/config.py unset MBEDTLS_PK_C
    scripts/config.py unset MBEDTLS_PKCS1_V15
    scripts/config.py unset MBEDTLS_PKCS1_V21
    scripts/config.py unset MBEDTLS_PKCS5_C
    scripts/config.py unset MBEDTLS_PKCS12_C
    # Indirect dependencies
    scripts/config.py unset MBEDTLS_ECDSA_DETERMINISTIC
    scripts/config.py unset MBEDTLS_PK_PARSE_C
    scripts/config.py unset MBEDTLS_PK_WRITE_C
    scripts/config.py unset MBEDTLS_RSA_C
    make

    msg "test: crypto_full minus MD"
    make test
}

component_test_full_no_cipher () {
    msg "build: full minus CIPHER"
    scripts/config.py full
    scripts/config.py unset MBEDTLS_CIPHER_C
    # Direct dependencies
    scripts/config.py unset MBEDTLS_CCM_C
    scripts/config.py unset MBEDTLS_CMAC_C
    scripts/config.py unset MBEDTLS_GCM_C
    scripts/config.py unset MBEDTLS_NIST_KW_C
    scripts/config.py unset MBEDTLS_PKCS12_C
    scripts/config.py unset MBEDTLS_PKCS5_C
    scripts/config.py unset MBEDTLS_PSA_CRYPTO_C
    scripts/config.py unset MBEDTLS_SSL_TLS_C
    scripts/config.py unset MBEDTLS_SSL_TICKET_C
    # Indirect dependencies
    scripts/config.py unset MBEDTLS_SSL_CLI_C
    scripts/config.py unset MBEDTLS_PSA_CRYPTO_SE_C
    scripts/config.py unset MBEDTLS_PSA_CRYPTO_STORAGE_C
    scripts/config.py unset MBEDTLS_SSL_DTLS_ANTI_REPLAY
    scripts/config.py unset MBEDTLS_SSL_DTLS_CONNECTION_ID
    scripts/config.py unset MBEDTLS_SSL_PROTO_TLS1_3
    scripts/config.py unset MBEDTLS_SSL_SRV_C
    scripts/config.py unset MBEDTLS_USE_PSA_CRYPTO
    make

    msg "test: full minus CIPHER"
    make test
}

component_test_crypto_full_no_cipher () {
    msg "build: crypto_full minus CIPHER"
    scripts/config.py crypto_full
    scripts/config.py unset MBEDTLS_CIPHER_C
    # Direct dependencies
    scripts/config.py unset MBEDTLS_CCM_C
    scripts/config.py unset MBEDTLS_CMAC_C
    scripts/config.py unset MBEDTLS_GCM_C
    scripts/config.py unset MBEDTLS_NIST_KW_C
    scripts/config.py unset MBEDTLS_PKCS12_C
    scripts/config.py unset MBEDTLS_PKCS5_C
    scripts/config.py unset MBEDTLS_PSA_CRYPTO_C
    # Indirect dependencies
    scripts/config.py unset MBEDTLS_PSA_CRYPTO_SE_C
    scripts/config.py unset MBEDTLS_PSA_CRYPTO_STORAGE_C
    scripts/config.py unset MBEDTLS_USE_PSA_CRYPTO
    make

    msg "test: crypto_full minus CIPHER"
    make test
}

component_test_psa_external_rng_use_psa_crypto () {
    msg "build: full + PSA_CRYPTO_EXTERNAL_RNG + USE_PSA_CRYPTO minus CTR_DRBG"
    scripts/config.py full
    scripts/config.py set MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG
    scripts/config.py set MBEDTLS_USE_PSA_CRYPTO
    scripts/config.py unset MBEDTLS_CTR_DRBG_C
    make CFLAGS="$ASAN_CFLAGS -O2" LDFLAGS="$ASAN_CFLAGS"

    msg "test: full + PSA_CRYPTO_EXTERNAL_RNG + USE_PSA_CRYPTO minus CTR_DRBG"
    make test

    msg "test: full + PSA_CRYPTO_EXTERNAL_RNG + USE_PSA_CRYPTO minus CTR_DRBG"
    tests/ssl-opt.sh -f 'Default\|opaque'
}

component_test_everest () {
    msg "build: Everest ECDH context (ASan build)" # ~ 6 min
    scripts/config.py set MBEDTLS_ECDH_VARIANT_EVEREST_ENABLED
    CC=clang cmake -D CMAKE_BUILD_TYPE:String=Asan .
    make

    msg "test: Everest ECDH context - main suites (inc. selftests) (ASan build)" # ~ 50s
    make test

    msg "test: Everest ECDH context - ECDH-related part of ssl-opt.sh (ASan build)" # ~ 5s
    tests/ssl-opt.sh -f ECDH

    msg "test: Everest ECDH context - compat.sh with some ECDH ciphersuites (ASan build)" # ~ 3 min
    # Exclude some symmetric ciphers that are redundant here to gain time.
    tests/compat.sh -f ECDH -V NO -e 'ARIA\|CAMELLIA\|CHACHA'
}

component_test_everest_curve25519_only () {
    msg "build: Everest ECDH context, only Curve25519" # ~ 6 min
    scripts/config.py set MBEDTLS_ECDH_VARIANT_EVEREST_ENABLED
    scripts/config.py unset MBEDTLS_ECDSA_C
    scripts/config.py unset MBEDTLS_KEY_EXCHANGE_ECDH_ECDSA_ENABLED
    scripts/config.py unset MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED
    scripts/config.py unset MBEDTLS_ECJPAKE_C
    # Disable all curves
    for c in $(sed -n 's/#define \(MBEDTLS_ECP_DP_[0-9A-Z_a-z]*_ENABLED\).*/\1/p' <"$CONFIG_H"); do
        scripts/config.py unset "$c"
    done
    scripts/config.py set MBEDTLS_ECP_DP_CURVE25519_ENABLED

    make CFLAGS="$ASAN_CFLAGS -O2" LDFLAGS="$ASAN_CFLAGS"

    msg "test: Everest ECDH context, only Curve25519" # ~ 50s
    make test
}

component_test_small_ssl_out_content_len () {
    msg "build: small SSL_OUT_CONTENT_LEN (ASan build)"
    scripts/config.py set MBEDTLS_SSL_IN_CONTENT_LEN 16384
    scripts/config.py set MBEDTLS_SSL_OUT_CONTENT_LEN 4096
    CC=gcc cmake -D CMAKE_BUILD_TYPE:String=Asan .
    make

    msg "test: small SSL_OUT_CONTENT_LEN - ssl-opt.sh MFL and large packet tests"
    tests/ssl-opt.sh -f "Max fragment\|Large packet"
}

component_test_small_ssl_in_content_len () {
    msg "build: small SSL_IN_CONTENT_LEN (ASan build)"
    scripts/config.py set MBEDTLS_SSL_IN_CONTENT_LEN 4096
    scripts/config.py set MBEDTLS_SSL_OUT_CONTENT_LEN 16384
    CC=gcc cmake -D CMAKE_BUILD_TYPE:String=Asan .
    make

    msg "test: small SSL_IN_CONTENT_LEN - ssl-opt.sh MFL tests"
    tests/ssl-opt.sh -f "Max fragment"
}

component_test_small_ssl_dtls_max_buffering () {
    msg "build: small MBEDTLS_SSL_DTLS_MAX_BUFFERING #0"
    scripts/config.py set MBEDTLS_SSL_DTLS_MAX_BUFFERING 1000
    CC=gcc cmake -D CMAKE_BUILD_TYPE:String=Asan .
    make

    msg "test: small MBEDTLS_SSL_DTLS_MAX_BUFFERING #0 - ssl-opt.sh specific reordering test"
    tests/ssl-opt.sh -f "DTLS reordering: Buffer out-of-order hs msg before reassembling next, free buffered msg"
}

component_test_small_mbedtls_ssl_dtls_max_buffering () {
    msg "build: small MBEDTLS_SSL_DTLS_MAX_BUFFERING #1"
    scripts/config.py set MBEDTLS_SSL_DTLS_MAX_BUFFERING 190
    CC=gcc cmake -D CMAKE_BUILD_TYPE:String=Asan .
    make

    msg "test: small MBEDTLS_SSL_DTLS_MAX_BUFFERING #1 - ssl-opt.sh specific reordering test"
    tests/ssl-opt.sh -f "DTLS reordering: Buffer encrypted Finished message, drop for fragmented NewSessionTicket"
}

component_test_psa_collect_statuses () {
  msg "build+test: psa_collect_statuses" # ~30s
  scripts/config.py full
  tests/scripts/psa_collect_statuses.py
  # Check that psa_crypto_init() succeeded at least once
  grep -q '^0:psa_crypto_init:' tests/statuses.log
  rm -f tests/statuses.log
}

component_test_full_cmake_clang () {
    msg "build: cmake, full config, clang" # ~ 50s
    scripts/config.py full
    CC=clang cmake -D CMAKE_BUILD_TYPE:String=Release -D ENABLE_TESTING=On .
    make

    msg "test: main suites (full config, clang)" # ~ 5s
    make test

    msg "test: psa_constant_names (full config, clang)" # ~ 1s
    tests/scripts/test_psa_constant_names.py

    msg "test: ssl-opt.sh default, ECJPAKE, SSL async (full config)" # ~ 1s
    tests/ssl-opt.sh -f 'Default\|ECJPAKE\|SSL async private'

    msg "test: compat.sh NULL (full config)" # ~ 2 min
    env OPENSSL_CMD="$OPENSSL_LEGACY" GNUTLS_CLI="$GNUTLS_LEGACY_CLI" GNUTLS_SERV="$GNUTLS_LEGACY_SERV" tests/compat.sh -e '^$' -f 'NULL'

    msg "test: compat.sh ARIA + ChachaPoly"
    env OPENSSL_CMD="$OPENSSL_NEXT" tests/compat.sh -e '^$' -f 'ARIA\|CHACHA'
}

component_test_memsan_constant_flow () {
    # This tests both (1) accesses to undefined memory, and (2) branches or
    # memory access depending on secret values. To distinguish between those:
    # - unset MBEDTLS_TEST_CONSTANT_FLOW_MEMSAN - does the failure persist?
    # - or alternatively, change the build type to MemSanDbg, which enables
    # origin tracking and nicer stack traces (which are useful for debugging
    # anyway), and check if the origin was TEST_CF_SECRET() or something else.
    msg "build: cmake MSan (clang), full config minus MBEDTLS_USE_PSA_CRYPTO with constant flow testing"
    scripts/config.py full
    scripts/config.py set MBEDTLS_TEST_CONSTANT_FLOW_MEMSAN
    scripts/config.py unset MBEDTLS_USE_PSA_CRYPTO
    scripts/config.py unset MBEDTLS_AESNI_C # memsan doesn't grok asm
    CC=clang cmake -D CMAKE_BUILD_TYPE:String=MemSan .
    make

    msg "test: main suites (full minus MBEDTLS_USE_PSA_CRYPTO, Msan + constant flow)"
    make test
}

component_test_memsan_constant_flow_psa () {
    # This tests both (1) accesses to undefined memory, and (2) branches or
    # memory access depending on secret values. To distinguish between those:
    # - unset MBEDTLS_TEST_CONSTANT_FLOW_MEMSAN - does the failure persist?
    # - or alternatively, change the build type to MemSanDbg, which enables
    # origin tracking and nicer stack traces (which are useful for debugging
    # anyway), and check if the origin was TEST_CF_SECRET() or something else.
    msg "build: cmake MSan (clang), full config with constant flow testing"
    scripts/config.py full
    scripts/config.py set MBEDTLS_TEST_CONSTANT_FLOW_MEMSAN
    scripts/config.py unset MBEDTLS_AESNI_C # memsan doesn't grok asm
    CC=clang cmake -D CMAKE_BUILD_TYPE:String=MemSan .
    make

    msg "test: main suites (Msan + constant flow)"
    make test
}

component_test_valgrind_constant_flow () {
    # This tests both (1) everything that valgrind's memcheck usually checks
    # (heap buffer overflows, use of uninitialized memory, use-after-free,
    # etc.) and (2) branches or memory access depending on secret values,
    # which will be reported as uninitialized memory. To distinguish between
    # secret and actually uninitialized:
    # - unset MBEDTLS_TEST_CONSTANT_FLOW_VALGRIND - does the failure persist?
    # - or alternatively, build with debug info and manually run the offending
    # test suite with valgrind --track-origins=yes, then check if the origin
    # was TEST_CF_SECRET() or something else.
    msg "build: cmake release GCC, full config minus MBEDTLS_USE_PSA_CRYPTO with constant flow testing"
    scripts/config.py full
    scripts/config.py set MBEDTLS_TEST_CONSTANT_FLOW_VALGRIND
    scripts/config.py unset MBEDTLS_USE_PSA_CRYPTO
    cmake -D CMAKE_BUILD_TYPE:String=Release .
    make

    # this only shows a summary of the results (how many of each type)
    # details are left in Testing/<date>/DynamicAnalysis.xml
    msg "test: main suites (full minus MBEDTLS_USE_PSA_CRYPTO, valgrind + constant flow)"
    make memcheck
}

component_test_valgrind_constant_flow_psa () {
    # This tests both (1) everything that valgrind's memcheck usually checks
    # (heap buffer overflows, use of uninitialized memory, use-after-free,
    # etc.) and (2) branches or memory access depending on secret values,
    # which will be reported as uninitialized memory. To distinguish between
    # secret and actually uninitialized:
    # - unset MBEDTLS_TEST_CONSTANT_FLOW_VALGRIND - does the failure persist?
    # - or alternatively, build with debug info and manually run the offending
    # test suite with valgrind --track-origins=yes, then check if the origin
    # was TEST_CF_SECRET() or something else.
    msg "build: cmake release GCC, full config with constant flow testing"
    scripts/config.py full
    scripts/config.py set MBEDTLS_TEST_CONSTANT_FLOW_VALGRIND
    cmake -D CMAKE_BUILD_TYPE:String=Release .
    make

    # this only shows a summary of the results (how many of each type)
    # details are left in Testing/<date>/DynamicAnalysis.xml
    msg "test: main suites (valgrind + constant flow)"
    make memcheck
}

component_test_default_no_deprecated () {
    # Test that removing the deprecated features from the default
    # configuration leaves something consistent.
    msg "build: make, default + MBEDTLS_DEPRECATED_REMOVED" # ~ 30s
    scripts/config.py set MBEDTLS_DEPRECATED_REMOVED
    make CC=gcc CFLAGS='-O -Werror -Wall -Wextra'

    msg "test: make, default + MBEDTLS_DEPRECATED_REMOVED" # ~ 5s
    make test
}

component_test_full_no_deprecated () {
    msg "build: make, full_no_deprecated config" # ~ 30s
    scripts/config.py full_no_deprecated
    make CC=gcc CFLAGS='-O -Werror -Wall -Wextra'

    msg "test: make, full_no_deprecated config" # ~ 5s
    make test
}

component_test_full_no_deprecated_deprecated_warning () {
    # Test that there is nothing deprecated in "full_no_deprecated".
    # A deprecated feature would trigger a warning (made fatal) from
    # MBEDTLS_DEPRECATED_WARNING.
    msg "build: make, full_no_deprecated config, MBEDTLS_DEPRECATED_WARNING" # ~ 30s
    scripts/config.py full_no_deprecated
    scripts/config.py unset MBEDTLS_DEPRECATED_REMOVED
    scripts/config.py set MBEDTLS_DEPRECATED_WARNING
    make CC=gcc CFLAGS='-O -Werror -Wall -Wextra'

    msg "test: make, full_no_deprecated config, MBEDTLS_DEPRECATED_WARNING" # ~ 5s
    make test
}

component_test_full_deprecated_warning () {
    # Test that when MBEDTLS_DEPRECATED_WARNING is enabled, the build passes
    # with only certain whitelisted types of warnings.
    msg "build: make, full config + MBEDTLS_DEPRECATED_WARNING, expect warnings" # ~ 30s
    scripts/config.py full
    scripts/config.py set MBEDTLS_DEPRECATED_WARNING
    # Expect warnings from '#warning' directives in check_config.h.
    make CC=gcc CFLAGS='-O -Werror -Wall -Wextra -Wno-error=cpp' lib programs

    msg "build: make tests, full config + MBEDTLS_DEPRECATED_WARNING, expect warnings" # ~ 30s
    # Set MBEDTLS_TEST_DEPRECATED to enable tests for deprecated features.
    # By default those are disabled when MBEDTLS_DEPRECATED_WARNING is set.
    # Expect warnings from '#warning' directives in check_config.h and
    # from the use of deprecated functions in test suites.
    make CC=gcc CFLAGS='-O -Werror -Wall -Wextra -Wno-error=deprecated-declarations -Wno-error=cpp -DMBEDTLS_TEST_DEPRECATED' tests

    msg "test: full config + MBEDTLS_TEST_DEPRECATED" # ~ 30s
    make test
}

# Check that the specified libraries exist and are empty.
are_empty_libraries () {
  nm "$@" >/dev/null 2>/dev/null
  ! nm "$@" 2>/dev/null | grep -v ':$' | grep .
}

component_build_crypto_default () {
  msg "build: make, crypto only"
  scripts/config.py crypto
  make CFLAGS='-O1 -Werror'
  are_empty_libraries library/libmbedx509.* library/libmbedtls.*
}

component_build_crypto_full () {
  msg "build: make, crypto only, full config"
  scripts/config.py crypto_full
  make CFLAGS='-O1 -Werror'
  are_empty_libraries library/libmbedx509.* library/libmbedtls.*
}

component_build_crypto_baremetal () {
  msg "build: make, crypto only, baremetal config"
  scripts/config.py crypto_baremetal
  make CFLAGS="-O1 -Werror -I$PWD/tests/include/baremetal-override/"
  are_empty_libraries library/libmbedx509.* library/libmbedtls.*
}
support_build_crypto_baremetal () {
    support_build_baremetal "$@"
}

component_build_baremetal () {
  msg "build: make, baremetal config"
  scripts/config.py baremetal
  make CFLAGS="-O1 -Werror -I$PWD/tests/include/baremetal-override/"
}
support_build_baremetal () {
    # Older Glibc versions include time.h from other headers such as stdlib.h,
    # which makes the no-time.h-in-baremetal check fail. Ubuntu 16.04 has this
    # problem, Ubuntu 18.04 is ok.
    ! grep -q -F time.h /usr/include/x86_64-linux-gnu/sys/types.h
}

component_test_depends_curves () {
    msg "test/build: curves.pl (gcc)" # ~ 4 min
    tests/scripts/curves.pl
}

component_test_depends_curves_psa () {
    msg "test/build: curves.pl with MBEDTLS_USE_PSA_CRYPTO defined (gcc)"
    scripts/config.py set MBEDTLS_USE_PSA_CRYPTO
    tests/scripts/curves.pl
}

component_test_depends_hashes () {
    msg "test/build: depends-hashes.pl (gcc)" # ~ 2 min
    tests/scripts/depends-hashes.pl
}

component_test_depends_hashes_psa () {
    msg "test/build: depends-hashes.pl with MBEDTLS_USE_PSA_CRYPTO defined (gcc)"
    scripts/config.py set MBEDTLS_USE_PSA_CRYPTO
    tests/scripts/depends-hashes.pl
}

component_test_depends_pkalgs () {
    msg "test/build: depends-pkalgs.pl (gcc)" # ~ 2 min
    tests/scripts/depends-pkalgs.pl
}

component_test_depends_pkalgs_psa () {
    msg "test/build: depends-pkalgs.pl with MBEDTLS_USE_PSA_CRYPTO defined (gcc)"
    scripts/config.py set MBEDTLS_USE_PSA_CRYPTO
    tests/scripts/depends-pkalgs.pl
}

component_build_key_exchanges () {
    msg "test/build: key-exchanges (gcc)" # ~ 1 min
    tests/scripts/key-exchanges.pl
}

component_test_make_cxx () {
    msg "build: Unix make, full, gcc + g++"
    scripts/config.py full
    make TEST_CPP=1 lib programs

    msg "test: cpp_dummy_build"
    programs/test/cpp_dummy_build
}

component_build_module_alt () {
    msg "build: MBEDTLS_XXX_ALT" # ~30s
    scripts/config.py full
    # Disable options that are incompatible with some ALT implementations.
    # aesni.c and padlock.c reference mbedtls_aes_context fields directly.
    scripts/config.py unset MBEDTLS_AESNI_C
    scripts/config.py unset MBEDTLS_PADLOCK_C
    # You can only have one threading implementation: alt or pthread, not both.
    scripts/config.py unset MBEDTLS_THREADING_PTHREAD
    # The SpecifiedECDomain parsing code accesses mbedtls_ecp_group fields
    # directly and assumes the implementation works with partial groups.
    scripts/config.py unset MBEDTLS_PK_PARSE_EC_EXTENDED
    # MBEDTLS_SHA256_*ALT can't be used with MBEDTLS_SHA256_USE_A64_CRYPTO_*
    scripts/config.py unset MBEDTLS_SHA256_USE_A64_CRYPTO_IF_PRESENT
    scripts/config.py unset MBEDTLS_SHA256_USE_A64_CRYPTO_ONLY
    # MBEDTLS_SHA512_*ALT can't be used with MBEDTLS_SHA512_USE_A64_CRYPTO_*
    scripts/config.py unset MBEDTLS_SHA512_USE_A64_CRYPTO_IF_PRESENT
    scripts/config.py unset MBEDTLS_SHA512_USE_A64_CRYPTO_ONLY
    # Enable all MBEDTLS_XXX_ALT for whole modules. Do not enable
    # MBEDTLS_XXX_YYY_ALT which are for single functions.
    scripts/config.py set-all 'MBEDTLS_([A-Z0-9]*|NIST_KW)_ALT'
    scripts/config.py unset MBEDTLS_DHM_ALT #incompatible with MBEDTLS_DEBUG_C
    # We can only compile, not link, since we don't have any implementations
    # suitable for testing with the dummy alt headers.
    make CC=gcc CFLAGS='-Werror -Wall -Wextra -I../tests/include/alt-dummy' lib
}

component_build_dhm_alt () {
    msg "build: MBEDTLS_DHM_ALT" # ~30s
    scripts/config.py full
    scripts/config.py set MBEDTLS_DHM_ALT
    # debug.c currently references mbedtls_dhm_context fields directly.
    scripts/config.py unset MBEDTLS_DEBUG_C
    # We can only compile, not link, since we don't have any implementations
    # suitable for testing with the dummy alt headers.
    make CC=gcc CFLAGS='-Werror -Wall -Wextra -I../tests/include/alt-dummy' lib
}

component_test_no_use_psa_crypto_full_cmake_asan() {
    # full minus MBEDTLS_USE_PSA_CRYPTO: run the same set of tests as basic-build-test.sh
    msg "build: cmake, full config minus MBEDTLS_USE_PSA_CRYPTO, ASan"
    scripts/config.py full
    scripts/config.py set MBEDTLS_ECP_RESTARTABLE  # not using PSA, so enable restartable ECC
    scripts/config.py unset MBEDTLS_PSA_CRYPTO_C
    scripts/config.py unset MBEDTLS_USE_PSA_CRYPTO
    scripts/config.py unset MBEDTLS_SSL_PROTO_TLS1_3
    scripts/config.py unset MBEDTLS_PSA_ITS_FILE_C
    scripts/config.py unset MBEDTLS_PSA_CRYPTO_SE_C
    scripts/config.py unset MBEDTLS_PSA_CRYPTO_STORAGE_C
    CC=gcc cmake -D CMAKE_BUILD_TYPE:String=Asan .
    make

    msg "test: main suites (full minus MBEDTLS_USE_PSA_CRYPTO)"
    make test

    msg "test: ssl-opt.sh (full minus MBEDTLS_USE_PSA_CRYPTO)"
    tests/ssl-opt.sh

    msg "test: compat.sh default (full minus MBEDTLS_USE_PSA_CRYPTO)"
    tests/compat.sh

    msg "test: compat.sh NULL (full minus MBEDTLS_USE_PSA_CRYPTO)"
    env OPENSSL_CMD="$OPENSSL_LEGACY" GNUTLS_CLI="$GNUTLS_LEGACY_CLI" GNUTLS_SERV="$GNUTLS_LEGACY_SERV" tests/compat.sh -f 'NULL'

    msg "test: compat.sh ARIA + ChachaPoly (full minus MBEDTLS_USE_PSA_CRYPTO)"
    env OPENSSL_CMD="$OPENSSL_NEXT" tests/compat.sh -e '^$' -f 'ARIA\|CHACHA'
}

component_test_psa_crypto_config_accel_ecdsa () {
    msg "test: MBEDTLS_PSA_CRYPTO_CONFIG with accelerated ECDSA"

    # Disable ALG_STREAM_CIPHER and ALG_ECB_NO_PADDING to avoid having
    # partial support for cipher operations in the driver test library.
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_STREAM_CIPHER
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_ECB_NO_PADDING

    # SHA384 needed for some ECDSA signature tests.
    scripts/config.py -f tests/include/test/drivers/config_test_driver.h set MBEDTLS_SHA384_C
    scripts/config.py -f tests/include/test/drivers/config_test_driver.h set MBEDTLS_SHA512_C

    loc_accel_list="ALG_ECDSA ALG_DETERMINISTIC_ECDSA KEY_TYPE_ECC_KEY_PAIR KEY_TYPE_ECC_PUBLIC_KEY"
    loc_accel_flags=$( echo "$loc_accel_list" | sed 's/[^ ]* */-DLIBTESTDRIVER1_MBEDTLS_PSA_ACCEL_&/g' )
    make -C tests libtestdriver1.a CFLAGS="$ASAN_CFLAGS $loc_accel_flags" LDFLAGS="$ASAN_CFLAGS"

    # Restore test driver base configuration
    scripts/config.py -f tests/include/test/drivers/config_test_driver.h unset MBEDTLS_SHA384_C
    scripts/config.py -f tests/include/test/drivers/config_test_driver.h unset MBEDTLS_SHA512_C

    scripts/config.py set MBEDTLS_PSA_CRYPTO_DRIVERS
    scripts/config.py set MBEDTLS_PSA_CRYPTO_CONFIG
    scripts/config.py unset MBEDTLS_USE_PSA_CRYPTO
    scripts/config.py unset MBEDTLS_SSL_PROTO_TLS1_3
    scripts/config.py unset MBEDTLS_ECDSA_C
    scripts/config.py unset MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED
    scripts/config.py unset MBEDTLS_KEY_EXCHANGE_ECDH_ECDSA_ENABLED

    loc_accel_flags="$loc_accel_flags $( echo "$loc_accel_list" | sed 's/[^ ]* */-DMBEDTLS_PSA_ACCEL_&/g' )"
    make CFLAGS="$ASAN_CFLAGS -O -Werror -I../tests/include -I../tests -DPSA_CRYPTO_DRIVER_TEST -DMBEDTLS_TEST_LIBTESTDRIVER1 $loc_accel_flags" LDFLAGS="-ltestdriver1 $ASAN_CFLAGS"

    not grep mbedtls_ecdsa_ library/ecdsa.o

    msg "test: MBEDTLS_PSA_CRYPTO_CONFIG with accelerated ECDSA"
    make test
}

component_test_psa_crypto_config_accel_rsa_signature () {
    msg "test: MBEDTLS_PSA_CRYPTO_CONFIG with accelerated RSA signature"

    # Disable ALG_STREAM_CIPHER and ALG_ECB_NO_PADDING to avoid having
    # partial support for cipher operations in the driver test library.
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_STREAM_CIPHER
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_ECB_NO_PADDING

    # It seems it is not possible to remove only the support for RSA signature
    # in the library. Thus we have to remove all RSA support (signature and
    # encryption/decryption). AS there is no driver support for asymmetric
    # encryption/decryption so far remove RSA encryption/decryption from the
    # application algorithm list.
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_RSA_OAEP
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_RSA_PKCS1V15_CRYPT

    # Make sure both the library and the test library support the SHA hash
    # algorithms and only those ones (SHA256 is included by default). That way:
    # - the test library can compute the RSA signatures even in the case of a
    #   composite RSA signature algorithm based on a SHA hash (no other hash
    #   used in the unit tests).
    # - the dependency of RSA signature tests on PSA_WANT_ALG_SHA_xyz is
    #   fulfilled as the hash SHA algorithm is supported by the library, and
    #   thus the tests are run, not skipped.
    # - when testing a signature key with an algorithm wildcard built from
    #   PSA_ALG_ANY_HASH as algorithm to test with the key, the chosen hash
    #   algorithm based on the hashes supported by the library is also
    #   supported by the test library.
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_MD5
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_RIPEMD160_C

    scripts/config.py -f tests/include/test/drivers/config_test_driver.h set MBEDTLS_SHA1_C
    scripts/config.py -f tests/include/test/drivers/config_test_driver.h set MBEDTLS_SHA512_C
    # We need PEM parsing in the test library as well to support the import
    # of PEM encoded RSA keys.
    scripts/config.py -f tests/include/test/drivers/config_test_driver.h set MBEDTLS_PEM_PARSE_C
    scripts/config.py -f tests/include/test/drivers/config_test_driver.h set MBEDTLS_BASE64_C

    loc_accel_list="ALG_RSA_PKCS1V15_SIGN ALG_RSA_PSS KEY_TYPE_RSA_KEY_PAIR KEY_TYPE_RSA_PUBLIC_KEY"
    loc_accel_flags=$( echo "$loc_accel_list" | sed 's/[^ ]* */-DLIBTESTDRIVER1_MBEDTLS_PSA_ACCEL_&/g' )
    make -C tests libtestdriver1.a CFLAGS="$ASAN_CFLAGS $loc_accel_flags" LDFLAGS="$ASAN_CFLAGS"

    # Restore test driver base configuration
    scripts/config.py -f tests/include/test/drivers/config_test_driver.h unset MBEDTLS_SHA1_C
    scripts/config.py -f tests/include/test/drivers/config_test_driver.h unset MBEDTLS_SHA512_C
    scripts/config.py -f tests/include/test/drivers/config_test_driver.h unset MBEDTLS_PEM_PARSE_C
    scripts/config.py -f tests/include/test/drivers/config_test_driver.h unset MBEDTLS_BASE64_C


    # Mbed TLS library build
    scripts/config.py set MBEDTLS_PSA_CRYPTO_DRIVERS
    scripts/config.py set MBEDTLS_PSA_CRYPTO_CONFIG

    # Remove RSA support and its dependencies
    scripts/config.py unset MBEDTLS_PKCS1_V15
    scripts/config.py unset MBEDTLS_PKCS1_V21
    scripts/config.py unset MBEDTLS_KEY_EXCHANGE_DHE_RSA_ENABLED
    scripts/config.py unset MBEDTLS_KEY_EXCHANGE_ECDH_RSA_ENABLED
    scripts/config.py unset MBEDTLS_KEY_EXCHANGE_ECDHE_RSA_ENABLED
    scripts/config.py unset MBEDTLS_KEY_EXCHANGE_RSA_PSK_ENABLED
    scripts/config.py unset MBEDTLS_KEY_EXCHANGE_RSA_ENABLED
    scripts/config.py unset MBEDTLS_RSA_C
    scripts/config.py unset MBEDTLS_X509_RSASSA_PSS_SUPPORT

    scripts/config.py unset MBEDTLS_MD5_C
    scripts/config.py unset MBEDTLS_RIPEMD160_C
    scripts/config.py unset MBEDTLS_SSL_PROTO_TLS1
    scripts/config.py unset MBEDTLS_SSL_PROTO_TLS1_1
    scripts/config.py unset MBEDTLS_SSL_CBC_RECORD_SPLITTING

    loc_accel_flags="$loc_accel_flags $( echo "$loc_accel_list" | sed 's/[^ ]* */-DMBEDTLS_PSA_ACCEL_&/g' )"
    make CFLAGS="$ASAN_CFLAGS -Werror -I../tests/include -I../tests -DPSA_CRYPTO_DRIVER_TEST -DMBEDTLS_TEST_LIBTESTDRIVER1 $loc_accel_flags" LDFLAGS="-ltestdriver1 $ASAN_CFLAGS"

    not grep mbedtls_rsa_rsassa_pkcs1_v15_sign library/rsa.o
    not grep mbedtls_rsa_rsassa_pss_sign_ext library/rsa.o

    msg "test: MBEDTLS_PSA_CRYPTO_CONFIG with accelerated RSA signature"
    make test
}

component_test_psa_crypto_config_accel_hash () {
    msg "test: MBEDTLS_PSA_CRYPTO_CONFIG with accelerated hash"

    # Disable ALG_STREAM_CIPHER and ALG_ECB_NO_PADDING to avoid having
    # partial support for cipher operations in the driver test library.
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_STREAM_CIPHER
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_ECB_NO_PADDING

    loc_accel_list="ALG_MD5 ALG_RIPEMD160 ALG_SHA_1 ALG_SHA_224 ALG_SHA_256 ALG_SHA_384 ALG_SHA_512"
    loc_accel_flags=$( echo "$loc_accel_list" | sed 's/[^ ]* */-DLIBTESTDRIVER1_MBEDTLS_PSA_ACCEL_&/g' )
    make -C tests libtestdriver1.a CFLAGS="$ASAN_CFLAGS $loc_accel_flags" LDFLAGS="$ASAN_CFLAGS"

    scripts/config.py set MBEDTLS_PSA_CRYPTO_DRIVERS
    scripts/config.py set MBEDTLS_PSA_CRYPTO_CONFIG
    scripts/config.py unset MBEDTLS_MD5_C
    scripts/config.py unset MBEDTLS_RIPEMD160_C
    scripts/config.py unset MBEDTLS_SHA1_C
    # Don't unset MBEDTLS_SHA256_C as it is needed by PSA crypto core.
    scripts/config.py unset MBEDTLS_SHA384_C
    scripts/config.py unset MBEDTLS_SHA512_C
    # Unset MBEDTLS_SSL_PROTO_SSL3, MBEDTLS_SSL_PROTO_TLS1 and MBEDTLS_SSL_PROTO_TLS1_1 as they depend on MBEDTLS_SHA1_C
    scripts/config.py unset MBEDTLS_SSL_PROTO_SSL3
    scripts/config.py unset MBEDTLS_SSL_PROTO_TLS1
    scripts/config.py unset MBEDTLS_SSL_PROTO_TLS1_1
    # Unset MBEDTLS_SSL_CBC_RECORD_SPLITTING as it depends on MBEDTLS_SSL_PROTO_TLS1 in the default configuration.
    scripts/config.py unset MBEDTLS_SSL_CBC_RECORD_SPLITTING
    loc_accel_flags="$loc_accel_flags $( echo "$loc_accel_list" | sed 's/[^ ]* */-DMBEDTLS_PSA_ACCEL_&/g' )"
    make CFLAGS="$ASAN_CFLAGS -Werror -I../tests/include -I../tests -DPSA_CRYPTO_DRIVER_TEST -DMBEDTLS_TEST_LIBTESTDRIVER1 $loc_accel_flags" LDFLAGS="-ltestdriver1 $ASAN_CFLAGS"

    not grep mbedtls_sha512_init library/sha512.o
    not grep mbedtls_sha1_init library/sha1.o

    msg "test: MBEDTLS_PSA_CRYPTO_CONFIG with accelerated hash"
    make test
}

component_test_psa_crypto_config_accel_cipher () {
    msg "test: MBEDTLS_PSA_CRYPTO_CONFIG with accelerated cipher"

    loc_accel_list="ALG_CBC_NO_PADDING ALG_CBC_PKCS7 ALG_CTR ALG_CFB ALG_OFB ALG_XTS KEY_TYPE_DES"
    loc_accel_flags=$( echo "$loc_accel_list" | sed 's/[^ ]* */-DLIBTESTDRIVER1_MBEDTLS_PSA_ACCEL_&/g' )
    make -C tests libtestdriver1.a CFLAGS="$ASAN_CFLAGS $loc_accel_flags" LDFLAGS="$ASAN_CFLAGS"

    scripts/config.py set MBEDTLS_PSA_CRYPTO_DRIVERS
    scripts/config.py set MBEDTLS_PSA_CRYPTO_CONFIG

    # There is no intended accelerator support for ALG STREAM_CIPHER and
    # ALG_ECB_NO_PADDING. Therefore, asking for them in the build implies the
    # inclusion of the Mbed TLS cipher operations. As we want to test here with
    # cipher operations solely supported by accelerators, disabled those
    # PSA configuration options.
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_STREAM_CIPHER
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_ECB_NO_PADDING
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_CMAC

    scripts/config.py unset MBEDTLS_CIPHER_MODE_CBC
    scripts/config.py unset MBEDTLS_CIPHER_PADDING_PKCS7
    scripts/config.py unset MBEDTLS_CIPHER_MODE_CTR
    scripts/config.py unset MBEDTLS_CIPHER_MODE_CFB
    scripts/config.py unset MBEDTLS_CIPHER_MODE_OFB
    scripts/config.py unset MBEDTLS_CIPHER_MODE_XTS
    scripts/config.py unset MBEDTLS_DES_C

    loc_accel_flags="$loc_accel_flags $( echo "$loc_accel_list" | sed 's/[^ ]* */-DMBEDTLS_PSA_ACCEL_&/g' )"
    make CFLAGS="$ASAN_CFLAGS -Werror -I../tests/include -I../tests -DPSA_CRYPTO_DRIVER_TEST -DMBEDTLS_TEST_LIBTESTDRIVER1 $loc_accel_flags" LDFLAGS="-ltestdriver1 $ASAN_CFLAGS"

    not grep mbedtls_des* library/des.o

    msg "test: MBEDTLS_PSA_CRYPTO_CONFIG with accelerated hash"
    make test
}

component_test_psa_crypto_config_no_driver() {
    # full plus MBEDTLS_PSA_CRYPTO_CONFIG
    msg "build: full + MBEDTLS_PSA_CRYPTO_CONFIG minus MBEDTLS_PSA_CRYPTO_DRIVERS"
    scripts/config.py full
    scripts/config.py set MBEDTLS_PSA_CRYPTO_CONFIG
    scripts/config.py unset MBEDTLS_PSA_CRYPTO_DRIVERS
    scripts/config.py unset MBEDTLS_USE_PSA_CRYPTO
    scripts/config.py unset MBEDTLS_SSL_PROTO_TLS1_3
    make CC=gcc CFLAGS="$ASAN_CFLAGS -O2" LDFLAGS="$ASAN_CFLAGS"

    msg "test: full + MBEDTLS_PSA_CRYPTO_CONFIG minus MBEDTLS_PSA_CRYPTO_DRIVERS"
    make test
}

component_test_psa_crypto_config_chachapoly_disabled() {
    # full minus MBEDTLS_CHACHAPOLY_C without PSA_WANT_ALG_GCM and PSA_WANT_ALG_CHACHA20_POLY1305
    msg "build: full minus MBEDTLS_CHACHAPOLY_C without PSA_WANT_ALG_GCM and PSA_WANT_ALG_CHACHA20_POLY1305"
    scripts/config.py full
    scripts/config.py unset MBEDTLS_CHACHAPOLY_C
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_GCM
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_CHACHA20_POLY1305
    make CC=gcc CFLAGS="$ASAN_CFLAGS -O2" LDFLAGS="$ASAN_CFLAGS"

    msg "test: full minus MBEDTLS_CHACHAPOLY_C without PSA_WANT_ALG_GCM and PSA_WANT_ALG_CHACHA20_POLY1305"
    make test
}

# This should be renamed to test and updated once the accelerator ECDSA code is in place and ready to test.
component_build_psa_accel_alg_ecdsa() {
    # full plus MBEDTLS_PSA_CRYPTO_CONFIG with PSA_WANT_ALG_ECDSA
    # without MBEDTLS_ECDSA_C
    # PSA_WANT_ALG_ECDSA and PSA_WANT_ALG_DETERMINISTIC_ECDSA are already
    # set in include/psa/crypto_config.h
    msg "build: full + MBEDTLS_PSA_CRYPTO_CONFIG + PSA_WANT_ALG_ECDSA without MBEDTLS_ECDSA_C"
    scripts/config.py full
    scripts/config.py set MBEDTLS_PSA_CRYPTO_CONFIG
    scripts/config.py set MBEDTLS_PSA_CRYPTO_DRIVERS
    scripts/config.py unset MBEDTLS_USE_PSA_CRYPTO
    scripts/config.py unset MBEDTLS_SSL_PROTO_TLS1_3
    scripts/config.py unset MBEDTLS_ECDSA_C
    scripts/config.py unset MBEDTLS_KEY_EXCHANGE_ECDH_ECDSA_ENABLED
    scripts/config.py unset MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED
    # Need to define the correct symbol and include the test driver header path in order to build with the test driver
    make CC=gcc CFLAGS="$ASAN_CFLAGS -DPSA_CRYPTO_DRIVER_TEST -DMBEDTLS_PSA_ACCEL_ALG_ECDSA -DMBEDTLS_PSA_ACCEL_ALG_DETERMINISTIC_ECDSA -I../tests/include -O2" LDFLAGS="$ASAN_CFLAGS"
}

# This should be renamed to test and updated once the accelerator ECDH code is in place and ready to test.
component_build_psa_accel_alg_ecdh() {
    # full plus MBEDTLS_PSA_CRYPTO_CONFIG with PSA_WANT_ALG_ECDH
    # without MBEDTLS_ECDH_C
    msg "build: full + MBEDTLS_PSA_CRYPTO_CONFIG + PSA_WANT_ALG_ECDH without MBEDTLS_ECDH_C"
    scripts/config.py full
    scripts/config.py set MBEDTLS_PSA_CRYPTO_CONFIG
    scripts/config.py set MBEDTLS_PSA_CRYPTO_DRIVERS
    scripts/config.py unset MBEDTLS_USE_PSA_CRYPTO
    scripts/config.py unset MBEDTLS_SSL_PROTO_TLS1_3
    scripts/config.py unset MBEDTLS_ECDH_C
    scripts/config.py unset MBEDTLS_KEY_EXCHANGE_ECDH_RSA_ENABLED
    scripts/config.py unset MBEDTLS_KEY_EXCHANGE_ECDH_ECDSA_ENABLED
    scripts/config.py unset MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED
    scripts/config.py unset MBEDTLS_KEY_EXCHANGE_ECDHE_RSA_ENABLED
    scripts/config.py unset MBEDTLS_KEY_EXCHANGE_ECDHE_PSK_ENABLED
    # Need to define the correct symbol and include the test driver header path in order to build with the test driver
    make CC=gcc CFLAGS="$ASAN_CFLAGS -DPSA_CRYPTO_DRIVER_TEST -DMBEDTLS_PSA_ACCEL_ALG_ECDH -I../tests/include -O2" LDFLAGS="$ASAN_CFLAGS"
}

# This should be renamed to test and updated once the accelerator ECC key pair code is in place and ready to test.
component_build_psa_accel_key_type_ecc_key_pair() {
    # full plus MBEDTLS_PSA_CRYPTO_CONFIG with PSA_WANT_KEY_TYPE_ECC_KEY_PAIR
    msg "build: full + MBEDTLS_PSA_CRYPTO_CONFIG + PSA_WANT_KEY_TYPE_ECC_KEY_PAIR"
    scripts/config.py full
    scripts/config.py set MBEDTLS_PSA_CRYPTO_CONFIG
    scripts/config.py set MBEDTLS_PSA_CRYPTO_DRIVERS
    scripts/config.py unset MBEDTLS_USE_PSA_CRYPTO
    scripts/config.py unset MBEDTLS_SSL_PROTO_TLS1_3
    scripts/config.py -f include/psa/crypto_config.h set PSA_WANT_KEY_TYPE_ECC_KEY_PAIR 1
    scripts/config.py -f include/psa/crypto_config.h set PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY 1
    # Need to define the correct symbol and include the test driver header path in order to build with the test driver
    make CC=gcc CFLAGS="$ASAN_CFLAGS -DPSA_CRYPTO_DRIVER_TEST -DMBEDTLS_PSA_ACCEL_KEY_TYPE_ECC_KEY_PAIR -I../tests/include -O2" LDFLAGS="$ASAN_CFLAGS"
}

# This should be renamed to test and updated once the accelerator ECC public key code is in place and ready to test.
component_build_psa_accel_key_type_ecc_public_key() {
    # full plus MBEDTLS_PSA_CRYPTO_CONFIG with PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY
    msg "build: full + MBEDTLS_PSA_CRYPTO_CONFIG + PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY"
    scripts/config.py full
    scripts/config.py set MBEDTLS_PSA_CRYPTO_CONFIG
    scripts/config.py set MBEDTLS_PSA_CRYPTO_DRIVERS
    scripts/config.py unset MBEDTLS_USE_PSA_CRYPTO
    scripts/config.py unset MBEDTLS_SSL_PROTO_TLS1_3
    scripts/config.py -f include/psa/crypto_config.h set PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY 1
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_KEY_TYPE_ECC_KEY_PAIR
    # Need to define the correct symbol and include the test driver header path in order to build with the test driver
    make CC=gcc CFLAGS="$ASAN_CFLAGS -DPSA_CRYPTO_DRIVER_TEST -DMBEDTLS_PSA_ACCEL_KEY_TYPE_ECC_PUBLIC_KEY -I../tests/include -O2" LDFLAGS="$ASAN_CFLAGS"
}

# This should be renamed to test and updated once the accelerator HMAC code is in place and ready to test.
component_build_psa_accel_alg_hmac() {
    # full plus MBEDTLS_PSA_CRYPTO_CONFIG with PSA_WANT_ALG_HMAC
    msg "build: full + MBEDTLS_PSA_CRYPTO_CONFIG + PSA_WANT_ALG_HMAC"
    scripts/config.py full
    scripts/config.py set MBEDTLS_PSA_CRYPTO_CONFIG
    scripts/config.py set MBEDTLS_PSA_CRYPTO_DRIVERS
    scripts/config.py unset MBEDTLS_USE_PSA_CRYPTO
    scripts/config.py unset MBEDTLS_SSL_PROTO_TLS1_3
    # Need to define the correct symbol and include the test driver header path in order to build with the test driver
    make CC=gcc CFLAGS="$ASAN_CFLAGS -DPSA_CRYPTO_DRIVER_TEST -DMBEDTLS_PSA_ACCEL_ALG_HMAC -I../tests/include -O2" LDFLAGS="$ASAN_CFLAGS"
}

# This should be renamed to test and updated once the accelerator HKDF code is in place and ready to test.
component_build_psa_accel_alg_hkdf() {
    # full plus MBEDTLS_PSA_CRYPTO_CONFIG with PSA_WANT_ALG_HKDF
    # without MBEDTLS_HKDF_C
    msg "build: full + MBEDTLS_PSA_CRYPTO_CONFIG + PSA_WANT_ALG_HKDF without MBEDTLS_HKDF_C"
    scripts/config.py full
    scripts/config.py set MBEDTLS_PSA_CRYPTO_CONFIG
    scripts/config.py set MBEDTLS_PSA_CRYPTO_DRIVERS
    scripts/config.py unset MBEDTLS_USE_PSA_CRYPTO
    scripts/config.py unset MBEDTLS_SSL_PROTO_TLS1_3
    scripts/config.py unset MBEDTLS_HKDF_C
    # Make sure to unset TLS1_3 since it requires HKDF_C and will not build properly without it.
    scripts/config.py unset MBEDTLS_SSL_PROTO_TLS1_3
    # Need to define the correct symbol and include the test driver header path in order to build with the test driver
    make CC=gcc CFLAGS="$ASAN_CFLAGS -DPSA_CRYPTO_DRIVER_TEST -DMBEDTLS_PSA_ACCEL_ALG_HKDF -I../tests/include -O2" LDFLAGS="$ASAN_CFLAGS"
}

# This should be renamed to test and updated once the accelerator MD5 code is in place and ready to test.
component_build_psa_accel_alg_md5() {
    # full plus MBEDTLS_PSA_CRYPTO_CONFIG with PSA_WANT_ALG_MD5 without other hashes
    msg "build: full + MBEDTLS_PSA_CRYPTO_CONFIG + PSA_WANT_ALG_MD5 - other hashes"
    scripts/config.py full
    scripts/config.py set MBEDTLS_PSA_CRYPTO_CONFIG
    scripts/config.py set MBEDTLS_PSA_CRYPTO_DRIVERS
    scripts/config.py unset MBEDTLS_USE_PSA_CRYPTO
    scripts/config.py unset MBEDTLS_SSL_PROTO_TLS1_3
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_RIPEMD160
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_SHA_1
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_SHA_224
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_SHA_256
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_SHA_384
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_SHA_512
    # Need to define the correct symbol and include the test driver header path in order to build with the test driver
    make CC=gcc CFLAGS="$ASAN_CFLAGS -DPSA_CRYPTO_DRIVER_TEST -DMBEDTLS_PSA_ACCEL_ALG_MD5 -I../tests/include -O2" LDFLAGS="$ASAN_CFLAGS"
}

# This should be renamed to test and updated once the accelerator RIPEMD160 code is in place and ready to test.
component_build_psa_accel_alg_ripemd160() {
    # full plus MBEDTLS_PSA_CRYPTO_CONFIG with PSA_WANT_ALG_RIPEMD160 without other hashes
    msg "build: full + MBEDTLS_PSA_CRYPTO_CONFIG + PSA_WANT_ALG_RIPEMD160 - other hashes"
    scripts/config.py full
    scripts/config.py set MBEDTLS_PSA_CRYPTO_CONFIG
    scripts/config.py set MBEDTLS_PSA_CRYPTO_DRIVERS
    scripts/config.py unset MBEDTLS_USE_PSA_CRYPTO
    scripts/config.py unset MBEDTLS_SSL_PROTO_TLS1_3
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_MD5
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_SHA_1
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_SHA_224
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_SHA_256
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_SHA_384
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_SHA_512
    # Need to define the correct symbol and include the test driver header path in order to build with the test driver
    make CC=gcc CFLAGS="$ASAN_CFLAGS -DPSA_CRYPTO_DRIVER_TEST -DMBEDTLS_PSA_ACCEL_ALG_RIPEMD160 -I../tests/include -O2" LDFLAGS="$ASAN_CFLAGS"
}

# This should be renamed to test and updated once the accelerator SHA1 code is in place and ready to test.
component_build_psa_accel_alg_sha1() {
    # full plus MBEDTLS_PSA_CRYPTO_CONFIG with PSA_WANT_ALG_SHA_1 without other hashes
    msg "build: full + MBEDTLS_PSA_CRYPTO_CONFIG + PSA_WANT_ALG_SHA_1 - other hashes"
    scripts/config.py full
    scripts/config.py set MBEDTLS_PSA_CRYPTO_CONFIG
    scripts/config.py set MBEDTLS_PSA_CRYPTO_DRIVERS
    scripts/config.py unset MBEDTLS_USE_PSA_CRYPTO
    scripts/config.py unset MBEDTLS_SSL_PROTO_TLS1_3
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_MD5
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_RIPEMD160
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_SHA_224
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_SHA_256
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_SHA_384
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_SHA_512
    # Need to define the correct symbol and include the test driver header path in order to build with the test driver
    make CC=gcc CFLAGS="$ASAN_CFLAGS -DPSA_CRYPTO_DRIVER_TEST -DMBEDTLS_PSA_ACCEL_ALG_SHA_1 -I../tests/include -O2" LDFLAGS="$ASAN_CFLAGS"
}

# This should be renamed to test and updated once the accelerator SHA224 code is in place and ready to test.
component_build_psa_accel_alg_sha224() {
    # full plus MBEDTLS_PSA_CRYPTO_CONFIG with PSA_WANT_ALG_SHA_224 without other hashes
    msg "build: full + MBEDTLS_PSA_CRYPTO_CONFIG + PSA_WANT_ALG_SHA_224 - other hashes"
    scripts/config.py full
    scripts/config.py set MBEDTLS_PSA_CRYPTO_CONFIG
    scripts/config.py set MBEDTLS_PSA_CRYPTO_DRIVERS
    scripts/config.py unset MBEDTLS_USE_PSA_CRYPTO
    scripts/config.py unset MBEDTLS_SSL_PROTO_TLS1_3
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_MD5
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_RIPEMD160
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_SHA_1
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_SHA_384
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_SHA_512
    # Need to define the correct symbol and include the test driver header path in order to build with the test driver
    make CC=gcc CFLAGS="$ASAN_CFLAGS -DPSA_CRYPTO_DRIVER_TEST -DMBEDTLS_PSA_ACCEL_ALG_SHA_224 -I../tests/include -O2" LDFLAGS="$ASAN_CFLAGS"
}

# This should be renamed to test and updated once the accelerator SHA256 code is in place and ready to test.
component_build_psa_accel_alg_sha256() {
    # full plus MBEDTLS_PSA_CRYPTO_CONFIG with PSA_WANT_ALG_SHA_256 without other hashes
    msg "build: full + MBEDTLS_PSA_CRYPTO_CONFIG + PSA_WANT_ALG_SHA_256 - other hashes"
    scripts/config.py full
    scripts/config.py set MBEDTLS_PSA_CRYPTO_CONFIG
    scripts/config.py set MBEDTLS_PSA_CRYPTO_DRIVERS
    scripts/config.py unset MBEDTLS_USE_PSA_CRYPTO
    scripts/config.py unset MBEDTLS_SSL_PROTO_TLS1_3
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_MD5
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_RIPEMD160
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_SHA_1
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_SHA_224
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_SHA_384
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_SHA_512
    # Need to define the correct symbol and include the test driver header path in order to build with the test driver
    make CC=gcc CFLAGS="$ASAN_CFLAGS -DPSA_CRYPTO_DRIVER_TEST -DMBEDTLS_PSA_ACCEL_ALG_SHA_256 -I../tests/include -O2" LDFLAGS="$ASAN_CFLAGS"
}

# This should be renamed to test and updated once the accelerator SHA384 code is in place and ready to test.
component_build_psa_accel_alg_sha384() {
    # full plus MBEDTLS_PSA_CRYPTO_CONFIG with PSA_WANT_ALG_SHA_384 without other hashes
    msg "build: full + MBEDTLS_PSA_CRYPTO_CONFIG + PSA_WANT_ALG_SHA_384 - other hashes"
    scripts/config.py full
    scripts/config.py set MBEDTLS_PSA_CRYPTO_CONFIG
    scripts/config.py set MBEDTLS_PSA_CRYPTO_DRIVERS
    scripts/config.py unset MBEDTLS_USE_PSA_CRYPTO
    scripts/config.py unset MBEDTLS_SSL_PROTO_TLS1_3
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_MD5
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_RIPEMD160
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_SHA_1
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_SHA_224
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_SHA_256
    # Need to define the correct symbol and include the test driver header path in order to build with the test driver
    make CC=gcc CFLAGS="$ASAN_CFLAGS -DPSA_CRYPTO_DRIVER_TEST -DMBEDTLS_PSA_ACCEL_ALG_SHA_384 -I../tests/include -O2" LDFLAGS="$ASAN_CFLAGS"
}

# This should be renamed to test and updated once the accelerator SHA512 code is in place and ready to test.
component_build_psa_accel_alg_sha512() {
    # full plus MBEDTLS_PSA_CRYPTO_CONFIG with PSA_WANT_ALG_SHA_512 without other hashes
    msg "build: full + MBEDTLS_PSA_CRYPTO_CONFIG + PSA_WANT_ALG_SHA_512 - other hashes"
    scripts/config.py full
    scripts/config.py set MBEDTLS_PSA_CRYPTO_CONFIG
    scripts/config.py set MBEDTLS_PSA_CRYPTO_DRIVERS
    scripts/config.py unset MBEDTLS_USE_PSA_CRYPTO
    scripts/config.py unset MBEDTLS_SSL_PROTO_TLS1_3
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_MD5
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_RIPEMD160
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_SHA_1
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_SHA_224
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_SHA_256
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_SHA_384
    # Need to define the correct symbol and include the test driver header path in order to build with the test driver
    make CC=gcc CFLAGS="$ASAN_CFLAGS -DPSA_CRYPTO_DRIVER_TEST -DMBEDTLS_PSA_ACCEL_ALG_SHA_512 -I../tests/include -O2" LDFLAGS="$ASAN_CFLAGS"
}

# This should be renamed to test and updated once the accelerator RSA code is in place and ready to test.
component_build_psa_accel_alg_rsa_pkcs1v15_crypt() {
    # full plus MBEDTLS_PSA_CRYPTO_CONFIG with PSA_WANT_ALG_RSA_PKCS1V15_CRYPT
    msg "build: full + MBEDTLS_PSA_CRYPTO_CONFIG + PSA_WANT_ALG_RSA_PKCS1V15_CRYPT + PSA_WANT_KEY_TYPE_RSA_PUBLIC_KEY"
    scripts/config.py full
    scripts/config.py set MBEDTLS_PSA_CRYPTO_CONFIG
    scripts/config.py set MBEDTLS_PSA_CRYPTO_DRIVERS
    scripts/config.py unset MBEDTLS_USE_PSA_CRYPTO
    scripts/config.py unset MBEDTLS_SSL_PROTO_TLS1_3
    scripts/config.py -f include/psa/crypto_config.h set PSA_WANT_ALG_RSA_PKCS1V15_CRYPT 1
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_RSA_PKCS1V15_SIGN
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_RSA_OAEP
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_RSA_PSS
    # Need to define the correct symbol and include the test driver header path in order to build with the test driver
    make CC=gcc CFLAGS="$ASAN_CFLAGS -DPSA_CRYPTO_DRIVER_TEST -DMBEDTLS_PSA_ACCEL_ALG_RSA_PKCS1V15_CRYPT -I../tests/include -O2" LDFLAGS="$ASAN_CFLAGS"
}

# This should be renamed to test and updated once the accelerator RSA code is in place and ready to test.
component_build_psa_accel_alg_rsa_pkcs1v15_sign() {
    # full plus MBEDTLS_PSA_CRYPTO_CONFIG with PSA_WANT_ALG_RSA_PKCS1V15_SIGN and PSA_WANT_KEY_TYPE_RSA_PUBLIC_KEY
    msg "build: full + MBEDTLS_PSA_CRYPTO_CONFIG + PSA_WANT_ALG_RSA_PKCS1V15_SIGN + PSA_WANT_KEY_TYPE_RSA_PUBLIC_KEY"
    scripts/config.py full
    scripts/config.py set MBEDTLS_PSA_CRYPTO_CONFIG
    scripts/config.py set MBEDTLS_PSA_CRYPTO_DRIVERS
    scripts/config.py unset MBEDTLS_USE_PSA_CRYPTO
    scripts/config.py unset MBEDTLS_SSL_PROTO_TLS1_3
    scripts/config.py -f include/psa/crypto_config.h set PSA_WANT_ALG_RSA_PKCS1V15_SIGN 1
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_RSA_PKCS1V15_CRYPT
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_RSA_OAEP
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_RSA_PSS
    # Need to define the correct symbol and include the test driver header path in order to build with the test driver
    make CC=gcc CFLAGS="$ASAN_CFLAGS -DPSA_CRYPTO_DRIVER_TEST -DMBEDTLS_PSA_ACCEL_ALG_RSA_PKCS1V15_SIGN -I../tests/include -O2" LDFLAGS="$ASAN_CFLAGS"
}

# This should be renamed to test and updated once the accelerator RSA code is in place and ready to test.
component_build_psa_accel_alg_rsa_oaep() {
    # full plus MBEDTLS_PSA_CRYPTO_CONFIG with PSA_WANT_ALG_RSA_OAEP and PSA_WANT_KEY_TYPE_RSA_PUBLIC_KEY
    msg "build: full + MBEDTLS_PSA_CRYPTO_CONFIG + PSA_WANT_ALG_RSA_OAEP + PSA_WANT_KEY_TYPE_RSA_PUBLIC_KEY"
    scripts/config.py full
    scripts/config.py set MBEDTLS_PSA_CRYPTO_CONFIG
    scripts/config.py set MBEDTLS_PSA_CRYPTO_DRIVERS
    scripts/config.py unset MBEDTLS_USE_PSA_CRYPTO
    scripts/config.py unset MBEDTLS_SSL_PROTO_TLS1_3
    scripts/config.py -f include/psa/crypto_config.h set PSA_WANT_ALG_RSA_OAEP 1
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_RSA_PKCS1V15_CRYPT
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_RSA_PKCS1V15_SIGN
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_RSA_PSS
    # Need to define the correct symbol and include the test driver header path in order to build with the test driver
    make CC=gcc CFLAGS="$ASAN_CFLAGS -DPSA_CRYPTO_DRIVER_TEST -DMBEDTLS_PSA_ACCEL_ALG_RSA_OAEP -I../tests/include -O2" LDFLAGS="$ASAN_CFLAGS"
}

# This should be renamed to test and updated once the accelerator RSA code is in place and ready to test.
component_build_psa_accel_alg_rsa_pss() {
    # full plus MBEDTLS_PSA_CRYPTO_CONFIG with PSA_WANT_ALG_RSA_PSS and PSA_WANT_KEY_TYPE_RSA_PUBLIC_KEY
    msg "build: full + MBEDTLS_PSA_CRYPTO_CONFIG + PSA_WANT_ALG_RSA_PSS + PSA_WANT_KEY_TYPE_RSA_PUBLIC_KEY"
    scripts/config.py full
    scripts/config.py set MBEDTLS_PSA_CRYPTO_CONFIG
    scripts/config.py set MBEDTLS_PSA_CRYPTO_DRIVERS
    scripts/config.py unset MBEDTLS_USE_PSA_CRYPTO
    scripts/config.py unset MBEDTLS_SSL_PROTO_TLS1_3
    scripts/config.py -f include/psa/crypto_config.h set PSA_WANT_ALG_RSA_PSS 1
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_RSA_PKCS1V15_CRYPT
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_RSA_PKCS1V15_SIGN
    scripts/config.py -f include/psa/crypto_config.h unset PSA_WANT_ALG_RSA_OAEP
    # Need to define the correct symbol and include the test driver header path in order to build with the test driver
    make CC=gcc CFLAGS="$ASAN_CFLAGS -DPSA_CRYPTO_DRIVER_TEST -DMBEDTLS_PSA_ACCEL_ALG_RSA_PSS -I../tests/include -O2" LDFLAGS="$ASAN_CFLAGS"
}

# This should be renamed to test and updated once the accelerator RSA code is in place and ready to test.
component_build_psa_accel_key_type_rsa_key_pair() {
    # full plus MBEDTLS_PSA_CRYPTO_CONFIG with PSA_WANT_KEY_TYPE_RSA_KEY_PAIR and PSA_WANT_ALG_RSA_PSS
    msg "build: full + MBEDTLS_PSA_CRYPTO_CONFIG + PSA_WANT_KEY_TYPE_RSA_KEY_PAIR + PSA_WANT_ALG_RSA_PSS"
    scripts/config.py full
    scripts/config.py set MBEDTLS_PSA_CRYPTO_CONFIG
    scripts/config.py set MBEDTLS_PSA_CRYPTO_DRIVERS
    scripts/config.py unset MBEDTLS_USE_PSA_CRYPTO
    scripts/config.py unset MBEDTLS_SSL_PROTO_TLS1_3
    scripts/config.py -f include/psa/crypto_config.h set PSA_WANT_ALG_RSA_PSS 1
    scripts/config.py -f include/psa/crypto_config.h set PSA_WANT_KEY_TYPE_RSA_KEY_PAIR 1
    # Need to define the correct symbol and include the test driver header path in order to build with the test driver
    make CC=gcc CFLAGS="$ASAN_CFLAGS -DPSA_CRYPTO_DRIVER_TEST -DMBEDTLS_PSA_ACCEL_KEY_TYPE_RSA_KEY_PAIR -I../tests/include -O2" LDFLAGS="$ASAN_CFLAGS"
}

# This should be renamed to test and updated once the accelerator RSA code is in place and ready to test.
component_build_psa_accel_key_type_rsa_public_key() {
    # full plus MBEDTLS_PSA_CRYPTO_CONFIG with PSA_WANT_KEY_TYPE_RSA_PUBLIC_KEY and PSA_WANT_ALG_RSA_PSS
    msg "build: full + MBEDTLS_PSA_CRYPTO_CONFIG + PSA_WANT_KEY_TYPE_RSA_PUBLIC_KEY + PSA_WANT_ALG_RSA_PSS"
    scripts/config.py full
    scripts/config.py set MBEDTLS_PSA_CRYPTO_CONFIG
    scripts/config.py set MBEDTLS_PSA_CRYPTO_DRIVERS
    scripts/config.py unset MBEDTLS_USE_PSA_CRYPTO
    scripts/config.py unset MBEDTLS_SSL_PROTO_TLS1_3
    scripts/config.py -f include/psa/crypto_config.h set PSA_WANT_ALG_RSA_PSS 1
    scripts/config.py -f include/psa/crypto_config.h set PSA_WANT_KEY_TYPE_RSA_PUBLIC_KEY 1
    # Need to define the correct symbol and include the test driver header path in order to build with the test driver
    make CC=gcc CFLAGS="$ASAN_CFLAGS -DPSA_CRYPTO_DRIVER_TEST -DMBEDTLS_PSA_ACCEL_KEY_TYPE_RSA_PUBLIC_KEY -I../tests/include -O2" LDFLAGS="$ASAN_CFLAGS"
}

component_test_no_platform () {
    # Full configuration build, without platform support, file IO and net sockets.
    # This should catch missing mbedtls_printf definitions, and by disabling file
    # IO, it should catch missing '#include <stdio.h>'
    msg "build: full config except platform/fsio/net, make, gcc, C99" # ~ 30s
    scripts/config.py full
    scripts/config.py unset MBEDTLS_PLATFORM_C
    scripts/config.py unset MBEDTLS_NET_C
    scripts/config.py unset MBEDTLS_PLATFORM_MEMORY
    scripts/config.py unset MBEDTLS_PLATFORM_PRINTF_ALT
    scripts/config.py unset MBEDTLS_PLATFORM_FPRINTF_ALT
    scripts/config.py unset MBEDTLS_PLATFORM_SNPRINTF_ALT
    scripts/config.py unset MBEDTLS_PLATFORM_TIME_ALT
    scripts/config.py unset MBEDTLS_PLATFORM_EXIT_ALT
    scripts/config.py unset MBEDTLS_PLATFORM_SETBUF_ALT
    scripts/config.py unset MBEDTLS_PLATFORM_NV_SEED_ALT
    scripts/config.py unset MBEDTLS_ENTROPY_NV_SEED
    scripts/config.py unset MBEDTLS_FS_IO
    scripts/config.py unset MBEDTLS_PSA_CRYPTO_SE_C
    scripts/config.py unset MBEDTLS_PSA_CRYPTO_STORAGE_C
    scripts/config.py unset MBEDTLS_PSA_ITS_FILE_C
    # Note, _DEFAULT_SOURCE needs to be defined for platforms using glibc version >2.19,
    # to re-enable platform integration features otherwise disabled in C99 builds
    make CC=gcc CFLAGS='-Werror -Wall -Wextra -std=c99 -pedantic -Os -D_DEFAULT_SOURCE' lib programs
    make CC=gcc CFLAGS='-Werror -Wall -Wextra -Os' test
}

component_build_no_std_function () {
    # catch compile bugs in _uninit functions
    msg "build: full config with NO_STD_FUNCTION, make, gcc" # ~ 30s
    scripts/config.py full
    scripts/config.py set MBEDTLS_PLATFORM_NO_STD_FUNCTIONS
    scripts/config.py unset MBEDTLS_ENTROPY_NV_SEED
    scripts/config.py unset MBEDTLS_PLATFORM_NV_SEED_ALT
    CC=gcc cmake -D CMAKE_BUILD_TYPE:String=Check .
    make
}

component_build_no_ssl_srv () {
    msg "build: full config except SSL server, make, gcc" # ~ 30s
    scripts/config.py full
    scripts/config.py unset MBEDTLS_SSL_SRV_C
    make CC=gcc CFLAGS='-Werror -Wall -Wextra -O1'
}

component_build_no_ssl_cli () {
    msg "build: full config except SSL client, make, gcc" # ~ 30s
    scripts/config.py full
    scripts/config.py unset MBEDTLS_SSL_CLI_C
    make CC=gcc CFLAGS='-Werror -Wall -Wextra -O1'
}

component_build_no_sockets () {
    # Note, C99 compliance can also be tested with the sockets support disabled,
    # as that requires a POSIX platform (which isn't the same as C99).
    msg "build: full config except net_sockets.c, make, gcc -std=c99 -pedantic" # ~ 30s
    scripts/config.py full
    scripts/config.py unset MBEDTLS_NET_C # getaddrinfo() undeclared, etc.
    scripts/config.py set MBEDTLS_NO_PLATFORM_ENTROPY # uses syscall() on GNU/Linux
    make CC=gcc CFLAGS='-Werror -Wall -Wextra -O1 -std=c99 -pedantic' lib
}

component_test_memory_buffer_allocator_backtrace () {
    msg "build: default config with memory buffer allocator and backtrace enabled"
    scripts/config.py set MBEDTLS_MEMORY_BUFFER_ALLOC_C
    scripts/config.py set MBEDTLS_PLATFORM_MEMORY
    scripts/config.py set MBEDTLS_MEMORY_BACKTRACE
    scripts/config.py set MBEDTLS_MEMORY_DEBUG
    CC=gcc cmake -DCMAKE_BUILD_TYPE:String=Release .
    make

    msg "test: MBEDTLS_MEMORY_BUFFER_ALLOC_C and MBEDTLS_MEMORY_BACKTRACE"
    make test
}

component_test_memory_buffer_allocator () {
    msg "build: default config with memory buffer allocator"
    scripts/config.py set MBEDTLS_MEMORY_BUFFER_ALLOC_C
    scripts/config.py set MBEDTLS_PLATFORM_MEMORY
    CC=gcc cmake -DCMAKE_BUILD_TYPE:String=Release .
    make

    msg "test: MBEDTLS_MEMORY_BUFFER_ALLOC_C"
    make test

    msg "test: ssl-opt.sh, MBEDTLS_MEMORY_BUFFER_ALLOC_C"
    # MBEDTLS_MEMORY_BUFFER_ALLOC is slow. Skip tests that tend to time out.
    tests/ssl-opt.sh -e '^DTLS proxy'
}

component_test_no_max_fragment_length () {
    # Run max fragment length tests with MFL disabled
    msg "build: default config except MFL extension (ASan build)" # ~ 30s
    scripts/config.py unset MBEDTLS_SSL_MAX_FRAGMENT_LENGTH
    CC=gcc cmake -D CMAKE_BUILD_TYPE:String=Asan .
    make

    msg "test: ssl-opt.sh, MFL-related tests"
    tests/ssl-opt.sh -f "Max fragment length"
}

component_test_asan_remove_peer_certificate () {
    msg "build: default config with MBEDTLS_SSL_KEEP_PEER_CERTIFICATE disabled (ASan build)"
    scripts/config.py unset MBEDTLS_SSL_KEEP_PEER_CERTIFICATE
    CC=gcc cmake -D CMAKE_BUILD_TYPE:String=Asan .
    make

    msg "test: !MBEDTLS_SSL_KEEP_PEER_CERTIFICATE"
    make test

    msg "test: ssl-opt.sh, !MBEDTLS_SSL_KEEP_PEER_CERTIFICATE"
    tests/ssl-opt.sh

    msg "test: compat.sh, !MBEDTLS_SSL_KEEP_PEER_CERTIFICATE"
    tests/compat.sh

    msg "test: context-info.sh, !MBEDTLS_SSL_KEEP_PEER_CERTIFICATE"
    tests/context-info.sh
}

component_test_no_max_fragment_length_small_ssl_out_content_len () {
    msg "build: no MFL extension, small SSL_OUT_CONTENT_LEN (ASan build)"
    scripts/config.py unset MBEDTLS_SSL_MAX_FRAGMENT_LENGTH
    scripts/config.py set MBEDTLS_SSL_IN_CONTENT_LEN 16384
    scripts/config.py set MBEDTLS_SSL_OUT_CONTENT_LEN 4096
    CC=gcc cmake -D CMAKE_BUILD_TYPE:String=Asan .
    make

    msg "test: MFL tests (disabled MFL extension case) & large packet tests"
    tests/ssl-opt.sh -f "Max fragment length\|Large buffer"

    msg "test: context-info.sh (disabled MFL extension case)"
    tests/context-info.sh
}

component_test_variable_ssl_in_out_buffer_len () {
    msg "build: MBEDTLS_SSL_VARIABLE_BUFFER_LENGTH enabled (ASan build)"
    scripts/config.py set MBEDTLS_SSL_VARIABLE_BUFFER_LENGTH
    CC=gcc cmake -D CMAKE_BUILD_TYPE:String=Asan .
    make

    msg "test: MBEDTLS_SSL_VARIABLE_BUFFER_LENGTH enabled"
    make test

    msg "test: ssl-opt.sh, MBEDTLS_SSL_VARIABLE_BUFFER_LENGTH enabled"
    tests/ssl-opt.sh

    msg "test: compat.sh, MBEDTLS_SSL_VARIABLE_BUFFER_LENGTH enabled"
    tests/compat.sh
}

component_test_variable_ssl_in_out_buffer_len_CID () {
    msg "build: MBEDTLS_SSL_VARIABLE_BUFFER_LENGTH and MBEDTLS_SSL_DTLS_CONNECTION_ID enabled (ASan build)"
    scripts/config.py set MBEDTLS_SSL_VARIABLE_BUFFER_LENGTH
    scripts/config.py set MBEDTLS_SSL_DTLS_CONNECTION_ID

    CC=gcc cmake -D CMAKE_BUILD_TYPE:String=Asan .
    make

    msg "test: MBEDTLS_SSL_VARIABLE_BUFFER_LENGTH and MBEDTLS_SSL_DTLS_CONNECTION_ID"
    make test

    msg "test: ssl-opt.sh, MBEDTLS_SSL_VARIABLE_BUFFER_LENGTH and MBEDTLS_SSL_DTLS_CONNECTION_ID enabled"
    tests/ssl-opt.sh

    msg "test: compat.sh, MBEDTLS_SSL_VARIABLE_BUFFER_LENGTH and MBEDTLS_SSL_DTLS_CONNECTION_ID enabled"
    tests/compat.sh
}

component_test_ssl_alloc_buffer_and_mfl () {
    msg "build: default config with memory buffer allocator and MFL extension"
    scripts/config.py set MBEDTLS_MEMORY_BUFFER_ALLOC_C
    scripts/config.py set MBEDTLS_PLATFORM_MEMORY
    scripts/config.py set MBEDTLS_MEMORY_DEBUG
    scripts/config.py set MBEDTLS_SSL_MAX_FRAGMENT_LENGTH
    scripts/config.py set MBEDTLS_SSL_VARIABLE_BUFFER_LENGTH
    CC=gcc cmake -DCMAKE_BUILD_TYPE:String=Release .
    make

    msg "test: MBEDTLS_SSL_VARIABLE_BUFFER_LENGTH, MBEDTLS_MEMORY_BUFFER_ALLOC_C, MBEDTLS_MEMORY_DEBUG and MBEDTLS_SSL_MAX_FRAGMENT_LENGTH"
    make test

    msg "test: MBEDTLS_SSL_VARIABLE_BUFFER_LENGTH, MBEDTLS_MEMORY_BUFFER_ALLOC_C, MBEDTLS_MEMORY_DEBUG and MBEDTLS_SSL_MAX_FRAGMENT_LENGTH"
    tests/ssl-opt.sh -f "Handshake memory usage"
}

component_test_when_no_ciphersuites_have_mac () {
    msg "build: when no ciphersuites have MAC"
    scripts/config.py unset MBEDTLS_CIPHER_NULL_CIPHER
    scripts/config.py unset MBEDTLS_CIPHER_MODE_CBC
    scripts/config.py unset MBEDTLS_CMAC_C
    make

    msg "test: !MBEDTLS_SSL_SOME_MODES_USE_MAC"
    make test

    msg "test ssl-opt.sh: !MBEDTLS_SSL_SOME_MODES_USE_MAC"
    tests/ssl-opt.sh -f 'Default\|EtM' -e 'without EtM'
}

component_test_no_date_time () {
    msg "build: default config without MBEDTLS_HAVE_TIME_DATE"
    scripts/config.py unset MBEDTLS_HAVE_TIME_DATE
    CC=gcc cmake -D CMAKE_BUILD_TYPE:String=Check .
    make

    msg "test: !MBEDTLS_HAVE_TIME_DATE - main suites"
    make test
}

component_test_platform_calloc_macro () {
    msg "build: MBEDTLS_PLATFORM_{CALLOC/FREE}_MACRO enabled (ASan build)"
    scripts/config.py set MBEDTLS_PLATFORM_MEMORY
    scripts/config.py set MBEDTLS_PLATFORM_CALLOC_MACRO calloc
    scripts/config.py set MBEDTLS_PLATFORM_FREE_MACRO   free
    CC=gcc cmake -D CMAKE_BUILD_TYPE:String=Asan .
    make

    msg "test: MBEDTLS_PLATFORM_{CALLOC/FREE}_MACRO enabled (ASan build)"
    make test
}

component_test_malloc_0_null () {
    msg "build: malloc(0) returns NULL (ASan+UBSan build)"
    scripts/config.py full
    make CC=gcc CFLAGS="'-DMBEDTLS_CONFIG_FILE=\"$PWD/tests/configs/config-wrapper-malloc-0-null.h\"' $ASAN_CFLAGS -O" LDFLAGS="$ASAN_CFLAGS"

    msg "test: malloc(0) returns NULL (ASan+UBSan build)"
    make test

    msg "selftest: malloc(0) returns NULL (ASan+UBSan build)"
    # Just the calloc selftest. "make test" ran the others as part of the
    # test suites.
    programs/test/selftest calloc

    msg "test ssl-opt.sh: malloc(0) returns NULL (ASan+UBSan build)"
    # Run a subset of the tests. The choice is a balance between coverage
    # and time (including time indirectly wasted due to flaky tests).
    # The current choice is to skip tests whose description includes
    # "proxy", which is an approximation of skipping tests that use the
    # UDP proxy, which tend to be slower and flakier.
    tests/ssl-opt.sh -e 'proxy'
}

component_test_aes_fewer_tables () {
    msg "build: default config with AES_FEWER_TABLES enabled"
    scripts/config.py set MBEDTLS_AES_FEWER_TABLES
    make CC=gcc CFLAGS='-Werror -Wall -Wextra'

    msg "test: AES_FEWER_TABLES"
    make test
}

component_test_aes_rom_tables () {
    msg "build: default config with AES_ROM_TABLES enabled"
    scripts/config.py set MBEDTLS_AES_ROM_TABLES
    make CC=gcc CFLAGS='-Werror -Wall -Wextra'

    msg "test: AES_ROM_TABLES"
    make test
}

component_test_aes_fewer_tables_and_rom_tables () {
    msg "build: default config with AES_ROM_TABLES and AES_FEWER_TABLES enabled"
    scripts/config.py set MBEDTLS_AES_FEWER_TABLES
    scripts/config.py set MBEDTLS_AES_ROM_TABLES
    make CC=gcc CFLAGS='-Werror -Wall -Wextra'

    msg "test: AES_FEWER_TABLES + AES_ROM_TABLES"
    make test
}

component_test_ctr_drbg_aes_256_sha_256 () {
    msg "build: full + MBEDTLS_ENTROPY_FORCE_SHA256 (ASan build)"
    scripts/config.py full
    scripts/config.py unset MBEDTLS_MEMORY_BUFFER_ALLOC_C
    scripts/config.py set MBEDTLS_ENTROPY_FORCE_SHA256
    CC=gcc cmake -D CMAKE_BUILD_TYPE:String=Asan .
    make

    msg "test: full + MBEDTLS_ENTROPY_FORCE_SHA256 (ASan build)"
    make test
}

component_test_ctr_drbg_aes_128_sha_512 () {
    msg "build: full + MBEDTLS_CTR_DRBG_USE_128_BIT_KEY (ASan build)"
    scripts/config.py full
    scripts/config.py unset MBEDTLS_MEMORY_BUFFER_ALLOC_C
    scripts/config.py set MBEDTLS_CTR_DRBG_USE_128_BIT_KEY
    CC=gcc cmake -D CMAKE_BUILD_TYPE:String=Asan .
    make

    msg "test: full + MBEDTLS_CTR_DRBG_USE_128_BIT_KEY (ASan build)"
    make test
}

component_test_ctr_drbg_aes_128_sha_256 () {
    msg "build: full + MBEDTLS_CTR_DRBG_USE_128_BIT_KEY + MBEDTLS_ENTROPY_FORCE_SHA256 (ASan build)"
    scripts/config.py full
    scripts/config.py unset MBEDTLS_MEMORY_BUFFER_ALLOC_C
    scripts/config.py set MBEDTLS_CTR_DRBG_USE_128_BIT_KEY
    scripts/config.py set MBEDTLS_ENTROPY_FORCE_SHA256
    CC=gcc cmake -D CMAKE_BUILD_TYPE:String=Asan .
    make

    msg "test: full + MBEDTLS_CTR_DRBG_USE_128_BIT_KEY + MBEDTLS_ENTROPY_FORCE_SHA256 (ASan build)"
    make test
}

component_test_se_default () {
    msg "build: default config + MBEDTLS_PSA_CRYPTO_SE_C"
    scripts/config.py set MBEDTLS_PSA_CRYPTO_SE_C
    make CC=clang CFLAGS="$ASAN_CFLAGS -Os" LDFLAGS="$ASAN_CFLAGS"

    msg "test: default config + MBEDTLS_PSA_CRYPTO_SE_C"
    make test
}

component_test_psa_crypto_drivers () {
    msg "build: MBEDTLS_PSA_CRYPTO_DRIVERS w/ driver hooks"
    scripts/config.py full
    scripts/config.py set MBEDTLS_PSA_CRYPTO_DRIVERS
    scripts/config.py set MBEDTLS_PSA_CRYPTO_BUILTIN_KEYS
    loc_cflags="$ASAN_CFLAGS -DPSA_CRYPTO_DRIVER_TEST_ALL"
    loc_cflags="${loc_cflags} '-DMBEDTLS_USER_CONFIG_FILE=\"../tests/configs/user-config-for-test.h\"'"
    loc_cflags="${loc_cflags} -I../tests/include -O2"

    make CC=gcc CFLAGS="${loc_cflags}" LDFLAGS="$ASAN_CFLAGS"

    msg "test: full + MBEDTLS_PSA_CRYPTO_DRIVERS"
    make test
}

component_test_make_shared () {
    msg "build/test: make shared" # ~ 40s
    make SHARED=1 all check
    ldd programs/util/strerror | grep libmbedcrypto
    programs/test/dlopen_demo.sh
}

component_test_cmake_shared () {
    msg "build/test: cmake shared" # ~ 2min
    cmake -DUSE_SHARED_MBEDTLS_LIBRARY=On .
    make
    ldd programs/util/strerror | grep libmbedcrypto
    make test
    programs/test/dlopen_demo.sh
}

test_build_opt () {
    info=$1 cc=$2; shift 2
    for opt in "$@"; do
          msg "build/test: $cc $opt, $info" # ~ 30s
          make CC="$cc" CFLAGS="$opt -std=c99 -pedantic -Wall -Wextra -Werror"
          # We're confident enough in compilers to not run _all_ the tests,
          # but at least run the unit tests. In particular, runs with
          # optimizations use inline assembly whereas runs with -O0
          # skip inline assembly.
          make test # ~30s
          make clean
    done
}

component_test_clang_opt () {
    scripts/config.py full
    test_build_opt 'full config' clang -O0 -Os -O2
}

component_test_gcc_opt () {
    scripts/config.py full
    test_build_opt 'full config' gcc -O0 -Os -O2
}

component_build_mbedtls_config_file () {
    msg "build: make with MBEDTLS_CONFIG_FILE" # ~40s
    scripts/config.py -w full_config.h full
    echo '#error "MBEDTLS_CONFIG_FILE is not working"' >"$CONFIG_H"
    make CFLAGS="-I '$PWD' -DMBEDTLS_CONFIG_FILE='\"full_config.h\"'"
    # Make sure this feature is enabled. We'll disable it in the next phase.
    programs/test/query_compile_time_config MBEDTLS_NIST_KW_C
    make clean

    msg "build: make with MBEDTLS_CONFIG_FILE + MBEDTLS_USER_CONFIG_FILE"
    # In the user config, disable one feature (for simplicity, pick a feature
    # that nothing else depends on).
    echo '#undef MBEDTLS_NIST_KW_C' >user_config.h
    make CFLAGS="-I '$PWD' -DMBEDTLS_CONFIG_FILE='\"full_config.h\"' -DMBEDTLS_USER_CONFIG_FILE='\"user_config.h\"'"
    not programs/test/query_compile_time_config MBEDTLS_NIST_KW_C

    rm -f user_config.h full_config.h
}

component_build_psa_config_file () {
    msg "build: make with MBEDTLS_PSA_CRYPTO_CONFIG_FILE" # ~40s
    scripts/config.py set MBEDTLS_PSA_CRYPTO_CONFIG
    cp "$CRYPTO_CONFIG_H" psa_test_config.h
    echo '#error "MBEDTLS_PSA_CRYPTO_CONFIG_FILE is not working"' >"$CRYPTO_CONFIG_H"
    make CFLAGS="-I '$PWD' -DMBEDTLS_PSA_CRYPTO_CONFIG_FILE='\"psa_test_config.h\"'"
    # Make sure this feature is enabled. We'll disable it in the next phase.
    programs/test/query_compile_time_config MBEDTLS_CMAC_C
    make clean

    msg "build: make with MBEDTLS_PSA_CRYPTO_CONFIG_FILE + MBEDTLS_PSA_CRYPTO_USER_CONFIG_FILE" # ~40s
    # In the user config, disable one feature, which will reflect on the
    # mbedtls configuration so we can query it with query_compile_time_config.
    echo '#undef PSA_WANT_ALG_CMAC' >psa_user_config.h
    scripts/config.py unset MBEDTLS_CMAC_C
    make CFLAGS="-I '$PWD' -DMBEDTLS_PSA_CRYPTO_CONFIG_FILE='\"psa_test_config.h\"' -DMBEDTLS_PSA_CRYPTO_USER_CONFIG_FILE='\"psa_user_config.h\"'"
    not programs/test/query_compile_time_config MBEDTLS_CMAC_C

    rm -f psa_test_config.h psa_user_config.h
}

component_test_m32_o0 () {
    # Build without optimization, so as to use portable C code (in a 32-bit
    # build) and not the i386-specific inline assembly.
    msg "build: i386, make, gcc -O0 (ASan build)" # ~ 30s
    scripts/config.py full
    make CC=gcc CFLAGS="$ASAN_CFLAGS -m32 -O0" LDFLAGS="-m32 $ASAN_CFLAGS"

    msg "test: i386, make, gcc -O0 (ASan build)"
    make test
}
support_test_m32_o0 () {
    case $(uname -m) in
        *64*) true;;
        *) false;;
    esac
}

component_test_m32_o2 () {
    # Build with optimization, to use the i386 specific inline assembly
    # and go faster for tests.
    msg "build: i386, make, gcc -O2 (ASan build)" # ~ 30s
    scripts/config.py full
    make CC=gcc CFLAGS="$ASAN_CFLAGS -m32 -O2" LDFLAGS="-m32 $ASAN_CFLAGS"

    msg "test: i386, make, gcc -O2 (ASan build)"
    make test

    msg "test ssl-opt.sh, i386, make, gcc-O2"
    tests/ssl-opt.sh
}
support_test_m32_o2 () {
    support_test_m32_o0 "$@"
}

component_test_m32_everest () {
    msg "build: i386, Everest ECDH context (ASan build)" # ~ 6 min
    scripts/config.py set MBEDTLS_ECDH_VARIANT_EVEREST_ENABLED
    make CC=gcc CFLAGS="$ASAN_CFLAGS -m32 -O2" LDFLAGS="-m32 $ASAN_CFLAGS"

    msg "test: i386, Everest ECDH context - main suites (inc. selftests) (ASan build)" # ~ 50s
    make test

    msg "test: i386, Everest ECDH context - ECDH-related part of ssl-opt.sh (ASan build)" # ~ 5s
    tests/ssl-opt.sh -f ECDH

    msg "test: i386, Everest ECDH context - compat.sh with some ECDH ciphersuites (ASan build)" # ~ 3 min
    # Exclude some symmetric ciphers that are redundant here to gain time.
    tests/compat.sh -f ECDH -V NO -e 'ARIA\|CAMELLIA\|CHACHA'
}
support_test_m32_everest () {
    support_test_m32_o0 "$@"
}

component_test_mx32 () {
    msg "build: 64-bit ILP32, make, gcc" # ~ 30s
    scripts/config.py full
    make CC=gcc CFLAGS='-Werror -Wall -Wextra -mx32' LDFLAGS='-mx32'

    msg "test: 64-bit ILP32, make, gcc"
    make test
}
support_test_mx32 () {
    case $(uname -m) in
        amd64|x86_64) true;;
        *) false;;
    esac
}

component_test_min_mpi_window_size () {
    msg "build: Default + MBEDTLS_MPI_WINDOW_SIZE=1 (ASan build)" # ~ 10s
    scripts/config.py set MBEDTLS_MPI_WINDOW_SIZE 1
    CC=gcc cmake -D CMAKE_BUILD_TYPE:String=Asan .
    make

    msg "test: MBEDTLS_MPI_WINDOW_SIZE=1 - main suites (inc. selftests) (ASan build)" # ~ 10s
    make test
}

component_test_have_int32 () {
    msg "build: gcc, force 32-bit bignum limbs"
    scripts/config.py unset MBEDTLS_HAVE_ASM
    scripts/config.py unset MBEDTLS_AESNI_C
    scripts/config.py unset MBEDTLS_PADLOCK_C
    make CC=gcc CFLAGS='-Werror -Wall -Wextra -DMBEDTLS_HAVE_INT32'

    msg "test: gcc, force 32-bit bignum limbs"
    make test
}

component_test_have_int64 () {
    msg "build: gcc, force 64-bit bignum limbs"
    scripts/config.py unset MBEDTLS_HAVE_ASM
    scripts/config.py unset MBEDTLS_AESNI_C
    scripts/config.py unset MBEDTLS_PADLOCK_C
    make CC=gcc CFLAGS='-Werror -Wall -Wextra -DMBEDTLS_HAVE_INT64'

    msg "test: gcc, force 64-bit bignum limbs"
    make test
}

component_test_no_udbl_division () {
    msg "build: MBEDTLS_NO_UDBL_DIVISION native" # ~ 10s
    scripts/config.py full
    scripts/config.py set MBEDTLS_NO_UDBL_DIVISION
    make CFLAGS='-Werror -O1'

    msg "test: MBEDTLS_NO_UDBL_DIVISION native" # ~ 10s
    make test
}

component_test_no_64bit_multiplication () {
    msg "build: MBEDTLS_NO_64BIT_MULTIPLICATION native" # ~ 10s
    scripts/config.py full
    scripts/config.py set MBEDTLS_NO_64BIT_MULTIPLICATION
    make CFLAGS='-Werror -O1'

    msg "test: MBEDTLS_NO_64BIT_MULTIPLICATION native" # ~ 10s
    make test
}

component_test_no_strings () {
    msg "build: no strings" # ~10s
    scripts/config.py full
    # Disable options that activate a large amount of string constants.
    scripts/config.py unset MBEDTLS_DEBUG_C
    scripts/config.py unset MBEDTLS_ERROR_C
    scripts/config.py set MBEDTLS_ERROR_STRERROR_DUMMY
    scripts/config.py unset MBEDTLS_VERSION_FEATURES
    make CFLAGS='-Werror -Os'

    msg "test: no strings" # ~ 10s
    make test
}

component_test_no_x509_info () {
    msg "build: full + MBEDTLS_X509_REMOVE_INFO" # ~ 10s
    scripts/config.pl full
    scripts/config.pl unset MBEDTLS_MEMORY_BACKTRACE # too slow for tests
    scripts/config.pl set MBEDTLS_X509_REMOVE_INFO
    make CFLAGS='-Werror -O2'

    msg "test: full + MBEDTLS_X509_REMOVE_INFO" # ~ 10s
    make test

    msg "test: ssl-opt.sh, full + MBEDTLS_X509_REMOVE_INFO" # ~ 1 min
    tests/ssl-opt.sh
}

component_build_arm_none_eabi_gcc () {
    msg "build: ${ARM_NONE_EABI_GCC_PREFIX}gcc -O1, baremetal+debug" # ~ 10s
    scripts/config.py baremetal
    make CC="${ARM_NONE_EABI_GCC_PREFIX}gcc" AR="${ARM_NONE_EABI_GCC_PREFIX}ar" LD="${ARM_NONE_EABI_GCC_PREFIX}ld" CFLAGS='-std=c99 -Werror -Wall -Wextra -O1' lib

    msg "size: ${ARM_NONE_EABI_GCC_PREFIX}gcc -O1, baremetal+debug"
    ${ARM_NONE_EABI_GCC_PREFIX}size library/*.o
}

component_build_arm_linux_gnueabi_gcc_arm5vte () {
    msg "build: ${ARM_LINUX_GNUEABI_GCC_PREFIX}gcc -march=arm5vte, baremetal+debug" # ~ 10s
    scripts/config.py baremetal
    # Build for a target platform that's close to what Debian uses
    # for its "armel" distribution (https://wiki.debian.org/ArmEabiPort).
    # See https://github.com/Mbed-TLS/mbedtls/pull/2169 and comments.
    # Build everything including programs, see for example
    # https://github.com/Mbed-TLS/mbedtls/pull/3449#issuecomment-675313720
    make CC="${ARM_LINUX_GNUEABI_GCC_PREFIX}gcc" AR="${ARM_LINUX_GNUEABI_GCC_PREFIX}ar" CFLAGS='-Werror -Wall -Wextra -march=armv5te -O1' LDFLAGS='-march=armv5te'

    msg "size: ${ARM_LINUX_GNUEABI_GCC_PREFIX}gcc -march=armv5te -O1, baremetal+debug"
    ${ARM_LINUX_GNUEABI_GCC_PREFIX}size library/*.o
}
support_build_arm_linux_gnueabi_gcc_arm5vte () {
    type ${ARM_LINUX_GNUEABI_GCC_PREFIX}gcc >/dev/null 2>&1
}

component_build_arm_none_eabi_gcc_arm5vte () {
    msg "build: ${ARM_NONE_EABI_GCC_PREFIX}gcc -march=arm5vte, baremetal+debug" # ~ 10s
    scripts/config.py baremetal
    # This is an imperfect substitute for
    # component_build_arm_linux_gnueabi_gcc_arm5vte
    # in case the gcc-arm-linux-gnueabi toolchain is not available
    make CC="${ARM_NONE_EABI_GCC_PREFIX}gcc" AR="${ARM_NONE_EABI_GCC_PREFIX}ar" CFLAGS='-std=c99 -Werror -Wall -Wextra -march=armv5te -O1' LDFLAGS='-march=armv5te' SHELL='sh -x' lib

    msg "size: ${ARM_NONE_EABI_GCC_PREFIX}gcc -march=armv5te -O1, baremetal+debug"
    ${ARM_NONE_EABI_GCC_PREFIX}size library/*.o
}

component_build_arm_none_eabi_gcc_m0plus () {
    msg "build: ${ARM_NONE_EABI_GCC_PREFIX}gcc -mthumb -mcpu=cortex-m0plus, baremetal_size" # ~ 10s
    scripts/config.py baremetal_size
    make CC="${ARM_NONE_EABI_GCC_PREFIX}gcc" AR="${ARM_NONE_EABI_GCC_PREFIX}ar" LD="${ARM_NONE_EABI_GCC_PREFIX}ld" CFLAGS='-std=c99 -Werror -Wall -Wextra -mthumb -mcpu=cortex-m0plus -Os' lib

    msg "size: ${ARM_NONE_EABI_GCC_PREFIX}gcc -mthumb -mcpu=cortex-m0plus -Os, baremetal_size"
    ${ARM_NONE_EABI_GCC_PREFIX}size library/*.o
}

component_build_arm_none_eabi_gcc_no_udbl_division () {
    msg "build: ${ARM_NONE_EABI_GCC_PREFIX}gcc -DMBEDTLS_NO_UDBL_DIVISION, make" # ~ 10s
    scripts/config.py baremetal
    scripts/config.py set MBEDTLS_NO_UDBL_DIVISION
    make CC="${ARM_NONE_EABI_GCC_PREFIX}gcc" AR="${ARM_NONE_EABI_GCC_PREFIX}ar" LD="${ARM_NONE_EABI_GCC_PREFIX}ld" CFLAGS='-std=c99 -Werror -Wall -Wextra' lib
    echo "Checking that software 64-bit division is not required"
    not grep __aeabi_uldiv library/*.o
}

component_build_arm_none_eabi_gcc_no_64bit_multiplication () {
    msg "build: ${ARM_NONE_EABI_GCC_PREFIX}gcc MBEDTLS_NO_64BIT_MULTIPLICATION, make" # ~ 10s
    scripts/config.py baremetal
    scripts/config.py set MBEDTLS_NO_64BIT_MULTIPLICATION
    make CC="${ARM_NONE_EABI_GCC_PREFIX}gcc" AR="${ARM_NONE_EABI_GCC_PREFIX}ar" LD="${ARM_NONE_EABI_GCC_PREFIX}ld" CFLAGS='-std=c99 -Werror -O1 -march=armv6-m -mthumb' lib
    echo "Checking that software 64-bit multiplication is not required"
    not grep __aeabi_lmul library/*.o
}

component_build_armcc () {
    msg "build: ARM Compiler 5"
    scripts/config.py baremetal
    # armc[56] don't support SHA-512 intrinsics
    scripts/config.py unset MBEDTLS_SHA512_USE_A64_CRYPTO_IF_PRESENT

    make CC="$ARMC5_CC" AR="$ARMC5_AR" WARNING_CFLAGS='--strict --c99' lib

    msg "size: ARM Compiler 5"
    "$ARMC5_FROMELF" -z library/*.o

    make clean

    # ARM Compiler 6 - Target ARMv7-A
    armc6_build_test "--target=arm-arm-none-eabi -march=armv7-a"

    # ARM Compiler 6 - Target ARMv7-M
    armc6_build_test "--target=arm-arm-none-eabi -march=armv7-m"

    # ARM Compiler 6 - Target ARMv8-A - AArch32
    armc6_build_test "--target=arm-arm-none-eabi -march=armv8.2-a"

    # ARM Compiler 6 - Target ARMv8-M
    armc6_build_test "--target=arm-arm-none-eabi -march=armv8-m.main"

    # ARM Compiler 6 - Target ARMv8.2-A - AArch64
    armc6_build_test "--target=aarch64-arm-none-eabi -march=armv8.2-a+crypto"
}

component_test_tls13_only () {
    msg "build: default config with MBEDTLS_SSL_PROTO_TLS1_3, without MBEDTLS_SSL_PROTO_TLS1_2"
    make CFLAGS="'-DMBEDTLS_USER_CONFIG_FILE=\"../tests/configs/tls13-only.h\"'"

    msg "test: default config with MBEDTLS_SSL_PROTO_TLS1_3 enabled, without MBEDTLS_SSL_PROTO_TLS1_2"
    if_build_succeeded make test

    msg "ssl-opt.sh (TLS 1.3)"
    if_build_succeeded tests/ssl-opt.sh
}

component_test_tls13_only_with_hooks () {
    msg "build: default config with MBEDTLS_SSL_PROTO_TLS1_3 and MBEDTLS_TEST_HOOKS, without MBEDTLS_SSL_PROTO_TLS1_2"
    scripts/config.py set MBEDTLS_TEST_HOOKS
    make CFLAGS="'-DMBEDTLS_USER_CONFIG_FILE=\"../tests/configs/tls13-only.h\"'"

    msg "test: default config with MBEDTLS_SSL_PROTO_TLS1_3 enabled, without MBEDTLS_SSL_PROTO_TLS1_2"
    if_build_succeeded make test

    msg "ssl-opt.sh (TLS 1.3)"
    if_build_succeeded tests/ssl-opt.sh
}

component_test_tls13 () {
    msg "build: default config with MBEDTLS_SSL_PROTO_TLS1_3 enabled, without padding"
    scripts/config.py set MBEDTLS_SSL_PROTO_TLS1_3
    scripts/config.py set MBEDTLS_SSL_TLS1_3_COMPATIBILITY_MODE
    scripts/config.py set MBEDTLS_SSL_CID_TLS1_3_PADDING_GRANULARITY 1
    CC=gcc cmake -D CMAKE_BUILD_TYPE:String=Asan .
    make
    msg "test: default config with MBEDTLS_SSL_PROTO_TLS1_3 enabled, without padding"
    make test
    msg "ssl-opt.sh (TLS 1.3)"
    tests/ssl-opt.sh
}

component_test_tls13_no_compatibility_mode () {
    msg "build: default config with MBEDTLS_SSL_PROTO_TLS1_3 enabled, without padding"
    scripts/config.py set   MBEDTLS_SSL_PROTO_TLS1_3
    scripts/config.py unset MBEDTLS_SSL_TLS1_3_COMPATIBILITY_MODE
    scripts/config.py set   MBEDTLS_SSL_CID_TLS1_3_PADDING_GRANULARITY 1
    CC=gcc cmake -D CMAKE_BUILD_TYPE:String=Asan .
    make
    msg "test: default config with MBEDTLS_SSL_PROTO_TLS1_3 enabled, without padding"
    make test
    msg "ssl-opt.sh (TLS 1.3 no compatibility mode)"
    tests/ssl-opt.sh
}

component_test_tls13_with_padding () {
    msg "build: default config with MBEDTLS_SSL_PROTO_TLS1_3 enabled, with padding"
    scripts/config.py set MBEDTLS_SSL_PROTO_TLS1_3
    scripts/config.py set MBEDTLS_SSL_TLS1_3_COMPATIBILITY_MODE
    scripts/config.py set MBEDTLS_SSL_CID_TLS1_3_PADDING_GRANULARITY 16
    CC=gcc cmake -D CMAKE_BUILD_TYPE:String=Asan .
    make
    msg "test: default config with MBEDTLS_SSL_PROTO_TLS1_3 enabled, with padding"
    make test
    msg "ssl-opt.sh (TLS 1.3 with padding)"
    tests/ssl-opt.sh
}

component_test_tls13_with_ecp_restartable () {
    msg "build: default config with MBEDTLS_SSL_PROTO_TLS1_3 enabled, with ecp_restartable"
    scripts/config.py set MBEDTLS_SSL_PROTO_TLS1_3
    scripts/config.py set MBEDTLS_SSL_TLS1_3_COMPATIBILITY_MODE
    scripts/config.py set MBEDTLS_ECP_RESTARTABLE
    CC=gcc cmake -D CMAKE_BUILD_TYPE:String=Asan .
    make
    msg "test: default config with MBEDTLS_SSL_PROTO_TLS1_3 enabled, with ecp_restartable"
    make test
    msg "ssl-opt.sh (TLS 1.3 with ecp_restartable)"
    tests/ssl-opt.sh
}

component_test_tls13_with_everest () {
    msg "build: default config with MBEDTLS_SSL_PROTO_TLS1_3 enabled, with Everest"
    scripts/config.py set MBEDTLS_SSL_PROTO_TLS1_3
    scripts/config.py set MBEDTLS_SSL_TLS1_3_COMPATIBILITY_MODE
    scripts/config.py set MBEDTLS_ECDH_VARIANT_EVEREST_ENABLED
    scripts/config.py unset MBEDTLS_ECP_RESTARTABLE
    CC=gcc cmake -D CMAKE_BUILD_TYPE:String=Asan .
    make
    msg "test: default config with MBEDTLS_SSL_PROTO_TLS1_3 enabled, with Everest"
    make test
    msg "ssl-opt.sh (TLS 1.3 with everest)"
    tests/ssl-opt.sh
}

component_build_mingw () {
    msg "build: Windows cross build - mingw64, make (Link Library)" # ~ 30s
    make CC=i686-w64-mingw32-gcc AR=i686-w64-mingw32-ar LD=i686-w64-minggw32-ld CFLAGS='-Werror -Wall -Wextra' WINDOWS_BUILD=1 lib programs

    # note Make tests only builds the tests, but doesn't run them
    make CC=i686-w64-mingw32-gcc AR=i686-w64-mingw32-ar LD=i686-w64-minggw32-ld CFLAGS='-Werror' WINDOWS_BUILD=1 tests
    make WINDOWS_BUILD=1 clean

    msg "build: Windows cross build - mingw64, make (DLL)" # ~ 30s
    make CC=i686-w64-mingw32-gcc AR=i686-w64-mingw32-ar LD=i686-w64-minggw32-ld CFLAGS='-Werror -Wall -Wextra' WINDOWS_BUILD=1 SHARED=1 lib programs
    make CC=i686-w64-mingw32-gcc AR=i686-w64-mingw32-ar LD=i686-w64-minggw32-ld CFLAGS='-Werror -Wall -Wextra' WINDOWS_BUILD=1 SHARED=1 tests
    make WINDOWS_BUILD=1 clean
}
support_build_mingw() {
    case $(i686-w64-mingw32-gcc -dumpversion) in
        [0-5]*) false;;
        *) true;;
    esac
}

component_test_memsan () {
    msg "build: MSan (clang)" # ~ 1 min 20s
    scripts/config.py unset MBEDTLS_AESNI_C # memsan doesn't grok asm
    CC=clang cmake -D CMAKE_BUILD_TYPE:String=MemSan .
    make

    msg "test: main suites (MSan)" # ~ 10s
    make test

    msg "test: ssl-opt.sh (MSan)" # ~ 1 min
    tests/ssl-opt.sh

    # Optional part(s)

    if [ "$MEMORY" -gt 0 ]; then
        msg "test: compat.sh (MSan)" # ~ 6 min 20s
        tests/compat.sh
    fi
}

component_test_valgrind () {
    msg "build: Release (clang)"
    CC=clang cmake -D CMAKE_BUILD_TYPE:String=Release .
    make

    msg "test: main suites valgrind (Release)"
    make memcheck

    # Optional parts (slow; currently broken on OS X because programs don't
    # seem to receive signals under valgrind on OS X).
    if [ "$MEMORY" -gt 0 ]; then
        msg "test: ssl-opt.sh --memcheck (Release)"
        tests/ssl-opt.sh --memcheck
    fi

    if [ "$MEMORY" -gt 1 ]; then
        msg "test: compat.sh --memcheck (Release)"
        tests/compat.sh --memcheck
    fi

    if [ "$MEMORY" -gt 0 ]; then
        msg "test: context-info.sh --memcheck (Release)"
        tests/context-info.sh --memcheck
    fi
}

support_test_cmake_out_of_source () {
    distrib_id=""
    distrib_ver=""
    distrib_ver_minor=""
    distrib_ver_major=""

    # Attempt to parse lsb-release to find out distribution and version. If not
    # found this should fail safe (test is supported).
    if [[ -f /etc/lsb-release ]]; then

        while read -r lsb_line; do
            case "$lsb_line" in
                "DISTRIB_ID"*) distrib_id=${lsb_line/#DISTRIB_ID=};;
                "DISTRIB_RELEASE"*) distrib_ver=${lsb_line/#DISTRIB_RELEASE=};;
            esac
        done < /etc/lsb-release

        distrib_ver_major="${distrib_ver%%.*}"
        distrib_ver="${distrib_ver#*.}"
        distrib_ver_minor="${distrib_ver%%.*}"
    fi

    # Running the out of source CMake test on Ubuntu 16.04 using more than one
    # processor (as the CI does) can create a race condition whereby the build
    # fails to see a generated file, despite that file actually having been
    # generated. This problem appears to go away with 18.04 or newer, so make
    # the out of source tests unsupported on Ubuntu 16.04.
    [ "$distrib_id" != "Ubuntu" ] || [ "$distrib_ver_major" -gt 16 ]
}

component_test_cmake_out_of_source () {
    msg "build: cmake 'out-of-source' build"
    MBEDTLS_ROOT_DIR="$PWD"
    mkdir "$OUT_OF_SOURCE_DIR"
    cd "$OUT_OF_SOURCE_DIR"
    cmake -D CMAKE_BUILD_TYPE:String=Check "$MBEDTLS_ROOT_DIR"
    make

    msg "test: cmake 'out-of-source' build"
    make test
    # Check that ssl-opt.sh can find the test programs.
    # Also ensure that there are no error messages such as
    # "No such file or directory", which would indicate that some required
    # file is missing (ssl-opt.sh tolerates the absence of some files so
    # may exit with status 0 but emit errors).
    ./tests/ssl-opt.sh -f 'Default' >ssl-opt.out 2>ssl-opt.err
    grep PASS ssl-opt.out
    cat ssl-opt.err >&2
    # If ssl-opt.err is non-empty, record an error and keep going.
    [ ! -s ssl-opt.err ]
    rm ssl-opt.out ssl-opt.err
    cd "$MBEDTLS_ROOT_DIR"
    rm -rf "$OUT_OF_SOURCE_DIR"
}

component_test_cmake_as_subdirectory () {
    msg "build: cmake 'as-subdirectory' build"
    cd programs/test/cmake_subproject
    cmake .
    make
    ./cmake_subproject
}
support_test_cmake_as_subdirectory () {
    support_test_cmake_out_of_source
}

component_test_cmake_as_package () {
    msg "build: cmake 'as-package' build"
    cd programs/test/cmake_package
    cmake .
    make
    ./cmake_package
}
support_test_cmake_as_package () {
    support_test_cmake_out_of_source
}

component_test_cmake_as_package_install () {
    msg "build: cmake 'as-installed-package' build"
    cd programs/test/cmake_package_install
    cmake .
    make
    ./cmake_package_install
}
support_test_cmake_as_package_install () {
    support_test_cmake_out_of_source
}

component_test_zeroize () {
    # Test that the function mbedtls_platform_zeroize() is not optimized away by
    # different combinations of compilers and optimization flags by using an
    # auxiliary GDB script. Unfortunately, GDB does not return error values to the
    # system in all cases that the script fails, so we must manually search the
    # output to check whether the pass string is present and no failure strings
    # were printed.

    # Don't try to disable ASLR. We don't care about ASLR here. We do care
    # about a spurious message if Gdb tries and fails, so suppress that.
    gdb_disable_aslr=
    if [ -z "$(gdb -batch -nw -ex 'set disable-randomization off' 2>&1)" ]; then
        gdb_disable_aslr='set disable-randomization off'
    fi

    for optimization_flag in -O2 -O3 -Ofast -Os; do
        for compiler in clang gcc; do
            msg "test: $compiler $optimization_flag, mbedtls_platform_zeroize()"
            make programs CC="$compiler" DEBUG=1 CFLAGS="$optimization_flag"
            gdb -ex "$gdb_disable_aslr" -x tests/scripts/test_zeroize.gdb -nw -batch -nx 2>&1 | tee test_zeroize.log
            grep "The buffer was correctly zeroized" test_zeroize.log
            not grep -i "error" test_zeroize.log
            rm -f test_zeroize.log
            make clean
        done
    done
}

component_test_psa_compliance () {
    msg "build: make, default config (out-of-box), libmbedcrypto.a only"
    make -C library libmbedcrypto.a

    msg "unit test: test_psa_compliance.py"
    ./tests/scripts/test_psa_compliance.py
}

support_test_psa_compliance () {
    # psa-compliance-tests only supports CMake >= 3.10.0
    ver="$(cmake --version)"
    ver="${ver#cmake version }"
    ver_major="${ver%%.*}"

    ver="${ver#*.}"
    ver_minor="${ver%%.*}"

    [ "$ver_major" -eq 3 ] && [ "$ver_minor" -ge 10 ]
}

component_check_python_files () {
    msg "Lint: Python scripts"
    tests/scripts/check-python-files.sh
}

component_check_test_helpers () {
    msg "unit test: generate_test_code.py"
    # unittest writes out mundane stuff like number or tests run on stderr.
    # Our convention is to reserve stderr for actual errors, and write
    # harmless info on stdout so it can be suppress with --quiet.
    ./tests/scripts/test_generate_test_code.py 2>&1

    msg "unit test: translate_ciphers.py"
    python3 -m unittest tests/scripts/translate_ciphers.py 2>&1
}

################################################################
#### Termination
################################################################

post_report () {
    msg "Done, cleaning up"
    final_cleanup

    final_report
}



################################################################
#### Run all the things
################################################################

# Function invoked by --error-test to test error reporting.
pseudo_component_error_test () {
    msg "Testing error reporting $error_test_i"
    if [ $KEEP_GOING -ne 0 ]; then
        echo "Expect three failing commands."
    fi
    # If the component doesn't run in a subshell, changing error_test_i to an
    # invalid integer will cause an error in the loop that runs this function.
    error_test_i=this_should_not_be_used_since_the_component_runs_in_a_subshell
    # Expected error: 'grep non_existent /dev/null -> 1'
    grep non_existent /dev/null
    # Expected error: '! grep -q . tests/scripts/all.sh -> 1'
    not grep -q . "$0"
    # Expected error: 'make unknown_target -> 2'
    make unknown_target
    false "this should not be executed"
}

# Run one component and clean up afterwards.
run_component () {
    current_component="$1"
    export MBEDTLS_TEST_CONFIGURATION="$current_component"

    # Unconditionally create a seedfile that's sufficiently long.
    # Do this before each component, because a previous component may
    # have messed it up or shortened it.
    local dd_cmd
    dd_cmd=(dd if=/dev/urandom of=./tests/seedfile bs=64 count=1)
    case $OSTYPE in
        linux*|freebsd*|openbsd*) dd_cmd+=(status=none)
    esac
    "${dd_cmd[@]}"

    # Run the component in a subshell, with error trapping and output
    # redirection set up based on the relevant options.
    if [ $KEEP_GOING -eq 1 ]; then
        # We want to keep running if the subshell fails, so 'set -e' must
        # be off when the subshell runs.
        set +e
    fi
    (
        if [ $QUIET -eq 1 ]; then
            # msg() will be silenced, so just print the component name here.
            echo "${current_component#component_}"
            exec >/dev/null
        fi
        if [ $KEEP_GOING -eq 1 ]; then
            # Keep "set -e" off, and run an ERR trap instead to record failures.
            set -E
            trap err_trap ERR
        fi
        # The next line is what runs the component
        "$@"
        if [ $KEEP_GOING -eq 1 ]; then
            trap - ERR
            exit $last_failure_status
        fi
    )
    component_status=$?
    if [ $KEEP_GOING -eq 1 ]; then
        set -e
        if [ $component_status -ne 0 ]; then
            failure_count=$((failure_count + 1))
        fi
    fi

    # Restore the build tree to a clean state.
    cleanup
    unset current_component
}

# Preliminary setup
pre_check_environment
pre_initialize_variables
pre_parse_command_line "$@"

pre_check_git
pre_restore_files
pre_back_up

build_status=0
if [ $KEEP_GOING -eq 1 ]; then
    pre_setup_keep_going
fi
pre_prepare_outcome_file
pre_print_configuration
pre_check_tools
cleanup
pre_generate_files

# Run the requested tests.
for ((error_test_i=1; error_test_i <= error_test; error_test_i++)); do
    run_component pseudo_component_error_test
done
unset error_test_i
for component in $RUN_COMPONENTS; do
    run_component "component_$component"
done

# We're done.
post_report
