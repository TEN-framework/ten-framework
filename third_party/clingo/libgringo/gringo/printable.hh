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

#ifndef GRINGO_PRINTABLE_HH
#define GRINGO_PRINTABLE_HH

#include <ostream>

namespace Gringo {

// {{{ defintion of Printable

class Printable {
public:
    Printable() = default;
    Printable(Printable const &other) = default;
    Printable(Printable && other) noexcept = default;
    Printable &operator=(Printable const &other) = default;
    Printable &operator=(Printable &&other) noexcept = default;
    virtual ~Printable() noexcept = default;

    virtual void print(std::ostream &out) const = 0;
};

inline std::ostream &operator<<(std::ostream &out, Printable const &x) {
    x.print(out);
    return out;
}

// }}}

} // namespace Gringo

#endif // GRINGO_PRINTABLE_HH
