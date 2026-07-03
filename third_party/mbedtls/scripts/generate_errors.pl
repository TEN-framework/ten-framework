#!/usr/bin/env perl

# Generate error.c
#
# Usage: ./generate_errors.pl or scripts/generate_errors.pl without arguments,
# or generate_errors.pl include_dir data_dir error_file
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
use warnings;

my ($include_dir, $data_dir, $error_file);

if( @ARGV ) {
    die "Invalid number of arguments" if scalar @ARGV != 3;
    ($include_dir, $data_dir, $error_file) = @ARGV;

    -d $include_dir or die "No such directory: $include_dir\n";
    -d $data_dir or die "No such directory: $data_dir\n";
} else {
    $include_dir = 'include/mbedtls';
    $data_dir = 'scripts/data_files';
    $error_file = 'library/error.c';

    unless( -d $include_dir && -d $data_dir ) {
        chdir '..' or die;
        -d $include_dir && -d $data_dir
            or die "Without arguments, must be run from root or scripts\n"
    }
}

my $error_format_file = $data_dir.'/error.fmt';

my @low_level_modules = qw( AES ARIA ASN1 BASE64 BIGNUM
                            CAMELLIA CCM CHACHA20 CHACHAPOLY CMAC CTR_DRBG DES
                            ENTROPY ERROR GCM HKDF HMAC_DRBG MD5
                            NET OID PADLOCK PBKDF2 PLATFORM POLY1305 RIPEMD160
                            SHA1 SHA256 SHA512 THREADING );
my @high_level_modules = qw( CIPHER DHM ECP MD
                             PEM PK PKCS12 PKCS5
                             RSA SSL X509 );

undef $/;

open(FORMAT_FILE, '<:crlf', "$error_format_file") or die "Opening error format file '$error_format_file': $!";
my $error_format = <FORMAT_FILE>;
close(FORMAT_FILE);

my @files = <$include_dir/*.h>;
my @necessary_include_files;
my @matches;
foreach my $file (@files) {
    open(FILE, '<:crlf', "$file");
    my $content = <FILE>;
    close FILE;
    my $found = 0;
    while ($content =~ m[
            # Both the before-comment and the after-comment are optional.
            # Only the comment content is a regex capture group. The comment
            # start and end parts are outside the capture group.
            (?:/\*[*!](?!<)             # Doxygen before-comment start
                ((?:[^*]|\*+[^*/])*)    # $1: Comment content (no */ inside)
                \*/)?                   # Comment end
            \s*\#\s*define\s+(MBEDTLS_ERR_\w+)  # $2: name
            \s+\-(0[Xx][0-9A-Fa-f]+)\s*         # $3: value (without the sign)
            (?:/\*[*!]<                 # Doxygen after-comment start
                ((?:[^*]|\*+[^*/])*)    # $4: Comment content (no */ inside)
                \*/)?                   # Comment end
    ]gsx) {
        my ($before, $name, $value, $after) = ($1, $2, $3, $4);
        # Discard Doxygen comments that are coincidentally present before
        # an error definition but not attached to it. This is ad hoc, based
        # on what actually matters (or mattered at some point).
        undef $before if defined($before) && $before =~ /\s*\\name\s/s;
        die "Description neither before nor after $name in $file\n"
          if !defined($before) && !defined($after);
        die "Description both before and after $name in $file\n"
          if defined($before) && defined($after);
        my $description = (defined($before) ? $before : $after);
        $description =~ s/^\s+//;
        $description =~ s/\n( *\*)? */ /g;
        $description =~ s/\.?\s+$//;
        push @matches, [$name, $value, $description];
        ++$found;
    }
    if ($found) {
        my $include_name = $file;
        $include_name =~ s!.*/!!;
        push @necessary_include_files, $include_name;
    }
}

my $ll_old_define = "";
my $hl_old_define = "";

my $ll_code_check = "";
my $hl_code_check = "";

my $headers = "";
my %included_headers;

my %error_codes_seen;

foreach my $match (@matches)
{
    my ($error_name, $error_code, $description) = @$match;

    die "Duplicated error code: $error_code ($error_name)\n"
        if( $error_codes_seen{$error_code}++ );

    $description =~ s/\\/\\\\/g;

    my ($module_name) = $error_name =~ /^MBEDTLS_ERR_([^_]+)/;

    # Fix faulty ones
    $module_name = "BIGNUM" if ($module_name eq "MPI");
    $module_name = "CTR_DRBG" if ($module_name eq "CTR");
    $module_name = "HMAC_DRBG" if ($module_name eq "HMAC");

    my $define_name = $module_name;
    $define_name = "X509_USE,X509_CREATE" if ($define_name eq "X509");
    $define_name = "ASN1_PARSE" if ($define_name eq "ASN1");
    $define_name = "SSL_TLS" if ($define_name eq "SSL");
    $define_name = "PEM_PARSE,PEM_WRITE" if ($define_name eq "PEM");

    my $include_name = $module_name;
    $include_name =~ tr/A-Z/a-z/;

    # Fix faulty ones
    $include_name = "net_sockets" if ($module_name eq "NET");

    $included_headers{"${include_name}.h"} = $module_name;

    my $found_ll = grep $_ eq $module_name, @low_level_modules;
    my $found_hl = grep $_ eq $module_name, @high_level_modules;
    if (!$found_ll && !$found_hl)
    {
        printf("Error: Do not know how to handle: $module_name\n");
        exit 1;
    }

    my $code_check;
    my $old_define;
    my $white_space;
    my $first;

    if ($found_ll)
    {
        $code_check = \$ll_code_check;
        $old_define = \$ll_old_define;
        $white_space = '        ';
    }
    else
    {
        $code_check = \$hl_code_check;
        $old_define = \$hl_old_define;
        $white_space = '        ';
    }

    if ($define_name ne ${$old_define})
    {
        if (${$old_define} ne "")
        {
            ${$code_check} .= "#endif /* ";
            $first = 0;
            foreach my $dep (split(/,/, ${$old_define}))
            {
                ${$code_check} .= " || " if ($first++);
                ${$code_check} .= "MBEDTLS_${dep}_C";
            }
            ${$code_check} .= " */\n\n";
        }

        ${$code_check} .= "#if ";
        $headers .= "#if " if ($include_name ne "");
        $first = 0;
        foreach my $dep (split(/,/, ${define_name}))
        {
            ${$code_check} .= " || " if ($first);
            $headers       .= " || " if ($first++);

            ${$code_check} .= "defined(MBEDTLS_${dep}_C)";
            $headers       .= "defined(MBEDTLS_${dep}_C)" if
                                                    ($include_name ne "");
        }
        ${$code_check} .= "\n";
        $headers .= "\n#include \"mbedtls/${include_name}.h\"\n".
                    "#endif\n\n" if ($include_name ne "");
        ${$old_define} = $define_name;
    }

    ${$code_check} .= "${white_space}case -($error_name):\n".
                      "${white_space}    return( \"$module_name - $description\" );\n"
};

if ($ll_old_define ne "")
{
    $ll_code_check .= "#endif /* ";
    my $first = 0;
    foreach my $dep (split(/,/, $ll_old_define))
    {
        $ll_code_check .= " || " if ($first++);
        $ll_code_check .= "MBEDTLS_${dep}_C";
    }
    $ll_code_check .= " */\n";
}
if ($hl_old_define ne "")
{
    $hl_code_check .= "#endif /* ";
    my $first = 0;
    foreach my $dep (split(/,/, $hl_old_define))
    {
        $hl_code_check .= " || " if ($first++);
        $hl_code_check .= "MBEDTLS_${dep}_C";
    }
    $hl_code_check .= " */\n";
}

$error_format =~ s/HEADER_INCLUDED\n/$headers/g;
$error_format =~ s/LOW_LEVEL_CODE_CHECKS\n/$ll_code_check/g;
$error_format =~ s/HIGH_LEVEL_CODE_CHECKS\n/$hl_code_check/g;

open(ERROR_FILE, ">$error_file") or die "Opening destination file '$error_file': $!";
print ERROR_FILE $error_format;
close(ERROR_FILE);

my $errors = 0;
for my $include_name (@necessary_include_files)
{
    if (not $included_headers{$include_name})
    {
        print STDERR "The header file \"$include_name\" defines error codes but has not been included!\n";
        ++$errors;
    }
}

exit !!$errors;
