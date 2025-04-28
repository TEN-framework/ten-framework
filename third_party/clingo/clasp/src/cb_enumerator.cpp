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
#include <clasp/cb_enumerator.h>
#include <clasp/solver.h>
#include <clasp/clause.h>
#if CLASP_HAS_THREADS
#include <clasp/mt/thread.h>
#define ACQUIRE_LOCK(m) while ( (m).exchange(1) != 0 ) Clasp::mt::this_thread::yield()
#define RELEASE_LOCK(m) (m) = 0
#else
#define ACQUIRE_LOCK(m)
#define RELEASE_LOCK(m)
#endif
namespace Clasp {
/////////////////////////////////////////////////////////////////////////////////////////
// CBConsequences::SharedConstraint
/////////////////////////////////////////////////////////////////////////////////////////
class CBConsequences::SharedConstraint {
public:
	SharedConstraint() : current(0) { mutex = 0; }
	SharedLiterals* fetch_if_neq(SharedLiterals* last) const {
		ACQUIRE_LOCK(mutex);
		SharedLiterals* ret = last != current ? current->share() : 0;
		RELEASE_LOCK(mutex);
		return ret;
	}
	void release(SharedLiterals* newLits) {
		ACQUIRE_LOCK(mutex);
		SharedLiterals* old = current;
		current = newLits;
		RELEASE_LOCK(mutex);
		if (old) { old->release(); }
	}
	SharedLiterals* current;
	typedef Clasp::Atomic_t<int>::type MutexType;
	mutable MutexType mutex;
};
#undef ACQUIRE_LOCK
#undef RELEASE_LOCK
/////////////////////////////////////////////////////////////////////////////////////////
// CBConsequences::CBFinder
/////////////////////////////////////////////////////////////////////////////////////////
class CBConsequences::CBFinder : public EnumerationConstraint {
public:
	typedef CBConsequences::SharedConstraint  SharedCon;
	typedef Solver::ConstraintDB              ConstraintDB;
	typedef SharedLiterals                    SharedLits;
	explicit CBFinder(SharedCon* sh) : EnumerationConstraint(), shared(sh), last(0) {}
	ConPtr clone() { return new CBFinder(shared); }
	void   doCommitModel(Enumerator& ctx, Solver& s) { static_cast<CBConsequences&>(ctx).addCurrent(s, current, s.model, rootLevel()); }
	void   destroy(Solver* s, bool detach);
	bool   doUpdate(Solver& s);
	void   pushLocked(Solver& s, ClauseHead* h);
	LitVec       current;
	SharedCon*   shared;
	SharedLits*  last;
	ConstraintDB locked;
};
/////////////////////////////////////////////////////////////////////////////////////////
// CBConsequences::QueryFinder
/////////////////////////////////////////////////////////////////////////////////////////
class CBConsequences::QueryFinder : public EnumerationConstraint{
public:
	class State {
	public:
		State(Model& m, uint32 nVars) : model_(&m) {
			refs_  = 1;
			size_  = nVars;
			value_ = new ValueType[nVars];
			for (uint32 i = 0; i != nVars; ++i) { value_[i] = 0; }
		}
		State* share()    { ++refs_; return this; }
		void   release()  { if (--refs_ == 0) delete this; }
		uint32 size()          const { return size_; }
		bool   open(Literal p) const { return (value_[p.var()] & Model::estMask(p)) != 0; }
		void   setModel(Clasp::ValueVec& m, bool update) {
			m.assign(value_, value_ + size_);
			if (update) { model_->values = &m; model_->up = 1; }
		}
		void   push(Literal p) { value_[p.var()] = Model::estMask(p)|trueValue(p);}
		void   pop(Literal p)  { value_[p.var()] = 0; }
		void   fix(Literal p)  { value_[p.var()] = trueValue(p); }
	private:
		~State() { delete [] value_; }
		typedef Clasp::Atomic_t<uint8>::type  ValueType;
		typedef Clasp::Atomic_t<uint32>::type SizeType;
		typedef ValueType* ValueVec;
		ValueVec value_;
		Model*   model_;
		uint32   size_;
		SizeType refs_;
	};
	explicit QueryFinder(const LitVec& c, Model& m, uint32 nVars) : EnumerationConstraint(), open_(c), state_(new State(m, nVars)), query_(lit_false()), level_(0), dirty_(false) {
		state_->push(query_);
	}
	explicit QueryFinder(const LitVec& c, State* st) : EnumerationConstraint(), open_(c), state_(st), query_(lit_false()), level_(0), dirty_(false) {
	}
	~QueryFinder() { state_->release(); }
	ConPtr clone() { return new QueryFinder(open_, state_->share()); }
	bool    hasQuery() const { return query_ != lit_false(); }
	bool    doUpdate(Solver& s);
	void    doCommitModel(Enumerator&, Solver&);
	void    doCommitUnsat(Enumerator&, Solver&);
	void    updateUpper(Solver& s, uint32 rl, ValueVec& mOut);
	void    updateLower(Solver& s, uint32 rl, ValueVec& mOut);
	bool    selectOpen(Solver& s, Literal& q);
	void    reason(Solver& s, Literal p, LitVec& out) {
		for (uint32 i = 1, end = s.level(p.var()); i <= end; ++i) {
			Literal q = s.decision(i);
			if (q != p) { out.push_back(q); }
		}
	}
	bool    popQuery(Solver& s) {
		if (!hasQuery() || s.rootLevel() == level_ || s.value(query_.var()) == value_free) {
			return s.popRootLevel(0);
		}
		return s.popRootLevel((s.rootLevel() - level_) + 1);
	}
	LitVec  open_;
	State*  state_;
	Literal query_;
	uint32  level_;
	bool    dirty_;
};
// Reduce the overestimate by computing c = c \cap M,
// where M is the current model stored in s.
void CBConsequences::QueryFinder::updateUpper(Solver& s, uint32 root, ValueVec& mOut) {
	LitVec::iterator j = open_.begin();
	for (LitVec::iterator it = j, end = open_.end(); it != end; ++it) {
		if      (!state_->open(*it))        { continue; }
		else if (!s.isTrue(*it))            { state_->pop(*it); }
		else if (s.level(it->var()) > root) { *j++ = *it; }
		else                                { state_->fix(*it); }
	}
	open_.erase(j, open_.end());
	state_->setModel(mOut, dirty_ = false);
}
// Adds facts to (under) estimate.
void CBConsequences::QueryFinder::updateLower(Solver& s, uint32 rl, ValueVec& mOut) {
	LitVec::iterator j = open_.begin();
	for (LitVec::iterator it = j, end = open_.end(); it != end; ++it) {
		ValueRep val = s.value(it->var());
		if (val != value_free && s.level(it->var()) > rl) {
			val = value_free;
		}
		if      (!state_->open(*it)) { continue; }
		else if (val == value_free)  { *j++ = *it; }
		else if (s.isTrue(*it))      { state_->fix(*it); }
		else                         { state_->pop(*it); }
	}
	if (j != open_.end()) { dirty_ = true; }
	open_.erase(j, open_.end());
	state_->setModel(mOut, dirty_);
	dirty_ = false;
}
bool CBConsequences::QueryFinder::selectOpen(Solver& s, Literal& q) {
	for (LitVec::size_type i = 0, end = open_.size();; --end, open_.pop_back()) {
		for (; i != end && s.value(open_[i].var()) == value_free && state_->open(open_[i]); ++i) { ; }
		if (i == end) { break; }
		q = open_[i];
		open_[i] = open_.back();
		if (s.isTrue(q)) { state_->fix(q); }
		else             { state_->pop(q); }
		dirty_ = true;
	}
	if (open_.empty()) { return false; }
	q = s.heuristic()->selectRange(s, &open_[0], &open_[0] + open_.size());
	return true;
}
// solve(~query) produced a model - query is not a cautious consequence, update overstimate
void CBConsequences::QueryFinder::doCommitModel(Enumerator&, Solver& s) {
	if (!hasQuery() && state_->open(query_)) {
		// init state to first model
		for (LitVec::iterator it = open_.begin(), end = open_.end(); it != end; ++it) {
			if (s.isTrue(*it)) { state_->push(*it); }
		}
	}
	state_->pop(query_);
	updateUpper(s, level_, s.model);
	query_.flag();
}
// solve(~query) failed - query is a cautious consequence
void CBConsequences::QueryFinder::doCommitUnsat(Enumerator&, Solver& s) {
	bool commit = !disjointPath() && s.hasConflict() && !s.hasStopConflict() && hasQuery();
	popQuery(s);
	if (commit) {
		state_->fix(query_);
		query_.flag();
	}
	updateLower(s, level_, s.model);
}
bool CBConsequences::QueryFinder::doUpdate(Solver& s) {
	bool newQ = query_.flagged() || !state_->open(query_);
	if (newQ || s.value(query_.var()) == value_free) { // query was SAT/UNSAT or solved by other thread
		if (!popQuery(s)) { return false; }
		assert(s.decisionLevel() == s.rootLevel());
		level_ = s.rootLevel();
		return newQ && !selectOpen(s, query_) ? s.force(query_ = lit_false(), this) : s.pushRoot(~query_);
	}
	return true;
}
/////////////////////////////////////////////////////////////////////////////////////////
// CBConsequences
/////////////////////////////////////////////////////////////////////////////////////////
CBConsequences::CBConsequences(Type type, Algo algo)
	: Enumerator()
	, shared_(0)
	, type_(type)
	, algo_(algo) {
	if (type_ != Cautious) { algo_ = Default; }
}

Enumerator* EnumOptions::createConsEnumerator(const EnumOptions& opts) {
	return new CBConsequences(opts.enumMode == enum_brave ? CBConsequences::Brave : CBConsequences::Cautious, opts.enumMode != enum_query ? CBConsequences::Default : CBConsequences::Query);
}
CBConsequences::~CBConsequences() {
	delete shared_;
}
bool CBConsequences::supportsSplitting(const SharedContext& problem) const {
	return algo_ == Default && Enumerator::supportsSplitting(problem);
}
int CBConsequences::unsatType() const {
	return algo_ == Default ? Enumerator::unsatType() : Enumerator::unsat_sync;
}
EnumerationConstraint* CBConsequences::doInit(SharedContext& ctx, SharedMinimizeData* m, int) {
	cons_.clear();
	const OutputTable& out = ctx.output;
	if (out.projectMode() == ProjectMode_t::Output) {
		for (OutputTable::pred_iterator it = out.pred_begin(), end = out.pred_end(); it != end; ++it) {
			addLit(ctx, it->cond);
		}
		for (OutputTable::range_iterator it = out.vars_begin(), end = out.vars_end(); it != end; ++it) {
			addLit(ctx, posLit(*it));
		}
	}
	else {
		for (OutputTable::lit_iterator it = out.proj_begin(), end = out.proj_end(); it != end; ++it) {
			addLit(ctx, *it);
		}
	}
	// init M to either cons or {} depending on whether we compute cautious or brave cons.
	const uint32 fMask = (type_ == Cautious && algo_ != Query);
	const uint32 vMask = (type_ == Cautious) ? 3u : 0u;
	for (LitVec::iterator it = cons_.begin(), end = cons_.end(); it != end; ++it) {
		it->rep() |= fMask;
		ctx.unmark(it->var());
		if (!ctx.varInfo(it->var()).nant()) {
			ctx.master()->setPref(it->var(), ValueSet::def_value, static_cast<ValueRep>(trueValue(*it) ^ vMask));
		}
	}
	delete shared_; shared_ = 0;
	setIgnoreSymmetric(true);
	if (m && m->optimize() && algo_ == Query) {
		ctx.warn("Query algorithm does not support optimization!");
		algo_ = Default;
	}
	if (type_ != Cautious || algo_ != Query) {
		shared_ = ctx.concurrency() > 1 ? new SharedConstraint() : 0;
		return new CBFinder(shared_);
	}
	return new QueryFinder(cons_, model(), ctx.numVars() + 1);
}
void CBConsequences::addLit(SharedContext& ctx, Literal p) {
	if (!ctx.marked(p) && !ctx.eliminated(p.var())) {
		cons_.push_back(p);
		ctx.setFrozen(p.var(), true);
		ctx.mark(p);
	}
}
void CBConsequences::addCurrent(Solver& s, LitVec& con, ValueVec& m, uint32 root) {
	con.assign(1, ~s.sharedContext()->stepLiteral());
	// reset state of relevant variables
	for (LitVec::iterator it = cons_.begin(), end = cons_.end(); it != end; ++it) {
		m[it->var()] = 0;
	}
	// let M be all lits p with p.watch() == true
	for (LitVec::iterator it = cons_.begin(), end = cons_.end(); it != end; ++it) {
		Literal& p = *it;
		uint32  dl = s.level(p.var());
		uint32 ost = dl > root ? Model::estMask(p) : 0;
		if (type_ == Brave) {
			// brave: extend M with true literals and force a literal not in M to true
			if      (p.flagged() || s.isTrue(p)) { p.flag(); ost = 0; }
			else if (dl)                         { con.push_back(p); }
		}
		else if (type_ == Cautious) {
			// cautious: intersect M with true literals and force a literal in M to false
			if      (!p.flagged() || s.isFalse(p)) { p.unflag(); ost = 0; }
			else if (dl)                           { con.push_back(~p); }
		}
		// set output state
		if (p.flagged()) { ost |= trueValue(p); }
		m[p.var()] |= ost;
	}
	if (shared_) {
		shared_->release(SharedLiterals::newShareable(con, Constraint_t::Other, 1));
	}
}
/////////////////////////////////////////////////////////////////////////////////////////
// CBConsequences::CBFinder implementation
/////////////////////////////////////////////////////////////////////////////////////////
void CBConsequences::CBFinder::destroy(Solver* s, bool detach) {
	Clasp::destroyDB(locked, s, detach);
	if (last) {
		last->release();
	}
	EnumerationConstraint::destroy(s, detach);
}
void CBConsequences::CBFinder::pushLocked(Solver& s, ClauseHead* c) {
	for (ClauseHead* h; !locked.empty() && !(h = static_cast<ClauseHead*>(locked.back()))->locked(s);) {
		h->destroy(&s, true);
		locked.pop_back();
	}
	locked.push_back(c);
}
bool CBConsequences::CBFinder::doUpdate(Solver& s) {
	ClauseCreator::Result ret;
	uint32 flags = ClauseCreator::clause_explicit|ClauseCreator::clause_no_add;
	if (!shared) {
		ret  = !current.empty() ? ClauseCreator::create(s, current, flags, ConstraintInfo(Constraint_t::Other)) : ClauseCreator::Result();
	}
	else if (SharedLiterals* x = shared->fetch_if_neq(last)) {
		if (last) { last->release(); }
		last = x;
		ret  = ClauseCreator::integrate(s, x, flags | ClauseCreator::clause_no_release);
	}
	if (ret.local) { pushLocked(s, ret.local); }
	current.clear();
	return ret.ok();
}
}
