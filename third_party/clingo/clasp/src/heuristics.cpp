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
#include <clasp/heuristics.h>
#include <clasp/clause.h>
#include <potassco/basic_types.h>
#include <algorithm>
#include <limits>
#include <cstdlib>
#include <string>
#include <utility>
#include <cmath>
namespace Clasp {
/////////////////////////////////////////////////////////////////////////////////////////
// Lookback selection strategies
/////////////////////////////////////////////////////////////////////////////////////////
uint32 momsScore(const Solver& s, Var v) {
	uint32 sc;
	if (s.sharedContext()->numBinary()) {
		uint32 s1 = s.estimateBCP(posLit(v), 0) - 1;
		uint32 s2 = s.estimateBCP(negLit(v), 0) - 1;
		sc = ((s1 * s2)<<10) + (s1 + s2);
	}
	else {
		// problem does not contain binary constraints - fall back to counting watches
		uint32 s1 = s.numWatches(posLit(v));
		uint32 s2 = s.numWatches(negLit(v));
		sc = ((s1 * s2)<<10) + (s1 + s2);
	}
	return sc;
}
static void addOther(TypeSet& t, uint32 other) {
	if (other != HeuParams::other_no)  { t.addSet(Constraint_t::Loop); }
	if (other == HeuParams::other_all) { t.addSet(Constraint_t::Other); }
}
/////////////////////////////////////////////////////////////////////////////////////////
// Berkmin selection strategy
/////////////////////////////////////////////////////////////////////////////////////////
#define BERK_NUM_CANDIDATES 5
#define BERK_CACHE_GROW 2.0
#define BERK_MAX_MOMS_VARS 9999
#define BERK_MAX_MOMS_DECS 50
#define BERK_MAX_DECAY 65534

ClaspBerkmin::ClaspBerkmin(const HeuParams& params)
	: topConflict_(UINT32_MAX)
	, topOther_(UINT32_MAX)
	, front_(1)
	, cacheSize_(5)
	, numVsids_(0) {
	ClaspBerkmin::setConfig(params);
}

void ClaspBerkmin::setConfig(const HeuParams& params) {
	maxBerkmin_     = params.param == 0 ? UINT32_MAX : params.param;
	order_.nant     = params.nant != 0;
	order_.huang    = params.huang != 0;
	order_.resScore = params.score == HeuParams::score_auto ? static_cast<uint32>(HeuParams::score_multi_set) : params.score;
	addOther(types_ = TypeSet(), params.other);
	if (params.moms) { types_.addSet(Constraint_t::Static); }
}

Var ClaspBerkmin::getMostActiveFreeVar(const Solver& s) {
	++numVsids_;
	// first: check for a cache hit
	for (Pos end = cache_.end(); cacheFront_ != end; ++cacheFront_) {
		if (s.value(*cacheFront_) == value_free) {
			return *cacheFront_;
		}
	}
	// Second: cache miss - refill cache with most active vars
	if (!cache_.empty() && cacheSize_ < s.numFreeVars()/10) {
		cacheSize_ = static_cast<uint32>( (cacheSize_*BERK_CACHE_GROW) + .5 );
	}
	cache_.clear();
	Order::Compare  comp(&order_);
	// Pre: At least one unassigned var!
	for (; s.value( front_ ) != value_free; ++front_) {;}
	Var v = front_;
	LitVec::size_type cs = std::min(cacheSize_, s.numFreeVars());
	for (;;) { // add first cs free variables to cache
		cache_.push_back(v);
		std::push_heap(cache_.begin(), cache_.end(), comp);
		if (cache_.size() == cs) break;
		while ( s.value(++v) != value_free ) {;} // skip over assigned vars
	}
	for (v = (cs == cacheSize_ ? v+1 : s.numVars()+1); v <= s.numVars(); ++v) {
		// replace vars with low activity
		if (s.value(v) == value_free && comp(v, cache_[0])) {
			std::pop_heap(cache_.begin(), cache_.end(), comp);
			cache_.back() = v;
			std::push_heap(cache_.begin(), cache_.end(), comp);
		}
	}
	std::sort_heap(cache_.begin(), cache_.end(), comp);
	return *(cacheFront_ = cache_.begin());
}

Var ClaspBerkmin::getTopMoms(const Solver& s) {
	// Pre: At least one unassigned var!
	for (; s.value( front_ ) != value_free; ++front_) {;}
	Var var   = front_;
	uint32 ms = momsScore(s, var);
	uint32 ls = 0;
	for (Var v = var+1; v <= s.numProblemVars(); ++v) {
		if (s.value(v) == value_free && (ls = momsScore(s, v)) > ms) {
			var = v;
			ms  = ls;
		}
	}
	if (++numVsids_ >= BERK_MAX_MOMS_DECS || ms < 2) {
		// Scores are not relevant or too many moms-based decisions
		// - disable MOMS
		hasActivities(true);
	}
	return var;
}

void ClaspBerkmin::startInit(const Solver& s) {
	if (order_.score.empty()) {
		rng_.srand(s.rng.seed());
	}
	order_.score.resize(s.numVars()+1);
	initHuang(order_.huang);

	cache_.clear();
	cacheSize_ = 5;
	cacheFront_ = cache_.end();

	freeLits_.clear();
	freeOtherLits_.clear();
	topConflict_ = topOther_ = (uint32)-1;

	front_    = 1;
	numVsids_ = 0;
}

void ClaspBerkmin::endInit(Solver& s) {
	if (initHuang()) {
		const bool clearScore = types_.inSet(Constraint_t::Static);
		// initialize preferred values of vars
		cache_.clear();
		for (Var v = 1; v <= s.numVars(); ++v) {
			order_.decayedScore(v);
			if (order_.occ(v) != 0 && s.pref(v).get(ValueSet::saved_value) == value_free) {
				s.setPref(v, ValueSet::saved_value, order_.occ(v) > 0 ? value_true : value_false);
			}
			if   (clearScore) order_.score[v] = HScore(order_.decay);
			else cache_.push_back(v);
		}
		initHuang(false);
	}
	if (!types_.inSet(Constraint_t::Static) || s.numFreeVars() > BERK_MAX_MOMS_VARS) {
		hasActivities(true);
	}
	std::stable_sort(cache_.begin(), cache_.end(), Order::Compare(&order_));
	cacheFront_ = cache_.begin();
}

void ClaspBerkmin::updateVar(const Solver& s, Var v, uint32 n) {
	if (s.validVar(v)) { growVecTo(order_.score, v+n); }
	front_ = 1;
	cache_.clear();
	cacheFront_ = cache_.end();
}

void ClaspBerkmin::newConstraint(const Solver& s, const Literal* first, LitVec::size_type size, ConstraintType t) {
	if (t == Constraint_t::Conflict) { hasActivities(true); }
	if ((t == Constraint_t::Conflict && order_.resScore == HeuParams::score_min)
	 || (t == Constraint_t::Static   && order_.huang)) {
		for (const Literal* x = first, *end = first+size; x != end; ++x) {
			order_.inc(*x, s.varInfo(x->var()).nant());
		}
	}
	if (t != Constraint_t::Static && !order_.huang) {
		for (const Literal* x = first, *end = first+size; x != end; ++x) { order_.incOcc(*x); }
	}
}

void ClaspBerkmin::updateReason(const Solver& s, const LitVec& lits, Literal r) {
	if (order_.resScore > HeuParams::score_min) {
		const bool ms = order_.resScore == HeuParams::score_multi_set;
		for (LitVec::size_type i = 0, e = lits.size(); i != e; ++i) {
			if (ms || !s.seen(lits[i])) { order_.inc(~lits[i], s.varInfo(lits[i].var()).nant()); }
		}
	}
	if ((order_.resScore & 1u) != 0 && !isSentinel(r)) {
		order_.inc(r, s.varInfo(r.var()).nant());
	}
}

bool ClaspBerkmin::bump(const Solver& s, const WeightLitVec& lits, double adj) {
	for (WeightLitVec::const_iterator it = lits.begin(), end = lits.end(); it != end; ++it) {
		if (!order_.nant || s.varInfo(it->first.var()).nant()) {
			uint32 xf = order_.decayedScore(it->first.var()) + static_cast<weight_t>(it->second*adj);
			order_.score[it->first.var()].act = (uint16)std::min(xf, UINT32_MAX>>16);
		}
	}
	return true;
}

void ClaspBerkmin::undoUntil(const Solver&, LitVec::size_type) {
	topConflict_ = topOther_ = static_cast<uint32>(-1);
	front_ = 1;
	cache_.clear();
	cacheFront_ = cache_.end();
	if (cacheSize_ > 5 && numVsids_ > 0 && (numVsids_*3) < cacheSize_) {
		cacheSize_ = static_cast<uint32>(cacheSize_/BERK_CACHE_GROW);
	}
	numVsids_ = 0;
}

bool ClaspBerkmin::hasTopUnsat(Solver& s) {
	topConflict_  = std::min(s.numLearntConstraints(), topConflict_);
	topOther_     = std::min(s.numLearntConstraints(), topOther_);
	assert(topConflict_ <= topOther_);
	freeOtherLits_.clear();
	freeLits_.clear();
	TypeSet ts = types_;
	if (ts.m > 1) {
		while (topOther_ > topConflict_) {
			if (s.getLearnt(topOther_-1).isOpen(s, ts, freeLits_) != 0) {
				freeLits_.swap(freeOtherLits_);
				ts.m = 0;
				break;
			}
			--topOther_;
			freeLits_.clear();
		}
	}
	ts.addSet(Constraint_t::Conflict);
	uint32 stopAt = topConflict_ < maxBerkmin_ ? 0 : topConflict_ - maxBerkmin_;
	while (topConflict_ != stopAt) {
		uint32 x = s.getLearnt(topConflict_-1).isOpen(s, ts, freeLits_);
		if (x != 0) {
			if (x == Constraint_t::Conflict) { break; }
			topOther_  = topConflict_;
			freeLits_.swap(freeOtherLits_);
			ts.m = 0;
			ts.addSet(Constraint_t::Conflict);
		}
		--topConflict_;
		freeLits_.clear();
	}
	if (freeOtherLits_.empty())  topOther_ = topConflict_;
	if (freeLits_.empty())      freeOtherLits_.swap(freeLits_);
	return !freeLits_.empty();
}

Literal ClaspBerkmin::doSelect(Solver& s) {
	const uint32 decayMask = order_.huang ? 127 : 511;
	if ( ((s.stats.choices + 1)&decayMask) == 0 ) {
		if ((order_.decay += (1+!order_.huang)) == BERK_MAX_DECAY) {
			order_.resetDecay();
		}
	}
	if (hasTopUnsat(s)) {        // Berkmin based decision
		assert(!freeLits_.empty());
		Literal x = selectRange(s, &freeLits_[0], &freeLits_[0]+freeLits_.size());
		return selectLiteral(s, x.var(), false);
	}
	else if (hasActivities()) {  // Vsids based decision
		return selectLiteral(s, getMostActiveFreeVar(s), true);
	}
	else {                       // Moms based decision
		return selectLiteral(s, getTopMoms(s), true);
	}
}

Literal ClaspBerkmin::selectRange(Solver& s, const Literal* first, const Literal* last) {
	Literal candidates[BERK_NUM_CANDIDATES];
	candidates[0] = *first;
	uint32 c = 1;
	uint32  ms  = static_cast<uint32>(-1);
	uint32  ls  = 0;
	for (++first; first != last; ++first) {
		Var v = first->var();
		assert(s.value(v) == value_free);
		int cmp = order_.compare(v, candidates[0].var());
		if (cmp > 0) {
			candidates[0] = *first;
			c = 1;
			ms  = static_cast<uint32>(-1);
		}
		else if (cmp == 0) {
			if (ms == static_cast<uint32>(-1)) ms = momsScore(s, candidates[0].var());
			if ( (ls = momsScore(s,v)) > ms) {
				candidates[0] = *first;
				c = 1;
				ms  = ls;
			}
			else if (ls == ms && c != BERK_NUM_CANDIDATES) {
				candidates[c++] = *first;
			}
		}
	}
	return c == 1 ? candidates[0] : candidates[rng_.irand(c)];
}

Literal ClaspBerkmin::selectLiteral(Solver& s, Var v, bool vsids) const {
	ValueSet pref = s.pref(v);
	int signScore = order_.occ(v);
	if (order_.huang && std::abs(signScore) > 32 && !pref.has(ValueSet::user_value)) {
		return Literal(v, signScore < 0);
	}
	// compute expensive sign score only if necessary
	if (vsids && !pref.has(ValueSet::user_value | ValueSet::pref_value | ValueSet::saved_value)) {
		int32 w0 = (int32)s.estimateBCP(posLit(v), 5);
		int32 w1 = (int32)s.estimateBCP(negLit(v), 5);
		if (w1 != 1 || w0 != w1) { signScore = w0 - w1; }
	}
	return DecisionHeuristic::selectLiteral(s, v, signScore);
}

void ClaspBerkmin::Order::resetDecay() {
	for (Scores::size_type i = 1, end = score.size(); i < end; ++i) {
		decayedScore(i);
		score[i].dec = 0;
	}
	decay = 0;
}
/////////////////////////////////////////////////////////////////////////////////////////
// ClaspVmtf selection strategy
/////////////////////////////////////////////////////////////////////////////////////////
ClaspVmtf::ClaspVmtf(const HeuParams& params) : decay_(0) {
	ClaspVmtf::setConfig(params);
}

void ClaspVmtf::setConfig(const HeuParams& params) {
	nMove_  = params.param ? std::max(params.param, uint32(2)) : 8u;
	scType_ = params.score != HeuParams::score_auto ? params.score : static_cast<uint32>(HeuParams::score_min);
	nant_   = params.nant != 0;
	addOther(types_ = TypeSet(), params.other != HeuParams::other_auto ? params.other : static_cast<uint32>(HeuParams::other_no));
	if (params.moms) { types_.addSet(Constraint_t::Static); }
	if (scType_ == HeuParams::score_min) { types_.addSet(Constraint_t::Conflict); }
}

void ClaspVmtf::startInit(const Solver& s) {
	score_.resize(s.numVars()+1, VarInfo(vars_.end()));
}

void ClaspVmtf::endInit(Solver& s) {
	bool moms = types_.inSet(Constraint_t::Static);
	for (Var v = 1; v <= s.numVars(); ++v) {
		if (s.value(v) == value_free && score_[v].pos_ == vars_.end()) {
			score_[v].activity(decay_);
			if (moms) {
				score_[v].activity_ = momsScore(s, v);
				score_[v].decay_    = decay_+1;
			}
			score_[v].pos_ = vars_.insert(vars_.end(), v);
		}
	}
	if (moms) {
		vars_.sort(LessLevel(s, score_));
		for (VarList::iterator it = vars_.begin(), end = vars_.end(); it != end; ++it) {
			if (score_[*it].decay_ != decay_) {
				score_[*it].activity_ = 0;
				score_[*it].decay_    = decay_;
			}
		}
	}
	front_ = vars_.begin();
}

void ClaspVmtf::updateVar(const Solver& s, Var v, uint32 n) {
	if (s.validVar(v)) {
		growVecTo(score_, v+n, VarInfo(vars_.end()));
		for (uint32 end = v+n; v != end; ++v) {
			if (score_[v].pos_ == vars_.end()) { score_[v].pos_ = vars_.insert(vars_.end(), v); }
			else                               { front_ = vars_.begin(); }
		}
	}
	else if (v < score_.size()) {
		if ((v + n) > score_.size()) { n = score_.size() - v; }
		for (uint32 x = v + n; x-- != v; ) {
			if (score_[x].pos_ != vars_.end()) {
				vars_.erase(score_[x].pos_);
				score_[x].pos_ = vars_.end();
			}
		}
	}
}

void ClaspVmtf::simplify(const Solver& s, LitVec::size_type i) {
	for (; i < s.numAssignedVars(); ++i) {
		if (score_[s.trail()[i].var()].pos_ != vars_.end()) {
			vars_.erase(score_[s.trail()[i].var()].pos_);
			score_[s.trail()[i].var()].pos_ = vars_.end();
		}
	}
	front_ = vars_.begin();
}

void ClaspVmtf::newConstraint(const Solver& s, const Literal* first, LitVec::size_type size, ConstraintType t) {
	if (t != Constraint_t::Static) {
		LessLevel comp(s, score_);
		const bool upAct = types_.inSet(t);
		const VarVec::size_type mtf = t == Constraint_t::Conflict ? nMove_ : (nMove_*uint32(upAct))/2;
		for (LitVec::size_type i = 0; i < size; ++i, ++first) {
			Var v = first->var();
			score_[v].occ_ += 1 - (((int)first->sign())<<1);
			if (upAct) { ++score_[v].activity(decay_); }
			if (mtf && (!nant_ || s.varInfo(v).nant())){
				if (mtf_.size() < mtf) {
					mtf_.push_back(v);
					std::push_heap(mtf_.begin(), mtf_.end(), comp);
				}
				else if (comp(v, mtf_[0])) {
					assert(s.level(v) <= s.level(mtf_[0]));
					std::pop_heap(mtf_.begin(), mtf_.end(), comp);
					mtf_.back() = v;
					std::push_heap(mtf_.begin(), mtf_.end(), comp);
				}
			}
		}
		for (VarVec::size_type i = 0; i != mtf_.size(); ++i) {
			Var v = mtf_[i];
			if (score_[v].pos_ != vars_.end()) {
				vars_.splice(vars_.begin(), vars_, score_[v].pos_);
			}
		}
		mtf_.clear();
		front_ = vars_.begin();
	}
}

void ClaspVmtf::undoUntil(const Solver&, LitVec::size_type) {
	front_ = vars_.begin();
}

void ClaspVmtf::updateReason(const Solver& s, const LitVec& lits, Literal r) {
	if (scType_ > HeuParams::score_min) {
		const bool  ms = scType_ == HeuParams::score_multi_set;
		const uint32 D = decay_;
		for (LitVec::size_type i = 0, e = lits.size(); i != e; ++i) {
			if (ms || !s.seen(lits[i])) { ++score_[lits[i].var()].activity(D); }
		}
	}
	if (scType_ & 1u) { ++score_[r.var()].activity(decay_); }
}

bool ClaspVmtf::bump(const Solver&, const WeightLitVec& lits, double adj) {
	for (WeightLitVec::const_iterator it = lits.begin(), end = lits.end(); it != end; ++it) {
		score_[it->first.var()].activity(decay_) += static_cast<uint32>(it->second*adj);
	}
	return true;
}

Literal ClaspVmtf::doSelect(Solver& s) {
	decay_ += ((s.stats.choices + 1) & 511) == 0;
	for (; s.value(*front_) != value_free; ++front_) {;}
	Literal c;
	if (s.numFreeVars() > 1) {
		VarList::iterator v2 = front_;
		uint32 distance = 0;
		do {
			++v2;
			++distance;
		} while (s.value(*v2) != value_free);
		c = (score_[*front_].activity(decay_) + (distance<<1)+ 3) > score_[*v2].activity(decay_)
		    ? selectLiteral(s, *front_, score_[*front_].occ_)
		    : selectLiteral(s, *v2, score_[*v2].occ_);
	}
	else {
		c = selectLiteral(s, *front_, score_[*front_].occ_);
	}
	return c;
}

Literal ClaspVmtf::selectRange(Solver&, const Literal* first, const Literal* last) {
	Literal best = *first;
	for (++first; first != last; ++first) {
		if (score_[first->var()].activity(decay_) > score_[best.var()].activity(decay_)) {
			best = *first;
		}
	}
	return best;
}

/////////////////////////////////////////////////////////////////////////////////////////
// ClaspVsids selection strategy
/////////////////////////////////////////////////////////////////////////////////////////
static double initDecay(uint32 p) {
	double m = static_cast<double>(p ? p : 0.95);
	while (m > 1.0) { m /= 10.0; }
	return m;
}

template <class ScoreType>
ClaspVsids_t<ScoreType>::ClaspVsids_t(const HeuParams& params)
	: vars_(CmpScore(score_))
	, inc_(1.0)
	, acids_(false) {
	ClaspVsids_t<ScoreType>::setConfig(params);
}

template <class ScoreType>
void ClaspVsids_t<ScoreType>::setConfig(const HeuParams& params) {
	addOther(types_ = TypeSet(), params.other != HeuParams::other_auto ? params.other : static_cast<uint32>(HeuParams::other_no));
	scType_ = params.score != HeuParams::score_auto ? params.score : static_cast<uint32>(HeuParams::score_min);
	decay_  = Decay(params.decay.init ? initDecay(params.decay.init) : 0.0, initDecay(params.param), params.decay.bump, params.decay.freq);
	acids_  = params.acids != 0;
	nant_   = params.nant != 0;
	if (params.moms) { types_.addSet(Constraint_t::Static); }
	if (scType_ == HeuParams::score_min) { types_.addSet(Constraint_t::Conflict); }
}

template <class ScoreType>
void ClaspVsids_t<ScoreType>::startInit(const Solver& s) {
	score_.resize(s.numVars()+1);
	occ_.resize(s.numVars()+1);
	vars_.reserve(s.numVars() + 1);
}

template <class ScoreType>
void ClaspVsids_t<ScoreType>::endInit(Solver& s) {
	vars_.clear();
	initScores(s, types_.inSet(Constraint_t::Static));
	double mx = 0;
	for (Var v = 1; v <= s.numVars(); ++v) {
		if (s.value(v) != value_free) { continue; }
		mx = std::max(mx, score_[v].get());
		if (!vars_.is_in_queue(v)) { vars_.push(v); }
	}
	if (acids_ && mx > inc_) {
		inc_ = std::ceil(mx);
	}
}
template <class ScoreType>
void ClaspVsids_t<ScoreType>::updateVarActivity(const Solver& s, Var v, double f) {
	if (!nant_ || s.varInfo(v).nant()) {
		double o = score_[v].get(), n;
		f = ScoreType::applyFactor(score_, v, f);
		if      (!acids_)  { n = o + (f * inc_); }
		else if (f == 1.0) { n = (o + inc_) / 2.0; }
		else if (f != 0.0) { n = std::max( (o + inc_ + f) / 2.0, f + o ); }
		else               { return; }
		score_[v].set(n);
		if (n > 1e100) { normalize(); }
		if (vars_.is_in_queue(v)) {
			if (n >= o) { vars_.increase(v); }
			else        { vars_.decrease(v); }
		}
	}
}
template <class ScoreType>
void ClaspVsids_t<ScoreType>::updateVar(const Solver& s, Var v, uint32 n) {
	if (s.validVar(v)) {
		growVecTo(score_, v+n);
		growVecTo(occ_, v+n);
		for (uint32 end = v+n; v != end; ++v) { vars_.update(v); }
	}
	else {
		for (uint32 end = v+n; v != end; ++v) { vars_.remove(v); }
	}
}
template <class ScoreType>
void ClaspVsids_t<ScoreType>::initScores(Solver& s, bool moms) {
	if (!moms) { return; }
	double maxS = 0.0, ms;
	for (Var v = 1; v <= s.numVars(); ++v) {
		if (s.value(v) == value_free && score_[v].get() == 0.0 && (ms = (double)momsScore(s, v)) != 0.0) {
			maxS = std::max(maxS, ms);
			score_[v].set(-ms);
		}
	}
	for (Var v = 1; v <= s.numVars(); ++v) {
		double d = score_[v].get();
		if (d < 0) {
			d *= -1.0;
			d /= maxS;
			score_[v].set(d);
		}
	}
}
template <class ScoreType>
void ClaspVsids_t<ScoreType>::simplify(const Solver& s, LitVec::size_type i) {
	for (; i < s.numAssignedVars(); ++i) {
		vars_.remove(s.trail()[i].var());
	}
}

template <class ScoreType>
void ClaspVsids_t<ScoreType>::normalize() {
	const double min  = std::numeric_limits<double>::min();
	const double minD = min * 1e100;
	inc_             *= 1e-100;
	for (LitVec::size_type i = 0; i != score_.size(); ++i) {
		double d = score_[i].get();
		if (d > 0) {
			// keep relative ordering but
			// actively avoid denormals
			d += minD;
			d *= 1e-100;
		}
		score_[i].set(d);
	}
}
template <class ScoreType>
void ClaspVsids_t<ScoreType>::newConstraint(const Solver& s, const Literal* first, LitVec::size_type size, ConstraintType t) {
	if (t != Constraint_t::Static) {
		const bool upAct = types_.inSet(t);
		for (LitVec::size_type i = 0; i < size; ++i, ++first) {
			incOcc(*first);
			if (upAct) {
				updateVarActivity(s, first->var());
			}
		}
		if (t == Constraint_t::Conflict) {
			if (decay_.next && --decay_.next == 0 && decay_.lo < decay_.hi) {
				decay_.lo  += decay_.bump / 100.0;
				decay_.next = decay_.freq;
				decay_.df   = 1.0 / decay_.lo;
			}
			if (!acids_) { inc_ *= decay_.df; }
			else         { inc_ += 1.0; }
		}
	}
}
template <class ScoreType>
void ClaspVsids_t<ScoreType>::updateReason(const Solver& s, const LitVec& lits, Literal r) {
	if (scType_ > HeuParams::score_min) {
		const bool ms = scType_ == HeuParams::score_multi_set;
		for (LitVec::size_type i = 0, end = lits.size(); i != end; ++i) {
			if (ms || !s.seen(lits[i])) { updateVarActivity(s, lits[i].var()); }
		}
	}
	if ((scType_ & 1u) != 0 && r.var() != 0) { updateVarActivity(s, r.var()); }
}
template <class ScoreType>
bool ClaspVsids_t<ScoreType>::bump(const Solver& s, const WeightLitVec& lits, double adj) {
	double mf = 1.0, f;
	for (WeightLitVec::const_iterator it = lits.begin(), end = lits.end(); it != end; ++it) {
		updateVarActivity(s, it->first.var(), (f = it->second*adj));
		if (acids_ && f > mf) { mf = f; }
	}
	if (acids_ && mf > 1.0) {
		inc_ = std::ceil(mf + inc_);
	}
	return true;
}
template <class ScoreType>
void ClaspVsids_t<ScoreType>::undoUntil(const Solver& s , LitVec::size_type st) {
	const LitVec& a = s.trail();
	for (; st < a.size(); ++st) {
		if (!vars_.is_in_queue(a[st].var())) {
			vars_.push(a[st].var());
		}
	}
}
template <class ScoreType>
Literal ClaspVsids_t<ScoreType>::doSelect(Solver& s) {
	while ( s.value(vars_.top()) != value_free ) {
		vars_.pop();
	}
	return selectLiteral(s, vars_.top(), occ(vars_.top()));
}
template <class ScoreType>
Literal ClaspVsids_t<ScoreType>::selectRange(Solver&, const Literal* first, const Literal* last) {
	Literal best = *first;
	for (++first; first != last; ++first) {
		if (vars_.key_compare()(first->var(), best.var())) {
			best = *first;
		}
	}
	return best;
}
template class ClaspVsids_t<VsidsScore>;

/////////////////////////////////////////////////////////////////////////////////////////
// DomainHeuristic selection strategy
/////////////////////////////////////////////////////////////////////////////////////////
DomainHeuristic::DomainHeuristic(const HeuParams& params) : ClaspVsids_t<DomScore>(params), domSeen_(0), defMax_(0), defMod_(0), defPref_(0) {
	frames_.push_back(Frame(0,DomAction::UNDO_NIL));
	setDefaultMod(static_cast<HeuParams::DomMod>(params.domMod), params.domPref);
}
DomainHeuristic::~DomainHeuristic() {}

void DomainHeuristic::setConfig(const HeuParams& params) {
	ClaspVsids_t<DomScore>::setConfig(params);
	setDefaultMod(static_cast<HeuParams::DomMod>(params.domMod), params.domPref);
}
void DomainHeuristic::setDefaultMod(HeuParams::DomMod mod, uint32 prefSet) {
	defMod_ = (uint16)mod;
	defPref_= prefSet;
}
void DomainHeuristic::detach(Solver& s) {
	if (!actions_.empty()) {
		for (DomainTable::iterator it = s.sharedContext()->heuristic.begin(), end = s.sharedContext()->heuristic.end(); it != end; ++it) {
			if (it->hasCondition()) {
				s.removeWatch(it->cond(), this);
			}
		}
	}
	while (frames_.back().dl != 0) {
		s.removeUndoWatch(frames_.back().dl, this);
		frames_.pop_back();
	}
	Var end = std::min(static_cast<Var>(score_.size()), static_cast<Var>(s.numVars() + 1));
	for (Var v = 0; v != end; ++v) {
		if (score_[v].sign) { s.setPref(v, ValueSet::user_value, value_free); }
	}
	actions_.clear();
	prios_.clear();
	domSeen_ = 0;
	defMax_  = 0;
}
void DomainHeuristic::startInit(const Solver& s) {
	BaseType::startInit(s);
	domSeen_ = std::min(domSeen_, s.sharedContext()->heuristic.size());
}
void DomainHeuristic::initScores(Solver& s, bool moms) {
	const DomainTable& domTab = s.sharedContext()->heuristic;
	BaseType::initScores(s, moms);
	uint32 nKey = (uint32)prios_.size();
	if (defMax_) {
		defMax_ = std::min(defMax_, s.numVars()) + 1;
		for (Var v = 1; v != defMax_; ++v) {
			if (score_[v].domP >= nKey) {
				bool sign = score_[v].sign;
				score_[v] = DomScore(score_[v].value);
				if (sign) { s.setPref(v, ValueSet::user_value, value_free); }
			}
		}
		defMax_ = 0;
	}
	if (domSeen_ < domTab.size()) {
		// apply new domain modifications
		VarScoreVec saved;
		Literal lastW = lit_true();
		uint32 dKey = nKey;
		for (DomainTable::iterator it = domTab.begin() + domSeen_, end = domTab.end(); it != end; ++it) {
			if (s.topValue(it->var()) != value_free || s.isFalse(it->cond())) {
				continue;
			}
			if (score_[it->var()].domP >= nKey){
				score_[it->var()].setDom(nKey++);
				prios_.push_back(DomPrio());
				prios_.back().clear();
			}
			uint32 k = addDomAction(*it, s, saved, lastW);
			if (k > dKey) { dKey = k; }
		}
		for (VarScoreVec::value_type x; !saved.empty(); saved.pop_back()) {
			x = saved.back();
			score_[x.first].value += x.second;
			score_[x.first].init   = 0;
		}
		if (!actions_.empty()) {
			actions_.back().next = 0;
		}
		if ((nKey - dKey) > dKey && s.sharedContext()->solveMode() == SharedContext::solve_once) {
			PrioVec(prios_.begin(), prios_.begin()+dKey).swap(prios_);
		}
		domSeen_ = domTab.size();
	}
	// apply default modification
	if (defMod_) {
		struct DefAction : public DomainTable::DefaultAction {
			DomainHeuristic* self; Solver* solver; uint32 key;
			DefAction(DomainHeuristic& h, Solver& s, uint32 k) : self(&h), solver(&s), key(k) { }
			virtual void atom(Literal p, HeuParams::DomPref s, uint32 b) {
				self->addDefAction(*solver, p, b ? b : 1, key + log2(s));
			}
		} act(*this, s, nKey + 1);
		DomainTable::applyDefault(*s.sharedContext(), act, defPref_);
	}
}

uint32 DomainHeuristic::addDomAction(const DomMod& e, Solver& s, VarScoreVec& initOut, Literal& lastW) {
	if (e.comp()) {
		DomMod level(e.var(), DomModType::Level, e.bias(), e.prio(), e.cond());
		DomMod sign(e.var(), DomModType::Sign, e.type() == DomModType::True ? 1 : -1, e.prio(), e.cond());
		uint32 k1 = addDomAction(level, s, initOut, lastW);
		uint32 k2 = addDomAction(sign, s, initOut, lastW);
		return std::max(k1, k2);
	}
	bool isStatic = !e.hasCondition() || s.topValue(e.cond().var()) == trueValue(e.cond());
	uint16& sPrio = prio(e.var(), e.type());
	if (e.prio() < sPrio || (!isStatic && e.type() == DomModType::Init)) {
		return 0;
	}
	if (e.type() == DomModType::Init && score_[e.var()].init == 0) {
		initOut.push_back(std::make_pair(e.var(), score_[e.var()].value));
		score_[e.var()].init = 1;
	}
	DomAction a = { e.var(), (uint32)e.type(), DomAction::UNDO_NIL, uint32(0), e.bias(), e.prio() };
	if (a.mod == DomModType::Sign && a.bias != 0) {
		a.bias = a.bias > 0 ? value_true : value_false;
	}
	POTASSCO_ASSERT(e.type() == a.mod, "Invalid dom modifier!");
	if (isStatic) {
		applyAction(s, a, sPrio);
		score_[e.var()].sign |= static_cast<uint32>(e.type() == DomModType::Sign);
		return 0;
	}
	if (e.cond() != lastW) {
		s.addWatch(lastW = e.cond(), this, (uint32)actions_.size());
	}
	else {
		actions_.back().next = 1;
	}
	actions_.push_back(a);
	return score_[e.var()].domP + 1;
}

void DomainHeuristic::addDefAction(Solver& s, Literal x, int16 lev, uint32 domKey) {
	if (s.value(x.var()) != value_free || score_[x.var()].domP < domKey) { return; }
	const bool signMod = defMod_ <  HeuParams::mod_init && ((defMod_ & HeuParams::mod_init) != 0);
	const bool valMod  = defMod_ >= HeuParams::mod_init || ((defMod_ & HeuParams::mod_level)!= 0);
	if (score_[x.var()].domP > domKey && lev && valMod) {
		if      (defMod_ < HeuParams::mod_init)    { score_[x.var()].level  += lev; }
		else if (defMod_ == HeuParams::mod_init)   { score_[x.var()].value  += (lev*100); }
		else if (defMod_ == HeuParams::mod_factor) { score_[x.var()].factor += 1 + (lev > 3) + (lev > 15); }
	}
	if (signMod) {
		ValueRep oPref = s.pref(x.var()).get(ValueSet::user_value);
		ValueRep nPref = (defMod_ & HeuParams::mod_spos) != 0 ? trueValue(x) : falseValue(x);
		if (oPref == value_free || (score_[x.var()].sign == 1 && domKey != score_[x.var()].domP)) {
			s.setPref(x.var(), ValueSet::user_value, nPref);
			score_[x.var()].sign = 1;
		}
		else if (score_[x.var()].sign == 1 && oPref != nPref) {
			s.setPref(x.var(), ValueSet::user_value, value_free);
			score_[x.var()].sign = 0;
		}
	}
	if (x.var() > defMax_) {
		defMax_ = x.var();
	}
	score_[x.var()].setDom(domKey);
}


Literal DomainHeuristic::doSelect(Solver& s) {
	Literal x = BaseType::doSelect(s);
	s.stats.addDomChoice(score_[x.var()].isDom());
	return x;
}

Constraint::PropResult DomainHeuristic::propagate(Solver& s, Literal, uint32& aId) {
	uint32 n = aId;
	uint32 D = s.decisionLevel();
	do {
		DomAction& a = actions_[n];
		uint16& prio = this->prio(a.var, a.mod);
		if (s.value(a.var) == value_free && a.prio >= prio) {
			applyAction(s, a, prio);
			if (D != frames_.back().dl) {
				s.addUndoWatch(D, this);
				frames_.push_back(Frame(D, DomAction::UNDO_NIL));
			}
			pushUndo(frames_.back().head, n);
		}
	} while (actions_[n++].next);
	return PropResult(true, true);
}

void DomainHeuristic::applyAction(Solver& s, DomAction& a, uint16& gPrio) {
	std::swap(gPrio, a.prio);
	switch (a.mod) {
		default: assert(false); break;
		case DomModType::Init:
			score_[a.var].value = a.bias;
			break;
		case DomModType::Factor: std::swap(score_[a.var].factor, a.bias); break;
		case DomModType::Level:
			std::swap(score_[a.var].level, a.bias);
			if (vars_.is_in_queue(a.var)) { vars_.update(a.var); }
			break;
		case DomModType::Sign:
			int16 old = s.pref(a.var).get(ValueSet::user_value);
			s.setPref(a.var, ValueSet::user_value, ValueRep(a.bias));
			a.bias    = old;
			break;
	}
}
void DomainHeuristic::pushUndo(uint32& head, uint32 actionId) {
	actions_[actionId].undo = head;
	head = actionId;
}

void DomainHeuristic::undoLevel(Solver& s) {
	while (frames_.back().dl >= s.decisionLevel()) {
		for (uint32 n = frames_.back().head; n != DomAction::UNDO_NIL;) {
			DomAction& a = actions_[n];
			n            = a.undo;
			applyAction(s, a, prio(a.var, a.mod));
		}
		frames_.pop_back();
	}
}
template class ClaspVsids_t<DomScore>;
}
