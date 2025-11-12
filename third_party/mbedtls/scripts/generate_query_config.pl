#! /usr/bin/env perl

# Generate query_config.c
#
# The file query_config.c contains a C function that can be used to check if
# a configuration macro is defined and to retrieve its expansion in string
# form (if any). This facilitates querying the compile time configuration of
# the library, for example, for testing.
#
# The query_config.c is generated from the current configuration at
# include/mbedtls/mbedtls_config.h. The idea is that the mbedtls_config.h contains ALL the
# compile time configurations available in Mbed TLS (commented or uncommented).
# This script extracts the configuration macros from the mbedtls_config.h and this
# information is used to automatically generate the body of the query_config()
# function by using the template in scripts/data_files/query_config.fmt.
#
# Usage: scripts/generate_query_config.pl without arguments, or
# generate_query_config.pl config_file template_file output_file
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

use strict;

my ($config_file, $query_config_format_file, $query_config_file);

if( @ARGV ) {
    die "Invalid number of arguments - usage: $0 [CONFIG_FILE TEMPLATE_FILE OUTPUT_FILE]" if scalar @ARGV != 3;
    ($config_file, $query_config_format_file, $query_config_file) = @ARGV;

    -f $config_file or die "No such file: $config_file";
    -f $query_config_format_file or die "No such file: $query_config_format_file";
} else {
    $config_file = "./include/mbedtls/mbedtls_config.h";
    $query_config_format_file = "./scripts/data_files/query_config.fmt";
    $query_config_file = "./programs/test/query_config.c";

    unless( -f $config_file && -f $query_config_format_file ) {
        chdir '..' or die;
        -f $config_file && -f $query_config_format_file
          or die "No arguments supplied, must be run from project root or a first-level subdirectory\n";
    }
}

# Excluded macros from the generated query_config.c. For example, macros that
# have commas or function-like macros cannot be transformed into strings easily
# using the preprocessor, so they should be excluded or the preprocessor will
# throw errors.
my @excluded = qw(
MBEDTLS_SSL_CIPHERSUITES
);
my $excluded_re = join '|', @excluded;

open(CONFIG_FILE, "$config_file") or die "Opening config file '$config_file': $!";

# This variable will contain the string to replace in the CHECK_CONFIG of the
# format file
my $config_check = "";
my $list_config = "";

while (my $line = <CONFIG_FILE>) {
    if ($line =~ /^(\/\/)?\s*#\s*define\s+(MBEDTLS_\w+).*/) {
        my $name = $2;

        # Skip over the macro if it is in the ecluded list
        next if $name =~ /$excluded_re/;

        $config_check .= "#if defined($name)\n";
        $config_check .= "    if( strcmp( \"$name\", config ) == 0 )\n";
        $config_check .= "    {\n";
        $config_check .= "        MACRO_EXPANSION_TO_STR( $name );\n";
        $config_check .= "        return( 0 );\n";
        $config_check .= "    }\n";
        $config_check .= "#endif /* $name */\n";
        $config_check .= "\n";

        $list_config .= "#if defined($name)\n";
        $list_config .= "    OUTPUT_MACRO_NAME_VALUE($name);\n";
        $list_config .= "#endif /* $name */\n";
        $list_config .= "\n";
    }
}

# Read the full format file into a string
local $/;
open(FORMAT_FILE, "$query_config_format_file") or die "Opening query config format file '$query_config_format_file': $!";
my $query_config_format = <FORMAT_FILE>;
close(FORMAT_FILE);

# Replace the body of the query_config() function with the code we just wrote
$query_config_format =~ s/CHECK_CONFIG/$config_check/g;
$query_config_format =~ s/LIST_CONFIG/$list_config/g;

# Rewrite the query_config.c file
open(QUERY_CONFIG_FILE, ">$query_config_file") or die "Opening destination file '$query_config_file': $!";
print QUERY_CONFIG_FILE $query_config_format;
close(QUERY_CONFIG_FILE);
