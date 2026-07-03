#!/bin/bash
clingo --ifs="\n" "$@" convert.lp | grep "^_" | sed -e 's/$/./' -e 's/^_//'
