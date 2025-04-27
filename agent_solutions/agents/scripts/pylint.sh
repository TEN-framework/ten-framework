#!/bin/bash

pylint --rcfile=tools/pylint/.pylintrc ./agent_solutions/agents/ten_packages/extension/. || pylint-exit --warn-fail --error-fail $?
