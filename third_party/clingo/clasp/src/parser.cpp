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
#include <clasp/parser.h>
#include <clasp/program_builder.h>
#include <clasp/logic_program.h>
#include <clasp/dependency_graph.h>
#include <clasp/minimize_constraint.h>
#include <clasp/shared_context.h>
#include <clasp/solver.h>
#include <clasp/clause.h>
#include <clasp/util/hash.h>
#include <potassco/theory_data.h>
#include <potassco/aspif.h>
#include <potassco/smodels.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string>
#ifdef _MSC_VER
#pragma warning (disable : 4996)
#endif
namespace Clasp {
ProblemType detectProblemType(std::istream& in) {
	for (std::istream::int_type x, line = 1, pos = 1; (x = in.peek()) != std::char_traits<char>::eof();) {
		char c = static_cast<char>(x);
		if (c == ' ' || c == '\t')  { in.get(); ++pos; continue; }
		if (AspParser::accept(c))   { return Problem_t::Asp; }
		if (DimacsReader::accept(c)){ return Problem_t::Sat; }
		if (OpbReader::accept(c))   { return Problem_t::Pb;  }
		POTASSCO_REQUIRE(c == '\n', "parse error in line %d:%d: '%c': unrecognized input format", (int)line,(int)pos, c);
		in.get();
		++line;
	}
	throw std::logic_error("bad input stream");
}
/////////////////////////////////////////////////////////////////////////////////////////
// ProgramParser
/////////////////////////////////////////////////////////////////////////////////////////
ProgramParser::ProgramParser() : strat_(0) {}
ProgramParser::~ProgramParser() {}
bool ProgramParser::accept(std::istream& str, const ParserOptions& o) {
	if ((strat_ = doAccept(str, o)) != 0) {
		strat_->setMaxVar(VAR_MAX);
		return true;
	}
	return false;
}
bool ProgramParser::isOpen() const {
	return strat_ != 0;
}
bool ProgramParser::incremental() const {
	return strat_ && strat_->incremental();
}
bool ProgramParser::parse() {
	return strat_ && strat_->parse();
}
bool ProgramParser::more() {
	return strat_ && strat_->more();
}
void ProgramParser::reset() {
	if (strat_) { strat_->reset(); }
	strat_ = 0;
}
/////////////////////////////////////////////////////////////////////////////////////////
// AspParser::SmAdapter
//
// Callback interface for smodels parser
/////////////////////////////////////////////////////////////////////////////////////////
struct AspParser::SmAdapter : public Asp::LogicProgramAdapter, public Potassco::AtomTable {
	typedef POTASSCO_EXT_NS::unordered_map<ConstString, Var, StrHash, StrEq> StrMap;
	typedef SingleOwnerPtr<StrMap> StrMapPtr;
	SmAdapter(Asp::LogicProgram& prg) : Asp::LogicProgramAdapter(prg) {}
	void endStep() {
		Asp::LogicProgramAdapter::endStep();
		if (inc_ && lp_->ctx()->hasMinimize()) {
			lp_->ctx()->removeMinimize();
		}
		if (!inc_) { atoms_ = 0; }
	}
	void add(Potassco::Atom_t id, const Potassco::StringSpan& name, bool output) {
		ConstString n(name);
		if (atoms_.get()) { atoms_->insert(StrMap::value_type(n, id)); }
		if (output) { lp_->addOutput(n, id); }
	}
	Potassco::Atom_t find(const Potassco::StringSpan& name) {
		if (!atoms_.get()) { return 0; }
		ConstString n(name);
		StrMap::iterator it = atoms_->find(n);
		return it != atoms_->end() ? it->second : 0;
	}
	StrMapPtr atoms_;
};
/////////////////////////////////////////////////////////////////////////////////////////
// AspParser
/////////////////////////////////////////////////////////////////////////////////////////
AspParser::AspParser(Asp::LogicProgram& prg)
	: lp_(&prg)
	, in_(0)
	, out_(0) {}
AspParser::~AspParser() {
	delete in_;
	delete out_;
}
bool AspParser::accept(char c) { return Potassco::BufferedStream::isDigit(c) || c == 'a'; }

AspParser::StrategyType* AspParser::doAccept(std::istream& str, const ParserOptions& o) {
	delete in_;
	delete out_;
	if (Potassco::BufferedStream::isDigit((char)str.peek())) {
		out_ = new SmAdapter(*lp_);
		Potassco::SmodelsInput::Options so;
		so.enableClaspExt();
		if (o.isEnabled(ParserOptions::parse_heuristic)) {
			so.convertHeuristic();
			static_cast<SmAdapter*>(out_)->atoms_ = new AspParser::SmAdapter::StrMap();
		}
		if (o.isEnabled(ParserOptions::parse_acyc_edge)) {
			so.convertEdges();
		}
		in_ = new Potassco::SmodelsInput(*out_, so, static_cast<SmAdapter*>(out_));
	}
	else {
		out_ = new Asp::LogicProgramAdapter(*lp_);
		in_ = new Potassco::AspifInput(*out_);
	}
	return in_->accept(str) ? in_ : 0;
}

void AspParser::write(Asp::LogicProgram& prg, std::ostream& os) {
	write(prg, os, prg.supportsSmodels() ? format_smodels : format_aspif);
}
void AspParser::write(Asp::LogicProgram& prg, std::ostream& os, Format f) {
	using namespace Potassco;
	SingleOwnerPtr<AbstractProgram> out;
	if (f == format_aspif) {
		out.reset(new Potassco::AspifOutput(os));
	}
	else {
		out.reset(new Potassco::SmodelsOutput(os, true, prg.falseAtom()));
	}
	if (prg.startAtom() == 1) { out->initProgram(prg.isIncremental()); }
	out->beginStep();
	prg.accept(*out);
	out->endStep();
}
/////////////////////////////////////////////////////////////////////////////////////////
// clasp specific extensions for Dimacs/OPB
/////////////////////////////////////////////////////////////////////////////////////////
SatReader::SatReader() {}
bool SatReader::skipLines(char c) {
	while (peek(true) == c) { skipLine(); }
	return true;
}
Literal SatReader::matchLit(Var max) {
	for (char c; (c = stream()->peek()) == ' ' || c == '\t';) { stream()->get(); }
	bool sign = stream()->peek() == '-';
	if (sign) { stream()->get(); }
	if (stream()->peek() == 'x') { stream()->get(); }
	int64 id;
	require(stream()->match(id) && id >= 0 && id <= (int64)max, "identifier expected");
	return Literal(static_cast<uint32>(id), sign);
}
void SatReader::parseGraph(uint32 maxVar, const char* pre, ExtDepGraph& graph) {
	int maxNode = matchPos("graph: positive number of nodes expected");
	while (match(pre)) {
		if      (match("node ")) { skipLine(); }
		else if (match("arc "))  {
			Literal lit = matchLit(maxVar);
			Var beg = matchPos(maxNode, "graph: invalid start node");
			Var end = matchPos(maxNode, "graph: invalid end node");
			graph.addEdge(lit, beg, end);
		}
		else if (match("endgraph")) { return; }
		else { break; }
	}
	require(false, "graph: endgraph expected");
}
void SatReader::parseProject(uint32 maxVar, SharedContext& ctx) {
	for (unsigned n = this->line(); (stream()->skipWs(), this->line() == n);) {
		Literal x = matchLit(maxVar);
		if (x == lit_true()) break;
		require(!x.sign(), "project: positive literal expected");
		ctx.output.addProject(x);
	}
}
void SatReader::parseAssume(uint32 maxVar) {
	for (unsigned n = this->line(); (stream()->skipWs(), this->line() == n);) {
		Literal x = matchLit(maxVar);
		if (x == lit_true()) { break; }
		addAssumption(x);
	}
}
void SatReader::parseHeuristic(uint32 maxVar, SharedContext& ctx) {
	using Potassco::Heuristic_t;
	Heuristic_t type = static_cast<Heuristic_t>(matchPos(Heuristic_t::eMax, "heuristic: modifier expected"));
	Literal atom = matchLit(maxVar);
	require(!atom.sign(), "heuristic: positive literal expected");
	int16    bias = (int16)matchInt(INT16_MIN, INT16_MAX, "heuristic: bias expected");
	uint16   prio = (uint16)matchPos(UINT16_MAX, "heuristic: priority expected");
	ctx.heuristic.add(atom.var(), type, bias, prio, matchLit(maxVar));
}
void SatReader::parseOutput(uint32 maxVar, SharedContext& ctx) {
	if (match("range ")) {
		Literal lo = matchLit(maxVar);
		Literal hi = matchLit(maxVar);
		require(lo.var() <= hi.var(), "output: invalid range");
		ctx.output.setVarRange(Range32(lo.var(), hi.var() + 1));
	}
	else {
		Literal cond = matchLit(maxVar);
		while (peek(false) == ' ') { stream()->get(); }
		std::string name;
		for (char c; (c = stream()->get()) != '\n' && c;) { name += c; }
		name.erase(name.find_last_not_of(" \t")+1);
		ctx.output.add(ConstString(Potassco::toSpan(name)), cond);
	}
}
void SatReader::parseExt(const char* pre, uint32 maxVar, SharedContext& ctx) {
	const bool acyc = options.isEnabled(ParserOptions::parse_acyc_edge);
	const bool minw = options.isEnabled(ParserOptions::parse_minimize);
	const bool proj = options.isEnabled(ParserOptions::parse_project);
	const bool heur = options.isEnabled(ParserOptions::parse_heuristic);
	const bool assu = options.isEnabled(ParserOptions::parse_assume);
	uint32     outp = options.isEnabled(ParserOptions::parse_output);
	for (ExtDepGraph* g = 0; match(pre);) {
		if (acyc && match("graph ")) {
			require(g == 0, "graph: only one graph supported");
			g = ctx.extGraph.get();
			if (!g) { ctx.extGraph = (g = new ExtDepGraph()); }
			else { g->update(); }
			parseGraph(maxVar, pre, *g);
			g->finalize(ctx);
		}
		else if (minw && match("minweight ")) {
			WeightLitVec min;
			for (unsigned n = this->line(); (stream()->skipWs(), this->line() == n);) {
				Literal lit = matchLit(maxVar);
				if (lit == lit_true()) {
					skipLine();
					break;
				}
				min.push_back(WeightLiteral(lit, matchInt(CLASP_WEIGHT_T_MIN, CLASP_WEIGHT_T_MAX, "minweight: weight expected")));
			}
			addObjective(min);
		}
		else if (proj && match("project "))   { parseProject(maxVar, ctx); }
		else if (heur && match("heuristic ")) { parseHeuristic(maxVar, ctx); }
		else if (assu && match("assume "))    { parseAssume(maxVar); }
		else if (outp && match("output "))    {
			if (outp++ == 1) { ctx.output.setVarRange(Range32(0, 0)); }
			parseOutput(maxVar, ctx);
		}
		else { skipLine(); }
	}
}

SatParser::SatParser(SatBuilder& prg) : reader_(new DimacsReader(prg)) {}
SatParser::SatParser(PBBuilder& prg)  : reader_(new OpbReader(prg)) {}
SatParser::~SatParser() { delete reader_; }
ProgramParser::StrategyType* SatParser::doAccept(std::istream& str, const ParserOptions& o) {
	reader_->options = o;
	return reader_->accept(str) ? reader_ : 0;
}
/////////////////////////////////////////////////////////////////////////////////////////
// DimacsReader
/////////////////////////////////////////////////////////////////////////////////////////
DimacsReader::DimacsReader(SatBuilder& prg) : program_(&prg) {}

// Parses the p line: p [w]cnf[+] #vars #clauses [max clause weight]
bool DimacsReader::doAttach(bool& inc) {
	inc = false;
	if (!accept(peek(false))) { return false; }
	skipLines('c');
	require(match("p "), "missing problem line");
	wcnf_ = match("w");
	require(match("cnf", false), "unrecognized format, [w]cnf expected");
	if (stream()->peek() == '+') { stream()->get(); }
	require(stream()->get() == ' ', "invalid problem line: expected ' ' after format");
	numVar_     = matchPos(ProgramParser::VAR_MAX, "#vars expected");
	uint32 numC = matchPos("#clauses expected");
	wsum_t cw   = 0;
	while (stream()->peek() == ' ')  { stream()->get(); };
	if (wcnf_ && peek(false) != '\n'){ stream()->match(cw); }
	while (stream()->peek() == ' ')  { stream()->get(); };
	require(stream()->get() == '\n', "invalid extra characters in problem line");
	program_->prepareProblem(numVar_, cw, numC);
	if (options.anyOf(ParserOptions::parse_full)) {
		parseExt("c ", numVar_, *program_->ctx());
	}
	return true;
}
bool DimacsReader::doParse() {
	LitVec cc; WeightLitVec wlc;
	const bool  wcnf = wcnf_;
	const int64 maxV = static_cast<int64>(numVar_);
	for (int64 cw = (int64)options.isEnabled(ParserOptions::parse_maxsat), lit = 0; skipLines('c') && peek(true); lit = 0, cc.clear()) {
		if (wcnf) { require(stream()->match(cw) && cw > 0, "wcnf: positive clause weight expected"); }
		while (stream()->match(lit) && lit != 0) {
			require(lit >= -maxV && lit <= maxV, "invalid variable in clause");
			cc.push_back(toLit(static_cast<int32>(lit)));
		}
		if (lit == 0) { program_->addClause(cc, cw); }
		else {
			require(!wcnf, "invalid character in clause");
			const int  sign = match("<= ") ? -1 : int(require(match(">= "), "invalid constraint operator"));
			const int bound = matchInt(CLASP_WEIGHT_T_MIN, CLASP_WEIGHT_T_MAX, "invalid constraint bound");
			wlc.clear();
			for (LitVec::const_iterator it = cc.begin(), end = cc.end(); it != end; ++it) {
				wlc.push_back(WeightLiteral(*it, sign));
			}
			program_->addConstraint(wlc, bound * sign);
		}
	}
	return require(!more(), "unrecognized format");
}
void DimacsReader::addObjective(const WeightLitVec& vec) {
	program_->addObjective(vec);
}
void DimacsReader::addAssumption(Literal x) {
	program_->addAssumption(x);
}
/////////////////////////////////////////////////////////////////////////////////////////
// OpbReader
/////////////////////////////////////////////////////////////////////////////////////////
OpbReader::OpbReader(PBBuilder& prg) : program_(&prg) {}

void OpbReader::addObjective(const WeightLitVec& vec) {
	program_->addObjective(vec);
}
void OpbReader::addAssumption(Literal x) {
	program_->addAssumption(x);
}

// * #variable= int #constraint= int [#product= int sizeproduct= int] [#soft= int mincost= int maxcost= int sumcost= int]
// where [] indicate optional parts, i.e.
//  LIN-PBO: * #variable= int #constraint= int
//  NLC-PBO: * #variable= int #constraint= int #product= int sizeproduct= int
//  LIN-WBO: * #variable= int #constraint= int #soft= int mincost= int maxcost= int sumcost= int
//  NLC-WBO: * #variable= int #constraint= int #product= int sizeproduct= int #soft= int mincost= int maxcost= int sumcost= int
bool OpbReader::doAttach(bool& inc) {
	inc = false;
	if (!accept(peek(false))) { return false; }
	require(match("* #variable="), "missing problem line '* #variable='");
	unsigned numV = matchPos(ProgramParser::VAR_MAX, "number of vars expected");
	require(match("#constraint="), "bad problem line: missing '#constraint='");
	unsigned numC = matchPos("number of constraints expected");
	unsigned numProd = 0, sizeProd = 0, numSoft = 0;
	minCost_ = 0, maxCost_ = 0;
	if (match("#product=")) { // NLC instance
		numProd = matchPos();
		require(match("sizeproduct="), "'sizeproduct=' expected");
		sizeProd= matchPos();
		(void)sizeProd;
	}
	if (match("#soft=")) { // WBO instance
		numSoft = matchPos();
		require(match("mincost="), "'mincost=' expected");
		minCost_= (weight_t)matchPos(CLASP_WEIGHT_T_MAX, "invalid min costs");
		require(match("maxcost="), "'maxcost=' expected");
		maxCost_= (weight_t)matchPos(CLASP_WEIGHT_T_MAX, "invalid max costs");
		require(match("sumcost="), "'sumcost=' expected");
		wsum_t sum;
		require(stream()->match(sum) && sum > 0, "positive integer expected");
	}
	program_->prepareProblem(numV, numProd, numSoft, numC);
	return true;
}
bool OpbReader::doParse() {
	if (options.anyOf(ParserOptions::parse_full - ParserOptions::parse_minimize)) {
		options.assign(ParserOptions::parse_minimize, false);
		parseExt("* ", program_->numVars(), *program_->ctx());
	}
	skipLines('*');
	parseOptObjective();
	for (;;) {
		skipLines('*');
		if (!more()) { return true; }
		parseConstraint();
	}
}
// <objective>::= "min:" <zeroOrMoreSpace> <sum>  ";"
// OR
// <softobj>  ::= "soft:" [<unsigned_integer>] ";"
void OpbReader::parseOptObjective() {
	if (match("min:")) {
		parseSum();
		program_->addObjective(active_.lits);
	}
	else if (match("soft:")) {
		wsum_t softCost;
		require(stream()->match(softCost) && softCost > 0, "positive integer expected");
		require(match(";"), "semicolon missing after constraint");
		program_->setSoftBound(softCost);
	}
}

// <constraint>::= <sum> <relational_operator> <zeroOrMoreSpace> <integer> <zeroOrMoreSpace> ";"
// OR
// <softconstr>::= "[" <zeroOrMoreSpace> <unsigned_integer> <zeroOrMoreSpace> "]" <constraint>
void OpbReader::parseConstraint() {
	weight_t cost = 0;
	if (match("[")) {
		cost = matchInt(minCost_, maxCost_, "invalid soft constraint cost");
		require(match("]"), "invalid soft constraint");
	}
	parseSum();
	active_.eq = match("=");
	require(active_.eq || match(">=", false), "relational operator expected");
	active_.bound = matchInt(CLASP_WEIGHT_T_MIN, CLASP_WEIGHT_T_MAX, "invalid coefficient on rhs of constraint");
	require(match(";"), "semicolon missing after constraint");
	program_->addConstraint(active_.lits, active_.bound, active_.eq, cost);
}

// <sum>::= <weightedterm> | <weightedterm> <sum>
// <weightedterm>::= <integer> <oneOrMoreSpace> <term> <oneOrMoreSpace>
void OpbReader::parseSum() {
	active_.lits.clear();
	while (!match(";")) {
		int coeff = matchInt(INT_MIN+1, INT_MAX, "coefficient expected");
		parseTerm();
		Literal x = active_.term.size() == 1 ? active_.term[0] : program_->addProduct(active_.term);
		active_.lits.push_back(WeightLiteral(x, coeff));
		char p = peek(true);
		if (p == '>' || p == '=') break;
	}
}
// <term>::=<variablename>
// OR
// <term>::= <literal> | <literal> <space>+ <term>
void OpbReader::parseTerm() {
	active_.term.clear();
	char peek;
	do  {
		match("*");             // optionally
		bool sign = match("~"); // optionally
		require(match("x"), "identifier expected");
		Var var   = matchAtom();
		require(var <= program_->numVars(), "identifier out of range");
		active_.term.push_back(Literal(var, sign));
		peek = this->peek(true);
	} while (peek == '*' || peek == '~' || peek == 'x');
}

}
