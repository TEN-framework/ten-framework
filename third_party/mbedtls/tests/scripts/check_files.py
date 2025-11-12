#!/usr/bin/env python3

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

"""
This script checks the current state of the source code for minor issues,
including incorrect file permissions, presence of tabs, non-Unix line endings,
trailing whitespace, and presence of UTF-8 BOM.
Note: requires python 3, must be run from Mbed TLS root.
"""

import os
import argparse
import logging
import codecs
import re
import subprocess
import sys
try:
    from typing import FrozenSet, Optional, Pattern # pylint: disable=unused-import
except ImportError:
    pass


class FileIssueTracker:
    """Base class for file-wide issue tracking.

    To implement a checker that processes a file as a whole, inherit from
    this class and implement `check_file_for_issue` and define ``heading``.

    ``suffix_exemptions``: files whose name ends with a string in this set
     will not be checked.

    ``path_exemptions``: files whose path (relative to the root of the source
    tree) matches this regular expression will not be checked. This can be
    ``None`` to match no path. Paths are normalized and converted to ``/``
    separators before matching.

    ``heading``: human-readable description of the issue
    """

    suffix_exemptions = frozenset() #type: FrozenSet[str]
    path_exemptions = None #type: Optional[Pattern[str]]
    # heading must be defined in derived classes.
    # pylint: disable=no-member

    def __init__(self):
        self.files_with_issues = {}

    @staticmethod
    def normalize_path(filepath):
        """Normalize ``filepath`` with / as the directory separator."""
        filepath = os.path.normpath(filepath)
        # On Windows, we may have backslashes to separate directories.
        # We need slashes to match exemption lists.
        seps = os.path.sep
        if os.path.altsep is not None:
            seps += os.path.altsep
        return '/'.join(filepath.split(seps))

    def should_check_file(self, filepath):
        """Whether the given file name should be checked.

        Files whose name ends with a string listed in ``self.suffix_exemptions``
        or whose path matches ``self.path_exemptions`` will not be checked.
        """
        for files_exemption in self.suffix_exemptions:
            if filepath.endswith(files_exemption):
                return False
        if self.path_exemptions and \
           re.match(self.path_exemptions, self.normalize_path(filepath)):
            return False
        return True

    def check_file_for_issue(self, filepath):
        """Check the specified file for the issue that this class is for.

        Subclasses must implement this method.
        """
        raise NotImplementedError

    def record_issue(self, filepath, line_number):
        """Record that an issue was found at the specified location."""
        if filepath not in self.files_with_issues.keys():
            self.files_with_issues[filepath] = []
        self.files_with_issues[filepath].append(line_number)

    def output_file_issues(self, logger):
        """Log all the locations where the issue was found."""
        if self.files_with_issues.values():
            logger.info(self.heading)
            for filename, lines in sorted(self.files_with_issues.items()):
                if lines:
                    logger.info("{}: {}".format(
                        filename, ", ".join(str(x) for x in lines)
                    ))
                else:
                    logger.info(filename)
            logger.info("")

BINARY_FILE_PATH_RE_LIST = [
    r'docs/.*\.pdf\Z',
    r'programs/fuzz/corpuses/[^.]+\Z',
    r'tests/data_files/[^.]+\Z',
    r'tests/data_files/.*\.(crt|csr|db|der|key|pubkey)\Z',
    r'tests/data_files/.*\.req\.[^/]+\Z',
    r'tests/data_files/.*malformed[^/]+\Z',
    r'tests/data_files/format_pkcs12\.fmt\Z',
]
BINARY_FILE_PATH_RE = re.compile('|'.join(BINARY_FILE_PATH_RE_LIST))

class LineIssueTracker(FileIssueTracker):
    """Base class for line-by-line issue tracking.

    To implement a checker that processes files line by line, inherit from
    this class and implement `line_with_issue`.
    """

    # Exclude binary files.
    path_exemptions = BINARY_FILE_PATH_RE

    def issue_with_line(self, line, filepath):
        """Check the specified line for the issue that this class is for.

        Subclasses must implement this method.
        """
        raise NotImplementedError

    def check_file_line(self, filepath, line, line_number):
        if self.issue_with_line(line, filepath):
            self.record_issue(filepath, line_number)

    def check_file_for_issue(self, filepath):
        """Check the lines of the specified file.

        Subclasses must implement the ``issue_with_line`` method.
        """
        with open(filepath, "rb") as f:
            for i, line in enumerate(iter(f.readline, b"")):
                self.check_file_line(filepath, line, i + 1)


def is_windows_file(filepath):
    _root, ext = os.path.splitext(filepath)
    return ext in ('.bat', '.dsp', '.dsw', '.sln', '.vcxproj')


class PermissionIssueTracker(FileIssueTracker):
    """Track files with bad permissions.

    Files that are not executable scripts must not be executable."""

    heading = "Incorrect permissions:"

    # .py files can be either full scripts or modules, so they may or may
    # not be executable.
    suffix_exemptions = frozenset({".py"})

    def check_file_for_issue(self, filepath):
        is_executable = os.access(filepath, os.X_OK)
        should_be_executable = filepath.endswith((".sh", ".pl"))
        if is_executable != should_be_executable:
            self.files_with_issues[filepath] = None


class ShebangIssueTracker(FileIssueTracker):
    """Track files with a bad, missing or extraneous shebang line.

    Executable scripts must start with a valid shebang (#!) line.
    """

    heading = "Invalid shebang line:"

    # Allow either /bin/sh, /bin/bash, or /usr/bin/env.
    # Allow at most one argument (this is a Linux limitation).
    # For sh and bash, the argument if present must be options.
    # For env, the argument must be the base name of the interpreter.
    _shebang_re = re.compile(rb'^#! ?(?:/bin/(bash|sh)(?: -[^\n ]*)?'
                             rb'|/usr/bin/env ([^\n /]+))$')
    _extensions = {
        b'bash': 'sh',
        b'perl': 'pl',
        b'python3': 'py',
        b'sh': 'sh',
    }

    def is_valid_shebang(self, first_line, filepath):
        m = re.match(self._shebang_re, first_line)
        if not m:
            return False
        interpreter = m.group(1) or m.group(2)
        if interpreter not in self._extensions:
            return False
        if not filepath.endswith('.' + self._extensions[interpreter]):
            return False
        return True

    def check_file_for_issue(self, filepath):
        is_executable = os.access(filepath, os.X_OK)
        with open(filepath, "rb") as f:
            first_line = f.readline()
        if first_line.startswith(b'#!'):
            if not is_executable:
                # Shebang on a non-executable file
                self.files_with_issues[filepath] = None
            elif not self.is_valid_shebang(first_line, filepath):
                self.files_with_issues[filepath] = [1]
        elif is_executable:
            # Executable without a shebang
            self.files_with_issues[filepath] = None


class EndOfFileNewlineIssueTracker(FileIssueTracker):
    """Track files that end with an incomplete line
    (no newline character at the end of the last line)."""

    heading = "Missing newline at end of file:"

    path_exemptions = BINARY_FILE_PATH_RE

    def check_file_for_issue(self, filepath):
        with open(filepath, "rb") as f:
            try:
                f.seek(-1, 2)
            except OSError:
                # This script only works on regular files. If we can't seek
                # 1 before the end, it means that this position is before
                # the beginning of the file, i.e. that the file is empty.
                return
            if f.read(1) != b"\n":
                self.files_with_issues[filepath] = None


class Utf8BomIssueTracker(FileIssueTracker):
    """Track files that start with a UTF-8 BOM.
    Files should be ASCII or UTF-8. Valid UTF-8 does not start with a BOM."""

    heading = "UTF-8 BOM present:"

    suffix_exemptions = frozenset([".vcxproj", ".sln"])
    path_exemptions = BINARY_FILE_PATH_RE

    def check_file_for_issue(self, filepath):
        with open(filepath, "rb") as f:
            if f.read().startswith(codecs.BOM_UTF8):
                self.files_with_issues[filepath] = None


class UnixLineEndingIssueTracker(LineIssueTracker):
    """Track files with non-Unix line endings (i.e. files with CR)."""

    heading = "Non-Unix line endings:"

    def should_check_file(self, filepath):
        if not super().should_check_file(filepath):
            return False
        return not is_windows_file(filepath)

    def issue_with_line(self, line, _filepath):
        return b"\r" in line


class WindowsLineEndingIssueTracker(LineIssueTracker):
    """Track files with non-Windows line endings (i.e. CR or LF not in CRLF)."""

    heading = "Non-Windows line endings:"

    def should_check_file(self, filepath):
        if not super().should_check_file(filepath):
            return False
        return is_windows_file(filepath)

    def issue_with_line(self, line, _filepath):
        return not line.endswith(b"\r\n") or b"\r" in line[:-2]


class TrailingWhitespaceIssueTracker(LineIssueTracker):
    """Track lines with trailing whitespace."""

    heading = "Trailing whitespace:"
    suffix_exemptions = frozenset([".dsp", ".md"])

    def issue_with_line(self, line, _filepath):
        return line.rstrip(b"\r\n") != line.rstrip()


class TabIssueTracker(LineIssueTracker):
    """Track lines with tabs."""

    heading = "Tabs present:"
    suffix_exemptions = frozenset([
        ".pem", # some openssl dumps have tabs
        ".sln",
        "/Makefile",
        "/Makefile.inc",
        "/generate_visualc_files.pl",
    ])

    def issue_with_line(self, line, _filepath):
        return b"\t" in line


class MergeArtifactIssueTracker(LineIssueTracker):
    """Track lines with merge artifacts.
    These are leftovers from a ``git merge`` that wasn't fully edited."""

    heading = "Merge artifact:"

    def issue_with_line(self, line, _filepath):
        # Detect leftover git conflict markers.
        if line.startswith(b'<<<<<<< ') or line.startswith(b'>>>>>>> '):
            return True
        if line.startswith(b'||||||| '): # from merge.conflictStyle=diff3
            return True
        if line.rstrip(b'\r\n') == b'=======' and \
           not _filepath.endswith('.md'):
            return True
        return False


class IntegrityChecker:
    """Sanity-check files under the current directory."""

    def __init__(self, log_file):
        """Instantiate the sanity checker.
        Check files under the current directory.
        Write a report of issues to log_file."""
        self.check_repo_path()
        self.logger = None
        self.setup_logger(log_file)
        self.issues_to_check = [
            PermissionIssueTracker(),
            ShebangIssueTracker(),
            EndOfFileNewlineIssueTracker(),
            Utf8BomIssueTracker(),
            UnixLineEndingIssueTracker(),
            WindowsLineEndingIssueTracker(),
            TrailingWhitespaceIssueTracker(),
            TabIssueTracker(),
            MergeArtifactIssueTracker(),
        ]

    @staticmethod
    def check_repo_path():
        if not all(os.path.isdir(d) for d in ["include", "library", "tests"]):
            raise Exception("Must be run from Mbed TLS root")

    def setup_logger(self, log_file, level=logging.INFO):
        self.logger = logging.getLogger()
        self.logger.setLevel(level)
        if log_file:
            handler = logging.FileHandler(log_file)
            self.logger.addHandler(handler)
        else:
            console = logging.StreamHandler()
            self.logger.addHandler(console)

    @staticmethod
    def collect_files():
        bytes_output = subprocess.check_output(['git', 'ls-files', '-z'])
        bytes_filepaths = bytes_output.split(b'\0')[:-1]
        ascii_filepaths = map(lambda fp: fp.decode('ascii'), bytes_filepaths)
        # Prepend './' to files in the top-level directory so that
        # something like `'/Makefile' in fp` matches in the top-level
        # directory as well as in subdirectories.
        return [fp if os.path.dirname(fp) else os.path.join(os.curdir, fp)
                for fp in ascii_filepaths]

    def check_files(self):
        for issue_to_check in self.issues_to_check:
            for filepath in self.collect_files():
                if issue_to_check.should_check_file(filepath):
                    issue_to_check.check_file_for_issue(filepath)

    def output_issues(self):
        integrity_return_code = 0
        for issue_to_check in self.issues_to_check:
            if issue_to_check.files_with_issues:
                integrity_return_code = 1
            issue_to_check.output_file_issues(self.logger)
        return integrity_return_code


def run_main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "-l", "--log_file", type=str, help="path to optional output log",
    )
    check_args = parser.parse_args()
    integrity_check = IntegrityChecker(check_args.log_file)
    integrity_check.check_files()
    return_code = integrity_check.output_issues()
    sys.exit(return_code)


if __name__ == "__main__":
    run_main()
