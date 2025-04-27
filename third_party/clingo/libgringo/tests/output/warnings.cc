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

#include "gringo/ground/dependency.hh"
#include "gringo/input/nongroundparser.hh"

namespace Gringo { namespace Output { namespace Test {

TEST_CASE("output-warnings", "[output]") {
    SECTION("warnings_simplify") {
        REQUIRE("([[]],[-:1:3-6: info: operation undefined:\n  (a+0)\n])" == IO::to_string(solve("p(a+0).")));
        REQUIRE("([[]],[-:1:8-11: info: operation undefined:\n  (a+0)\n])" == IO::to_string(solve("q :- X=a+0.")));
        REQUIRE("([[]],[-:1:8-11: info: operation undefined:\n  (a+0)\n])" == IO::to_string(solve("q :- p(a+0).")));
        REQUIRE("([[]],[-:1:3-6: info: operation undefined:\n  (a+0)\n])" == IO::to_string(solve("p(a+0) : #true.")));
        REQUIRE("([[]],[-:1:8-11: info: operation undefined:\n  (a+0)\n])" == IO::to_string(solve("q :- p(a+0) : #true.")));
        REQUIRE("([[]],[-:1:6-9: info: operation undefined:\n  (a+0)\n])" == IO::to_string(solve(":~ p(a+0). [0]")));
        REQUIRE("([[]],[-:1:7-10: info: operation undefined:\n  (a+0)\n])" == IO::to_string(solve(":~ . [a+0]")));

        REQUIRE("([[]],[-:1:5-8: info: operation undefined:\n  (a+0)\n])" == IO::to_string(solve("{ p(a+0) }.")));
        REQUIRE("([[]],[-:1:9-12: info: operation undefined:\n  (a+0)\n])" == IO::to_string(solve("{ q : p(a+0) }.")));
        REQUIRE("([[]],[-:1:16-19: info: operation undefined:\n  (a+0)\n])" == IO::to_string(solve("#count { q : p(a+0) }.")));
        REQUIRE("([[]],[-:1:12-15: info: operation undefined:\n  (a+0)\n])" == IO::to_string(solve("#count { p(a+0) : q }.")));
        REQUIRE("([[]],[-:1:20-23: info: operation undefined:\n  (a+0)\n])" == IO::to_string(solve("#count { q : q : p(a+0) }.")));

        REQUIRE("([[q]],[-:1:16-19: info: operation undefined:\n  (a+0)\n])" == IO::to_string(solve("q :- #true : p(a+0).")));
        REQUIRE("([[q]],[-:1:12-15: info: operation undefined:\n  (a+0)\n])" == IO::to_string(solve("q :- 0 { p(a+0) } 0.")));
        REQUIRE("([[q]],[-:1:16-19: info: operation undefined:\n  (a+0)\n])" == IO::to_string(solve("q :- 0 { q : p(a+0) } 0.")));
        REQUIRE("([[q]],[-:1:19-22: info: operation undefined:\n  (a+0)\n])" == IO::to_string(solve("q :- 0 #count { p(a+0) } 0.")));
        REQUIRE("([[q]],[-:1:23-26: info: operation undefined:\n  (a+0)\n])" == IO::to_string(solve("q :- 0 #count { q : p(a+0) } 0.")));

        REQUIRE("([],[-:1:11-14: info: operation undefined:\n  (a+0)\n])" == IO::to_string(solve("#true : q(a+0).")));
    }

    SECTION("warnings") {
        REQUIRE("([[]],[-:1:3-7: info: interval undefined:\n  1..f\n])" == IO::to_string(solve("p(1..f).")));
        REQUIRE("([[p(1)]],[-:1:11-15: info: interval undefined:\n  1..f\n])" == IO::to_string(solve("p(1). :-p(1..f).")));
        REQUIRE("([[p(bot)]],[-:2:3-6: info: operation undefined:\n  (1/X)\n])" == IO::to_string(solve("p(bot).\nq(1/X):-p(X).\n")));
        REQUIRE("([[p(bot)]],[-:2:6-7: info: operation undefined:\n  (X+0)\n])" == IO::to_string(solve("p(bot).\n#sum{X:q(X):p(X)}.\n")));
        REQUIRE("([[p(bot)]],[-:2:10-11: info: tuple ignored:\n  bot\n])" == IO::to_string(solve("p(bot).\nx:-1#sum{X:p(X)}.\n")));
        REQUIRE("([[p(bot)]],[-:2:4-16: info: empty tuple ignored\n])" == IO::to_string(solve("p(bot).\nx:-1#sum{:p(X)}.\n")));
        REQUIRE("([[]],[-:1:1-13: info: no atoms over signature occur in program:\n  bot/0\n])" == IO::to_string(solve("#show bot/0.\n")));
        REQUIRE("([[]],[-:1:4-7: info: atom does not occur in any rule head:\n  bot\n])" == IO::to_string(solve("x:-bot.\n")));
        REQUIRE("([[p(bot)]],[-:2:11-12: info: tuple ignored:\n  bot@0\n])" == IO::to_string(solve("p(bot).\n:~ p(X). [X]\n")));
        REQUIRE("([[a]],[])" == IO::to_string(solve("a:-#sum{-1:a;1:a}>=0.\n")));
        REQUIRE("([[a]],[])" == IO::to_string(solve("a:-#sum{1:a;2:a}!=1.\n")));
        REQUIRE("([[a]],[])" == IO::to_string(solve("a:-X=#sum{-1:a;1:a},X>=0,X<=0.\n")));
        REQUIRE("([],[-:1:1-12: error: cyclic constant definition:\n  #const a=b.\n-:2:1-12: note: cycle involves definition:\n  #const b=a.\n])" == IO::to_string(solve("#const a=b.\n#const b=a.\n")));
        REQUIRE("([[]],[])" == IO::to_string(solve("#const a=a.\n")));
        REQUIRE("([],[-:2:1-12: error: redefinition of constant:\n  #const a=2.\n-:1:1-12: note: constant also defined here\n])" == IO::to_string(solve("#const a=1.\n#const a=2.\np(a).\n")));
        struct Del {
            Del()  { std::ofstream("wincluded.lp"); }
            ~Del() { std::remove("wincluded.lp"); }
        } del;
        REQUIRE("([[]],[-:1:25-49: warning: already included file:\n  wincluded.lp\n])" == IO::to_string(solve("#include \"wincluded.lp\".#include \"wincluded.lp\".")));
        REQUIRE("([[]],[-:1:28-29: info: atom does not occur in any rule head:\n  c\n])" == IO::to_string(solve("#defined b/0. a :- b. a :- c.")));
    }
}

} } } // namespace Test Output Gringo
