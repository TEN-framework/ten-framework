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
#include <clasp/satelite.h>
#include <clasp/program_builder.h>
#include <clasp/solver.h>
#include <clasp/clause.h>
#include <clasp/parser.h>
#include <algorithm>
#include <sstream>
#include "catch.hpp"
#ifdef _MSC_VER
#pragma warning (disable : 4267) //  conversion from 'size_t' to unsigned int
#pragma once
#endif


namespace Clasp { namespace Test {

TEST_CASE("SatElite preprocessor", "[sat]") {
	SharedContext ctx;
	SatElite	    pre;
	LitVec cl;
	for (int i = 0; i < 10; ++i) {
		ctx.addVar(Var_t::Atom);
	}
	BasicSatConfig opts;
	opts.satPre.type = SatPreParams::sat_pre_ve_bce;
	ctx.startAddConstraints();
	SECTION("testDontAddSatClauses") {
		cl.push_back(posLit(1)); cl.push_back(posLit(2));
		pre.addClause(cl);
		ctx.addUnary(posLit(1));
		REQUIRE(pre.preprocess(ctx, opts.satPre));
		REQUIRE(0u == ctx.numConstraints());
	}
	SECTION("testSimpleSubsume") {
		cl.push_back(posLit(1)); cl.push_back(posLit(2));
		pre.addClause(cl);
		cl.push_back(posLit(3));
		pre.addClause(cl);
		REQUIRE(pre.preprocess(ctx, opts.satPre));
		REQUIRE(0u == ctx.numConstraints());
	}

	SECTION("testSimpleStrengthen") {
		cl.push_back(posLit(1)); cl.push_back(posLit(2));
		pre.addClause(cl);
		cl[0] = negLit(1);
		pre.addClause(cl);
		REQUIRE(pre.preprocess(ctx, opts.satPre));
		REQUIRE(0u == ctx.numConstraints());
	}

	SECTION("testClauseCreatorAddsToPreprocessor") {
		ctx.setConfiguration(&opts, Ownership_t::Retain);
		ctx.setPreserveModels();
		ClauseCreator nc(ctx.master());
		nc.start().add(posLit(1)).add(posLit(2)).end();
		REQUIRE(0u == ctx.numConstraints());
		ctx.endInit();
		REQUIRE(1u == ctx.numConstraints());
		REQUIRE(1u == ctx.numBinary());
	}
	SECTION("testElimPureLits") {
		cl.push_back(posLit(1)); cl.push_back(posLit(2));
		pre.addClause(cl);
		opts.satPre.disableBce();
		pre.preprocess(ctx, opts.satPre);
		REQUIRE(0u == ctx.numConstraints());
		REQUIRE(ctx.eliminated(1) == true);
	}
	SECTION("testDontElimPureLits") {
		cl.push_back(posLit(1)); cl.push_back(posLit(2));
		pre.addClause(cl);
		ctx.setPreserveModels(true);
		pre.preprocess(ctx, opts.satPre);
		REQUIRE(1u == ctx.numConstraints());
		REQUIRE(ctx.eliminated(1) == false);
	}

	SECTION("with program") {
		SharedContext ctx2;
		std::stringstream prg;
		SatBuilder api;
		SECTION("testDimacs") {
			opts.satPre.disableBce();
			ctx2.setConfiguration(&opts, Ownership_t::Retain);
			api.startProgram(ctx2);
			prg << "c simple test case\n"
				<< "p cnf 2 4\n"
				<< "1 2 2 1 0\n"
				<< "1 2 1 2 0\n"
				<< "-1 -2 -1 0\n"
				<< "-2 -1 -2 0\n";
			REQUIRE((api.parseProgram(prg) && api.endProgram()));
			REQUIRE((ctx2.numVars() == 2 && ctx2.master()->numAssignedVars() == 0));
			REQUIRE(ctx2.endInit());
			REQUIRE(ctx2.eliminated(2));
		}

		SECTION("testFreeze") {
			ctx2.setConfiguration(&opts, Ownership_t::Retain);
			api.startProgram(ctx2);
			prg << "c simple test case\n"
				<< "p cnf 2 2\n"
				<< "1 2 0\n"
				<< "-1 -2 0\n";
			REQUIRE(api.parseProgram(prg));
			ctx2.setFrozen(1, true);
			ctx2.setFrozen(2, true);
			REQUIRE(ctx2.numVars() == 2);
			REQUIRE(ctx2.endInit());
			REQUIRE(ctx2.numConstraints() == 2);
			REQUIRE(ctx2.numBinary() == 2);
		}
	}
	SECTION("with model") {
		SharedContext ctx2;
		ctx2.setPreserveModels(true);
		opts.satPre.disableBce();
		ctx2.setConfiguration(&opts, Ownership_t::Retain);
		ctx2.addVar(Var_t::Atom);
		ctx2.addVar(Var_t::Atom);
		ctx2.addVar(Var_t::Atom);
		SECTION("test expand") {
			ctx2.startAddConstraints();
			ClauseCreator nc(ctx2.master());
			nc.start().add(negLit(1)).add(posLit(2)).end();
			nc.start().add(posLit(1)).add(negLit(2)).end();
			nc.start().add(posLit(2)).add(negLit(3)).end();
			ctx2.endInit();
			REQUIRE(1u == ctx2.numConstraints());
			REQUIRE((ctx2.eliminated(1) == true && ctx2.numEliminatedVars() == 1));
			// Eliminated vars are initially assigned
			REQUIRE(value_free != ctx2.master()->value(1));
			ctx2.master()->assume(negLit(2)) && ctx2.master()->propagate();
			ctx2.master()->search(-1, -1, 1.0);
			// negLit(2) -> negLit(1) -> 1 == F
			REQUIRE(value_false == ctx2.master()->model[1]);
		}
		SECTION("text expand more") {
			ctx2.addVar(Var_t::Atom);
			ctx2.startAddConstraints();
			ClauseCreator nc(ctx2.master());

			nc.start().add(posLit(1)).add(posLit(3)).add(posLit(2)).end();
			nc.start().add(negLit(1)).add(posLit(3)).add(negLit(2)).end();

			nc.start().add(posLit(2)).add(posLit(3)).add(posLit(4)).end();
			nc.start().add(negLit(2)).add(posLit(3)).add(negLit(4)).end();

			ctx2.endInit();
			ctx2.master()->assume(posLit(3));
			ctx2.master()->search(-1, -1, 1.0);
			uint32 n = 1;
			LitVec sym = ctx2.master()->symmetric();
			while (!sym.empty()) {
				++n;
				ctx2.satPrepro->extendModel(ctx2.master()->model, sym);
			}
			REQUIRE(n == 4);
		}
	}
}
} }
