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
namespace Clasp { namespace Test {
using namespace Clasp::mt;

TEST_CASE("ClauseCreator create", "[constraint][core]") {
	SharedContext ctx;
	ClauseCreator creator;
	Literal a, b, c, d, e;
	a = posLit(ctx.addVar(Var_t::Atom));
	b = posLit(ctx.addVar(Var_t::Atom));
	c = posLit(ctx.addVar(Var_t::Atom));
	d = posLit(ctx.addVar(Var_t::Atom));
	e = posLit(ctx.addVar(Var_t::Atom));
	creator.setSolver(*ctx.master());
	ctx.startAddConstraints(1);
	Solver& s = *ctx.master();
	SECTION("test empty clause is false") {
		REQUIRE_FALSE((bool)creator.start().end());
	}
	SECTION("test facts are asserted") {
		REQUIRE((bool)creator.start().add(a).end());
		REQUIRE(s.isTrue(a));
	}
	SECTION("test top level sat clauses are not added") {
		s.force(a, 0);
		REQUIRE((bool)creator.start().add(a).add(b).end());
		REQUIRE(0u == ctx.numConstraints());
	}
	SECTION("test top level false lits are removed") {
		s.force(~a, 0);
		REQUIRE((bool)creator.start().add(a).add(b).end());
		REQUIRE(0u == ctx.numConstraints());
		REQUIRE(s.isTrue(b));
	}
	SECTION("test add binary clause") {
		REQUIRE((bool)creator.start().add(a).add(b).end());
		REQUIRE(1u == ctx.numConstraints());
		REQUIRE(1u == ctx.numBinary());
	}
	SECTION("test add ternary clause") {
		REQUIRE((bool)creator.start().add(a).add(b).add(c).end());
		REQUIRE(1u == ctx.numConstraints());
		REQUIRE(1u == ctx.numTernary());
	}
	SECTION("test add generic clause") {
		REQUIRE((bool)creator.start().add(a).add(b).add(c).add(d).end());
		REQUIRE(1u == ctx.numConstraints());
	}
	SECTION("test creator acquires missing vars") {
		Literal f = posLit(ctx.addVar(Var_t::Atom));
		Literal g = posLit(ctx.addVar(Var_t::Atom));
		Literal h = posLit(ctx.addVar(Var_t::Atom));
		REQUIRE(!s.validVar(f.var()));
		REQUIRE(!s.validVar(g.var()));
		REQUIRE(!s.validVar(h.var()));
		REQUIRE((bool)creator.start().add(a).add(b).add(g).add(f).end());
		REQUIRE(1u == ctx.numConstraints());
		REQUIRE(s.validVar(f.var()));
		REQUIRE(s.validVar(g.var()));
		REQUIRE(s.validVar(h.var()));
	}
	SECTION("test creator asserts first literal") {
		ctx.endInit();
		s.assume(~b);
		s.assume(~c);
		s.assume(~d);
		s.propagate();
		REQUIRE((bool)creator.start(Constraint_t::Conflict).add(a).add(b).add(c).add(d).end());
		REQUIRE(s.isTrue(a));
		REQUIRE_FALSE(s.hasConflict());
		REQUIRE(1u == s.numLearntConstraints());
	}
	SECTION("test creator inits watches") {
		ctx.endInit();
		s.assume(~b);
		s.assume(~c);
		s.assume(~d);

		creator.start(Constraint_t::Conflict).add(a).add(b).add(d).add(c);
		REQUIRE((bool)creator.end());	// asserts a
		s.undoUntil(2);	// clear a and d
		s.assume(~d);		// hopefully d was watched.
		s.propagate();
		REQUIRE(value_true == s.value(a.var()));
	}
	SECTION("test add successive") {
		creator.start(Constraint_t::Conflict).add(a);
		s.assume(b) && s.propagate();
		creator.add(~b);
		s.assume(c) && s.propagate();
		creator.add(~c);
		s.assume(d) && s.propagate();
		creator.add(~d);
		creator.end();
		REQUIRE(creator[0] == a);
	}
	SECTION("test creator simplify") {
		SECTION("creator removes duplicate literals") {
			creator.start().add(a).add(b).add(c).add(a).add(b).add(d).prepare(true);
			REQUIRE(creator.size() == 4);
			REQUIRE(creator[0] == a);
			REQUIRE(creator[1] == b);
			REQUIRE(creator[2] == c);
			REQUIRE(creator[3] == d);
		}
		SECTION("creator detects tautologies") {
			creator.start().add(a).add(b).add(c).add(a).add(b).add(~a).end(ClauseCreator::clause_force_simplify);
			REQUIRE(0 == ctx.numConstraints());
		}
		SECTION("test creator moves watches") {
			ctx.endInit();
			s.assume(a) && s.propagate();
			s.assume(b) && s.propagate();
			creator.start(Constraint_t::Loop);
			creator.add(~a).add(~b).add(~b).add(~a).add(c).add(d).prepare(true);
			REQUIRE(creator.size() == 4);
			REQUIRE(c == creator[0]);
			REQUIRE(d == creator[1]);
		}
		SECTION("test regression") {
			LitVec clause;
			clause.push_back(lit_false());
			clause.push_back(a);
			clause.push_back(b);
			ClauseCreator::prepare(*ctx.master(), clause, ClauseCreator::clause_force_simplify);
			REQUIRE(clause.size() == 2);
		}
	}
	SECTION("test create non-asserting learnt clause") {
		ctx.endInit();
		s.assume(a); s.propagate();
		s.assume(b); s.propagate();
		creator.start(Constraint_t::Loop);
		creator.add(~a).add(~b).add(c).add(d);
		REQUIRE(creator.end());
		REQUIRE(c == creator[0]);
		REQUIRE(d == creator[1]);
		REQUIRE(s.numLearntConstraints() == 1);

		s.undoUntil(0);
		// test with a short clause
		s.assume(a); s.propagate();
		creator.start(Constraint_t::Loop);
		creator.add(~a).add(b).add(c);
		REQUIRE(creator.end());
		REQUIRE(b == creator[0]);
		REQUIRE(c == creator[1]);
		REQUIRE(ctx.numLearntShort() == 1);
	}
	SECTION("test create conflicting learnt clause") {
		ctx.endInit();
		s.assume(a); s.force(b, 0); s.propagate();// level 1
		s.assume(c); s.propagate();                     // level 2
		s.assume(d); s.propagate();                     // level 3
		s.assume(e); s.propagate();                     // level 4

		creator.start(Constraint_t::Loop);
		creator.add(~c).add(~a).add(~d).add(~b); // 2 1 3 1
		REQUIRE_FALSE((bool)creator.end());
		// make sure we watch highest levels, i.e. 3 and 2
		REQUIRE(~d == creator[0]);
		REQUIRE(~c == creator[1]);
		REQUIRE(s.numLearntConstraints() == 0);


		s.undoUntil(0);
		// test with a short clause
		s.assume(a); s.propagate();// level 1
		s.assume(c); s.propagate();// level 2
		creator.start(Constraint_t::Loop);
		creator.add(~a).add(~c);
		REQUIRE_FALSE((bool)creator.end());
		REQUIRE(~c == creator[0]);
		REQUIRE(~a == creator[1]);
		REQUIRE(ctx.numBinary() == 0);
	}

	SECTION("test create asserting learnt clause") {
		ctx.endInit();
		s.assume(a); s.force(b, 0); s.propagate();// level 1
		s.assume(c); s.propagate();                     // level 2
		creator.start(Constraint_t::Loop);
		creator.add(~c).add(~a).add(d).add(~b);                       // 2 1 Free 1
		REQUIRE((bool)creator.end());
		// make sure we watch the right lits, i.e. d (free) and ~c (highest DL)
		REQUIRE(d == creator[0]);
		REQUIRE(~c == creator[1]);
		REQUIRE(s.isTrue(d));
		REQUIRE(s.numLearntConstraints() == 1);
		// test with a short clause
		s.undoUntil(0);
		s.reduceLearnts(1.0f);
		s.assume(a); s.force(b, 0); s.propagate();// level 1
		s.assume(c); s.propagate();                     // level 2
		creator.start(Constraint_t::Loop);
		creator.add(~c).add(~a).add(d);                               // 2 1 Free
		REQUIRE((bool)creator.end());
		// make sure we watch the right lits, i.e. d (free) and ~c (highest DL)
		REQUIRE(d == creator[0]);
		REQUIRE(~c == creator[1]);
		REQUIRE(s.isTrue(d));
	}

	SECTION("test create bogus unit") {
		s.assume(a) && s.propagate();
		s.assume(~b) && s.propagate();
		s.force(~d, 0) && s.propagate();
		s.assume(~c) && s.propagate();
		REQUIRE(s.decisionLevel() == 3);

		creator.start(Constraint_t::Other).add(d).add(b).add(c).add(a);
		REQUIRE((ClauseCreator::status(s, &creator.lits()[0], &creator.lits()[0] + creator.size()) == ClauseCreator::status_sat));

		ClauseCreator::Result r = creator.end();
		REQUIRE(r.ok());
		REQUIRE(s.decisionLevel() == 3);
	}
	SECTION("test creator notifies heuristic") {
		struct FakeHeu : public SelectFirst {
			void newConstraint(const Solver&, const Literal*, LitVec::size_type size, ConstraintType t) {
				clSizes_.push_back(size);
				clTypes_.push_back(t);
			}
			std::vector<LitVec::size_type> clSizes_;
			std::vector<ConstraintType> clTypes_;
		}heu;
		s.setHeuristic(&heu, Ownership_t::Retain);
		REQUIRE((bool)creator.start().add(a).add(b).add(c).add(d).end());
		ctx.endInit();
		s.assume(a);
		s.assume(b);
		s.propagate();

		REQUIRE((bool)creator.start(Constraint_t::Conflict).add(c).add(~a).add(~b).end());

		REQUIRE((bool)creator.start(Constraint_t::Loop).add(c).add(~a).add(~b).end(0));

		REQUIRE(uint32(3) == (uint32)heu.clSizes_.size());
		REQUIRE(uint32(4) == (uint32)heu.clSizes_[0]);
		REQUIRE(uint32(3) == (uint32)heu.clSizes_[1]);
		REQUIRE(uint32(3) == (uint32)heu.clSizes_[2]);

		REQUIRE(Constraint_t::Static == heu.clTypes_[0]);
		REQUIRE(Constraint_t::Conflict == heu.clTypes_[1]);
		REQUIRE(Constraint_t::Loop == heu.clTypes_[2]);
	}
}
TEST_CASE("ClauseCreator integrate", "[constraint][core]") {
	SharedContext ctx;
	ClauseCreator creator;
	Literal a, b, c, d, e, f;
	a = posLit(ctx.addVar(Var_t::Atom));
	b = posLit(ctx.addVar(Var_t::Atom));
	c = posLit(ctx.addVar(Var_t::Atom));
	d = posLit(ctx.addVar(Var_t::Atom));
	e = posLit(ctx.addVar(Var_t::Atom));
	f = posLit(ctx.addVar(Var_t::Atom));
	creator.setSolver(*ctx.master());
	ctx.startAddConstraints(1);
	Solver& s = *ctx.master();
	LitVec cl;
	SECTION("test can't integrate empty clause") {
		s.assume(~a) && s.propagate();
		s.pushRootLevel(s.decisionLevel());
		SharedLiterals* p(SharedLiterals::newShareable(cl, Constraint_t::Other));
		ClauseCreator::Result r = ClauseCreator::integrate(s, p, 0);
		REQUIRE_FALSE(r.ok());
		REQUIRE(s.hasConflict());
		REQUIRE_FALSE(s.clearAssumptions());
	}
	SECTION("test integrate unit clause") {
		s.assume(a) && s.propagate();
		s.assume(b) && s.propagate();
		s.assume(c) && s.propagate();
		s.assume(d) && s.propagate();
		s.pushRootLevel(s.decisionLevel());
		s.assume(e) && s.propagate();

		// ~a ~b ~c f -> Unit: f@3
		cl.push_back(~a); cl.push_back(f);
		cl.push_back(~c); cl.push_back(~b);
		SharedLiterals* p(SharedLiterals::newShareable(cl, Constraint_t::Other));
		ClauseCreator::integrate(s, p, 0);
		REQUIRE(s.isTrue(f));
		REQUIRE(s.decisionLevel() == s.rootLevel());
		s.popRootLevel();
		s.backtrack() && s.propagate();
		REQUIRE(s.isTrue(f));
		REQUIRE(s.decisionLevel() == s.rootLevel());
		s.popRootLevel();
		s.backtrack() && s.propagate();
		REQUIRE_FALSE(s.isTrue(f));
		REQUIRE(s.value(c.var()) == value_free);
	}

	SECTION("test integrate sat unit clause") {
		s.assume(a) && s.propagate();
		cl.push_back(a);
		SharedLiterals* p(SharedLiterals::newShareable(cl, Constraint_t::Other));
		ClauseCreator::integrate(s, p, 0);
		REQUIRE(s.isTrue(a));
		REQUIRE(s.decisionLevel() == 0);
	}

	SECTION("test integrate conflicting clause") {
		s.assume(a) && s.propagate();
		s.assume(b) && s.propagate();
		s.assume(c) && s.propagate();
		s.force(d, 0) && s.propagate();

		// ~a ~b ~c ~d -> conflicting@3
		cl.push_back(~a); cl.push_back(~c); cl.push_back(~b); cl.push_back(~d);
		SharedLiterals* p(SharedLiterals::newShareable(cl, Constraint_t::Other));
		ClauseCreator::Result r = ClauseCreator::integrate(s, p, 0);
		REQUIRE(!r.ok());
		REQUIRE(r.local != 0);
		REQUIRE(s.hasConflict());
	}

	SECTION("test integrate asserting conflict") {
		s.assume(a) && s.propagate();
		s.assume(b) && s.propagate();
		s.assume(c) && s.propagate();
		s.assume(d) && s.propagate();

		// ~a ~b ~c -> Conflict @3
		cl.push_back(~a); cl.push_back(~c); cl.push_back(~b);
		SharedLiterals* p(SharedLiterals::newShareable(cl, Constraint_t::Other));
		ClauseCreator::integrate(s, p, 0);
		REQUIRE(s.decisionLevel() == uint32(2));
	}
	SECTION("test integrate asserting conflict below root") {
		s.assume(a) && s.propagate();
		s.assume(b) && s.propagate();
		s.assume(d) && s.propagate();
		s.pushRootLevel(s.decisionLevel());
		s.assume(c) && s.propagate();
		// ~a ~b ~c -> Conflict @3, Asserting @2
		cl.push_back(~a); cl.push_back(~c); cl.push_back(~b);
		SharedLiterals* p(SharedLiterals::newShareable(cl, Constraint_t::Other));
		ClauseCreator::integrate(s, p, 0);
		REQUIRE(s.decisionLevel() == uint32(3));
		s.popRootLevel();
		s.backtrack() && s.propagate();
		REQUIRE(s.isTrue(~c));
	}
	SECTION("test integrate conflict below root()") {
		cl.push_back(a); cl.push_back(b); cl.push_back(c);
		s.assume(~a) && s.propagate();
		s.assume(~b) && s.propagate();
		s.assume(~c) && s.propagate();
		s.assume(~d) && s.propagate();
		s.pushRootLevel(s.decisionLevel());
		SharedLiterals* p(SharedLiterals::newShareable(cl, Constraint_t::Other));
		ClauseCreator::Result r = ClauseCreator::integrate(s, p, ClauseCreator::clause_explicit);
		REQUIRE_FALSE(r.ok());
		REQUIRE(uint32(1) == s.numLearntConstraints());
	}
	SECTION("test init watches") {
		cl.push_back(a);
		cl.push_back(b);
		cl.push_back(c);
		cl.push_back(d);
		s.assume(~b)   && s.propagate();
		s.force(~c, 0) && s.propagate();
		s.assume(~a)   && s.propagate();
		s.assume(d)    && s.propagate();
		// aF@2 bF@1 cF@1 dT@3 -> dT@3 aF@2 cF@1 bF@1
		LitVec temp = cl;
		ClauseCreator::prepare(s, temp, 0);
		REQUIRE(temp[0] == d);
		REQUIRE(temp[1] == a);

		SharedLiterals* p(SharedLiterals::newShareable(cl, Constraint_t::Other));
		ClauseCreator::Result r = ClauseCreator::integrate(s, p, ClauseCreator::clause_no_add);
		temp.clear();
		r.local->clause()->toLits(temp);
		REQUIRE(temp[0] == d);
		REQUIRE(temp[1] == a);
		r.local->destroy(&s, true);
	}
	SECTION("test integrate unsat") {
		s.force(~a, 0) && s.propagate();
		s.assume(b);
		cl.push_back(a);
		SharedLiterals* p(SharedLiterals::newShareable(cl, Constraint_t::Other));
		ClauseCreator::Result r = ClauseCreator::integrate(s, p, 0);
		REQUIRE_FALSE(r.ok());
		REQUIRE(0 == r.local);
		REQUIRE(uint32(0) == s.decisionLevel());
	}
	SECTION("test integrate sat") {
		cl.push_back(d); cl.push_back(b); cl.push_back(a); cl.push_back(c);
		SECTION("test1") {
			s.assume(~a) && s.propagate();
			s.assume(b) && s.propagate();
			s.assume(~d) && s.propagate();
			do {
				SharedLiterals* p(SharedLiterals::newShareable(cl, Constraint_t::Other));
				ClauseCreator::Result r = ClauseCreator::integrate(s, p, ClauseCreator::clause_not_sat);
				REQUIRE(r.ok());
				REQUIRE(s.numAssignedVars() == 3);
			} while (std::next_permutation(cl.begin(), cl.end()));
			REQUIRE(s.numLearntConstraints() == 0);
		}
		SECTION("bug1") {
			s.assume(~a) && s.propagate();
			s.assume(b) && s.propagate();
			s.assume(~d) && s.propagate();
			SharedLiterals* p(SharedLiterals::newShareable(cl, Constraint_t::Other));
			ClauseCreator::Result r = ClauseCreator::integrate(s, p, 0);
			REQUIRE(r.ok());
			REQUIRE(3u == s.numAssignedVars());
		}
		SECTION("bug2") {
			s.assume(~a) && s.propagate();
			s.assume(b) && s.propagate();
			s.assume(~d) && s.propagate();
			s.force(c, 0) && s.propagate();
			SharedLiterals* p(SharedLiterals::newShareable(cl, Constraint_t::Other));
			ClauseCreator::Result r = ClauseCreator::integrate(s, p, 0);
			REQUIRE(r.ok());
		}
		SECTION("bug3") {
			s.assume(~a) && s.propagate();
			s.assume(~b) && s.propagate();
			s.force(~d, 0) && s.propagate();
			s.assume(c) && s.propagate();
			REQUIRE(s.decisionLevel() == 3);
			SharedLiterals* p(SharedLiterals::newShareable(cl, Constraint_t::Other));
			ClauseCreator::Result r = ClauseCreator::integrate(s, p, ClauseCreator::clause_not_sat);
			REQUIRE(r.ok());
			REQUIRE(s.decisionLevel() == 2);
			REQUIRE(s.isTrue(c));
		}
		SECTION("bug4") {
			cl.clear();  cl.push_back(a); cl.push_back(b);
			s.force(~a, 0);
			s.assume(b);
			SharedLiterals* p(SharedLiterals::newShareable(cl, Constraint_t::Other));
			REQUIRE((ClauseCreator::status(s, p->begin(), p->end()) & ClauseCreator::status_unit) != 0);
			ClauseCreator::integrate(s, p, ClauseCreator::clause_explicit);
		}
	}
	SECTION("test integrate known order") {
		cl.push_back(a);
		SharedLiterals* p(SharedLiterals::newShareable(cl, Constraint_t::Other));
		REQUIRE((ClauseCreator::status(s, p->begin(), p->end()) & ClauseCreator::status_unit) != 0);
		ClauseCreator::integrate(s, p, ClauseCreator::clause_no_prepare);
		REQUIRE(s.isTrue(a));
	}
	SECTION("test integrate not conflicting") {
		cl.push_back(a); cl.push_back(b);
		SharedLiterals* p(SharedLiterals::newShareable(cl, Constraint_t::Other));
		s.assume(~a) && s.propagate();
		s.force(~b, 0)&& s.propagate();
		REQUIRE((ClauseCreator::status(s, p->begin(), p->end()) & ClauseCreator::status_unsat) != 0);
		ClauseCreator::Result r = ClauseCreator::integrate(s, p, ClauseCreator::clause_not_conflict);
		REQUIRE(r.ok() == false);
		REQUIRE(r.local == 0);
	}
	SECTION("test integrate asserting below BT") {
		s.assume(a) && s.propagate();
		s.assume(b) && s.propagate();
		s.assume(c) && s.propagate();
		s.assume(d) && s.propagate();
		s.setBacktrackLevel(s.decisionLevel());
		// ~a ~b ~c -> Conflict @3, Asserting @2
		cl.push_back(~a); cl.push_back(~c); cl.push_back(~b);
		SharedLiterals* p(SharedLiterals::newShareable(cl, Constraint_t::Other));
		ClauseCreator::Result r = ClauseCreator::integrate(s, p, 0);
		REQUIRE_FALSE(r.ok());
		s.backtrack();
		REQUIRE(s.isTrue(~c));
		REQUIRE(uint32(2) == s.decisionLevel());
		s.backtrack();
		REQUIRE(!s.isTrue(~c));
	}
	SECTION("test integrate conflict below BT") {
		s.assume(a) && s.propagate();
		s.assume(b) && s.force(c, 0) && s.propagate();
		s.assume(d) && s.propagate();
		s.assume(e) && s.propagate();
		s.setBacktrackLevel(s.decisionLevel());
		// ~a ~b ~c -> Conflict @2
		cl.push_back(~a); cl.push_back(~c); cl.push_back(~b);
		SharedLiterals* p(SharedLiterals::newShareable(cl, Constraint_t::Other));
		ClauseCreator::Result r = ClauseCreator::integrate(s, p, 0);
		REQUIRE_FALSE(r.ok());
		REQUIRE(s.resolveConflict());
		REQUIRE(uint32(1) == s.decisionLevel());
	}
	SECTION("test simplify") {
		SECTION("test1") {
			cl.push_back(a); cl.push_back(b); cl.push_back(c);
			cl.push_back(d); cl.push_back(e); cl.push_back(f);
			SharedLiterals* p(SharedLiterals::newShareable(cl, Constraint_t::Other));
			s.force(~d, 0) && s.propagate();
			s.assume(~a)   && s.propagate();
			ClauseCreator::Result r = ClauseCreator::integrate(s, p, ClauseCreator::clause_no_add);
			REQUIRE(r.ok());
			REQUIRE(r.local != 0);
			cl.clear();
			r.local->toLits(cl);
			REQUIRE(cl.size() == 5);
			REQUIRE(std::find(cl.begin(), cl.end(), d) == cl.end());
		}
		SECTION("test facts are removed from learnt") {
			ctx.enableStats(1);
			ctx.addUnary(a);
			ctx.endInit();
			s.assume(~b) && s.propagate();
			creator.start(Constraint_t::Conflict);
			creator.add(b).add(c).add(~a).end();

			REQUIRE(1u == ctx.numLearntShort());
			REQUIRE(s.stats.extra->lits[0] == 2);
		}
	}
}

} }
