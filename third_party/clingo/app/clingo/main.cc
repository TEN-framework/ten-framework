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

#ifdef CLINGO_WITH_PYTHON
#   include <pyclingo.h>
#endif
#ifdef CLINGO_WITH_LUA
#   include <luaclingo.h>
#endif

#include <clingo.h>
#include <iostream>

extern "C" CLINGO_VISIBILITY_DEFAULT int clingo_main_(int argc, char *argv[]);

int main(int argc, char** argv) {
#   ifdef CLINGO_WITH_PYTHON
    if (!clingo_register_python_()) {
        std::cerr << clingo_error_message() << std::endl;
        return 1;
    }
#   endif
#   ifdef CLINGO_WITH_LUA
    if (!clingo_register_lua_(nullptr)) {
        std::cerr << clingo_error_message() << std::endl;
        return 1;
    }
#   endif
    return clingo_main_(argc, argv);
}

