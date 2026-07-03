#!/usr/bin/env python3
"""Generate library/psa_crypto_driver_wrappers.c

   This module is invoked by the build scripts to auto generate the
   psa_crypto_driver_wrappers.c based on template files in
   script/data_files/driver_templates/.
"""
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

import sys
import os
import argparse
import jinja2
from mbedtls_dev import build_tree

def render(template_path: str) -> str:
    """
    Render template from the input file.
    """
    environment = jinja2.Environment(
        loader=jinja2.FileSystemLoader(os.path.dirname(template_path)),
        keep_trailing_newline=True)
    template = environment.get_template(os.path.basename(template_path))

    return template.render()

def generate_driver_wrapper_file(mbedtls_root: str, output_dir: str) -> None:
    """
    Generate the file psa_crypto_driver_wrapper.c.
    """
    driver_wrapper_template_filename = \
        os.path.join(mbedtls_root, \
        "scripts/data_files/driver_templates/psa_crypto_driver_wrappers.c.jinja")

    result = render(driver_wrapper_template_filename)

    with open(os.path.join(output_dir, "psa_crypto_driver_wrappers.c"), 'w') as out_file:
        out_file.write(result)

def main() -> int:
    """
    Main with command line arguments.
    """
    def_arg_mbedtls_root = build_tree.guess_mbedtls_root()
    def_arg_output_dir = os.path.join(def_arg_mbedtls_root, 'library')

    parser = argparse.ArgumentParser()
    parser.add_argument('--mbedtls-root', nargs='?', default=def_arg_mbedtls_root,
                        help='root directory of mbedtls source code')
    parser.add_argument('output_directory', nargs='?',
                        default=def_arg_output_dir, help='output file\'s location')
    args = parser.parse_args()

    mbedtls_root = os.path.abspath(args.mbedtls_root)
    output_directory = args.output_directory

    generate_driver_wrapper_file(mbedtls_root, output_directory)

    return 0

if __name__ == '__main__':
    sys.exit(main())
