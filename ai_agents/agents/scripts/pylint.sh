#!/bin/bash

pylint --rcfile=../tools/pylint/.pylintrc --type-checking-mode=basic ./agents/ten_packages/extension/. || pylint-exit --warn-fail --error-fail $?
