#!/bin/sh

cmake .. -DLWS_WITH_GCOV=1 && \
make clean && \
rm -f `find . -name "*.gcno" -o -name "*.gcda"` && \
make -j16 && sudo make install
