// {{{ MIT License

// Copyright 2017 Roland Kaminski

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

// }}}

#ifndef PYCLINGO_CLINGO_H
#define PYCLINGO_CLINGO_H

#include <clingo.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
#  define CFFI_DLLEXPORT  __declspec(dllexport)
#elif defined(__GNUC__)
#  define CFFI_DLLEXPORT  __attribute__((visibility("default")))
#else
#  define CFFI_DLLEXPORT  /* nothing */
#endif

CFFI_DLLEXPORT bool pyclingo_execute(clingo_location_t const *loc, char const *code, void *);
CFFI_DLLEXPORT bool pyclingo_call(clingo_location_t const *loc, char const *name, clingo_symbol_t const *arguments, size_t size, clingo_symbol_callback_t symbol_callback, void *symbol_callback_data, void *);
CFFI_DLLEXPORT bool pyclingo_callable(char const * name, bool *ret, void *);
CFFI_DLLEXPORT bool pyclingo_main(clingo_control_t *ctl, void *);
bool clingo_register_python_();

#ifdef __cplusplus
}
#endif

#endif // PYCLINGO_CLINGO_H
