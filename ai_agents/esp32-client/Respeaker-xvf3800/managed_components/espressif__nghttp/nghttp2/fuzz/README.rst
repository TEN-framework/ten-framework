Fuzzer
======

This directory contains fuzzer target mainly written to integrate
nghttp2 into `oss-fuzz <https://github.com/google/oss-fuzz>`_.

fuzz_target.cc contains an entry point of fuzzer.  corpus directory
contains initial data for fuzzer.

The file name of initial data under corpus is the lower-cased hex
string of SHA-256 hash of its own content.

corpus/h2spec contains input data which was recorded when we ran
`h2spec <https://github.com/summerwind/h2spec>`_ against nghttpd.

corpus/nghttp contains input data which was recorded when we ran
nghttp against nghttpd with some varying command line options of
nghttp.


To build fuzz_target.cc, make sure that libnghttp2 is built with
following compiler/linker flags:

.. code-block:: text

    CPPFLAGS="-fsanitize-coverage=edge -fsanitize=address"
    LDFLAGS="-fsanitize-coverage=edge -fsanitize=address"

Then, fuzz_target.cc can be built using the following command:

.. code-block:: text

    $ clang++ -fsanitize-coverage=edge -fsanitize=address -I../lib/includes -std=c++11 fuzz_target.cc ../lib/.libs/libnghttp2.a  /usr/lib/llvm-3.9/lib/libFuzzer.a -o nghttp2_fuzzer
