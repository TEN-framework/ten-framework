#!/usr/bin/env perl

# curves.pl
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
# The purpose of this test script is to validate that the library works
# when only a single curve is enabled. In particular, this validates that
# curve-specific code is guarded by the proper preprocessor conditionals,
# both in the library and in tests.
#
# Since this script only tests builds with a single curve, it can't detect
# bugs that are only triggered when multiple curves are present. We do
# also test in many configurations where all curves are enabled, as well
# as a few configurations in configs/*.h with a restricted subset of curves.
#
# Here are some known test gaps that could be addressed by testing all
# 2^n combinations of support for n curves, which is impractical:
# * There could be product bugs when curves A and B are enabled but not C.
#   For example, a MAX_SIZE calculation that forgets B, where
#   size(A) < size(B) < size(C).
# * For test cases that require three or more curves, validate that they're
#   not missing dependencies. This is extremely rare. (For test cases that
#   require curves A and B but are missing a dependency on B, this is
#   detected in the A-only build.)
# Usage: tests/scripts/curves.pl
#
# This script should be executed from the root of the project directory.
#
# Only curves that are enabled in mbedtls_config.h will be tested.
#
# For best effect, run either with cmake disabled, or cmake enabled in a mode
# that includes -Werror.

use warnings;
use strict;

-d 'library' && -d 'include' && -d 'tests' or die "Must be run from root\n";

my $sed_cmd = 's/^#define \(MBEDTLS_ECP_DP.*_ENABLED\)/\1/p';
my $config_h = 'include/mbedtls/mbedtls_config.h';
my @curves = split( /\s+/, `sed -n -e '$sed_cmd' $config_h` );

# Determine which curves support ECDSA by checking the dependencies of
# ECDSA in check_config.h.
my %curve_supports_ecdsa = ();
{
    local $/ = "";
    local *CHECK_CONFIG;
    open(CHECK_CONFIG, '<', 'include/mbedtls/check_config.h')
        or die "open include/mbedtls/check_config.h: $!";
    while (my $stanza = <CHECK_CONFIG>) {
        if ($stanza =~ /\A#if defined\(MBEDTLS_ECDSA_C\)/) {
            for my $curve ($stanza =~ /(?<=\()MBEDTLS_ECP_DP_\w+_ENABLED(?=\))/g) {
                $curve_supports_ecdsa{$curve} = 1;
            }
            last;
        }
    }
    close(CHECK_CONFIG);
}

system( "cp $config_h $config_h.bak" ) and die;
sub abort {
    system( "mv $config_h.bak $config_h" ) and warn "$config_h not restored\n";
    # use an exit code between 1 and 124 for git bisect (die returns 255)
    warn $_[0];
    exit 1;
}

# Disable all the curves. We'll then re-enable them one by one.
for my $curve (@curves) {
    system( "scripts/config.pl unset $curve" )
        and abort "Failed to disable $curve\n";
}
# Depends on a specific curve. Also, ignore error if it wasn't enabled.
system( "scripts/config.pl unset MBEDTLS_KEY_EXCHANGE_ECJPAKE_ENABLED" );
system( "scripts/config.pl unset MBEDTLS_ECJPAKE_C" );

# Test with only $curve enabled, for each $curve.
for my $curve (@curves) {
    system( "make clean" ) and die;

    print "\n******************************************\n";
    print "* Testing with only curve: $curve\n";
    print "******************************************\n";
    $ENV{MBEDTLS_TEST_CONFIGURATION} = "$curve";

    system( "scripts/config.pl set $curve" )
        and abort "Failed to enable $curve\n";

    my $ecdsa = $curve_supports_ecdsa{$curve} ? "set" : "unset";
    for my $dep (qw(MBEDTLS_ECDSA_C
                    MBEDTLS_KEY_EXCHANGE_ECDH_ECDSA_ENABLED
                    MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED)) {
        system( "scripts/config.pl $ecdsa $dep" )
            and abort "Failed to $ecdsa $dep\n";
    }

    system( "CFLAGS='-Werror -Wall -Wextra' make" )
        and abort "Failed to build: only $curve\n";
    system( "make test" )
        and abort "Failed test suite: only $curve\n";

    system( "scripts/config.pl unset $curve" )
        and abort "Failed to disable $curve\n";
}

system( "mv $config_h.bak $config_h" ) and die "$config_h not restored\n";
system( "make clean" ) and die;
exit 0;
