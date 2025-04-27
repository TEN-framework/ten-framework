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
#include "tests/output/solver_helper.hh"
#include "tests/ground/grounder_helper.hh"

namespace Gringo { namespace Output { namespace Test {

TEST_CASE("output-theory", "[output]") {
    SECTION("directive") {
        std::string theory =
            "#theory t {"
            "    group { };"
            "    &a/0 : group, directive"
            "}.";
        REQUIRE(
            "9 1 0 1 a\n"
            "9 5 0 0 0\n"
            "0\n" ==
            Gringo::Ground::Test::groundAspif(
                theory +
                "&a { }."));
    }

    SECTION("head") {
        std::string theory =
            "#theory t {"
            "    group { };"
            "    &a/0 : group, head"
            "}.";
        REQUIRE(
            "1 0 1 1 0 0\n"
            "9 1 0 1 a\n"
            "9 5 1 0 0\n"
            "0\n" ==
            Gringo::Ground::Test::groundAspif(
                theory +
                "&a { }."));
    }

    SECTION("body") {
        std::string theory =
            "#theory t {"
            "    group { };"
            "    &a/0 : group, body"
            "}.";
        REQUIRE(
            "1 0 0 0 1 1\n"
            "9 1 0 1 a\n"
            "9 5 1 0 0\n"
            "0\n" ==
            Gringo::Ground::Test::groundAspif(
                theory +
                ":-&a { }."));
    }

    SECTION("guard") {
        std::string theory =
            "#theory t {\n"
            "    group { };\n"
            "    &a/0 : group, {=}, group, directive\n"
            "}.\n";
        REQUIRE(
            "9 1 0 1 a\n"
            "9 1 2 1 =\n"
            "9 0 1 1\n"
            "9 6 0 0 0 2 1\n"
            "0\n" ==
            Gringo::Ground::Test::groundAspif(
                theory +
                "&a { } = 1."));
    }

    SECTION("term") {
        std::string theory =
            "#theory t {\n"
            "  group {\n"
            "    + : 4, unary;\n"
            "    - : 4, unary;\n"
            "    ^ : 3, binary, right;\n"
            "    * : 2, binary, left;\n"
            "    + : 1, binary, left;\n"
            "    - : 1, binary, left\n"
            "  };\n"
            "  &a/0 : group, head\n"
            "}.\n";
        REQUIRE(
            "1 0 1 1 0 0\n"
            "9 1 0 1 a\n"
            "9 4 0 0 0\n"
            "9 5 1 0 1 0\n"
            "0\n" == Gringo::Ground::Test::groundAspif(theory + "&a { : }."));
        REQUIRE(
            "1 0 1 1 0 0\n"
            "9 1 0 1 a\n"
            "9 0 1 1\n"
            "9 4 0 2 1 1 0\n"
            "9 5 1 0 1 0\n"
            "0\n" == Gringo::Ground::Test::groundAspif(theory + "&a { 1,1 }."));
        REQUIRE(
            "1 0 1 1 0 0\n"
            "9 1 0 1 a\n"
            "9 2 1 -1 0\n"
            "9 4 0 1 1 0\n"
            "9 5 1 0 1 0\n"
            "0\n" == Gringo::Ground::Test::groundAspif(theory + "&a { () }."));
        REQUIRE(
            "1 0 1 1 0 0\n"
            "9 1 0 1 a\n"
            "9 1 1 4 #sup\n"
            "9 4 0 1 1 0\n"
            "9 5 1 0 1 0\n"
            "0\n" == Gringo::Ground::Test::groundAspif(theory + "&a { #sup }."));
        REQUIRE(
            "1 0 1 1 0 0\n"
            "9 1 0 1 a\n"
            "9 1 1 4 #inf\n"
            "9 4 0 1 1 0\n"
            "9 5 1 0 1 0\n"
            "0\n" == Gringo::Ground::Test::groundAspif(theory + "&a { #inf }."));
        REQUIRE(
            "1 0 1 1 0 0\n"
            "9 1 0 1 a\n"
            "9 0 2 1\n"
            "9 1 1 1 f\n"
            "9 2 3 1 1 2\n"
            "9 4 0 1 3 0\n"
            "9 5 1 0 1 0\n"
            "0\n" == Gringo::Ground::Test::groundAspif(theory + "&a { f(1) }."));
        REQUIRE(
            "1 0 1 1 0 0\n"
            "9 1 0 1 a\n"
            "9 0 1 1\n"
            "9 2 2 -1 1 1\n"
            "9 4 0 1 2 0\n"
            "9 5 1 0 1 0\n"
            "0\n" == Gringo::Ground::Test::groundAspif(theory + "&a { (1,) }."));
        REQUIRE(
            "1 0 1 1 0 0\n"
            "9 1 0 1 a\n"
            "9 0 1 1\n"
            "9 0 2 2\n"
            "9 2 3 -1 2 1 2\n"
            "9 4 0 1 3 0\n"
            "9 5 1 0 1 0\n"
            "0\n" == Gringo::Ground::Test::groundAspif(theory + "&a { (1,2) }."));
        REQUIRE(
            "1 0 1 1 0 0\n"
            "9 1 0 1 a\n"
            "9 0 1 1\n"
            "9 0 2 2\n"
            "9 2 3 -2 2 1 2\n"
            "9 4 0 1 3 0\n"
            "9 5 1 0 1 0\n"
            "0\n" == Gringo::Ground::Test::groundAspif(theory + "&a { {1,2} }."));
        REQUIRE(
            "1 0 1 1 0 0\n"
            "9 1 0 1 a\n"
            "9 0 1 1\n"
            "9 0 2 2\n"
            "9 2 3 -3 2 1 2\n"
            "9 4 0 1 3 0\n"
            "9 5 1 0 1 0\n"
            "0\n" == Gringo::Ground::Test::groundAspif(theory + "&a { [1,2] }."));
        REQUIRE(
            "1 0 1 1 0 0\n"
            "9 1 0 1 a\n"
            "9 0 2 1\n"
            "9 1 1 1 -\n"
            "9 2 3 1 1 2\n"
            "9 4 0 1 3 0\n"
            "9 5 1 0 1 0\n"
            "0\n" == Gringo::Ground::Test::groundAspif(theory + "&a { (-1) }."));
        REQUIRE(
            "1 0 1 1 0 0\n"
            "9 1 0 1 a\n"
            "9 0 2 1\n"
            "9 1 1 1 -\n"
            "9 2 3 1 1 2\n"
            "9 4 0 1 3 0\n"
            "9 5 1 0 1 0\n"
            "0\n" == Gringo::Ground::Test::groundAspif(theory + "&a { X } :- X=-1."));
        REQUIRE(
            "1 0 1 1 0 0\n"
            "9 1 0 1 a\n"
            "9 0 3 1\n"
            "9 1 2 1 +\n"
            "9 2 4 2 1 3\n"
            "9 1 1 1 -\n"
            "9 2 5 1 1 4\n"
            "9 4 0 1 5 0\n"
            "9 5 1 0 1 0\n"
            "0\n" == Gringo::Ground::Test::groundAspif(theory + "&a { - + 1 }."));
        REQUIRE(
            "1 0 1 1 0 0\n"
            "9 1 0 1 a\n"
            "9 0 2 1\n"
            "9 0 3 2\n"
            "9 1 1 1 ^\n"
            "9 2 4 1 2 2 3\n"
            "9 4 0 1 4 0\n"
            "9 5 1 0 1 0\n"
            "0\n" == Gringo::Ground::Test::groundAspif(theory + "&a { 1^2 }."));
        REQUIRE(
            "1 0 1 1 0 0\n"
            "9 1 0 1 a\n"
            "9 0 2 1\n"
            "9 0 4 2\n"
            "9 0 5 3\n"
            "9 1 3 1 *\n"
            "9 2 6 3 2 4 5\n"
            "9 1 1 1 +\n"
            "9 2 7 1 2 2 6\n"
            "9 4 0 1 7 0\n"
            "9 5 1 0 1 0\n"
            "0\n" == Gringo::Ground::Test::groundAspif(theory + "&a { 1+2*3 }."));
    }
}

} } } // namespace Test Output Gringo

