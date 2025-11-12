#!/usr/bin/env perl

# Generate main file, individual apps and solution files for MS Visual Studio
# 2010
#
# Must be run from mbedTLS root or scripts directory.
# Takes no argument.
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

use warnings;
use strict;
use Digest::MD5 'md5_hex';

my $vsx_dir = "visualc/VS2010";
my $vsx_ext = "vcxproj";
my $vsx_app_tpl_file = "scripts/data_files/vs2010-app-template.$vsx_ext";
my $vsx_main_tpl_file = "scripts/data_files/vs2010-main-template.$vsx_ext";
my $vsx_main_file = "$vsx_dir/mbedTLS.$vsx_ext";
my $vsx_sln_tpl_file = "scripts/data_files/vs2010-sln-template.sln";
my $vsx_sln_file = "$vsx_dir/mbedTLS.sln";

my $programs_dir = 'programs';
my $mbedtls_header_dir = 'include/mbedtls';
my $psa_header_dir = 'include/psa';
my $source_dir = 'library';
my $test_source_dir = 'tests/src';
my $test_header_dir = 'tests/include/test';
my $test_drivers_header_dir = 'tests/include/test/drivers';
my $test_drivers_source_dir = 'tests/src/drivers';

my @thirdparty_header_dirs = qw(
    3rdparty/everest/include/everest
);
my @thirdparty_source_dirs = qw(
    3rdparty/everest/library
    3rdparty/everest/library/kremlib
    3rdparty/everest/library/legacy
);

# Directories to add to the include path.
# Order matters in case there are files with the same name in more than
# one directory: the compiler will use the first match.
my @include_directories = qw(
    include
    3rdparty/everest/include/
    3rdparty/everest/include/everest
    3rdparty/everest/include/everest/vs2010
    3rdparty/everest/include/everest/kremlib
    tests/include
);
my $include_directories = join(';', map {"../../$_"} @include_directories);

# Directories to add to the include path when building the library, but not
# when building tests or applications.
my @library_include_directories = qw(
    library
);
my $library_include_directories =
  join(';', map {"../../$_"} (@library_include_directories,
                              @include_directories));

my @excluded_files = qw(
    3rdparty/everest/library/Hacl_Curve25519.c
);
my %excluded_files = ();
foreach (@excluded_files) { $excluded_files{$_} = 1 }

my $vsx_hdr_tpl = <<EOT;
    <ClInclude Include="..\\..\\{NAME}" />
EOT
my $vsx_src_tpl = <<EOT;
    <ClCompile Include="..\\..\\{NAME}" />
EOT

my $vsx_sln_app_entry_tpl = <<EOT;
Project("{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}") = "{APPNAME}", "{APPNAME}.vcxproj", "{GUID}"
	ProjectSection(ProjectDependencies) = postProject
		{46CF2D25-6A36-4189-B59C-E4815388E554} = {46CF2D25-6A36-4189-B59C-E4815388E554}
	EndProjectSection
EndProject
EOT

my $vsx_sln_conf_entry_tpl = <<EOT;
		{GUID}.Debug|Win32.ActiveCfg = Debug|Win32
		{GUID}.Debug|Win32.Build.0 = Debug|Win32
		{GUID}.Debug|x64.ActiveCfg = Debug|x64
		{GUID}.Debug|x64.Build.0 = Debug|x64
		{GUID}.Release|Win32.ActiveCfg = Release|Win32
		{GUID}.Release|Win32.Build.0 = Release|Win32
		{GUID}.Release|x64.ActiveCfg = Release|x64
		{GUID}.Release|x64.Build.0 = Release|x64
EOT

exit( main() );

sub check_dirs {
    foreach my $d (@thirdparty_header_dirs, @thirdparty_source_dirs) {
        if (not (-d $d)) { return 0; }
    }
    return -d $vsx_dir
        && -d $mbedtls_header_dir
        && -d $psa_header_dir
        && -d $source_dir
        && -d $test_source_dir
        && -d $test_drivers_source_dir
        && -d $test_header_dir
        && -d $test_drivers_header_dir
        && -d $programs_dir;
}

sub slurp_file {
    my ($filename) = @_;

    local $/ = undef;
    open my $fh, '<:crlf', $filename or die "Could not read $filename\n";
    my $content = <$fh>;
    close $fh;

    return $content;
}

sub content_to_file {
    my ($content, $filename) = @_;

    open my $fh, '>:crlf', $filename or die "Could not write to $filename\n";
    print $fh $content;
    close $fh;
}

sub gen_app_guid {
    my ($path) = @_;

    my $guid = md5_hex( "mbedTLS:$path" );
    $guid =~ s/(.{8})(.{4})(.{4})(.{4})(.{12})/\U{$1-$2-$3-$4-$5}/;

    return $guid;
}

sub gen_app {
    my ($path, $template, $dir, $ext) = @_;

    my $guid = gen_app_guid( $path );
    $path =~ s!/!\\!g;
    (my $appname = $path) =~ s/.*\\//;

    my $srcs = "<ClCompile Include=\"..\\..\\programs\\$path.c\" \/>";
    if( $appname eq "ssl_client2" or $appname eq "ssl_server2" or
        $appname eq "query_compile_time_config" ) {
        $srcs .= "\n    <ClCompile Include=\"..\\..\\programs\\test\\query_config.c\" \/>";
    }
    if( $appname eq "ssl_client2" or $appname eq "ssl_server2" ) {
        $srcs .= "\n    <ClCompile Include=\"..\\..\\programs\\ssl\\ssl_test_lib.c\" \/>";
    }

    my $content = $template;
    $content =~ s/<SOURCES>/$srcs/g;
    $content =~ s/<APPNAME>/$appname/g;
    $content =~ s/<GUID>/$guid/g;
    $content =~ s/INCLUDE_DIRECTORIES\n/$include_directories/g;

    content_to_file( $content, "$dir/$appname.$ext" );
}

sub get_app_list {
    my $makefile_contents = slurp_file('programs/Makefile');
    $makefile_contents =~ /\n\s*APPS\s*=[\\\s]*(.*?)(?<!\\)[\#\n]/s
      or die "Cannot find APPS = ... in programs/Makefile\n";
    return split /(?:\s|\\)+/, $1;
}

sub gen_app_files {
    my @app_list = @_;

    my $vsx_tpl = slurp_file( $vsx_app_tpl_file );

    for my $app ( @app_list ) {
        gen_app( $app, $vsx_tpl, $vsx_dir, $vsx_ext );
    }
}

sub gen_entry_list {
    my ($tpl, @names) = @_;

    my $entries;
    for my $name (@names) {
        (my $entry = $tpl) =~ s/{NAME}/$name/g;
        $entries .= $entry;
    }

    return $entries;
}

sub gen_main_file {
    my ($headers, $sources,
        $hdr_tpl, $src_tpl,
        $main_tpl, $main_out) = @_;

    my $header_entries = gen_entry_list( $hdr_tpl, @$headers );
    my $source_entries = gen_entry_list( $src_tpl, @$sources );

    my $out = slurp_file( $main_tpl );
    $out =~ s/SOURCE_ENTRIES\n/$source_entries/m;
    $out =~ s/HEADER_ENTRIES\n/$header_entries/m;
    $out =~ s/INCLUDE_DIRECTORIES\n/$library_include_directories/g;

    content_to_file( $out, $main_out );
}

sub gen_vsx_solution {
    my (@app_names) = @_;

    my ($app_entries, $conf_entries);
    for my $path (@app_names) {
        my $guid = gen_app_guid( $path );
        (my $appname = $path) =~ s!.*/!!;

        my $app_entry = $vsx_sln_app_entry_tpl;
        $app_entry =~ s/{APPNAME}/$appname/g;
        $app_entry =~ s/{GUID}/$guid/g;

        $app_entries .= $app_entry;

        my $conf_entry = $vsx_sln_conf_entry_tpl;
        $conf_entry =~ s/{GUID}/$guid/g;

        $conf_entries .= $conf_entry;
    }

    my $out = slurp_file( $vsx_sln_tpl_file );
    $out =~ s/APP_ENTRIES\n/$app_entries/m;
    $out =~ s/CONF_ENTRIES\n/$conf_entries/m;

    content_to_file( $out, $vsx_sln_file );
}

sub del_vsx_files {
    unlink glob "'$vsx_dir/*.$vsx_ext'";
    unlink $vsx_main_file;
    unlink $vsx_sln_file;
}

sub main {
    if( ! check_dirs() ) {
        chdir '..' or die;
        check_dirs or die "Must but run from mbedTLS root or scripts dir\n";
    }

    # Remove old files to ensure that, for example, project files from deleted
    # apps are not kept
    del_vsx_files();

    my @app_list = get_app_list();
    my @header_dirs = (
                       $mbedtls_header_dir,
                       $psa_header_dir,
                       $test_header_dir,
                       $test_drivers_header_dir,
                       $source_dir,
                       @thirdparty_header_dirs,
                      );
    my @headers = (map { <$_/*.h> } @header_dirs);
    my @source_dirs = (
                       $source_dir,
                       $test_source_dir,
                       $test_drivers_source_dir,
                       @thirdparty_source_dirs,
                      );
    my @sources = (map { <$_/*.c> } @source_dirs);

    @headers = grep { ! $excluded_files{$_} } @headers;
    @sources = grep { ! $excluded_files{$_} } @sources;
    map { s!/!\\!g } @headers;
    map { s!/!\\!g } @sources;

    gen_app_files( @app_list );

    gen_main_file( \@headers, \@sources,
                   $vsx_hdr_tpl, $vsx_src_tpl,
                   $vsx_main_tpl_file, $vsx_main_file );

    gen_vsx_solution( @app_list );

    return 0;
}
