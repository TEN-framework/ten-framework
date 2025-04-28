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

#include <algorithm>
#include <cstring>
#include <gringo/symbol.hh>
#include <gringo/hash_set.hh>
#include <iterator>
#include <mutex>

#ifdef _MSC_VER
#pragma warning (disable : 4200) // nonstandard extension used: zero-sized array in struct/union
#endif

namespace Gringo {

namespace {

// {{{1 auxiliary functions

constexpr const uint16_t upperMax = std::numeric_limits<uint16_t>::max();
constexpr const uint16_t lowerMax = 3;

uint16_t upper(uint64_t rep) {
    return rep >> 48;
}

uint8_t lower(uint64_t rep) {
    return rep & 3;
}

uintptr_t ptr(uint64_t rep) {
    return static_cast<uintptr_t>(rep & 0xFFFFFFFFFFFC);
}

uint64_t combine(uint16_t u, uintptr_t ptr, uint8_t l) {
    static_cast<void>(lowerMax);
    assert(l <= lowerMax);
    return static_cast<uint64_t>(u) << 48 | ptr | l;
}

uint64_t combine(uint16_t u, int32_t num) {
    return static_cast<uint64_t>(u) << 48 | static_cast<uint64_t>(static_cast<uint32_t>(num));
}

uint64_t setUpper(uint16_t u, uint64_t rep) {
    return combine(u, 0, 0) | (rep & 0xFFFFFFFFFFFF);
}

//uint64_t setLower(uint8_t l, uint8_t rep) {
//    assert(l <= lowerMax);
//    return combine(0, 0, l) | (rep & 0xFFFFFFFFFFFFFFFC);
//}

enum class SymbolType_ : uint8_t {
    Inf = 0,
    Num = 1,
    IdP = 2,
    IdN = 3,
    Str = 4,
    Fun = 5,
    Special = 6,
    Sup = 7
};

SymbolType_ symbolType_(uint64_t rep) {
    return static_cast<SymbolType_>(upper(rep));
}

template <class T>
T const *cast(uint64_t rep) {
    return reinterpret_cast<T const *>(ptr(rep)); // NOLINT
}

String toString(uint64_t rep) {
    return String::fromRep(ptr(rep));
}

// {{{1 definition of Unique

template <class T>
struct UniqueConstruct {
public:
    using Set = hash_set<T, typename T::Hash, typename T::EqualTo>;

    template <class U>
    static T const &construct(U &&x) {
        // TODO: in C++17 this can use a read/write lock to not block reading threads
        size_t hash = typename T::Hash{}(x);
        std::lock_guard<std::mutex> g(mutex_);
        auto it = set_.find(x, hash);
        if (it != set_.end()) {
            return *it;
        }
        return *set_.insert(T{std::forward<U>(x), hash}).first;
    }

private:
    static Set set_; // NOLINT
    static std::mutex mutex_; // NOLINT
};

template <class T>
typename UniqueConstruct<T>::Set UniqueConstruct<T>::set_; // NOLINT

template <class T>
typename std::mutex UniqueConstruct<T>::mutex_; // NOLINT

template <class T, class U>
T const &construct_unique(U &&x) {
    return UniqueConstruct<T>::construct(std::forward<U>(x));
}

// {{{1 definition of USig

class MSig {
public:
    using Cons = std::pair<String, uint32_t>;

    struct Hash {
        size_t operator()(MSig const &sig) const {
            return sig.hash_;
        }
        size_t operator()(Cons const &sig) const {
            return hash_mix(get_value_hash(sig));
        }
    };
    struct EqualTo {
        using is_transparent = void;
        template <class A, class B>
        size_t operator()(A const &a, B const &b) const {
            return name(a) == name(b) && arity(a) == arity(b);
        }
    };

    explicit MSig(Cons const &cons, size_t hash)
    : sig_{cons.first, cons.second}
    , hash_{hash} {}

    Cons const &as_sig() const {
        return sig_;
    }

private:
    static String name(MSig const &a) {
        return a.sig_.first;
    }

    static String name(Cons const &a) {
        return a.first;
    }

    static uint32_t arity(MSig const &a) {
        return a.sig_.second;
    }

    static uint32_t arity(Cons const &a) {
        return a.second;
    }

    std::pair<String, uint32_t> sig_;
    size_t hash_;
};

uint64_t encodeSig(String name, uint32_t arity, bool sign) {
    uint8_t isign = sign ? 1 : 0;
    return arity < upperMax
        ? combine(arity, String::toRep(name), isign)
        : combine(upperMax,
                  reinterpret_cast<uintptr_t>(&construct_unique<MSig>(std::make_pair(name, arity)).as_sig()), // NOLINT
                  isign);
}

// {{{1 definition of Fun

class Fun {
public:
    Fun(Fun const &other) = delete;
    Fun(Fun &&other) noexcept = delete;
    Fun &operator=(Fun const &other) = delete;
    Fun &operator=(Fun &&other) noexcept = delete;

    static Fun *make(Sig sig, SymSpan args, size_t hash) {
        auto *mem = ::operator new(sizeof(Fun) + args.size * sizeof(Symbol));
        return new(mem) Fun(sig, args, hash); // NOLINT
    }

    void destroy() noexcept {
        this->~Fun();
        ::operator delete(this);
    }

    Sig sig() const {
        return sig_;
    }

    SymSpan args() const {
        return {args_, sig().arity()};
    }

    size_t hash() const {
        return hash_;
    }

private:
    ~Fun() noexcept = default;

    Fun(Sig sig, SymSpan args, size_t hash) noexcept
    : sig_(sig)
    , hash_{hash} {
        std::memcpy(static_cast<void*>(args_), args.first, args.size * sizeof(Symbol));
    }

    Sig const sig_;
    size_t hash_;
    Symbol args_[0]; // NOLINT
};

class MFun {
public:
    using Cons = std::pair<Sig, SymSpan>;

    struct Hash {
        size_t operator()(MFun const &a) const {
            return a.fun_->hash();
        }
        size_t operator()(Cons const &a) const {
            return hash_mix(get_value_hash(a.first, hash_range(begin(a.second), end(a.second))));
        }
    };
    struct EqualTo {
        using is_transparent = void;
        template <class A, class B>
        size_t operator()(A const &a, B const &b) const {
            auto args_a = args(a);
            auto args_b = args(b);
            return sig(a) == sig(b) && std::equal(begin(args_a), end(args_a), begin(args_b));
        }
    };

    explicit MFun(Cons fun, size_t hash)
    : fun_{Fun::make(fun.first, fun.second, hash)} { }
    MFun() = delete;
    MFun(MFun const &other) = delete;
    MFun(MFun &&other) noexcept {
        std::swap(fun_, other.fun_);
    }
    MFun &operator=(MFun const &other) = delete;
    MFun &operator=(MFun &&other) noexcept {
        std::swap(fun_, other.fun_);
        return *this;
    }
    ~MFun() noexcept {
        if (fun_ != nullptr) {
            fun_->destroy();
        }
    }

    Fun const &as_fun() const {
        return *fun_;
    }

private:
    static Sig sig(MFun const &a) {
        return a.fun_->sig();
    }

    static Sig sig(Cons const &a) {
        return a.first;
    }

    static SymSpan args(MFun const &a) {
        return a.fun_->args();
    }

    static SymSpan args(Cons const &a) {
        return a.second;
    }

    Fun *fun_ = nullptr;
};

// }}}1

} // namespace

// {{{1 definition of String

// {{{1 definition of MString

class String::Impl {
public:
    class MString;

    Impl(Impl const &other) = delete;
    Impl(Impl &&other) noexcept = delete;
    Impl &operator=(Impl const &other) = delete;
    Impl &operator=(Impl &&other) noexcept = delete;

    static Impl *make(StringSpan const &span, size_t hash) {
        size_t n = span.size;
        auto *mem = ::operator new(sizeof(Impl) + (n + 1) * sizeof(char));
        return new(mem) Impl(span.first, n, hash); // NOLINT
    }

    static Impl *make(char const *str, size_t hash) {
        size_t n = strlen(str);
        auto *mem = ::operator new(sizeof(Impl) + (n + 1) * sizeof(char));
        return new(mem) Impl(str, n, hash); // NOLINT
    }

    void destroy() noexcept {
        this->~Impl();
        ::operator delete(this);
    }

    char const *str() const {
        return str_;
    }

    size_t hash() const {
        return hash_;
    }

private:
    Impl(char const *str, size_t n, size_t hash) noexcept
    : hash_{hash} {
        std::memcpy(str_, str, n * sizeof(char));
        str_[n] = '\0'; // NOLINT
    }
    ~Impl() noexcept = default;

    size_t hash_;
    char str_[0]; // NOLINT
};

class String::Impl::MString {
public:
    struct Hash {
        size_t operator()(MString const &str) const {
            return str.str_->hash();
        }
        size_t operator()(char const *str) const {
            return hash_mix(strhash(str));
        }
        size_t operator()(StringSpan const &str) const {
            return hash_mix(strhash(str));
        }
    };
    struct EqualTo {
        using is_transparent = void;
        bool operator()(MString const &a, char const *b) const {
            return std::strcmp(a.as_impl()->str(), b) == 0;
        }
        bool operator()(MString const &a, StringSpan const &span_b) const {
            auto const *str_a = a.as_impl()->str();
            StringSpan span_a = {str_a, std::strlen(str_a)};
            return span_a.size == span_b.size && std::equal(begin(span_a), end(span_a), begin(span_b));
        }
        bool operator()(MString const &a, MString const &b) const {
            return operator()(a, b.as_impl()->str());
        }
        bool operator()(char const *a, MString const &b) const {
            return operator()(b, a);
        }
        bool operator()(StringSpan const &a, MString const &b) const {
            return operator()(b, a);
        }
    };

    explicit MString(char const *str, size_t hash)
    : str_{String::Impl::make(str, hash)} {
    }
    explicit MString(StringSpan str, size_t hash)
    : str_{String::Impl::make(str, hash)} {
    }
    MString() = delete;
    MString(MString const &other) = delete;
    MString(MString &&other) noexcept {
        std::swap(str_, other.str_);
    }
    MString &operator=(MString const &other) = delete;
    MString &operator=(MString &&other) noexcept {
        std::swap(str_, other.str_);
        return *this;
    }
    ~MString() noexcept {
        if (str_ != nullptr) {
            str_->destroy();
        }
    }

    Impl *as_impl() const {
        return str_;
    }

private:
    String::Impl *str_ = nullptr;
};

String::String(char const *str)
: str_(construct_unique<Impl::MString>(str).as_impl()) { }

String::String(StringSpan str)
: str_(construct_unique<Impl::MString>(str).as_impl()) { }

String::String(uintptr_t r) noexcept
: str_(reinterpret_cast<Impl*>(r)) { } // NOLINT

const char *String::c_str() const {
    return str_->str();
}

bool String::empty() const {
    return *c_str() == '\0'; // NOLINT
}

size_t String::length() const {
    return std::strlen(c_str());
}

bool String::startsWith(char const *prefix) const {
    return std::strncmp(prefix, c_str(), strlen(prefix)) == 0;
}

size_t String::hash() const {
    return reinterpret_cast<uintptr_t>(str_); // NOLINT
}

uintptr_t String::toRep(String s) noexcept {
    return reinterpret_cast<uintptr_t>(s.str_); // NOLINT
}

String String::fromRep(uintptr_t t) noexcept {
    return {t};
}

// {{{1 definition of Signature

Sig::Sig(String name, uint32_t arity, bool sign)
: Sig{encodeSig(name, arity, sign)} { }

String Sig::name() const {
    uint16_t u = upper(rep());
    return u < upperMax ? toString(rep()) : cast<std::pair<String, uint32_t>>(rep())->first;
}

Sig Sig::flipSign() const {
    return Sig(rep() ^ 1);
}

uint32_t Sig::arity() const {
    uint16_t u = upper(rep());
    return u < upperMax ? u : cast<std::pair<String, uint32_t>>(rep())->second;
}

bool Sig::sign() const {
    return lower(rep()) != 0;
}

size_t Sig::hash() const {
    return get_value_hash(rep());
}

namespace {

bool less(Sig const &a, Sig const &b) {
    if (a.sign() != b.sign()) { return !a.sign() && b.sign(); }
    if (a.arity() != b.arity()) { return a.arity() < b.arity(); }
    return a.name() < b.name();
}

} // namespace

bool Sig::operator==(Sig s) const {
    return rep() == s.rep();
}

bool Sig::operator!=(Sig s) const {
    return rep() != s.rep();
}

bool Sig::operator<(Sig s) const {
    return *this != s && less(*this, s);
}

bool Sig::operator>(Sig s) const {
    return *this != s && less(s, *this);
}

bool Sig::operator<=(Sig s) const {
    return *this == s || less(*this, s);
}

bool Sig::operator>=(Sig s) const {
    return *this == s || less(s, *this);
}

// {{{1 definition of Symbol

// {{{2 construction

Symbol::Symbol()
: Symbol(combine(static_cast<uint16_t>(SymbolType_::Special), 0, 0)) { }

Symbol Symbol::createInf() {
    return Symbol(combine(static_cast<uint16_t>(SymbolType_::Inf), 0, 0));
}

Symbol Symbol::createSup() {
    return Symbol(combine(static_cast<uint16_t>(SymbolType_::Sup), 0, 0));
}

Symbol Symbol::createNum(int num) {
    return Symbol(combine(static_cast<uint16_t>(SymbolType_::Num), num));
}

Symbol Symbol::createId(String val, bool sign) {
    return Symbol(combine(static_cast<uint16_t>(sign ? SymbolType_::IdN : SymbolType_::IdP), String::toRep(val), 0));
}

Symbol Symbol::createStr(String val) {
    return Symbol(combine(static_cast<uint16_t>(SymbolType_::Str), String::toRep(val), 0));
}

Symbol Symbol::createTuple(SymSpan args) {
    return createFun("", args, false);
}

Symbol Symbol::createFun(String name, SymSpan args, bool sign) {
    return args.size != 0
        ?  Symbol(combine(static_cast<uint16_t>(SymbolType_::Fun),
                          reinterpret_cast<uintptr_t>(&construct_unique<MFun>(std::make_pair(Sig(name, numeric_cast<uint32_t>(args.size), sign), args)).as_fun()), // NOLINT
                          0))
        : createId(name, sign);
}

// {{{2 inspection

SymbolType Symbol::type() const {
    auto t = symbolType_(rep_);
    switch (t) {
        case SymbolType_::IdP:
        case SymbolType_::IdN: { return SymbolType::Fun; }
        default:               { return static_cast<SymbolType>(t); }
    }
}

int32_t Symbol::num() const {
    assert(type() == SymbolType::Num);
    return static_cast<int32_t>(static_cast<uint32_t>(rep_));
}

String Symbol::string() const {
    assert(type() == SymbolType::Str);
    return toString(rep_);
}

Sig Symbol::sig() const{
    assert(type() == SymbolType::Fun);
    switch (symbolType_(rep_)) {
        case SymbolType_::IdP: { return {toString(rep_), 0, false}; }
        case SymbolType_::IdN: { return {toString(rep_), 0, true}; }
        default:               { return cast<Fun>(rep_)->sig(); }
    }
}

bool Symbol::hasSig() const {
    return type() == SymbolType::Fun;
}

String Symbol::name() const {
    assert(type() == SymbolType::Fun);
    switch (symbolType_(rep_)) {
        case SymbolType_::IdP:
        case SymbolType_::IdN: { return toString(rep_); }
        default:               { return cast<Fun>(rep_)->sig().name(); }
    }
}

SymSpan Symbol::args() const {
    assert(type() == SymbolType::Fun);
    switch (symbolType_(rep_)) {
        case SymbolType_::IdP:
        case SymbolType_::IdN: { return SymSpan{nullptr, 0}; }
        default:               { return cast<Fun>(rep_)->args(); }
    }
}

bool Symbol::sign() const {
    assert(type() == SymbolType::Fun || type() == SymbolType::Num);
    switch (symbolType_(rep_)) {
        case SymbolType_::Num: { return num() < 0; }
        case SymbolType_::IdP: { return false; }
        case SymbolType_::IdN: { return true; }
        default:               { return cast<Fun>(rep_)->sig().sign(); }
    }
}

// {{{2 modification

Symbol Symbol::flipSign() const {
    assert(type() == SymbolType::Fun || type() == SymbolType::Num);
    switch (symbolType_(rep_)) {
        case SymbolType_::Num: { return Symbol::createNum(-num()); }
        case SymbolType_::IdP: { return Symbol(setUpper(static_cast<uint16_t>(SymbolType_::IdN), rep_)); }
        case SymbolType_::IdN: { return Symbol(setUpper(static_cast<uint16_t>(SymbolType_::IdP), rep_)); }
        default: {
            auto const *f = cast<Fun>(rep_);
            auto s = f->sig();
            return createFun(s.name(), f->args(), !s.sign());
        }
    }
}

Symbol Symbol::replace(IdSymMap const &map) const {
    assert(symbolType_(rep_) != SymbolType_::IdN);
    switch(symbolType_(rep_)) {
        case SymbolType_::Fun: {
            SymVec vals;
            for (auto const &x : args()) {
                vals.emplace_back(x.replace(map));
            }
            return createFun(name(), Potassco::toSpan(vals));
        }
        case SymbolType_::IdP: {
            auto it = map.find(name());
            if (it != map.end()) {
                return it->second;
            }
        }
        default: {
            return *this;
        }
    }
}

// {{{2 comparison

size_t Symbol::hash() const {
    return get_value_hash(rep_);
}

namespace {

bool less(Symbol const &a, Symbol const &b) {
    auto ta = symbolType_(a.rep());
    auto tb = symbolType_(b.rep());
    if (ta != tb) { return ta < tb; }
    switch(ta) {
        case SymbolType_::Num: { return a.num() < b.num(); }
        case SymbolType_::IdN:
        case SymbolType_::IdP: { return a.name() < b.name(); }
        case SymbolType_::Str: { return a.string() < b.string(); }
        case SymbolType_::Inf:
        case SymbolType_::Sup: { return false; }
        case SymbolType_::Fun: {
            auto sa = a.sig();
            auto sb = b.sig();
            if (sa != sb) {
                return sa < sb;
            }
            auto aa = a.args();
            auto ab = b.args();
            return std::lexicographical_compare(begin(aa), end(aa), begin(ab), end(ab));
        }
        case SymbolType_::Special: { return false; }
    }
    assert(false);
    return false;
}

} // namespace

bool Symbol::operator==(Symbol const &other) const {
    return rep_ == other.rep_;
}

bool Symbol::operator!=(Symbol const &other) const {
    return rep_ != other.rep_;
}

bool Symbol::operator<(Symbol const &other) const {
    return (*this != other) && less(*this, other);
}

bool Symbol::operator>(Symbol const &other) const {
    return (*this != other) && less(other, *this);
}

bool Symbol::operator<=(Symbol const &other) const {
    return (*this == other) || less(*this, other);
}

bool Symbol::operator>=(Symbol const &other) const {
    return (*this == other) || less(other, *this);
}

// {{{2 output

void Symbol::print(std::ostream& out) const {
    switch(symbolType_(rep_)) {
        case SymbolType_::Num: { out << num(); break; }
        case SymbolType_::IdN: { out << "-"; }
        case SymbolType_::IdP: {
            char const *n = name().c_str();
            out << (n[0] != '\0' ? n : "()"); // NOLINT
            break;
        }
        case SymbolType_::Str: { out << '"' << quote(string().c_str()) << '"'; break; }
        case SymbolType_::Inf: { out << "#inf"; break; }
        case SymbolType_::Sup: { out << "#sup"; break; }
        case SymbolType_::Fun: {
            auto s = sig();
            if (s.sign()) {
                out << "-";
            }
            out << s.name();
            auto a = args();
            out << "(";
            if (a.size > 0) {
                std::copy(begin(a), end(a) - 1, std::ostream_iterator<Symbol>(out, ",")); // NOLINT
                out << *(end(a) - 1); // NOLINT
            }
            if (a.size == 1 && s.name() == "") {
                out << ",";
            }
            out << ")";
            break;
        }
        case SymbolType_::Special: { out << "#special"; break; }
    }
}

// }}}2

// }}}1

} // namespace Gringo

