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

#include "gringo/bug.hh"
#include "gringo/output/theory.hh"
#include "tests/tests.hh"
#include "tests/term_helper.hh"

#include <climits>
#include <sstream>
#include <functional>
#ifdef _MSC_VER
#pragma warning (disable : 4503) // decorated name length exceeded
#endif

namespace Gringo { namespace Test {

using namespace Gringo::IO;

namespace {

using S = std::string;

size_t hash(UTerm const &x) {
    return x->gterm()->hash();
}

UGTerm gterm(UTerm const &x) {
    return x->gterm();
}


} // namespace

TEST_CASE("term", "[base]") {
    TestGringoModule log;

    auto rewriteDots = [&](UTerm &&x) -> std::tuple<UTerm, SimplifyState::DotsMap, SimplifyState::ScriptMap>  {
        SimplifyState state;
        x->simplify(state, true, false, log).update(x, false);
        return std::make_tuple(std::move(x), state.dots(), state.scripts());
    };

    auto rewriteProject = [&](UTerm &&x) -> std::string {
        UTerm sig{make_locatable<ValTerm>(x->loc(), Symbol::createId("#p"))};
        AuxGen arithGen;
        SimplifyState state;
        x->simplify(state, true, false, log).update(x, false);
        auto ret(x->project(sig.get() != nullptr, arithGen));
        Term::replace(x, std::move(std::get<0>(ret)));
        auto projected(std::move(std::get<1>(ret)));
        auto project(std::move(std::get<2>(ret)));
        return to_string(std::make_tuple(
            std::move(x),
            projected ? std::move(projected) : val(Symbol::createStr("#undef")),
            project ? std::move(project) : val(Symbol::createStr("#undef"))));
    };

    auto rewriteArithmetics = [&](UTerm &&x) -> std::string {
        Term::ArithmeticsMap vec;
        vec.emplace_back(gringo_make_unique<Term::LevelMap>());
        AuxGen gen;
        UTerm y(x->rewriteArithmetics(vec, gen));
        return to_string(std::make_pair(y ? std::move(y) : std::move(x), std::move(vec)));
    };

    auto simplify = [&](UTerm &&x) -> UTerm {
        SimplifyState state;
        if (x->simplify(state, true, false, log).update(x, false).undefined()) {
            return make_locatable<ValTerm>(x->loc(), Symbol::createId("#undefined"));
        }
        return std::move(x);
    };

    auto rewrite = [&] (UTerm &&x) -> std::vector<std::tuple<UTerm, SimplifyState::DotsMap, Term::ArithmeticsMap>> {
        AuxGen arithGen;
        SimplifyState state;
        std::vector<std::tuple<UTerm, SimplifyState::DotsMap, Term::ArithmeticsMap>> res;
        for(auto &term : unpool(x)) {
            auto elemState = SimplifyState::make_substate(state);
            Term::ArithmeticsMap arith;
            arith.emplace_back(gringo_make_unique<Term::LevelMap>());
            term->simplify(elemState, true, false, log).update(term, false);
            Term::replace(term, term->rewriteArithmetics(arith, arithGen));
            res.emplace_back(std::move(term), elemState.dots(), std::move(arith));
        }
        return res;
    };

    auto bindVars = [&](UTerm &&x) -> UTerm {
        AuxGen arithGen;
        SimplifyState state;
        Term::ArithmeticsMap arith;
        Term::VarSet set;
        arith.emplace_back(gringo_make_unique<Term::LevelMap>());
        x->simplify(state, true, false, log).update(x, false);
        Term::replace(x, x->rewriteArithmetics(arith, arithGen));
        x->bind(set);
        return std::move(x);
    };

    SECTION("hash") {
        // term
        CHECK(val(NUM(1))->hash() == val(NUM(1))->hash());
        CHECK(!(val(NUM(1))->hash() == val(NUM(2))->hash()));
        CHECK(var("X")->hash() == var("X")->hash());
        CHECK(!(var("X")->hash() == var("Y")->hash()));
        CHECK(binop(BinOp::ADD, val(NUM(1)), val(NUM(2)))->hash() == binop(BinOp::ADD, val(NUM(1)), val(NUM(2)))->hash());
        CHECK(!(binop(BinOp::ADD, val(NUM(1)), val(NUM(2)))->hash() == binop(BinOp::ADD, val(NUM(2)), val(NUM(1)))->hash()));
        CHECK(unop(UnOp::NEG, val(NUM(1)))->hash() == unop(UnOp::NEG, val(NUM(1)))->hash());
        CHECK(!(unop(UnOp::NEG, val(NUM(1)))->hash() == unop(UnOp::ABS, val(NUM(1)))->hash()));
        CHECK(fun("f", val(NUM(1)), val(NUM(2)))->hash() == fun("f", val(NUM(1)), val(NUM(2)))->hash());
        CHECK(!(fun("f", val(NUM(1)), val(NUM(2)))->hash() == fun("g", val(NUM(1)), val(NUM(2)))->hash()));
        CHECK(lua("f", val(NUM(1)), val(NUM(2)))->hash() == lua("f", val(NUM(1)), val(NUM(2)))->hash());
        CHECK(!(lua("f", val(NUM(1)), val(NUM(2)))->hash() == lua("g", val(NUM(1)), val(NUM(2)))->hash()));
        CHECK(pool(val(NUM(1)), val(NUM(2)))->hash() == pool(val(NUM(1)), val(NUM(2)))->hash());
        CHECK(!(pool(val(NUM(1)), val(NUM(2)))->hash() == pool(val(NUM(2)), val(NUM(1)))->hash()));
        CHECK(dots(val(NUM(1)), val(NUM(2)))->hash() == dots(val(NUM(1)), val(NUM(2)))->hash());
        CHECK(!(dots(val(NUM(1)), val(NUM(2)))->hash() == dots(val(NUM(2)), val(NUM(1)))->hash()));
        // gterm
        CHECK(hash(val(NUM(1))) == hash(val(NUM(1))));
        CHECK(hash(val(NUM(1))) != hash(val(NUM(2))));
        CHECK(hash(lin("X",2,3)) == hash(lin("X",2,3)));
        CHECK(hash(lin("X",2,3)) == hash(lin("Y",2,3))); // Note: intended
        CHECK(hash(lin("X",2,3)) != hash(lin("X",1,3)));
        CHECK(hash(lin("X",2,3)) != hash(lin("X",2,2)));
        CHECK(hash(var("X")) == hash(var("X")));
        CHECK(hash(var("X")) == hash(var("Y"))); // Note: intended
        CHECK(hash(fun("f", val(NUM(1)), val(NUM(2)))) == hash(fun("f", val(NUM(1)), val(NUM(2)))));
        CHECK(hash(fun("f", val(NUM(1)), val(NUM(2)))) != hash(fun("g", val(NUM(1)), val(NUM(2)))));
        CHECK(hash(fun("f", val(NUM(1)), val(NUM(2)))) != hash(fun("f", val(NUM(2)), val(NUM(2)))));
        CHECK(hash(fun("f", val(NUM(1)), val(NUM(2)))) != hash(fun("f", val(NUM(1)), val(NUM(1)))));
    }

    SECTION("equal") {
        // term
        REQUIRE(*val(NUM(1)) == *val(NUM(1)));
        REQUIRE(!(*val(NUM(1)) == *val(NUM(2))));
        REQUIRE(*var("X") == *var("X"));
        REQUIRE(!(*var("X") == *var("Y")));
        REQUIRE(*binop(BinOp::ADD, val(NUM(1)), val(NUM(2))) == *binop(BinOp::ADD, val(NUM(1)), val(NUM(2))));
        REQUIRE(!(*binop(BinOp::ADD, val(NUM(1)), val(NUM(2))) == *binop(BinOp::ADD, val(NUM(2)), val(NUM(1)))));
        REQUIRE(*unop(UnOp::NEG, val(NUM(1))) == *unop(UnOp::NEG, val(NUM(1))));
        REQUIRE(!(*unop(UnOp::NEG, val(NUM(1))) == *unop(UnOp::ABS, val(NUM(1)))));
        REQUIRE(*fun("f", val(NUM(1)), val(NUM(2))) == *fun("f", val(NUM(1)), val(NUM(2))));
        REQUIRE(!(*fun("f", val(NUM(1)), val(NUM(2))) == *fun("g", val(NUM(1)), val(NUM(2)))));
        REQUIRE(*lua("f", val(NUM(1)), val(NUM(2))) == *lua("f", val(NUM(1)), val(NUM(2))));
        REQUIRE(!(*lua("f", val(NUM(1)), val(NUM(2))) == *lua("g", val(NUM(1)), val(NUM(2)))));
        REQUIRE(*pool(val(NUM(1)), val(NUM(2))) == *pool(val(NUM(1)), val(NUM(2))));
        REQUIRE(!(*pool(val(NUM(1)), val(NUM(2))) == *pool(val(NUM(2)), val(NUM(1)))));
        REQUIRE(!(*dots(val(NUM(1)), val(NUM(2))) == *dots(val(NUM(1)), val(NUM(2))))); // Note: intended
        REQUIRE(!(*dots(val(NUM(1)), val(NUM(2))) == *dots(val(NUM(2)), val(NUM(1)))));
        // gterm
        REQUIRE(*gterm(val(NUM(1))) == *gterm(val(NUM(1))));
        REQUIRE(*gterm(val(NUM(1))) != *gterm(val(NUM(2))));
        REQUIRE(*gterm(lin("X",2,3)) == *gterm(lin("X",2,3)));
        REQUIRE(*gterm(lin("X",2,3)) == *gterm(lin("Y",2,3))); // Note: intended
        REQUIRE(*gterm(lin("X",2,3)) != *gterm(lin("X",1,3)));
        REQUIRE(*gterm(lin("X",2,3)) != *gterm(lin("X",2,2)));
        REQUIRE(*gterm(var("X")) == *gterm(var("X")));
        REQUIRE(*gterm(fun("f", var("X"), var("X"))) != *gterm(fun("f", var("X"), var("X", 1)))); // Note: all levels=0
        REQUIRE(*gterm(var("X")) == *gterm(var("Y"))); // Note: intended
        REQUIRE(*gterm(fun("f", val(NUM(1)), val(NUM(2)))) == *gterm(fun("f", val(NUM(1)), val(NUM(2)))));
        REQUIRE(*gterm(fun("f", val(NUM(1)), val(NUM(2)))) != *gterm(fun("g", val(NUM(1)), val(NUM(2)))));
        REQUIRE(*gterm(fun("f", val(NUM(1)), val(NUM(2)))) != *gterm(fun("f", val(NUM(2)), val(NUM(2)))));
        REQUIRE(*gterm(fun("f", val(NUM(1)), val(NUM(2)))) != *gterm(fun("f", val(NUM(1)), val(NUM(1)))));
    }

    SECTION("eval") {
        bool undefined;
        REQUIRE(Symbol(NUM(1)) == val(NUM(1))->eval(undefined, log));

        REQUIRE(NUM(0)   == binop(BinOp::DIV, val(NUM(7)), val(NUM(0)))->eval(undefined, log));
        REQUIRE(NUM(1)   == binop(BinOp::MOD, val(NUM(7)), val(NUM(3)))->eval(undefined, log));
        REQUIRE(NUM(2)   == binop(BinOp::DIV, val(NUM(7)), val(NUM(3)))->eval(undefined, log));
        REQUIRE(NUM(3)   == binop(BinOp::AND, val(NUM(7)), val(NUM(3)))->eval(undefined, log));
        REQUIRE(NUM(4)   == binop(BinOp::XOR, val(NUM(7)), val(NUM(3)))->eval(undefined, log));
        REQUIRE(NUM(5)   == binop(BinOp::SUB, val(NUM(7)), val(NUM(2)))->eval(undefined, log));
        REQUIRE(NUM(7)   == binop(BinOp::OR,  val(NUM(7)), val(NUM(3)))->eval(undefined, log));
        REQUIRE(NUM(10)  == binop(BinOp::ADD, val(NUM(7)), val(NUM(3)))->eval(undefined, log));
        REQUIRE(NUM(21)  == binop(BinOp::MUL, val(NUM(7)), val(NUM(3)))->eval(undefined, log));
        REQUIRE(NUM(343) == binop(BinOp::POW, val(NUM(7)), val(NUM(3)))->eval(undefined, log));

        REQUIRE(NUM(-1)  == unop(UnOp::NEG, val(NUM(1)))->eval(undefined, log));
        REQUIRE(NUM(1)   == unop(UnOp::ABS, val(NUM(-1)))->eval(undefined, log));
        REQUIRE(NUM(-13) == unop(UnOp::NOT, val(NUM(12)))->eval(undefined, log));

        REQUIRE(FUN("f", {NUM(1), NUM(5)}) == fun("f", val(NUM(1)), binop(BinOp::ADD, val(NUM(2)), val(NUM(3))))->eval(undefined, log));

        REQUIRE(Symbol::createId("a").flipSign() == unop(UnOp::NEG, val(ID("a")))->eval(undefined, log));
    }

    SECTION("rewriteArithmetics") {
        REQUIRE("(#Arith0,[{(X\\Y):#Arith0}])" == rewriteArithmetics(binop(BinOp::MOD, var("X"), var("Y"))));
        REQUIRE("(f(#Arith0,1),[{(g(X)\\Y):#Arith0}])" == rewriteArithmetics(fun("f", binop(BinOp::MOD, fun("g", var("X")), var("Y")), val(NUM(1)))));
        REQUIRE("(#Arith0,[{@f((@g(X)\\Y)):#Arith0}])" == rewriteArithmetics(lua("f", binop(BinOp::MOD, lua("g", var("X")), var("Y")))));
        REQUIRE("(#Arith0,[{|(X\\Y)|:#Arith0}])" == rewriteArithmetics(unop(UnOp::ABS, binop(BinOp::MOD, var("X"), var("Y")))));
        REQUIRE("((-#Arith0),[{(X\\Y):#Arith0}])" == rewriteArithmetics(unop(UnOp::NEG, binop(BinOp::MOD, var("X"), var("Y")))));
        REQUIRE("(f(#Arith0,#Arith0),[{(X\\Y):#Arith0}])" == rewriteArithmetics(fun("f", binop(BinOp::MOD, var("X"), var("Y")), binop(BinOp::MOD, var("X"), var("Y")))));
    }

    SECTION("rewriteDots") {
        REQUIRE("((#Range0+0),[(#Range0,X,Y)],[])" == to_string(rewriteDots(dots(var("X"), var("Y")))));
        REQUIRE("((#Range0+0),[(#Range0,1,2)],[])" == to_string(rewriteDots(dots(val(NUM(1)), val(NUM(2))))));
        REQUIRE("(f((#Range0+0),3),[(#Range0,g(1),2)],[])" == to_string(rewriteDots(fun("f", dots(fun("g", val(NUM(1))), val(NUM(2))), val(NUM(3))))));
        REQUIRE("(#Script2,[(#Range1,#Script0,2)],[(#Script0,g,[X]),(#Script2,f,[(#Range1+0),3])])" == to_string(rewriteDots(lua("f", dots(lua("g", var("X")), val(NUM(2))), val(NUM(3))))));
        REQUIRE("((#Range0+4),[(#Range0,3,3)],[])" == to_string(rewriteDots(binop(BinOp::ADD, dots(binop(BinOp::ADD, val(NUM(1)), val(NUM(2))), val(NUM(3))), val(NUM(4))))));
        REQUIRE("(|#Range0|,[(#Range0,1,2)],[])" == to_string(rewriteDots(unop(UnOp::ABS, dots(unop(UnOp::ABS, val(NUM(1))), val(NUM(2)))))));
    }

    SECTION("unpool") {
        REQUIRE("[X,Y]" == to_string(unpool(pool(var("X"), var("Y")))));
        REQUIRE("[f(X),f(Y)]" == to_string(unpool(fun("f", pool(var("X"), var("Y"))))));
        REQUIRE("[f(g(X),h(A)),f(g(Y),h(A)),f(g(X),h(B)),f(g(Y),h(B))]" == to_string(unpool(fun("f", fun("g", pool(var("X"), var("Y"))), fun("h", pool(var("A"), var("B")))))));
        REQUIRE("[@f(@g(X),@h(A)),@f(@g(Y),@h(A)),@f(@g(X),@h(B)),@f(@g(Y),@h(B))]" == to_string(unpool(lua("f", lua("g", pool(var("X"), var("Y"))), lua("h", pool(var("A"), var("B")))))));
        REQUIRE("[(-X),(-Y)]" == to_string(unpool(unop(UnOp::NEG, pool(var("X"), var("Y"))))));
        REQUIRE("[(X+A),(X+B),(Y+A),(Y+B)]" == to_string(unpool(binop(BinOp::ADD, pool(var("X"), var("Y")), pool(var("A"), var("B"))))));
        REQUIRE("[(1..(2..4)),(1..(3..4)),5]" == to_string(unpool(pool(dots(val(NUM(1)), dots(pool(val(NUM(2)), val(NUM(3))), val(NUM(4)))), val(NUM(5))))));
    }

    SECTION("simplify") {
        REQUIRE("(X+1)" == to_string(simplify(binop(BinOp::ADD, val(NUM(1)), var("X")))));
        REQUIRE("(X+1)" == to_string(simplify(binop(BinOp::ADD, var("X"), val(NUM(1))))));
        REQUIRE("(-1*X+1)" == to_string(simplify(binop(BinOp::SUB, val(NUM(1)), var("X")))));
        REQUIRE("(2*X)" == to_string(simplify(binop(BinOp::MUL, val(NUM(2)), var("X")))));
        REQUIRE("(0*X)" == to_string(simplify(binop(BinOp::MUL, val(NUM(0)), var("X")))));
        REQUIRE("#undefined" == to_string(simplify(binop(BinOp::ADD, fun("f", val(NUM(1))), var("X")))));
        REQUIRE("#undefined" == to_string(simplify(fun("f", binop(BinOp::ADD, val(NUM(1)), val(ID("a")))))));
        REQUIRE("#undefined" == to_string(simplify(fun("f", binop(BinOp::MOD, val(NUM(1)), val(NUM(0)))))));
        REQUIRE("#undefined" == to_string(simplify(fun("f", binop(BinOp::DIV, val(NUM(1)), val(NUM(0)))))));
        REQUIRE("(X+0)" == to_string(simplify(binop(BinOp::SUB, val(NUM(1)), binop(BinOp::SUB, val(NUM(1)), var("X"))))));
        REQUIRE("(X+1)" == to_string(simplify(binop(BinOp::SUB, val(NUM(1)), unop(UnOp::NEG, var("X"))))));
        REQUIRE("3" == to_string(simplify(binop(BinOp::ADD, val(NUM(1)), val(NUM(2))))));
        REQUIRE("1" == to_string(simplify(unop(UnOp::NEG, val(NUM(-1))))));
        REQUIRE("f((X+1))" == to_string(simplify(fun("f", binop(BinOp::ADD, val(NUM(1)), var("X"))))));
    }

    SECTION("rewrite()") {
        REQUIRE("[(f((#Range1+1)),[(#Range0,2,3),(#Range1,1,#Range0)],[{}]),(f((#Range3+1)),[(#Range2,2,4),(#Range3,1,#Range2)],[{}]),(f((-1*X+1)),[],[{}])]" == to_string(rewrite(fun("f", pool(binop(BinOp::ADD, dots(val(NUM(1)), dots(val(NUM(2)), pool(val(NUM(3)), val(NUM(4))))), val(NUM(1))), binop(BinOp::SUB, val(NUM(1)), var("X")))))));
    }

    SECTION("undefined") {
        bool undefined = false;

        REQUIRE(NUM(0) == binop(BinOp::POW, val(ID("a")), val(NUM(1)))->eval(undefined, log));
        REQUIRE(undefined);
        REQUIRE("dummy:1:1: info: operation undefined:\n  (a**1)\n" == log.messages().back());

        undefined = false;
        REQUIRE(NUM(0) == unop(UnOp::NOT, val(ID("a")))->eval(undefined, log));
        REQUIRE(undefined);
        REQUIRE("dummy:1:1: info: operation undefined:\n  (~a)\n" == log.messages().back());

        undefined = false;
        REQUIRE(NUM(0) == binop(BinOp::POW, val(NUM(0)), val(NUM(-2)))->eval(undefined, log));
        REQUIRE(undefined);
        REQUIRE("dummy:1:1: info: operation undefined:\n  (0**-2)\n" == log.messages().back());

        undefined = false;
        REQUIRE(NUM(0) == binop(BinOp::MOD, val(NUM(10)), val(NUM(0)))->eval(undefined, log));
        REQUIRE(undefined);
        REQUIRE("dummy:1:1: info: operation undefined:\n  (10\\0)\n" == log.messages().back());
    }

    SECTION("project") {
        REQUIRE("(#p_p(#p),#p_p(#p),p(#P0))" == to_string(rewriteProject(fun("p", var("_")))));
        REQUIRE("(#p_p(#b(X),#p),#p_p(#b(#X0),#p),p(#X0,#P1))" == to_string(rewriteProject(fun("p", var("X"), var("_")))));
        REQUIRE("(#p_p(g(#p)),#p_p(g(#p)),p(g(#P0)))" == to_string(rewriteProject(fun("p", fun("g", var("_"))))));
        REQUIRE("(#p_p(#p,f(#b(X),#p),g(#p)),#p_p(#p,f(#b(#X1),#p),g(#p)),p(#P0,f(#X1,#P2),g(#P3)))" == to_string(rewriteProject(fun("p", var("_"), fun("f", var("X"), var("_")), fun("g", var("_"))))));
        REQUIRE("(#p_p(#p,f(h(#p),#b((X+2)),#p),g(#p)),#p_p(#p,f(h(#p),#b(#X2),#p),g(#p)),p(#P0,f(h(#P1),#X2,#P3),g(#P4)))" == to_string(rewriteProject(fun("p", var("_"), fun("f", fun("h", var("_")), binop(BinOp::ADD, var("X"), val(NUM(2))), var("_")), fun("g", var("_"))))));
        REQUIRE("(#p_p(#b((#Anon0+1))),#p_p(#b(#X0)),p(#X0))" == to_string(rewriteProject(fun("p", binop(BinOp::ADD, var("_"), val(NUM(1)))))));
    }

    SECTION("match") {
        REQUIRE(bindVars(val(NUM(1)))->match(NUM(1)));
        REQUIRE(!bindVars(val(NUM(1)))->match(NUM(2)));
        REQUIRE(bindVars(binop(BinOp::ADD, val(NUM(1)), var("X")))->match(NUM(1)));
        REQUIRE(!bindVars(binop(BinOp::ADD, val(NUM(1)), var("X")))->match(ID("a")));
        REQUIRE(bindVars(unop(UnOp::NEG, var("X")))->match(NUM(1)));
        REQUIRE(bindVars(unop(UnOp::NEG, var("X")))->match(ID("a").flipSign()));
        REQUIRE(bindVars(unop(UnOp::NEG, var("X")))->match(ID("a")));
        REQUIRE(!bindVars(unop(UnOp::NEG, val(ID("a"))))->match(ID("a")));
        REQUIRE(!bindVars(unop(UnOp::NEG, var("X")))->match(STR("a")));
        REQUIRE(bindVars(unop(UnOp::NEG, fun("f", var("X"))))->match(FUN("f", {NUM(0)}).flipSign()));
        REQUIRE(!bindVars(unop(UnOp::NEG, fun("f", var("X"))))->match(FUN("f", {NUM(0)})));
        REQUIRE(bindVars(fun("p", var("X"), var("X")))->match(FUN("p", {NUM(1), NUM(1)})));
        REQUIRE(!bindVars(fun("p", var("X"), var("X")))->match(FUN("p", {NUM(1), NUM(2)})));
        REQUIRE(bindVars(fun("p", binop(BinOp::SUB, val(NUM(4)), binop(BinOp::MUL, val(NUM(3)), var("X"))), unop(UnOp::NEG, var("X"))))->match(FUN("p", {NUM(-2), NUM(-2)})));
        REQUIRE(bindVars(fun("p", binop(BinOp::SUB, val(NUM(4)), binop(BinOp::MUL, val(NUM(3)), var("X"))), unop(UnOp::NEG, var("X"))))->match(FUN("p", {NUM(-5), NUM(-3)})));
        REQUIRE(!bindVars(fun("p", binop(BinOp::SUB, val(NUM(4)), binop(BinOp::MUL, val(NUM(3)), var("X"))), unop(UnOp::NEG, var("X"))))->match(FUN("p", {NUM(2), NUM(2)})));
        REQUIRE(!bindVars(fun("p", binop(BinOp::SUB, val(NUM(4)), binop(BinOp::MUL, val(NUM(3)), var("X"))), unop(UnOp::NEG, var("X"))))->match(FUN("p", {NUM(1), NUM(2)})));
    }

    SECTION("theory") {
        Potassco::TheoryData td;
        Output::TheoryData data(td);
        auto T = [&data](Symbol v) {
            std::ostringstream out;
            data.printTerm(out, data.addTerm(v));
            return out.str();
        };
        Symbol px = Symbol::createId("x", false);
        Symbol nx = Symbol::createId("x", true);
        Symbol str = Symbol::createStr("x\ny");
        Symbol sup = Symbol::createSup();
        Symbol inf = Symbol::createInf();
        Symbol pf = Symbol::createFun("f", Potassco::toSpan(SymVec{px, nx, str, sup, inf}), false);
        Symbol nf = Symbol::createFun("f", Potassco::toSpan(SymVec{px, nx, str, sup, inf}), true);
        Symbol t = Symbol::createTuple(Potassco::toSpan(SymVec{px, nx, str, sup, inf}));
        Potassco::Id_t nfId = data.addTerm(nf);
        REQUIRE("x" == T(px));
        REQUIRE("(-x)" == T(nx));
        REQUIRE("#inf" == T(inf));
        REQUIRE("#sup" == T(sup));
        REQUIRE("\"x\\ny\"" == T(str));
        REQUIRE("f(x,(-x),\"x\\ny\",#sup,#inf)" == T(pf));
        REQUIRE("(-f(x,(-x),\"x\\ny\",#sup,#inf))" == T(nf));
        REQUIRE("(x,(-x),\"x\\ny\",#sup,#inf)" == T(t));
        REQUIRE(nfId == data.addTerm(nf));
    }
}

} } // namespace Test Gringo

