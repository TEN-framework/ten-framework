#!/bin/bash
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
# Sets the version numbers in the source code to those given.
#
# Usage: bump_version.sh [ --version <version> ] [ --so-crypto <version>]
#                           [ --so-x509 <version> ] [ --so-tls <version> ]
#                           [ -v | --verbose ] [ -h | --help ]
#

set -e

VERSION=""
SOVERSION=""

# Parse arguments
#
until [ -z "$1" ]
do
  case "$1" in
    --version)
      # Version to use
      shift
      VERSION=$1
      ;;
    --so-crypto)
      shift
      SO_CRYPTO=$1
      ;;
    --so-x509)
      shift
      SO_X509=$1
      ;;
    --so-tls)
      shift
      SO_TLS=$1
      ;;
    -v|--verbose)
      # Be verbose
      VERBOSE="1"
      ;;
    -h|--help)
      # print help
      echo "Usage: $0"
      echo -e "  -h|--help\t\tPrint this help."
      echo -e "  --version <version>\tVersion to bump to."
      echo -e "  --so-crypto <version>\tSO version to bump libmbedcrypto to."
      echo -e "  --so-x509 <version>\tSO version to bump libmbedx509 to."
      echo -e "  --so-tls <version>\tSO version to bump libmbedtls to."
      echo -e "  -v|--verbose\t\tVerbose."
      exit 1
      ;;
    *)
      # print error
      echo "Unknown argument: '$1'"
      exit 1
      ;;
  esac
  shift
done

if [ "X" = "X$VERSION" ];
then
  echo "No version specified. Unable to continue."
  exit 1
fi

[ $VERBOSE ] && echo "Bumping VERSION in CMakeLists.txt"
sed -e "s/ VERSION [0-9.]\{1,\}/ VERSION $VERSION/g" < CMakeLists.txt > tmp
mv tmp CMakeLists.txt

[ $VERBOSE ] && echo "Bumping VERSION in library/CMakeLists.txt"
sed -e "s/ VERSION [0-9.]\{1,\}/ VERSION $VERSION/g" < library/CMakeLists.txt > tmp
mv tmp library/CMakeLists.txt

if [ "X" != "X$SO_CRYPTO" ];
then
  [ $VERBOSE ] && echo "Bumping SOVERSION for libmbedcrypto in library/CMakeLists.txt"
  sed -e "/mbedcrypto/ s/ SOVERSION [0-9]\{1,\}/ SOVERSION $SO_CRYPTO/g" < library/CMakeLists.txt > tmp
  mv tmp library/CMakeLists.txt

  [ $VERBOSE ] && echo "Bumping SOVERSION for libmbedcrypto in library/Makefile"
  sed -e "s/SOEXT_CRYPTO=so.[0-9]\{1,\}/SOEXT_CRYPTO=so.$SO_CRYPTO/g" < library/Makefile > tmp
  mv tmp library/Makefile
fi

if [ "X" != "X$SO_X509" ];
then
  [ $VERBOSE ] && echo "Bumping SOVERSION for libmbedx509 in library/CMakeLists.txt"
  sed -e "/mbedx509/ s/ SOVERSION [0-9]\{1,\}/ SOVERSION $SO_X509/g" < library/CMakeLists.txt > tmp
  mv tmp library/CMakeLists.txt

  [ $VERBOSE ] && echo "Bumping SOVERSION for libmbedx509 in library/Makefile"
  sed -e "s/SOEXT_X509=so.[0-9]\{1,\}/SOEXT_X509=so.$SO_X509/g" < library/Makefile > tmp
  mv tmp library/Makefile
fi

if [ "X" != "X$SO_TLS" ];
then
  [ $VERBOSE ] && echo "Bumping SOVERSION for libmbedtls in library/CMakeLists.txt"
  sed -e "/mbedtls/ s/ SOVERSION [0-9]\{1,\}/ SOVERSION $SO_TLS/g" < library/CMakeLists.txt > tmp
  mv tmp library/CMakeLists.txt

  [ $VERBOSE ] && echo "Bumping SOVERSION for libmbedtls in library/Makefile"
  sed -e "s/SOEXT_TLS=so.[0-9]\{1,\}/SOEXT_TLS=so.$SO_TLS/g" < library/Makefile > tmp
  mv tmp library/Makefile
fi

[ $VERBOSE ] && echo "Bumping VERSION in include/mbedtls/build_info.h"
read MAJOR MINOR PATCH <<<$(IFS="."; echo $VERSION)
VERSION_NR="$( printf "0x%02X%02X%02X00" $MAJOR $MINOR $PATCH )"
cat include/mbedtls/build_info.h |                                    \
    sed -e "s/\(# *define  *[A-Z]*_VERSION\)_MAJOR .\{1,\}/\1_MAJOR  $MAJOR/" |    \
    sed -e "s/\(# *define  *[A-Z]*_VERSION\)_MINOR .\{1,\}/\1_MINOR  $MINOR/" |    \
    sed -e "s/\(# *define  *[A-Z]*_VERSION\)_PATCH .\{1,\}/\1_PATCH  $PATCH/" |    \
    sed -e "s/\(# *define  *[A-Z]*_VERSION\)_NUMBER .\{1,\}/\1_NUMBER         $VERSION_NR/" |    \
    sed -e "s/\(# *define  *[A-Z]*_VERSION\)_STRING .\{1,\}/\1_STRING         \"$VERSION\"/" |    \
    sed -e "s/\(# *define  *[A-Z]*_VERSION\)_STRING_FULL .\{1,\}/\1_STRING_FULL    \"mbed TLS $VERSION\"/" \
    > tmp
mv tmp include/mbedtls/build_info.h

[ $VERBOSE ] && echo "Bumping version in tests/suites/test_suite_version.data"
sed -e "s/version:\".\{1,\}/version:\"$VERSION\"/g" < tests/suites/test_suite_version.data > tmp
mv tmp tests/suites/test_suite_version.data

[ $VERBOSE ] && echo "Bumping PROJECT_NAME in doxygen/mbedtls.doxyfile and doxygen/input/doc_mainpage.h"
for i in doxygen/mbedtls.doxyfile doxygen/input/doc_mainpage.h;
do
  sed -e "s/mbed TLS v[0-9\.]\{1,\}/mbed TLS v$VERSION/g" < $i > tmp
  mv tmp $i
done

[ $VERBOSE ] && echo "Re-generating library/error.c"
scripts/generate_errors.pl

[ $VERBOSE ] && echo "Re-generating programs/test/query_config.c"
scripts/generate_query_config.pl

[ $VERBOSE ] && echo "Re-generating library/version_features.c"
scripts/generate_features.pl

[ $VERBOSE ] && echo "Re-generating visualc files"
scripts/generate_visualc_files.pl

