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
#include <clasp/logic_program.h>
#include <clasp/program_builder.h>
#include <clasp/solver.h>
#include <clasp/unfounded_check.h>
#include <clasp/minimize_constraint.h>
#include "lpcompare.h"
#include "catch.hpp"
using namespace std;
namespace Clasp { namespace Test {
using namespace Clasp::Asp;
struct ClauseObserver : public DecisionHeuristic {
	Literal doSelect(Solver&){return Literal();}
	void updateVar(const Solver&, Var, uint32) {}
	void newConstraint(const Solver&, const Literal* first, LitVec::size_type size, ConstraintType) {
		Clause c;
		for (LitVec::size_type i = 0; i < size; ++i, ++first) {
			c.push_back(*first);
		}
		std::sort(c.begin(), c.end());
		clauses_.push_back(c);
	}
	typedef std::vector<Literal> Clause;
	typedef std::vector<Clause> Clauses;
	Clauses clauses_;
};

TEST_CASE("Logic program types", "[asp]") {
	SECTION("testInvalidNodeId") {
		REQUIRE_THROWS_AS(PrgNode(PrgNode::noNode + 1), std::overflow_error);
	}
	SECTION("testMergeValue") {
		PrgAtom lhs(0), rhs(1);
		ValueRep ok[15] = {
			value_free, value_free, value_free,
			value_free, value_true, value_true,
			value_free, value_false, value_false,
			value_free, value_weak_true, value_weak_true,
			value_true, value_weak_true, value_true
		};
		ValueRep fail[4] = {value_true, value_false, value_weak_true, value_false};
		for (uint32 x = 0; x != 2; ++x) {
			for (uint32 i = 0; i != 15; i += 3) {
				lhs.clearLiteral(true);
				rhs.clearLiteral(true);
				REQUIRE(lhs.assignValue(ok[i+x]));
				REQUIRE(rhs.assignValue(ok[i+(!x)]));
				REQUIRE(mergeValue(&lhs, &rhs));
				REQUIRE((lhs.value() == ok[i+2] && rhs.value() == ok[i+2]));
			}
		}
		for (uint32 x = 0; x != 2; ++x) {
			for (uint32 i = 0; i != 4; i += 2) {
				lhs.clearLiteral(true);
				rhs.clearLiteral(true);
				REQUIRE(lhs.assignValue(fail[i+x]));
				REQUIRE(rhs.assignValue(fail[i+(!x)]));
				REQUIRE(!mergeValue(&lhs, &rhs));
			}
		}
	}

	SECTION("ConstString") {
		ConstString s1;
		ConstString s2("");
		ConstString s3("Hello World - abcdefghi");
		ConstString s4(s3);

		REQUIRE(s1 == s2);
		REQUIRE(s3 == s4);
		REQUIRE(s3.c_str() == s4.c_str());
	}
}
TEST_CASE("Logic program", "[asp]") {
	Var a = 1, b = 2, c = 3, d = 4, e = 5, f = 6;
	SharedContext ctx;
	LogicProgram  lp;
	SECTION("testIgnoreRules") {
		lpAdd(lp.start(ctx), "a :- a. b :- a.\n");
		REQUIRE(1u == lp.stats.rules[0].sum());
	}
	SECTION("testDuplicateRule") {
		lpAdd(lp.start(ctx, LogicProgram::AspOptions().iterations(1)),
			"{b}.\n"
			"a :- b.\n"
			"a :- b.\n");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE(lp.getLiteral(a) == lp.getLiteral(b));
	}

	SECTION("testNotAChoice") {
		lpAdd(lp.start(ctx),
			"{b}.\n"
			"{a} :- not b."
			"a :-not b.");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		ctx.master()->assume(~lp.getLiteral(b)) && ctx.master()->propagate();
		// if b is false, a must be true because a :- not b. is stronger than {a} :- not b.
		REQUIRE(ctx.master()->isTrue(lp.getLiteral(a)));
	}

	SECTION("testNotAChoiceMerge") {
		lpAdd(lp.start(ctx),
			"{b}.\n"
			"{a} :- b.\n"
			"a :- b, not c.");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE(lp.getLiteral(a) == lp.getLiteral(b));
	}

	SECTION("testMergeToSelfblocker") {
		lpAdd(lp.start(ctx),
			"a :- not b.\n"
			"b :- a.\n");
		REQUIRE_FALSE((lp.endProgram() && ctx.endInit()));
	}

	SECTION("testMergeToSelfblocker2") {
		lpAdd(lp.start(ctx),
			"a :- not d.\n"
			"a :- not c.\n"
			"b :- not c.\n"
			"c :- a.");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE(ctx.master()->isTrue(lp.getLiteral(a)));
		REQUIRE(ctx.numVars() == 0);
	}
	SECTION("testMergeToSelfblocker3") {
		lpAdd(lp.start(ctx),
			"b :- a.\n"
			"{c}.\n"
			"d :- 2 {a, b}.\n"
			"e :- 2 {a}.\n"
			"a :- not d, not e.\n");
		REQUIRE_FALSE(lp.endProgram());
	}

	SECTION("testDerivedTAUT") {
		lpAdd(lp.start(ctx),
			"{x1;x2}.\n"
			"x3 :- not x2.\n"
			"x4 :- x3.\n"
			"x3 :- x1, x4.\n");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE(ctx.numVars() == 2);
	}

	SECTION("testOneLoop") {
		lpAdd(lp.start(ctx),
			"a :- not b.\n"
			"b :- not a.\n");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE( 1u == ctx.numVars() );
		REQUIRE( 0u == ctx.numConstraints() );
		REQUIRE( lp.getLiteral(a) == ~lp.getLiteral(b) );
	}

	SECTION("testZeroLoop") {
		lpAdd(lp.start(ctx),
			"a :- b.\n"
			"b :- a.\n"
			"a :- not c.\n"
			"c :- not a.");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE( 1u == ctx.numVars() );
		REQUIRE( 0u == ctx.numConstraints() );
		REQUIRE( lp.getLiteral(a) == lp.getLiteral(b) );
	}

	SECTION("testEqSuccs") {
		lpAdd(lp.start(ctx),
			"{a;b}.\n"
			"c :- a, b.\n"
			"d :- a, b.\n");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE( 3u == ctx.numVars() );
		REQUIRE( 3u == ctx.numConstraints() );
		REQUIRE( lp.getLiteral(c) == lp.getLiteral(d) );
	}

	SECTION("testEqCompute") {
		lpAdd(lp.start(ctx),
			"{c}.\n"
			"a :- not c.\n"
			"a :- b.\n"
			"b :- a.\n"
			"  :- not b.");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE(ctx.master()->isTrue(lp.getLiteral(a)));
		REQUIRE(lp.getLiteral(a) == lp.getLiteral(b));
		REQUIRE(lp.isFact(a));
		REQUIRE(lp.isFact(b));
	}

	SECTION("testFactsAreAsserted") {
		lpAdd(lp.start(ctx),
			"a :- not b.\n"
			"c.");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE(ctx.master()->isTrue(lp.getLiteral(a)));
		REQUIRE(ctx.master()->isTrue(lp.getLiteral(c)));
	}
	SECTION("testSelfblockersAreAsserted") {
		lpAdd(lp.start(ctx),
			"x1 :- not x1.\n"
			"x2 :- not x1.\n");
		REQUIRE_FALSE((lp.endProgram() && ctx.endInit()));
	}
	SECTION("testConstraintsAreAsserted") {
		lpAdd(lp.start(ctx),
			"a :- not b.\n"
			"b :- not a.\n"
			"  :- a.");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE(ctx.master()->isFalse(lp.getLiteral(a)));
	}

	SECTION("testConflictingCompute") {
		lpAdd(lp.start(ctx),
			"  :- a.\n"
			"  :- not a.");
		REQUIRE_FALSE((lp.endProgram() && ctx.endInit()));
		REQUIRE(!ctx.ok());
	}
	SECTION("testForceUnsuppAtomFails") {
		lpAdd(lp.start(ctx), "a :- not b. :- not b.");
		REQUIRE_FALSE((lp.endProgram() && ctx.endInit()));
	}

	SECTION("testTrivialConflictsAreDeteced") {
		lpAdd(lp.start(ctx), "a :- not a. :- not a.");
		REQUIRE_FALSE((lp.endProgram() && ctx.endInit()));

	}
	SECTION("testBuildEmpty") {
		lp.start(ctx);
		lp.endProgram();
		REQUIRE(0u == ctx.numVars());
		REQUIRE(0u == ctx.numConstraints());
	}
	SECTION("testAddOneFact") {
		lpAdd(lp.start(ctx), "a.");
		REQUIRE(lp.endProgram());
		REQUIRE(0u == ctx.numVars());
		// a fact does not introduce a constraint, it is just a top-level assignment
		REQUIRE(0u == ctx.numConstraints());
		REQUIRE(lp.isFact(a));
	}

	SECTION("testTwoFactsOnlyOneVar") {
		lpAdd(lp.start(ctx), "a. b.");
		REQUIRE(lp.endProgram());
		REQUIRE(0u == ctx.numVars());
		REQUIRE(0u == ctx.numConstraints());
		REQUIRE(lp.isFact(a));
		REQUIRE(lp.isFact(b));
	}

	SECTION("testDontAddDuplicateSumBodies") {
		lpAdd(lp.start(ctx),
			"{a; b; c}.\n"
			"d :- 2 {a=1, b=2, not c=1}.\n"
			"e :- 2 {a=1, b=2, not c=1}.\n");
		REQUIRE(lp.endProgram());
		REQUIRE(lp.numBodies() == 2);
	}

	SECTION("testAddLargeSumBody") {
		VarVec atoms;
		Potassco::WLitVec lits;
		lp.start(ctx);
		for (uint32 i = 1; i != 21; ++i) { atoms.push_back(i); }
		lp.addRule(Head_t::Choice, Potassco::toSpan(atoms), Potassco::toSpan<Potassco::Lit_t>());

		atoms.assign(1, 20);
		for (int32 i = 1; i != 20; ++i) {
			Potassco::WeightLit_t wl = {i&1 ? Potassco::lit(i) : Potassco::neg(i), i};
			lits.push_back(wl);
		}
		lp.addRule(Head_t::Disjunctive, Potassco::toSpan(atoms), 10, Potassco::toSpan(lits));
		lits.clear();
		for (int32 i = 20; --i;) {
			Potassco::WeightLit_t wl = {i&1 ? Potassco::lit(i) : Potassco::neg(i), 20 - i};
			lits.push_back(wl);
		}
		lp.addRule(Head_t::Disjunctive, Potassco::toSpan(atoms), 10, Potassco::toSpan(lits));
		REQUIRE(lp.endProgram());
		REQUIRE(lp.numBodies() == 3u);
	}
	SECTION("testDontAddDuplicateSimpBodies") {
		lpAdd(lp.start(ctx),
			"{x1;x2;x3;x4}.\n"
			"x1 :- x2, x3, x4.\n"
			"x1 :- 8 {x3=2, x2=3, x4=4}.");
		REQUIRE(lp.endProgram());
		REQUIRE(lp.numBodies() == 2);
	}

	SECTION("testDontAddUnsupportedExtNoEq") {
		lpAdd(lp.start(ctx, LogicProgram::AspOptions().noEq()),
			"a :- not d.\n"
			"c :- a, d.\n"
			"b :- 2 {a, c, not d}. % -> a\n");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE(ctx.master()->isFalse(lp.getLiteral(c)));
		REQUIRE(ctx.master()->isTrue(lp.getLiteral(a)));
		REQUIRE(ctx.master()->isTrue(lp.getLiteral(b)));
	}

	SECTION("testAssertSelfblockers") {
		lpAdd(lp.start(ctx),
			"a :- b, not c.\n"
			"c :- b, not c.\n"
			"b.");
		// Program is unsat because b must be true and false at the same time.
		REQUIRE_FALSE((lp.endProgram() && ctx.endInit()));
	}

	SECTION("testRegressionR1") {
		lpAdd(lp.start(ctx), "b. b :- not c.");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE(lp.getLiteral(b) == lit_true());
		REQUIRE(lp.getLiteral(c) == lit_false());
		REQUIRE(ctx.numVars() == 0);
	}

	SECTION("testRegressionR2") {
		lpAdd(lp.start(ctx),
			"b :- not c.\n"
			"b :- not d.\n"
			"c :- not b.\n"
			"d :- not b.\n"
			"b.");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE(lp.getLiteral(b) == lit_true());
		REQUIRE(lp.getLiteral(c) == lit_false());
		REQUIRE(lp.getLiteral(d) == lit_false());
		REQUIRE(ctx.numVars() == 0);
	}
	SECTION("testFuzzBug") {
		lpAdd(lp.start(ctx), "x1 :- not x1, not x2, not x3, not x2, not x1, not x4.");
		REQUIRE_FALSE((lp.endProgram() && ctx.endInit()));
	}
	SECTION("testBackpropNoEqBug") {
		lpAdd(lp.start(ctx, LogicProgram::AspOptions().noEq().backpropagate()),
			"{x2;x3;x4} :- x5.\n"
			"x1 :- x7, x5.\n"
			"x5.\n"
			"x7 :- x2, x3.\n"
			"x7 :- x6.\n"
			"x6 :- x8.\n"
			"x8 :- x4.\n"
			":- x1.");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE(ctx.master()->isFalse(lp.getLiteral(6)));
	}

	SECTION("testCloneProgram") {
		lpAdd(lp.start(ctx),
			"{x1;x2;x3}.\n"
			"x4 :- x1, x2, x3.");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE( uint32(4) >= ctx.numVars() );
		REQUIRE( uint32(4) >= ctx.numConstraints() );

		SharedContext ctx2;
		REQUIRE((lp.clone(ctx2) && ctx2.endInit()));
		REQUIRE( ctx.numVars() == ctx2.numVars() );
		REQUIRE( ctx.numConstraints() == ctx2.numConstraints() );
		REQUIRE(!ctx.isShared());
		REQUIRE(!ctx2.isShared());
		REQUIRE(ctx.output.size() == ctx2.output.size() );
	}

	SECTION("testBug") {
		lpAdd(lp.start(ctx),
			"x1 :- x2.\n"
			"x2 :- x3, x1.\n"
			"x3 :- x4.\n"
			"x4 :-not x3.\n");
		REQUIRE_FALSE((lp.endProgram() && ctx.endInit()));
	}

	SECTION("testSatBodyBug") {
		lpAdd(lp.start(ctx),
			"{c;b;a}.\n"
			"a.\n"
			"b :- 1 {a, c}.\n"
			"d :- b, c.\n");
		REQUIRE(std::distance(lp.getAtom(c)->deps_begin(), lp.getAtom(c)->deps_end()) <= 2u);
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE(std::distance(lp.getAtom(c)->deps_begin(), lp.getAtom(c)->deps_end()) == 1u);
		REQUIRE(std::distance(lp.getAtom(b)->deps_begin(), lp.getAtom(b)->deps_end()) == 0u);
		REQUIRE(value_free == ctx.master()->value(lp.getLiteral(c).var()));
	}
	SECTION("testDepBodyBug") {
		lpAdd(lp.start(ctx),
			"{d;e;f}.\n"
			"a :- d, e.\n"
			"b :- a."
			"c :- b, f, a.\n");
		REQUIRE(lp.endProgram());
		REQUIRE((lp.getAtom(a)->deps_end() - lp.getAtom(a)->deps_begin()) == 2);
	}

	SECTION("testAddUnknownAtomToMinimize") {
		lpAdd(lp.start(ctx), "#minimize{a}.");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE(lp.hasMinimize());
	}

	SECTION("testWriteWeakTrue") {
		lpAdd(lp.start(ctx),
			"{c}.\n"
			"a :- not b, c.\n"
			"b :- not a.\n"
			"  :- not a.\n");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		Var a = lp.falseAtom();
		std::stringstream exp;
		exp << "1 " << a << " 1 1 3\n";
		REQUIRE(findSmodels(exp, lp));
	}

	SECTION("testSimplifyBodyEqBug") {
		lpAdd(lp.start(ctx),
			"{d;e;f}.\n"
			"a :- d,f.\n"
			"b :- d,f.\n"
			"c :- a, e, b.\n");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		PrgAtom* na = lp.getAtom(a);
		PrgAtom* nb = lp.getAtom(b);
		REQUIRE((nb->eq() && nb->id() == a));
		REQUIRE((na->deps_end() - na->deps_begin()) == 1);
	}

	SECTION("testNoEqSameLitBug") {
		lpAdd(lp.start(ctx, LogicProgram::AspOptions().noEq()),
			"{x4;x5}.\n"
			"x1 :- x4,x5.\n"
			"x2 :- x4,x5.\n"
			"x3 :- x1, x2.\n");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE(ctx.numVars() == 5);
	}

	SECTION("testAssertEqSelfblocker") {
		lpAdd(lp.start(ctx),
			"a :- not b, not c.\n"
			"a :- not d, not e.\n"
			"b :- not c.\n"
			"c :- not b.\n"
			"d :- not e.\n"
			"e :- not d.\n");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE(2u == ctx.numVars());
		REQUIRE(ctx.master()->isFalse(lp.getLiteral(a)));
	}

	SECTION("testAddClauses") {
		ClauseObserver o;
		ctx.master()->setHeuristic(&o, Ownership_t::Retain);
		lpAdd(lp.start(ctx),
			"a :- not b.\n"
			"a :- b, c.\n"
			"b :- not a.\n"
			"c :- not b.\n");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE(3u == ctx.numVars());
		REQUIRE(0u == ctx.master()->numAssignedVars());

		REQUIRE(8u == ctx.numConstraints());

		Var bodyNotB = 1;
		Var bodyBC = 3;
		REQUIRE(Var_t::Body == ctx.varInfo(3).type());
		Literal litA = lp.getLiteral(a);
		REQUIRE(lp.getLiteral(b) == ~lp.getLiteral(a));

		// a - HeadClauses
		ClauseObserver::Clause ac;
		ac.push_back(~litA);
		ac.push_back(posLit(bodyNotB));
		ac.push_back(posLit(bodyBC));
		std::sort(ac.begin(), ac.end());
		REQUIRE(std::find(o.clauses_.begin(), o.clauses_.end(), ac) != o.clauses_.end());

		// bodyNotB - Body clauses
		ClauseObserver::Clause cl;
		cl.push_back(negLit(bodyNotB)); cl.push_back(~lp.getLiteral(b));
		std::sort(cl.begin(), cl.end());
		REQUIRE(std::find(o.clauses_.begin(), o.clauses_.end(), cl) != o.clauses_.end());
		cl.clear();
		cl.push_back(posLit(bodyNotB)); cl.push_back(lp.getLiteral(b));
		std::sort(cl.begin(), cl.end());
		REQUIRE(std::find(o.clauses_.begin(), o.clauses_.end(), cl) != o.clauses_.end());
		cl.clear();
		cl.push_back(negLit(bodyNotB)); cl.push_back(litA);
		std::sort(cl.begin(), cl.end());
		REQUIRE(std::find(o.clauses_.begin(), o.clauses_.end(), cl) != o.clauses_.end());

		// bodyBC - Body clauses
		cl.clear();
		cl.push_back(negLit(bodyBC)); cl.push_back(lp.getLiteral(b));
		std::sort(cl.begin(), cl.end());
		REQUIRE(std::find(o.clauses_.begin(), o.clauses_.end(), cl) != o.clauses_.end());
		cl.clear();
		cl.push_back(negLit(bodyBC)); cl.push_back(lp.getLiteral(c));
		std::sort(cl.begin(), cl.end());
		REQUIRE(std::find(o.clauses_.begin(), o.clauses_.end(), cl) != o.clauses_.end());
		cl.clear();
		cl.push_back(posLit(bodyBC)); cl.push_back(~lp.getLiteral(b)); cl.push_back(~lp.getLiteral(c));
		std::sort(cl.begin(), cl.end());
		REQUIRE(std::find(o.clauses_.begin(), o.clauses_.end(), cl) != o.clauses_.end());
		cl.clear();
		cl.push_back(negLit(bodyBC)); cl.push_back(litA);
		std::sort(cl.begin(), cl.end());
		REQUIRE(std::find(o.clauses_.begin(), o.clauses_.end(), cl) != o.clauses_.end());
	}

	SECTION("testAddEmptyMinimizeConstraint") {
		lpAdd(lp.start(ctx), "#minimize{}.");
		REQUIRE(lp.endProgram());
		REQUIRE(ctx.hasMinimize());
	}

	SECTION("testNonTight") {
		lpAdd(lp.start(ctx),
			"b :- c.\n"
			"c :- b.\n"
			"b :- not a.\n"
			"c :- not a.\n"
			"a :- not b.\n");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE( lp.stats.sccs != 0 );
	}

	SECTION("testIgnoreCondFactsInLoops") {
		lpAdd(lp.start(ctx),
			"{a}.\n"
			"b :- a.\n"
			"a :- b.\n");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE( lp.stats.sccs == 0);
	}

	SECTION("testCrEqBug") {
		lpAdd(lp.start(ctx),
			"a.\n"
			"{b}.\n");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE(1u == ctx.numVars());
		REQUIRE(value_free == ctx.master()->value(lp.getLiteral(b).var()));
	}

	SECTION("testEqOverComp") {
		lpAdd(lp.start(ctx),
			"a :- not b.\n"
			"a :- b.\n"
			"b :- not c.\n"
			"c :- not a.\n");
		REQUIRE(lp.endProgram());
		REQUIRE(lp.getLiteral(a) == lp.getLiteral(b));
		REQUIRE((ctx.master()->numFreeVars() == 0 && ctx.master()->isTrue(lp.getLiteral(a))));
	}

	SECTION("testEqOverBodiesOfDiffType") {
		lpAdd(lp.start(ctx),
			"{a;b}.\n"
			"d :- 2{a, b, c}.\n"
			"c :- d.\n"
			"  :- b.\n");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE(3u >= ctx.numVars());
		REQUIRE(ctx.master()->isFalse(lp.getLiteral(b)));
		REQUIRE(ctx.master()->isFalse(lp.getLiteral(c)));
		REQUIRE(ctx.master()->isFalse(lp.getLiteral(d)));
	}

	SECTION("testNoBodyUnification") {
		Var g = 7;
		lpAdd(lp.start(ctx),
			"{a;b;c}.\n"
			"d :- a, g.\n"
			"d :- b.\n"
			"e :- not d.\n"
			"f :- not e.\n"
			"g :- d.\n"
			"g :- c.\n");
		REQUIRE((lp.endProgram() && ctx.sccGraph.get()));
		ctx.master()->addPost(new DefaultUnfoundedCheck(*ctx.sccGraph));
		REQUIRE(ctx.endInit());
		ctx.master()->assume(~lp.getLiteral(b));
		ctx.master()->propagate();
		ctx.master()->assume(~lp.getLiteral(c));
		ctx.master()->propagate();
		REQUIRE(ctx.master()->isFalse(lp.getLiteral(g)));
	}

	SECTION("testNoEqAtomReplacement") {
		lpAdd(lp.start(ctx),
			"{a;b}.\n"
			"c :- a.\n"
			"c :- b.\n"
			"d :- not c.\n"
			"e :- not d.\n");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		ctx.master()->assume(~lp.getLiteral(a));
		ctx.master()->propagate();
		ctx.master()->assume(~lp.getLiteral(b));
		ctx.master()->propagate();
		REQUIRE(ctx.master()->isFalse(lp.getLiteral(c)));
	}

	SECTION("testAllBodiesSameLit") {
		lpAdd(lp.start(ctx),
			"{a}.\n"
			"b :- not a.\n"
			"c :- not b.\n"
			"d :- b.\n"
			"d :- not c.\n"
			"b :- d.\n");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE(lp.getLiteral(b) == lp.getLiteral(d));
		REQUIRE(lp.getLiteral(a) != lp.getLiteral(c));
		ctx.master()->assume(lp.getLiteral(a)) && ctx.master()->propagate();
		REQUIRE(ctx.master()->value(lp.getLiteral(c).var()) == value_free);
		ctx.master()->assume(~lp.getLiteral(c)) && ctx.master()->propagate();
		REQUIRE((ctx.master()->numFreeVars() == 0 && ctx.master()->isTrue(lp.getLiteral(b))));
	}

	SECTION("testAllBodiesSameLit2") {
		lpAdd(lp.start(ctx),
			"{a;e}.\n"
			"b :- not a.\n"
			"c :- not b.\n"
			"d :- b.\n"
			"d :- not c.\n"
			"b :- d, e.\n");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE(lp.getLiteral(b) == lp.getLiteral(d));
		REQUIRE(lp.getLiteral(a) != lp.getLiteral(c));
		ctx.master()->assume(lp.getLiteral(a)) && ctx.master()->propagate();
		REQUIRE(ctx.master()->value(lp.getLiteral(c).var()) == value_free);
		ctx.master()->assume(~lp.getLiteral(c)) && ctx.master()->propagate();
		REQUIRE((ctx.master()->numFreeVars() == 0 && ctx.master()->isTrue(lp.getLiteral(b))));
		REQUIRE(ctx.numVars() == 4);
	}

	SECTION("testCompLit") {
		lpAdd(lp.start(ctx),
			"{d}.\n"
			"a :- not c.\n"
			"c :- not a.\n"
			"b :- a, c.\n"
			"e :- a, d, not c. % a == ~c -> {a,c} = F -> {a, not c} = {a, d}\n");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE(3u == ctx.numVars());
		REQUIRE(ctx.master()->isFalse(lp.getLiteral(b)));
	}

	SECTION("testFunnySelfblockerOverEqByTwo") {
		lpAdd(lp.start(ctx),
			"{a,b,c}.\n"
			"d :- a, b.\n"
			"e :- a, b.\n"
			"f :- b, c.\n"
			"g :- b, c.\n"
			"f :- d, not f.\n"
			"h :- e, not g.\n"
			"i :- a, h.\n"
			"% -> d == e, f == g -> {e, not g} == {d, not f} -> F"
			"% -> h == i are both unsupported!");
		Var h = 8, i = 9;
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE(ctx.master()->isFalse(lp.getLiteral(h)));
		REQUIRE(ctx.master()->isFalse(lp.getLiteral(i)));
	}

	SECTION("testSimplifyToTrue") {
		lpAdd(lp.start(ctx),
			"a.\n"
			"b :-  1 {c, a}.");
		REQUIRE(lp.endProgram());
		REQUIRE(lp.numBodies() <= 1);
	}

	SECTION("testSimplifyToCardBug") {
		lpAdd(lp.start(ctx),
			"a. b.\n"
			"c :- 168 {not a = 576, not b=381}.\n"
			"  :- c.\n");
		REQUIRE(lp.endProgram());
		REQUIRE(ctx.master()->numFreeVars() == 0);
	}

	SECTION("testSimplifyCompBug") {
		Var h = 8;
		lpAdd(lp.start(ctx, LogicProgram::AspOptions().iterations(1)),
			"a :- not b.\n"
			"a :- b.\n"
			"b :- not c.\n"
			"c :- not a.\n"
			"d. e. g. {f}.\n"
			"h :- d, e, b, f, g.\n");
		REQUIRE(lp.endProgram());
		PrgAtom* x = lp.getAtom(h);
		PrgBody* B = lp.getBody(x->supps_begin()->node());
		REQUIRE((B->size() == 2 && !B->goal(0).sign() && B->goal(1).sign()));
		REQUIRE(B->goal(0).var() == f);
		REQUIRE(B->goal(1).var() == c);
	}

	SECTION("testBPWeight") {
		lpAdd(lp.start(ctx),
			"{a, b, c, d}.\n"
			"e :-  2 {a=1, b=2, not c=2, d=1}.\n"
			"  :- e.");
		REQUIRE(lp.endProgram());
		REQUIRE(ctx.master()->isFalse(lp.getLiteral(b)));
		REQUIRE(ctx.master()->isTrue(lp.getLiteral(c)));
	}

	SECTION("testExtLitsAreFrozen") {
		Var g = 7, h = 8, i = 9;
		lpAdd(lp.start(ctx),
			"{a, b, c, d, e, f, g}.\n"
			" h :- 2 {b, c, d}.\n"
			"i :- 2 {c=1, d=2, e=1}.\n"
			"#minimize {f, g}.\n");
		REQUIRE(lp.endProgram());
		REQUIRE_FALSE(ctx.varInfo(lp.getLiteral(a).var()).frozen());
		REQUIRE(ctx.varInfo(lp.getLiteral(b).var()).frozen());
		REQUIRE(ctx.varInfo(lp.getLiteral(c).var()).frozen());
		REQUIRE(ctx.varInfo(lp.getLiteral(d).var()).frozen());
		REQUIRE(ctx.varInfo(lp.getLiteral(e).var()).frozen());
		REQUIRE(ctx.varInfo(lp.getLiteral(h).var()).frozen());
		REQUIRE(ctx.varInfo(lp.getLiteral(i).var()).frozen());

		// minimize lits only frozen if constraint is actually used
		REQUIRE_FALSE(ctx.varInfo(lp.getLiteral(f).var()).frozen());
		REQUIRE_FALSE(ctx.varInfo(lp.getLiteral(g).var()).frozen());
		ctx.minimize();
		REQUIRE(ctx.varInfo(lp.getLiteral(f).var()).frozen());
		REQUIRE(ctx.varInfo(lp.getLiteral(g).var()).frozen());
	}

	SECTION("testExternalsAreFrozen") {
		lpAdd(lp.start(ctx),
			"#external a.\n");
		REQUIRE(lp.endProgram());
		REQUIRE(lp.getLiteral(a).var() == 1);
		REQUIRE(ctx.varInfo(lp.getLiteral(a).var()).frozen());
	}

	SECTION("testComputeTrueBug") {
		lpAdd(lp.start(ctx),
			"a :- not b.\n"
			"b :- a.\n"
			"a :- c.\n"
			"c :- a.\n"
			"  :- not a.\n");
		REQUIRE_FALSE((lp.endProgram() && ctx.endInit()));
	}

	SECTION("testBackprop") {
		lpAdd(lp.start(ctx, LogicProgram::AspOptions().backpropagate()),
			"{e;f;g}.\n"
			"d :- e, a.\n"
			"a :- f, d.\n"
			"b :- e, g.\n"
			"c :- f, g.\n"
			"h :- e, not d.\n"
			"h :- f, not b.\n"
			"h :- g, not c.\n"
			"  :- h.\n");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE(ctx.numVars() == 0);
	}
	SECTION("testBackpropTrueCon") {
		Var x2 = 2, x3 = 3;
		lpAdd(lp.start(ctx, LogicProgram::AspOptions().backpropagate()),
			"x7.\n"
			"{x2;x3}.\n"
			"x4 :- not x6.\n"
			"   :- not x5.\n"
			"x5 :- 1 {not x2, not x3}.");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		ctx.master()->assume(lp.getLiteral(x2)) && ctx.master()->propagate();
		REQUIRE(ctx.master()->isFalse(lp.getLiteral(x3)));
		ctx.master()->undoUntil(0);
		ctx.master()->assume(lp.getLiteral(x3)) && ctx.master()->propagate();
		REQUIRE(ctx.master()->isFalse(lp.getLiteral(x2)));
	}
	SECTION("testBuildUnsat") {
		lpAdd(lp.start(ctx), "x1. :- x1.");
		REQUIRE_FALSE(lp.endProgram());
		std::stringstream exp("1 2 0 0\n0\n0\nB+\n0\nB-\n2\n0\n1\n");
		REQUIRE(compareSmodels(exp, lp));
	}

	SECTION("testDontAddOnePredsThatAreNotHeads") {
		lpAdd(lp.start(ctx),
			"x1 :-not x2, not x3.\n"
			"x3.\n"
			"#output a : x1.\n"
			"#output b : x2.\n"
			"#output c : x3.");
		REQUIRE(lp.endProgram());
		REQUIRE(0u == ctx.numVars());
		std::stringstream exp("1 3 0 0 \n0\n3 c\n0\nB+\n0\nB-\n0\n1\n");
		REQUIRE(compareSmodels(exp, lp));
	}

	SECTION("testDontAddDuplicateBodies") {
		lpAdd(lp.start(ctx),
			"a :- b, not c.\n"
			"d :- b, not c.\n"
			"b. c.");
		lp.addOutput("a", a).addOutput("b", b).addOutput("c", c);
		REQUIRE(lp.endProgram());
		REQUIRE(0u == ctx.numVars());
		std::stringstream exp("1 2 0 0 \n1 3 0 0 \n0\n2 b\n3 c\n0\nB+\n0\nB-\n0\n1\n");
		REQUIRE(compareSmodels(exp, lp));
	}
	SECTION("testDontAddUnsupported") {
		lpAdd(lp.start(ctx),
			"x1 :- x3, x2.\n"
			"x3 :- not x4.\n"
			"x2 :- x1.");
		REQUIRE(lp.endProgram());
		std::stringstream exp("1 3 0 0 \n0");
		REQUIRE(compareSmodels(exp, lp));
	}
	SECTION("testDontAddUnsupportedNoEq") {
		lpAdd(lp.start(ctx, LogicProgram::AspOptions().noEq()),
			"x1 :- x3, x2.\n"
			"x3 :- not x4.\n"
			"x2 :- x1.");
		lp.addOutput("a", a).addOutput("b", b).addOutput("c", c);
		REQUIRE(lp.endProgram());
		REQUIRE(ctx.numVars() <= 2u);
		std::stringstream exp("1 3 0 0 \n0\n3 c\n0\nB+\n0\nB-\n0\n1\n");
		REQUIRE(compareSmodels(exp, lp));
	}
	SECTION("testAddCardConstraint") {
		lpAdd(lp.start(ctx),
			"a :- 1 { not b, c, d }.\n"
			"{b;c}.");
		lp.addOutput("a", a).addOutput("b", b).addOutput("c", c);

		REQUIRE(lp.endProgram());
		REQUIRE(3u == ctx.numVars());

		std::stringstream exp("2 1 2 1 1 2 3 \n3 2 2 3 0 0 \n0\n1 a\n2 b\n3 c\n0\nB+\n0\nB-\n0\n1\n");
		REQUIRE(compareSmodels(exp, lp));
	}

	SECTION("testAddWeightConstraint") {
		lpAdd(lp.start(ctx),
			"a :- 2 {not b=1, c=2, d=2}.\n"
			"{b;c}.");
		lp.addOutput("a", a).addOutput("b", b).addOutput("c", c).addOutput("d", d);

		REQUIRE(lp.endProgram());
		REQUIRE(3u == ctx.numVars());
		std::stringstream exp("5 1 2 2 1 2 3 1 2 \n3 2 2 3 0 0 \n0\n1 a\n2 b\n3 c\n0\nB+\n0\nB-\n0\n1\n");
		REQUIRE(compareSmodels(exp, lp));
	}
	SECTION("testAddMinimizeConstraint") {
		lpAdd(lp.start(ctx),
			"a :- not b.\n"
			"b :- not a.\n"
			"#minimize{a}@0.\n"
			"#minimize{b}@1.\n"
			"#output a : a. #output b : b.");
		REQUIRE(lp.endProgram());
		std::stringstream exp;
		exp
			<< "6 0 1 0 1 1 \n"
			<< "6 0 1 0 2 1 \n"
			<< "1 1 1 1 2 \n"
			<< "1 2 1 1 1 \n"
			<< "0\n1 a\n2 b\n0\nB+\n0\nB-\n0\n1\n";
		REQUIRE(compareSmodels(exp, lp));
	}

	SECTION("testWriteMinimizeEq") {
		lpAdd(lp.start(ctx),
			"{a}.\n"
			"b :- a.\n"
			"#minimize{b}@0.\n"
		);
		REQUIRE(lp.endProgram());
		REQUIRE(lp.inProgram(1));
		REQUIRE_FALSE(lp.inProgram(2));
		std::stringstream exp("6 0 1 0 2 1");
		REQUIRE(findSmodels(exp, lp));
	}
	SECTION("testEqOverChoiceRule") {
		lpAdd(lp.start(ctx),
			"{a}.\n"
			"b :- a.\n");
		REQUIRE(lp.endProgram());
		REQUIRE(1u == ctx.numVars());
		std::stringstream exp;
		exp
			<< "3 1 1 0 0 \n"
			<< "1 2 1 0 1 \n"
			<< "0\n0\nB+\n0\nB-\n0\n1\n";
		REQUIRE(compareSmodels(exp, lp));
	}
	SECTION("testRemoveKnownAtomFromWeightRule") {
		lpAdd(lp.start(ctx),
			"{c, d}.\n"
			"a."
			"e :- 5 {a = 2, not b = 2, c = 1, d = 1}.\n");
		REQUIRE(lp.endProgram());
		std::stringstream exp;
		exp << "1 1 0 0 \n"       // a.
			<< "3 2 3 4 0 0 \n"   // {c, d}.
			<< "2 5 2 0 1 3 4 \n" // e :- 1 { c, d }.
			<< "0\n";
		REQUIRE(compareSmodels(exp, lp));
	}

	SECTION("testMergeEquivalentAtomsInConstraintRule") {
		lpAdd(lp.start(ctx),
			"{a, c}.\n"
			"a :- b.\n"
			"b :- a.\n"
			"d :-  2 {a, b, c}.\n");
		REQUIRE(lp.endProgram());
		std::stringstream exp("5 4 2 2 0 1 3 2 1");
		// d :-  2 [a=2, c].
		REQUIRE(findSmodels(exp, lp));
	}

	SECTION("testMergeEquivalentAtomsInWeightRule") {
		lpAdd(lp.start(ctx),
			"{a, c, e}.\n"
			"a :- b.\n"
			"b :- a.\n"
			"d :-  3 {a = 1, c = 4, b = 2, e=1}.\n");
		REQUIRE(lp.endProgram());
		// x :-  3 [a=3, c=3,d=1].
		std::stringstream exp("5 4 3 3 0 1 3 5 3 3 1");
		REQUIRE(findSmodels(exp, lp));
	}

	SECTION("testBothLitsInConstraintRule") {
		lpAdd(lp.start(ctx),
			"{a}.\n"
			"b :- a.\n"
			"c :- b.\n"
			"d :-  1 {a, b, not b, not c}.\n");
		REQUIRE(lp.endProgram());
		// d :-  1 [a=2, not a=2].
		std::stringstream exp("5 4 1 2 1 1 1 2 2");
		REQUIRE(findSmodels(exp, lp));
	}

	SECTION("testBothLitsInWeightRule") {
		lpAdd(lp.start(ctx),
			"{a, e}.\n"
			"b :- a.\n"
			"c :- b.\n"
			"d :-  3 {a=3, not b=1, not c=3, e=2}.\n");
		REQUIRE(lp.endProgram());
		// d :-  3 [a=3, not a=4, e=2].
		std::stringstream exp("5 4 3 3 1 1 1 5 4 3 2");
		REQUIRE(findSmodels(exp, lp));
	}

	SECTION("testWeightlessAtomsInWeightRule") {
		lpAdd(lp.start(ctx),
			"{a, b, c}.\n"
			"d :-  1 {a=1, b=1, c=0}.\n");
		REQUIRE(lp.endProgram());
		// d :-  1 {a, b}.
		std::stringstream exp("2 4 2 0 1 1 2");
		REQUIRE(findSmodels(exp, lp));
	}

	SECTION("testSimplifyToNormal") {
		lpAdd(lp.start(ctx),
			"{a}.\n"
			"b :-  2 {a=2,not c=1}.\n");
		REQUIRE(lp.endProgram());
		// b :-  a.
		std::stringstream exp("1 2 1 0 1");
		REQUIRE(findSmodels(exp, lp));
	}
	SECTION("writeIntegrityConstraint") {
		lpAdd(lp.start(ctx),
			"{a;b;c}."
			"a :- c, not b.\n"
			"b :- c, not b.\n");
		REQUIRE(lp.endProgram());

		// falseAtom :- c, not b.
		// compute {not falseAtom}.
		std::stringstream exp("1 4 2 1 2 3\nB-\n4");
		REQUIRE(findSmodels(exp, lp));
	}
	SECTION("testBackpropWrite") {
		lpAdd(lp.start(ctx, LogicProgram::AspOptions().backpropagate()),
			"a|b.\n"
			"c :- a.\n"
			"a :- c.\n"
			"d :- not c.\n"
			"e :- a,c.\n"
			"  :- e.\n");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE(ctx.numVars() == 0);
		REQUIRE(ctx.master()->isFalse(lp.getLiteral(a)));
		REQUIRE(ctx.master()->isFalse(lp.getLiteral(c)));
		REQUIRE(ctx.master()->isTrue(lp.getLiteral(b)));
		REQUIRE(ctx.master()->isTrue(lp.getLiteral(d)));

		std::stringstream exp("1 2 0 0\n1 4 0 0");
		REQUIRE(findSmodels(exp, lp));
		REQUIRE(!lp.isDefined(a));
		REQUIRE(!lp.isDefined(c));
	}

	SECTION("testStatisticsObject") {
		StatisticObject stats = StatisticObject::map(&lp.stats);
		for (uint32 i = 0, end = stats.size(); i != end; ++i) {
			const char* k = stats.key(i);
			StatisticObject x = stats.at(k);
			REQUIRE(x.value() == 0.0);
		}
		lpAdd(lp.start(ctx),
			"a :- not b.\n"
			"b :- not a.\n");
		lp.endProgram();
		REQUIRE(stats.at("atoms").value() == (double)lp.stats.atoms);
		REQUIRE(stats.at("rules").value() == (double)lp.stats.rules[0].sum());
	}
	SECTION("testFactIsDefined") {
		lp.start(ctx);
		lpAdd(lp, "a.");
		lp.endProgram();
		REQUIRE(lp.isDefined(a));
	}
	SECTION("testBogusAtomIsNotDefined") {
		lp.start(ctx);
		lp.endProgram();
		REQUIRE_FALSE(lp.isDefined(0xDEAD));
		REQUIRE_FALSE(lp.isExternal(0xDEAD));
		REQUIRE(lit_false() == lp.getLiteral(0xDEAD));
	}
	SECTION("testMakeAtomicCondition") {
		lpAdd(lp.start(ctx), "{a;b}.");
		Potassco::Lit_t cond[2] = { Potassco::lit(a), Potassco::neg(b) };
		Id_t c1 = lp.newCondition(Potassco::toSpan(cond, 1));
		Id_t c2 = lp.newCondition(Potassco::toSpan(cond + 1, 1));
		REQUIRE(lp.endProgram());
		REQUIRE(lp.getLiteral(c1) == lp.getLiteral(a));
		REQUIRE(lp.getLiteral(c2) == ~lp.getLiteral(b));
	}

	SECTION("testMakeComplexCondition") {
		lpAdd(lp.start(ctx), "{a;b}.");
		Potassco::Lit_t cond[2] = { Potassco::lit(a), Potassco::neg(b) };
		Id_t c1 = lp.newCondition(Potassco::toSpan(cond, 0));
		Id_t c2 = lp.newCondition(Potassco::toSpan(cond, 2));
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE(lp.getLiteral(c1) == lit_true());
		Literal c2Lit = lp.getLiteral(c2);
		Solver& s = *ctx.master();
		REQUIRE(value_free == s.value(c2Lit.var()));
		REQUIRE((s.assume(c2Lit) && s.propagate()));
		REQUIRE(s.isTrue(lp.getLiteral(a)));
		REQUIRE(s.isFalse(lp.getLiteral(b)));
	}
	SECTION("testMakeFalseCondition") {
		lp.start(ctx);
		Var a = lp.newAtom();
		Potassco::Lit_t cond[2] ={Potassco::lit(a), Potassco::neg(a)};
		Id_t c1 = lp.newCondition(Potassco::toSpan(cond, 2));
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE(lp.getLiteral(c1) == lit_false());
		REQUIRE_THROWS_AS(solverLiteral(lp, c1), std::logic_error);
	}
	SECTION("testMakeInvalidCondition") {
		lp.start(ctx);
		Var a = lp.newAtom();
		Var b = lp.newAtom();
		Potassco::Lit_t cond[2] ={Potassco::lit(a), Potassco::neg(b)};
		Id_t c1 = lp.newCondition(Potassco::toSpan(cond, 2));
		Potassco::Lit_t cAsLit = static_cast<Potassco::Lit_t>(c1);
		REQUIRE_THROWS_AS(lp.newCondition(Potassco::toSpan(&cAsLit, 1)), std::overflow_error);
	}
	SECTION("testExtractCondition") {
		lpAdd(lp.start(ctx), "{a;b}.");
		Potassco::Lit_t cond[2] = { Potassco::lit(a), Potassco::lit(b) };
		Id_t c1 = lp.newCondition(Potassco::toSpan(cond, 2));
		REQUIRE(lp.endProgram());

		Potassco::LitVec ext;
		lp.extractCondition(c1, ext);
		REQUIRE((ext.size() == 2 && std::equal(ext.begin(), ext.end(), cond)));

		lp.dispose(false);
		REQUIRE_THROWS_AS(lp.extractCondition(c1, ext), std::logic_error);
	}
	SECTION("testGetConditionAfterDispose") {
		lpAdd(lp.start(ctx), "{a;b}.");
		Potassco::Lit_t cond[2] = { Potassco::lit(a), Potassco::lit(b) };
		Id_t c1 = lp.newCondition(Potassco::toSpan(cond, 2));
		REQUIRE(lp.endProgram());

		REQUIRE(!isSentinel(lp.getLiteral(c1)));
		lp.dispose(false);
		REQUIRE_THROWS_AS(lp.getLiteral(c1), std::logic_error);
	}

	SECTION("testTheoryAtomsAreFrozen") {
		lpAdd(lp.start(ctx), "b :- a.");
		Potassco::TheoryData& t = lp.theoryData();
		t.addTerm(0, "Theory");
		t.addAtom(a, 0, Potassco::toSpan<Potassco::Id_t>());
		lp.endProgram();
		REQUIRE(lp.getLiteral(a) != lit_false());
		REQUIRE(lp.isExternal(a));
		REQUIRE(lp.getLiteral(b) != lit_false());
		ctx.endInit();
		REQUIRE(ctx.varInfo(lp.getLiteral(a).var()).frozen());
	}

	SECTION("testAcceptIgnoresAuxChoicesFromTheoryAtoms") {
		lp.start(ctx);
		a = lp.newAtom();
		Potassco::TheoryData& t = lp.theoryData();
		t.addTerm(0, "Theory");
		t.addAtom(a, 0, Potassco::toSpan<Potassco::Id_t>());
		lp.endProgram();
		REQUIRE(lp.getLiteral(a) != lit_false());
		std::stringstream str;
		AspParser::write(lp, str, AspParser::format_aspif);
		for (std::string x; std::getline(str, x);) {
			REQUIRE(x.find("1 1") != 0);
			REQUIRE(x.find("5 1") != 0);
		}
		ctx.endInit();
		REQUIRE(ctx.varInfo(lp.getLiteral(a).var()).frozen());
	}
	SECTION("testFalseHeadTheoryAtomsAreRemoved") {
		lpAdd(lp.start(ctx), "a :- b.");
		Potassco::TheoryData& t = lp.theoryData();
		t.addTerm(0, "Theory");
		t.addAtom(a, 0, Potassco::toSpan<Potassco::Id_t>());
		lp.endProgram();
		REQUIRE(lp.getLiteral(a) == lit_false());
		REQUIRE(lp.getLiteral(b) == lit_false());
		REQUIRE(t.numAtoms() == 0);
	}

	SECTION("testTheoryHeadEvenIfRuleIsDropped") {
		lpAdd(lp.start(ctx),
			":- b.\n"
			"a :- b, c.\n");
		Potassco::TheoryData& t = lp.theoryData();
		t.addTerm(0, "ta");
		t.addTerm(1, "tc");
		t.addAtom(a, 0, Potassco::toSpan<Potassco::Id_t>());
		t.addAtom(c, 1, Potassco::toSpan<Potassco::Id_t>());
		REQUIRE(t.numAtoms() == 2);
		lp.endProgram();
		REQUIRE(lp.getLiteral(a) == lit_false());
		REQUIRE(lp.getLiteral(b) == lit_false());
		REQUIRE(lp.getLiteral(c) != lit_false());
		REQUIRE(t.numAtoms() == 1);
	}

	SECTION("testTheoryHeadEvenIfRuleIsDroppedLater") {
		lpAdd(lp.start(ctx),
			"{c}.\n"
			"b :- c.\n"
			":- c.\n");
		Potassco::TheoryData& t = lp.theoryData();
		t.addTerm(0, "x");
		t.addTerm(1, "c");
		Potassco::Id_t term = 1;
		t.addElement(0, Potassco::toSpan(&term, 1), 0);
		term = 0;
		t.addAtom(b, 0, Potassco::toSpan(&term, 1));
		REQUIRE(t.numAtoms() == 1);
		lp.endProgram();
		REQUIRE(lp.getLiteral(a) == lit_false());
		REQUIRE(lp.getLiteral(b) == lit_false());
		REQUIRE(lp.getLiteral(c) == lit_false());
		REQUIRE(t.numAtoms() == 0);
	}

	SECTION("testTheoryHeadEvenIfRuleIsSkipped") {
		lpAdd(lp.start(ctx),
			"{a}.\n"
			"b :- a, not a.\n");
		Potassco::TheoryData& t = lp.theoryData();
		t.addTerm(0, "ta");
		t.addAtom(b, 0, Potassco::toSpan<Potassco::Id_t>());
		REQUIRE(t.numAtoms() == 1);
		lp.endProgram();
		REQUIRE(lp.getLiteral(a) != lit_false());
		REQUIRE(lp.getLiteral(b) == lit_false());
		REQUIRE(t.numAtoms() == 0);
	}

	SECTION("testFalseBodyTheoryAtomsAreKept") {
		lp.start(ctx);
		a = lp.newAtom();
		Potassco::TheoryData& t = lp.theoryData();
		t.addTerm(0, "Theory");
		t.addAtom(a, 0, Potassco::toSpan<Potassco::Id_t>());
		lpAdd(lp, ":- a.");
		lp.endProgram();
		REQUIRE(lp.getLiteral(a) == lit_false());
		REQUIRE(t.numAtoms() == 1);
		std::stringstream str;
		AspParser::write(lp, str, AspParser::format_aspif);
		REQUIRE(str.str().find("1 0 0 0 1 1") != std::string::npos);
	}
	SECTION("testOutputFactsNotSupportedInSmodels") {
		lp.start(ctx);
		lp.addOutput("Hallo", Potassco::toSpan<Potassco::Lit_t>());
		lp.addOutput("World", Potassco::toSpan<Potassco::Lit_t>());
		lp.endProgram();
		REQUIRE_FALSE(lp.supportsSmodels());
	}
	SECTION("testDisposeBug") {
		lp.start(ctx);
		lp.theoryData().addTerm(0, 99);
		lp.start(ctx);
		REQUIRE_FALSE(lp.theoryData().hasTerm(0));
	}
}

TEST_CASE("Incremental logic program", "[asp]") {
	SharedContext ctx;
	LogicProgram lp;
	Var a = 1, b = 2, c = 3, d = 4, e = 5, f = 6;
	SECTION("testSimple") {
		lp.start(ctx);
		lp.updateProgram();
		lpAdd(lp, "a :- not b. b :- not a.");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE(ctx.numVars() == 1);
		REQUIRE(lp.getLiteral(a) == ~lp.getLiteral(b));
		lp.updateProgram();
		lpAdd(lp,
			"c :- a, not d.\n"
			"d :- b, not c.\n"
			"e :- d.\n");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE(ctx.numVars() == 3);
		REQUIRE(lp.getLiteral(a) == ~lp.getLiteral(b));
		REQUIRE(lp.getLiteral(e) == lp.getLiteral(d));
	}
	SECTION("testDistinctFacts") {
		lp.start(ctx);
		lp.enableDistinctTrue();
		lp.updateProgram();
		lpAdd(lp,
			"a.\n"
			"b :- not c.\n"
			"c.\n");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE(ctx.numVars() == 0);
		REQUIRE(lp.getLiteral(a) == lit_true());
		REQUIRE(lp.getLiteral(b) == lit_false());
		REQUIRE(lp.getLiteral(b, MapLit_t::Refined) == lit_false());
		REQUIRE(lp.getLiteral(c) == lit_true());
		lp.updateProgram();
		Var g = f + 1;
		lpAdd(lp,
			"g :- not f.\n"
			"d.\n"
			"e :- f.\n"
			"f.\n");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE(ctx.numVars() == 1);
		REQUIRE(lp.getLiteral(d) == lit_true());
		REQUIRE(lp.getLiteral(e) == lit_true());
		REQUIRE(lp.getLiteral(f) == lit_true());
		REQUIRE(lp.getLiteral(g) == lit_false());
		REQUIRE(lp.getLiteral(d, MapLit_t::Refined) == posLit(1));
		REQUIRE(lp.getLiteral(e, MapLit_t::Refined) == posLit(1));
		REQUIRE(lp.getLiteral(f, MapLit_t::Refined) == posLit(1));
		REQUIRE(lp.getLiteral(g, MapLit_t::Refined) == negLit(1));
	}
	SECTION("testDistinctFactsSimple") {
		lp.start(ctx, LogicProgram::AspOptions().noEq());
		lp.enableDistinctTrue();
		lp.updateProgram();
		lpAdd(lp, "a.\n");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE(ctx.numVars() == 0);
		REQUIRE(lp.getLiteral(a) == lit_true());
		lp.updateProgram();
		lpAdd(lp,
			"b :- d.\n"
			"c :- e.\n"
			"d. e.\n");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE(ctx.numVars() == 1);
		REQUIRE(lp.getLiteral(b) == lit_true());
		REQUIRE(lp.getLiteral(c) == lit_true());
		REQUIRE(lp.getLiteral(d) == lit_true());
		REQUIRE(lp.getLiteral(b, MapLit_t::Refined) == posLit(1));
		REQUIRE(lp.getLiteral(c, MapLit_t::Refined) == posLit(1));
		REQUIRE(lp.getLiteral(d, MapLit_t::Refined) == posLit(1));
		lp.updateProgram();
		lpAdd(lp, "f.");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE(ctx.numVars() == 2);
		REQUIRE(lp.getLiteral(a, MapLit_t::Refined) == posLit(0));
		REQUIRE(lp.getLiteral(b, MapLit_t::Refined) == posLit(1));
		REQUIRE(lp.getLiteral(f, MapLit_t::Refined) == posLit(2));
	}
	SECTION("testFreeze") {
		lp.start(ctx, LogicProgram::AspOptions().noEq());
		lp.updateProgram();
		lpAdd(lp,
			"{d}.\n"
			"a :- not c.\n"
			"b :- a, d.\n"
			"#external c.");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE(lp.getLiteral(c) != lit_false());
		Solver& solver = *ctx.master();
		solver.assume(lp.getLiteral(c));
		solver.propagate();
		REQUIRE(ctx.master()->isFalse(lp.getLiteral(b)));
		solver.undoUntil(0);

		lp.updateProgram();
		lpAdd(lp,
			"{e}.\n"
			"c :- e.\n"
			"#external c. [release]");

		REQUIRE((lp.endProgram() && ctx.endInit()));
		solver.assume(lp.getLiteral(e));
		solver.propagate();
		REQUIRE(ctx.master()->isFalse(lp.getLiteral(b)));
		solver.undoUntil(0);
		solver.assume(~lp.getLiteral(e));
		solver.propagate();
		REQUIRE(ctx.master()->isFalse(lp.getLiteral(c)));
		solver.assume(lp.getLiteral(d));
		solver.propagate();
		REQUIRE(ctx.master()->isTrue(lp.getLiteral(b)));
	}
	SECTION("testFreezeValue") {
		lp.start(ctx, LogicProgram::AspOptions().noEq());
		lp.updateProgram();
		lp.freeze(a).freeze(b, value_true).freeze(c, value_false);

		REQUIRE((lp.endProgram() && ctx.endInit()));
		LitVec assume;
		lp.getAssumptions(assume);
		Solver& solver = *ctx.master();
		solver.pushRoot(assume);
		REQUIRE(solver.isFalse(lp.getLiteral(a)));
		REQUIRE(solver.isTrue(lp.getLiteral(b)));
		REQUIRE(solver.isFalse(lp.getLiteral(c)));
		solver.popRootLevel(solver.decisionLevel());

		lp.updateProgram();
		lp.unfreeze(a).freeze(c, value_true).freeze(b, value_true).freeze(b, value_false);
		REQUIRE((lp.endProgram() && ctx.endInit()));
		assume.clear();
		lp.getAssumptions(assume);
		REQUIRE((assume.size() == 2 && solver.isFalse(lp.getLiteral(a))));
		solver.pushRoot(assume);
		REQUIRE(solver.isFalse(lp.getLiteral(b)));
		REQUIRE(solver.isTrue(lp.getLiteral(c)));
	}

	SECTION("testFreezeOpen") {
		lp.start(ctx, LogicProgram::AspOptions().noEq());
		// I1:
		// freeze(a, value_free)
		lp.updateProgram();
		lp.freeze(a, value_free);
		REQUIRE((lp.endProgram() && ctx.endInit()));
		LitVec assume;
		lp.getAssumptions(assume);
		Solver& solver = *ctx.master();
		REQUIRE(assume.empty());
		REQUIRE(solver.value(lp.getLiteral(a).var()) == value_free);

		// I2:
		lp.updateProgram();
		REQUIRE((lp.endProgram() && ctx.endInit()));
		assume.clear();
		lp.getAssumptions(assume);
		REQUIRE(assume.empty());
		REQUIRE(solver.value(lp.getLiteral(a).var()) == value_free);

		// I3:
		lp.updateProgram();
		lp.freeze(a, value_true);
		REQUIRE((lp.endProgram() && ctx.endInit()));
		assume.clear();
		lp.getAssumptions(assume);
		REQUIRE((assume.size() == 1 && assume[0] == lp.getLiteral(a)));
	}
	SECTION("testKeepFrozen") {
		lp.start(ctx);
		// I0:
		lp.updateProgram();
		lp.freeze(a);

		REQUIRE(lp.endProgram());
		PrgAtom* x = lp.getAtom(a);
		Literal xLit = x->literal();
		// I1:
		lp.updateProgram();
		REQUIRE(lp.endProgram());
		REQUIRE(x->literal() == xLit);
		REQUIRE(x->frozen());
	}
	SECTION("testFreezeCompute") {
		lp.start(ctx);
		lp.updateProgram();
		lpAdd(lp,
			"#external a. #external b. #external c. #external d.\n"
			":- not a.\n"
			":- not b.\n"
			":- c.\n"
			":- d.\n");
		REQUIRE(lp.endProgram());
		PrgAtom* x = lp.getAtom(a);
		PrgAtom* y = lp.getAtom(b);
		REQUIRE((x->frozen() && y->frozen()));
		REQUIRE(x->literal() != y->literal());
		PrgAtom* z = lp.getAtom(c);
		PrgAtom* z2 = lp.getAtom(d);
		REQUIRE((z->frozen() && z2->frozen()));
		REQUIRE((z->literal() == z2->literal()));
		// I1:
		lp.updateProgram();
		lpAdd(lp, ":- a.");
		REQUIRE_FALSE(lp.endProgram());
	}
	SECTION("testUnfreezeUnsupp") {
		lp.start(ctx, LogicProgram::AspOptions().noEq());
		lp.updateProgram();
		lpAdd(lp, "a :- not b. #external b.");
		REQUIRE(lp.endProgram());
		DefaultUnfoundedCheck* ufsCheck = 0;
		if (ctx.sccGraph.get()) {
			ctx.master()->addPost(ufsCheck = new DefaultUnfoundedCheck(*ctx.sccGraph));
		}
		ctx.endInit();
		lp.updateProgram();
		lpAdd(lp,
			"c :- b.\n"
			"b :- c.\n"
			"#external b. [release]");
		REQUIRE(lp.endProgram());
		if (ctx.sccGraph.get() && !ufsCheck) {
			ctx.master()->addPost(ufsCheck = new DefaultUnfoundedCheck(*ctx.sccGraph));
		}
		ctx.endInit();
		REQUIRE(ctx.master()->isTrue(lp.getLiteral(a)));
		REQUIRE(ctx.master()->isFalse(lp.getLiteral(b)));
		REQUIRE(ctx.master()->isFalse(lp.getLiteral(c)));
	}

	SECTION("testUnfreezeCompute") {
		lp.start(ctx);
		lp.updateProgram();
		lpAdd(lp,
			"{d}.\n"
			"a :- b, c.\n"
			"b :- d.\n"
			"#external c.");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE(3u == ctx.numConstraints());

		lp.updateProgram();
		lpAdd(lp,
			"#external c.[release]\n"
			":- c.\n");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE(3u >= ctx.numConstraints());
		REQUIRE(ctx.master()->numFreeVars() == 1);
	}

	SECTION("testCompute") {
		lp.start(ctx, LogicProgram::AspOptions());
		lp.updateProgram();
		Var x2 = 2, x3 = 3, x4 = 4;
		lpAdd(lp,
			"{x2;x3}.\n"
			"x1 :- x2, x3.\n"
			"   :- x1.\n");

		REQUIRE((lp.endProgram() && ctx.endInit()));
		lp.updateProgram();
		lpAdd(lp, "{x4}. :- x2, x4.");

		REQUIRE((lp.endProgram() && ctx.endInit()));
		ctx.master()->assume(lp.getLiteral(x2));
		ctx.master()->propagate();
		REQUIRE(ctx.master()->isFalse(lp.getLiteral(x3)));
		REQUIRE(ctx.master()->isFalse(lp.getLiteral(x4)));
	}
	SECTION("testComputeBackprop") {
		lp.start(ctx, LogicProgram::AspOptions().backpropagate());
		lp.updateProgram();
		lpAdd(lp,
			"a :- not b.\n"
			"b :- not a.\n"
			"  :- not a.\n");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		// I2:
		lp.updateProgram();
		lpAdd(lp, "c :- b. d :- not b.");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE_FALSE(lp.getAtom(c)->hasVar());
		REQUIRE(lit_true() == lp.getLiteral(d));
	}

	SECTION("testBackpropStep") {
		lp.start(ctx);
		// I1:
		lp.updateProgram();
		lpAdd(lp, "{a}.");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		// I2:
		lp.updateProgram();
		lpAdd(lp, "{c}.    b :- a, c.    :- not b.");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE(ctx.master()->isTrue(lp.getLiteral(a)));
	}

	SECTION("testEq") {
		lp.start(ctx);
		lp.updateProgram();
		lpAdd(lp,
			"{c;d}.\n"
			"a :- c.\n"
			"a :- d.\n"
			"b :- a.\n");

		// b == a
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE(lp.isDefined(a));
		REQUIRE_FALSE(lp.isDefined(b));

		std::stringstream exp("1 2 1 0 1");
		REQUIRE(findSmodels(exp, lp));
		lp.updateProgram();
		lpAdd(lp,
			"e :- a, b.\n"
			"#output e : e.\n");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		exp.str("1 5 1 0 1\n5 e");
		REQUIRE(findSmodels(exp, lp));
	}

	SECTION("testSimplifyRuleEq") {
		lp.start(ctx);
		lp.updateProgram();
		lpAdd(lp,
			"{x3}.\n"
			"x1 :- x3.\n"
			"x2 :- x3.\n");

		// x1 == x2
		REQUIRE((lp.endProgram() && ctx.endInit()));
		uint32 x1 = lp.getAtom(1)->id();
		uint32 x2 = lp.getAtom(2)->id();
		REQUIRE(x1 == x2);

		lp.updateProgram();

		lpAdd(lp,
			"{x4}.\n"
			"x5 :- 2{x1, x2, x4}.");
		REQUIRE(lp.endProgram());
		REQUIRE(lp.getAtom(5)->supports() == 1);
		PrgBody* body = lp.getBody(lp.getAtom(5)->supps_begin()->node());
		REQUIRE(body->type() == Potassco::Body_t::Sum);
		REQUIRE(body->bound() == 2);
		REQUIRE(body->size() == 2);

		uint32 eqIdx = body->goal(0).var() == 4;
		REQUIRE(body->weight(eqIdx) == 2);
		REQUIRE(body->weight(1 - eqIdx) == 1);
	}

	SECTION("testEqUnfreeze") {
		lp.start(ctx);
		lp.updateProgram();
		lp.freeze(a);
		lp.endProgram();
		lp.updateProgram();
		lpAdd(lp,
			"{d}.\n"
			"a :- c.\n"
			"b :- d, not a.\n"
			"#external a. [release]");

		lp.endProgram();
		REQUIRE((ctx.numVars() == 2 && ctx.master()->numFreeVars() == 1));
		REQUIRE(lp.getLiteral(b) == lp.getLiteral(d));
	}

	SECTION("testEqComplement") {
		lp.start(ctx);
		lp.updateProgram();
		lpAdd(lp, "a :- not b.  b :- not a.");

		REQUIRE((lp.endProgram() && ctx.endInit()));
		PrgAtom* na = lp.getAtom(a);
		PrgAtom* nb = lp.getAtom(b);
		REQUIRE(nb->literal() == ~na->literal());
		// I1:
		lp.updateProgram();
		lpAdd(lp, "c :- a, b.");
		REQUIRE(lp.endProgram());
		PrgAtom* nc = lp.getAtom(c);
		REQUIRE(nc->hasVar() == false);
	}

	SECTION("testEqUpdateAssigned") {
		lp.start(ctx);
		// I0:
		lp.updateProgram();
		lp.freeze(a);
		REQUIRE((lp.endProgram() && ctx.endInit()));

		// I1:
		lp.updateProgram();
		lpAdd(lp, "a :- b.  b :- a.");

		PrgAtom* x = lp.getAtom(a);
		x->setValue(value_weak_true);
		lp.endProgram();
		// only weak-true so still relevant in bodies!
		REQUIRE(x->deps_begin()->sign() == false);
	}

	SECTION("testEqResetState") {
		lp.start(ctx);
		// I0:
		lp.updateProgram();
		lpAdd(lp, "{a;b}.");
		REQUIRE((lp.endProgram() && ctx.endInit()));

		// I1:
		lp.updateProgram();
		lpAdd(lp,
			"c :- f.\n"
			"d :- g.\n"
			"c :- d, a.\n"
			"d :- c, b.\n"
			"f :- e, not h.\n"
			"g :- e, not i.\n"
			"{e}.");
		lp.endProgram();
		REQUIRE(lp.getAtom(c)->scc() == 0);
		REQUIRE(lp.getAtom(d)->scc() == 0);
	}

	SECTION("testUnfreezeUnsuppEq") {
		lp.start(ctx, LogicProgram::AspOptions().noScc());
		// I0:
		lp.updateProgram();
		lp.freeze(a);

		REQUIRE((lp.endProgram() && ctx.endInit()));
		// I1:
		lp.updateProgram();
		lpAdd(lp,
			"a :- b.\n"
			"b :- a, c.\n"
			"{c}.\n");
		lp.endProgram();
		PrgAtom* x = lp.getAtom(a);
		PrgAtom* y = lp.getAtom(b);
		REQUIRE(ctx.master()->isFalse(x->literal()));
		REQUIRE(ctx.master()->isFalse(y->literal()));
	}

	SECTION("testUnfreezeEq") {
		lp.start(ctx);
		// I0:
		lp.updateProgram();
		lp.freeze(a);

		REQUIRE(lp.endProgram());
		// I1:
		lp.updateProgram();
		lpAdd(lp,
			"{c}.\n"
			"b :- c.\n"
			"a :- b.\n");
		PrgAtom* x = lp.getAtom(a);
		lp.endProgram();
		// body {b} is eq to body {c}
		REQUIRE(ctx.master()->value(x->var()) == value_free);
		REQUIRE((x->supports() == 1 && x->supps_begin()->node() == 1));
	}

	SECTION("testStats") {
		LpStats incStats;
		lp.start(ctx, LogicProgram::AspOptions().noEq());
		// I1:
		lp.updateProgram();
		lpAdd(lp,
			"a :- not b.\n"
			"b :- not a.\n"
			"#external c.\n");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		incStats = lp.stats;
		REQUIRE(uint32(3) == incStats.atoms);
		REQUIRE(uint32(2) == incStats.bodies[0].sum());
		REQUIRE(uint32(2) == incStats.rules[0].sum());
		REQUIRE(uint32(0) == incStats.ufsNodes);
		REQUIRE(uint32(0) == incStats.sccs);

		// I2:
		lp.updateProgram();
		lpAdd(lp,
			"{c, f}.\n"
			"d :- not c.\n"
			"d :- e, f.\n"
			"e :- d, f.\n");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		incStats.accu(lp.stats);
		REQUIRE(uint32(6) == incStats.atoms);
		REQUIRE(uint32(5) == incStats.bodies[0].sum());
		REQUIRE(uint32(6) == incStats.rules[0].sum());
		REQUIRE(uint32(1) == incStats.sccs);
		// I3:
		lp.updateProgram();
		lpAdd(lp,
			"g :- d, not i.\n"
			"i :- not g.\n"
			"g :- h.\n"
			"h :- g, not f.\n");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		incStats.accu(lp.stats);
		REQUIRE(uint32(9) == incStats.atoms);
		REQUIRE(uint32(9) == incStats.bodies[0].sum());
		REQUIRE(uint32(10) == incStats.rules[0].sum());
		REQUIRE(uint32(2) == incStats.sccs);
	}

	SECTION("testTransform") {
		lp.start(ctx, LogicProgram::AspOptions().noEq());
		lp.setExtendedRuleMode(LogicProgram::mode_transform);
		// I1:
		lp.updateProgram();
		lpAdd(lp, "{a}. % -> a :- not a'. a' :- not a.");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE(ctx.sccGraph.get() == 0);
		// I2:
		lp.updateProgram();
		lpAdd(lp,
			"% a' must not take up id!\n"
			"b :- a.\n"
			"b :- c.\n"
			"c :- b.\n");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE(ctx.sccGraph.get() != 0);
	}

	SECTION("testBackpropCompute") {
		lp.start(ctx);
		// I0:
		lp.updateProgram();
		lpAdd(lp,
			"a :- b.\n"
			"  :- not a.\n"
			"#external b.");
		REQUIRE(lp.endProgram());
		REQUIRE(lp.getRootId(a) == 2);
		PrgAtom* x = lp.getAtom(b);
		REQUIRE(x->value() == value_weak_true);
		// I1:
		lp.updateProgram();
		lpAdd(lp, "b :- c.  c :- b.");

		// UNSAT: no support for b,c
		REQUIRE_FALSE(lp.endProgram());
	}

	SECTION("testBackpropSolver") {
		lp.start(ctx);
		// I0:
		lp.updateProgram();
		lpAdd(lp,
			"{a}.\n"
			":- not a.\n"
			":- not b.\n"
			"#external b.");
		REQUIRE(lp.endProgram());
		PrgAtom* na = lp.getAtom(a);
		PrgAtom* nb = lp.getAtom(b);
		REQUIRE(na->value() == value_true);
		REQUIRE(nb->value() == value_weak_true);
		// I1:
		lp.updateProgram();
		REQUIRE(lp.endProgram());
		REQUIRE(na->value() == value_true);
		REQUIRE(nb->value() == value_weak_true);
	}

	SECTION("testFreezeUnfreeze") {
		lp.start(ctx);
		// I0:
		lp.updateProgram();
		lpAdd(lp,
			"{a}."
			"#external b.\n"
			"#external b. [release]");
		REQUIRE(lp.endProgram());
		REQUIRE(lit_false() == lp.getLiteral(b));
		// I1:
		lp.updateProgram();
		lpAdd(lp,
			"#external c.\n"
			"c :- b.");
		REQUIRE(lp.endProgram());
		REQUIRE(ctx.master()->isFalse(lp.getLiteral(b)));
	}
	SECTION("testSymbolUpdate") {
		lp.start(ctx);
		// I0:
		lp.updateProgram();
		lpAdd(lp, "{a}.");
		lp.addOutput("a", a);
		REQUIRE(lp.endProgram());
		uint32 os = ctx.output.size();
		// I1:
		lp.updateProgram();
		lpAdd(lp, "{b;c}.");
		lp.addOutput("b", b);

		REQUIRE(lp.endProgram());
		REQUIRE(!isSentinel(lp.getLiteral(c)));
		REQUIRE(os + 1 == ctx.output.size());
	}
	SECTION("testFreezeDefined") {
		lp.start(ctx);
		// I0:
		lp.updateProgram();
		lpAdd(lp, "{a}.");
		REQUIRE(lp.endProgram());
		// I1:
		lp.updateProgram();
		lpAdd(lp, "#external a.");
		REQUIRE(lp.endProgram());
		REQUIRE(lp.getAtom(a)->frozen() == false);
	}
	SECTION("testUnfreezeDefined") {
		lp.start(ctx);
		// I0:
		lp.updateProgram();
		lpAdd(lp, "{a}.");
		REQUIRE(lp.endProgram());
		// I1:
		lp.updateProgram();
		lpAdd(lp, "#external a. [release]");
		REQUIRE(lp.endProgram());
		REQUIRE(!ctx.master()->isFalse(lp.getLiteral(a)));
	}
	SECTION("testUnfreezeAsFact") {
		lp.start(ctx);
		// I0:
		lp.updateProgram();
		lpAdd(lp, "#external a.");
		REQUIRE(lp.endProgram());
		Literal litA = lp.getLiteral(a);
		// I1:
		lp.updateProgram();
		lpAdd(lp, "a.");
		REQUIRE(true  == lp.endProgram());
		REQUIRE(litA == lp.getLiteral(a));
		REQUIRE_FALSE(lp.isExternal(a));
		REQUIRE(true  == lp.isDefined(a));
		REQUIRE(ctx.master()->isTrue(litA));
	}
	SECTION("testImplicitUnfreeze") {
		lp.start(ctx);
		// I0:
		lp.updateProgram();
		lpAdd(lp, "#external a.");
		REQUIRE(lp.endProgram());
		REQUIRE(lp.getAtom(a)->frozen() == true);
		REQUIRE(!ctx.master()->isFalse(lp.getLiteral(a)));
		// I1:
		lp.updateProgram();
		lpAdd(lp, "{a}.");
		REQUIRE(lp.endProgram());
		REQUIRE(lp.getAtom(a)->frozen() == false);
	}
	SECTION("testRedefine") {
		lp.start(ctx);
		// I0:
		lp.updateProgram();
		lpAdd(lp, "{a}.");
		REQUIRE(lp.endProgram());
		// I1:
		lp.updateProgram();
		REQUIRE_THROWS_AS(lpAdd(lp, "{a}."), RedefinitionError);
	}
	SECTION("testGetAssumptions") {
		lp.start(ctx);
		// I0:
		lp.updateProgram();
		lpAdd(lp, "#external a. #external b.");
		REQUIRE(lp.endProgram());
		LitVec assume;
		lp.getAssumptions(assume);
		REQUIRE((assume.size() == 2 && assume[0] == ~lp.getLiteral(a) && assume[1] == ~lp.getLiteral(b)));
	}

	SECTION("testSimplifyCard") {
		lp.start(ctx);
		// I0:
		lp.updateProgram();
		lpAdd(lp, "a.");
		REQUIRE(lp.endProgram());
		lp.updateProgram();
		lpAdd(lp,
			"b :- 1 {c, a}.\n"
			"d :- 1 {c, b}.\n"
			"c :- a, b.");
		REQUIRE(lp.endProgram());
	}

	SECTION("testSimplifyMinimize") {
		lp.start(ctx);
		// I0:
		lp.updateProgram();
		lpAdd(lp, "a. #minimize{a}.");
		REQUIRE(lp.endProgram());
		REQUIRE(ctx.minimize()->adjust(0) == 1);
		ctx.removeMinimize();

		lp.updateProgram();
		lpAdd(lp, "#minimize{a}.");
		REQUIRE(lp.endProgram());
		REQUIRE(ctx.hasMinimize());
		REQUIRE(ctx.minimize()->adjust(0) == 1);
		REQUIRE(lp.endProgram());
	}

	SECTION("testEqOverNeg") {
		lp.start(ctx);
		// I0: {a}.
		lp.updateProgram();
		lpAdd(lp, "{a}.");
		REQUIRE(lp.endProgram());
		// I1:
		lp.updateProgram();
		lpAdd(lp,
			"b :- not a.\n"
			"c :- b.\n"
			"c :- f.\n"
			"d :- c.\n"
			"e :- d.\n"
			"g :- c.\n"
			"h :- not f, c.\n"
			"  :- not e.\n"
			"  :- not h.\n");
		REQUIRE(lp.endProgram());
		REQUIRE((ctx.numVars() == 1 && ctx.master()->isFalse(lp.getLiteral(a))));
	}
	SECTION("testKeepExternalValue") {
		lp.start(ctx);
		// I0:
		lp.updateProgram();
		lpAdd(lp,
			"b.\n"
			"a :- c.\n"
			"  :- a.\n"
			"#external c. [true]");

		REQUIRE((lp.endProgram() && ctx.endInit()));
		LitVec assume;
		lp.getAssumptions(assume);
		REQUIRE_FALSE(ctx.master()->pushRoot(assume));

		REQUIRE(lp.update());
		REQUIRE((lp.endProgram() && ctx.endInit()));
		assume.clear();
		lp.getAssumptions(assume);
		REQUIRE_FALSE(ctx.master()->pushRoot(assume));

		REQUIRE(lp.update());
		lpAdd(lp, "c.");
		REQUIRE_FALSE(lp.endProgram());
	}

	SECTION("testWriteMinimize") {
		lp.start(ctx);
		lp.updateProgram();
		lpAdd(lp, "{a}. #minimize{a}.");
		lp.endProgram();
		std::stringstream exp("6 0 1 0 1 1");
		REQUIRE(findSmodels(exp, lp));
		lp.updateProgram();
		lpAdd(lp, "{b}. #minimize{b}.");

		lp.endProgram();
		exp.str("6 0 2 0 1 2 1 1");
		REQUIRE(findSmodels(exp, lp));
		lp.updateProgram();
		lpAdd(lp, "{c}.");
		lp.endProgram();
		exp.clear();
		exp.seekg(0, ios::beg);
		REQUIRE_FALSE(findSmodels(exp, lp));
	}
	SECTION("testWriteExternal") {
		lp.start(ctx);
		lp.updateProgram();
		lpAdd(lp, "#external a.");
		lp.endProgram();
		std::stringstream str;
		AspParser::write(lp, str, AspParser::format_aspif);
		bool foundA = false, foundB = false;
		for (std::string x; std::getline(str, x);) {
			if (x.find("5 1 2") == 0) { foundA = true; }
			if (x.find("5 2 2") == 0) { foundB = true; }
		}
		REQUIRE((foundA && !foundB));
		lp.updateProgram();
		lpAdd(lp, "#external b.");
		lp.endProgram();
		foundA = foundB = false;
		str.clear();
		AspParser::write(lp, str, AspParser::format_aspif);
		for (std::string x; std::getline(str, x);) {
			if (x.find("5 1 2") == 0) { foundA = true; }
			if (x.find("5 2 2") == 0) { foundB = true; }
		}
		REQUIRE((!foundA && foundB));
	}

	SECTION("testWriteExternalBug") {
		lp.start(ctx);
		lp.updateProgram();
		lpAdd(lp,
			"#external a."
			"#external b."
			"#external c."
			"#external d."
			"b.");
		lp.endProgram();
		std::stringstream str;
		AspParser::write(lp, str, AspParser::format_aspif);
		int foundA = 0, foundB = 0, foundC = 0, foundD = 0;
		for (std::string x; std::getline(str, x);) {
			if (x.find("5 1 2") == 0) { ++foundA; }
			if (x.find("5 2 2") == 0) { ++foundB; }
			if (x.find("5 3 2") == 0) { ++foundC; }
			if (x.find("5 4 2") == 0) { ++foundD; }
		}
		REQUIRE(foundA == 1);
		REQUIRE(foundB == 0);
		REQUIRE(foundC == 1);
		REQUIRE(foundD == 1);
	}

	SECTION("testWriteUnfreeze") {
		lp.start(ctx);
		lp.updateProgram();
		lpAdd(lp, "#external a.");
		lp.endProgram();
		lp.updateProgram();
		lpAdd(lp, "#external a. [release]");
		lp.endProgram();
		REQUIRE(!lp.isExternal(a));
		std::stringstream str;
		AspParser::write(lp, str, AspParser::format_aspif);
		bool found = false;
		for (std::string x; std::getline(str, x);) {
			if (x.find("5 1 3") == 0) { found = true; break; }
		}
		REQUIRE(found);
	}
	SECTION("testSetInputAtoms") {
		lp.start(ctx);
		lp.updateProgram();
		lpAdd(lp, "{a;b;c}.");
		REQUIRE(lp.numAtoms() == 3);
		lp.setMaxInputAtom(lp.numAtoms());
		Var d = lp.newAtom();
		Var e = lp.newAtom();
		REQUIRE((d == 4 && e == 5));
		lp.endProgram();
		REQUIRE((lp.numAtoms() == 5 && lp.startAuxAtom() == 4 && lp.stats.auxAtoms == 2));
		lp.updateProgram();
		INFO("aux atoms are removed on update");
		REQUIRE(lp.numAtoms() == 3);
		REQUIRE_FALSE(lp.validAtom(d));
		REQUIRE_FALSE(lp.validAtom(e));
		REQUIRE(d == lp.newAtom());
	}

	SECTION("testFreezeIsExternal") {
		lp.start(ctx);
		lp.updateProgram();
		lpAdd(lp, "{b}. #external a.");
		REQUIRE(true  == lp.isExternal(a));
		REQUIRE_FALSE(lp.isExternal(b));
		lp.endProgram();
		REQUIRE(true  == lp.isExternal(a));
		REQUIRE_FALSE(lp.isExternal(b));
	}
	SECTION("testUnfreezeIsNotExternal") {
		lp.start(ctx);
		lp.updateProgram();
		lpAdd(lp,
			"#external a.\n"
			"#external b. [release]\n"
			"#external a. [release]\n"
			"#external b.\n");
		REQUIRE_FALSE(lp.isExternal(a));
		REQUIRE_FALSE(lp.isExternal(b));
		lp.endProgram();
		REQUIRE_FALSE(lp.isExternal(a));
		REQUIRE_FALSE(lp.isExternal(b));
	}
	SECTION("testFreezeStaysExternal") {
		lp.start(ctx);
		lp.updateProgram();
		lpAdd(lp, "#external a.");
		REQUIRE(lp.isExternal(a));
		lp.endProgram();
		REQUIRE(lp.isExternal(a));
		lp.updateProgram();
		REQUIRE(lp.isExternal(a));
		lp.endProgram();
		REQUIRE(lp.isExternal(a));
	}
	SECTION("testDefinedIsNotExternal") {
		lp.start(ctx);
		lp.updateProgram();
		lpAdd(lp, "#external a.");
		lp.endProgram();
		lp.updateProgram();
		lpAdd(lp, "a :- b.");
		REQUIRE_FALSE(lp.isExternal(a));
		lp.endProgram();
		REQUIRE_FALSE(lp.isExternal(a));
	}
	SECTION("testFactIsNotExternal") {
		lp.start(ctx);
		lp.updateProgram();
		lpAdd(lp, "a. #external a.");
		lp.endProgram();
		REQUIRE_FALSE(lp.isExternal(a));
	}

	SECTION("testAssumptionsAreVolatile") {
		lp.start(ctx);
		lp.updateProgram();
		lpAdd(lp, "{a}. #assume{a}.");
		REQUIRE(lp.endProgram());
		LitVec assume;
		lp.getAssumptions(assume);
		REQUIRE((assume.size() == 1 && assume[0] == lp.getLiteral(a)));
		lp.updateProgram();
		lpAdd(lp, "#assume{not a}.");
		REQUIRE(lp.endProgram());
		assume.clear();
		lp.getAssumptions(assume);
		REQUIRE((assume.size() == 1 && assume[0] == ~lp.getLiteral(a)));
	}

	SECTION("testAssumptionsAreFrozen") {
		lpAdd(lp.start(ctx), "{a}. #assume{a}.");
		REQUIRE(lp.endProgram());
		REQUIRE(lp.getLiteral(a).var() == 1);
		REQUIRE(ctx.varInfo(lp.getLiteral(a).var()).frozen());
	}

	SECTION("testProjectionIsExplicitAndCumulative") {
		lp.start(ctx);
		lp.updateProgram();
		lpAdd(lp, "{a;b}. #project {a}.");
		REQUIRE(lp.endProgram());

		REQUIRE(ctx.output.projectMode() == ProjectMode_t::Explicit);
		REQUIRE(std::find(ctx.output.proj_begin(), ctx.output.proj_end(), lp.getLiteral(a)) != ctx.output.proj_end());
		REQUIRE(std::find(ctx.output.proj_begin(), ctx.output.proj_end(), lp.getLiteral(b)) == ctx.output.proj_end());

		lp.updateProgram();
		lpAdd(lp, "{c;d}. #project{d}.");
		REQUIRE(lp.endProgram());

		REQUIRE(ctx.output.projectMode() == ProjectMode_t::Explicit);
		REQUIRE(std::find(ctx.output.proj_begin(), ctx.output.proj_end(), lp.getLiteral(a)) != ctx.output.proj_end());
		REQUIRE(std::find(ctx.output.proj_begin(), ctx.output.proj_end(), lp.getLiteral(b)) == ctx.output.proj_end());
		REQUIRE(std::find(ctx.output.proj_begin(), ctx.output.proj_end(), lp.getLiteral(c)) == ctx.output.proj_end());
		REQUIRE(std::find(ctx.output.proj_begin(), ctx.output.proj_end(), lp.getLiteral(d)) != ctx.output.proj_end());
	}
	SECTION("testTheoryAtomsAreFrozenIncremental") {
		lp.start(ctx).update();
		lpAdd(lp, "b :- a.");
		Potassco::TheoryData& t = lp.theoryData();
		t.addTerm(0, "Theory");
		t.addAtom(a, 0, Potassco::toSpan<Potassco::Id_t>());
		lp.endProgram();
		REQUIRE(lp.getLiteral(a) != lit_false());
		REQUIRE(lp.getLiteral(b) != lit_false());
		std::stringstream str;
		AspParser::write(lp, str, AspParser::format_aspif);
		for (std::string x; std::getline(str, x);) {
			REQUIRE(x.find("5 1") != 0);
		}
		lp.update();
		lpAdd(lp,
			"{c}."
			"a :- c.");
		lp.endProgram();
		ctx.endInit();
		REQUIRE(ctx.master()->value(lp.getLiteral(a).var()) == value_free);
		ctx.addUnary(~lp.getLiteral(c));
		ctx.master()->propagate();
		REQUIRE(ctx.master()->isFalse(lp.getLiteral(a)));
	}
	SECTION("testFactTheoryAtomsAreNotExternal") {
		lp.start(ctx).updateProgram();
		lpAdd(lp, "a.");
		Potassco::TheoryData& t = lp.theoryData();
		t.addAtom(a, 0, Potassco::toSpan<Potassco::Id_t>());
		lp.endProgram();
		REQUIRE(lp.getLiteral(a) == lit_true());
		REQUIRE(lp.isDefined(a));
		REQUIRE(lp.isFact(a));
		REQUIRE(!lp.isExternal(a));
		REQUIRE(lp.getRootAtom(a)->supports() == 0);
		lp.updateProgram();
		lpAdd(lp, "b.");
		t.addAtom(b, 0, Potassco::toSpan<Potassco::Id_t>());
		lp.endProgram();
		REQUIRE(lp.getLiteral(b) == lit_true());
		REQUIRE(lp.isDefined(b));
		REQUIRE(lp.isFact(b));
		REQUIRE(lp.getRootAtom(b)->supports() == 0);
	}
	SECTION("testTheoryAtomsAreAdded") {
		lp.start(ctx).updateProgram();
		lpAdd(lp, "{a;b}.");
		Potassco::TheoryData& t = lp.theoryData();
		t.addAtom(c, 0, Potassco::toSpan<Potassco::Id_t>());
		lp.endProgram();
		REQUIRE(lp.getLiteral(c).var() != 0);
		REQUIRE(lp.isExternal(c));
		lp.updateProgram();
		lpAdd(lp, "c.");
		lp.endProgram();
		REQUIRE(lp.isFact(c));
		REQUIRE(!lp.isExternal(c));
		REQUIRE(ctx.master()->isTrue(lp.getLiteral(c)));
		LitVec vec;
		lp.getAssumptions(vec);
		REQUIRE(vec.empty());
	}

	SECTION("testCompEqBug") {
		lp.start(ctx);
		lp.updateProgram();
		lpAdd(lp, "{a;b}.\n"
		      "c :- not a.\n"
			  "d :- not b.\n"
		      "e :- a, b.");
		REQUIRE(lp.endProgram());
		lp.updateProgram();
		lpAdd(lp,
		      "f :- c,d.\n"
		      "  :- not f.");
		CHECK(lp.endProgram());
		CHECK(lp.getAtom(e)->supports() == 1);
		CHECK(lp.getLiteral(f) == lit_true());
	}
}

TEST_CASE("Sat builder", "[sat]") {
	SharedContext ctx;
	SatBuilder    builder;
	builder.startProgram(ctx);
	SECTION("testPrepare") {
		builder.prepareProblem(10);
		builder.endProgram();
		REQUIRE(ctx.numVars() == 10);
	}
	SECTION("testNoClauses") {
		builder.prepareProblem(2);
		builder.endProgram();
		REQUIRE(ctx.master()->numFreeVars() == 0);
	}
	SECTION("testAddClause") {
		builder.prepareProblem(2);
		LitVec clause; clause.push_back(posLit(1)); clause.push_back(posLit(2));
		builder.addClause(clause);
		builder.endProgram();
		REQUIRE(ctx.numConstraints() == 1);
		REQUIRE(!ctx.hasMinimize());
	}
	SECTION("testAddSoftClause") {
		builder.prepareProblem(3);
		LitVec clause;
		clause.push_back(posLit(1));
		builder.addClause(clause, 1);
		clause.clear();
		clause.push_back(negLit(1));
		clause.push_back(posLit(2));
		clause.push_back(negLit(3));
		builder.addClause(clause, 2);
		builder.endProgram();
		REQUIRE(ctx.numConstraints() == 1);
		REQUIRE(ctx.minimize()->numRules() == 1);
		REQUIRE(ctx.numVars() > 3);
	}
	SECTION("testAddEmptySoftClause") {
		builder.prepareProblem(3);
		LitVec clause;
		clause.push_back(posLit(1));
		// force 1
		builder.addClause(clause);

		clause.clear();
		clause.push_back(negLit(1));
		builder.addClause(clause, 1);
		REQUIRE(builder.endProgram());
		REQUIRE(ctx.minimize()->numRules() == 1);
		REQUIRE(ctx.minimize()->adjust(0) == 1);
	}
	SECTION("testAddConflicting") {
		builder.prepareProblem(3);
		LitVec clause;
		clause.push_back(posLit(1));
		REQUIRE(builder.addClause(clause));
		clause.clear();
		clause.push_back(negLit(1));
		REQUIRE(builder.addClause(clause) == false);
		clause.clear();
		clause.push_back(posLit(2));
		clause.push_back(posLit(3));
		REQUIRE(builder.addClause(clause) == false);
		REQUIRE(builder.endProgram() == false);
		REQUIRE(ctx.ok() == false);
	}
}

TEST_CASE("Pb builder", "[pb]") {
	SharedContext ctx;
	PBBuilder     builder;
	builder.startProgram(ctx);
	SECTION("testPrepare") {
		builder.prepareProblem(10, 0, 0);
		builder.endProgram();
		REQUIRE(ctx.numVars() == 10);
	}
	SECTION("testNegativeWeight") {
		builder.prepareProblem(5, 0, 0);
		WeightLitVec con;
		con.push_back(WeightLiteral(posLit(1), 2));
		con.push_back(WeightLiteral(posLit(2), -2));
		con.push_back(WeightLiteral(posLit(3), -1));
		con.push_back(WeightLiteral(posLit(4), 1));
		con.push_back(WeightLiteral(posLit(5), 1));
		builder.addConstraint(con, 2);
		builder.endProgram();
		REQUIRE(ctx.numConstraints() == 1);
		REQUIRE(ctx.master()->numAssignedVars() == 0);
		ctx.master()->assume(negLit(1)) && ctx.master()->propagate();
		REQUIRE(ctx.master()->numFreeVars() == 0);
	}
	SECTION("testProduct") {
		builder.prepareProblem(3, 1, 1);
		LitVec p1(3), p2;
		p1[0] = posLit(1);
		p1[1] = posLit(2);
		p1[2] = posLit(3);
		p2    = p1;
		Literal x = builder.addProduct(p1);
		Literal y = builder.addProduct(p2);
		REQUIRE((x.var() == 4 && x == y));
	}
	SECTION("testProductTrue") {
		builder.prepareProblem(3, 1, 1);
		LitVec p1(3);
		p1[0] = posLit(1);
		p1[1] = posLit(2);
		p1[2] = posLit(3);
		ctx.master()->force(posLit(1), 0);
		ctx.master()->force(posLit(2), 0);
		ctx.master()->force(posLit(3), 0);
		Literal x = builder.addProduct(p1);
		REQUIRE(x == lit_true());
	}
	SECTION("testProductFalse") {
		builder.prepareProblem(3, 1, 1);
		LitVec p1(3);
		p1[0] = posLit(1);
		p1[1] = posLit(2);
		p1[2] = posLit(3);
		ctx.master()->force(negLit(2), 0);
		Literal x = builder.addProduct(p1);
		REQUIRE(x == lit_false());
	}
	SECTION("testInconsistent") {
		builder.prepareProblem(1, 0, 0);
		WeightLitVec con;
		con.push_back(WeightLiteral(posLit(1), 1));
		REQUIRE(builder.addConstraint(con, 1));
		con.assign(1, WeightLiteral(posLit(1), 1));
		REQUIRE_FALSE(builder.addConstraint(con, 0, true));
		REQUIRE_FALSE(builder.endProgram());
	}
	SECTION("testInconsistentW") {
		builder.prepareProblem(2, 0, 0);
		WeightLitVec con;
		con.push_back(WeightLiteral(posLit(1), 1));
		REQUIRE(builder.addConstraint(con, 1));
		con.assign(1, WeightLiteral(posLit(1), 3));
		con.push_back(WeightLiteral(posLit(2), 2));
		REQUIRE_FALSE(builder.addConstraint(con, 2, true));
		REQUIRE_FALSE(builder.endProgram());
	}
	SECTION("testCoefficientReductionOnEq") {
		builder.prepareProblem(4, 0, 0);
		WeightLitVec con;
		con.push_back(WeightLiteral(posLit(1), 3));
		con.push_back(WeightLiteral(posLit(2), 2));
		con.push_back(WeightLiteral(posLit(3), 1));
		con.push_back(WeightLiteral(posLit(4), 1));
		REQUIRE(builder.addConstraint(con, 2, true));
		REQUIRE(builder.endProgram());
		REQUIRE(ctx.master()->isFalse(posLit(1)));
	}
}
 } }
