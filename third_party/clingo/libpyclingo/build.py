from cffi import FFI

ffi = FFI()
ffi.set_source('clingo._clingo', '''\
#include <clingo.h>
void pyclingo_finalize() { }
''', libraries=['clingo'])
with open('_clingo.cdef') as f:
    ffi.cdef(f.read())
ffi.compile()
