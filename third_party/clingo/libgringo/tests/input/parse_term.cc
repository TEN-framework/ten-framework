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
#include "tests/term_helper.hh"

#include <climits>
#include <sstream>

namespace Gringo { namespace Input { namespace Test {

using namespace Gringo::Test;

TEST_CASE("input-parse_term", "[input]") {

    SECTION("parse") {
        TestGringoModule m;
        REQUIRE_THROWS_AS(m.parseValue("a."), std::runtime_error);
        REQUIRE(Symbol() == m.parseValue("x+y"));
        REQUIRE(NUM(1) == m.parseValue("1"));
        REQUIRE(NUM(-1) == m.parseValue("-1"));
        REQUIRE(ID("x") == m.parseValue("x"));
        REQUIRE(ID("x", true) == m.parseValue("-x"));
        REQUIRE(FUN("f", {ID("x")}) == m.parseValue("f(x)"));
        REQUIRE(FUN("f", {ID("x")}, true) == m.parseValue("-f(x)"));
        REQUIRE(NUM(39) == m.parseValue("7?32"));
        REQUIRE(NUM(5) == m.parseValue("7&13"));
        REQUIRE(NUM(8) == m.parseValue("13-5"));
        REQUIRE(NUM(18) == m.parseValue("13+5"));
        REQUIRE(NUM(65) == m.parseValue("13*5"));
        REQUIRE(NUM(3) == m.parseValue("13\\5"));
        REQUIRE(NUM(2) == m.parseValue("13/5"));
        REQUIRE(NUM(371293) == m.parseValue("13**5"));
        REQUIRE(NUM(-2) == m.parseValue("~1"));
        REQUIRE(FUN("", {}) == m.parseValue("()"));
        REQUIRE(FUN("", {NUM(1)}) == m.parseValue("(1,)"));
        REQUIRE(FUN("", {NUM(1),NUM(2)}) == m.parseValue("(1,2)"));
        REQUIRE(NUM(2) == m.parseValue("|-2|"));
        REQUIRE(STR("hallo") == m.parseValue("\"hallo\""));
        REQUIRE(INF() == m.parseValue("#inf"));
        REQUIRE(SUP() == m.parseValue("#sup"));
        REQUIRE(NUM(11259375) == m.parseValue("0xabcdef"));
        REQUIRE(NUM(11259375) == m.parseValue("0xABCDEF"));
        REQUIRE(NUM(342391) == m.parseValue("0o1234567"));
        REQUIRE(NUM(85) == m.parseValue("0b1010101"));
    }

}

} } } // namespace Test Input Gringo

