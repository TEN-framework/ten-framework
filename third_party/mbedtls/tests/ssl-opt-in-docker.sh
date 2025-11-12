#!/bin/bash -eu

# ssl-opt-in-docker.sh
#
# Purpose
# -------
# This runs ssl-opt.sh in a Docker container.
#
# Notes for users
# ---------------
# If OPENSSL_CMD, GNUTLS_CLI, or GNUTLS_SERV are specified, the path must
# correspond to an executable inside the Docker container. The special
# values "next" and "legacy" are also allowed as shorthand for the
# installations inside the container.
#
# See also:
# - scripts/docker_env.sh for general Docker prerequisites and other information.
# - ssl-opt.sh for notes about invocation of that script.

# Copyright The Mbed TLS Contributors
# SPDX-License-Identifier: Apache-2.0
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may
# not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#  http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

source tests/scripts/docker_env.sh

case "${OPENSSL_CMD:-default}" in
    "legacy")  export OPENSSL_CMD="/usr/local/openssl-1.0.1j/bin/openssl";;
    "next")    export OPENSSL_CMD="/usr/local/openssl-1.1.1a/bin/openssl";;
    *) ;;
esac

case "${GNUTLS_CLI:-default}" in
    "legacy")  export GNUTLS_CLI="/usr/local/gnutls-3.3.8/bin/gnutls-cli";;
    "next")  export GNUTLS_CLI="/usr/local/gnutls-3.7.2/bin/gnutls-cli";;
    *) ;;
esac

case "${GNUTLS_SERV:-default}" in
    "legacy")  export GNUTLS_SERV="/usr/local/gnutls-3.3.8/bin/gnutls-serv";;
    "next")  export GNUTLS_SERV="/usr/local/gnutls-3.7.2/bin/gnutls-serv";;
    *) ;;
esac

run_in_docker \
    -e P_SRV \
    -e P_CLI \
    -e P_PXY \
    -e GNUTLS_CLI \
    -e GNUTLS_SERV \
    -e OPENSSL_CMD \
    tests/ssl-opt.sh \
    $@
