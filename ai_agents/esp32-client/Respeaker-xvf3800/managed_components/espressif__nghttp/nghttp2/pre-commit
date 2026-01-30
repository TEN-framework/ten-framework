#!/bin/sh -e
#
# An example hook script to verify what is about to be committed.
# Called by "git commit" with no arguments.  The hook should
# exit with non-zero status after issuing an appropriate message if
# it wants to stop the commit.
#

CLANGFORMATDIFF=`git config --get clangformatdiff.binary`

if [ -z "$CLANGFORMATDIFF" ]; then
    CLANGFORMATDIFF=clang-format-diff.py
fi

errors=`git diff-index --cached --diff-filter=ACMR -p HEAD lib src examples tests | $CLANGFORMATDIFF -p1`

if [ -n "$errors" ]; then
    echo "$errors"
    echo "--"
    echo "[ERROR] We have detected the difference between the code to commit"
    echo "and clang-format style rules.  Please fix this problem in either:"
    echo "1) Apply patch above."
    echo "2) Use clang-format to format lines."
    echo "3) Reformat these lines manually."
    echo "Aborting commit."
    exit 1
fi
