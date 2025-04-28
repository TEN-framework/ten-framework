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
#include <clasp/solver.h>
#include <clasp/clause.h>
#include <clasp/statistics.h>
#include <clasp/weight_constraint.h>
#include "catch.hpp"
namespace Clasp { namespace Test {
using namespace Clasp::mt;
struct TestingConstraint : public Constraint {
	TestingConstraint(bool* del = 0, ConstraintType t = Constraint_t::Static)
		: type_(t), propagates(0), undos(0), sat(false), keepWatch(true), setConflict(false), deleted(del) {}
	Constraint* cloneAttach(Solver&) {
		return new TestingConstraint(0, type_);
	}
	PropResult propagate(Solver&, Literal, uint32&) {
		++propagates;
		return PropResult(!setConflict, keepWatch);
	}
	void undoLevel(Solver&) {
		++undos;
	}
	bool simplify(Solver&, bool) { return sat; }
	void reason(Solver&, Literal, LitVec& out) { out = ante; }
	void destroy(Solver* s, bool b) {
		if (deleted) *deleted = true;
		if (s && b) {
			for (Var v = 1; s->validVar(v); ++v) {
				s->removeWatch(posLit(v), this);
				s->removeWatch(negLit(v), this);
			}
			for (uint32 i = 1; i <= s->decisionLevel(); ++i) {
				s->removeUndoWatch(i, this);
			}
		}
		Constraint::destroy(s, b);
	}
	bool locked(const Solver&) const { return false; }
	uint32 isOpen(const Solver&, const TypeSet&, LitVec&) { return 0; }
	static uint32 size() { return uint32(10); }
	ConstraintType type() const { return type_; }
	LitVec ante;
	ConstraintType type_;
	int propagates;
	int undos;
	bool sat;
	bool keepWatch;
	bool setConflict;
	bool* deleted;
};
struct TestingPostProp : public PostPropagator {
	explicit TestingPostProp(bool cfl, uint32 p = PostPropagator::priority_class_simple) : props(0), resets(0), inits(0), prio(p), conflict(cfl), deleteOnProp(false) {}
	bool propagateFixpoint(Solver& s, PostPropagator*) {
		++props;
		bool ok = !conflict;
		if (deleteOnProp) {
			s.removePost(this);
			this->destroy();
		}
		return ok;
	}
	void reset() {
		++resets;
	}
	bool init(Solver&) { ++inits; return true; }
	uint32 priority() const { return prio; }
	int props;
	int resets;
	int inits;
	uint32 prio;
	bool conflict;
	bool deleteOnProp;
};
template <class ST>
static void testReasonStore() {
	ST store;
	store.resize(1);
	Constraint* x = new TestingConstraint(0, Constraint_t::Conflict);
	store[0] = x;
	store.setData(0, 22);
	REQUIRE(store[0]      == x);
	REQUIRE(store.data(0) == 22);
	Literal p(10, 0), q(22, 0);
	store[0] = Antecedent(p, q);
	uint32 old = store.data(0);
	store.setData(0, 74);
	REQUIRE(store.data(0) == 74);
	store.setData(0, old);
	REQUIRE((store[0].firstLiteral() == p && store[0].secondLiteral() == q));

	typedef typename ST::value_type ReasonWithData;
	ReasonWithData rwd(x, 169);
	store[0] = rwd.ante();
	if (rwd.data() != UINT32_MAX) {
		store.setData(0, rwd.data());
	}
	REQUIRE(store[0] == x);
	REQUIRE(store.data(0) == 169);

	rwd = ReasonWithData(p, UINT32_MAX);
	store[0] = rwd.ante();
	if (rwd.data() != UINT32_MAX) {
		store.setData(0, rwd.data());
	}
	REQUIRE(store[0].firstLiteral() == p);
	x->destroy();
}
static void testDefaults(SharedContext& ctx) {
	const SolverParams& x = ctx.configuration()->solver(0);
	const Solver& s = *ctx.master();
	REQUIRE(ctx.stats().vars.frozen == 0);
	REQUIRE(x.heuId == 0);
	REQUIRE(x.ccMinRec == 0);
	REQUIRE(x.ccMinAntes == SolverStrategies::all_antes);
	REQUIRE(x.search == SolverStrategies::use_learning);
	REQUIRE(x.compress == 0);
	REQUIRE(x.initWatches == SolverStrategies::watch_rand);

	REQUIRE(x.heuristic.score == HeuParams::score_auto);
	REQUIRE(x.heuristic.other == HeuParams::other_auto);
	REQUIRE(x.heuristic.moms == 1);

	REQUIRE(0u ==s.numVars());
	REQUIRE(0u ==s.numAssignedVars());
	REQUIRE(0u ==s.numConstraints());
	REQUIRE(0u ==s.numLearntConstraints());
	REQUIRE(0u ==s.decisionLevel());
	REQUIRE(0u ==s.queueSize());
	ctx.setFrozen(0, true);
	REQUIRE(ctx.stats().vars.frozen == 0);
}
TEST_CASE("Solver types", "[core]") {
	SECTION("test reason store") {
		if (sizeof(void*) == sizeof(uint32)) {
			testReasonStore<ReasonStore32>();
		}
		testReasonStore<ReasonStore64>();
	}
	SECTION("test single owner pointer") {
		bool conDel1 = false, conDel2 = false;
		TestingConstraint* f = new TestingConstraint(&conDel2);
		{
			SingleOwnerPtr<Constraint, DestroyObject> x(new TestingConstraint(&conDel1));
			SingleOwnerPtr<Constraint, DestroyObject> y(f);
			y.release();
		}
		REQUIRE(conDel1);
		REQUIRE_FALSE(conDel2);
		{
			SingleOwnerPtr<Constraint, DestroyObject> y(f);
			y = f;
			REQUIRE(y.is_owner());
			y.release();
			REQUIRE_FALSE(conDel2);
			y = f;
			REQUIRE((!conDel2 && y.is_owner()));
		}
		REQUIRE(conDel2);
	}
	SECTION("test value set") {
		ValueSet vs;
		REQUIRE(vs.empty());
		vs.set(ValueSet::pref_value, value_true);
		REQUIRE_FALSE(vs.empty());
		REQUIRE(vs.has(ValueSet::pref_value));
		REQUIRE_FALSE(vs.sign());
		vs.set(ValueSet::saved_value, value_false);
		REQUIRE(vs.has(ValueSet::saved_value));
		REQUIRE(vs.sign());

		vs.set(ValueSet::user_value, value_true);
		REQUIRE(vs.has(ValueSet::user_value));
		REQUIRE_FALSE(vs.sign());

		vs.set(ValueSet::user_value, value_free);
		REQUIRE_FALSE(vs.has(ValueSet::user_value));
		REQUIRE(vs.sign());
	}
	SECTION("test var true is sentinel") {
		Literal p = lit_true();
		REQUIRE(isSentinel(p));
		REQUIRE(isSentinel(~p));
	}
	SECTION("test compare scores") {
		ReduceStrategy rs;
		ConstraintScore a1 = makeScore(100, 5), a2 = makeScore(90, 3);
		REQUIRE(rs.compare(ReduceStrategy::score_act, a1, a2) > 0);
		REQUIRE(rs.compare(ReduceStrategy::score_lbd, a1, a2) < 0);
		REQUIRE(rs.compare(ReduceStrategy::score_both, a1, a2) > 0);

		REQUIRE((!a1.bumped() && !a2.bumped()));
		a1.bumpLbd(4);
		REQUIRE(rs.compare(ReduceStrategy::score_act, a1, a2) > 0);
		REQUIRE(rs.compare(ReduceStrategy::score_lbd, a1, a2) < 0);
		REQUIRE(rs.compare(ReduceStrategy::score_both, a1, a2) > 0);
		REQUIRE((a1.bumped() && a1.activity() == 100 && a1.lbd() == 4));
		a1.clearBumped();
		REQUIRE((!a1.bumped() && a1.activity() == 100 && a1.lbd() == 4));

		a1.bumpActivity();
		REQUIRE((!a1.bumped() && a1.activity() == 101 && a1.lbd() == 4));
		a1.reset(Clasp::ACT_MAX - 1, 2);
		a1.bumpActivity();
		REQUIRE((!a1.bumped() && a1.activity() == Clasp::ACT_MAX && a1.lbd() == 2));
		a1.bumpActivity();
		REQUIRE((!a1.bumped() && a1.activity() == Clasp::ACT_MAX && a1.lbd() == 2));
	}
}

TEST_CASE("Solver", "[core]") {
	SharedContext ctx;
	Solver& s = *ctx.master();
	SECTION("testDefaults") {
		testDefaults(ctx);
	}

	SECTION("testSolverAlwaysContainsSentinelVar") {
		REQUIRE(value_true == s.value(sentVar));
		REQUIRE(s.isTrue(posLit(sentVar)));
		REQUIRE(s.isFalse(negLit(sentVar)));
		REQUIRE(s.seen(sentVar) == true);
	}
	SECTION("testSolverOwnsConstraints") {
		bool conDel = false;
		bool lconDel = false;
		{
			SharedContext localCtx;
			Solver& localS = localCtx.startAddConstraints();
			localCtx.add(new TestingConstraint(&conDel));
			localCtx.endInit();
			localS.addLearnt(new TestingConstraint(&lconDel, Constraint_t::Conflict), TestingConstraint::size());
			REQUIRE(1u == localS.numConstraints());
			REQUIRE(1u == localS.numLearntConstraints());
		}
		REQUIRE(conDel);
		REQUIRE(lconDel);
	}

	SECTION("testAddVar") {
		Var v1 = ctx.addVar(Var_t::Atom);
		Var v2 = ctx.addVar(Var_t::Body);
		ctx.startAddConstraints();
		ctx.endInit();
		REQUIRE(2u == s.numVars());
		REQUIRE(0u ==s.numAssignedVars());
		REQUIRE(2u == s.numFreeVars());
		REQUIRE(Var_t::Atom == ctx.varInfo(v1).type());
		REQUIRE(Var_t::Body == ctx.varInfo(v2).type());

		REQUIRE(true == ctx.varInfo(v1).preferredSign());
		REQUIRE(false == ctx.varInfo(v2).preferredSign());
	}

	SECTION("testEliminateVar") {
		Var v1 = ctx.addVar(Var_t::Atom);
		ctx.addVar(Var_t::Body);
		ctx.startAddConstraints();
		ctx.eliminate(v1);
		REQUIRE(uint32(2) == s.numVars());
		REQUIRE(uint32(1) == ctx.numEliminatedVars());
		REQUIRE(uint32(1) == s.numFreeVars());
		REQUIRE(uint32(0) == s.numAssignedVars());
		REQUIRE(ctx.eliminated(v1));
		// so v1 is ignored by heuristics!
		REQUIRE(s.value(v1) != value_free);

		// ignore subsequent calls
		ctx.eliminate(v1);
		REQUIRE(uint32(1) == ctx.numEliminatedVars());
		ctx.endInit();
	}

	SECTION("testPreferredLitByType") {
		Var v1 = ctx.addVar(Var_t::Atom);
		Var v2 = ctx.addVar(Var_t::Body);
		Var v3 = ctx.addVar(Var_t::Hybrid);
		Var v4 = ctx.addVar(Var_t::Body, VarInfo::Eq);
		REQUIRE(true == ctx.varInfo(v1).preferredSign());
		REQUIRE(false == ctx.varInfo(v2).preferredSign());
		REQUIRE(true == ctx.varInfo(v3).preferredSign());
		REQUIRE(false == ctx.varInfo(v4).preferredSign());
	}

	SECTION("testInitSavedValue") {
		Var v1 = ctx.addVar(Var_t::Atom);
		Var v2 = ctx.addVar(Var_t::Body);
		ctx.startAddConstraints();
		ctx.endInit();
		REQUIRE(value_free == s.pref(v1).get(ValueSet::saved_value));
		REQUIRE(value_free == s.pref(v2).get(ValueSet::saved_value));

		s.setPref(v1, ValueSet::saved_value, value_true);
		s.setPref(v2, ValueSet::saved_value, value_false);

		REQUIRE(value_true == s.pref(v1).get(ValueSet::saved_value));
		REQUIRE(value_false == s.pref(v2).get(ValueSet::saved_value));
	}

	SECTION("testReset") {
		ctx.addVar(Var_t::Atom); ctx.addVar(Var_t::Body);
		ctx.startAddConstraints();
		s.add(new TestingConstraint(0));
		ctx.endInit();
		s.addLearnt(new TestingConstraint(0, Constraint_t::Conflict), TestingConstraint::size());
		s.assume(posLit(1));
		ctx.reset();
		testDefaults(ctx);
		Var n = ctx.addVar(Var_t::Body);
		ctx.startAddConstraints();
		ctx.endInit();
		REQUIRE(Var_t::Body == ctx.varInfo(n).type());
	}

	SECTION("testForce") {
		Var v1 = ctx.addVar(Var_t::Atom);
		Var v2 = ctx.addVar(Var_t::Atom);
		ctx.startAddConstraints();
		ctx.endInit();
		REQUIRE(s.force(posLit(v1), 0));
		REQUIRE(s.force(negLit(v2), posLit(v1)));
		REQUIRE(s.isTrue(posLit(v1)));
		REQUIRE(s.isTrue(negLit(v2)));
		REQUIRE(s.reason(negLit(v2)).type() == Antecedent::Binary);

		REQUIRE(2u == s.queueSize());
	}

	SECTION("testNoUpdateOnConsistentAssign") {
		Var v1 = ctx.addVar(Var_t::Atom);
		Var v2 = ctx.addVar(Var_t::Atom);
		ctx.startAddConstraints();
		s.force(posLit(v2), 0);
		s.force(posLit(v1), 0);
		uint32 oldA = s.numAssignedVars();
		REQUIRE(s.force(posLit(v1), posLit(v2)));
		REQUIRE(oldA == s.numAssignedVars());
		REQUIRE(2u == s.queueSize());
	}

	SECTION("testAssume") {
		Literal p = posLit(ctx.addVar(Var_t::Atom));
		ctx.startAddConstraints();
		REQUIRE(s.assume(p));
		REQUIRE(value_true == s.value(p.var()));
		REQUIRE(1u == s.decisionLevel());
		REQUIRE(1u == s.queueSize());
	}

	SECTION("testGetDecision") {
		Literal p = posLit(ctx.addVar(Var_t::Atom));
		Literal q = posLit(ctx.addVar(Var_t::Atom));
		Literal r = posLit(ctx.addVar(Var_t::Atom));
		ctx.startAddConstraints();
		s.assume(p);
		s.assume(q);
		s.assume(~r);
		REQUIRE(p == s.decision(1));
		REQUIRE(q == s.decision(2));
		REQUIRE(~r == s.decision(3));
		REQUIRE(~r == s.decision(s.decisionLevel()));
	}
	SECTION("testAddWatch") {
		Literal p = posLit(ctx.addVar(Var_t::Atom));
		ctx.startAddConstraints();
		TestingConstraint c;
		REQUIRE_FALSE(s.hasWatch(p, &c));
		s.addWatch(p, &c);
		REQUIRE(s.hasWatch(p, &c));
		REQUIRE(1u == s.numWatches(p));
	}

	SECTION("testRemoveWatch") {
		Literal p = posLit(ctx.addVar(Var_t::Atom));
		ctx.startAddConstraints();
		TestingConstraint c;
		s.addWatch(p, &c);
		s.removeWatch(p, &c);
		REQUIRE_FALSE(s.hasWatch(p, &c));
	}

	SECTION("testNotifyWatch") {
		Literal p = posLit(ctx.addVar(Var_t::Atom)), q = posLit(ctx.addVar(Var_t::Atom));
		TestingConstraint c;
		ctx.startAddConstraints();
		ctx.endInit();
		s.addWatch(p, &c);
		s.addWatch(q, &c);
		s.assume(p);
		s.propagate();
		REQUIRE(1 == c.propagates);
		s.assume(~q);
		s.propagate();
		REQUIRE(1 == c.propagates);
	}

	SECTION("testKeepWatchOnPropagate") {
		Literal p = posLit(ctx.addVar(Var_t::Atom));
		ctx.startAddConstraints();
		ctx.endInit();
		TestingConstraint c;
		s.addWatch(p, &c);
		s.assume(p);
		s.propagate();
		REQUIRE(s.hasWatch(p, &c));
	}

	SECTION("testRemoveWatchOnPropagate") {
		Literal p = posLit(ctx.addVar(Var_t::Atom));
		ctx.startAddConstraints();
		ctx.endInit();
		TestingConstraint c;
		c.keepWatch = false;
		s.addWatch(p, &c);
		s.assume(p);
		s.propagate();
		REQUIRE_FALSE(s.hasWatch(p, &c));
	}

	SECTION("testWatchOrder") {
		Literal p = posLit(ctx.addVar(Var_t::Atom));
		ctx.startAddConstraints();
		ctx.endInit();
		TestingConstraint c1, c2, c3;
		c1.keepWatch = false;
		c2.setConflict = true;
		s.addWatch(p, &c1);
		s.addWatch(p, &c2);
		s.addWatch(p, &c3);
		s.assume(p);
		REQUIRE_FALSE(s.propagate());
		REQUIRE_FALSE(s.hasWatch(p, &c1));
		REQUIRE(s.hasWatch(p, &c2));
		REQUIRE(s.hasWatch(p, &c3));
		REQUIRE(1 == c1.propagates);
		REQUIRE(1 == c2.propagates);
		REQUIRE(0 == c3.propagates);
	}

	SECTION("testUndoUntil") {
		Literal a = posLit(ctx.addVar(Var_t::Atom)), b = posLit(ctx.addVar(Var_t::Atom))
			, c = posLit(ctx.addVar(Var_t::Atom)), d = posLit(ctx.addVar(Var_t::Atom));
		ctx.startAddConstraints();
		s.assume(a);
		s.force(~b, a);
		s.force(~c, a);
		s.force(d, a);
		REQUIRE(4u == s.queueSize());
		REQUIRE(4u == s.numAssignedVars());
		s.undoUntil(0u);
		REQUIRE(0u ==s.numAssignedVars());
		for (Var i = a.var(); i != d.var()+1; ++i) {
			REQUIRE(value_free == s.value(i));
		}
	}

	SECTION("testUndoWatches") {
		Literal a = posLit(ctx.addVar(Var_t::Atom)), b = posLit(ctx.addVar(Var_t::Atom));
		TestingConstraint c;
		ctx.startAddConstraints();
		ctx.endInit();
		s.assume(a);
		s.addUndoWatch(1, &c);
		s.assume(b);
		s.undoUntil(1);
		REQUIRE(0 == c.undos);
		s.undoUntil(0);
		REQUIRE(1 == c.undos);
	}
	SECTION("testLazyRemoveWatches") {
		Literal a = posLit(ctx.addVar(Var_t::Atom));
		ctx.startAddConstraints();
		uint32  x = s.numWatches(a);
		Solver::ConstraintDB db;
		for (uint32 i = 0; i != 10; ++i) {
			db.push_back(new TestingConstraint);
			s.addWatch(a, db[i]);
		}
		ctx.endInit();
		s.assume(a);
		for (uint32 i = 0; i != 10; ++i) {
			s.addUndoWatch(1, db[i]);
		}
		s.destroyDB(db);
		s.undoUntil(0);
		REQUIRE(s.numWatches(a) == x);
	}

	SECTION("with binary clause") {
		LitVec bin;
		bin.push_back(posLit(ctx.addVar(Var_t::Atom)));
		bin.push_back(posLit(ctx.addVar(Var_t::Atom)));
		ctx.startAddConstraints();
		ctx.addBinary(bin[0], bin[1]);
		ctx.endInit();
		SECTION("test propagate") {
			for (int i = 0; i < 2; ++i) {
				s.assume(~bin[i]);
				REQUIRE(s.propagate());
				int o = (i+1)%2;
				REQUIRE(s.isTrue(bin[o]));
				REQUIRE(Antecedent::Binary == s.reason(bin[o]).type());
				LitVec r;
				s.reason(bin[o], r);
				REQUIRE(1u == (uint32)r.size());
				REQUIRE(~bin[i] == r[0]);
				s.undoUntil(0);
			}
			s.assume(~bin[0]);
			s.force(~bin[1], 0);
			REQUIRE_FALSE(s.propagate());
			const LitVec& r = s.conflict();
			REQUIRE(2u == (uint32)r.size());
			REQUIRE(std::find(r.begin(), r.end(), ~bin[0]) != r.end());
			REQUIRE(std::find(r.begin(), r.end(), ~bin[1]) != r.end());
		}
		SECTION("testRestartAfterUnitLitResolvedBug") {
			s.force(~bin[0], 0);
			s.undoUntil(0);
			s.propagate();
			REQUIRE(s.isTrue(~bin[0]));
			REQUIRE(s.isTrue(bin[1]));
		}
	}

	SECTION("testPropTernary") {
		LitVec tern;
		tern.push_back(posLit(ctx.addVar(Var_t::Atom)));
		tern.push_back(posLit(ctx.addVar(Var_t::Atom)));
		tern.push_back(posLit(ctx.addVar(Var_t::Atom)));
		ctx.startAddConstraints();
		ctx.addTernary(tern[0], tern[1], tern[2]);
		ctx.endInit();
		for (int i = 0; i < 3; ++i) {
			s.assume(~tern[i]);
			s.assume(~tern[(i+1)%3]);
			REQUIRE(s.propagate());
			int o = (i+2)%3;
			REQUIRE(s.isTrue(tern[o]));
			REQUIRE(Antecedent::Ternary == s.reason(tern[o]).type());
			LitVec r;
			s.reason(tern[o], r);
			REQUIRE(2u == (uint32)r.size());
			REQUIRE(std::find(r.begin(), r.end(), ~tern[i]) != r.end());
			REQUIRE(std::find(r.begin(), r.end(), ~tern[(i+1)%3]) != r.end());
			s.undoUntil(0);
		}
		s.assume(~tern[0]);
		s.force(~tern[1], 0);
		s.force(~tern[2], 0);
		REQUIRE_FALSE(s.propagate());
		const LitVec& r = s.conflict();
		REQUIRE(3u == (uint32)r.size());
		REQUIRE(std::find(r.begin(), r.end(), ~tern[0]) != r.end());
		REQUIRE(std::find(r.begin(), r.end(), ~tern[1]) != r.end());
		REQUIRE(std::find(r.begin(), r.end(), ~tern[2]) != r.end());
	}

	SECTION("testEstimateBCP") {
		Literal a = posLit(ctx.addVar(Var_t::Atom));
		Literal b = posLit(ctx.addVar(Var_t::Atom));
		Literal c = posLit(ctx.addVar(Var_t::Atom));
		Literal d = posLit(ctx.addVar(Var_t::Atom));
		Literal e = posLit(ctx.addVar(Var_t::Atom));
		ctx.startAddConstraints();
		ctx.addBinary(a, b);
		ctx.addBinary(~b, c);
		ctx.addBinary(~c, d);
		ctx.addBinary(~d, e);
		ctx.endInit();
		for (int i = 0; i < 4; ++i) {
			uint32 est = s.estimateBCP(~a, i);
			REQUIRE(uint32(i + 2) == est);
		}
	}

	SECTION("testEstimateBCPLoop") {
		Literal a = posLit(ctx.addVar(Var_t::Atom));
		Literal b = posLit(ctx.addVar(Var_t::Atom));
		Literal c = posLit(ctx.addVar(Var_t::Atom));
		ctx.startAddConstraints();
		ctx.addBinary(a, b);
		ctx.addBinary(~b, c);
		ctx.addBinary(~c, ~a);
		ctx.endInit();
		REQUIRE(uint32(3) == s.estimateBCP(~a, -1));
	}

	SECTION("testAssertImmediate") {
		Literal a = posLit(ctx.addVar(Var_t::Atom));
		Literal b = posLit(ctx.addVar(Var_t::Atom));
		Literal d = posLit(ctx.addVar(Var_t::Atom));
		Literal q = posLit(ctx.addVar(Var_t::Atom));
		Literal f = posLit(ctx.addVar(Var_t::Atom));
		Literal x = posLit(ctx.addVar(Var_t::Atom));
		Literal z = posLit(ctx.addVar(Var_t::Atom));
		ctx.startAddConstraints();

		ClauseCreator cl(&s);
		cl.addDefaultFlags(ClauseCreator::clause_watch_first);
		cl.start().add(~z).add(d).end();
		cl.start().add(a).add(b).end();
		cl.start().add(a).add(~b).add(z).end();
		cl.start().add(a).add(~b).add(~z).add(d).end();
		cl.start().add(~b).add(~z).add(~d).add(q).end();
		cl.start().add(~q).add(f).end();
		cl.start().add(~f).add(~z).add(x).end();
		s.assume(~a);
		REQUIRE(true == s.propagate());

		REQUIRE(7u == s.numAssignedVars());

		Antecedent whyB = s.reason(b);
		Antecedent whyZ = s.reason(z);
		Antecedent whyD = s.reason(d);
		Antecedent whyQ = s.reason(q);
		Antecedent whyF = s.reason(f);
		Antecedent whyX = s.reason(x);

		REQUIRE((whyB.type() == Antecedent::Binary && whyB.firstLiteral() == ~a));
		REQUIRE((whyZ.type() == Antecedent::Ternary && whyZ.firstLiteral() == ~a && whyZ.secondLiteral() == b));
		REQUIRE(whyD.type() == Antecedent::Generic);
		REQUIRE(whyQ.type() == Antecedent::Generic);

		REQUIRE((whyF.type() == Antecedent::Binary && whyF.firstLiteral() == q));
		REQUIRE((whyX.type() == Antecedent::Ternary && whyX.firstLiteral() == f && whyX.secondLiteral() == z));
	}

	SECTION("testPreferShortBfs") {
		Literal a = posLit(ctx.addVar(Var_t::Atom));
		Literal b = posLit(ctx.addVar(Var_t::Atom));
		Literal p = posLit(ctx.addVar(Var_t::Atom));
		Literal q = posLit(ctx.addVar(Var_t::Atom));
		Literal x = posLit(ctx.addVar(Var_t::Atom));
		Literal y = posLit(ctx.addVar(Var_t::Atom));
		Literal z = posLit(ctx.addVar(Var_t::Atom));
		ctx.startAddConstraints();
		ClauseCreator cl(&s);
		cl.addDefaultFlags(ClauseCreator::clause_watch_least);
		cl.start().add(a).add(x).add(y).add(p).end();   // c1
		cl.start().add(a).add(x).add(y).add(z).end();   // c2
		cl.start().add(a).add(p).end();                 // c3
		cl.start().add(a).add(~p).add(z).end();         // c4
		cl.start().add(~z).add(b).end();                // c5
		cl.start().add(a).add(x).add(q).add(~b).end();  // c6
		cl.start().add(a).add(~b).add(~p).add(~q).end();// c7

		REQUIRE(7u == s.numConstraints());
		REQUIRE(2u == ctx.numBinary());
		REQUIRE(1u == ctx.numTernary());

		s.assume(~x);
		s.propagate();
		s.assume(~y);
		s.propagate();
		REQUIRE(2u == s.numAssignedVars());
		s.assume(~a);

		REQUIRE_FALSE(s.propagate());

		REQUIRE(7u == s.numAssignedVars());

		REQUIRE(s.reason(b).type() == Antecedent::Binary);
		REQUIRE(s.reason(p).type() == Antecedent::Binary);
		REQUIRE(s.reason(z).type() == Antecedent::Ternary);
		REQUIRE(s.reason(q).type() == Antecedent::Generic);
	}
	SECTION("testPostPropInit") {
		TestingPostProp* p = new TestingPostProp(false);
		ctx.startAddConstraints();
		s.addPost(p);
		REQUIRE(0 == p->inits); // init is called *after* setup
		ctx.endInit();
		REQUIRE(1 == p->inits);
		ctx.startAddConstraints();
		ctx.endInit();
		REQUIRE(2 == p->inits);
	}
	SECTION("testPropagateCallsPostProp") {
		TestingPostProp* p = new TestingPostProp(false);
		ctx.startAddConstraints();
		s.addPost(p);
		s.propagate();
		REQUIRE(0 == p->props); // not yet enabled
		ctx.endInit();
		REQUIRE(1 == p->props);
		REQUIRE(0 == p->resets);
	}
	SECTION("testPropagateCallsResetOnConflict") {
		TestingPostProp* p = new TestingPostProp(true);
		ctx.startAddConstraints();
		s.addPost(p);
		ctx.endInit();
		REQUIRE(1 == p->props);
		REQUIRE(1 == p->resets);
	}

	SECTION("testPostpropPriority") {
		TestingPostProp* p1 = new TestingPostProp(false);
		p1->prio += 10;
		TestingPostProp* p2 = new TestingPostProp(false);
		p2->prio += 30;
		TestingPostProp* p3 = new TestingPostProp(false);
		p3->prio += 20;
		ctx.startAddConstraints();
		s.addPost(p2);
		s.addPost(p1);
		s.addPost(p3);
		REQUIRE((p1->next == p3 && p3->next == p2));
	}

	SECTION("testPostpropPriorityExt") {
		TestingPostProp* p1 = new TestingPostProp(false);
		TestingPostProp* p2 = new TestingPostProp(false);
		TestingPostProp* p3 = new TestingPostProp(false);
		TestingPostProp* p4 = new TestingPostProp(false);
		p1->prio = 10;
		p2->prio = 20;
		p3->prio = PostPropagator::priority_class_general;
		p4->prio = PostPropagator::priority_class_general;
		ctx.startAddConstraints();
		s.addPost(p3);
		s.addPost(p2);
		REQUIRE(s.getPost(PostPropagator::priority_class_general));
		REQUIRE(s.getPost(20));
		REQUIRE(p2->next == p3);
		s.addPost(p4);
		REQUIRE(p2->next == p3);
		REQUIRE(p3->next == p4);
		ctx.endInit();
		REQUIRE(p3->inits == 1);
		p3->props = 0;
		p2->props = 0;
		p4->props = 0;
		s.removePost(p2);
		s.removePost(p3);
		s.addPost(p3);
		s.propagate();
		REQUIRE((p3->props == 1 && p4->props == 1));
		s.addPost(p1);
		REQUIRE(p1->next == p4);
		s.addPost(p2);
		REQUIRE((p1->next == p2 && p2->next == p4));
		s.removePost(p3);
		s.removePost(p4);
		s.propagate();
		REQUIRE((p3->props == 1 && p4->props == 1 && p1->props == 1 && p2->props == 1));
		s.addPost(p3);
		s.addPost(p4);
		p3->conflict = true;
		s.propagate();
		REQUIRE((p3->props == 2 && p1->props == 2 && p2->props == 2 && p4->props == 1));
	}

	SECTION("testPostpropRemove") {
		SingleOwnerPtr<TestingPostProp> p1(new TestingPostProp(false, 1));
		SingleOwnerPtr<TestingPostProp> p2(new TestingPostProp(false, 2));
		SingleOwnerPtr<TestingPostProp> p3(new TestingPostProp(false, 3));
		ctx.startAddConstraints();
		s.addPost(p1.release());
		s.addPost(p2.release());
		s.addPost(p3.release());
		REQUIRE((p1->next == p2.get() && p2->next == p3.get()));
		s.removePost(p2.acquire());
		REQUIRE((p1->next == p3.get() && p3->next == 0 && p2->next == 0));
		s.removePost(p2.acquire());
		s.removePost(p3.acquire());
		REQUIRE(p1->next == 0);
		ctx.endInit();
		REQUIRE(p1->props == 1);
	}

	SECTION("testPostpropRemoveOnProp") {
		TestingPostProp* p1 = new TestingPostProp(false);
		TestingPostProp* p2 = new TestingPostProp(false);
		TestingPostProp* p3 = new TestingPostProp(false);
		ctx.startAddConstraints();
		s.addPost(p1);
		s.addPost(p2);
		s.addPost(p3);
		ctx.endInit();
		p2->deleteOnProp = true;
		s.propagate();
		REQUIRE((p1->props == 2 && p3->props == 2));
	}

	SECTION("testPostpropBug") {
		ctx.startAddConstraints();
		SingleOwnerPtr<TestingPostProp> p1(new TestingPostProp(false));
		s.addPost(p1.release());
		ctx.endInit();
		// later
		ctx.startAddConstraints();
		s.removePost(p1.get());
		p1.acquire();
		ctx.endInit();
		s.removePost(p1.get());
		REQUIRE(p1->inits == 1);
	}

	SECTION("testPostpropAddAfterInitBug") {
		ctx.startAddConstraints();
		SingleOwnerPtr<TestingPostProp> p1(new TestingPostProp(false));
		ctx.endInit();
		s.addPost(p1.release());
		REQUIRE(p1->inits == 1);
		s.propagate();
		REQUIRE(p1->props == 1);
	}

	SECTION("testSimplifyRemovesSatBinClauses") {
		Literal a = posLit(ctx.addVar(Var_t::Atom));
		Literal b = posLit(ctx.addVar(Var_t::Atom));
		Literal c = posLit(ctx.addVar(Var_t::Atom));
		Literal d = posLit(ctx.addVar(Var_t::Atom));
		ctx.startAddConstraints();
		ctx.addBinary(a, b);
		ctx.addBinary(a, c);
		ctx.addBinary(~a, d);
		s.force(a, 0);
		s.simplify();
		REQUIRE(0u ==ctx.numBinary());
	}

	SECTION("testSimplifyRemovesSatTernClauses") {
		Literal a = posLit(ctx.addVar(Var_t::Atom));
		Literal b = posLit(ctx.addVar(Var_t::Atom));
		Literal c = posLit(ctx.addVar(Var_t::Atom));
		Literal d = posLit(ctx.addVar(Var_t::Atom));
		ctx.startAddConstraints();
		ctx.addTernary(a, b, d);
		ctx.addTernary(~a, b, c);
		s.force(a, 0);
		s.simplify();
		s.assume(~b);
		REQUIRE(0u ==ctx.numTernary());

		// simplify transformed the tern-clause ~a b c to the bin clause b c
		// because ~a is false on level 0
		REQUIRE(1u == ctx.numBinary());
		s.propagate();
		REQUIRE(s.isTrue(c));
	}

	SECTION("testSimplifyRemovesSatConstraints") {
		Literal a = posLit(ctx.addVar(Var_t::Atom));
		ctx.startAddConstraints();
		TestingConstraint* t1;
		TestingConstraint* t2;
		TestingConstraint* t3;
		TestingConstraint* t4;
		bool t2Del = false, t3Del = false;
		s.add(t1 = new TestingConstraint);
		s.add(t2 = new TestingConstraint(&t2Del));
		ctx.endInit();
		s.addLearnt(t3 = new TestingConstraint(&t3Del, Constraint_t::Conflict), TestingConstraint::size());
		s.addLearnt(t4 = new TestingConstraint(0, Constraint_t::Conflict), TestingConstraint::size());
		t1->sat = false;
		t2->sat = true;
		t3->sat = true;
		t4->sat = false;
		REQUIRE(2u == s.numLearntConstraints());
		REQUIRE(2u == s.numLearntConstraints());
		s.force(a, 0);
		s.simplify();
		REQUIRE(1u == s.numLearntConstraints());
		REQUIRE(1u == s.numLearntConstraints());
		REQUIRE(t2Del);
		REQUIRE(t3Del);
	}

	SECTION("testRemoveConditional") {
		Var a = ctx.addVar(Var_t::Atom);
		Var b = ctx.addVar(Var_t::Atom);
		Var c = ctx.addVar(Var_t::Atom);
		ctx.startAddConstraints();
		ctx.endInit();
		Literal tag = posLit(s.pushTagVar(false));
		ClauseCreator cc(&s);
		cc.start(Constraint_t::Conflict).add(posLit(a)).add(posLit(b)).add(posLit(c)).add(~tag).end();
		REQUIRE(s.numLearntConstraints() == 1);
		s.removeConditional();
		REQUIRE(s.numLearntConstraints() == 0);
	}

	SECTION("testStrengthenConditional") {
		Var a = ctx.addVar(Var_t::Atom);
		Var b = ctx.addVar(Var_t::Atom);
		Var c = ctx.addVar(Var_t::Atom);
		ctx.startAddConstraints();
		ctx.endInit();
		ClauseCreator cc(&s);
		Literal tag = posLit(s.pushTagVar(false));
		cc.start(Constraint_t::Conflict).add(posLit(a)).add(posLit(b)).add(posLit(c)).add(~tag).end();
		REQUIRE(s.numLearntConstraints() == 1);
		s.strengthenConditional();
		REQUIRE((ctx.numLearntShort() == 1 || ctx.numTernary() == 1));
	}

	SECTION("testLearnConditional") {
		Var b = ctx.addVar(Var_t::Atom);
		ctx.startAddConstraints();
		ctx.endInit();
		Literal tag = posLit(s.pushTagVar(true));
		s.assume(posLit(b));
		s.propagate();
		TestingConstraint* cfl = new TestingConstraint;
		cfl->ante.push_back(tag);
		cfl->ante.push_back(posLit(b));
		s.force(lit_false(), cfl);
		cfl->destroy(&s, true);
		s.resolveConflict();
		REQUIRE((ctx.numLearntShort() == 0 && ctx.numBinary() == 0));
		REQUIRE(((s.numLearntConstraints() == 1 && s.decisionLevel() == 1)));
		s.strengthenConditional();
		s.clearAssumptions();
		REQUIRE(s.isTrue(negLit(b)));
	}

	SECTION("testResolveUnary") {
		ctx.enableStats(1);
		Var a = ctx.addVar(Var_t::Atom);
		Var b = ctx.addVar(Var_t::Atom);
		Var c = ctx.addVar(Var_t::Atom);
		ctx.startAddConstraints();
		ctx.addBinary(posLit(a), posLit(b));
		ctx.addBinary(negLit(b), posLit(c));
		ctx.addBinary(negLit(a), posLit(c));
		s.assume(negLit(c));
		REQUIRE_FALSE(s.propagate());
		REQUIRE(s.resolveConflict());
		REQUIRE_FALSE(s.hasConflict());
		REQUIRE(s.isTrue(posLit(c)));
		REQUIRE(0u ==s.decisionLevel());
		REQUIRE(s.stats.extra->learnts[Constraint_t::Conflict-1] == 1);
	}

	SECTION("testResolveConflict") {
		Literal x1 = posLit(ctx.addVar(Var_t::Atom)); Literal x2 = posLit(ctx.addVar(Var_t::Atom));
		Literal x3 = posLit(ctx.addVar(Var_t::Atom)); Literal x4 = posLit(ctx.addVar(Var_t::Atom));
		Literal x5 = posLit(ctx.addVar(Var_t::Atom)); Literal x6 = posLit(ctx.addVar(Var_t::Atom));
		Literal x7 = posLit(ctx.addVar(Var_t::Atom)); Literal x8 = posLit(ctx.addVar(Var_t::Atom));
		Literal x9 = posLit(ctx.addVar(Var_t::Atom)); Literal x10 = posLit(ctx.addVar(Var_t::Atom));
		Literal x11 = posLit(ctx.addVar(Var_t::Atom)); Literal x12 = posLit(ctx.addVar(Var_t::Atom));
		Literal x13 = posLit(ctx.addVar(Var_t::Atom)); Literal x14 = posLit(ctx.addVar(Var_t::Atom));
		Literal x15 = posLit(ctx.addVar(Var_t::Atom)); Literal x16 = posLit(ctx.addVar(Var_t::Atom));
		Literal x17 = posLit(ctx.addVar(Var_t::Atom));
		ctx.startAddConstraints();
		ClauseCreator cl(&s);
		cl.start().add(~x11).add(x12).end();
		cl.start().add(x1).add(~x12).add(~x13).end();
		cl.start().add(~x4).add(~x12).add(x14).end();
		cl.start().add(x13).add(~x14).add(~x15).end();
		cl.start().add(~x2).add(x15).add(x16).end();
		cl.start().add(x3).add(x15).add(~x17).end();
		cl.start().add(~x6).add(~x16).add(x17).end();
		cl.start().add(~x2).add(x9).add(x10).end();
		cl.start().add(~x4).add(~x7).add(~x8).end();
		cl.start().add(x5).add(x6).end();
		REQUIRE(ctx.endInit());
		REQUIRE(0u ==s.queueSize());

		REQUIRE((s.assume(~x1) && s.propagate()));
		REQUIRE((s.assume(x2) && s.propagate()));
		REQUIRE((s.assume(~x3) && s.propagate()));
		REQUIRE((s.assume(x4) && s.propagate()));
		REQUIRE((s.assume(~x5) && s.propagate()));
		REQUIRE((s.assume(x7) && s.propagate()));
		REQUIRE((s.assume(~x9) && s.propagate()));

		REQUIRE_FALSE((s.assume(x11) && s.propagate()));

		REQUIRE(s.resolveConflict());
		REQUIRE(s.trail().back() == x15); // UIP
		REQUIRE(5u == s.decisionLevel());
		REQUIRE(Antecedent::Generic == s.reason(s.trail().back()).type());

		LitVec cflClause;
		s.reason(s.trail().back(), cflClause);
		cflClause.push_back(s.trail().back());
		REQUIRE(LitVec::size_type(4) == cflClause.size());
		REQUIRE(std::find(cflClause.begin(), cflClause.end(), x2) != cflClause.end());
		REQUIRE(std::find(cflClause.begin(), cflClause.end(), ~x3) != cflClause.end());
		REQUIRE(std::find(cflClause.begin(), cflClause.end(), x6) != cflClause.end());
		REQUIRE(std::find(cflClause.begin(), cflClause.end(), x15) != cflClause.end());
	}

	SECTION("testResolveConflictBounded") {
		Literal x1 = posLit(ctx.addVar(Var_t::Atom)); Literal x2 = posLit(ctx.addVar(Var_t::Atom));
		Literal x3 = posLit(ctx.addVar(Var_t::Atom)); Literal x4 = posLit(ctx.addVar(Var_t::Atom));
		Literal x5 = posLit(ctx.addVar(Var_t::Atom)); Literal x6 = posLit(ctx.addVar(Var_t::Atom));
		Literal x7 = posLit(ctx.addVar(Var_t::Atom)); Literal x8 = posLit(ctx.addVar(Var_t::Atom));
		Literal x9 = posLit(ctx.addVar(Var_t::Atom)); Literal x10 = posLit(ctx.addVar(Var_t::Atom));
		Literal x11 = posLit(ctx.addVar(Var_t::Atom)); Literal x12 = posLit(ctx.addVar(Var_t::Atom));
		Literal x13 = posLit(ctx.addVar(Var_t::Atom)); Literal x14 = posLit(ctx.addVar(Var_t::Atom));
		Literal x15 = posLit(ctx.addVar(Var_t::Atom)); Literal x16 = posLit(ctx.addVar(Var_t::Atom));
		Literal x17 = posLit(ctx.addVar(Var_t::Atom)); Literal x18 = posLit(ctx.addVar(Var_t::Atom));
		ctx.startAddConstraints();
		ClauseCreator cl(&s);
		cl.start().add(~x11).add(x12).end();
		cl.start().add(x1).add(~x12).add(~x13).end();
		cl.start().add(~x4).add(~x12).add(x14).end();
		cl.start().add(x13).add(~x14).add(~x15).end();
		cl.start().add(~x2).add(x15).add(x16).end();
		cl.start().add(x3).add(x15).add(~x17).end();
		cl.start().add(~x6).add(~x16).add(x17).end();
		cl.start().add(~x2).add(x9).add(x10).end();
		cl.start().add(~x4).add(~x7).add(~x8).end();
		cl.start().add(x5).add(x6).end();
		REQUIRE(ctx.endInit());
		REQUIRE(0u ==s.queueSize());

		REQUIRE((s.assume(~x1) && s.propagate()));
		REQUIRE((s.assume(x2) && s.propagate()));
		REQUIRE((s.assume(~x3) && s.propagate()));
		REQUIRE((s.assume(x4) && s.propagate()));
		REQUIRE((s.assume(~x5) && s.propagate()));
		REQUIRE((s.assume(x7) && s.propagate()));

		// force backtrack-level to 6
		REQUIRE((s.assume(x18) && s.propagate()));
		REQUIRE(s.backtrack());

		REQUIRE((s.assume(~x9) && s.propagate()));
		REQUIRE_FALSE((s.assume(x11) && s.propagate()));

		REQUIRE(s.resolveConflict());
		REQUIRE(s.trail().back() == x15); // UIP
		REQUIRE(6u == s.decisionLevel());  // Jump was bounded!
		Antecedent ante = s.reason(s.trail().back());
		REQUIRE(Antecedent::Generic == ante.type());
		ClauseHead* cflClause = (ClauseHead*)ante.constraint();
		LitVec r;
		cflClause->reason(s, s.trail().back(), r);
		REQUIRE(std::find(r.begin(), r.end(), x2) != r.end());
		REQUIRE(std::find(r.begin(), r.end(), ~x3) != r.end());
		REQUIRE(std::find(r.begin(), r.end(), x6) != r.end());

		REQUIRE(s.hasWatch(x6, cflClause));

		REQUIRE(s.backtrack());
		REQUIRE(s.isTrue(x15));  // still true, because logically implied on level 5
		REQUIRE(s.backtrack());
		REQUIRE(value_free == s.value(x15.var()));
	}

	SECTION("testSearchKeepsAssumptions") {
		Var a = ctx.addVar(Var_t::Atom);
		Var b = ctx.addVar(Var_t::Atom);
		Var c = ctx.addVar(Var_t::Atom);
		Var d = ctx.addVar(Var_t::Atom);
		ctx.startAddConstraints();
		ClauseCreator cl(&s);
		ctx.addBinary(posLit(a), posLit(b));
		ctx.addBinary(negLit(b), posLit(c));
		ctx.addBinary(negLit(a), posLit(c));
		ctx.addBinary(negLit(c), negLit(d));
		ctx.endInit();
		s.simplify();
		s.assume(posLit(d));
		s.pushRootLevel();
		REQUIRE(value_false == s.search(-1, -1, 0));
		REQUIRE(1u == s.decisionLevel());
	}
	SECTION("testSearchAddsLearntFacts") {
		Var a = ctx.addVar(Var_t::Atom);
		Var b = ctx.addVar(Var_t::Atom);
		Var c = ctx.addVar(Var_t::Atom);
		Var d = ctx.addVar(Var_t::Atom);
		struct Dummy : public DecisionHeuristic {
			Dummy(Literal first, Literal second) { lit_[0] = first; lit_[1] = second; }
			void updateVar(const Solver&, Var, uint32) {}
			Literal doSelect(Solver& s) {
				for (uint32 i = 0; i < 2; ++i) {
					if (s.value(lit_[i].var()) == value_free) {
						return lit_[i];
					}
				}
				return Literal();
			}
			Literal lit_[2];
		}h(negLit(c), negLit(a));
		ctx.master()->setHeuristic(&h, Ownership_t::Retain);
		ctx.startAddConstraints();
		ClauseCreator cl(&s);
		ctx.addBinary(posLit(a), posLit(b));
		ctx.addBinary(negLit(b), posLit(c));
		ctx.addBinary(negLit(a), posLit(c));
		ctx.endInit();
		s.assume(posLit(d));
		s.pushRootLevel();
		REQUIRE(value_true == s.search(-1, -1, 0));
		s.clearAssumptions();
		REQUIRE(0u ==s.decisionLevel());
		REQUIRE(s.isTrue(posLit(c)));
	}

	SECTION("testSearchMaxConflicts") {
		Var a = ctx.addVar(Var_t::Atom);
		Var b = ctx.addVar(Var_t::Atom);
		Var c = ctx.addVar(Var_t::Atom);
		ctx.addVar(Var_t::Atom);
		ctx.startAddConstraints();
		ctx.addBinary(posLit(a), negLit(b));
		ctx.addBinary(negLit(a), posLit(b));
		ctx.addBinary(negLit(a), negLit(b));
		ctx.endInit();
		s.simplify();
		s.assume(posLit(c));
		s.pushRootLevel();
		s.assume(posLit(a));
		REQUIRE(value_free == s.search(1, -1, 0));
		REQUIRE(1u == s.decisionLevel());
	}

	SECTION("testClearAssumptions") {
		Var a = ctx.addVar(Var_t::Atom);
		Var b = ctx.addVar(Var_t::Atom);
		ctx.addVar(Var_t::Atom);
		ctx.startAddConstraints();
		ctx.addBinary(negLit(a), negLit(b));
		ctx.addBinary(negLit(a), posLit(b));
		ctx.endInit();
		s.assume(posLit(a));
		s.pushRootLevel();
		REQUIRE_FALSE(s.propagate());
		REQUIRE(s.clearAssumptions());
		REQUIRE(0u ==s.decisionLevel());

		s.force(posLit(a), 0);
		REQUIRE_FALSE(s.propagate());
		REQUIRE_FALSE(s.clearAssumptions());
	}

	SECTION("testStopConflict") {
		Var a = ctx.addVar(Var_t::Atom);
		Var b = ctx.addVar(Var_t::Atom);
		Var c = ctx.addVar(Var_t::Atom);
		Var d = ctx.addVar(Var_t::Atom);
		ctx.startAddConstraints();
		ctx.addBinary(negLit(a), negLit(b));
		ctx.addBinary(negLit(a), posLit(b));
		ctx.endInit();
		s.assume(posLit(a)) && !s.propagate() && s.resolveConflict();
		REQUIRE((s.decisionLevel() == 0 && s.queueSize() == 1 && !s.hasConflict()));
		s.setStopConflict();
		REQUIRE((s.hasConflict() && !s.resolveConflict()));
		s.clearStopConflict();
		REQUIRE((s.decisionLevel() == 0 && s.queueSize() == 1 && !s.hasConflict()));
		s.propagate();
		s.assume(posLit(c)) && s.propagate();
		s.pushRootLevel(1);
		REQUIRE(s.rootLevel() == 1);
		s.assume(posLit(d));
		s.setStopConflict();
		REQUIRE(s.rootLevel() == 2);
		s.clearStopConflict();
		REQUIRE((s.rootLevel() == 1 && s.queueSize() == 1));
	}

	SECTION("testClearStopConflictResetsBtLevel") {
		Var a = ctx.addVar(Var_t::Atom);
		Var b = ctx.addVar(Var_t::Atom);
		Var c = ctx.addVar(Var_t::Atom);
		Var d = ctx.addVar(Var_t::Atom);
		ctx.startAddConstraints();
		ctx.addBinary(negLit(c), posLit(d));
		ctx.endInit();
		s.assume(posLit(a)) && s.propagate();
		s.assume(posLit(b)) && s.propagate();
		s.assume(posLit(c)) && s.propagate();
		REQUIRE(s.numFreeVars() == 0);
		s.setBacktrackLevel(s.decisionLevel());
		s.backtrack();
		uint32 bt = s.backtrackLevel();
		s.assume(posLit(d)) && s.propagate();
		REQUIRE(bt != s.decisionLevel());
		s.setStopConflict();
		REQUIRE(s.backtrackLevel() == s.decisionLevel());
		s.clearStopConflict();
		REQUIRE(s.backtrackLevel() == bt);
	}

	SECTION("testProblemStats") {
		ProblemStats p1, p2;
		REQUIRE(uint32(0) == p1.vars.num);
		REQUIRE(uint32(0) == p2.vars.eliminated);
		REQUIRE(uint32(0) == p1.constraints.other);
		REQUIRE(uint32(0) == p2.constraints.binary);
		REQUIRE(uint32(0) == p2.constraints.ternary);

		p1.vars.num = 100; p2.vars.num = 150;
		p1.vars.eliminated = 20; p2.vars.eliminated = 30;
		p1.constraints.other = 150; p2.constraints.other = 150;
		p1.constraints.binary = 0; p2.constraints.binary = 100;
		p1.constraints.ternary = 100; p2.constraints.ternary = 0;
		p1.diff(p2);

		REQUIRE(uint32(50) == p1.vars.num);
		REQUIRE(uint32(10) == p1.vars.eliminated);
		REQUIRE(uint32(0) == p1.constraints.other);
		REQUIRE(uint32(100) == p1.constraints.binary);
		REQUIRE(uint32(100) == p1.constraints.ternary);

		StatisticObject s = StatisticObject::map(&p1);
		REQUIRE(s.size() == p1.size());
		REQUIRE(s.at("vars").value() == double(p1.vars.num));
		REQUIRE(s.at("constraints").value() == (double)p1.constraints.other);
		REQUIRE(s.at("constraints_binary").value() == (double)p1.constraints.binary);
		REQUIRE(s.at("constraints_ternary").value() == (double)p1.constraints.ternary);
	}

	SECTION("testSolverStats") {
		SolverStats st, st2;
		st.enableExtended();
		st2.enableExtended();

		st.conflicts = 12; st2.conflicts = 3;
		st.choices = 100; st2.choices = 99;
		st.restarts = 7;  st2.restarts = 8;

		st.extra->models = 10; st2.extra->models = 2;
		st.extra->learnts[0] = 6;  st2.extra->learnts[0] = 4;
		st.extra->learnts[1] = 5;  st2.extra->learnts[1] = 4;
		st.extra->lits[0] = 15; st2.extra->lits[0] = 14;
		st.extra->lits[1] = 5;  st2.extra->lits[1] = 4;
		st.extra->binary = 6;  st2.extra->ternary = 5;
		st.extra->deleted = 10;

		st.accu(st2);

		REQUIRE(uint64(15) == st.conflicts);
		REQUIRE(uint64(199) == st.choices);
		REQUIRE(uint64(15) == st.restarts);
		REQUIRE(uint64(12) == st.extra->models);
		REQUIRE(uint64(29) == st.extra->lits[0]);
		REQUIRE(uint64(9) == st.extra->lits[1]);
		REQUIRE(uint64(10) == st.extra->learnts[0]);
		REQUIRE(uint64(9) == st.extra->learnts[1]);
		REQUIRE(uint32(6) == st.extra->binary);
		REQUIRE(uint32(5) == st.extra->ternary);
		REQUIRE(uint64(10) == st.extra->deleted);

		StatisticObject s = StatisticObject::map(&st);
		REQUIRE(double(15) == s.at("conflicts").value());
		REQUIRE(double(199) == s.at("choices").value());
		REQUIRE(double(15) == s.at("restarts").value());
		StatisticObject e = s.at("extra");
		REQUIRE(double(12) == e.at("models").value());
		REQUIRE(double(29) == e.at("lits_conflict").value());
		REQUIRE(double(9) == e.at("lemmas_loop").value());
		REQUIRE(double(6) == e.at("lemmas_binary").value());
		REQUIRE(double(5) == e.at("lemmas_ternary").value());
		REQUIRE(double(10) == e.at("lemmas_deleted").value());
	}
	SECTION("testClaspStats") {
		typedef ClaspStatistics::Key_t Key_t;
		SolverStats st;
		st.enableExtended();
		st.choices = 100;
		st.extra->learnts[1] = 5;
		st.extra->binary = 6;
		ClaspStatistics stats(StatisticObject::map(&st));
		Key_t root = stats.root();
		REQUIRE(stats.type(root) == Potassco::Statistics_t::Map);
		REQUIRE(stats.writable(root) == false);
		Key_t choices = stats.get(root, "choices");
		REQUIRE(stats.type(choices) == Potassco::Statistics_t::Value);
		REQUIRE(stats.value(choices) == (double)100);
		Key_t extra = stats.get(root, "extra");
		REQUIRE(stats.type(extra) == Potassco::Statistics_t::Map);
		Key_t bin = stats.get(extra, "lemmas_binary");
		REQUIRE(stats.type(bin) == Potassco::Statistics_t::Value);
		REQUIRE(stats.value(bin) == (double)6);

		Key_t binByPath = stats.get(root, "extra.lemmas_binary");
		REQUIRE(binByPath == bin);
	}

	SECTION("testSplitInc") {
		Literal a = posLit(ctx.addVar(Var_t::Atom));
		Literal b = posLit(ctx.addVar(Var_t::Atom));
		Literal c = posLit(ctx.addVar(Var_t::Atom));
		Literal d = posLit(ctx.addVar(Var_t::Atom));
		ctx.startAddConstraints();
		ctx.endInit();
		s.assume(a) && s.propagate();
		s.assume(b) && s.propagate();
		s.assume(c) && s.propagate();
		s.assume(d) && s.propagate();
		LitVec gp;
		s.copyGuidingPath(gp);
		s.pushRootLevel();
		gp.push_back(~a);
		REQUIRE((gp.size() == 1 && gp[0] == ~a && s.rootLevel() == 1));
		gp.pop_back();

		s.copyGuidingPath(gp);
		s.pushRootLevel();
		gp.push_back(~b);
		REQUIRE((gp.size() == 2 && gp[1] == ~b && s.rootLevel() == 2));
		gp.pop_back();

		s.copyGuidingPath(gp);
		s.pushRootLevel();
		gp.push_back(~c);
		REQUIRE((gp.size() == 3 && gp[2] == ~c && s.rootLevel() == 3));
		gp.pop_back();
	}

	SECTION("testSplitFlipped") {
		Literal a = posLit(ctx.addVar(Var_t::Atom));
		Literal b = posLit(ctx.addVar(Var_t::Atom));
		Literal c = posLit(ctx.addVar(Var_t::Atom));
		Literal d = posLit(ctx.addVar(Var_t::Atom));
		ctx.startAddConstraints();
		ctx.endInit();

		LitVec gp;

		s.assume(a) && s.propagate();
		s.pushRootLevel();
		s.assume(b) && s.propagate();
		s.backtrack();

		s.assume(c) && s.propagate();
		s.backtrack();

		s.assume(d) && s.propagate();
		s.copyGuidingPath(gp);
		REQUIRE(std::find(gp.begin(), gp.end(), ~b) != gp.end());
		REQUIRE(std::find(gp.begin(), gp.end(), ~c) != gp.end());
	}

	SECTION("testSplitFlipToNewRoot") {
		Literal a = posLit(ctx.addVar(Var_t::Atom));
		Literal b = posLit(ctx.addVar(Var_t::Atom));
		Literal c = posLit(ctx.addVar(Var_t::Atom));
		Literal d = posLit(ctx.addVar(Var_t::Atom));
		ctx.startAddConstraints();
		ctx.endInit();

		LitVec gp;
		s.assume(a) && s.propagate();
		s.copyGuidingPath(gp);
		s.pushRootLevel();

		s.assume(b) && s.propagate();
		s.assume(c) && s.propagate();

		s.backtrack(); // bt-level now 2, rootLevel = 1

		s.copyGuidingPath(gp);
		s.pushRootLevel();
		REQUIRE(s.rootLevel() == s.backtrackLevel());
		s.assume(d) && s.propagate();
		s.copyGuidingPath(gp);
		s.pushRootLevel();
		REQUIRE(std::find(gp.begin(), gp.end(), ~c) != gp.end());
	}

	SECTION("testSplitImplied") {
		Literal a = posLit(ctx.addVar(Var_t::Atom));
		Literal b = posLit(ctx.addVar(Var_t::Atom));
		Literal c = posLit(ctx.addVar(Var_t::Atom));
		Literal d = posLit(ctx.addVar(Var_t::Atom));
		Literal e = posLit(ctx.addVar(Var_t::Atom));
		Literal f = posLit(ctx.addVar(Var_t::Atom));
		ctx.startAddConstraints();
		ctx.endInit();

		s.assume(a) && s.propagate();
		s.assume(b) && s.propagate();
		s.pushRootLevel(2);

		s.assume(c);
		s.setBacktrackLevel(s.decisionLevel());
		SingleOwnerPtr<TestingConstraint> x(new TestingConstraint);
		s.force(~d, 2, x.get());

		LitVec gp;
		s.copyGuidingPath(gp);

		REQUIRE(std::find(gp.begin(), gp.end(), ~d) != gp.end());
		s.pushRootLevel();
		s.assume(e);
		s.setBacktrackLevel(s.decisionLevel());
		s.force(~f, 2, x.get());

		s.copyGuidingPath(gp);
		REQUIRE(std::find(gp.begin(), gp.end(), ~f) != gp.end());
	}

	SECTION("testAddShortIncremental") {
		Literal a = posLit(ctx.addVar(Var_t::Atom));
		Literal b = posLit(ctx.addVar(Var_t::Atom));
		ctx.setConcurrency(2);
		ctx.startAddConstraints();
		ctx.addBinary(a, b);
		ctx.endInit();
		REQUIRE(ctx.numBinary()  == 1);
		ctx.startAddConstraints();
		ctx.addBinary(~a, ~b);
		ctx.endInit();
		REQUIRE(ctx.numBinary()  == 2);
	}

	SECTION("testSwitchToMtIncremental") {
		Literal a = posLit(ctx.addVar(Var_t::Atom));
		Literal b = posLit(ctx.addVar(Var_t::Atom));
		Literal c = posLit(ctx.addVar(Var_t::Atom));
		Literal d = posLit(ctx.addVar(Var_t::Atom));
		ctx.startAddConstraints();
		ClauseCreator cl(&s);
		cl.start().add(a).add(b).add(c).add(d).end();
		ctx.endInit(true);
		REQUIRE((s.numVars() == 4 && s.numConstraints() == 1));
		ctx.unfreeze();
		Solver& s2 = ctx.pushSolver();
		REQUIRE(ctx.concurrency() == 2);
		ctx.startAddConstraints();
		cl.start().add(~a).add(~b).add(~c).add(~d).end();
		ctx.endInit(true);
		REQUIRE((s.numVars() == 4 && s.numConstraints() == 2));
		REQUIRE((s2.numVars() == 4 && s2.numConstraints() == 2));
	}
	SECTION("testPushAux") {
		Literal a = posLit(ctx.addVar(Var_t::Atom));
		Literal b = posLit(ctx.addVar(Var_t::Atom));
		ctx.startAddConstraints();
		ctx.endInit();
		REQUIRE(s.numVars() == s.sharedContext()->numVars());

		Var aux = s.pushAuxVar();
		REQUIRE(s.numVars() > s.sharedContext()->numVars());
		REQUIRE((s.validVar(aux) && !s.sharedContext()->validVar(aux)));
		LitVec clause;
		clause.push_back(posLit(aux));
		clause.push_back(a);
		clause.push_back(b);
		ClauseCreator::create(s, clause, 0, Constraint_t::Conflict);
		REQUIRE(s.sharedContext()->numTernary() == 0);
		REQUIRE(s.numLearntConstraints() == 1);
		s.assume(~a) && s.propagate();
		s.assume(~b) && s.propagate();
		REQUIRE(s.isTrue(posLit(aux)));
		s.popAuxVar();
		REQUIRE(s.decisionLevel() < 2u);
		REQUIRE(s.numVars() == s.sharedContext()->numVars());
	}
	SECTION("testPushAuxFact") {
		Literal a = posLit(ctx.addVar(Var_t::Atom));
		Literal b = posLit(ctx.addVar(Var_t::Atom));
		ctx.startAddConstraints();
		ctx.endInit();
		Var aux = s.pushAuxVar();
		LitVec clause;
		clause.push_back(posLit(aux));
		clause.push_back(a);
		ClauseCreator::create(s, clause, 0, Constraint_t::Conflict);
		s.force(~a) && s.propagate();
		s.force(b)  && s.simplify();
		REQUIRE(s.numFreeVars() == 0);
		s.popAuxVar();
		REQUIRE((s.numFreeVars() == 0 && s.validVar(aux) == false));
	}
	SECTION("testPopAuxRemovesConstraints") {
		Literal a = posLit(ctx.addVar(Var_t::Atom));
		Literal b = posLit(ctx.addVar(Var_t::Atom));
		Literal c = posLit(ctx.addVar(Var_t::Atom));
		ctx.startAddConstraints();
		ctx.endInit();
		Var aux = s.pushAuxVar();
		LitVec clause;
		clause.push_back(a);
		clause.push_back(b);
		clause.push_back(c);
		clause.push_back(posLit(aux));
		ClauseCreator::create(s, clause, 0, Constraint_t::Conflict);
		clause.clear();
		clause.push_back(a);
		clause.push_back(b);
		clause.push_back(~c);
		clause.push_back(negLit(aux));
		ClauseCreator::create(s, clause, 0, Constraint_t::Conflict);
		REQUIRE(s.numLearntConstraints() == 2);
		s.popAuxVar();
		REQUIRE(s.numLearntConstraints() == 0);
	}
	SECTION("testPopAuxRemovesConstraintsRegression") {
		Literal a = posLit(ctx.addVar(Var_t::Atom));
		Literal b = posLit(ctx.addVar(Var_t::Atom));
		Literal c = posLit(ctx.addVar(Var_t::Atom));
		ctx.startAddConstraints();
		ctx.endInit();
		Var aux = s.pushAuxVar();
		WeightLitVec lits;
		lits.push_back(WeightLiteral(a, 1));
		lits.push_back(WeightLiteral(b, 1));
		lits.push_back(WeightLiteral(c, 1));
		lits.push_back(WeightLiteral(posLit(aux), 1));
		Solver::ConstraintDB t;
		t.push_back(WeightConstraint::create(s, lit_false(), lits, 3, WeightConstraint::create_explicit | WeightConstraint::create_no_add | WeightConstraint::create_no_freeze | WeightConstraint::create_no_share).first());
		s.force(posLit(aux)) && s.propagate();
		s.popAuxVar(1, &t);
	}
	SECTION("testPopAuxMaintainsQueue") {
		Literal a = posLit(ctx.addVar(Var_t::Atom));
		Literal b = posLit(ctx.addVar(Var_t::Atom));
		ctx.startAddConstraints();
		ctx.endInit();
		Var aux = s.pushAuxVar();
		s.force(a, 0); s.propagate();
		s.force(posLit(aux), 0);
		s.force(b, 0);
		s.popAuxVar();
		REQUIRE((s.isTrue(a) && s.isTrue(b) && s.queueSize() == 1 && s.assignment().units() == 1));
	}

	SECTION("testIncrementalAux") {
		Literal a = posLit(ctx.addVar(Var_t::Atom));
		ctx.startAddConstraints();
		Solver& s2 = ctx.pushSolver();
		ctx.endInit(true);
		Var aux = s2.pushAuxVar();
		REQUIRE((!ctx.validVar(aux) && !s.validVar(aux)));
		LitVec clause;
		clause.push_back(a);
		clause.push_back(posLit(aux));
		ClauseCreator::create(s2, clause, 0, Constraint_t::Conflict);
		ctx.unfreeze();
		Var n = ctx.addVar(Var_t::Atom);
		ctx.startAddConstraints();
		ctx.endInit(true);
		s2.assume(negLit(n)) && s2.propagate();
		REQUIRE(s2.value(a.var()) == value_free);
	}

	SECTION("testUnfreezeStepBug") {
		Literal a = posLit(ctx.addVar(Var_t::Atom));
		Literal b = posLit(ctx.addVar(Var_t::Atom));
		ctx.startAddConstraints();
		Solver& s2 = ctx.pushSolver();
		ctx.addBinary(~a, b);
		ctx.endInit(true);
		s2.force(b);
		ctx.unfreeze();
		ctx.endInit(true);
		REQUIRE(((s.force(a) && s.propagate())));
		REQUIRE(s.isTrue(b));
	}
	SECTION("testRemoveConstraint") {
		ctx.startAddConstraints();
		Solver& s2 = ctx.pushSolver();
		ctx.add(new TestingConstraint());
		ctx.endInit(true);
		REQUIRE(s2.numConstraints() == 1);
		ctx.removeConstraint(0, true);
		REQUIRE(s.numConstraints() == 0);
		REQUIRE(s2.numConstraints() == 1);
		ctx.unfreeze();
		ctx.startAddConstraints();
		ctx.add(new TestingConstraint());
		ctx.add(new TestingConstraint());
		ctx.endInit(true);
		REQUIRE(s.numConstraints() == 2);
		REQUIRE(s2.numConstraints() == 3);
	}
	SECTION("testPopVars") {
		ctx.addVar(Var_t::Atom);
		ctx.addVar(Var_t::Atom);
		ctx.addVar(Var_t::Atom);
		REQUIRE(ctx.numVars() == 3u);
		REQUIRE(ctx.master()->numVars() == 0u);
		ctx.popVars(2);
		REQUIRE(ctx.numVars() == 1u);
	}
	SECTION("testPopVarsAfterCommit") {
		ctx.addVar(Var_t::Atom);
		ctx.addVar(Var_t::Atom);
		ctx.addVar(Var_t::Atom);
		ctx.startAddConstraints();
		REQUIRE(ctx.master()->numVars() == 3u);
		REQUIRE(ctx.master()->numFreeVars() == 3u);
		ctx.popVars(2);
		REQUIRE(ctx.numVars() == 1u);
		ctx.endInit();
		REQUIRE(ctx.master()->numVars() == 1u);
		REQUIRE(ctx.master()->numFreeVars() == 1u);
	}
	SECTION("testPopVarsIncremental") {
		ctx.requestStepVar();
		Var v1 = ctx.addVar(Var_t::Atom);
		ctx.addVar(Var_t::Atom);
		Var v3 = ctx.addVar(Var_t::Atom);
		ctx.startAddConstraints();
		ctx.endInit();
		REQUIRE(ctx.numVars() == 4u);
		REQUIRE(ctx.stepLiteral().var() == 4u);
		ctx.addUnary(posLit(v3));
		ctx.addUnary(posLit(v1));
		REQUIRE(ctx.master()->trail().size() == 2u);
		REQUIRE(ctx.master()->trail()[0] == posLit(v3));
		REQUIRE(ctx.master()->trail()[1] == posLit(v1));
		ctx.unfreeze();
		ctx.popVars(2u);
		INFO("step var is not counted");
		REQUIRE(ctx.numVars() == 1u);
		REQUIRE(ctx.stepLiteral().var() == 0u);
		REQUIRE(ctx.master()->trail().size() == 1u);
		REQUIRE(ctx.master()->trail()[0] == posLit(v1));
		ctx.endInit();
		ctx.unfreeze();
		Var v = ctx.addVar(Var_t::Atom);
		ctx.setFrozen(v, true);
		ctx.startAddConstraints();
		ctx.endInit();
		REQUIRE(ctx.numVars() == 3u);
		ctx.unfreeze();
		ctx.popVars(2u);
		INFO("step var is not counted");
		REQUIRE(ctx.stepLiteral().var() == 0u);
		REQUIRE(ctx.stats().vars.frozen == 0u);
	}
	SECTION("testPopVarsIncrementalBug") {
		ctx.requestStepVar();
		ctx.addVar(Var_t::Atom);
		ctx.addVar(Var_t::Atom);
		ctx.startAddConstraints();
		ctx.endInit();
		ctx.unfreeze();
		Var c = ctx.addVar(Var_t::Atom);
		ctx.addVar(Var_t::Atom);
		ctx.startAddConstraints();
		ctx.addUnary(posLit(c));
		ctx.popVars(1);
		ctx.endInit();
		REQUIRE(ctx.master()->isTrue(posLit(c)));
		REQUIRE(ctx.master()->numFreeVars() == 3);
		REQUIRE(ctx.master()->numAssignedVars() == 1);
	}
	SECTION("testPopVarsMT") {
		ctx.requestStepVar();
		ctx.addVar(Var_t::Atom);
		ctx.addVar(Var_t::Atom);
		Var c = ctx.addVar(Var_t::Atom);
		Solver& s2 = ctx.pushSolver();
		ctx.startAddConstraints();
		ctx.endInit(true);
		s2.force(posLit(c));
		ctx.unfreeze();
		REQUIRE((ctx.master()->isTrue(posLit(c)) && s2.isTrue(posLit(c))));
		ctx.popVars(2); // pop c, b
		Var d = ctx.addVar(Var_t::Atom);
		ctx.startAddConstraints();
		REQUIRE(ctx.master()->value(d) == value_free);
		ctx.endInit(true);
		REQUIRE(s2.value(d) == value_free);
	}

	SECTION("testPopAuxVarKeepsQueueSize") {
		Var v1 = ctx.addVar(Var_t::Atom);
		Var v2 = ctx.addVar(Var_t::Atom);
		ctx.startAddConstraints();
		ctx.endInit(true);
		Var a1 = s.pushAuxVar();
		Var a2 = s.pushAuxVar();
		s.force(posLit(a1));
		s.force(posLit(v1));
		s.force(posLit(a2));
		s.propagate();
		s.force(posLit(v2));
		REQUIRE(s.assignment().units() == 3u);
		REQUIRE(s.queueSize() == 1u);
		REQUIRE(s.numAssignedVars() == 4u);
		s.popAuxVar();
		REQUIRE_FALSE(s.validVar(a1));
		REQUIRE_FALSE(s.validVar(a2));
		REQUIRE(s.numAssignedVars() == 2u);
		REQUIRE(s.queueSize() == 1u);
		REQUIRE(s.assignment().units() == 1u);
	}

	SECTION("testPopAuxVarCountsCorrectly") {
		Var v[5];
		for (Var* it = v, *end = v + 5; it != end; ++it) {
			*it = ctx.addVar(Var_t::Atom);
		}
		ctx.startAddConstraints();
		ctx.endInit(true);
		Var a[6];
		for (Var* it = a, *end = a + 6; it != end; ++it) {
			*it = s.pushAuxVar();
		}
		SECTION("with empty queue") {
			s.force(posLit(v[0]));
			s.force(posLit(v[1]));
			s.force(posLit(a[0]));
			s.force(posLit(a[1]));
			s.force(posLit(a[2]));
			s.force(posLit(v[2]));
			s.force(posLit(a[3]));
			s.force(posLit(v[3]));
			s.force(posLit(a[4]));
			s.force(posLit(a[5]));
			s.force(posLit(v[4]));
			s.propagate() && s.simplify();
			REQUIRE(s.assignment().units() == 11u);
			REQUIRE(s.queueSize() == 0u);
			REQUIRE(s.numAssignedVars() == 11u);
			s.popAuxVar();
			REQUIRE(s.queueSize() == 0u);
			REQUIRE(s.assignment().units() == 5u);
		}
		SECTION("with non-empty queue") {
			s.force(posLit(v[0]));
			s.force(posLit(v[1]));
			s.force(posLit(a[0]));
			s.force(posLit(a[1]));
			s.force(posLit(a[2]));
			s.force(posLit(v[2]));
			s.force(posLit(a[3]));
			s.force(posLit(v[3]));
			s.force(posLit(a[4]));
			s.propagate() && s.simplify();
			s.force(posLit(a[5]));
			s.force(posLit(v[4]));
			REQUIRE(s.assignment().units() == 9u);
			REQUIRE(s.queueSize() == 2u);
			REQUIRE(s.numAssignedVars() == 11u);
			s.popAuxVar();
			REQUIRE(s.queueSize() == 1u);
			REQUIRE(s.assignment().units() == 4u);
		}
		REQUIRE(s.numAssignedVars() == 5u);
		REQUIRE(s.numAuxVars() == 0u);
	}
};
TEST_CASE("once", "[.once]") {
	SECTION("testScheduleAdvance") {
		ScheduleStrategy r1 = ScheduleStrategy::geom(100, 1.5, 13);
		for (uint32 i = 0, m = (1u << 15)-1; i != m; ++i, r1.next()) {
			ScheduleStrategy r2 = ScheduleStrategy::geom(100, 1.5, 13);
			r2.advanceTo(i);
			REQUIRE((r1.idx == r2.idx && r1.len == r2.len));
		}
	}
	SECTION("testLubyAdvance") {
		ScheduleStrategy r1 = ScheduleStrategy::luby(64, 10);
		for (uint32 i = 0, m = (1u << 15)-1; i != m; ++i, r1.next()) {
			ScheduleStrategy r2 = ScheduleStrategy::luby(64, 10);
			r2.advanceTo(i);
			REQUIRE((r1.idx == r2.idx && r1.len == r2.len));
		}
	}
}

#if CLASP_HAS_THREADS
static void integrateGp(Solver& s, LitVec& gp) {
	s.clearAssumptions();
	for (LitVec::size_type i = 0; i != gp.size(); ++i) {
		if (s.value(gp[i].var()) == value_free) {
			s.assume(gp[i]) && s.propagate();
			s.pushRootLevel();
		}
	}
}

TEST_CASE("Solver mt", "[core][mt]") {
	SharedContext ctx;
	Literal a = posLit(ctx.addVar(Var_t::Atom));
	Literal b = posLit(ctx.addVar(Var_t::Atom));
	Literal c = posLit(ctx.addVar(Var_t::Atom));
	Literal d = posLit(ctx.addVar(Var_t::Atom));
	ctx.setConcurrency(2);
	Solver& s = *ctx.master();
	SECTION("testLearntShort") {
		ctx.setShareMode(ContextParams::share_problem);
		ctx.startAddConstraints();
		ctx.addBinary(c, d);
		ctx.endInit();
		ClauseCreator cc(&s);
		REQUIRE(cc.start(Constraint_t::Conflict).add(a).add(b).end());
		REQUIRE(cc.start(Constraint_t::Conflict).add(~a).add(~b).add(c).end());
		REQUIRE(ctx.numLearntShort() == 2);
		REQUIRE(ctx.numBinary()  == 1);
		REQUIRE(ctx.numTernary() == 0);

		cc.start(Constraint_t::Conflict).add(a).add(b).add(c).end();
		// ignore subsumed/duplicate clauses
		REQUIRE(ctx.numLearntShort() == 2);

		s.assume(~b);
		s.propagate();
		REQUIRE(((s.isTrue(a) && s.reason(a).firstLiteral() == ~b)));
		s.undoUntil(0);
		s.assume(a);
		s.propagate();
		s.assume(b);
		s.propagate();
		REQUIRE(s.isTrue(c));
		LitVec res;
		s.reason(c, res);
		REQUIRE(std::find(res.begin(), res.end(), a) != res.end());
		REQUIRE(std::find(res.begin(), res.end(), b) != res.end());
	}

	SECTION("testLearntShortAreDistributed") {
		struct Dummy : public Distributor {
			Dummy() : Distributor(Policy(UINT32_MAX, UINT32_MAX, UINT32_MAX)), unary(0), binary(0), ternary(0) {}
			void publish(const Solver&, SharedLiterals* lits) {
				uint32 size = lits->size();
				unary   += size == 1;
				binary  += size == 2;
				ternary += size == 3;
				shared.push_back(lits);
			}
			uint32 receive(const Solver&, SharedLiterals** out, uint32 num) {
				uint32 r = 0;
				while (!shared.empty() && num--) {
					out[r++] = shared.back();
					shared.pop_back();
				}
				return r;
			}
			uint32 unary;
			uint32 binary;
			uint32 ternary;
			PodVector<SharedLiterals*>::type shared;
		}* dummy;
		ctx.distributor.reset(dummy = new Dummy());
		ctx.startAddConstraints();
		ctx.endInit();
		LitVec lits; lits.resize(2);
		lits[0] = a; lits[1] = b;
		ClauseCreator::create(s, lits, 0, ConstraintInfo(Constraint_t::Conflict));
		lits.resize(3);
		lits[0] = ~a; lits[1] = ~b; lits[2] = ~c;
		ClauseCreator::create(s, lits, 0, ConstraintInfo(Constraint_t::Loop));
		lits.resize(1);
		lits[0] = d;
		ClauseCreator::create(s, lits, 0, ConstraintInfo(Constraint_t::Conflict));
		REQUIRE(dummy->unary  == 1);
		REQUIRE(dummy->binary == 1);
		REQUIRE(dummy->ternary == 1);
		SharedLiterals* rec[3];
		REQUIRE(dummy->receive(s, rec, 3) == 3);
		REQUIRE(ClauseCreator::integrate(s, rec[0], 0).ok() == true);
		REQUIRE(ClauseCreator::integrate(s, rec[1], 0).ok() == true);
		REQUIRE(ClauseCreator::integrate(s, rec[2], 0).ok() == true);
	}

	SECTION("testAuxAreNotDistributed") {
		struct Dummy : public Distributor {
			Dummy() : Distributor(Policy(UINT32_MAX, UINT32_MAX, UINT32_MAX)) {}
			void publish(const Solver&, SharedLiterals* lits) {
				shared.push_back(lits);
			}
			uint32 receive(const Solver&, SharedLiterals**, uint32) { return 0; }
			PodVector<SharedLiterals*>::type shared;
		}* dummy;
		ctx.distributor.reset(dummy = new Dummy());
		ctx.startAddConstraints();
		ctx.endInit();
		Var aux = s.pushAuxVar();

		LitVec lits; lits.resize(2);
		lits[0] = a; lits[1] = posLit(aux);
		ClauseCreator::create(s, lits, 0, ConstraintInfo(Constraint_t::Conflict));
		lits.resize(4);
		lits[0] = ~a; lits[1] = posLit(aux); lits[2] = b; lits[3] = c;
		ClauseCreator::create(s, lits, 0, ConstraintInfo(Constraint_t::Conflict));
		REQUIRE(s.numLearntConstraints() == 2);
		REQUIRE(dummy->shared.empty());
		REQUIRE(s.getLearnt(0).simplify(s, false) == false);
		s.pushRoot(posLit(aux));
		s.pushRoot(a);
		lits.clear();
		s.copyGuidingPath(lits);
		REQUIRE((lits.size() == 1 && lits[0] == a));
		s.clearAssumptions();
		lits.resize(4);
		lits[0] = ~a; lits[1] = posLit(aux); lits[2] = ~b; lits[3] = c;
		ClauseCreator::create(s, lits, 0, ConstraintInfo(Constraint_t::Conflict));
		s.assume(a) && s.propagate();
		s.assume(negLit(aux)) && s.propagate();
		s.assume(~c) && s.propagate();
		REQUIRE(s.hasConflict());
		s.resolveConflict();
		REQUIRE(s.numLearntConstraints() == 4);
		REQUIRE(dummy->shared.empty());
		REQUIRE(s.sharedContext()->numTernary() + s.sharedContext()->numBinary() == 0);
	}

	SECTION("testAttachToDB") {
		ctx.startAddConstraints();
		ClauseCreator cc(&s);
		cc.start().add(a).add(b).add(c).add(d).end();
		Solver& s2 = ctx.pushSolver();
		ctx.endInit();
		ctx.attach(s2);
		REQUIRE(s2.numConstraints() == 1);
		ctx.unfreeze();
		Literal e = posLit(ctx.addVar( Var_t::Atom ));
		Literal f = posLit(ctx.addVar( Var_t::Atom ));
		Literal g = posLit(ctx.addVar( Var_t::Atom ));
		Literal h = posLit(ctx.addVar( Var_t::Atom ));
		ctx.startAddConstraints();
		cc.start().add(e).add(f).add(g).add(h).end();
		cc.start().add(a).end();
		ctx.endInit();
		REQUIRE(s.numConstraints() == 1);
		ctx.attach(s2);
		REQUIRE(s2.numConstraints() == 1);
		s2.assume(~e) && s2.propagate();
		s2.assume(~f) && s2.propagate();
		s2.assume(~g) && s2.propagate();
		REQUIRE(s2.isTrue(h));
	}

	SECTION("testAttachDeferred") {
		ctx.startAddConstraints();
		ClauseCreator cc(&s);
		cc.start().add(a).add(b).add(c).add(d).end();
		Solver& s2= ctx.pushSolver();
		ctx.endInit(true);
		REQUIRE(s2.numConstraints() == 1);
		ctx.unfreeze();
		ctx.startAddConstraints();
		cc.start().add(~a).add(~b).add(c).add(d).end();
		ctx.endInit(false);
		REQUIRE(s.numConstraints() == 2);
		REQUIRE(s2.numConstraints() == 1);
		ctx.unfreeze();
		ctx.startAddConstraints();
		cc.start().add(a).add(b).add(~c).add(~d).end();
		ctx.endInit(true);
		REQUIRE(s.numConstraints() == 3);
		REQUIRE(s2.numConstraints() == 3);
	}
	SECTION("testUnfortunateSplitSeq") {
		ctx.startAddConstraints();
		Solver& s2= ctx.pushSolver();
		ctx.endInit(true);

		s.assume(a)   && s.propagate();
		// new fact
		s.backtrack() && s.propagate();

		s.assume(b) && s.propagate();

		LitVec sGp;
		s.copyGuidingPath(sGp);

		sGp.push_back(~b);
		s.pushRootLevel();
		integrateGp(s2, sGp);
		sGp.pop_back();
		s.clearAssumptions();

		LitVec s2Gp;

		s2.assume(c)&& s.propagate();
		s2.copyGuidingPath(s2Gp);
		s.pushRootLevel();
		s2Gp.push_back(~c);
		integrateGp(s, s2Gp);
		s2.clearAssumptions();
		s2Gp.clear();

		s.assume(d)&& s.propagate();
		sGp.clear();
		s.copyGuidingPath(sGp);

		integrateGp(s2, sGp);
		REQUIRE(s2.isTrue(~a));
	}
}
#endif
} }

