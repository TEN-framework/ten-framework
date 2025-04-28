//
// Copyright (c) 2009-2017 Benjamin Kaufmann
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
#include <clasp/lookahead.h>
#include <algorithm>
namespace Clasp {
/////////////////////////////////////////////////////////////////////////////////////////
// Lookahead scoring
/////////////////////////////////////////////////////////////////////////////////////////
uint32 ScoreLook::countNant(const Solver& s, const Literal* b, const Literal *e) const {
	uint32 sc = 1;
	for (; b != e; ++b) { sc += s.varInfo(b->var()).nant(); }
	return sc;
}
void ScoreLook::scoreLits(const Solver& s, const Literal* b, const Literal *e) {
	assert(b < e);
	uint32 sc = !nant ? uint32(e-b) : countNant(s, b, e);
	Var v     = b->var();
	assert(validVar(v));
	score[v].setScore(*b, sc);
	if (addDeps) {
		if ((score[v].testedBoth() || mode == score_max) && greater(v, best)) {
			best = v;
		}
		for (; b != e; ++b) {
			v = b->var();
			if (validVar(v) && (s.varInfo(v).type() & types) != 0) {
				if (!score[v].seen()) { deps.push_back(v); }
				score[v].setDepScore(*b, sc);
				score[v].setSeen(*b);
			}
		}
	}
}
void ScoreLook::clearDeps() {
	for (VarVec::size_type i = 0, end = deps.size(); i != end; ++i) {
		score[deps[i]].clear();
	}
	deps.clear();
	best = 0;
}
bool ScoreLook::greater(Var lhs, Var rhs) const {
	uint32 rhsMax, rhsMin;
	score[rhs].score(rhsMax, rhsMin);
	return mode == score_max
		? greaterMax(lhs, rhsMax)
		: greaterMaxMin(lhs, rhsMax, rhsMin);
}
/////////////////////////////////////////////////////////////////////////////////////////
// Lookahead propagator
/////////////////////////////////////////////////////////////////////////////////////////
Lookahead::Lookahead(const Params& p)
	: nodes_(2, LitNode(lit_true()))
	, last_(head_id)    // circular list
	, pos_(head_id)     // lookahead start pos
	, top_(uint32(-2))
	, limit_(p.lim) {
	head()->next = head_id;
	undo()->next = UINT32_MAX;
	if (p.type != Var_t::Hybrid) {
		score.mode = ScoreLook::score_max_min;
	}
	else {
		score.mode = ScoreLook::score_max;
	}
	score.types = p.type;
	if (p.topLevelImps) { head()->lit.flag(); }
	score.nant = p.restrictNant;
}

Lookahead::~Lookahead() {}

void Lookahead::detach(Solver& s) {
	s.removePost(this);
	while (saved_.size()>1) {
		s.removeUndoWatch(uint32(saved_.size()-1), this);
		saved_.pop_back();
	}
}
void Lookahead::destroy(Solver* s, bool detach) {
	if (s && detach) { Lookahead::detach(*s); }
	PostPropagator::destroy(s, detach);
}

uint32 Lookahead::priority() const { return priority_reserved_look; }

void Lookahead::clear() {
	score.clearDeps();
	while (!saved_.empty()) {
		if (saved_.back() != UINT32_MAX) {
			splice(saved_.back());
		}
		saved_.pop_back();
	}
	LookList(2, *head()).swap(nodes_);
	head()->next = head_id;
	undo()->next = UINT32_MAX;
	last_        = head_id;
	top_         = UINT32_MAX;
}

bool Lookahead::init(Solver& s) {
	ScoreLook& sc = score;
	sc.clearDeps();
	Var start     = (uint32)sc.score.size();
	sc.score.resize(s.numVars()+1);
	const VarType types= sc.types;
	const bool uniform = types != Var_t::Hybrid;
	uint32 add    = 0;
	uint32 nants  = 0;
	for (Var v = start; v <= s.numVars(); ++v) {
		if (s.value(v) == value_free && (s.varInfo(v).type() & types) != 0 ) {
			++add;
			nants += s.varInfo(v).nant();
		}
	}
	nodes_.reserve(nodes_.size() + add);
	for (Var v = start; v <= s.numVars(); ++v) {
		if (s.value(v) == value_free && (s.varInfo(v).type() & types) != 0 ) {
			append(Literal(v, s.varInfo(v).preferredSign()), uniform || s.varInfo(v).type() == Var_t::Hybrid);
		}
	}
	if (add && score.nant) {
		score.nant = add != nants && nants != 0;
	}
	return true;
}

void Lookahead::append(Literal p, bool testBoth) {
	node(last_)->next = static_cast<NodeId>(nodes_.size());
	nodes_.push_back(LitNode(p));
	last_             = node(last_)->next;
	node(last_)->next = head_id;
	// remember to also test ~p by setting watched-flag
	if (testBoth) { node(last_)->lit.flag(); }
}

// test p and optionally ~p if necessary
bool Lookahead::test(Solver& s, Literal p) {
	return (score.score[p.var()].seen(p) || s.test(p, this))
		&& (!p.flagged() || score.score[p.var()].seen(~p) || s.test(~p, this))
		&& (imps_.empty() || checkImps(s, p));
}

bool Lookahead::checkImps(Solver& s, Literal p) {
	assert(!imps_.empty());
	bool ok = true;
	if (score.score[p.var()].testedBoth()) {
		for (LitVec::const_iterator it = imps_.begin(), end = imps_.end(); it != end && ok; ++it) {
			ok  = s.force(*it, lit_true());
		}
	}
	imps_.clear();
	return ok && (s.queueSize() == 0 || s.propagateUntil(this));
}

// failed-literal detection - stop on failed-literal
bool Lookahead::propagateLevel(Solver& s) {
	assert(!s.hasConflict());
	saved_.resize(s.decisionLevel()+1, UINT32_MAX);
	uint32 undoId = saved_[s.decisionLevel()];
	if (undoId == UINT32_MAX) {
		undoId = undo_id;
		if (s.decisionLevel() != 0) {
			s.addUndoWatch(s.decisionLevel(), this);
		}
	}
	score.clearDeps();
	score.addDeps = true;
	Literal p     = node(pos_)->lit;
	bool   ok     = s.value(p.var()) != value_free || test(s, p);
	for (LitNode* r = node(pos_); r->next != pos_ && ok; ) {
		p = node(r->next)->lit;
		if (s.value(p.var()) == value_free) {
			if (test(s, p)) { r   = node(r->next); }
			else            { pos_= r->next; ok = false; }
		}
		else if (r->next != last_ && r->next != head_id) {
			// unlink from candidate list
			NodeId t       = r->next;
			r->next        = node(t)->next;
			// append to undo list
			LitNode* u     = node(undoId);
			node(t)->next  = u->next;
			u->next        = t;
			undoId         = t;
		}
		else { r = node(r->next); } // keep iterators valid; never unlink last node and dummy head
	}
	saved_.back() = undoId;
	return ok;
}

bool Lookahead::propagateFixpoint(Solver& s, PostPropagator* ctx) {
	if ((empty() || top_ == s.numAssignedVars()) && !score.deps.empty()) {
		// nothing to lookahead
		return true;
	}
	bool ok = true;
	uint32 dl;
	for (dl = s.decisionLevel(); !propagateLevel(s); dl = s.decisionLevel()) {
		// some literal failed
		// resolve and propagate conflict
		assert(s.decisionLevel() >= dl);
		if (!s.resolveConflict() || !s.propagateUntil(this)) {
			ok = false;
			score.clearDeps();
			break;
		}
	}
	if (dl == 0 && ok) {
		// remember top-level size - no need to redo lookahead
		// on level 0 unless we learn a new implication
		assert(s.queueSize() == 0);
		top_ = s.numAssignedVars();
		LitVec().swap(imps_);
	}
	if (!ctx && limit_ && --limit_ == 0) {
		this->destroy(&s, true);
	}
	return ok;
}

// splice list [undo_.next, ul] back into candidate list
void Lookahead::splice(NodeId ul) {
	assert(ul != UINT32_MAX);
	if (ul != undo_id) {
		assert(undo()->next != UINT32_MAX);
		// unlink from undo list
		LitNode* ulNode= node(ul);
		NodeId   first = undo()->next;
		undo()->next   = ulNode->next;
		// splice into look-list
		ulNode->next   = head()->next;
		head()->next   = first;
	}
}

void Lookahead::undoLevel(Solver& s) {
	if (s.decisionLevel() == saved_.size()) {
		cancelPropagation();
		const LitVec& a = s.trail();
		score.scoreLits(s, &a[0]+s.levelStart(s.decisionLevel()), &a[0]+a.size());
		if (s.decisionLevel() == static_cast<uint32>(head()->lit.flagged())) {
			const Literal* b = &a[0]+s.levelStart(s.decisionLevel());
			if (b->flagged()) {
				// remember current DL for b
				uint32 dist = static_cast<uint32>(((&a[0]+a.size()) - b));
				imps_.assign(b+1, b + std::min(dist, uint32(2048)));
			}
			else if (score.score[b->var()].testedBoth()) {
				// all true lits in imps_ follow from both *b and ~*b
				// and are therefore implied
				LitVec::iterator j = imps_.begin();
				for (LitVec::iterator it = imps_.begin(), end = imps_.end(); it != end; ++it) {
					if (s.isTrue(*it)) { *j++ = *it; }
				}
				imps_.erase(j, imps_.end());
			}
		}
	}
	else {
		assert(saved_.size() >= s.decisionLevel()+1);
		saved_.resize(s.decisionLevel()+1);
		NodeId n = saved_.back();
		saved_.pop_back();
		splice(n);
		assert(node(last_)->next == head_id);
		score.clearDeps();
	}
}

Literal Lookahead::heuristic(Solver& s) {
	if (s.value(score.best) != value_free) {
		// no candidate available
		return lit_true();
	}
	ScoreLook& sc = score;
	Literal choice= Literal(sc.best, sc.score[sc.best].prefSign());
	if (!sc.deps.empty() && sc.mode == ScoreLook::score_max_min) {
		// compute heuristic values for candidates skipped during last lookahead
		uint32 min, max;
		sc.score[sc.best].score(max, min);
		sc.addDeps = false;
		bool ok    = true;
		LitVec::size_type i = 0;
		do {
			Var v        = sc.deps[i];
			VarScore& vs = sc.score[v];
			if (s.value(v) == value_free) {
				uint32 vMin, vMax;
				vs.score(vMax, vMin);
				if (vMin == 0 || vMin > min || (vMin == min && vMax > max)) {
					uint32 neg = vs.score(negLit(v)) > 0 ? vs.score(negLit(v)) : max+1;
					uint32 pos = vs.score(posLit(v)) > 0 ? vs.score(posLit(v)) : max+1;
					if (!vs.tested(negLit(v))) {
						ok  = ok && s.test(negLit(v), this);
						neg = vs.score(negLit(v));
					}
					if ((neg > min || (neg == min && pos > max)) && !vs.tested(posLit(v))) {
						ok  = ok && s.test(posLit(v), this);
					}
				}
				if (vs.testedBoth() && sc.greaterMaxMin(v, max, min)) {
					vs.score(max, min);
					choice = Literal(v, vs.prefSign());
				}
			}
		} while (++i != sc.deps.size() && ok);
		if (!ok) {
			// One of the candidates failed. Since none of them failed
			// during previous propagation, this indicates that
			// either some post propagator has wrong priority or
			// parallel solving is active and a stop conflict was set.
			// Since we can't resolve the problem here, we simply return the
			// literal that caused the conflict
			assert(s.hasConflict());
			return lit_false();
		}
	}
	return choice;
}
/////////////////////////////////////////////////////////////////////////////////////////
// Lookahead heuristic
/////////////////////////////////////////////////////////////////////////////////////////
UnitHeuristic::UnitHeuristic() { }
void UnitHeuristic::endInit(Solver& s) {
	Lookahead* look = static_cast<Lookahead*>(s.getPost(Lookahead::priority_reserved_look));
	if (!look) { s.addPost(new Lookahead(Var_t::Atom)); }
}
Literal UnitHeuristic::doSelect(Solver& s) {
	Lookahead* look = static_cast<Lookahead*>(s.getPost(Lookahead::priority_reserved_look));
	Literal x       = look ? look->heuristic(s) : lit_true();
	if (x != lit_true()) { return x; }
	return SelectFirst::doSelect(s);
}
/////////////////////////////////////////////////////////////////////////////////////////
// Restricted Lookahead heuristic - lookahead and heuristic for a limited number of ops
/////////////////////////////////////////////////////////////////////////////////////////
class Restricted : public UnitHeuristic {
public:
	typedef LitVec::size_type size_t;
	typedef ConstraintType    con_t;
	Restricted(DecisionHeuristic* other)
		: UnitHeuristic()
		, other_(other)
		, disabled_(false) {
	}
	Literal doSelect(Solver& s) {
		return !disabled_ ? heuristic(s) : other_->doSelect(s);
	}
	// heuristic interface - forward to other
	void startInit(const Solver& s)           { other_->startInit(s); }
	void endInit(Solver& s)                   { other_->endInit(s); }
	void setConfig(const HeuParams& p)        { other_->setConfig(p); }
	void detach(Solver& s)                    { if (other_.is_owner()) { other_->detach(s); } }
	void simplify(const Solver& s, size_t st) { other_->simplify(s, st); }
	void undoUntil(const Solver& s, size_t st){ other_->undoUntil(s, st); }
	void updateReason(const Solver& s, const LitVec& x, Literal r)            { other_->updateReason(s, x, r); }
	bool bump(const Solver& s, const WeightLitVec& w, double d)               { return other_->bump(s, w, d); }
	void newConstraint(const Solver& s, const Literal* p, size_t sz, con_t t) { other_->newConstraint(s, p, sz, t); }
	void updateVar(const Solver& s, Var v, uint32 n)                          { other_->updateVar(s, v, n); }
	Literal selectRange(Solver& s, const Literal* f, const Literal* l)        { return other_->selectRange(s, f, l); }
private:
	void disable(Solver& s) {
		disabled_ = true;
		if (s.heuristic() == this)
			s.setHeuristic(other_.release(), Ownership_t::Acquire);
	}
	Literal heuristic(Solver& s) {
		Literal choice;
		Lookahead* look = static_cast<Lookahead*>(s.getPost(Lookahead::priority_reserved_look));
		if (!look || !look->hasLimit()) {
			choice = other_->doSelect(s);
			disable(s);
		}
		else {
			Literal p = look->heuristic(s);
			choice = p != lit_true() ? p : other_->doSelect(s);
		}
		return choice;
	}
	typedef SingleOwnerPtr<DecisionHeuristic> HeuPtr;
	HeuPtr other_;
	bool   disabled_;
};

UnitHeuristic* UnitHeuristic::restricted(DecisionHeuristic* other) {
	return new Restricted(other);
}
}
