//
// Copyright (c) 2009-2017 Benjamin Kaufmann
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
#include <clasp/dependency_graph.h>
#include <clasp/solver.h>
#include "lpcompare.h"
#include "catch.hpp"
namespace Clasp { namespace Test {
using namespace Clasp::Asp;


TEST_CASE("Dependency graph", "[asp][propagator]") {
	typedef Asp::PrgDepGraph DG;
	SharedContext ctx;
	LogicProgram  lp;
	SECTION("test Tight Program") {
		lp.start(ctx);
		lpAdd(lp, "x1 :- not x2.");
		lp.endProgram();
		REQUIRE(lp.stats.sccs == 0);
		REQUIRE(ctx.sccGraph.get() == 0);
	}

	SECTION("test Init Order") {
		lp.start(ctx, LogicProgram::AspOptions().noEq());
		lpAdd(lp,
			"x4 :- x3.\n"
			"x3 :- x4.\n"
			"x2 :- x3.\n"
			"x2 :- x1.\n"
			"x1 :- x2.\n"
			"x3 :- not a.\n");
		lp.endProgram();

		REQUIRE(lp.stats.sccs == 2);

		DG* graph = ctx.sccGraph.get();

		REQUIRE(uint32(10) == graph->nodes());

		const DG::AtomNode& b = graph->getAtom(lp.getAtom(2)->id());
		const DG::AtomNode& x = graph->getAtom(lp.getAtom(3)->id());

		REQUIRE(graph->getBody(b.body(0)).scc != b.scc);
		REQUIRE(graph->getBody(b.body(1)).scc == b.scc);
		REQUIRE(b.bodies_begin()+2 == b.bodies_end());

		REQUIRE(graph->getBody(x.body(0)).scc != x.scc);
		REQUIRE(graph->getBody(x.body(1)).scc == x.scc);
		REQUIRE(x.bodies_begin()+2 == x.bodies_end());

		const DG::BodyNode& xBody = graph->getBody(b.body(0));
		REQUIRE(graph->getAtom(xBody.heads_begin()[0]).scc == xBody.scc);
		REQUIRE(graph->getAtom(xBody.heads_begin()[1]).scc != xBody.scc);
		REQUIRE(xBody.heads_begin()+2 == xBody.heads_end());
	}

	SECTION("test Program With Loops") {
		lpAdd(lp.start(ctx),
			"a :- not f.\n"
			"b :- a.\n"
			"a :- b, d.\n"
			"b :- not e.\n"
			"c :- f.\n"
			"d :- c.\n"
			"c :- d.\n"
			"d :- not e.\n"
			"g :- not e.\n"
			"f :- g.\n"
			"e :- not g.\n");
		lp.endProgram();

		DG* graph = ctx.sccGraph.get();
		REQUIRE(lp.getLiteral(6) == lp.getLiteral(7));
		REQUIRE(~lp.getLiteral(6) == lp.getLiteral(5));

		REQUIRE(graph->getAtom(lp.getAtom(1)->id()).scc == 0);
		REQUIRE(graph->getAtom(lp.getAtom(2)->id()).scc == 0);
		REQUIRE(graph->getAtom(lp.getAtom(3)->id()).scc == 1);
		REQUIRE(graph->getAtom(lp.getAtom(4)->id()).scc == 1);
		const uint32 noId = PrgNode::noNode;
		REQUIRE(lp.getAtom(5)->id() == noId);
		REQUIRE((lp.getAtom(6)->eq() || lp.getAtom(6)->id() == noId));
		REQUIRE(lp.getAtom(7)->id() == noId);

		REQUIRE(uint32(11) == graph->nodes());
		// check that lists are partitioned by component number
		const DG::AtomNode& a = graph->getAtom(lp.getAtom(1)->id());
		REQUIRE(graph->getBody(a.body(0)).scc == +PrgNode::noScc);
		REQUIRE(graph->getBody(a.body(1)).scc == a.scc);
		REQUIRE(a.bodies_begin()+2 == a.bodies_end());
		REQUIRE(ctx.varInfo(a.lit.var()).frozen());

		const DG::BodyNode& bd = graph->getBody(a.body(1));
		REQUIRE(ctx.varInfo(bd.lit.var()).frozen());
		REQUIRE(graph->getAtom(bd.preds()[0]).lit == lp.getLiteral(2));
		REQUIRE(bd.preds()[1]== idMax);
		REQUIRE(bd.heads_begin()[0] == graph->id(a));
		REQUIRE(bd.heads_begin()+1 == bd.heads_end());
	}

	SECTION("test With Simple Cardinality Constraint") {
		lpAdd(lp.start(ctx),
			"{x2}.\n"
			"x1 :- 1 {x1, x2}.\n");
		REQUIRE(lp.endProgram());
		DG* graph = ctx.sccGraph.get();
		REQUIRE(uint32(3) == graph->nodes());
		const DG::AtomNode& a = graph->getAtom(lp.getAtom(1)->id());
		const DG::BodyNode& body = graph->getBody(a.body(0));

		REQUIRE(body.num_preds() == 2);
		REQUIRE(body.extended());
		REQUIRE(body.ext_bound() == 1);
		REQUIRE(body.pred_inc() == 1);
		REQUIRE(body.preds()[0] == graph->id(a));
		REQUIRE(body.preds()[1] == idMax);
		REQUIRE(body.preds()[2] == lp.getLiteral(2).rep());
		REQUIRE(body.preds()[3] == idMax);
		REQUIRE(body.pred_weight(0, false) == 1);
		REQUIRE(body.pred_weight(1, true) == 1);

		REQUIRE(a.inExtended());
		REQUIRE(a.succs()[0] == idMax);
		REQUIRE(a.succs()[1] == a.body(0));
		REQUIRE(a.succs()[2] == 0);
		REQUIRE(a.succs()[3] == idMax);
	}

	SECTION("test With Simple Weight Constraint") {
		lpAdd(lp.start(ctx),
			"{x2;x3}.\n"
			"x1 :- 2 {x1 = 2, x2 = 2, x3 = 1}.\n");
		REQUIRE(lp.endProgram());
		DG* graph = ctx.sccGraph.get();
		REQUIRE(uint32(3) == graph->nodes());

		const DG::AtomNode& a = graph->getAtom(lp.getAtom(1)->id());
		const DG::BodyNode& body = graph->getBody(a.body(0));

		REQUIRE(body.num_preds() == 3);
		REQUIRE(body.extended());
		REQUIRE(body.ext_bound() == 2);
		REQUIRE(body.pred_inc() == 2);
		REQUIRE(body.preds()[0] == graph->id(a));
		REQUIRE(body.preds()[2] == idMax);
		REQUIRE(body.preds()[3] == lp.getLiteral(2).rep());
		REQUIRE(body.preds()[5] == lp.getLiteral(3).rep());
		REQUIRE(body.preds()[7] == idMax);
		REQUIRE(body.pred_weight(0, false) == 2);
		REQUIRE(body.pred_weight(1, true) == 2);
		REQUIRE(body.pred_weight(2, true) == 1);

		REQUIRE(a.inExtended());
		REQUIRE(a.succs()[0] == idMax);
		REQUIRE(a.succs()[1] == a.body(0));
		REQUIRE(a.succs()[2] == 0);
		REQUIRE(a.succs()[3] == idMax);
	}

	SECTION("test Ignore Atoms From Prev Steps") {
		lp.start(ctx, LogicProgram::AspOptions().noEq());
		lp.updateProgram();
		lpAdd(lp,
			"{x1;x2;x3}.\n"
			"x4 :- x5.\n"
			"x5 :- x4.\n"
			"x4 :- x1.\n");
		REQUIRE(lp.endProgram());
		DG* graph = ctx.sccGraph.get();
		uint32 nA = lp.getAtom(4)->id();
		{
			const DG::AtomNode& a = graph->getAtom(nA);
			REQUIRE(std::distance(a.bodies_begin(), a.bodies_end()) == 2);
		}

		lp.update();
		lpAdd(lp,
			"x6|x7.\n"
			"x7 :- x6.\n"
			"x6 :- x7.\n");
		REQUIRE(lp.endProgram());
		{
			const DG::AtomNode& a = graph->getAtom(nA);
			REQUIRE(std::distance(a.bodies_begin(), a.bodies_end()) == 2);
		}
		const DG::AtomNode& c = graph->getAtom(lp.getAtom(6)->id());
		REQUIRE(std::distance(c.bodies_begin(), c.bodies_end()) == 2);
	}
};

TEST_CASE("Acyclicity checking", "[asp][propagator]") {
	SharedContext ctx;
	ExtDepGraph graph;
	SECTION("test Self Loop") {
		Literal x1 = posLit(ctx.addVar(Var_t::Atom));
		graph.addEdge(x1, 0, 0);
		graph.finalize(ctx);
		ctx.startAddConstraints();
		AcyclicityCheck* check;
		ctx.master()->addPost(check = new AcyclicityCheck(&graph));
		ctx.endInit();
		REQUIRE(ctx.master()->isFalse(x1));
	}
	SECTION("test Simple Loop") {
		Literal x1 = posLit(ctx.addVar(Var_t::Atom));
		Literal x2 = posLit(ctx.addVar(Var_t::Atom));
		graph.addEdge(x1, 0, 1);
		graph.addEdge(x2, 1, 0);
		graph.finalize(ctx);
		ctx.startAddConstraints();
		AcyclicityCheck* check;
		ctx.master()->addPost(check = new AcyclicityCheck(&graph));
		ctx.endInit();
		REQUIRE(ctx.master()->topValue(x1.var()) == value_free);
		REQUIRE(ctx.master()->topValue(x2.var()) == value_free);
		REQUIRE(ctx.master()->hasWatch(x1, check));
		REQUIRE(ctx.master()->hasWatch(x2, check));
		REQUIRE((ctx.master()->assume(x1) && ctx.master()->propagate()));

		REQUIRE(ctx.master()->isFalse(x2));
		ctx.master()->removePost(check);
		check->destroy(ctx.master(), true);
		REQUIRE(!ctx.master()->hasWatch(x1, check));
		REQUIRE(!ctx.master()->hasWatch(x2, check));
	}
	SECTION("test No Outgoing Edge") {
		Literal x1 = posLit(ctx.addVar(Var_t::Atom));
		Literal x2 = posLit(ctx.addVar(Var_t::Atom));
		graph.addEdge(x1, 0, 1);
		graph.finalize(ctx);
		ctx.startAddConstraints();
		AcyclicityCheck* check;
		ctx.master()->addPost(check = new AcyclicityCheck(&graph));
		ctx.endInit();
		REQUIRE((ctx.master()->assume(x1) && ctx.master()->propagate()));

		REQUIRE(!ctx.master()->isFalse(x2));
		ctx.master()->removePost(check);
		check->destroy(ctx.master(), true);
		REQUIRE(!ctx.master()->hasWatch(x1, check));
	}
	SECTION("test Logic Program") {
		Asp::LogicProgram lp;
		lpAdd(lp.start(ctx),
			"{x1;x2}.\n"
			"x3 :- x1.\n"
			"x4 :- x2.\n"
			"#edge (0,1) : x3.\n"
			"#edge (1,0) : x4.\n"
			"% ignore because x5 is false\n"
			"#edge (1,0) : x5.\n");
		REQUIRE(lp.endProgram());
		REQUIRE((ctx.extGraph.get() && ctx.extGraph->nodes() == 2));
		AcyclicityCheck* check;
		ctx.master()->addPost(check = new AcyclicityCheck(0));
		REQUIRE(ctx.endInit());

		ctx.master()->assume(lp.getLiteral(1));
		ctx.master()->propagate();
		REQUIRE(ctx.master()->isFalse(lp.getLiteral(2)));
	}

	SECTION("test Incremental Only New") {
		Asp::LogicProgram lp;
		lp.start(ctx);
		lp.updateProgram();
		lpAdd(lp,
			"{x1;x2}.\n"
			"#edge (0,1) : x1.\n"
			"#edge (1,0) : x2.\n");

		REQUIRE(lp.endProgram());

		AcyclicityCheck* check;
		ctx.master()->addPost(check = new AcyclicityCheck(0));
		REQUIRE(ctx.endInit());

		lp.updateProgram();
		lpAdd(lp,
			"{x3;x4}.\n"
			"#edge (2,3) : x3.\n"
			"#edge (3,2) : x4.\n");

		REQUIRE(lp.endProgram());
		REQUIRE((ctx.extGraph.get() && ctx.extGraph->nodes() == 4));
		REQUIRE(ctx.endInit());
		Literal lit = lp.getLiteral(3);
		REQUIRE(ctx.master()->hasWatch(lit, check));
	}

	SECTION("test Incremental Extend") {
		Asp::LogicProgram lp;
		lp.start(ctx);
		lp.updateProgram();
		lpAdd(lp,
			"{x1;x2}.\n"
			"#edge (1,2) : x1.\n"
			"#edge (2,1) : x2.\n");
		REQUIRE(lp.endProgram());

		AcyclicityCheck* check;
		ctx.master()->addPost(check = new AcyclicityCheck(0));
		REQUIRE(ctx.endInit());

		lp.updateProgram();
		lpAdd(lp, "{x3;x4;x5;x6;x7}.\n"
			"#edge (2,3) : x3.\n"
			"#edge (3,4) : x4.\n"
			"#edge (4,1) : x5.\n"
			"#edge (1,5) : x6.\n"
			"#edge (5,3) : x7.\n");

		REQUIRE(lp.endProgram());
		REQUIRE((ctx.extGraph.get() && ctx.extGraph->edges() == 7));
		REQUIRE(ctx.endInit());
		Var x1 =1, x2 = 2, x3 = 3, x4 = 4, x5 = 5, x6 = 6, x7 = 7;
		Solver& s = *ctx.master();
		REQUIRE((s.assume(lp.getLiteral(x1)) && s.propagate()));
		REQUIRE(s.isFalse(lp.getLiteral(x2)));
		REQUIRE((s.assume(lp.getLiteral(x3)) && s.propagate()));
		REQUIRE((s.assume(lp.getLiteral(x4)) && s.propagate()));
		REQUIRE(s.isFalse(lp.getLiteral(x5)));
		s.undoUntil(0);
		REQUIRE((s.assume(lp.getLiteral(x4)) && s.propagate()));
		REQUIRE((s.assume(lp.getLiteral(x7)) && s.propagate()));
		REQUIRE((s.assume(lp.getLiteral(x5)) && s.propagate()));
		REQUIRE(s.isFalse(lp.getLiteral(x6)));
	}
}
} }
