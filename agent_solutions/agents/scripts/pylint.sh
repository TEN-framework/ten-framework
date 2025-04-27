#!/bin/bash

pylint ./agent_solutions/agents/ten_packages/extension/. || pylint-exit --warn-fail --error-fail $?
