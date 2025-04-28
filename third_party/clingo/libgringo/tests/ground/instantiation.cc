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

#include "gringo/input/nongroundparser.hh"
#include "gringo/input/program.hh"
#include "gringo/ground/program.hh"
#include "gringo/output/output.hh"

#include "tests/tests.hh"

#include <regex>

namespace Gringo { namespace Ground { namespace Test {

using namespace Gringo::IO;

namespace {

std::string ground(std::string const &str, std::initializer_list<std::string> filter = {""}) {
    std::regex delayedDef("^#delayed\\(([0-9]+)\\) <=> (.*)$");
    std::regex delayedOcc("#delayed\\(([0-9]+)\\)");
    std::map<std::string, std::string> delayedMap;
    std::stringstream ss;

    Potassco::TheoryData td;
    Output::OutputBase out(td, {}, ss, Output::OutputFormat::TEXT);
    Input::Program prg;
    Defines defs;
    Gringo::Test::TestGringoModule module;
    Gringo::Test::TestContext context;
    NullBackend bck;
    Input::NongroundProgramBuilder pb{ context, prg, out.outPreds, defs };
    bool incmode;
    Input::NonGroundParser ngp{ pb, bck, incmode };
    ngp.pushStream("-", gringo_make_unique<std::stringstream>(str), module);
    ngp.parse(module);
    prg.rewrite(defs, module);
    Program gPrg(prg.toGround({Sig{"base", 0, false}}, out.data, module));
    Parameters params;
    params.add("base", {});
    gPrg.prepare(params, out, module);
    gPrg.ground(context, out, module);
    out.endStep({});

    std::string line;
    std::vector<std::string> res;
    ss.seekg(0, std::ios::beg);
    while (std::getline(ss, line)) {
        std::smatch m;
        if (std::regex_match(line, m, delayedDef)) {
            delayedMap[m[1]] = m[2];
        }
        else if (!line.compare(0, 9, "#delayed(")) {
            res.emplace_back(std::move(line));
        }
        else {
            for (auto &x : filter) {
                if (!line.compare(0, x.size(), x)) {
                    res.emplace_back(std::move(line));
                    break;
                }
            }
        }
    }
    for (auto &x : res) {
        std::string r;
        auto st = x.cbegin();
        for (auto it = std::sregex_iterator(x.begin(), x.end(), delayedOcc), ie = std::sregex_iterator(); it != ie; ++it) {
            std::smatch match = *it;
            r.append(st, match.prefix().second);
            st = match.suffix().first;
            r.append(delayedMap[match[1]]);
        }
        r.append(st, x.cend());
        x = r;
    }
    std::stringstream oss;
    std::sort(res.begin(), res.end());
    for (auto &x : res) { oss << x << "\n"; }
    for (auto &x : module.messages()) { oss << x; }
    return oss.str();
}

std::string gbie() {
    return
        "char_to_digit(X,X) :- X=0..9.\n"
        "digit(Y-1,Y,D) :- char(C,Y), char_to_digit(C,D), Y > 0.\n"
        "sign(X, 1) :- char(p,X).\n"
        "sign(X,-1) :- char(m,X).\n"
        "num_1(X,Y,A)      :- digit(X,Y,A), not digit(_,X,_).\n"
        "num_1(X,Z,10*A+B) :- num_1(X,Y,A), digit(Y,Z,B).\n"
        "num(X,Y,A)        :- num_1(X,Y,A), not digit(_,Y+1,_).\n"
        "par_expr(Y-1,Z+1,A) :- char(o,Y), expr(Y,Z,A), char(c,Z+1), Y >= 0.\n"
        "sign_expr(Y-1,Z,A*S) :- par_expr(Y,Z,A), sign(Y,S), Y > 0.\n"
        "sign_expr(Y-1,Z,A*S) :- num(Y,Z,A),      sign(Y,S), Y > 0.\n"
        "expr(0,Y,A)   :- num(0,Y,A).\n"
        "expr(X,Y,A)   :- char(o,X), num(X,Y,A).\n"
        "expr(X,Y,A)   :- par_expr(X,Y,A).\n"
        "expr(X,Y,A)   :- sign_expr(X,Y,A).\n"
        "expr(X,Z,A+B) :- expr(X,Y,A), sign_expr(Y,Z,B).\n"
        "cmp(Y-1,A)   :- char(g,Y), num(Y,_,A), Y > 0.\n"
        "cmp(Y-1,A*S) :- char(g,Y), sign(Y+1,S), num(Y+1,_,A), Y > 0.\n"
        "le(A,B) :- expr(0,Y,A), cmp(Y,B), A <= B.\n"
        "gt(A,B) :- expr(0,Y,A), cmp(Y,B), A >  B.\n";
}

std::string gbie1() {
    return
        "char(p,1).\n"  "char(0,2).\n"  "char(m,3).\n"  "char(o,4).\n"
        "char(o,5).\n"  "char(6,6).\n"  "char(m,7).\n"  "char(7,8).\n"
        "char(p,9).\n"  "char(3,10).\n" "char(p,11).\n" "char(o,12).\n"
        "char(4,13).\n" "char(c,14).\n" "char(c,15).\n" "char(m,16).\n"
        "char(2,17).\n" "char(p,18).\n" "char(o,19).\n" "char(p,20).\n"
        "char(0,21).\n" "char(c,22).\n" "char(c,23).\n" "char(p,24).\n"
        "char(2,25).\n" "char(m,26).\n" "char(o,27).\n" "char(p,28).\n"
        "char(1,29).\n" "char(c,30).\n" "char(g,31).\n" "char(m,32).\n"
        "char(4,33).\n";
}

std::string gbie2() {
    return
        "char(o,1).\n"  "char(m,2).\n"  "char(o,3).\n"  "char(p,4).\n"
        "char(1,5).\n"  "char(m,6).\n"  "char(o,7).\n"  "char(m,8).\n"
        "char(2,9).\n"  "char(c,10).\n" "char(p,11).\n" "char(7,12).\n"
        "char(c,13).\n" "char(m,14).\n" "char(5,15).\n" "char(m,16).\n"
        "char(o,17).\n" "char(m,18).\n" "char(4,19).\n" "char(p,20).\n"
        "char(o,21).\n" "char(p,22).\n" "char(o,23).\n" "char(p,24).\n"
        "char(2,25).\n" "char(c,26).\n" "char(c,27).\n" "char(c,28).\n"
        "char(c,29).\n" "char(p,30).\n" "char(o,31).\n" "char(o,32).\n"
        "char(m,33).\n" "char(6,34).\n" "char(p,35).\n" "char(1,36).\n"
        "char(c,37).\n" "char(c,38).\n" "char(g,39).\n" "char(m,40).\n"
        "char(1,41).\n" "char(6,42).\n";
}

std::string strategicA1() {
    return
        "controlled_by(1,5,2,2).\n"
        "controlled_by(1,5,3,3).\n"
        "controlled_by(2,6,3,5).\n"
        "controlled_by(2,6,3,1).\n"
        "controlled_by(3,4,1,5).\n"
        "controlled_by(3,4,1,6).\n"
        "controlled_by(4,2,6,6).\n"
        "controlled_by(4,2,5,5).\n"
        "controlled_by(5,1,1,1).\n"
        "controlled_by(6,4,4,4).\n"
        "produced_by(p1, 4,2).\n"
        "produced_by(p2, 5,1).\n"
        "produced_by(p3, 2,6).\n"
        "produced_by(p4, 4,2).\n"
        "produced_by(p5, 5,6).\n"
        "produced_by(p6, 2,4).\n"
        "produced_by(p7, 4,3).\n"
        "produced_by(p8, 5,2).\n"
        "produced_by(p9, 5,5).\n"
        "produced_by(p10, 6,2).\n"
        "produced_by(p11, 4,4).\n"
        "produced_by(p12, 4,3).\n"
        "produced_by(p13, 4,6).\n"
        "produced_by(p14, 4,4).\n"
        "produced_by(p15, 6,4).\n"
        "produced_by(p16, 1,1).\n"
        "produced_by(p17, 5,5).\n"
        "produced_by(p18, 1,6).\n"
        "produced_by(p19, 2,5).\n"
        "produced_by(p20, 1,6).\n"
        "produced_by(p21, 4,6).\n"
        "produced_by(p22, 2,5).\n"
        "produced_by(p23, 2,6).\n"
        "produced_by(p24, 6,6).\n";
}

std::string strategicB1() {
    return
        "controlled_by(c1,1,(5;2;2)).\n"
        "controlled_by(c2,1,(5;3;3)).\n"
        "controlled_by(c3,2,(6;3;5)).\n"
        "controlled_by(c4,2,(6;3;1)).\n"
        "controlled_by(c5,3,(4;1;5)).\n"
        "controlled_by(c6,3,(4;1;6)).\n"
        "controlled_by(c7,4,(2;6;6)).\n"
        "controlled_by(c8,4,(2;5;5)).\n"
        "controlled_by(c9,5,(1;1;1)).\n"
        "controlled_by(c10,6,(4;4;4)).\n"
        "produced_by(p1, (4;2)).\n"
        "produced_by(p2, (5;1)).\n"
        "produced_by(p3, (2;6)).\n"
        "produced_by(p4, (4;2)).\n"
        "produced_by(p5, (5;6)).\n"
        "produced_by(p6, (2;4)).\n"
        "produced_by(p7, (4;3)).\n"
        "produced_by(p8, (5;2)).\n"
        "produced_by(p9, (5;5)).\n"
        "produced_by(p10,(6;2)).\n"
        "produced_by(p11,(4;4)).\n"
        "produced_by(p12,(4;3)).\n"
        "produced_by(p13,(4;6)).\n"
        "produced_by(p14,(4;4)).\n"
        "produced_by(p15,(6;4)).\n"
        "produced_by(p16,(1;1)).\n"
        "produced_by(p17,(5;5)).\n"
        "produced_by(p18,(1;6)).\n"
        "produced_by(p19,(2;5)).\n"
        "produced_by(p20,(1;6)).\n"
        "produced_by(p21,(4;6)).\n"
        "produced_by(p22,(2;5)).\n"
        "produced_by(p23,(2;6)).\n"
        "produced_by(p24,(6;6)).\n";
}

}

TEST_CASE("ground-instantiation", "[ground]") {
    SECTION("instantiateRec") {
        REQUIRE(
            "a(0).\n"
            "a(1).\n"
            "a(2).\n"
            "a(3).\n"
            "a(4).\n"
            "b(0).\n"
            "b(1).\n"
            "b(2).\n"
            "b(3).\n"
            "b(4).\n"
            "c(0).\n"
            "c(1).\n"
            "c(2).\n"
            "c(3).\n"
            "c(4).\n"
            "d(0).\n"
            "d(1).\n"
            "d(2).\n"
            "d(3).\n"
            "d(4).\n"
            "p(0).\n"
            "p(1).\n"
            "p(2).\n"
            "p(3).\n"
            "p(4).\n"
            "p(5).\n" == ground(
                "p(0).\n"
                "a(X) :- p(X), X < 5.\n"
                "b(X) :- a(X).\n"
                "c(X) :- b(X), a(X).\n"
                "d(X) :- c(X), b(X), a(X).\n"
                "p(X+1) :- a(X), b(X), c(X), d(X).\n"));
    }

    SECTION("instantiate") {
        REQUIRE(
            "e(1,2).\n"
            "e(2,1).\n"
            "r(1).\n"
            "r(2).\n"
            "v(1).\n" == ground("v(1).e(1,2).e(2,1).r(X):-v(X).r(Y):-r(X),e(X,Y)."));
        REQUIRE(
            "p(1,2).\n" "p(1,3).\n" "p(1,4).\n" "p(1,5).\n" "p(1,6).\n" "p(1,7).\n"
            "p(2,3).\n" "p(2,4).\n" "p(2,5).\n" "p(2,6).\n" "p(2,7).\n"
            "p(3,4).\n" "p(3,5).\n" "p(3,6).\n" "p(3,7).\n"
            "p(4,5).\n" "p(4,6).\n" "p(4,7).\n"
            "p(5,6).\n" "p(5,7).\n"
            "p(6,7).\n" == ground(
                "p(1,2)."
                "p(2,3)."
                "p(3,4)."
                "p(4,5)."
                "p(5,6)."
                "p(6,7)."
                "p(X,Z) :- p(X,Y), p(Y,Z)."));
        REQUIRE(
            "x.\n"
            "y.\n"
            "z.\n" == ground(
                "x :- y, z."
                "x :- not v."
                "y."
                "z."
                "v :- not x."));
        REQUIRE(
            "p(f(1)).\n"
            "p(f(2)).\n"
            "p(g(1)):-not p(f(3)).\n"
            "p(g(2)):-not p(f(3)).\n" == ground(
                "p(f(1)).\n"
                "p(f(2)).\n"
                "p(Y):-p(f(X)),Y=g(X),not p(f(3)).\n"));
        REQUIRE(
            "p(f(1)).\n"
            "p(f(2)).\n"
            "p(g(1)).\n"
            "p(g(2)).\n" == ground(
                "p(f(1)).\n"
                "p(f(2)).\n"
                "p(g(X)):-p(f(X)),not p(f(3)).\n"));
        REQUIRE(
            "0>=#count{0,a:#true}.\n"
            "a.\n" == ground(
                "a.\n"
                "0 { a } 0 :- a, #false : #false.\n"));
    }

    SECTION("bodyaggregate") {
        REQUIRE(
            "company(c1).\n" "company(c2).\n" "company(c3).\n" "company(c4).\n"
            "controls(c1,c2).\n" "controls(c1,c3).\n" "controls(c1,c4).\n" "controls(c3,c4).\n"
            "owns(c1,c2,60).\n" "owns(c1,c3,20).\n" "owns(c2,c3,35).\n" "owns(c3,c4,51).\n" == ground(
                "controls(X,Y) :- 51 #sum+ { S: owns(X,Y,S); S: owns(Z,Y,S), controls(X,Z), X != Y }, company(X), company(Y), X != Y."
                "company(c1)."
                "company(c2)."
                "company(c3)."
                "company(c4)."
                "owns(c1,c2,60)."
                "owns(c1,c3,20)."
                "owns(c2,c3,35)."
                "owns(c3,c4,51)."));
    }

    SECTION("bodyaggregate2") {
        REQUIRE(
            "company(c1).\n" "company(c2).\n" "company(c3).\n" "company(c4).\n"
            "controls(c1,c2):-#sum{60:}.\n"
            "controls(c1,c3):-51<=#sum{20:;35:controls(c1,c2)}.\n"
            "controls(c1,c4):-51<=#sum{51:controls(c1,c3)}.\n"
            "controls(c3,c4):-#sum{51:}.\n"
            "owns(c1,c2,60).\n" "owns(c1,c3,20).\n" "owns(c2,c3,35).\n" "owns(c3,c4,51).\n" == ground(
                "controls(X,Y) :- 51 #sum { S: owns(X,Y,S); S: owns(Z,Y,S), controls(X,Z), X != Y }, company(X), company(Y), X != Y."
                "company(c1)."
                "company(c2)."
                "company(c3)."
                "company(c4)."
                "owns(c1,c2,60)."
                "owns(c1,c3,20)."
                "owns(c2,c3,35)."
                "owns(c3,c4,51)."));
    }

    SECTION("bodyaggregate3") {
        REQUIRE(
            "a:-not c,#true.\n"
            "c.\n" == ground(
                "a :- not { c } >= 1, not c."
                "b :- a, #false."
                "c :- not b, {b; not b} >= 1."
                ));
        REQUIRE(
            "a:-not c,#false.\n"
            "c.\n" == ground(
                "a :- not not { c } >= 1, not c."
                "b :- a, #false."
                "c :- not b, {b; not b} >= 1."
                ));
    }

    SECTION("min_fail") {
        REQUIRE(
            "c.\n"
            "p(1):-not r(1).\n"
            "p(2):-not r(2).\n"
            "r(1):-not p(1).\n"
            "r(2):-not p(2).\n"
            "-:27:42-43: info: atom does not occur in any rule head:\n  b\n"
            == ground(
                "p(X) :- not r(X), X=1..2.\n"
                "r(X) :- not p(X), X=1..2.\n"
                "c.\n"
                "fmin(1)  :-            1 #max {}.\n"
                "fmin(1)  :- not not    1 #max {}.\n"
                "fmin(2)  :- not          #max {} 1.\n"
                "fmin(3)  :-              #min {} 1.\n"
                "fmin(3)  :- not not      #min {} 1.\n"
                "fmin(4)  :- not        1 #min {}.\n"
                "fmin(5)  :- not        1 #max {1:c} 2.\n"
                "fmin(5)  :-            1 #max {c:c} 2.\n"
                "fmin(5)  :- not not    1 #max {c:c} 2.\n"
                "fmin(6)  :- not        1 #min {1:c} 2.\n"
                "fmin(6)  :-            1 #min {c:c} 2.\n"
                "fmin(6)  :- not not    1 #min {c:c} 2.\n"
                "fmin(7)  :-            3 #max {2:p(1); 1:r(1)}.\n"
                "fmin(7)  :- not not    3 #max {2:p(1); 1:r(1)}.\n"
                "fmin(8)  :-              #min {2:p(1); 1:r(1)} 0.\n"
                "fmin(8)  :- not not      #min {2:p(1); 1:r(1)} 0.\n"
                "fmin(9)  :- not          #max {2:p(1); 1:r(1)} 2.\n"
                "fmin(10) :- not        0 #min {2:p(1); 1:r(1)}.\n"
                "fmin(11) :-            1 #max {2:p(1); 1:r(1); 3:c} 1.\n"
                "fmin(11) :- not not    1 #max {2:p(1); 1:r(1); 3:c} 1.\n"
                "fmin(12) :- not        1 #max {2:p(1); 1:r(1); 3:c}.\n"
                "fmin(13) :-            3 #min {2:p(1); 1:r(1); 1,c:c} 3.\n"
                "fmin(13) :- not not    3 #min {2:p(1); 1:r(1); 1,c:c} 3.\n"
                "fmin(14) :- not          #min {2:p(1); 3:b;    1:c} 1.\n"
                ));
    }

    SECTION("min_true") {
        REQUIRE(
            "c.\n"
            "p(1):-not r(1).\n"
            "p(2):-not r(2).\n"
            "r(1):-not p(1).\n"
            "r(2):-not p(2).\n"
            "tmin(1).\n"  "tmin(10).\n" "tmin(11).\n" "tmin(12).\n"
            "tmin(13).\n" "tmin(14).\n" "tmin(15).\n" "tmin(16).\n"
            "tmin(17).\n" "tmin(18).\n" "tmin(19).\n" "tmin(2).\n"
            "tmin(20).\n" "tmin(21).\n" "tmin(22).\n" "tmin(23).\n"
            "tmin(24).\n" "tmin(3).\n"  "tmin(4).\n"  "tmin(5).\n"
            "tmin(6).\n"  "tmin(7).\n"  "tmin(8).\n"  "tmin(9).\n"
            "-:26:39-40: info: atom does not occur in any rule head:\n  b\n"
            "-:27:39-40: info: atom does not occur in any rule head:\n  b\n"
            == ground(
                "p(X) :- not r(X), X=1..2.\n"
                "r(X) :- not p(X), X=1..2.\n"
                "c.\n"
                "tmin(1)  :-     not 1 #max {}.\n"
                "tmin(2)  :-           #max {} 1.\n"
                "tmin(3)  :- not not   #max {} 1.\n"
                "tmin(4)  :- not       #min {} 1.\n"
                "tmin(5)  :-         1 #min {}.\n"
                "tmin(6)  :- not not 1 #min {}.\n"
                "tmin(7)  :-         1 #max {1:c} 2.\n"
                "tmin(8)  :- not not 1 #max {1:c} 2.\n"
                "tmin(9)  :- not     1 #max {c:c} 2.\n"
                "tmin(10) :-         1 #min {1:c} 2.\n"
                "tmin(11) :- not not 1 #min {1:c} 2.\n"
                "tmin(12) :- not     1 #min {c:c} 2.\n"
                "tmin(13) :- not     3 #max {2:p(1); 1:r(1)}.\n"
                "tmin(14) :- not       #min {2:p(1); 1:r(1)} 0.\n"
                "tmin(15) :-           #max {2:p(1); 1:r(1)} 2.\n"
                "tmin(16) :- not not   #max {2:p(1); 1:r(1)} 2.\n"
                "tmin(17) :-         0 #min {2:p(1); 1:r(1)}.\n"
                "tmin(18) :- not not 0 #min {2:p(1); 1:r(1)}.\n"
                "tmin(19) :- not     1 #max {2:p(1); 1:r(1); 3:c} 1.\n"
                "tmin(20) :-         1 #max {2:p(1); 1:r(1); 3:c}.\n"
                "tmin(21) :- not not 1 #max {2:p(1); 1:r(1); 3:c}.\n"
                "tmin(22) :- not     3 #min {2:p(1); 1:r(1); 1,c:c} 3.\n"
                "tmin(23) :-           #min {2:p(1); 3:b;    1:c} 1.\n"
                "tmin(24) :- not not   #min {2:p(1); 3:b;    1:c} 1.\n"
                ));
    }

    SECTION("min_open") {
        REQUIRE(
                "c.\n"
                "mfmin(10):-3<=#min{2:p(1);1:r(1)}<=3.\n"
                "mfmin(5):-0<=#max{2:p(1);1:r(1)}<=0.\n"
                "mmin(1):-2<=#max{2:p(1);1:r(1)}.\n"
                "mmin(11):-1>=#min{1:p(1);2:p(2)}.\n"
                "mmin(12,1):-0>=#min{0:r(1)}.\n"
                "mmin(12,2):-1>=#min{1:r(2)}.\n"
                "mmin(2):-1>=#max{2:p(1);1:r(1)}.\n"
                "mmin(3):-1<=#max{2:p(1);1:r(1)}<=1.\n"
                "mmin(4):-1<=#max{2:p(1);1:r(1)}.\n"
                "mmin(6):-2<=#min{2:p(1);1:r(1)}.\n"
                "mmin(7):-1>=#min{2:p(1);1:r(1)}.\n"
                "mmin(8):-1>=#min{2:p(1);1:r(1)}.\n"
                "mmin(9):-2>=#min{2:p(1);1:r(1)}.\n"
                "p(1):-not r(1).\n"
                "p(2):-not r(2).\n"
                "r(1):-not p(1).\n"
                "r(2):-not p(2).\n" ==
            ground(
                "p(X) :- not r(X), X=1..2.\n"
                "r(X) :- not p(X), X=1..2.\n"
                "c.\n"
                "mmin(1)    :-   2 #max {2:p(1); 1:r(1)}.\n"
                "mmin(2)    :-     #max {2:p(1); 1:r(1)} 1.\n"
                "mmin(3)    :-   1 #max {2:p(1); 1:r(1), c} 1.\n"
                "mmin(4)    :-   1 #max {2:p(1); 1:r(1)} 2.\n"
                "mfmin(5)   :-   0 #max {2:p(1); 1:r(1)} 0.\n"
                "mmin(6)    :-   2 #min {2:p(1); 1:r(1)}.\n"
                "mmin(7)    :-     #min {2:p(1); 1:r(1)} 1.\n"
                "mmin(8)    :-   1 #min {2:p(1); 1:r(1), c} 1.\n"
                "mmin(9)    :-   1 #min {2:p(1); 1:r(1)} 2.\n"
                "mfmin(10)  :-   3 #min {2:p(1); 1:r(1)} 3.\n"
                "mmin(11)   :-   1 #min {X:p(X),X=1..2} 1.\n"
                "mmin(12,X) :- X-1 #min {X-1:r(X)} X-1, X=1..2.\n"
                ));
    }

    SECTION("assign_min") {
        REQUIRE(
            "p(-1).\n"
            "p(-2).\n"
            "p(-3).\n"
            "p(0).\n"
            "p(1).\n"
            "p(2).\n"
            "p(3).\n"
            "p(4).\n"
            "s(-3).\n" == ground(
                "p(X) :- X=-3..4.\n"
                "s(S) :- S=#min { X:p(X) }.\n"
                ));
    }

    SECTION("assign_max") {
        REQUIRE(
            "p(-1).\n"
            "p(-2).\n"
            "p(-3).\n"
            "p(0).\n"
            "p(1).\n"
            "p(2).\n"
            "p(3).\n"
            "p(4).\n"
            "s(4).\n" == ground(
                "p(X) :- X=-3..4.\n"
                "s(S) :- S=#max { X:p(X) }.\n"
                ));
    }

    SECTION("assign_sump") {
        REQUIRE(
            "p(-1).\n"
            "p(-2).\n"
            "p(-3).\n"
            "p(0).\n"
            "p(1).\n"
            "p(2).\n"
            "p(3).\n"
            "p(4).\n"
            "s(10).\n"
            "-:2:19-20: info: tuple ignored:\n  -3\n"
            "-:2:19-20: info: tuple ignored:\n  -2\n"
            "-:2:19-20: info: tuple ignored:\n  -1\n"
            == ground(
                "p(X) :- X=-3..4.\n"
                "s(S) :- S=#sum+ { X:p(X) }.\n"
                ));
    }

    SECTION("assign_count") {
        REQUIRE(
            "p(1).\n"
            "p(2).\n"
            "p(3).\n"
            "p(4).\n"
            "p(5).\n"
            "p(6).\n"
            "p(7).\n"
            "s(1,8).\n"
            "s(2,8).\n"
            "s(3,8).\n" == ground(
                "p(X) :- X=1..7.\n"
                "s(Y,S) :- S=#count { X,a:p(X); Y,b }, Y=1..3.\n"
                ));
    }

    SECTION("assign_sum") {
        REQUIRE(
            "p(1).\n"
            "p(2).\n"
            "p(3).\n"
            "p(4).\n"
            "p(5).\n"
            "p(6).\n"
            "p(7).\n"
            "s(1,29).\n"
            "s(2,30).\n"
            "s(3,31).\n" == ground(
                "p(X) :- X=1..7.\n"
                "s(Y,S) :- S=#sum { X,a:p(X); Y,b }, Y=1..3.\n"
                ));
        REQUIRE(
            "p(-1).\n"
            "p(-2).\n"
            "p(-3).\n"
            "p(0).\n"
            "p(1).\n"
            "p(2).\n"
            "p(3).\n"
            "p(4).\n"
            "s(4).\n" == ground(
                "p(X) :- X=-3..4.\n"
                "s(S) :- S=#sum { X:p(X) }.\n"
                ));
    }

    SECTION("rec_count") {
        REQUIRE(
            "edge(1,3).\n" "edge(2,3).\n" "edge(3,4).\n" "edge(3,6).\n" "edge(4,5).\n" "edge(4,6).\n" "edge(6,7).\n" "edge(6,8).\n"
            "inlink(3,2).\n" "inlink(4,3).\n" "inlink(6,4).\n"
            "inner(3).\n" "inner(4).\n" "inner(6).\n"
            "irreducible(3,s(s(s(z)))).\n" "irreducible(3,s(s(z))).\n" "irreducible(3,s(z)).\n" "irreducible(6,s(z)).\n"
            "link(1,3).\n" "link(2,3).\n" "link(3,4).\n" "link(3,6).\n" "link(4,5).\n" "link(4,6).\n" "link(6,7).\n" "link(6,8).\n"
            "linked(1,3).\n" "linked(1,3,s(s(s(z)))).\n" "linked(1,3,s(s(z))).\n" "linked(1,3,s(z)).\n" "linked(1,4).\n" "linked(1,6).\n"
            "linked(2,3).\n" "linked(2,3,s(s(s(z)))).\n" "linked(2,3,s(s(z))).\n" "linked(2,3,s(z)).\n" "linked(2,4).\n" "linked(2,6).\n"
            "linked(3,4).\n" "linked(3,4,s(z)).\n" "linked(3,5).\n" "linked(3,5,s(s(s(z)))).\n" "linked(3,5,s(s(z))).\n" "linked(3,6).\n"
            "linked(3,6,s(s(z))).\n" "linked(3,6,s(z)).\n" "linked(3,7).\n" "linked(3,7,s(s(s(z)))).\n" "linked(3,8).\n" "linked(3,8,s(s(s(z)))).\n"
            "linked(4,5).\n" "linked(4,5,s(z)).\n" "linked(4,6).\n" "linked(4,6,s(z)).\n" "linked(4,7).\n" "linked(4,8).\n" "linked(6,7).\n"
            "linked(6,7,s(s(z))).\n" "linked(6,7,s(z)).\n" "linked(6,8).\n" "linked(6,8,s(s(z))).\n" "linked(6,8,s(z)).\n"
            "oulink(3,5).\n" "oulink(4,4).\n" "oulink(6,2).\n"
            "outedge(1,3).\n" "outedge(2,3).\n" "outedge(3,5).\n" "outedge(3,7).\n" "outedge(3,8).\n"
            "outvertex(1).\n" "outvertex(2).\n" "outvertex(3).\n" "outvertex(5).\n" "outvertex(7).\n" "outvertex(8).\n"
            "prefer(3,0).\n" "prefer(4,1).\n" "prefer(6,2).\n"
            "reduced(4,s(z)).\n" "reduced(6,s(s(z))).\n" "reduced(s(s(z))).\n" "reduced(s(z)).\n"
            "reducible(4,s(z)).\n" "reducible(6,s(s(z))).\n" "unlinked(1,4,s(z)).\n"
            "unlinked(1,6,s(s(z))).\n" "unlinked(1,6,s(z)).\n" "unlinked(2,4,s(z)).\n" "unlinked(2,6,s(s(z))).\n" "unlinked(2,6,s(z)).\n"
            "unlinked(3,4,s(s(s(z)))).\n" "unlinked(3,4,s(s(z))).\n" "unlinked(3,5,s(z)).\n" "unlinked(3,6,s(s(s(z)))).\n" "unlinked(3,7,s(s(z))).\n"
            "unlinked(3,7,s(z)).\n" "unlinked(3,8,s(s(z))).\n" "unlinked(3,8,s(z)).\n" "unlinked(4,6,s(s(z))).\n" "unlinked(4,7,s(z)).\n" "unlinked(4,8,s(z)).\n"
            "unreduced(3,s(s(s(z)))).\n" "unreduced(3,s(s(z))).\n" "unreduced(3,s(z)).\n" "unreduced(4,s(z)).\n" "unreduced(6,s(s(z))).\n" "unreduced(6,s(z)).\n"
            "vertex(1).\n" "vertex(2).\n" "vertex(3).\n" "vertex(4).\n" "vertex(5).\n" "vertex(6).\n" "vertex(7).\n" "vertex(8).\n" == ground(
                "%%%%%%%%%%%%%%%%%%%%\n"
                "% INPUT PREDICATES %\n"
                "%%%%%%%%%%%%%%%%%%%%\n"
                "\n"
                "vertex(1). vertex(2). vertex(3). vertex(4). vertex(5). vertex(6). vertex(7). vertex(8).\n"
                "edge(1,3). edge(2,3). edge(3,4). edge(3,6). edge(4,5). edge(4,6). edge(6,7). edge(6,8).\n"
                "\n"
                "% 1-+ 4-+-5\n"
                "%   | | |\n"
                "%   3-+-6-+-7\n"
                "%   |     |\n"
                "% 2-+     8\n"
                "\n"
                "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%\n"
                "% INNER VERTICES WITHOUT SELF-LOOP ARE REDUCIBLE %\n"
                "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%\n"
                "\n"
                "inner(V) :- edge(U,V), edge(V,W), not edge(V,V).\n"
                "\n"
                "link(U,V) :- edge(U,V), 1 #sum+{ 1 : inner(U); 1 : inner(V) }.\n"
                "\n"
                "linked(U,V) :- link(U,V).\n"
                "linked(U,V) :- link(U,W), linked(W,V), inner(W), 1 #sum+ { 1 : inner(U); 1 : inner(V) }.\n"
                "\n"
                "inlink(V,N) :- inner(V), N = #sum+{ 1,U : linked(U,V), U != V }.\n"
                "oulink(V,N) :- inner(V), N = #sum+{ 1,W : linked(V,W), V != W }.\n"
                "\n"
                "prefer(V,N) :- inner(V), N = #sum+{ 1,U : inner(U), U < V }.\n"
                "\n"
                "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%\n"
                "% ITERATIVELY REDUCE INNER VERTICES AS LONG AS SOME HAS ONE PREDECESSOR/SUCCESSOR %\n"
                "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%\n"
                "\n"
                "unreduced(V,s(z)) :- inner(V).\n"
                "unreduced(V,s(I)) :- unreduced(V,I), reduced(W,I), V != W.\n"
                "\n"
                "linked(U,V,s(z)) :- link(U,V).\n"
                "linked(U,V,s(I)) :- linked(U,W,I), linked(W,V,I), reduced(W,I).\n"
                "linked(U,V,s(I)) :- linked(U,V,I), reduced(I),\n"
                "         2 #sum+{ 1,a : unreduced(U,s(I)); 1,b : unreduced(V,s(I)); 1,a : not inner(U); 1,b : not inner(V) }.\n"
                "\n"
                "irreducible(V,I) :- unreduced(V,I), linked(V,V,I).\n"
                "irreducible(V,I) :- unreduced(V,I),\n"
                "         2 #sum+{ 1,U : linked(U,V,I), U != V }, 2 #sum+{ 1,W : linked(V,W,I), V != W }.\n"
                "\n"
                "unlinked(U,V,s(z)) :- linked(U,V), not link(U,V).\n"
                "unlinked(U,V,s(I)) :- linked(U,V,I), reduced(U,I), inner(V).\n"
                "unlinked(U,V,s(I)) :- linked(U,V,I), reduced(V,I), inner(U).\n"
                "unlinked(U,V,s(I)) :- unlinked(U,V,I), reduced(U,J), J <= I, unreduced(V,s(I)).\n"
                "unlinked(U,V,s(I)) :- unlinked(U,V,I), reduced(V,J), J <= I, unreduced(U,s(I)).\n"
                "unlinked(U,V,s(I)) :- unlinked(U,V,I), reduced(W,I), U != W, V != W,\n"
                "         1 #sum+{ 1 : unlinked(U,W,I); 1 : unlinked(W,V,I); 1 : not linked(U,W); 1 : not linked(W,V) }.\n"
                "\n"
                "reducible(V,I) :- unreduced(V,I), inlink(V,N),\n"
                "         N #sum+{ 1; 1,U : unlinked(U,V,I), U != V }, 1 #sum+{ 1 : unlinked(V,V,I); 1 : not linked(V,V) }.\n"
                "reducible(V,I) :- unreduced(V,I), oulink(V,N),\n"
                "         N #sum+{ 1; 1,W : unlinked(V,W,I), V != W }, 1 #sum+{ 1 : unlinked(V,V,I); 1 : not linked(V,V) }.\n"
                "\n"
                "reduced(V,I) :- reducible(V,I), prefer(V,N),\n"
                "         N #sum+{ 1,U : irreducible(U,I), U < V; 1,W,J : reduced(W,J), W < V, J < I }.\n"
                "reduced(I)   :- reduced(V,I).\n"
                "\n"
                "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%\n"
                "% FINAL RESULT OF ITERATIVE REDUCTION %\n"
                "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%\n"
                "\n"
                "outvertex(V) :- vertex(V), not inner(V).\n"
                "outvertex(V) :- unreduced(V,I), not reduced(I).\n"
                "\n"
                "outedge(U,V) :- edge(U,V), not link(U,V).\n"
                "outedge(U,V) :- linked(U,V,I), not reduced(I).\n"));
    }

    SECTION("rec_count2") {
        REQUIRE("a:-1!=#count{0,a:a}.\n" == ground("a:-{a}!=1.\n"));
    }

    SECTION("rec_conj") {
        REQUIRE(
            "edge(2,3).\n" "edge(2,5).\n" "edge(3,1).\n" "edge(3,4).\n"
            "edge(3,6).\n" "edge(7,8).\n" "edge(8,9).\n"
            "input(1).\n" "input(3).\n" "input(4).\n" "input(6).\n" "input(7).\n" "input(8).\n"
            "observed(5).\n" "observed(9).\n" == ground(
                "% example instance\n"
                "\n"
                "edge(2,3).\n"
                "edge(3,1). edge(3,4). edge(3,6).\n"
                "edge(2,5).\n"
                "edge(7,8).\n"
                "edge(8,9).\n"
                "\n"
                "input(7).\n"
                "observed(5). observed(9).\n"
                "\n"
                "% a vertex is an input vertex if all its outgoing edges\n"
                "% lead to input vertices and the vertex itself is no observation\n"
                "% example:\n"
                "% edges - from left to right\n"
                "% x     - some vertex\n"
                "% i     - input vertex\n"
                "% o     - observation vertex\n"
                "%\n"
                "%       i\n"
                "%      /\n"
                "% x---i---i\n"
                "%  \\   \\\n"
                "%   o   i\n"
                "input(V) :- edge(U,V), input(W) : edge(V,W); not observed(V).\n"
                "\n"
                "% An unobserved input vertex can explain any single non-input target.\n"
                "% example:\n"
                "% edges - from left to right\n"
                "% (i)   - explicitly marked input vertex\n"
                "%  i    - input vertex\n"
                "%  o    - observation vertex\n"
                "%\n"
                "% (i)---i---o\n"
                "input(V) :- input(U); edge(U,V); input(W) : edge(U,W), V != W; not observed(U), not observed(V).\n"));
    }

    SECTION("conj") {
        REQUIRE(
            "p(1).\n"
            "p(2).\n"
            "p(3).\n"
            "p(4).\n"
            "p(5).\n"
            "p(6).\n"
            "p(7).\n"
            "s(1,2).\n"
            "s(2,3).\n"
            "s(3,4).\n"
            "s(4,5).\n"
            "s(5,6).\n"
            "s(6,7).\n" == ground(
                "p(1..7).\n"
                "s(X,Z) :- p(X), #false : p(Y), X < Y, Y < Z; p(Z), X < Z.\n"));
        REQUIRE(
            "p(1):-not not p(1).\n"
            "p(2):-not not p(2).\n"
            "p(3):-not not p(3).\n"
            "p(4):-not not p(4).\n"
            "s(1,2):-p(2),p(1).\n"
            "s(1,3):-p(3),p(1),#false:p(2).\n"
            "s(1,4):-p(4),p(1),#false:p(2)|p(3).\n"
            "s(2,3):-p(3),p(2).\n"
            "s(2,4):-p(4),p(2),#false:p(3).\n"
            "s(3,4):-p(4),p(3).\n" == ground(
                "p(X) :- not not p(X), X=1..4.\n"
                "s(X,Z) :- p(X), #false : p(Y), X < Y, Y < Z; p(Z), X < Z.\n"));
        REQUIRE(
            "p(1):-not not p(1).\n"
            "p(2):-not not p(2).\n"
            "p(3):-not not p(3).\n"
            "p(4):-not not p(4).\n"
            "s(1,2):-p(2),p(1).\n"
            "s(1,3):-p(3),p(1),not p(2):p(2).\n"
            "s(1,4):-p(4),p(1),not p(2):p(2);not p(3):p(3).\n"
            "s(2,3):-p(3),p(2).\n"
            "s(2,4):-p(4),p(2),not p(3):p(3).\n"
            "s(3,4):-p(4),p(3).\n" == ground(
                "p(X) :- not not p(X), X=1..4.\n"
                "s(X,Z) :- p(X), not p(Y) : p(Y), X < Y, Y < Z; p(Z), X < Z.\n"
            ));
    }

    SECTION("meta") {
        REQUIRE(
            "hold(atom(p)):-hold(conjunction(0)).\n"
            "hold(atom(q)):-hold(conjunction(2)).\n"
            "hold(atom(r)):-hold(conjunction(2)).\n"
            "hold(atom(s)):-hold(conjunction(1)).\n"
            "hold(atom(t)):-hold(conjunction(0)).\n"
            "hold(conjunction(0)):-hold(sum(1,0,2)).\n"
            "hold(conjunction(1)):-not hold(atom(q)),not hold(atom(r)).\n"
            "hold(conjunction(2)):-hold(sum(1,1,2)).\n"
            "hold(conjunction(3)):-hold(atom(r)),hold(atom(q)).\n"
            "hold(sum(1,0,2)):-1<=#sum{1,2:not hold(atom(t));1,1:hold(atom(s));1,0:hold(atom(r))}<=2.\n"
            "hold(sum(1,1,2)):-1<=#sum{1,1:hold(atom(t));1,0:hold(atom(p))}.\n" == ground(
                "wlist(0,0,pos(atom(r)),1).\n"
                "wlist(0,1,pos(atom(s)),1).\n"
                "wlist(0,2,neg(atom(t)),1).\n"
                "set(0,pos(sum(1,0,2))).\n"
                "rule(pos(atom(p)),pos(conjunction(0))).\n"
                "rule(pos(atom(t)),pos(conjunction(0))).\n"
                "set(1,neg(atom(q))).\n"
                "set(1,neg(atom(r))).\n"
                "rule(pos(atom(s)),pos(conjunction(1))).\n"
                "wlist(1,0,pos(atom(p)),1).\n"
                "wlist(1,1,pos(atom(t)),1).\n"
                "set(2,pos(sum(1,1,2))).\n"
                "rule(pos(atom(q)),pos(conjunction(2))).\n"
                "rule(pos(atom(r)),pos(conjunction(2))).\n"
                "set(3,pos(atom(r))).\n"
                "set(3,pos(atom(q))).\n"
                "rule(pos(false),pos(conjunction(3))).\n"
                "scc(0,pos(atom(t))).\n"
                "scc(0,pos(atom(r))).\n"
                "scc(0,pos(sum(1,0,2))).\n"
                "scc(0,pos(conjunction(0))).\n"
                "scc(0,pos(atom(p))).\n"
                "scc(0,pos(sum(1,1,2))).\n"
                "scc(0,pos(conjunction(2))).\n"
                "\n"
                "litb(B) :- rule(_,B).\n"
                "litb(E) :- litb(pos(conjunction(S))), set(S,E).\n"
                "litb(E) :- eleb(sum(_,S,_)), wlist(S,_,E,_).\n"
                "litb(E) :- eleb(min(_,S,_)), wlist(S,_,E,_).\n"
                "litb(E) :- eleb(max(_,S,_)), wlist(S,_,E,_).\n"
                "\n"
                "eleb(P) :- litb(pos(P)).\n"
                "eleb(N) :- litb(neg(N)).\n"
                "\n"
                "hold(conjunction(S)) :- eleb(conjunction(S)),\n"
                "      hold(P) : set(S,pos(P));\n"
                "  not hold(N) : set(S,neg(N)).\n"
                "hold(sum(L,S,U))     :- eleb(sum(L,S,U)),\n"
                "  L #sum { W,Q :     hold(P), wlist(S,Q,pos(P),W);\n"
                "           W,Q : not hold(N), wlist(S,Q,neg(N),W) } U.\n"
                "hold(min(L,S,U))     :-  eleb(min(L,S,U)),\n"
                "  L #max { W,Q :     hold(P), wlist(S,Q,pos(P),W);\n"
                "           W,Q : not hold(N), wlist(S,Q,neg(N),W) } U.\n"
                "hold(max(L,S,U))     :-  eleb(max(L,S,U)),\n"
                "  L #min { W,Q :     hold(P), wlist(S,Q,pos(P),W);\n"
                "           W,Q : not hold(N), wlist(S,Q,neg(N),W) } U.\n"
                "\n"
                "hold(atom(A)) :- rule(pos(atom(A)), pos(B)), hold(B).\n", {"hold("}));
    }

    SECTION("head") {
        REQUIRE(
            "2<=#count{q,1:q(1);q,2:q(2);q,3:q(3);r,3:r(3);r,3:#true:not r(3);r,4:r(4);r,4:#true:not r(4);r,5:r(5);r,5:#true:not r(5)}<=4.\n"
            "out(q(1)):-q(1).\n" "out(q(2)):-q(2).\n" "out(q(3)):-q(3).\n"
            "out(r(3)):-r(3).\n" "out(r(4)):-r(4).\n" "out(r(5)):-r(5).\n"
            "p(1).\n" "p(2).\n" "p(3).\n" "p(4).\n" "p(5).\n" == ground(
                "p(1..5).\n"
                "2 #count { q,X : q(X) : p(X), X <= 3; r,X : r(X) : p(X), X >= 3; r,X : not r(X) : p(X), X >= 3 } 4.\n"
                "out(q(X)):-q(X).\n"
                "out(r(X)):-r(X).\n"));
    }

    SECTION("head2") {
        REQUIRE(
            "#count{0,d:d;0,e:e}.\n"
            "a.\n"
            "b.\n"
            "c.\n"
            "f:-d,e.\n" == ground(
                "a.\n"
                "b.\n"
                "c.\n"
                "{d:a;e:b}:-c.\n"
                "f:-e,d.\n"
            ));
        REQUIRE(
            "#count{0,r(a):#true}:-r(d).\n"
            "#count{0,r(b):r(b)}:-r(e).\n"
            "#count{0,r(e):r(e)}:-r(b).\n"
            "1>=#count{0,r(b):r(b);0,r(c):r(c)}.\n"
            "1>=#count{0,r(d):r(d);0,r(e):r(e)}:-r(c).\n"
            "p(a,b).\n"
            "p(a,c).\n"
            "p(b,e).\n"
            "p(c,d).\n"
            "p(c,e).\n"
            "p(d,a).\n"
            "p(e,b).\n"
            "r(a).\n" == ground(
                "p(a,b).\n"
                "p(a,c).\n"
                "p(c,d).\n"
                "p(c,e).\n"
                "p(d,a).\n"
                "p(b,e).\n"
                "p(e,b).\n"
                "\n"
                "r(a).\n"
                "{ r(Y) : p(X,Y) } 1 :- r(X).\n"
            ));
        REQUIRE(
            "a.\n"
            "b.\n"
            "c.\n"
            "d;e.\n"
            "f:-d,e.\n" == ground(
                "a.\n"
                "b.\n"
                "c.\n"
                "d:a | e:b :- c.\n"
                "f :- e;d.\n"
            ));
        REQUIRE(
            "#true:-r(d).\n"
            "p(a,b).\n"
            "p(a,c).\n"
            "p(b,e).\n"
            "p(c,d).\n"
            "p(c,e).\n"
            "p(d,a).\n"
            "p(e,b).\n"
            "r(a).\n"
            "r(b):-r(e).\n"
            "r(b);r(c).\n"
            "r(d);r(e):-r(c).\n"
            "r(e):-r(b).\n" == ground(
                "p(a,b).\n"
                "p(a,c).\n"
                "p(c,d).\n"
                "p(c,e).\n"
                "p(d,a).\n"
                "p(b,e).\n"
                "p(e,b).\n"
                "\n"
                "r(a).\n"
                "r(Y) : p(X,Y) :- r(X).\n"
            ));
    }

    SECTION("disjunction") {
        REQUIRE(
            "out(q(1)):-q(1).\n" "out(q(2)):-q(2).\n" "out(q(3)):-q(3).\n"
            "out(r(3)):-r(3).\n" "out(r(4)):-r(4).\n" "out(r(5)):-r(5).\n"
            "p(1).\n" "p(2).\n" "p(3).\n" "p(4).\n" "p(5).\n"
            "q(1);q(2);q(3);r(3);r(4);r(5);not r(3);not r(4);not r(5).\n" == ground(
                "p(1..5).\n"
                "q(X) : p(X), X <= 3; r(X) : p(X), X >= 3; not r(X) : p(X), X >= 3.\n"
                "out(q(X)):-q(X).\n"
                "out(r(X)):-r(X).\n"));
    }

    SECTION("gbie") {
        REQUIRE("gt(-3,-4).\n" == ground(gbie()+gbie1(), {"gt(", "le("}));
        REQUIRE("le(-18,-16).\n" == ground(gbie()+gbie2(), {"gt(", "le("}));
    }

    SECTION("strategicA") {
        REQUIRE(
            "strategic(1):-strategic(2),strategic(2),strategic(5).\n"
            "strategic(1):-strategic(3),strategic(3),strategic(5).\n"
            "strategic(1);strategic(1).\n"
            "strategic(1);strategic(5).\n"
            "strategic(2):-strategic(1),strategic(3),strategic(6).\n"
            "strategic(2):-strategic(5),strategic(3),strategic(6).\n"
            "strategic(2);strategic(4).\n"
            "strategic(2);strategic(4).\n"
            "strategic(2);strategic(5).\n"
            "strategic(2);strategic(6).\n"
            "strategic(3):-strategic(5),strategic(1),strategic(4).\n"
            "strategic(3):-strategic(6),strategic(1),strategic(4).\n"
            "strategic(3);strategic(4).\n"
            "strategic(3);strategic(4).\n"
            "strategic(4):-strategic(5),strategic(5),strategic(2).\n"
            "strategic(4):-strategic(6),strategic(6),strategic(2).\n"
            "strategic(4);strategic(2).\n"
            "strategic(4);strategic(4).\n"
            "strategic(4);strategic(4).\n"
            "strategic(4);strategic(6).\n"
            "strategic(5):-strategic(1),strategic(1),strategic(1).\n"
            "strategic(5);strategic(2).\n"
            "strategic(5);strategic(2).\n"
            "strategic(5);strategic(5).\n"
            "strategic(5);strategic(5).\n"
            "strategic(6):-strategic(4),strategic(4),strategic(4).\n"
            "strategic(6);strategic(1).\n"
            "strategic(6);strategic(1).\n"
            "strategic(6);strategic(2).\n"
            "strategic(6);strategic(2).\n"
            "strategic(6);strategic(4).\n"
            "strategic(6);strategic(4).\n"
            "strategic(6);strategic(5).\n"
            "strategic(6);strategic(6).\n" == ground(
                strategicA1() +
                "strategic(X2); strategic(X1) :- produced_by(P,X1,X2).\n"
                "strategic(W) :- controlled_by(W,X1,X2,X3), strategic(X1), strategic(X2), strategic(X3).\n", {"strategic("}));
    }

    SECTION("strategicB") {
        REQUIRE(
            "strategic(1).\n"
            "strategic(1):-strategic(5),strategic(2).\n"
            "strategic(1):-strategic(5),strategic(3).\n"
            "strategic(1);strategic(6).\n"
            "strategic(1);strategic(6).\n"
            "strategic(2):-strategic(6),strategic(3),strategic(1).\n"
            "strategic(2):-strategic(6),strategic(3),strategic(5).\n"
            "strategic(2);strategic(4).\n"
            "strategic(2);strategic(5).\n"
            "strategic(2);strategic(5).\n"
            "strategic(2);strategic(6).\n"
            "strategic(2);strategic(6).\n"
            "strategic(3):-strategic(4),strategic(1),strategic(5).\n"
            "strategic(3):-strategic(4),strategic(1),strategic(6).\n"
            "strategic(4).\n"
            "strategic(4).\n"
            "strategic(4):-strategic(2),strategic(5).\n"
            "strategic(4):-strategic(2),strategic(6).\n"
            "strategic(4);strategic(2).\n"
            "strategic(4);strategic(2).\n"
            "strategic(4);strategic(3).\n"
            "strategic(4);strategic(3).\n"
            "strategic(4);strategic(6).\n"
            "strategic(4);strategic(6).\n"
            "strategic(5).\n"
            "strategic(5).\n"
            "strategic(5):-strategic(1).\n"
            "strategic(5);strategic(1).\n"
            "strategic(5);strategic(2).\n"
            "strategic(5);strategic(6).\n"
            "strategic(6).\n"
            "strategic(6):-strategic(4).\n"
            "strategic(6);strategic(2).\n"
            "strategic(6);strategic(4).\n" == ground(
                strategicB1() +
                "strategic(X) : produced_by(P,X) :- produced_by(P,_).\n"
                "strategic(X) :- controlled_by(C,X,_), strategic(Y) : controlled_by(C,X,Y).\n", {"strategic("}));
    }

    SECTION("optimize") {
        REQUIRE(
            ":~p(1).[1@0]\n"
            ":~p(2).[2@0]\n"
            ":~p(3).[3@0]\n"
            ":~p(4).[4@0]\n"
            "{p(1)}.\n"
            "{p(2)}.\n"
            "{p(3)}.\n"
            "{p(4)}.\n" == ground(
                "{p((1..4))}."
                ":~p(X).[X@0]"));
        REQUIRE(
            ":~p(1).[1@0]\n"
            ":~p(2).[2@0]\n"
            ":~p(3).[3@0]\n"
            ":~p(4).[4@0]\n"
            "{p(1)}.\n"
            "{p(2)}.\n"
            "{p(3)}.\n"
            "{p(4)}.\n" == ground(
                "{p((1..4))}."
                "#minimize{X@0:p(X)}."));
        REQUIRE(
            ":~p(1).[-1@0]\n"
            ":~p(2).[-2@0]\n"
            ":~p(3).[-3@0]\n"
            ":~p(4).[-4@0]\n"
            "{p(1)}.\n"
            "{p(2)}.\n"
            "{p(3)}.\n"
            "{p(4)}.\n" == ground(
                "{p((1..4))}."
                "#maximize{X@0:p(X)}."));
        REQUIRE(
            ":~.[1@1]\n"
            ":~p(1),p(1).[1@1,f,g]\n"
            ":~p(1).[1@0]\n"
            ":~p(1).[1@2]\n"
            ":~p(2),p(2).[2@2,f,g]\n"
            ":~p(2).[2@0]\n"
            ":~p(2).[2@2]\n"
            ":~p(3),p(3).[3@3,f,g]\n"
            ":~p(3).[3@0]\n"
            ":~p(3).[3@2]\n"
            ":~p(4),p(4).[4@4,f,g]\n"
            ":~p(4).[4@0]\n"
            ":~p(4).[4@2]\n"
            "{p(1)}.\n"
            "{p(2)}.\n"
            "{p(3)}.\n"
            "{p(4)}.\n"
            "-:1:41-42: info: tuple ignored:\n  f@f\n"
            == ground(
                "{p((1..4))}."
                "#minimize{X:p(X); X@2:p(X); f@f; 1@1; X@X,f,g:p(X),p(X)}."));
    }

    SECTION("neg") {
        REQUIRE(
            "-q(1):-not not -q(1).\n"
            "-q(2):-not not -q(2).\n"
            "-q(3):-not not -q(3).\n"
            "-q(4):-not not -q(4).\n"
            ":-q(1),-q(1).\n"
            ":-q(2),-q(2).\n"
            ":-q(3),-q(3).\n"
            ":-q(4),-q(4).\n"
            "p(1).\n"
            "p(2).\n"
            "p(3).\n"
            "p(4).\n"
            "q(1):-not not q(1).\n"
            "q(2):-not not q(2).\n"
            "q(3):-not not q(3).\n"
            "q(4):-not not q(4).\n" == ground(
                "p(1..4).\n"
                "-q(X):-not not -q(X), p(X).\n"
                " q(X):-not not  q(X), p(X).\n"));
    }
    SECTION("two aggregates") {
        REQUIRE(
            "p:-#count{0,a:a}=0.\n"
            "p:-#count{0,a:a}=1,1<=#count{0,b:b}.\n"
            "{a;b}.\n" == ground(
                "{a;b}.\n"
                "p :- X = { a }, X { b }.\n"));
    }

    SECTION("tuple") {
        REQUIRE("p(((),())).\n" == ground("p(((),())).\n"));
    }

}

} } } // namespace Test Ground Gringo

