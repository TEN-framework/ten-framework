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
#ifdef _MSC_VER
#pragma warning (disable: 4996)
#endif
#include <clasp/solver.h>
#include "lpcompare.h"
#include <utility>
#include "catch.hpp"
namespace Potassco {
static bool operator==(const LitSpan& lhs, const LitSpan& rhs) {
	return lhs.size == rhs.size && std::equal(begin(lhs), end(lhs), begin(rhs));
}
static bool operator==(const Sum_t& lhs, const Sum_t rhs) {
	return lhs.bound == rhs.bound && lhs.lits.size == rhs.lits.size && std::equal(begin(lhs.lits), end(lhs.lits), begin(rhs.lits));
}
static bool operator==(const Rule_t& lhs, const Rule_t& rhs) {
	return lhs.ht == rhs.ht && lhs.head.size == rhs.head.size && std::equal(begin(lhs.head), end(lhs.head), rhs.head.first)
		&&   lhs.normal() == rhs.normal() && (lhs.normal() ? lhs.cond == rhs.cond : lhs.agg == rhs.agg);
}
static bool operator==(const RuleBuilder& rb, const Rule_t& rhs) {
	return rb.rule() == rhs;
}
}
namespace Clasp { namespace Test {
using namespace Clasp::Asp;
TEST_CASE("Rule simplification", "[asp][rule]") {
	typedef LogicProgram::SRule SRule;
	typedef Potassco::RuleBuilder RuleBuilder;
	SharedContext ctx;
	LogicProgram  prg;
	RuleBuilder   mem;
	RuleBuilder   rule;
	SRule         meta;
	prg.startProgram(ctx);
	SECTION("testHashIgnoresOrder") {
		RuleBuilder r1, r2, r3;
		LogicProgram::SRule info1, info2, info3;
		r1.start().addHead(1).addGoal(Potassco::neg(10)).addGoal(20).addGoal(25).end();
		r2.start().addHead(1).addGoal(20).addGoal(25).addGoal(Potassco::neg(10)).end();
		r3.start().addHead(1).addGoal(25).addGoal(Potassco::neg(10)).addGoal(20).end();
		prg.simplifyRule(r1.rule(), mem, info1);
		prg.simplifyRule(r2.rule(), mem, info2);
		prg.simplifyRule(r3.rule(), mem, info3);
		REQUIRE((info1.hash == info2.hash && info2.hash == info3.hash));
		REQUIRE((info1.pos == info2.pos && info2.pos == info3.pos));
	}

	SECTION("testRemoveDuplicateInNormal") {
		// a :- b, b, not c -> a :- b, not c.
		rule.start(Head_t::Disjunctive).addHead(1).addGoal(2).addGoal(2).addGoal(Potassco::neg(3)).end();
		prg.simplifyRule(rule.rule(), mem, meta);
		REQUIRE(1 == meta.pos);
		REQUIRE(rule.start().addHead(1).addGoal(2).addGoal(Potassco::neg(3)) == mem.rule());
	}

	SECTION("testMergeDuplicateInExtended") {
		// a :- 2 {b, not c, b} -> a :- 2 [b=2, not c].
		rule.addHead(1).startSum(2).addGoal(2).addGoal(Potassco::neg(3)).addGoal(2).end();
		REQUIRE(prg.simplifyRule(rule.rule(), mem, meta));
		REQUIRE(1 == meta.pos);
		REQUIRE(rule.start().addHead(1).startSum(2).addGoal(2, 2).addGoal(Potassco::neg(3)) == mem.rule());
	}
	SECTION("testContraNormal") {
		// a :- b, c, not b.
		rule.start().addHead(1).addGoal(2).addGoal(3).addGoal(Potassco::neg(2)).end();
		REQUIRE(!prg.simplifyRule(rule.rule(), mem, meta));
	}

	SECTION("testNoContraExtended") {
		// a :- 2 {b, c, not b}.
		rule.addHead(1).startSum(2).addGoal(2).addGoal(3).addGoal(Potassco::neg(2)).end();
		REQUIRE(prg.simplifyRule(rule.rule(), mem, meta));
		Rule r = mem.rule();
		REQUIRE(r.bt == Body_t::Count);

		// a :- 4 {not b, b, b, c, d}.
		rule.start().addHead(1).startSum(4).addGoal(Potassco::neg(2)).addGoal(2).addGoal(2).addGoal(3).addGoal(4).end();
		REQUIRE(prg.simplifyRule(rule.rule(), mem, meta));
		r = mem.rule();
		REQUIRE(r.bt == Body_t::Sum);
		REQUIRE(r.head[0] == 1);

		// a :- 4 [b=2, c=1, not b=1, not c=2].
		rule.start().addHead(1).startSum(4).addGoal(2, 2).addGoal(3).addGoal(Potassco::neg(2)).addGoal(Potassco::neg(3), 2).end();
		REQUIRE(prg.simplifyRule(rule.rule(), mem, meta));
		r = mem.rule();
		REQUIRE(r.bt == Body_t::Sum);
		REQUIRE(r.head[0] == 1);
	}

	SECTION("testContraExtended") {
		// a :- 3 {b, c, not b, not c}.
		rule.addHead(1).startSum(3).addGoal(2).addGoal(3).addGoal(Potassco::neg(2)).addGoal(Potassco::neg(3)).end();
		REQUIRE(!prg.simplifyRule(rule.rule(), mem, meta));

		// a :- 4 [b=2, c=1, not b=1, not c=1].
		rule.start().addHead(1).startSum(4).addGoal(2, 2).addGoal(3).addGoal(Potassco::neg(2)).addGoal(Potassco::neg(3)).end();
		REQUIRE(!prg.simplifyRule(rule.rule(), mem, meta));
	}

	SECTION("testMultiSimplify") {
		// a :- 1 [b=0,c=0,d=2,e=0] -> a :- d.
		//  - remove 0 weights: 1 [d=2]
		//  - bound weights   : 1 [d=1]
		//  - flatten         : d.
		rule.addHead(1).startSum(1).addGoal(2, 0).addGoal(3, 0).addGoal(4, 2).addGoal(5, 0).end();
		REQUIRE(prg.simplifyRule(rule.rule(), mem, meta));
		REQUIRE(meta.pos == 1);
		REQUIRE(rule.start().addHead(1).addGoal(4) == mem.rule());
	}
	SECTION("testCardinalityIfAllWeightsEqual") {
		// a :- 3 [b=2,c=2, d=2,e=0] ->
		rule.addHead(1).startSum(3).addGoal(2, 2).addGoal(3, 2).addGoal(4, 2).addGoal(5, 0).end();
		REQUIRE(prg.simplifyRule(rule.rule(), mem, meta));
		REQUIRE(rule.start().addHead(1).startSum(2).addGoal(2).addGoal(3).addGoal(4).end() == mem.rule());

		// a :- 2 [b=1,c=2 b=1] -> 1 {b,c}
		rule.start().addHead(1).startSum(2).addGoal(2, 1).addGoal(3, 2).addGoal(2, 1).end();
		REQUIRE(prg.simplifyRule(rule.rule(), mem, meta));
		REQUIRE(rule.start().addHead(1).startSum(1).addGoal(2).addGoal(3) == mem.rule());
	}

	SECTION("testNormalIfMinWeightNeeded") {
		// a :- 8 [b=4,c=3, d=2,e=0] -> b,c,d
		rule.addHead(1).startSum(8).addGoal(2, 4).addGoal(3, 3).addGoal(4, 2).addGoal(5, 0).end();
		REQUIRE(prg.simplifyRule(rule.rule(), mem, meta));
		REQUIRE(rule.start().addHead(1).addGoal(2).addGoal(3).addGoal(4).end() == mem.rule());
	}
	SECTION("testSelfblockNormal") {
		// a :- not a.
		rule.start().addHead(1).addGoal(Potassco::neg(1)).end();
		REQUIRE(prg.simplifyRule(rule.rule(), mem, meta));
		REQUIRE(rule.start().addGoal(Potassco::neg(1)).end() == mem.rule());
	}

	SECTION("testTautNormal") {
		// a :- a, b.
		rule.start().addHead(1).addGoal(1).addGoal(2).end();
		REQUIRE(!prg.simplifyRule(rule.rule(), mem, meta));
	}

	SECTION("testTrivialDisjunctive") {
		// a :- x.
		rule.start().addHead(1).addGoal(2).end();
		REQUIRE(prg.simplifyRule(rule.rule(), mem, meta));
	}
	SECTION("testEmptyDisjunctive") {
		rule.start().addGoal(2).end();
		REQUIRE(prg.simplifyRule(rule.rule(), mem, meta));
	}

	SECTION("testDisjunctive") {
		// a | b :- x.
		rule.start().addHead(1).addHead(2).addGoal(3).end();
		REQUIRE((prg.simplifyRule(rule.rule(), mem, meta) && rule == mem.rule()));
	}

	SECTION("testRemoveDuplicateInDisjunctive") {
		// a | b | a :- x, x.
		rule.start().addHead(1).addHead(2).addHead(1).addGoal(3).addGoal(3).end();
		REQUIRE(prg.simplifyRule(rule.rule(), mem, meta));
		REQUIRE(rule.start().addHead(1).addHead(2).addGoal(3).end() == mem.rule());
	}

	SECTION("testDisjunctiveTAUT") {
		// a | b | c :- b, x.
		rule.start().addHead(1).addHead(2).addHead(3).addGoal(4).addGoal(2).end();
		REQUIRE(!prg.simplifyRule(rule.rule(), mem, meta));
	}

	SECTION("testDisjunctiveBLOCK") {
		// a | b | c :- x, not b.
		rule.start().addHead(1).addHead(2).addHead(3).addGoal(4).addGoal(Potassco::neg(2)).end();
		REQUIRE(prg.simplifyRule(rule.rule(), mem, meta));
		REQUIRE(rule.start().addHead(1).addHead(3).addGoal(4).addGoal(Potassco::neg(2)).end() == mem.rule());
	}
	SECTION("testDisjunctiveBLOCKALL") {
		// a | b :- not a, not b.
		rule.start().addHead(1).addHead(2).addGoal(Potassco::neg(1)).addGoal(Potassco::neg(2)).end();
		REQUIRE(prg.simplifyRule(rule.rule(), mem, meta));
		REQUIRE(rule.start().addGoal(Potassco::neg(1)).addGoal(Potassco::neg(2)).end() == mem.rule());
	}
};
TEST_CASE("Rule transformation", "[asp][rule]") {
	typedef Potassco::RuleBuilder RuleBuilder;
	SharedContext ctx;
	LogicProgram  prg;
	RuleBuilder   rule;
	prg.start(ctx, LogicProgram::AspOptions().noEq().noScc());
	for (Var v; (v = prg.newAtom()) != 7;) { ; }

	SECTION("testChoiceRuleEmpty") {
		prg.setExtendedRuleMode(LogicProgram::mode_transform_choice);
		prg.addRule(Head_t::Choice, Potassco::toSpan<Atom_t>(), Potassco::toSpan<Potassco::Lit_t>());
		REQUIRE(0u == prg.stats.rules[1][RuleStats::Choice]);
	}
	SECTION("testChoiceRuleOneHead") {
		prg.setExtendedRuleMode(LogicProgram::mode_transform_choice);
		lpAdd(prg, "{a}.");;

		prg.endProgram();
		std::stringstream exp;
		exp << "1 1 1 1 8 \n"
			  << "1 8 1 1 1 \n";
		REQUIRE(findSmodels(exp, prg));
	}

	SECTION("testChoiceRuleUseExtraHead") {
		lpAdd(prg, "{d,e,f,g}.");
		prg.setExtendedRuleMode(LogicProgram::mode_transform_choice);
		lpAdd(prg, "{a,b,c} :- d, e, not f, not g.");

		prg.endProgram();
		std::stringstream exp;
		exp << "1 1 2 1 9 8\n"      // a    :- auxBody, not auxA.
		    << "1 9 1 1 1\n"        // auxA :- not a.
		    << "1 2 2 1 10 8\n"     // b    :- auxBody, not auxB.
		    << "1 10 1 1 2\n"       // auxB :- not b.
		    << "1 3 2 1 11 8\n"     // c    :- auxBody, not auxC.
		    << "1 11 1 1 3\n"       // auxC :- not c.
		    << "1 8 4 2 6 7 4 5\n"; // auxB :- d, e, not f, not g.
		REQUIRE(findSmodels(exp, prg));
	}

	SECTION("testTrivialConstraintRule") {
		prg.setExtendedRuleMode(LogicProgram::mode_transform);
		Potassco::Atom_t a = 1;
		prg.addRule(Head_t::Disjunctive, Potassco::toSpan<Atom_t>(&a, 1), 0, Potassco::toSpan<Potassco::WeightLit_t>());
		prg.endProgram();
		REQUIRE(1u == prg.stats.rules[1].sum());
	}

	SECTION("testUnsatConstraintRule") {
		prg.setExtendedRuleMode(LogicProgram::mode_transform);
		lpAdd(prg, "a :- 2 {b}.");
		REQUIRE(0u == prg.stats.rules[0].sum());

		lpAdd(prg, "a :- 3 {b = 2}.");
		REQUIRE(0u == prg.stats.rules[0].sum());
	}
	SECTION("testDegeneratedConstraintRule") {
		prg.setExtendedRuleMode(LogicProgram::mode_transform);
		lpAdd(prg,
			"{b, c, d}."
			"a :- 3 { b, c, d }.");
		REQUIRE(0u == prg.stats.bodies[0][Body_t::Count]);
	}
	SECTION("testBoundEqOneExp") {
		prg.setExtendedRuleMode(LogicProgram::mode_transform_weight);
		lpAdd(prg,
			"{b, c, d}."
			"a :- 1 { b, c, d }.");
		prg.endProgram();
		REQUIRE(4u == prg.stats.rules[1].sum());
		std::stringstream exp;
		exp << "1 1 1 0 2 \n"
		    << "1 1 1 0 3 \n"
		    << "1 1 1 0 4 \n";
		REQUIRE(findSmodels(exp, prg));
	}
	SECTION("testBoundEqOneQuad") {
		prg.setExtendedRuleMode(LogicProgram::mode_transform_weight);
		lpAdd(prg,"{b, c, d}.");
		// a :- 1 { b, c, d }.
		rule.addHead(1).startSum(1).addGoal(2).addGoal(3).addGoal(4);
		RuleTransform tm(prg);
		REQUIRE(4u == tm.transform(rule.rule(), RuleTransform::strategy_allow_aux));
		prg.endProgram();
		std::stringstream exp;
		exp << "1 1 1 0 2 \n"
		    << "1 1 1 0 8 \n"
		    << "1 8 1 0 3 \n"
		    << "1 8 1 0 4 \n";
		REQUIRE(findSmodels(exp, prg));
	}
	SECTION("testSixThreeExp") {
		prg.setExtendedRuleMode(LogicProgram::mode_transform_weight);
		lpAdd(prg, "{b, c, d, e, f, g}.");
		// a :- 3 {b, c, d, e, f, g}
		rule.addHead(1).startSum(3).addGoal(2).addGoal(3).addGoal(4).addGoal(5).addGoal(6).addGoal(7);
		RuleTransform tm(prg);
		REQUIRE(20u == tm.transform(rule.rule(), RuleTransform::strategy_no_aux));
		prg.endProgram();
		std::stringstream exp;
		exp // starting with b
			<< "1 1 3 0 2 3 4 \n"
			<< "1 1 3 0 2 3 5 \n"
			<< "1 1 3 0 2 3 6 \n"
			<< "1 1 3 0 2 3 7 \n"
			<< "1 1 3 0 2 4 5 \n"
			<< "1 1 3 0 2 4 6 \n"
			<< "1 1 3 0 2 4 7 \n"
			<< "1 1 3 0 2 5 6 \n"
			<< "1 1 3 0 2 5 7 \n"
			<< "1 1 3 0 2 6 7 \n"
			// starting with c
			<< "1 1 3 0 3 4 5 \n"
			<< "1 1 3 0 3 4 6 \n"
			<< "1 1 3 0 3 4 7 \n"
			<< "1 1 3 0 3 5 6 \n"
			<< "1 1 3 0 3 5 7 \n"
			<< "1 1 3 0 3 6 7 \n"
			// starting with d
			<< "1 1 3 0 4 5 6 \n"
			<< "1 1 3 0 4 5 7 \n"
			<< "1 1 3 0 4 6 7 \n"
			// starting with e
			<< "1 1 3 0 5 6 7 \n";
		REQUIRE(findSmodels(exp, prg));
	}

	SECTION("testSixThreeQuad") {
		prg.setExtendedRuleMode(LogicProgram::mode_transform_weight);
		lpAdd(prg, "{b, c, d, e, f, g}.");
		// a :- 3 {b, c, d, e, f, g}
		rule.addHead(1).startSum(3).addGoal(2).addGoal(3).addGoal(4).addGoal(5).addGoal(6).addGoal(7);
		RuleTransform tm(prg);
		REQUIRE(18u == tm.transform(rule.rule()));
		REQUIRE(15u == prg.numAtoms());
		prg.endProgram();
		std::stringstream exp;
		exp
			<< "1 1 2 0 2 8 \n"   // a     :- b, (c,2)
			<< "1 1 1 0 9 \n"     // a     :- (c,3)

			<< "1 8 2 0 3 10 \n"  // (c,2) :- c, (d,1)
			<< "1 8 1 0 11 \n"    // (c,2) :- (d,2)
			<< "1 9 2 0 3 11 \n"  // (c,3) :- c, (d,2)
			<< "1 9 1 0 12 \n"    // (c,3) :- (d,3)

			<< "1 10 1 0 4 \n"    // (d,1) :- d.
			<< "1 10 1 0 13 \n"   // (d,1) :- (e,1)
			<< "1 11 2 0 4 13 \n" // (d,2) :- d, (e,1).
			<< "1 11 1 0 14 \n"   // (d,2) :- (e,2).
			<< "1 12 2 0 4 14 \n" // (d,3) :- d, (e,2).
			<< "1 12 3 0 5 6 7 \n"// (d,3) :- (e,2).

			<< "1 13 1 0 5 \n"    // (e,1) :- e.
			<< "1 13 1 0 15 \n"   // (e,1) :- (f,1).
			<< "1 14 2 0 5 15 \n" // (e,2) :- e, (f,1).
			<< "1 14 2 0 6 7 \n"  // (e,2) :- f,g.

			<< "1 15 1 0 6 \n"    // (f,1) :- f
			<< "1 15 1 0 7 \n";   // (f,1) :- g
		REQUIRE(findSmodels(exp, prg));
	}

	SECTION("testWeightSixFourExp") {
		prg.setExtendedRuleMode(LogicProgram::mode_transform_weight);
		lpAdd(prg, "{b, c, d, e, f, g}.");
		// a :- 4 {b=4, c=3, d=2, e=2, f=1, g=1}
		rule.addHead(1).startSum(4).addGoal(2, 4).addGoal(3, 3).addGoal(4, 2).addGoal(5, 2).addGoal(6, 1).addGoal(7, 1);
		RuleTransform tm(prg);
		REQUIRE(8u == tm.transform(rule.rule(), RuleTransform::strategy_no_aux));
		prg.endProgram();
		std::stringstream exp;
		exp // starting with b
			<< "1 1 1 0 2 \n"
			<< "1 1 2 0 3 4 \n"
			<< "1 1 2 0 3 5 \n"
			<< "1 1 2 0 3 6 \n"
			<< "1 1 2 0 3 7 \n"
			<< "1 1 2 0 4 5 \n"
			<< "1 1 3 0 4 6 7 \n"
			<< "1 1 3 0 5 6 7 \n";
		REQUIRE(findSmodels(exp, prg));
	}
	SECTION("testWeightFourExp") {
		prg.setExtendedRuleMode(LogicProgram::mode_transform_weight);
		Var a = 1, b = 2, c = 3, d = 4, e = 5;
		lpAdd(prg, "{b, c, d, e}.");
		// a :- 4 {b = 2, c = 2, d = 1, e = 1}.
		rule.addHead(a).startSum(4).addGoal(b, 2).addGoal(c, 2).addGoal(d, 1).addGoal(e, 1);
		RuleTransform tm(prg);
		REQUIRE(3u == tm.transform(rule.rule(), RuleTransform::strategy_no_aux));
		prg.endProgram();
		std::stringstream exp;
		exp
			<< "1 1 2 0 2 3 \n"
			<< "1 1 3 0 2 4 5 \n"
			<< "1 1 3 0 3 4 5 \n";
		REQUIRE(findSmodels(exp, prg));
	}

	SECTION("testWeightSixFourQuad") {
		prg.setExtendedRuleMode(LogicProgram::mode_transform_weight);
		lpAdd(prg, "{b, c, d, e, f, g}.");
		// a :- 4 {b=4, c=3, d=2, e=2, f=1, g=1}
		rule.addHead(1).startSum(4).addGoal(2, 4).addGoal(3, 3).addGoal(4, 2).addGoal(5, 2).addGoal(6, 1).addGoal(7, 1);
		RuleTransform tm(prg);
		REQUIRE(14u == tm.transform(rule.rule()));
		REQUIRE(13u == prg.numAtoms());
		prg.endProgram();
		std::stringstream exp;
		exp // head
			<< "1 1 1 0 2 \n"     // a     :- b.
			<< "1 1 1 0 8 \n"     // a     :- (c, 4)

			<< "1 8 2 0 3 9 \n"   // (c,4) :- c, (d,1).
			<< "1 8 1 0 10 \n"    // (c,4) :- (d, 4)

			<< "1 9 1 0 4 \n"     // (d,1) :- d.
			<< "1 9 1 0 11 \n"    // (d,1) :- (e, 1)
			<< "1 10 2 0 4 12 \n" // (d,4) :- d, (e,2)
			<< "1 10 3 0 5 6 7 \n"// (d,4) :- e,f,g.

			<< "1 11 1 0 5 \n"    // (e,1) :- e.
			<< "1 12 1 0 5 \n"    // (e,2) :- e.
			<< "1 11 1 0 13 \n"   // (e,1) :- (f,1).
			<< "1 12 2 0 6 7 \n"  // (e,2) :- f,g.

			<< "1 13 1 0 6 \n"    // (f,1) :- f.
			<< "1 13 1 0 7 \n";   // (f,1) :- g.

		REQUIRE(findSmodels(exp, prg));

	}

	SECTION("testWeightBug") {
		prg.setExtendedRuleMode(LogicProgram::mode_transform_weight);
		lpAdd(prg, "{b, c, d}.");
		// a :- 5 {b=3, c=3, d=1}
		rule.addHead(1).startSum(5).addGoal(2, 3).addGoal(3, 3).addGoal(4, 1);
		RuleTransform tm(prg);
		REQUIRE(2u == tm.transform(rule.rule()));
		REQUIRE(8u == prg.numAtoms());
		prg.endProgram();
		std::stringstream exp;
		exp
			<< "1 1 2 0 2 8 \n"// a    :- b, (c, 2)
			<< "1 8 1 0 3 \n"; // (c,2):- c.
		REQUIRE(findSmodels(exp, prg));
	}

	SECTION("testDegeneratedWeightRule") {
		prg.setExtendedRuleMode(LogicProgram::mode_transform_weight);
		lpAdd(prg, "{b, c, d}.");
		// a :- 20 {b=18, c=18, d=18}
		rule.addHead(1).startSum(20).addGoal(2, 18).addGoal(3, 18).addGoal(4, 18);
		Potassco::RuleBuilder mem;
		LogicProgram::SRule meta;
		prg.simplifyRule(rule.rule(), mem, meta);
		RuleTransform tm(prg);
		REQUIRE(3u == tm.transform(mem.rule(), RuleTransform::strategy_no_aux));
		prg.endProgram();
		std::stringstream exp;
		exp
			<< "1 1 2 0 2 3 \n"  // a :- b, c
			<< "1 1 2 0 2 4 \n"  // a :- b, d
			<< "1 1 2 0 3 4 \n"; // a :- c, d
		REQUIRE(findSmodels(exp, prg));
	}

	SECTION("testStupidWeightBug") {
		prg.setExtendedRuleMode(LogicProgram::mode_transform_weight);
		Var v8 = prg.newAtom();
		Var v9 = prg.newAtom();
		lpAdd(prg, "{a,b,c,d,e,f,g,h}.");
		// x :- 24 {a=31, b=29, c=29, d=28, e=21, f=15, g=8, h=5}
		rule.addHead(v9).startSum(24).addGoal(1, 31).addGoal(2, 29).addGoal(3, 29).addGoal(4, 28).addGoal(5, 21).addGoal(6, 15)
			.addGoal(7, 8).addGoal(v8, 5);

		RuleTransform tm(prg);
		uint32 prev = prg.numAtoms();
		REQUIRE(14u == tm.transform(rule.rule()));
		REQUIRE(prg.numAtoms() == prev+6);
		prg.endProgram();
		std::stringstream exp;
		exp  << "1 13 3 0 6 7 8 \n";
		REQUIRE(findSmodels(exp, prg));
	}

	SECTION("testWeightBogusNormal") {
		prg.setExtendedRuleMode(LogicProgram::mode_transform_weight);
		lpAdd(prg, "{a,b,c}.");
		// a :- 24 {b=12,c=12}.
		rule.addHead(1).startSum(24).addGoal(2, 12).addGoal(3, 12);
		RuleTransform tm(prg);
		uint32 prev = prg.numAtoms();
		REQUIRE(1u == tm.transform(rule.rule()));
		REQUIRE(prg.numAtoms() == prev);
	}

	SECTION("testShiftDisjunction") {
		rule.start().addHead(1).addHead(2).addGoal(3).addGoal(4);
		RuleTransform tm(prg);
		uint32 nr = tm.transform(rule.rule());
		REQUIRE(3 == nr);
	}
	SECTION("testMixedRule") {
		prg.setExtendedRuleMode(LogicProgram::mode_transform);
		lpAdd(prg,
			"{a} :- 3 {b=2, c=1, d=2, e=1}."
			"{b,c,d,e}.");
		REQUIRE(prg.endProgram());
		std::stringstream exp;
		exp
			<< "1 8 2 0 2 3 \n"  // aux :- b, c
			<< "1 8 2 0 2 4 \n"  // aux :- b, d
			<< "1 8 2 0 2 5 \n"  // aux :- b, e
			<< "1 1 2 1 9 8\n";  // a   :- not a', aux
		REQUIRE(findSmodels(exp, prg));
	}
	SECTION("testDisjMixedRule") {
		LogicProgram prg;
		SharedContext ctx;
		prg.start(ctx);
		prg.setExtendedRuleMode(LogicProgram::mode_transform);
		lpAdd(prg,
			"a|b :- 3{c=2, d=1, e=2, f=1}."
			"a :- b."
			"b :- a."
			"{c,d,e,f}.");
		REQUIRE(prg.endProgram());
		std::stringstream exp;
		exp
			<< "1 7 2 0 3 4 \n"   // aux :- c, d
			<< "1 7 2 0 3 5 \n"   // aux :- c, e
			<< "1 7 2 0 3 6 \n"   // aux :- c, f
			<< "8 2 1 2 1 0 7\n"; // a|b :- aux
		REQUIRE(findSmodels(exp, prg));
	}
}

} }
