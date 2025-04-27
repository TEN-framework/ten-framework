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

#include "gringo/input/programbuilder.hh"
#include "gringo/input/program.hh"
#include "gringo/output/output.hh"

#include "tests/tests.hh"
#include "tests/term_helper.hh"

#include <climits>
#include <sstream>

namespace Gringo { namespace Input { namespace Test {

using namespace Gringo::Test;

// {{{ declaration of Builder

struct Builder {
    Builder();

    void setUp();
    // {{{ auxiliary function
    typedef std::initializer_list<char const*> Lits;
    typedef std::initializer_list<int> Terms;
    typedef std::initializer_list<std::pair<char const*, Lits>> CondLits;
    typedef std::initializer_list<std::pair<Relation, int>> Bounds;
    typedef std::initializer_list<std::pair<Terms, Lits>> BodyAggrElems;
    typedef std::initializer_list<std::tuple<Terms, char const *, Lits>> HeadAggrElems;
    LitUid lit(const char *name);
    LitVecUid litvec(Lits names);
    TermVecUid termvec(Terms terms);
    CondLitVecUid condlitvec(CondLits names);
    BdAggrElemVecUid bdaggrelemvec(BodyAggrElems elems);
    HdAggrElemVecUid hdaggrelemvec(HeadAggrElems elems);
    BoundVecUid boundvec(Bounds bounds);
    BdLitVecUid bodyaggr(NAF naf, AggregateFunction fun, Bounds, CondLits);
    BdLitVecUid bodyaggr(NAF naf, AggregateFunction fun, Bounds, BodyAggrElems);
    HdLitUid headaggr(AggregateFunction fun, Bounds, CondLits);
    HdLitUid headaggr(AggregateFunction fun, Bounds, HeadAggrElems);
    // }}}
    void test_term();
    void test_literal();
    void test_bdaggr();
    void test_hdaggr();
    void test_rule();

    Gringo::Test::TestGringoModule module;
    std::ostringstream oss;
    Potassco::TheoryData td;
    Output::OutputBase out;
    Location l;
    Defines defs;
    Program prg;
    Gringo::Test::TestContext context;
    NongroundProgramBuilder p;
};

// }}}

using namespace Gringo::IO;

// {{{ definition of Builder

Builder::Builder()
    : out(td, {}, oss)
    , l("dummy", 1, 1, "dummy", 1, 1)
    , p(context, prg, out.outPreds, defs) { }

void Builder::setUp() {
    defs.~Defines();
    new (&defs) Defines();
    prg.~Program();
    new (&prg) Program();
    p.~NongroundProgramBuilder();
    new (&p) NongroundProgramBuilder(context, prg, out.outPreds, defs);
}

// {{{ auxiliary functions

LitUid Builder::lit(const char *name) {
    return p.predlit(l, NAF::POS, p.predRep(l, false, name, p.termvecvec(p.termvecvec(), p.termvec())));
}

LitVecUid Builder::litvec(Lits names) {
    LitVecUid lits = p.litvec();
    for (auto &x : names) { lits = p.litvec(lits, lit(x)); }
    return lits;
}

TermVecUid Builder::termvec(Terms terms) {
    TermVecUid vec = p.termvec();
    for (auto &x : terms) { vec = p.termvec(vec, p.term(l, NUM(x))); }
    return vec;
}

CondLitVecUid Builder::condlitvec(CondLits elems) {
    CondLitVecUid vec = p.condlitvec();
    for (auto &x : elems) { p.condlitvec(vec, lit(x.first), litvec(x.second)); }
    return vec;
}

HdAggrElemVecUid Builder::hdaggrelemvec(HeadAggrElems elems) {
    HdAggrElemVecUid vec = p.headaggrelemvec();
    for (auto &x : elems) { p.headaggrelemvec(vec, termvec(std::get<0>(x)), lit(std::get<1>(x)), litvec(std::get<2>(x))); }
    return vec;
}

BdAggrElemVecUid Builder::bdaggrelemvec(BodyAggrElems elems) {
    BdAggrElemVecUid vec = p.bodyaggrelemvec();
    for (auto &x : elems) { p.bodyaggrelemvec(vec, termvec(x.first), litvec(x.second)); }
    return vec;
}

BoundVecUid Builder::boundvec(Bounds bounds) {
    BoundVecUid vec = p.boundvec();
    for (auto &x : bounds) { vec = p.boundvec(vec, x.first, p.term(l, NUM(x.second))); }
    return vec;
}

BdLitVecUid Builder::bodyaggr(NAF naf, AggregateFunction fun, Bounds bounds, CondLits elems) {
    return p.bodyaggr(p.body(), l, naf, fun, boundvec(bounds), condlitvec(elems));
}

BdLitVecUid Builder::bodyaggr(NAF naf, AggregateFunction fun, Bounds bounds, BodyAggrElems elems) {
    return p.bodyaggr(p.body(), l, naf, fun, boundvec(bounds), bdaggrelemvec(elems));
}

HdLitUid Builder::headaggr(AggregateFunction fun, Bounds bounds, CondLits elems) {
    return p.headaggr(l, fun, boundvec(bounds), condlitvec(elems));
}

HdLitUid Builder::headaggr(AggregateFunction fun, Bounds bounds, HeadAggrElems elems) {
    return p.headaggr(l, fun, boundvec(bounds), hdaggrelemvec(elems));
}

// }}}

void Builder::test_term() {
    auto t = [&](TermUid uid) -> std::string {
        p.rule(l, p.headlit(p.predlit(l, NAF::POS, p.predRep(l, false, "p", p.termvecvec(p.termvecvec(), p.termvec(p.termvec(), uid))))));
        auto s(to_string(prg));
        setUp();
        if (!s.empty()) { s.pop_back(); }
        return s;
    };
    REQUIRE("p(1)." == t(p.term(l, NUM(1))));
    REQUIRE("p(X)." == t(p.term(l, String("X"))));
    REQUIRE("p(_)." == t(p.term(l, String("_"))));
    REQUIRE("p(|1|)." == t(p.term(l, UnOp::ABS, p.term(l, NUM(1)))));
    REQUIRE("p(|1|;|2|;|3|)." == t(p.term(l, UnOp::ABS, p.termvec(p.termvec(p.termvec(p.termvec(), p.term(l, NUM(1))), p.term(l, NUM(2))), p.term(l, NUM(3))))));
    REQUIRE("p((1+2))." == t(p.term(l, BinOp::ADD, p.term(l, NUM(1)), p.term(l, NUM(2)))));
    REQUIRE("p((1..2))." == t(p.term(l, p.term(l, NUM(1)), p.term(l, NUM(2)))));
    REQUIRE("p(f(1);f(2,3,4))." == t(p.term(l, String("f"), p.termvecvec(p.termvecvec(p.termvecvec(), p.termvec(p.termvec(), p.term(l, NUM(1)))), p.termvec(p.termvec(p.termvec(p.termvec(), p.term(l, NUM(2))), p.term(l, NUM(3))), p.term(l, NUM(4)))), false)));
    REQUIRE("p(@f(1);@f(2,3,4))." == t(p.term(l, String("f"), p.termvecvec(p.termvecvec(p.termvecvec(), p.termvec(p.termvec(), p.term(l, NUM(1)))), p.termvec(p.termvec(p.termvec(p.termvec(), p.term(l, NUM(2))), p.term(l, NUM(3))), p.term(l, NUM(4)))), true)));
    REQUIRE("p(1;(2,3,4))." == t(p.pool(l,p.termvec(p.termvec(p.termvec(),p.term(l, p.termvec(p.termvec(), p.term(l, NUM(1))), false)), p.term(l, p.termvec(p.termvec(p.termvec(p.termvec(), p.term(l, NUM(2))), p.term(l, NUM(3))), p.term(l, NUM(4))), false)))));
    REQUIRE("p((1,);(2,3,4))." == t(
        p.pool(l,
            p.termvec(
                p.termvec(
                    p.termvec(),
                    p.term(l, p.termvec(p.termvec(), p.term(l, NUM(1))), true)),
                p.term(l, p.termvec(p.termvec(p.termvec(p.termvec(), p.term(l, NUM(2))), p.term(l, NUM(3))), p.term(l, NUM(4))), false)))));
}

void Builder::test_literal() {
    auto t = [&](LitUid uid) -> std::string {
        p.rule(l, p.headlit(uid));
        auto s(to_string(prg));
        setUp();
        if (!s.empty()) { s.pop_back(); }
        return s;
    };
    REQUIRE("0=0." == t(p.boollit(l, true)));
    REQUIRE("0!=0." == t(p.boollit(l, false)));
    REQUIRE("p." == t(lit("p")));
    REQUIRE("(-p)." == t(p.predlit(l, NAF::POS, p.predRep(l, true, "p", p.termvecvec(p.termvecvec(), p.termvec())))));
    REQUIRE("not p." == t(p.predlit(l, NAF::NOT, p.predRep(l, false, "p", p.termvecvec(p.termvecvec(), p.termvec())))));
    REQUIRE("not not p." == t(p.predlit(l, NAF::NOTNOT, p.predRep(l, false, "p", p.termvecvec(p.termvecvec(), p.termvec())))));
    REQUIRE("1<2." == t(p.rellit(l, NAF::POS, p.term(l, NUM(1)), p.rellitvec(l, Relation::LT, p.term(l, NUM(2))))));
    REQUIRE("1<2<=3." == t(p.rellit(l, NAF::POS, p.term(l, NUM(1)), p.rellitvec(l, p.rellitvec(l, Relation::LT, p.term(l, NUM(2))), Relation::LEQ, p.term(l, NUM(3))))));
    REQUIRE("not 1<2<=3." == t(p.rellit(l, NAF::NOT, p.term(l, NUM(1)), p.rellitvec(l, p.rellitvec(l, Relation::LT, p.term(l, NUM(2))), Relation::LEQ, p.term(l, NUM(3))))));
    // Note: the programbuilder strips the double negation
    REQUIRE("1<2<=3." == t(p.rellit(l, NAF::NOTNOT, p.term(l, NUM(1)), p.rellitvec(l, p.rellitvec(l, Relation::LT, p.term(l, NUM(2))), Relation::LEQ, p.term(l, NUM(3))))));
}

void Builder::test_bdaggr() {
    auto t = [&](BdLitVecUid uid) -> std::string {
        p.rule(l, p.headlit(p.boollit(l, false)), uid);
        auto s(to_string(prg));
        setUp();
        if (!s.empty()) { s.pop_back(); }
        return s;
    };
    // BodyAggrElems
    REQUIRE("0!=0:-#sum{}." == t(bodyaggr(NAF::POS, AggregateFunction::SUM, {}, BodyAggrElems({}))));
    REQUIRE("0!=0:-not #sum{}." == t(bodyaggr(NAF::NOT, AggregateFunction::SUM, {}, BodyAggrElems({}))));
    REQUIRE("0!=0:-not not #sum{}." == t(bodyaggr(NAF::NOTNOT, AggregateFunction::SUM, {}, BodyAggrElems({}))));
    REQUIRE("0!=0:-#count{}." == t(bodyaggr(NAF::POS, AggregateFunction::COUNT, {}, BodyAggrElems({}))));
    REQUIRE("0!=0:-1<#sum{}." == t(bodyaggr(NAF::POS, AggregateFunction::SUM, {{Relation::GT, 1}}, BodyAggrElems({}))));
    REQUIRE("0!=0:-1<#sum{}<2." == t(bodyaggr(NAF::POS, AggregateFunction::SUM, {{Relation::GT, 1},{Relation::LT, 2}}, BodyAggrElems({}))));
    REQUIRE("0!=0:-1<#sum{}<2<3." == t(bodyaggr(NAF::POS, AggregateFunction::SUM, {{Relation::GT, 1},{Relation::LT, 2},{Relation::LT, 3}}, BodyAggrElems({}))));
    REQUIRE("0!=0:-#sum{:}." == t(bodyaggr(NAF::POS, AggregateFunction::SUM, {}, BodyAggrElems({ {{}, {}} }))));
    REQUIRE("0!=0:-#sum{1:p}." == t(bodyaggr(NAF::POS, AggregateFunction::SUM, {}, BodyAggrElems({ {{1}, {"p"}} }))));
    REQUIRE("0!=0:-#sum{1,2:p,q}." == t(bodyaggr(NAF::POS, AggregateFunction::SUM, {}, BodyAggrElems({ {{1, 2},{"p", "q"}} }))));
    REQUIRE("0!=0:-#sum{:;:}." == t(bodyaggr(NAF::POS, AggregateFunction::SUM, {}, BodyAggrElems({ {{},{}}, {{},{}} }))));
    // CondLits
    REQUIRE("0!=0:-#sum{}." == t(bodyaggr(NAF::POS, AggregateFunction::SUM, {}, CondLits({}))));
    REQUIRE("0!=0:-not #sum{}." == t(bodyaggr(NAF::NOT, AggregateFunction::SUM, {}, CondLits({}))));
    REQUIRE("0!=0:-not not #sum{}." == t(bodyaggr(NAF::NOTNOT, AggregateFunction::SUM, {}, CondLits({}))));
    REQUIRE("0!=0:-#count{}." == t(bodyaggr(NAF::POS, AggregateFunction::COUNT, {}, CondLits({}))));
    REQUIRE("0!=0:-1<#sum{}." == t(bodyaggr(NAF::POS, AggregateFunction::SUM, {{Relation::GT, 1}}, CondLits({}))));
    REQUIRE("0!=0:-1<#sum{}<2." == t(bodyaggr(NAF::POS, AggregateFunction::SUM, {{Relation::GT, 1},{Relation::LT, 2}}, CondLits({}))));
    REQUIRE("0!=0:-1<#sum{}<2<3." == t(bodyaggr(NAF::POS, AggregateFunction::SUM, {{Relation::GT, 1},{Relation::LT, 2},{Relation::LT, 3}}, CondLits({}))));
    REQUIRE("0!=0:-#sum{p:}." == t(bodyaggr(NAF::POS, AggregateFunction::SUM, {}, CondLits({ {"p",{}} }))));
    REQUIRE("0!=0:-#sum{p:q}." == t(bodyaggr(NAF::POS, AggregateFunction::SUM, {}, CondLits({ {"p",{"q"}} }))));
    REQUIRE("0!=0:-#sum{p:q,r}." == t(bodyaggr(NAF::POS, AggregateFunction::SUM, {}, CondLits({ {"p",{"q", "r"}} }))));
    REQUIRE("0!=0:-#sum{p:;q:}." == t(bodyaggr(NAF::POS, AggregateFunction::SUM, {}, CondLits({ {"p",{}}, {"q",{}} }))));
    // Conjunction
    REQUIRE("0!=0:-p:." == t(p.conjunction(p.body(), l, lit("p"), litvec({}))));
    REQUIRE("0!=0:-p:q." == t(p.conjunction(p.body(), l, lit("p"), litvec({"q"}))));
    REQUIRE("0!=0:-p:q,r." == t(p.conjunction(p.body(), l, lit("p"), litvec({"q", "r"}))));
}

void Builder::test_hdaggr() {
    auto t = [&](HdLitUid uid) -> std::string {
        p.rule(l, uid);
        auto s(to_string(prg));
        setUp();
        if (!s.empty()) { s.pop_back(); }
        return s;
    };
    // HeadAggrElems
    REQUIRE("#sum{}." == t(headaggr(AggregateFunction::SUM, {}, HeadAggrElems({}))));
    REQUIRE("#count{}." == t(headaggr(AggregateFunction::COUNT, {}, HeadAggrElems({}))));
    REQUIRE("1<#sum{}." == t(headaggr(AggregateFunction::SUM, {{Relation::GT, 1}}, HeadAggrElems({}))));
    REQUIRE("1<#sum{}<2." == t(headaggr(AggregateFunction::SUM, {{Relation::GT, 1},{Relation::LT, 2}}, HeadAggrElems({}))));
    REQUIRE("1<#sum{}<2<3." == t(headaggr(AggregateFunction::SUM, {{Relation::GT, 1},{Relation::LT, 2},{Relation::LT, 3}}, HeadAggrElems({}))));
    typedef std::tuple<Terms, char const *, Lits> E;
    REQUIRE("#sum{:p:}." == t(headaggr(AggregateFunction::SUM, {}, HeadAggrElems({ E{{},"p",{}} }))));
    REQUIRE("#sum{1:p:q}." == t(headaggr(AggregateFunction::SUM, {}, HeadAggrElems({ E{{1},"p",{"q"}} }))));
    REQUIRE("#sum{1,2:p:q,r}." == t(headaggr(AggregateFunction::SUM, {}, HeadAggrElems({ E{{1,2},"p",{"q", "r"}} }))));
    REQUIRE("#sum{:p:;:p:}." == t(headaggr(AggregateFunction::SUM, {}, HeadAggrElems({ E{{},"p",{}}, E{{},"p",{}} }))));
    // CondLits
    REQUIRE("#sum{}." == t(headaggr(AggregateFunction::SUM, {}, CondLits({}))));
    REQUIRE("#count{}." == t(headaggr(AggregateFunction::COUNT, {}, CondLits({}))));
    REQUIRE("1<#sum{}." == t(headaggr(AggregateFunction::SUM, {{Relation::GT,1}}, CondLits({}))));
    REQUIRE("1<#sum{}<2." == t(headaggr(AggregateFunction::SUM, {{Relation::GT,1},{Relation::LT,2}}, CondLits({}))));
    REQUIRE("1<#sum{}<2<3." == t(headaggr(AggregateFunction::SUM, {{Relation::GT,1},{Relation::LT,2},{Relation::LT,3}}, CondLits({}))));
    REQUIRE("#sum{p:}." == t(headaggr(AggregateFunction::SUM, {}, CondLits({ {"p",{}} }))));
    REQUIRE("#sum{p:q}." == t(headaggr(AggregateFunction::SUM, {}, CondLits({ {"p",{"q"}} }))));
    REQUIRE("#sum{p:q,r}." == t(headaggr(AggregateFunction::SUM, {}, CondLits({ {"p",{"q","r"}} }))));
    REQUIRE("#sum{p:;q:}." == t(headaggr(AggregateFunction::SUM, {}, CondLits({ {"p",{}},{"q",{}} }))));
    // Disjunction
    REQUIRE("p::." == t(p.disjunction(l, condlitvec({ {"p",{}} }))));
    REQUIRE("p::q." == t(p.disjunction(l, condlitvec({ {"p", {"q"}} }))));
    REQUIRE("p::q,r." == t(p.disjunction(l, condlitvec({ {"p",{"q","r"}} }))));
    REQUIRE("p::;q::." == t(p.disjunction(l, condlitvec({ {"p",{}}, {"q",{}} }))));
}

void Builder::test_rule() {
    auto t = [&](char const *name, Lits lits) -> std::string {
        BdLitVecUid vec = p.body();
        for (auto &x : lits) { vec = p.bodylit(vec, lit(x)); }
        p.rule(l, p.headlit(lit(name)), vec);
        auto s(to_string(prg));
        setUp();
        if (!s.empty()) { s.pop_back(); }
        return s;
    };
    REQUIRE("a." == t("a",{}));
    REQUIRE("a:-b." == t("a",{"b"}));
    REQUIRE("a:-b;c." == t("a",{"b","c"}));
    REQUIRE("a:-b;c;d." == t("a",{"b","c","d"}));
}

TEST_CASE("input-builder", "[input]") {
    Builder b;
    SECTION("term") { b.test_term(); }
    SECTION("literal") { b.test_literal(); }
    SECTION("rule") { b.test_rule(); }
    SECTION("bdaggr") { b.test_bdaggr(); }
    SECTION("hdaggr") { b.test_hdaggr(); }
}

} } } // namespace Test Input Gringo


