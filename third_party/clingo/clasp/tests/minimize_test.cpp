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
#pragma warning (disable : 4996) // std::equal may be unsafe
#endif
#include <clasp/minimize_constraint.h>
#include <clasp/solver.h>
#include <clasp/solve_algorithms.h>
#include <algorithm>
#include "catch.hpp"
namespace Clasp { namespace Test {
namespace {
struct BranchAndBoundTest {
	BranchAndBoundTest() : min(0), data(0) {}
	~BranchAndBoundTest() {
		if (min)  { min->destroy(ctx.master(), true); }
		if (data) { data->release(); }
	}
	uint32 countMinLits() {
		uint32 lits = 0;
		const WeightLiteral* it = min->shared()->lits;
		for (; !isSentinel(it->first); ++it, ++lits) { ; }
		return lits;
	}
	bool setOptimum(Solver& s, SumVec& vec, bool less) {
		SharedMinimizeData* d = const_cast<SharedMinimizeData*>(min->shared());
		if (!less) {
			vec.back() += 1;
		}
		d->setOptimum(&vec[0]);
		while (!min->integrateBound(s)) {
			if (!s.resolveConflict()) { return false; }
		}
		return true;
	}
	DefaultMinimize* createMin(Solver& s, SharedMinimizeData* m, OptParams::BBAlgo param = OptParams::bb_lin) {
		ctx.endInit();
		OptParams p; p.algo = param;
		return static_cast<DefaultMinimize*>(m->attach(s, p));
	}
	DefaultMinimize* buildAndAttach(MinimizeBuilder& x, MinimizeMode m = MinimizeMode_t::optimize, const wsum_t* b = 0, uint32 bs = 0) {
		DefaultMinimize* con = 0;
		if ((data = x.build(ctx)) != 0 && data->setMode(m, b, bs)) {
			con = createMin(*ctx.master(), data);
			con->integrateBound(*ctx.master());
		}
		return con;
	}
	SharedContext       ctx;
	DefaultMinimize*    min;
	SharedMinimizeData* data;
};
}
TEST_CASE("Model-guided minimize", "[constraint][asp]") {
	BranchAndBoundTest test;
	SharedContext& ctx = test.ctx;
	DefaultMinimize*& newMin = test.min;
	SharedMinimizeData*& data = test.data;
	Literal a = posLit(ctx.addVar(Var_t::Atom));
	Literal b = posLit(ctx.addVar(Var_t::Atom));
	Literal c = posLit(ctx.addVar(Var_t::Atom));
	Literal d = posLit(ctx.addVar(Var_t::Atom));
	Literal e = posLit(ctx.addVar(Var_t::Atom));
	Literal f = posLit(ctx.addVar(Var_t::Atom));
	Literal x = posLit(ctx.addVar(Var_t::Atom));
	Literal y = posLit(ctx.addVar(Var_t::Atom));
	ctx.startAddConstraints();
	MinimizeBuilder mb;
	WeightLitVec min;
	Solver& solver = *ctx.master();
	SECTION("testEmpty") {
		REQUIRE(!mb.build(ctx));
	}
	SECTION("testEmptyMultiLevel") {
		mb.add(0, WeightLiteral(posLit(0), 1));
		mb.add(1, WeightLiteral(posLit(0), 1));
		data = mb.build(ctx);
		REQUIRE(isSentinel(data->lits[0].first));
		REQUIRE(data->lits[0].second == 0);
		REQUIRE(data->weights.size() == 1);
	}
	SECTION("testNewVariableAfterStartAddConstraints") {
		Literal z = posLit(ctx.addVar(Var_t::Atom));
		mb.add(0, WeightLiteral(z, 1));
		REQUIRE_FALSE(ctx.master()->validVar(z.var()));
		SECTION("assigned") {
			ctx.addUnary(z);
			REQUIRE(ctx.master()->validVar(z.var()));
			data = mb.build(ctx);
		}
		SECTION("unassigned") {
			data = mb.build(ctx);
			REQUIRE(ctx.master()->validVar(z.var()));
		}
		REQUIRE(data);
	}

	SECTION("testOneLevelLits") {
		ctx.addUnary(c);
		ctx.addUnary(~e);
		min.push_back( WeightLiteral(a, 1) );
		min.push_back( WeightLiteral(b, 2) );
		min.push_back( WeightLiteral(c, 1) ); // true lit
		min.push_back( WeightLiteral(a, 2) ); // duplicate lit
		min.push_back( WeightLiteral(d, 1) );
		min.push_back( WeightLiteral(e, 2) ); // false lit
		newMin = test.buildAndAttach(mb.add(0, min));
		REQUIRE(newMin->numRules() == 1);
		REQUIRE(test.countMinLits() == 3);
		REQUIRE(newMin->shared()->adjust(0) == 1);
		const SharedMinimizeData* data = newMin->shared();
		REQUIRE((data->lits[0].first == a && data->lits[0].second == 3));
		for (const WeightLiteral* it = data->lits; !isSentinel(it->first); ++it) {
			REQUIRE(it->second >= (it+1)->second);
			REQUIRE(ctx.master()->hasWatch(it->first, newMin));
			REQUIRE(ctx.varInfo(it->first.var()).frozen());
		}
		newMin->destroy(ctx.master(), true);
		newMin = 0;
		REQUIRE(!ctx.master()->hasWatch(a, newMin));
	}

	SECTION("testMultiLevelLits") {
		ctx.addUnary(c);
		ctx.addUnary(~e);
		min.push_back( WeightLiteral(a, 1) );
		min.push_back( WeightLiteral(b, 1) );
		min.push_back( WeightLiteral(c, 1) ); // true lit
		min.push_back( WeightLiteral(b, 2) ); // duplicate lit
		min.push_back( WeightLiteral(d, 1) );
		min.push_back( WeightLiteral(b, 1) ); // duplicate lit

		mb.add(3, min);
		min.clear();
		min.push_back( WeightLiteral(e, 2) ); // false lit
		min.push_back( WeightLiteral(f, 1) );
		min.push_back( WeightLiteral(x, 2) );
		min.push_back( WeightLiteral(y, 3) );
		min.push_back( WeightLiteral(b, 1) ); // duplicate lit
		mb.add(2, min);
		min.clear();
		min.push_back( WeightLiteral(b, 2) ); // duplicate lit
		min.push_back( WeightLiteral(a, 1) ); // duplicate lit
		min.push_back( WeightLiteral(a, 2) ); // duplicate lit
		min.push_back( WeightLiteral(c, 2) ); // true lit
		min.push_back( WeightLiteral(d, 1) ); // duplicate lit
		mb.add(1, min);

		newMin = test.buildAndAttach(mb);
		REQUIRE(newMin->numRules() == 3);
		REQUIRE(test.countMinLits() == 6);
		const SharedMinimizeData* data = newMin->shared();
		REQUIRE((data->adjust(0) == 1 && data->adjust(1) == 0 && data->adjust(2) == 2));
		REQUIRE(data->lits[0].first == b);
		REQUIRE(data->weights.size() == 11);
		for (const WeightLiteral* it = data->lits; !isSentinel(it->first); ++it) {
			REQUIRE(ctx.master()->hasWatch(it->first, newMin));
		}
	}

	SECTION("testMultiLevelWeightsAreReused") {
		min.push_back( WeightLiteral(a, 1) );
		min.push_back( WeightLiteral(b, 1) );
		min.push_back( WeightLiteral(b, 2) );
		min.push_back( WeightLiteral(c, 1) );
		min.push_back( WeightLiteral(d, 3) );
		mb.add(3, min);

		min.clear();
		min.push_back( WeightLiteral(b, 1) );
		min.push_back( WeightLiteral(d, 1) );
		mb.add(2, min);

		newMin = test.buildAndAttach(mb);
		// b = 0
		// d = 0
		// a = 2
		// c = 2
		// weights: [(0,3)(1,1)][(0,1)] + [(1,0)] for sentinel
		REQUIRE(newMin->shared()->weights.size() == 4);
	}

	SECTION("testMergeComplementaryLits") {
		min.push_back( WeightLiteral(a, 1) );
		min.push_back( WeightLiteral(b, 1) );
		min.push_back( WeightLiteral(c, 2) );
		min.push_back( WeightLiteral(d, 1) );
		min.push_back( WeightLiteral(~d, 2) );
		mb.add(2, min);
		min.clear();
		min.push_back( WeightLiteral(~c, 1) );
		min.push_back( WeightLiteral(e, 1) );
		mb.add(1, min);
		newMin = test.buildAndAttach(mb);
		REQUIRE(test.countMinLits() == 5);
		REQUIRE((newMin->shared()->adjust(0) == 1 && newMin->shared()->adjust(1) == 1));

		ctx.master()->assume(c) && ctx.master()->propagate();
		REQUIRE(newMin->sum(1, true) == 0);

	}

	SECTION("testMergeComplementaryLits2") {
		min.push_back( WeightLiteral(a, 1) );
		min.push_back( WeightLiteral(~a, 1) );
		mb.add(2, min);
		min.clear();
		min.push_back( WeightLiteral(a, 1) );
		mb.add(1, min);
		newMin = test.buildAndAttach(mb);
		REQUIRE(test.countMinLits() == 1);
		REQUIRE((newMin->shared()->adjust(0) == 1 && newMin->shared()->adjust(1) == 0));
	}
	SECTION("testMergeComplementaryLits3") {
		min.push_back( WeightLiteral(~a, 2) );
		min.push_back( WeightLiteral(~a, -3));
		mb.add(1, min);
		min.clear();
		min.push_back(WeightLiteral(~a, 1) );
		min.push_back(WeightLiteral(a, -4) );
		mb.add(0, min);
		newMin = test.buildAndAttach(mb);
		REQUIRE(test.countMinLits() == 1);
		REQUIRE((newMin->shared()->lits[0].first == a));
		REQUIRE((newMin->shared()->adjust(0) == -1 && newMin->shared()->adjust(1) == 1));
		REQUIRE((newMin->shared()->weights[0].weight == 1));
		REQUIRE((newMin->shared()->weights[1].weight == -5));
	}
	SECTION("testSparseCompare") {
		mb.add(0, WeightLiteral(b, 1));
		mb.add(1, WeightLiteral(a, 1));
		mb.add(2, WeightLiteral(c, -1));
		mb.add(2, WeightLiteral(b, -2));
		mb.add(2, WeightLiteral(a, -2));

		newMin = test.buildAndAttach(mb);
		MinimizeConstraint::SharedDataP shared = newMin->shared();

#define CHECK_LEVEL_WEIGHT(idx, w, l, n) \
	CHECK(shared->weights.at(idx).weight == w); \
	CHECK(shared->weights.at(idx).level == l); \
	CHECK(shared->weights.at(idx).next == n)

		REQUIRE(test.countMinLits() == 3);

		CHECK(shared->lits[0] == WeightLiteral(~b, 0));
		CHECK_LEVEL_WEIGHT(0, 2, 0, 1);
		CHECK_LEVEL_WEIGHT(1, -1, 2, 0);

		CHECK(shared->lits[1] == WeightLiteral(~a, 2));
		CHECK_LEVEL_WEIGHT(2, 2, 0, 1);
		CHECK_LEVEL_WEIGHT(3, -1, 1, 0);

		CHECK(shared->lits[2] == WeightLiteral(~c, 4));
		CHECK_LEVEL_WEIGHT(4, 1, 0, 0);

		CHECK(shared->adjust(0) == -5);
		CHECK(shared->adjust(1) == 1);
		CHECK(shared->adjust(2) == 1);
	}
	SECTION("testInitFromOther") {
		min.push_back( WeightLiteral(~a, 2) );
		min.push_back( WeightLiteral(~a, -3));
		mb.add(1, min);
		mb.add(1, CLASP_WEIGHT_T_MIN);
		min.clear();
		min.push_back(WeightLiteral(~a, 1) );
		min.push_back(WeightLiteral(a, -4) );
		mb.add(0, min);
		mb.add(0, CLASP_WEIGHT_T_MAX);
		SharedMinimizeData* d1 = mb.build(ctx);
		mb.clear();
		mb.add(*d1);
		SharedMinimizeData* d2 = mb.build(ctx);
		REQUIRE(d1->numRules() == d2->numRules());
		REQUIRE(std::equal(d1->adjust(), d1->adjust() + d1->numRules(), d2->adjust()));
		REQUIRE(d1->weights.size() == d2->weights.size());
		for (uint32 i = 0, end = (uint32)d1->weights.size(); i != end; ++i) {
			REQUIRE((d1->weights[i].level == d2->weights[i].level && d1->weights[i].weight == d2->weights[i].weight && d1->weights[i].next == d2->weights[i].next));
		}
		for (const WeightLiteral* it = d1->lits, *oit = d2->lits; !isSentinel(it->first); ++it, ++oit) {
			REQUIRE(*it == *oit);
		}
		d1->release();
		d2->release();
	}
	SECTION("testNegativeLowerInit") {
		WeightLitVec aMin, bMin, cMin;
		aMin.push_back( WeightLiteral(a, 1) );
		aMin.push_back( WeightLiteral(b, 1) );
		bMin.push_back( WeightLiteral(~a, 1) );
		bMin.push_back( WeightLiteral(~b, 1) );
		cMin.push_back( WeightLiteral(a, 1) );
		cMin.push_back( WeightLiteral(b, 1) );
		data = mb
			.add(3, aMin)
			.add(2, bMin)
			.add(1, cMin)
			.build(ctx);
		REQUIRE(data->lower(0) == 0);
		REQUIRE(data->lower(1) == -2);
		REQUIRE(data->lower(2) == 0);
	}
	SECTION("testNegativeLower") {
		ctx.addBinary(a, b);
		WeightLitVec aMin, bMin, cMin;
		aMin.push_back( WeightLiteral(a, 1) );
		aMin.push_back( WeightLiteral(b, 1) );
		bMin.push_back( WeightLiteral(~a, 1) );
		cMin.push_back( WeightLiteral(a, 1) );
		data = mb
			.add(3, aMin)
			.add(2, bMin)
			.add(1, cMin)
			.build(ctx);
		newMin = test.createMin(solver, data, OptParams::bb_hier);
		newMin->integrateBound(solver);

		solver.assume(b) && solver.propagate();
		solver.assume(~a) && solver.propagate();
		newMin->handleModel(solver);
		solver.undoUntil(0);
		REQUIRE((!newMin->integrate(solver) || !solver.propagate()));
		LitVec ignore;
		REQUIRE((newMin->handleUnsat(solver, true, ignore)));
		REQUIRE((newMin->integrate(solver) && solver.propagate()));

		REQUIRE(solver.isFalse(b));
		REQUIRE((solver.assume(a) && solver.propagate()));
		newMin->handleModel(solver);
	}

	SECTION("testOrder") {
		WeightLitVec aMin, bMin;
		aMin.push_back( WeightLiteral(a, 1) );
		bMin.push_back( WeightLiteral(b, 1) );
		newMin = test.buildAndAttach(mb
			.add(2, aMin)
			.add(1, bMin));

		solver.assume(b);
		REQUIRE(solver.propagate());
		solver.assume(~a);
		REQUIRE(solver.propagate());
		REQUIRE((newMin->sum(0, true) == 0 && newMin->sum(1, true) == 1));
		newMin->commitUpperBound(solver);
		REQUIRE(newMin->integrateBound(solver));
		REQUIRE(solver.isFalse(a));
		REQUIRE(solver.isFalse(b));
		REQUIRE(solver.decisionLevel() == 0);
	}

	SECTION("testSkipLevel") {
		WeightLitVec aMin, bMin;
		aMin.push_back( WeightLiteral(c, 2) );
		aMin.push_back( WeightLiteral(d, 1) );
		aMin.push_back( WeightLiteral(e, 2) );

		bMin.push_back( WeightLiteral(a, 1) );
		bMin.push_back( WeightLiteral(b, 1) );

		newMin = test.buildAndAttach(mb.add(2, aMin).add(1, bMin));
		REQUIRE((solver.assume(a) && solver.propagate()));
		REQUIRE((solver.assume(b) && solver.propagate()));
		REQUIRE((solver.assume(~d) && solver.propagate()));
		REQUIRE((solver.assume(c) && solver.propagate()));
		solver.force(~e, 0); solver.propagate();
		newMin->commitUpperBound(solver);
		solver.backtrack();
		solver.force(e, 0);
		REQUIRE(newMin->integrateBound(solver));
		REQUIRE((solver.decisionLevel() == 2 && solver.backtrackLevel() == 2));
		REQUIRE((solver.isFalse(c) && solver.isFalse(e)));
		solver.backtrack();
	}

	SECTION("testReassertAfterBacktrack") {
		min.push_back( WeightLiteral(x, 1) );
		min.push_back( WeightLiteral(y, 1) );
		newMin = test.buildAndAttach(mb.add(0, min));
		REQUIRE((solver.assume(a) && solver.propagate()));
		REQUIRE((solver.assume(~x) && solver.propagate()));
		REQUIRE((solver.assume(y) && solver.propagate()));
		newMin->commitUpperBound(solver);
		solver.undoUntil(1);
		// disable backjumping
		solver.setBacktrackLevel(1, Solver::undo_pop_proj_level);
		REQUIRE(newMin->integrateBound(solver));
		REQUIRE((solver.decisionLevel() == 1 && solver.isTrue(~x)));
		solver.backtrack();
		REQUIRE((solver.decisionLevel() == 0 && solver.isTrue(~x)));
	}

	SECTION("testConflict") {
		WeightLitVec aMin, bMin;
		aMin.push_back( WeightLiteral(c, 2) );
		aMin.push_back( WeightLiteral(d, 1) );
		aMin.push_back( WeightLiteral(e, 2) );

		bMin.push_back( WeightLiteral(a, 1) );
		bMin.push_back( WeightLiteral(b, 1) );

		newMin = test.buildAndAttach(mb.add(2, aMin).add(1, bMin));

		REQUIRE((solver.assume(a) && solver.propagate()));
		REQUIRE((solver.assume(b) && solver.propagate()));
		REQUIRE((solver.assume(~d) && solver.propagate()));
		REQUIRE((solver.assume(c) && solver.propagate()));
		solver.force(~e, 0); solver.propagate();
		newMin->commitUpperBound(solver);
		solver.undoUntil(0);
		REQUIRE(newMin->integrateBound(solver));
		REQUIRE((solver.assume(a) && solver.propagate()));
		solver.force(c, 0);
		solver.force(d, 0);
		REQUIRE_FALSE(solver.propagate());
		const LitVec& cfl = solver.conflict();
		REQUIRE(cfl.size() == 3);
		REQUIRE(std::find(cfl.begin(), cfl.end(), a) != cfl.end());
		REQUIRE(std::find(cfl.begin(), cfl.end(), c) != cfl.end());
		REQUIRE(std::find(cfl.begin(), cfl.end(), d) != cfl.end());
	}

	SECTION("testOptimize") {
		WeightLitVec aMin, bMin;
		aMin.push_back( WeightLiteral(a, 1) );
		aMin.push_back( WeightLiteral(b, 1) );
		bMin.push_back( WeightLiteral(c, 1) );
		bMin.push_back( WeightLiteral(d, 1) );
		newMin = test.buildAndAttach(mb.add(2, aMin).add(1, bMin));
		REQUIRE((solver.assume(a) && solver.propagate()));
		REQUIRE((solver.assume(c) && solver.propagate()));
		REQUIRE(newMin->shared()->optimum(0) == SharedMinimizeData::maxBound());
		newMin->commitUpperBound(solver);
		REQUIRE(newMin->shared()->optimum(0) != SharedMinimizeData::maxBound());
		solver.undoUntil(0);
		REQUIRE(newMin->integrateBound(solver));
		REQUIRE((solver.assume(d) && solver.propagate()));
		REQUIRE(solver.isTrue(~a));
		REQUIRE(solver.isTrue(~b));
		REQUIRE(value_free == solver.value(c.var()));
	}

	SECTION("testEnumerate") {
		WeightLitVec aMin, bMin;
		aMin.push_back( WeightLiteral(a, 1) );
		aMin.push_back( WeightLiteral(b, 1) );
		bMin.push_back( WeightLiteral(c, 1) );
		bMin.push_back( WeightLiteral(d, 1) );
		wsum_t bound[2] = {1,1};
		newMin = test.buildAndAttach(mb.add(2, aMin).add(1, bMin), MinimizeMode_t::enumerate, bound, 2);

		REQUIRE(newMin->shared()->optimum(0) == SharedMinimizeData::maxBound());
		REQUIRE(newMin->shared()->sum(0) == SharedMinimizeData::maxBound());
		REQUIRE(newMin->shared()->upper(0) == 1);
		REQUIRE(solver.propagate());

		REQUIRE((solver.assume(~a) && solver.propagate()));
		REQUIRE((solver.assume(c) && solver.propagate()));

		newMin->commitUpperBound(solver);
		solver.undoUntil(0);
		newMin->relaxBound();
		newMin->integrateBound(solver);
		REQUIRE(solver.queueSize() == 0);

		REQUIRE((solver.assume(d) && solver.propagate()));
		REQUIRE((solver.assume(b) && solver.propagate()));
		REQUIRE(solver.isTrue(~c));
		REQUIRE(solver.isTrue(~a));
	}

	SECTION("testComputeImplicationLevel") {
		min.push_back( WeightLiteral(a, 1) );
		min.push_back( WeightLiteral(b, 1) );
		min.push_back( WeightLiteral(c, 1) );
		min.push_back( WeightLiteral(d, 2) );
		newMin = test.buildAndAttach(mb.add(0, min));
		solver.assume(a) && solver.propagate();
		solver.force(b,0)&& solver.propagate();
		solver.assume(c) && solver.propagate();
		solver.force(~d,0) && solver.propagate();
		newMin->commitUpperBound(solver);
		solver.undoUntil(1u);
		REQUIRE(newMin->integrateBound(solver));
		REQUIRE(solver.isFalse(d));
		LitVec r1, r2;
		solver.reason(~d, r1);
		solver.undoUntil(0);
		solver.assume(a) && solver.propagate();
		solver.reason(~d, r2);
		REQUIRE((r2.size() <= r1.size() && r2.size() == 1));
		REQUIRE(r1[0] == r2[0]);
		REQUIRE((r1.size() == 1 || b == r1[1]));
		REQUIRE(r1.size() == r2.size());
	}

	SECTION("testSetModelMayBacktrackMultiLevels") {
		newMin = test.buildAndAttach(mb.add(0, WeightLiteral(a, 1)));
		REQUIRE((solver.assume(a) && solver.propagate()));
		REQUIRE((solver.assume(b) && solver.propagate()));
		REQUIRE((solver.assume(c) && solver.propagate()));
		newMin->commitUpperBound(solver);
		newMin->integrateBound(solver);
		REQUIRE(solver.decisionLevel() == 0);
	}

	SECTION("testPriorityBug") {
		WeightLitVec aMin, bMin;
		aMin.push_back( WeightLiteral(a, 1) );
		aMin.push_back( WeightLiteral(b, 1) );
		aMin.push_back( WeightLiteral(c, 1) );
		bMin.push_back( WeightLiteral(d, 1) );
		bMin.push_back( WeightLiteral(e, 1) );
		bMin.push_back( WeightLiteral(f, 1) );
		newMin = test.buildAndAttach(mb
			.add(2, aMin)
			.add(1, bMin));
		REQUIRE((solver.assume(a) && solver.propagate()));
		REQUIRE((solver.assume(e) && solver.propagate()));
		REQUIRE((solver.assume(f) && solver.propagate()));
		newMin->commitUpperBound(solver);
		solver.backtrack();
		// disbale backjumping
		solver.setBacktrackLevel(2, Solver::undo_pop_proj_level);
		REQUIRE(newMin->integrateBound(solver));
		REQUIRE(solver.decisionLevel() == 2);
		solver.backtrack();
		REQUIRE(solver.propagate());
		REQUIRE((solver.assume(d) && solver.propagate()));
		REQUIRE(solver.isTrue(~b));
		LitVec r;
		solver.reason(~b, r);
		REQUIRE(LitVec::size_type(1) == r.size());
		REQUIRE(a == r[0]);
	}

	SECTION("testStrengthenImplication") {
		min.push_back( WeightLiteral(a, 1) );
		min.push_back( WeightLiteral(b, 2) );
		min.push_back( WeightLiteral(c, 1) );
		wsum_t bound   = 2;
		newMin         = test.buildAndAttach(mb.add(0, min), MinimizeMode_t::optimize, &bound, 1);
		solver.assume(a) && solver.propagate();
		solver.setBacktrackLevel(1, Solver::undo_pop_proj_level);
		REQUIRE(solver.isTrue(~b));
		LitVec r;
		solver.reason(~b, r);
		REQUIRE((r.size() == 1 && r[0] == a));
		solver.assume(x) && solver.propagate();
		solver.assume(y) && solver.propagate();
		solver.assume(c) && solver.propagate();
		solver.assume(e) && solver.propagate();
		newMin->commitUpperBound(solver);
		solver.undoUntil(3);
		REQUIRE(newMin->integrateBound(solver));
		REQUIRE((solver.decisionLevel() == 1 && solver.isTrue(~b)));
		r.clear();
		solver.reason(~b, r);
		REQUIRE((r.empty() || (r.size() == 1 && r[0] == lit_true())));
	}

	SECTION("testRootLevelMadness") {
		WeightLitVec aMin, bMin;
		aMin.push_back( WeightLiteral(a, 1) );
		aMin.push_back( WeightLiteral(b, 2) );
		aMin.push_back( WeightLiteral(c, 1) );
		bMin.push_back( WeightLiteral(d, 1) );
		bMin.push_back( WeightLiteral(e, 2) );
		bMin.push_back( WeightLiteral(f, 1) );
		newMin = test.buildAndAttach(mb
			.add(2, aMin)
			.add(1, bMin));
		solver.assume(a) && solver.propagate();
		solver.assume(c) && solver.propagate();
		solver.pushRootLevel(2);
		SumVec opt(newMin->numRules());
		opt[0] = 2;
		opt[1] = 3;
		test.setOptimum(solver, opt, false);
		LitVec r;
		REQUIRE(solver.isFalse(b));
		solver.reason(~b, r);
		REQUIRE((r.size() == 1 && r[0] == a));
		solver.clearAssumptions();
		solver.assume(d) && solver.propagate();
		solver.assume(e) && solver.propagate();
		solver.assume(f) && solver.propagate();
		solver.pushRootLevel(3);
		opt[0] = 2;
		opt[1] = 2;
		test.setOptimum(solver, opt, false);
		REQUIRE(solver.isFalse(b));
		r.clear();
		solver.reason(~b, r);
		REQUIRE((r.size() == 2 && r[0] == d && r[1] == e));
	}

	SECTION("testIntegrateOptimum") {
		min.push_back( WeightLiteral(a, 1) );
		min.push_back( WeightLiteral(b, 1) );
		min.push_back( WeightLiteral(c, 1) );
		min.push_back( WeightLiteral(d, 1) );
		data   = mb.add(0, min).build(ctx);
		newMin = test.createMin(solver, data);
		newMin->integrateBound(solver);
		solver.assume(a) && solver.propagate();
		solver.assume(e) && solver.propagate();
		solver.assume(b) && solver.propagate();
		solver.assume(f) && solver.propagate();
		solver.assume(c) && solver.propagate();

		SumVec opt(data->numRules());
		opt[0] = 2;
		REQUIRE(test.setOptimum(solver, opt, true));
		REQUIRE((solver.decisionLevel() == 1 && solver.queueSize() == 3));
		newMin->destroy(&solver, true);
		newMin = 0;
	}

	SECTION("testIntegrateOptimumConflict") {
		min.push_back( WeightLiteral(a, 1) );
		min.push_back( WeightLiteral(b, 1) );
		min.push_back( WeightLiteral(c, 1) );
		min.push_back( WeightLiteral(d, 1) );
		data = mb.add(0, min).build(ctx);
		SumVec opt(data->numRules());
		newMin = test.createMin(solver, data);
		newMin->integrateBound(solver);
		solver.assume(a) && solver.propagate();
		solver.assume(e) && solver.propagate();
		solver.assume(b) && solver.propagate();
		solver.assume(f) && solver.propagate();
		solver.assume(c) && solver.propagate();
		solver.setBacktrackLevel(1, Solver::undo_pop_proj_level);
		opt[0] = 2;
		REQUIRE(test.setOptimum(solver, opt, true));
		REQUIRE((solver.decisionLevel() == 1 && solver.queueSize() >= 3));
		solver.propagate();
		opt[0] = 0;
		REQUIRE(!test.setOptimum(solver, opt, true));
		REQUIRE(solver.hasConflict());
		REQUIRE(solver.clearAssumptions());
		REQUIRE((newMin->integrateBound(solver) == false && solver.hasConflict()));
		REQUIRE(!solver.resolveConflict());
		newMin->destroy(&solver, true);
		newMin = 0;
	}
	SECTION("testIntegrateBug") {
		WeightLitVec min;
		min.push_back( WeightLiteral(a, 1) );
		min.push_back( WeightLiteral(b, 1) );
		min.push_back( WeightLiteral(c, 1) );
		data   = mb.add(0, min).build(ctx);
		wsum_t bound = 2;
		newMin = test.createMin(solver, data, OptParams::bb_dec);
		data->setMode(MinimizeMode_t::optimize, &bound, 1);
		solver.pushRoot(solver.tagLiteral());
		solver.pushRoot(a);
		solver.pushRoot(b);
		solver.pushRoot(c);
		REQUIRE(!newMin->integrateBound(solver));
	}
	SECTION("testReasonBug") {
		WeightLitVec aMin, bMin;
		aMin.push_back( WeightLiteral(a, 1) );
		aMin.push_back( WeightLiteral(b, 1) );
		bMin.push_back( WeightLiteral(d, 2) );
		bMin.push_back( WeightLiteral(e, 1) );
		bMin.push_back( WeightLiteral(f, 3) );

		newMin = test.buildAndAttach(mb
			.add(2, aMin)
			.add(1, bMin));
		REQUIRE((solver.assume(a) && solver.propagate()));
		REQUIRE((solver.assume(b) && solver.propagate()));
		REQUIRE((solver.assume(d) && solver.propagate()));
		newMin->commitUpperBound(solver);
		solver.backtrack();
		REQUIRE(newMin->integrateBound(solver));
		REQUIRE((solver.assume(e) && solver.propagate()));
		REQUIRE(solver.isTrue(~f));
		REQUIRE((solver.assume(c) && solver.propagate()));
		solver.backtrack();
		REQUIRE(solver.isTrue(~f));
		LitVec r;
		solver.reason(~f, r);
		REQUIRE(std::find(r.begin(), r.end(), e) == r.end());
	}

	SECTION("testSmartBacktrack") {
		WeightLitVec min;
		min.push_back( WeightLiteral(a, 1) );
		min.push_back( WeightLiteral(b, 1) );
		min.push_back( WeightLiteral(c, 1) );
		newMin = test.buildAndAttach(mb.add(0, min));
		REQUIRE((solver.assume(a) && solver.propagate()));
		REQUIRE((solver.assume(b) && solver.propagate()));
		REQUIRE((solver.assume(~c) && solver.propagate()));

		newMin->commitUpperBound(solver);
		REQUIRE(newMin->integrateBound(solver));

		REQUIRE(solver.isTrue(~b));
		REQUIRE(solver.isTrue(~c));
	}

	SECTION("testBacktrackToTrue") {
		WeightLitVec min1, min2;
		min1.push_back( WeightLiteral(a, 1) );
		min2.push_back( WeightLiteral(b, 1) );
		newMin = test.buildAndAttach(mb
			.add(2, min1)
			.add(1, min2));
		REQUIRE((solver.assume(~a) && solver.propagate()));
		REQUIRE((solver.force(b, 0) && solver.propagate()));

		newMin->commitUpperBound(solver);
		solver.backtrack();
		REQUIRE_FALSE(newMin->integrateBound(solver));
	}

	SECTION("testMultiAssignment") {
		WeightLitVec min1, min2;
		min1.push_back( WeightLiteral(a, 1) );
		min1.push_back( WeightLiteral(b, 1) );
		min1.push_back( WeightLiteral(c, 1) );
		min1.push_back( WeightLiteral(d, 1) );
		min1.push_back( WeightLiteral(e, 1) );

		min2.push_back( WeightLiteral(f, 1) );

		newMin = test.buildAndAttach(mb
			.add(2, min1)
			.add(1, min2));
		SumVec opt(newMin->numRules());
		opt[0] = 3;
		opt[1] = 0;
		REQUIRE(test.setOptimum(solver, opt, false));

		solver.assume(f);
		solver.force(a, 0);
		solver.force(b, 0);
		solver.force(c, 0);

		REQUIRE_FALSE(solver.propagate());
	}

	SECTION("testBugBacktrackFromFalse") {
		WeightLitVec min1, min2;
		min1.push_back( WeightLiteral(a, 1) );
		min1.push_back( WeightLiteral(b, 1) );
		min1.push_back( WeightLiteral(c, 1) );
		min2.push_back( WeightLiteral(d, 1) );
		min2.push_back( WeightLiteral(e, 1) );
		min2.push_back( WeightLiteral(f, 1) );

		newMin = test.buildAndAttach(mb
			.add(2, min1)
			.add(1, min2));
		REQUIRE((solver.assume(a) && solver.propagate()));
		REQUIRE((solver.assume(b) && solver.propagate()));
		REQUIRE((solver.assume(x) && solver.propagate()));
		REQUIRE((solver.force(~c, 0) && solver.propagate()));
		REQUIRE((solver.assume(y) && solver.propagate()));
		REQUIRE((solver.force(d, 0) && solver.propagate()));
		REQUIRE((solver.force(e, 0) && solver.propagate()));
		REQUIRE((solver.force(~f,0) && solver.propagate()));

		newMin->commitUpperBound(solver);
		solver.undoUntil(3);
		solver.setBacktrackLevel(3, Solver::undo_pop_proj_level);
		REQUIRE(newMin->integrateBound(solver));
		REQUIRE((solver.force(f,0) && solver.propagate()));
		REQUIRE(solver.isTrue(~d));
		REQUIRE(solver.isTrue(~e));

		newMin->commitUpperBound(solver);
		solver.undoUntil(2);
		solver.backtrack();
		REQUIRE(newMin->integrateBound(solver));
		REQUIRE(solver.isTrue(~x));
		REQUIRE(solver.isTrue(~f));
		REQUIRE(solver.isTrue(~d));
		REQUIRE(solver.isTrue(~e));
		REQUIRE(solver.isTrue(~c));
	}

	SECTION("testBugBacktrackToTrue") {
		WeightLitVec min1, min2;
		min1.push_back( WeightLiteral(a, 1) );
		min1.push_back( WeightLiteral(b, 1) );
		min1.push_back( WeightLiteral(~b, 2) );
		min2.push_back( WeightLiteral(a, 1) );
		min2.push_back( WeightLiteral(b, 1) );
		min2.push_back( WeightLiteral(c, 1) );

		newMin = test.buildAndAttach(mb
			.add(2, min1)
			.add(1, min2));

		REQUIRE((solver.assume(a) && solver.propagate()));
		REQUIRE((solver.assume(b) && solver.propagate()));
		REQUIRE((solver.assume(c) && solver.propagate()));

		newMin->commitUpperBound(solver);
		REQUIRE((newMin->integrateBound(solver) && 1 == solver.decisionLevel()));
		REQUIRE(solver.propagate());

		newMin->commitUpperBound(solver);
		solver.undoUntil(0);
		REQUIRE(newMin->integrateBound(solver));
		REQUIRE(solver.isTrue(~a));
		REQUIRE(solver.value(b.var()) == value_free);
	}

	SECTION("testBugInitOptHierarch") {
		WeightLitVec aMin, bMin;
		aMin.push_back( WeightLiteral(a, 1) );
		aMin.push_back( WeightLiteral(b, 1) );
		bMin.push_back( WeightLiteral(c, 1) );
		bMin.push_back( WeightLiteral(d, 1) );
		data = mb
			.add(2, aMin)
			.add(1, bMin)
			.build(ctx);
		wsum_t bound[2] = {wsum_t(1),wsum_t(1)};
		data->setMode(MinimizeMode_t::optimize, bound, 2);
		newMin = test.createMin(solver, data, OptParams::bb_hier);
		newMin->integrateBound(solver);
		REQUIRE(ctx.master()->value(a.var()) == value_free);
		ctx.master()->assume(a) && ctx.master()->propagate();
		REQUIRE(ctx.master()->isFalse(b));
		REQUIRE(ctx.master()->value(c.var()) == value_free);
		ctx.master()->assume(c) && ctx.master()->propagate();
		REQUIRE(ctx.master()->isFalse(d));
	}

	SECTION("testBugAdjustSum") {
		WeightLitVec aMin, bMin;
		aMin.push_back( WeightLiteral(a, 1) );
		aMin.push_back( WeightLiteral(b, 1) );
		bMin.push_back( WeightLiteral(~a, 1) );
		bMin.push_back( WeightLiteral(d, 1) );
		data = mb
			.add(2, aMin)
			.add(1, bMin)
			.build(ctx);
		newMin = static_cast<DefaultMinimize*>(data->attach(solver, OptParams()));
		SumVec opt;
		opt.push_back(1);
		opt.push_back(1);
		test.setOptimum(solver, opt, true);
		// a ~b ~d is a valid model!
		REQUIRE(ctx.master()->value(a.var()) == value_free);
		REQUIRE((ctx.master()->assume(a) && ctx.master()->propagate()));
		REQUIRE((ctx.master()->assume(~b) && ctx.master()->propagate()));
		REQUIRE((ctx.master()->assume(~d) && ctx.master()->propagate()));
	}

	SECTION("testWeightNullBug") {
		min.push_back( WeightLiteral(a, 1) );
		min.push_back( WeightLiteral(b, 0) );
		newMin = test.buildAndAttach(mb.add(0, min));
		REQUIRE((solver.assume(a) && solver.propagate()));
		REQUIRE((solver.assume(b) && solver.force(~c,0) && solver.propagate()));
		newMin->commitUpperBound(solver);
		newMin->integrateBound(solver);
		REQUIRE((0u == solver.decisionLevel() && solver.isFalse(a)));
	}

	SECTION("testAdjust") {
		min.push_back( WeightLiteral(a, 1) );
		min.push_back( WeightLiteral(b, 1) );
		newMin = test.buildAndAttach(mb.add(0, min).add(0, -2));
		REQUIRE((solver.assume(a) && solver.propagate()));
		REQUIRE((solver.assume(b) && solver.propagate()));
		newMin->commitUpperBound(solver);
		REQUIRE(0 == newMin->shared()->optimum(0));

		solver.clearAssumptions();
		REQUIRE((solver.assume(~a) && solver.propagate()));
		REQUIRE((solver.assume(~b) && solver.propagate()));
		newMin->commitUpperBound(solver);
		REQUIRE(-2 == newMin->shared()->optimum(0));
	}

	SECTION("testAdjustFact") {
		min.push_back( WeightLiteral(a, 2) );
		min.push_back( WeightLiteral(b, 1) );
		min.push_back( WeightLiteral(c, 1) );
		min.push_back( WeightLiteral(d, 1) );
		data = mb.add(0, min).add(0, -2).build(ctx);
		ctx.addUnary(a);
		solver.propagate();
		newMin = test.createMin(solver, data);
		newMin->integrateBound(solver);
		REQUIRE((solver.assume(b) && solver.propagate()));
		REQUIRE((solver.assume(c) && solver.propagate()));
		REQUIRE((solver.assume(d) && solver.propagate()));
		newMin->commitUpperBound(solver);
		newMin->integrateBound(solver);
		REQUIRE(2 == solver.decisionLevel());
	}

	SECTION("testAssumption") {
		min.push_back( WeightLiteral(a, 1) );
		min.push_back( WeightLiteral(b, 1) );
		min.push_back( WeightLiteral(c, 1) );
		data   = mb.add(0, min).build(ctx);
		newMin = test.createMin(solver, data, OptParams::bb_inc);
		Literal minAssume = posLit(solver.pushTagVar(true));
		SumVec opt(1, 0);
		test.setOptimum(solver, opt, false);
		REQUIRE((solver.isFalse(a) && solver.reason(~a).constraint() == newMin));
		REQUIRE((solver.isFalse(b) && solver.reason(~b).constraint() == newMin));
		REQUIRE((solver.isFalse(c) && solver.reason(~c).constraint() == newMin));

		LitVec r;
		solver.reason(~a, r);
		REQUIRE((r.size() == 1 && r[0] == minAssume));

		solver.clearAssumptions();
		REQUIRE(solver.numAssignedVars() == 0);
	}

	SECTION("testHierarchicalSetModel") {
		min.push_back( WeightLiteral(a, 1) );
		min.push_back( WeightLiteral(b, 1) );
		min.push_back( WeightLiteral(c, 1) );
		mb.add(2, min);
		min.clear();
		min.push_back( WeightLiteral(d, 1) );
		min.push_back( WeightLiteral(e, 1) );
		min.push_back( WeightLiteral(f, 1) );
		mb.add(0, min);
		data   = mb.build(ctx);
		newMin = test.createMin(solver, data, OptParams::bb_hier);
		newMin->integrateBound(solver);
		solver.assume(a); solver.propagate();
		solver.assume(b); solver.propagate();
		solver.assume(c); solver.propagate();
		solver.assume(d); solver.propagate();
		solver.assume(e); solver.propagate();
		solver.assume(f); solver.propagate();
		newMin->commitUpperBound(solver);
		solver.undoUntil(solver.level(b.var()));
		newMin->integrateBound(solver);
		REQUIRE(solver.isFalse(c));
		REQUIRE(solver.numAssignedVars() == 4);
	}

	SECTION("testHierarchical") {
		min.push_back( WeightLiteral(a, 1) );
		mb.add(2, min);
		min.clear();
		min.push_back( WeightLiteral(~b, 1) );
		mb.add(1, min);

		ctx.addBinary(~a, b);
		ctx.addBinary(a, ~b);
		data   = mb.build(ctx);
		newMin = test.createMin(solver, data, OptParams::bb_hier);
		newMin->integrateBound(solver);
		solver.assume(a);
		solver.propagate();
		newMin->commitUpperBound(solver);
		solver.backtrack();
		REQUIRE(newMin->integrateBound(solver));
		REQUIRE(solver.propagate() == true);
		newMin->commitUpperBound(solver);
		REQUIRE(!newMin->integrateBound(solver));
	}
	SECTION("testInconsistent") {
		min.push_back( WeightLiteral(a, 1) );
		min.push_back( WeightLiteral(b, 1) );
		min.push_back( WeightLiteral(c, 1) );
		mb.add(0, min);
		wsum_t bound = 1;
		ctx.addUnary(a);
		ctx.addUnary(b);
		REQUIRE(test.buildAndAttach(mb, MinimizeMode_t::optimize, &bound, 1) == 0);
	}
};

TEST_CASE("Core-guided minimize", "[constraint][asp]") {
	SharedContext ctx;
	SharedMinimizeData* data = 0;
	MinimizeConstraint* min = 0;
	struct Destroy {
		SharedMinimizeData** data;
		MinimizeConstraint** min;
		~Destroy() {
			if (*data) { (*data)->release(); }
			if (*min)  { (*min)->destroy(0, false); }
		}
	} destroy = {&data, &min};
	MinimizeBuilder mb;
	WeightLitVec lits;
	Var a = ctx.addVar(Var_t::Atom);
	Var b = ctx.addVar(Var_t::Atom);
	Var c = ctx.addVar(Var_t::Atom);
	Solver& solver = ctx.startAddConstraints();
	SECTION("testEmpty") {
		data = mb.build(ctx);
		REQUIRE(data == 0);
	}
	SECTION("testEnumerate") {
		lits.push_back(WeightLiteral(posLit(a), 1));
		lits.push_back(WeightLiteral(posLit(b), 1));
		lits.push_back(WeightLiteral(posLit(c), 1));
		mb.add(0, lits);
		data = mb.build(ctx);
		ctx.endInit();
		wsum_t bound = 1;
		data->setMode(MinimizeMode_t::enumerate, &bound, 1);
		min = data->attach(solver, OptParams::type_usc);
		REQUIRE(min->integrate(solver));

		ctx.master()->assume(posLit(a));
		ctx.master()->propagate();
		REQUIRE(solver.isFalse(posLit(b)));
		REQUIRE(solver.isFalse(posLit(c)));
	}
	SECTION("testOptimize") {
		lits.push_back(WeightLiteral(posLit(a), 1));
		lits.push_back(WeightLiteral(posLit(b), 1));
		mb.add(0, lits);
		data = mb.build(ctx);
		ctx.endInit();
		data->setMode(MinimizeMode_t::optimize);
		OptParams p(OptParams::type_usc);
		p.opts |= OptParams::usc_disjoint;
		min = data->attach(solver, p);
		BasicSolve solve(solver);
		LitVec gp;
		while (min->integrate(solver) || min->handleUnsat(solver, true, gp)) {
			ValueRep v = solve.solve();
			REQUIRE(v == value_true);
			min->handleModel(solver);
		}
		REQUIRE(gp.empty());
		REQUIRE((solver.isFalse(lits[0].first) && solver.isFalse(lits[1].first)));
	}
	SECTION("testOptimizeGP") {
		lits.push_back(WeightLiteral(posLit(a), 1));
		lits.push_back(WeightLiteral(posLit(b), 1));
		mb.add(0, lits);
		data = mb.build(ctx);
		ctx.endInit();
		data->setMode(MinimizeMode_t::optimize);
		OptParams p(OptParams::type_usc);
		p.opts |= OptParams::usc_disjoint;
		min = data->attach(solver, p);
		BasicSolve solve(solver);
		LitVec gp; gp.push_back(posLit(a));
		solve.assume(gp);
		gp.clear();
		while (min->integrate(solver) || min->handleUnsat(solver, true, gp)) {
			ValueRep v = solve.solve();
			REQUIRE(v == value_true);
			min->handleModel(solver);
		}
		REQUIRE((solver.rootLevel() == 1 && solver.decision(1) == posLit(a)));
		REQUIRE((solver.isTrue(lits[0].first) && solver.isFalse(lits[1].first)));
	}

	SECTION("testMtBug1") {
		lits.push_back(WeightLiteral(posLit(a), 1));
		lits.push_back(WeightLiteral(posLit(b), 1));
		Solver& s1 = ctx.startAddConstraints();
		data       = MinimizeBuilder().add(0, lits).build(ctx);
		ctx.addUnary(negLit(c));
		ctx.setConcurrency(2, SharedContext::resize_reserve);
		ctx.endInit(true);
		Solver& s2 = *ctx.solver(1);
		data->setMode(MinimizeMode_t::enumOpt);
		OptParams p(OptParams::type_usc);
		p.opts |= OptParams::usc_disjoint;
		MinimizeConstraint* m1 = data->attach(s1, p);
		MinimizeConstraint* m2 = data->attach(s2, p);
		s1.setEnumerationConstraint(m1);
		s2.setEnumerationConstraint(m2);
		BasicSolve solve(s1);
		LitVec gp;
		while (m1->integrate(s1) || m1->handleUnsat(s1, true, gp)) {
			ValueRep v = solve.solve();
			REQUIRE(v == value_true);
			m1->handleModel(s1);
		}
		data->markOptimal();
		m1->relax(s1, true);
		m2->relax(s2, true);
		solve.reset(s2, s2.searchConfig());
		uint32 numModels = 0;
		s2.setPref(1, ValueSet::user_value, value_true);
		for (;;) {
			ValueRep v = m2->integrate(s2) ? solve.solve() : value_false;
			if (v == value_true) {
				++numModels;
				REQUIRE(s2.isFalse(lits[0].first));
				REQUIRE(s2.isFalse(lits[1].first));
				if (!s2.backtrack()) { break; }
			}
			else if (v == value_false) {
				break;
			}
		}
		REQUIRE(numModels == 1);
	}
	SECTION("testNegativeLower") {
		Solver& s = ctx.startAddConstraints();
		ctx.addBinary(posLit(a), posLit(b));
		WeightLitVec aMin, bMin;
		aMin.push_back( WeightLiteral(posLit(a), 1) );
		aMin.push_back( WeightLiteral(posLit(b), 1) );
		bMin.push_back( WeightLiteral(negLit(a), 1) );
		data = MinimizeBuilder()
			.add(2, aMin)
			.add(1, bMin)
			.build(ctx);
		ctx.endInit();
		data->setMode(MinimizeMode_t::optimize);
		OptParams p(OptParams::type_usc);
		p.opts |= OptParams::usc_disjoint;
		min = data->attach(s, p);

		LitVec ignore;
		while (!min->integrate(*ctx.master()) && min->handleUnsat(s, true, ignore)) { ; }
		REQUIRE((s.assume(negLit(a)) && s.propagate()));
		REQUIRE((s.assume(posLit(b)) && s.propagate()));
		REQUIRE(min->handleModel(s));
		s.undoUntil(0);
		while (!min->integrate(*ctx.master()) && min->handleUnsat(s, true, ignore)) { ; }
		REQUIRE((s.assume(posLit(a)) && s.propagate()));
		REQUIRE(s.isFalse(posLit(b)));
		min->handleModel(s);
		s.undoUntil(0);
		while (!min->integrate(*ctx.master()) && min->handleUnsat(s, true, ignore)) { ; }
		REQUIRE(s.hasConflict());
	}
}
} }
