#!/bin/sh -e

BCPATH=doc/bash_completion

for prog in nghttp nghttpd nghttpx h2load; do
    $BCPATH/make_bash_completion.py src/$prog > $BCPATH/$prog
done
