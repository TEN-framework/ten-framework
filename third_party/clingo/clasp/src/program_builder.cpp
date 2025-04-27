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
#include <clasp/program_builder.h>
#include <clasp/shared_context.h>
#include <clasp/solver.h>
#include <clasp/clause.h>
#include <clasp/weight_constraint.h>
#include <clasp/parser.h>
#include <limits>
namespace Clasp {

/////////////////////////////////////////////////////////////////////////////////////////
// class ProgramBuilder
/////////////////////////////////////////////////////////////////////////////////////////
ProgramBuilder::ProgramBuilder() : ctx_(0), frozen_(true) {}
ProgramBuilder::~ProgramBuilder() {}
bool ProgramBuilder::ok() const { return ctx_ && ctx_->ok(); }
bool ProgramBuilder::startProgram(SharedContext& ctx) {
	ctx.report(Event::subsystem_load);
	ctx_    = &ctx;
	frozen_ = ctx.frozen();
	return ctx_->ok() && doStartProgram();
}
bool ProgramBuilder::updateProgram() {
	POTASSCO_REQUIRE(ctx_, "startProgram() not called!");
	bool up = frozen();
	bool ok = ctx_->ok() && ctx_->unfreeze() && doUpdateProgram() && (ctx_->setSolveMode(SharedContext::solve_multi), true);
	frozen_ = ctx_->frozen();
	if (up && !frozen()){ ctx_->report(Event::subsystem_load); }
	return ok;
}
bool ProgramBuilder::endProgram() {
	POTASSCO_REQUIRE(ctx_, "startProgram() not called!");
	bool ok = ctx_->ok();
	if (ok && !frozen_) {
		ctx_->report(Event::subsystem_prepare);
		ok = doEndProgram();
		frozen_ = true;
	}
	return ok;
}
void ProgramBuilder::getAssumptions(LitVec& out) const {
	POTASSCO_REQUIRE(ctx_ && frozen());
	doGetAssumptions(out);
}
void ProgramBuilder::getWeakBounds(SumVec& out) const {
	POTASSCO_REQUIRE(ctx_ && frozen());
	doGetWeakBounds(out);
}
ProgramParser& ProgramBuilder::parser() {
	if (!parser_.get()) {
		parser_.reset(doCreateParser());
	}
	return *parser_;
}
bool ProgramBuilder::parseProgram(std::istream& input) {
	POTASSCO_REQUIRE(ctx_ && !frozen());
	ProgramParser& p = parser();
	POTASSCO_REQUIRE(p.accept(input), "unrecognized input format");
	return p.parse();
}
void ProgramBuilder::addMinLit(weight_t prio, WeightLiteral x) {
	ctx_->addMinimize(x, prio);
}
void ProgramBuilder::markOutputVariables() const {
	const OutputTable& out = ctx_->output;
	for (OutputTable::range_iterator it = out.vars_begin(), end = out.vars_end(); it != end; ++it) {
		ctx_->setOutput(*it, true);
	}
	for (OutputTable::pred_iterator it = out.pred_begin(), end = out.pred_end(); it != end; ++it) {
		ctx_->setOutput(it->cond.var(), true);
	}
}
void ProgramBuilder::doGetWeakBounds(SumVec&) const  {}
/////////////////////////////////////////////////////////////////////////////////////////
// class SatBuilder
/////////////////////////////////////////////////////////////////////////////////////////
SatBuilder::SatBuilder() : ProgramBuilder(), hardWeight_(0), vars_(0), pos_(0) {}
bool SatBuilder::markAssigned() {
	if (pos_ == ctx()->master()->trail().size()) { return true; }
	bool ok = ctx()->ok() && ctx()->master()->propagate();
	for (const LitVec& trail = ctx()->master()->trail(); pos_ < trail.size(); ++pos_) {
		markLit(~trail[pos_]);
	}
	return ok;
}
void SatBuilder::prepareProblem(uint32 numVars, wsum_t cw, uint32 clauseHint) {
	POTASSCO_REQUIRE(ctx(), "startProgram() not called!");
	Var start = ctx()->addVars(numVars, Var_t::Atom, VarInfo::Input | VarInfo::Nant);
	ctx()->output.setVarRange(Range32(start, start + numVars));
	ctx()->startAddConstraints(std::min(clauseHint, uint32(10000)));
	varState_.resize(start + numVars);
	vars_       = ctx()->numVars();
	hardWeight_ = cw;
	markAssigned();
}
bool SatBuilder::addObjective(const WeightLitVec& min) {
	for (WeightLitVec::const_iterator it = min.begin(), end = min.end(); it != end; ++it) {
		addMinLit(0, *it);
		varState_[it->first.var()] |= (falseValue(it->first) << 2u);
	}
	return ctx()->ok();
}
void SatBuilder::addProject(Var v) {
	ctx()->output.addProject(posLit(v));
}
void SatBuilder::addAssumption(Literal x) {
	assume_.push_back(x);
}
bool SatBuilder::addClause(LitVec& clause, wsum_t cw) {
	if (!ctx()->ok() || satisfied(clause)) { return ctx()->ok(); }
	POTASSCO_REQUIRE(cw >= 0 && (cw <= std::numeric_limits<weight_t>::max() || cw == hardWeight_), "Clause weight out of bounds");
	if (cw == hardWeight_) {
		return ClauseCreator::create(*ctx()->master(), clause, Constraint_t::Static).ok() && markAssigned();
	}
	else {
		// Store weight, relaxtion var, and (optionally) clause
		softClauses_.push_back(Literal::fromRep((uint32)cw));
		if      (clause.size() > 1){ softClauses_.push_back(posLit(++vars_)); softClauses_.insert(softClauses_.end(), clause.begin(), clause.end()); }
		else if (!clause.empty())  { softClauses_.push_back(~clause.back());  }
		else                       { softClauses_.push_back(lit_true()); }
		softClauses_.back().flag(); // mark end of clause
	}
	return true;
}
bool SatBuilder::satisfied(LitVec& cc) {
	bool sat = false;
	LitVec::iterator j = cc.begin();
	for (LitVec::const_iterator it = cc.begin(), end = cc.end(); it != end; ++it) {
		Literal x = *it;
		uint32  m = 1+x.sign();
		uint32  n = uint32(varState_[it->var()] & 3u) + m;
		if      (n == m) { varState_[it->var()] |= m; x.unflag(); *j++ = x; }
		else if (n == 3u){ sat = true; break; }
	}
	cc.erase(j, cc.end());
	for (LitVec::const_iterator it = cc.begin(), end = cc.end(); it != end; ++it) {
		if (!sat) { varState_[it->var()] |= (varState_[it->var()] & 3u) << 2; }
		varState_[it->var()] &= ~3u;
	}
	return sat;
}
bool SatBuilder::addConstraint(WeightLitVec& lits, weight_t bound) {
	if (!ctx()->ok()) { return false; }
	WeightLitsRep rep = WeightLitsRep::create(*ctx()->master(), lits, bound);
	if (rep.open()) {
		for (const WeightLiteral* x = rep.lits, *end = rep.lits + rep.size; x != end; ++x) {
			varState_[x->first.var()] |= (trueValue(x->first) << 2);
		}
	}
	return WeightConstraint::create(*ctx()->master(), lit_true(), rep, 0u).ok();
}
bool SatBuilder::doStartProgram() {
	vars_ = ctx()->numVars();
	pos_  = 0;
	assume_.clear();
	return markAssigned();
}
ProgramParser* SatBuilder::doCreateParser() {
	return new SatParser(*this);
}
bool SatBuilder::doEndProgram() {
	bool ok = ctx()->ok();
	if (!softClauses_.empty() && ok) {
		ctx()->setPreserveModels(true);
		uint32 softVars = vars_ - ctx()->numVars();
		ctx()->addVars(softVars, Var_t::Atom, VarInfo::Nant);
		ctx()->startAddConstraints();
		LitVec cc;
		for (LitVec::const_iterator it = softClauses_.begin(), end = softClauses_.end(); it != end && ok; ++it) {
			weight_t w     = (weight_t)it->rep();
			Literal  relax = *++it;
			if (!relax.flagged()) {
				cc.assign(1, relax);
				do { cc.push_back(*++it); } while (!cc.back().flagged());
				cc.back().unflag();
				ok = ClauseCreator::create(*ctx()->master(), cc, Constraint_t::Static).ok();
			}
			addMinLit(0, WeightLiteral(relax.unflag(), w));
		}
		LitVec().swap(softClauses_);
	}
	if (ok) {
		const uint32 seen = 12;
		const bool   elim = !ctx()->preserveModels();
		for (Var v = 1; v != (Var)varState_.size() && ok; ++v) {
			uint32 m = varState_[v];
			if ( (m & seen) != seen ) {
				if      (m)   { ctx()->setNant(v, false); ctx()->master()->setPref(v, ValueSet::def_value, ValueRep(m >> 2)); }
				else if (elim){ ctx()->eliminate(v); }
			}
		}
		markOutputVariables();
	}
	return ok;
}
/////////////////////////////////////////////////////////////////////////////////////////
// class PBBuilder
/////////////////////////////////////////////////////////////////////////////////////////
PBBuilder::PBBuilder() : auxVar_(1) {}
void PBBuilder::prepareProblem(uint32 numVars, uint32 numProd, uint32 numSoft, uint32 numCons) {
	POTASSCO_REQUIRE(ctx(), "startProgram() not called!");
	Var out = ctx()->addVars(numVars, Var_t::Atom, VarInfo::Nant | VarInfo::Input);
	auxVar_ = ctx()->addVars(numProd + numSoft, Var_t::Atom, VarInfo::Nant);
	endVar_ = auxVar_ + numProd + numSoft;
	ctx()->output.setVarRange(Range32(out, out + numVars));
	ctx()->startAddConstraints(numCons);
}
uint32 PBBuilder::getAuxVar() {
	POTASSCO_REQUIRE(ctx()->validVar(auxVar_), "Variables out of bounds");
	return auxVar_++;
}
bool PBBuilder::addConstraint(WeightLitVec& lits, weight_t bound, bool eq, weight_t cw) {
	if (!ctx()->ok()) { return false; }
	Var eqVar = 0;
	if (cw > 0) { // soft constraint
		if (lits.size() != 1) {
			eqVar = getAuxVar();
			addMinLit(0, WeightLiteral(negLit(eqVar), cw));
		}
		else {
			if (lits[0].second < 0)    { bound += (lits[0].second = -lits[0].second); lits[0].first = ~lits[0].first; }
			if (lits[0].second < bound){ lits[0].first = lit_false(); }
			addMinLit(0, WeightLiteral(~lits[0].first, cw));
			return true;
		}
	}
	return WeightConstraint::create(*ctx()->master(), posLit(eqVar), lits, bound, !eq ? 0 : WeightConstraint::create_eq_bound).ok();
}

bool PBBuilder::addObjective(const WeightLitVec& min) {
	for (WeightLitVec::const_iterator it = min.begin(), end = min.end(); it != end; ++it) {
		addMinLit(0, *it);
	}
	return ctx()->ok();
}
void PBBuilder::addProject(Var v) {
	ctx()->output.addProject(posLit(v));
}
void PBBuilder::addAssumption(Literal x) {
	assume_.push_back(x);
}
bool PBBuilder::setSoftBound(wsum_t b) {
	if (b > 0) { soft_ = b-1; }
	return true;
}

void PBBuilder::doGetWeakBounds(SumVec& out) const {
	if (soft_ != std::numeric_limits<wsum_t>::max()) {
		if      (out.empty())   { out.push_back(soft_); }
		else if (out[0] > soft_){ out[0] = soft_; }
	}
}

Literal PBBuilder::addProduct(LitVec& lits) {
	if (!ctx()->ok()) { return lit_false(); }
	prod_.lits.reserve(lits.size() + 1);
	if (productSubsumed(lits, prod_)){
		return lits[0];
	}
	Literal& eq = products_[prod_];
	if (eq != lit_true()) {
		return eq;
	}
	eq = posLit(getAuxVar());
	addProductConstraints(eq, lits);
	return eq;
}
bool PBBuilder::productSubsumed(LitVec& lits, PKey& prod) {
	Literal last       = lit_true();
	LitVec::iterator j = lits.begin();
	Solver& s          = *ctx()->master();
	uint32  abst       = 0;
	prod.lits.assign(1, lit_true()); // room for abst
	for (LitVec::const_iterator it = lits.begin(), end = lits.end(); it != end; ++it) {
		if (s.isFalse(*it) || ~*it == last) { // product is always false
			lits.assign(1, lit_false());
			return true;
		}
		else if (last.var() > it->var()) { // not sorted - redo with sorted product
			std::sort(lits.begin(), lits.end());
			return productSubsumed(lits, prod);
		}
		else if (!s.isTrue(*it) && last != *it) {
			prod.lits.push_back(*it);
			abst += hashLit(*it);
			last  = *it;
			*j++  = last;
		}
	}
	prod.lits[0].rep() = abst;
	lits.erase(j, lits.end());
	if (lits.empty()) { lits.assign(1, lit_true()); }
	return lits.size() < 2;
}
void PBBuilder::addProductConstraints(Literal eqLit, LitVec& lits) {
	Solver& s = *ctx()->master();
	assert(s.value(eqLit.var()) == value_free);
	bool ok   = ctx()->ok();
	for (LitVec::iterator it = lits.begin(), end = lits.end(); it != end && ok; ++it) {
		assert(s.value(it->var()) == value_free);
		ok    = ctx()->addBinary(~eqLit, *it);
		*it   = ~*it;
	}
	lits.push_back(eqLit);
	if (ok) { ClauseCreator::create(s, lits, ClauseCreator::clause_no_prepare); }
}

bool PBBuilder::doStartProgram() {
	auxVar_ = ctx()->numVars() + 1;
	soft_   = std::numeric_limits<wsum_t>::max();
	assume_.clear();
	return true;
}
bool PBBuilder::doEndProgram() {
	while (auxVar_ != endVar_) {
		if (!ctx()->addUnary(negLit(getAuxVar()))) { return false; }
	}
	markOutputVariables();
	return true;
}
ProgramParser* PBBuilder::doCreateParser() {
	return new SatParser(*this);
}
/////////////////////////////////////////////////////////////////////////////////////////
// class BasicProgramAdapter
/////////////////////////////////////////////////////////////////////////////////////////
BasicProgramAdapter::BasicProgramAdapter(ProgramBuilder& prg) : prg_(&prg), inc_(false) {
	int t = prg_->type();
	POTASSCO_REQUIRE(t == Problem_t::Sat || t == Problem_t::Pb, "unknown program type");
}
void BasicProgramAdapter::initProgram(bool inc) { inc_ = inc; }
void BasicProgramAdapter::beginStep() { if (inc_ || prg_->frozen()) { prg_->updateProgram(); } }

void BasicProgramAdapter::rule(Potassco::Head_t, const Potassco::AtomSpan& head, const Potassco::LitSpan& body) {
	using namespace Potassco;
	POTASSCO_REQUIRE(empty(head), "unsupported rule type");
	if (prg_->type() == Problem_t::Sat) {
		clause_.clear();
		for (LitSpan::iterator it = begin(body), end = Potassco::end(body); it != end; ++it) { clause_.push_back(~toLit(*it)); }
		static_cast<SatBuilder&>(*prg_).addClause(clause_);
	}
	else {
		constraint_.clear();
		for (LitSpan::iterator it = begin(body), end = Potassco::end(body); it != end; ++it) { constraint_.push_back(WeightLiteral(~toLit(*it), 1)); }
		static_cast<PBBuilder&>(*prg_).addConstraint(constraint_, 1);
	}
}
void BasicProgramAdapter::rule(Potassco::Head_t, const Potassco::AtomSpan& head, Potassco::Weight_t bound, const Potassco::WeightLitSpan& body) {
	using namespace Potassco;
	POTASSCO_REQUIRE(empty(head), "unsupported rule type");
	constraint_.clear();
	Potassco::Weight_t sum = 0;
	for (WeightLitSpan::iterator it = begin(body), end = Potassco::end(body); it != end; ++it) {
		constraint_.push_back(WeightLiteral(~toLit(it->lit), it->weight));
		sum += it->weight;
	}
	if (prg_->type() == Problem_t::Sat) {
		static_cast<SatBuilder&>(*prg_).addConstraint(constraint_, (sum - bound) + 1);
	}
	else {
		static_cast<PBBuilder&>(*prg_).addConstraint(constraint_, (sum - bound) + 1);
	}
}
void BasicProgramAdapter::minimize(Potassco::Weight_t prio, const Potassco::WeightLitSpan& lits) {
	POTASSCO_REQUIRE(prio == 0, "unsupported rule type");
	using namespace Potassco;
	constraint_.clear();
	for (WeightLitSpan::iterator it = begin(lits), end = Potassco::end(lits); it != end; ++it) { constraint_.push_back(WeightLiteral(toLit(it->lit), it->weight)); }
	if (prg_->type() == Problem_t::Sat) {
		static_cast<SatBuilder&>(*prg_).addObjective(constraint_);
	}
	else {
		static_cast<PBBuilder&>(*prg_).addObjective(constraint_);
	}
}
}
