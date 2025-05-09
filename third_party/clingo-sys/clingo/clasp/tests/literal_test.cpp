//
// Copyright (c) 2006-2017 Benjamin Kaufmann
//
// This file is part of Clasp. See http://www.cs.uni-potsdam.de/clasp/
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//
#include <clasp/solver_types.h>
#include "catch.hpp"
namespace Clasp { namespace Test {

static bool testBin(const Literal& p) {
	Antecedent ap(p);
	Antecedent aNotp(~p);
	REQUIRE_FALSE(ap.isNull());
	REQUIRE(Antecedent::Binary == ap.type());
	REQUIRE(p == ap.firstLiteral());

	REQUIRE_FALSE(aNotp.isNull());
	REQUIRE(Antecedent::Binary == aNotp.type());
	REQUIRE(~p == aNotp.firstLiteral());
	return true;
}
static bool testTern(const Literal& p, const Literal& q) {
	Antecedent app(p, q);
	Antecedent apn(p, ~q);
	Antecedent anp(~p, q);
	Antecedent ann(~p, ~q);

	REQUIRE_FALSE(app.isNull());
	REQUIRE(Antecedent::Ternary == app.type());
	REQUIRE_FALSE(apn.isNull());
	REQUIRE(Antecedent::Ternary == apn.type());
	REQUIRE_FALSE(anp.isNull());
	REQUIRE(Antecedent::Ternary == anp.type());
	REQUIRE_FALSE(ann.isNull());
	REQUIRE(Antecedent::Ternary == ann.type());

	REQUIRE((p == app.firstLiteral() && q == app.secondLiteral()));
	REQUIRE((p == apn.firstLiteral() && ~q == apn.secondLiteral()));
	REQUIRE((~p == anp.firstLiteral() && q == anp.secondLiteral()));
	REQUIRE((~p == ann.firstLiteral() && ~q == ann.secondLiteral()));
	return true;
}

TEST_CASE("Literal", "[core]") {
	Literal min = lit_true(), mid = posLit(varMax / 2), max = posLit(varMax - 1);
	SECTION("testCtor") {
		Literal p, q(42, true);
		REQUIRE(Var(0) == p.var());
		REQUIRE_FALSE(p.sign());
		REQUIRE_FALSE(p.flagged());

		REQUIRE(Var(42) == q.var());
		REQUIRE(q.sign());
		REQUIRE_FALSE(q.flagged());

		Literal x = posLit(7);
		Literal y = negLit(7);
		REQUIRE((x.var() == y.var() && y.var() == Var(7)));
		REQUIRE_FALSE(x.sign());
		REQUIRE(y.sign());
	}
	SECTION("testId") {
		uint32 minIdx	= min.id();
		uint32 maxIdx	= max.id();
		uint32 midIdx	= mid.id();

		REQUIRE(uint32(0) == minIdx);
		REQUIRE(uint32(1) == (~min).id());

		REQUIRE(uint32((max.var()*2)) == maxIdx);
		REQUIRE(uint32((max.var()*2)+1) == (~max).id());

		REQUIRE(uint32((mid.var()*2)) == midIdx);
		REQUIRE(uint32((mid.var()*2)+1) == (~mid).id());

	}
	SECTION("testIdIgnoresFlag") {
		Literal maxW = max;
		maxW.flag();
		REQUIRE(max.id() == maxW.id());
	}
	SECTION("testFromId") {
		REQUIRE(min == Literal::fromId(min.id()));
		REQUIRE(max == Literal::fromId(max.id()));
		REQUIRE(mid == Literal::fromId(mid.id()));
	}
	SECTION("testFlag") {
		Literal p = posLit(42);
		p.flag();
		REQUIRE(p.flagged());
		p.unflag();
		REQUIRE_FALSE(p.flagged());
	}
	SECTION("testFlagCopy") {
		Literal p = posLit(42);
		p.flag();
		Literal q = p;
		REQUIRE(q.flagged());
	}
	SECTION("testComplement") {
		Literal lit = posLit(7);
		Literal cLit = ~lit;
		REQUIRE(lit.var() == cLit.var());
		REQUIRE_FALSE(lit.sign());
		REQUIRE(cLit.sign());
		REQUIRE(lit == ~~lit);
	}
	SECTION("testComplementIsNotFlagged") {
		Literal lit = posLit(7);
		lit.flag();
		Literal cLit = ~lit;
		REQUIRE_FALSE(cLit.flagged());
	}

	SECTION("testEquality") {
		Literal p = posLit(42);
		Literal q = negLit(42);
		REQUIRE(p == p);
		REQUIRE(p == ~q);
		REQUIRE_FALSE(p == q);
		REQUIRE(Literal() == Literal());
	}

	SECTION("testValue") {
		REQUIRE(value_true == trueValue(lit_true()));
		REQUIRE(value_false == trueValue(lit_false()));
		REQUIRE(value_false == falseValue(lit_true()));
		REQUIRE(value_true == falseValue(lit_false()));
	}

	SECTION("testLess") {
		Literal p = posLit(42), q = negLit(42);
		REQUIRE_FALSE(p < p);
		REQUIRE_FALSE(q < q);
		REQUIRE(p < q);
		REQUIRE_FALSE(q < p);

		Literal one(1, false);
		Literal two(2, true);
		Literal negOne = ~one;
		REQUIRE(one < two);
		REQUIRE(negOne < two);
		REQUIRE_FALSE(two < negOne);
	}

	SECTION("testAntecedentNullPointer") {
		Antecedent a;
		Antecedent b( (Constraint*) 0 );
		REQUIRE(a.isNull());
		REQUIRE(b.isNull());
	}

	SECTION("testAntecedentPointer") {
		struct Con : Constraint {
			PropResult propagate(Solver&, Literal, uint32&) { return PropResult(true, true); }
			void reason(Solver&, Literal, LitVec&) {}
			Constraint* cloneAttach(Solver&) { return 0; }
		};

		Constraint* c = new Con;
		Antecedent a(c);
		REQUIRE_FALSE(a.isNull());
		REQUIRE(Antecedent::Generic == a.type());
		REQUIRE(c == a.constraint());
		c->destroy();
	}

	SECTION("testAntecedentBin") {
		REQUIRE(testBin(max));
		REQUIRE(testBin(min));
		REQUIRE(testBin(mid));
	}

	SECTION("testAntecedentTern") {
		REQUIRE(testTern(max, max));
		REQUIRE(testTern(max, mid));
		REQUIRE(testTern(max, min));
		REQUIRE(testTern(mid, max));
		REQUIRE(testTern(mid, mid));
		REQUIRE(testTern(mid, min));
		REQUIRE(testTern(min, max));
		REQUIRE(testTern(min, mid));
		REQUIRE(testTern(min, min));
	}

}
} }
