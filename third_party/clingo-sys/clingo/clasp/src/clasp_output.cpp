//
// Copyright (c) 2009-present Benjamin Kaufmann
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
#include <clasp/cli/clasp_output.h>
#include <clasp/solver.h>
#include <clasp/satelite.h>
#include <clasp/util/timer.h>
#include <potassco/string_convert.h>
#include <stdio.h>
#if !defined(_WIN32)
#include <signal.h>
#elif !defined(SIGALRM)
#define SIGALRM 14
#endif
#ifdef _MSC_VER
#include <float.h>
#define CLASP_ISNAN(x) (_isnan(x) != 0)
#pragma warning (disable : 4996)
#elif defined(__cplusplus) && __cplusplus >= 201103L
#include <cmath>
#define CLASP_ISNAN(x) std::isnan(x)
#else
#include <math.h>
#define CLASP_ISNAN(x) isnan(x)
#endif
inline bool isNan(double d) { return CLASP_ISNAN(d); }
#undef CLASP_ISNAN
#if defined(_MSC_VER) || defined(__MINGW32__)
static inline void flockfile(FILE* file) {
	_lock_file(file);
}

static inline void funlockfile(FILE* file) {
	_unlock_file(file);
}
#endif
struct FileLock {
	FileLock(FILE* f) {
		flockfile(file = f);
	}
	~FileLock() {
		fflush(file);
		funlockfile(file);
	}
	FILE* file;
};
namespace Clasp { namespace Cli {
/////////////////////////////////////////////////////////////////////////////////////////
// Event formatting
/////////////////////////////////////////////////////////////////////////////////////////
template <>
void formatEvent(const Clasp::BasicSolveEvent& ev, Potassco::StringBuilder& str) {
	const Solver& s = *ev.solver;
	str.appendFormat("%2u:%c|%7u/%-7u|%8u/%-8u|%10" PRIu64"/%-6.3f|%8" PRId64"/%-10" PRId64"|"
		, s.id()
		, static_cast<char>(ev.op)
		, s.numFreeVars()
		, (s.decisionLevel() > 0 ? s.levelStart(1) : s.numAssignedVars())
		, s.numConstraints()
		, s.numLearntConstraints()
		, s.stats.conflicts
		, ratio(s.stats.conflicts, s.stats.choices)
		, ev.cLimit <= (UINT32_MAX) ? (int64)ev.cLimit:-1
		, ev.lLimit != (UINT32_MAX) ? (int64)ev.lLimit:-1
	);
}
template <>
void formatEvent(const Clasp::SolveTestEvent& ev, Potassco::StringBuilder& str) {
	str.appendFormat("%2u:%c| %c HCC: %-6u |%8u/%-8u|%10" PRIu64"/%-6.3f| Time: %10.3fs |", ev.solver->id()
		, "FP"[ev.partial]
		, "?NY"[Range<int>(-1, 1).clamp(ev.result) + 1]
		, ev.hcc
		, ev.solver->numConstraints()
		, ev.solver->numLearntConstraints()
		, ev.conflicts()
		, ratio(ev.conflicts(), ev.choices())
		, ev.time
	);
}
#if CLASP_HAS_THREADS
template <>
void formatEvent(const Clasp::mt::MessageEvent& ev, Potassco::StringBuilder& str) {
	typedef Clasp::mt::MessageEvent EV;
	if (ev.op != EV::completed) { str.appendFormat("%2u:X| %-15s %-53s |", ev.solver->id(), ev.msg, ev.op == EV::sent ? "sent" : "received"); }
	else                        { str.appendFormat("%2u:X| %-15s %-35s in %13.3fs |", ev.solver->id(), ev.msg, "completed", ev.time); }
}
#endif
/////////////////////////////////////////////////////////////////////////////////////////
// Output
/////////////////////////////////////////////////////////////////////////////////////////
Output::Output(uint32 verb) : time_(-1.0), model_(-1.0), summary_(0), verbose_(0), last_(false) {
	std::memset(quiet_, 0, sizeof(quiet_));
	setCallQuiet(print_no);
	setVerbosity(verb);
}
Output::~Output() {}
void Output::setVerbosity(uint32 verb) {
	Event::Verbosity x = (Event::Verbosity)std::min(verbose_ = verb, (uint32)Event::verbosity_max);
	EventHandler::setVerbosity(Event::subsystem_facade , x);
	EventHandler::setVerbosity(Event::subsystem_load   , x);
	EventHandler::setVerbosity(Event::subsystem_prepare, x);
	EventHandler::setVerbosity(Event::subsystem_solve  , x);
}
void Output::setModelQuiet(PrintLevel model){ quiet_[0] = static_cast<uint8>(model); }
void Output::setOptQuiet(PrintLevel opt)    { quiet_[1] = static_cast<uint8>(opt);   }
void Output::setCallQuiet(PrintLevel call)  { quiet_[2] = static_cast<uint8>(call);  }

void Output::onEvent(const Event& ev) {
	typedef ClaspFacade::StepStart StepStart;
	typedef ClaspFacade::StepReady StepReady;
	if (const StepStart* start = event_cast<StepStart>(ev)) {
		if (time_ == -1.0) { time_ = RealTime::getTime(); }
		startStep(*start->facade);
	}
	else if (const StepReady* ready = event_cast<StepReady>(ev)) {
		stopStep(*ready->summary);
	}
}
bool Output::onModel(const Solver& s, const Model& m) {
	PrintLevel type = (m.opt == 1 && !m.consequences()) || m.def ? print_best : print_all;
	bool hasMeta = m.consequences() || m.costs;
	model_ = elapsedTime();
	if (modelQ() <= type || (hasMeta && optQ() <= type)) {
		printModel(s.outputTable(), m, type);
	}
	last_ = type != print_best && (modelQ() == print_best || (optQ() == print_best && hasMeta));
	return true;
}
bool Output::onUnsat(const Solver& s, const Model& m) {
	if (m.ctx) {
		const LowerBound* lower = m.ctx->optimize() && s.lower.active() ? &s.lower : 0;
		const Model*  prevModel = m.num ? &m : 0;
		if (modelQ() == print_all || optQ() == print_all) {
			printUnsat(s.outputTable(), lower, prevModel);
		}
	}
	return true;
}
void Output::startStep(const ClaspFacade&) { summary_ = 0; last_ = false; }
void Output::stopStep(const ClaspFacade::Summary& s){
	if (s.model() && last_) {
		Model m = *s.model();
		m.up = 0; // ignore update state and always print as model
		printModel(s.ctx().output, m, print_best);
	}
	else if (modelQ() == print_all && s.model() && s.model()->up && !s.model()->def) {
		printModel(s.ctx().output, *s.model(), print_all);
	}
	if (callQ() == print_all) {
		printSummary(s, false);
		if (stats(s)) { printStatistics(s, false); }
	}
	else if (callQ() == print_best) {
		summary_ = &s;
	}
}
void Output::shutdown(const ClaspFacade::Summary& summary) {
	if (summary_) {
		printSummary(*summary_, false);
		if (stats(summary)) { printStatistics(*summary_, false); }
	}
	printSummary(summary, true);
	if (stats(summary)) { printStatistics(summary, true); }
	shutdown();
	time_ = -1.0;
}
// Returns the number of consequences and estimates in m.
// For a model m with m.consequences and a return value ret:
//   - ret.first  is the number of definite consequences
//   - ret.second is the number of remaining possible consequences
Output::UPair Output::numCons(const OutputTable& out, const Model& m) const {
	uint32 low = 0, up = 0;
	if (out.projectMode() == ProjectMode_t::Output) {
		low += out.numFacts();
		for (OutputTable::pred_iterator it = out.pred_begin(); it != out.pred_end(); ++it) {
			low += m.isDef(it->cond);
			up  += m.isEst(it->cond);
		}
		for (OutputTable::range_iterator it = out.vars_begin(), end = out.vars_end(); it != end; ++it) {
			Literal p = posLit(*it);
			low += m.isDef(p);
			up  += m.isEst(p);
		}
	}
	else {
		for (OutputTable::lit_iterator it = out.proj_begin(), end = out.proj_end(); it != end; ++it) {
			low += m.isDef(*it);
			up  += m.isEst(*it);
		}
	}
	return UPair(low, m.def ? 0 : up);
}

// Prints shown symbols in model.
// The functions prints
// - true literals in definite answer, followed by
// - true literals in current estimate if m.consequences()
void Output::printWitness(const OutputTable& out, const Model& m, uintp data) {
	for (OutputTable::fact_iterator it = out.fact_begin(); it != out.fact_end(); ++it) {
		data = doPrint(OutPair(*it, lit_true()), data);
	}
	for (const char* x = out.theory ? out.theory->first(m) : 0; x; x = out.theory->next()) {
		data = doPrint(OutPair(x, lit_true()), data);
	}
	const bool onlyD = m.type != Model::Cautious || m.def;
	for (bool D = true;; D = !D) {
		for (OutputTable::pred_iterator it = out.pred_begin(); it != out.pred_end(); ++it) {
			if (m.isTrue(it->cond) && (onlyD || m.isDef(it->cond) == D)) {
				data = doPrint(OutPair(it->name, lit_true()), data);
			}
		}
		if (out.vars_begin() != out.vars_end()) {
			const bool showNeg = !m.consequences();
			if (out.projectMode() == ProjectMode_t::Output || !out.filter("_")) {
				for (OutputTable::range_iterator it = out.vars_begin(), end = out.vars_end(); it != end; ++it) {
					Literal p = posLit(*it);
					if ((showNeg || m.isTrue(p)) && (onlyD || m.isDef(p) == D)) {
						data = doPrint(OutPair(static_cast<const char*>(0), m.isTrue(p) ? p : ~p), data);
					}
				}
			}
			else {
				for (OutputTable::lit_iterator it = out.proj_begin(), end = out.proj_end(); it != end; ++it) {
					if ((showNeg || m.isTrue(*it)) && (onlyD || m.isDef(*it) == D)) {
						data = doPrint(OutPair(static_cast<const char*>(0), m.isTrue(*it) ? *it : ~*it), data);
					}
				}
			}
		}
		if (D == onlyD) { return; }
	}
}
void Output::printUnsat(const OutputTable&, const LowerBound*, const Model*) {}
uintp Output::doPrint(const OutPair&, UPtr x) { return x; }
bool Output::stats(const ClaspFacade::Summary& summary) const {
	return summary.facade->config()->context().stats != 0;
}
double Output::elapsedTime() const { return time_ != -1.0 ? RealTime::getTime() - time_ : -1.0; }
double Output::modelTime() const { return model_; }
/////////////////////////////////////////////////////////////////////////////////////////
// JsonOutput
/////////////////////////////////////////////////////////////////////////////////////////
JsonOutput::JsonOutput(uint32 v) : Output(std::min(v, uint32(1))), open_("") {
	objStack_.reserve(10);
}
JsonOutput::~JsonOutput() { JsonOutput::shutdown(); }
void JsonOutput::run(const char* solver, const char* version, const std::string* iBeg, const std::string* iEnd) {
	if (indent() == 0) {
		open_ = "";
		pushObject();
	}
	printKeyValue("Solver", std::string(solver).append(" version ").append(version).c_str());
	pushObject("Input", type_array, true);
	for (const char* sep = ""; iBeg != iEnd; ++iBeg, sep = ",") {
		printString(iBeg->c_str(), sep);
	}
	popObject();
	pushObject("Call", type_array);
}
void JsonOutput::shutdown() {
	if (!objStack_.empty()) {
		popUntil(0u);
		printf("\n");
		fflush(stdout);
	}
}
void JsonOutput::startStep(const ClaspFacade& f) {
	Output::startStep(f);
	popUntil(2u);
	pushObject(0, type_object);
	printTime("Start", elapsedTime());
	fflush(stdout);
}
void JsonOutput::stopStep(const ClaspFacade::Summary& s) {
	assert(!objStack_.empty());
	Output::stopStep(s);
	popUntil(3u);
	printTime("Stop", elapsedTime());
	if (callQ() != print_best) { // keep call object open for last summary
		popObject();
	}
	fflush(stdout);
}

bool JsonOutput::visitThreads(Operation op) {
	if      (op == Enter) { pushObject("Thread", type_array); }
	else if (op == Leave) { popObject(); }
	return true;
}
bool JsonOutput::visitTester(Operation op) {
	if      (op == Enter) { pushObject("Tester"); }
	else if (op == Leave) { popObject(); }
	return true;
}
bool JsonOutput::visitHccs(Operation op) {
	if      (op == Enter) { pushObject("HCC", type_array); }
	else if (op == Leave) { popObject(); }
	return true;
}
void JsonOutput::visitThread(uint32, const SolverStats& stats) {
	pushObject(0, type_object);
	JsonOutput::visitSolverStats(stats);
	popObject();
}
void JsonOutput::visitHcc(uint32, const ProblemStats& p, const SolverStats& s) {
	pushObject(0, type_object);
	JsonOutput::visitProblemStats(p);
	JsonOutput::visitSolverStats(s);
	popObject();
}

void JsonOutput::visitSolverStats(const SolverStats& stats) {
	printCoreStats(stats);
	if (stats.extra) {
		printExtStats(*stats.extra, objStack_.size() == 2);
		printJumpStats(stats.extra->jumps);
	}
}

void JsonOutput::printChildren(const StatisticObject& s) {
	for (uint32 i = 0; i != s.size(); ++i) {
		const char* key = s.type() == Potassco::Statistics_t::Map ? s.key(i) : 0;
		StatisticObject child = key ? s.at(key) : s[i];
		if (child.type() == Potassco::Statistics_t::Value) {
			printKeyValue(key, child);
		}
		else if (child.size()) {
			pushObject(key, child.type() == Potassco::Statistics_t::Map ? type_object : type_array);
			printChildren(child);
			popObject();
		}
	}
}

void JsonOutput::visitExternalStats(const StatisticObject& stats) {
	POTASSCO_ASSERT(stats.type() == Potassco::Statistics_t::Map, "Non map statistic!");
	printChildren(stats);
}

void JsonOutput::printCoreStats(const CoreStats& st) {
	pushObject("Core");
	printKeyValue("Choices"    , st.choices);
	printKeyValue("Conflicts"  , st.conflicts);
	printKeyValue("Backtracks" , st.backtracks());
	printKeyValue("Backjumps"  , st.backjumps());
	printKeyValue("Restarts"   , st.restarts);
	printKeyValue("RestartAvg" , st.avgRestart());
	printKeyValue("RestartLast", st.lastRestart);
	popObject(); // Core
}

void JsonOutput::printExtStats(const ExtendedStats& stx, bool generator) {
	pushObject("More");
	printKeyValue("CPU", stx.cpuTime);
	printKeyValue("Models", stx.models);
	if (stx.domChoices) {
		printKeyValue("DomChoices", stx.domChoices);
	}
	if (stx.hccTests) {
		pushObject("StabTests");
		printKeyValue("Sum", stx.hccTests);
		printKeyValue("Full", stx.hccTests - stx.hccPartial);
		printKeyValue("Partial", stx.hccPartial);
		popObject();
	}
	if (stx.models) {
		printKeyValue("AvgModel", stx.avgModel());
	}
	printKeyValue("Splits", stx.splits);
	printKeyValue("Problems", stx.gps);
	printKeyValue("AvgGPLength", stx.avgGp());
	pushObject("Lemma");
	printKeyValue("Sum", stx.lemmas());
	printKeyValue("Deleted" , stx.deleted);
	pushObject("Type", type_array);
	const char* names[] = {"Short", "Conflict", "Loop", "Other"};
	for (int i = Constraint_t::Static; i <= Constraint_t::Other; ++i) {
		pushObject();
		printKeyValue("Type", names[i]);
		if (i == 0) {
			printKeyValue("Sum", stx.binary+stx.ternary);
			printKeyValue("Ratio", percent(stx.binary+stx.ternary, stx.lemmas()));
			printKeyValue("Binary", stx.binary);
			printKeyValue("Ternary", stx.ternary);
		}
		else {
			printKeyValue("Sum", stx.lemmas(ConstraintType(i)));
			printKeyValue("AvgLen", stx.avgLen(ConstraintType(i)));
		}
		popObject();
	}
	popObject();
	popObject(); // Lemma
	if (stx.distributed || stx.integrated) {
		pushObject("Distribution");
		printKeyValue("Distributed", stx.distributed);
		printKeyValue("Ratio", stx.distRatio());
		printKeyValue("AvgLbd", stx.avgDistLbd());
		popObject();
		pushObject("Integration");
		printKeyValue("Integrated", stx.integrated);
		printKeyValue("Units", stx.intImps);
		printKeyValue("AvgJump", stx.avgIntJump());
		if (generator) { printKeyValue("Ratio", stx.intRatio()); }
		popObject();
	}
	popObject(); // More
}
void JsonOutput::printJumpStats(const JumpStats& st) {
	pushObject("Jumps");
	printKeyValue("Sum", st.jumps);
	printKeyValue("Max", st.maxJump);
	printKeyValue("MaxExec", st.maxJumpEx);
	printKeyValue("Avg", st.avgJump());
	printKeyValue("AvgExec", st.avgJumpEx());
	printKeyValue("Levels", st.jumpSum);
	printKeyValue("LevelsExec", st.jumped());
	pushObject("Bounded");
	printKeyValue("Sum", st.bounded);
	printKeyValue("Max", st.maxBound);
	printKeyValue("Avg", st.avgBound());
	printKeyValue("Levels", st.boundSum);
	popObject();
	popObject();
}
void JsonOutput::visitLogicProgramStats(const Asp::LpStats& lp) {
	using namespace Asp;
	pushObject("LP");
	pushObject("Rules");
	printKeyValue("Original", lp.rules[0].sum());
	printKeyValue("Final",    lp.rules[1].sum());
	for (uint32 i = 0; i != RuleStats::numKeys(); ++i) {
		if (i != RuleStats::Normal && lp.rules[0][i]) {
			pushObject(RuleStats::toStr(i));
			printKeyValue("Original", lp.rules[0][i]);
			printKeyValue("Final",    lp.rules[1][i]);
			popObject();
		}
	}
	popObject(); // Rules
	printKeyValue("Atoms", lp.atoms);
	if (lp.auxAtoms) { printKeyValue("AuxAtoms", lp.auxAtoms); }
	if (lp.disjunctions[0]) {
		pushObject("Disjunctions");
		printKeyValue("Original", lp.disjunctions[0]);
		printKeyValue("Final", lp.disjunctions[1]);
		popObject();
	}
	pushObject("Bodies");
	printKeyValue("Original", lp.bodies[0].sum());
	printKeyValue("Final"   , lp.bodies[1].sum());
	for (uint32 i = 1; i != BodyStats::numKeys(); ++i) {
		if (lp.bodies[0][i]) {
			pushObject(BodyStats::toStr(i));
			printKeyValue("Original", lp.bodies[0][i]);
			printKeyValue("Final",    lp.bodies[1][i]);
			popObject();
		}
	}
	popObject();
	if      (lp.sccs == 0)              { printKeyValue("Tight", "yes"); }
	else if (lp.sccs == PrgNode::noScc) { printKeyValue("Tight", "N/A"); }
	else                                {
		printKeyValue("Tight", "no");
		printKeyValue("SCCs", lp.sccs);
		printKeyValue("NonHcfs", lp.nonHcfs);
		printKeyValue("UfsNodes", lp.ufsNodes);
		printKeyValue("NonHcfGammas", lp.gammas);
	}
	pushObject("Equivalences");
	printKeyValue("Sum", lp.eqs());
	printKeyValue("Atom", lp.eqs(Var_t::Atom));
	printKeyValue("Body", lp.eqs(Var_t::Body));
	printKeyValue("Other", lp.eqs(Var_t::Hybrid));
	popObject();
	popObject(); // LP
}
void JsonOutput::visitProblemStats(const ProblemStats& p) {
	pushObject("Problem");
	printKeyValue("Variables", p.vars.num);
	printKeyValue("Eliminated", p.vars.eliminated);
	printKeyValue("Frozen", p.vars.frozen);
	pushObject("Constraints");
	uint32 sum = p.numConstraints();
	printKeyValue("Sum", sum);
	printKeyValue("Binary", p.constraints.binary);
	printKeyValue("Ternary", p.constraints.ternary);
	popObject(); // Constraints
	printKeyValue("AcycEdges", p.acycEdges);
	popObject(); // PS
}

void JsonOutput::printKey(const char* k) {
	if (k) { printf("%s%-*.*s\"%s\": ", open_, indent(), indent(), " ", k); }
	else   { printf("%s%-*.*s", open_, indent(), indent(), " "); }
}

void JsonOutput::printString(const char* v, const char* sep) {
	assert(v);
	const uint32 BUF_SIZE = 1024;
	char buf[BUF_SIZE];
	const char* special = "\b\f\n\r\t\"\\";
	const char* replace = "bfnrt\"\\";
	buf[0] = '"';
	for (uint32 n = 1; (buf[n] = *v) != 0; ++v) {
		if (const char* esc = strchr(special, buf[n])) {
			buf[n]   = '\\';
			buf[++n] = replace[esc - special];
		}
		if (++n > BUF_SIZE - 2) { buf[n] = 0; printf("%s%s", sep, buf); n = 0; sep = ""; }
	}
	printf("%s%s\"", sep, buf);
}

void JsonOutput::printKeyValue(const char* k, const char* v) {
	printf("%s%-*s\"%s\": ", open_, indent(), " ", k);
	printString(v,"");
	open_ = ",\n";
}
void JsonOutput::printKeyValue(const char* k, uint64 v) {
	printf("%s%-*s\"%s\": %" PRIu64, open_, indent(), " ", k, v);
	open_ = ",\n";
}
void JsonOutput::printKeyValue(const char* k, uint32 v) { return printKeyValue(k, uint64(v)); }
void JsonOutput::printKeyValue(const char* k, double v) {
	if (!isNan(v)) { printf("%s%-*s\"%s\": %.3f", open_, indent(), " ", k, v); }
	else           { printf("%s%-*s\"%s\": %s", open_, indent(), " ", k, "null"); }
	open_ = ",\n";
}
void JsonOutput::printKeyValue(const char* k, const StatisticObject& o) {
	double v = o.value();
	printKey(k);
	if (!isNan(v)) { printf("%g", v); }
	else           { printf("%s", "null"); }
	open_ = ",\n";
}

void JsonOutput::pushObject(const char* k, ObjType t, bool startIndent) {
	printKey(k);
	char o = t == type_object ? '{' : '[';
	objStack_ += o;
	printf("%c\n", o);
	open_ = "";
	if (startIndent) { printf("%-*s", indent(), " "); }
}
char JsonOutput::popObject() {
	assert(!objStack_.empty());
	char o = *objStack_.rbegin();
	objStack_.erase(objStack_.size()-1);
	printf("\n%-*.*s%c", indent(), indent(), " ", o == '{' ? '}' : ']');
	open_ = ",\n";
	return o;
}
void JsonOutput::startWitness(double time) {
	if (!hasWitnesses()) {
		pushObject("Witnesses", type_array);
	}
	pushObject();
	printTime("Time", time);
}
void JsonOutput::endWitness() {
	popObject();
	fflush(stdout);
}
void JsonOutput::popUntil(uint32 sz) {
	while (sizeVec(objStack_) > sz) {
		popObject();
	}
}
uintp JsonOutput::doPrint(const OutPair& out, uintp data) {
	const char* sep = reinterpret_cast<const char*>(data);
	if (out.first){ printString(out.first, sep); }
	else          { printf("%s%d", sep, out.second.sign() ? -static_cast<int>(out.second.var()) : static_cast<int>(out.second.var())); }
	return reinterpret_cast<UPtr>(", ");
}
void JsonOutput::printModel(const OutputTable& out, const Model& m, PrintLevel x) {
	bool hasModel = false;
	if (modelQ() <= x) {
		hasModel = (startWitness(modelTime()), true);
		pushObject("Value", type_array, true);
		printWitness(out, m, reinterpret_cast<UPtr>(""));
		popObject();
	}
	if (optQ() <= x && (m.consequences() || m.costs)) {
		if (!hasModel)        { hasModel = (startWitness(modelTime()), true); }
		if (m.consequences()) { printCons(numCons(out, m)); }
		if (m.costs)          { printCosts(*m.costs); }
	}
	if (hasModel) { endWitness(); }
}
void JsonOutput::printUnsat(const OutputTable& out, const LowerBound* lower, const Model* prevModel) {
	if (lower && optQ() == print_all) {
		startWitness(elapsedTime());
		Potassco::Span<wsum_t> first = Potassco::toSpan<wsum_t>();
		if (prevModel && prevModel->costs && prevModel->costs->size() > lower->level) {
			first = Potassco::toSpan(&prevModel->costs->at(0), lower->level);
		}
		printSum("Lower", first, &lower->bound);
		endWitness();
	}
}
void JsonOutput::printSum(const char* name, Potassco::Span<wsum_t> sum, const wsum_t* last) {
	pushObject(name, type_array, true);
	const char* sep = "";
	for (Potassco::Span<wsum_t>::iterator it = Potassco::begin(sum), end = Potassco::end(sum); it != end; ++it) {
		printf("%s%" PRId64, sep, *it);
		sep = ", ";
	}
	if (last) {
		printf("%s%" PRId64, sep, *last);
	}
	popObject();
}
void JsonOutput::printCosts(const SumVec& opt, const char* name) {
	printSum(name, Potassco::toSpan(opt));
}
void JsonOutput::printCons(const UPair& cons) {
	pushObject("Consequences");
	printKeyValue("True", cons.first);
	printKeyValue("Open", cons.second);
	popObject();
}

void JsonOutput::printSummary(const ClaspFacade::Summary& run, bool final) {
	popUntil(final ? 1u : 3u);
	const char* res = "UNKNOWN";
	if      (run.unsat()) { res = "UNSATISFIABLE"; }
	else if (run.sat())   { res = !run.optimum() ? "SATISFIABLE" : "OPTIMUM FOUND"; }
	printKeyValue("Result", res);
	if (verbosity()) {
		if (run.result.interrupted()){ printKeyValue(run.result.signal != SIGALRM ? "INTERRUPTED" : "TIME LIMIT", uint32(1));  }
		pushObject("Models");
		printKeyValue("Number", run.numEnum);
		printKeyValue("More"  , run.complete() ? "no" : "yes");
		if (run.sat()) {
			if (run.consequences()){
				printKeyValue(run.consequences(), run.complete() ? "yes":"unknown");
				printCons(numCons(run.ctx().output, *run.model()));
			}
			if (run.optimize())    {
				printKeyValue("Optimum", run.optimum()?"yes":"unknown");
				printKeyValue("Optimal", run.optimal());
				printCosts(*run.costs());
			}
		}
		popObject();
		if (run.hasLower() && !run.optimum()) {
			pushObject("Bounds");
			printCosts(run.lower(), "Lower");
			printCosts(run.costs() ? *run.costs() : SumVec(), "Upper");
			popObject();
		}
		if (final) { printKeyValue("Calls", run.step + 1); }
		pushObject("Time");
		printKeyValue("Total", run.totalTime);
		printKeyValue("Solve", run.solveTime);
		printKeyValue("Model", run.satTime);
		printKeyValue("Unsat", run.unsatTime);
		printKeyValue("CPU"  , run.cpuTime);
		popObject(); // Time
		if (run.ctx().concurrency() > 1) {
			printKeyValue("Threads", run.ctx().concurrency());
			printKeyValue("Winner",  run.ctx().winner());
		}
	}
}
void JsonOutput::printStatistics(const ClaspFacade::Summary& summary, bool final) {
	popUntil(final ? 1u : 3u);
	pushObject("Stats", type_object);
	summary.accept(*this);
	popObject();
}
void JsonOutput::printTime(const char* name, double t) {
	if (t >= 0.0) { printKeyValue(name, t); }
}
/////////////////////////////////////////////////////////////////////////////////////////
// TextOutput
/////////////////////////////////////////////////////////////////////////////////////////
#define printKeyValue(k, fmt, value) printf("%s%-*s: " fmt, format[cat_comment], width_, (k), (value))
#define printLN(cat, fmt, ...)       printf("%s" fmt "\n", format[cat], __VA_ARGS__)
#define printBR(cat)                 printf("%s\n", format[cat])
#define printKey(k)                  printf("%s%-*s: ", format[cat_comment], width_, (k))
const char* const rowSep   = "------------------------------------------------------------------------------------------|";
const char* const finalSep = "====================================== Accumulation ======================================|";
const char* const satPre   = "Sat-Prepro";

static inline std::string prettify(const std::string& str) {
	if (str.size() < 40) return str;
	std::string t("...");
	t.append(str.end()-38, str.end());
	return t;
}
TextOutput::TextOutput(uint32 verbosity, Format fmt, const char* catAtom, char ifs) : Output(verbosity), stTime_(0.0), state_(0) {
	result[res_unknown]    = "UNKNOWN";
	result[res_sat]        = "SATISFIABLE";
	result[res_unsat]      = "UNSATISFIABLE";
	result[res_opt]        = "OPTIMUM FOUND";
	format[cat_comment]    = "";
	format[cat_value]      = "";
	format[cat_objective]  = "Optimization: ";
	format[cat_result]     = "";
	format[cat_value_term] = "";
	format[cat_atom_name]  = "%s";
	format[cat_atom_var]   = "-%d";
	if (fmt == format_aspcomp) {
		format[cat_comment]   = "% ";
		format[cat_value]     = "ANSWER\n";
		format[cat_objective] = "COST ";
		format[cat_atom_name] = "%s.";
		result[res_sat]       = "";
		result[res_unsat]     = "INCONSISTENT";
		result[res_opt]       = "OPTIMUM";
		setModelQuiet(print_best);
		setOptQuiet(print_best);
	}
	else if (fmt == format_sat09 || fmt == format_pb09) {
		format[cat_comment]   = "c ";
		format[cat_value]     = "v ";
		format[cat_objective] = "o ";
		format[cat_result]    = "s ";
		format[cat_value_term]= "0";
		if (fmt == format_pb09) {
			format[cat_value_term]= "";
			format[cat_atom_var]  = "-x%d";
			setModelQuiet(print_best);
		}
	}
	if (catAtom && *catAtom) {
		char f = 0;
		for (const char* x = catAtom; *x; ++x) {
			POTASSCO_REQUIRE(*x != '\n', "cat_atom: Invalid format string - new line not allowed");
			if (*x == '%')  {
				POTASSCO_REQUIRE(*++x, "cat_atom: Invalid format string - missing format specifier");
				if (*x != '%') {
					POTASSCO_REQUIRE(f == 0, "cat_atom: Invalid format string - too many arguments");
					POTASSCO_REQUIRE(std::strchr("sd0", *x) != 0, "cat_atom: Invalid format string - invalid format specifier");
					f = *x;
				}
			}
		}
		if (f == '0') {
			std::size_t len = std::strlen(catAtom);
			fmt_.reserve((len * 2) + 2);
			fmt_.append(catAtom).append(1, '\0').append(1, '-').append(catAtom);
			std::string::size_type p = fmt_.find("%0") + 1;
			fmt_[p] = 's';
			fmt_[len + 2 + p] = 'd';
			format[cat_atom_name] = fmt_.c_str();
			format[cat_atom_var]  = fmt_.c_str() + len + 1;
		}
		else {
			format[f == 's' ? cat_atom_name : cat_atom_var] = catAtom;
		}
	}
	POTASSCO_REQUIRE(*format[cat_atom_var] == '-' , "cat_atom: Invalid format string - must start with '-'");
	ifs_[0] = ifs;
	ifs_[1] = 0;
	width_  = 13+(int)strlen(format[cat_comment]);
	progress_.clear();
}
TextOutput::~TextOutput() {}

void TextOutput::comment(uint32 v, const char* fmt, ...) const {
	if (verbosity() >= v) {
		printf("%s", format[cat_comment]);
		va_list args;
		va_start(args, fmt);
		vfprintf(stdout, fmt, args);
		va_end(args);
		fflush(stdout);
	}
}

void TextOutput::run(const char* solver, const char* version, const std::string* begInput, const std::string* endInput) {
	if (!version) version = "";
	if (solver) comment(1, "%s version %s\n", solver, version);
	if (begInput != endInput) {
		comment(1, "Reading from %s%s\n", prettify(*begInput).c_str(), (endInput - begInput) > 1 ? " ..." : "");
	}
}
void TextOutput::shutdown() {}
void TextOutput::printSummary(const ClaspFacade::Summary& run, bool final) {
	if (final && callQ() != print_no){
		comment(1, "%s\n", finalSep);
	}
	const char* res = result[res_unknown];
	if      (run.unsat()) { res = result[res_unsat]; }
	else if (run.sat())   { res = !run.optimum() ? result[res_sat] : result[res_opt]; }
	if (*res) { printLN(cat_result, "%s", res); }
	if (verbosity() || stats(run)) {
		printBR(cat_comment);
		if (run.result.interrupted()){ printKeyValue((run.result.signal != SIGALRM ? "INTERRUPTED" : "TIME LIMIT"), "%u\n", uint32(1));  }
		const char* const moreStr = run.complete() ? "" :  "+";
		printKey("Models");
		printf("%" PRIu64 "%s\n", run.numEnum, moreStr);
		if (run.sat()) {
			if (run.consequences()) { printLN(cat_comment, "  %-*s: %s", width_-2, run.consequences(), (run.complete()?"yes":"unknown")); }
			if (run.costs())        { printKeyValue("  Optimum", "%s\n", run.optimum()?"yes":"unknown"); }
			if (run.optimize())     {
				if (run.optimal() > 1){ printKeyValue("  Optimal", "%" PRIu64"\n", run.optimal()); }
				printKey("Optimization");
				printCostsImpl(*run.costs(), ' ');
				printf("\n");
			}
			if (run.consequences()) {
				printKey("Consequences");
				printf("%u%s\n", numCons(run.ctx().output, *run.model()).first, moreStr);
			}
		}
		if (run.hasLower() && !run.optimum()) {
			printKey("Bounds");
			printBounds(run.lower(), run.costs() ? *run.costs() : SumVec());
			printf("\n");
		}
		if (final) { printKeyValue("Calls", "%u\n", run.step + 1); }
		printKey("Time");
		printf("%.3fs (Solving: %.2fs 1st Model: %.2fs Unsat: %.2fs)\n"
			, run.totalTime
			, run.solveTime
			, run.satTime
			, run.unsatTime);
		printKeyValue("CPU Time", "%.3fs\n", run.cpuTime);
		if (run.ctx().concurrency() > 1) {
			printKeyValue("Threads", "%-8u", run.ctx().concurrency());
			printf(" (Winner: %u)\n", run.ctx().winner());
		}
	}
}
void TextOutput::printStatistics(const ClaspFacade::Summary& run, bool) {
	printBR(cat_comment);
	accu_ = true;
	run.accept(*this);
}
void TextOutput::startStep(const ClaspFacade& f) {
	Output::startStep(f);
	setState(Event::subsystem_facade, 0, 0);
	if (callQ() != print_no) {
		comment(1, "%s\n", rowSep);
		comment(2, "%-13s: %d\n", "Call", f.step()+1);
	}
}
void TextOutput::stopStep(const ClaspFacade::Summary& s) {
	setState(Event::subsystem_facade, 0, 0);
	comment(2 - (callQ() != print_no), "%s\n", rowSep);
	Output::stopStep(s);
}
void TextOutput::onEvent(const Event& ev) {
	typedef SatElite::Progress SatPre;
	if (ev.verb <= verbosity() && ev.system != Event::subsystem_facade) {
		if (ev.system == state_) {
			if      (ev.system == Event::subsystem_solve)       { printSolveProgress(ev); }
			else if (const SatPre* sat = event_cast<SatPre>(ev)){
				if      (sat->op != SatElite::Progress::event_algorithm) { comment(2, "%-13s: %c: %8u/%-8u\r", satPre, (char)sat->op, sat->cur, sat->max); }
				else if (sat->cur!= sat->max)                            { setState(ev.system, 2, satPre); }
				else {
					SatPreprocessor* p = sat->self;
					double tEnd = RealTime::getTime();
					comment(2, "%-13s: %.3fs (ClRemoved: %u ClAdded: %u LitsStr: %u)\n", satPre, tEnd - stTime_, p->stats.clRemoved, p->stats.clAdded, p->stats.litsRemoved);
					state_ = Event::subsystem_facade;
				}
			}
		}
		else if (const LogEvent* log = event_cast<LogEvent>(ev)) {
			setState(ev.system, ev.verb, log->msg);
		}
	}
	Output::onEvent(ev);
}
void TextOutput::setState(uint32 state, uint32 verb, const char* m) {
	double ts = RealTime::getTime();
	if (verb <= verbosity()) {
		if (state_ == Event::subsystem_load || state_ == Event::subsystem_prepare) {
			printf("%.3fs\n", ts - stTime_);
		}
		if      (state == Event::subsystem_load)   { comment(2, "%-13s: ", m ? m : "Reading"); }
		else if (state == Event::subsystem_prepare){ comment(2, "%-13s:%s", m ? m : "Preprocessing", m == satPre ? "\r" : " "); }
		else if (state == Event::subsystem_solve)  { comment(1, "Solving...\n"); }
	}
	progress_.clear();
	stTime_ = ts;
	state_  = state;
}
void TextOutput::printSolveProgress(const Event& ev) {
	if (ev.id == SolveTestEvent::id_s  && (verbosity() & 4) == 0) { return; }
	if (ev.id == BasicSolveEvent::id_s && (verbosity() & 1) == 0) { return; }
	char lEnd = '\n';
	char line[128];
	int eventId = static_cast<int>(ev.id);
	Potassco::StringBuilder str(line, sizeof(line));
	if      (const BasicSolveEvent* be = event_cast<BasicSolveEvent>(ev)) { Clasp::Cli::formatEvent(*be, str); }
	else if (const SolveTestEvent*  te = event_cast<SolveTestEvent>(ev) ) { Clasp::Cli::formatEvent(*te, str); lEnd= te->result == -1 ? '\r' : '\n'; }
#if CLASP_HAS_THREADS
	else if (const mt::MessageEvent*me = event_cast<mt::MessageEvent>(ev)){
		Clasp::Cli::formatEvent(*me, str);
		eventId = LogEvent::id_s;
	}
#endif
	else if (const LogEvent* log = event_cast<LogEvent>(ev))              {
		char timeBuffer[30];
		Potassco::StringBuilder time(timeBuffer, sizeof(timeBuffer));
		time.appendFormat("[Solving+%.3fs]", RealTime::getTime() - stTime_);
		str.appendFormat("%2u:L| %-30s %-38s |", log->solver->id(), time.c_str(), log->msg);
	}
	else                                                                  { return; }
	str.appendFormat(" %10.3fs |", elapsedTime());
	FileLock lock(stdout);
	if (progress_.lines <= 0 || eventId != progress_.last) {
		if (progress_.lines <= 0) {
			const char* prefix = format[cat_comment];
			if ((this->verbosity() & 1) != 0 || ev.id == SolveTestEvent::id_s) {
				printf("%s%s\n"
					"%sID:T       Vars           Constraints         State            Limits            Time     |\n"
					"%s       #free/#fixed   #problem/#learnt  #conflicts/ratio #conflict/#learnt                |\n"
					"%s%s\n", prefix, rowSep, prefix, prefix, prefix, rowSep);
			}
			else {
				printf("%s%s\n"
					"%sID:T       Info                     Info                      Info               Time     |\n"
					"%s%s\n", prefix, rowSep, prefix, prefix, rowSep);
			}
			progress_.lines = 20;
		}
		else if (progress_.last != -1) {
			printLN(cat_comment, "%s", rowSep);
		}
		progress_.last = eventId;
	}
	progress_.lines -= static_cast<int>(lEnd == '\n');
	printf("%s%s%c", format[cat_comment], line, lEnd);
}

static inline bool endsWith(const char* str, char c) {
	return *str && str[strlen(str) - 1] == c;
}

const char* TextOutput::getIfsSuffix(char ifs, CategoryKey c) const {
	return ifs != '\n' || endsWith(format[c], '\n') ? "" : format[c];
}
const char* TextOutput::getIfsSuffix(CategoryKey c) const {  return getIfsSuffix(ifs_[0], c);  }
const char* TextOutput::fieldSeparator() const { return ifs_; }
bool TextOutput::clearProgress(int nLines) {
	if (progress_.last != -1) {
		if (progress_.last != INT_MAX ) { progress_.last = INT_MAX; comment(2, "%s\n", rowSep); }
		progress_.lines -= nLines;
		return true;
	}
	return false;
}
int TextOutput::printSep(CategoryKey k) const { return printf("%s%s", fieldSeparator(), getIfsSuffix(k)); }
uintp TextOutput::doPrint(const OutPair& s, UPtr data) {
	const uint32 MSB = 31u;
	uint32& accu    = reinterpret_cast<UPair*>(data)->first;
	uint32& maxLine = reinterpret_cast<UPair*>(data)->second;
	if (accu == 0 && *getIfsSuffix(cat_value)) store_set_bit(accu, MSB);
	const char* suf = test_bit(accu, MSB) ? format[cat_value] : "";
	store_clear_bit(accu, MSB);
	if      (accu < maxLine) { accu += printf("%c%s", *fieldSeparator(), suf); }
	else if (!maxLine)       { maxLine = s.first || *fieldSeparator() != ' ' ? UINT32_MAX : 70; }
	else                     { printf("%c%s", '\n', getIfsSuffix('\n', cat_value)); accu = 0; }
	if (s.first){ accu += printf(format[cat_atom_name], s.first); }
	else        { accu += printf(format[cat_atom_var] + !s.second.sign(), static_cast<int>(s.second.var())); }
	if (*suf) store_set_bit(accu, MSB);
	return data;
}
void TextOutput::printValues(const OutputTable& out, const Model& m) {
	printf("%s", format[cat_value]);
	UPair data;
	printWitness(out, m, reinterpret_cast<UPtr>(&data));
	if (*format[cat_value_term]) {
		printf("%c%s%s", *fieldSeparator(), getIfsSuffix(cat_value), format[cat_value_term]);
	}
	printf("\n");
}
void TextOutput::printMeta(const OutputTable& out, const Model& m) {
	if (m.consequences()) {
		UPair cons = numCons(out, m);
		printLN(cat_comment, "Consequences: [%u;%u]", cons.first, cons.first + cons.second);
	}
	if (m.costs) {
		printf("%s", format[cat_objective]);
		printCosts(*m.costs);
		printf("\n");
	}
}
void TextOutput::printModelValues(const OutputTable& out, const Model& m) {
	printValues(out, m);
}
void TextOutput::printModel(const OutputTable& out, const Model& m, PrintLevel x) {
	FileLock lock(stdout);
	bool printValues = modelQ() <= x;
	bool printOpt    = optQ() <= x;
	if (printValues || printOpt) {
		const char* type = !m.up ? "Answer" : "Update";
		clearProgress(3);
		comment(1, "%s: %" PRIu64" (Time: %.3fs)\n", type, m.num, modelTime());
		if (printValues) { printModelValues(out, m); }
		if (printOpt)    { printMeta(out, m); }
	}
}
void TextOutput::printUnsat(const OutputTable& out, const LowerBound* lower, const Model* prevModel) {
	FileLock lock(stdout);
	if (lower && optQ() == print_all) {
		const SumVec* costs = prevModel ? prevModel->costs : 0;
		double ts = elapsedTime();
		clearProgress(1);
		comment(0, "%-12s: ", "Progression");
		if (costs && costs->size() > lower->level) {
			for (uint32 i = 0; i != lower->level; ++i) {
				printf("%" PRId64 " ", (*costs)[i]);
			}
			wsum_t ub = (*costs)[lower->level];
			int w = 1; for (wsum_t x = ub; x > 9; ++w) { x /= 10; }
			double err = double(ub - lower->bound)/double(lower->bound);
			if (err < 0) { err = -err; }
			printf("[%*" PRId64 ";%" PRId64 "] (Error: %g ", w, lower->bound, ub, err);
		}
		else {
			printf("[%6" PRId64 ";inf] (", lower->bound);
		}
		printf("Time: %.3fs)\n", ts);
	}
	if (prevModel && prevModel->up && optQ() == print_all) {
		printMeta(out, *prevModel);
	}
}

void TextOutput::printBounds(const SumVec& lower, const SumVec& upper) const {
	const char* sep = "";
	for (uint32 i = 0, uMax = upper.size(), lMax = lower.size(), end = std::max(uMax, lMax); i != end; ++i) {
		if (i >= uMax) {
			printf("%s[%" PRId64";*]", sep, lower[i]);
		}
		else if (i >= lMax || lower[i] == upper[i]) {
			printf("%s%" PRId64, sep, upper[i]);
		}
		else {
			printf("%s[%" PRId64";%" PRId64"]", sep, lower[i], upper[i]);
		}
		sep = " ";
	}
}

void TextOutput::printCosts(const SumVec& costs) const {
	printCostsImpl(costs, *fieldSeparator(), getIfsSuffix(cat_objective));
}

void TextOutput::printCostsImpl(const SumVec& costs, char ifs, const char* ifsSuffix) const {
	if (!costs.empty()) {
		printf("%" PRId64, costs[0]);
		for (uint32 i = 1, end = (uint32)costs.size(); i != end; ++i) {
			printf("%c%s%" PRId64, ifs, ifsSuffix, costs[i]);
		}
	}
}
bool TextOutput::startSection(const char* n) const {
	printLN(cat_comment, "============ %s Stats ============", n);
	printBR(cat_comment);
	return true;
}
void TextOutput::startObject(const char* n, uint32 i) const {
	printLN(cat_comment, "[%s %u]", n, i);
	printBR(cat_comment);
}
bool TextOutput::visitThreads(Operation op) {
	accu_ = false;
	return op != Enter || startSection("Thread");
}
bool TextOutput::visitTester(Operation op) {
	accu_ = false;
	return op != Enter || startSection("Tester");
}
void TextOutput::visitThread(uint32 i, const SolverStats& stats) {
	startObject("Thread", i);
	TextOutput::visitSolverStats(stats);
}
void TextOutput::visitHcc(uint32 i, const ProblemStats& p, const SolverStats& s) {
	startObject("HCC", i);
	TextOutput::visitProblemStats(p);
	TextOutput::visitSolverStats(s);
}
void TextOutput::visitLogicProgramStats(const Asp::LpStats& lp) {
	using namespace Asp;
	uint32 rFinal = lp.rules[1].sum(), rOriginal = lp.rules[0].sum();
	printKeyValue("Rules", "%-8u", rFinal);
	if (rFinal != rOriginal) {
		printf(" (Original: %u)", rOriginal);
	}
	printf("\n");
	Potassco::StringBuilder out;
	for (uint32 i = 0; i != RuleStats::numKeys(); ++i) {
		if (i == RuleStats::Normal) { continue; }
		if (uint32 r = lp.rules[0][i]) {
			printKeyValue(out.append("  ").append(RuleStats::toStr(i)).c_str(), "%-8u", lp.rules[1][i]);
			if (r != lp.rules[1][i]) { printf(" (Original: %u)", r); }
			printf("\n");
			out.clear();
		}
	}
	printKeyValue("Atoms", "%-8u", lp.atoms);
	if (lp.auxAtoms) {
		printf(" (Original: %u Auxiliary: %u)", lp.atoms-lp.auxAtoms, lp.auxAtoms);
	}
	printf("\n");
	if (lp.disjunctions[0]) {
		printKeyValue("Disjunctions", "%-8u", lp.disjunctions[1]);
		printf(" (Original: %u)\n", lp.disjunctions[0]);
	}
	uint32 bFinal = lp.bodies[1].sum(), bOriginal = lp.bodies[0].sum();
	printKeyValue("Bodies", "%-8u", bFinal);
	if (bFinal != bOriginal) {
		printf(" (Original: %u)", bOriginal);
	}
	printf("\n");
	for (uint32 i = 1; i != BodyStats::numKeys(); ++i) {
		if (uint32 b = lp.bodies[0][i]) {
			printKeyValue(out.append("  ").append(BodyStats::toStr(i)).c_str(), "%-8u", lp.bodies[1][i]);
			if (b != lp.bodies[1][i]) { printf(" (Original: %u)", b); }
			printf("\n");
			out.clear();
		}
	}
	if (lp.eqs() > 0) {
		printKeyValue("Equivalences", "%-8u", lp.eqs());
		printf(" (Atom=Atom: %u Body=Body: %u Other: %u)\n"
			, lp.eqs(Var_t::Atom)
			, lp.eqs(Var_t::Body)
			, lp.eqs(Var_t::Hybrid));
	}
	printKey("Tight");
	if      (lp.sccs == 0)              { printf("Yes"); }
	else if (lp.sccs != PrgNode::noScc) { printf("%-8s (SCCs: %u Non-Hcfs: %u Nodes: %u Gammas: %u)", "No", lp.sccs, lp.nonHcfs, lp.ufsNodes, lp.gammas); }
	else                                { printf("N/A"); }
	printf("\n");
}
void TextOutput::visitProblemStats(const ProblemStats& ps) {
	uint32 sum = ps.numConstraints();
	printKeyValue("Variables", "%-8u", ps.vars.num);
	printf(" (Eliminated: %4u Frozen: %4u)\n", ps.vars.eliminated, ps.vars.frozen);
	printKeyValue("Constraints", "%-8u", sum);
	printf(" (Binary: %5.1f%% Ternary: %5.1f%% Other: %5.1f%%)\n"
		, percent(ps.constraints.binary, sum)
		, percent(ps.constraints.ternary, sum)
		, percent(ps.constraints.other, sum));
	if (ps.acycEdges) {
		printKeyValue("Acyc-Edges", "%-8u\n", ps.acycEdges);
	}
	printBR(cat_comment);
}
void TextOutput::visitSolverStats(const SolverStats& st) {
	printStats(st);
	printBR(cat_comment);
}

int TextOutput::printChildKey(unsigned level, const char* key, uint32 idx, const char* pre) const {
	const unsigned indent = level * 2;
	int len = 0;
	printf("%s%-*.*s", format[cat_comment], indent, indent, " ");
	if      (key) { len = printf("%s", key); }
	else if (pre) { len = printf("[%s %u]", pre, idx); }
	else          { len = printf("[%u]", idx); }
	return (width_ - (int)indent) - len;
}

void TextOutput::printChildren(const StatisticObject& s, unsigned level, char const* prefix) {
	const bool map = s.type() == Potassco::Statistics_t::Map;
	for (uint32 i = 0; i != s.size(); ++i) {
		const char* key       = map ? s.key(i)  : 0;
		StatisticObject child = map ? s.at(key) : s[i];
		if (child.type() == Potassco::Statistics_t::Value) {
			int align = printChildKey(level, key, i, prefix);
			printf("%-*s: %g\n", std::max(0, align), "", child.value());
		}
		else if (child.type() == Potassco::Statistics_t::Array && key) {
			printChildren(child, level, key);
		}
		else if (child.size()) {
			printChildKey(level, key, i, prefix);
			printf("\n");
			printChildren(child, level + 1);
		}
	}
}

void TextOutput::visitExternalStats(const StatisticObject& stats) {
	POTASSCO_ASSERT(stats.type() == Potassco::Statistics_t::Map, "Non map statistic!");
	printChildren(stats);
}

void TextOutput::printStats(const SolverStats& st) const {
	if (!accu_ && st.extra) {
		printKeyValue("CPU Time", "%.3fs\n", st.extra->cpuTime);
		printKeyValue("Models", "%" PRIu64"\n", st.extra->models);
	}
	printKeyValue("Choices", "%-8" PRIu64, st.choices);
	if (st.extra && st.extra->domChoices) { printf(" (Domain: %" PRIu64")", st.extra->domChoices); }
	printf("\n");
	printKeyValue("Conflicts", "%-8" PRIu64"", st.conflicts);
	printf(" (Analyzed: %" PRIu64")\n", st.backjumps());
	printKeyValue("Restarts", "%-8" PRIu64"", st.restarts);
	if (st.restarts) {
		printf(" (Average: %.2f Last: %" PRIu64" Blocked: %" PRIu64")", st.avgRestart(), st.lastRestart, st.blRestarts);
	}
	printf("\n");
	if (!st.extra) return;
	const ExtendedStats& stx = *st.extra;
	if (stx.hccTests) {
		printKeyValue("Stab. Tests", "%-8" PRIu64, stx.hccTests);
		printf(" (Full: %" PRIu64" Partial: %" PRIu64")\n", stx.hccTests - stx.hccPartial, stx.hccPartial);
	}
	if (stx.models) {
		printKeyValue("Model-Level", "%-8.1f\n", stx.avgModel());
	}
	printKeyValue("Problems", "%-8" PRIu64, (uint64)stx.gps);
	printf(" (Average Length: %.2f Splits: %" PRIu64")\n", stx.avgGp(), (uint64)stx.splits);
	uint64 sum = stx.lemmas();
	printKeyValue("Lemmas", "%-8" PRIu64, sum);
	printf(" (Deleted: %" PRIu64")\n", stx.deleted);
	printKeyValue("  Binary", "%-8" PRIu64, uint64(stx.binary));
	printf(" (Ratio: %6.2f%%)\n", percent(stx.binary, sum));
	printKeyValue("  Ternary", "%-8" PRIu64, uint64(stx.ternary));
	printf(" (Ratio: %6.2f%%)\n", percent(stx.ternary, sum));
	const char* names[] = {"  Conflict", "  Loop", "  Other"};
	for (int i = 0; i != sizeof(names)/sizeof(names[0]); ++i) {
		ConstraintType type = static_cast<ConstraintType>(i+1);
		printKeyValue(names[i], "%-8" PRIu64, stx.lemmas(type));
		printf(" (Average Length: %6.1f Ratio: %6.2f%%) \n", stx.avgLen(type), percent(stx.lemmas(type), sum));
	}
	if (stx.distributed || stx.integrated) {
		printKeyValue("  Distributed", "%-8" PRIu64, stx.distributed);
		printf(" (Ratio: %6.2f%% Average LBD: %.2f) \n", stx.distRatio()*100.0, stx.avgDistLbd());
		printKeyValue("  Integrated", "%-8" PRIu64, stx.integrated);
		if (accu_) { printf(" (Ratio: %6.2f%% ", stx.intRatio()*100.0); }
		else { printf(" ("); }
		printf("Unit: %" PRIu64" Average Jumps: %.2f)\n", stx.intImps, stx.avgIntJump());
	}
	printJumps(stx.jumps);
}
void TextOutput::printJumps(const JumpStats& st) const {
	printKeyValue("Backjumps", "%-8" PRIu64, st.jumps);
	printf(" (Average: %5.2f Max: %3u Sum: %6" PRIu64")\n", st.avgJump(), st.maxJump, st.jumpSum);
	printKeyValue("  Executed", "%-8" PRIu64, st.jumps-st.bounded);
	printf(" (Average: %5.2f Max: %3u Sum: %6" PRIu64" Ratio: %6.2f%%)\n", st.avgJumpEx(), st.maxJumpEx, st.jumped(), st.jumpedRatio()*100.0);
	printKeyValue("  Bounded", "%-8" PRIu64, st.bounded);
	printf(" (Average: %5.2f Max: %3u Sum: %6" PRIu64" Ratio: %6.2f%%)\n", st.avgBound(), st.maxBound, st.boundSum, 100.0 - (st.jumpedRatio()*100.0));
}

#undef printKeyValue
#undef printKey
#undef printLN
#undef printBR
}}
