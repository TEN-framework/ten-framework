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

#ifndef GRINGO_CLONABLE_HH
#define GRINGO_CLONABLE_HH

namespace Gringo {

// {{{ declaration of Clonable

template <class Base>
class Clonable {
public:
    Clonable() = default;
    Clonable(Clonable const &other) = default;
    Clonable(Clonable && other) noexcept = default;
    Clonable &operator=(Clonable const &other) = default;
    Clonable &operator=(Clonable &&other) noexcept = default;
    virtual ~Clonable() noexcept = default;

    virtual Base *clone() const = 0;
};

// }}}

} // namespace Gringo

#define GRINGO_CALL_CLONE(T) /* NOLINT */ \
namespace Gringo { \
template <> \
struct clone<T> { \
    inline T operator()(T const &x) const { /* NOLINT */ \
        return x.clone(); \
    } \
}; \
}

#endif // GRINGO_CLONABLE_HH

