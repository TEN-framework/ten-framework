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

#include "tests/tests.hh"
#include "gringo/symbol.hh"

#include <climits>

namespace Gringo { namespace Test {

TEST_CASE("symbol_bug", "[base]") {
    SECTION("fun") {
        auto a = Symbol::createId(std::string{"x"}.c_str());
        auto b = Symbol::createId(std::string{"x"}.c_str());
        REQUIRE(a == b);
    }
}

TEST_CASE("symbol", "[base]") {
    SymVec args = { Symbol::createNum(42), Symbol::createId("a") };
    SymVec symbols = {
        Symbol::createNum(INT_MIN),
        Symbol::createNum(INT_MAX),
        Symbol::createNum(0),
        Symbol::createNum(42),

        Symbol::createId("x"),
        Symbol::createId("abc"),

        Symbol::createStr(""),
        Symbol::createStr("xyz"),

        Symbol::createInf(),
        Symbol::createSup(),

        Symbol::createTuple(Potassco::toSpan(args)),
        Symbol::createFun("f", Potassco::toSpan(args))
    };

    SECTION("types") {
        REQUIRE(SymbolType::Num == symbols[0].type());
        REQUIRE(SymbolType::Num == symbols[1].type());
        REQUIRE(SymbolType::Num == symbols[2].type());
        REQUIRE(SymbolType::Num == symbols[3].type());

        REQUIRE(SymbolType::Fun == symbols[4].type());
        REQUIRE(SymbolType::Fun == symbols[5].type());

        REQUIRE(SymbolType::Str == symbols[6].type());
        REQUIRE(SymbolType::Str == symbols[7].type());

        REQUIRE(SymbolType::Inf == symbols[8].type());
        REQUIRE(symbols[9].type() == SymbolType::Sup);

        REQUIRE(SymbolType::Fun == symbols[11].type());
    }

    SECTION("symbols") {
        REQUIRE(INT_MIN == symbols[0].num());
        REQUIRE(INT_MAX == symbols[1].num());
        REQUIRE( 0 == symbols[2].num());
        REQUIRE(42 == symbols[3].num());

        REQUIRE(String("x") == symbols[4].name());
        REQUIRE(String("abc") == symbols[5].name());

        REQUIRE(String("") == symbols[6].string());
        REQUIRE(String("xyz") == symbols[7].string());

        REQUIRE(size_t(2u) == symbols[10].args().size);
        REQUIRE(42 == symbols[10].args()[0].num());
        REQUIRE(String("a") == symbols[10].args()[1].name());

        REQUIRE(String("f") == symbols[11].name());
        REQUIRE(42 == symbols[11].args()[0].num());
        REQUIRE(String("a") == symbols[11].args()[1].name());
    }

    SECTION("cmp_num") {
        REQUIRE(!(Symbol::createNum(0) < Symbol::createNum(0)));
        REQUIRE( (Symbol::createNum(0) < Symbol::createNum(1)));
        REQUIRE(!(Symbol::createNum(1) < Symbol::createNum(0)));

        REQUIRE(!(Symbol::createNum(0) > Symbol::createNum(0)));
        REQUIRE(!(Symbol::createNum(0) > Symbol::createNum(1)));
        REQUIRE( (Symbol::createNum(1) > Symbol::createNum(0)));

        REQUIRE( (Symbol::createNum(0) <= Symbol::createNum(0)));
        REQUIRE( (Symbol::createNum(0) <= Symbol::createNum(1)));
        REQUIRE(!(Symbol::createNum(1) <= Symbol::createNum(0)));

        REQUIRE( (Symbol::createNum(0) >= Symbol::createNum(0)));
        REQUIRE(!(Symbol::createNum(0) >= Symbol::createNum(1)));
        REQUIRE( (Symbol::createNum(1) >= Symbol::createNum(0)));

        REQUIRE( (Symbol::createNum(0) == Symbol::createNum(0)));
        REQUIRE(!(Symbol::createNum(0) == Symbol::createNum(1)));
        REQUIRE(!(Symbol::createNum(1) == Symbol::createNum(0)));

        REQUIRE(!(Symbol::createNum(0) != Symbol::createNum(0)));
        REQUIRE( (Symbol::createNum(0) != Symbol::createNum(1)));
        REQUIRE( (Symbol::createNum(1) != Symbol::createNum(0)));
    }

    SECTION("cmp_type") {
        REQUIRE(symbols[8] < symbols[0]);
        REQUIRE(symbols[0] < symbols[4]);
        REQUIRE(symbols[4] < symbols[6]);
        REQUIRE(symbols[6] < symbols[10]);
        REQUIRE(symbols[11] < symbols[9]);
    }

    SECTION("cmp_other") {
        REQUIRE(!(Symbol::createId("a") == Symbol::createId("a").flipSign()));
        REQUIRE( (Symbol::createId("a") != Symbol::createId("a").flipSign()));

        REQUIRE(!(Symbol::createId("a") < Symbol::createId("a")));
        REQUIRE( (Symbol::createId("a") < Symbol::createId("a").flipSign()));
        REQUIRE( (Symbol::createId("b") < Symbol::createId("a").flipSign()));
        REQUIRE( (Symbol::createId("aaa") < Symbol::createId("aab")));
        REQUIRE( (Symbol::createId("a") < Symbol::createId("aa")));
        REQUIRE(!(Symbol::createId("aa") < Symbol::createId("a")));

        REQUIRE(!(Symbol::createStr("a") < Symbol::createStr("a")));
        REQUIRE( (Symbol::createStr("aaa") < Symbol::createStr("aab")));
        REQUIRE( (Symbol::createStr("a") < Symbol::createStr("aa")));
        REQUIRE(!(Symbol::createStr("aa") < Symbol::createStr("a")));

        Symbol a = Symbol::createTuple( Potassco::toSpan(SymVec{ Symbol::createNum(1), Symbol::createNum(1) }) );
        Symbol b = Symbol::createTuple( Potassco::toSpan(SymVec{ Symbol::createNum(1) }) );
        Symbol c = Symbol::createTuple( Potassco::toSpan(SymVec{ Symbol::createNum(1), Symbol::createNum(2) }) );
        Symbol d = Symbol::createTuple( Potassco::toSpan(SymVec{ Symbol::createNum(2) }) );
        REQUIRE(((b < a) && !(a < b)));
        REQUIRE(((a < c) && !(c < a)));
        REQUIRE(((b < d) && !(d < b)));
        REQUIRE(((d < a) && !(a < d)));

        Symbol fa = Symbol::createFun( "f", Potassco::toSpan(SymVec{ Symbol::createNum(1), Symbol::createNum(1) }) );
        Symbol ga = Symbol::createFun( "g", Potassco::toSpan(SymVec{ Symbol::createNum(1), Symbol::createNum(1) }));
        Symbol fb = Symbol::createFun( "f", Potassco::toSpan(SymVec{ Symbol::createNum(1) }) );
        Symbol fc = Symbol::createFun( "f", Potassco::toSpan(SymVec{ Symbol::createNum(1), Symbol::createNum(2) }) );
        Symbol fd = Symbol::createFun( "f", Potassco::toSpan(SymVec{ Symbol::createNum(2) }) );
        Symbol gd = Symbol::createFun( "g", Potassco::toSpan(SymVec{ Symbol::createNum(2) }) );
        REQUIRE(((fb < fa) && !(fa < fb)));
        REQUIRE(((fa < fc) && !(fc < fa)));
        REQUIRE(((fb < fd) && !(fd < fb)));
        REQUIRE(((fd < fa) && !(fa < fd)));
        REQUIRE(((fa < ga) && !(ga < fa)));
        REQUIRE(((gd < fa) && !(fa < gd)));
        REQUIRE(((fa < fa.flipSign()) && !(fa.flipSign() < fa)));
        REQUIRE(((fa < ga.flipSign()) && !(ga.flipSign() < fa)));
    }

    SECTION("print") {
        std::ostringstream oss;
        auto toString = [&oss](Symbol const &val) -> std::string {
            oss << val;
            std::string str = oss.str();
            oss.str("");
            return str;

        };
        REQUIRE("0" == toString(symbols[2]));
        REQUIRE("42" == toString(symbols[3]));
        REQUIRE("x" == toString(symbols[4]));
        REQUIRE("-x" == toString(symbols[4].flipSign()));
        REQUIRE("x" == toString(symbols[4].flipSign().flipSign()));
        REQUIRE("abc" == toString(symbols[5]));
        REQUIRE("\"\"" == toString(symbols[6]));
        REQUIRE("\"xyz\"" == toString(symbols[7]));
        REQUIRE("#inf" == toString(symbols[8]));
        REQUIRE("#sup" == toString(symbols[9]));
        REQUIRE("(42,a)" == toString(symbols[10]));
        REQUIRE("f(42,a)" == toString(symbols[11]));
        REQUIRE("-f(42,a)" == toString(symbols[11].flipSign()));
        REQUIRE("f(42,a)" == toString(symbols[11].flipSign().flipSign()));
        REQUIRE("()" == toString(Symbol::createTuple(SymSpan{nullptr, 0})));

        std::string comp = toString(Symbol::createFun("g", SymSpan{symbols.data() + 2, symbols.size() - 2}));
        REQUIRE("g(0,42,x,abc,\"\",\"xyz\",#inf,#sup,(42,a),f(42,a))" == comp);
    }

    SECTION("sig") {
        std::vector<char const *> names { "a", "b", "c", "d" };
        for (uint32_t i = 1; i < 1073741824; i*=2) {
            String name = names[i % names.size()];
            Sig sig{ name.c_str(), i, false };
            REQUIRE(name == sig.name());
            REQUIRE(i == sig.arity());
        }
    }
}

} } // namespace Test Gringo

