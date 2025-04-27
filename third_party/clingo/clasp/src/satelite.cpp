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
#include <clasp/clause.h>

namespace Clasp {
/////////////////////////////////////////////////////////////////////////////////////////
// SatElite preprocessing
//
/////////////////////////////////////////////////////////////////////////////////////////
SatElite::SatElite()
	: occurs_(0)
	, elimHeap_(LessOccCost(occurs_))
	, qFront_(0)
	, facts_(0) {
}

SatElite::~SatElite() {
	SatElite::doCleanUp();
}

void SatElite::reportProgress(Progress::EventOp id, uint32 curr, uint32 max) {
	ctx_->report(Progress(this, id, curr, max));
}

Clasp::SatPreprocessor* SatElite::clone() {
	SatElite* cp = new SatElite();
	return cp;
}

void SatElite::doCleanUp() {
	delete [] occurs_;  occurs_ = 0;
	ClauseList().swap(resCands_);
	VarVec().swap(occT_[pos]);
	VarVec().swap(occT_[neg]);
	LitVec().swap(resolvent_);
	VarVec().swap(queue_);
	elimHeap_.clear();
	qFront_ = facts_ = 0;
}

SatPreprocessor::Clause* SatElite::popSubQueue() {
	if (Clause* c = clause( queue_[qFront_++] )) {
		c->setInQ(false);
		return c;
	}
	return 0;
}

void SatElite::addToSubQueue(uint32 clauseId) {
	assert(clause(clauseId) != 0);
	if (!clause(clauseId)->inQ()) {
		queue_.push_back(clauseId);
		clause(clauseId)->setInQ(true);
	}
}

void SatElite::attach(uint32 clauseId, bool initialClause) {
	Clause& c = *clause(clauseId);
	c.abstraction() = 0;
	for (uint32 i = 0; i != c.size(); ++i) {
		Var v = c[i].var();
		occurs_[v].add(clauseId, c[i].sign());
		occurs_[v].unmark();
		c.abstraction() |= Clause::abstractLit(c[i]);
		if (elimHeap_.is_in_queue(v)) {
			elimHeap_.decrease(v);
		}
		else if (initialClause) {
			updateHeap(v);
		}
	}
	occurs_[c[0].var()].addWatch(clauseId);
	addToSubQueue(clauseId);
	stats.clAdded += !initialClause;
}

void SatElite::detach(uint32 id) {
	Clause& c = *clause(id);
	occurs_[c[0].var()].removeWatch(id);
	for (uint32 i = 0; i != c.size(); ++i) {
		Var v = c[i].var();
		occurs_[v].remove(id, c[i].sign(), false);
		updateHeap(v);
	}
	destroyClause(id);
}

void SatElite::bceVeRemove(uint32 id, bool freeId, Var ev, bool blocked) {
	Clause& c  = *clause(id);
	occurs_[c[0].var()].removeWatch(id);
	uint32 pos = 0;
	for (uint32 i = 0; i != c.size(); ++i) {
		Var v = c[i].var();
		if (v != ev) {
			occurs_[v].remove(id, c[i].sign(), freeId);
			updateHeap(v);
		}
		else {
			occurs_[ev].remove(id, c[i].sign(), false);
			pos = i;
		}
	}
	std::swap(c[0], c[pos]);
	c.setMarked(blocked);
	eliminateClause(id);
}

bool SatElite::initPreprocess(Options& opts) {
	reportProgress(Progress::event_algorithm, 0,100);
	opts_     = &opts;
	occurs_   = new OccurList[ctx_->numVars()+1];
	qFront_   = 0;
	occurs_[0].bce = (opts.type == Options::sat_pre_full);
	return true;
}
bool SatElite::doPreprocess() {
	// 1. add clauses to occur lists
	for (uint32 i  = 0, end = numClauses(); i != end; ++i) {
		attach(i, true);
	}
	// 2. remove subsumed clauses, eliminate vars by clause distribution
	timeout_ = opts_->limTime ? time(0) + opts_->limTime : std::numeric_limits<std::time_t>::max();
	for (uint32 i = 0, end = opts_->limIters ? opts_->limIters : UINT32_MAX; queue_.size()+elimHeap_.size() > 0; ++i) {
		if (!backwardSubsume())   { return false; }
		if (timeout() || i == end){ break;        }
		if (!eliminateVars())     { return false; }
	}
	reportProgress(Progress::event_algorithm, 100,100);
	return true;
}

// (Destructive) unit propagation on clauses.
// Removes satisfied clauses and shortens clauses w.r.t. false literals.
// Pre:   Assignment is propagated w.r.t other non-clause constraints
// Post:  Assignment is fully propagated and no clause contains an assigned literal
bool SatElite::propagateFacts() {
	Solver* s = ctx_->master();
	assert(s->queueSize() == 0);
	while (facts_ != s->numAssignedVars()) {
		Literal l     = s->trail()[facts_++];
		OccurList& ov = occurs_[l.var()];
		ClRange cls   = occurs_[l.var()].clauseRange();
		for (ClIter x = cls.first; x != cls.second; ++x) {
			if      (clause(x->var()) == 0)          { continue; }
			else if (x->sign() == l.sign())          { detach(x->var()); }
			else if (!strengthenClause(x->var(), ~l)){ return false; }
		}
		ov.clear();
		ov.mark(!l.sign());
	}
	assert(s->queueSize() == 0);
	return true;
}

// Backward subsumption and self-subsumption resolution until fixpoint
bool SatElite::backwardSubsume() {
	if (!propagateFacts()) return false;
	while (qFront_ != queue_.size()) {
		if ((qFront_ & 8191) == 0) {
			if (timeout()) break;
			if (queue_.size() > 1000) reportProgress(Progress::event_subsumption, qFront_, queue_.size());
		}
		if (peekSubQueue() == 0) { ++qFront_; continue; }
		Clause& c = *popSubQueue();
		// Try to minimize effort by testing against the var in c that occurs least often;
		Literal best = c[0];
		for (uint32 i = 1; i < c.size(); ++i) {
			if (occurs_[c[i].var()].numOcc() < occurs_[best.var()].numOcc()) {
				best  = c[i];
			}
		}
		// Test against all clauses containing best
		ClWList& cls = occurs_[best.var()].refs;
		Literal res  = lit_false();
		uint32  j    = 0;
		// must use index access because cls might change!
		for (uint32 i = 0, end = cls.left_size(); i != end; ++i) {
			Literal cl     = cls.left(i);
			uint32 otherId = cl.var();
			Clause* other  = clause(otherId);
			if (other && other!= &c && (res = subsumes(c, *other, best.sign()==cl.sign()?lit_true():best)) != lit_false()) {
				if (res == lit_true()) {
					// other is subsumed - remove it
					detach(otherId);
					other = 0;
				}
				else {
					// self-subsumption resolution; other is subsumed by c\{res} U {~res}
					// remove ~res from other, add it to subQ so that we can check if it now subsumes c
					res = ~res;
					occurs_[res.var()].remove(otherId, res.sign(), res.var() != best.var());
					updateHeap(res.var());
					if (!strengthenClause(otherId, res))              { return false; }
					if (res.var() == best.var() || !clause(otherId))  { other = 0; }
				}
			}
			if (other && j++ != i)  { cls.left(j-1) = cl; }
		}
		cls.shrink_left(cls.left_begin()+j);
		occurs_[best.var()].dirty = 0;
		assert(occurs_[best.var()].numOcc() == (uint32)cls.left_size());
		if (!propagateFacts()) return false;
	}
	queue_.clear();
	qFront_ = 0;
	return true;
}

// checks if 'c' subsumes 'other', and at the same time, if it can be used to
// simplify 'other' by subsumption resolution.
// Return:
//  - lit_false() - No subsumption or simplification
//  - lit_true() - 'c' subsumes 'other'
//  - l         - The literal l can be deleted from 'other'
Literal SatElite::subsumes(const Clause& c, const Clause& other, Literal res) const {
	if (other.size() < c.size() || (c.abstraction() & ~other.abstraction()) != 0) {
		return lit_false();
	}
	if (c.size() < 10 || other.size() < 10) {
		for (uint32 i = 0; i != c.size(); ++i) {
			for (uint32 j = 0; j != other.size(); ++j) {
				if (c[i].var() == other[j].var()) {
					if (c[i].sign() == other[j].sign())     { goto found; }
					else if (res != lit_true() && res!=c[i]) { return lit_false(); }
					res = c[i];
					goto found;
				}
			}
			return lit_false();
		found:;
		}
	}
	else {
		markAll(&other[0], other.size());
		for (uint32 i = 0; i != c.size(); ++i) {
			if (occurs_[c[i].var()].litMark == 0) { res = lit_false(); break; }
			if (occurs_[c[i].var()].marked(!c[i].sign())) {
				if (res != lit_true()&&res!=c[i]) { res = lit_false(); break; }
				res = c[i];
			}
		}
		unmarkAll(&other[0], other.size());
	}
	return res;
}

uint32 SatElite::findUnmarkedLit(const Clause& c, uint32 x) const {
	for (; x != c.size() && occurs_[c[x].var()].marked(c[x].sign()); ++x)
		;
	return x;
}

// checks if 'cl' is subsumed by one of the existing clauses and at the same time
// strengthens 'cl' if possible.
// Return:
//  - true  - 'cl' is subsumed
//  - false - 'cl' is not subsumed but may itself subsume other clauses
// Pre: All literals of l are marked, i.e.
// for each literal l in cl, occurs_[l.var()].marked(l.sign()) == true
bool SatElite::subsumed(LitVec& cl) {
	Literal l;
	uint32 x = 0;
	uint32 str = 0;
	LitVec::size_type j = 0;
	for (LitVec::size_type i = 0; i != cl.size(); ++i) {
		l = cl[i];
		if (occurs_[l.var()].litMark == 0) { --str; continue; }
		ClWList& cls   = occurs_[l.var()].refs; // right: all clauses watching either l or ~l
		WIter wj       = cls.right_begin();
		for (WIter w = wj, end = cls.right_end(); w != end; ++w) {
			Clause& c = *clause(*w);
			if (c[0] == l)  {
				if ( (x = findUnmarkedLit(c, 1)) == c.size() ) {
					while (w != end) { *wj++ = *w++; }
					cls.shrink_right( wj );
					return true;
				}
				c[0] = c[x];
				c[x] = l;
				occurs_[c[0].var()].addWatch(*w);
				if (occurs_[c[0].var()].litMark != 0 && findUnmarkedLit(c, x+1) == c.size()) {
					occurs_[c[0].var()].unmark();  // no longer part of cl
					++str;
				}
			}
			else if ( findUnmarkedLit(c, 1) == c.size() ) {
				occurs_[l.var()].unmark(); // no longer part of cl
				while (w != end) { *wj++ = *w++; }
				cls.shrink_right( wj );
				goto removeLit;
			}
			else { *wj++ = *w; }
		}
		cls.shrink_right(wj);
		if (j++ != i) { cl[j-1] = cl[i]; }
removeLit:;
	}
	cl.erase(cl.begin()+j, cl.end());
	if (str > 0) {
		for (LitVec::size_type i = 0; i != cl.size();) {
			if (occurs_[cl[i].var()].litMark == 0) {
				cl[i] = cl.back();
				cl.pop_back();
				if (--str == 0) break;
			}
			else { ++i; }
		}
	}
	return false;
}

// Pre: c contains l
// Pre: c was already removed from l's occur-list
bool SatElite::strengthenClause(uint32 clauseId, Literal l) {
	Clause& c = *clause(clauseId);
	if (c[0] == l) {
		occurs_[c[0].var()].removeWatch(clauseId);
		// Note: Clause::strengthen shifts literals after l to the left. Thus
		// c[1] will be c[0] after strengthen
		occurs_[c[1].var()].addWatch(clauseId);
	}
	++stats.litsRemoved;
	c.strengthen(l);
	if (c.size() == 1) {
		Literal unit = c[0];
		detach(clauseId);
		return ctx_->addUnary(unit) && ctx_->master()->propagate();
	}
	addToSubQueue(clauseId);
	return true;
}

// Split occurrences of v into pos and neg and
// mark all clauses containing v
SatElite::ClRange SatElite::splitOcc(Var v, bool mark) {
	ClRange cls      = occurs_[v].clauseRange();
	occurs_[v].dirty = 0;
	occT_[pos].clear(); occT_[neg].clear();
	ClIter j = cls.first;
	for (ClIter x = j; x != cls.second; ++x) {
		if (Clause* c = clause(x->var())) {
			assert(c->marked() == false);
			c->setMarked(mark);
			int sign = (int)x->sign();
			occT_[sign].push_back(x->var());
			if (j != x) *j = *x;
			++j;
		}
	}
	occurs_[v].refs.shrink_left(j);
	return occurs_[v].clauseRange();
}

void SatElite::markAll(const Literal* lits, uint32 size) const {
	for (uint32 i = 0; i != size; ++i) {
		occurs_[lits[i].var()].mark(lits[i].sign());
	}
}
void SatElite::unmarkAll(const Literal* lits, uint32 size) const {
	for (uint32 i = 0; i != size; ++i) {
		occurs_[lits[i].var()].unmark();
	}
}

// Run variable and/or blocked clause elimination on var v.
// If the number of non-trivial resolvents is <= maxCnt,
// v is eliminated by clause distribution. If bce is enabled,
// clauses blocked on a literal of v are removed.
bool SatElite::bceVe(Var v, uint32 maxCnt) {
	Solver* s = ctx_->master();
	if (s->value(v) != value_free) return true;
	assert(!ctx_->varInfo(v).frozen() && !ctx_->eliminated(v));
	resCands_.clear();
	// distribute clauses on v
	// check if number of clauses decreases if we'd eliminate v
	uint32 bce     = opts_->bce();
	ClRange cls    = splitOcc(v, bce > 1);
	uint32 cnt     = 0;
	uint32 markMax = ((uint32)occT_[neg].size() * (bce>1));
	uint32 blocked = 0;
	bool stop      = false;
	Clause* lhs, *rhs;
	for (VarVec::const_iterator i = occT_[pos].begin(); i != occT_[pos].end() && !stop; ++i) {
		lhs         = clause(*i);
		markAll(&(*lhs)[0], lhs->size());
		lhs->setMarked(bce != 0);
		for (VarVec::const_iterator j = occT_[neg].begin(); j != occT_[neg].end(); ++j) {
			if (!trivialResolvent(*(rhs = clause(*j)), v)) {
				markMax -= rhs->marked();
				rhs->setMarked(false); // not blocked on v
				lhs->setMarked(false); // not blocked on v
				if (++cnt <= maxCnt) {
					resCands_.push_back(lhs);
					resCands_.push_back(rhs);
				}
				else if (!markMax) {
					stop = (bce == 0);
					break;
				}
			}
		}
		unmarkAll(&(*lhs)[0], lhs->size());
		if (lhs->marked()) {
			occT_[pos][blocked++] = *i;
		}
	}
	if (cnt <= maxCnt) {
		// eliminate v by clause distribution
		ctx_->eliminate(v);  // mark var as eliminated
		// remove old clauses, store them in the elimination table so that
		// (partial) models can be extended.
		for (ClIter it = cls.first; it != cls.second; ++it) {
			// reuse first cnt ids for resolvents
			if (clause(it->var())) {
				bool freeId = (cnt && cnt--);
				bceVeRemove(it->var(), freeId, v, false);
			}
		}
		// add non trivial resolvents
		assert( resCands_.size() % 2 == 0 );
		ClIter it = cls.first;
		for (VarVec::size_type i = 0; i != resCands_.size(); i+=2, ++it) {
			if (!addResolvent(it->var(), *resCands_[i], *resCands_[i+1])) {
				return false;
			}
		}
		assert(occurs_[v].numOcc() == 0);
		// release memory
		occurs_[v].clear();
	}
	else if ( (blocked + markMax) > 0 ) {
		// remove blocked clauses
		for (uint32 i = 0; i != blocked; ++i) {
			bceVeRemove(occT_[pos][i], false, v, true);
		}
		for (VarVec::const_iterator it = occT_[neg].begin(); markMax; ++it) {
			if ( (rhs = clause(*it))->marked() ) {
				bceVeRemove(*it, false, v, true);
				--markMax;
			}
		}
	}
	return opts_->limIters != 0 || backwardSubsume();
}

bool SatElite::bce() {
	uint32 ops = 0;
	for (ClWList& bce= occurs_[0].refs; bce.right_size() != 0; ++ops) {
		Var v          = *(bce.right_end()-1);
		bce.pop_right();
		occurs_[v].bce=0;
		if ((ops & 1023) == 0)   {
			if (timeout())         { bce.clear(); return true; }
			if ((ops & 8191) == 0) { reportProgress(Progress::event_bce, ops, 1+bce.size()); }
		}
		if (!cutoff(v) && !bceVe(v, 0)) { return false; }
	}
	return true;
}

bool SatElite::eliminateVars() {
	Var     v          = 0;
	uint32  occ        = 0;
	if (!bce()) return false;
	for (uint32 ops = 0; !elimHeap_.empty(); ++ops) {
		v   = elimHeap_.top();  elimHeap_.pop();
		occ = occurs_[v].numOcc();
		if ((ops & 1023) == 0)   {
			if (timeout())         { elimHeap_.clear(); return true; }
			if ((ops & 8191) == 0) { reportProgress(Progress::event_var_elim, ops, 1+elimHeap_.size()); }
		}
		if (!cutoff(v) && !bceVe(v, occ)) {
			return false;
		}
	}
	return opts_->limIters != 0 || bce();
}

// returns true if the result of resolving c1 (implicitly given) and c2 on v yields a tautologous clause
bool SatElite::trivialResolvent(const Clause& c2, Var v) const {
	for (uint32 i = 0, end = c2.size(); i != end; ++i) {
		Literal x = c2[i];
		if (occurs_[x.var()].marked(!x.sign()) && x.var() != v) {
			return true;
		}
	}
	return false;
}

// Pre: lhs and rhs can be resolved on lhs[0].var()
// Pre: trivialResolvent(lhs, rhs, lhs[0].var()) == false
bool SatElite::addResolvent(uint32 id, const Clause& lhs, const Clause& rhs) {
	resolvent_.clear();
	Solver* s = ctx_->master();
	assert(lhs[0] == ~rhs[0]);
	uint32 i, end;
	Literal l;
	for (i = 1, end = lhs.size(); i != end; ++i) {
		l = lhs[i];
		if (!s->isFalse(l)) {
			if (s->isTrue(l)) goto unmark;
			occurs_[l.var()].mark(l.sign());
			resolvent_.push_back(l);
		}
	}
	for (i = 1, end = rhs.size(); i != end; ++i) {
		l = rhs[i];
		if (!s->isFalse(l) && !occurs_[l.var()].marked(l.sign())) {
			if (s->isTrue(l)) goto unmark;
			occurs_[l.var()].mark(l.sign());
			resolvent_.push_back(l);
		}
	}
	if (!subsumed(resolvent_))  {
		if (resolvent_.empty())   {
			return s->force(negLit(0));
		}
		if (resolvent_.size()==1) {
			occurs_[resolvent_[0].var()].unmark();
			return s->force(resolvent_[0]) && s->propagate() && propagateFacts();
		}
		setClause(id, resolvent_);
		attach(id, false);
		return true;
	}
unmark:
	if (!resolvent_.empty()) {
		unmarkAll(&resolvent_[0], resolvent_.size());
	}
	return true;
}

// extends the model given in assign by the vars that were eliminated
void SatElite::doExtendModel(ValueVec& m, LitVec& unconstr) {
	if (!elimTop_) return;
	const ValueRep value_eliminated = 4u;
	// compute values of eliminated vars / blocked literals by "unit propagating"
	// eliminated/blocked clauses in reverse order
	uint32 uv = 0;
	uint32 us = unconstr.size();
	Clause* r = elimTop_;
	do {
		Literal x  = (*r)[0];
		Var last   = x.var();
		bool check = true;
		if (!r->marked()) {
			// eliminated var - compute the implied value
			m[last] = value_eliminated;
		}
		if (uv != us && unconstr[uv].var() == last) {
			// last is unconstraint w.r.t the current model -
			// set remembered value
			check   = false;
			m[last] = trueValue(unconstr[uv]);
			++uv;
		}
		do {
			Clause& c = *r;
			if (m[x.var()] != trueValue(x) && check) {
				for (uint32 i = 1, end = c.size(); i != end; ++i) {
					ValueRep vi = m[c[i].var()] & 3u;
					if (vi != falseValue(c[i])) {
						x = c[i];
						break;
					}
				}
				if (x == c[0]) {
					// all lits != x are false
					// clause is unit or conflicting
					assert(c.marked() || m[x.var()] != falseValue(x));
					m[x.var()] = trueValue(x);
					check      = false;
				}
			}
			r = r->next();
		} while (r && (x = (*r)[0]).var() == last);
		if (m[last] == value_eliminated) {
			// last seems unconstraint w.r.t the model
			m[last] |= value_true;
			unconstr.push_back(posLit(last));
		}
	} while (r);
	// check whether newly added unconstraint vars are really unconstraint w.r.t the model
	// or if they are implied by some blocked clause.
	LitVec::iterator j = unconstr.begin()+us;
	for (LitVec::iterator it = j, end = unconstr.end(); it != end; ++it) {
		if ((m[it->var()] & value_eliminated) != 0) {
			// var is unconstraint - assign to true and remember it
			// so that we can later enumerate the model containing ~var
			m[it->var()] = value_true;
			*j++ = *it;
		}
	}
	unconstr.erase(j, unconstr.end());
}
SatPreprocessor* SatPreParams::create(const SatPreParams& opts) {
	if (opts.type != 0) { return new SatElite(); }
	return 0;
}
}
