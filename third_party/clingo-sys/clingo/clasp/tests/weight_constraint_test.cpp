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
#include <clasp/weight_constraint.h>
#include <clasp/solver.h>
#include <algorithm>
#include "catch.hpp"
using namespace std;

namespace Clasp { namespace Test {
static bool newWeightConstraint(SharedContext& ctx, Literal W, WeightLitVec& lits, weight_t bound) {
	return WeightConstraint::create(*ctx.master(), W, lits, bound).ok();
}
static bool newCardinalityConstraint(SharedContext& ctx, const LitVec& lits, int bound) {
	REQUIRE(lits.size() >  1);
	WeightLitVec wlits;
	for (LitVec::size_type i = 1; i < lits.size(); ++i) {
		wlits.push_back(WeightLiteral(lits[i], 1));
	}
	return newWeightConstraint(ctx, lits[0], wlits, bound);
}
static bool checkPropagate(Solver& solver, LitVec& assume, const LitVec& expect) {
	std::sort(assume.begin(), assume.end());
	do {
		for (uint32 i = 0; i < assume.size(); ++i) {
			REQUIRE(solver.assume(assume[i]));
			REQUIRE(solver.propagate());
		}
		for (uint32 i = 0; i < expect.size(); ++i) {
			REQUIRE(solver.isTrue(expect[i]));
			LitVec reason;
			solver.reason(expect[i], reason);
			REQUIRE(assume.size() == reason.size());
			for (uint32 j = 0; j < assume.size(); ++j) {
				REQUIRE(find(reason.begin(), reason.end(), assume[j]) != reason.end());
			}
		}
		solver.undoUntil(0);
	} while (std::next_permutation(assume.begin(), assume.end()));
	return true;
}
TEST_CASE("Cardinality constraints", "[constraint][pb][asp]") {
	SharedContext ctx;
	Solver& solver = *ctx.master();
	Literal body = posLit(ctx.addVar(Var_t::Body));
	Literal a = posLit(ctx.addVar(Var_t::Atom));
	Literal b = posLit(ctx.addVar(Var_t::Atom));
	Literal c = posLit(ctx.addVar(Var_t::Atom));
	Literal d = posLit(ctx.addVar(Var_t::Atom));
	Literal e = posLit(ctx.addVar(Var_t::Atom));
	ctx.startAddConstraints(10);
	LitVec lits;
	lits.push_back(body);
	lits.push_back(a);
	lits.push_back(~b);
	lits.push_back(~c);
	lits.push_back(d);
	SECTION("testAssertTriviallySat") {
		lits.resize(2);
		REQUIRE(newCardinalityConstraint(ctx, lits, 0));
		REQUIRE(solver.isTrue(body));
		REQUIRE(ctx.numConstraints() == 0);
	}
	SECTION("testAssertTriviallyUnSat") {
		lits.resize(2);
		REQUIRE(newCardinalityConstraint(ctx, lits, 2));
		REQUIRE(solver.isFalse(body));
		REQUIRE(ctx.numConstraints() == 0);
	}
	SECTION("testAssertNotSoTriviallySat") {
		solver.force(lits[1], 0);
		solver.force(lits[2], 0);
		REQUIRE(newCardinalityConstraint(ctx, lits, 2));
		REQUIRE(solver.isTrue(body));
		REQUIRE(ctx.numConstraints() == 0);
	}
	SECTION("testAssertNotSoTriviallyUnSat") {
		solver.force(~lits[1], 0);
		solver.force(~lits[3], 0);
		REQUIRE(newCardinalityConstraint(ctx, lits, 3));
		REQUIRE(solver.isTrue(~body));
		REQUIRE(ctx.numConstraints() == 0);
	}
	SECTION("testTrivialBackpropFalse") {
		solver.force(~body, 0);
		REQUIRE(newCardinalityConstraint(ctx, lits, 1));
		REQUIRE(ctx.numConstraints() == 0);
		REQUIRE(solver.isFalse(lits[1]));
		REQUIRE(solver.isFalse(lits[2]));
		REQUIRE(solver.isFalse(lits[3]));
		REQUIRE(solver.isFalse(lits[4]));
	}
	SECTION("testIntegrateNewVars") {
		lits.resize(3);
		Literal f = posLit(ctx.addVar(Var_t::Atom));
		REQUIRE_FALSE(ctx.master()->validVar(f.var()));
		SECTION("body") {
			lits[0] = f;
		}
		SECTION("other") {
			lits[2] = f;
		}
		REQUIRE(newCardinalityConstraint(ctx, lits, 1));
		REQUIRE(ctx.master()->validVar(f.var()));
	}

	SECTION("test propagate") {
		LitVec assume, expect;
		REQUIRE(newCardinalityConstraint(ctx, lits, 2));
		SECTION("forwardTrue") {
			assume.push_back(a);
			assume.push_back(~c);
			expect.push_back(body);
		}
		SECTION("forwardFalse") {
			assume.push_back(~a);
			assume.push_back(c);
			assume.push_back(~d);
			expect.push_back(~body);
		}
		SECTION("backwardTrue") {
			assume.push_back(body);
			assume.push_back(c);
			assume.push_back(~d);
			expect.push_back(a);
			expect.push_back(~b);
		}
		SECTION("backwardFalse") {
			assume.push_back(~body);
			assume.push_back(d);
			expect.push_back(~a);
			expect.push_back(b);
			expect.push_back(c);
		}
		REQUIRE(checkPropagate(solver, assume, expect));
	}
	SECTION("test propagate conflict") {
		LitVec assume;
		REQUIRE(newCardinalityConstraint(ctx, lits, 2));
		Literal cflLit;
		SECTION("forwardTrueConflict") {
			assume.push_back(a);
			assume.push_back(~c);
			cflLit = ~body;
		}
		SECTION("forwardFalseConflict") {
			assume.push_back(~a);
			assume.push_back(c);
			assume.push_back(~d);
			cflLit = body;
		}
		SECTION("backwardTrueConflict") {
			assume.push_back(body);
			assume.push_back(c);
			assume.push_back(~d);
			cflLit = b;
		}
		SECTION("backwardFalseConflict") {
			assume.push_back(~body);
			assume.push_back(d);
			cflLit = ~b;
		}
		do {
			for (uint32 i = 0; i < assume.size()-1; ++i) {
				REQUIRE(solver.assume(assume[i]));
				REQUIRE(solver.propagate());
			}
			REQUIRE(solver.assume(assume.back()));
			REQUIRE(solver.force(cflLit, 0));
			REQUIRE_FALSE(solver.propagate());
			LitVec cfl = solver.conflict();
			REQUIRE(assume.size() + 1 == cfl.size());
			for (uint32 i = 0; i < assume.size(); ++i) {
				REQUIRE(std::find(cfl.begin(), cfl.end(), assume[i]) != cfl.end());
			}
			REQUIRE(std::find(cfl.begin(), cfl.end(), cflLit) != cfl.end());
			solver.undoUntil(0);
		} while (std::next_permutation(assume.begin(), assume.end()));
	}
	SECTION("testReasonBug") {
		lits.push_back(~e);
		REQUIRE(newCardinalityConstraint(ctx, lits, 3));
		LitVec assume, reason;
		assume.push_back(a);
		assume.push_back(~b);
		assume.push_back(~d);
		assume.push_back(e);
		for (uint32 i = 0; i < assume.size(); ++i) {
			REQUIRE(solver.assume(assume[i]));
			REQUIRE(solver.propagate());
		}
		REQUIRE(assume.size() == solver.numAssignedVars());

		// B -> ~c because of: ~d, e, B
		REQUIRE(solver.assume(body));
		REQUIRE(solver.propagate());
		REQUIRE(solver.isTrue(~c));
		//REQUIRE(con == solver.reason(c.var()).constraint());
		solver.reason(~c, reason);
		REQUIRE(LitVec::size_type(3) == reason.size());
		REQUIRE(std::find(reason.begin(), reason.end(), ~d) != reason.end());
		REQUIRE(std::find(reason.begin(), reason.end(), e) != reason.end());
		REQUIRE(std::find(reason.begin(), reason.end(), body) != reason.end());
		solver.undoUntil(solver.decisionLevel()-1);
		reason.clear();

		// ~B -> c because of: a, ~b, ~B
		REQUIRE(solver.assume(~body));
		REQUIRE(solver.propagate());
		REQUIRE(solver.isTrue(c));
		// REQUIRE(con == solver.reason(c.var()).constraint());
		solver.reason(c, reason);
		REQUIRE(LitVec::size_type(3) == reason.size());
		REQUIRE(std::find(reason.begin(), reason.end(), a) != reason.end());
		REQUIRE(std::find(reason.begin(), reason.end(), ~b) != reason.end());
		REQUIRE(std::find(reason.begin(), reason.end(), ~body) != reason.end());
		solver.undoUntil(solver.decisionLevel()-1);
		reason.clear();

		// ~c -> B because of: a, ~b, ~c
		REQUIRE(solver.assume(~c));
		REQUIRE(solver.propagate());
		REQUIRE(solver.isTrue(body));
		//REQUIRE(con == solver.reason(body.var()).constraint());
		solver.reason(body, reason);
		REQUIRE(LitVec::size_type(3) == reason.size());
		REQUIRE(std::find(reason.begin(), reason.end(), a) != reason.end());
		REQUIRE(std::find(reason.begin(), reason.end(), ~b) != reason.end());
		REQUIRE(std::find(reason.begin(), reason.end(), ~c) != reason.end());
		solver.undoUntil(solver.decisionLevel()-1);

		// c -> ~B because of: ~d, e, c
		REQUIRE(solver.assume(c));
		REQUIRE(solver.propagate());
		REQUIRE(solver.isTrue(~body));
		//REQUIRE(con == solver.reason(body.var()).constraint());
		solver.reason(~body, reason);
		REQUIRE(LitVec::size_type(3) == reason.size());
		REQUIRE(std::find(reason.begin(), reason.end(), ~d) != reason.end());
		REQUIRE(std::find(reason.begin(), reason.end(), e) != reason.end());
		REQUIRE(std::find(reason.begin(), reason.end(), c) != reason.end());
		solver.undoUntil(solver.decisionLevel()-1);
		reason.clear();
	}
	SECTION("testOrderBug") {
		lits.clear();
		lits.push_back(body);
		lits.push_back(a);
		lits.push_back(b);
		REQUIRE(newCardinalityConstraint(ctx, lits, 1));
		solver.assume(e) && solver.propagate();

		solver.force(~a, 0);
		solver.force(body, 0);
		REQUIRE(solver.propagate());
		REQUIRE(solver.isTrue(b));
		LitVec reason;
		solver.reason(b, reason);
		REQUIRE(LitVec::size_type(2) == reason.size());
		REQUIRE(std::find(reason.begin(), reason.end(), body) != reason.end());
		REQUIRE(std::find(reason.begin(), reason.end(), ~a) != reason.end());
	}
	SECTION("testBackwardAfterForward") {
		lits.clear();
		lits.push_back(body);
		lits.push_back(a);
		lits.push_back(b);
		REQUIRE(newCardinalityConstraint(ctx, lits, 1));
		solver.assume(a);
		REQUIRE(solver.propagate());
		REQUIRE(solver.isTrue(body));
		LitVec reason;
		solver.reason(body, reason);
		REQUIRE(LitVec::size_type(1) == reason.size());
		REQUIRE(std::find(reason.begin(), reason.end(), a) != reason.end());

		solver.assume(~b);
		REQUIRE(solver.propagate());
		REQUIRE(solver.isTrue(body));
	}
	SECTION("testSimplify") {
		REQUIRE(newCardinalityConstraint(ctx, lits, 2));
		REQUIRE(solver.simplify());
		REQUIRE(1u == ctx.numConstraints());
		solver.force(a, 0);
		solver.simplify();
		REQUIRE(1u == ctx.numConstraints());
		solver.force(~c, 0);
		solver.simplify();
		REQUIRE(0u == ctx.numConstraints());
	}

	SECTION("testSimplifyCardinality") {
		REQUIRE(newCardinalityConstraint(ctx, lits, 2));
		ctx.addUnary(~a);
		ctx.addUnary(~d);
		ctx.addUnary(~b);
		solver.simplify();
		solver.assume(~c);
		solver.propagate();
		REQUIRE(solver.isTrue(body));
		LitVec out;
		solver.reason(body, out);
		// a, d, and ~b were removed from constraint
		REQUIRE((out.size() == 1 && out[0] == ~c));
	}
	SECTION("testSimplifyWatches") {
		REQUIRE(newCardinalityConstraint(ctx, lits, 2));
		uint32 nw3 = solver.numWatches(lits[3]) + solver.numWatches(~lits[3]);
		solver.pushRoot(body);
		REQUIRE(nw3 >= solver.numWatches(lits[3]) + solver.numWatches(~lits[3]));
		solver.popRootLevel(1);
		REQUIRE(nw3 == solver.numWatches(lits[3]) + solver.numWatches(~lits[3]));
	}
}
TEST_CASE("Weight constraints", "[constraint][pb][asp]") {
	SharedContext ctx;
	Solver& solver = *ctx.master();
	Literal body = posLit(ctx.addVar(Var_t::Body));
	Literal a = posLit(ctx.addVar(Var_t::Atom));
	Literal b = posLit(ctx.addVar(Var_t::Atom));
	Literal c = posLit(ctx.addVar(Var_t::Atom));
	Literal d = posLit(ctx.addVar(Var_t::Atom));
	Literal e = posLit(ctx.addVar(Var_t::Atom));
	Literal f = posLit(ctx.addVar(Var_t::Atom));
	ctx.startAddConstraints(10);
	WeightLitVec wlits;
	wlits.push_back(WeightLiteral(a, 4));
	wlits.push_back(WeightLiteral(~b, 2));
	wlits.push_back(WeightLiteral(~c, 1));
	wlits.push_back(WeightLiteral(d, 1));
	SECTION("testAssertWeightTriviallySat") {
		wlits.assign(1, WeightLiteral(a, 2));
		REQUIRE(newWeightConstraint(ctx, body, wlits, 0));
		REQUIRE(solver.isTrue(body));
	}
	SECTION("testAssertWeightTriviallyUnSat") {
		wlits.assign(1, WeightLiteral(a, 2));
		REQUIRE(newWeightConstraint(ctx, body, wlits, 3));
		REQUIRE(solver.isFalse(body));
	}
	SECTION("testAssertWeightNotSoTriviallySat") {
		solver.force(wlits[1].first, 0);
		REQUIRE(newWeightConstraint(ctx, body, wlits, 2));
		REQUIRE(solver.isTrue(body));
	}
	SECTION("testAssertWeightNotSoTriviallyUnSat") {
		solver.force(~wlits[0].first, 0);
		solver.force(~wlits[2].first, 0);
		REQUIRE(newWeightConstraint(ctx, body, wlits, 4));
		REQUIRE(solver.isTrue(~body));
	}
	SECTION("testTrivialBackpropTrue") {
		solver.force(body, 0);
		REQUIRE(newWeightConstraint(ctx, body, wlits, 7));
		REQUIRE(ctx.numConstraints() == 1);
		REQUIRE(solver.isTrue(a));
		REQUIRE(solver.isTrue(~b));
		solver.propagate();
		solver.assume(~wlits[0].first) && solver.propagate();
		REQUIRE(solver.isTrue(wlits[1].first));
	}
	SECTION("testTrivialBackpropFalseWeight") {
		solver.force(~body, 0);
		REQUIRE(newWeightConstraint(ctx, body, wlits, 2));
		REQUIRE(ctx.numConstraints() == 1);
		REQUIRE(solver.isFalse(a));
		REQUIRE(solver.isFalse(~b));
	}
	SECTION("testWeightReasonAfterBackprop") {
		REQUIRE(newWeightConstraint(ctx, body, wlits, 3));
		solver.assume(~body) && solver.propagate();
		REQUIRE(solver.isTrue(~a));
		solver.assume(d) && solver.propagate();
		REQUIRE(solver.isTrue(b));
		LitVec r;
		solver.reason(~a, r);
		REQUIRE((r.size() == 1 && r[0] == ~body));
		solver.reason(b, r);
		REQUIRE((r.size() == 2 && r[0] == ~body && r[1] == d));
		solver.undoUntil(solver.decisionLevel()-1);
		solver.reason(~a, r);
		REQUIRE((r.size() == 1 && r[0] == ~body));
		solver.undoUntil(solver.decisionLevel()-1);
	}

	SECTION("testOnlyBTB") {
		ctx.setConcurrency(2);
		wlits.clear();
		wlits.push_back(WeightLiteral(a, 1));
		wlits.push_back(WeightLiteral(b, 1));
		wlits.push_back(WeightLiteral(c, 1));
		wlits.push_back(WeightLiteral(d, 1));
		WeightConstraint::CPair res = WeightConstraint::create(solver, body, wlits, 2, WeightConstraint::create_only_btb);
		REQUIRE(res.first());
		ctx.endInit(true);
		for (int i = 0; i != 2; ++i) {
			INFO("Solver " << i);
			Solver* s = ctx.solver(i);
			Constraint* con = s->constraints()[0];
			CHECK(dynamic_cast<WeightConstraint*>(con));
			CHECK(s->hasWatch(body, con));
			CHECK_FALSE(s->hasWatch(~body, con));
			for (WeightLitVec::const_iterator it = wlits.begin(), end = wlits.end(); it != end; ++it) {
				CHECK_FALSE(s->hasWatch(it->first, con));
				CHECK(s->hasWatch(~it->first, con));
			}

			s->assume(a) && s->propagate();
			s->assume(b) && s->propagate();
			// FTB_BFB not added
			CHECK(s->value(body.var()) == value_free);
			s->undoUntil(0);
			s->assume(~a) && s->propagate();
			s->assume(~b) && s->propagate();
			uint32 dl = s->decisionLevel();
			s->assume(body) && s->propagate();
			CHECK((s->isTrue(c) && s->isTrue(d)));
			s->undoUntil(dl);
			s->assume(~c) && s->propagate();
			CHECK(s->isFalse(body));
		}
	}
	SECTION("testOnlyBFB") {
		ctx.setConcurrency(2);
		wlits.clear();
		wlits.push_back(WeightLiteral(a, 1));
		wlits.push_back(WeightLiteral(b, 1));
		wlits.push_back(WeightLiteral(c, 1));
		wlits.push_back(WeightLiteral(d, 1));
		WeightConstraint::CPair res = WeightConstraint::create(solver, body, wlits, 2, WeightConstraint::create_only_bfb);
		REQUIRE(res.first());
		ctx.endInit(true);
		for (int i = 0; i != 2; ++i) {
			INFO("Solver " << i);
			Solver* s = ctx.solver(i);
			Constraint* con = s->constraints()[0];
			CHECK(dynamic_cast<WeightConstraint*>(con));
			CHECK_FALSE(s->hasWatch(body, con));
			CHECK(s->hasWatch(~body, con));
			for (WeightLitVec::const_iterator it = wlits.begin(), end = wlits.end(); it != end; ++it) {
				CHECK(s->hasWatch(it->first, con));
				CHECK_FALSE(s->hasWatch(~it->first, con));
			}
			s->assume(a) && s->propagate();
			uint32 dl = s->decisionLevel();
			s->assume(b) && s->propagate();
			CHECK(s->isTrue(body));
			s->undoUntil(dl);
			s->assume(~body) && s->propagate();
			CHECK((s->isFalse(b) && s->isFalse(c) && s->isFalse(d)));
			s->undoUntil(0);
			s->assume(~a) && s->propagate();
			s->assume(~b) && s->propagate();
			s->assume(~b) && s->propagate();
			CHECK(s->value(body.var()) == value_free);
		}
	}

	SECTION("testSimplifyWeight") {
		wlits[2].second = 2;
		wlits.push_back(WeightLiteral(e, 1));
		wlits.push_back(WeightLiteral(f, 1));
		REQUIRE(newWeightConstraint(ctx, body, wlits, 2));
		ctx.addUnary(body);
		ctx.addUnary(~a);
		ctx.addUnary(~d);
		ctx.addUnary(~e);
		ctx.addUnary(~f);
		solver.simplify();
		REQUIRE(solver.value(b.var()) == value_free);
		solver.assume(c);
		solver.propagate();
		REQUIRE(solver.isTrue(~b));
		LitVec out;
		solver.reason(~b, out);
		REQUIRE((out.size() == 1 && out[0] == c));
	}

	SECTION("test propagate") {
		LitVec assume, expect;
		SECTION("weight forward true") {
			expect.push_back(body);
			SECTION("with max weight") {
				assume.push_back(a);
			}
			SECTION("with med weight") {
				assume.push_back(~b);
				assume.push_back(~c);
			}
			SECTION("with min weight") {
				assume.push_back(~b);
				assume.push_back(d);
			}
		}
		SECTION("weight forward false") {
			assume.push_back(~a);
			assume.push_back(b);
			expect.push_back(~body);
		}
		REQUIRE(newWeightConstraint(ctx, body, wlits, 3));
		REQUIRE(checkPropagate(solver, assume, expect));
	}

	SECTION("testWeightBackwardTrue") {
		REQUIRE(newWeightConstraint(ctx, body, wlits, 3));
		solver.assume(~a);
		solver.force(body, 0);
		REQUIRE(solver.propagate());
		REQUIRE(solver.isTrue(~b));
		REQUIRE(value_free == solver.value(c.var()));
		LitVec r;
		solver.reason(~b, r);
		REQUIRE(r.size() == 2);
		REQUIRE(std::find(r.begin(), r.end(), ~a) != r.end());
		REQUIRE(std::find(r.begin(), r.end(), body) != r.end());

		solver.assume(~d);
		REQUIRE(solver.propagate());
		REQUIRE(solver.isTrue(~c));

		solver.reason(~c, r);
		REQUIRE(r.size() == 3);
		REQUIRE(std::find(r.begin(), r.end(), ~a) != r.end());
		REQUIRE(std::find(r.begin(), r.end(), body) != r.end());
		REQUIRE(std::find(r.begin(), r.end(), ~d) != r.end());

		solver.undoUntil(solver.decisionLevel()-1);
		solver.reason(~b, r);
		REQUIRE(r.size() == 2);
		REQUIRE(std::find(r.begin(), r.end(), ~a) != r.end());
		REQUIRE(std::find(r.begin(), r.end(), body) != r.end());
	}

	SECTION("testWeightBackwardFalse") {
		REQUIRE(newWeightConstraint(ctx, body, wlits, 3));
		solver.assume(~body);
		REQUIRE(solver.propagate());
		REQUIRE(solver.isTrue(~a));
		LitVec r;
		solver.reason(~a, r);
		REQUIRE(r.size() == 1);
		REQUIRE(std::find(r.begin(), r.end(), ~body) != r.end());

		solver.force(~b, 0);
		REQUIRE(solver.propagate());
		REQUIRE(solver.isTrue(c));
		REQUIRE(solver.isTrue(~d));

		LitVec r2;
		solver.reason(c, r);
		solver.reason(~d, r2);
		REQUIRE(r == r2);
		REQUIRE(r.size() == 2);
		REQUIRE(std::find(r.begin(), r.end(), ~body) != r.end());
		REQUIRE(std::find(r.begin(), r.end(), ~b) != r.end());
	}

	SECTION("testWeightConflict") {
		REQUIRE(newWeightConstraint(ctx, body, wlits, 3));
		LitVec assume;
		assume.push_back(body);
		assume.push_back(~a);
		assume.push_back(b);
		std::sort(assume.begin(), assume.end());
		do {
			REQUIRE(solver.assume(assume[0]));
			for (uint32 i = 1; i < assume.size(); ++i) {
				REQUIRE(solver.force(assume[i],0));
			}
			REQUIRE_FALSE(solver.propagate());
			solver.undoUntil(0);
		} while (std::next_permutation(assume.begin(), assume.end()));
	}

	SECTION("testCloneWeight") {
		ctx.setConcurrency(2);
		REQUIRE(newWeightConstraint(ctx, body, wlits, 3));
		solver.force(~a, 0);
		Solver& solver2 = ctx.pushSolver();
		ctx.endInit(true);

		REQUIRE(solver2.numConstraints() == 1);

		REQUIRE((solver2.numWatches(a) == 0 && solver2.numWatches(~a) == 0));
		solver2.assume(body);
		solver2.propagate();
		solver2.assume(~d);
		solver2.propagate();
		REQUIRE(solver2.isTrue(~b));
		REQUIRE(solver2.isTrue(~c));
		LitVec out;
		solver2.reason(~b, out);
		REQUIRE(std::find(out.begin(), out.end(), body) != out.end());
		REQUIRE(std::find(out.begin(), out.end(), ~a) != out.end());
		REQUIRE(std::find(out.begin(), out.end(), ~d) == out.end());
		out.clear();
		solver2.reason(~c, out);
		REQUIRE(std::find(out.begin(), out.end(), ~d) != out.end());
	}

	SECTION("testCloneWeightShared") {
		ctx.setConcurrency(2);
		ctx.setShareMode(ContextParams::share_problem);
		REQUIRE(newWeightConstraint(ctx, body, wlits, 3));
		solver.force(~a, 0);
		Solver& solver2 = ctx.pushSolver();
		ctx.endInit(true);

		REQUIRE(solver2.numConstraints() == 1);

		REQUIRE((solver2.numWatches(a) == 0 && solver2.numWatches(~a) == 0));
		solver2.assume(body);
		solver2.propagate();
		solver2.assume(~d);
		solver2.propagate();
		REQUIRE(solver2.isTrue(~b));
		REQUIRE(solver2.isTrue(~c));
		LitVec out;
		solver2.reason(~b, out);
		REQUIRE(std::find(out.begin(), out.end(), body) != out.end());
		REQUIRE(std::find(out.begin(), out.end(), ~a) != out.end());
		REQUIRE(std::find(out.begin(), out.end(), ~d) == out.end());
		out.clear();
		solver2.reason(~c, out);
		REQUIRE(std::find(out.begin(), out.end(), ~d) != out.end());
	}

	SECTION("testAddOnLevel") {
		ctx.endInit(true);
		uint32 sz = sizeVec(wlits) + 1;
		Solver& s = *ctx.master();
		s.pushRoot(f);
		WeightConstraint::CPair res = WeightConstraint::create(s, body, wlits, 2, WeightConstraint::create_no_add);
		REQUIRE((res.ok() && res.first() != 0));
		REQUIRE((res.first()->size() == sz && wlits.size() == sz-1));
		s.force(body);
		s.force(~wlits[0].first);
		s.force(~wlits[1].first,0);
		s.propagate();
		REQUIRE((s.isTrue(wlits[2].first) && s.isTrue(wlits[3].first)));
		res.first()->destroy(&s, true);
	}
	SECTION("testAddPBOnLevel") {
		ctx.endInit(true);
		Solver& s = *ctx.master();
		s.pushRoot(f);
		WeightConstraint* wc = WeightConstraint::create(s, lit_true(), wlits, 2, WeightConstraint::create_no_add).first();
		wc->destroy(&s, true);
		REQUIRE_FALSE(s.removeUndoWatch(s.decisionLevel(), wc));
	}
	SECTION("testIntegrateRoot") {
		ctx.endInit(true);
		Solver& s = *ctx.master();
		s.pushRoot(~c);
		s.pushRoot(d);
		WeightConstraint* wc = WeightConstraint::create(s, body, wlits, 3, WeightConstraint::create_no_add).first();
		REQUIRE(s.removeUndoWatch(2, wc));
		REQUIRE(s.removeUndoWatch(1, wc));
		wc->destroy(&s, true);
	}
	SECTION("testCreateSat") {
		ctx.endInit(true);
		Solver& s = *ctx.master();

		s.force(wlits[0].first);
		s.force(wlits[1].first);
		WeightConstraint::CPair res = WeightConstraint::create(s, body, wlits, 2, WeightConstraint::create_no_add|WeightConstraint::create_sat);
		REQUIRE((res.ok() && res.first() != 0));
		REQUIRE(s.isTrue(body));
		s.propagate();
		REQUIRE(s.isTrue(body));
		while (!wlits.empty()) {
			REQUIRE((s.force(~wlits.back().first) && s.propagate()));
			wlits.pop_back();
		}
		res.first()->destroy(&s, true);
	}
	SECTION("testCreateSatOnRoot") {
		ctx.endInit(true);
		Solver& s = *ctx.master();
		s.pushRoot(f);
		REQUIRE(s.rootLevel() == 1);
		s.force(a, 0);
		s.force(b, 0);
		s.propagate();
		wlits.clear();
		wlits.push_back(WeightLiteral(a, 1));
		wlits.push_back(WeightLiteral(b, 1));
		wlits.push_back(WeightLiteral(c, 1));
		wlits.push_back(WeightLiteral(d, 1));
		WeightLitsRep rep           = WeightLitsRep::create(s, wlits, 2);
		WeightConstraint::CPair res = WeightConstraint::create(s, body, rep, WeightConstraint::create_no_add|WeightConstraint::create_sat);
		REQUIRE((res.ok() && res.first()));
		REQUIRE(s.isTrue(body));
		REQUIRE(s.reason(body) == res.first());
		s.popRootLevel(1);
		REQUIRE(s.value(body.var()) == value_free);
		res.first()->destroy(&s, true);
	}
	SECTION("testCreateSatOnRootNoProp") {
		ctx.endInit(true);
		Solver& s = *ctx.master();
		s.pushRoot(f);
		REQUIRE(s.rootLevel() == 1);
		s.force(a, 0);
		s.force(b, 0);
		wlits.clear();
		wlits.push_back(WeightLiteral(a, 1));
		wlits.push_back(WeightLiteral(b, 1));
		wlits.push_back(WeightLiteral(c, 1));
		wlits.push_back(WeightLiteral(d, 1));
		WeightLitsRep rep           = WeightLitsRep::create(s, wlits, 2);
		WeightConstraint::CPair res = WeightConstraint::create(s, body, rep, WeightConstraint::create_no_add|WeightConstraint::create_sat);
		REQUIRE((res.ok() && res.first()));
		REQUIRE(!s.isTrue(body));
		REQUIRE(s.propagate());
		REQUIRE(s.isTrue(body));
		REQUIRE(s.reason(body) == res.first());
		s.popRootLevel(1);
		REQUIRE(s.value(body.var()) == value_free);
		res.first()->destroy(&s, true);
	}
	SECTION("testMergeNegativeWeight") {
		wlits.clear();
		wlits.push_back(WeightLiteral(a, -1));
		wlits.push_back(WeightLiteral(a, -1));
		wlits.push_back(WeightLiteral(b, -2));
		WeightLitsRep rep = WeightLitsRep::create(*ctx.master(), wlits, -2);
		REQUIRE(rep.size == 2);
		REQUIRE(rep.bound == 1);
		REQUIRE(rep.reach == 2);
		REQUIRE(rep.lits[0].second == 1);
		REQUIRE(rep.lits[1].second == 1);
	}
}
} }
