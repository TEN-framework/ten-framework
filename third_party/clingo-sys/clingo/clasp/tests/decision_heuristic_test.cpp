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
#include <clasp/heuristics.h>
#include <clasp/lookahead.h>
#include <clasp/logic_program.h>
#include <clasp/clause.h>
#include <clasp/solver.h>
#include "lpcompare.h"
#include "catch.hpp"
namespace Clasp { namespace Test {
using namespace Clasp::Asp;
/////////////////////////////////////////////////////////////////////////////////////////
// Lookahead
/////////////////////////////////////////////////////////////////////////////////////////
TEST_CASE("Lookahead", "[heuristic][pp]") {
	SharedContext ctx;
	LogicProgram lp;
	SECTION("body lookahead") {
		Solver& s = ctx.pushSolver();
		lpAdd(lp.start(ctx, LogicProgram::AspOptions().noEq().noScc()),
			"x1 :- not x1.\n"
			"x1 :- not x2, not x5.\n"
			"x2 :- not x5.\n"
			"x5 :- not x2.\n"
			"x1 :- not x3, not x6.\n"
			"x3 :- not x6.\n"
			"x6 :- not x3.\n"
			"x1 :- not x4, not x7.\n"
			"x4 :- not x7.\n"
			"x7 :- not x4.\n");

		REQUIRE((lp.endProgram() && ctx.endInit()) == true);
		s.addPost(new Lookahead(Lookahead::Params(Var_t::Body).addImps(false)));
		REQUIRE_FALSE(ctx.attach(s));
		ctx.detach(s, true);
		s.addPost(new Lookahead(Lookahead::Params(Var_t::Atom).addImps(false)));
		REQUIRE(ctx.attach(s));
		ctx.detach(s, true);
		s.addPost(new Lookahead(Lookahead::Params(Var_t::Hybrid).addImps(false)));
		REQUIRE_FALSE(ctx.attach(s));
	}
	SECTION("atom lookahead") {
		Solver& s = ctx.pushSolver();
		lpAdd(lp.start(ctx, LogicProgram::AspOptions().noEq().noScc()),
			"x1 :- x2, x3, x4, not x1.\n"
			"x2 :- not x5.\n"
			"x2 :- not x8.\n"
			"x5 :- not x8.\n"
			"x8 :- not x5.\n"
			"x3 :- not x6.\n"
			"x3 :- not x9.\n"
			"x6 :- not x9.\n"
			"x9 :- not x6.\n"
			"x4 :- not x7.\n"
			"x4 :- not x10.\n"
			"x7 :- not x10.\n"
			"x10 :- not x7.\n");

		REQUIRE((lp.endProgram() && ctx.endInit()) == true);
		Lookahead::Params p; p.addImps(false);
		s.addPost(new Lookahead(p.lookahead(Var_t::Body)));
		REQUIRE(ctx.attach(s));
		ctx.detach(s, true);
		s.addPost(new Lookahead(p.lookahead(Var_t::Atom)));
		REQUIRE_FALSE(ctx.attach(s));
		ctx.detach(s, true);
		s.addPost(new Lookahead(p.lookahead(Var_t::Hybrid)));
		REQUIRE_FALSE(ctx.attach(s));
	}

	SECTION("test lookahead bug: ") {
		UnitHeuristic unit;
		ctx.master()->setHeuristic(&unit, Ownership_t::Retain);
		Literal a = posLit(ctx.addVar(Var_t::Atom));
		Literal b = posLit(ctx.addVar(Var_t::Atom));
		Literal c = posLit(ctx.addVar(Var_t::Atom));
		Literal e = posLit(ctx.addVar(Var_t::Atom));
		Literal f = posLit(ctx.addVar(Var_t::Atom));
		Solver& s = ctx.startAddConstraints(5);
		SECTION("simplify") {
			ctx.addBinary(a, b);
			s.addPost(new Lookahead(Var_t::Atom));
			ctx.endInit();
			ctx.addBinary(a, ~b);
			s.assume(e) && s.propagate();
			REQUIRE(unit.select(s));
			REQUIRE(s.isTrue(a));
			REQUIRE(s.seen(a.var()));
			REQUIRE(s.decisionLevel() == 1);
		}
		SECTION("deps are cleared") {
			ctx.addBinary(a, b);
			ctx.addBinary(b, c);
			ctx.addBinary(c, f);
			ctx.addUnary(e);
			s.addPost(new Lookahead(Var_t::Atom));
			ctx.endInit();
			// Assume without using lookahead (e.g. a random choice)
			s.assume(b);
			s.propagate();
			// Deps not cleared
			REQUIRE(unit.select(s));
			REQUIRE((s.isFalse(c) || s.isFalse(f)));
		}
		SECTION("no deps") {
			s.addPost(new Lookahead(Var_t::Atom));
			ctx.addBinary(a, b);
			ctx.addBinary(b, c);
			ctx.addUnary(e);
			ctx.endInit();
			REQUIRE(unit.select(s));
			REQUIRE(s.isFalse(b));
			s.undoUntil(0);
			s.simplify();
			REQUIRE(unit.select(s));
			REQUIRE(s.isFalse(b));
		}
		SECTION("no nant") {
			Clasp::Lookahead::Params p(Var_t::Atom);
			p.restrictNant = true;
			s.addPost(new Lookahead(p));
			ctx.addBinary(a, b);
			ctx.addBinary(b, c);
			ctx.addBinary(~a, ~b);
			ctx.addBinary(~b, ~c);
			ctx.endInit();
			uint32 n = s.numFreeVars();
			REQUIRE((unit.select(s) && s.numFreeVars() != n));
		}
		SECTION("stop conflict") {
			ctx.addBinary(a, b);
			s.addPost(new Lookahead(Var_t::Atom));
			ctx.endInit();
			struct StopConflict : public PostPropagator {
				bool propagateFixpoint(Solver& s, PostPropagator*) { s.setStopConflict(); return false; }
				uint32 priority() const { return PostPropagator::priority_class_simple; }
			}*x = new StopConflict;
			s.addPost(x);
			REQUIRE((!unit.select(s) && s.hasConflict()));
			REQUIRE(s.search(0, 0) == value_false);
		}
	}
	SECTION("test strange seq") {
		Lookahead::Params p(Var_t::Body); p.limit(3);
		ctx.master()->setHeuristic(UnitHeuristic::restricted(new SelectFirst), Ownership_t::Acquire);
		Literal a = posLit(ctx.addVar(Var_t::Body));
		Literal b = posLit(ctx.addVar(Var_t::Atom));
		Solver& s = ctx.startAddConstraints();
		s.addPost(new Lookahead(p));
		ctx.endInit();
		s.force(a);
		s.simplify();
		bool x = s.decideNextBranch();
		REQUIRE((x == true && s.value(b.var()) != value_free));
	}
	SECTION("test strange seq2") {
		Lookahead::Params p(Var_t::Atom); p.limit(2);
		ctx.master()->setHeuristic(UnitHeuristic::restricted(new SelectFirst), Ownership_t::Acquire);
		Literal a = posLit(ctx.addVar(Var_t::Atom));
		Literal b = posLit(ctx.addVar(Var_t::Atom));
		Solver& s = ctx.startAddConstraints();
		s.addPost(new Lookahead(p));
		ctx.addBinary(a, b);
		ctx.addBinary(a, ~b);
		ctx.addBinary(~a, b);
		ctx.endInit();
		bool x = ctx.master()->decideNextBranch();
		REQUIRE((x == false && s.decisionLevel() == 0 && s.numFreeVars() == 0));
	}
	SECTION("test restricted heuristic is detached") {
		Lookahead::Params p(Var_t::Atom); p.limit(3);
		ctx.master()->setHeuristic(UnitHeuristic::restricted(new SelectFirst), Ownership_t::Acquire);
		Literal a = posLit(ctx.addVar(Var_t::Atom));
		Literal b = posLit(ctx.addVar(Var_t::Atom));
		posLit(ctx.addVar(Var_t::Atom));
		posLit(ctx.addVar(Var_t::Atom));
		Solver& s = ctx.startAddConstraints();
		s.addPost(new Lookahead(p));
		ctx.addBinary(a, b);
		ctx.endInit();
		bool x = ctx.master()->decideNextBranch();
		REQUIRE((x == true && s.decisionLevel() == 1));
		s.propagate();
		REQUIRE(s.getPost(PostPropagator::priority_reserved_look) != 0);
		s.setHeuristic(new SelectFirst, Ownership_t::Acquire);
		while (s.getPost(PostPropagator::priority_reserved_look) != 0) {
			s.propagate();
			s.decideNextBranch();
		}
	}
}
/////////////////////////////////////////////////////////////////////////////////////////
// Lookback
/////////////////////////////////////////////////////////////////////////////////////////
TEST_CASE("Lookback heuristics", "[heuristic]") {
	SharedContext ctx;
	SECTION("test berkmin") {
		ClaspBerkmin berkmin;
		ctx.master()->setHeuristic(&berkmin, Ownership_t::Retain);
		Literal a = posLit(ctx.addVar(Var_t::Atom));
		Literal b = posLit(ctx.addVar(Var_t::Atom));
		Literal c = posLit(ctx.addVar(Var_t::Atom));
		Literal d = posLit(ctx.addVar(Var_t::Atom));
		Literal e = posLit(ctx.addVar(Var_t::Atom));
		Literal f = posLit(ctx.addVar(Var_t::Atom));
		Literal g = posLit(ctx.addVar(Var_t::Atom));
		Solver& s = ctx.startAddConstraints();
		ctx.endInit();
		s.stats.conflicts = 1;
		LitVec up;
		berkmin.updateReason(s, up, Literal());
		up.push_back(a);
		berkmin.updateReason(s, up, a);
		up.clear();
		up.push_back(b);
		up.push_back(b);
		berkmin.updateReason(s, up, b);
		up.clear();
		berkmin.updateReason(s, up, e);
		s.assume(~b);
		s.assume(~c);
		s.assume(~d);
		ClauseCreator cc(&s);
		cc.start(Constraint_t::Conflict).add(a).add(b).add(c).add(d).end();
		s.undoUntil(0);
		s.assume(~e);
		s.assume(~f);
		s.assume(~g);
		cc.start(Constraint_t::Loop).add(d).add(e).add(f).add(g).end();
		s.undoUntil(0);
		REQUIRE(0u == s.numAssignedVars());
		REQUIRE(berkmin.select(s));
		REQUIRE(b == s.trail().back());	// from conflict clause
		s.propagate();
		REQUIRE(berkmin.select(s));
		REQUIRE(e == s.trail().back());	// from loop clause
		s.propagate();
		REQUIRE(berkmin.select(s));
		REQUIRE(a.var() == s.trail().back().var());	// highest activity
	}
	SECTION("test vmtf") {
		ClaspVmtf vmtf;
		ctx.master()->setHeuristic(&vmtf, Ownership_t::Retain);
		ctx.addVar(Var_t::Atom);
		ctx.addVar(Var_t::Atom);
		Solver& s = ctx.startAddConstraints();
		ctx.endInit();
		REQUIRE(vmtf.select(s));
		s.propagate();
		REQUIRE(vmtf.select(s));
		s.propagate();
		REQUIRE_FALSE(vmtf.select(s));
	}
	SECTION("test vsids") {
		ClaspVsids vsids;
		ctx.master()->setHeuristic(&vsids, Ownership_t::Retain);
		Literal a = posLit(ctx.addVar(Var_t::Atom));
		Literal b = posLit(ctx.addVar(Var_t::Atom));
		Solver& s = ctx.startAddConstraints();
		ctx.endInit();
		LitVec up;
		up.push_back(a);
		vsids.updateReason(s, up, a);
		REQUIRE(vsids.select(s));
		REQUIRE((s.trail().back() == ~a && s.propagate()));
		REQUIRE(vsids.select(s));
		REQUIRE((s.trail().back() == ~b && s.propagate()));
		REQUIRE_FALSE(vsids.select(s));
	}
	SECTION("test vsids aux") {
		ClaspVsids vsids;
		ctx.master()->setHeuristic(&vsids, Ownership_t::Retain);
		ctx.addVar(Var_t::Atom);
		ctx.addVar(Var_t::Atom);
		Solver& s = ctx.startAddConstraints();
		ctx.endInit();
		Var v = s.pushAuxVar();
		LitVec up;
		vsids.updateReason(s, up, posLit(v));
		REQUIRE(vsids.select(s));
		REQUIRE(s.value(v) != value_free);
		s.popAuxVar(1);
		REQUIRE(vsids.select(s));
		REQUIRE(s.trail().back().var() != v);
	}
}
/////////////////////////////////////////////////////////////////////////////////////////
// Domain heuristic
/////////////////////////////////////////////////////////////////////////////////////////
inline Literal getDomLiteral(const LogicProgram& lp, Var a) {
	return lp.getLiteral((a), MapLit_t::Refined);
}
TEST_CASE("Domain Heuristic", "[heuristic][asp]") {
	SharedContext ctx;
	LogicProgram lp;
	DomainHeuristic* dom;
	ctx.master()->setHeuristic((dom = new DomainHeuristic), Ownership_t::Acquire);
	Solver& s = *ctx.master();
	SECTION("test sign") {
		Var a = 1;
		lp.start(ctx).addRule(Head_t::Choice, Potassco::toSpan(&a, 1), Potassco::toSpan<Potassco::Lit_t>());
		SECTION("pos") {
			lp.addDomHeuristic(a, DomModType::Sign, 1, 1);
			REQUIRE((lp.endProgram() && ctx.endInit()));
			REQUIRE(s.decideNextBranch());
			REQUIRE(s.isTrue(lp.getLiteral(a)));
		}
		SECTION("neg") {
			lp.addDomHeuristic(a, DomModType::Sign, -1, 1);
			REQUIRE((lp.endProgram() && ctx.endInit()));
			REQUIRE(s.decideNextBranch());
			REQUIRE(s.isTrue(~lp.getLiteral(a)));
		}
		SECTION("inv") {
			lpAdd(lp.start(ctx), "a :- not b.\n"
				"b :- not a.\n");
			lp.addDomHeuristic(a, DomModType::Sign, 1, 1);
			REQUIRE((lp.endProgram() && ctx.endInit()));
			REQUIRE(s.decideNextBranch());
			REQUIRE(s.isTrue(lp.getLiteral(a)));
		}
	}
	SECTION("test level") {
		Var a = 1, b = 2;
		lpAdd(lp.start(ctx), "{a;b}.");
		lp.addDomHeuristic(a, DomModType::Sign, 1, 1);
		lp.addDomHeuristic(b, DomModType::Sign, 1, 1);
		lp.addDomHeuristic(a, DomModType::Level, 10, 10);
		REQUIRE((lp.endProgram() && ctx.endInit()));

		REQUIRE(s.decideNextBranch());
		REQUIRE(s.isTrue(lp.getLiteral(a)));
	}
	SECTION("test dynamic rules") {
		Var a = 1, b = 2, c = 3;
		lpAdd(lp.start(ctx), "{a;b;c}.\n"
			"d :- a, b.\n"
			":- d.\n");

		lp.addDomHeuristic(a, DomModType::Sign, 1, 1);
		lp.addDomHeuristic(b, DomModType::Sign, 1, 1);
		lp.addDomHeuristic(a, DomModType::Level, 10, 10);
		lp.addDomHeuristic(c, DomModType::Sign, 1, 1, b);
		lp.addDomHeuristic(c, DomModType::Sign, -1, 1, Potassco::neg(b));

		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE(s.decideNextBranch());
		REQUIRE(s.isTrue(lp.getLiteral(a)));
		s.propagate();
		REQUIRE(s.isFalse(lp.getLiteral(b)));

		REQUIRE(s.decideNextBranch());
		REQUIRE(s.isTrue(~lp.getLiteral(c)));

		s.clearAssumptions();
		uint32 n = s.numWatches(posLit(2));
		// test removal of watches
		ctx.master()->setHeuristic(new SelectFirst(), Ownership_t::Acquire);
		REQUIRE(s.numWatches(posLit(2)) != n);
	}
	SECTION("test priority") {
		Var a = 1, b = 2, c = 3;
		SECTION("test 1") {
			lpAdd(lp.start(ctx), "{a;b;c}.\n"
				"d :- a, b.\n"
				":- d.\n"
				"#heuristic b.         [1@1,sign]\n"
				"#heuristic a.         [10@10,true]\n"
				"#heuristic c.         [1@10,sign]\n"
				"#heuristic c : not b. [-1@20,sign]\n");
			REQUIRE((lp.endProgram() && ctx.endInit()));
			REQUIRE(s.decideNextBranch());
			REQUIRE(s.isTrue(lp.getLiteral(a)));
			s.propagate();
			REQUIRE(s.isFalse(lp.getLiteral(b)));

			REQUIRE(s.decideNextBranch());
			REQUIRE(s.isTrue(~lp.getLiteral(c)));
		}
		SECTION("test 2") {
			lpAdd(lp.start(ctx), "{a;b;c}.\n"
				"d :- a, b.\n"
				":- d.\n"
				"#heuristic b.         [1@1,sign]\n"
				"#heuristic a.         [10@10,true]\n"
				"#heuristic c.         [1@30,sign]\n"
				"#heuristic c : not b. [-1@20,sign]\n");
			REQUIRE((lp.endProgram() && ctx.endInit()));
			REQUIRE(s.decideNextBranch());
			REQUIRE(s.isTrue(lp.getLiteral(a)));
			s.propagate();
			REQUIRE(s.isFalse(lp.getLiteral(b)));
			REQUIRE(s.decideNextBranch());
			REQUIRE(s.isTrue(lp.getLiteral(c)));
		}
		SECTION("test 3") {
			lpAdd(lp.start(ctx), "{a;c}.\n"
				"b :- a.\n"
				"#heuristic a.     [1@30,true]\n"
				"#heuristic a.     [1@20,false]\n"
				"#heuristic b.     [2@10,true]\n"
				"#heuristic b : c. [2@25,false]\n");
			REQUIRE((lp.endProgram() && ctx.endInit()));
			s.assume(lp.getLiteral(c)) && s.propagate();
			REQUIRE(s.decideNextBranch());
			REQUIRE(s.isTrue(~getDomLiteral(lp, b)));
		}
	}
	SECTION("test init") {
		Var a = 1, b = 2;
		lpAdd(lp.start(ctx), "{a;b}.\n"
			"#heuristic a. [10@20,init]\n"
			"#heuristic a. [50@10,init]\n"
			"#heuristic b. [10@10,init]\n"
			"#heuristic b. [30@20,init]\n");
		REQUIRE(lp.endProgram());
		ctx.heuristic.add(lp.getLiteral(a).var(), DomModType::Init, 21, 20, lit_true());
		ctx.endInit();
		REQUIRE(s.decideNextBranch());
		REQUIRE(s.value(lp.getLiteral(b).var()) != value_free);
	}

	SECTION("test incremental") {
		lp.start(ctx).updateProgram();
		SECTION("test simple") {
			Var a = 1, b = 2, c = 3, d = 4, e = 5;
			ctx.master()->setHeuristic(new SelectFirst(), Ownership_t::Acquire);
			lpAdd(lp, "{a;b;c;d}.\n");
			lp.addDomHeuristic(a, DomModType::Level, 1, 1, c);
			lp.addDomHeuristic(b, DomModType::Level, 1, 1, d);
			REQUIRE((lp.endProgram() && ctx.endInit()));
			uint32 n = s.numWatches(posLit(c));
			ctx.master()->setHeuristic((dom = new DomainHeuristic), Ownership_t::Acquire);
			dom->startInit(s);
			dom->endInit(s);
			REQUIRE(s.numWatches(posLit(c)) > n);
			REQUIRE(lp.updateProgram());
			lpAdd(lp, "{e}.");
			lp.addDomHeuristic(e, DomModType::Level, 1, 1, c);
			REQUIRE((lp.endProgram() && ctx.endInit()));
			s.setHeuristic(new SelectFirst(), Ownership_t::Acquire);
			REQUIRE(s.numWatches(posLit(c)) == n);
		}
		SECTION("test increase priority") {
			lpAdd(lp, "{a}.");
			Var a = 1, b = 2;
			lp.addDomHeuristic(a, DomModType::False, 3, 3);
			REQUIRE((lp.endProgram() && ctx.endInit()));
			REQUIRE((s.decideNextBranch() && s.isFalse(lp.getLiteral(a))));
			s.undoUntil(0);
			REQUIRE(lp.updateProgram());
			lpAdd(lp, "{b}.\n");
			lp.addDomHeuristic(a, DomModType::False, 1, 1);
			lp.addDomHeuristic(b, DomModType::False, 2, 2);
			REQUIRE((lp.endProgram() && ctx.endInit()));
			REQUIRE((s.decideNextBranch() && s.isFalse(lp.getLiteral(a))));
		}
		SECTION("test increase dynamic priority") {
			Var a = 1, b = 2;
			lpAdd(lp, "{a;b}.");
			lp.addDomHeuristic(a, DomModType::True, 1, 10);
			lp.addDomHeuristic(a, DomModType::False, 1, 20, b);
			REQUIRE((lp.endProgram() && ctx.endInit()));
			REQUIRE((s.decideNextBranch() && s.isTrue(lp.getLiteral(a))));
			s.undoUntil(0);

			REQUIRE(lp.updateProgram());
			lp.addDomHeuristic(a, DomModType::True, 1, 30);
			lp.addDomHeuristic(b, DomModType::True, 2, 2);
			REQUIRE((lp.endProgram() && ctx.endInit()));
			REQUIRE((s.decideNextBranch() && s.isTrue(lp.getLiteral(b))));
			REQUIRE(s.propagate());
			REQUIRE((s.decideNextBranch() && s.isTrue(lp.getLiteral(a))));
		}
		SECTION("test reinit") {
			Var b = 2;
			lpAdd(lp, "{a;b}.");
			lp.addDomHeuristic(b, DomModType::Level, 1, 1);
			REQUIRE((lp.endProgram() && ctx.endInit()));
			REQUIRE((s.decideNextBranch() && s.value(lp.getLiteral(b).var()) != value_free));
			REQUIRE(lp.updateProgram());
			ctx.master()->setHeuristic(new DomainHeuristic, Ownership_t::Acquire);
			lpAdd(lp, "{c}.");
			REQUIRE((lp.endProgram() && ctx.endInit()));

			REQUIRE((s.decideNextBranch() && s.value(lp.getLiteral(b).var()) != value_free));
		}
		SECTION("test init modifier") {
			Var a = 1, b = 2;
			lpAdd(lp, "{a;b;c}."
				"#heuristic a. [10@10,init]\n"
				"#heuristic b. [20@20,init]\n");

			REQUIRE((lp.endProgram() && ctx.endInit()));
			REQUIRE(lp.updateProgram());
			lpAdd(lp,
				"#heuristic a. [30@30,init]\n"
				"#heuristic b. [10@10,init]\n");
			REQUIRE((lp.endProgram() && ctx.endInit()));
			REQUIRE(dom->score(lp.getLiteral(a).var()).value == 40.0);
			REQUIRE(dom->score(lp.getLiteral(b).var()).value == 20.0);

			REQUIRE(lp.updateProgram());
			ctx.master()->setHeuristic(dom = new DomainHeuristic, Ownership_t::Acquire);
			REQUIRE((lp.endProgram() && ctx.endInit()));
			REQUIRE(dom->score(lp.getLiteral(a).var()).value == 30.0);
			REQUIRE(dom->score(lp.getLiteral(b).var()).value == 20.0);
		}
	}
	SECTION("test min bug") {
		Var a = 1, b = 2;
		lpAdd(lp.start(ctx), "a :- not b. b :- not a.");
		lp.addDomHeuristic(a, DomModType::False, 1, 1);
		lp.addDomHeuristic(b, DomModType::False, 1, 1);
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE((s.pref(lp.getLiteral(a, MapLit_t::Refined).var()).get(ValueSet::user_value) == value_false));
		REQUIRE((s.pref(lp.getLiteral(b, MapLit_t::Refined).var()).get(ValueSet::user_value) == value_false));
	}
	SECTION("test default modification") {
		Var v1 = ctx.addVar(Var_t::Atom);
		Var v2 = ctx.addVar(Var_t::Body);
		ctx.startAddConstraints();
		dom->setDefaultMod(HeuParams::mod_level, 0);
		ctx.endInit();
		REQUIRE(dom->score(v1).level == 1);
		REQUIRE(dom->score(v2).level == 0);
		ctx.unfreeze();
		Var v3 = ctx.addVar(Var_t::Atom);
		ctx.output.add("v1", posLit(v1));
		ctx.output.add("v3", posLit(v3));
		ctx.startAddConstraints();
		dom->setDefaultMod(HeuParams::mod_level, HeuParams::pref_show);
		ctx.endInit();
		REQUIRE(dom->score(v1).level == dom->score(v3).level);
		REQUIRE(dom->score(v2).level == 0);
	}
}
TEST_CASE("Domain Heuristic with eq-preprocessing", "[heuristic][asp]") {
	SharedContext ctx;
	LogicProgram lp;
	DomainHeuristic* dom;
	ctx.master()->setHeuristic((dom = new DomainHeuristic), Ownership_t::Acquire);
	Solver& s = *ctx.master();
	Var a = 1, b = 2, c = 3, d = 4;
	SECTION("test map to one var") {
		lpAdd(lp.start(ctx), "a :- not b. b :- not a.");
		lp.addDomHeuristic(a, DomModType::True, 2, 2);
		lp.addDomHeuristic(b, DomModType::True, 1, 1);
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE((s.decideNextBranch() && s.isTrue(lp.getLiteral(a))));
	}
	SECTION("test level") {
		lpAdd(lp.start(ctx),
			"{a;c}.\n"
			"b :- a.\n");
		lp.addDomHeuristic(a, DomModType::Level, 1, 3);
		lp.addDomHeuristic(b, DomModType::Level, 3, 2);
		lp.addDomHeuristic(c, DomModType::Level, 2, 1);
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE(s.decideNextBranch());
		REQUIRE(s.value(getDomLiteral(lp, b).var()) != value_free);
	}
	SECTION("test sign") {
		lpAdd(lp.start(ctx), "{a}. b :- a.");
		lp.addDomHeuristic(a, DomModType::Sign, 1, 1);
		lp.addDomHeuristic(b, DomModType::Sign, -1, 1);
		REQUIRE((lp.endProgram() && ctx.endInit()));

		REQUIRE((s.pref(getDomLiteral(lp, a).var()).has(ValueSet::user_value)));
		REQUIRE((s.pref(getDomLiteral(lp, b).var()).has(ValueSet::user_value)));
	}
	SECTION("test complementary atom level") {
		lpAdd(lp.start(ctx), "{a;c}. b :- not a.");
		lp.addDomHeuristic(a, DomModType::Level, 1, 3);
		lp.addDomHeuristic(b, DomModType::Level, 3, 2);
		lp.addDomHeuristic(c, DomModType::Level, 2, 1);
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE(s.decideNextBranch());
		REQUIRE(s.value(getDomLiteral(lp, b).var()) != value_free);
	}
	SECTION("test complementary atom sign") {
		lpAdd(lp.start(ctx), "{a}. b :- not a.");
		lp.addDomHeuristic(a, DomModType::Sign, 1, 1);
		lp.addDomHeuristic(b, DomModType::Sign, 1, 1);
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE((s.pref(getDomLiteral(lp, a).var()).has(ValueSet::user_value)));
		REQUIRE((s.pref(getDomLiteral(lp, b).var()).has(ValueSet::user_value)));
	}
	SECTION("test complementary atom true/false") {
		lpAdd(lp.start(ctx), "a :- not b. b :- not a. {c}.");
		lp.addDomHeuristic(a, DomModType::True, 10, 10);
		lp.addDomHeuristic(b, DomModType::True, 20, 20);
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE(s.decideNextBranch());
		REQUIRE(s.isTrue(getDomLiteral(lp, b)));
	}

	SECTION("test same var different level") {
		lp.start(ctx);
		lp.updateProgram();
		lpAdd(lp, "{a;c}. b :- a.");
		lp.addDomHeuristic(a, DomModType::Level, 2, 1);
		lp.addDomHeuristic(c, DomModType::Level, 2, 1);
		lp.addDomHeuristic(c, DomModType::Init, 10, 1);
		SECTION("once") {
			lp.addDomHeuristic(b, DomModType::Init, 20, 1);
			REQUIRE((lp.endProgram() && ctx.endInit()));
			REQUIRE(s.value(lp.getLiteral(c).var()) == value_free);
			REQUIRE(s.decideNextBranch());
			REQUIRE(s.value(lp.getLiteral(c).var()) != value_free);
		}
		SECTION("incremental") {
			REQUIRE((lp.endProgram() && ctx.endInit()));
			REQUIRE(s.decideNextBranch());
			REQUIRE(s.value(lp.getLiteral(c).var()) != value_free);
			s.undoUntil(0);
			REQUIRE(lp.updateProgram());
			lp.addDomHeuristic(b, DomModType::Init, 20, 1);
			REQUIRE((lp.endProgram() && ctx.endInit()));
			REQUIRE(s.decideNextBranch());
			REQUIRE(s.value(lp.getLiteral(c).var()) != value_free);
		}
	}
	SECTION("test same var diff level cond") {
		lp.start(ctx);
		lpAdd(lp, "{a;b;c;d}. b :- a.");
		lp.addDomHeuristic(b, DomModType::Init, 40, 1);
		lp.addDomHeuristic(d, DomModType::Init, 50, 1);
		lp.addDomHeuristic(d, DomModType::Sign, 1, 1);
		lp.addDomHeuristic(a, DomModType::True, 2, 1, d);
		lp.addDomHeuristic(c, DomModType::True, 2, 1, d);
		lp.addDomHeuristic(c, DomModType::Init, 30, 1);

		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE(s.decideNextBranch());
		REQUIRE(s.isTrue(lp.getLiteral(d)));
		s.propagate();
		REQUIRE(s.decideNextBranch());
		REQUIRE(s.value(lp.getLiteral(c).var()) != value_free);
	}
	SECTION("test same var diff prio") {
		lp.start(ctx).updateProgram();
		lpAdd(lp, "{a}. b :- a.");
		lp.addDomHeuristic(a, DomModType::False, 2, 3);
		lp.addDomHeuristic(b, DomModType::False, 1, 1);
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE((s.decideNextBranch() && s.isFalse(getDomLiteral(lp, a))));
		s.undoUntil(0);
		REQUIRE(lp.updateProgram());
		lp.addDomHeuristic(b, DomModType::True, 3, 2);
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE((s.decideNextBranch() && s.isTrue(getDomLiteral(lp, b))));
	}
}

}}
