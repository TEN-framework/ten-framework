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

#ifndef GRINGO_SYMBOL_HH
#define GRINGO_SYMBOL_HH

#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <vector>
#include <string>
#include <functional>
#include <cassert>
#include <iterator>
#include <algorithm>
#include <utility>
#include <potassco/basic_types.h>

namespace Gringo {

// {{{1 declaration of String (flyweight)

using StringSpan = Potassco::StringSpan;

class String {
public:
    String(StringSpan str);
    String(char const *str);

    const char *c_str() const;
    bool empty() const;
    size_t length() const;
    bool startsWith(char const *prefix) const;
    size_t hash() const;
    static uintptr_t toRep(String s) noexcept;
    static String fromRep(uintptr_t t) noexcept;

private:
    class Impl;

    String(uintptr_t) noexcept;
    Impl *str_;
};

inline bool operator==(String a, String b) { return std::strcmp(a.c_str(), b.c_str()) == 0; }
inline bool operator!=(String a, String b) { return std::strcmp(a.c_str(), b.c_str()) != 0; }
inline bool operator< (String a, String b) { return std::strcmp(a.c_str(), b.c_str()) <  0; }
inline bool operator> (String a, String b) { return std::strcmp(a.c_str(), b.c_str()) >  0; }
inline bool operator<=(String a, String b) { return std::strcmp(a.c_str(), b.c_str()) <= 0; }
inline bool operator>=(String a, String b) { return std::strcmp(a.c_str(), b.c_str()) >= 0; }

inline bool operator==(String a, char const *b) { return std::strcmp(a.c_str(), b) == 0; }
inline bool operator!=(String a, char const *b) { return std::strcmp(a.c_str(), b) != 0; }
inline bool operator< (String a, char const *b) { return std::strcmp(a.c_str(), b) <  0; }
inline bool operator> (String a, char const *b) { return std::strcmp(a.c_str(), b) >  0; }
inline bool operator<=(String a, char const *b) { return std::strcmp(a.c_str(), b) <= 0; }
inline bool operator>=(String a, char const *b) { return std::strcmp(a.c_str(), b) >= 0; }

inline bool operator==(char const *a, String b) { return std::strcmp(a, b.c_str()) == 0; }
inline bool operator!=(char const *a, String b) { return std::strcmp(a, b.c_str()) != 0; }
inline bool operator< (char const *a, String b) { return std::strcmp(a, b.c_str()) <  0; }
inline bool operator> (char const *a, String b) { return std::strcmp(a, b.c_str()) >  0; }
inline bool operator<=(char const *a, String b) { return std::strcmp(a, b.c_str()) <= 0; }
inline bool operator>=(char const *a, String b) { return std::strcmp(a, b.c_str()) >= 0; }

inline std::ostream &operator<<(std::ostream &out, String x) {
    out << x.c_str();
    return out;
}

// {{{1 declaration of Signature (flyweight)

class Sig {
public:
    Sig(String name, uint32_t arity, bool sign);
    explicit Sig(uint64_t rep) : rep_(rep) {  }

    String name() const;
    Sig flipSign() const;
    uint32_t arity() const;
    bool sign() const;
    size_t hash() const;
    uint64_t const &rep() const { return rep_; }
    bool match(String n, uint32_t a, bool s = false) const {
        return name() == n && arity() == a && sign() == s;
    }
    bool match(char const *n, uint32_t a, bool s = false) const {
        return name() == n && arity() == a && sign() == s;
    }
    bool operator==(Sig s) const;
    bool operator!=(Sig s) const;
    bool operator<(Sig s) const;
    bool operator>(Sig s) const;
    bool operator<=(Sig s) const;
    bool operator>=(Sig s) const;

private:
    uint64_t rep_;
};

inline std::ostream &operator<<(std::ostream &out, Sig x) {
    if (x.sign()) { out << "-"; }
    out << x.name() << "/" << x.arity();
    return out;
}

// {{{1 declaration of Symbol (flyweight)

enum class SymbolType : uint8_t {
    Inf     = 0,
    Num     = 1,
    Str     = 4,
    Fun     = 5,
    Special = 6,
    Sup     = 7
};

inline std::ostream &operator<<(std::ostream &out, SymbolType sym) {
    switch (sym) {
        case SymbolType::Inf: { out << "Inf"; break; }
        case SymbolType::Num: { out << "Num"; break; }
        case SymbolType::Str: { out << "Str"; break; }
        case SymbolType::Fun: { out << "Fun"; break; }
        case SymbolType::Special: { out << "Special"; break; }
        case SymbolType::Sup: { out << "Sup"; break; }
    }
    return out;
}

class Symbol;
using SymVec = std::vector<Symbol>;
using SymSpan = Potassco::Span<Symbol>;
using IdSymMap = std::unordered_map<String, Symbol>;

class Symbol {
public:
    // construction
    Symbol(); // createSpecial
    explicit Symbol(uint64_t sym) : rep_(sym) { };
    static Symbol createId(String val, bool sign = false);
    static Symbol createStr(String val);
    static Symbol createNum(int num);
    static Symbol createInf();
    static Symbol createSup();
    static Symbol createTuple(SymSpan args);
    static Symbol createTuple(SymVec const &args) {
        return createTuple(Potassco::toSpan(args));
    }
    static Symbol createFun(String name, SymSpan args, bool sign = false);
    static Symbol createFun(String name, SymVec const &args, bool sign = false) {
        return createFun(name, Potassco::toSpan(args), sign);
    }

    // value retrieval
    SymbolType type() const;
    int num() const;
    String string() const;
    Sig sig() const;
    bool hasSig() const;
    uint32_t arity() const { return sig().arity(); }
    String name() const;
    SymSpan args() const;
    bool sign() const;

    // modifying values
    Symbol replace(IdSymMap const &map) const;
    Symbol flipSign() const;

    // comparison
    size_t hash() const;
    bool operator==(Symbol const &other) const;
    bool operator!=(Symbol const &other) const;
    bool operator<(Symbol const &other) const;
    bool operator>(Symbol const &other) const;
    bool operator<=(Symbol const &other) const;
    bool operator>=(Symbol const &other) const;

    // ouput
    void print(std::ostream& out) const;

    uint64_t const &rep () const { return rep_; }
private:
    uint64_t rep_;
};

inline std::ostream& operator<<(std::ostream& out, Symbol sym) {
    sym.print(out);
    return out;
}

// {{{1 definition of quote/unquote

inline std::string quote(StringSpan str) {
    std::string res;
    for (auto c : str) {
        switch (c) {
            case '\n': {
                res.push_back('\\');
                res.push_back('n');
                break;
            }
            case '\\': {
                res.push_back('\\');
                res.push_back('\\');
                break;
            }
            case '"': {
                res.push_back('\\');
                res.push_back('"');
                break;
            }
            default: {
                res.push_back(c);
                break;
            }
        }
    }
    return res;
}
inline std::string quote(char const *str) {
    return quote({str, strlen(str)});
}

inline std::string unquote(StringSpan str) {
    std::string res;
    bool slash = false;
    for (auto c : str) {
        if (slash) {
            switch (c) {
                case 'n': {
                    res.push_back('\n');
                    break;
                }
                case '\\': {
                    res.push_back('\\');
                    break;
                }
                case '"': {
                    res.push_back('"');
                    break;
                }
                default: {
                    assert(false);
                    break;
                }
            }
            slash = false;
        }
        else if (c == '\\') { slash = true; }
        else { res.push_back(c); }
    }
    return res;
}

inline std::string unquote(char const *str) {
    return unquote({str, strlen(str)});
}

// }}}1

} // namespace Gringo

namespace std {

// {{{1 definition of hash functions

template<>
struct hash<Gringo::String> {
    size_t operator()(Gringo::String const &str) const { return str.hash(); }
};

template<>
struct hash<Gringo::Sig> {
    size_t operator()(Gringo::Sig const &sig) const { return sig.hash(); }
};

template<>
struct hash<Gringo::Symbol> {
    size_t operator()(Gringo::Symbol const &sym) const { return sym.hash(); }
};

// }}}1

} // namespace std

#endif // GRINGO_SYMBOL_HH

