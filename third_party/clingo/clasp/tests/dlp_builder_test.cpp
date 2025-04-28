//
// Copyright (c) 2012-2017 Benjamin Kaufmann
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
#include <clasp/logic_program.h>
#include <clasp/unfounded_check.h>
#include "lpcompare.h"
#include <sstream>
#include "catch.hpp"
using namespace std;
namespace Clasp { namespace Test {
using namespace Clasp::Asp;

TEST_CASE("Disjunctive logic programs", "[asp][dlp]") {
	typedef Asp::PrgDepGraph DG;
	SharedContext ctx;
	LogicProgram  lp;
	Var           a = 1, b = 2, c = 3, d = 4, e = 5, f = 6;
	SECTION("testSimpleChoice") {
		lpAdd(lp.start(ctx), "a | b.");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE(lp.stats.disjunctions[0] == 1);
		REQUIRE(lp.getLiteral(a) != lp.getLiteral(b));
		ctx.master()->assume(lp.getLiteral(a)) && ctx.master()->propagate();
		REQUIRE(ctx.master()->isFalse(lp.getLiteral(b)));
		ctx.master()->undoUntil(0);
		ctx.master()->assume(lp.getLiteral(b)) && ctx.master()->propagate();
		REQUIRE(ctx.master()->isFalse(lp.getLiteral(a)));
	}
	SECTION("testNotAChoice") {
		lpAdd(lp.start(ctx), "a | b. a :- not c.");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE(lp.stats.disjunctions[0] == 1);
		REQUIRE(lp.getLiteral(b) == lp.getLiteral(c));
		REQUIRE(lp.getLiteral(a) == lit_true());
	}

	SECTION("testStillAChoice") {
		lpAdd(lp.start(ctx), "a|b. {b}.");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE(lp.stats.disjunctions[0] == 1);
		REQUIRE(lp.getLiteral(a) != lp.getLiteral(b));
		ctx.master()->assume(lp.getLiteral(a)) && ctx.master()->propagate();
		REQUIRE(ctx.master()->isFalse(lp.getLiteral(b)));
		ctx.master()->undoUntil(0);
		ctx.master()->assume(lp.getLiteral(b)) && ctx.master()->propagate();
		REQUIRE(ctx.master()->isFalse(lp.getLiteral(a)));
	}

	SECTION("testSubsumedChoice") {
		lpAdd(lp.start(ctx, LogicProgram::AspOptions().iterations(-1)),
			"a | b.\n"
			"a | b | c | d.\n");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE(lp.stats.disjunctions[0] == 2);
		REQUIRE((lp.getLiteral(c) == lit_false() || !(ctx.master()->assume(lp.getLiteral(c)) && ctx.master()->propagate())));
		ctx.master()->undoUntil(0);
		REQUIRE((lp.getLiteral(d) == lit_false() || !(ctx.master()->assume(lp.getLiteral(d)) && ctx.master()->propagate())));
	}

	SECTION("testSubsumedByChoice") {
		lpAdd(lp.start(ctx),
			"a | b.\n"
			"{a,b}.");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE(lp.stats.disjunctions[0] == 1);
		ctx.master()->assume(~lp.getLiteral(a)) && ctx.master()->propagate();
		REQUIRE(ctx.master()->isTrue(lp.getLiteral(b)));
	}
	SECTION("testChoiceDisjBug") {
		lpAdd(lp.start(ctx),
			"a | b.\n"
			"{a,b,b}.");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE(lp.stats.disjunctions[0] == 1);

		ctx.master()->assume(lp.getLiteral(c)) && ctx.master()->propagate();
		REQUIRE(ctx.master()->value(lp.getLiteral(a).var()) == value_free);
		REQUIRE(ctx.master()->value(lp.getLiteral(b).var()) == value_free);
		ctx.master()->assume(~lp.getLiteral(a)) && ctx.master()->propagate();
		REQUIRE(ctx.master()->isTrue(lp.getLiteral(b)));
		ctx.master()->undoUntil(0);
		ctx.master()->assume(lp.getLiteral(a)) && ctx.master()->propagate();
		ctx.master()->assume(lp.getLiteral(b)) && ctx.master()->propagate();
		ctx.master()->assume(lp.getLiteral(c)) && ctx.master()->propagate();
		REQUIRE((!ctx.master()->hasConflict() && ctx.master()->numFreeVars() == 0));
	}

	SECTION("testTautOverOne") {
		lpAdd(lp.start(ctx),
			"{b}.\n"
			"c :- b.\n"
			"a | b :- c.");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE(lp.stats.disjunctions[0] == 1);

		REQUIRE((lp.getLiteral(a) == lit_false() && !(ctx.master()->assume(lp.getLiteral(a)) && ctx.master()->propagate())));
	}

	SECTION("testSimpleLoop") {
		lpAdd(lp.start(ctx),
			"a | b."
			"a :- b."
			"b :- a.");
		REQUIRE((lp.endProgram() && ctx.sccGraph.get()));
		REQUIRE(lp.stats.disjunctions[0] == 1);
		ctx.master()->addPost(new DefaultUnfoundedCheck(*ctx.sccGraph));
		REQUIRE(ctx.endInit());

		REQUIRE(lp.getLiteral(a) != lp.getLiteral(b));
		REQUIRE((ctx.master()->assume(lp.getLiteral(a)) && ctx.master()->propagate()));
		REQUIRE(!ctx.master()->isFalse(lp.getLiteral(b)));
		ctx.master()->undoUntil(0);
		REQUIRE((ctx.master()->assume(lp.getLiteral(b)) && ctx.master()->propagate()));
		REQUIRE(!ctx.master()->isFalse(lp.getLiteral(a)));
	}

	SECTION("testSimpleLoopNoGamma") {
		lpAdd(lp.start(ctx, LogicProgram::AspOptions().disableGamma()),
			"a | b.\n"
			"a :- b.\n"
			"b :- a.\n");
		REQUIRE(lp.endProgram());
		REQUIRE(lp.stats.disjunctions[0] == 1);

		REQUIRE((!(ctx.master()->assume(~lp.getLiteral(a)) && ctx.master()->propagate()) || ctx.master()->isTrue(lp.getLiteral(b))));
	}
	SECTION("testComputeTrue") {
		lpAdd(lp.start(ctx, LogicProgram::AspOptions().noScc()),
			"a | b.\n"
			"a :- b.\n"
			":- not a.\n");
		REQUIRE(lp.getAtom(a)->value() == value_true);
		REQUIRE(lp.endProgram());
		REQUIRE(lp.stats.disjunctions[0] == 1);

		REQUIRE(ctx.master()->isTrue(lp.getLiteral(a)));
		REQUIRE(ctx.master()->isFalse(lp.getLiteral(b)));
	}
	SECTION("testComputeFalse") {
		lpAdd(lp.start(ctx),
			"a | b | c.\n"
			"a :- c.\n"
			" :- a.");
		REQUIRE(lp.getAtom(a)->value() == value_false);
		REQUIRE(lp.endProgram());
		REQUIRE(lp.stats.disjunctions[0] == 1);

		REQUIRE(ctx.master()->isFalse(lp.getLiteral(a)));
		REQUIRE(ctx.master()->isFalse(lp.getLiteral(c)));
		REQUIRE(ctx.master()->isTrue(lp.getLiteral(b)));
	}

	SECTION("testOddLoop") {
		lpAdd(lp.start(ctx),
			"a|b :- not c.\n"
			"c|d :- not a.\n");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE(lp.stats.disjunctions[0] == 2);
		REQUIRE(lp.stats.disjunctions[1] == 0);
	}

	SECTION("testNoEqRegression") {
		lpAdd(lp.start(ctx, LogicProgram::AspOptions().noEq()),
			"{c}.\n"
			"a | b :- c.\n");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		ctx.master()->assume(lp.getLiteral(a));
		ctx.master()->propagate();
		REQUIRE(ctx.master()->numFreeVars() == 0u);
	}

	SECTION("testOutputRegression") {
		lpAdd(lp.start(ctx, LogicProgram::AspOptions().noEq()),
			"{c}.\n"
			"a | b :- c.\n");
		lp.addOutput("foo", Potassco::id(d));
		REQUIRE((lp.endProgram() && ctx.endInit()));
		for (OutputTable::pred_iterator it = ctx.output.pred_begin(), end = ctx.output.pred_end(); it != end; ++it) {
			REQUIRE((it->user != d || it->cond == lit_false()));
		}
	}

	SECTION("testIssue84") {
		lpAdd(lp.start(ctx),
			"d:-b.\n"
			"e|f :-c, d.\n"
			"d :-e.\n"
			"a :-c, d.\n"
			"{b;c }.\n");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		const DG& graph = *ctx.sccGraph;

		REQUIRE(graph.numNonHcfs() == 0);
		CHECK(graph.numAtoms() == 4);  // bot, d, e, and either a or some new atom x
		CHECK(graph.numBodies() == 4); // {c,d}, {b}, {e}, {a/x, not f}
	}

	SECTION("testIncremental") {
		lp.start(ctx);
		lp.updateProgram();
		lpAdd(lp,
			"% Step 0:\n"
			"a | b :- c.\n"
			"a :- b.\n"
			"b :- a.\n"
			"#external c.");
		REQUIRE(lp.endProgram());
		REQUIRE(lp.stats.disjunctions[0] == 1);
		REQUIRE(lp.stats.sccs == 1);
		REQUIRE(lp.stats.nonHcfs == 1);

		lp.update();
		lpAdd(lp,
			"% Step 1:\n"
			"d | e :- a, f.\n"
			"d :- e, b.\n"
			"e :- d, a.\n"
			"#external f.\n");
		REQUIRE(lp.endProgram());
		REQUIRE(lp.stats.disjunctions[0] == 1);
		REQUIRE(lp.stats.sccs == 1);
		REQUIRE(lp.stats.nonHcfs == 1);
	}

	SECTION("testSimplify") {
		lp.start(ctx);
		lp.updateProgram();
		lpAdd(lp,
			"a | b :- d.\n"
			"a :- b.\n"
			"b :- c.\n"
			"c :- a,e.\n"
			"{d,e}.\n");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE(lp.stats.disjunctions[0] == 1);
		REQUIRE(lp.stats.sccs == 1);
		REQUIRE(lp.stats.nonHcfs == 1);
		DG& graph = *ctx.sccGraph;
		REQUIRE(graph.numNonHcfs() == 1);
		ctx.addUnary(~lp.getLiteral(a));
		ctx.master()->propagate();
		graph.simplify(*ctx.master());
		REQUIRE(graph.numNonHcfs() == 0);
	}
	SECTION("testSimplifyNoRemove") {
		ctx.setConcurrency(2);
		lp.start(ctx);
		lp.updateProgram();
		lpAdd(lp,
			"a | b :- d.\n"
			"a :- b.\n"
			"b :- c.\n"
			"c :- a,e.\n"
			"{d,e}.\n");
		REQUIRE(lp.endProgram());
		Solver& s2= ctx.pushSolver();
		REQUIRE(ctx.endInit(true));
		DG& graph = *ctx.sccGraph;
		REQUIRE(graph.numNonHcfs() == 1);
		Literal u = ~lp.getLiteral(a);
		ctx.master()->add(ClauseRep::create(&u, 1, Constraint_t::Conflict));
		ctx.master()->simplify() && s2.simplify();
		graph.simplify(*ctx.master());
		graph.simplify(s2);
		REQUIRE(graph.numNonHcfs() == 1);
		const DG::NonHcfComponent* c = *graph.nonHcfBegin();

		LitVec temp;
		VarVec ufs;
		c->assumptionsFromAssignment(*ctx.master(), temp);
		REQUIRE((c->test(*ctx.master(), temp, ufs) && ufs.empty()));

		temp.clear();
		c->assumptionsFromAssignment(s2, temp);
		REQUIRE((c->test(s2, temp, ufs) && ufs.empty()));
	}
	SECTION("testNoDupGamma") {
		lpAdd(lp.start(ctx),
			"a | b.\n"
			"a :- b.\n"
			"b :- a.\n"
			"a :- not b.\n");
		REQUIRE(lp.endProgram());
		REQUIRE(lp.numBodies() == 5); // + b :- not a.
		REQUIRE(lp.stats.gammas == 1);
		std::stringstream exp("1 1 1 1 2\n1 1 1 1 2");
		REQUIRE_FALSE(findSmodels(exp, lp));
	}
	SECTION("testPreNoGamma") {
		lpAdd(lp.start(ctx),
			"a | b.\n"
			"a :- b.\n"
			"b :- a.\n");
		REQUIRE(lp.endProgram());
		std::stringstream exp("1 1 1 1 2");
		REQUIRE_FALSE(findSmodels(exp, lp));
	}
	SECTION("testPreNoGamma2") {
		lpAdd(lp.start(ctx, LogicProgram::AspOptions().disableGamma()),
			"a | b.\n"
			"a :- b.\n"
			"b :- a.\n");
		REQUIRE(lp.endProgram());
		std::stringstream exp("3 2 1 2 0 0");
		REQUIRE_FALSE(findSmodels(exp, lp));
	}
	SECTION("testRecAgg") {
		lpAdd(lp.start(ctx),
			"{c}.\n"
			"a | b.\n"
			"a :- 1 {c, b}.\n"
			"b :- 1 {c, a}.");
		REQUIRE(lp.stats.disjunctions[0] == 1);
		REQUIRE(lp.stats.bodies[0][Body_t::Count] == 2);
		std::stringstream exp("2 2 2 0 1 3 1");
		REQUIRE(findSmodels(exp, lp));
		REQUIRE(lp.endProgram());
		exp.clear(); exp.seekg(0);
		std::stringstream str;
		AspParser::write(lp, str, AspParser::format_smodels);
		REQUIRE_FALSE(findProgram(exp, str));
		exp.clear();  exp.str("");
		str.clear(); str.seekg(0);
		exp << "1 1 1 0 3\n" << "1 2 1 0 3\n";
		REQUIRE(findProgram(exp, str));
	}
	SECTION("testPropagateSource") {
		lpAdd(lp.start(ctx),
			"d; c.\n"
			"b; a:-c.\n"
			"b; a.\n");
		REQUIRE(lp.endProgram());
		REQUIRE(lp.getLiteral(a) != lp.getLiteral(b));
		REQUIRE(lp.getLiteral(b) != lp.getLiteral(c));
		REQUIRE(lp.getLiteral(c) != lp.getLiteral(d));
	}
}
 } }

