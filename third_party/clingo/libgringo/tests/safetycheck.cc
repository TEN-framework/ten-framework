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

#include "gringo/safetycheck.hh"

#include "tests/tests.hh"
#include "tests/term_helper.hh"

#include <map>

namespace Gringo { namespace Test {

using namespace Gringo::IO;

// {{{ definition of auxiliary functions

namespace {

typedef SafetyChecker<std::string, std::string> C;
typedef std::tuple<std::string, std::vector<std::string>, std::vector<std::string>> T;

std::string check(std::vector<T> list) {
    C dep;
    std::unordered_map<std::string, C::VarNode*> vars;
    auto iv = [&vars, &dep](std::string const &y) -> C::VarNode& {
        auto &pVar = vars[y];
        if (!pVar) { pVar = &dep.insertVar(y); }
        return *pVar;
    };
    for (auto &x : list) {
        auto &ent(dep.insertEnt(std::get<0>(x)));
        for (auto &y : std::get<1>(x)) { dep.insertEdge(iv(y), ent); }
        for (auto &y : std::get<2>(x)) { dep.insertEdge(ent, iv(y)); }
    }
    std::vector<std::string> order, open;
    for (auto &x : dep.order()) { order.push_back(x->data); }
    for (auto &x : dep.open()) { open.push_back(x->data); }
    std::sort(open.begin(), open.end());
    return to_string(std::make_pair(std::move(order), std::move(open)));
}

} // namespace

// }}}

TEST_CASE("safetycheck", "[base]") {
    SECTION("safety") {
        REQUIRE("([Y=1,X=Y],[])" == check({ T{"X=Y",{"Y"},{"X"}}, T{"Y=1",{},{"Y"}} }));
        REQUIRE("([],[X,Y])" == check({ T{"0=X+Y",{"X","Y"}, {}} }));
        REQUIRE("([A=1,B=A,C=B,D=C],[])" == check({ T{"A=1",{}, {"A"}}, T{"B=A",{"A"}, {"B"}}, T{"C=B",{"B"}, {"C"}}, T{"D=C",{"C"}, {"D"}} }));
        REQUIRE("([],[A,B])" == check({ T{"A=B",{"B"}, {"A"}}, T{"B=A",{"A"}, {"B"}} }));
        REQUIRE("([A=1,B=A,A=B],[])" == check({ T{"A=B",{"B"}, {"A"}}, T{"B=A",{"A"}, {"B"}}, T{"A=1",{}, {"A"}} }));
        REQUIRE("([(X,Y)=(1,1),2=X+Y],[])" == check({ T{"(X,Y)=(1,1)",{}, {"X","Y"}}, T{"2=X+Y",{"X", "Y"}, {}} }));
    }
}

} } // namespace Test Gringo
