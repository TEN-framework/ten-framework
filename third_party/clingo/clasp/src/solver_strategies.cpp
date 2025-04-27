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
#include <clasp/solver_strategies.h>
#include <clasp/solver.h>
#include <clasp/heuristics.h>
#include <clasp/lookahead.h>
#include <cmath>
#if defined(__GNUC__) && __GNUC__ >= 8
#pragma GCC diagnostic ignored "-Wclass-memaccess"
#endif
namespace Clasp {
/////////////////////////////////////////////////////////////////////////////////////////
// SolverStrategies / SolverParams
/////////////////////////////////////////////////////////////////////////////////////////
SolverStrategies::SolverStrategies() {
	struct X { uint32 z[2]; };
	static_assert(sizeof(SolverStrategies) == sizeof(X), "Unsupported Padding");
	std::memset(this, 0, sizeof(SolverStrategies));
}
void SolverStrategies::prepare() {
	if (search == SolverStrategies::no_learning) {
		compress    = 0;
		saveProgress= 0;
		reverseArcs = 0;
		otfs        = 0;
		updateLbd   = 0;
		ccMinAntes  = SolverStrategies::no_antes;
		bumpVarAct  = 0;
	}
}
HeuParams::HeuParams() {
	std::memset(this, 0, sizeof(HeuParams));
	moms = 1;
}
OptParams::OptParams(Type t) {
	std::memset(this, 0, sizeof(OptParams));
	type = t;
}
SolverParams::SolverParams() {
	struct X { uint32 strat[2]; uint32 self[5]; };
	static_assert(sizeof(SolverParams) == sizeof(X), "Unsupported Padding");
	std::memset(&seed, 0, sizeof(uint32)*2);
	seed = RNG().seed();
}
uint32 SolverParams::prepare() {
	uint32 res = 0;
	if (search == SolverStrategies::no_learning && Heuristic_t::isLookback(heuId)) {
		heuId = Heuristic_t::None;
		res  |= 1;
	}
	if (heuId == Heuristic_t::Unit) {
		if (!Lookahead::isType(lookType)) { res |= 2; lookType = Var_t::Atom; }
		lookOps = 0;
	}
	if (heuId != Heuristic_t::Domain && (heuristic.domPref || heuristic.domMod)) {
		res |= 4;
		heuristic.domPref= 0;
		heuristic.domMod = 0;
	}
	SolverStrategies::prepare();
	return res;
}
/////////////////////////////////////////////////////////////////////////////////////////
// ScheduleStrategy
/////////////////////////////////////////////////////////////////////////////////////////
double growR(uint32 idx, double g) { return pow(g, (double)idx); }
double addR(uint32 idx, double a)  { return a * idx; }
uint32 lubyR(uint32 idx)           {
	uint32 i = idx + 1;
	while ((i & (i+1)) != 0) {
		i    -= ((1u << log2(i)) - 1);
	}
	return (i+1)>>1;
}
ScheduleStrategy::ScheduleStrategy(Type t, uint32 b, double up, uint32 lim)
	: base(b), type(t), idx(0), len(lim), grow(0.0)  {
	if      (t == Geometric)  { grow = static_cast<float>(std::max(1.0, up)); }
	else if (t == Arithmetic) { grow = static_cast<float>(std::max(0.0, up)); }
	else if (t == User)       { grow = static_cast<float>(std::max(0.0, up)); }
	else if (t == Luby && lim){ len  = std::max(uint32(2), (static_cast<uint32>(std::pow(2.0, std::ceil(log(double(lim))/log(2.0)))) - 1)*2); }
}

uint64 ScheduleStrategy::current() const {
	enum { t_add = ScheduleStrategy::Arithmetic, t_luby = ScheduleStrategy::Luby };
	if      (base == 0)     return UINT64_MAX;
	else if (type == t_add) return static_cast<uint64>(addR(idx, grow)  + base);
	else if (type == t_luby)return static_cast<uint64>(lubyR(idx)) * base;
	uint64 x = static_cast<uint64>(growR(idx, grow) * base);
	return x + !x;
}
uint64 ScheduleStrategy::next() {
	if (++idx != len) { return current(); }
	// length reached or overflow
	len = (len + !!idx) << uint32(type == Luby);
	idx = 0;
	return current();
}
void ScheduleStrategy::advanceTo(uint32 n) {
	if (!len || n < len)       {
		idx = n;
		return;
	}
	if (type != Luby) {
		double dLen = len;
		uint32 x    = uint32(sqrt(dLen * (4.0 * dLen - 4.0) + 8.0 * double(n+1))-2*dLen+1)/2;
    idx         = n - uint32(x*dLen+double(x-1.0)*x/2.0);
    len        += x;
		return;
	}
	while (n >= len) {
		n   -= len++;
		len *= 2;
	}
	idx = n;
}
/////////////////////////////////////////////////////////////////////////////////////////
// RestartParams
/////////////////////////////////////////////////////////////////////////////////////////
RestartParams::RestartParams()
	: sched()
	, blockScale(1.4f), blockWindow(0), blockFirst(0)
	, counterRestart(0), counterBump(9973)
	, shuffle(0), shuffleNext(0)
	, upRestart(0), cntLocal(0), dynRestart(0) {
	static_assert(sizeof(RestartParams) == sizeof(ScheduleStrategy) + (3 * sizeof(uint32)) + sizeof(float), "Invalid structure alignment");
}
void RestartParams::disable() {
	std::memset(this, 0, sizeof(RestartParams));
	sched = ScheduleStrategy::none();
}
uint32 RestartParams::prepare(bool withLookback) {
	if (!withLookback || sched.disabled()) {
		disable();
	}
	return 0;
}
/////////////////////////////////////////////////////////////////////////////////////////
// ReduceParams
/////////////////////////////////////////////////////////////////////////////////////////
uint32 ReduceParams::getLimit(uint32 base, double f, const Range32& r) {
	base = (f != 0.0 ? (uint32)std::min(base*f, double(UINT32_MAX)) : UINT32_MAX);
	return r.clamp( base );
}
uint32 ReduceParams::getBase(const SharedContext& ctx) const {
	uint32 st = strategy.estimate != ReduceStrategy::est_dynamic || ctx.isExtended() ? strategy.estimate : (uint32)ReduceStrategy::est_num_constraints;
	switch(st) {
		default:
		case ReduceStrategy::est_dynamic        : {
			uint32 m = std::min(ctx.stats().vars.num, ctx.stats().numConstraints());
			uint32 M = std::max(ctx.stats().vars.num, ctx.stats().numConstraints());
			return M > (m * 10) ? M : m;
		}
		case ReduceStrategy::est_con_complexity : return ctx.stats().complexity;
		case ReduceStrategy::est_num_constraints: return ctx.stats().numConstraints();
		case ReduceStrategy::est_num_vars       : return ctx.stats().vars.num;
	}
}
void ReduceParams::disable() {
	cflSched  = ScheduleStrategy::none();
	growSched = ScheduleStrategy::none();
	strategy.fReduce = 0;
	fGrow     = 0.0f; fInit = 0.0f; fMax = 0.0f;
	initRange = Range<uint32>(UINT32_MAX, UINT32_MAX);
	maxRange  = UINT32_MAX;
	memMax    = 0;
}
Range32 ReduceParams::sizeInit(const SharedContext& ctx) const {
	if (!growSched.disabled() || growSched.defaulted()) {
		uint32 base = getBase(ctx);
		uint32 lo   = std::min(getLimit(base, fInit, initRange), maxRange);
		uint32 hi   = getLimit(base, fMax, Range32(lo, maxRange));
		return Range32(lo, hi);
	}
	return Range32(maxRange, maxRange);
}
uint32 ReduceParams::cflInit(const SharedContext& ctx) const {
	return cflSched.disabled() ? 0 : getLimit(getBase(ctx), fInit, initRange);
}
uint32 ReduceParams::prepare(bool withLookback) {
	if (!withLookback || fReduce() == 0.0f) {
		disable();
		return 0;
	}
	if (cflSched.defaulted() && growSched.disabled() && !growSched.defaulted()) {
		cflSched = ScheduleStrategy::arith(4000, 600);
	}
	if (fMax != 0.0f) { fMax = std::max(fMax, fInit); }
	return 0;
}
/////////////////////////////////////////////////////////////////////////////////////////
// SolveParams
/////////////////////////////////////////////////////////////////////////////////////////
SolveParams::SolveParams()
	: randRuns(0u), randConf(0u)
	, randProb(0.0f) {
}
uint32 SolveParams::prepare(bool withLookback) {
	return restart.prepare(withLookback) | reduce.prepare(withLookback);
}
bool SolveParams::randomize(Solver& s) const {
	for (uint32 r = 0, c = randConf; r != randRuns && c; ++r) {
		if (s.search(c, UINT32_MAX, false, 1.0) != value_free) { return !s.hasConflict(); }
		s.undoUntil(0);
	}
	return true;
}
/////////////////////////////////////////////////////////////////////////////////////////
// Configurations
/////////////////////////////////////////////////////////////////////////////////////////
Configuration::~Configuration() {}
Configuration* Configuration::config(const char* n) {
	return !n || !*n || ((*n == '.' || *n == '/') && !n[1]) ? this : 0;
}
bool UserConfiguration::addPost(Solver& s) const {
	const SolverOpts& x = solver(s.id());
	bool  ok            = true;
	if (Lookahead::isType(x.lookType)) {
		PostPropagator* pp = s.getPost(PostPropagator::priority_reserved_look);
		if (pp) { pp->destroy(&s, true); }
		Lookahead::Params p(static_cast<VarType>(x.lookType));
		p.nant(x.heuristic.nant != 0);
		p.limit(x.lookOps);
		ok = s.addPost(new Lookahead(p));
	}
	return ok;
}

BasicSatConfig::HeuristicCreator::~HeuristicCreator() {}

BasicSatConfig::BasicSatConfig() {
	solver_.push_back(SolverParams());
	search_.push_back(SolveParams());
}
void BasicSatConfig::prepare(SharedContext& ctx) {
	uint32 warn = 0;
	for (uint32 i = 0, end = solver_.size(), mod = search_.size(); i != end; ++i) {
		warn |= solver_[i].prepare();
		warn |= search_[i%mod].prepare(solver_[i].search != SolverStrategies::no_learning);
		if (solver_[i].updateLbd == SolverStrategies::lbd_fixed && search_[i%mod].reduce.strategy.protect) { warn |= 8; }
	}
	if ((warn & 1) != 0) { ctx.warn("Selected heuristic requires lookback strategy!"); }
	if ((warn & 2) != 0) { ctx.warn("Heuristic 'Unit' implies lookahead. Using 'atom'."); }
	if ((warn & 4) != 0) { ctx.warn("Domain options require heuristic 'Domain'!"); }
	if ((warn & 8) != 0) { ctx.warn("Deletion protection requires LBD updates, which are off!"); }
}
DecisionHeuristic* BasicSatConfig::heuristic(uint32 i)  const {
	const SolverParams& p = BasicSatConfig::solver(i);
	Heuristic_t::Type hId = static_cast<Heuristic_t::Type>(p.heuId);
	if (hId == Heuristic_t::Default && p.search == SolverStrategies::use_learning) hId = Heuristic_t::Berkmin;
	POTASSCO_REQUIRE(p.search == SolverStrategies::use_learning || !Heuristic_t::isLookback(hId), "Selected heuristic requires lookback!");
	DecisionHeuristic* h = 0;
	if (heu_.get()) { h = heu_->create(hId, p.heuristic); }
	if (!h) { h = Heuristic_t::create(hId, p.heuristic); }
	if (Lookahead::isType(p.lookType) && p.lookOps > 0 && hId != Heuristic_t::Unit) {
		h = UnitHeuristic::restricted(h);
	}
	return h;
}
SolverParams& BasicSatConfig::addSolver(uint32 i) {
	while (i >= solver_.size()) {
		solver_.push_back(SolverParams().setId(static_cast<uint32>(solver_.size())));
	}
	return solver_[i];
}
SolveParams& BasicSatConfig::addSearch(uint32 i) {
	if (i >= search_.size()) { search_.resize(i+1); }
	return search_[i];
}

void BasicSatConfig::reset() {
	static_cast<ContextParams&>(*this) = ContextParams();
	BasicSatConfig::resize(1, 1);
	solver_[0] = SolverParams();
	search_[0] = SolveParams();
}
void BasicSatConfig::resize(uint32 solver, uint32 search) {
	solver_.resize(solver);
	search_.resize(search);
}
void BasicSatConfig::setHeuristicCreator(HeuristicCreator* hc, Ownership_t::Type owner) {
	HeuFactory(hc, owner).swap(heu_);
}
/////////////////////////////////////////////////////////////////////////////////////////
// Heuristics
/////////////////////////////////////////////////////////////////////////////////////////
DecisionHeuristic* Heuristic_t::create(Type id, const HeuParams& p) {
	if (id == Berkmin) { return new ClaspBerkmin(p); }
	if (id == Vmtf)    { return new ClaspVmtf(p); }
	if (id == Unit)    { return new UnitHeuristic(); }
	if (id == Vsids)   { return new ClaspVsids(p); }
	if (id == Domain)  { return new DomainHeuristic(p); }
	POTASSCO_REQUIRE(id == Default || id == None, "Unknown heuristic id!");
	return new SelectFirst();
}

ModelHandler::~ModelHandler() {}
bool ModelHandler::onUnsat(const Solver&, const Model&) { return true; }
}
