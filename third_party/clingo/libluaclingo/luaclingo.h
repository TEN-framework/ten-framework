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

#ifndef LUACLINGO_LUACLINGO_H
#define LUACLINGO_LUACLINGO_H

#include <clingo.h>

#if defined _WIN32 || defined __CYGWIN__
#   define LUACLINGO_WIN
#endif
#ifdef LUACLINGO_NO_VISIBILITY
#   define LUACLINGO_VISIBILITY_DEFAULT
#   define LUACLINGO_VISIBILITY_PRIVATE
#else
#   ifdef LUACLINGO_WIN
#       ifdef LUACLINGO_BUILD_LIBRARY
#           define LUACLINGO_VISIBILITY_DEFAULT __declspec (dllexport)
#       else
#           define LUACLINGO_VISIBILITY_DEFAULT __declspec (dllimport)
#       endif
#       define LUACLINGO_VISIBILITY_PRIVATE
#   else
#       if __GNUC__ >= 4
#           define LUACLINGO_VISIBILITY_DEFAULT  __attribute__ ((visibility ("default")))
#           define LUACLINGO_VISIBILITY_PRIVATE __attribute__ ((visibility ("hidden")))
#       else
#           define LUACLINGO_VISIBILITY_DEFAULT
#           define LUACLINGO_VISIBILITY_PRIVATE
#       endif
#   endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct lua_State;

LUACLINGO_VISIBILITY_DEFAULT int clingo_init_lua_(lua_State *L);
LUACLINGO_VISIBILITY_DEFAULT bool clingo_register_lua_(lua_State *L);

#ifdef __cplusplus
}
#endif

#endif // LUACLINGO_LUACLINGO_H


