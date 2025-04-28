#!/bin/bash

pylint --rcfile=../tools/pylint/.pylintrc ./agents/ten_packages/extension/. || pylint-exit --warn-fail --error-fail $?
