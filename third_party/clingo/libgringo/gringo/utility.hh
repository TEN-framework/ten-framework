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

#ifndef GRINGO_UTILITY_HH
#define GRINGO_UTILITY_HH

#include <memory>
#include <vector>
#include <set>
#include <functional>
#include <sstream>
#include <map>
#include <unordered_map>
#include <algorithm>
#include <cassert>
#include <iterator>
#include <type_traits>
#include <limits>
#include <potassco/basic_types.h>

namespace Gringo {
namespace detail {
    template <int X> using int_type = std::integral_constant<int, X>;
    template <class T, class S>
    inline void nc_check(S s, int_type<0> t) { // same sign
        static_cast<void>(s);
        static_cast<void>(t);
        assert((std::is_same<T, S>::value) || (s >= std::numeric_limits<T>::min() && s <= std::numeric_limits<T>::max()));
    }
    template <class T, class S>
    inline void nc_check(S s, int_type<-1> t) { // Signed -> Unsigned
        static_cast<void>(s);
        static_cast<void>(t);
        assert(s >= 0 && static_cast<S>(static_cast<T>(s)) == s);
    }
    template <class T, class S>
    inline void nc_check(S s, int_type<1> t) { // Unsigned -> Signed
        static_cast<void>(s);
        static_cast<void>(t);
        assert(!(s > static_cast<typename std::make_unsigned<T>::type>(std::numeric_limits<T>::max())));
    }
} // namespace detail

template <class T, class S>
inline T numeric_cast(S s) {
    constexpr int sv = int(std::numeric_limits<T>::is_signed) - int(std::numeric_limits<S>::is_signed);
    detail::nc_check<T>(s, detail::int_type<sv>());
    return static_cast<T>(s);
}

using Potassco::StringSpan;

// {{{1 declaration of single_owner_ptr<T>

template <class T>
class single_owner_ptr {
public:
    single_owner_ptr()
    : ptr_(0) { }
    explicit single_owner_ptr(T* ptr, bool owner)
    : ptr_(uintptr_t(ptr) | (owner && ptr)) { }
    single_owner_ptr(single_owner_ptr &&p) noexcept
    : ptr_(p.ptr_) {
        p.release();
    }
    single_owner_ptr(std::unique_ptr<T>&& p) noexcept // NOLINT
    : single_owner_ptr(p.release(), true) { }
    single_owner_ptr(const single_owner_ptr&) = delete;
    ~single_owner_ptr() {
        if (is_owner()) {
            delete get();
        }
    }
    single_owner_ptr& operator=(single_owner_ptr&& p) noexcept {
        bool owner = p.is_owner();
        reset(p.release(), owner);
        return *this;
    }
    single_owner_ptr& operator=(const single_owner_ptr&) = delete;
    bool is_owner() const {
        return (ptr_ & 1) == 1;
    }
    T* get() const {
        return (T*)(ptr_ & ~1);
    }
    T& operator*() const {
        return *get();
    }
    T* operator->() const {
        return  get();
    }
    T* release() {
        ptr_ = uintptr_t(get());
        return (T*)ptr_;
    }
    void reset(T* ptr, bool owner) {
        if (is_owner() && ptr != get()) {
            delete get();
        }
        ptr_ = uintptr_t(ptr) | uintptr_t(owner);
    }
    void swap(single_owner_ptr& p) {
        std::swap(ptr_, p.ptr_);
    }
private:
    uintptr_t ptr_;
};

template<typename T, typename ...Args>
single_owner_ptr<T> make_single_owner(Args&& ...args) {
    return single_owner_ptr<T>(new T(std::forward<Args>(args)...), true);
}

// {{{ declaration of gringo_make_unique<T, Args>

template<typename T, typename ...Args>
std::unique_ptr<T> gringo_make_unique(Args&& ...args);

// {{{1 declaration of helpers to perform comparisons

template <class T>
struct value_equal_to {
    bool operator()(T const &a, T const &b) const;
};

template <class T>
struct value_equal_to<T*> {
    bool operator()(T const *a, T const *b) const;
};

template <class T>
struct value_equal_to<std::unique_ptr<T>> {
    bool operator()(std::unique_ptr<T> const &a, std::unique_ptr<T> const &b) const;
};

template <class T>
struct value_equal_to<single_owner_ptr<T>> {
    bool operator()(single_owner_ptr<T> const &a, single_owner_ptr<T> const &b) const;
};

template <class T>
struct value_equal_to<std::reference_wrapper<T>> {
    bool operator()(std::reference_wrapper<T> const &a, std::reference_wrapper<T> const &b) const;
};

template <class T, class U>
struct value_equal_to<std::pair<T, U>> {
    bool operator()(std::pair<T, U> const &a, std::pair<T, U> const &b) const;
};

template <class... T>
struct value_equal_to<std::tuple<T...>> {
    bool operator()(std::tuple<T...> const &a, std::tuple<T...> const &b) const;
};

template <class... T>
struct value_equal_to<std::vector<T...>> {
    bool operator()(std::vector<T...> const &a, std::vector<T...> const &b) const;
};

template <class T>
bool is_value_equal_to(T const &a, T const &b);

// {{{1 declaration of helpers to calculate hashes

size_t hash_mix(size_t seed);

template <class T, class H=std::hash<T>>
void hash_combine(size_t &seed, T const &v, H h=H{});

template <class T>
size_t hash_range(T begin, T end);

template <class T, class H>
size_t hash_range(T begin, T end, H h);

template <class T>
struct value_hash {
    size_t operator()(T const &x) const;
};

template <class T>
struct value_hash<T*> {
    size_t operator()(T const *x) const;
};

template <class T>
struct value_hash<std::unique_ptr<T>> {
    size_t operator()(std::unique_ptr<T> const &p) const;
};

template <class T>
struct value_hash<single_owner_ptr<T>> {
    size_t operator()(single_owner_ptr<T> const &p) const;
};

template <class T>
struct value_hash<std::reference_wrapper<T>> {
    size_t operator()(std::reference_wrapper<T> const &x) const;
};

template <class T, class U>
struct value_hash<std::pair<T, U>> {
    size_t operator()(std::pair<T, U> const &v) const;
};

template <class... T>
struct value_hash<std::tuple<T...>> {
    size_t operator()(std::tuple<T...> const &x) const;
};

template <class T>
struct value_hash<std::vector<T>> {
    size_t operator()(std::vector<T> const &v) const;
};

template <class T>
struct value_hash<Potassco::Span<T>> {
    size_t operator()(Potassco::Span<T> const &v) const;
};

template <class T, class U=std::hash<T>>
struct mix_hash : U {
    using U::U;
    template <class K>
    size_t operator()(K const &x) const {
        size_t hash = U::operator()(x);
        hash_mix(hash);
        return hash;
    }
};

template <class T>
using mix_value_hash = mix_hash<T, value_hash<T>>;

template <class T>
inline size_t get_value_hash(T const &x);

template <class T, class U, class... V>
inline size_t get_value_hash(T const &x, U const &y, V const &... args);

inline size_t strhash(char const *x);
inline size_t strhash(StringSpan x);

// {{{1 declaration of helpers to perform clone copies

template <class T>
struct clone {
    T operator()(T const &x) const;
};

template <class T>
struct clone<T*> {
    T *operator()(T *x) const;
};

template <class T>
struct clone<std::unique_ptr<T>> {
    std::unique_ptr<T> operator()(std::unique_ptr<T> const &x) const;
};

template <class T>
struct clone<single_owner_ptr<T>> {
    single_owner_ptr<T> operator()(single_owner_ptr<T> const &x) const;
};

template <class... T>
struct clone<std::tuple<T...>> {
    std::tuple<T...> operator()(std::tuple<T...> const &x) const;
};

template <class T, class U>
struct clone<std::pair<T, U>> {
    std::pair<T, U> operator()(std::pair<T, U> const &x) const;
};

template <class T>
struct clone<std::vector<T>> {
    std::vector<T> operator()(std::vector<T> const &x) const;
};

template <class T>
T get_clone(T const &x);

// {{{1 declaration of algorithms

template <class S, class T, class U>
void print_comma(S &out, T const &x, const char *sep, U const &f);

template <class S, class T>
void print_comma(S &out, T const &x, const char *sep);

template <class T>
void cross_product(std::vector<std::vector<T>> &vec);

template <class Map, class Pred>
void erase_if(Map &m, Pred p);

template <class T, class Less>
void sort_unique(T &vec, Less less) {
    using E = decltype(*vec.begin());
    std::sort(vec.begin(), vec.end(), less);
    vec.erase(std::unique(vec.begin(), vec.end(), [less](E const &a, E const &b) { return !less(a,b) && !less(b,a); }), vec.end());
}

template <class T>
void sort_unique(T &vec) {
    sort_unique(vec, std::less<typename std::remove_reference<decltype(*vec.begin())>::type>());
}

template<class A, class B, class Pred>
void move_if(A &a, B &b, Pred p);

// {{{1 custom streams

class CountBuf : public std::streambuf {
public:
    CountBuf() = default;
    size_t count() const { return static_cast<size_t>(count_); }
protected:
    int_type overflow(int_type ch) override {
        count_++;
        return ch;
    }
    std::streamsize xsputn(char_type const *c, std::streamsize count) override {
        static_cast<void>(c);
        count_ += count;
        return count;
    }
private:
    std::streamsize count_ = 0;
};

class CountStream : public std::ostream {
public:
    CountStream() : std::ostream(&buf_) {
        exceptions(std::ios_base::badbit | std::ios_base::failbit | std::ios_base::eofbit);
    }
    size_t count() const { return buf_.count(); }
private:
    CountBuf buf_;
};

class ArrayBuf : public std::streambuf {
public:
    ArrayBuf(char *begin, size_t size) {
        // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        setg(begin, begin, begin + size);
        setp(begin, begin + size);
        // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    }
    pos_type seekoff(off_type off, std::ios_base::seekdir dir, std::ios_base::openmode which) override {
        if (dir == std::ios_base::cur)      { off += offset(which); }
        else if (dir == std::ios_base::end) { off = size() - off; }
        return seekpos(off, which);
    }
    pos_type seekpos(pos_type off, std::ios_base::openmode which) override {
        if (off >= 0 && off <= size()) {
            if ((which & std::ios_base::in) != 0) { gbump(static_cast<int>(off - offset(which))); }
            else                                  { pbump(static_cast<int>(off - offset(which))); }
            return off;
        }
        return std::streambuf::seekpos(off, which);
    }
private:
    off_type size() const { return egptr() - eback(); }
    off_type offset(std::ios_base::openmode which) const {
        return ((which & std::ios_base::out) != 0)
            ? pptr() - pbase()
            : gptr() - eback();
    }
};

class ArrayStream : public std::iostream {
public:
    ArrayStream(char *begin, size_t size)
    : std::iostream(&buf_)
    , buf_(begin, size) {
        exceptions(std::ios_base::badbit | std::ios_base::failbit | std::ios_base::eofbit);
    }
private:
    ArrayBuf buf_;
};

// {{{1 onExit

// NOTE: decorating with noexcept requires C++17

// NOLINTBEGIN(performance-noexcept-move-constructor)
template <class T>
class ScopeExit {
public:
    ScopeExit(T &&exit) : exit_(std::forward<T>(exit)) { }
    ScopeExit(ScopeExit const &other) = default;
    ScopeExit(ScopeExit &&other) = default;
    ScopeExit &operator=(ScopeExit const &other) = default;
    ScopeExit &operator=(ScopeExit &&other) = default;
    ~ScopeExit() {
        exit_();
    }
private:
    T exit_;
};
// NOLINTEND(performance-noexcept-move-constructor)

template <typename T>
ScopeExit<T> onExit(T &&exit) {
    return ScopeExit<T>(std::forward<T>(exit));
}

// }}}1

// {{{ definition of gringo_make_unique

template<typename T, typename ...Args>
std::unique_ptr<T> gringo_make_unique(Args&& ...args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

// }}}
// {{{ definition of helpers to perform comparisons

template <class T>
bool value_equal_to<T>::operator()(T const &a, T const &b) const {
    return std::equal_to<T>()(a, b);
}

template <class T>
bool value_equal_to<T*>::operator()(T const *a, T const *b) const {
    return is_value_equal_to(*a, *b);
}

template <class T>
bool value_equal_to<std::unique_ptr<T>>::operator()(std::unique_ptr<T> const &a, std::unique_ptr<T> const &b) const {
    assert(a && b);
    return is_value_equal_to(*a, *b);
}

template <class T>
bool value_equal_to<single_owner_ptr<T>>::operator()(single_owner_ptr<T> const &a, single_owner_ptr<T> const &b) const {
    assert(a && b);
    return is_value_equal_to(*a, *b);
}

template <class T>
bool value_equal_to<std::reference_wrapper<T>>::operator()(std::reference_wrapper<T> const &a, std::reference_wrapper<T> const &b) const {
    return is_value_equal_to(a.get(), b.get());
}

template <class T, class U>
bool value_equal_to<std::pair<T, U>>::operator()(std::pair<T, U> const &a, std::pair<T, U> const &b) const {
    return is_value_equal_to(a.first, b.first) && is_value_equal_to(a.second, b.second);
}

namespace detail {
    template <int N>
    struct equal {
        template <class... T>
        bool operator()(std::tuple<T...> const &a, std::tuple<T...> const &b) const {
            return is_value_equal_to(std::get<sizeof...(T) - N>(a), std::get<sizeof...(T) - N>(b)) && equal<N - 1>()(a, b);
        }
    };
    template <>
    struct equal<0> {
        template <class... T>
        bool operator()(std::tuple<T...> const &a, std::tuple<T...> const &b) const {
            static_cast<void>(a);
            static_cast<void>(b);
            return true;
        }
    };
}

template <class... T>
bool value_equal_to<std::tuple<T...>>::operator()(std::tuple<T...> const &a, std::tuple<T...> const &b) const {
    return detail::equal<sizeof...(T)>()(a, b);
}

template <class... T>
bool value_equal_to<std::vector<T...>>::operator()(std::vector<T...> const &a, std::vector<T...> const &b) const {
    if (a.size() != b.size()) { return false; }
    for (auto i = a.begin(), j = b.begin(), end = a.end(); i != end; ++i, ++j)
    {
        if (!is_value_equal_to(*i, *j)) { return false; }
    }
    return true;
}

template <class T>
inline bool is_value_equal_to(T const &a, T const &b) {
    return value_equal_to<T>()(a, b);
}

// }}}
// {{{ definition of helpers to calculate hashes

namespace Detail {

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)

template <size_t bytes> struct Select;
template <> struct Select<4> { using Type = uint32_t; };
template <> struct Select<8> { using Type = uint64_t; };

// Note: the following functions have been taken from MurmurHash3
//       see: https://code.google.com/p/smhasher/
inline uint32_t hash_mix(uint32_t h) {
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h;
}

inline uint64_t hash_mix(uint64_t h) {
    h ^= h >> 33;
    h *= 0xff51afd7ed558ccd;
    h ^= h >> 33;
    h *= 0xc4ceb9fe1a85ec53;
    h ^= h >> 33;
    return h;
}

inline uint32_t hash_combine(uint32_t seed, uint32_t h) {
    seed*= 0xcc9e2d51;
    seed = (seed >> 17) | (seed << 15);
    seed*= 0x1b873593;
    seed^= hash_mix(h);
    seed = (seed >> 19) | (seed << 13);
    seed = seed * 5 + 0xe6546b64;
    return seed;
}

inline uint64_t hash_combine(uint64_t seed, uint64_t h) {
    seed*= 0x87c37b91114253d5;
    seed = (seed >> 31) | (seed << 33);
    seed*= 0x4cf5ad432745937f;
    seed^= hash_mix(h);
    seed = (seed >> 27) | (seed << 37);
    seed = seed * 5 + 0x52dce729;
    return seed;
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers)

}

template <class T, class H>
void hash_combine(size_t &seed, T const &v, H h) {
    using type = typename Detail::Select<sizeof(size_t)>::Type;
    seed = Detail::hash_combine(static_cast<type>(seed), static_cast<type>(h(v)));
}

inline size_t hash_mix(size_t seed) {
    using type = typename Detail::Select<sizeof(size_t)>::Type;
    return Detail::hash_mix(static_cast<type>(seed));
}

template <class T>
size_t hash_range(T begin, T end) {
    return hash_range(begin, end, std::hash<typename std::remove_cv<typename std::remove_reference<decltype(*begin)>::type>::type>());
}

template <class T, class H>
size_t hash_range(T begin, T end, H h) {
    size_t seed = 0;
    for (auto it = begin; it != end; ++it) {
        hash_combine(seed, *it, h);
    }
    return seed;
}

template <class T>
inline size_t value_hash<T>::operator()(T const &x) const {
    return std::hash<T>()(x);
}

template <class T>
size_t value_hash<T*>::operator()(T const *x) const {
    return get_value_hash(*x);
}

template <class T>
size_t value_hash<std::unique_ptr<T>>::operator()(std::unique_ptr<T> const &p) const {
    assert(p);
    return get_value_hash(*p);
}

template <class T>
size_t value_hash<single_owner_ptr<T>>::operator()(single_owner_ptr<T> const &p) const {
    assert(p);
    return get_value_hash(*p);
}

template <class T>
size_t value_hash<std::reference_wrapper<T>>::operator()(std::reference_wrapper<T> const &x) const {
    return get_value_hash(x.get());
}

template <class T, class U>
size_t value_hash<std::pair<T, U>>::operator()(std::pair<T, U> const &v) const {
    size_t seed = 1;
    hash_combine(seed, get_value_hash(v.first));
    hash_combine(seed, get_value_hash(v.second));
    return seed;
}

namespace detail {
    template <int N>
    struct hash {
        template <class... T>
        size_t operator()(std::tuple<T...> const &x) const {
            size_t seed = get_value_hash(std::get<sizeof...(T) - N>(x));
            hash_combine(seed, hash<N-1>()(x));
            return seed;
        }
    };
    template <>
    struct hash<0> {
        template <class... T>
        size_t operator()(std::tuple<T...> const &x) const {
            static_cast<void>(x);
            return 2;
        }
    };
}

template <class... T>
inline size_t value_hash<std::tuple<T...>>::operator()(std::tuple<T...> const &x) const {
    return detail::hash<sizeof...(T)>()(x);
}

template <class T>
inline size_t value_hash<std::vector<T>>::operator()(std::vector<T> const &v) const {
    size_t seed = 3;
    for (auto const &x : v) {
        hash_combine(seed, get_value_hash(x));
    }
    return seed;
}

template <class T>
inline size_t value_hash<Potassco::Span<T>>::operator()(Potassco::Span<T> const &v) const {
    size_t seed = 4;
    for (auto const &x : v) {
        hash_combine(seed, get_value_hash(x));
    }
    return seed;
}

template <class T>
inline size_t get_value_hash(T const &x) {
    return value_hash<T>()(x);
}

template <class T, class U, class... V>
inline size_t get_value_hash(T const &x, U const &y, V const &... args) {
    size_t seed = value_hash<T>()(x);
    hash_combine(seed, get_value_hash(y, args...));
    return seed;
}

inline size_t strhash(char const *x) {
    size_t seed = 0;
    for ( ; *x != '\0'; ++x) { // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        hash_combine(seed, *x);
    }
    return seed;
}

inline size_t strhash(StringSpan x) {
    size_t seed = 0;
    for (auto const &c : x) {
        hash_combine(seed, c);
    }
    return seed;
}

// }}}
// {{{ definition of helpers to perform clone copies

template <class T>
inline T clone<T>::operator()(T const &x) const {
    return x;
}

template <class T>
inline T *clone<T*>::operator()(T *x) const {
    assert(x);
    return x->clone();
}

template <class T>
inline std::unique_ptr<T> clone<std::unique_ptr<T>>::operator()(std::unique_ptr<T> const &x) const {
    T *y(get_clone(x.get()));
    assert(x.get() != y);
    return std::unique_ptr<T>(y);
}

template <class T>
inline single_owner_ptr<T> clone<single_owner_ptr<T>>::operator()(single_owner_ptr<T> const &x) const {
    T *y(get_clone(x.get()));
    assert(x.get() != y);
    return single_owner_ptr<T>(y, true);
}

template <class T, class U>
inline std::pair<T, U> clone<std::pair<T, U>>::operator()(std::pair<T, U> const &x) const {
    return std::make_pair(get_clone(x.first), get_clone(x.second));
}

namespace detail {
    template <int ...>
    struct seq { };

    template <int N, int ...S>
    struct gens : gens<N-1, N-1, S...> { };

    template <int ...S>
    struct gens<0, S...> {
        using type = seq<S...>;
    };

    template <class... T, int... S>
    std::tuple<T...> clone(const std::tuple<T...> &x, seq<S...> s) {
        static_cast<void>(s);
        return std::tuple<T...> { get_clone(std::get<S>(x))... };
    }
}

template <class... T>
std::tuple<T...> clone<std::tuple<T...>>::operator()(std::tuple<T...> const &x) const {
    return detail::clone(x, typename detail::gens<sizeof...(T)>::type());
}

template <class T>
inline std::vector<T> clone<std::vector<T>>::operator()(std::vector<T> const &x) const {
    std::vector<T> res;
    res.reserve(x.size());
    for (auto &y : x) { res.emplace_back(get_clone(y)); }
    return res;
}

template <class T>
T get_clone(T const &x) {
    return clone<T>()(x);
}

// }}}
// {{{ definition of algorithms

template <class S, class T, class U>
void print_comma(S &out, T const &x, const char *sep, U const &f) {
    using std::begin;
    using std::end;
    auto it = begin(x);
    auto ie = end(x);
    if (it != ie) {
        f(out, *it);
        for (++it; it != ie; ++it) { out << sep; f(out, *it); }
    }
}

template <class S, class T>
void print_comma(S &out, T const &x, const char *sep) {
    using namespace std;
    auto it = begin(x);
    auto ie = end(x);
    if (it != ie) {
        out << *it;
        for (++it; it != ie; ++it) { out << sep << *it; }
    }
}

template <class T>
inline void cross_product(std::vector<std::vector<T>> &vec) {
    size_t size = 1;
    for (auto &x : vec) {
        size_t n = x.size();
        if (n == 0) {
            vec.clear();
            return;
        }
        size *= n;
    }
    std::vector<std::vector<T>> res;
    res.reserve(size);
    res.emplace_back();
    res.back().reserve(vec.size());
    for (auto &x : vec) {
        std::size_t it = 0; // res.begin();
        for (auto lt = x.begin(), mt = x.end() - 1; lt != mt; ++lt) {
            auto jt = it;
            it = res.size();
            auto kt = jt;
            for (; kt != it; ++kt) { res.emplace_back(get_clone(res[kt])); }
            for (kt = jt; kt != it - 1; ++kt) { res[kt].emplace_back(get_clone(*lt)); }
            res[kt].emplace_back(std::move(*lt));
        }
        for (auto kt = res.size() - 1; it != kt; ++it) { res[it].emplace_back(get_clone(x.back())); }
        res[it].emplace_back(std::move(x.back()));
    }
    vec = std::move(res);
}

template <class Map, class Pred>
void erase_if(Map &m, Pred p) {
    using namespace std;
    for (auto it = begin(m), ie = end(m); it != ie; ) {
        if (p(*it)) {
            it = m.erase(it);
        }
        else {
            ++it;
        }
    }
}

template<class A, class B, class Pred>
void move_if(A &a, B &b, Pred p) {
    using std::begin;
    auto first = begin(a);
    auto last = end(a);
    first = std::find_if(first, last, p);
    if (first != last) {
        b.emplace_back(std::move(*first));
        for(auto it = first; ++it != last; ) {
            if (!p(*it)) {
                *first++ = std::move(*it);
            }
            else {
                b.emplace_back(std::move(*it));
            }
        }
    }
    a.erase(first, last);
}

// }}}

} // namespace Gringo

#endif // GRINGO_UTILITY_HH
