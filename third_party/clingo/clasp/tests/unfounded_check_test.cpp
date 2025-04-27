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
#include <clasp/unfounded_check.h>
#include <clasp/logic_program.h>
#include <clasp/clause.h>
#include "lpcompare.h"
#include <memory>
#include "catch.hpp"
namespace Clasp { namespace Test {
using namespace Clasp::Asp;

struct UfsTest {
	SharedContext ctx;
	SingleOwnerPtr<DefaultUnfoundedCheck> ufs;
	void attach() {
		if (ctx.sccGraph.get()) {
			ufs = new DefaultUnfoundedCheck(*ctx.sccGraph);
			ctx.master()->addPost(ufs.release());
		}
		ctx.endInit();
	}
	bool propagate() {
		return ufs->propagateFixpoint(*ctx.master(), 0);
	}
};
TEST_CASE("Unfounded set checking", "[asp][propagator]") {
	UfsTest test;
	SharedContext& ctx = test.ctx;
	Solver& solver = *ctx.master();
	SingleOwnerPtr<DefaultUnfoundedCheck>& ufs = test.ufs;
	LogicProgram lp;
	LitVec index;

	SECTION("with simple program") {
		lpAdd(lp.start(ctx),
			"{x5;x6;x7;x3}.\n"
			"x2 :- x1.\n"
			"x1 :- x2, x4.\n"
			"x4 :- x1, x3.\n"
			"x1 :- not x5.\n"
			"x2 :- not x7.\n"
			"x4 :- not x6.\n");
		REQUIRE(lp.endProgram());
		index.assign(1, lit_true());
		for (Var v = 1; v <= lp.numAtoms(); ++v) {
			index.push_back(lp.getLiteral(v));
		}
		test.attach();
		REQUIRE(solver.propagateUntil(ufs.get()));

		SECTION("testAllUncoloredNoUnfounded") {
			uint32 x = solver.numAssignedVars();
			REQUIRE(test.propagate());
			REQUIRE(x == solver.numAssignedVars());
		}
		SECTION("testAlternativeSourceNotUnfounded") {
			solver.assume(index[6]);
			solver.propagateUntil(ufs.get());
			uint32 old = solver.numAssignedVars();
			REQUIRE(test.propagate());
			REQUIRE(old == solver.numAssignedVars());
		}
		SECTION("testOnlyOneSourceUnfoundedIfMinus") {
			solver.assume(index[6]);
			solver.assume(index[5]);
			solver.propagateUntil(ufs.get());
			uint32 old = solver.numAssignedVars();
			uint32 oldC = ctx.numConstraints();
			REQUIRE(test.propagate());
			REQUIRE(old < solver.numAssignedVars());
			REQUIRE(solver.isFalse(index[4]));
			REQUIRE(solver.isFalse(index[1]));
			REQUIRE(!solver.isFalse(index[2]));
			REQUIRE(oldC+1 == ctx.numConstraints() + ctx.numLearntShort());
		}
	}

	SECTION("testWithSimpleCardinalityConstraint") {
		lpAdd(lp.start(ctx), "{x2}. x1 :- 1 {x1, x2}.");
		REQUIRE(lp.endProgram());
		test.attach();

		REQUIRE(solver.propagateUntil(ufs.get()));
		REQUIRE(2u == solver.numVars());
		REQUIRE(0u == solver.numAssignedVars());
		REQUIRE(test.propagate() );
		REQUIRE(0u == solver.numAssignedVars());
		REQUIRE(solver.assume(~lp.getLiteral(2)));
		REQUIRE(solver.propagateUntil(ufs.get()));
		REQUIRE(1u == solver.numAssignedVars());
		REQUIRE(test.propagate() );
		REQUIRE(2u == solver.numAssignedVars());
		LitVec r;
		solver.reason(~lp.getLiteral(1), r);
		REQUIRE(1 == r.size());
		REQUIRE(r[0] == ~lp.getLiteral(2));
	}
	SECTION("testWithSimpleWeightConstraint") {
		lpAdd(lp.start(ctx), "{x2;x3}. x1 :- 2 {x1 = 2, x2 = 2, x3}.");
		REQUIRE(lp.endProgram());
		test.attach();
		ufs->setReasonStrategy(DefaultUnfoundedCheck::only_reason);

		REQUIRE(solver.propagateUntil(ufs.get()));
		REQUIRE(3u == solver.numVars());
		REQUIRE(0u == solver.numAssignedVars());
		REQUIRE(test.propagate() );
		REQUIRE(0u == solver.numAssignedVars());
		REQUIRE(solver.assume(~lp.getLiteral(3)));
		REQUIRE(solver.propagateUntil(ufs.get()));
		REQUIRE(1u == solver.numAssignedVars());
		REQUIRE(test.propagate() );
		REQUIRE(1u == solver.numAssignedVars());

		REQUIRE(solver.assume(~lp.getLiteral(2)));
		REQUIRE(solver.propagateUntil(ufs.get()));
		REQUIRE(2u == solver.numAssignedVars());

		REQUIRE(test.propagate() );
		REQUIRE(3u == solver.numAssignedVars());

		LitVec r;
		solver.reason(~lp.getLiteral(1), r);
		REQUIRE(std::find(r.begin(), r.end(), ~lp.getLiteral(2)) != r.end());

		solver.undoUntil(0);
		REQUIRE(solver.assume(~lp.getLiteral(2)));
		REQUIRE(solver.propagateUntil(ufs.get()));
		REQUIRE(1u == solver.numAssignedVars());
		REQUIRE(test.propagate() );
		REQUIRE(2u == solver.numAssignedVars());
		r.clear();
		solver.reason(~lp.getLiteral(1), r);
		REQUIRE(1 == r.size());
		REQUIRE(r[0] == ~lp.getLiteral(2));
	}

	SECTION("testNtExtendedBug") {
		lpAdd(lp.start(ctx),
			"{x1;x2;x3}.\n"
			"x4 :- 2 {x2, x4, x5}.\n"
			"x5 :- x4, x3.\n"
			"x5 :- x1.");
		REQUIRE(lp.endProgram());
		test.attach();

		// T: {x4,x3}
		solver.assume(Literal(6, false));
		REQUIRE(solver.propagateUntil(ufs.get()));
		REQUIRE(test.propagate());
		solver.assume(~lp.getLiteral(1));
		REQUIRE(solver.propagateUntil(ufs.get()));
		REQUIRE(false == test.propagate());  // {x4, x5} are unfounded!

		solver.undoUntil(0);
		ufs->reset();

		// F: {x4,x3}
		solver.assume(Literal(6, true));
		REQUIRE(solver.propagateUntil(ufs.get()));
		// F: x1
		solver.assume(~lp.getLiteral(1));
		REQUIRE(solver.propagateUntil(ufs.get()));
		// x5 is false because both of its bodies are false
		REQUIRE(solver.isFalse(lp.getLiteral(5)));

		// x4 is now unfounded but its defining body is not
		REQUIRE(test.propagate());
		REQUIRE(solver.isFalse(lp.getLiteral(4)));
		LitVec r;
		solver.reason(~lp.getLiteral(4), r);
		REQUIRE((r.size() == 1 && r[0] == ~lp.getLiteral(5)));
	}

	SECTION("testNtExtendedFalse") {
		lpAdd(lp.start(ctx, LogicProgram::AspOptions().noEq()),
			"{x3}.\n"
			"x4 :- 2 {x1, x2, x5}.\n"
			"x5 :- x4, x3.\n"
			"x4 :- x5, x3.\n"
			"x1 :- not x3.\n"
			"x2 :- not x3.");
		REQUIRE(lp.endProgram());
		test.attach();

		solver.assume(lp.getLiteral(3));
		REQUIRE(solver.propagateUntil(ufs.get()));
		REQUIRE(test.propagate());
		REQUIRE(solver.numVars() == solver.numAssignedVars());
		REQUIRE(solver.isFalse(lp.getLiteral(4)));
		REQUIRE(solver.isFalse(lp.getLiteral(5)));

		solver.backtrack();
		REQUIRE(solver.propagateUntil(ufs.get()));
		REQUIRE(test.propagate());
		REQUIRE(solver.numVars() == solver.numAssignedVars());
		REQUIRE(solver.isTrue(lp.getLiteral(4)));
		REQUIRE(solver.isFalse(lp.getLiteral(5)));
	}

	SECTION("testDependentExtReason") {
		lpAdd(lp.start(ctx, LogicProgram::AspOptions().noEq()),
			"{x3, x4, x5}.\n"
			"x1 :- not x4.\n"
			"x1 :- 2 {x1, x2, x3}.\n"
			"x1 :- x2, x3.\n"
			"x2 :- x1, x5.");

		REQUIRE(lp.endProgram());
		test.attach();

		// assume ~B1, where B1 = 2 {x1, x2, x3}
		const Asp::PrgDepGraph::AtomNode& a = ufs->graph()->getAtom(lp.getAtom(1)->id());
		Literal x1 = ufs->graph()->getBody(a.body(1)).lit;

		solver.assume(~x1);
		REQUIRE((solver.propagateUntil(ufs.get()) && test.propagate()));
		REQUIRE(value_free == solver.value(lp.getLiteral(1).var()));
		REQUIRE(value_free == solver.value(lp.getLiteral(2).var()));
		// B1
		REQUIRE(1u == solver.numAssignedVars());

		// assume x4
		solver.assume(lp.getLiteral(4));
		REQUIRE(solver.propagateUntil(ufs.get()));
		// B1 + x4 (hence {not x4})
		REQUIRE(2u == solver.numAssignedVars());

		// U = {x1, x2}.
		REQUIRE(test.propagate());
		REQUIRE(solver.isFalse(lp.getLiteral(1)));
		REQUIRE(solver.isFalse(lp.getLiteral(2)));
		Literal extBody = ufs->graph()->getBody(a.body(0)).lit;
		LitVec r;
		solver.reason(~lp.getLiteral(1), r);
		REQUIRE((r.size() == 1u && r[0] == ~extBody));
	}

	SECTION("testEqBodyDiffType") {
		lpAdd(lp.start(ctx),
			"{x1; x4; x5}.\n"
			"{x2} :- x1.\n"
			"x3 :- x1.\n"
			"x2 :- x3, x4.\n"
			"x3 :- x2, x5.");
		lp.endProgram();
		REQUIRE(lp.stats.sccs == 1);
		test.attach();

		solver.assume(~lp.getLiteral(1));
		REQUIRE(solver.propagateUntil(ufs.get()));
		REQUIRE(test.propagate());
		REQUIRE(solver.isFalse(lp.getLiteral(2)));
		REQUIRE(solver.isFalse(lp.getLiteral(3)));
	}

	SECTION("testChoiceCardInterplay") {
		lpAdd(lp.start(ctx, LogicProgram::AspOptions().noEq()),
			"{x4}.\n"
			"{x1} :- x4.\n"
			"x2 :- 1 {x1, x3}.\n"
			"x3 :- x2.\n"
			"{x1} :- x3.");
		lp.endProgram();
		REQUIRE(lp.stats.sccs == 1);
		test.attach();

		solver.assume(~lp.getLiteral(1));
		REQUIRE(solver.propagateUntil(ufs.get()));
		REQUIRE(test.propagate());
		REQUIRE(solver.isFalse(lp.getLiteral(2)));
		REQUIRE(solver.isFalse(lp.getLiteral(3)));
	}

	SECTION("testCardInterplayOnBT") {
		lpAdd(lp.start(ctx, LogicProgram::AspOptions().noEq()),
			"{x1;x5}.\n"
			"x2 :- 1 {x1, x3}.\n"
			"x2 :- x4.\n"
			"x4 :- x2.\n"
			"x4 :- x5.\n"
			"x3 :- x2, x4.");
		lp.endProgram();
		REQUIRE(lp.stats.sccs == 1);
		test.attach();

		solver.assume(~lp.getLiteral(1));
		REQUIRE((solver.propagateUntil(ufs.get()) && test.propagate()));
		REQUIRE(false == solver.isFalse(lp.getLiteral(2)));
		REQUIRE(false == solver.isFalse(lp.getLiteral(3)));
		solver.undoUntil(0);

		solver.assume(~lp.getLiteral(5));
		REQUIRE((solver.propagateUntil(ufs.get()) && test.propagate()));
		REQUIRE(false == solver.isFalse(lp.getLiteral(2)));
		REQUIRE(false == solver.isFalse(lp.getLiteral(3)));
	}

	SECTION("testInitNoSource") {
		lpAdd(lp.start(ctx, LogicProgram::AspOptions().noEq()),
			"{x3}.\n"
			"{x1;x2} :- x3.\n"
			"x1 :- x2.\n"
			"x2 :- x1.\n");
		lp.endProgram();
		REQUIRE(lp.stats.sccs == 1);

		solver.force(~lp.getLiteral(3));
		test.attach();
		REQUIRE(solver.isFalse(lp.getLiteral(1)));
	}

	SECTION("testIncrementalUfs") {
		lp.start(ctx, LogicProgram::AspOptions().noEq());
		lp.updateProgram();
		lpAdd(lp,
			"% I1:\n"
			"x1 :- not x2.\n"
			"x2 :- not x1.\n"
			"#external x3.");
		REQUIRE((lp.endProgram() && ctx.endInit()));
		REQUIRE(ctx.sccGraph.get() == 0);
		lp.updateProgram();
		lpAdd(lp,
			"% I2:\n"
			"{x3;x6}.\n"
			"x4 :- not x3.\n"
			"x4 :- x5, x6.\n"
			"x5 :- x4, x6.\n"
			"#external x3. [release]");
		REQUIRE(lp.endProgram());
		REQUIRE(6u == ctx.sccGraph.get()->nodes());
		REQUIRE(1 == lp.stats.sccs);
		test.attach();
		REQUIRE(6u == ufs->nodes());

		lp.updateProgram();
		lpAdd(lp,
			"% I3:\n"
			"x7 :- x4, not x9.\n"
			"x9 :- not x7.\n"
			"x7 :- x8.\n"
			"x8 :- x7, not x6.\n");

		REQUIRE(lp.endProgram());
		REQUIRE(11u == ctx.sccGraph.get()->nodes());
		REQUIRE(1 == lp.stats.sccs);
		ctx.endInit();
		REQUIRE(11u == ufs->nodes());
		REQUIRE(lp.getAtom(7)->scc() != lp.getAtom(4)->scc());
	}

	SECTION("testInitialStopConflict") {
		lpAdd(lp.start(ctx),
			"{x3;x4}.\n"
			"x1 :- x3, x4.\n"
			"x1 :- x2, x4.\n"
			"x2 :- x1, x4.");
		REQUIRE(lp.endProgram());
		REQUIRE(lp.stats.sccs == 1);
		struct M : PostPropagator {
			uint32 priority() const { return priority_reserved_msg; }
			bool propagateFixpoint(Solver& s, PostPropagator*) {
				s.setStopConflict();
				return false;
			}
		} m;
		solver.addPost(&m);
		ctx.addUnary(~lp.getLiteral(3));
		test.attach();
		REQUIRE(solver.hasStopConflict());
		solver.removePost(&m);
		solver.popRootLevel();
		solver.propagate();
		REQUIRE(solver.isFalse(lp.getLiteral(3)));
		REQUIRE((solver.isFalse(lp.getLiteral(1)) && solver.isFalse(lp.getLiteral(2))));
	}

	SECTION("testIncrementalLearnFact") {
		lp.start(ctx);
		lp.update();
		lpAdd(lp,
			"{x3;x4}.\n"
			"x1 :- x3, x4.\n"
			"x1 :- x2, x4.\n"
			"x2 :- x1, x4.");
		REQUIRE(lp.endProgram());
		test.attach();
		lp.update();
		lp.endProgram();
		ctx.addUnary(~lp.getLiteral(3));
		REQUIRE(solver.propagate());
		REQUIRE(ctx.endInit());
		REQUIRE(solver.isFalse(lp.getLiteral(3)));
		REQUIRE((solver.isFalse(lp.getLiteral(1)) && solver.isFalse(lp.getLiteral(2))));
	}

	SECTION("testDetachRemovesWatches") {
		lpAdd(lp.start(ctx, LogicProgram::AspOptions().noEq()),
			"{x3;x4;x5}.\n"
			"x1 :- x4.\n"
			"x1 :- 2 {x1, x2, x3}.\n"
			"x1 :- x2, x3.\n"
			"x2 :- x1, x5.");
		REQUIRE(lp.endProgram());
		ctx.endInit();
		uint32 numW = 0;
		for (uint32 i = 1; i <= solver.numVars(); ++i) {
			numW += solver.numWatches(posLit(i));
			numW += solver.numWatches(negLit(i));
		}
		ufs = new DefaultUnfoundedCheck(*ctx.sccGraph);
		solver.addPost(ufs.release());
		ufs->destroy(&solver, true);
		REQUIRE(!solver.getPost(PostPropagator::priority_reserved_ufs));
		for (uint32 i = 1; i <= solver.numVars(); ++i) {
			numW -= solver.numWatches(posLit(i));
			numW -= solver.numWatches(negLit(i));
		}
		REQUIRE(numW == 0);
	}

	SECTION("testApproxUfs") {
		lpAdd(lp.start(ctx),
			"a | b.\n"
			"c | d.\n"
			"c | e :- b.\n"
			"b | d :- c.\n"
			"c :- d, not b.\n"
			"c :- e.\n"
			"d :- e, not a.\n"
			"e :- c, d.");
		lp.endProgram();
		REQUIRE(lp.stats.sccs == 1);
		REQUIRE(lp.getAtom(1)->scc() == +PrgNode::noScc);
		REQUIRE(5u == ctx.sccGraph.get()->numAtoms());
		REQUIRE(8u == ctx.sccGraph.get()->numBodies());
		test.attach();
		REQUIRE((solver.assume(lp.getLiteral(2)) && solver.propagate()));
		solver.assume(lp.getLiteral(3));
		REQUIRE(solver.propagateUntil(ufs.get()));

		REQUIRE(solver.value(lp.getLiteral(4).var()) == value_free);
		REQUIRE(test.propagate());

		REQUIRE((solver.isFalse(lp.getLiteral(4)) || "TODO: Implement approx. ufs!"));
	}

	SECTION("testWeightReason") {
		lpAdd(lp.start(ctx),
			"{x4;x5;x6;x7}.\n"
			"x1 :- 2 {x2, x3 = 2, not x4}.\n"
			"x2 :- x1, x5.\n"
			"x3 :- x2, x6.\n"
			"x3 :- x7.");
		REQUIRE(lp.endProgram());
		REQUIRE(lp.stats.sccs == 1);
		test.attach();
		ufs->setReasonStrategy(DefaultUnfoundedCheck::only_reason);
		ctx.master()->assume(~lp.getLiteral(7)) && ctx.master()->propagate();
		REQUIRE(ctx.master()->isFalse(lp.getLiteral(3)));
		REQUIRE(ctx.master()->isFalse(lp.getLiteral(1)));
		ctx.master()->undoUntil(0);

		ctx.master()->assume(lp.getLiteral(4)) && ctx.master()->propagate();
		REQUIRE(ctx.master()->value(lp.getLiteral(1).var()) == value_free);
		ctx.master()->assume(lp.getLiteral(5)) && ctx.master()->propagate();
		ctx.master()->assume(lp.getLiteral(6)) && ctx.master()->propagate();
		REQUIRE(ctx.master()->numAssignedVars() == 3);
		ctx.master()->assume(~lp.getLiteral(7)) && ctx.master()->propagate();
		REQUIRE(ctx.master()->isFalse(lp.getLiteral(1)));
		REQUIRE(ctx.master()->isFalse(lp.getLiteral(2)));
		REQUIRE(ctx.master()->isFalse(lp.getLiteral(3)));

		LitVec reason;
		ctx.master()->reason(~lp.getLiteral(3), reason);
		REQUIRE(std::find(reason.begin(), reason.end(), ~lp.getLiteral(7)) != reason.end());
		REQUIRE(reason.size() == 1);
	}

	SECTION("testCardExtSet") {
		lpAdd(lp.start(ctx),
			"{x4; x5; x6; x7; x8}.\n"
			"c :- 2 {a, b, x4, x5}.\n"
			"a :- c,x6.\n"
			"b :- a,x7.\n"
			"a :- x8.\n");
		Var a = 1, b = 2, c = 3, x4 = 4, x5 = 5, x6 = 6, x7 = 7, x8 = 8;
		REQUIRE(lp.endProgram());
		REQUIRE(lp.stats.sccs == 1);
		test.attach();
		ufs->setReasonStrategy(DefaultUnfoundedCheck::only_reason);
		Solver& solver = *ctx.master();
		solver.undoUntil(0);
		solver.assume(~lp.getLiteral(x4)) && solver.propagate();
		solver.assume(~lp.getLiteral(x8)) && solver.propagate();
		REQUIRE(solver.isFalse(lp.getLiteral(a)));
		REQUIRE(solver.isFalse(lp.getLiteral(b)));
		REQUIRE(solver.isFalse(lp.getLiteral(c)));

		solver.undoUntil(0);
		solver.assume(~lp.getLiteral(x5)) && solver.propagate();
		solver.assume(~lp.getLiteral(x8)) && solver.propagate();
		REQUIRE(solver.isFalse(lp.getLiteral(a)));
		REQUIRE(solver.isFalse(lp.getLiteral(b)));
		REQUIRE(solver.isFalse(lp.getLiteral(c)));

		solver.undoUntil(0);
		solver.assume(~lp.getLiteral(x4)) && solver.propagate();
		solver.assume(~lp.getLiteral(x5)) && solver.propagate();
		REQUIRE(solver.numAssignedVars() == 2);

		solver.assume(lp.getLiteral(x6)) && solver.propagate();
		solver.assume(lp.getLiteral(x7)) && solver.propagate();
		REQUIRE(solver.numAssignedVars() == 4);

		ctx.master()->assume(~lp.getLiteral(x8)) && ctx.master()->propagate();
		REQUIRE(solver.isFalse(lp.getLiteral(a)));
		REQUIRE(solver.isFalse(lp.getLiteral(b)));
		REQUIRE(solver.isFalse(lp.getLiteral(c)));

		LitVec reason;
		ctx.master()->reason(~lp.getLiteral(a), reason);
		bool hasP = std::find(reason.begin(), reason.end(), ~lp.getLiteral(x4)) != reason.end();
		bool hasQ = std::find(reason.begin(), reason.end(), ~lp.getLiteral(x5)) != reason.end();
		bool hasT = std::find(reason.begin(), reason.end(), ~lp.getLiteral(x8)) != reason.end();
		REQUIRE(((hasP ^ hasQ) == true && hasT));
	}

	SECTION("testCardNoSimp") {
		lpAdd(lp.start(ctx),
			"b :- 2 {a, c, d, not a}.\n"
			"a :-b.\n"
			"a :- x5.\n"
			"{c;d;x5}.");
		Var a = 1, d = 4, x5 = 5;
		REQUIRE(lp.endProgram());
		REQUIRE(lp.stats.sccs == 1);
		test.attach();

		ufs->setReasonStrategy(DefaultUnfoundedCheck::only_reason);
		Solver& solver = *ctx.master();

		solver.assume(~lp.getLiteral(d)) && solver.propagate();
		solver.assume(lp.getLiteral(a)) && solver.propagate();

		REQUIRE((solver.assume(~lp.getLiteral(x5)) && solver.propagateUntil(ufs.get())));
		REQUIRE(!test.propagate());
		solver.resolveConflict();
		REQUIRE(solver.isTrue(lp.getLiteral(x5)));
		LitVec r;
		solver.reason(lp.getLiteral(x5), r);
		REQUIRE(r.size() == 2);
		REQUIRE(std::find(r.begin(), r.end(), ~lp.getLiteral(d)) != r.end());
		REQUIRE(std::find(r.begin(), r.end(), lp.getLiteral(a)) != r.end());
	}
}
} }
