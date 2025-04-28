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

#ifndef GRINGO_BUG_H
#define GRINGO_BUG_H

#include <iterator>

// Note 1: harmless because I don't have any throwing move constructors
// Note 2: should be first include

#undef _GLIBCXX_MAKE_MOVE_ITERATOR
#undef _GLIBCXX_MAKE_MOVE_IF_NOEXCEPT_ITERATOR

#define _GLIBCXX_MAKE_MOVE_ITERATOR(_Iter) std::make_move_iterator(_Iter) // NOLINT
#define _GLIBCXX_MAKE_MOVE_IF_NOEXCEPT_ITERATOR(_Iter) std::make_move_iterator(_Iter) // NOLINT

#ifdef MISSING_STD_TO_STRING

#include <sstream>

namespace std {

inline string to_string(int x) {
    ostringstream ss;
    ss << x;
    return ss.str();
}

inline string to_string(unsigned x) {
    ostringstream ss;
    ss << x;
    return ss.str();
}

inline string to_string(long x) {
    ostringstream ss;
    ss << x;
    return ss.str();
}

inline string to_string(unsigned long x) {
    ostringstream ss;
    ss << x;
    return ss.str();
}

inline string to_string(long long x) {
    ostringstream ss;
    ss << x;
    return ss.str();
}

inline string to_string(unsigned long long x) {
    ostringstream ss;
    ss << x;
    return ss.str();
}

} // namespace std

#endif // MISSING_STD_TO_STRING

#endif // GRINGO_BUG_H
