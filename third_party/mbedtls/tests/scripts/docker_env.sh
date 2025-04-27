#!/bin/bash -eu

# docker_env.sh
#
# Purpose
# -------
#
# This is a helper script to enable running tests under a Docker container,
# thus making it easier to get set up as well as isolating test dependencies
# (which include legacy/insecure configurations of openssl and gnutls).
#
# Notes for users
# ---------------
# This script expects a Linux x86_64 system with a recent version of Docker
# installed and available for use, as well as http/https access. If a proxy
# server must be used, invoke this script with the usual environment variables
# (http_proxy and https_proxy) set appropriately. If an alternate Docker
# registry is needed, specify MBEDTLS_DOCKER_REGISTRY to point at the
# host name.
#
#
# Running this script directly will check for Docker availability and set up
# the Docker image.

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


# default values, can be overridden by the environment
: ${MBEDTLS_DOCKER_GUEST:=bionic}


DOCKER_IMAGE_TAG="armmbed/mbedtls-test:${MBEDTLS_DOCKER_GUEST}"

# Make sure docker is available
if ! which docker > /dev/null; then
    echo "Docker is required but doesn't seem to be installed. See https://www.docker.com/ to get started"
    exit 1
fi

# Figure out if we need to 'sudo docker'
if groups | grep docker > /dev/null; then
    DOCKER="docker"
else
    echo "Using sudo to invoke docker since you're not a member of the docker group..."
    DOCKER="sudo docker"
fi

# Figure out the number of processors available
if [ "$(uname)" == "Darwin" ]; then
    NUM_PROC="$(sysctl -n hw.logicalcpu)"
else
    NUM_PROC="$(nproc)"
fi

# Build the Docker image
echo "Getting docker image up to date (this may take a few minutes)..."
${DOCKER} image build \
    -t ${DOCKER_IMAGE_TAG} \
    --cache-from=${DOCKER_IMAGE_TAG} \
    --build-arg MAKEFLAGS_PARALLEL="-j ${NUM_PROC}" \
    --network host \
    ${http_proxy+--build-arg http_proxy=${http_proxy}} \
    ${https_proxy+--build-arg https_proxy=${https_proxy}} \
    ${MBEDTLS_DOCKER_REGISTRY+--build-arg MY_REGISTRY="${MBEDTLS_DOCKER_REGISTRY}/"} \
    tests/docker/${MBEDTLS_DOCKER_GUEST}

run_in_docker()
{
    ENV_ARGS=""
    while [ "$1" == "-e" ]; do
        ENV_ARGS="${ENV_ARGS} $1 $2"
        shift 2
    done

    ${DOCKER} container run -it --rm \
        --cap-add SYS_PTRACE \
        --user "$(id -u):$(id -g)" \
        --volume $PWD:$PWD \
        --workdir $PWD \
        -e MAKEFLAGS \
        -e PYLINTHOME=/tmp/.pylintd \
        ${ENV_ARGS} \
        ${DOCKER_IMAGE_TAG} \
        $@
}
