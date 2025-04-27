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
#include <clasp/clause.h>
#include <clasp/solver.h>
#include <algorithm>
#include "catch.hpp"
#ifdef _MSC_VER
#pragma warning (disable : 4267) //  conversion from 'size_t' to unsigned int
#pragma once
#endif

namespace Clasp { namespace Test {
static int countWatches(const Solver& s, ClauseHead* c, const LitVec& lits) {
	int w = 0;
	for (LitVec::size_type i = 0; i != lits.size(); ++i) {
		w += s.hasWatch(~lits[i], c);
	}
	return w;
}
static ClauseHead* createClause(Solver& s, LitVec& lits, const ConstraintInfo& info = Constraint_t::Static) {
	uint32 flags = ClauseCreator::clause_explicit | ClauseCreator::clause_no_add | ClauseCreator::clause_no_prepare;
	return ClauseCreator::create(s, lits, flags, info).local;
}
static ClauseHead* createShared(Solver& s, LitVec& lits, const ConstraintInfo& info = Constraint_t::Static) {
	assert(lits.size() >= 2);
	SharedLiterals* shared_lits = SharedLiterals::newShareable(lits, info.type());
	return Clasp::Clause::newShared(s, shared_lits, info, &lits[0], false);
}
static LitVec& makeLits(LitVec& lits, uint32 pos, uint32 neg) {
	lits.clear();
	for (uint32 i = 0, end = pos + neg; i != end; ++i) {
		if (pos) { lits.push_back(posLit(i+1)); --pos; }
		else     { lits.push_back(negLit(i+1));}
	}
	return lits;
}

TEST_CASE("Clause", "[core][constraint]") {
	typedef ConstraintInfo ClauseInfo;
	SharedContext ctx;
	for (int i = 1; i < 15; ++i) {
		ctx.addVar(Var_t::Atom);
	}
	Literal x1 = posLit(1), x2 = posLit(2), x3 = posLit(3);
	ctx.startAddConstraints(10);
	Solver& solver = *ctx.master();
	LitVec clLits;
	ClauseHead* c = 0;
	SECTION("with simple clause") {
		makeLits(clLits, 2, 2);
		SECTION("test ctor adds watches") {
			c = createClause(solver, clLits, Constraint_t::Static);
			solver.add(c);
			REQUIRE(2 == countWatches(solver, c, clLits));
		}
		SECTION("test clause types") {
			ClauseInfo t;
			SECTION("static") {
				t = Constraint_t::Static;
			}
			SECTION("conflict") {
				t = Constraint_t::Conflict;
			}
			SECTION("loop") {
				t = Constraint_t::Loop;
			}
			c = createClause(solver, clLits, t);
			REQUIRE(c->type() == t.type());
			c->destroy();
		}
		SECTION("testPropGenericClause") {
			solver.add(c = createClause(solver, clLits));
			solver.assume(~clLits[0]);
			solver.propagate();
			solver.assume(~clLits.back());
			solver.propagate();

			solver.assume(~clLits[1]);
			solver.propagate();

			REQUIRE(solver.isTrue(clLits[2]));
			REQUIRE(c->locked(solver));
			Antecedent reason = solver.reason(clLits[2]);
			REQUIRE(reason == c);

			LitVec r;
			reason.reason(solver, clLits[2], r);
			REQUIRE(std::find(r.begin(), r.end(), ~clLits[0]) != r.end());
			REQUIRE(std::find(r.begin(), r.end(), ~clLits[1]) != r.end());
			REQUIRE(std::find(r.begin(), r.end(), ~clLits[3]) != r.end());
		}
		SECTION("testClauseActivity") {
			uint32 exp = 258;
			ClauseHead* cl1 = createClause(solver, clLits, ClauseInfo(Constraint_t::Conflict).setActivity(exp));
			ClauseHead* cl2 = createClause(solver, clLits, ClauseInfo(Constraint_t::Loop).setActivity(exp));
			solver.add(cl1);
			solver.add(cl2);
			while (exp != 0) {
				REQUIRE((cl1->activity().activity() == cl2->activity().activity() && cl1->activity().activity() == exp));
				exp >>= 1;
				cl1->decreaseActivity();
				cl2->decreaseActivity();
			}
			REQUIRE((cl1->activity().activity() == cl2->activity().activity() && cl1->activity().activity() == exp));
		}
		SECTION("testPropGenericClauseConflict") {
			solver.add(c = createClause(solver, clLits));
			solver.assume(~clLits[0]);
			solver.force(~clLits[1], 0);
			solver.force(~clLits[2], 0);
			solver.force(~clLits[3], 0);

			REQUIRE_FALSE(solver.propagate());
			const LitVec& r = solver.conflict();
			REQUIRE(std::find(r.begin(), r.end(), ~clLits[0]) != r.end());
			REQUIRE(std::find(r.begin(), r.end(), ~clLits[1]) != r.end());
			REQUIRE(std::find(r.begin(), r.end(), ~clLits[2]) != r.end());
			REQUIRE(std::find(r.begin(), r.end(), ~clLits[3]) != r.end());
		}
		SECTION("testPropAlreadySatisfied") {
			solver.add(c = createClause(solver, clLits));

			// satisfy the clause...
			solver.force(clLits[2], 0);
			solver.propagate();

			// ...make all but one literal false
			solver.force(~clLits[0], 0);
			solver.force(~clLits[1], 0);
			solver.propagate();

			// ...and assert that the last literal is still unassigned
			REQUIRE(value_free == solver.value(clLits[3].var()));
		}
		SECTION("testReasonBumpsActivityIfLearnt") {
			ctx.endInit();
			ClauseHead* cl1 = createClause(solver, clLits, ClauseInfo(Constraint_t::Conflict));
			solver.addLearnt(cl1, (uint32)clLits.size());
			solver.assume(~clLits[0]);
			solver.propagate();
			solver.assume(~clLits[1]);
			solver.propagate();
			solver.assume(~clLits[2]);
			uint32 a = cl1->activity().activity();
			solver.force(~clLits[3], Antecedent(0));
			REQUIRE_FALSE(solver.propagate());
			REQUIRE(a+1 == cl1->activity().activity());
		}
		SECTION("testSimplifyUnitButNotLocked") {
			solver.add(c = createClause(solver, clLits));
			solver.force(clLits[0], 0);  // SAT clause
			solver.force(~clLits[1], 0);
			solver.force(~clLits[2], 0);
			solver.force(~clLits[3], 0);
			solver.propagate();
			REQUIRE(c->simplify(solver, false));
		}
	}

	SECTION("testSimplifySAT") {
		makeLits(clLits, 3, 2);
		solver.add(c = createClause(solver, clLits));
		solver.force( ~clLits[1], 0);
		solver.force( clLits[2], 0 );
		solver.propagate();

		REQUIRE(c->simplify(solver, false));
	}
	SECTION("testSimplifyRemovesFalseLitsBeg") {
		solver.add(c = createClause(solver, makeLits(clLits, 3, 3)));
		REQUIRE(6 == c->size());

		solver.force(~clLits[0], 0);
		solver.force(~clLits[1], 0);
		solver.propagate();

		REQUIRE_FALSE(c->simplify(solver));
		REQUIRE(4 == c->size());

		REQUIRE(2 == countWatches(solver, c, clLits));
	}

	SECTION("testSimplifyRemovesFalseLitsMid") {
		solver.add(c = createClause(solver, makeLits(clLits, 3, 3)));
		REQUIRE(6 == c->size());
		solver.force(~clLits[1], 0);
		solver.force(~clLits[2], 0);
		solver.propagate();

		REQUIRE_FALSE(c->simplify(solver));
		REQUIRE(4 == c->size());

		REQUIRE(2 == countWatches(solver, c, clLits));
	}

	SECTION("test simplify short") {
		solver.add(c = createClause(solver, makeLits(clLits, 2, 3)));
		SECTION("removesFalseLitsBeg") {
			solver.force(~clLits[0], 0);
			solver.propagate();
		}
		SECTION("removesFalseLitsMid") {
			solver.force(~clLits[2], 0);
			solver.propagate();
		}
		SECTION("removesFalseLitsEnd") {
			solver.force(~clLits[4], 0);
			solver.propagate();
		}
		REQUIRE_FALSE(c->simplify(solver));
		REQUIRE(4 == c->size());
		REQUIRE(2 == countWatches(solver, c, clLits));
	}
	SECTION("testSimplifyRemovesFalseLitsEnd") {
		solver.add(c = createClause(solver, makeLits(clLits, 3, 3)));
		REQUIRE(6 == c->size());

		solver.force(~clLits[4], 0);
		solver.force(~clLits[5], 0);
		solver.propagate();

		REQUIRE_FALSE(c->simplify(solver));
		REQUIRE(4 == c->size());

		REQUIRE(2 == countWatches(solver, c, clLits));
	}
	SECTION("testStrengthen") {
		c = createClause(solver, makeLits(clLits, 6, 0), ClauseInfo());
		REQUIRE_FALSE(c->strengthen(solver, x2).second);
		REQUIRE(c->size() == 5);
		REQUIRE_FALSE(c->strengthen(solver, x3).second);
		REQUIRE(c->size() == 4);

		REQUIRE(c->strengthen(solver, x1).second);
		REQUIRE(c->size() == 3);
		c->destroy(&solver, true);
	}

	SECTION("testStrengthenToUnary") {
		Literal b = posLit(ctx.addVar( Var_t::Atom ));
		Literal x = posLit(ctx.addVar( Var_t::Atom ));
		Literal y = posLit(ctx.addVar( Var_t::Atom ));
		ctx.startAddConstraints();
		ctx.endInit();
		Literal a = posLit(solver.pushTagVar(true));
		solver.assume(x) && solver.propagate();
		solver.assume(y) && solver.propagate();
		solver.setBacktrackLevel(solver.decisionLevel());
		clLits.clear();
		clLits.push_back(b);
		clLits.push_back(~a);
		ClauseInfo extra(Constraint_t::Conflict); extra.setTagged(true);
		c = ClauseCreator::create(solver, clLits, 0, extra).local;
		REQUIRE(c->size() == 2);
		REQUIRE((solver.isTrue(b) && solver.reason(b).constraint() == c));
		solver.backtrack() && solver.propagate();
		REQUIRE((solver.isTrue(b) && solver.reason(b).constraint() == c));
		c->strengthen(solver, ~a);
		solver.backtrack();
		REQUIRE(solver.isTrue(b));
		LitVec out;
		solver.reason(b, out);
		REQUIRE((out.size() == 1 && out[0] == lit_true()));
		solver.clearAssumptions();
		REQUIRE(solver.isTrue(b));
	}

	SECTION("testStrengthenContracted") {
		ctx.endInit();
		LitVec lits;
		lits.push_back(x1);
		for (uint32 i = 2; i <= 12; ++i) {
			solver.assume(negLit(i));
			lits.push_back(posLit(i));
		}
		solver.strategies().compress = 4;
		c = ClauseCreator::create(solver, lits, 0, Constraint_t::Conflict).local;
		uint32 si = c->size();
		c->strengthen(solver, posLit(12));
		solver.undoUntil(solver.decisionLevel()-1);
		REQUIRE((solver.value(posLit(12).var()) == value_free && si == c->size()));
		solver.undoUntil(solver.level(posLit(9).var())-1);

		REQUIRE(si+1 <= c->size());
		si = c->size();

		c->strengthen(solver, posLit(2));
		c->strengthen(solver, posLit(6));
		REQUIRE(si == c->size());
		c->strengthen(solver, posLit(9));

		c->strengthen(solver, posLit(8));
		c->strengthen(solver, posLit(5));
		c->strengthen(solver, posLit(4));
		c->strengthen(solver, posLit(3));

		REQUIRE_FALSE(c->strengthen(solver, posLit(7), false).second);
		REQUIRE(uint32(3) == c->size());
		REQUIRE(c->strengthen(solver, posLit(11)).second);
		REQUIRE(uint32(sizeof(Clause) + (9*sizeof(Literal))) == ((Clause*)c)->computeAllocSize());
	}
	SECTION("testStrengthen") {
		ctx.endInit();
		LitVec clause;
		clause.push_back(x1);
		for (uint32 i = 2; i <= 6; ++i) {
			solver.assume(negLit(8-i)) && solver.propagate();
			clause.push_back(posLit(i));
		}
		SECTION("test bug") {
			c = Clause::newContractedClause(solver, ClauseRep::create(&clause[0], (uint32)clause.size(), ClauseInfo(Constraint_t::Conflict)), 5, true);
			solver.addLearnt(c, 5);
			uint32 si = c->size();
			REQUIRE(si == 5);
			c->strengthen(solver, posLit(4));
			LitVec clause2;
			c->toLits(clause2);
			REQUIRE(clause2.size() == 5);
			for (uint32 i = 0; i != clause.size(); ++i) {
				REQUIRE((std::find(clause2.begin(), clause2.end(), clause[i]) != clause2.end() || clause[i] == posLit(4)));
			}
		}
		SECTION("test no extend") {
			ClauseRep x = ClauseRep::create(&clause[0], (uint32)clause.size(), ClauseInfo(Constraint_t::Conflict));
			c = Clause::newContractedClause(solver, x, 4, false);
			solver.addLearnt(c, 4);
			REQUIRE(c->size() == 4);
			c->strengthen(solver, posLit(2));
			REQUIRE(c->size() == 4);
			solver.undoUntil(0);
			REQUIRE(c->size() == 4);
		}
	}
	SECTION("testStrengthenLocked") {
		Var a = ctx.addVar( Var_t::Atom );
		Var b = ctx.addVar( Var_t::Atom );
		Var c = ctx.addVar( Var_t::Atom );
		ctx.startAddConstraints();
		ctx.endInit();
		Literal tag = posLit(solver.pushTagVar(true));
		solver.assume(posLit(a)) && solver.propagate();
		solver.assume(posLit(b)) && solver.propagate();
		ClauseCreator cc(&solver);
		cc.start(Constraint_t::Conflict).add(negLit(a)).add(negLit(b)).add(negLit(c)).add(~tag);
		ClauseHead* clause = cc.end().local;
		REQUIRE(clause->locked(solver));
		REQUIRE(!clause->strengthen(solver, ~tag).second);
		REQUIRE(clause->locked(solver));
		LitVec r;
		solver.reason(negLit(c), r);
		REQUIRE(r.size() == 2);
		REQUIRE(std::find(r.begin(), r.end(), posLit(a)) != r.end());
		REQUIRE(std::find(r.begin(), r.end(), posLit(b)) != r.end());
	}

	SECTION("testStrengthenLockedEarly") {
		Literal b = posLit(ctx.addVar( Var_t::Atom ));
		Literal c = posLit(ctx.addVar( Var_t::Atom ));
		Literal d = posLit(ctx.addVar( Var_t::Atom ));
		Literal x = posLit(ctx.addVar( Var_t::Atom ));
		ctx.startAddConstraints();
		ctx.endInit();
		Literal a = posLit(solver.pushTagVar(true));
		solver.assume(b) && solver.propagate();
		solver.force(c, 0) && solver.propagate();
		solver.assume(x) && solver.propagate();
		solver.setBacktrackLevel(solver.decisionLevel());

		ClauseCreator cc(&solver);
		cc.start(Constraint_t::Conflict).add(~a).add(~b).add(~c).add(d);
		ClauseHead* clause = cc.end().local;
		REQUIRE(clause->locked(solver));
		bool remove = clause->strengthen(solver, ~a).second;
		solver.backtrack();
		REQUIRE(solver.isTrue(d));
		REQUIRE((!remove || solver.reason(d).type() != Antecedent::Generic || solver.reason(d).constraint() != clause));
	}

	SECTION("testSimplifyTagged") {
		Var a = ctx.addVar( Var_t::Atom );
		Var b = ctx.addVar( Var_t::Atom );
		Var c = ctx.addVar( Var_t::Atom );
		ctx.startAddConstraints();
		ctx.endInit();
		Literal tag = posLit(solver.pushTagVar(true));
		ClauseCreator cc(&solver);
		// ~a ~b ~c ~tag
		cc.start(Constraint_t::Conflict).add(negLit(a)).add(negLit(b)).add(negLit(c)).add(~tag);
		ClauseHead* clause = cc.end().local;

		solver.force(posLit(c));
		REQUIRE_FALSE(clause->strengthen(solver, negLit(c)).second);
	}

	SECTION("testClauseSatisfied") {
		ConstraintType t = Constraint_t::Conflict;
		TypeSet ts; ts.addSet(t);
		solver.addLearnt(c = createClause(solver, makeLits(clLits, 2, 2), t), 4);
		LitVec free;
		REQUIRE(uint32(t) == c->isOpen(solver, ts, free));
		REQUIRE(LitVec::size_type(4) == free.size());

		solver.assume( clLits[2] );
		solver.propagate();
		free.clear();
		REQUIRE(uint32(0) == c->isOpen(solver, ts, free));
		solver.undoUntil(0);
		solver.assume( ~clLits[1] );
		solver.assume( ~clLits[2] );
		solver.propagate();
		free.clear();
		REQUIRE(uint32(t) == c->isOpen(solver, ts, free));
		REQUIRE(LitVec::size_type(2) == free.size());
		ts.m = 0; ts.addSet(Constraint_t::Loop);
		REQUIRE(uint32(0) == c->isOpen(solver, ts, free));
	}

	SECTION("testContraction") {
		ctx.endInit();
		LitVec lits(1, x1);
		for (uint32 i = 2; i <= 12; ++i) {
			solver.assume(negLit(i));
			lits.push_back(posLit(i));
		}
		solver.strategies().compress = 6;
		c = ClauseCreator::create(solver, lits, 0, Constraint_t::Conflict).local;
		uint32  s1 = c->size();
		REQUIRE(s1 < lits.size());
		LitVec r;
		solver.reason(x1, r);
		REQUIRE( r.size() == lits.size()-1 );

		solver.undoUntil(0);
		REQUIRE(c->size() == lits.size());
	}

	SECTION("testNewContractedClause") {
		ctx.endInit();
		// head
		clLits.push_back(x1);
		clLits.push_back(x2);
		clLits.push_back(x3);
		for (uint32 i = 4; i <= 12; ++i) {
			solver.assume(negLit(i));
			// (false) tail
			clLits.push_back(posLit(i));
		}
		ClauseRep x = ClauseRep::create(&clLits[0], (uint32)clLits.size(), ClauseInfo(Constraint_t::Conflict));
		c = Clause::newContractedClause(solver, x, 3, false);
		solver.addLearnt(c, static_cast<uint32>(clLits.size()));
		REQUIRE(c->size() < clLits.size());

		solver.assume(~x1) && solver.propagate();
		solver.assume(~x3) && solver.propagate();
		REQUIRE(solver.isTrue(x2));
		LitVec r;
		solver.reason(x2, r);
		REQUIRE(r.size() == clLits.size()-1);
	}
	SECTION("testBug") {
		solver.add(c = createClause(solver, makeLits(clLits, 3, 3)));
		solver.assume(~clLits[1]);
		solver.propagate();
		solver.assume(~clLits[2]);
		solver.propagate();
		solver.assume(~clLits[3]);
		solver.propagate();
		solver.assume(~clLits[0]);
		solver.propagate();
		uint32 exp = solver.numAssignedVars();
		solver.undoUntil(0);
		solver.assume(~clLits[1]);
		solver.propagate();
		solver.assume(~clLits[2]);
		solver.propagate();
		solver.assume(~clLits[3]);
		solver.propagate();
		solver.assume(~clLits[4]);
		solver.propagate();

		REQUIRE(exp == solver.numAssignedVars());
		REQUIRE(solver.hasWatch(~clLits[0], c));
		REQUIRE(solver.hasWatch(~clLits[5], c));
	}

	SECTION("testClone") {
		Solver& solver2 = ctx.pushSolver();
		ctx.endInit(true);
		c = createClause(solver, makeLits(clLits, 3, 3));
		ClauseHead* clone  = (ClauseHead*)c->cloneAttach(solver2);
		LitVec lits;
		clone->toLits(lits);
		REQUIRE(lits == clLits);
		REQUIRE(countWatches(solver2, clone, lits) == 2);
		clone->destroy(&solver2, true);

		solver.force(~clLits[0], 0);
		solver.force(~clLits[2], 0);
		solver.propagate();
		c->simplify(solver);
		clone = (ClauseHead*)c->cloneAttach(solver2);
		lits.clear();
		clone->toLits(lits);
		REQUIRE(lits.size() == 4);
		REQUIRE(countWatches(solver2, clone, lits) == 2);
		clone->destroy(&solver2, true);
		c->destroy(&solver, true);
	}
}
TEST_CASE("Propagate random clause", "[constraint][core]") {
	LitVec lits, r;
	for (int size = 2; size != 12; ++size) {
		for (int run = 0; run != 4; ++run) {
			SharedContext ctx;
			Solver& solver = *ctx.master();
			for (int i = 0; i != 12; ++i) { ctx.addVar(Var_t::Atom); }
			ctx.startAddConstraints(1);
			int pos = rand() % size + 1;
			makeLits(lits, pos, size - pos);
			ClauseHead* clause = 0;
			if (run & 1) { clause = createClause(solver, lits); }
			else         { clause = createShared(solver, lits); }
			solver.add(clause);
			std::random_shuffle(lits.begin(), lits.end());
			REQUIRE(value_free == solver.value(lits.back().var()));
			for (LitVec::size_type i = 0; i != lits.size() - 1; ++i) {
				REQUIRE(value_free == solver.value(lits[i].var()));
				solver.force(~lits[i], 0);
				solver.propagate();
			}
			REQUIRE(solver.isTrue(lits.back()));
			Antecedent reason = solver.reason(lits.back());
			REQUIRE(reason == clause);
			r.clear();
			clause->reason(solver, lits.back(), r);
			for (LitVec::size_type i = 0; i != lits.size() - 1; ++i) {
				LitVec::iterator it = std::find(r.begin(), r.end(), ~lits[i]);
				REQUIRE(it != r.end());
				r.erase(it);
			}
			while (!r.empty() && isSentinel(r.back())) r.pop_back();
			REQUIRE(r.empty());
		}
	}
}

TEST_CASE("Loop formula", "[constraint][asp]") {
	SharedContext ctx;
	Literal a1 = posLit(ctx.addVar(Var_t::Atom));
	Literal a2 = posLit(ctx.addVar(Var_t::Atom));
	Literal a3 = posLit(ctx.addVar(Var_t::Atom));
	Literal b1 = posLit(ctx.addVar(Var_t::Body));
	Literal b2 = posLit(ctx.addVar(Var_t::Body));
	Literal b3 = posLit(ctx.addVar(Var_t::Body));
	Solver& solver = ctx.startAddConstraints();
	SECTION("with init") {
		ctx.endInit();
		solver.assume(~b1);
		solver.assume(~b2);
		solver.assume(~b3);
		solver.propagate();
		Literal lits[] = {~a1, b3, b2, b1, ~a1, ~a2, ~a3};
		LoopFormula* lf = LoopFormula::newLoopFormula(solver, ClauseRep::prepared(lits, 4), lits+4, 3);
		solver.addLearnt(lf, lf->size());
		solver.force(~a1, lf);
		solver.force(~a2, lf);
		solver.force(~a3, lf);
		solver.propagate();

		SECTION("has initial watches") {
			REQUIRE(solver.hasWatch(a1, lf));
			REQUIRE(solver.hasWatch(a2, lf));
			REQUIRE(solver.hasWatch(a3, lf));
			REQUIRE(solver.hasWatch(~b3, lf));
		}
		SECTION("simplifyLFIfOneBodyTrue") {
			solver.undoUntil(0);
			solver.force(b2, 0);
			solver.propagate();

			REQUIRE(lf->simplify(solver));
			REQUIRE_FALSE(solver.hasWatch(a1, lf));
			REQUIRE_FALSE(solver.hasWatch(~b3, lf));
		}
		SECTION("simplifyLFIfAllAtomsFalse") {
			solver.undoUntil(0);
			solver.force(~a1, 0);
			solver.force(~a2, 0);
			solver.propagate();
			REQUIRE_FALSE(lf->simplify(solver));
			solver.assume(a3);
			solver.propagate();
			solver.backtrack();
			solver.propagate();
			REQUIRE(lf->simplify(solver));
			REQUIRE_FALSE(solver.hasWatch(~b3, lf));
			REQUIRE_FALSE(solver.hasWatch(a1, lf));
			REQUIRE_FALSE(solver.hasWatch(a2, lf));
			REQUIRE_FALSE(solver.hasWatch(a3, lf));
			solver.reduceLearnts(1.0f);
		}

		SECTION("simplifyLFRemovesFalseBodies") {
			solver.undoUntil(0);

			solver.force(~b1, 0);
			solver.propagate();
			REQUIRE(lf->simplify(solver));
			REQUIRE(3u == solver.sharedContext()->numLearntShort());
		}

		SECTION("simplifyLFRemovesFalseAtoms") {
			solver.undoUntil(0);
			solver.force(~a1, 0);
			solver.propagate();
			REQUIRE_FALSE(lf->simplify(solver));
			REQUIRE(5 == lf->size());

			solver.force(~a3, 0);
			solver.propagate();
			REQUIRE_FALSE(lf->simplify(solver));
			REQUIRE(4 == lf->size());

			solver.force(~a2, 0);
			solver.propagate();
			REQUIRE(lf->simplify(solver));
		}

		SECTION("simplifyLFRemovesTrueAtoms") {
			solver.undoUntil(0);
			solver.force(a1, 0);
			solver.propagate();
			REQUIRE(lf->simplify(solver));

			REQUIRE(1u == solver.sharedContext()->numLearntShort());
		}

		SECTION("loopFormulaPropagateBody") {
			solver.undoUntil(0);
			solver.assume(~b1);
			solver.propagate();
			solver.assume(~b3);
			solver.propagate();
			solver.assume(a3);
			solver.propagate();

			REQUIRE(true == solver.isTrue(b2));
			REQUIRE(Antecedent::Generic == solver.reason(b2).type());
			LitVec r;
			solver.reason(b2, r);
			REQUIRE(LitVec::size_type(3) == r.size());
			REQUIRE(std::find(r.begin(), r.end(), a3) != r.end());
			REQUIRE(std::find(r.begin(), r.end(), ~b3) != r.end());
			REQUIRE(std::find(r.begin(), r.end(), ~b1) != r.end());

			REQUIRE(lf->locked(solver));
		}

		SECTION("loopFormulaPropagateBody2") {
			solver.undoUntil(0);
			solver.assume(a1);
			solver.propagate();
			solver.undoUntil(0);

			solver.assume(~b1);
			solver.propagate();
			solver.assume(a2);
			solver.propagate();
			solver.assume(~b2);
			solver.propagate();

			REQUIRE(true == solver.isTrue(b3));

			REQUIRE(Antecedent::Generic == solver.reason(b3).type());
			LitVec r;
			solver.reason(b3, r);
			REQUIRE(LitVec::size_type(3) == r.size());
			REQUIRE(std::find(r.begin(), r.end(), ~b1) != r.end());
			REQUIRE(std::find(r.begin(), r.end(), ~b2) != r.end());
			REQUIRE(std::find(r.begin(), r.end(), a2) != r.end());

			REQUIRE(lf->locked(solver));
		}

		SECTION("loopFormulaPropagateAtoms") {
			solver.undoUntil(0);
			solver.assume(~b3);
			solver.propagate();
			solver.assume(~b1);
			solver.propagate();

			solver.assume(~a1);
			solver.propagate();

			solver.assume(~b2);
			solver.propagate();

			REQUIRE(true == solver.isTrue(~a1));
			REQUIRE(true == solver.isTrue(~a2));
			REQUIRE(true == solver.isTrue(~a3));

			REQUIRE(Antecedent::Generic == solver.reason(~a2).type());
			LitVec r;
			solver.reason(~a2, r);
			REQUIRE(LitVec::size_type(3) == r.size());
			REQUIRE(std::find(r.begin(), r.end(), ~b1) != r.end());
			REQUIRE(std::find(r.begin(), r.end(), ~b2) != r.end());
			REQUIRE(std::find(r.begin(), r.end(), ~b3) != r.end());

			REQUIRE(lf->locked(solver));
		}

		SECTION("loopFormulaPropagateAtoms2") {
			solver.undoUntil(0);
			solver.assume(a1);
			solver.force(a2, 0);
			solver.propagate();
			solver.undoUntil(0);

			solver.assume(~b3);
			solver.propagate();
			solver.assume(~b1);
			solver.propagate();
			solver.assume(~b2);
			solver.propagate();

			REQUIRE(true == solver.isTrue(~a1));
			REQUIRE(true == solver.isTrue(~a2));
			REQUIRE(true == solver.isTrue(~a3));


			REQUIRE(Antecedent::Generic == solver.reason(~a1).type());
			LitVec r;
			solver.reason(~a1, r);
			REQUIRE(LitVec::size_type(3) == r.size());
			REQUIRE(std::find(r.begin(), r.end(), ~b1) != r.end());
			REQUIRE(std::find(r.begin(), r.end(), ~b2) != r.end());
			REQUIRE(std::find(r.begin(), r.end(), ~b3) != r.end());

			REQUIRE(lf->locked(solver));
		}

		SECTION("loopFormulaBodyConflict") {
			solver.undoUntil(0);

			solver.assume(~b3);
			solver.propagate();
			solver.assume(~b2);
			solver.propagate();
			solver.force(a3, 0);
			solver.force(~b1, 0);

			REQUIRE(false == solver.propagate());
			const LitVec& r = solver.conflict();

			REQUIRE(LitVec::size_type(4) == r.size());
			REQUIRE(std::find(r.begin(), r.end(), a3) != r.end());
			REQUIRE(std::find(r.begin(), r.end(), ~b3) != r.end());
			REQUIRE(std::find(r.begin(), r.end(), ~b1) != r.end());
			REQUIRE(std::find(r.begin(), r.end(), ~b2) != r.end());
		}
		SECTION("loopFormulaAtomConflict") {
			solver.undoUntil(0);
			solver.assume(~b3);
			solver.propagate();
			solver.assume(~b1);
			solver.propagate();
			solver.force(~b2, 0);
			solver.force(a2, 0);
			REQUIRE(false == solver.propagate());


			LitVec r = solver.conflict();

			REQUIRE(LitVec::size_type(4) == r.size());
			REQUIRE(std::find(r.begin(), r.end(), a2) != r.end());
			REQUIRE(std::find(r.begin(), r.end(), ~b3) != r.end());
			REQUIRE(std::find(r.begin(), r.end(), ~b1) != r.end());
			REQUIRE(std::find(r.begin(), r.end(), ~b2) != r.end());

			REQUIRE(true == solver.isTrue(~a1));
			solver.reason(~a1, r);
			REQUIRE(LitVec::size_type(3) == r.size());
			REQUIRE(std::find(r.begin(), r.end(), ~b3) != r.end());
			REQUIRE(std::find(r.begin(), r.end(), ~b1) != r.end());
			REQUIRE(std::find(r.begin(), r.end(), ~b2) != r.end());
		}

		SECTION("loopFormulaDontChangeSat") {
			solver.undoUntil(0);
			REQUIRE((solver.assume(~b1) && solver.propagate()));
			REQUIRE((solver.assume(~b3) && solver.propagate()));
			REQUIRE((solver.assume(a2) && solver.propagate()));

			REQUIRE(solver.isTrue(b2));
			LitVec rold;
			solver.reason(b2, rold);

			REQUIRE((solver.assume(a1) && solver.propagate()));
			REQUIRE(solver.isTrue(b2));
			LitVec rnew;
			solver.reason(b2, rnew);
			REQUIRE(rold == rnew);
		}

		SECTION("loopFormulaSatisfied") {
			ConstraintType t = Constraint_t::Loop;
			TypeSet ts, other; ts.addSet(t); other.addSet(Constraint_t::Conflict);
			LitVec free;
			REQUIRE(uint32(0) == lf->isOpen(solver, ts, free));
			solver.undoUntil(0);
			free.clear();
			REQUIRE(uint32(lf->type()) == lf->isOpen(solver, ts, free));
			REQUIRE(LitVec::size_type(6) == free.size());
			REQUIRE(uint32(0) == lf->isOpen(solver, other, free));
			solver.assume(a1);
			solver.assume(~b2);
			solver.propagate();
			free.clear();
			REQUIRE(uint32(lf->type()) == lf->isOpen(solver, ts, free));
			REQUIRE(LitVec::size_type(4) == free.size());
			solver.assume(~b1);
			solver.propagate();
			REQUIRE(uint32(0) == lf->isOpen(solver, ts, free));
		}

		SECTION("testLoopFormulaPropTrueAtomInSatClause") {
			solver.undoUntil(0);
			solver.assume(~a1);
			solver.propagate();

			solver.assume(a2);
			solver.force(~b3, 0);
			solver.force(~b2, 0);
			solver.propagate();

			REQUIRE(true == solver.isTrue(b1));
		}
	}
	SECTION("testLoopFormulaBugEq") {
		ctx.endInit();
		Literal body1 = b1;
		Literal body2 = b2;
		Literal body3 = ~b3; // assume body3 is equivalent to some literal ~xy
		solver.assume(~body1);
		solver.assume(~body2);
		solver.assume(~body3);
		solver.propagate();
		Literal lits[4] = {~a1, body3, body2, body1};

		LoopFormula* lf = LoopFormula::newLoopFormula(solver, ClauseRep::prepared(lits, 4), lits, 1);
		solver.addLearnt(lf, lf->size());
		solver.force(~a1, lf);
		solver.propagate();
		solver.undoUntil(solver.decisionLevel()-2);
		REQUIRE((solver.assume(~body3) && solver.propagate()));
		REQUIRE((solver.assume(a1) && solver.propagate()));
		REQUIRE(solver.isTrue(body2));
	}
}

TEST_CASE("Shared clause", "[core][constraint]") {
	typedef ConstraintInfo ClauseInfo;
	SharedContext ctx;
	LitVec clLits;
	for (int i = 1; i < 15; ++i) {
		ctx.addVar(Var_t::Atom);
	}
	ctx.startAddConstraints(10);
	Solver& solver = *ctx.master();
	SECTION("testClauseCtorAddsWatches") {
		makeLits(clLits, 2, 2);
		ClauseHead* sharedCl = createShared(solver, clLits, ClauseInfo());
		ctx.add(sharedCl);
		REQUIRE(2 == countWatches(solver, sharedCl, clLits));
	}

	SECTION("testPropSharedClauseConflict") {
		makeLits(clLits, 2, 2);
		ClauseHead* c = createShared(solver, clLits, ClauseInfo());
		solver.add(c);
		solver.assume(~clLits[0]);
		solver.propagate();
		solver.assume(~clLits.back());
		solver.propagate();
		solver.assume(~clLits[1]);
		solver.propagate();

		REQUIRE(solver.isTrue(clLits[2]));
		REQUIRE(c->locked(solver));
		Antecedent reason = solver.reason(clLits[2]);
		REQUIRE(reason == c);

		LitVec r;
		reason.reason(solver, clLits[2], r);
		REQUIRE(std::find(r.begin(), r.end(), ~clLits[0]) != r.end());
		REQUIRE(std::find(r.begin(), r.end(), ~clLits[1]) != r.end());
		REQUIRE(std::find(r.begin(), r.end(), ~clLits[3]) != r.end());
	}

	SECTION("testPropAlreadySatisfied") {
		ClauseHead* c1 = createShared(solver, makeLits(clLits, 2, 2), ClauseInfo());
		ctx.add(c1);

		// satisfy the clauses...
		ctx.addUnary(clLits[3]);
		solver.propagate();

		// ...make all but one literal false
		ctx.addUnary(~clLits[0]);
		ctx.addUnary(~clLits[1]);
		solver.propagate();

		// ...and assert that the last literal is still unassigned
		REQUIRE(value_free == solver.value(clLits[2].var()));
	}

	SECTION("testReasonBumpsActivityIfLearnt") {
		ctx.endInit();
		ClauseInfo e(Constraint_t::Conflict);
		ClauseHead* c = createShared(solver, makeLits(clLits, 4, 0), e);
		Solver& solver = *ctx.master();
		solver.addLearnt(c, (uint32)clLits.size());

		solver.assume(~clLits[0]);
		solver.propagate();
		solver.assume(~clLits[1]);
		solver.propagate();
		uint32 a = c->activity().activity();
		solver.assume(~clLits[2]);
		solver.force(~clLits[3], Antecedent(0));
		REQUIRE_FALSE(solver.propagate());
		REQUIRE(a+1 == c->activity().activity());
	}

	SECTION("testSimplifySAT") {
		ClauseHead* c1 = createShared(solver, makeLits(clLits, 3, 2), ClauseInfo());
		ctx.add(c1);

		ctx.addUnary(~clLits[4]);
		ctx.addUnary(clLits[3]);
		solver.propagate();

		REQUIRE(c1->simplify(*ctx.master(), false));
	}

	SECTION("testSimplifyUnique") {
		ClauseHead* c = createShared(solver, makeLits(clLits, 3, 3), ClauseInfo());
		ctx.add(c);

		ctx.addUnary(~clLits[2]);
		ctx.addUnary(~clLits[3]);
		solver.propagate();

		REQUIRE_FALSE(c->simplify(*ctx.master(), false));
		REQUIRE(4 == c->size());
		REQUIRE(2 == countWatches(*ctx.master(), c, clLits));
	}

	SECTION("testSimplifyShared") {
		makeLits(clLits, 3, 3);
		SharedLiterals* sLits = SharedLiterals::newShareable(clLits, Constraint_t::Conflict);
		REQUIRE((sLits->unique() && sLits->type() == Constraint_t::Conflict && sLits->size() == 6));
		SharedLiterals* other = sLits->share();
		REQUIRE(!sLits->unique());

		ctx.addUnary(~clLits[2]);
		ctx.addUnary(~clLits[3]);
		solver.propagate();

		REQUIRE(uint32(4) == sLits->simplify(*ctx.master()));
		REQUIRE(uint32(6) == sLits->size());
		sLits->release();
		other->release();
	}

	SECTION("testCloneShared") {
		ClauseHead* c = createShared(solver, makeLits(clLits, 3, 2), ClauseInfo());
		Solver& solver2 = ctx.pushSolver();
		ctx.endInit(true);
		ClauseHead* clone = (ClauseHead*)c->cloneAttach(solver2);
		LitVec lits;
		clone->toLits(lits);
		REQUIRE(lits == clLits);
		REQUIRE(countWatches(solver2, clone, clLits) == 2);
		c->destroy(ctx.master(), true);

		for (uint32 i = 0; i != clLits.size()-1; ++i) {
			solver2.assume(~clLits[i]);
			solver2.propagate();
		}
		REQUIRE(solver2.isTrue(clLits.back()));
		REQUIRE(solver2.reason(clLits.back()) == clone);
		clone->destroy(&solver2, true);
	}
}
} }
