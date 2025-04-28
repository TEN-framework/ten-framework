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

#ifndef _GRINGO_TEST_TERM_HELPER_HH
#define _GRINGO_TEST_TERM_HELPER_HH

#include "tests/tests.hh"
#include "gringo/term.hh"

namespace Gringo { namespace Test {

// {{{ definition of functions to create terms

inline std::unique_ptr<ValTerm> val(Symbol v) {
    Location loc(String("dummy"), 1, 1, String("dummy"), 1, 1);
    return make_locatable<ValTerm>(loc, v);
}

inline std::unique_ptr<VarTerm> var(char const *x, int level = 0) {
    static std::unordered_map<String, Term::SVal> vals;
    auto &ret(vals[x]);
    if (!ret) { ret = std::make_shared<Symbol>(); }
    Location loc(String("dummy"), 1, 1, String("dummy"), 1, 1);
    auto var(make_locatable<VarTerm>(loc, String(x), ret));
    var->level = level;
    return var;
}

inline std::unique_ptr<LinearTerm> lin(char const *x, int m, int n) {
    Location loc(String("dummy"), 1, 1, String("dummy"), 1, 1);
    return make_locatable<LinearTerm>(loc, *var(x), m, n);
}

inline std::unique_ptr<DotsTerm> dots(UTerm &&left, UTerm &&right) {
    Location loc(String("dummy"), 1, 1, String("dummy"), 1, 1);
    return make_locatable<DotsTerm>(loc, std::move(left), std::move(right));
}

inline std::unique_ptr<UnOpTerm> unop(UnOp op, UTerm &&arg) {
    Location loc(String("dummy"), 1, 1, String("dummy"), 1, 1);
    return make_locatable<UnOpTerm>(loc, op, std::move(arg));
}

inline std::unique_ptr<BinOpTerm> binop(BinOp op, UTerm &&left, UTerm &&right) {
    Location loc(String("dummy"), 1, 1, String("dummy"), 1, 1);
    return make_locatable<BinOpTerm>(loc, op, std::move(left), std::move(right));
}

template <class... T>
std::unique_ptr<FunctionTerm> fun(char const *name, T&&... args) {
    Location loc(String("dummy"), 1, 1, String("dummy"), 1, 1);
    return make_locatable<FunctionTerm>(loc, String(name), Test::init<UTermVec>(std::forward<T>(args)...));
}

template <class... T>
std::unique_ptr<LuaTerm> lua(char const *name, T&&... args) {
    Location loc(String("dummy"), 1, 1, String("dummy"), 1, 1);
    return make_locatable<LuaTerm>(loc, String(name), Test::init<UTermVec>(std::forward<T>(args)...));
}

template <class... T>
std::unique_ptr<PoolTerm> pool(T&&... args) {
    Location loc(String("dummy"), 1, 1, String("dummy"), 1, 1);
    return make_locatable<PoolTerm>(loc, Test::init<UTermVec>(std::forward<T>(args)...));
}

template <class... T>
UTermVec termvec(T&&... args) {
    return Test::init<UTermVec>(std::forward<T>(args)...);
}

inline Symbol NUM(int num) { return Symbol::createNum(num); }
inline Symbol SUP() { return Symbol::createSup(); }
inline Symbol INF() { return Symbol::createInf(); }
inline Symbol STR(String name) { return Symbol::createStr(name); }
inline Symbol ID(String name, bool sign = false) { return Symbol::createId(name, sign); }
inline Symbol FUN(String name, SymVec args, bool sign = false) { return Symbol::createFun(name, Potassco::toSpan(args), sign); }

// }}}

} } // namespace Gringo Test

#endif // _GRINGO_TEST_TERM_HELPER_HH

