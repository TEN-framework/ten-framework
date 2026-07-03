#!/bin/bash

# Optional first argument to lint a specific path; default to all extensions
TARGET_PATH=./agents/ten_packages/extension/${1:-.}

pylint --rcfile=../tools/pylint/.pylintrc "$TARGET_PATH" || pylint-exit --warn-fail --error-fail $?
