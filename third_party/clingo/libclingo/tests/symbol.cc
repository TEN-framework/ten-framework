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

#include "tests.hh"
#include <iostream>
#include <fstream>

namespace Clingo { namespace Test {

TEST_CASE("symbol", "[clingo]") {
    SECTION("signature") {
        Signature a("a", 2, false);
        Signature b("a", 2, false);
        Signature c("a", 2, true);
        REQUIRE(a.name() == S("a"));
        REQUIRE(a.arity() == 2);
        REQUIRE(a.negative());
        REQUIRE(!a.positive());
        REQUIRE(b == a);
        REQUIRE(c != a);
        REQUIRE(c < a);
        REQUIRE(c <= a);
        REQUIRE(a <= b);
        REQUIRE_FALSE(a <= c);
        REQUIRE(a > c);
        REQUIRE(a >= c);
        REQUIRE(a >= b);
        REQUIRE_FALSE(c >= a);
        REQUIRE(c.hash() != a.hash());
        REQUIRE(b.hash() == a.hash());
    }
    SECTION("symbol") {
        std::vector<Symbol> args;
        // numbers
        auto sym = Number(42);
        REQUIRE(42 == sym.number());
        args.emplace_back(sym);
        // inf
        sym = Infimum();
        REQUIRE(SymbolType::Infimum == sym.type());
        args.emplace_back(sym);
        // sup
        sym = Supremum();
        REQUIRE(SymbolType::Supremum == sym.type());
        args.emplace_back(sym);
        // str
        sym = String("x");
        REQUIRE(S("x") == sym.string());
        args.emplace_back(sym);
        // id
        sym = Id("x", false);
        REQUIRE(SymbolType::Function == sym.type());
        REQUIRE(sym.is_negative());
        REQUIRE(!sym.is_positive());
        REQUIRE(S("x") == sym.name());
        args.emplace_back(sym);
        // fun
        sym = Function("f", args);
        REQUIRE(SymbolType::Function == sym.type());
        REQUIRE(!sym.is_negative());
        REQUIRE(S("f") == sym.name());
        REQUIRE("f(42,#inf,#sup,\"x\",-x)" == sym.to_string());
        REQUIRE((args.size() == sym.arguments().size() && std::equal(args.begin(), args.end(), sym.arguments().begin())));
        try { sym.number(); }
        catch (std::exception const &e) { REQUIRE(e.what() == S("unexpected")); }
        // comparison
        auto a = Number(1), b = Number(2);
        REQUIRE(a < b);
        REQUIRE_FALSE(a < a);
        REQUIRE_FALSE(b < a);
        REQUIRE(b > a);
        REQUIRE_FALSE(a > a);
        REQUIRE_FALSE(a > b);
        REQUIRE(a <= a);
        REQUIRE(a <= b);
        REQUIRE_FALSE(b <= a);
        REQUIRE(a >= a);
        REQUIRE(b >= a);
        REQUIRE_FALSE(a >= b);
        REQUIRE(a == a);
        REQUIRE_FALSE(a == b);
        REQUIRE(a != b);
        REQUIRE_FALSE(a != a);
        REQUIRE(a.hash() == a.hash());
        REQUIRE(a.hash() != b.hash());
    }
}

} } // namespace Test Clingo
