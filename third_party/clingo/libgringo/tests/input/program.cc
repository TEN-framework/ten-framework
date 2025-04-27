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
#include "gringo/input/programbuilder.hh"
#include "gringo/input/program.hh"
#include "gringo/output/output.hh"

#include "tests/tests.hh"
#include "tests/term_helper.hh"

namespace Gringo { namespace Input { namespace Test {

using namespace Gringo::IO;
using namespace Gringo::Test;

// {{{ definition of auxiliary functions

namespace {

struct Grounder {
    Grounder()
    : out(td, {}, oss)
    , pb( context, p, out.outPreds, d )
    , ngp( pb, bck, incmode )
    { }
    Gringo::Test::TestGringoModule module;
    Potassco::TheoryData td;
    std::ostringstream oss;
    Output::OutputBase out;
    Gringo::Test::TestContext context;
    Program p;
    Defines d;
    NullBackend bck;
    NongroundProgramBuilder pb;
    NonGroundParser ngp;
    bool incmode{false};

};

std::unique_ptr<Grounder> parse(std::string const &str) {
    std::unique_ptr<Grounder> g = gringo_make_unique<Grounder>();
    g->ngp.pushStream("-", gringo_make_unique<std::stringstream>(str), g->module);
    g->ngp.parse(g->module);
    return g;
}

std::string rewrite(std::unique_ptr<Grounder> g) {
    g->d.init(g->module);
    g->p.rewrite(g->d, g->module);
    auto str(to_string(g->p));
    str.erase(std::remove(str.begin(), str.end(), '\n'), str.end());
    replace_all(str, ";[#inc_base]", "");
    replace_all(str, ":-[#inc_base].", ".");
    replace_all(str, ":-[#inc_base],", ":-");
    replace_all(str, ":-[#inc_base];", ":-");
    replace_all(str, ";  ", ";");
    replace_all(str, "{  ", "{");
    return str;
}

bool check(std::string const &prg, std::string const &messages = "") {
    auto g = parse(prg);
    g->p.rewrite(g->d, g->module);
    g->p.check(g->module);
    if (!messages.empty()) {
        REQUIRE(messages == to_string(g->module));
    }
    return g->module.messages().empty();
}

} // namespace

TEST_CASE("input-program", "[input]") {
    SECTION("rewrite") {
        REQUIRE("p(1):-q.p(2):-q.p(3):-q.p:-q." == rewrite(parse("p(1;2;3;):-q.")));
        REQUIRE("p:-q(1).p:-q(2).p:-q(3).p:-q." == rewrite(parse("p:-q(1;2;3;).")));
        REQUIRE("p(1):-q(3).p(2):-q(3).p(1):-q(4).p(2):-q(4)." == rewrite(parse("p(1;2):-q(3;4).")));
        REQUIRE("p((X+Y)):-q(#Arith0);#Arith0=(X+Y)." == rewrite(parse("p(X+Y):-q(X+Y).")));
        REQUIRE("#Arith0<=#count{(X+Y):q((X+Y)):r(#Arith0),s(#Arith1),#Arith1=(A+B)}:-t(#Arith0);#Arith0=(X+Y);1<=#count{(X+Y):u(#Arith0),v(#Arith2),#Arith2=(A+B)}." == rewrite(parse("X+Y#count{X+Y:q(X+Y):r(X+Y),s(A+B)}:-t(X+Y),1#count{X+Y:u(X+Y),v(A+B)}.")));
        REQUIRE("p((#Range0+0)):-q((#Range1+0));#range(#Range1,A,B);#range(#Range0,X,Y)." == rewrite(parse("p(X..Y):-q(A..B).")));
        REQUIRE("p(1):-q.p(2):-q.p(3):-q.p:-q." == rewrite(parse("p(1;2;3;):-q.")));
        REQUIRE("p(Z):-p(A,B);Y=#count{0,q(B):q(B)};X=#count{0,q(A):q(A)};Z=#count{0,r(X,Y):r(X,Y)}." == rewrite(parse("p(Z):-p(A,B),X={q(A)},Y={q(B)},Z={r(X,Y)}.")));
        REQUIRE("p(Z):-Z=#count{0,p(X):p(X)};Z>0." == rewrite(parse("p(Z):-Z={p(X)},Z>0.")));
        REQUIRE(":~[#inc_p(#Inc0,#Inc1)];0=0.[#Inc0@0,#Inc1]" == rewrite(parse("#program p(k,t). :~ #true. [ k,t ]")));
        REQUIRE("#project p(#Arith0):-[p(#Arith0)];#Arith0=(X+X)." == rewrite(parse("#project p(X+X).")));
        REQUIRE("#project p(X):-[p(X)].#project p(Y):-[p(Y)]." == rewrite(parse("#project p(X;Y).")));
        REQUIRE("#edge(a,b).#edge(c,d)." == rewrite(parse("#edge (a,b;c,d).")));
        REQUIRE("#edge((X+X),b)." == rewrite(parse("#edge (X+X,b).")));
        REQUIRE("#edge((#Range0+0),b):-#range(#Range0,1,10)." == rewrite(parse("#edge (1..10,b).")));
        REQUIRE("#heuristic a(#Arith0)[1@0,sign]:-[a(#Arith0)];#Arith0=(X+X)." == rewrite(parse("#heuristic a(X+X). [1@0,sign]")));
        REQUIRE("#heuristic a[(X+X)@2,sign]:-[a]." == rewrite(parse("#heuristic a. [X+X@2,sign]")));
        REQUIRE("#heuristic a((#Range0+0))[2@0,sign]:-[a((#Range0+0))];#range(#Range0,1,2)." == rewrite(parse("#heuristic a(1..2). [2,sign]")));
        REQUIRE("#theory x{node{};&edge/1:node,head}.#false:-p(Z);not &edge((Z+Z)){(z),(Y): p(Y,#Arith1),#Arith1=(Y+Y)}." == rewrite(parse("#theory x{ node{}; &edge/1: node, head }.&edge(Z+Z) { z, Y : p(Y,Y+Y)} :- p(Z).")));
        REQUIRE("#theory x{node{};&edge/1:node,head}.#false:-p(Z);#range(#Range0,Z,Z);not &edge((#Range0+0)){(z),(Y): p(Y,(#Range1+0)),#range(#Range1,Y,Y)}." == rewrite(parse("#theory x{ node{}; &edge/1: node, head }.&edge(Z..Z) { z, Y : p(Y,Y..Y)} :- p(Z).")));
    }

    SECTION("rewrite-comparisons") {
        // head
        REQUIRE("#void:-1>=2;q.#void:-2>=3;q." == rewrite(parse("1<2<3:-q.")));
        REQUIRE("#void:-2<3;1<2;q." == rewrite(parse("not 1<2<3:-q.")));
        REQUIRE("#void::;#void:::-5<6;4<5;2<3;1<2;q." == rewrite(parse("not 1<2<3|not 4<5<6:-q.")));
        REQUIRE("#void:-#range(#Range0,1,1);#range(#Range0,1,2);q;1>=(#Range0+0)."
                "#void:-#range(#Range0,1,0);#range(#Range0,1,2);q;(#Range0+0)>=3." == rewrite(parse("1<1..2<3:-q.")));
        REQUIRE("#void:-#range(#Range0,2,2);#range(#Range0,1,2);q;1<(#Range0+0);(#Range0+0)<3." == rewrite(parse("not 1<1..2<3:-q.")));
        REQUIRE("#void:-1>=2;q.#void:-2>=4;q.#void:-1>=3;q.#void:-3>=4;q." == rewrite(parse("1<(2;3)<4:-q.")));
        REQUIRE("#void:-2<4;1<2;q.#void:-3<4;1<3;q." == rewrite(parse("not 1<(2;3)<4:-q.")));
        // body
        REQUIRE("q:-2<3;1<2." == rewrite(parse("q :- 1<2<3.")));
        REQUIRE("q:-1>=2."
                "q:-2>=3." == rewrite(parse("q :- not 1<2<3.")));
        REQUIRE("q:-#range(#Range0,2,2);#range(#Range0,1,2);1<(#Range0+0);(#Range0+0)<3." == rewrite(parse("q :- 1<1..2<3.")));
        REQUIRE("q:-#range(#Range0,1,1);#range(#Range0,1,2);1>=(#Range0+0)."
                "q:-#range(#Range0,1,0);#range(#Range0,1,2);(#Range0+0)>=3." == rewrite(parse("q :- not 1<1..2<3.")));
        REQUIRE("q:-2<4;1<2."
                "q:-3<4;1<3." == rewrite(parse("q :- 1<(2;3)<4.")));
        REQUIRE("q:-1>=2."
                "q:-2>=4."
                "q:-1>=3."
                "q:-3>=4." == rewrite(parse("q :- not 1<(2;3)<4.")));
        // disjunctions
        REQUIRE("#void:1>=2&#void:2>=3:;"
                "#void:4>=5&#void:5>=6::-q." == rewrite(parse("1<2<3|4<5<6:-q.")));
        REQUIRE("#void:#range(#Range0,2,3),1>=(#Range0+0)&#void:#range(#Range0,2,3),(#Range0+0)>=4:;"
                "#void:4>=5&#void:5>=7&#void:4>=6&#void:6>=7::-q." == rewrite(parse("1<(2..3)<4|4<(5;6)<7:-q.")));
        REQUIRE("#void::;#void:::-5<6;4<5;2<3;1<2;q." == rewrite(parse("not 1<2<3|not 4<5<6:-q.")));
        REQUIRE("#void:1<2,2<3:b;"
                "#void:4<5,5<6:c:-q." == rewrite(parse("not 1<2<3:b|not 4<5<6:c:-q.")));
        REQUIRE("#void::;#void:4<5,5<7&#void:4<6,6<7:"
                ":-#range(#Range0,2,3);q;1<(#Range0+0);(#Range0+0)<4." == rewrite(parse("not 1<(2..3)<4|not 4<(5;6)<7:-q.")));
        REQUIRE("#void:#range(#Range0,2,3),1<(#Range0+0),(#Range0+0)<4:b;"
                "#void:4<5,5<7&#void:4<6,6<7:c:-q." == rewrite(parse("not 1<(2..3)<4:b|not 4<(5;6)<7:c:-q.")));
        REQUIRE("b::1<2,2<3:-q." == rewrite(parse("b:1<2<3:-q.")));
        REQUIRE("b::1>=2;"
                "b::2>=3:-q." == rewrite(parse("b:not 1<2<3:-q.")));
        // conjunctions
        REQUIRE("h:-1<2&2<3:a." == rewrite(parse("h:-1<2<3:a.")));
        REQUIRE("h:-a:1<2,2<3." == rewrite(parse("h:-a:1<2<3.")));
        REQUIRE("h:-1>=2|2>=3:a." == rewrite(parse("h:-not 1<2<3:a.")));
        REQUIRE("h:-a:2>=3;a:1>=2." == rewrite(parse("h:-a:not 1<2<3.")));
        REQUIRE("h:-1<2&2<4|1<3&3<4:a." == rewrite(parse("h:-1<(2;3)<4:a.")));
        REQUIRE("h:-a:1<3,3<4;a:1<2,2<4." == rewrite(parse("h:-a:1<(2;3)<4.")));
        REQUIRE("h:-1>=2|2>=4|1>=3|3>=4:a." == rewrite(parse("h:-not 1<(2;3)<4:a.")));
        REQUIRE("h:-a:3>=4;a:1>=3;a:2>=4;a:1>=2." == rewrite(parse("h:-a:not 1<(2;3)<4.")));
        REQUIRE("h:-1<(#Range0+0)&(#Range0+0)<4&#range(#Range0,2,3):a." == rewrite(parse("h:-1<(2..3)<4:a.")));
        REQUIRE("h:-a:1<(#Range0+0),(#Range0+0)<4,#range(#Range0,2,3)." == rewrite(parse("h:-a:1<(2..3)<4.")));
        // Note: should be correct
        REQUIRE("h:-1>=(#Range0+0)&#range(#Range0,2,3)|(#Range0+0)>=4&#range(#Range0,2,3):a." == rewrite(parse("h:-not 1<(2..3)<4:a.")));
        REQUIRE("h:-"
                "a:(#Range0+0)>=4,#range(#Range0,2,3),#range(#Range0,1,0);"
                "a:1>=(#Range0+0),#range(#Range0,2,3),#range(#Range0,1,0)." == rewrite(parse("h:-a:not 1<(2..3)<4.")));
        // set based head aggregates
        REQUIRE("1<=#count{3:#void:1<2}:-q." == rewrite(parse("{ 1<2 } >= 1:-q.")));
        REQUIRE("1<=#count{"
                "3:#void:1<2;"
                "4:#void:3<4"
                "}:-q." == rewrite(parse("{ 1<2; 3<4 } >= 1:-q.")));
        REQUIRE("1<=#count{"
                "3:#void:1<2,2<4;"
                "4:#void:1<3,3<4"
                "}:-q." == rewrite(parse("{ 1<(2;3)<4 } >= 1:-q.")));
        REQUIRE("#count{"
                "3:#void:1>=2;"
                "3:#void:2>=4;"
                "4:#void:1>=3;"
                "4:#void:3>=4"
                "}:-q." == rewrite(parse("{ not 1<(2;3)<4 }:-q.")));
        // Note the 3<=3 is the tuple and is meant to handle undefined terms
        REQUIRE("#count{0:#void:}:-6<7;5<6;3<=3;q." == rewrite(parse("{ 5<6<7 }:-q.")));
        // head aggreagets
        REQUIRE("1<=#count{1:#void:p,1<2}:-q." == rewrite(parse("#count{ 1:1<2:p } >= 1:-q.")));
        REQUIRE("1<=#count{1:#void:p,1<2,2<3}:-q." == rewrite(parse("#count{ 1:1<2<3:p } >= 1:-q.")));
        REQUIRE("1<=#count{"
                "1:#void:p,1>=2;"
                "1:#void:p,2>=3"
                "}:-q." == rewrite(parse("#count{ 1:not 1<2<3:p } >= 1:-q.")));
        REQUIRE("1<=#count{"
                "1:#void:p,1<2,2<4;"
                "1:#void:p,1<3,3<4"
                "}:-q." == rewrite(parse("#count{ 1:1<(2;3)<4:p } >= 1:-q.")));
        REQUIRE("1<=#count{"
                "1:#void:p,1>=2;"
                "1:#void:p,2>=4;"
                "1:#void:p,1>=3;"
                "1:#void:p,3>=4"
                "}:-q." == rewrite(parse("#count{ 1:not 1<(2;3)<4:p } >= 1:-q.")));
        REQUIRE("1<=#count{"
                "1:p:1<2,2<4;"
                "1:p:1<3,3<4"
                "}:-q." == rewrite(parse("#count{ 1:p:1<(2;3)<4 } >= 1:-q.")));
        REQUIRE("1<=#count{"
                "1:p:1>=2;"
                "1:p:2>=4;"
                "1:p:1>=3;"
                "1:p:3>=4"
                "}:-q." == rewrite(parse("#count{ 1:p:not 1<(2;3)<4 } >= 1:-q.")));
        // set based body aggregates
        REQUIRE("q:-1<=#count{3:1<2}." == rewrite(parse("q :- { 1<2 } >= 1.")));
        REQUIRE("q:-1<=#count{"
                "3:1<2;"
                "4:3<4"
                "}." == rewrite(parse("q :- { 1<2; 3<4 } >= 1.")));
        REQUIRE("q:-1<=#count{"
                "3:1<2,2<4;"
                "4:1<3,3<4"
                "}." == rewrite(parse("q :- { 1<(2;3)<4 } >= 1.")));
        REQUIRE("q:-1<=#count{"
                "3:1>=2;"
                "3:2>=4;"
                "4:1>=3;"
                "4:3>=4"
                "}." == rewrite(parse("q :- { not 1<(2;3)<4 } >= 1.")));
        // body aggregates
        REQUIRE("q:-1<=#count{1:1<2}." == rewrite(parse("q :- #count{ 1:1<2 } >= 1.")));
        REQUIRE("q:-1<=#count{1:1<2,2<3}." == rewrite(parse("q :- #count{ 1:1<2<3 } >= 1.")));
        REQUIRE("q:-1<=#count{"
                "1:1>=2;"
                "1:2>=3"
                "}." == rewrite(parse("q :- #count{ 1:not 1<2<3 } >= 1.")));
        REQUIRE("q:-1<=#count{"
                "1:1<2,2<4;"
                "1:1<3,3<4"
                "}." == rewrite(parse("q :- #count{ 1:1<(2;3)<4 } >= 1.")));
        REQUIRE("q:-1<=#count{"
                "1:1>=2;"
                "1:2>=4;"
                "1:1>=3;"
                "1:3>=4"
                "}." == rewrite(parse("q :- #count{ 1:not 1<(2;3)<4 } >= 1.")));
    }

    SECTION("iesolve") {
        // conjunction
        REQUIRE("q:-p(X):1<=X,X<=3,#range(X,1,3)." ==  rewrite(parse("q :- p(X): 1 <= X <= 3.")));
        // disjunction
        REQUIRE("p(X)::1<=X,X<=3,#range(X,1,3)." ==  rewrite(parse("p(X): 1 <= X <= 3.")));
        // head aggregate
        REQUIRE("2<=#count{X:p(X):1<=X,X<=3,#range(X,1,3)}<=3." ==  rewrite(parse("2 #count { X: p(X): 1 <= X <= 3 } 3.")));
        // body aggregate
        REQUIRE("q:-2<=#count{0,p(X):1<=X,X<=3,p(X),#range(X,1,3)}<=3." ==  rewrite(parse("q :- 2 { p(X): 1 <= X <= 3 } 3.")));
        REQUIRE("q:-2<=#count{X:p(X),1<=X,X<=3,#range(X,1,3)}<=3." ==  rewrite(parse("q :- 2 #count { X: p(X), 1 <= X <= 3 } 3.")));
        // rule
        REQUIRE("q:-#range(X,1,3);p(X);1<=X;X<=3." ==  rewrite(parse("q :- p(X), 1 <= X <= 3.")));
    }

    SECTION("defines") {
        REQUIRE("p(1):-q." ==  rewrite(parse("#const a=1.#const b=a.#const c=b.#const d=c.p(d):-q.")));
        REQUIRE("p(2):-q." ==  rewrite(parse("#const c=a+b.#const b=a.#const a=1.p(c):-q.")));
        REQUIRE("p(1,2,3)." == rewrite(parse("#const x=1.#const y=1+x.#const z=1+y.p(x,y,z).")));
        REQUIRE("a." ==        rewrite(parse("#const a=b.a.")));
        REQUIRE("a(b)." ==     rewrite(parse("#const a=b.a(a).")));
        REQUIRE("#project p(2):-[p(2)]." == rewrite(parse("#project p(x).#const x=2.")));
        REQUIRE("#project x:-[x]." == rewrite(parse("#project x.#const x=2.")));
        REQUIRE("#edge(2,y)." == rewrite(parse("#edge(x,y).#const x=2.")));
        REQUIRE("#heuristic x[2@2,sign]:-[x]." == rewrite(parse("#heuristic x. [x@x,sign]#const x=2.")));
        REQUIRE("#heuristic p(2)[2@2,sign]:-[p(2)]." == rewrite(parse("#heuristic p(x). [x@x,sign]#const x=2.")));
        REQUIRE("#theory x{node{};&edge/1:node,{<<},node,head}.#false:-not &edge(2){(2),(Y): p(2,Y)}<<(2)." == rewrite(parse("#theory x{ node{}; &edge/1: node, {<<}, node, head }.&edge(z) { z, Y : p(z,Y)} << z. #const z=2.")));
        REQUIRE("p(1)." ==  rewrite(parse(R"(#const a=1. [override] #const a=2. [default] p(a).)")));
        REQUIRE("p(2)." ==  rewrite(parse(R"(#const a=1. #const a=2. [override] p(a).)")));
    }

    SECTION("check") {
        REQUIRE( check("p(X):-q(X)."));
        REQUIRE(!check("p(X,Y,Z):-q(X).",
            "[-:1:1-16: error: unsafe variables in:\n"
            "  p(X,Y,Z):-[#inc_base];q(X).\n"
            "-:1:5-6: note: 'Y' is unsafe\n"
            "-:1:7-8: note: 'Z' is unsafe\n]"));
        REQUIRE( check("p(X):-p(Y),X=Y+Y."));
        REQUIRE( check("p(X):-p(Y),Y+Y=X."));
        REQUIRE(!check("p(X):-p(Y),Y!=X."));
        REQUIRE(!check("p(X):-p(Y),p(1..Y)"));
        // body aggregates
        REQUIRE( check("p(X):-X=#sum{Y,Z:p(Y,Z)}."));
        REQUIRE(!check("p(X):-X=#sum{Y,Z:p(Y)}.",
            "[-:1:7-23: error: unsafe variables in:\n"
            "  X=#sum{Y,Z:p(Y)}\n"
            "-:1:16-17: note: 'Z' is unsafe\n]"));
        REQUIRE( check(":-p(Y),1#count{X:q(X)}Y."));
        REQUIRE( check(":-p(Y),1{p(X):q(Y)}Y."));
        REQUIRE( check(":-p(Y),p(X):q(X,Y)."));
        // head aggregates
        REQUIRE( check("1#count{X:p(X):q(X,Y)}Y:-r(Y)."));
        REQUIRE( check("1{p(X):q(X,Y)}Y:-r(Y)."));
        REQUIRE( check("p(X):q(X,Y):-r(Y)."));
        REQUIRE( check("#project p(X)."));
        REQUIRE(!check("#project p(X+X)."));
        REQUIRE( check("#edge (x,y)."));
        REQUIRE(!check("#edge (X,Y)."));
        REQUIRE( check("#edge (X,Y):p(X,Y)."));
        REQUIRE( check("#heuristic p(X). [2@X,sign]"));
        REQUIRE(!check("#heuristic p(X). [2@Y,sign]"));
        REQUIRE( check("#heuristic p(X) : p(Y). [2@Y,sign]"));
        REQUIRE( check("#theory x{ node{}; &edge/1: node, head }.&edge(Z) { X, Y : p(X,Y)}:-p(Z)."));
        REQUIRE( check("#theory x{ node{}; &edge/1: node, any }.&edge(Z) { X, Y : p(X,Y)}:-p(Z)."));
        REQUIRE(!check("#theory x{ node{}; &edge/1: node, body }.&edge(Z) { X, Y : p(X,Y)}:-p(Z)."));
        REQUIRE(!check("#theory x{ node{}; &edge/1: node, directive }.&edge(Z) { X, Y : p(X,Y)}:-p(Z)."));
        REQUIRE( check("#theory x{ node{}; &edge/1: node, body }.:-&edge(Z) { X, Y : p(X,Y)},p(Z)."));
        REQUIRE( check("#theory x{ node{}; &edge/1: node, any }.:-&edge(Z) { X, Y : p(X,Y)},p(Z)."));
        REQUIRE(!check("#theory x{ node{}; &edge/1: node, head }.:-&edge(Z) { X, Y : p(X,Y)},p(Z)."));
        REQUIRE(!check("#theory x{ node{}; &edge/1: node, directive }.:-&edge(Z) { X, Y : p(X,Y)},p(Z)."));
        REQUIRE( check("#theory x{ node{}; &edge/1: node, directive }.&edge(z) { X, Y : p(X,Y)}."));
        REQUIRE(!check("#theory x{ node{}; &edge/1: node, head }.&edge(ZZ) { X, Y : p(X,Y)}:-p(Z)."));
        REQUIRE(!check("#theory x{ node{}; &edge/1: node, head }.&edge(Z) { XX, Y : p(X,Y)}:-p(Z)."));
        REQUIRE(!check("#theory x{ node{}; &edge/1: node, head }.&edge(Z) { XX, Y : p(X,Y)}."));
        REQUIRE(!check("#theory x{ node{}; &edge/1: node, head }.&edge(z) { XX, Y : p(X,Y)}."));
    }

    SECTION("projection") {
        REQUIRE("x:-q;#p_p(#p).#p_p(#p):-p(#P0)." == rewrite(parse("x:-p(_),q.")));
        REQUIRE("x:-#p_p(#p,#b(X),f);q.#p_p(#p,#b(#X1),f):-p(#P0,#X1,f)." == rewrite(parse("x:-q;p(_,X,f).")));
        REQUIRE("x:-#p_p(#p);q.y:-#p_p(#p);q.#p_p(#p):-p(#P0)." == rewrite(parse("x:-q,p(_).y:-q,p(_).")));
        REQUIRE("x:-#p_p(f(1,#p),2);q.y:-#p_p(f(#b(X),#p),g(#b(X)));q.#p_p(f(1,#p),2):-p(f(1,#P0),2).#p_p(f(#b(#X0),#p),g(#b(#X2))):-p(f(#X0,#P1),g(#X2))." == rewrite(parse("x:-q,p(f(1,_),2).y:-q,p(f(X,_),g(X)).")));
        REQUIRE("x:-not #p_p(#p).#p_p(#p):-p(#P0)." == rewrite(parse("x:-not p(_).")));
        REQUIRE("x:-#p_p(#p,1);q.y:-#p_p(1,#p);q.#p_p(#p,1):-p(#P0,1).#p_p(1,#p):-p(1,#P0)." == rewrite(parse("x:-q;p(_,1).y:-q;p(1,_).")));
        // NOTE: projection disabled for now
        REQUIRE("x:-1<=#count{0,y:p(#Anon0),y}." == rewrite(parse("x:-1{y:p(_)}.")));
        REQUIRE("x:-1<=#count{0,y:not #p_p(#p),y}.#p_p(#p):-p(#P0)." == rewrite(parse("x:-1{y:not p(_)}.")));
        REQUIRE("#project p(#Anon0):-[p(#Anon0)]." == rewrite(parse("#project p(_).")));
        REQUIRE("#edge(X,#Anon0)." == rewrite(parse("#edge (X,_).")));
        REQUIRE("#heuristic p(#Anon0)[#Anon1@#Anon2,sign]:-[p(#Anon0)]." == rewrite(parse("#heuristic p(_). [_@_,sign]")));
        REQUIRE("#theory x{node{};&edge/2:node,directive}.#false:-not &edge(#Anon0,X){(X),(Y): p(#Anon1,Y)}." == rewrite(parse("#theory x{ node{}; &edge/2: node, directive }.&edge(_,X) { X, Y : p(_,Y)}.")));
    }

    SECTION("statements") {
        REQUIRE("#project p:-[p]." == rewrite(parse("#project p/0.")));
        REQUIRE("#project p(X0):-[p(X0)]." == rewrite(parse("#project p/1.")));
        REQUIRE("#project p(X0,X1,X2):-[p(X0,X1,X2)]." == rewrite(parse("#project p/3.")));
        REQUIRE("#project p:-[p]." == rewrite(parse("#project p.")));
        REQUIRE("#project p(X,Y):-[p(X,Y)]." == rewrite(parse("#project p(X,Y).")));
        REQUIRE("#edge(a,b)." == rewrite(parse("#edge (a,b).")));
        REQUIRE("#heuristic a[1@2,sign]:-[a]." == rewrite(parse("#heuristic a. [1@2,sign]")));
        REQUIRE("#heuristic a[1@2,level]:-[a]." == rewrite(parse("#heuristic a. [1@2,level]")));
        REQUIRE("#heuristic a[1@0,level]:-[a];c." == rewrite(parse("#heuristic a : c. [1,level]")));
        REQUIRE("#external p:[#inc_base].[false]" == rewrite(parse("#external p.")));
        REQUIRE("#external p(X):[#inc_base].[false]#external p(Y):[#inc_base].[false]" == rewrite(parse("#external p(X;Y).")));
        REQUIRE("#external p:[#inc_base].[X]#external p:[#inc_base].[Y]" == rewrite(parse("#external p. [(X;Y)]")));
        REQUIRE("#external p(X):[#inc_base].[X]#external p(X):[#inc_base].[Y]#external p(Y):[#inc_base].[X]#external p(Y):[#inc_base].[Y]" == rewrite(parse("#external p(X;Y). [(X;Y)]")));
    }

    SECTION("iesolver") {
        REQUIRE("q(5)."
                "p(S):-"
                "#range(Y,0,1);"
                "#range(X,0,1);"
                "#range(S,0,2147483647);"
                "#range(#Arith0,0,2147483647);"
                "q(B);"
                "0<=Y;"
                "Y<=2147483647;"
                "0<=X;"
                "X<=2147483647;"
                "S=((2147483647*X)+(2147483647*Y));"
                "#Arith0=((2147483647*X)+(2147483647*Y));"
                "#Arith0=S;"
                "S<=B." == rewrite(parse("q(5). "
                                         "p(S) :- q(B), "
                                         "S<=B, "
                                         "0<=X<=0x7FFFFFFF, "
                                         "0<=Y<=0x7FFFFFFF, "
                                         "S=0x7FFFFFFF*X+0x7FFFFFFF*Y.")));
        REQUIRE("q(5)."
                "p(S):-"
                "#range(Z,0,1);"
                "#range(Y,0,1);"
                "#range(X,0,1);"
                "#range(S,0,2147483647);"
                "#range(#Arith0,0,2147483647);"
                "q(B);"
                "0<=Z;"
                "Z<=2147483647;"
                "0<=Y;"
                "Y<=2147483647;"
                "0<=X;"
                "X<=2147483647;"
                "S=(((2147483647*X)+(2147483647*Y))+(2147483647*Z));"
                "#Arith0=(((2147483647*X)+(2147483647*Y))+(2147483647*Z));"
                "#Arith0=S;"
                "S<=B." == rewrite(parse("q(5). "
                                         "p(S) :- q(B), "
                                         "S<=B, "
                                         "0<=X<=0x7FFFFFFF, "
                                         "0<=Y<=0x7FFFFFFF, "
                                         "0<=Z<=0x7FFFFFFF, "
                                         "S=0x7FFFFFFF*X+0x7FFFFFFF*Y+0x7FFFFFFF*Z.")));
    }

    SECTION("theory") {
        REQUIRE("#theory x{}." == rewrite(parse("#theory x{  }.")));
        REQUIRE("#theory x{node{};&edge/0:node,directive}." == rewrite(parse("#theory x{ node{}; &edge/0: node, directive }.")));
        REQUIRE("#theory x{node{};&edge/0:node,directive}.#false:-not &edge{(X),(Y): p(X,Y)}." == rewrite(parse("#theory x{ node{}; &edge/0: node, directive }.&edge { X, Y : p(X,Y)}.")));
        REQUIRE("#theory x{node{};&edge/0:node,directive}.#false:-not &edge{(X),(Y): p(X,Y)}." == rewrite(parse("#theory x{ node{}; &edge/0: node, directive }.&edge { X, Y : p(X,Y)}.")));
        std::string theory =
            "#theory csp{\n"
            "    term {\n"
            "        < : 0,binary,left;\n"
            "        + : 1,binary,left;\n"
            "        - : 1,binary,left;\n"
            "        * : 2,binary,left;\n"
            "        / : 2,binary,left;\n"
            "        ^ : 3,binary,right;\n"
            "        - : 4,unary\n"
            "    };\n"
            "    &linear/0: term, any\n"
            "}.\n";
        std::string parsed = "#theory csp{term{< :0,binary,left,+ :1,binary,left,- :1,binary,left,* :2,binary,left,/ :2,binary,left,^ :3,binary,right,- :4,unary};&linear/0:term,any}.";
        REQUIRE(parsed == rewrite(parse(theory)));
        REQUIRE(parsed+"#false:-not &linear{(((1)<(((2)+(3))-(((4)*(5))/((6)^((-(7))^(8))))))<(9))}." == rewrite(parse(theory + "&linear { 1 < 2+3-4*5/6^ -7^8 < 9 }.")));
    }

}

} } } // namespace Test Input Gringo

