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

#ifndef _GRINGO_TEST_TESTS_HH
#define _GRINGO_TEST_TESTS_HH

#include <catch2/catch_test_macros.hpp>
#include "gringo/backend.hh"
#include "gringo/utility.hh"
#include "gringo/logger.hh"
#include "gringo/base.hh"
#include "gringo/input/groundtermparser.hh"
#include <algorithm>
#include <unordered_map>

namespace Gringo {

// {{{ declaration of helpers to print containers

namespace IO {

template <class T>
std::ostream &operator<<(std::ostream &out, std::unique_ptr<T> const &x);
template <class... T>
std::ostream &operator<<(std::ostream &out, std::vector<T...> const &x);
template <class T>
std::ostream &operator<<(std::ostream &out, std::initializer_list<T> const &x);
template <class... T>
std::ostream &operator<<(std::ostream &out, std::set<T...> const &x);
template <class T, class U>
std::ostream &operator<<(std::ostream &out, std::pair<T, U> const &x);
template <class... T>
std::ostream &operator<<(std::ostream &out, std::tuple<T...> const &x);
template <class... T>
std::ostream &operator<<(std::ostream &out, std::map<T...> const &x);
template <class... T>
std::ostream &operator<<(std::ostream &out, std::unordered_map<T...> const &x);
template <class T>
std::string to_string(T const &x);

} // namespace IO

// }}}

// {{{ definition of helpers to print containers

namespace IO {

template <class T>
std::ostream &operator<<(std::ostream &out, std::unique_ptr<T> const &x) {
    out << *x;
    return out;
}

template <class T, class U>
std::ostream &operator<<(std::ostream &out, std::pair<T, U> const &x) {
    out << "(" << x.first << "," << x.second << ")";
    return out;
}

template <class... T>
std::ostream &operator<<(std::ostream &out, std::vector<T...> const &vec) {
    out << "[";
    auto it(vec.begin()), end(vec.end());
    if (it != end) {
        out << *it;
        for (++it; it != end; ++it) { out << "," << *it; }
    }
    out << "]";
    return out;
}

template <class T>
std::ostream &operator<<(std::ostream &out, std::initializer_list<T> const &vec) {
    out << "[";
    auto it(vec.begin()), end(vec.end());
    if (it != end) {
        out << *it;
        for (++it; it != end; ++it) { out << "," << *it; }
    }
    out << "]";
    return out;
}

template <class... T>
std::ostream &operator<<(std::ostream &out, std::set<T...> const &x) {
    out << "{";
    auto it(x.begin()), end(x.end());
    if (it != end) {
        out << *it;
        for (++it; it != end; ++it) { out << "," << *it; }
    }
    out << "}";
    return out;
}


namespace detail {
    template <int N>
    struct print {
        template <class... T>
        void operator()(std::ostream &out, std::tuple<T...> const &x) const {
            out << std::get<sizeof...(T) - N>(x) << ",";
            print<N-1>()(out, x);
        }
    };
    template <>
    struct print<1> {
        template <class... T>
        void operator()(std::ostream &out, std::tuple<T...> const &x) const {
            out << std::get<sizeof...(T) - 1>(x);
        }
    };
    template <>
    struct print<0> {
        template <class... T>
        void operator()(std::ostream &, std::tuple<T...> const &) const { }
    };
}

template <class... T>
std::ostream &operator<<(std::ostream &out, std::tuple<T...> const &x) {
    out << "(";
    detail::print<sizeof...(T)>()(out, x);
    out << ")";
    return out;
}

template <class... T>
std::ostream &operator<<(std::ostream &out, std::unordered_map<T...> const &x) {
    std::vector<std::pair<std::string, std::string>> vals;
    for (auto const &y : x) { vals.emplace_back(to_string(y.first), to_string(y.second)); }
    std::sort(vals.begin(), vals.end());
    out << "{";
    auto it = vals.begin(), end = vals.end();
    if (it != end) {
        out << it->first << ":" << it->second;
        for (++it; it != end; ++it) { out << "," << it->first << ":" << it->second; }
    }
    out << "}";
    return out;
}

template <class... T>
std::ostream &operator<<(std::ostream &out, std::map<T...> const &x) {
    out << "{";
    auto it = x.begin(), end = x.end();
    if (it != end) {
        out << it->first << ":" << it->second;
        for (++it; it != end; ++it) { out << "," << it->first << ":" << it->second; }
    }
    out << "}";
    return out;
}

template <class T>
std::string to_string(T const &x) {
    std::stringstream ss;
    ss << x;
    return ss.str();
}

} // namesapce IO

// }}}

inline std::string &replace_all(std::string &haystack, std::string const &needle, std::string const &replace) {
    size_t index = 0;
    while (true) {
        index = haystack.find(needle, index);
        if (index == std::string::npos) break;
        haystack.replace(index, needle.length(), replace);
        index += replace.length();
    }
    return haystack;
}

inline std::string replace_all(std::string &&haystack, std::string const &needle, std::string const &replace) {
    replace_all(haystack, needle, replace);
    return std::move(haystack);
}

}

namespace Gringo { namespace Test {

// {{{1 definition of helpers to initialize vectors

namespace Detail {

template <int, class, int, class...>
struct walker;

template <class, int, int, int, class...>
struct emplacer;

template <class V, int S1, int S2, int S3, class A, class... T>
struct emplacer<V, S1, S2, S3, A, T...> {
    void operator()(V& v, A&& a, T&&... args) const {
        // reverse
        emplacer<V, S1-1, S2, S3, T..., A>()(v, std::forward<T>(args)..., std::forward<A>(a));
    }
};

template <class V, int S2, int S3, class A, class... T>
struct emplacer<V, 0, S2, S3, A, T...> {
    void operator()(V& v, A&&, T&&... args) const {
        // drop
        emplacer<V, 0, S2-1, S3, T...>()(v, std::forward<T>(args)...);
    }
};

template <class V, int S3, class A, class... T>
struct emplacer<V, 0, 0, S3, A, T...> {
    void operator()(V& v, A&& a, T&&... args) const {
        // reverse
        emplacer<V, 0, 0, S3-1, T..., A>()(v, std::forward<T>(args)..., std::forward<A>(a));
    }
};

template <class V, class A, class... T>
struct emplacer<V, 0, 0, 0, A, T...> {
    void operator()(V &v, A&& a, T&&... args) const {
        v.emplace_back(std::forward<A>(a), std::forward<T>(args)...);
    }
};

template <int N, class V, int I, class A, class... T>
struct walker<N, V, I, A, T...> {
    void operator()(V& v, A&&, T&&... args) const {
        walker<N, V, I-1, T...>()(v, std::forward<T>(args)...);
    }
};

template <int N, class V, class A, class... T>
struct walker<N, V, 0, A, T...> {
    void operator()(V& v, A&& a, T&&... args) const {
        emplacer<V, N, sizeof...(T)+1-N, N, A, T...>()(v, std::forward<A>(a), std::forward<T>(args)...);
        walker<N, V, N-1, T...>()(v, std::forward<T>(args)...);
    }
};

// Note: optimization for N=1
template <class V, class A, class... T>
struct walker<1, V, 0, A, T...> {
    void operator()(V& v, A&& a, T&&... args) const {
        v.emplace_back(std::forward<A>(a));
        walker<1, V, 0, T...>()(v, std::forward<T>(args)...);
    }
};

template <int N, class V, int I>
struct walker<N, V, I> {
    static_assert(N > 0, "think - makes no sense!");
    void operator()(V&) const { }
};

} // namespace Detail

template <class V, class... T>
V init(T&&... args) {
    V v;
    Detail::walker<1, V, 0, T...>()(v, std::forward<T>(args)...);
    return v;
}

template <int N, class V, class... T>
V init(T&&... args) {
    static_assert(N > 0, "think - makes no sense!");
    V v;
    Detail::walker<N, V, 0, T...>()(v, std::forward<T>(args)...);
    return v;
}

// {{{1 definition of TestLogger

struct TestGringoModule {
    TestGringoModule()
    : logger([&](Warnings, char const *msg){
        messages_.emplace_back(msg);
    }, std::numeric_limits<unsigned>::max()) { }

    Gringo::Symbol parseValue(std::string const &str, Logger::Printer = nullptr, unsigned = 0) {
        return parser.parse(str, logger);
    }
    operator Logger &() { return logger; }
    void reset() { messages_.clear(); }
    std::vector<std::string> const &messages() const { return messages_; }
    Gringo::Input::GroundTermParser parser;
    std::vector<std::string> messages_;
    Logger logger;
};

struct TestContext : Context {
    bool callable(String) override { return false; }
    SymVec call(Location const &, String, SymSpan, Logger &) override { throw std::runtime_error("not implemented"); }
    void exec(String type, Location, String) override { throw std::runtime_error("not implemented"); }
};

inline std::ostream &operator<<(std::ostream &out, TestGringoModule const &mod) {
    using Gringo::IO::operator<<;
    out << mod.messages();
    return out;
}

// }}}1

} } // namespace Test Gringo

#endif // _GRINGO_TEST_TESTS_HH
