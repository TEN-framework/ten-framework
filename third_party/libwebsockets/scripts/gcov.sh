#!/bin/sh

gcov `find . -name *.c.gcno | grep -v test-apps` -b | sed "/\.h.\$/,/^$/d"
