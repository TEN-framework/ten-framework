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
#include <clasp/parser.h>
#include <clasp/minimize_constraint.h>
#include <clasp/solver.h>
#include <potassco/theory_data.h>
#include <potassco/aspif.h>
#include <potassco/smodels.h>
#include <potassco/string_convert.h>
#include "lpcompare.h"
#include "catch.hpp"
namespace Clasp { namespace Test {

template <class Api>
static bool parse(Api& api, std::istream& str, const ParserOptions& opts = ParserOptions()) {
	ProgramParser& p = api.parser();
	return p.accept(str, opts) && p.parse();
}
VarVec& clear(VarVec& vec) { vec.clear(); return vec; }
static VarVec& operator, (VarVec& vec, int val) {
	vec.push_back(val);
	return vec;
}
static struct Empty {
	template <class T>
	operator Potassco::Span<T> () const { return Potassco::toSpan<T>(); }
} empty;

#define NOT_EMPTY(X,...)
#define SPAN(VEC,...)      NOT_EMPTY(__VA_ARGS__) Potassco::toSpan((clear(VEC) , __VA_ARGS__))

using Potassco::SmodelsOutput;
using Potassco::AspifOutput;
namespace {
struct Input {
	std::stringstream prg;
	void toSmodels(const char* txt, Var falseAt = 0) {
		prg.clear();
		prg.str("");
		SmodelsOutput sm(prg, true, falseAt);
		Potassco::AspifTextInput x(&sm);
		std::stringstream temp; temp << txt;
		REQUIRE(Potassco::readProgram(temp, x, 0) == 0);
	}
	void toAspif(const char* txt) {
		prg.clear();
		prg.str("");
		AspifOutput aspif(prg);
		Potassco::AspifTextInput x(&aspif);
		std::stringstream temp; temp << txt;
		REQUIRE(Potassco::readProgram(temp, x, 0) == 0);
	}
	operator std::stringstream& () { return prg; }
};
}
TEST_CASE("Smodels parser", "[parser][asp]") {
	SharedContext     ctx;
	Input             in;
	Asp::LogicProgram api;
	api.start(ctx, Asp::LogicProgram::AspOptions().noScc());
	SECTION("testEmptySmodels") {
		in.toSmodels("");
		REQUIRE(parse(api, in));
		api.endProgram();
		std::stringstream empty;
		REQUIRE(compareSmodels(empty, api));
	}
	SECTION("testSingleFact") {
		in.toSmodels("x1.");
		REQUIRE(parse(api, in));
		api.endProgram();
		REQUIRE(0u == ctx.numVars());
	}
	SECTION("testComputeStatementAssumptions") {
		in.toSmodels(
			"x1 :- not x2.\n"
			"x2 :- not x3.\n"
			"x3 :- not x4.\n"
			"x4 :- not x3.\n"
			":- x2.\n", 2);
		REQUIRE(parse(api, in));
		api.endProgram() && ctx.endInit();
		REQUIRE(ctx.master()->isTrue(api.getLiteral(3)));
		REQUIRE(ctx.master()->isTrue(api.getLiteral(1)));
		REQUIRE(ctx.master()->isFalse(api.getLiteral(2)));
		REQUIRE(ctx.master()->isFalse(api.getLiteral(4)));
	}
	SECTION("testTransformSimpleConstraintRule") {
		in.toSmodels(
			"x1 :- 2{not x3, x2, x4}.\n"
			"x2 :- not x3.\n"
			"x3 :- not x2.\n"
			"x4 :- not x3.\n");
		api.setExtendedRuleMode(Asp::LogicProgram::mode_transform_weight);
		REQUIRE(parse(api, in));
		REQUIRE(api.endProgram());
		REQUIRE(api.getLiteral(2) == api.getLiteral(1));
		REQUIRE(api.getLiteral(4) == api.getLiteral(1));
		in.toSmodels(
			"x1 :- not x3.\n"
			"x3 :-not x1.\n"
			"x2 :- x1.\n"
			"x4 :- x1.\n");
		REQUIRE(compareSmodels(in, api));
	}
	SECTION("testTransformSimpleWeightRule") {
		in.toSmodels(
			"x1 :- 2 {x2=1, not x3=2, x4=3}.\n"
			"x2 :- not x3.\n"
			"x3 :- not x2.\n"
			"x4 :-not x3.");
		api.setExtendedRuleMode(Asp::LogicProgram::mode_transform_weight);
		REQUIRE(parse(api, in));
		REQUIRE(api.endProgram());
		REQUIRE(1u == ctx.numVars());
		in.toSmodels(
			"x1 :- not x3.\n"
			"x3 :- not x1.\n"
			"x2 :- x1.\n"
			"x4 :- x1.\n");
		REQUIRE(compareSmodels(in, api));
	}
	SECTION("testTransformSimpleChoiceRule") {
		in.toSmodels("{x1;x2;x3} :- x2, not x3.\n"
			"x2 :- not x1.\n"
			"x3 :- not x2.");
		api.setExtendedRuleMode(Asp::LogicProgram::mode_transform_choice);
		REQUIRE(parse(api, in));
		REQUIRE(api.endProgram());
		in.toSmodels("x2. x4.");
		REQUIRE(compareSmodels(in, api));
	}
	SECTION("testSimpleConstraintRule") {
		in.toSmodels("x1 :- 2 {x3, x2, x4}.\n"
			"x2 :- not x3.\n"
			"x3 :- not x2.\n"
			"x4 :- not x3.");
		REQUIRE(parse(api, in));
		REQUIRE(api.endProgram());
		REQUIRE(2u == ctx.numVars());
		in.toSmodels("x1 :- 2 {x3, x2 = 2}.\n"
			"x2 :- not x3.\n"
			"x3 :- not x2.\n"
			"x4 :- x2.");
		REQUIRE(compareSmodels(in, api));
	}
	SECTION("testSimpleWeightRule") {
		in.toSmodels("x1 :- 2 {x3 = 2, x2 = 1, x4 = 3}. % but (x4 = 3 -> x4 = 2).\n"
			"x2 :- not x3.\n"
			"x3 :- not x2.\n"
			"x4 :- not x3.");
		REQUIRE(parse(api, in));
		REQUIRE(api.endProgram());
		REQUIRE(2u == ctx.numVars());
		in.toSmodels("x1 :- 2 {x3 = 2, x2 = 3}.\n"
			"x2 :- not x3.\n"
			"x3 :- not x2.\n"
			"x4 :- x2.");
		REQUIRE(compareSmodels(in, api));
	}
	SECTION("testSimpleChoiceRule") {
		in.toSmodels("{x1;x2;x3} :- x2, not x3.\n"
			"x2 :- not x1.\n"
			"x3 :- not x2.\n");
		REQUIRE(parse(api, in));
		REQUIRE(api.endProgram());
		in.toSmodels("x2.");
		REQUIRE(compareSmodels(in, api));
	}
	SECTION("testMinimizePriority") {
		in.toSmodels("{x1;x2;x3;x4}.\n"
			"#minimize{not x1, not x2}.\n"
			"#minimize{x3, x4}.");
		REQUIRE(parse(api, in));
		REQUIRE(api.endProgram());
		SharedMinimizeData* m1 = ctx.minimize();
		REQUIRE(m1->numRules() == 2);
		REQUIRE(std::find(m1->lits, m1->lits + 2, WeightLiteral(api.getLiteral(3), 0)) != m1->lits + 2);
		REQUIRE(std::find(m1->lits, m1->lits + 2, WeightLiteral(api.getLiteral(4), 0)) != m1->lits + 2);
		REQUIRE(std::find(m1->lits + 2, m1->lits + 4, WeightLiteral(~api.getLiteral(1), 1)) != m1->lits + 4);
		REQUIRE(std::find(m1->lits + 2, m1->lits + 4, WeightLiteral(~api.getLiteral(2), 1)) != m1->lits + 4);
	}
	SECTION("testEdgeDirectives") {
		in.toSmodels("{x1;x2}.\n"
			"#output _edge(0,1)  : x1.\n"
			"#output _acyc_1_1_0 : x2.");
		REQUIRE(parse(api, in, ParserOptions().enableAcycEdges()));
		REQUIRE((api.endProgram() && ctx.endInit()));
		REQUIRE(uint32(2) == ctx.stats().acycEdges);
	}
	SECTION("testHeuristicDirectives") {
		in.toSmodels("{x1;x2;x3}.\n"
			"#output _heuristic(a,true,1) : x1.\n"
			"#output _heuristic(_heuristic(a,true,1),true,1) : x2.\n"
			"#output a : x3.");
		REQUIRE(parse(api, in, ParserOptions().enableHeuristic()));
		REQUIRE((api.endProgram() && ctx.endInit()));
		REQUIRE(ctx.heuristic.size() == 2);
	}
	SECTION("testSimpleIncremental") {
		in.toSmodels(
			"#incremental.\n"
			"{x1;x2} :-x3.\n"
			"#external x3.\n"
			"#step.\n"
			"#external x3.[release]\n"
			"{x3}.");
		api.updateProgram();
		ProgramParser& p = api.parser();
		REQUIRE(p.accept(in));
		REQUIRE(p.parse());
		REQUIRE(api.endProgram());
		REQUIRE(p.more());
		REQUIRE(api.stats.rules[0].sum() == 1);
		REQUIRE(api.isExternal(3));
		api.updateProgram();
		REQUIRE(p.parse());
		REQUIRE(!p.more());
		REQUIRE(api.stats.rules[0].sum() == 1);
		REQUIRE(!api.isExternal(3));
	}
	SECTION("testIncrementalMinimize") {
		in.toSmodels(
			"#incremental.\n"
			"{x1; x2}.\n"
			"#minimize{not x1, not x2}.\n"
			"#step."
			"#minimize{x1, x2}.\n");
		api.updateProgram();
		ProgramParser& p = api.parser();
		REQUIRE(p.accept(in));
		REQUIRE(p.parse());
		REQUIRE(api.endProgram());
		SharedMinimizeData* m1 = ctx.minimize();
		m1->share();
		REQUIRE(std::find(m1->lits, m1->lits + 2, WeightLiteral(~api.getLiteral(1), 1)) != m1->lits + 2);
		REQUIRE(std::find(m1->lits, m1->lits + 2, WeightLiteral(~api.getLiteral(2), 1)) != m1->lits + 2);

		REQUIRE(p.more());
		api.updateProgram();
		REQUIRE(p.parse());
		REQUIRE(!p.more());
		REQUIRE(api.endProgram());
		SharedMinimizeData* m2 = ctx.minimize();
		REQUIRE(m1 != m2);
		REQUIRE(isSentinel(m2->lits[2].first));
		REQUIRE(std::find(m2->lits, m2->lits + 2, WeightLiteral(api.getLiteral(1), 1)) != m2->lits + 2);
		REQUIRE(std::find(m2->lits, m2->lits + 2, WeightLiteral(api.getLiteral(2), 1)) != m2->lits + 2);
		m1->release();
	}
};

static bool sameProgram(Asp::LogicProgram& a, std::stringstream& prg) {
	std::stringstream out;
	AspParser::write(a, out, AspParser::format_aspif);
	prg.clear();
	prg.seekg(0);
	return compareProgram(prg, out);
}

TEST_CASE("Aspif parser", "[parser][asp]") {
	SharedContext     ctx;
	Input             in;
	Asp::LogicProgram api;
	api.start(ctx, Asp::LogicProgram::AspOptions().noScc());
	SECTION("testEmptyProgram") {
		in.toAspif("");
		REQUIRE(parse(api, in));
		REQUIRE(api.endProgram());
		REQUIRE(api.stats.rules[0].sum() == 0);
	}
	SECTION("testSingleFact") {
		in.toAspif("x1.");
		REQUIRE(parse(api, in));
		REQUIRE(api.endProgram());
		REQUIRE(api.stats.rules[0].sum() == 1);
		REQUIRE(api.getLiteral(1) == lit_true());
	}
	SECTION("testSimpleRule") {
		in.toAspif("x1 :- x3, not x2.");
		REQUIRE(parse(api, in));
		REQUIRE(api.stats.rules[0].sum() == 1);
		REQUIRE(api.numAtoms() == 3);
		REQUIRE(api.numBodies() == 1);
		REQUIRE(api.getBody(0)->size() == 2);
		REQUIRE(api.getBody(0)->goal(0) == posLit(3));
		REQUIRE(api.getBody(0)->goal(1) == negLit(2));
		REQUIRE(sameProgram(api, in));
	}
	SECTION("testInvalidAtom") {
		in.toAspif(POTASSCO_FORMAT("x%u :- not x2, x3.", varMax));
		REQUIRE_THROWS_AS(parse(api, in), std::logic_error);
	}
	SECTION("testInvalidLiteral") {
		in.toAspif(POTASSCO_FORMAT("x1 :- not x%u, x3.", varMax));
		REQUIRE_THROWS_AS(parse(api, in), std::logic_error);
	}
	SECTION("testIntegrityConstraint") {
		in.toAspif(":- not x1, x2.");
		REQUIRE(parse(api, in));
		REQUIRE(api.stats.rules[0].sum() == 1);
		REQUIRE(api.numAtoms() == 2);
		REQUIRE(api.numBodies() == 1);
		REQUIRE(api.getBody(0)->size() == 2);
		REQUIRE(api.getBody(0)->goal(0) == posLit(2));
		REQUIRE(api.getBody(0)->goal(1) == negLit(1));
		REQUIRE(api.getBody(0)->value() == value_false);
	}
	SECTION("testDisjunctiveRule") {
		in.toAspif("x1|x2.");
		REQUIRE(parse(api, in));
		REQUIRE(api.stats.rules[0].sum() == 1);
		REQUIRE(api.stats.disjunctions[0] == 1);
		REQUIRE(api.numAtoms() == 2);
		REQUIRE(sameProgram(api, in));
	}
	SECTION("testChoiceRule") {
		in.toAspif("{x1;x2;x3}.");
		REQUIRE(parse(api, in));
		REQUIRE(api.stats.rules[0].sum() == 1);
		REQUIRE(api.stats.rules[0][Asp::RuleStats::Choice] == 1);
		REQUIRE(api.numAtoms() == 3);
		REQUIRE(sameProgram(api, in));
	}
	SECTION("testWeightRule") {
		in.toAspif(
			"x1 :- 2{x2, x3, not x4}.\n"
			"x5 :- 4{x2 = 2, x3, not x4 = 3}.\n");
		REQUIRE(parse(api, in));
		REQUIRE(api.stats.rules[0].sum() == 2);
		REQUIRE(api.stats.bodies[0][Asp::Body_t::Sum] == 1);
		REQUIRE(api.stats.bodies[0][Asp::Body_t::Count] == 1);
		REQUIRE(api.numBodies() == 2);
		REQUIRE((api.getBody(0)->bound() == 2 && !api.getBody(0)->hasWeights()));
		REQUIRE((api.getBody(1)->bound() == 4 && api.getBody(1)->hasWeights()));
		REQUIRE(sameProgram(api, in));
	}
	SECTION("testDisjunctiveWeightRule") {
		in.toAspif("x1|x2 :- 2{x3, x4, not x5}.");
		REQUIRE(parse(api, in));
		REQUIRE(sameProgram(api, in));
	}
	SECTION("testChoiceWeightRule") {
		in.toAspif("{x1;x2} :- 2{x3, x4, not x5}.");
		REQUIRE(parse(api, in));
		REQUIRE(sameProgram(api, in));
	}
	SECTION("testInvalidNegativeWeightInWeightRule") {
		in.toAspif("x1 :- 2 {x2 = -1, not x3 = 1, x4}.");
		REQUIRE_THROWS_AS(parse(api, in), std::logic_error);
	}
	SECTION("testInvalidHeadInWeightRule") {
		in.prg
			<< "asp 1 0 0"
			<< "\n1 0 1 0 1 2 3 2 1 -3 1 4 1"
			<< "\n0\n";
		REQUIRE_THROWS_AS(parse(api, in), std::logic_error);
	}
	SECTION("testNegativeBoundInWeightRule") {
		in.toAspif("x1 :- -1 {x2, not x3, x4}.");
		REQUIRE(parse(api, in));
		REQUIRE(api.stats.rules[0].sum() == 1);
		REQUIRE(api.stats.bodies[0][Asp::Body_t::Sum] == 0);
	}
	SECTION("testMinimizeRule") {
		in.toAspif(
			"{x1;x2;x3}.\n"
			"#minimize{x1 = 2, x2, not x3 = 4}.");
		REQUIRE(parse(api, in));
		REQUIRE(api.stats.rules[0][Asp::RuleStats::Minimize] == 1);
		REQUIRE(sameProgram(api, in));
	}
	SECTION("testMinimizeRuleMergePriority") {
		in.toAspif(
			"#minimize{x1 = 2, x2, not x3 = 4}@1.\n"
			"#minimize{x4 = 2, x2 = 2, x3 = 4}@1.");
		REQUIRE(parse(api, in));
		REQUIRE(api.stats.rules[0].sum() == 1);
		REQUIRE(api.stats.rules[0][Asp::RuleStats::Minimize] == 1);
	}
	SECTION("testMinimizeRuleWithNegativeWeights") {
		in.toAspif("#minimize{x4 = -2, x2 = -1, x3 = 4}.");
		REQUIRE(parse(api, in));
		REQUIRE(api.stats.rules[0].sum() == 1);
		REQUIRE(api.endProgram());
		std::stringstream exp("6 0 2 2 4 2 2 1 ");
		REQUIRE(findSmodels(exp, api));
	}
	SECTION("testIncremental") {
		in.toAspif("#incremental.\n"
			"{x1;x2}.\n"
			"#minimize{x1=2, x2}.\n"
			"#step.\n"
			"{x3}.\n"
			"#minimize{x3=4}.\n");

		REQUIRE(parse(api, in));
		REQUIRE(api.stats.rules[0].sum() == 2);
		REQUIRE(api.endProgram());

		ProgramParser& p = api.parser();
		REQUIRE(p.more());
		api.updateProgram();
		REQUIRE(p.parse());
		REQUIRE_FALSE(p.more());
		REQUIRE(api.stats.rules[0].sum() > 0);
		// minimize rule was merged
		REQUIRE(ctx.minimize()->numRules() == 1);
	}
	SECTION("testIncrementalExternal") {
		in.toAspif("#incremental."
			"x1 :- x2.\n"
			"#external x2. [true]\n"
			"#step.\n"
			"#external x2. [false]\n"
			"#step.\n"
			"#external x2. [release]\n");

		REQUIRE(parse(api, in));
		REQUIRE(api.endProgram());
		LitVec assume;
		api.getAssumptions(assume);
		REQUIRE((assume.size() == 1 && assume[0] == api.getLiteral(2)));

		ProgramParser& p = api.parser();
		REQUIRE(p.more());
		api.updateProgram();
		REQUIRE(p.parse());

		REQUIRE(api.endProgram());
		assume.clear();
		api.getAssumptions(assume);
		REQUIRE((assume.size() == 1 && assume[0] == ~api.getLiteral(2)));

		REQUIRE(p.more());
		api.updateProgram();
		REQUIRE(p.parse());
		REQUIRE(!p.more());
		REQUIRE(api.endProgram());
		assume.clear();
		api.getAssumptions(assume);
		REQUIRE(assume.empty());
		ctx.endInit();
		REQUIRE(ctx.master()->isFalse(api.getLiteral(2)));
	}
	SECTION("testSimpleEdgeDirective") {
		in.toAspif("{x1;x2}."
			"#edge (0,1) : x1.\n"
			"#edge (1,0) : x2.\n");
		REQUIRE(parse(api, in));
		REQUIRE((api.endProgram() && ctx.endInit()));
		REQUIRE(ctx.stats().acycEdges == 2);
		REQUIRE(sameProgram(api, in));
	}
	SECTION("testComplexEdgeDirective") {
		in.toAspif("{x1;x2;x3;x4}."
			"#edge (0,1) : x1, not x2.\n"
			"#edge (1,0) : x3, not x4.\n");
		REQUIRE(parse(api, in));
		REQUIRE((api.endProgram() && ctx.endInit()));
		REQUIRE(ctx.stats().acycEdges == 2);
		REQUIRE(sameProgram(api, in));
	}
	SECTION("testHeuristicDirective") {
		in.toAspif("{x1;x2;x3;x4}."
			"#heuristic x1. [-1@1,sign]\n"
			"#heuristic x1 : x3, not x4. [1@1,factor]");
		REQUIRE(parse(api, in));
		REQUIRE((api.endProgram() && ctx.endInit()));
		REQUIRE(ctx.heuristic.size() == 2);
		REQUIRE(ctx.heuristic.begin()->type() == DomModType::Sign);
		REQUIRE((ctx.heuristic.begin() + 1)->type() == DomModType::Factor);
		REQUIRE(ctx.heuristic.begin()->var() == (ctx.heuristic.begin() + 1)->var());
		REQUIRE(sameProgram(api, in));
	}

	SECTION("testOutputDirective") {
		in.toAspif("{x1;x2}."
			"#output fact.\n"
			"#output conj : x1, x2.");
		REQUIRE(parse(api, in));
		REQUIRE((api.endProgram() && ctx.endInit()));
		REQUIRE(ctx.output.size() == 2);
		REQUIRE(ctx.output.numFacts() == 1);
		REQUIRE(sameProgram(api, in));
	}
	SECTION("testAssumptionDirective") {
		in.toAspif("{x1;x2}."
			"#assume{not x2, x1}.");
		REQUIRE(parse(api, in));
		REQUIRE(api.endProgram());
		LitVec assume;
		api.getAssumptions(assume);
		REQUIRE(assume.size() == 2);
		REQUIRE(std::find(assume.begin(), assume.end(), ~api.getLiteral(2)) != assume.end());
		REQUIRE(std::find(assume.begin(), assume.end(), api.getLiteral(1)) != assume.end());
		REQUIRE(sameProgram(api, in));
	}
	SECTION("testProjectionDirective") {
		in.toAspif("{x1;x2;x3;x4}."
			"#output a : x1.\n"
			"#output b : x2.\n"
			"#output c : x3.\n"
			"#output d : x4.\n"
			"#project{x1, x3}.");
		REQUIRE(parse(api, in));
		REQUIRE(api.endProgram());
		REQUIRE(ctx.output.size() == 4);
		Literal a = api.getLiteral(1);
		Literal b = api.getLiteral(2);
		Literal c = api.getLiteral(3);
		Literal d = api.getLiteral(4);
		OutputTable::lit_iterator proj_begin = ctx.output.proj_begin(), proj_end = ctx.output.proj_end();
		REQUIRE(std::find(proj_begin, proj_end, a) != proj_end);
		REQUIRE_FALSE(std::find(proj_begin, proj_end, b) != proj_end);
		REQUIRE(std::find(proj_begin, proj_end, c) != proj_end);
		REQUIRE_FALSE(std::find(proj_begin, proj_end, d) != proj_end);
		REQUIRE(sameProgram(api, in));
	}
	SECTION("testEmptyProjectionDirective") {
		in.toAspif("{x1;x2;x3;x4}."
			"#project.");
		REQUIRE(parse(api, in));
		REQUIRE(api.endProgram());
		REQUIRE(ctx.output.projectMode() == ProjectMode_t::Explicit);
		REQUIRE(sameProgram(api, in));
	}
	SECTION("testIgnoreCommentDirective") {
		in.prg
			<< "asp 1 0 0"
			<< "\n" << Potassco::Directive_t::Comment << " Ignore me!"
			<< "\n0\n";
		REQUIRE(parse(api, in));
	}
	SECTION("testReadTheoryAtoms") {
		AspifOutput aspif(in.prg);
		aspif.initProgram(false);
		aspif.beginStep();
		aspif.theoryTerm(0, 1);                     // (number 1)
		aspif.theoryTerm(1, 2);                     // (number 2)
		aspif.theoryTerm(2, 3);                     // (number 3)
		aspif.theoryTerm(3, 4);                     // (number 4)
		aspif.theoryTerm(4, Potassco::toSpan("x")); // (string x)
		aspif.theoryTerm(5, Potassco::toSpan("z")); // (string z)
		aspif.theoryTerm(6, Potassco::toSpan("+")); // (string +)
		aspif.theoryTerm(7, Potassco::toSpan("*")); // (string *)
		VarVec ids;
		aspif.theoryTerm(8,  4, SPAN(ids, 0));         // (function x(1))
		aspif.theoryTerm(9,  4, SPAN(ids, 1));         // (function x(2))
		aspif.theoryTerm(10, 4, SPAN(ids, 2));         // (function x(3))
		aspif.theoryTerm(11, 7, SPAN(ids, 0, 8));      // (term 1*x(1))
		aspif.theoryTerm(12, 7, SPAN(ids, 1, 9));      // (term 2*x(2))
		aspif.theoryTerm(13, 7, SPAN(ids, 2, 10));     // (term 3*x(3))
		aspif.theoryTerm(14, 7, SPAN(ids, 3, 5));      // (term 3*x(3))

		aspif.theoryElement(0, SPAN(ids, 11), empty);      // (element 1*x(1):)
		aspif.theoryElement(1, SPAN(ids, 12), empty);      // (element 2*x(2):)
		aspif.theoryElement(2, SPAN(ids, 13), empty);      // (element 3*x(3):)
		aspif.theoryElement(3, SPAN(ids, 14), empty);      // (element 4*z:)
		aspif.theoryTerm(15, Potassco::toSpan("sum"));    // (string sum)
		aspif.theoryTerm(16, Potassco::toSpan(">="));     // (string >=)
		aspif.theoryTerm(17, 42);                         // (number 42)
		aspif.theoryAtom(1, 15, SPAN(ids, 0, 1, 2, 3), 16, 17); // (&sum { 1*x(1); 2*x(2); 3*x(3); 4*z     } >= 42)
		aspif.endStep();
		REQUIRE(parse(api, in));

		Potassco::TheoryData& t = api.theoryData();
		REQUIRE(t.numAtoms() == 1);

		const Potassco::TheoryAtom& a = **t.begin();
		REQUIRE(a.atom() == 1);
		REQUIRE(std::strcmp(t.getTerm(a.term()).symbol(), "sum") == 0);
		REQUIRE(a.size() == 4);
		for (Potassco::TheoryAtom::iterator it = a.begin(), end = a.end(); it != end; ++it) {
			const Potassco::TheoryElement& e = t.getElement(*it);
			REQUIRE(e.size() == 1);
			REQUIRE(Literal::fromId(e.condition()) == lit_true());
			REQUIRE(t.getTerm(*e.begin()).type() == Potassco::Theory_t::Compound);
		}
		REQUIRE(std::strcmp(t.getTerm(*a.guard()).symbol(), ">=") == 0);
		REQUIRE(t.getTerm(*a.rhs()).number() == 42);


		struct BuildStr : public Potassco::TheoryAtomStringBuilder {
			virtual Potassco::LitSpan getCondition(Potassco::Id_t) const { return Potassco::toSpan<Potassco::Lit_t>(); }
			virtual std::string       getName(Potassco::Atom_t)    const { return ""; }
		} builder;

		std::string n = builder.toString(t, a);
		REQUIRE(n == "&sum{1 * x(1); 2 * x(2); 3 * x(3); 4 * z} >= 42");
	}

	SECTION("testWriteTheoryAtoms") {
		VarVec ids;
		AspifOutput aspif(in.prg);
		aspif.initProgram(true);
		aspif.beginStep();
		aspif.rule(Potassco::Head_t::Choice, SPAN(ids, 1), empty);
		aspif.theoryTerm(0, 1);                      // (number 1)
		aspif.theoryTerm(1, Potassco::toSpan("x"));  // (string x)
		aspif.theoryTerm(3, Potassco::toSpan("foo"));// (string foo)
		aspif.theoryTerm(2, 1, SPAN(ids, 0));         // (function x(1))
		aspif.theoryElement(0, SPAN(ids, 2), empty);  // (element x(1):)
		aspif.theoryAtom(1, 3, SPAN(ids, 0));         // (&foo { x(1); })
		aspif.endStep();
		std::stringstream step1;
		step1 << in.prg.str();
		aspif.beginStep();
		aspif.rule(Potassco::Head_t::Choice, SPAN(ids, 2), empty);
		aspif.theoryAtom(2, 3, SPAN(ids, 0));              // (&foo { x(1); })
		aspif.endStep();
		REQUIRE(parse(api, in));
		REQUIRE((api.endProgram() && api.theoryData().numAtoms() == 1));
		std::stringstream out;
		AspParser::write(api, out);
		REQUIRE(findProgram(step1, out));
		ProgramParser& p = api.parser();
		REQUIRE(p.more());
		api.updateProgram();
		REQUIRE(api.theoryData().numAtoms() == 0);
		REQUIRE(p.parse());
		REQUIRE((api.endProgram() && api.theoryData().numAtoms() == 1));
		AspParser::write(api, out);
	}
	SECTION("testTheoryElementWithCondition") {
		VarVec ids;
		AspifOutput aspif(in.prg);
		aspif.initProgram(false);
		aspif.beginStep();
		aspif.rule(Potassco::Head_t::Choice, SPAN(ids, 1, 2), empty);
		aspif.theoryTerm(0, 1); // (number 1)
		aspif.theoryTerm(1, Potassco::toSpan("foo"));
		Potassco::Lit_t lits[2] = {1, -2};
		aspif.theoryElement(0, SPAN(ids, 0), Potassco::toSpan(lits, 2));
		aspif.theoryAtom(0, 1, SPAN(ids, 0));
		aspif.endStep();
		REQUIRE(parse(api, in));
		in.prg.clear();
		in.prg.seekg(0);
		Potassco::TheoryData& t = api.theoryData();
		REQUIRE(t.numAtoms() == 1);
		const Potassco::TheoryAtom& a = **t.begin();
		REQUIRE(a.size() == 1);
		const Potassco::TheoryElement& e = t.getElement(*a.begin());
		REQUIRE(e.condition() != 0);
		Potassco::LitVec cond;
		api.extractCondition(e.condition(), cond);
		REQUIRE(cond.size() == 2);
		std::stringstream out;
		REQUIRE(api.endProgram());
		AspParser::write(api, out);
		REQUIRE(findProgram(out, in));
	}
}

TEST_CASE("Dimacs parser", "[parser][sat]") {
	SharedContext ctx;
	SatBuilder    api;
	api.startProgram(ctx);
	std::stringstream prg;
	SECTION("testDimacs") {
		prg << "c simple test case\n"
		    << "p cnf 4 3\n"
		    << "1 2 0\n"
		    << "3 4 0\n"
		    << "-1 -2 -3 -4 0\n";
		REQUIRE((parse(api, prg) && api.endProgram()));
		REQUIRE(ctx.numVars() == 4);
		REQUIRE(ctx.numConstraints() == 3);
	}

	SECTION("testDimacsDontAddTaut") {
		prg << "c simple test case\n"
		    << "p cnf 4 4\n"
		    << "1 2 0\n"
		    << "3 4 0\n"
		    << "-1 -2 -3 -4 0\n"
		    << "1 -2 -3 2 0\n";
		REQUIRE((parse(api, prg) && api.endProgram()));
		REQUIRE(ctx.numVars() == 4);
		REQUIRE(ctx.numConstraints() == 3);
	}

	SECTION("testDimacsDontAddDupLit") {
		prg << "c simple test case\n"
		    << "p cnf 2 4\n"
		    << "1 2 2 1 0\n"
		    << "1 2 1 2 0\n"
		    << "-1 -2 -1 0\n"
		    << "-2 -1 -2 0\n";
		REQUIRE((parse(api, prg) && api.endProgram()));
		REQUIRE(ctx.numVars() == 2);
		REQUIRE(ctx.numConstraints() == 4);
		REQUIRE(ctx.numBinary() == 4);
	}

	SECTION("testDimacsBadVars") {
		prg << "p cnf 1 1\n"
		    << "1 2 0\n";
		REQUIRE_THROWS_AS(parse(api, prg), std::logic_error);
	}

	SECTION("testWcnf") {
		prg << "c comments Weighted Max-SAT\n"
		    << "p wcnf 3 5\n"
		    << "10 1 -2 0\n"
		    << "3 -1 2 -3 0\n"
		    << "8 -3 2 0\n"
		    << "5 1 3 0\n"
		    << "2 3 0\n";
		REQUIRE((parse(api, prg) && api.endProgram()));
		REQUIRE(ctx.numVars() == 7);
		REQUIRE(ctx.output.size() == 3);
		REQUIRE(ctx.numConstraints() == 4);

		SharedMinimizeData* wLits = ctx.minimize();
		REQUIRE(wLits->numRules() == 1);
		REQUIRE(wLits->lits[0].second == 10);
		REQUIRE(wLits->lits[1].second == 8);
		REQUIRE(wLits->lits[2].second == 5);
		REQUIRE(wLits->lits[3].second == 3);
	}

	SECTION("testPartialWcnf") {
		prg << "c comments Weigthed Partial Max-SAT\n"
		    << "p wcnf 4 5 16\n"
		    << "16 1 -2 4 0\n"
		    << "16 -1 -2 3 0\n"
		    << "8 -2 -4 0\n"
		    << "4 -3 2 0\n"
		    << "1 1 3 0\n";
		REQUIRE((parse(api, prg) && api.endProgram()));
		REQUIRE(ctx.numVars() == 7);
		REQUIRE(ctx.output.size() == 4);
		REQUIRE(ctx.numConstraints() == 5);
		SharedMinimizeData* wLits = ctx.minimize();;
		REQUIRE(wLits->numRules() == 1);
		REQUIRE(wLits->lits[0].second == 8);
		REQUIRE(wLits->lits[1].second == 4);
		REQUIRE(wLits->lits[2].second == 1);
	}
	SECTION("testDimacsExtSupportsGraph") {
		prg
			<< "p cnf 4 3\n"
			<< "c graph 2\n"
			<< "c node 1 1\n"
			<< "c node 2 1\n"
			<< "c arc 1 1 2\n"
			<< "c arc 2 2 1\n"
			<< "c endgraph\n"
			<< "1 2 0\n"
			<< "3 4 0\n"
			<< "-1 -2 -3 -4 0\n";
		REQUIRE((parse(api, prg, ParserOptions().enableAcycEdges()) && api.endProgram() && ctx.endInit()));
		REQUIRE(uint32(2) == ctx.stats().acycEdges);
	}
	SECTION("testDimacsExtSupportsCostFunc") {
		prg
			<< "p cnf 4 3\n"
			<< "c minweight 1 2 2 4 -3 1 -4 2 0\n"
			<< "1 2 0\n"
			<< "3 4 0\n"
			<< "-1 -2 -3 -4 0\n";
		REQUIRE((parse(api, prg, ParserOptions().enableMinimize()) && api.endProgram()));
		REQUIRE(ctx.hasMinimize());
		SharedMinimizeData* wLits = ctx.minimize();;
		REQUIRE(wLits->numRules() == 1);
		REQUIRE(wLits->lits[0] == WeightLiteral(posLit(2), 4));
		REQUIRE(wLits->lits[1] == WeightLiteral(posLit(1), 2));
		REQUIRE(wLits->lits[2] == WeightLiteral(negLit(4), 2));
		REQUIRE(wLits->lits[3] == WeightLiteral(negLit(3), 1));
	}
	SECTION("testDimacsExtSupportsProject") {
		prg
			<< "p cnf 4 3\n"
			<< "c project 1 4\n"
			<< "1 2 0\n"
			<< "3 4 0\n"
			<< "-1 -2 -3 -4 0\n";
		REQUIRE((parse(api, prg, ParserOptions().enableProject()) && api.endProgram()));
		REQUIRE(ctx.output.projectMode() == ProjectMode_t::Explicit);
		REQUIRE(std::find(ctx.output.proj_begin(), ctx.output.proj_end(), posLit(1)) != ctx.output.proj_end());
		REQUIRE(std::find(ctx.output.proj_begin(), ctx.output.proj_end(), posLit(4)) != ctx.output.proj_end());
	}
	SECTION("testDimacsExtSupportsHeuristic") {
		prg
			<< "p cnf 4 0\n"
			<< "c heuristic 4 1 1 0 0\n"
			<< "c heuristic 5 2 1 0 0\n"
			<< "c heuristic 5 3 1 0 -1\n";
		REQUIRE((parse(api, prg, ParserOptions().enableHeuristic()) && api.endProgram()));
		REQUIRE(ctx.heuristic.size() == 3);
		DomainTable::iterator it, end = ctx.heuristic.end();
		for (it = ctx.heuristic.begin(); it != end && it->var() != 3; ++it) { ;  }
		REQUIRE(it != end);
		REQUIRE(it->bias() == 1);
		REQUIRE(it->prio() == 0);
		REQUIRE(it->type() == Potassco::Heuristic_t::False);
		REQUIRE(it->cond() == negLit(1));
	}
	SECTION("testDimacsExtSupportsAssumptions") {
		prg
			<< "p cnf 4 0\n"
			<< "c assume 1 -2 3\n";
		REQUIRE((parse(api, prg, ParserOptions().enableAssume()) && api.endProgram()));
		LitVec ass;
		api.getAssumptions(ass);
		REQUIRE(ass.size() == 3);
		REQUIRE(ass[0] == posLit(1));
		REQUIRE(ass[1] == negLit(2));
		REQUIRE(ass[2] == posLit(3));
	}
	SECTION("testDimacsExtSupportsOutputRange") {
		prg
			<< "p cnf 4 0\n"
			<< "c output range 2 3\n";
		REQUIRE((parse(api, prg, ParserOptions().enableOutput()) && api.endProgram()));
		REQUIRE(*ctx.output.vars_begin()   == 2);
		REQUIRE(*(ctx.output.vars_end()-1) == 3);
	}
	SECTION("testDimacsExtSupportsOutputTable") {
		prg
			<< "p cnf 4 0\n"
			<< "c output 1  var(1)      \n"
			<< "c output -2 not_var(2)  \n"
			<< "c 0\n";
		REQUIRE((parse(api, prg, ParserOptions().enableOutput()) && api.endProgram()));
		REQUIRE(ctx.output.vars_begin() == ctx.output.vars_end());
		REQUIRE(ctx.output.size() == 2);
		for (OutputTable::pred_iterator it = ctx.output.pred_begin(), end = ctx.output.pred_end(); it != end; ++it) {
			REQUIRE((
				(it->name.c_str() == std::string("var(1)") && it->cond == posLit(1))
				||
				(it->name.c_str() == std::string("not_var(2)") && it->cond == negLit(2))
				));
		}
	}
}

TEST_CASE("Opb parser", "[parser][pb]") {
	SharedContext ctx;
	PBBuilder     api;
	api.startProgram(ctx);
	std::stringstream prg;

	SECTION("testBadVar") {
		prg << "* #variable= 1 #constraint= 1\n+1 x2 >= 1;\n";
		REQUIRE_THROWS_AS(parse(api, prg), std::logic_error);
	}
	SECTION("testBadVarInGraph") {
		prg << "* #variable= 1 #constraint= 1\n"
		    << "* graph 2\n"
		    << "* arc 1 1 2\n"
		    << "* arc 2 2 1\n"
		    << "* endgraph\n";
		REQUIRE_THROWS_AS(parse(api, prg, ParserOptions().enableAcycEdges()), std::logic_error);
	}
	SECTION("testWBO") {
		prg << "* #variable= 1 #constraint= 2 #soft= 2 mincost= 2 maxcost= 3 sumcost= 5\n"
		    << "soft: 6 ;\n"
		    << "[2] +1 x1 >= 1 ;\n"
		    << "[3] -1 x1 >= 0 ;\n";
		REQUIRE((parse(api, prg) && api.endProgram()));
		REQUIRE(ctx.numVars() == 3);
		REQUIRE((ctx.numConstraints() == 0 || ctx.numConstraints() == 2));
		REQUIRE(ctx.output.size() == 1);
		SumVec bound;
		api.getWeakBounds(bound);
		REQUIRE((bound.size() == 1 && bound[0] == 5));
		SharedMinimizeData* wLits = ctx.minimize();
		REQUIRE((wLits && wLits->numRules() == 1));
		REQUIRE(wLits->adjust(0) == 2);
	}

	SECTION("testNLC") {
		prg << "* #variable= 5 #constraint= 4 #product= 5 sizeproduct= 13\n"
		    << "1 x1 +4 x1 ~x2 -2 x5 >=1;\n"
		    << "-1 x1 +4 x2 -2 x5 >= 3;\n"
		    << "10 x4 +4 x3 >= 10;\n"
		    << "2 x2 x3 +3 x4 ~x5 +2 ~x1 x2 +3 ~x1 x2 x3 ~x4 ~x5 >= 1 ;\n";
		REQUIRE((parse(api, prg) && api.endProgram()));
		REQUIRE(ctx.numVars() == 10);
		REQUIRE(ctx.numConstraints() >= 4);
		REQUIRE(ctx.output.size() == 5);
	}

	SECTION("testNLCUnsorted") {
		prg << "* #variable= 4 #constraint= 2 #product= 2 sizeproduct= 8\n"
		    << "1 x1 +1 x2 x1 >=1;\n"
		    << "1 x1 +1 x2 x3 x4 ~x4 x2 x3 >=1;\n";
		REQUIRE((parse(api, prg) && api.endProgram()));
		REQUIRE(ctx.numVars() == 6);
		REQUIRE(ctx.master()->isTrue(posLit(1)));
		REQUIRE(ctx.master()->isFalse(posLit(6)));
	}

	SECTION("testPBEqualityBug") {
		prg << "* #variable= 4 #constraint= 2\n"
		    << "+1 x1 = 1;\n"
		    << "+1 x1 +1 x2 +1 x3 +1 x4 = 1;\n";
		REQUIRE((parse(api, prg) && api.endProgram()));
		REQUIRE(ctx.master()->isTrue(posLit(1)));
		REQUIRE(ctx.master()->isFalse(posLit(2)));
		REQUIRE(ctx.master()->isFalse(posLit(3)));
		REQUIRE(ctx.master()->isFalse(posLit(4)));
	}
	SECTION("testPBProject") {
		prg << "* #variable= 6 #constraint= 0\n"
			<< "* project x1 x2\n"
			<< "* project x4\n";
		REQUIRE((parse(api, prg, ParserOptions().enableProject()) && api.endProgram()));
		REQUIRE(ctx.output.projectMode() == ProjectMode_t::Explicit);
		REQUIRE(std::distance(ctx.output.proj_begin(), ctx.output.proj_end()) == 3u);
	}
	SECTION("testPBAssume") {
		prg << "* #variable= 6 #constraint= 0\n"
			  << "* assume x1 -x5\n";
		REQUIRE((parse(api, prg, ParserOptions().enableAssume()) && api.endProgram()));
		LitVec ass;
		api.getAssumptions(ass);
		REQUIRE(ass.size() == 2);
		REQUIRE(ass[0] == posLit(1));
		REQUIRE(ass[1] == negLit(5));
	}
	SECTION("testPBOutput") {
		prg << "* #variable= 6 #constraint= 0\n"
			<< "* output range x2 x4\n";
		REQUIRE((parse(api, prg, ParserOptions().enableOutput()) && api.endProgram()));
		REQUIRE(*ctx.output.vars_begin()  == 2);
		REQUIRE(*(ctx.output.vars_end()-1)== 4);
	}
	SECTION("testPBOutputTable") {
		prg << "* #variable= 6 #constraint= 0\n"
		  << "* output x1 var(1)\n"
		  << "* output -x2 not_var(2)\n"
		  << "* 0\n";
		REQUIRE((parse(api, prg, ParserOptions().enableOutput()) && api.endProgram()));
		REQUIRE(ctx.output.vars_begin() == ctx.output.vars_end());
		REQUIRE(ctx.output.size() == 2);
		for (OutputTable::pred_iterator it = ctx.output.pred_begin(), end = ctx.output.pred_end(); it != end; ++it) {
			REQUIRE((
				(it->name.c_str() == std::string("var(1)") && it->cond == posLit(1))
				||
				(it->name.c_str() == std::string("not_var(2)") && it->cond == negLit(2))
				));
		}
	}
}
} }
