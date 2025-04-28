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
#include "gringo/input/aggregate.hh"

#include "tests/tests.hh"
#include "tests/term_helper.hh"
#include "tests/input/lit_helper.hh"
#include "tests/input/aggregate_helper.hh"

#include <climits>
#include <sstream>

namespace Gringo { namespace Input { namespace Test {

using namespace Gringo::IO;

// {{{ auxiliary functions and classes

namespace {

ULit lit(char const *name) { return pred(NAF::POS, val(ID(name))); }
template <class... T>
ULit lit(T... name) { return pred(NAF::POS, pool(val(ID(name))...)); }

UTerm dterm(int a, int b) { return dots(val(NUM(a)), val(NUM(b))); }
ULit dlit(int a, int b) { return pred(NAF::POS, fun("p", dterm(a, b))); }

UTerm aterm(char const *a, char const * b) { return binop(BinOp::ADD, var(a), var(b)); }
ULit alit(char const * a, char const * b) { return pred(NAF::POS, fun("p", aterm(a, b))); }

BoundVec bound1() { return boundvec(Relation::LT, val(NUM(3)), Relation::GT, val(NUM(1))); }
BoundVec bound2() { return boundvec(Relation::LT, pool(val(NUM(1)), val(NUM(2))), Relation::GT, pool(val(NUM(3)), val(NUM(4)))); }
BoundVec bound3() { return boundvec(Relation::LT, pool(val(NUM(1)), val(NUM(2)))); }

BodyAggrElemVec bdvec1() { return bdelemvec(); }
BodyAggrElemVec bdvec2() { return bdelemvec(termvec(), litvec()); }
BodyAggrElemVec bdvec3() { return bdelemvec(termvec(val(NUM(1))), litvec(lit("p"))); }
BodyAggrElemVec bdvec4() { return bdelemvec(termvec(val(NUM(1)), val(NUM(2))), litvec(lit("p"), lit("q"))); }
BodyAggrElemVec bdvec5() { return bdelemvec(termvec(), litvec(), termvec(), litvec(lit("p"))); }
// with pool term
BodyAggrElemVec bdvec6() { return bdelemvec(termvec(pool(val(NUM(1)), val(NUM(2)))), litvec(pred(NAF::POS, pool(val(ID("p")), val(ID("q"))))), termvec(), litvec()); }
BodyAggrElemVec bdvec7() { return bdelemvec(termvec(pool(val(NUM(1)), val(NUM(2))), pool(val(NUM(3)), val(NUM(4)))), litvec()); }

CondLitVec condvec1() { return condlitvec(); }
CondLitVec condvec2() { return condlitvec(lit("p"), litvec()); }
CondLitVec condvec3() { return condlitvec(lit("p"), litvec(lit("q"))); }
CondLitVec condvec4() { return condlitvec(lit("p"), litvec(lit("q"), lit("r"))); }
CondLitVec condvec5() { return condlitvec(lit("p"), litvec(), lit("q"), litvec()); }
CondLitVec condvec6() { return condlitvec(lit("p", "q"), litvec(lit("r", "s")), lit("t"), litvec()); }

HeadAggrElemVec hdvec1() { return hdelemvec(); }
HeadAggrElemVec hdvec2() { return hdelemvec(termvec(), lit("p"), litvec()); }
HeadAggrElemVec hdvec3() { return hdelemvec(termvec(val(NUM(1))), lit("p"), litvec(lit("q"))); }
HeadAggrElemVec hdvec4() { return hdelemvec(termvec(val(NUM(1)), val(NUM(2))), lit("p"), litvec(lit("q"), lit("r"))); }
HeadAggrElemVec hdvec5() { return hdelemvec(termvec(), lit("p"), litvec(), termvec(), lit("q"), litvec(lit("r"))); }
// with pool term
HeadAggrElemVec hdvec6() { return hdelemvec(termvec(pool(val(NUM(1)), val(NUM(2)))), lit("p"), litvec(lit("q", "r")), termvec(), lit("s"), litvec()); }
HeadAggrElemVec hdvec7() { return hdelemvec(termvec(pool(val(NUM(1)), val(NUM(2))), pool(val(NUM(3)), val(NUM(4)))), lit("p"), litvec()); }
HeadAggrElemVec hdvec8() { return hdelemvec(termvec(val(NUM(1))), lit("p", "q"), litvec()); }

std::string simplify(UBodyAggr &&x) {
    Gringo::Test::TestGringoModule log;
    Projections project;
    SimplifyState state;
    x->simplify(project, state, false, log);
    return to_string(std::make_pair(std::move(x), state.dots()));
}

std::string simplify(UHeadAggr &&x) {
    Gringo::Test::TestGringoModule log;
    Projections project;
    SimplifyState state;
    x->simplify(project, state, log);
    return to_string(std::make_pair(std::move(x), state.dots()));
}

std::string rewrite(UBodyAggr &&x) {
    Literal::RelationVec assign;
    Term::ArithmeticsMap arith;
    arith.emplace_back(gringo_make_unique<Term::LevelMap>());
    AuxGen gen;
    AssignLevel v;
    x->assignLevels(v);
    v.assignLevels();
    x->rewriteArithmetics(arith, assign, gen);
    return to_string(std::make_tuple(std::move(x), std::move(arith), std::move(assign)));
}

std::string rewrite(UHeadAggr &&x) {
    Term::ArithmeticsMap arith;
    arith.emplace_back(gringo_make_unique<Term::LevelMap>());
    AuxGen gen;
    AssignLevel v;
    x->assignLevels(v);
    v.assignLevels();
    x->rewriteArithmetics(arith, gen);
    return to_string(std::make_pair(std::move(x), std::move(arith)));
}

UHeadAggrVec unpool(UHeadAggr &&x) {
    UHeadAggrVec y;
    x->unpool(y);
    return y;
}

UBodyAggrVec unpool(UBodyAggr &&x) {
    UBodyAggrVec y;
    x->unpool(y);
    return y;
}

} // namespace

// }}}

TEST_CASE("input-aggregate", "[input]") {
    SECTION("print") {
        // body tuple aggregate
        REQUIRE("3>#sum{}>1" == to_string(bdaggr(NAF::POS, AggregateFunction::SUM, bound1(), bdvec1())));
        REQUIRE("not 3>#sum{}>1" == to_string(bdaggr(NAF::NOT, AggregateFunction::SUM, bound1(), bdvec1())));
        REQUIRE("3>#count{}>1" == to_string(bdaggr(NAF::POS, AggregateFunction::COUNT, bound1(), bdvec1())));
        REQUIRE("3>#count{:}>1" == to_string(bdaggr(NAF::POS, AggregateFunction::COUNT, bound1(), bdvec2())));
        REQUIRE("3>#count{1:p}>1" == to_string(bdaggr(NAF::POS, AggregateFunction::COUNT, bound1(), bdvec3())));
        REQUIRE("3>#count{1,2:p,q}>1" == to_string(bdaggr(NAF::POS, AggregateFunction::COUNT, bound1(), bdvec4())));
        REQUIRE("3>#count{:;:p}>1" == to_string(bdaggr(NAF::POS, AggregateFunction::COUNT, bound1(), bdvec5())));
        // body lit aggregate
        REQUIRE("3>#sum{}>1" == to_string(bdaggr(NAF::POS, AggregateFunction::SUM, bound1(), condvec1())));
        REQUIRE("not 3>#sum{}>1" == to_string(bdaggr(NAF::NOT, AggregateFunction::SUM, bound1(), condvec1())));
        REQUIRE("3>#count{}>1" == to_string(bdaggr(NAF::POS, AggregateFunction::COUNT, bound1(), condvec1())));
        REQUIRE("3>#count{p:}>1" == to_string(bdaggr(NAF::POS, AggregateFunction::COUNT, bound1(), condvec2())));
        REQUIRE("3>#count{p:q}>1" == to_string(bdaggr(NAF::POS, AggregateFunction::COUNT, bound1(), condvec3())));
        REQUIRE("3>#count{p:q,r}>1" == to_string(bdaggr(NAF::POS, AggregateFunction::COUNT, bound1(), condvec4())));
        REQUIRE("3>#count{p:;q:}>1" == to_string(bdaggr(NAF::POS, AggregateFunction::COUNT, bound1(), condvec5())));
        // conjunction
        REQUIRE("p:" == to_string(bdaggr(lit("p"), litvec())));
        REQUIRE("p:q" == to_string(bdaggr(lit("p"), litvec(lit("q")))));
        // head tuple aggregate
        REQUIRE("3>#sum{}>1" == to_string(hdaggr(AggregateFunction::SUM, bound1(), hdvec1())));
        REQUIRE("3>#count{}>1" == to_string(hdaggr(AggregateFunction::COUNT, bound1(), hdvec1())));
        REQUIRE("3>#count{:p:}>1" == to_string(hdaggr(AggregateFunction::COUNT, bound1(), hdvec2())));
        REQUIRE("3>#count{1:p:q}>1" == to_string(hdaggr(AggregateFunction::COUNT, bound1(), hdvec3())));
        REQUIRE("3>#count{1,2:p:q,r}>1" == to_string(hdaggr(AggregateFunction::COUNT, bound1(), hdvec4())));
        REQUIRE("3>#count{:p:;:q:r}>1" == to_string(hdaggr(AggregateFunction::COUNT, bound1(), hdvec5())));
        // head lit aggregate
        REQUIRE("3>#sum{}>1" == to_string(hdaggr(AggregateFunction::SUM, bound1(), condvec1())));
        REQUIRE("3>#count{}>1" == to_string(hdaggr(AggregateFunction::COUNT, bound1(), condvec1())));
        REQUIRE("3>#count{p:}>1" == to_string(hdaggr(AggregateFunction::COUNT, bound1(), condvec2())));
        REQUIRE("3>#count{p:q}>1" == to_string(hdaggr(AggregateFunction::COUNT, bound1(), condvec3())));
        REQUIRE("3>#count{p:q,r}>1" == to_string(hdaggr(AggregateFunction::COUNT, bound1(), condvec4())));
        REQUIRE("3>#count{p:;q:}>1" == to_string(hdaggr(AggregateFunction::COUNT, bound1(), condvec5())));
        // disjunction
        REQUIRE("" == to_string(hdaggr(condvec1())));
        REQUIRE("p::" == to_string(hdaggr(condvec2())));
        REQUIRE("p::q" == to_string(hdaggr(condvec3())));
        REQUIRE("p::q,r" == to_string(hdaggr(condvec4())));
        REQUIRE("p::;q::" == to_string(hdaggr(condvec5())));
    }

    SECTION("clone") {
        // body tuple aggregate
        REQUIRE("3>#sum{}>1" == to_string(get_clone(bdaggr(NAF::POS, AggregateFunction::SUM, bound1(), bdvec1()))));
        REQUIRE("not 3>#sum{}>1" == to_string(get_clone(bdaggr(NAF::NOT, AggregateFunction::SUM, bound1(), bdvec1()))));
        REQUIRE("3>#count{}>1" == to_string(get_clone(bdaggr(NAF::POS, AggregateFunction::COUNT, bound1(), bdvec1()))));
        REQUIRE("3>#count{:}>1" == to_string(get_clone(bdaggr(NAF::POS, AggregateFunction::COUNT, bound1(), bdvec2()))));
        REQUIRE("3>#count{1:p}>1" == to_string(get_clone(bdaggr(NAF::POS, AggregateFunction::COUNT, bound1(), bdvec3()))));
        REQUIRE("3>#count{1,2:p,q}>1" == to_string(get_clone(bdaggr(NAF::POS, AggregateFunction::COUNT, bound1(), bdvec4()))));
        REQUIRE("3>#count{:;:p}>1" == to_string(get_clone(bdaggr(NAF::POS, AggregateFunction::COUNT, bound1(), bdvec5()))));
        // body lit aggregate
        REQUIRE("3>#sum{}>1" == to_string(get_clone(bdaggr(NAF::POS, AggregateFunction::SUM, bound1(), condvec1()))));
        REQUIRE("not 3>#sum{}>1" == to_string(get_clone(bdaggr(NAF::NOT, AggregateFunction::SUM, bound1(), condvec1()))));
        REQUIRE("3>#count{}>1" == to_string(get_clone(bdaggr(NAF::POS, AggregateFunction::COUNT, bound1(), condvec1()))));
        REQUIRE("3>#count{p:}>1" == to_string(get_clone(bdaggr(NAF::POS, AggregateFunction::COUNT, bound1(), condvec2()))));
        REQUIRE("3>#count{p:q}>1" == to_string(get_clone(bdaggr(NAF::POS, AggregateFunction::COUNT, bound1(), condvec3()))));
        REQUIRE("3>#count{p:q,r}>1" == to_string(get_clone(bdaggr(NAF::POS, AggregateFunction::COUNT, bound1(), condvec4()))));
        REQUIRE("3>#count{p:;q:}>1" == to_string(get_clone(bdaggr(NAF::POS, AggregateFunction::COUNT, bound1(), condvec5()))));
        // conjunction
        REQUIRE("p:" == to_string(get_clone(bdaggr(lit("p"), litvec()))));
        REQUIRE("p:q" == to_string(get_clone(bdaggr(lit("p"), litvec(lit("q"))))));
        // head tuple aggregate
        REQUIRE("3>#sum{}>1" == to_string(get_clone(hdaggr(AggregateFunction::SUM, bound1(), hdvec1()))));
        REQUIRE("3>#count{}>1" == to_string(get_clone(hdaggr(AggregateFunction::COUNT, bound1(), hdvec1()))));
        REQUIRE("3>#count{:p:}>1" == to_string(get_clone(hdaggr(AggregateFunction::COUNT, bound1(), hdvec2()))));
        REQUIRE("3>#count{1:p:q}>1" == to_string(get_clone(hdaggr(AggregateFunction::COUNT, bound1(), hdvec3()))));
        REQUIRE("3>#count{1,2:p:q,r}>1" == to_string(get_clone(hdaggr(AggregateFunction::COUNT, bound1(), hdvec4()))));
        REQUIRE("3>#count{:p:;:q:r}>1" == to_string(get_clone(hdaggr(AggregateFunction::COUNT, bound1(), hdvec5()))));
        // head lit aggregate
        REQUIRE("3>#sum{}>1" == to_string(get_clone(hdaggr(AggregateFunction::SUM, bound1(), condvec1()))));
        REQUIRE("3>#count{}>1" == to_string(get_clone(hdaggr(AggregateFunction::COUNT, bound1(), condvec1()))));
        REQUIRE("3>#count{p:}>1" == to_string(get_clone(hdaggr(AggregateFunction::COUNT, bound1(), condvec2()))));
        REQUIRE("3>#count{p:q}>1" == to_string(get_clone(hdaggr(AggregateFunction::COUNT, bound1(), condvec3()))));
        REQUIRE("3>#count{p:q,r}>1" == to_string(get_clone(hdaggr(AggregateFunction::COUNT, bound1(), condvec4()))));
        REQUIRE("3>#count{p:;q:}>1" == to_string(get_clone(hdaggr(AggregateFunction::COUNT, bound1(), condvec5()))));
        // disjunction
        REQUIRE("" == to_string(get_clone(hdaggr(condvec1()))));
        REQUIRE("p::" == to_string(get_clone(hdaggr(condvec2()))));
        REQUIRE("p::q" == to_string(get_clone(hdaggr(condvec3()))));
        REQUIRE("p::q,r" == to_string(get_clone(hdaggr(condvec4()))));
        REQUIRE("p::;q::" == to_string(get_clone(hdaggr(condvec5()))));
    }

    SECTION("hash") {
        // body tuple aggregate
        auto a1(bdaggr(NAF::POS, AggregateFunction::SUM, bound1(), bdvec1()));
        auto a2(bdaggr(NAF::POS, AggregateFunction::SUM, bound1(), bdvec2()));
        auto a3(bdaggr(NAF::NOT, AggregateFunction::SUM, bound1(), bdvec1()));
        auto a4(bdaggr(NAF::POS, AggregateFunction::COUNT, boundvec(), bdvec1()));
        CHECK(a1->hash() == get_clone(a1)->hash());
        CHECK(a2->hash() == get_clone(a2)->hash());
        CHECK(a3->hash() == get_clone(a3)->hash());
        CHECK(a4->hash() == get_clone(a4)->hash());
        CHECK(a1->hash() != a2->hash());
        CHECK(a1->hash() != a3->hash());
        CHECK(a1->hash() != a4->hash());
        CHECK(a2->hash() != a3->hash());
        CHECK(a2->hash() != a4->hash());
        CHECK(a3->hash() != a4->hash());
        // body lit aggregate
        auto b1(bdaggr(NAF::POS, AggregateFunction::SUM, bound1(), condvec1()));
        auto b2(bdaggr(NAF::POS, AggregateFunction::SUM, bound1(), condvec2()));
        auto b3(bdaggr(NAF::NOT, AggregateFunction::SUM, bound1(), condvec1()));
        auto b4(bdaggr(NAF::POS, AggregateFunction::COUNT, boundvec(), condvec1()));
        CHECK(b1->hash() == get_clone(b1)->hash());
        CHECK(b2->hash() == get_clone(b2)->hash());
        CHECK(b3->hash() == get_clone(b3)->hash());
        CHECK(b4->hash() == get_clone(b4)->hash());
        CHECK(b1->hash() != b2->hash());
        CHECK(b1->hash() != b3->hash());
        CHECK(b1->hash() != b4->hash());
        CHECK(b2->hash() != b3->hash());
        CHECK(b2->hash() != b4->hash());
        CHECK(b3->hash() != b4->hash());
        // conjunction
        auto c1(bdaggr(lit("p"), litvec()));
        auto c2(bdaggr(lit("p"), litvec(lit("q"))));
        CHECK(c1->hash() == get_clone(c1)->hash());
        CHECK(c2->hash() == get_clone(c2)->hash());
        CHECK(c1->hash() != c2->hash());
        // head tuple aggregate
        auto d1(hdaggr(AggregateFunction::SUM, bound1(), hdvec1()));
        auto d2(hdaggr(AggregateFunction::SUM, bound1(), hdvec2()));
        auto d3(hdaggr(AggregateFunction::COUNT, boundvec(), hdvec1()));
        CHECK(b1->hash() == get_clone(b1)->hash());
        CHECK(b2->hash() == get_clone(b2)->hash());
        CHECK(b3->hash() == get_clone(b3)->hash());
        CHECK(b1->hash() != b2->hash());
        CHECK(b1->hash() != b3->hash());
        CHECK(b2->hash() != b3->hash());
        // head lit aggregate
        auto e1(hdaggr(AggregateFunction::SUM, bound1(), condvec1()));
        auto e2(hdaggr(AggregateFunction::SUM, bound1(), condvec2()));
        auto e3(hdaggr(AggregateFunction::COUNT, boundvec(), condvec1()));
        CHECK(e1->hash() == get_clone(e1)->hash());
        CHECK(e2->hash() == get_clone(e2)->hash());
        CHECK(e3->hash() == get_clone(e3)->hash());
        CHECK(e1->hash() != e2->hash());
        CHECK(e1->hash() != e3->hash());
        CHECK(e2->hash() != e3->hash());
        // disjunction
        auto f1(hdaggr(condvec1()));
        auto f2(hdaggr(condvec2()));
        CHECK(f1->hash() == get_clone(f1)->hash());
        CHECK(f2->hash() == get_clone(f2)->hash());
        CHECK(f1->hash() != f2->hash());
    }

    SECTION("equal") {
        // body tuple aggregate
        auto a1(bdaggr(NAF::POS, AggregateFunction::SUM, bound1(), bdvec1()));
        auto a2(bdaggr(NAF::POS, AggregateFunction::SUM, bound1(), bdvec2()));
        auto a3(bdaggr(NAF::NOT, AggregateFunction::SUM, bound1(), bdvec1()));
        auto a4(bdaggr(NAF::POS, AggregateFunction::COUNT, boundvec(), bdvec1()));
        REQUIRE(*a1 == *get_clone(a1));
        REQUIRE(*a2 == *get_clone(a2));
        REQUIRE(*a3 == *get_clone(a3));
        REQUIRE(*a4 == *get_clone(a4));
        REQUIRE(!(*a1 == *a2));
        REQUIRE(!(*a1 == *a3));
        REQUIRE(!(*a1 == *a4));
        REQUIRE(!(*a2 == *a3));
        REQUIRE(!(*a2 == *a4));
        REQUIRE(!(*a3 == *a4));
        // body lit aggregate
        auto b1(bdaggr(NAF::POS, AggregateFunction::SUM, bound1(), condvec1()));
        auto b2(bdaggr(NAF::POS, AggregateFunction::SUM, bound1(), condvec2()));
        auto b3(bdaggr(NAF::NOT, AggregateFunction::SUM, bound1(), condvec1()));
        auto b4(bdaggr(NAF::POS, AggregateFunction::COUNT, boundvec(), condvec1()));
        REQUIRE(*b1 == *get_clone(b1));
        REQUIRE(*b2 == *get_clone(b2));
        REQUIRE(*b3 == *get_clone(b3));
        REQUIRE(*b4 == *get_clone(b4));
        REQUIRE(!(*b1 == *b2));
        REQUIRE(!(*b1 == *b3));
        REQUIRE(!(*b1 == *b4));
        REQUIRE(!(*b2 == *b3));
        REQUIRE(!(*b2 == *b4));
        REQUIRE(!(*b3 == *b4));
        // conjunction
        auto c1(bdaggr(lit("p"), litvec()));
        auto c2(bdaggr(lit("p"), litvec(lit("q"))));
        REQUIRE(*c1 == *get_clone(c1));
        REQUIRE(*c2 == *get_clone(c2));
        REQUIRE(!(*c1 == *c2));
        // head tuple aggregate
        auto d1(hdaggr(AggregateFunction::SUM, bound1(), hdvec1()));
        auto d2(hdaggr(AggregateFunction::SUM, bound1(), hdvec2()));
        auto d3(hdaggr(AggregateFunction::COUNT, boundvec(), hdvec1()));
        REQUIRE(*b1 == *get_clone(b1));
        REQUIRE(*b2 == *get_clone(b2));
        REQUIRE(*b3 == *get_clone(b3));
        REQUIRE(!(*b1 == *b2));
        REQUIRE(!(*b1 == *b3));
        REQUIRE(!(*b2 == *b3));
        // head lit aggregate
        auto e1(hdaggr(AggregateFunction::SUM, bound1(), condvec1()));
        auto e2(hdaggr(AggregateFunction::SUM, bound1(), condvec2()));
        auto e3(hdaggr(AggregateFunction::COUNT, boundvec(), condvec1()));
        REQUIRE(*e1 == *get_clone(e1));
        REQUIRE(*e2 == *get_clone(e2));
        REQUIRE(*e3 == *get_clone(e3));
        REQUIRE(!(*e1 == *e2));
        REQUIRE(!(*e1 == *e3));
        REQUIRE(!(*e2 == *e3));
        // disjunction
        auto f1(hdaggr(condvec1()));
        auto f2(hdaggr(condvec2()));
        REQUIRE(*f1 == *get_clone(f1));
        REQUIRE(*f2 == *get_clone(f2));
        REQUIRE(!(*f1 == *f2));
    }

    SECTION("unpool") {
        // body tuple aggregate
        REQUIRE("[3>#sum{1,3:;2,3:;1,4:;2,4:}>1]" == to_string(unpool(bdaggr(NAF::POS, AggregateFunction::SUM, bound1(), bdvec7()))));
        REQUIRE("[1>#sum{}>3,2>#sum{}>3,1>#sum{}>4,2>#sum{}>4]" == to_string(unpool(bdaggr(NAF::POS, AggregateFunction::SUM, bound2(), bdvec1()))));
        REQUIRE("[not 1>#min{}>3,not 2>#min{}>3,not 1>#min{}>4,not 2>#min{}>4]" == to_string(unpool(bdaggr(NAF::NOT, AggregateFunction::MIN, bound2(), bdvec1()))));
        REQUIRE("[3>#sum{1:p;1:q;2:p;2:q;:}>1]" == to_string(unpool(bdaggr(NAF::POS, AggregateFunction::SUM, bound1(), bdvec6()))));
        REQUIRE("[1>#sum{1:p;1:q;2:p;2:q;:},2>#sum{1:p;1:q;2:p;2:q;:}]" == to_string(unpool(bdaggr(NAF::POS, AggregateFunction::SUM, bound3(), bdvec6()))));
        // body lit aggregate
        REQUIRE("[1>#sum{}>3,2>#sum{}>3,1>#sum{}>4,2>#sum{}>4]" == to_string(unpool(bdaggr(NAF::POS, AggregateFunction::SUM, bound2(), condvec1()))));
        REQUIRE("[not 1>#min{}>3,not 2>#min{}>3,not 1>#min{}>4,not 2>#min{}>4]" == to_string(unpool(bdaggr(NAF::NOT, AggregateFunction::MIN, bound2(), condvec1()))));
        REQUIRE("[3>#sum{p:r;p:s;q:r;q:s;t:}>1]" == to_string(unpool(bdaggr(NAF::POS, AggregateFunction::SUM, bound1(), condvec6()))));
        REQUIRE("[1>#sum{p:r;p:s;q:r;q:s;t:},2>#sum{p:r;p:s;q:r;q:s;t:}]" == to_string(unpool(bdaggr(NAF::POS, AggregateFunction::SUM, bound3(), condvec6()))));
        // conjunction
        REQUIRE("[p|q:r]" == to_string(unpool(bdaggr(lit("p", "q"), litvec(lit("r"))))));
        REQUIRE("[p:q;p:r]" == to_string(unpool(bdaggr(lit("p"), litvec(lit("q", "r"))))));
        REQUIRE("[p|q:r;p|q:s]" == to_string(unpool(bdaggr(lit("p", "q"), litvec(lit("r", "s"))))));
        // head tuple aggregate
        REQUIRE("[3>#sum{1,3:p:;2,3:p:;1,4:p:;2,4:p:}>1]" == to_string(unpool(hdaggr(AggregateFunction::SUM, bound1(), hdvec7()))));
        REQUIRE("[1>#sum{}>3,2>#sum{}>3,1>#sum{}>4,2>#sum{}>4]" == to_string(unpool(hdaggr(AggregateFunction::SUM, bound2(), hdvec1()))));
        REQUIRE("[3>#sum{1:p:q;1:p:r;2:p:q;2:p:r;:s:}>1]" == to_string(unpool(hdaggr(AggregateFunction::SUM, bound1(), hdvec6()))));
        REQUIRE("[3>#sum{1:p:;1:q:}>1]" == to_string(unpool(hdaggr(AggregateFunction::SUM, bound1(), hdvec8()))));
        // head lit aggregate
        REQUIRE("[1>#sum{}>3,2>#sum{}>3,1>#sum{}>4,2>#sum{}>4]" == to_string(unpool(hdaggr(AggregateFunction::SUM, bound2(), condvec1()))));
        REQUIRE("[3>#sum{p:r;p:s;q:r;q:s;t:}>1]" == to_string(unpool(hdaggr(AggregateFunction::SUM, bound1(), condvec6()))));
        REQUIRE("[1>#sum{p:r;p:s;q:r;q:s;t:},2>#sum{p:r;p:s;q:r;q:s;t:}]" == to_string(unpool(hdaggr(AggregateFunction::SUM, bound3(), condvec6()))));
        // disjunction
        REQUIRE("[p:&q::r;p:&q::s;t::]" == to_string(unpool(hdaggr(condvec6()))));
    }

    SECTION("simplify") {
        // body tuple aggregate
        REQUIRE("((#Range0+0)>#sum{(#Range1+0):p((#Range2+0)),#range(#Range1,3,4),#range(#Range2,5,6)},[(#Range0,1,2)])" == simplify(bdaggr(NAF::POS, AggregateFunction::SUM, boundvec(Relation::LT, dterm(1,2)), bdelemvec(termvec(dterm(3,4)), litvec(dlit(5,6))))));
        // body lit aggregate
        REQUIRE("((#Range0+0)>#sum{p((#Range1+0)):p((#Range2+0)),#range(#Range1,3,4),#range(#Range2,5,6)},[(#Range0,1,2)])" == simplify(bdaggr(NAF::POS, AggregateFunction::SUM, boundvec(Relation::LT, dterm(1,2)), condlitvec(dlit(3,4), litvec(dlit(5,6))))));
        // conjunction
        REQUIRE("(p((#Range0+0))&#range(#Range0,1,2):p((#Range1+0)),#range(#Range1,3,4),[])" == simplify(bdaggr(dlit(1,2), litvec(dlit(3,4)))));
        // head tuple aggregate
        REQUIRE("((#Range0+0)>#sum{(#Range1+0):p((#Range2+0)):p((#Range3+0)),#range(#Range1,3,4),#range(#Range2,5,6),#range(#Range3,7,8)},[(#Range0,1,2)])" == simplify(hdaggr(AggregateFunction::SUM, boundvec(Relation::LT, dterm(1,2)), hdelemvec(termvec(dterm(3,4)), dlit(5,6), litvec(dlit(7,8))))));
        // head lit aggregate
        REQUIRE("((#Range0+0)>#sum{p((#Range1+0)):p((#Range2+0)),#range(#Range1,3,4),#range(#Range2,5,6)},[(#Range0,1,2)])" == simplify(hdaggr(AggregateFunction::SUM, boundvec(Relation::LT, dterm(1,2)), condlitvec(dlit(3,4), litvec(dlit(5,6))))));
        // disjunction
        REQUIRE("(p((#Range0+0)):#range(#Range0,1,2):p((#Range1+0)),#range(#Range1,3,4),[])" == simplify(hdaggr(condlitvec(dlit(1,2), litvec(dlit(3,4))))));
    }

    SECTION("rewriteArithmetics") {
        // body tuple aggregate
        REQUIRE("(#Arith0>#sum{(C+D):p(#Arith1),#Arith1=(E+F)},[{(A+B):#Arith0}],[])" == rewrite(bdaggr(NAF::POS, AggregateFunction::SUM, boundvec(Relation::LT, aterm("A","B")), bdelemvec(termvec(aterm("C","D")), litvec(alit("E","F"))))));
        REQUIRE("(#Arith0=#sum{(C+D):p(#Arith0)},[{(A+B):#Arith0}],[])" == rewrite(bdaggr(NAF::POS, AggregateFunction::SUM, boundvec(Relation::EQ, aterm("A","B")), bdelemvec(termvec(aterm("C","D")), litvec(alit("A","B"))))));
        // body lit aggregate
        REQUIRE("(#Arith0>#sum{p((C+D)):p(#Arith1),#Arith1=(E+F)},[{(A+B):#Arith0}],[])" == rewrite(bdaggr(NAF::POS, AggregateFunction::SUM, boundvec(Relation::LT, aterm("A","B")), condlitvec(alit("C","D"), litvec(alit("E","F"))))));
        REQUIRE("(#Arith0=#sum{p((C+D)):p(#Arith0)},[{(A+B):#Arith0}],[])" == rewrite(bdaggr(NAF::POS, AggregateFunction::SUM, boundvec(Relation::EQ, aterm("A","B")), condlitvec(alit("C","D"), litvec(alit("A","B"))))));
        // conjunction
        REQUIRE("(p(#Arith0)&#Arith0=(C+D):p(#Arith1),#Arith1=(E+F),[{}],[])" == rewrite(bdaggr(alit("C","D"), litvec(alit("E","F")))));
        // head tuple aggregate
        REQUIRE("(#Arith0>#sum{(C+D):p((E+F)):p(#Arith1),#Arith1=(G+H)},[{(A+B):#Arith0}])" == rewrite(hdaggr(AggregateFunction::SUM, boundvec(Relation::LT, aterm("A","B")), hdelemvec(termvec(aterm("C","D")), alit("E", "F"), litvec(alit("G","H"))))));
        // head lit aggregate
        REQUIRE("(#Arith0>#sum{p((C+D)):p(#Arith1),#Arith1=(E+F)},[{(A+B):#Arith0}])" == rewrite(hdaggr(AggregateFunction::SUM, boundvec(Relation::LT, aterm("A","B")), condlitvec(alit("C","D"), litvec(alit("E","F"))))));
        // disjunction
        REQUIRE("(p((C+D))::p(#Arith0),#Arith0=(E+F),[{}])" == rewrite(hdaggr(condlitvec(alit("C","D"), litvec(alit("E","F"))))));
    }

}

} } } // namespace Test Input Gringo

