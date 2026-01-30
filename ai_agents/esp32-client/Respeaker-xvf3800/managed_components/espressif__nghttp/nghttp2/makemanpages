#!/bin/sh -e

for prog in nghttp nghttpd nghttpx h2load; do
    src/$prog -h | ./help2rst.py -i doc/$prog.h2r > doc/$prog.1.rst
done

cd doc
make man

for prog in nghttp nghttpd nghttpx h2load; do
    cp manual/man/$prog.1 $prog.1
done
