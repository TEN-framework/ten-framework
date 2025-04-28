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

#ifndef UTIL_PROGRAM_HH
#define UTIL_PROGRAM_HH

#include <utility>
#include <memory>
#include <functional>
#include <vector>
#include <iostream>
#include <potassco/basic_types.h>

namespace Reify {

inline std::ostream &operator<<(std::ostream &out, Potassco::StringSpan str) {
    out.write(str.first, str.size);
    return out;
}

template<typename Lambda>
class ScopeGuard {
public:
    ScopeGuard(Lambda &&f) : f_(std::forward<Lambda>(f)) { }
    ~ScopeGuard() { f_(); }
private:
    Lambda f_;
};

template<typename Lambda>
ScopeGuard<Lambda> makeScopeGuard(Lambda &&f) {
    return {std::forward<Lambda>(f)};
}

template<typename T, typename ...Args>
std::unique_ptr<T> gringo_make_unique(Args&& ...args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

template<typename T>
void printValue(std::ostream &out, T const &value) {
    out << value;
}

template<typename T, typename U>
void printValue(std::ostream &out, std::pair<T, U> const &value) {
    printValue(out, value.first);
    out << ",";
    printValue(out, value.second);
}

template<typename T>
void printComma(std::ostream &out, T const &t) {
    printValue(out, t);
}

template<typename T, typename U, typename... V>
void printComma(std::ostream &out, T const &t, U const &u, V const &...v) {
    printValue(out, t);
    out << ",";
    printComma(out, u, v...);
}

template<typename T>
struct Hash : std::hash<T> { };

template<typename T, typename U>
struct Hash<std::pair<T, U>> {
    size_t operator()(std::pair<T, U> const &p) const noexcept {
        size_t hash = std::hash<T>()(p.first);
        hash ^= Hash<U>()(p.second) + 0x9e3779b9 + (hash<<6) + (hash>>2);
        return hash;
    }
};

template<typename T>
struct Hash<std::vector<T>> {
    size_t operator()(std::vector<T> const &vec) const noexcept {
        size_t hash = vec.size();
        for (auto &x : vec) {
            hash ^= Hash<typename std::vector<T>::value_type>()(x) + 0x9e3779b9 + (hash<<6) + (hash>>2);
        }
        return hash;
    }
};

template <class T>
std::vector<T> toVec(Potassco::Span<T> span) {
    return {span.first, span.first + span.size};
}

} // namespace Reify

#endif // UTIL_PROGRAM_HH
