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
#include "tests/input/lit_helper.hh"

#include <climits>
#include <sstream>

namespace Gringo { namespace Input { namespace Test {

using namespace Gringo::IO;

// {{{ auxiliary functions and classes

namespace {

ULit rewrite(ULit &&x) {
    Gringo::Test::TestGringoModule log;
    Projections project;
    Literal::RelationVec assign;
    AuxGen arithGen;
    SimplifyState state;
    Term::ArithmeticsMap arith;
    x->simplify(log, project, state);
    x->rewriteArithmetics(arith, assign, arithGen);
    return std::move(x);
}

} // namespace

// }}}

TEST_CASE("input-literal", "[input]") {

    SECTION("print") {
        REQUIRE("x" == to_string(pred(NAF::POS, val(ID("x")))));
        REQUIRE("not x" == to_string(pred(NAF::NOT, val(ID("x")))));
        REQUIRE("not not x" == to_string(pred(NAF::NOTNOT, val(ID("x")))));
        REQUIRE("x=y" == to_string(rel(Relation::EQ, val(ID("x")), val(ID("y")))));
        REQUIRE("#range(x,y,z)" == to_string(range(val(ID("x")), val(ID("y")), val(ID("z")))));
    }

    SECTION("clone") {
        REQUIRE("x" == to_string(get_clone(pred(NAF::POS, val(ID("x"))))));
        REQUIRE("not x" == to_string(get_clone(pred(NAF::NOT, val(ID("x"))))));
        REQUIRE("not not x" == to_string(get_clone(pred(NAF::NOTNOT, val(ID("x"))))));
        REQUIRE("x=x" == to_string(get_clone(rel(Relation::EQ, val(ID("x")), val(ID("x"))))));
        REQUIRE("#range(x,y,z)" == to_string(get_clone(range(val(ID("x")), val(ID("y")), val(ID("z"))))));
    }

    SECTION("hash") {
        CHECK(pred(NAF::POS, val(ID("x")))->hash() == pred(NAF::POS, val(ID("x")))->hash());
        CHECK(!(pred(NAF::POS, val(ID("x")))->hash() == pred(NAF::NOT, val(ID("x")))->hash()));
        CHECK(!(pred(NAF::POS, val(ID("x")))->hash() == pred(NAF::POS, val(ID("y")))->hash()));
        CHECK(rel(Relation::EQ, val(ID("x")), val(ID("x")))->hash() == rel(Relation::EQ, val(ID("x")), val(ID("x")))->hash());
        CHECK(!(rel(Relation::EQ, val(ID("x")), val(ID("x")))->hash() == rel(Relation::LT, val(ID("x")), val(ID("x")))->hash()));
        CHECK(!(rel(Relation::EQ, val(ID("x")), val(ID("x")))->hash() == rel(Relation::EQ, val(ID("y")), val(ID("x")))->hash()));
        CHECK(!(rel(Relation::EQ, val(ID("x")), val(ID("x")))->hash() == rel(Relation::EQ, val(ID("x")), val(ID("y")))->hash()));
        CHECK(range(val(ID("x")), val(ID("y")), val(ID("z")))->hash() == range(val(ID("x")), val(ID("y")), val(ID("z")))->hash());
        CHECK(!(range(val(ID("x")), val(ID("x")), val(ID("z")))->hash() == range(val(ID("x")), val(ID("y")), val(ID("z")))->hash()));
        CHECK(!(range(val(ID("x")), val(ID("y")), val(ID("x")))->hash() == range(val(ID("x")), val(ID("y")), val(ID("z")))->hash()));
        CHECK(!(range(val(ID("y")), val(ID("y")), val(ID("z")))->hash() == range(val(ID("x")), val(ID("y")), val(ID("z")))->hash()));
    }

    SECTION("equal") {
        REQUIRE(is_value_equal_to(pred(NAF::POS, val(ID("x"))), pred(NAF::POS, val(ID("x")))));
        REQUIRE(!is_value_equal_to(pred(NAF::POS, val(ID("x"))), pred(NAF::NOT, val(ID("x")))));
        REQUIRE(!is_value_equal_to(pred(NAF::POS, val(ID("x"))), pred(NAF::POS, val(ID("y")))));
        REQUIRE(is_value_equal_to(rel(Relation::EQ, val(ID("x")), val(ID("x"))), rel(Relation::EQ, val(ID("x")), val(ID("x")))));
        REQUIRE(!is_value_equal_to(rel(Relation::EQ, val(ID("x")), val(ID("x"))), rel(Relation::LT, val(ID("x")), val(ID("x")))));
        REQUIRE(!is_value_equal_to(rel(Relation::EQ, val(ID("x")), val(ID("x"))), rel(Relation::EQ, val(ID("y")), val(ID("x")))));
        REQUIRE(!is_value_equal_to(rel(Relation::EQ, val(ID("x")), val(ID("x"))), rel(Relation::EQ, val(ID("x")), val(ID("y")))));
        REQUIRE(is_value_equal_to(range(val(ID("x")), val(ID("y")), val(ID("z"))), range(val(ID("x")), val(ID("y")), val(ID("z")))));
        REQUIRE(!is_value_equal_to(range(val(ID("x")), val(ID("x")), val(ID("z"))), range(val(ID("x")), val(ID("y")), val(ID("z")))));
        REQUIRE(!is_value_equal_to(range(val(ID("x")), val(ID("y")), val(ID("x"))), range(val(ID("x")), val(ID("y")), val(ID("z")))));
        REQUIRE(!is_value_equal_to(range(val(ID("y")), val(ID("y")), val(ID("z"))), range(val(ID("x")), val(ID("y")), val(ID("z")))));
    }

    SECTION("unpool") {
        REQUIRE("[x,y,z]" == to_string(pred(NAF::POS, pool(val(ID("x")), val(ID("y")), val(ID("z"))))->unpool(false)));
        REQUIRE("[not x,not y,not z]" == to_string(pred(NAF::NOT, pool(val(ID("x")), val(ID("y")), val(ID("z"))))->unpool(false)));
        REQUIRE("[a!=x,a!=y,b!=x,b!=y]" == to_string(rel(Relation::NEQ, pool(val(ID("a")), val(ID("b"))), pool(val(ID("x")), val(ID("y"))))->unpool(false)));
    }

    SECTION("rewrite") {
        REQUIRE("p(3)" == to_string(rewrite(pred(NAF::POS, fun("p", binop(BinOp::ADD, val(NUM(1)), val(NUM(2))))))));
        REQUIRE("3>7" == to_string(rewrite(rel(Relation::GT, binop(BinOp::ADD, val(NUM(1)), val(NUM(2))), binop(BinOp::ADD, val(NUM(3)), val(NUM(4)))))));
    }

}

} } } // namespace Test Input Gringo

