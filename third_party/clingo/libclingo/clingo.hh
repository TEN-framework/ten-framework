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

#ifndef CLINGO_HH
#define CLINGO_HH

#include <clingo.h>
#include <string>
#include <cstring>
#include <functional>
#include <ostream>
#include <algorithm>
#include <vector>
#include <cassert>
#include <type_traits>
#include <memory>
#include <tuple>
#include <forward_list>
#include <atomic>
#include <limits>

#include <iostream>

namespace Clingo {

constexpr unsigned g_message_limit = 20;

// {{{1 variant

namespace Detail {

template <class T, class U>
T cast(U u) {
    return reinterpret_cast<T>(u); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
}

template <class T, class... U>
struct TypeInList : std::false_type { };

template <class T, class... U>
struct TypeInList<T, T, U...> : std::true_type { };

template <class T, class V, class... U>
struct TypeInList<T, V, U...> : TypeInList<T, U...> { };

template <unsigned, class... U>
struct VariantHolder;

template <unsigned n>
struct VariantHolder<n> {
    bool check_type() const { return type_ == 0; }
    void emplace() { }
    void emplace2() { }
    void copy(VariantHolder const &h) {
        static_cast<void>(h);
    }
    void destroy() {
        type_ = 0;
        data_ = nullptr;
    }
    void print(std::ostream &out) const {
        static_cast<void>(out);
    }
    void swap(VariantHolder &other) noexcept {
        std::swap(type_, other.type_);
        std::swap(data_, other.data_);
    }
    unsigned type_ = 0;
    void *data_ = nullptr;
};

template <unsigned n, class T, class... U>
struct VariantHolder<n, T, U...> : VariantHolder<n+1, U...>{
    using Helper = VariantHolder<n+1, U...>;
    using Helper::check_type;
    using Helper::emplace;
    using Helper::emplace2;
    using Helper::data_;
    using Helper::type_;
    bool check_type(T *v) const {
        static_cast<void>(v);
        return type_ == n;
    }
    template <class... Args>
    void emplace(T *v, Args&& ...x) {
        static_cast<void>(v);
        data_ = new T{std::forward<Args>(x)...}; // NOLINT
        type_ = n;
    }
    // NOTE: http://www.open-std.org/jtc1/sc22/wg21/docs/cwg_defects.html#1467
    template <class... Args>
    void emplace2(T *v, Args&& ...x) {
        static_cast<void>(v);
        data_ = new T(std::forward<Args>(x)...); // NOLINT
        type_ = n;
    }
    void copy(VariantHolder const &src) {
        if (src.type_ == n) {
            data_ = new T(*static_cast<T const*>(src.data_)); // NOLINT
            type_ = src.type_;
        }
        Helper::copy(src);
    }
    // NOTE: workaround for visual studio (C++14 can also simply use auto)
#   define CLINGO_VARIANT_RETURN(Type) decltype(std::declval<V>().visit(std::declval<Type&>(), std::declval<Args>()...)) // NOLINT
    template <class V, class... Args>
    using Ret_ = CLINGO_VARIANT_RETURN(T);
    template <class V, class... Args>
    using ConstRet_ = CLINGO_VARIANT_RETURN(T const);
    // non-const
    template <class V, class U1, class... U2, class... Args>
    auto accept_(V &&visitor, Args &&... args) -> CLINGO_VARIANT_RETURN(T) {
        static_assert(std::is_same<Ret_<V, Args...>, typename Helper::template Ret_<V, Args...>>::value, "");
        return n == type_
            ? visitor.visit(*static_cast<T*>(data_), std::forward<Args>(args)...)
            : Helper::template accept<V>(std::forward<V>(visitor), std::forward<Args>(args)...);
    }
    template <class V, class... Args>
    auto accept_(V &&visitor, Args &&... args) -> CLINGO_VARIANT_RETURN(T) {
        assert(n == type_);
        return std::forward<V>(visitor).visit(*static_cast<T*>(data_), std::forward<Args>(args)...);
    }
    template <class V, class... Args>
    auto accept(V &&visitor, Args &&... args) -> CLINGO_VARIANT_RETURN(T) {
        return accept_<V, U...>(std::forward<V>(visitor), std::forward<Args>(args)...);
    }
    // const
    template <class V, class U1, class... U2, class... Args>
    auto accept_(V &&visitor, Args &&... args) const -> CLINGO_VARIANT_RETURN(T const) {
        static_assert(std::is_same<ConstRet_<V, Args...>, typename Helper::template ConstRet_<V, Args...>>::value, "");
        return n == type_
            ? visitor.visit(*static_cast<T const *>(data_), std::forward<Args>(args)...)
            : Helper::template accept<V>(std::forward<V>(visitor), std::forward<Args>(args)...);
    }
    template <class V, class... Args>
    auto accept_(V &&visitor, Args &&... args) const -> CLINGO_VARIANT_RETURN(T const) {
        assert(n == type_);
        return std::forward<V>(visitor).visit(*static_cast<T const *>(data_), std::forward<Args>(args)...);
    }
    template <class V, class... Args>
    auto accept(V &&visitor, Args &&... args) const -> CLINGO_VARIANT_RETURN(T const) {
        return accept_<V, U...>(std::forward<V>(visitor), std::forward<Args>(args)...);
    }
#   undef CLINGO_VARIANT_RETURN
    void destroy() {
        if (n == type_) { delete static_cast<T*>(data_); }
        Helper::destroy();
    }
    void print(std::ostream &out) const {
        if (n == type_) { out << *static_cast<T const*>(data_); }
        Helper::print(out);
    }
};

} // Detail

template <class T>
class Optional {
public:
    Optional() = default;
    ~Optional() = default;
    Optional(T const &x) : data_(new T(x)) { }
    Optional(T &x) : data_(new T(x)) { }
    Optional(T &&x) noexcept : data_(std::make_unique<T>(std::move(x))) { }
    template <class... Args>
    Optional(Args&&... x) : data_(new T{std::forward<Args>(x)...}) { }
    Optional(Optional &&opt) noexcept : data_(opt.data_.release()) { }
    Optional(Optional &opt) : data_(opt ? new T(*opt) : nullptr) { }
    Optional(Optional const &opt) : data_(opt ? new T(*opt) : nullptr) { }
    Optional &operator=(T const &x) {
        clear();
        data_.reset(new T(x));
        return *this;
    }
    Optional &operator=(T &x) {
        clear();
        data_.reset(new T(x));
        return *this;
    }
    Optional &operator=(T &&x) {
        clear();
        data_.reset(new T(std::move(x)));
        return *this;
    }
    Optional &operator=(Optional &&opt) noexcept {
        data_ = std::move(opt.data_);
        return *this;
    }
    Optional &operator=(Optional const &opt) {
        clear();
        data_.reset(opt ? new T(*opt) : nullptr);
        return *this;
    }
    T *get() { return data_.get(); }
    T const *get() const { return data_.get(); }
    T *operator->() { return get(); }
    T const *operator->() const { return get(); }
    T &operator*() & { return *get(); }
    T const &operator*() const & { return *get(); }
    T &&operator*() && { return std::move(*get()); }
    T const &&operator*() const && { return std::move(*get()); }
    template <class... Args>
    void emplace(Args&&... x) {
        clear();
        data_(new T{std::forward<Args>(x)...});
    }
    void clear() { data_.reset(nullptr); }
    explicit operator bool() const { return data_ != nullptr; }
private:
    std::unique_ptr<T> data_;
};

template <class... T>
class Variant {
    using Holder = Detail::VariantHolder<1, T...>;
public:
    Variant(Variant const &other) : Variant(other.data_) { }
    Variant(Variant &&other) noexcept { data_.swap(other.data_); }
    template <class U>
    Variant(U &&u, std::enable_if_t<Detail::TypeInList<U, T...>::value> *t = nullptr) {
        static_cast<void>(t);
        emplace2<U>(std::forward<U>(u));
    }
    template <class U>
    Variant(U &u, std::enable_if_t<Detail::TypeInList<U, T...>::value> *t = nullptr) {
        static_cast<void>(t);
        emplace2<U>(u);
    }
    template <class U>
    Variant(U const &u, typename std::enable_if_t<Detail::TypeInList<U, T...>::value> *t = nullptr) { emplace2<U>(u); }
    template <class U, class... Args>
    static Variant make(Args&& ...args) {
        Variant<T...> x;
        x.data_.emplace(static_cast<U*>(nullptr), std::forward<Args>(args)...);
        return x;
    }
    ~Variant() { data_.destroy(); }
    Variant &operator=(Variant const &other) {
        *this = other.data_;
        return *this;
    }
    Variant &operator=(Variant &&other) noexcept {
        *this = std::move(other.data_);
        return *this;
    }
    template <class U>
    typename std::enable_if<Detail::TypeInList<U, T...>::value, Variant>::type &operator=(U &&u) { // NOLINT
        emplace2<U>(std::forward<U>(u));
        return *this;
    }
    template <class U>
    typename std::enable_if<Detail::TypeInList<U, T...>::value, Variant>::type &operator=(U &u) { // NOLINT
        emplace2<U>(u);
        return *this;
    }
    template <class U>
    typename std::enable_if<Detail::TypeInList<U, T...>::value, Variant>::type &operator=(U const &u) { // NOLINT
        emplace2<U>(u);
        return *this;
    }
    template <class U>
    U &get() {
        if (!data_.check_type(static_cast<U*>(nullptr))) { throw std::bad_cast(); }
        return *static_cast<U*>(data_.data_);
    }
    template <class U>
    U const &get() const {
        if (!data_.check_type(static_cast<U*>(nullptr))) { throw std::bad_cast(); }
        return *static_cast<U*>(data_.data_);
    }
    template <class U, class... Args>
    void emplace(Args&& ...args) {
        Variant<T...> x;
        x.data_.emplace(static_cast<U*>(nullptr), std::forward<Args>(args)...);
        data_.swap(x.data_);
    }
    template <class U>
    bool is() const { return data_.check_type(static_cast<U*>(nullptr)); }
    void swap(Variant &other) noexcept { data_.swap(other.data_); }
    template <class V, class... Args>
    typename Holder::template Ret_<V, Args...> accept(V &&visitor, Args &&... args) {
        return data_.accept(std::forward<V>(visitor), std::forward<Args>(args)...);
    }
    template <class V, class... Args>
    typename Holder::template ConstRet_<V, Args...> accept(V &&visitor, Args &&... args) const {
        return data_.accept(std::forward<V>(visitor), std::forward<Args>(args)...);
    }
    friend std::ostream &operator<<(std::ostream &out, Variant const &x) {
        x.data_.print(out);
        return out;
    }

private:
    Variant() = default;
    Variant(Holder const &data) {
        data_.copy(data);
    }
    Variant &operator=(Holder const &data) {
        Variant x(data);
        data_.swap(x.data_);
        return *this;
    }
    Variant &operator=(Holder &&data) noexcept { // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
        Holder x;
        x.swap(data);
        // Destroy the old data_ only after securing the new data
        // Otherwise, data would be destroyed together with data_ if it was a descendant of data_
        data_.destroy();
        x.swap(data_);
        return *this;
    }
    template <class U, class... Args>
    void emplace2(Args&& ...args) {
        Variant<T...> x;
        x.data_.emplace2(static_cast<U*>(nullptr), std::forward<Args>(args)...);
        data_.swap(x.data_);
    }

    Holder data_;
};

// {{{1 span

template <class Iterator>
class IteratorRange {
public:
    using reference = typename Iterator::reference;
    using difference_type = typename Iterator::difference_type;
    IteratorRange(Iterator begin, Iterator end)
    : begin_(begin)
    , end_(end) { }
    reference operator[](difference_type n) {
        auto it = begin_;
        std::advance(it, n);
        return *it;
    }
    difference_type size() { return std::distance(begin_, end_); }
    bool empty() { return begin_ == end_; }
    Iterator begin() { return begin_; }
    Iterator end() { return end_; }
private:
    Iterator begin_;
    Iterator end_;
};

template <class Iterator>
IteratorRange<Iterator> make_range(Iterator ib, Iterator ie) {
    return {ib, ie};
}

template <class T>
class ValuePointer {
public:
    ValuePointer(T value) : value_(std::move(value)) { }
    T &operator*() { return value_; }
    T *operator->() { return &value_; }
private:
    T value_;
};

template <class T, class A=T*, class P=ValuePointer<T>>
class ArrayIterator {
public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type = T;
    using difference_type = ptrdiff_t;
    using pointer = P;
    using reference = T;

    explicit ArrayIterator(A arr, size_t index = 0)
    : arr_(std::move(arr))
    , index_(index) { }
    ArrayIterator& operator++() { ++index_; return *this; }
    ArrayIterator operator++(int) {
        ArrayIterator t(*this);
        ++*this;
        return t;
    }
    ArrayIterator& operator--() { --index_; return *this; }
    ArrayIterator operator--(int) {
        ArrayIterator t(*this);
        --*this;
        return t;
    }
    ArrayIterator& operator+=(difference_type n) { index_ += n; return *this; }
    ArrayIterator& operator-=(difference_type n) { index_ -= n; return *this; }
    friend ArrayIterator operator+(ArrayIterator it, difference_type n) { return ArrayIterator{it.arr_, it.index_ + n}; }
    friend ArrayIterator operator+(difference_type n, ArrayIterator it) { return ArrayIterator{it.arr_, it.index_ + n}; }
    friend ArrayIterator operator-(ArrayIterator it, difference_type n) { return ArrayIterator{it.arr_, it.index_ - n}; }
    friend difference_type operator-(ArrayIterator a, ArrayIterator b)  { return a.index_ - b.index_; }
    reference operator*() { return arr_->operator[](index_); }
    pointer operator->() { return arr_->at(index_); }
    friend void swap(ArrayIterator& lhs, ArrayIterator& rhs) noexcept {
        std::swap(lhs.arr_, rhs.arr_);
        std::swap(lhs.index_, rhs.index_);
    }
    friend bool operator==(ArrayIterator lhs, ArrayIterator rhs) { return lhs.index_ == rhs.index_; }
    friend bool operator!=(ArrayIterator lhs, ArrayIterator rhs) { return !(lhs == rhs); }
    friend bool operator< (ArrayIterator lhs, ArrayIterator rhs) { return (lhs.index_ + 1) < (rhs.index_ + 1); }
    friend bool operator> (ArrayIterator lhs, ArrayIterator rhs) { return rhs < lhs; }
    friend bool operator<=(ArrayIterator lhs, ArrayIterator rhs) { return !(lhs > rhs); }
    friend bool operator>=(ArrayIterator lhs, ArrayIterator rhs) { return !(lhs < rhs); }
private:
    A arr_;
    size_t index_;
};

template <class T>
struct ToIterator {
    T const *operator()(T const *x) const { return x; }
};

template <class T, class I = ToIterator<T>>
class Span : private I {
public:
#if __cplusplus >= 201703L
    using IteratorType = typename std::invoke_result_t<I, T const *>;
#else
    using IteratorType = std::result_of_t<I(T const *)>;
#endif
    using ReferenceType = decltype(*std::declval<IteratorType>());
    Span(I to_it = I())
    : Span(nullptr, size_t(0), to_it) { }
    template <class U>
    Span(U const *begin, size_t size)
    : Span(static_cast<T const *>(begin), size) { }
    Span(T const *begin, size_t size, I to_it = I())
    : Span(begin, begin + size, to_it) { } // NOLINT
    Span(std::initializer_list<T> c, I to_it = I())
    : Span(c.size() > 0 ? &*c.begin() : nullptr, c.size(), to_it) { }
    template <class U>
    Span(U const &c, I to_it = I())
    : Span(!c.empty() ? &*c.begin() : nullptr, c.size(), to_it) { }
    Span(T const *begin, T const *end, I to_it = I())
    : I(to_it)
    , begin_(begin)
    , end_(end) { }
    IteratorType begin() const { return I::operator()(begin_); }
    IteratorType end() const { return I::operator()(end_); }
    ReferenceType operator[](size_t offset) const { return *(begin() + offset); }
    ReferenceType front() const { return *begin(); }
    ReferenceType back() const { return *I::operator()(end_-1); }
    T const *data() const { return begin_; }
    size_t size() const { return end_ - begin_; }
    bool empty() const { return begin_ == end_; }
private:
    T const *begin_;
    T const *end_;
};

template <class T, class I>
inline typename Span<T, I>::IteratorType begin(Span<T, I> const &span) {
    return span.begin();
}

template <class T, class I>
inline typename Span<T, I>::IteratorType end(Span<T, I> const& span) {
    return span.end();
}

template <class T, class I = ToIterator<T>>
inline Span<T, I> make_span(T const *begin, size_t size, I to_it = I()) {
    return {begin, size, std::move(to_it)};
}

template <class T, class U>
bool equal_range(T const &a, U const &b) {
    using namespace std;
    return a.size() == b.size() && std::equal(begin(a), end(a), begin(b));
}

template <class T, class I>
bool operator==(Span<T, I> const &a, Span<T, I> const &b) { return equal_range(a, b); }

template <class T, class I>
std::ostream &operator<<(std::ostream &out, Span<T, I> const &span) {
    out << "{";
    bool comma = false;
    for (auto &x : span) {
        if (comma) { out << ", "; }
        else { out << " "; }
        out << x;
        comma = true;
    }
    out << " }";
    return out;
}

// {{{1 basic types

// consider using upper case
using literal_t = clingo_literal_t;
using id_t = clingo_id_t;
using weight_t = clingo_weight_t;
using atom_t = clingo_atom_t;

class WeightedLiteral {
public:
    WeightedLiteral(clingo_literal_t lit, clingo_weight_t weight)
    : wlit_{lit, weight} { }
    explicit WeightedLiteral(clingo_weighted_literal_t wlit)
    : wlit_(wlit) { }
    literal_t literal() const { return wlit_.literal; }
    weight_t weight() const { return wlit_.weight; }
    clingo_weighted_literal_t const &to_c() const { return wlit_; }
    clingo_weighted_literal_t &to_c() { return wlit_; }
private:
    clingo_weighted_literal_t wlit_;
};
using WeightedLiteralSpan = Span<WeightedLiteral>;

enum class TruthValue {
    Free  = clingo_truth_value_free,
    True  = clingo_truth_value_true,
    False = clingo_truth_value_false
};

inline std::ostream &operator<<(std::ostream &out, TruthValue tv) {
    switch (tv) {
        case TruthValue::Free:  { out << "Free"; break; }
        case TruthValue::True:  { out << "True"; break; }
        case TruthValue::False: { out << "False"; break; }
    }
    return out;
}

// {{{1 signature

class Signature {
public:
    explicit Signature(clingo_signature_t sig)
    : sig_(sig) { }
    Signature(char const *name, uint32_t arity, bool positive = true);
    char const *name() const;
    uint32_t arity() const;
    bool positive() const;
    bool negative() const;
    size_t hash() const;

    clingo_signature_t const &to_c() const { return sig_; }
    clingo_signature_t &to_c() { return sig_; }
private:
    clingo_signature_t sig_;
};

inline std::ostream &operator<<(std::ostream &out, Signature sig) {
    out << (sig.negative() ? "-" : "") << sig.name() << "/" << sig.arity();
    return out;
}
bool operator==(Signature a, Signature b);
bool operator!=(Signature a, Signature b);
bool operator< (Signature a, Signature b);
bool operator<=(Signature a, Signature b);
bool operator> (Signature a, Signature b);
bool operator>=(Signature a, Signature b);

} namespace std {

template<>
struct hash<Clingo::Signature> {
    size_t operator()(Clingo::Signature sig) const { return sig.hash(); }
};

} namespace Clingo {

// {{{1 symbol

enum class SymbolType : clingo_symbol_type_t {
    Infimum  = clingo_symbol_type_infimum,
    Number   = clingo_symbol_type_number,
    String   = clingo_symbol_type_string,
    Function = clingo_symbol_type_function,
    Supremum = clingo_symbol_type_supremum
};

class Symbol;
using SymbolSpan = Span<Symbol>;
using SymbolVector = std::vector<Symbol>;
using SymbolSpanCallback = std::function<void (SymbolSpan)>;

class Symbol {
public:
    Symbol();
    explicit Symbol(clingo_symbol_t c_sym);
    int number() const;
    char const *name() const;
    char const *string() const;
    bool is_positive() const;
    bool is_negative() const;
    SymbolSpan arguments() const;
    SymbolType type() const;
    std::string to_string() const;
    bool match(char const *name, unsigned arity) const;
    size_t hash() const;
    clingo_symbol_t &to_c() { return sym_; }
    clingo_symbol_t const &to_c() const { return sym_; }
private:
    clingo_symbol_t sym_ = 0;
};

Symbol Number(int num);
Symbol Supremum();
Symbol Infimum();
Symbol String(char const *str);
Symbol Id(char const *id, bool positive = true);
Symbol Function(char const *name, SymbolSpan args, bool positive = true);

std::ostream &operator<<(std::ostream &out, Symbol sym);
bool operator==(Symbol a, Symbol b);
bool operator!=(Symbol a, Symbol b);
bool operator< (Symbol a, Symbol b);
bool operator<=(Symbol a, Symbol b);
bool operator> (Symbol a, Symbol b);
bool operator>=(Symbol a, Symbol b);

} namespace std {

template<>
struct hash<Clingo::Symbol> {
    size_t operator()(Clingo::Symbol sym) const { return sym.hash(); }
};

} namespace Clingo {

// {{{1 symbolic atoms

class SymbolicAtom {
    friend class SymbolicAtomIterator;
public:
    explicit SymbolicAtom(clingo_symbolic_atoms_t const *atoms, clingo_symbolic_atom_iterator_t range)
    : atoms_(atoms)
    , range_(range) { }
    Symbol symbol() const;
    literal_t literal() const;
    bool is_fact() const;
    bool is_external() const;
    bool match(char const *name, unsigned arity) const;
    clingo_symbolic_atom_iterator_t to_c() const { return range_; }
private:
    clingo_symbolic_atoms_t const *atoms_;
    clingo_symbolic_atom_iterator_t range_;
};

class SymbolicAtomIterator : private SymbolicAtom {
public:
    using iterator_category = std::input_iterator_tag;
    using value_type = SymbolicAtom;
    using difference_type = ptrdiff_t;
    using pointer = SymbolicAtom*;
    using reference = SymbolicAtom&;

    explicit SymbolicAtomIterator(clingo_symbolic_atoms_t const *atoms, clingo_symbolic_atom_iterator_t range)
    : SymbolicAtom{atoms, range} { }
    SymbolicAtom &operator*() { return *this; }
    SymbolicAtom *operator->() { return this; }
    SymbolicAtomIterator &operator++();
    SymbolicAtomIterator operator++ (int) {
        auto range = range_;
        ++(*this);
        return SymbolicAtomIterator{atoms_, range};
    }
    bool operator==(SymbolicAtomIterator it) const;
    bool operator!=(SymbolicAtomIterator it) const { return !(*this == it); }
    explicit operator bool() const;
    clingo_symbolic_atom_iterator_t to_c() const { return range_; }
};

class SymbolicAtoms {
public:
    explicit SymbolicAtoms(clingo_symbolic_atoms_t const *atoms)
    : atoms_(atoms) { }
    SymbolicAtomIterator begin() const;
    SymbolicAtomIterator begin(Signature sig) const;
    SymbolicAtomIterator end() const;
    SymbolicAtomIterator find(Symbol atom) const;
    std::vector<Signature> signatures() const;
    size_t length() const;
    clingo_symbolic_atoms_t const *to_c() const { return atoms_; }
    SymbolicAtom operator[](Symbol atom) const { return *find(atom); }
private:
    clingo_symbolic_atoms_t const *atoms_;
};

// {{{1 theory atoms

enum class TheoryTermType : clingo_theory_term_type_t {
    Tuple = clingo_theory_term_type_tuple,
    List = clingo_theory_term_type_list,
    Set = clingo_theory_term_type_set,
    Function = clingo_theory_term_type_function,
    Number = clingo_theory_term_type_number,
    Symbol = clingo_theory_term_type_symbol
};

template <class T>
class TheoryIterator {
public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type = T const;
    using difference_type = ptrdiff_t;
    using pointer = T*;
    using reference = T;

    explicit TheoryIterator(clingo_theory_atoms_t const *atoms, clingo_id_t const* id)
    : elem_(atoms)
    , id_(id) { }
    TheoryIterator& operator++() { ++id_; return *this; } // NOLINT
    TheoryIterator operator++(int) {
        TheoryIterator t(*this);
        ++*this;
        return t;
    }
    TheoryIterator& operator--() { --id_; return *this; } // NOLINT
    TheoryIterator operator--(int) {
        TheoryIterator t(*this);
        --*this;
        return t;
    }
    TheoryIterator& operator+=(difference_type n) { id_ += n; return *this; } // NOLINT
    TheoryIterator& operator-=(difference_type n) { id_ -= n; return *this; } // NOLINT
    friend TheoryIterator operator+(TheoryIterator it, difference_type n) { return TheoryIterator{it.atoms(), it.id_ + n}; }
    friend TheoryIterator operator+(difference_type n, TheoryIterator it) { return TheoryIterator{it.atoms(), it.id_ + n}; }
    friend TheoryIterator operator-(TheoryIterator it, difference_type n) { return TheoryIterator{it.atoms(), it.id_ - n}; }
    friend difference_type operator-(TheoryIterator a, TheoryIterator b)  { return a.id_ - b.id_; }
    T operator*() { return elem_ = *id_; }
    T *operator->() { return &(elem_ = *id_); }
    friend void swap(TheoryIterator& lhs, TheoryIterator& rhs) noexcept {
        std::swap(lhs.id_, rhs.id_);
        std::swap(lhs.elem_, rhs.elem_);
    }
    friend bool operator==(TheoryIterator lhs, TheoryIterator rhs) { return lhs.id_ == rhs.id_; }
    friend bool operator!=(TheoryIterator lhs, TheoryIterator rhs) { return !(lhs == rhs); }
    friend bool operator< (TheoryIterator lhs, TheoryIterator rhs) { return lhs.id_ < rhs.id_; }
    friend bool operator> (TheoryIterator lhs, TheoryIterator rhs) { return rhs < lhs; }
    friend bool operator<=(TheoryIterator lhs, TheoryIterator rhs) { return !(lhs > rhs); }
    friend bool operator>=(TheoryIterator lhs, TheoryIterator rhs) { return !(lhs < rhs); }

private:
    clingo_theory_atoms_t const *&atoms() { return elem_.atoms_; }

    T                  elem_;
    clingo_id_t const *id_;
};

template <class T>
class ToTheoryIterator {
public:
    explicit ToTheoryIterator(clingo_theory_atoms_t const *atoms)
    : atoms_(atoms) { }
    T operator ()(clingo_id_t const *id) const {
        return T{atoms_, id};
    }
private:
    clingo_theory_atoms_t const *atoms_;
};

class TheoryTerm;
using TheoryTermIterator = TheoryIterator<TheoryTerm>;
using TheoryTermSpan = Span<clingo_id_t, ToTheoryIterator<TheoryTermIterator>>;

class TheoryTerm {
    friend class TheoryIterator<TheoryTerm>;
public:
    explicit TheoryTerm(clingo_theory_atoms_t const *atoms, clingo_id_t id)
    : atoms_(atoms)
    , id_(id) { }
    TheoryTermType type() const;
    int number() const;
    char const *name() const;
    TheoryTermSpan arguments() const;
    clingo_id_t to_c() const { return id_; }
    std::string to_string() const;
private:
    TheoryTerm(clingo_theory_atoms_t const *atoms)
    : TheoryTerm(atoms, 0) { }
    TheoryTerm &operator=(clingo_id_t id) {
        id_ = id;
        return *this;
    }

    clingo_theory_atoms_t const *atoms_;
    clingo_id_t id_;
};
std::ostream &operator<<(std::ostream &out, TheoryTerm term);

class TheoryElement;
using TheoryElementIterator = TheoryIterator<TheoryElement>;
using TheoryElementSpan = Span<clingo_id_t, ToTheoryIterator<TheoryElementIterator>>;
using LiteralSpan = Span<literal_t>;

class TheoryElement {
    friend class TheoryIterator<TheoryElement>;
public:
    explicit TheoryElement(clingo_theory_atoms_t const *atoms, clingo_id_t id)
    : atoms_(atoms)
    , id_(id) { }
    TheoryTermSpan tuple() const;
    LiteralSpan condition() const;
    literal_t condition_id() const;
    std::string to_string() const;
    clingo_id_t to_c() const { return id_; }
private:
    TheoryElement(clingo_theory_atoms_t const *atoms)
    : TheoryElement(atoms, 0) { }
    TheoryElement &operator=(clingo_id_t id) {
        id_ = id;
        return *this;
    }

    clingo_theory_atoms_t const *atoms_;
    clingo_id_t id_;
};
std::ostream &operator<<(std::ostream &out, TheoryElement term);

class TheoryAtom {
    friend class TheoryAtomIterator;
public:
    explicit TheoryAtom(clingo_theory_atoms_t const *atoms, clingo_id_t id)
    : atoms_(atoms)
    , id_(id) { }
    TheoryElementSpan elements() const;
    TheoryTerm term() const;
    bool has_guard() const;
    literal_t literal() const;
    std::pair<char const *, TheoryTerm> guard() const;
    std::string to_string() const;
    clingo_id_t to_c() const { return id_; }
private:
    TheoryAtom(clingo_theory_atoms_t const *atoms)
    : TheoryAtom(atoms, 0) { }
    TheoryAtom &operator=(clingo_id_t id) {
        id_ = id;
        return *this;
    }

    clingo_theory_atoms_t const *atoms_;
    clingo_id_t id_;
};
std::ostream &operator<<(std::ostream &out, TheoryAtom term);

class TheoryAtomIterator : private TheoryAtom {
public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type = TheoryAtom;
    using difference_type = ptrdiff_t;
    using pointer = TheoryAtom*;
    using reference = TheoryAtom;

    explicit TheoryAtomIterator(clingo_theory_atoms_t const *atoms, clingo_id_t id)
    : TheoryAtom{atoms, id} { }
    TheoryAtomIterator& operator++() { ++id_; return *this; }
    TheoryAtomIterator operator++(int) {
        auto t = *this;
        ++*this;
        return t;
    }
    TheoryAtomIterator& operator--() { --id_; return *this; }
    TheoryAtomIterator operator--(int) {
        auto t = *this;
        --*this;
        return t;
    }
    TheoryAtomIterator& operator+=(difference_type n) { id_ += static_cast<clingo_id_t>(n); return *this; }
    TheoryAtomIterator& operator-=(difference_type n) { id_ -= static_cast<clingo_id_t>(n); return *this; }
    friend TheoryAtomIterator operator+(TheoryAtomIterator it, difference_type n) { return TheoryAtomIterator{it.atoms(), clingo_id_t(it.id() + n)}; }
    friend TheoryAtomIterator operator+(difference_type n, TheoryAtomIterator it) { return TheoryAtomIterator{it.atoms(), clingo_id_t(it.id() + n)}; }
    friend TheoryAtomIterator operator-(TheoryAtomIterator it, difference_type n) { return TheoryAtomIterator{it.atoms(), clingo_id_t(it.id() - n)}; }
    friend difference_type operator-(TheoryAtomIterator a, TheoryAtomIterator b)  { return a.id() - b.id(); }
    TheoryAtom operator*() { return *this; }
    TheoryAtom *operator->() { return this; }
    friend void swap(TheoryAtomIterator& lhs, TheoryAtomIterator& rhs) noexcept {
        std::swap(lhs.atoms(), rhs.atoms());
        std::swap(lhs.id(), rhs.id());
    }
    friend bool operator==(TheoryAtomIterator lhs, TheoryAtomIterator rhs) { return lhs.atoms() == rhs.atoms() && lhs.id() == rhs.id(); }
    friend bool operator!=(TheoryAtomIterator lhs, TheoryAtomIterator rhs) { return !(lhs == rhs); }
    friend bool operator< (TheoryAtomIterator lhs, TheoryAtomIterator rhs) { assert(lhs.atoms() == rhs.atoms()); return (lhs.id() + 1) < (rhs.id() + 1); }
    friend bool operator> (TheoryAtomIterator lhs, TheoryAtomIterator rhs) { return rhs < lhs; }
    friend bool operator<=(TheoryAtomIterator lhs, TheoryAtomIterator rhs) { return !(lhs > rhs); }
    friend bool operator>=(TheoryAtomIterator lhs, TheoryAtomIterator rhs) { return !(lhs < rhs); }
private:
    clingo_theory_atoms_t const *&atoms() { return atoms_; }
    clingo_id_t &id() { return id_; }
};

class TheoryAtoms {
public:
    explicit TheoryAtoms(clingo_theory_atoms_t const *atoms)
    : atoms_(atoms) { }
    TheoryAtomIterator begin() const;
    TheoryAtomIterator end() const;
    size_t size() const;
    clingo_theory_atoms_t const *to_c() const { return atoms_; }
private:
    clingo_theory_atoms_t const *atoms_;
};

// {{{1 trail

template <class T, class U, class I>
class IndexIterator {
public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type = typename T::value_type;
    using difference_type = I;
    using pointer = value_type*;
    using reference = value_type;

    explicit IndexIterator(T *con = nullptr, U index = 0)
    : con_(con)
    , index_(index) { }
    IndexIterator& operator++() { ++index_; return *this; }
    IndexIterator operator++(int) {
        IndexIterator t(*this);
        ++*this;
        return t;
    }
    IndexIterator& operator--() { --index_; return *this; }
    IndexIterator operator--(int) {
        IndexIterator t(*this);
        --*this;
        return t;
    }
    IndexIterator& operator+=(difference_type n) { index_ += n; return *this; }
    IndexIterator& operator-=(difference_type n) { index_ -= n; return *this; }
    friend IndexIterator operator+(IndexIterator it, difference_type n) { return IndexIterator{it.con_, it.index_ + n}; }
    friend IndexIterator operator+(difference_type n, IndexIterator it) { return IndexIterator{it.con_, it.index_ + n}; }
    friend IndexIterator operator-(IndexIterator it, difference_type n) { return IndexIterator{it.con_, it.index_ - n}; }
    friend difference_type operator-(IndexIterator a, IndexIterator b)  { return a.index_ - b.index_; }
    value_type operator*() { return con_->at(index_); }
    friend void swap(IndexIterator& lhs, IndexIterator& rhs) noexcept {
        std::swap(lhs.con_, rhs.con_);
        std::swap(lhs.index_, rhs.index_);
    }
    friend bool operator==(IndexIterator lhs, IndexIterator rhs) { return lhs.index_ == rhs.index_; }
    friend bool operator!=(IndexIterator lhs, IndexIterator rhs) { return !(lhs == rhs); }
    friend bool operator< (IndexIterator lhs, IndexIterator rhs) { return lhs.index_ + 1 < rhs.index_ + 1; }
    friend bool operator> (IndexIterator lhs, IndexIterator rhs) { return rhs < lhs; }
    friend bool operator<=(IndexIterator lhs, IndexIterator rhs) { return !(lhs > rhs); }
    friend bool operator>=(IndexIterator lhs, IndexIterator rhs) { return !(lhs < rhs); }
private:
    T *con_;
    U index_;
};

class Trail {
public:
    using iterator = IndexIterator<Trail const, uint32_t, int32_t>;
    using value_type = literal_t;
    explicit Trail(clingo_assignment_t const *ass) : ass_{ass} { }
    uint32_t size() const;
    uint32_t begin_offset(uint32_t level) const;
    uint32_t end_offset(uint32_t level) const;
    iterator begin() const { return iterator{this, 0}; }
    iterator begin(uint32_t level) const { return iterator{this, begin_offset(level)}; }
    iterator end() const { return iterator{this, size()}; }
    iterator end(uint32_t level) const { return iterator{this, end_offset(level)}; }
    literal_t at(uint32_t offset) const;
    literal_t operator[](uint32_t offset) const { return at(offset); }
    clingo_assignment_t const *to_c() const { return ass_; }
private:
    clingo_assignment_t const *ass_;
};

// {{{1 assignment

class Assignment {
public:
    using iterator = IndexIterator<Assignment const, size_t, std::make_signed_t<size_t>>;
    using value_type = literal_t;
    explicit Assignment(clingo_assignment_t const *ass)
    : ass_(ass) { }
    bool has_conflict() const;
    uint32_t decision_level() const;
    uint32_t root_level() const;
    bool has_literal(literal_t lit) const;
    TruthValue truth_value(literal_t lit) const;
    uint32_t level(literal_t lit) const;
    literal_t decision(uint32_t level) const;
    bool is_fixed(literal_t lit) const;
    bool is_true(literal_t lit) const;
    bool is_false(literal_t lit) const;
    size_t size() const;
    bool is_total() const;
    literal_t at(size_t offset) const;
    literal_t operator[](size_t offset) const { return at(offset); }
    iterator begin() const { return iterator{this, 0}; }
    iterator end() const { return iterator{this, size()}; }
    Trail trail() const { return Trail{ass_}; }
    clingo_assignment_t const *to_c() const { return ass_; }
private:
    clingo_assignment_t const *ass_;
};

// {{{1 propagate init

enum PropagatorCheckMode : clingo_propagator_check_mode_t {
    None    = clingo_propagator_check_mode_none,
    Total   = clingo_propagator_check_mode_total,
    Partial = clingo_propagator_check_mode_fixpoint,
    Both    = clingo_propagator_check_mode_both,
};

enum class PropagatorUndoMode : clingo_propagator_undo_mode_t {
    Default = clingo_propagator_undo_mode_default,
    Always  = clingo_propagator_undo_mode_always,
};

enum WeightConstraintType : clingo_weight_constraint_type_t {
    LeftImplication  = clingo_weight_constraint_type_implication_left,
    RightImplication = clingo_weight_constraint_type_implication_right,
    Equivalence      = clingo_weight_constraint_type_equivalence,
};

class PropagateInit {
public:
    explicit PropagateInit(clingo_propagate_init_t *init)
    : init_(init) { }
    literal_t solver_literal(literal_t lit) const;
    void add_watch(literal_t lit);
    void add_watch(literal_t literal, id_t thread_id);
    void remove_watch(literal_t lit);
    void remove_watch(literal_t literal, id_t thread_id);
    void freeze_literal(literal_t lit);
    int number_of_threads() const;
    Assignment assignment() const;
    SymbolicAtoms symbolic_atoms() const;
    TheoryAtoms theory_atoms() const;
    PropagatorCheckMode get_check_mode() const;
    PropagatorUndoMode get_undo_mode() const;
    void set_check_mode(PropagatorCheckMode mode);
    void set_undo_mode(PropagatorUndoMode mode);
    literal_t add_literal(bool freeze = true);
    bool add_clause(LiteralSpan clause);
    bool add_weight_constraint(literal_t literal, WeightedLiteralSpan literals, weight_t bound, WeightConstraintType type, bool compare_equal = false);
    void add_minimize(literal_t literal, weight_t weight, weight_t priority = 0);
    bool propagate();
    clingo_propagate_init_t *to_c() const { return init_; }
private:
    clingo_propagate_init_t *init_;
};

// {{{1 propagate control

enum class ClauseType : clingo_clause_type_t {
    Learnt = clingo_clause_type_learnt,
    Static = clingo_clause_type_static,
    Volatile = clingo_clause_type_volatile,
    VolatileStatic = clingo_clause_type_volatile_static
};

inline std::ostream &operator<<(std::ostream &out, ClauseType t) {
    switch (t) {
        case ClauseType::Learnt:         { out << "Learnt"; break; }
        case ClauseType::Static:         { out << "Static"; break; }
        case ClauseType::Volatile:       { out << "Volatile"; break; }
        case ClauseType::VolatileStatic: { out << "VolatileStatic"; break; }
    }
    return out;
}

class PropagateControl {
public:
    explicit PropagateControl(clingo_propagate_control_t *ctl)
    : ctl_(ctl) { }
    id_t thread_id() const;
    Assignment assignment() const;
    literal_t add_literal();
    void add_watch(literal_t literal);
    bool has_watch(literal_t literal) const;
    void remove_watch(literal_t literal);
    bool add_clause(LiteralSpan clause, ClauseType type = ClauseType::Learnt);
    bool propagate();
    clingo_propagate_control_t *to_c() const { return ctl_; }
private:
    clingo_propagate_control_t *ctl_;
};

// {{{1 propagator

class Propagator {
public:
    Propagator() = default;
    Propagator(Propagator const &) = default;
    Propagator(Propagator &&) noexcept = default;
    Propagator &operator=(Propagator const &) = default;
    Propagator &operator=(Propagator &&) noexcept = default;
    virtual void init(PropagateInit &init);
    virtual void propagate(PropagateControl &ctl, LiteralSpan changes);
    virtual void undo(PropagateControl const &ctl, LiteralSpan changes) noexcept;
    virtual void check(PropagateControl &ctl);
    virtual ~Propagator() = default;
};

class Heuristic : public Propagator {
public:
    Heuristic() = default;
    Heuristic(Heuristic const &) = default;
    Heuristic(Heuristic &&) noexcept = default;
    Heuristic &operator=(Heuristic const &) = default;
    Heuristic &operator=(Heuristic &&) noexcept = default;
    virtual literal_t decide(id_t thread_id, Assignment const &assign, literal_t fallback);
    ~Heuristic() override = default;
};

// {{{1 ground program observer

using IdSpan = Span<id_t>;
using AtomSpan = Span<atom_t>;

enum class HeuristicType : clingo_heuristic_type_t {
    Level  = clingo_heuristic_type_level,
    Sign   = clingo_heuristic_type_sign,
    Factor = clingo_heuristic_type_factor,
    Init   = clingo_heuristic_type_init,
    True   = clingo_heuristic_type_true,
    False  = clingo_heuristic_type_false
};

inline std::ostream &operator<<(std::ostream &out, HeuristicType t) {
    switch (t) {
        case HeuristicType::Level:  { out << "Level"; break; }
        case HeuristicType::Sign:   { out << "Sign"; break; }
        case HeuristicType::Factor: { out << "Factor"; break; }
        case HeuristicType::Init:   { out << "Init"; break; }
        case HeuristicType::True:   { out << "True"; break; }
        case HeuristicType::False:  { out << "False"; break; }
    }
    return out;
}

enum class ExternalType {
    Free    = clingo_external_type_free,
    True    = clingo_external_type_true,
    False   = clingo_external_type_false,
    Release = clingo_external_type_release
};

inline std::ostream &operator<<(std::ostream &out, ExternalType t) {
    switch (t) {
        case ExternalType::Free:    { out << "Free"; break; }
        case ExternalType::True:    { out << "True"; break; }
        case ExternalType::False:   { out << "False"; break; }
        case ExternalType::Release: { out << "Release"; break; }
    }
    return out;
}

class GroundProgramObserver {
public:
    GroundProgramObserver() = default;
    GroundProgramObserver(GroundProgramObserver const &) = default;
    GroundProgramObserver(GroundProgramObserver &&) noexcept = default;
    GroundProgramObserver &operator=(GroundProgramObserver const &) = default;
    GroundProgramObserver &operator=(GroundProgramObserver &&) noexcept = default;

    virtual void init_program(bool incremental);
    virtual void begin_step();
    virtual void end_step();

    virtual void rule(bool choice, AtomSpan head, LiteralSpan body);
    virtual void weight_rule(bool choice, AtomSpan head, weight_t lower_bound, WeightedLiteralSpan body);
    virtual void minimize(weight_t priority, WeightedLiteralSpan literals);
    virtual void project(AtomSpan atoms);
    virtual void output_atom(Symbol symbol, atom_t atom);
    virtual void output_term(Symbol symbol, LiteralSpan condition);
    virtual void external(atom_t atom, ExternalType type);
    virtual void assume(LiteralSpan literals);
    virtual void heuristic(atom_t atom, HeuristicType type, int bias, unsigned priority, LiteralSpan condition);
    virtual void acyc_edge(int node_u, int node_v, LiteralSpan condition);

    virtual void theory_term_number(id_t term_id, int number);
    virtual void theory_term_string(id_t term_id, char const *name);
    virtual void theory_term_compound(id_t term_id, int name_id_or_type, IdSpan arguments);
    virtual void theory_element(id_t element_id, IdSpan terms, LiteralSpan condition);
    virtual void theory_atom(id_t atom_id_or_zero, id_t term_id, IdSpan elements);
    virtual void theory_atom_with_guard(id_t atom_id_or_zero, id_t term_id, IdSpan elements, id_t operator_id, id_t right_hand_side_id);
    virtual ~GroundProgramObserver() = default;
};

inline void GroundProgramObserver::init_program(bool incremental) {
    static_cast<void>(incremental);
}
inline void GroundProgramObserver::begin_step() { }
inline void GroundProgramObserver::end_step() { }

inline void GroundProgramObserver::rule(bool choice, AtomSpan head, LiteralSpan body) {
    static_cast<void>(choice);
    static_cast<void>(head);
    static_cast<void>(body);
}

inline void GroundProgramObserver::weight_rule(bool choice, AtomSpan head, weight_t lower_bound, WeightedLiteralSpan body) {
    static_cast<void>(choice);
    static_cast<void>(head);
    static_cast<void>(lower_bound);
    static_cast<void>(body);
}

inline void GroundProgramObserver::minimize(weight_t priority, WeightedLiteralSpan literals) {
    static_cast<void>(priority);
    static_cast<void>(literals);
}

inline void GroundProgramObserver::project(AtomSpan atoms) {
    static_cast<void>(atoms);
}

inline void GroundProgramObserver::output_atom(Symbol symbol, atom_t atom) {
    static_cast<void>(symbol);
    static_cast<void>(atom);
}

inline void GroundProgramObserver::output_term(Symbol symbol, LiteralSpan condition) {
    static_cast<void>(symbol);
    static_cast<void>(condition);
}

inline void GroundProgramObserver::external(atom_t atom, ExternalType type) {
    static_cast<void>(atom);
    static_cast<void>(type);
}

inline void GroundProgramObserver::assume(LiteralSpan literals) {
    static_cast<void>(literals);
}

inline void GroundProgramObserver::heuristic(atom_t atom, HeuristicType type, int bias, unsigned priority, LiteralSpan condition) {
    static_cast<void>(atom);
    static_cast<void>(type);
    static_cast<void>(bias);
    static_cast<void>(priority);
    static_cast<void>(condition);
}

inline void GroundProgramObserver::acyc_edge(int node_u, int node_v, LiteralSpan condition) {
    static_cast<void>(node_u);
    static_cast<void>(node_v);
    static_cast<void>(condition);
}


inline void GroundProgramObserver::theory_term_number(id_t term_id, int number) {
    static_cast<void>(term_id);
    static_cast<void>(number);
}

inline void GroundProgramObserver::theory_term_string(id_t term_id, char const *name) {
    static_cast<void>(term_id);
    static_cast<void>(name);
}

inline void GroundProgramObserver::theory_term_compound(id_t term_id, int name_id_or_type, IdSpan arguments) {
    static_cast<void>(term_id);
    static_cast<void>(name_id_or_type);
    static_cast<void>(arguments);
}

inline void GroundProgramObserver::theory_element(id_t element_id, IdSpan terms, LiteralSpan condition) {
    static_cast<void>(element_id);
    static_cast<void>(terms);
    static_cast<void>(condition);
}

inline void GroundProgramObserver::theory_atom(id_t atom_id_or_zero, id_t term_id, IdSpan elements) {
    static_cast<void>(atom_id_or_zero);
    static_cast<void>(term_id);
    static_cast<void>(elements);
}

inline void GroundProgramObserver::theory_atom_with_guard(id_t atom_id_or_zero, id_t term_id, IdSpan elements, id_t operator_id, id_t right_hand_side_id) {
    static_cast<void>(atom_id_or_zero);
    static_cast<void>(term_id);
    static_cast<void>(elements);
    static_cast<void>(operator_id);
    static_cast<void>(right_hand_side_id);
}


// {{{1 symbolic literal

class SymbolicLiteral {
public:
    SymbolicLiteral(Symbol sym, bool positive)
    : symbol_{sym.to_c()}
    , positive_{positive} { }
    Symbol symbol() const { return Symbol{symbol_}; }
    bool is_positive() const { return positive_; }
    bool is_negative() const { return !positive_; }
private:
    clingo_symbol_t symbol_;
    bool positive_;
};

using SymbolicLiteralSpan = Span<SymbolicLiteral>;

inline std::ostream &operator<<(std::ostream &out, SymbolicLiteral sym) {
    if (sym.is_negative()) { out << "~"; }
    out << sym.symbol();
    return out;
}
inline bool operator==(SymbolicLiteral a, SymbolicLiteral b) { return a.is_negative() == b.is_negative() && a.symbol() == b.symbol(); }
inline bool operator!=(SymbolicLiteral a, SymbolicLiteral b) { return !(a == b); }
inline bool operator< (SymbolicLiteral a, SymbolicLiteral b) {
    if (a.is_negative() != b.is_negative()) { return !a.is_negative() && b.is_negative(); }
    return a.symbol() < b.symbol();
}
inline bool operator<=(SymbolicLiteral a, SymbolicLiteral b) { return !(b < a); }
inline bool operator> (SymbolicLiteral a, SymbolicLiteral b) { return  (b < a); }
inline bool operator>=(SymbolicLiteral a, SymbolicLiteral b) { return !(a < b); }

// {{{1 solve control

class SolveControl {
public:
    explicit SolveControl(clingo_solve_control_t *ctl)
    : ctl_(ctl) { }
    void add_clause(SymbolicLiteralSpan clause);
    void add_clause(LiteralSpan clause);
    SymbolicAtoms symbolic_atoms() const;
    clingo_solve_control_t *to_c() const { return ctl_; }
private:
    clingo_solve_control_t *ctl_;
};

// {{{1 model

enum class ModelType : clingo_model_type_t {
    StableModel = clingo_model_type_stable_model,
    BraveConsequences = clingo_model_type_brave_consequences,
    CautiousConsequences = clingo_model_type_cautious_consequences
};

class ShowType {
public:
    enum Type : clingo_show_type_bitset_t {
        Shown      = clingo_show_type_shown,
        Atoms      = clingo_show_type_atoms,
        Terms      = clingo_show_type_terms,
        Theory     = clingo_show_type_theory,
        All        = clingo_show_type_all,
        Complement = clingo_show_type_complement
    };
    ShowType(clingo_show_type_bitset_t type) : type_(type) { }
    operator clingo_show_type_bitset_t() const { return type_; }
private:
    clingo_show_type_bitset_t type_;
};

enum class ConsequenceType {
    True = clingo_consequence_true,
    False = clingo_consequence_false,
    Unknown = clingo_consequence_unknown,
};

using CostVector = std::vector<int64_t>;
using PriorityVector = std::vector<weight_t>;

class Model {
public:
    explicit Model(clingo_model_t *model);
    Model(Model const &) = delete;
    Model(Model &&) noexcept = delete;
    Model &operator=(Model const &) = delete;
    Model &operator=(Model &&) = delete;
    ~Model() = default;
    bool contains(Symbol atom) const;
    bool is_true(literal_t literal) const;
    ConsequenceType is_consequence(literal_t literal) const;
    bool optimality_proven() const;
    CostVector cost() const;
    PriorityVector priorities() const;
    void extend(SymbolSpan symbols);
    SymbolVector symbols(ShowType show = ShowType::Shown) const;
    SolveControl context() const;
    ModelType type() const;
    id_t thread_id() const;
    uint64_t number() const;
    explicit operator bool() const { return model_ != nullptr; }
    clingo_model_t *to_c() const { return model_; }
private:
    clingo_model_t *model_;
};

inline std::ostream &operator<<(std::ostream &out, Model const &m) {
    out << SymbolSpan(m.symbols(ShowType::Shown));
    return out;
}

// {{{1 solve result

class SolveResult {
public:
    SolveResult() = default;
    explicit SolveResult(clingo_solve_result_bitset_t res)
    : res_(res) { }
    bool is_satisfiable() const { return (res_ & clingo_solve_result_satisfiable) != 0; }
    bool is_unsatisfiable() const { return (res_ & clingo_solve_result_unsatisfiable) != 0; }
    bool is_unknown() const { return (res_ & 3) == 0; }
    bool is_exhausted() const { return (res_ & clingo_solve_result_exhausted) != 0; }
    bool is_interrupted() const { return (res_ & clingo_solve_result_interrupted) != 0; }
    clingo_solve_result_bitset_t &to_c() { return res_; }
    clingo_solve_result_bitset_t const &to_c() const { return res_; }
    friend bool operator==(SolveResult a, SolveResult b) { return a.res_ == b.res_; }
    friend bool operator!=(SolveResult a, SolveResult b) { return a.res_ != b.res_; }
private:
    clingo_solve_result_bitset_t res_{0};
};

inline std::ostream &operator<<(std::ostream &out, SolveResult res) {
    if (res.is_satisfiable())    {
        out << "SATISFIABLE";
        if (!res.is_exhausted()) { out << "+"; }
    }
    else if (res.is_unsatisfiable())  { out << "UNSATISFIABLE"; }
    else { out << "UNKNOWN"; }
    if (res.is_interrupted()) { out << "/INTERRUPTED"; }
    return out;
}

// {{{1 location

class Location : public clingo_location_t {
public:
    explicit Location(clingo_location_t loc) : clingo_location_t(loc) { }
    Location(char const *begin_file, char const *end_file, size_t begin_line, size_t end_line, size_t begin_column, size_t end_column)
    : clingo_location_t{begin_file, end_file, begin_line, end_line, begin_column, end_column} { }
    char const *begin_file() const { return clingo_location_t::begin_file; }
    char const *end_file() const { return clingo_location_t::end_file; }
    size_t begin_line() const { return clingo_location_t::begin_line; }
    size_t end_line() const { return clingo_location_t::end_line; }
    size_t begin_column() const { return clingo_location_t::begin_column; }
    size_t end_column() const { return clingo_location_t::end_column; }
};

inline std::ostream &operator<<(std::ostream &out, Location loc) {
    out << loc.begin_file() << ":" << loc.begin_line() << ":" << loc.begin_column();
    bool dash = true;
    bool eq = std::strcmp(loc.begin_file(), loc.end_file()) == 0;
    if (!eq) { out << (dash ? "-" : ":") << loc.end_file(); dash = false; }
    eq = eq && (loc.begin_line() == loc.end_line());
    if (!eq) { out << (dash ? "-" : ":") << loc.end_line(); dash = false; }
    eq = eq && (loc.begin_column() == loc.end_column());
    if (!eq) { out << (dash ? "-" : ":") << loc.end_column(); }
    return out;
}

// {{{1 backend

enum class TheorySequenceType {
    Tuple = clingo_theory_sequence_type_tuple,
    List = clingo_theory_sequence_type_list,
    Set = clingo_theory_sequence_type_set,
};

class Backend {
public:
    explicit Backend(clingo_backend_t *backend);
    Backend(Backend const &backend) = delete;
    Backend(Backend &&backend) noexcept;
    Backend &operator=(Backend const &backend) = delete;
    Backend &operator=(Backend &&backend) noexcept;
    ~Backend() noexcept(false);
    void close();

    void rule(bool choice, AtomSpan head, LiteralSpan body);
    void weight_rule(bool choice, AtomSpan head, weight_t lower, WeightedLiteralSpan body);
    void minimize(weight_t prio, WeightedLiteralSpan body);
    void project(AtomSpan atoms);
    void external(atom_t atom, ExternalType type);
    void assume(LiteralSpan lits);
    void heuristic(atom_t atom, HeuristicType type, int bias, unsigned priority, LiteralSpan condition);
    void acyc_edge(int node_u, int node_v, LiteralSpan condition);
    atom_t add_atom();
    atom_t add_atom(Symbol symbol);
    id_t add_theory_term_number(int number);
    id_t add_theory_term_string(char const *string);
    id_t add_theory_term_sequence(TheorySequenceType type, IdSpan elements);
    id_t add_theory_term_function(char const *name, IdSpan elements);
    id_t add_theory_term_symbol(Symbol symbol);
    id_t add_theory_element(IdSpan tuple, LiteralSpan condition);
    atom_t theory_atom(id_t term_id, IdSpan elements);
    atom_t theory_atom(id_t term_id, IdSpan elements, char const *operator_name, id_t right_hand_side_id);
    atom_t theory_atom(id_t atom_id_or_zero, id_t term_id, IdSpan elements);
    atom_t theory_atom(id_t atom_id_or_zero, id_t term_id, IdSpan elements, char const *operator_name, id_t right_hand_side_id);
    clingo_backend_t *to_c() const { return backend_; }
private:
    clingo_backend_t *backend_ = nullptr;
};

// {{{1 statistics

template <class T>
class KeyIterator {
public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type = char const *;
    using difference_type = ptrdiff_t;
    using pointer = ValuePointer<char const *>;
    using reference = char const *;

    explicit KeyIterator(T const *map, size_t index = 0)
    : map_(map)
    , index_(index) { }
    KeyIterator& operator++() { ++index_; return *this; }
    KeyIterator operator++(int) {
        KeyIterator t(*this);
        ++*this;
        return t;
    }
    KeyIterator& operator--() { --index_; return *this; }
    KeyIterator operator--(int) {
        KeyIterator t(*this);
        --*this;
        return t;
    }
    KeyIterator& operator+=(difference_type n) { index_ += n; return *this; }
    KeyIterator& operator-=(difference_type n) { index_ -= n; return *this; }
    friend KeyIterator operator+(KeyIterator it, difference_type n) { return KeyIterator{it.map_, it.index_ + n}; }
    friend KeyIterator operator+(difference_type n, KeyIterator it) { return KeyIterator{it.map_, it.index_ + n}; }
    friend KeyIterator operator-(KeyIterator it, difference_type n) { return KeyIterator{it.map_, it.index_ - n}; }
    friend difference_type operator-(KeyIterator a, KeyIterator b)  { return a.index_ - b.index_; }
    reference operator*() { return map_->key_name(index_); }
    pointer operator->() { return pointer(**this); }
    friend void swap(KeyIterator& lhs, KeyIterator& rhs)  noexcept {
        std::swap(lhs.map_, rhs.map_);
        std::swap(lhs.index_, rhs.index_);
    }
    friend bool operator==(KeyIterator lhs, KeyIterator rhs) { return lhs.index_ == rhs.index_; }
    friend bool operator!=(KeyIterator lhs, KeyIterator rhs) { return !(lhs == rhs); }
    friend bool operator< (KeyIterator lhs, KeyIterator rhs) { return (lhs.index_ + 1) < (rhs.index_ + 1); }
    friend bool operator> (KeyIterator lhs, KeyIterator rhs) { return rhs < lhs; }
    friend bool operator<=(KeyIterator lhs, KeyIterator rhs) { return !(lhs > rhs); }
    friend bool operator>=(KeyIterator lhs, KeyIterator rhs) { return !(lhs < rhs); }
private:
    T const *map_;
    size_t index_;
};

enum class StatisticsType : clingo_statistics_type_t {
    Value = clingo_statistics_type_value,
    Array = clingo_statistics_type_array,
    Map = clingo_statistics_type_map
};

template <bool constant>
class StatisticsBase {
    friend class KeyIterator<StatisticsBase>;
public:
    using statistics_t =  std::conditional_t<constant, clingo_statistics_t const *, clingo_statistics_t*>;
    using KeyIteratorT = KeyIterator<StatisticsBase>;
    using ArrayIteratorT = ArrayIterator<StatisticsBase, StatisticsBase const *>;
    using KeyRangeT = IteratorRange<KeyIteratorT>;
    explicit StatisticsBase(statistics_t stats, uint64_t key)
    : stats_(stats)
    , key_(key) { }
    // generic
    StatisticsType type() const;
    // arrays
    size_t size() const;
    void ensure_size(size_t size, StatisticsType type);
    StatisticsBase operator[](size_t index) const;
    StatisticsBase at(size_t index) const { return operator[](index); }
    ArrayIteratorT begin() const;
    ArrayIteratorT end() const;
    StatisticsBase push(StatisticsType type);
    // maps
    StatisticsBase operator[](char const *name) const;
    StatisticsBase at(char const *name) const { return operator[](name); }
    bool has_subkey(char const *name) const;
    KeyRangeT keys() const;
    StatisticsBase add_subkey(char const *name, StatisticsType type);
    // leafs
    operator double() const { return value(); }
    double value() const;
    void set_value(double d);
    statistics_t to_c() const { return stats_; }
private:
    char const *key_name(size_t index) const;
    statistics_t stats_;
    uint64_t key_;
};

using Statistics = StatisticsBase<true>;
using UserStatistics = StatisticsBase<false>;
using UserStatisticCallback = std::function<void (UserStatistics &)>;

// {{{1 solve handle

class SolveEventHandler {
public:
    SolveEventHandler() = default;
    SolveEventHandler(SolveEventHandler const &) = default;
    SolveEventHandler(SolveEventHandler &&) noexcept = default;
    SolveEventHandler &operator=(SolveEventHandler const &) = default;
    SolveEventHandler &operator=(SolveEventHandler &&) noexcept = default;
    virtual bool on_model(Model &model);
    virtual void on_unsat(Span<int64_t> lower_bound);
    virtual void on_statistics(UserStatistics step, UserStatistics accu);
    virtual void on_finish(SolveResult result);
    virtual ~SolveEventHandler() = default;
};

inline bool SolveEventHandler::on_model(Model &model) {
    static_cast<void>(model);
    return true;
}

inline void SolveEventHandler::on_unsat(Span<int64_t> lower_bound)  {
    static_cast<void>(lower_bound);
}

inline void SolveEventHandler::on_statistics(UserStatistics step, UserStatistics accu) {
    static_cast<void>(step);
    static_cast<void>(accu);
}

inline void SolveEventHandler::on_finish(SolveResult result) {
    static_cast<void>(result);
}

namespace Detail {

class AssignOnce;

} // namespace Detail

class SolveHandle {
public:
    SolveHandle() = default;
    explicit SolveHandle(clingo_solve_handle_t *it, Detail::AssignOnce &ptr);
    SolveHandle(SolveHandle &&it) noexcept;
    SolveHandle(SolveHandle const &) = delete;
    SolveHandle &operator=(SolveHandle &&it) noexcept;
    SolveHandle &operator=(SolveHandle const &) = delete;
    ~SolveHandle() noexcept(false);
    void close();

    void resume();
    void wait();
    bool wait(double timeout);
    Model const &model();
    Model const &next();
    LiteralSpan core();
    SolveResult get();
    void cancel();
    clingo_solve_handle_t *to_c() const { return iter_; }
private:
    Model model_{nullptr};
    clingo_solve_handle_t *iter_{nullptr};
    Detail::AssignOnce *exception_{nullptr};
};

class ModelIterator {
public:
    using iterator_category = std::input_iterator_tag;
    using value_type = Model;
    using difference_type = ptrdiff_t;
    using pointer = Model const *;
    using reference = Model const &;

    explicit ModelIterator(SolveHandle &iter)
    : iter_(&iter) { }
    ModelIterator() = default;
    ModelIterator &operator++() {
        iter_->next();
        return *this;
    }
    // Warning: the resulting iterator should not be used
    //          because its model is no longer valid
    ModelIterator operator++(int) {
        ModelIterator t = *this;
        ++*this;
        return t;
    }
    Model const &operator*() { return iter_->model(); }
    Model const *operator->() { return &iter_->model(); }
    friend bool operator==(ModelIterator a, ModelIterator b) {
        auto *ap = (a.iter_ != nullptr ? a.iter_->model().to_c() : nullptr);
        auto *bp = (b.iter_ != nullptr ? b.iter_->model().to_c() : nullptr);
        return ap == bp;
    }
    friend bool operator!=(ModelIterator a, ModelIterator b) { return !(a == b); }
private:
    SolveHandle *iter_{nullptr};
};

inline ModelIterator begin(SolveHandle &it) { return ModelIterator(it); }
inline ModelIterator end(SolveHandle &it) {
    static_cast<void>(it);
    return {};
}

// {{{1 configuration

class Configuration;
using ConfigurationArrayIterator = ArrayIterator<Configuration>;
using ConfigurationKeyIterator = KeyIterator<Configuration>;
using ConfigurationKeyRange = IteratorRange<ConfigurationKeyIterator>;

class Configuration {
    friend class KeyIterator<Configuration>;
public:
    explicit Configuration(clingo_configuration_t *conf, clingo_id_t key)
    : conf_(conf)
    , key_(key) { }
    // arrays
    bool is_array() const;
    Configuration operator[](size_t index);
    Configuration at(size_t index) { return operator[](index); }
    ConfigurationArrayIterator begin();
    ConfigurationArrayIterator end();
    size_t size() const;
    bool empty() const;
    // maps
    bool is_map() const;
    Configuration operator[](char const *name);
    Configuration get(char const *name) { return operator[](name); }
    ConfigurationKeyRange keys() const;
    // values
    bool is_value() const;
    bool is_assigned() const;
    std::string value() const;
    operator std::string() const { return value(); }
    Configuration &operator=(char const *value);
    // generic
    char const *decription() const;
    clingo_configuration_t *to_c() const { return conf_; }
private:
    char const *key_name(size_t index) const;
    clingo_configuration_t *conf_;
    unsigned key_;
};

// {{{1 control

class Part {
public:
    Part(char const *name, SymbolSpan params)
    : part_{name, Detail::cast<clingo_symbol_t const*>(params.begin()), params.size()} { }
    explicit Part(clingo_part_t part)
    : part_(part) { }
    char const *name() const { return part_.name; }
    SymbolSpan params() const { return {Detail::cast<Symbol const*>(part_.params), part_.size}; }
    clingo_part_t const &to_c() const { return part_; }
    clingo_part_t &to_c() { return part_; }
private:
    clingo_part_t part_;
};
using PartSpan = Span<Part>;
using GroundCallback = std::function<void (Location loc, char const *, SymbolSpan, SymbolSpanCallback)>;
using StringSpan = Span<char const *>;

enum class ErrorCode : clingo_error_t {
    Runtime = clingo_error_runtime,
    Logic = clingo_error_logic,
    BadAlloc = clingo_error_bad_alloc,
    Unknown = clingo_error_unknown,
};

inline std::ostream &operator<<(std::ostream &out, ErrorCode code) {
    out << clingo_error_string(static_cast<clingo_error_t>(code));
    return out;
}

enum class WarningCode : clingo_warning_t {
    OperationUndefined = clingo_warning_operation_undefined,
    RuntimeError = clingo_warning_runtime_error,
    AtomUndefined = clingo_warning_atom_undefined,
    FileIncluded = clingo_warning_file_included,
    VariableUnbounded = clingo_warning_variable_unbounded,
    GlobalVariable = clingo_warning_global_variable,
    Other = clingo_warning_other,
};

using Logger = std::function<void (WarningCode, char const *)>;

inline std::ostream &operator<<(std::ostream &out, WarningCode code) {
    out << clingo_warning_string(static_cast<clingo_warning_t>(code));
    return out;
}

class Control {
    struct Impl;
public:
    Control(StringSpan args = {}, Logger logger = nullptr, unsigned message_limit = g_message_limit);
    explicit Control(clingo_control_t *ctl, bool owns = true);
    Control(Control &&c) noexcept;
    Control(Control const &) = delete;
    Control &operator=(Control &&c) noexcept;
    Control &operator=(Control const &c) = delete;
    ~Control() noexcept;
    void add(char const *name, StringSpan params, char const *part);
    void ground(PartSpan parts, GroundCallback cb = nullptr);
    SolveHandle solve(LiteralSpan assumptions, SolveEventHandler *handler = nullptr, bool asynchronous = false, bool yield = true);
    SolveHandle solve(SymbolicLiteralSpan assumptions = {}, SolveEventHandler *handler = nullptr, bool asynchronous = false, bool yield = true);
    void assign_external(literal_t literal, TruthValue value);
    void assign_external(Symbol atom, TruthValue value);
    void release_external(literal_t literal);
    void release_external(Symbol atom);
    SymbolicAtoms symbolic_atoms() const;
    TheoryAtoms theory_atoms() const;
    void register_propagator(Propagator &propagator, bool sequential = false);
    void register_propagator(Heuristic &propagator, bool sequential = false);
    void register_observer(GroundProgramObserver &observer, bool replace = false);
    bool is_conflicting() const noexcept;
    bool has_const(char const *name) const;
    Symbol get_const(char const *name) const;
    void interrupt() noexcept;
    void *claspFacade();
    void load(char const *file);
    void enable_enumeration_assumption(bool value);
    bool enable_enumeration_assumption() const;
    void cleanup();
    void enable_cleanup(bool value);
    bool enable_cleanup() const;
    Backend backend();
    template <class F>
    void with_backend(F f) {
        auto b = backend();
        f(b);
    }
    Configuration configuration();
    Statistics statistics() const;
    clingo_control_t *to_c() const;
private:
    Impl *impl_;
};

// {{{1 ast v2

namespace AST {

using Clingo::TheorySequenceType;

enum class CommentType {
    Line = clingo_comment_type_line,
    Block = clingo_comment_type_block,
};

enum class ComparisonOperator {
    GreaterThan = clingo_ast_comparison_operator_greater_than,
    LessThan = clingo_ast_comparison_operator_less_than,
    LessEqual = clingo_ast_comparison_operator_less_equal,
    GreaterEqual = clingo_ast_comparison_operator_greater_equal,
    NotEqual = clingo_ast_comparison_operator_not_equal,
    Equal = clingo_ast_comparison_operator_equal,
};

enum class Sign {
    NoSign = clingo_ast_sign_no_sign,
    Negation = clingo_ast_sign_negation,
    DoubleNegation = clingo_ast_sign_double_negation,
};

enum class UnaryOperator {
    Minus = clingo_ast_unary_operator_minus,
    Negation = clingo_ast_unary_operator_negation,
    Absolute = clingo_ast_unary_operator_absolute,
};

enum class BinaryOperator {
    Xor = clingo_ast_binary_operator_xor,
    Or = clingo_ast_binary_operator_or,
    And = clingo_ast_binary_operator_and,
    Plus = clingo_ast_binary_operator_plus,
    Minus = clingo_ast_binary_operator_minus,
    Multiplication = clingo_ast_binary_operator_multiplication,
    Division = clingo_ast_binary_operator_division,
    Modulo = clingo_ast_binary_operator_modulo,
    Power = clingo_ast_binary_operator_power,
};

enum class AggregateFunction {
    Count = clingo_ast_aggregate_function_count,
    Sum = clingo_ast_aggregate_function_sum,
    Sump = clingo_ast_aggregate_function_sump,
    Min = clingo_ast_aggregate_function_min,
    Max = clingo_ast_aggregate_function_max,
};

enum class TheoryOperatorType {
     Unary = clingo_ast_theory_operator_type_unary,
     BinaryLeft = clingo_ast_theory_operator_type_binary_left,
     BinaryRight = clingo_ast_theory_operator_type_binary_right,
};

enum class TheoryAtomDefinitionType {
    Head = clingo_ast_theory_atom_definition_type_head,
    Body = clingo_ast_theory_atom_definition_type_body,
    Any = clingo_ast_theory_atom_definition_type_any,
    Directive = clingo_ast_theory_atom_definition_type_directive,
};

enum class Type {
    // terms
    Id = clingo_ast_type_id,
    Variable = clingo_ast_type_variable,
    SymbolicTerm = clingo_ast_type_symbolic_term,
    UnaryOperation = clingo_ast_type_unary_operation,
    BinaryOperation = clingo_ast_type_binary_operation,
    Interval = clingo_ast_type_interval,
    Function = clingo_ast_type_function,
    Pool = clingo_ast_type_pool,
    // simple atoms
    BooleanConstant = clingo_ast_type_boolean_constant,
    SymbolicAtom = clingo_ast_type_symbolic_atom,
    Comparison = clingo_ast_type_comparison,
    // aggregates
    Guard = clingo_ast_type_guard,
    ConditionalLiteral = clingo_ast_type_conditional_literal,
    Aggregate = clingo_ast_type_aggregate,
    BodyAggregateElement = clingo_ast_type_body_aggregate_element,
    BodyAggregate = clingo_ast_type_body_aggregate,
    HeadAggregateElement = clingo_ast_type_head_aggregate_element,
    HeadAggregate = clingo_ast_type_head_aggregate,
    Disjunction = clingo_ast_type_disjunction,
    // theory atoms
    TheorySequence = clingo_ast_type_theory_sequence,
    TheoryFunction = clingo_ast_type_theory_function,
    TheoryUnparsedTermElement = clingo_ast_type_theory_unparsed_term_element,
    TheoryUnparsedTerm = clingo_ast_type_theory_unparsed_term,
    TheoryGuard = clingo_ast_type_theory_guard,
    TheoryAtomElement = clingo_ast_type_theory_atom_element,
    TheoryAtom = clingo_ast_type_theory_atom,
    // literals
    Literal = clingo_ast_type_literal,
    // theory definition
    TheoryOperatorDefinition = clingo_ast_type_theory_operator_definition,
    TheoryTermDefinition = clingo_ast_type_theory_term_definition,
    TheoryGuardDefinition = clingo_ast_type_theory_guard_definition,
    TheoryAtomDefinition = clingo_ast_type_theory_atom_definition,
    // statements
    Rule = clingo_ast_type_rule,
    Definition = clingo_ast_type_definition,
    ShowSignature = clingo_ast_type_show_signature,
    ShowTerm = clingo_ast_type_show_term,
    Minimize = clingo_ast_type_minimize,
    Script = clingo_ast_type_script,
    Program = clingo_ast_type_program,
    External = clingo_ast_type_external,
    Edge = clingo_ast_type_edge,
    Heuristic = clingo_ast_type_heuristic,
    ProjectAtom = clingo_ast_type_project_atom,
    ProjectSignature = clingo_ast_type_project_signature,
    Defined = clingo_ast_type_defined,
    TheoryDefinition = clingo_ast_type_theory_definition,
    Comment = clingo_ast_type_comment,
};

enum class Attribute {
    Argument = clingo_ast_attribute_argument,
    Arguments = clingo_ast_attribute_arguments,
    Arity = clingo_ast_attribute_arity,
    Atom = clingo_ast_attribute_atom,
    Atoms = clingo_ast_attribute_atoms,
    AtomType = clingo_ast_attribute_atom_type,
    Bias = clingo_ast_attribute_bias,
    Body = clingo_ast_attribute_body,
    Code = clingo_ast_attribute_code,
    Coefficient = clingo_ast_attribute_coefficient,
    Comparison = clingo_ast_attribute_comparison,
    Condition = clingo_ast_attribute_condition,
    Elements = clingo_ast_attribute_elements,
    External = clingo_ast_attribute_external,
    ExternalType = clingo_ast_attribute_external_type,
    Function = clingo_ast_attribute_function,
    Guard = clingo_ast_attribute_guard,
    Guards = clingo_ast_attribute_guards,
    Head = clingo_ast_attribute_head,
    IsDefault = clingo_ast_attribute_is_default,
    Left = clingo_ast_attribute_left,
    LeftGuard = clingo_ast_attribute_left_guard,
    Literal = clingo_ast_attribute_literal,
    Location = clingo_ast_attribute_location,
    Modifier = clingo_ast_attribute_modifier,
    Name = clingo_ast_attribute_name,
    NodeU = clingo_ast_attribute_node_u,
    NodeV = clingo_ast_attribute_node_v,
    OperatorName = clingo_ast_attribute_operator_name,
    OperatorType = clingo_ast_attribute_operator_type,
    Operators = clingo_ast_attribute_operators,
    Parameters = clingo_ast_attribute_parameters,
    Positive = clingo_ast_attribute_positive,
    Priority = clingo_ast_attribute_priority,
    Right = clingo_ast_attribute_right,
    RightGuard = clingo_ast_attribute_right_guard,
    SequenceType = clingo_ast_attribute_sequence_type,
    Sign = clingo_ast_attribute_sign,
    Symbol = clingo_ast_attribute_symbol,
    Term = clingo_ast_attribute_term,
    Terms = clingo_ast_attribute_terms,
    Value = clingo_ast_attribute_value,
    Variable = clingo_ast_attribute_variable,
    Weight = clingo_ast_attribute_weight,
    CommentType = clingo_ast_attribute_comment_type,
};

class Node;
class NodeVector;
class StringVector;

using NodeValue = Variant<int, Symbol, Location, char const *, Node, Optional<Node>, StringVector, NodeVector>;

class Node {
public:
    explicit Node(clingo_ast_t *ast);
    template <class... Args>
    Node(Type type, Args&& ...args);
    Node(Node const &ast);
    Node(Node &&ast) noexcept;
    Node &operator=(Node const &ast);
    Node &operator=(Node &&ast) noexcept;
    ~Node();
    Node copy() const;
    Node deep_copy() const;
    Type type() const;
    NodeValue get(Attribute attribute) const;
    template <class T>
    T get(Attribute attribute) const;
    void set(Attribute attribute, NodeValue value);
    template <class Visitor>
    void visit_attribute(Visitor &&visitor) const;
    template <class Visitor>
    void visit_ast(Visitor &&visitor) const;
    template <class Visitor>
    Node transform_ast(Visitor &&visitor) const;
    std::string to_string() const;
    std::vector<Node> unpool(bool other=true, bool condition=true) const;
    clingo_ast_t *to_c() const { return ast_; }
    friend std::ostream &operator<<(std::ostream &out, Node const &ast);
    friend bool operator<(Node const &a, Node const &b);
    friend bool operator>(Node const &a, Node const &b);
    friend bool operator<=(Node const &a, Node const &b);
    friend bool operator>=(Node const &a, Node const &b);
    friend bool operator==(Node const &a, Node const &b);
    friend bool operator!=(Node const &a, Node const &b);
    size_t hash() const;
private:
    clingo_ast_t *ast_;
};

class NodeRef {
public:
    NodeRef(NodeVector *vec, size_t index);
    NodeRef &operator=(Node const &ast);
    Node get() const;
    operator Node () const;
private:
    NodeVector *vec_;
    size_t index_;
};

class NodeVector {
public:
    using value_type = Node;
    using iterator = ArrayIterator<NodeRef, NodeVector*>;
    using const_iterator = ArrayIterator<Node, NodeVector const *>;

    NodeVector(Node ast, clingo_ast_attribute_t attr);
    iterator begin();
    iterator end();
    const_iterator begin() const;
    const_iterator end() const;
    size_t size() const;
    bool empty() const;
    iterator insert(iterator it, Node const &ast);
    iterator erase(iterator it);
    void set(iterator it, Node const &ast);
    ValuePointer<NodeRef> at(size_t idx);
    NodeRef operator[](size_t idx);
    ValuePointer<Node> at(size_t idx) const;
    Node operator[](size_t idx) const;
    void push_back(Node const &ast);
    void pop_back();
    void clear();
    Node &ast();
    Node const &ast() const;
private:
    Node ast_;
    clingo_ast_attribute_t attr_;
};

class StringVector;

class StringRef {
public:
    StringRef(StringVector *vec, size_t index);
    StringRef &operator=(char const *str);
    char const *get() const;
    operator char const *() const;
private:
    StringVector *vec_;
    size_t index_;
};

class StringVector {
public:
    using value_type = char const *;
    using iterator = ArrayIterator<StringRef, StringVector*>;
    using const_iterator = ArrayIterator<char const *, StringVector const *>;

    StringVector(Node ast, clingo_ast_attribute_t attr);
    iterator begin();
    iterator end();
    const_iterator begin() const;
    const_iterator end() const;
    size_t size() const;
    bool empty() const;
    iterator insert(iterator it, char const *str);
    void set(iterator it, char const *str);
    iterator erase(iterator it);
    StringRef operator[](size_t idx);
    ValuePointer<StringRef> at(size_t idx);
    char const *operator[](size_t idx) const;
    ValuePointer<char const *> at(size_t idx) const;
    void push_back(char const *str);
    void pop_back();
    void clear();
    Node &ast();
    Node const &ast() const;
private:
    Node ast_;
    clingo_ast_attribute_t attr_;
};

class ProgramBuilder {
public:
    explicit ProgramBuilder(Control &ctl);
    explicit ProgramBuilder(clingo_program_builder_t *builder);
    ProgramBuilder() = delete;
    ProgramBuilder(ProgramBuilder const &builder) = delete;
    ProgramBuilder(ProgramBuilder &&builder) noexcept;
    ProgramBuilder &operator=(ProgramBuilder const &builder) = delete;
    ProgramBuilder &operator=(ProgramBuilder &&builder) noexcept;
    ~ProgramBuilder() noexcept(false);
    void close();

    void add(Node const &ast);
    clingo_program_builder_t *to_c() const { return builder_; }

private:
    clingo_program_builder_t *builder_;
};

template <class Callback>
void parse_string(char const *program, Callback &&cb, Logger logger = nullptr, unsigned message_limit = g_message_limit);

template <class Callback>
void parse_string(char const *program, Callback &&cb, Control &control, Logger logger = nullptr, unsigned message_limit = g_message_limit);

template <class Callback>
void parse_files(StringSpan files, Callback &&cb, Logger logger = nullptr, unsigned message_limit = g_message_limit);

template <class Callback>
void parse_files(StringSpan files, Callback &&cb, Control &control, Logger logger = nullptr, unsigned message_limit = g_message_limit);

template <class F>
inline void with_builder(Control &ctl, F f) {
    auto b = ProgramBuilder{ctl};
    f(b);
}

} // namespace AST

} namespace std {

template<>
struct hash<Clingo::AST::Node> {
    size_t operator()(Clingo::AST::Node const &ast) const { return ast.hash(); }
};

} namespace Clingo {

// {{{1 clingo application

namespace Detail {
    using ParserList = std::forward_list<std::function<bool (char const *value)>>;
}

class ClingoOptions {
public:
    explicit ClingoOptions(clingo_options_t *options, Detail::ParserList &parsers)
    : options_{options}
    , parsers_{&parsers} { }
    clingo_options_t *to_c() const { return options_; }
    void add(char const *group, char const *option, char const *description, std::function<bool (char const *value)> parser, bool multi = false, char const *argument = nullptr);
    void add_flag(char const *group, char const *option, char const *description, bool &target);
private:
    clingo_options_t *options_;
    Detail::ParserList *parsers_;
};

struct Application {
    Application() = default;
    Application(Application const &) = default;
    Application(Application &&) noexcept = default;
    Application &operator=(Application const &) = default;
    Application &operator=(Application &&) noexcept = default;
    virtual unsigned message_limit() const noexcept;
    virtual char const *program_name() const noexcept;
    virtual char const *version() const noexcept;
    virtual void main(Control &ctl, StringSpan files) = 0;
    virtual void log(WarningCode code, char const *message) noexcept;
    virtual void print_model(Model const &model, std::function<void()> default_printer) noexcept;
    virtual void register_options(ClingoOptions &options);
    virtual void validate_options();
    virtual ~Application() = default;
};

// {{{1 global functions

Symbol parse_term(char const *str, Logger logger = nullptr, unsigned message_limit = g_message_limit);
char const *add_string(char const *str);
std::tuple<int, int, int> version();

inline int clingo_main(Application &application, StringSpan arguments);

// }}}1

} // namespace Clingo

//{{{1 implementation

#define CLINGO_CALLBACK_TRY try // NOLINT
#define CLINGO_CALLBACK_CATCH(ref) catch (...){ (ref) = std::current_exception(); return false; } return true // NOLINT

#define CLINGO_TRY try // NOLINT
#define CLINGO_CATCH catch (...){ Detail::handle_cxx_error(); return false; } return true // NOLINT

namespace Clingo {

// {{{2 details

namespace Detail {

inline void handle_error(bool ret) {
    if (!ret) {
        char const *msg = clingo_error_message();
        if (msg == nullptr) { msg = "no message"; }
        switch (static_cast<clingo_error_e>(clingo_error_code())) {
            case clingo_error_runtime:   { throw std::runtime_error(msg); }
            case clingo_error_logic:     { throw std::logic_error(msg); }
            case clingo_error_bad_alloc: { throw std::bad_alloc(); }
            case clingo_error_unknown:
            case clingo_error_success:   { throw std::runtime_error(msg); }
        }
    }
}

inline void handle_error(bool ret, std::exception_ptr &exc) {
    if (!ret) {
        if (exc) {
            std::exception_ptr ptr = exc;
            exc = nullptr;
            std::rethrow_exception(ptr);
        }
        handle_error(false);
    }
}

enum class AssignState { Unassigned=0, Writing=1, Assigned=2 };

class AssignOnce {
public:
    AssignOnce &operator=(std::exception_ptr x) {
        auto ua = AssignState::Unassigned;
        if (state_.compare_exchange_strong(ua, AssignState::Writing)) {
            val_ = std::move(x);
            state_ = AssignState::Assigned;
        }
        return *this;
    }
    std::exception_ptr &operator*() {
        static std::exception_ptr null = nullptr;
        // NOTE: this is not necessarily the exception that caused the search to stop
        //       but just the first that has been set
        return state_ == AssignState::Assigned ? val_ : null;
    }
    void reset() {
        state_ = AssignState::Unassigned;
        val_ = nullptr;
    }
private:
    std::atomic<AssignState> state_{AssignState::Unassigned};
    std::exception_ptr val_ = nullptr; // NOLINT
};

inline void handle_error(bool ret, AssignOnce &exc) {
    if (!ret) { handle_error(ret, *exc); }
}

inline void handle_cxx_error() {
    try { throw; }
    catch (std::bad_alloc const &e)     { clingo_set_error(clingo_error_bad_alloc, e.what()); return; }
    catch (std::runtime_error const &e) { clingo_set_error(clingo_error_runtime, e.what()); return; }
    catch (std::logic_error const &e)   { clingo_set_error(clingo_error_logic, e.what()); return; }
    catch (...)                         { } // NOLINT(bugprone-empty-catch)
    clingo_set_error(clingo_error_unknown, "unknown error");
}

template <class S, class P, class ...Args>
std::string to_string(S size, P print, Args ...args) {
    std::vector<char> ret;
    size_t n = 0;
    Detail::handle_error(size(std::forward<Args>(args)..., &n));
    ret.resize(n);
    Detail::handle_error(print(std::forward<Args>(args)..., ret.data(), n));
    return {ret.begin(), ret.end()-1};
}

} // namespace Detail

// {{{2 signature

inline Signature::Signature(char const *name, uint32_t arity, bool positive)
: sig_{0} {
    Detail::handle_error(clingo_signature_create(name, arity, positive, &sig_));
}

inline char const *Signature::name() const {
    return clingo_signature_name(sig_);
}

inline uint32_t Signature::arity() const {
    return clingo_signature_arity(sig_);
}

inline bool Signature::positive() const {
    return clingo_signature_is_positive(sig_);
}

inline bool Signature::negative() const {
    return clingo_signature_is_negative(sig_);
}

inline size_t Signature::hash() const {
    return clingo_signature_hash(sig_);
}

inline bool operator==(Signature a, Signature b) { return  clingo_signature_is_equal_to(a.to_c(), b.to_c()); }
inline bool operator!=(Signature a, Signature b) { return !clingo_signature_is_equal_to(a.to_c(), b.to_c()); }
inline bool operator< (Signature a, Signature b) { return  clingo_signature_is_less_than(a.to_c(), b.to_c()); }
inline bool operator<=(Signature a, Signature b) { return !clingo_signature_is_less_than(b.to_c(), a.to_c()); }
inline bool operator> (Signature a, Signature b) { return  clingo_signature_is_less_than(b.to_c(), a.to_c()); }
inline bool operator>=(Signature a, Signature b) { return !clingo_signature_is_less_than(a.to_c(), b.to_c()); }

// {{{2 symbol

inline Symbol::Symbol() {
    clingo_symbol_create_number(0, &sym_);
}

inline Symbol::Symbol(clingo_symbol_t sym)
: sym_(sym) { }

inline Symbol Number(int num) {
    clingo_symbol_t sym = 0;
    clingo_symbol_create_number(num, &sym);
    return Symbol(sym);
}

inline Symbol Supremum() {
    clingo_symbol_t sym = 0;
    clingo_symbol_create_supremum(&sym);
    return Symbol(sym);
}

inline Symbol Infimum() {
    clingo_symbol_t sym = 0;
    clingo_symbol_create_infimum(&sym);
    return Symbol(sym);
}

inline Symbol String(char const *str) {
    clingo_symbol_t sym = 0;
    Detail::handle_error(clingo_symbol_create_string(str, &sym));
    return Symbol(sym);
}

inline Symbol Id(char const *id, bool positive) {
    clingo_symbol_t sym = 0;
    Detail::handle_error(clingo_symbol_create_id(id, positive, &sym));
    return Symbol(sym);
}

inline Symbol Function(char const *name, SymbolSpan args, bool positive) {
    clingo_symbol_t sym = 0;
    Detail::handle_error(clingo_symbol_create_function(name, Detail::cast<clingo_symbol_t const *>(args.begin()), args.size(), positive, &sym));
    return Symbol(sym);
}

inline int Symbol::number() const {
    int ret = 0;
    Detail::handle_error(clingo_symbol_number(sym_, &ret));
    return ret;
}

inline char const *Symbol::name() const {
    char const *ret = nullptr;
    Detail::handle_error(clingo_symbol_name(sym_, &ret));
    return ret;
}

inline char const *Symbol::string() const {
    char const *ret = nullptr;
    Detail::handle_error(clingo_symbol_string(sym_, &ret));
    return ret;
}

inline bool Symbol::is_positive() const {
    bool ret = false;
    Detail::handle_error(clingo_symbol_is_positive(sym_, &ret));
    return ret;
}

inline bool Symbol::is_negative() const {
    bool ret = false;
    Detail::handle_error(clingo_symbol_is_negative(sym_, &ret));
    return ret;
}

inline SymbolSpan Symbol::arguments() const {
    clingo_symbol_t const *ret = nullptr;
    size_t n = 0;
    Detail::handle_error(clingo_symbol_arguments(sym_, &ret, &n));
    return {Detail::cast<Symbol const *>(ret), n};
}

inline SymbolType Symbol::type() const {
    return static_cast<SymbolType>(clingo_symbol_type(sym_));
}

inline bool Symbol::match(char const *name, unsigned arity) const {
    return type() == SymbolType::Function && strcmp(this->name(), name) == 0 && this->arguments().size() == arity;
}

inline std::string Symbol::to_string() const {
    return Detail::to_string(clingo_symbol_to_string_size, clingo_symbol_to_string, sym_);
}

inline size_t Symbol::hash() const {
    return clingo_symbol_hash(sym_);
}

inline std::ostream &operator<<(std::ostream &out, Symbol sym) {
    out << sym.to_string();
    return out;
}

inline bool operator==(Symbol a, Symbol b) { return  clingo_symbol_is_equal_to(a.to_c(), b.to_c()); }
inline bool operator!=(Symbol a, Symbol b) { return !clingo_symbol_is_equal_to(a.to_c(), b.to_c()); }
inline bool operator< (Symbol a, Symbol b) { return  clingo_symbol_is_less_than(a.to_c(), b.to_c()); }
inline bool operator<=(Symbol a, Symbol b) { return !clingo_symbol_is_less_than(b.to_c(), a.to_c()); }
inline bool operator> (Symbol a, Symbol b) { return  clingo_symbol_is_less_than(b.to_c(), a.to_c()); }
inline bool operator>=(Symbol a, Symbol b) { return !clingo_symbol_is_less_than(a.to_c(), b.to_c()); }

// {{{2 symbolic atoms

inline Symbol SymbolicAtom::symbol() const {
    clingo_symbol_t ret = 0;
    clingo_symbolic_atoms_symbol(atoms_, range_, &ret);
    return Symbol(ret);
}

inline clingo_literal_t SymbolicAtom::literal() const {
    clingo_literal_t ret = 0;
    clingo_symbolic_atoms_literal(atoms_, range_, &ret);
    return ret;
}

inline bool SymbolicAtom::is_fact() const {
    bool ret = false;
    clingo_symbolic_atoms_is_fact(atoms_, range_, &ret);
    return ret;
}

inline bool SymbolicAtom::is_external() const {
    bool ret = false;
    clingo_symbolic_atoms_is_external(atoms_, range_, &ret);
    return ret;
}

inline bool SymbolicAtom::match(char const *name, unsigned arity) const {
    return symbol().match(name, arity);
}

inline SymbolicAtomIterator &SymbolicAtomIterator::operator++() {
    clingo_symbolic_atom_iterator_t range = 0;
    Detail::handle_error(clingo_symbolic_atoms_next(atoms_, range_, &range));
    range_ = range;
    return *this;
}

inline SymbolicAtomIterator::operator bool() const {
    bool ret = false;
    Detail::handle_error(clingo_symbolic_atoms_is_valid(atoms_, range_, &ret));
    return ret;
}

inline bool SymbolicAtomIterator::operator==(SymbolicAtomIterator it) const {
    bool ret = atoms_ == it.atoms_;
    if (ret) { Detail::handle_error(clingo_symbolic_atoms_iterator_is_equal_to(atoms_, range_, it.range_, &ret)); }
    return ret;
}

inline SymbolicAtomIterator SymbolicAtoms::begin() const {
    clingo_symbolic_atom_iterator_t it = 0;
    Detail::handle_error(clingo_symbolic_atoms_begin(atoms_, nullptr, &it));
    return SymbolicAtomIterator{atoms_,  it};
}

inline SymbolicAtomIterator SymbolicAtoms::begin(Signature sig) const {
    clingo_symbolic_atom_iterator_t it = 0;
    Detail::handle_error(clingo_symbolic_atoms_begin(atoms_, &sig.to_c(), &it));
    return SymbolicAtomIterator{atoms_, it};
}

inline SymbolicAtomIterator SymbolicAtoms::end() const {
    clingo_symbolic_atom_iterator_t it = 0;
    Detail::handle_error(clingo_symbolic_atoms_end(atoms_, &it));
    return SymbolicAtomIterator{atoms_, it};
}

inline SymbolicAtomIterator SymbolicAtoms::find(Symbol atom) const {
    clingo_symbolic_atom_iterator_t it = 0;
    Detail::handle_error(clingo_symbolic_atoms_find(atoms_, atom.to_c(), &it));
    return SymbolicAtomIterator{atoms_, it};
}

inline std::vector<Signature> SymbolicAtoms::signatures() const {
    size_t n = 0;
    clingo_symbolic_atoms_signatures_size(atoms_, &n);
    Signature sig("", 0);
    std::vector<Signature> ret;
    ret.resize(n, sig);
    Detail::handle_error(clingo_symbolic_atoms_signatures(atoms_, Detail::cast<clingo_signature_t *>(ret.data()), n));
    return ret;
}

inline size_t SymbolicAtoms::length() const {
    size_t ret = 0;
    Detail::handle_error(clingo_symbolic_atoms_size(atoms_, &ret));
    return ret;
}

// {{{2 theory atoms

inline TheoryTermType TheoryTerm::type() const {
    clingo_theory_term_type_t ret = 0;
    Detail::handle_error(clingo_theory_atoms_term_type(atoms_, id_, &ret));
    return static_cast<TheoryTermType>(ret);
}

inline int TheoryTerm::number() const {
    int ret = 0;
    Detail::handle_error(clingo_theory_atoms_term_number(atoms_, id_, &ret));
    return ret;
}

inline char const *TheoryTerm::name() const {
    char const *ret = nullptr;
    Detail::handle_error(clingo_theory_atoms_term_name(atoms_, id_, &ret));
    return ret;
}

inline TheoryTermSpan TheoryTerm::arguments() const {
    clingo_id_t const *ret = nullptr;
    size_t n = 0;
    Detail::handle_error(clingo_theory_atoms_term_arguments(atoms_, id_, &ret, &n));
    return {ret, n, ToTheoryIterator<TheoryTermIterator>{atoms_}};
}

inline std::ostream &operator<<(std::ostream &out, TheoryTerm term) {
    out << term.to_string();
    return out;
}

inline std::string TheoryTerm::to_string() const {
    return Detail::to_string(clingo_theory_atoms_term_to_string_size, clingo_theory_atoms_term_to_string, atoms_, id_);
}


inline TheoryTermSpan TheoryElement::tuple() const {
    clingo_id_t const *ret = nullptr;
    size_t n = 0;
    Detail::handle_error(clingo_theory_atoms_element_tuple(atoms_, id_, &ret, &n));
    return {ret, n, ToTheoryIterator<TheoryTermIterator>{atoms_}};
}

inline LiteralSpan TheoryElement::condition() const {
    clingo_literal_t const *ret = nullptr;
    size_t n = 0;
    Detail::handle_error(clingo_theory_atoms_element_condition(atoms_, id_, &ret, &n));
    return {ret, n};
}

inline literal_t TheoryElement::condition_id() const {
    clingo_literal_t ret = 0;
    Detail::handle_error(clingo_theory_atoms_element_condition_id(atoms_, id_, &ret));
    return ret;
}

inline std::string TheoryElement::to_string() const {
    return Detail::to_string(clingo_theory_atoms_element_to_string_size, clingo_theory_atoms_element_to_string, atoms_, id_);
}

inline std::ostream &operator<<(std::ostream &out, TheoryElement term) {
    out << term.to_string();
    return out;
}

inline TheoryElementSpan TheoryAtom::elements() const {
    clingo_id_t const *ret = nullptr;
    size_t n = 0;
    Detail::handle_error(clingo_theory_atoms_atom_elements(atoms_, id_, &ret, &n));
    return {ret, n, ToTheoryIterator<TheoryElementIterator>{atoms_}};
}

inline TheoryTerm TheoryAtom::term() const {
    clingo_id_t ret = 0;
    Detail::handle_error(clingo_theory_atoms_atom_term(atoms_, id_, &ret));
    return TheoryTerm{atoms_, ret};
}

inline bool TheoryAtom::has_guard() const {
    bool ret = false;
    Detail::handle_error(clingo_theory_atoms_atom_has_guard(atoms_, id_, &ret));
    return ret;
}

inline literal_t TheoryAtom::literal() const {
    clingo_literal_t ret = 0;
    Detail::handle_error(clingo_theory_atoms_atom_literal(atoms_, id_, &ret));
    return ret;
}

inline std::pair<char const *, TheoryTerm> TheoryAtom::guard() const {
    char const *name = nullptr;
    clingo_id_t term = 0;
    Detail::handle_error(clingo_theory_atoms_atom_guard(atoms_, id_, &name, &term));
    return {name, TheoryTerm{atoms_, term}};
}

inline std::string TheoryAtom::to_string() const {
    return Detail::to_string(clingo_theory_atoms_atom_to_string_size, clingo_theory_atoms_atom_to_string, atoms_, id_);
}

inline std::ostream &operator<<(std::ostream &out, TheoryAtom term) {
    out << term.to_string();
    return out;
}

inline TheoryAtomIterator TheoryAtoms::begin() const {
    return TheoryAtomIterator{atoms_, 0};
}

inline TheoryAtomIterator TheoryAtoms::end() const {
    return TheoryAtomIterator{atoms_, clingo_id_t(size())};
}

inline size_t TheoryAtoms::size() const {
    size_t ret = 0;
    Detail::handle_error(clingo_theory_atoms_size(atoms_, &ret));
    return ret;
}

// {{{2 trail

inline uint32_t Trail::size() const {
    uint32_t ret = 0;
    Detail::handle_error(clingo_assignment_trail_size(ass_, &ret));
    return ret;
}

inline uint32_t Trail::begin_offset(uint32_t level) const {
    uint32_t ret = 0;
    Detail::handle_error(clingo_assignment_trail_begin(ass_, level, &ret));
    return ret;
}

inline uint32_t Trail::end_offset(uint32_t level) const {
    uint32_t ret = 0;
    Detail::handle_error(clingo_assignment_trail_end(ass_, level, &ret));
    return ret;
}

inline literal_t Trail::at(uint32_t offset) const {
    clingo_literal_t ret = 0;
    Detail::handle_error(clingo_assignment_trail_at(ass_, offset, &ret));
    return ret;
}

// {{{2 assignment

inline bool Assignment::has_conflict() const {
    return clingo_assignment_has_conflict(ass_);
}

inline uint32_t Assignment::decision_level() const {
    return clingo_assignment_decision_level(ass_);
}

inline uint32_t Assignment::root_level() const {
    return clingo_assignment_root_level(ass_);
}

inline bool Assignment::has_literal(literal_t lit) const {
    return clingo_assignment_has_literal(ass_, lit);
}

inline TruthValue Assignment::truth_value(literal_t lit) const {
    clingo_truth_value_t ret = 0;
    Detail::handle_error(clingo_assignment_truth_value(ass_, lit, &ret));
    return static_cast<TruthValue>(ret);
}

inline uint32_t Assignment::level(literal_t lit) const {
    uint32_t ret = 0;
    Detail::handle_error(clingo_assignment_level(ass_, lit, &ret));
    return ret;
}

inline literal_t Assignment::decision(uint32_t level) const {
    literal_t ret = 0;
    Detail::handle_error(clingo_assignment_decision(ass_, level, &ret));
    return ret;
}

inline bool Assignment::is_fixed(literal_t lit) const {
    bool ret = false;
    Detail::handle_error(clingo_assignment_is_fixed(ass_, lit, &ret));
    return ret;
}

inline bool Assignment::is_true(literal_t lit) const {
    bool ret = false;
    Detail::handle_error(clingo_assignment_is_true(ass_, lit, &ret));
    return ret;
}

inline bool Assignment::is_false(literal_t lit) const {
    bool ret = false;
    Detail::handle_error(clingo_assignment_is_false(ass_, lit, &ret));
    return ret;
}

inline size_t Assignment::size() const {
    return clingo_assignment_size(ass_);
}

inline bool Assignment::is_total() const {
    return clingo_assignment_is_total(ass_);
}

inline literal_t Assignment::at(size_t offset) const {
    clingo_literal_t ret = 0;
    Detail::handle_error(clingo_assignment_at(ass_, offset, &ret));
    return ret;
}

// {{{2 propagate init

inline literal_t PropagateInit::solver_literal(literal_t lit) const {
    literal_t ret = 0;
    Detail::handle_error(clingo_propagate_init_solver_literal(init_, lit, &ret));
    return ret;
}

inline void PropagateInit::add_watch(literal_t lit) {
    Detail::handle_error(clingo_propagate_init_add_watch(init_, lit));
}

inline void PropagateInit::add_watch(literal_t lit, id_t thread_id) {
    Detail::handle_error(clingo_propagate_init_add_watch_to_thread(init_, lit, thread_id));
}

inline void PropagateInit::remove_watch(literal_t lit) {
    Detail::handle_error(clingo_propagate_init_remove_watch(init_, lit));
}

inline void PropagateInit::remove_watch(literal_t literal, id_t thread_id) {
    Detail::handle_error(clingo_propagate_init_remove_watch_from_thread(init_, literal, thread_id));
}

inline void PropagateInit::freeze_literal(literal_t lit) {
    Detail::handle_error(clingo_propagate_init_freeze_literal(init_, lit));
}

inline int PropagateInit::number_of_threads() const {
    return clingo_propagate_init_number_of_threads(init_);
}

inline Assignment PropagateInit::assignment() const {
    return Assignment{clingo_propagate_init_assignment(init_)};
}

inline SymbolicAtoms PropagateInit::symbolic_atoms() const {
    clingo_symbolic_atoms_t const *ret = nullptr;
    Detail::handle_error(clingo_propagate_init_symbolic_atoms(init_, &ret));
    return SymbolicAtoms{ret};
}

inline TheoryAtoms PropagateInit::theory_atoms() const {
    clingo_theory_atoms_t const *ret = nullptr;
    Detail::handle_error(clingo_propagate_init_theory_atoms(init_, &ret));
    return TheoryAtoms{ret};
}

inline PropagatorCheckMode PropagateInit::get_check_mode() const {
    return static_cast<PropagatorCheckMode>(clingo_propagate_init_get_check_mode(init_));
}

inline PropagatorUndoMode PropagateInit::get_undo_mode() const {
    return static_cast<PropagatorUndoMode>(clingo_propagate_init_get_undo_mode(init_));
}

inline void PropagateInit::set_check_mode(PropagatorCheckMode mode) {
    clingo_propagate_init_set_check_mode(init_, mode);
}

inline void PropagateInit::set_undo_mode(PropagatorUndoMode mode) {
    clingo_propagate_init_set_undo_mode(init_, static_cast<clingo_propagator_undo_mode_t>(mode));
}

inline literal_t PropagateInit::add_literal(bool freeze) {
    literal_t ret = 0;
    Detail::handle_error(clingo_propagate_init_add_literal(init_, freeze, &ret));
    return ret;
}

inline bool PropagateInit::add_clause(LiteralSpan clause) {
    bool ret = false;
    Detail::handle_error(clingo_propagate_init_add_clause(init_, clause.begin(), clause.size(), &ret));
    return ret;
}

inline bool PropagateInit::add_weight_constraint(literal_t literal, WeightedLiteralSpan literals, weight_t bound, WeightConstraintType type, bool compare_equal) {
    bool ret = false;
    Detail::handle_error(clingo_propagate_init_add_weight_constraint(init_, literal, Detail::cast<clingo_weighted_literal_t const *>(literals.begin()), literals.size(), bound, type, compare_equal, &ret));
    return ret;
}

inline void PropagateInit::add_minimize(literal_t literal, weight_t weight, weight_t priority) {
    Detail::handle_error(clingo_propagate_init_add_minimize(init_, literal, weight, priority));
}

inline bool PropagateInit::propagate() {
    bool ret = false;
    Detail::handle_error(clingo_propagate_init_propagate(init_, &ret));
    return ret;
}

// {{{2 propagate control

inline id_t PropagateControl::thread_id() const {
    return clingo_propagate_control_thread_id(ctl_);
}

inline Assignment PropagateControl::assignment() const {
    return Assignment{clingo_propagate_control_assignment(ctl_)};
}

inline literal_t PropagateControl::add_literal() {
    clingo_literal_t ret = 0;
    Detail::handle_error(clingo_propagate_control_add_literal(ctl_, &ret));
    return ret;
}

inline void PropagateControl::add_watch(literal_t literal) {
    Detail::handle_error(clingo_propagate_control_add_watch(ctl_, literal));
}

inline bool PropagateControl::has_watch(literal_t literal) const {
    return clingo_propagate_control_has_watch(ctl_, literal);
}

inline void PropagateControl::remove_watch(literal_t literal) {
    clingo_propagate_control_remove_watch(ctl_, literal);
}

inline bool PropagateControl::add_clause(LiteralSpan clause, ClauseType type) {
    bool ret = false;
    Detail::handle_error(clingo_propagate_control_add_clause(ctl_, clause.begin(), clause.size(), static_cast<clingo_clause_type_t>(type), &ret));
    return ret;
}

inline bool PropagateControl::propagate() {
    bool ret = false;
    Detail::handle_error(clingo_propagate_control_propagate(ctl_, &ret));
    return ret;
}

// {{{2 propagator

inline void Propagator::init(PropagateInit &init) {
    static_cast<void>(init);
}

inline void Propagator::propagate(PropagateControl &ctl, LiteralSpan changes) {
    static_cast<void>(ctl);
    static_cast<void>(changes);
}

inline void Propagator::undo(PropagateControl const &ctl, LiteralSpan changes) noexcept {
    static_cast<void>(ctl);
    static_cast<void>(changes);
}

inline void Propagator::check(PropagateControl &ctl) {
    static_cast<void>(ctl);
}

inline literal_t Heuristic::decide(id_t thread_id, Assignment const &assign, literal_t fallback) {
    static_cast<void>(thread_id);
    static_cast<void>(assign);
    static_cast<void>(fallback);
    return 0;
}

// {{{2 solve control

inline SymbolicAtoms SolveControl::symbolic_atoms() const {
    clingo_symbolic_atoms_t const *atoms = nullptr;
    Detail::handle_error(clingo_solve_control_symbolic_atoms(ctl_, &atoms));
    return SymbolicAtoms{atoms};
}

inline void SolveControl::add_clause(SymbolicLiteralSpan clause) {
    std::vector<literal_t> lits;
    auto atoms = symbolic_atoms();
    for (auto const &x : clause) {
        auto it = atoms.find(x.symbol());
        if (it != atoms.end()) {
            auto lit = it->literal();
            lits.emplace_back(x.is_positive() ? lit : -lit);
        }
        else if (x.is_negative()) { return; }
    }
    add_clause(LiteralSpan{lits});
}

inline void SolveControl::add_clause(LiteralSpan clause) {
    Detail::handle_error(clingo_solve_control_add_clause(ctl_, Detail::cast<clingo_literal_t const *>(clause.begin()), clause.size()));
}

// {{{2 model

inline Model::Model(clingo_model_t *model)
: model_(model) { }

inline bool Model::contains(Symbol atom) const {
    bool ret = false;
    Detail::handle_error(clingo_model_contains(model_, atom.to_c(), &ret));
    return ret;
}

inline bool Model::is_true(literal_t literal) const {
    bool ret = false;
    Detail::handle_error(clingo_model_is_true(model_, literal, &ret));
    return ret;
}

inline ConsequenceType Model::is_consequence(literal_t literal) const {
    clingo_consequence_t ret = clingo_consequence_false;
    Detail::handle_error(clingo_model_is_consequence(model_, literal, &ret));
    return static_cast<ConsequenceType>(ret);
}

inline CostVector Model::cost() const {
    CostVector ret;
    size_t n = 0;
    Detail::handle_error(clingo_model_cost_size(model_, &n));
    ret.resize(n);
    Detail::handle_error(clingo_model_cost(model_, ret.data(), n));
    return ret;
}

inline PriorityVector Model::priorities() const {
    PriorityVector ret;
    size_t n = 0;
    Detail::handle_error(clingo_model_cost_size(model_, &n));
    ret.resize(n);
    Detail::handle_error(clingo_model_priority(model_, ret.data(), n));
    return ret;
}

inline void Model::extend(SymbolSpan symbols) {
    Detail::handle_error(clingo_model_extend(model_, Detail::cast<clingo_symbol_t const *>(symbols.begin()), symbols.size()));
}

inline SymbolVector Model::symbols(ShowType show) const {
    SymbolVector ret;
    size_t n = 0;
    Detail::handle_error(clingo_model_symbols_size(model_, show, &n));
    ret.resize(n);
    Detail::handle_error(clingo_model_symbols(model_, show, Detail::cast<clingo_symbol_t *>(ret.data()), n));
    return ret;
}

inline uint64_t Model::number() const {
    uint64_t ret = 0;
    Detail::handle_error(clingo_model_number(model_, &ret));
    return ret;
}

inline bool Model::optimality_proven() const {
    bool ret = false;
    Detail::handle_error(clingo_model_optimality_proven(model_, &ret));
    return ret;
}

inline SolveControl Model::context() const {
    clingo_solve_control_t *ret = nullptr;
    Detail::handle_error(clingo_model_context(model_, &ret));
    return SolveControl{ret};
}

inline ModelType Model::type() const {
    clingo_model_type_t ret = 0;
    Detail::handle_error(clingo_model_type(model_, &ret));
    return static_cast<ModelType>(ret);
}

inline id_t Model::thread_id() const {
    id_t ret = 0;
    Detail::handle_error(clingo_model_thread_id(model_, &ret));
    return ret;
}

// {{{2 solve handle

namespace Detail {

} // namespace Detail

inline SolveHandle::SolveHandle(clingo_solve_handle_t *it, Detail::AssignOnce &ptr)
: iter_(it)
, exception_(&ptr) { }

inline SolveHandle::SolveHandle(SolveHandle &&it) noexcept
: SolveHandle() { *this = std::move(it); }

inline SolveHandle &SolveHandle::operator=(SolveHandle &&it) noexcept {
    std::swap(iter_, it.iter_);
    std::swap(exception_, it.exception_);
    return *this;
}

inline void SolveHandle::resume() {
    Detail::handle_error(clingo_solve_handle_resume(iter_), *exception_);
}

inline void SolveHandle::wait() {
    (void)wait(-1);
}

inline bool SolveHandle::wait(double timeout) {
    bool res = true;
    clingo_solve_handle_wait(iter_, timeout, &res);
    return res;
}

inline Model const &SolveHandle::model() {
    clingo_model_t const *m = nullptr;
    Detail::handle_error(clingo_solve_handle_model(iter_, &m), *exception_);
    new (&model_) Model{const_cast<clingo_model_t*>(m)}; // NOLINT
    return model_;
}

inline LiteralSpan SolveHandle::core() {
    literal_t const *core = nullptr;
    size_t size = 0;
    Detail::handle_error(clingo_solve_handle_core(iter_, &core, &size), *exception_);
    return {core, size};
}

inline Model const &SolveHandle::next() {
    resume();
    return model();
}

inline SolveResult SolveHandle::get() {
    clingo_solve_result_bitset_t ret = 0;
    Detail::handle_error(clingo_solve_handle_get(iter_, &ret), *exception_);
    return SolveResult{ret};
}

inline void SolveHandle::cancel() {
    Detail::handle_error(clingo_solve_handle_cancel(iter_), *exception_);
}

inline SolveHandle::~SolveHandle() noexcept(false) {
    close();
}

inline void SolveHandle::close() {
    if (iter_ != nullptr) {
        Detail::handle_error(clingo_solve_handle_close(iter_), *exception_);
        iter_ = nullptr;
    }
}

// {{{2 backend

inline Backend::Backend(Backend &&backend) noexcept {
    *this = std::move(backend);
}

inline Backend &Backend::operator=(Backend &&backend) noexcept {
    std::swap(backend_, backend.backend_);
    return *this;
}

inline Backend::Backend(clingo_backend_t *backend)
: backend_{backend} {
    Detail::handle_error(clingo_backend_begin(backend_));
}

inline Backend::~Backend() noexcept(false) {
    close();
}

inline void Backend::close() {
    if (backend_ != nullptr) {
        Detail::handle_error(clingo_backend_end(backend_));
        backend_ = nullptr;
    }
}

inline void Backend::rule(bool choice, AtomSpan head, LiteralSpan body) {
    Detail::handle_error(clingo_backend_rule(backend_, choice, head.begin(), head.size(), body.begin(), body.size()));
}

inline void Backend::weight_rule(bool choice, AtomSpan head, weight_t lower, WeightedLiteralSpan body) {
    Detail::handle_error(clingo_backend_weight_rule(backend_, choice, head.begin(), head.size(), lower, Detail::cast<clingo_weighted_literal_t const *>(body.begin()), body.size()));
}

inline void Backend::minimize(weight_t prio, WeightedLiteralSpan body) {
    Detail::handle_error(clingo_backend_minimize(backend_, prio, Detail::cast<clingo_weighted_literal_t const *>(body.begin()), body.size()));
}

inline void Backend::project(AtomSpan atoms) {
    Detail::handle_error(clingo_backend_project(backend_, atoms.begin(), atoms.size()));
}

inline void Backend::external(atom_t atom, ExternalType type) {
    Detail::handle_error(clingo_backend_external(backend_, atom, static_cast<clingo_external_type_t>(type)));
}

inline void Backend::assume(LiteralSpan lits) {
    Detail::handle_error(clingo_backend_assume(backend_, lits.begin(), lits.size()));
}

inline void Backend::heuristic(atom_t atom, HeuristicType type, int bias, unsigned priority, LiteralSpan condition) {
    Detail::handle_error(clingo_backend_heuristic(backend_, atom, static_cast<clingo_heuristic_type_t>(type), bias, priority, condition.begin(), condition.size()));
}

inline void Backend::acyc_edge(int node_u, int node_v, LiteralSpan condition) {
    Detail::handle_error(clingo_backend_acyc_edge(backend_, node_u, node_v, condition.begin(), condition.size()));
}

inline atom_t Backend::add_atom() {
    clingo_atom_t ret = 0;
    Detail::handle_error(clingo_backend_add_atom(backend_, nullptr, &ret));
    return ret;
}

inline atom_t Backend::add_atom(Symbol symbol) {
    clingo_atom_t ret = 0;
    clingo_symbol_t sym = symbol.to_c();
    Detail::handle_error(clingo_backend_add_atom(backend_, &sym, &ret));
    return ret;
}

inline id_t Backend::add_theory_term_number(int number) {
    clingo_id_t ret = 0;
    Detail::handle_error(clingo_backend_theory_term_number(backend_, number, &ret));
    return ret;
}

inline id_t Backend::add_theory_term_string(char const *string) {
    clingo_id_t ret = 0;
    Detail::handle_error(clingo_backend_theory_term_string(backend_, string, &ret));
    return ret;
}

inline id_t Backend::add_theory_term_sequence(TheorySequenceType type, IdSpan elements) {
    clingo_id_t ret = 0;
    Detail::handle_error(clingo_backend_theory_term_sequence(backend_, static_cast<clingo_theory_sequence_type_e>(type), elements.data(), elements.size(), &ret));
    return ret;
}

inline id_t Backend::add_theory_term_function(char const *name, IdSpan elements) {
    clingo_id_t ret = 0;
    Detail::handle_error(clingo_backend_theory_term_function(backend_, name, elements.data(), elements.size(), &ret));
    return ret;
}

inline id_t Backend::add_theory_term_symbol(Symbol symbol) {
    clingo_id_t ret = 0;
    Detail::handle_error(clingo_backend_theory_term_symbol(backend_, symbol.to_c(), &ret));
    return ret;
}

inline id_t Backend::add_theory_element(IdSpan tuple, LiteralSpan condition) {
    clingo_id_t ret = 0;
    Detail::handle_error(clingo_backend_theory_element(backend_, tuple.data(), tuple.size(), condition.begin(), condition.size(), &ret));
    return ret;
}

inline atom_t Backend::theory_atom(id_t term_id, IdSpan elements) {
    atom_t ret = 0;
    Detail::handle_error(clingo_backend_theory_atom(backend_, std::numeric_limits<atom_t>::max(), term_id, elements.begin(), elements.size(), &ret));
    return ret;
}

inline atom_t Backend::theory_atom(id_t term_id, IdSpan elements, char const *operator_name, id_t right_hand_side_id) {
    atom_t ret = 0;
    Detail::handle_error(clingo_backend_theory_atom_with_guard(backend_, std::numeric_limits<atom_t>::max(), term_id, elements.begin(), elements.size(), operator_name, right_hand_side_id, &ret));
    return ret;
}

inline atom_t Backend::theory_atom(id_t atom_id_or_zero, id_t term_id, IdSpan elements) {
    atom_t ret = 0;
    Detail::handle_error(clingo_backend_theory_atom(backend_, atom_id_or_zero, term_id, elements.begin(), elements.size(), &ret));
    return ret;
}

inline atom_t Backend::theory_atom(id_t atom_id_or_zero, id_t term_id, IdSpan elements, char const *operator_name, id_t right_hand_side_id) {
    atom_t ret = 0;
    Detail::handle_error(clingo_backend_theory_atom_with_guard(backend_, atom_id_or_zero, term_id, elements.begin(), elements.size(), operator_name, right_hand_side_id, &ret));
    return ret;
}

// {{{2 statistics

template <bool constant>
inline StatisticsType StatisticsBase<constant>::type() const {
    clingo_statistics_type_t ret = 0;
    Detail::handle_error(clingo_statistics_type(stats_, key_, &ret));
    return StatisticsType(ret);
}

template <bool constant>
inline size_t StatisticsBase<constant>::size() const {
    size_t ret = 0;
    Detail::handle_error(clingo_statistics_array_size(stats_, key_, &ret));
    return ret;
}

template <bool constant>
void StatisticsBase<constant>::ensure_size(size_t size, StatisticsType type) {
    for (auto s = this->size(); s < size; ++s) { push(type); }
}

template <bool constant>
inline StatisticsBase<constant> StatisticsBase<constant>::operator[](size_t index) const {
    uint64_t ret = 0;
    Detail::handle_error(clingo_statistics_array_at(stats_, key_, index, &ret));
    return StatisticsBase{stats_, ret};
}

template <bool constant>
inline StatisticsBase<constant> StatisticsBase<constant>::push(StatisticsType type) {
    uint64_t ret = 0;
    Detail::handle_error(clingo_statistics_array_push(stats_, key_, static_cast<clingo_statistics_type_t>(type), &ret));
    return StatisticsBase{stats_, ret};
}


template <bool constant>
inline typename StatisticsBase<constant>::ArrayIteratorT StatisticsBase<constant>::begin() const {
    return ArrayIteratorT{this, 0};
}

template <bool constant>
inline typename StatisticsBase<constant>::ArrayIteratorT StatisticsBase<constant>::end() const {
    return ArrayIteratorT{this, size()};
}

template <bool constant>
inline StatisticsBase<constant> StatisticsBase<constant>::operator[](char const *name) const {
    uint64_t ret = 0;
    Detail::handle_error(clingo_statistics_map_at(stats_, key_, name, &ret));
    return StatisticsBase{stats_, ret};
}

template <bool constant>
inline bool StatisticsBase<constant>::has_subkey(char const *name) const {
    bool ret = false;
    Detail::handle_error(clingo_statistics_map_has_subkey(stats_, key_, name, &ret));
    return ret;
}

template <bool constant>
inline StatisticsBase<constant> StatisticsBase<constant>::add_subkey(char const *name, StatisticsType type) {
    uint64_t ret = 0;
    Detail::handle_error(clingo_statistics_map_add_subkey(stats_, key_, name, static_cast<clingo_statistics_type_t>(type), &ret));
    return StatisticsBase{stats_, ret};
}

template <bool constant>
inline typename StatisticsBase<constant>::KeyRangeT StatisticsBase<constant>::keys() const {
    size_t ret = 0;
    Detail::handle_error(clingo_statistics_map_size(stats_, key_, &ret));
    return KeyRangeT{ KeyIteratorT{this, 0}, KeyIteratorT{this, ret} };
}

template <bool constant>
inline double StatisticsBase<constant>::value() const {
    double ret = 0;
    Detail::handle_error(clingo_statistics_value_get(stats_, key_, &ret));
    return ret;
}

template <bool constant>
inline void StatisticsBase<constant>::set_value(double d) {
    Detail::handle_error(clingo_statistics_value_set(stats_, key_, d));
}

template <bool constant>
inline char const *StatisticsBase<constant>::key_name(size_t index) const {
    char const *ret = nullptr;
    Detail::handle_error(clingo_statistics_map_subkey_name(stats_, key_, index, &ret));
    return ret;
}

// {{{2 configuration

inline Configuration Configuration::operator[](size_t index) {
    unsigned ret = 0;
    Detail::handle_error(clingo_configuration_array_at(conf_, key_, index, &ret));
    return Configuration{conf_, ret};
}

inline ConfigurationArrayIterator Configuration::begin() {
    return ConfigurationArrayIterator{this, 0};
}

inline ConfigurationArrayIterator Configuration::end() {
    return ConfigurationArrayIterator{this, size()};
}

inline size_t Configuration::size() const {
    size_t n = 0;
    Detail::handle_error(clingo_configuration_array_size(conf_, key_, &n));
    return n;
}

inline bool Configuration::empty() const {
    return size() == 0;
}

inline Configuration Configuration::operator[](char const *name) {
    clingo_id_t ret = 0;
    Detail::handle_error(clingo_configuration_map_at(conf_, key_, name, &ret));
    return Configuration{conf_, ret};
}

inline ConfigurationKeyRange Configuration::keys() const {
    size_t n = 0;
    Detail::handle_error(clingo_configuration_map_size(conf_, key_, &n));
    return ConfigurationKeyRange{ ConfigurationKeyIterator{this, size_t(0)}, ConfigurationKeyIterator{this, size_t(n)} };
}

inline bool Configuration::is_value() const {
    clingo_configuration_type_bitset_t type = 0;
    Detail::handle_error(clingo_configuration_type(conf_, key_, &type));
    return (type & clingo_configuration_type_value) != 0;
}

inline bool Configuration::is_array() const {
    clingo_configuration_type_bitset_t type = 0;
    Detail::handle_error(clingo_configuration_type(conf_, key_, &type));
    return (type & clingo_configuration_type_array) != 0;
}

inline bool Configuration::is_map() const {
    clingo_configuration_type_bitset_t type = 0;
    Detail::handle_error(clingo_configuration_type(conf_, key_, &type));
    return (type & clingo_configuration_type_map) != 0;
}

inline bool Configuration::is_assigned() const {
    bool ret = false;
    Detail::handle_error(clingo_configuration_value_is_assigned(conf_, key_, &ret));
    return ret;
}

inline std::string Configuration::value() const {
    size_t n = 0;
    Detail::handle_error(clingo_configuration_value_get_size(conf_, key_, &n));
    std::vector<char> ret(n);
    Detail::handle_error(clingo_configuration_value_get(conf_, key_, ret.data(), n));
    return {ret.begin(), ret.end() - 1};
}

inline Configuration &Configuration::operator=(char const *value) {
    Detail::handle_error(clingo_configuration_value_set(conf_, key_, value));
    return *this;
}

inline char const *Configuration::decription() const {
    char const *ret = nullptr;
    Detail::handle_error(clingo_configuration_description(conf_, key_, &ret));
    return ret;
}

inline char const *Configuration::key_name(size_t index) const {
    char const *ret = nullptr;
    Detail::handle_error(clingo_configuration_map_subkey_name(conf_, key_, index, &ret));
    return ret;
}


// {{{2 ast

namespace AST {

namespace ASTDetail {

template <size_t j>
struct construct_ast {
    template <size_t i, class... Args>
    static clingo_ast_t *construct(clingo_ast_type_t type, clingo_ast_constructor_t const &cons, int arg, Args&& ...args) {
        if (cons.size <= i || cons.arguments[i].type != clingo_ast_attribute_type_number) { // NOLINT
            throw std::runtime_error("invalid argument");
        }
        return construct_ast<j - 1>::template construct<i + 1>(type, cons, std::forward<Args>(args)..., arg);
    }
    template <size_t i, class... Args>
    static clingo_ast_t *construct(clingo_ast_type_t type, clingo_ast_constructor_t const &cons, char const *arg, Args&& ...args) {
        if (cons.size <= i || cons.arguments[i].type != clingo_ast_attribute_type_string) { // NOLINT
            throw std::runtime_error("invalid argument");
        }
        return construct_ast<j - 1>::template construct<i + 1>(type, cons, std::forward<Args>(args)..., arg);
    }
    template <size_t i, class... Args>
    static clingo_ast_t *construct(clingo_ast_type_t type, clingo_ast_constructor_t const &cons, Symbol const &arg, Args&& ...args) {
        if (cons.size <= i || cons.arguments[i].type != clingo_ast_attribute_type_symbol) { // NOLINT
            throw std::runtime_error("invalid argument");
        }
        return construct_ast<j - 1>::template construct<i + 1>(type, cons, std::forward<Args>(args)..., arg.to_c());
    }
    template <size_t i, class... Args>
    static clingo_ast_t *construct(clingo_ast_type_t type, clingo_ast_constructor_t const &cons, Location const &arg, Args&& ...args) {
        if (cons.size <= i || cons.arguments[i].type != clingo_ast_attribute_type_location) { // NOLINT
            throw std::runtime_error("invalid argument");
        }
        return construct_ast<j - 1>::template construct<i + 1>(type, cons, std::forward<Args>(args)..., &arg);
    }
    template <size_t i, class... Args>
    static clingo_ast_t *construct(clingo_ast_type_t type, clingo_ast_constructor_t const &cons, Node const &arg, Args&& ...args) {
        if (cons.size <= i || cons.arguments[i].type != clingo_ast_attribute_type_ast) { // NOLINT
            throw std::runtime_error("invalid argument");
        }
        return construct_ast<j - 1>::template construct<i + 1>(type, cons, std::forward<Args>(args)..., arg.to_c());
    }
    template <size_t i, class... Args>
    static clingo_ast_t *construct(clingo_ast_type_t type, clingo_ast_constructor_t const &cons, Optional<Node> const &arg, Args&& ...args) {
        if (cons.size <= i || cons.arguments[i].type != clingo_ast_attribute_type_optional_ast) { // NOLINT
            throw std::runtime_error("invalid argument");
        }
        return construct_ast<j - 1>::template construct<i + 1>(type, cons, std::forward<Args>(args)..., arg.get());
    }
    template <size_t i, class... Args>
    static clingo_ast_t *construct(clingo_ast_type_t type, clingo_ast_constructor_t const &cons, std::vector<Node> const &arg, Args&& ...args) {
        if (cons.size <= i || cons.arguments[i].type != clingo_ast_attribute_type_ast_array) { // NOLINT
            throw std::runtime_error("invalid argument");
        }
        return construct_ast<j - 1>::template construct<i + 1>(type, cons, std::forward<Args>(args)..., arg.data(), arg.size());
    }
    template <size_t i, class... Args>
    static clingo_ast_t *construct(clingo_ast_type_t type, clingo_ast_constructor_t const &cons, std::vector<char const *> const &arg, Args&& ...args) {
        if (cons.size <= i || cons.arguments[i].type != clingo_ast_attribute_type_string_array) { // NOLINT
            throw std::runtime_error("invalid argument");
        }
        return construct_ast<j - 1>::template construct<i + 1>(type, cons, std::forward<Args>(args)..., arg.data(), arg.size());
    }
};

template <>
struct construct_ast<0> {
    template <size_t i, class... Args>
    static clingo_ast_t *construct(clingo_ast_type_t type, clingo_ast_constructor_t const &cons, Args&& ...args) {
        if (cons.size != i) {
            throw std::runtime_error("invalid argument");
        }
        clingo_ast_t *ret = nullptr;
        clingo_ast_build(type, &ret, std::forward<Args>(args)...);
        return ret;
    }
};

template <class V>
struct ASTVisitor {
    void operator()(Attribute attr, NodeValue value) {
        if (value.is<Node>()) {
            auto &ast = value.get<Node>();
            if (v(ast)) {
                ast.visit_attribute(*this);
            }
        }
        else if (value.is<Optional<Node>>()) {
            auto *ast = value.get<Optional<Node>>().get();
            if (ast != nullptr) {
                if (v(*ast)) {
                    ast->visit_attribute(*this);
                }
            }
        }
        else if (value.is<NodeVector>()) {
            for (Node ast : value.get<NodeVector>()) {
                if (v(ast)) {
                    ast.visit_attribute(*this);
                }
            }
        }
    }
    V &v; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
};

} // namespace ASTDetail

// Node

inline Node::Node(clingo_ast_t *ast)
: ast_{ast} { }

template <class... Args>
Node::Node(Type type, Args&& ...args)
: ast_{ASTDetail::construct_ast<sizeof...(Args)>::template construct<0>(
    static_cast<clingo_ast_type_t>(type),
    g_clingo_ast_constructors.constructors[static_cast<size_t>(type)], // NOLINT
    std::forward<Args>(args)...)} {
}

inline Node::Node(Node const &ast)
: ast_{ast.ast_} {
    clingo_ast_acquire(ast_);
}

inline Node::Node(Node &&ast) noexcept
: ast_{ast.ast_} {
    ast.ast_ = nullptr;
}

inline Node &Node::operator=(Node const &ast) { // NOLINT
    if (ast_ != ast.ast_) {
        if (ast_ != nullptr) {
            clingo_ast_release(ast_);
        }
        ast_ = ast.ast_;
        if (ast_ != nullptr) {
            clingo_ast_acquire(ast_);
        }
    }
    return *this;
}

inline Node &Node::operator=(Node &&ast) noexcept {
    if (ast_ != ast.ast_) {
        if (ast_ != nullptr) {
            clingo_ast_release(ast_);
            ast_ = nullptr;
        }
        std::swap(ast_, ast.ast_);
    }
    return *this;
}

inline Node::~Node() {
    if (ast_ != nullptr) {
        clingo_ast_release(ast_);
    }
}

inline Node Node::copy() const {
    clingo_ast_t *ast = nullptr;
    Detail::handle_error(clingo_ast_copy(ast_, &ast));
    return Node{ast};
}

inline Node Node::deep_copy() const {
    clingo_ast_t *ast = nullptr;
    Detail::handle_error(clingo_ast_deep_copy(ast_, &ast));
    return Node{ast};
}

inline Type Node::type() const {
    clingo_ast_type_t type = 0;
    Detail::handle_error(clingo_ast_get_type(ast_, &type));
    return static_cast<Type>(type);
}

inline NodeValue Node::get(Attribute attribute) const {
    bool has_attribute = false;
    auto attr = static_cast<clingo_ast_attribute_t>(attribute);
    Detail::handle_error(clingo_ast_has_attribute(ast_, attr, &has_attribute));
    if (!has_attribute) {
        throw std::runtime_error("unknown attribute");
    }
    clingo_ast_attribute_type_t type = 0;
    Detail::handle_error(clingo_ast_attribute_type(ast_, attr, &type));
    switch (static_cast<clingo_ast_attribute_type_e>(type)) {
        case clingo_ast_attribute_type_number: {
            int ret = 0;
            Detail::handle_error(clingo_ast_attribute_get_number(ast_, attr, &ret));
            return {ret};
        }
        case clingo_ast_attribute_type_symbol: {
            clingo_symbol_t ret = 0;
            Detail::handle_error(clingo_ast_attribute_get_symbol(ast_, attr, &ret));
            return {Clingo::Symbol{ret}};
        }
        case clingo_ast_attribute_type_location: {
            clingo_location_t ret;
            Detail::handle_error(clingo_ast_attribute_get_location(ast_, attr, &ret));
            return {Clingo::Location{ret}};
        }
        case clingo_ast_attribute_type_string: {
            char const *ret = nullptr;
            Detail::handle_error(clingo_ast_attribute_get_string(ast_, attr, &ret));
            return {ret};
        }
        case clingo_ast_attribute_type_ast: {
            clingo_ast_t *ret = nullptr;
            Detail::handle_error(clingo_ast_attribute_get_ast(ast_, attr, &ret));
            return {Node{ret}};
        }
        case clingo_ast_attribute_type_optional_ast: {
            clingo_ast_t *ret = nullptr;
            Detail::handle_error(clingo_ast_attribute_get_optional_ast(ast_, attr, &ret));
            return ret == nullptr ? Optional<Node>{} : Optional<Node>{Node{ret}};
        }
        case clingo_ast_attribute_type_string_array: {
            return {StringVector{*this, attr}};
        }
        case clingo_ast_attribute_type_ast_array: {
            break;
        }
    }
    return {NodeVector{*this, attr}};
}

template <class T>
T Node::get(Attribute attribute) const {
    return get(attribute).get<T>();
}

inline void Node::set(Attribute attribute, NodeValue value) {
    bool has_attribute = false;
    auto attr = static_cast<clingo_ast_attribute_t>(attribute);
    Detail::handle_error(clingo_ast_has_attribute(ast_, attr, &has_attribute));
    if (!has_attribute) {
        throw std::runtime_error("unknow attribute");
    }
    clingo_ast_attribute_type_t type = 0;
    Detail::handle_error(clingo_ast_attribute_type(ast_, attr, &type));
    switch (static_cast<clingo_ast_attribute_type_e>(type)) {
        case clingo_ast_attribute_type_number: {
            return Detail::handle_error(clingo_ast_attribute_set_number(ast_, attr, value.get<int>()));
        }
        case clingo_ast_attribute_type_symbol: {
            return Detail::handle_error(clingo_ast_attribute_set_symbol(ast_, attr, value.get<Symbol>().to_c()));
        }
        case clingo_ast_attribute_type_location: {
            return Detail::handle_error(clingo_ast_attribute_set_location(ast_, attr, &value.get<Location>()));
        }
        case clingo_ast_attribute_type_string: {
            return Detail::handle_error(clingo_ast_attribute_set_string(ast_, attr, value.get<char const*>()));
        }
        case clingo_ast_attribute_type_ast: {
            return Detail::handle_error(clingo_ast_attribute_set_ast(ast_, attr, value.get<Node>().ast_));
        }
        case clingo_ast_attribute_type_optional_ast: {
            auto *ast = value.get<Optional<Node>>().get();
            return Detail::handle_error(clingo_ast_attribute_set_optional_ast(ast_, attr, ast != nullptr ? ast->ast_ : nullptr));
        }
        case clingo_ast_attribute_type_string_array: {
            auto val = get(attribute);
            auto &a = val.get<StringVector>();
            auto &b = value.get<StringVector>();
            if (a.ast().to_c() != b.ast().to_c()) {
                a.clear();
                for (auto x : b) {
                    a.push_back(x);
                }
            }
            return;
        }
        case clingo_ast_attribute_type_ast_array: {
            auto val = get(attribute);
            auto &a = val.get<NodeVector>();
            auto &b = value.get<NodeVector>();
            if (a.ast().to_c() != b.ast().to_c()) {
                a.clear();
                for (auto x : b) {
                    a.push_back(x);
                }
            }
            return;
        }
    }
}

template <class Visitor>
inline void Node::visit_attribute(Visitor &&visitor) const {
    auto visit = std::forward<Visitor>(visitor);
    auto const &cons = g_clingo_ast_constructors.constructors[static_cast<size_t>(type())]; // NOLINT
    for (auto const &x : make_span(cons.arguments, cons.size)) {
        auto attr = static_cast<Attribute>(x.attribute);
        visit(attr, get(attr));
    }
}

template <class Visitor>
inline void Node::visit_ast(Visitor &&visitor) const {
    ASTDetail::ASTVisitor<Visitor> v{visitor};
    if (std::forward<Visitor>(visitor)(*this)) {
        visit_attribute(v);
    }
}

template <class Transformer>
Node Node::transform_ast(Transformer &&visitor) const {
    auto visit = std::forward<Transformer>(visitor);
    std::vector<std::pair<Attribute, Variant<Node, Optional<Node>, std::vector<Node>>>> result;
    visit_attribute([&](Attribute attr, NodeValue value) {
        if (value.is<Node>()) {
            auto &ast = value.get<Node>();
            auto *ptr = ast.to_c();
            auto tra = visit(ast);
            if (tra.to_c() != ptr) {
                result.emplace_back(attr, std::move(tra));
            }
        }
        else if (value.is<Optional<Node>>()) {
            auto *ast = value.get<Optional<Node>>().get();
            if (ast != nullptr) {
                auto *ptr = ast->to_c();
                auto tra = visit(*ast);
                if (tra.to_c() != ptr) {
                    result.emplace_back(attr, Optional<Node>{std::move(tra)});
                }
            }
        }
        else if (value.is<NodeVector>()) {
            auto &ast_vec = value.get<NodeVector>();
            bool changed = false;
            std::vector<Node> vec;
            for (auto it = ast_vec.begin(), ie = ast_vec.end(); it != ie; ++it) {
                Node ast = *it;
                auto *ptr = ast.to_c();
                auto tra = visit(*it);
                if (tra.to_c() != ptr && !changed) {
                    changed = true;
                    vec.reserve(ast_vec.size());
                    // NOTE: some msvc 2017 problem
                    // vec.assign(ast_vec.begin(), it);
                    for (auto jt = ast_vec.begin(); jt != it; ++jt) {
                        vec.emplace_back(*jt);
                    }
                }
                if (changed) {
                    vec.emplace_back(std::move(tra));
                }
            }
            if (changed) {
                result.emplace_back(attr, vec);
            }
        }
    });
    if (result.empty()) {
        return *this;
    }
    auto ret = copy();
    for (auto &x : result) {
        if (x.second.is<Node>()) {
            ret.set(x.first, x.second.get<Node>());
        }
        else if (x.second.is<Optional<Node>>()) {
            ret.set(x.first, x.second.get<Optional<Node>>());
        }
        else {
            auto val = ret.get<NodeVector>(x.first);
            auto &vec = x.second.get<std::vector<Node>>();
            val.clear();
            std::move(vec.begin(), vec.end(), std::back_inserter(val));
        }
    }
    return ret;
}

inline std::string Node::to_string() const {
    return Detail::to_string(clingo_ast_to_string_size, clingo_ast_to_string, ast_);
}

inline std::vector<Node> Node::unpool(bool other, bool condition) const {
    clingo_ast_unpool_type_bitset_t type = 0;
    if (other) {
        type |= clingo_ast_unpool_type_other;
    }
    if (condition) {
        type |= clingo_ast_unpool_type_condition;
    }
    using Data = std::pair<std::vector<Node>, std::exception_ptr>;
    Data data({}, nullptr);
    Detail::handle_error(clingo_ast_unpool(ast_, type, [](clingo_ast_t *ast, void *data) -> bool {
        auto &d = *static_cast<Data*>(data);
        clingo_ast_acquire(ast);
        CLINGO_CALLBACK_TRY { d.first.emplace_back(ast); }
        CLINGO_CALLBACK_CATCH(d.second);
    }, &data));
    return std::move(data.first);
}

inline std::ostream &operator<<(std::ostream &out, Node const &ast) {
    out << ast.to_string();
    return out;
}

inline bool operator<(Node const &a, Node const &b) {
    if (a.ast_ == nullptr || b.ast_ == nullptr) {
        throw std::runtime_error("invalid Node");
    }
    return clingo_ast_less_than(a.ast_, b.ast_);
}

inline bool operator>(Node const &a, Node const &b) {
    return b < a;
}

inline bool operator<=(Node const &a, Node const &b) {
    return !(b < a);
}

inline bool operator>=(Node const &a, Node const &b) {
    return !(a < b);
}

inline bool operator==(Node const &a, Node const &b) {
    if (a.ast_ == nullptr || b.ast_ == nullptr) {
        throw std::runtime_error("invalid Node");
    }
    return clingo_ast_equal(a.ast_, b.ast_);
}

inline bool operator!=(Node const &a, Node const &b) {
    return !(a == b);
}

inline size_t Node::hash() const {
    return clingo_ast_hash(ast_);
}

// NodeVector

inline NodeRef::NodeRef(NodeVector *vec, size_t index)
: vec_{vec}
, index_{index} { }

inline NodeRef &NodeRef::operator=(Node const &ast) {
    vec_->set(vec_->begin() + static_cast<NodeVector::iterator::difference_type>(index_), ast);
    return *this;
}

inline Node NodeRef::get() const {
    return static_cast<NodeVector const *>(vec_)->operator[](index_);
}

inline NodeRef::operator Node () const {
    return get();
}

inline NodeVector::NodeVector(Node ast, clingo_ast_attribute_t attr)
: ast_{std::move(ast)}
, attr_{attr} { }

inline NodeVector::iterator NodeVector::begin() {
    return iterator{this, 0};
}

inline NodeVector::iterator NodeVector::end() {
    return iterator{this, size()};
}

inline NodeVector::const_iterator NodeVector::begin() const {
    return const_iterator{this, 0};
}

inline NodeVector::const_iterator NodeVector::end() const {
    return const_iterator{this, size()};
}

inline size_t NodeVector::size() const {
    size_t ret = 0;
    Detail::handle_error(clingo_ast_attribute_size_ast_array(ast_.to_c(), attr_, &ret));
    return ret;
}

inline bool NodeVector::empty() const {
    return size() == 0;
}

inline NodeVector::iterator NodeVector::insert(iterator it, Node const &ast) {
    Detail::handle_error(clingo_ast_attribute_insert_ast_at(ast_.to_c(), attr_, it - begin(), ast.to_c()));
    return it;
}

inline NodeVector::iterator NodeVector::erase(iterator it) {
    Detail::handle_error(clingo_ast_attribute_delete_ast_at(ast_.to_c(), attr_, it - begin()));
    return it;
}

inline void NodeVector::set(iterator it, Node const &ast) {
    Detail::handle_error(clingo_ast_attribute_set_ast_at(ast_.to_c(), attr_, it - begin(), ast.to_c()));
}

inline NodeRef NodeVector::operator[](size_t idx) {
    return NodeRef{this, idx};
}

inline ValuePointer<NodeRef> NodeVector::at(size_t idx) {
    return operator[](idx);
}

inline Node NodeVector::operator[](size_t idx) const {
    clingo_ast_t *ret = nullptr;
    Detail::handle_error(clingo_ast_attribute_get_ast_at(ast_.to_c(), attr_, idx, &ret));
    return Node{ret};
}

inline ValuePointer<Node> NodeVector::at(size_t idx) const {
    return operator[](idx);
}

inline void NodeVector::push_back(Node const &ast) {
    insert(end(), ast);
}

inline void NodeVector::pop_back() {
    erase(end() - 1);
}

inline void NodeVector::clear() {
    for (size_t n = size(); n > 0; --n) {
        Detail::handle_error(clingo_ast_attribute_delete_ast_at(ast_.to_c(), attr_, n - 1));
    }
}

inline Node &NodeVector::ast() {
    return ast_;
}

inline Node const &NodeVector::ast() const {
    return ast_;
}

// StringRef

inline StringRef::StringRef(StringVector *vec, size_t index)
: vec_{vec}
, index_{index} { }

inline StringRef &StringRef::operator=(char const *str) {
    vec_->set(vec_->begin() + static_cast<StringVector::iterator::difference_type>(index_), str);
    return *this;
}

inline char const *StringRef::get() const {
    return static_cast<StringVector const *>(vec_)->operator[](index_);
}

inline StringRef::operator char const *() const {
    return get();
}

// StringVector

inline StringVector::StringVector(Node ast, clingo_ast_attribute_t attr)
: ast_{std::move(ast)}
, attr_{attr} { }

inline StringVector::iterator StringVector::begin() {
    return iterator{this, 0};
}

inline StringVector::iterator StringVector::end() {
    return iterator{this, size()};
}

inline StringVector::const_iterator StringVector::begin() const {
    return const_iterator{this, 0};
}

inline StringVector::const_iterator StringVector::end() const {
    return const_iterator{this, size()};
}

inline size_t StringVector::size() const {
    size_t ret = 0;
    Detail::handle_error(clingo_ast_attribute_size_string_array(ast_.to_c(), attr_, &ret));
    return ret;
}

inline bool StringVector::empty() const {
    return size() == 0;
}

inline StringVector::iterator StringVector::insert(iterator it, char const *str) {
    Detail::handle_error(clingo_ast_attribute_insert_string_at(ast_.to_c(), attr_, it - begin(), str));
    return it;
}

inline StringVector::iterator StringVector::erase(iterator it) {
    Detail::handle_error(clingo_ast_attribute_delete_string_at(ast_.to_c(), attr_, it - begin()));
    return it;
}

inline StringRef StringVector::operator[](size_t idx)  {
    return {this, idx};
}

inline ValuePointer<StringRef> StringVector::at(size_t idx)  {
    return operator[](idx);
}

inline char const *StringVector::operator[](size_t idx) const {
    char const *ret = nullptr;
    Detail::handle_error(clingo_ast_attribute_get_string_at(ast_.to_c(), attr_, idx, &ret));
    return ret;
}

inline ValuePointer<char const *> StringVector::at(size_t idx) const {
    return operator[](idx);
}

inline void StringVector::push_back(char const *str) {
    insert(end(), str);
}

inline void StringVector::pop_back() {
    erase(end() - 1);
}

inline void StringVector::clear() {
    for (size_t n = size(); n > 0; --n) {
        Detail::handle_error(clingo_ast_attribute_delete_string_at(ast_.to_c(), attr_, n - 1));
    }
}

inline Node &StringVector::ast() {
    return ast_;
}

inline Node const &StringVector::ast() const {
    return ast_;
}

// ProgramBuilder

inline ProgramBuilder::ProgramBuilder(Control &ctl)
: builder_{nullptr} {
    Detail::handle_error(clingo_program_builder_init(ctl.to_c(), &builder_));
    Detail::handle_error(clingo_program_builder_begin(builder_));
}

inline ProgramBuilder::ProgramBuilder(clingo_program_builder_t *builder)
: builder_{builder} {
    Detail::handle_error(clingo_program_builder_begin(builder_));
}

inline ProgramBuilder::ProgramBuilder(ProgramBuilder &&builder) noexcept
: builder_{nullptr} {
    std::swap(builder_, builder.builder_);
}

inline void ProgramBuilder::close() {
    if (builder_ != nullptr) {
        Detail::handle_error(clingo_program_builder_end(builder_));
        builder_ = nullptr;
    }
}

inline ProgramBuilder::~ProgramBuilder() noexcept(false) {
    close();
}

inline void ProgramBuilder::add(Node const &ast) {
    Detail::handle_error(clingo_program_builder_add(builder_, ast.to_c()));
}

// functions

namespace ASTDetail {

template <class Callback>
inline void parse_string(char const *program, Callback &&cb, clingo_control_t *control, Logger logger, unsigned message_limit) {
    using Data = std::pair<Callback, std::exception_ptr>;
    auto data = Data(std::forward<Callback>(cb), nullptr);
    Detail::handle_error(clingo_ast_parse_string(program, [](clingo_ast_t *ast, void *data) -> bool {
        auto &d = *static_cast<Data*>(data);
        clingo_ast_acquire(ast);
        CLINGO_CALLBACK_TRY { d.first(Node{ast}); }
        CLINGO_CALLBACK_CATCH(d.second);
    }, &data, nullptr, [](clingo_warning_t code, char const *msg, void *data) {
        try { (*static_cast<Logger*>(data))(static_cast<WarningCode>(code), msg); }
        catch (...) { } // NOLINT
    }, &logger, message_limit), data.second);
}

template <class Callback>
inline void parse_files(StringSpan files, Callback &&cb, clingo_control_t *control, Logger logger, unsigned message_limit) {
    using Data = std::pair<Callback, std::exception_ptr>;
    auto data = Data(std::forward<Callback>(cb), nullptr);
    Detail::handle_error(clingo_ast_parse_files(files.begin(), files.size(), [](clingo_ast_t *ast, void *data) -> bool {
        auto &d = *static_cast<Data*>(data);
        clingo_ast_acquire(ast);
        CLINGO_CALLBACK_TRY { d.first(Node{ast}); }
        CLINGO_CALLBACK_CATCH(d.second);
    }, &data, nullptr, [](clingo_warning_t code, char const *msg, void *data) {
        try { (*static_cast<Logger*>(data))(static_cast<WarningCode>(code), msg); }
        catch (...) { } // NOLINT
    }, &logger, message_limit), data.second);
}

} // namespace

template <class Callback>
inline void parse_string(char const *program, Callback &&cb, Logger logger, unsigned message_limit) {
    ASTDetail::parse_string(program, std::forward<Callback>(cb), nullptr, logger, message_limit);
}

template <class Callback>
inline void parse_string(char const *program, Callback &&cb, Control &control, Logger logger, unsigned message_limit) {
    ASTDetail::parse_string(program, std::forward<Callback>(cb), control.to_c(), logger, message_limit);
}

template <class Callback>
inline void parse_files(StringSpan files, Callback &&cb, Logger logger, unsigned message_limit) {
    ASTDetail::parse_files(files, std::forward<Callback>(cb), nullptr, logger, message_limit);
}

template <class Callback>
inline void parse_files(StringSpan files, Callback &&cb, Control &control, Logger logger, unsigned message_limit) {
    ASTDetail::parse_files(files, std::forward<Callback>(cb), control.to_c(), logger, message_limit);
}

} // namespace AST

// {{{2 control

struct Control::Impl {
    Impl() = delete;
    Impl(Impl const &) = delete;
    Impl(Impl &&) noexcept = delete;
    Impl &operator=(Impl const &) = delete;
    Impl &operator=(Impl &&) noexcept = delete;
    Impl(Logger logger)
    : ctl(nullptr)
    , handler(nullptr)
    , logger(std::move(logger))
    , owns(true) { }
    Impl(clingo_control_t *ctl, bool owns)
    : ctl(ctl)
    , handler(nullptr)
    , owns(owns) { }
    ~Impl() {
        if (ctl != nullptr && owns) { clingo_control_free(ctl); }
    }
    operator clingo_control_t *() const { return ctl; }
    clingo_control_t *ctl;
    SolveEventHandler *handler;
    Detail::AssignOnce ptr;
    Logger logger;
    std::forward_list<std::pair<Propagator&, Detail::AssignOnce&>> propagators_;
    std::forward_list<std::pair<GroundProgramObserver&, Detail::AssignOnce&>> observers_;
    bool owns;
};

inline Clingo::Control::Control(StringSpan args, Logger logger, unsigned message_limit)
: impl_(new Clingo::Control::Impl(std::move(logger)))
{
    clingo_logger_t f = [](clingo_warning_t code, char const *msg, void *data) {
        try { (*static_cast<Logger*>(data))(static_cast<WarningCode>(code), msg); }
        catch (...) { } // NOLINT(bugprone-empty-catch)
    };
    Detail::handle_error(clingo_control_new(args.begin(), args.size(), impl_->logger ? f : nullptr, impl_->logger ? &impl_->logger : nullptr, message_limit, &impl_->ctl));
}

inline Control::Control(clingo_control_t *ctl, bool owns)
    : impl_(new Impl(ctl, owns)) { }

inline Control::Control(Control &&c) noexcept
: impl_(nullptr) {
    *this = std::move(c);
}

inline Control &Control::operator=(Control &&c) noexcept {
    std::swap(impl_, c.impl_);
    return *this;
}

inline Control::~Control() noexcept {
    delete impl_;
}

inline void Control::add(char const *name, StringSpan params, char const *part) {
    Detail::handle_error(clingo_control_add(*impl_, name, params.begin(), params.size(), part));
}

inline void Control::ground(PartSpan parts, GroundCallback cb) {
    using Data = std::pair<GroundCallback&, Detail::AssignOnce&>;
    Data data(cb, impl_->ptr);
    impl_->ptr.reset();
    clingo_ground_callback_t ccb = [](clingo_location_t const *loc, char const *name, clingo_symbol_t const *args, size_t n, void *data, clingo_symbol_callback_t cb, void *cbdata) -> bool {
        auto &d = *static_cast<Data*>(data);
        CLINGO_CALLBACK_TRY {
            if (d.first) {
                struct Ret : std::exception { };
                try {
                    d.first(Location(*loc), name, {Detail::cast<Symbol const *>(args), n}, [cb, cbdata](SymbolSpan symret) {
                        if (!cb(Detail::cast<clingo_symbol_t const *>(symret.begin()), symret.size(), cbdata)) { throw Ret(); }
                    });
                }
                catch (Ret const &e) { return false; }
            }
        }
        CLINGO_CALLBACK_CATCH(d.second);
    };
    Detail::handle_error(clingo_control_ground(*impl_, Detail::cast<clingo_part_t const *>(parts.begin()), parts.size(), cb ? ccb : nullptr, &data), data.second);
}

inline clingo_control_t *Control::to_c() const { return *impl_; }

inline SolveHandle Control::solve(SymbolicLiteralSpan assumptions, SolveEventHandler *handler, bool asynchronous, bool yield) {
    std::vector<literal_t> lits;
    auto atoms = symbolic_atoms();
    for (auto const &x : assumptions) {
        auto it = atoms.find(x.symbol());
        if (it != atoms.end()) {
            auto lit = it->literal();
            lits.emplace_back(x.is_positive() ? lit : -lit);
        }
        else if (x.is_positive()) {
            lits.emplace_back(1);
            lits.emplace_back(-1);
        }
    }
    return solve(LiteralSpan{lits}, handler, asynchronous, yield);
}

inline SolveHandle Control::solve(LiteralSpan assumptions, SolveEventHandler *handler, bool asynchronous, bool yield) {
    clingo_solve_handle_t *it = nullptr;
    clingo_solve_mode_bitset_t mode = 0;
    if (asynchronous) { mode |= clingo_solve_mode_async; }
    if (yield) { mode |= clingo_solve_mode_yield; }
    impl_->handler = handler;
    impl_->ptr.reset();
    clingo_solve_event_callback_t on_event = [](clingo_solve_event_type_t type, void *event, void *pdata, bool *goon) {
        Impl &data = *static_cast<Impl*>(pdata);
        switch (static_cast<clingo_solve_event_type_e>(type)) {
            case clingo_solve_event_type_model: {
                CLINGO_CALLBACK_TRY {
                    Model m{static_cast<clingo_model_t*>(event)};
                    *goon = data.handler->on_model(m);
                }
                CLINGO_CALLBACK_CATCH(data.ptr);
            }
            case clingo_solve_event_type_unsat: {
                CLINGO_CALLBACK_TRY {
                    auto *span = static_cast<std::pair<int64_t const *, size_t>*>(event);
                    data.handler->on_unsat(make_span(span->first, span->second));
                    *goon = true;
                }
                CLINGO_CALLBACK_CATCH(data.ptr);
            }
            case clingo_solve_event_type_statistics: {
                CLINGO_CALLBACK_TRY {
                    auto **stats = static_cast<clingo_statistics_t**>(event);
                    uint64_t step_root = 0;
                    uint64_t accu_root = 0;
                    Detail::handle_error(clingo_statistics_root(stats[0], &step_root)); // NOLINT
                    Detail::handle_error(clingo_statistics_root(stats[1], &accu_root)); // NOLINT
                    data.handler->on_statistics(UserStatistics{stats[0], step_root}, UserStatistics{stats[1], accu_root}); // NOLINT
                    *goon = true;
                    return true;
                }
                CLINGO_CALLBACK_CATCH(data.ptr);
            }
            case clingo_solve_event_type_finish: {
                CLINGO_CALLBACK_TRY {
                    data.handler->on_finish(SolveResult{*static_cast<clingo_solve_result_bitset_t*>(event)});
                    *goon = true;
                    return true;
                }
                CLINGO_CALLBACK_CATCH(data.ptr);
            }
        }
        return false;
    };
    Detail::handle_error(clingo_control_solve(*impl_, mode, assumptions.begin(), assumptions.size(), handler != nullptr ? on_event : nullptr, impl_, &it), impl_->ptr);
    return SolveHandle{it, impl_->ptr};
}

inline void Control::assign_external(literal_t literal, TruthValue value) {
    Detail::handle_error(clingo_control_assign_external(*impl_, literal, static_cast<clingo_truth_value_t>(value)));
}

inline void Control::assign_external(Symbol atom, TruthValue value) {
    auto atoms = symbolic_atoms();
    auto it = atoms.find(atom);
    if (it != atoms.end()) { assign_external(it->literal(), value); }
}

inline void Control::release_external(literal_t literal) {
    Detail::handle_error(clingo_control_release_external(*impl_, literal));
}

inline void Control::release_external(Symbol atom) {
    auto atoms = symbolic_atoms();
    auto it = atoms.find(atom);
    if (it != atoms.end()) { release_external(it->literal()); }
}

inline SymbolicAtoms Control::symbolic_atoms() const {
    clingo_symbolic_atoms_t const *ret = nullptr;
    Detail::handle_error(clingo_control_symbolic_atoms(*impl_, &ret));
    return SymbolicAtoms{ret};
}

inline TheoryAtoms Control::theory_atoms() const {
    clingo_theory_atoms_t const *ret = nullptr;
    clingo_control_theory_atoms(*impl_, &ret);
    return TheoryAtoms{ret};
}

namespace Detail {

using PropagatorData = std::pair<Propagator&, AssignOnce&>;

inline static bool g_init(clingo_propagate_init_t *ctl, void *pdata) {
    PropagatorData &data = *static_cast<PropagatorData*>(pdata);
    CLINGO_CALLBACK_TRY {
        PropagateInit pi(ctl);
        data.first.init(pi);
    }
    CLINGO_CALLBACK_CATCH(data.second);
}

inline static bool g_propagate(clingo_propagate_control_t *ctl, clingo_literal_t const *changes, size_t n, void *pdata) {
    PropagatorData &data = *static_cast<PropagatorData*>(pdata);
    CLINGO_CALLBACK_TRY {
        PropagateControl pc(ctl);
        data.first.propagate(pc, {changes, n});
    }
    CLINGO_CALLBACK_CATCH(data.second);
}

inline static void g_undo(clingo_propagate_control_t const *ctl, clingo_literal_t const *changes, size_t n, void *pdata) {
    PropagatorData &data = *static_cast<PropagatorData*>(pdata);
    PropagateControl pc(const_cast<clingo_propagate_control_t*>(ctl)); // NOLINT
    data.first.undo(pc, {changes, n});
}

inline static bool g_check(clingo_propagate_control_t *ctl, void *pdata) {
    PropagatorData &data = *static_cast<PropagatorData*>(pdata);
    CLINGO_CALLBACK_TRY {
        PropagateControl pc(ctl);
        data.first.check(pc);
    }
    CLINGO_CALLBACK_CATCH(data.second);
}

inline static bool g_decide(clingo_id_t ti, clingo_assignment_t const *a,  clingo_literal_t f, void *pdata, clingo_literal_t *l) {
    PropagatorData &data = *static_cast<PropagatorData*>(pdata);
    CLINGO_CALLBACK_TRY {
        Assignment ass{a};
        *l = static_cast<Heuristic&>(data.first).decide(ti, ass, f); // NOLINT
    }
    CLINGO_CALLBACK_CATCH(data.second);
}

} // namespace Detail

inline void Control::register_propagator(Propagator &propagator, bool sequential) {
    impl_->propagators_.emplace_front(propagator, impl_->ptr);
    static clingo_propagator_t g_propagator = {
        Detail::g_init,
        Detail::g_propagate,
        Detail::g_undo,
        Detail::g_check,
        nullptr,
    };
    Detail::handle_error(clingo_control_register_propagator(*impl_, &g_propagator, &impl_->propagators_.front(), sequential));
}

inline void Control::register_propagator(Heuristic &propagator, bool sequential) {
    impl_->propagators_.emplace_front(propagator, impl_->ptr);
    static clingo_propagator_t g_propagator = {
        Detail::g_init,
        Detail::g_propagate,
        Detail::g_undo,
        Detail::g_check,
        Detail::g_decide,
    };
    Detail::handle_error(clingo_control_register_propagator(*impl_, &g_propagator, &impl_->propagators_.front(), sequential));
}

namespace Detail {

using ObserverData = std::pair<GroundProgramObserver&, AssignOnce&>;

inline bool g_init_program(bool incremental, void *pdata) {
    ObserverData &data = *static_cast<ObserverData*>(pdata);
    CLINGO_CALLBACK_TRY { data.first.init_program(incremental); }
    CLINGO_CALLBACK_CATCH(data.second);
}
inline bool g_begin_step(void *pdata) {
    ObserverData &data = *static_cast<ObserverData*>(pdata);
    CLINGO_CALLBACK_TRY { data.first.begin_step(); }
    CLINGO_CALLBACK_CATCH(data.second);
}
inline bool g_end_step(void *pdata) {
    ObserverData &data = *static_cast<ObserverData*>(pdata);
    CLINGO_CALLBACK_TRY { data.first.end_step(); }
    CLINGO_CALLBACK_CATCH(data.second);
}

inline bool g_rule(bool choice, clingo_atom_t const *head, size_t head_size, clingo_literal_t const *body, size_t body_size, void *pdata) {
    ObserverData &data = *static_cast<ObserverData*>(pdata);
    CLINGO_CALLBACK_TRY { data.first.rule(choice, AtomSpan(head, head_size), LiteralSpan(body, body_size)); }
    CLINGO_CALLBACK_CATCH(data.second);
}

inline bool g_weight_rule(bool choice, clingo_atom_t const *head, size_t head_size, clingo_weight_t lower_bound, clingo_weighted_literal_t const *body, size_t body_size, void *pdata) {
    ObserverData &data = *static_cast<ObserverData*>(pdata);
    CLINGO_CALLBACK_TRY { data.first.weight_rule(choice, AtomSpan(head, head_size), lower_bound, WeightedLiteralSpan(Detail::cast<WeightedLiteral const*>(body), body_size)); }
    CLINGO_CALLBACK_CATCH(data.second);
}

inline bool g_minimize(clingo_weight_t priority, clingo_weighted_literal_t const* literals, size_t size, void *pdata) {
    ObserverData &data = *static_cast<ObserverData*>(pdata);
    CLINGO_CALLBACK_TRY { data.first.minimize(priority, WeightedLiteralSpan(Detail::cast<WeightedLiteral const*>(literals), size)); }
    CLINGO_CALLBACK_CATCH(data.second);
}

inline bool g_project(clingo_atom_t const *atoms, size_t size, void *pdata) {
    ObserverData &data = *static_cast<ObserverData*>(pdata);
    CLINGO_CALLBACK_TRY { data.first.project(AtomSpan(atoms, size)); }
    CLINGO_CALLBACK_CATCH(data.second);
}

inline bool g_output_atom(clingo_symbol_t symbol, clingo_atom_t atom, void *pdata) {
    ObserverData &data = *static_cast<ObserverData*>(pdata);
    CLINGO_CALLBACK_TRY { data.first.output_atom(Symbol{symbol}, atom); }
    CLINGO_CALLBACK_CATCH(data.second);
}

inline bool g_output_term(clingo_symbol_t symbol, clingo_literal_t const *condition, size_t size, void *pdata) {
    ObserverData &data = *static_cast<ObserverData*>(pdata);
    CLINGO_CALLBACK_TRY { data.first.output_term(Symbol{symbol}, LiteralSpan{condition, size}); }
    CLINGO_CALLBACK_CATCH(data.second);
}

inline bool g_external(clingo_atom_t atom, clingo_external_type_t type, void *pdata) {
    ObserverData &data = *static_cast<ObserverData*>(pdata);
    CLINGO_CALLBACK_TRY { data.first.external(atom, static_cast<ExternalType>(type)); }
    CLINGO_CALLBACK_CATCH(data.second);
}

inline bool g_assume(clingo_literal_t const *literals, size_t size, void *pdata) {
    ObserverData &data = *static_cast<ObserverData*>(pdata);
    CLINGO_CALLBACK_TRY { data.first.assume(LiteralSpan(literals, size)); }
    CLINGO_CALLBACK_CATCH(data.second);
}

inline bool g_heuristic(clingo_atom_t atom, clingo_heuristic_type_t type, int bias, unsigned priority, clingo_literal_t const *condition, size_t size, void *pdata) {
    ObserverData &data = *static_cast<ObserverData*>(pdata);
    CLINGO_CALLBACK_TRY { data.first.heuristic(atom, static_cast<HeuristicType>(type), bias, priority, LiteralSpan(condition, size)); }
    CLINGO_CALLBACK_CATCH(data.second);
}

inline bool g_acyc_edge(int node_u, int node_v, clingo_literal_t const *condition, size_t size, void *pdata) {
    ObserverData &data = *static_cast<ObserverData*>(pdata);
    CLINGO_CALLBACK_TRY { data.first.acyc_edge(node_u, node_v, LiteralSpan(condition, size)); }
    CLINGO_CALLBACK_CATCH(data.second);
}

inline bool g_theory_term_number(clingo_id_t term_id, int number, void *pdata) {
    ObserverData &data = *static_cast<ObserverData*>(pdata);
    CLINGO_CALLBACK_TRY { data.first.theory_term_number(term_id, number); }
    CLINGO_CALLBACK_CATCH(data.second);
}

inline bool g_theory_term_string(clingo_id_t term_id, char const *name, void *pdata) {
    ObserverData &data = *static_cast<ObserverData*>(pdata);
    CLINGO_CALLBACK_TRY { data.first.theory_term_string(term_id, name); }
    CLINGO_CALLBACK_CATCH(data.second);
}

inline bool g_theory_term_compound(clingo_id_t term_id, int name_id_or_type, clingo_id_t const *arguments, size_t size, void *pdata) {
    ObserverData &data = *static_cast<ObserverData*>(pdata);
    CLINGO_CALLBACK_TRY { data.first.theory_term_compound(term_id, name_id_or_type, IdSpan(arguments, size)); }
    CLINGO_CALLBACK_CATCH(data.second);
}

inline bool g_theory_element(clingo_id_t element_id, clingo_id_t const *terms, size_t terms_size, clingo_literal_t const *condition, size_t condition_size, void *pdata) {
    ObserverData &data = *static_cast<ObserverData*>(pdata);
    CLINGO_CALLBACK_TRY { data.first.theory_element(element_id, IdSpan(terms, terms_size), LiteralSpan(condition, condition_size)); }
    CLINGO_CALLBACK_CATCH(data.second);
}

inline bool g_theory_atom(clingo_id_t atom_id_or_zero, clingo_id_t term_id, clingo_id_t const *elements, size_t size, void *pdata) {
    ObserverData &data = *static_cast<ObserverData*>(pdata);
    CLINGO_CALLBACK_TRY { data.first.theory_atom(atom_id_or_zero, term_id, IdSpan(elements, size)); }
    CLINGO_CALLBACK_CATCH(data.second);
}

inline bool g_theory_atom_with_guard(clingo_id_t atom_id_or_zero, clingo_id_t term_id, clingo_id_t const *elements, size_t size, clingo_id_t operator_id, clingo_id_t right_hand_side_id, void *pdata) {
    ObserverData &data = *static_cast<ObserverData*>(pdata);
    CLINGO_CALLBACK_TRY { data.first.theory_atom_with_guard(atom_id_or_zero, term_id, IdSpan(elements, size), operator_id, right_hand_side_id); }
    CLINGO_CALLBACK_CATCH(data.second);
}

} // namespace Detail

inline void Control::register_observer(GroundProgramObserver &observer, bool replace) {
    impl_->observers_.emplace_front(observer, impl_->ptr);
    static clingo_ground_program_observer_t g_observer = {
        Detail::g_init_program,
        Detail::g_begin_step,
        Detail::g_end_step,
        Detail::g_rule,
        Detail::g_weight_rule,
        Detail::g_minimize,
        Detail::g_project,
        Detail::g_output_atom,
        Detail::g_output_term,
        Detail::g_external,
        Detail::g_assume,
        Detail::g_heuristic,
        Detail::g_acyc_edge,
        Detail::g_theory_term_number,
        Detail::g_theory_term_string,
        Detail::g_theory_term_compound,
        Detail::g_theory_element,
        Detail::g_theory_atom,
        Detail::g_theory_atom_with_guard
    };
    Detail::handle_error(clingo_control_register_observer(*impl_, &g_observer, replace, &impl_->observers_.front()));
}

inline bool Control::is_conflicting() const noexcept {
    return clingo_control_is_conflicting(*impl_);
}

inline bool Control::has_const(char const *name) const {
    bool ret = false;
    Detail::handle_error(clingo_control_has_const(*impl_, name, &ret));
    return ret;
}

inline Symbol Control::get_const(char const *name) const {
    clingo_symbol_t ret = 0;
    Detail::handle_error(clingo_control_get_const(*impl_, name, &ret));
    return Symbol(ret);
}

inline void Control::interrupt() noexcept {
    clingo_control_interrupt(*impl_);
}

inline void *Control::claspFacade() {
    void *ret = nullptr;
    Detail::handle_error(clingo_control_clasp_facade(impl_->ctl, &ret));
    return ret;
}

inline void Control::load(char const *file) {
    Detail::handle_error(clingo_control_load(*impl_, file));
}

inline void Control::enable_enumeration_assumption(bool value) {
    Detail::handle_error(clingo_control_set_enable_enumeration_assumption(*impl_, value));
}

inline bool Control::enable_enumeration_assumption() const {
    return clingo_control_get_enable_enumeration_assumption(*impl_);
}

inline void Control::cleanup() {
    Detail::handle_error(clingo_control_cleanup(*impl_));
}

inline void Control::enable_cleanup(bool value) {
    Detail::handle_error(clingo_control_set_enable_cleanup(*impl_, value));
}

inline bool Control::enable_cleanup() const {
    return clingo_control_get_enable_cleanup(*impl_);
}

inline Backend Control::backend() {
    clingo_backend_t *ret = nullptr;
    Detail::handle_error(clingo_control_backend(*impl_, &ret));
    return Backend{ret};
}

inline Configuration Control::configuration() {
    clingo_configuration_t *conf = nullptr;
    Detail::handle_error(clingo_control_configuration(*impl_, &conf));
    unsigned key = 0;
    Detail::handle_error(clingo_configuration_root(conf, &key));
    return Configuration{conf, key};
}

inline Statistics Control::statistics() const {
    clingo_statistics_t const *stats = nullptr;
    Detail::handle_error(clingo_control_statistics(impl_->ctl, &stats), impl_->ptr);
    uint64_t key = 0;
    Detail::handle_error(clingo_statistics_root(stats, &key));
    return Statistics{stats, key};
}

// {{{2 clingo application

inline void ClingoOptions::add(char const *group, char const *option, char const *description, std::function<bool (char const *value)> parse, bool multi, char const *argument) {
    parsers_->emplace_front(std::move(parse));
    Detail::handle_error(clingo_options_add(to_c(), group, option, description, [](char const *value, void *data) {
        auto& p = *static_cast<Detail::ParserList::value_type*>(data);
        try         { return p(value); }
        catch (...) { return false; }
    }, &parsers_->front(), multi, argument));
}

inline void ClingoOptions::add_flag(char const *group, char const *option, char const *description, bool &target) { // NOLINT
    Detail::handle_error(clingo_options_add_flag(to_c(), group, option, description, &target));
}

inline unsigned Application::message_limit() const noexcept {
    return g_message_limit;
}
inline char const *Application::program_name() const noexcept {
    return "clingo";
}
inline char const *Application::version() const noexcept {
    return CLINGO_VERSION;
}
inline void Application::print_model(Model const &model, std::function<void()> default_printer) noexcept { // NOLINT
    static_cast<void>(model);
    default_printer();
}
inline void Application::log(WarningCode code, char const *message) noexcept {
    static_cast<void>(code);
    fprintf(stderr, "%s\n", message); // NOLINT
    fflush(stderr);
}
inline void Application::register_options(ClingoOptions &options) {
    static_cast<void>(options);
}
inline void Application::validate_options() {
}

namespace Detail {

struct ApplicationData {
    Application *app;
    ParserList parsers;
};

inline static unsigned g_message_limit(void *adata) {
    ApplicationData &data = *static_cast<ApplicationData*>(adata);
    return data.app->message_limit();
}

inline static char const *g_program_name(void *adata) {
    ApplicationData &data = *static_cast<ApplicationData*>(adata);
    return data.app->program_name();
}

inline static char const *g_version(void *adata) {
    ApplicationData &data = *static_cast<ApplicationData*>(adata);
    return data.app->version();
}

inline static bool g_main(clingo_control_t *control, char const *const * files, size_t size, void *adata) {
    ApplicationData &data = *static_cast<ApplicationData*>(adata);
    CLINGO_TRY {
        Control ctl{control, false};
        data.app->main(ctl, {files, size});
    }
    CLINGO_CATCH;
}

inline static void g_logger(clingo_warning_t code, char const *message, void *adata) {
    ApplicationData &data = *static_cast<ApplicationData*>(adata);
    return data.app->log(static_cast<WarningCode>(code), message);
}

inline static bool g_model_printer(clingo_model_t const *model, clingo_default_model_printer_t printer, void *printer_data, void *data) {
    ApplicationData &app_data = *static_cast<ApplicationData*>(data);
    CLINGO_TRY {
        app_data.app->print_model(Model(const_cast<clingo_model_t*>(model)), [&]() { // NOLINT
            Detail::handle_error(printer(printer_data));
        });
    }
    CLINGO_CATCH;
}

inline static bool g_register_options(clingo_options_t *options, void *adata) {
    ApplicationData &data = *static_cast<ApplicationData*>(adata);
    CLINGO_TRY {
        ClingoOptions opts{options, data.parsers};
        data.app->register_options(opts);
    }
    CLINGO_CATCH;
}

inline static bool g_validate_options(void *adata) {
    ApplicationData &data = *static_cast<ApplicationData*>(adata);
    CLINGO_TRY { data.app->validate_options(); }
    CLINGO_CATCH;
}

} // namespace

// {{{2 global functions

inline Symbol parse_term(char const *str, Logger logger, unsigned message_limit) {
    clingo_symbol_t ret = 0;
    Detail::handle_error(clingo_parse_term(str, [](clingo_warning_t code, char const *msg, void *data) {
        try { (*static_cast<Logger*>(data))(static_cast<WarningCode>(code), msg); }
        catch (...) { } // NOLINT(bugprone-empty-catch)
    }, &logger, message_limit, &ret));
    return Symbol(ret);
}

inline char const *add_string(char const *str) {
    char const *ret = nullptr;
    Detail::handle_error(clingo_add_string(str, &ret));
    return ret;
}

inline std::tuple<int, int, int> version() {
    std::tuple<int, int, int> ret;
    clingo_version(&std::get<0>(ret), &std::get<1>(ret), &std::get<2>(ret));
    return ret;
}

inline int clingo_main(Application &application, StringSpan arguments) {
    Detail::ApplicationData data{&application, Detail::ParserList{}};
    static clingo_application_t g_app = {
        Detail::g_program_name,
        Detail::g_version,
        Detail::g_message_limit,
        Detail::g_main,
        Detail::g_logger,
        Detail::g_model_printer,
        Detail::g_register_options,
        Detail::g_validate_options
    };
    return ::clingo_main(&g_app, arguments.begin(), arguments.size(), &data);
}

// }}}2

} // namespace Clingo

#undef CLINGO_CALLBACK_TRY
#undef CLINGO_CALLBACK_CATCH
#undef CLINGO_TRY
#undef CLINGO_CATCH

// }}}1

#endif
