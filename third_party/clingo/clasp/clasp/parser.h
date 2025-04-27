//
// Copyright (c) 2014-2017 Benjamin Kaufmann
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
#ifndef CLASP_PARSER_H_INCLUDED
#define CLASP_PARSER_H_INCLUDED

#ifdef _MSC_VER
#pragma once
#endif

#include <istream>
#include <stdexcept>
#include <clasp/claspfwd.h>
#include <clasp/literal.h>
#include <clasp/util/misc_types.h>
#include <potassco/match_basic_types.h>
/*!
 * \file
 * \brief Defines parsers for supported input formats.
 */
namespace Clasp {
/////////////////////////////////////////////////////////////////////////////////////////
// PARSING BASE
/////////////////////////////////////////////////////////////////////////////////////////
/*!
 * \addtogroup problem
 */
//@{

//! Auto-detect type of program given in prg.
ProblemType detectProblemType(std::istream& prg);

//! Parse additional information in symbol table/comments.
struct ParserOptions {
	//! Supported parser extensions.
	enum Extension {
		parse_heuristic = 1u, //!< Parse heuristic info in smodels, dimacs, and pb format.
		parse_acyc_edge = 2u, //!< Parse acyc info in smodels, dimacs, and pb format.
		parse_minimize  = 4u, //!< Parse cost function in dimacs format.
		parse_project   = 8u, //!< Parse project directive in dimacs and pb format.
		parse_assume    = 16u,//!< Parse assumption directive in dimacs and pb format.
		parse_output    = 32u,//!< Parse output directive in dimacs and pb format.
		parse_full      = 63u,
		parse_maxsat    = 128u//!< Parse dimacs as MaxSAT problem
	};
	ParserOptions() : set(0) {}
	ParserOptions& enableHeuristic()  { set |= parse_heuristic; return *this; }
	ParserOptions& enableAcycEdges()  { set |= parse_acyc_edge; return *this; }
	ParserOptions& enableMinimize()   { set |= parse_minimize; return *this; }
	ParserOptions& enableProject()    { set |= parse_project; return *this; }
	ParserOptions& enableAssume()     { set |= parse_assume;  return *this; }
	ParserOptions& enableOutput()     { set |= parse_output;  return *this; }
	ParserOptions& assign(uint8 f, bool b) {
		if (b) { set |= f; }
		else   { set &= ~f; }
		return *this;
	}
	bool isEnabled(Extension e) const { return (set & static_cast<uint8>(e)) != 0u; }
	bool anyOf(uint8 f) const         { return (set & f) != 0u; }
	uint8 set;
};
//! Base class for parsers.
class ProgramParser {
public:
	typedef Potassco::ProgramReader StrategyType;
	static const Var VAR_MAX = varMax - 1;
	ProgramParser();
	virtual ~ProgramParser();
	bool accept(std::istream& str, const ParserOptions& o = ParserOptions());
	bool incremental() const;
	bool isOpen() const;
	bool parse();
	bool more();
	void reset();
private:
	virtual StrategyType* doAccept(std::istream& str, const ParserOptions& o) = 0;
	StrategyType* strat_;
};

//! Parser for logic programs in smodels-internal or aspif format.
class AspParser : public ProgramParser {
public:
	static bool accept(char c);
	explicit AspParser(Asp::LogicProgram& prg);
	~AspParser();
	enum Format { format_smodels = -1, format_aspif = 1 };
	static void write(Asp::LogicProgram& prg, std::ostream& os);
	static void write(Asp::LogicProgram& prg, std::ostream& os, Format f);
protected:
	virtual StrategyType* doAccept(std::istream& str, const ParserOptions& o);
private:
	struct SmAdapter;
	Asp::LogicProgram*         lp_;
	StrategyType*              in_;
	Potassco::AbstractProgram* out_;
};
/////////////////////////////////////////////////////////////////////////////////////////
// SAT parsing
/////////////////////////////////////////////////////////////////////////////////////////
//! Base class for dimacs and opb parser.
class SatReader : public Potassco::ProgramReader {
public:
	SatReader();
	ParserOptions options;
protected:
	bool skipLines(char start);
	void parseExt(const char* pre, uint32 maxVar, SharedContext& ctx);
	// <project> ::= { <var> } <EOL>
	void parseProject(uint32 maxVar, SharedContext& ctx);
	// <assume> ::= { <literal> } <EOL>
	void parseAssume(uint32 maxVar);
	// <heuristic> ::= <type> <var> <bias> <prio> <literal_condition>
	void parseHeuristic(uint32 maxVar, SharedContext& ctx);
	// <output> ::= "range" <var_lo> <var_hi>
	//           |  <literal_condition> <string> <EOL>
	void parseOutput(uint32 maxVar, SharedContext& ctx);
	void parseGraph(uint32 maxVar, const char* pre, ExtDepGraph& graph);
	virtual void addObjective(const WeightLitVec& vec) = 0;
	virtual void addAssumption(Literal x) = 0;
private:
	Literal matchLit(Var max);
};
//! Parser for (extended) dimacs format.
class DimacsReader : public SatReader {
public:
	static bool accept(char c) { return c == 'c' || c == 'p'; }
	DimacsReader(SatBuilder&);
protected:
	virtual bool doAttach(bool& inc);
	virtual bool doParse();
	virtual void addObjective(const WeightLitVec& vec);
	virtual void addAssumption(Literal x);
private:
	SatBuilder* program_;
	Var         numVar_;
	bool        wcnf_;
};
//! Parser for opb format.
class OpbReader : public SatReader {
public:
	OpbReader(PBBuilder&);
	static bool accept(char c) { return c == '*'; }
protected:
	virtual bool doAttach(bool& inc);
	virtual bool doParse();
	virtual void addObjective(const WeightLitVec& vec);
	virtual void addAssumption(Literal x);
	void parseOptObjective();
	void parseConstraint();
	void parseSum();
	void parseTerm();
private:
	PBBuilder* program_;
	weight_t   minCost_;
	weight_t   maxCost_;
	struct Temp {
		WeightLitVec lits;
		LitVec       term;
		weight_t     bound;
		bool         eq;
	}          active_;
};
//! Parser for SAT or PB problems.
class SatParser : public ProgramParser {
public:
	explicit SatParser(SatBuilder& prg);
	explicit SatParser(PBBuilder& prg);
	~SatParser();
protected:
	virtual StrategyType* doAccept(std::istream& str, const ParserOptions& o);
private:
	SatReader* reader_;
};
//@}

}
#endif
