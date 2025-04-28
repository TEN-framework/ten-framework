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
#include <clasp/clause.h>
#include <clasp/solver.h>
#include <clasp/util/misc_types.h>
#include <algorithm>

namespace Clasp { namespace Detail {
struct GreaterLevel {
	GreaterLevel(const Solver& s) : solver_(s) {}
	bool operator()(const Literal& p1, const Literal& p2) const {
		assert(solver_.value(p1.var()) != value_free && solver_.value(p2.var()) != value_free);
		return solver_.level(p1.var()) > solver_.level(p2.var());
	}
private:
	GreaterLevel& operator=(const GreaterLevel&);
	const Solver& solver_;
};

struct Sink {
	explicit Sink(SharedLiterals* c) : clause(c) {}
	~Sink() { if (clause) clause->release(); }
	SharedLiterals* clause;
};

void* alloc(uint32 size) {
	POTASSCO_PRAGMA_TODO("replace with CACHE_LINE_ALIGNED alloc")
	return ::operator new(size);
}
void free(void* mem) {
	::operator delete(mem);
}

} // namespace Detail

/////////////////////////////////////////////////////////////////////////////////////////
// SharedLiterals
/////////////////////////////////////////////////////////////////////////////////////////
SharedLiterals* SharedLiterals::newShareable(const Literal* lits, uint32 size, ConstraintType t, uint32 numRefs) {
	void* m = Detail::alloc(sizeof(SharedLiterals)+(size*sizeof(Literal)));
	return new (m) SharedLiterals(lits, size, t, numRefs);
}

SharedLiterals::SharedLiterals(const Literal* a_lits, uint32 size, ConstraintType t, uint32 refs)
	: size_type_( (size << 2) + t ) {
	refCount_ = std::max(uint32(1),refs);
	std::memcpy(lits_, a_lits, size*sizeof(Literal));
}

uint32 SharedLiterals::simplify(Solver& s) {
	bool   removeFalse = unique();
	uint32   newSize   = 0;
	Literal* r         = lits_;
	Literal* e         = lits_+size();
	ValueRep v;
	for (Literal* c = r; r != e; ++r) {
		if ( (v = s.value(r->var())) == value_free ) {
			if (c != r) *c = *r;
			++c; ++newSize;
		}
		else if (v == trueValue(*r)) {
			newSize = 0; break;
		}
		else if (!removeFalse) ++c;
	}
	if (removeFalse && newSize != size()) {
		size_type_ = (newSize << 2) | (size_type_ & uint32(3));
	}
	return newSize;
}

void SharedLiterals::release(uint32 n) {
	if ((refCount_ -= n) == 0) {
		this->~SharedLiterals();
		Detail::free(this);
	}
}
SharedLiterals* SharedLiterals::share() {
	++refCount_;
	return this;
}
/////////////////////////////////////////////////////////////////////////////////////////
// ClauseCreator
/////////////////////////////////////////////////////////////////////////////////////////
ClauseCreator::ClauseCreator(Solver* s)
	: solver_(s)
	, flags_(0){
}

ClauseCreator& ClauseCreator::start(ConstraintType t) {
	assert(solver_ && (solver_->decisionLevel() == 0 || t != Constraint_t::Static));
	literals_.clear();
	extra_ = ConstraintInfo(t);
	return *this;
}

uint32 ClauseCreator::watchOrder(const Solver& s, Literal p) {
	ValueRep value_p = s.value(p.var());
	// DL+1,  if isFree(p)
	// DL(p), if isFalse(p)
	// ~DL(p),if isTrue(p)
	uint32   abstr_p = value_p == value_free ? s.decisionLevel()+1 : s.level(p.var()) ^ -(value_p==trueValue(p));
	assert(abstr_p > 0 || (s.isFalse(p) && s.level(p.var()) == 0));
	return abstr_p;
}

ClauseRep ClauseCreator::prepare(Solver& s, const Literal* in, uint32 inSize, const ConstraintInfo& e, uint32 flags, Literal* out, uint32 outMax) {
	assert(out && outMax > 2);
	ClauseRep ret  = ClauseRep::prepared(out, 0, e);
	uint32 abst_w1 = 0, abst_w2 = 0;
	bool simplify  = ((flags & clause_force_simplify) != 0) && inSize > 2 && outMax >= inSize;
	Literal tag    = ~s.tagLiteral();
	Var     vMax   = s.numProblemVars() > s.numVars() && inSize ? std::max_element(in, in + inSize)->var() : 0;
	s.acquireProblemVar(vMax);
	for (uint32 i = 0, j = 0, MAX_OUT = outMax - 1; i != inSize; ++i) {
		Literal p     = in[i];
		uint32 abst_p = watchOrder(s, p);
		if ((abst_p + 1) > 1 && (!simplify || !s.seen(p.var()))) {
			out[j] = p;
			if (p == tag)         { ret.info.setTagged(true); }
			if (p.var() > vMax)   { vMax = p.var();}
			if (simplify)         { s.markSeen(p); }
			if (abst_p > abst_w1) { std::swap(abst_p, abst_w1); std::swap(out[0], out[j]); }
			if (abst_p > abst_w2) { std::swap(abst_p, abst_w2); std::swap(out[1], out[j]); }
			if (j != MAX_OUT)     { ++j;  }
			++ret.size;
		}
		else if (abst_p == UINT32_MAX || (simplify && abst_p && s.seen(~p))) {
			abst_w1 = UINT32_MAX;
			break;
		}
	}
	if (simplify) {
		for (uint32 x = 0, end = ret.size; x != end; ++x) { s.clearSeen(out[x].var()); }
	}
	if (abst_w1 == UINT32_MAX || (abst_w2 && out[0].var() == out[1].var())) {
		out[0]   = abst_w1 == UINT32_MAX || out[0] == ~out[1] ? lit_true() : out[0];
		ret.size = 1;
	}
	ret.info.setAux(s.auxVar(vMax));
	return ret;
}


ClauseRep ClauseCreator::prepare(Solver& s, LitVec& lits, uint32 flags, const ConstraintInfo& info) {
	if (lits.empty()) { lits.push_back(lit_false()); }
	if ((flags & clause_no_prepare) == 0 || (flags & clause_force_simplify) != 0) {
		ClauseRep x = prepare(s, &lits[0], (uint32)lits.size(), info, flags, &lits[0]);
		shrinkVecTo(lits, x.size);
		return x;
	}
	return ClauseRep::prepared(&lits[0], (uint32)lits.size(), info);
}

ClauseRep ClauseCreator::prepare(bool forceSimplify) {
	return prepare(*solver_, literals_, forceSimplify ? clause_force_simplify : 0, extra_);
}

ClauseCreator::Status ClauseCreator::status(const Solver& s, const Literal* clause_begin, const Literal* clause_end) {
	if (clause_end <= clause_begin) { return status_empty; }
	Literal temp[3];
	ClauseRep x = prepare(const_cast<Solver&>(s), clause_begin, uint32(clause_end - clause_begin), ConstraintInfo(), 0, temp, 3);
	return status(s, x);
}

ClauseCreator::Status ClauseCreator::status(const Solver& s, const ClauseRep& c) {
	if (!c.prep)
		return status(s, c.lits, c.lits + c.size);

	uint32 dl = s.decisionLevel();
	uint32 fw = c.size     ? watchOrder(s, c.lits[0]) : 0;
	if (fw == UINT32_MAX) { return status_subsumed; }
	uint32 sw = c.size > 1 ? watchOrder(s, c.lits[1]) : 0;
	uint32 st = status_open;
	if      (fw > varMax)   { st|= status_sat; fw = ~fw; }
	else if (fw <= dl)      { st|= (fw ? status_unsat : status_empty); }
	if (sw <= dl && fw > sw){ st|= status_unit;  }
	return static_cast<Status>(st);
}

bool ClauseCreator::ignoreClause(const Solver& s, const ClauseRep& c, Status st, uint32 flags) {
	uint32 x = (st & (status_sat|status_unsat));
	if (x == status_open)  { return false; }
	if (x == status_unsat) { return st != status_empty && (flags & clause_not_conflict) != 0; }
	return st == status_subsumed || (st == status_sat && ( (flags & clause_not_sat) != 0 || ((flags & clause_not_root_sat) != 0 && s.level(c.lits[0].var()) <= s.rootLevel())));
}

ClauseCreator::Result ClauseCreator::end(uint32 flags) {
	assert(solver_);
	flags |= flags_;
	return ClauseCreator::create_prepared(*solver_, prepare(*solver_, literals_, flags, extra_), flags);
}

ClauseHead* ClauseCreator::newProblemClause(Solver& s, const ClauseRep& clause, uint32 flags) {
	ClauseHead* ret;
	Solver::WatchInitMode wMode = s.watchInitMode();
	if      (flags&clause_watch_first){ wMode = SolverStrategies::watch_first;}
	else if (flags&clause_watch_rand) { wMode = SolverStrategies::watch_rand; }
	else if (flags&clause_watch_least){ wMode = SolverStrategies::watch_least;}
	if (clause.size > 2 && wMode != SolverStrategies::watch_first) {
		uint32 fw = 0, sw = 1;
		if (wMode == SolverStrategies::watch_rand) {
			fw = s.rng.irand(clause.size);
			do { sw = s.rng.irand(clause.size); } while (sw == fw);
		}
		else if (wMode == SolverStrategies::watch_least) {
			uint32 cw1 = s.numWatches(~clause.lits[0]);
			uint32 cw2 = s.numWatches(~clause.lits[1]);
			if (cw1 > cw2) { std::swap(fw, sw); std::swap(cw1, cw2); }
			for (uint32 i = 2; i != clause.size && cw2; ++i) {
				uint32 p   = i;
				uint32 cwp = s.numWatches(~clause.lits[i]);
				if (cwp < cw1) { std::swap(cwp, cw1); std::swap(fw, p); }
				if (cwp < cw2) { std::swap(cwp, cw2); std::swap(sw, p); }
			}
		}
		std::swap(clause.lits[0], clause.lits[fw]);
		std::swap(clause.lits[1], clause.lits[sw]);
	}
	if (clause.size <= Clause::MAX_SHORT_LEN || !s.sharedContext()->physicalShareProblem()) {
		ret = Clause::newClause(s, clause);
	}
	else {
		ret = Clause::newShared(s, SharedLiterals::newShareable(clause.lits, clause.size, clause.info.type(), 1), clause.info, clause.lits, false);
	}
	if ( (flags & clause_no_add) == 0 ) {
		assert(!clause.info.aux());
		s.add(ret);
	}
	return ret;
}

ClauseHead* ClauseCreator::newLearntClause(Solver& s, const ClauseRep& clause, uint32 flags) {
	ClauseHead* ret;
	Detail::Sink sharedPtr(0);
	sharedPtr.clause = s.distribute(clause.lits, clause.size, clause.info);
	if (clause.size <= Clause::MAX_SHORT_LEN || sharedPtr.clause == 0) {
		if (!s.isFalse(clause.lits[1]) || clause.size < s.compressLimit()) {
			ret = Clause::newClause(s, clause);
		}
		else {
			ret = Clause::newContractedClause(s, clause, 2, true);
		}
	}
	else {
		ret              = Clause::newShared(s, sharedPtr.clause, clause.info, clause.lits, false);
		sharedPtr.clause = 0;
	}
	if ( (flags & clause_no_add) == 0 ) {
		s.addLearnt(ret, clause.size, clause.info.type());
	}
	return ret;
}

ClauseHead* ClauseCreator::newUnshared(Solver& s, SharedLiterals* clause, const Literal* w, const ConstraintInfo& e) {
	LitVec temp; temp.reserve(clause->size());
	temp.assign(w, w+2);
	for (const Literal* x = clause->begin(), *end = clause->end(); x != end; ++x) {
		if (watchOrder(s, *x) > 0 && *x != temp[0] && *x != temp[1]) {
			temp.push_back(*x);
		}
	}
	return Clause::newClause(s, ClauseRep::prepared(&temp[0], (uint32)temp.size(), e));
}

ClauseCreator::Result ClauseCreator::create_prepared(Solver& s, const ClauseRep& clause, uint32 flags) {
	assert(s.decisionLevel() == 0 || (clause.info.learnt() && clause.prep));
	Status x = status(s, clause);
	if (ignoreClause(s, clause, x, flags)){
		return Result(0, x);
	}
	if (clause.size > 1) {
		Result ret(0, x);
		if (!clause.info.learnt() && s.satPrepro() && !s.sharedContext()->frozen()) {
			return Result(0, s.satPrepro()->addClause(clause.lits, clause.size) ? x : status_unsat);
		}
		if ((flags & clause_no_heuristic) == 0) { s.heuristic()->newConstraint(s, clause.lits, clause.size, clause.info.type()); }
		if (clause.size > 3 || (flags&clause_explicit) != 0 || !s.allowImplicit(clause)) {
			ret.local = clause.info.learnt() ? newLearntClause(s, clause, flags) : newProblemClause(s, clause, flags);
		}
		else {
			// add implicit short rep
			s.add(clause);
		}
		if ((x & (status_unit|status_unsat)) != 0) {
			Antecedent ante(ret.local);
			if (!ret.local){ ante = clause.size == 3 ? Antecedent(~clause.lits[1], ~clause.lits[2]) : Antecedent(~clause.lits[1]); }
			ret.status = s.force(clause.lits[0], s.level(clause.lits[1].var()), ante) ? status_unit : status_unsat;
		}
		return ret;
	}
	s.add(clause);
	return Result(0, !s.hasConflict() ? status_unit : status_unsat);
}

ClauseCreator::Result ClauseCreator::create(Solver& s, LitVec& lits, uint32 flags, const ConstraintInfo& extra) {
	return create_prepared(s, prepare(s, lits, flags, extra), flags);
}

ClauseCreator::Result ClauseCreator::create(Solver& s, const ClauseRep& rep, uint32 flags) {
	return create_prepared(s, (rep.prep == 0 && (flags & clause_no_prepare) == 0
		? prepare(s, rep.lits, rep.size, rep.info, flags, rep.lits)
		: ClauseRep::prepared(rep.lits, rep.size, rep.info)), flags);
}

ClauseCreator::Result ClauseCreator::integrate(Solver& s, SharedLiterals* clause, uint32 modeFlags, ConstraintType t) {
	assert(!s.hasConflict() && "ClauseCreator::integrate() - precondition violated!");
	Detail::Sink shared( (modeFlags & clause_no_release) == 0 ? clause : 0);
	// determine state of clause
	Literal temp[Clause::MAX_SHORT_LEN]; temp[0] = temp[1] = lit_false();
	ClauseRep x    = prepare(s, clause->begin(), clause->size(), ConstraintInfo(t), 0, temp, Clause::MAX_SHORT_LEN);
	uint32 impSize = (modeFlags & clause_explicit) != 0 || !s.allowImplicit(x) ? 1 : 3;
	Status xs      = status(s, x);
	if (ignoreClause(s, x, xs, modeFlags)) {
		return Result(0, xs);
	}
	Result result(0, xs);
	if ((modeFlags & clause_no_heuristic) == 0) { s.heuristic()->newConstraint(s, clause->begin(), clause->size(), t); }
	if (x.size > Clause::MAX_SHORT_LEN && s.sharedContext()->physicalShare(t)) {
		result.local  = Clause::newShared(s, clause, x.info, temp, shared.clause == 0);
		shared.clause = 0;
	}
	else if (x.size > impSize) {
		result.local  = x.size <= Clause::MAX_SHORT_LEN ? Clause::newClause(s, x) : newUnshared(s, clause, temp, x.info);
	}
	else {
		// unary clause or implicitly shared via binary/ternary implication graph;
		// only check for implication/conflict but do not create
		// a local representation for the clause
		s.stats.addLearnt(x.size, x.info.type());
		modeFlags |= clause_no_add;
	}
	if ((modeFlags & clause_no_add) == 0) { s.addLearnt(result.local, x.size, x.info.type()); }
	if ((xs & (status_unit|status_unsat)) != 0) {
		Antecedent ante = result.local ? Antecedent(result.local) : Antecedent(~temp[1], ~temp[2]);
		uint32 impLevel = s.level(temp[1].var());
		result.status   = s.force(temp[0], impLevel, ante) ? status_unit : status_unsat;
		if (result.local && (modeFlags & clause_int_lbd) != 0) {
			uint32 lbd = s.countLevels(clause->begin(), clause->end());
			result.local->resetScore(makeScore(x.info.activity(), lbd));
		}
	}
	return result;
}
ClauseCreator::Result ClauseCreator::integrate(Solver& s, SharedLiterals* clause, uint32 modeFlags) {
	return integrate(s, clause, modeFlags, clause->type());
}
/////////////////////////////////////////////////////////////////////////////////////////
// Clause
/////////////////////////////////////////////////////////////////////////////////////////
void* Clause::alloc(Solver& s, uint32 lits, bool learnt) {
	if (lits <= Clause::MAX_SHORT_LEN) {
		if (learnt) { s.addLearntBytes(32); }
		return s.allocSmall();
	}
	uint32 extra = std::max((uint32)ClauseHead::HEAD_LITS, lits) - ClauseHead::HEAD_LITS;
	uint32 bytes = sizeof(Clause) + (extra)*sizeof(Literal);
	if (learnt) { s.addLearntBytes(bytes); }
	return Detail::alloc(bytes);
}

ClauseHead* Clause::newClause(void* mem, Solver& s, const ClauseRep& rep) {
	assert(rep.size >= 2 && mem);
	return new (mem) Clause(s, rep);
}

ClauseHead* Clause::newShared(Solver& s, SharedLiterals* shared_lits, const InfoType& e, const Literal* lits, bool addRef) {
	return mt::SharedLitsClause::newClause(s, shared_lits, e, lits, addRef);
}

ClauseHead* Clause::newContractedClause(Solver& s, const ClauseRep& rep, uint32 tailStart, bool extend) {
	assert(rep.size >= 2);
	if (extend) { std::stable_sort(rep.lits+tailStart, rep.lits+rep.size, Detail::GreaterLevel(s)); }
	return new (alloc(s, rep.size, rep.info.learnt())) Clause(s, rep, tailStart, extend);
}
void ClauseHead::Local::init(uint32 sz) {
	std::memset(mem, 0, sizeof(mem));
	if (sz > ClauseHead::MAX_SHORT_LEN) { mem[0] = (sz << 3) + 1; }
	assert(isSmall() == (sz <= ClauseHead::MAX_SHORT_LEN));
}
Clause::Clause(Solver& s, const ClauseRep& rep, uint32 tail, bool extend)
	: ClauseHead(rep.info) {
	assert(tail >= rep.size || s.isFalse(rep.lits[tail]));
	local_.init(rep.size);
	if (!isSmall()) {
		// copy literals
		std::memcpy(head_, rep.lits, rep.size*sizeof(Literal));
		tail = std::max(tail, (uint32)ClauseHead::HEAD_LITS);
		if (tail < rep.size) {       // contracted clause
			head_[rep.size-1].flag();  // mark last literal of clause
			Literal t = head_[tail];
			if (s.level(t.var()) > 0) {
				local_.markContracted();
				if (extend) {
					s.addUndoWatch(s.level(t.var()), this);
				}
			}
			local_.setSize(tail);
		}
	}
	else {
		std::memcpy(head_, rep.lits, std::min(rep.size, (uint32)ClauseHead::HEAD_LITS)*sizeof(Literal));
		small()[0] = rep.size > ClauseHead::HEAD_LITS   ? rep.lits[ClauseHead::HEAD_LITS]   : lit_false();
		small()[1] = rep.size > ClauseHead::HEAD_LITS+1 ? rep.lits[ClauseHead::HEAD_LITS+1] : lit_false();
		assert(isSmall() && Clause::size() == rep.size);
	}
	attach(s);
}

Clause::Clause(Solver& s, const Clause& other) : ClauseHead(other.info_) {
	uint32 oSize = other.size();
	local_.init(oSize);
	if      (!isSmall())      { std::memcpy(head_, other.head_, oSize*sizeof(Literal)); }
	else if (other.isSmall()) { std::memcpy(&local_, &other.local_, (ClauseHead::MAX_SHORT_LEN+1)*sizeof(Literal)); }
	else { // this is small, other is not
		std::memcpy(head_, other.head_, ClauseHead::HEAD_LITS*sizeof(Literal));
		std::memcpy(&local_, other.head_+ClauseHead::HEAD_LITS, 2*sizeof(Literal));
	}
	attach(s);
}

Constraint* Clause::cloneAttach(Solver& other) {
	assert(!learnt());
	return new (alloc(other, Clause::size(), false)) Clause(other, *this);
}
Literal* Clause::small()          { return static_cast<Literal*>(static_cast<void*>(&local_)); }
bool Clause::contracted()   const { return local_.contracted(); }
bool Clause::isSmall()      const { return local_.isSmall(); }
bool Clause::strengthened() const { return local_.strengthened(); }
void Clause::destroy(Solver* s, bool detachFirst) {
	if (s) {
		if (detachFirst) { Clause::detach(*s); }
		if (learnt())    { s->freeLearntBytes(computeAllocSize()); }
	}
	void* mem   = static_cast<Constraint*>(this);
	bool  small = isSmall();
	this->~Clause();
	if (!small) { Detail::free(mem); }
	else if (s) { s->freeSmall(mem); }
}

void Clause::detach(Solver& s) {
	if (contracted()) {
		Literal* eoc = end();
		if (s.isFalse(*eoc) && s.level(eoc->var()) != 0) {
			s.removeUndoWatch(s.level(eoc->var()), this);
		}
	}
	ClauseHead::detach(s);
}

uint32 Clause::computeAllocSize() const {
	if (isSmall()) { return 32; }
	uint32 rt = sizeof(Clause) - (ClauseHead::HEAD_LITS*sizeof(Literal));
	uint32 sz = local_.size();
	uint32 nw = contracted() + strengthened();
	if (nw != 0u) {
		const Literal* eoc = head_ + sz;
		do { nw -= eoc++->flagged(); } while (nw);
		sz = static_cast<uint32>(eoc - head_);
	}
	return rt + (sz*sizeof(Literal));
}

bool Clause::updateWatch(Solver& s, uint32 pos) {
	Literal* it;
	if (!isSmall()) {
		for (Literal* begin = head_ + ClauseHead::HEAD_LITS, *end = this->end(), *first = begin + local_.mem[1];;) {
			for (it = first; it < end; ++it) {
				if (!s.isFalse(*it)) {
					std::swap(*it, head_[pos]);
					local_.mem[1] = static_cast<uint32>(++it - begin);
					return true;
				}
			}
			if (first == begin) { break; }
			end = first;
			first = begin;
		}
	}
	else if (!s.isFalse(*(it = this->small())) || !s.isFalse(*++it)) {
#if defined(__GNUC__) && __GNUC__ == 7 && __GNUC_MINOR__ < 2
		// Add compiler barrier to prevent gcc Bug 81365:
		// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=81365
		asm volatile("" ::: "memory");
#endif
		std::swap(head_[pos], *it);
		return true;
	}
	return false;
}
Clause::LitRange Clause::tail() {
	if (!isSmall()) { return LitRange(head_+ClauseHead::HEAD_LITS, end()); }
	Literal* tBeg = small(), *tEnd = tBeg;
	if (*tEnd != lit_false()) ++tEnd;
	if (*tEnd != lit_false()) ++tEnd;
	return LitRange(tBeg, tEnd);
}
void Clause::reason(Solver& s, Literal p, LitVec& out) {
	out.push_back(~head_[p == head_[0]]);
	if (!isSentinel(head_[2])) {
		out.push_back(~head_[2]);
		LitRange t = tail();
		for (const Literal* r = t.first; r != t.second; ++r) {
			out.push_back(~*r);
		}
		if (contracted()) {
			const Literal* r = t.second;
			do { out.push_back(~*r); } while (!r++->flagged());
		}
	}
	if (learnt()) {
		s.updateOnReason(info_.score(), p, out);
	}
}

bool Clause::minimize(Solver& s, Literal p, CCMinRecursive* rec) {
	s.updateOnMinimize(info_.score());
	uint32 other = p == head_[0];
	if (!s.ccMinimize(~head_[other], rec) || !s.ccMinimize(~head_[2], rec)) { return false; }
	LitRange t = tail();
	for (const Literal* r = t.first; r != t.second; ++r) {
		if (!s.ccMinimize(~*r, rec)) { return false; }
	}
	if (contracted()) {
		do {
			if (!s.ccMinimize(~*t.second, rec)) { return false; }
		} while (!t.second++->flagged());
	}
	return true;
}

bool Clause::isReverseReason(const Solver& s, Literal p, uint32 maxL, uint32 maxN) {
	uint32 other   = p == head_[0];
	if (!isRevLit(s, head_[other], maxL) || !isRevLit(s, head_[2], maxL)) { return false; }
	uint32 notSeen = !s.seen(head_[other].var()) + !s.seen(head_[2].var());
	LitRange t     = tail();
	for (const Literal* r = t.first; r != t.second && notSeen <= maxN; ++r) {
		if (!isRevLit(s, *r, maxL)) { return false; }
		notSeen += !s.seen(r->var());
	}
	if (contracted()) {
		const Literal* r = t.second;
		do { notSeen += !s.seen(r->var()); } while (notSeen <= maxN && !r++->flagged());
	}
	return notSeen <= maxN;
}

bool Clause::simplify(Solver& s, bool reinit) {
	assert(s.decisionLevel() == 0 && s.queueSize() == 0);
	if (ClauseHead::satisfied(s)) {
		detach(s);
		return true;
	}
	LitRange t = tail();
	Literal* it= t.first - !isSmall(), *j;
	// skip free literals
	while (it != t.second && s.value(it->var()) == value_free) { ++it; }
	// copy remaining free literals
	for (j = it; it != t.second; ++it) {
		if      (s.value(it->var()) == value_free) { *j++ = *it; }
		else if (s.isTrue(*it)) { Clause::detach(s); return true;}
	}
	// replace any false lits with sentinels
	for (Literal* r = j; r != t.second; ++r) { *r = lit_false(); }
	if (!isSmall()) {
		uint32 size = std::max((uint32)ClauseHead::HEAD_LITS, static_cast<uint32>(j-head_));
		local_.setSize(size);
		local_.clearIdx();
		if (j != t.second && learnt() && !strengthened()) {
			// mark last literal so that we can recompute alloc size later
			t.second[-1].flag();
			local_.markStrengthened();
		}
		if (reinit && size > 3) {
			detach(s);
			std::random_shuffle(head_, j, s.rng);
			attach(s);
		}
	}
	else if (s.isFalse(head_[2])) {
		head_[2]   = t.first[0];
		t.first[0] = t.first[1];
		t.first[1] = lit_false();
		--j;
	}
	return j <= t.first && ClauseHead::toImplication(s);
}

uint32 Clause::isOpen(const Solver& s, const TypeSet& x, LitVec& freeLits) {
	if (!x.inSet(ClauseHead::type()) || ClauseHead::satisfied(s)) {
		return 0;
	}
	assert(s.queueSize() == 0 && "Watches might be false!");
	freeLits.push_back(head_[0]);
	freeLits.push_back(head_[1]);
	if (!s.isFalse(head_[2])) freeLits.push_back(head_[2]);
	ValueRep v;
	LitRange t = tail();
	for (Literal* r = t.first; r != t.second; ++r) {
		if ( (v = s.value(r->var())) == value_free) {
			freeLits.push_back(*r);
		}
		else if (v == trueValue(*r)) {
			std::swap(head_[2], *r);
			return 0;
		}
	}
	return ClauseHead::type();
}

void Clause::undoLevel(Solver& s) {
	assert(!isSmall());
	uint32   t = local_.size();
	uint32  ul = s.jumpLevel();
	Literal* r = head_+t;
	while (!r->flagged() && (s.value(r->var()) == value_free || s.level(r->var()) > ul)) {
		++t;
		++r;
	}
	if (r->flagged() || s.level(r->var()) == 0) {
		r->unflag();
		t += !isSentinel(*r);
		local_.clearContracted();
	}
	else {
		s.addUndoWatch(s.level(r->var()), this);
	}
	local_.setSize(t);
}

void Clause::toLits(LitVec& out) const {
	out.insert(out.end(), head_, (head_+ClauseHead::HEAD_LITS)-isSentinel(head_[2]));
	LitRange t = const_cast<Clause&>(*this).tail();
	if (contracted()) { while (!t.second++->flagged()) {;} }
	out.insert(out.end(), t.first, t.second);
}

ClauseHead::BoolPair Clause::strengthen(Solver& s, Literal p, bool toShort) {
	LitRange t  = tail();
	Literal* eoh= head_+ClauseHead::HEAD_LITS;
	Literal* eot= t.second;
	Literal* it = std::find(head_, eoh, p);
	BoolPair ret(false, false);
	if (it != eoh) {
		if (it != head_+2) { // update watch
			*it = head_[2];
			s.removeWatch(~p, this);
			Literal* best = it, *n;
			for (n = t.first; n != eot && s.isFalse(*best); ++n) {
				if (!s.isFalse(*n) || s.level(n->var()) > s.level(best->var())) {
					best = n;
				}
			}
			std::swap(*it, *best);
			s.addWatch(~*it, ClauseWatch(this));
			it = head_+2;
		}
		// replace cache literal with literal from tail
		if ((*it  = *t.first) != lit_false()) {
			eot     = removeFromTail(s, t.first, eot);
		}
		ret.first = true;
	}
	else if ((it = std::find(t.first, eot, p)) != eot) {
		eot       = removeFromTail(s, it, eot);
		ret.first = true;
	}
	else if (contracted()) {
		for (; *it != p && !it->flagged(); ++it) { ; }
		ret.first = *it == p;
		eot       = *it == p ? removeFromTail(s, it, eot) : it + 1;
	}
	if (ret.first && ~p == s.tagLiteral()) {
		clearTagged();
	}
	ret.second = toShort && eot == t.first && toImplication(s);
	return ret;
}

Literal* Clause::removeFromTail(Solver& s, Literal* it, Literal* end) {
	assert(it != end || contracted());
	if (!contracted()) {
		*it  = *--end;
		*end = lit_false();
		if (!isSmall()) {
			local_.setSize(local_.size()-1);
			local_.clearIdx();
		}
	}
	else {
		uint32 uLev  = s.level(end->var());
		Literal* j   = it;
		while ( !j->flagged() ) { *j++ = *++it; }
		*j           = lit_false();
		uint32 nLev  = s.level(end->var());
		if (uLev != nLev && s.removeUndoWatch(uLev, this) && nLev != 0) {
			s.addUndoWatch(nLev, this);
		}
		if (j != end) { (j-1)->flag(); }
		else          { local_.clearContracted(); }
		end = j;
	}
	if (learnt() && !isSmall() && !strengthened()) {
		end->flag();
		local_.markStrengthened();
	}
	return end;
}
uint32 Clause::size() const {
	LitRange t = const_cast<Clause&>(*this).tail();
	return !isSentinel(head_[2])
		? 3u + static_cast<uint32>(t.second - t.first)
		: 2u;
}
/////////////////////////////////////////////////////////////////////////////////////////
// mt::SharedLitsClause
/////////////////////////////////////////////////////////////////////////////////////////
namespace mt {
ClauseHead* SharedLitsClause::newClause(Solver& s, SharedLiterals* shared_lits, const InfoType& e, const Literal* lits, bool addRef) {
	return new (s.allocSmall()) SharedLitsClause(s, shared_lits, lits, e, addRef);
}

SharedLitsClause::SharedLitsClause(Solver& s, SharedLiterals* shared_lits, const Literal* w, const InfoType& e, bool addRef)
	: ClauseHead(e) {
	static_assert(sizeof(SharedLitsClause) <= 32, "Unsupported Padding");
	shared_ = addRef ? shared_lits->share() : shared_lits;
	std::memcpy(head_, w, std::min((uint32)ClauseHead::HEAD_LITS, shared_lits->size())*sizeof(Literal));
	attach(s);
	if (learnt()) { s.addLearntBytes(32); }
}

Constraint* SharedLitsClause::cloneAttach(Solver& other) {
	return SharedLitsClause::newClause(other, shared_, InfoType(this->type()), head_);
}

bool SharedLitsClause::updateWatch(Solver& s, uint32 pos) {
	Literal  other = head_[1^pos];
	for (const Literal* r = shared_->begin(), *end = shared_->end(); r != end; ++r) {
		// at this point we know that head_[2] is false so we only need to check
		// that we do not watch the other watched literal twice!
		if (!s.isFalse(*r) && *r != other) {
			head_[pos] = *r; // replace watch
			// try to replace cache literal
			switch( std::min(static_cast<uint32>(8), static_cast<uint32>(end-r)) ) {
				case 8: if (!s.isFalse(*++r) && *r != other) { head_[2] = *r; return true; } // FALLTHRU
				case 7: if (!s.isFalse(*++r) && *r != other) { head_[2] = *r; return true; } // FALLTHRU
				case 6: if (!s.isFalse(*++r) && *r != other) { head_[2] = *r; return true; } // FALLTHRU
				case 5: if (!s.isFalse(*++r) && *r != other) { head_[2] = *r; return true; } // FALLTHRU
				case 4: if (!s.isFalse(*++r) && *r != other) { head_[2] = *r; return true; } // FALLTHRU
				case 3: if (!s.isFalse(*++r) && *r != other) { head_[2] = *r; return true; } // FALLTHRU
				case 2: if (!s.isFalse(*++r) && *r != other) { head_[2] = *r; return true; } // FALLTHRU
				default: return true;
			}
		}
	}
	return false;
}

void SharedLitsClause::reason(Solver& s, Literal p, LitVec& out) {
	for (const Literal* r = shared_->begin(), *end = shared_->end(); r != end; ++r) {
		assert(s.isFalse(*r) || *r == p);
		if (*r != p) { out.push_back(~*r); }
	}
	if (learnt()) {
		s.updateOnReason(info_.score(), p, out);
	}
}

bool SharedLitsClause::minimize(Solver& s, Literal p, CCMinRecursive* rec) {
	s.updateOnMinimize(info_.score());
	for (const Literal* r = shared_->begin(), *end = shared_->end(); r != end; ++r) {
		if (*r != p && !s.ccMinimize(~*r, rec)) { return false; }
	}
	return true;
}

bool SharedLitsClause::isReverseReason(const Solver& s, Literal p, uint32 maxL, uint32 maxN) {
	uint32 notSeen = 0;
	for (const Literal* r = shared_->begin(), *end = shared_->end(); r != end; ++r) {
		if (*r == p) continue;
		if (!isRevLit(s, *r, maxL)) return false;
		if (!s.seen(r->var()) && ++notSeen > maxN) return false;
	}
	return true;
}

bool SharedLitsClause::simplify(Solver& s, bool reinit) {
	if (ClauseHead::satisfied(s)) {
		detach(s);
		return true;
	}
	uint32 optSize = shared_->simplify(s);
	if (optSize == 0) {
		detach(s);
		return true;
	}
	else if (optSize <= Clause::MAX_SHORT_LEN) {
		Literal lits[Clause::MAX_SHORT_LEN];
		Literal* j = lits;
		for (const Literal* r = shared_->begin(), *e = shared_->end(); r != e; ++r) {
			if (!s.isFalse(*r)) *j++ = *r;
		}
		// safe extra data
		InfoType myInfo = info_;
		// detach & destroy but do not release memory
		detach(s);
		SharedLitsClause::destroy(0, false);
		// construct short clause in "this"
		ClauseHead* h = Clause::newClause(this, s, ClauseRep::prepared(lits, static_cast<uint32>(j-lits), myInfo));
		return h->simplify(s, reinit);
	}
	else if (s.isFalse(head_[2])) {
		// try to replace cache lit with non-false literal
		for (const Literal* r = shared_->begin(), *e = shared_->end(); r != e; ++r) {
			if (!s.isFalse(*r) && std::find(head_, head_+2, *r) == head_+2) {
				head_[2] = *r;
				break;
			}
		}
	}
	return false;
}

void SharedLitsClause::destroy(Solver* s, bool detachFirst) {
	if (s) {
		if (detachFirst) { ClauseHead::detach(*s); }
		if (learnt())    { s->freeLearntBytes(32); }
	}
	shared_->release();
	void* mem = this;
	this->~SharedLitsClause();
	if (s) { s->freeSmall(mem); }
}

uint32 SharedLitsClause::isOpen(const Solver& s, const TypeSet& x, LitVec& freeLits) {
	if (!x.inSet(ClauseHead::type()) || ClauseHead::satisfied(s)) {
		return 0;
	}
	Literal* head = head_;
	ValueRep v;
	for (const Literal* r = shared_->begin(), *end = shared_->end(); r != end; ++r) {
		if ( (v = s.value(r->var())) == value_free ) {
			freeLits.push_back(*r);
		}
		else if (v == trueValue(*r)) {
			head[2] = *r; // remember as cache literal
			return 0;
		}
	}
	return ClauseHead::type();
}

void SharedLitsClause::toLits(LitVec& out) const {
	out.insert(out.end(), shared_->begin(), shared_->end());
}

ClauseHead::BoolPair SharedLitsClause::strengthen(Solver&, Literal, bool) {
	return BoolPair(false, false);
}

uint32 SharedLitsClause::size() const { return shared_->size(); }
} // end namespace mt

/////////////////////////////////////////////////////////////////////////////////////////
// LoopFormula
/////////////////////////////////////////////////////////////////////////////////////////
LoopFormula* LoopFormula::newLoopFormula(Solver& s, const ClauseRep& c1, const Literal* atoms, uint32 nAtoms, bool heu) {
	uint32 bytes = sizeof(LoopFormula) + (c1.size + nAtoms + 2) * sizeof(Literal);
	void* mem = Detail::alloc(bytes);
	s.addLearntBytes(bytes);
	return new (mem)LoopFormula(s, c1, atoms, nAtoms, heu);
}
LoopFormula::LoopFormula(Solver& s, const ClauseRep& c1, const Literal* atoms, uint32 nAtoms, bool heu) {
	act_ = c1.info.score();
	lits_[0] = lit_true(); // Starting sentinel
	std::memcpy(lits_ + 1, c1.lits, c1.size * sizeof(Literal));
	lits_[end_ = c1.size + 1] = lit_true(); // Ending sentinel
	s.addWatch(~lits_[2], this, (2 << 1) + 1);
	lits_[2].flag();
	size_  = c1.size + nAtoms + 2;
	str_   = 0;
	xPos_  = 1;
	other_ = 1;
	for (uint32 i = 0, x = end_ + 1; i != nAtoms; ++i, ++x) {
		act_.bumpActivity();
		s.addWatch(~(lits_[x] = atoms[i]), this, (1 << 1) + 1);
		if (heu) {
			lits_[1] = atoms[i];
			s.heuristic()->newConstraint(s, lits_ + 1, c1.size, Constraint_t::Loop);
		}
	}
	(lits_[1] = c1.lits[0]).flag();
}
void LoopFormula::destroy(Solver* s, bool detach) {
	if (s) {
		if (detach) { this->detach(*s); }
		if (str_)   { while (lits_[size_++].rep() != 3u) { ; } }
		s->freeLearntBytes(sizeof(LoopFormula) + (size_ * sizeof(Literal)));
	}
	void* mem = static_cast<Constraint*>(this);
	this->~LoopFormula();
	Detail::free(mem);
}
void LoopFormula::detach(Solver& s) {
	for (Literal* it = begin() + xPos_; !isSentinel(*it); ++it) {
		if (it->flagged()) { s.removeWatch(~*it, this); it->unflag(); }
	}
	for (Literal* it = xBegin(), *end = xEnd(); it != end; ++it) {
		s.removeWatch(~*it, this);
	}
}
bool LoopFormula::otherIsSat(const Solver& s) {
	if (other_ != xPos_)          { return s.isTrue(lits_[other_]); }
	if (!s.isTrue(lits_[other_])) { return false; }
	for (Literal* it = xBegin(), *end = xEnd(); it != end; ++it) {
		if (!s.isTrue(*it)) {
			if (lits_[xPos_].flagged()){ (lits_[xPos_] = *it).flag(); }
			else                       { lits_[xPos_] = *it; }
			return false;
		}
	}
	return true;
}
Constraint::PropResult LoopFormula::propagate(Solver& s, Literal p, uint32& data) {
	if (otherIsSat(s)) { // already satisfied?
		return PropResult(true, true);
	}
	uint32 idx = data >> 1;
	Literal* w = lits_ + idx;
	bool  head = idx == xPos_;
	if (head) { // p is one of the atoms - move to active part
		p = ~p;
		if (*w != p && s.isFalse(*w)) { return PropResult(true, true); }
		if (!w->flagged())            { *w = p; return PropResult(true, true); }
		(*w = p).flag();
	}
	for (int bounds = 0, dir = ((data & 1) << 1) - 1;;) {
		// search non-false literal - sentinels guarantee termination
		for (w += dir; s.isFalse(*w); w += dir) {;}
		if (!isSentinel(*w)) {
			uint32 nIdx = static_cast<uint32>(w - lits_);
			// other watched literal?
			if (w->flagged()) { other_ = nIdx; continue; }
			// replace watch
			lits_[idx].unflag();
			w->flag();
			// add new watch only w is not one of the atoms
			// and keep previous watch if p is one of the atoms
			if (nIdx != xPos_) { s.addWatch(~*w, this, (nIdx << 1) + (dir==1)); }
			return PropResult(true, head);
		}
		else if (++bounds == 1) {
			w     = lits_ + idx; // Halfway through, restart search, but
			dir  *= -1;          // this time walk in the opposite direction.
			data ^= 1;           // Save new direction of watch
		}
		else { // clause is unit
			bool ok = s.force(lits_[other_], this);
			if (other_ == xPos_ && ok) { // all lits in inactive part are implied
				for (Literal* it = xBegin(), *end = xEnd(); it != end && (ok = s.force(*it, this)) == true; ++it) { ; }
			}
			return PropResult(ok, true);
		}
	}
}
void LoopFormula::reason(Solver& s, Literal p, LitVec& lits) {
	// p = body: all literals in active clause
	// p = atom: only bodies
	for (Literal* it = begin() + (other_ == xPos_); !isSentinel(*it); ++it) {
		if (*it != p) { lits.push_back(~*it); }
	}
	s.updateOnReason(act_, p, lits);
}
bool LoopFormula::minimize(Solver& s, Literal p, CCMinRecursive* rec) {
	s.updateOnMinimize(act_);
	for (Literal* it = begin() + (other_ == xPos_); !isSentinel(*it); ++it) {
		if (*it != p && !s.ccMinimize(~*it, rec)) { return false; }
	}
	return true;
}
uint32 LoopFormula::size() const {
	return size_ - (2 + xPos_);
}
bool LoopFormula::locked(const Solver& s) const {
	if (other_ != xPos_ || !s.isTrue(lits_[other_])) {
		return s.isTrue(lits_[other_]) && s.reason(lits_[other_]) == this;
	}
	LoopFormula& self = const_cast<LoopFormula&>(*this);
	for (const Literal* it = self.xBegin(), *end = self.xEnd(); it != end; ++it) {
		if (s.isTrue(*it) && s.reason(*it) == this) { return true; }
	}
	return false;
}
uint32 LoopFormula::isOpen(const Solver& s, const TypeSet& xs, LitVec& freeLits) {
	if (!xs.inSet(Constraint_t::Loop) || otherIsSat(s)) {
		return 0;
	}
	for (Literal* it = begin() + xPos_; !isSentinel(*it); ++it) {
		if      (s.value(it->var()) == value_free) { freeLits.push_back(*it); }
		else if (s.isTrue(*it))                    { other_ = static_cast<uint32>(it-lits_); return 0; }
	}
	for (Literal* it = xBegin(), *end = xEnd(); it != end; ++it) {
		if (s.value(it->var()) == value_free) { freeLits.push_back(*it); }
	}
	return Constraint_t::Loop;
}
bool LoopFormula::simplify(Solver& s, bool) {
	if (otherIsSat(s) || (other_ != xPos_ && (other_ = xPos_) != 0 && otherIsSat(s))) {
		detach(s);
		return true;
	}
	Literal* it = begin(), *j, *end = xEnd();
	while (s.value(it->var()) == value_free) { ++it; }
	if (!isSentinel(*(j=it))) {
		// simplify active clause
		if (*it == lits_[xPos_]){ xPos_ = 0; }
		for (GenericWatch* w; !isSentinel(*it); ++it) {
			if (s.value(it->var()) == value_free) {
				if (it->flagged() && (w = s.getWatch(~*it, this)) != 0) {
					w->data = (static_cast<uint32>(j - lits_) << 1) + (w->data&1);
				}
				*j++ = *it;
			}
			else if (s.isTrue(*it)) { detach(s); return true; }
			else                    { assert(!it->flagged() && "Constraint not propagated!"); }
		}
		*j   = lit_true();
		end_ = static_cast<uint32>(j - lits_);
	}
	// simplify extra part
	for (++it, ++j; it != end; ++it) {
		if (s.value(it->var()) == value_free && xPos_) { *j++ = *it; }
		else { s.removeWatch(~*it, this); }
	}
	bool isClause = static_cast<uint32>(j - xBegin()) == 1;
	if (isClause) { --j; }
	if (j != end) { // size changed?
		if (!str_)   { (end-1)->rep() = 3u; str_ = 1u; }
		if (isClause){
			assert(xPos_ && *j == lits_[xPos_]);
			if (!lits_[xPos_].flagged()) { s.removeWatch(~*j, this); }
			xPos_ = 0;
		}
		size_ = static_cast<uint32>((end = j) - lits_);
	}
	assert(!isClause || xPos_ == 0);
	other_ = xPos_ + 1;
	ClauseRep act = ClauseRep::create(begin(), end_ - 1, Constraint_t::Loop);
	if (act.isImp() && s.allowImplicit(act)) {
		detach(s);
		ClauseCreator::Result res;
		for (it = xBegin(); it != end && res.ok() && !res.local; ++it) {
			lits_[xPos_] = *it;
			res = ClauseCreator::create(s, act, ClauseCreator::clause_no_add);
			POTASSCO_ASSERT(lits_[xPos_] == *it, "LOOP MUST NOT CONTAIN ASSIGNED VARS!");
		}
		if (!xPos_) { res = ClauseCreator::create(s, act, ClauseCreator::clause_no_add); }
		POTASSCO_ASSERT(res.ok() && !res.local, "LOOP MUST NOT CONTAIN AUX VARS!");
		return true;
	}
	return false;
}

}
