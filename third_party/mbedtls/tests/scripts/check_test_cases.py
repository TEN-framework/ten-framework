#!/usr/bin/env python3

"""Sanity checks for test data.

This program contains a class for traversing test cases that can be used
independently of the checks.
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

import argparse
import glob
import os
import re
import sys

class Results:
    """Store file and line information about errors or warnings in test suites."""

    def __init__(self, options):
        self.errors = 0
        self.warnings = 0
        self.ignore_warnings = options.quiet

    def error(self, file_name, line_number, fmt, *args):
        sys.stderr.write(('{}:{}:ERROR:' + fmt + '\n').
                         format(file_name, line_number, *args))
        self.errors += 1

    def warning(self, file_name, line_number, fmt, *args):
        if not self.ignore_warnings:
            sys.stderr.write(('{}:{}:Warning:' + fmt + '\n')
                             .format(file_name, line_number, *args))
            self.warnings += 1

class TestDescriptionExplorer:
    """An iterator over test cases with descriptions.

The test cases that have descriptions are:
* Individual unit tests (entries in a .data file) in test suites.
* Individual test cases in ssl-opt.sh.

This is an abstract class. To use it, derive a class that implements
the process_test_case method, and call walk_all().
"""

    def process_test_case(self, per_file_state,
                          file_name, line_number, description):
        """Process a test case.

per_file_state: an object created by new_per_file_state() at the beginning
                of each file.
file_name: a relative path to the file containing the test case.
line_number: the line number in the given file.
description: the test case description as a byte string.
"""
        raise NotImplementedError

    def new_per_file_state(self):
        """Return a new per-file state object.

The default per-file state object is None. Child classes that require per-file
state may override this method.
"""
        #pylint: disable=no-self-use
        return None

    def walk_test_suite(self, data_file_name):
        """Iterate over the test cases in the given unit test data file."""
        in_paragraph = False
        descriptions = self.new_per_file_state() # pylint: disable=assignment-from-none
        with open(data_file_name, 'rb') as data_file:
            for line_number, line in enumerate(data_file, 1):
                line = line.rstrip(b'\r\n')
                if not line:
                    in_paragraph = False
                    continue
                if line.startswith(b'#'):
                    continue
                if not in_paragraph:
                    # This is a test case description line.
                    self.process_test_case(descriptions,
                                           data_file_name, line_number, line)
                in_paragraph = True

    def walk_ssl_opt_sh(self, file_name):
        """Iterate over the test cases in ssl-opt.sh or a file with a similar format."""
        descriptions = self.new_per_file_state() # pylint: disable=assignment-from-none
        with open(file_name, 'rb') as file_contents:
            for line_number, line in enumerate(file_contents, 1):
                # Assume that all run_test calls have the same simple form
                # with the test description entirely on the same line as the
                # function name.
                m = re.match(br'\s*run_test\s+"((?:[^\\"]|\\.)*)"', line)
                if not m:
                    continue
                description = m.group(1)
                self.process_test_case(descriptions,
                                       file_name, line_number, description)

    @staticmethod
    def collect_test_directories():
        """Get the relative path for the TLS and Crypto test directories."""
        if os.path.isdir('tests'):
            tests_dir = 'tests'
        elif os.path.isdir('suites'):
            tests_dir = '.'
        elif os.path.isdir('../suites'):
            tests_dir = '..'
        directories = [tests_dir]
        return directories

    def walk_all(self):
        """Iterate over all named test cases."""
        test_directories = self.collect_test_directories()
        for directory in test_directories:
            for data_file_name in glob.glob(os.path.join(directory, 'suites',
                                                         '*.data')):
                self.walk_test_suite(data_file_name)
            ssl_opt_sh = os.path.join(directory, 'ssl-opt.sh')
            if os.path.exists(ssl_opt_sh):
                self.walk_ssl_opt_sh(ssl_opt_sh)
            for ssl_opt_file_name in glob.glob(os.path.join(directory, 'opt-testcases',
                                                            '*.sh')):
                self.walk_ssl_opt_sh(ssl_opt_file_name)

class TestDescriptions(TestDescriptionExplorer):
    """Collect the available test cases."""

    def __init__(self):
        super().__init__()
        self.descriptions = set()

    def process_test_case(self, _per_file_state,
                          file_name, _line_number, description):
        """Record an available test case."""
        base_name = re.sub(r'\.[^.]*$', '', re.sub(r'.*/', '', file_name))
        key = ';'.join([base_name, description.decode('utf-8')])
        self.descriptions.add(key)

def collect_available_test_cases():
    """Collect the available test cases."""
    explorer = TestDescriptions()
    explorer.walk_all()
    return sorted(explorer.descriptions)

class DescriptionChecker(TestDescriptionExplorer):
    """Check all test case descriptions.

* Check that each description is valid (length, allowed character set, etc.).
* Check that there is no duplicated description inside of one test suite.
"""

    def __init__(self, results):
        self.results = results

    def new_per_file_state(self):
        """Dictionary mapping descriptions to their line number."""
        return {}

    def process_test_case(self, per_file_state,
                          file_name, line_number, description):
        """Check test case descriptions for errors."""
        results = self.results
        seen = per_file_state
        if description in seen:
            results.error(file_name, line_number,
                          'Duplicate description (also line {})',
                          seen[description])
            return
        if re.search(br'[\t;]', description):
            results.error(file_name, line_number,
                          'Forbidden character \'{}\' in description',
                          re.search(br'[\t;]', description).group(0).decode('ascii'))
        if re.search(br'[^ -~]', description):
            results.error(file_name, line_number,
                          'Non-ASCII character in description')
        if len(description) > 66:
            results.warning(file_name, line_number,
                            'Test description too long ({} > 66)',
                            len(description))
        seen[description] = line_number

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--list-all',
                        action='store_true',
                        help='List all test cases, without doing checks')
    parser.add_argument('--quiet', '-q',
                        action='store_true',
                        help='Hide warnings')
    parser.add_argument('--verbose', '-v',
                        action='store_false', dest='quiet',
                        help='Show warnings (default: on; undoes --quiet)')
    options = parser.parse_args()
    if options.list_all:
        descriptions = collect_available_test_cases()
        sys.stdout.write('\n'.join(descriptions + ['']))
        return
    results = Results(options)
    checker = DescriptionChecker(results)
    checker.walk_all()
    if (results.warnings or results.errors) and not options.quiet:
        sys.stderr.write('{}: {} errors, {} warnings\n'
                         .format(sys.argv[0], results.errors, results.warnings))
    sys.exit(1 if results.errors else 0)

if __name__ == '__main__':
    main()
