#!/bin/bash

# $1 is the input file, and the output after formatting should be dumpped to the stdout.

golines $1 --shorten-comments -m 80 --base-formatter="gofumpt"
