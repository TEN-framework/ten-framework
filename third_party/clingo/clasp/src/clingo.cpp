//
// Copyright (c) 2015-2017 Benjamin Kaufmann
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
#include <clasp/clingo.h>
#include <clasp/solver.h>
#include <clasp/clause.h>
#include <algorithm>
#include POTASSCO_EXT_INCLUDE(unordered_map)
namespace Clasp { namespace {
	template <class O, class L, void (L::*enter)(), void (L::*exit)(), class OP>
	struct Scoped {
		Scoped(L* lock, O* obj, const OP& op = OP()) : lock_(lock), obj_(obj), op_(op) { if (lock) (lock->*enter)(); }
		~Scoped() { if (lock_) (lock_->*exit)(); }
		O* operator->() const { op_(); return obj_; }
		L* lock_;
		O* obj_;
		OP op_;
	};
	struct Nop { void operator()() const {} };
	struct Inc { Inc(LitVec::size_type& e) : epoch_(&e) {} void operator()() const { ++*epoch_; } LitVec::size_type* epoch_; };
	typedef Scoped<Potassco::AbstractPropagator, ClingoPropagatorLock, &ClingoPropagatorLock::lock, &ClingoPropagatorLock::unlock, Inc> ScopedLock;
	typedef Scoped<ClingoPropagator, ClingoPropagatorLock, &ClingoPropagatorLock::unlock, &ClingoPropagatorLock::lock, Nop> ScopedUnlock;
}
ClingoPropagatorLock::~ClingoPropagatorLock() {}
/////////////////////////////////////////////////////////////////////////////////////////
// ClingoAssignment
/////////////////////////////////////////////////////////////////////////////////////////
static const uint32_t trailOffset = 1u; // Offset for handling true literal.

ClingoAssignment::ClingoAssignment(const Solver& s)
	: solver_(&s) {}

ClingoAssignment::Value_t ClingoAssignment::value(Lit_t lit)  const {
	POTASSCO_REQUIRE(ClingoAssignment::hasLit(lit), "Invalid literal");
	const uint32_t var = decodeVar(lit);
	switch (solver_->validVar(var) ? solver_->value(var) : value_free) {
		default: return Value_t::Free;
		case value_true:  return lit >= 0 ? Value_t::True  : Value_t::False;
		case value_false: return lit >= 0 ? Value_t::False : Value_t::True;
	}
}
uint32_t ClingoAssignment::level(Lit_t lit)  const {
	return ClingoAssignment::value(lit) != Potassco::Value_t::Free
		? solver_->level(decodeVar(lit))
		: UINT32_MAX;
}
ClingoAssignment::Lit_t ClingoAssignment::decision(uint32_t dl) const {
	POTASSCO_REQUIRE(dl <= solver_->decisionLevel(), "Invalid decision level");
	return encodeLit(dl ? solver_->decision(dl) : lit_true());
}
ClingoAssignment::Lit_t ClingoAssignment::trailAt(uint32_t pos) const {
	POTASSCO_REQUIRE(pos < trailSize(), "Invalid trail position");
	return pos != 0 ? encodeLit(solver_->trail()[pos - trailOffset]) : encodeLit(lit_true());
}
uint32_t ClingoAssignment::trailBegin(uint32_t dl) const {
	POTASSCO_REQUIRE(dl <= solver_->decisionLevel(), "Invalid decision level");
	return dl != 0 ? solver_->levelStart(dl) + trailOffset : 0;
}
uint32_t ClingoAssignment::size()            const { return std::max(solver_->numVars(), solver_->numProblemVars()) + trailOffset; }
uint32_t ClingoAssignment::unassigned()      const { return size() - trailSize(); }
bool     ClingoAssignment::hasConflict()     const { return solver_->hasConflict(); }
uint32_t ClingoAssignment::level()           const { return solver_->decisionLevel(); }
uint32_t ClingoAssignment::rootLevel()       const { return solver_->rootLevel(); }
bool     ClingoAssignment::hasLit(Lit_t lit) const { return decodeVar(lit) < size(); }
bool     ClingoAssignment::isTotal()         const { return unassigned() == 0u; }
uint32_t ClingoAssignment::trailSize()       const { return static_cast<uint32_t>(solver_->trail().size() + trailOffset); }
/////////////////////////////////////////////////////////////////////////////////////////
// ClingoPropagator::Control
/////////////////////////////////////////////////////////////////////////////////////////
class ClingoPropagator::Control : public Potassco::AbstractSolver {
public:
	Control(ClingoPropagator& ctx, Solver& s, uint32 st = 0u) : ctx_(&ctx), assignment_(s), state_(st | state_ctrl) {}
	const Potassco::AbstractAssignment& assignment() const { return assignment_; }

	// Potassco::AbstractSolver
	virtual Potassco::Id_t id() const { return solver().id(); }
	virtual bool addClause(const Potassco::LitSpan& clause, Potassco::Clause_t prop);
	virtual bool propagate();
	virtual Lit  addVariable();
	virtual bool hasWatch(Lit lit) const;
	virtual void addWatch(Lit lit);
	virtual void removeWatch(Lit lit);
private:
	typedef ClingoPropagator::State State;
	ClingoPropagatorLock* lock()   const { return (state_ & state_init) == 0 ? ctx_->call_->lock() : 0; }
	Solver&               solver() const { return const_cast<Solver&>(assignment_.solver()); }
	ClingoPropagator* ctx_;
	ClingoAssignment  assignment_;
	uint32            state_;
};
bool ClingoPropagator::Control::addClause(const Potassco::LitSpan& clause, Potassco::Clause_t prop) {
	POTASSCO_REQUIRE(!assignment_.hasConflict(), "Invalid addClause() on conflicting assignment");
	ScopedUnlock pp(lock(), ctx_);
	pp->toClause(solver(), clause, prop);
	return pp->addClause(solver(), state_);
}
bool ClingoPropagator::Control::propagate() {
	ScopedUnlock unlocked(lock(), ctx_);
	if (solver().hasConflict())    { return false; }
	if (solver().queueSize() == 0) { return true;  }

	ClingoPropagator::size_t epoch = ctx_->epoch_;
	ctx_->registerUndoCheck(solver());
	ctx_->propL_ = solver().decisionLevel();
	const bool result = (state_ & state_prop) != 0u && solver().propagateUntil(unlocked.obj_) && epoch == ctx_->epoch_;
	ctx_->propL_ = UINT32_MAX;
	return result;
}
Potassco::Lit_t ClingoPropagator::Control::addVariable() {
	POTASSCO_REQUIRE(!assignment_.hasConflict(), "Invalid addVariable() on conflicting assignment");
	ScopedUnlock unlocked(lock(), ctx_);
	return encodeLit(posLit(solver().pushAuxVar()));
}
bool ClingoPropagator::Control::hasWatch(Lit_t lit) const {
	ScopedUnlock unlocked(lock(), ctx_);
	return assignment_.hasLit(lit) && solver().hasWatch(decodeLit(lit), ctx_);
}
void ClingoPropagator::Control::addWatch(Lit_t lit) {
	ScopedUnlock unlocked(lock(), ctx_);
	POTASSCO_REQUIRE(assignment_.hasLit(lit), "Invalid literal");
	Literal p = decodeLit(lit);
	Solver& s = solver();
	if (!s.hasWatch(p, ctx_)) {
		POTASSCO_REQUIRE(!s.sharedContext()->validVar(p.var()) || !s.sharedContext()->eliminated(p.var()), "Watched literal not frozen");
		s.addWatch(p, ctx_);
		if ((state_ & state_init) != 0u && s.isTrue(p)) {
			// are we too late?
			bool inQ = std::find(s.trail().begin() + s.assignment().front, s.trail().end(), p) != s.trail().end();
			if (!inQ && !ctx_->inTrail(p)) {
				uint32 ignore = 0;
				ctx_->propagate(s, p, ignore);
			}
		}
	}
}
void ClingoPropagator::Control::removeWatch(Lit_t lit) {
	ScopedUnlock unlocked(lock(), ctx_);
	if (assignment_.hasLit(lit)) {
		solver().removeWatch(decodeLit(lit), ctx_);
	}
}
/////////////////////////////////////////////////////////////////////////////////////////
// ClingoPropagator
/////////////////////////////////////////////////////////////////////////////////////////
static const uint32 CHECK_BIT = 31;
// flags for clauses from propagator
static const uint32 ccFlags_s[2] = {
	/* 0: learnt */ ClauseCreator::clause_not_sat | Clasp::ClauseCreator::clause_int_lbd,
	/* 1: static */ ClauseCreator::clause_no_add  | ClauseCreator::clause_explicit
};
ClingoPropagator::ClingoPropagator(Propagator* p)
	: call_(p)
	, prop_(0), epoch_(0), level_(0), propL_(UINT32_MAX), front_(-1), init_(0) {
}
uint32 ClingoPropagator::priority() const { return static_cast<uint32>(priority_class_general); }

void ClingoPropagator::destroy(Solver* s, bool detach) {
	if (s && detach) {
		for (Var v = 1; v <= s->numVars(); ++v) {
			s->removeWatch(posLit(v), this);
			s->removeWatch(negLit(v), this);
		}
	}
	destroyDB(db_, s, detach);
	PostPropagator::destroy(s, detach);
}

bool ClingoPropagator::init(Solver& s) {
	POTASSCO_REQUIRE(s.decisionLevel() == 0 && prop_ <= trail_.size(), "Invalid init");
	Control ctrl(*this, s, state_init);
	s.acquireProblemVars();
	if (s.isMaster() && !s.sharedContext()->frozen())
		call_->prepare(const_cast<SharedContext&>(*s.sharedContext()));
	init_  = call_->init(init_, ctrl);
	front_ = (call_->checkMode() & ClingoPropagatorCheck_t::Fixpoint) ? -1 : INT32_MAX;
	return true;
}

bool ClingoPropagator::inTrail(Literal p) const {
	return std::find(trail_.begin(), trail_.end(), encodeLit(p)) != trail_.end();
}

void ClingoPropagator::registerUndo(Solver& s, uint32 data) {
	uint32 dl = s.decisionLevel();
	if (dl != level_) {
		POTASSCO_REQUIRE(dl > level_, "Stack property violated");
		// first time we see this level
		s.addUndoWatch(level_ = dl, this);
		undo_.push_back(data);
	}
	else if (!undo_.empty() && data < undo_.back()) {
		POTASSCO_ASSERT(test_bit(undo_.back(), CHECK_BIT));
		// first time a watched literal is processed on this level
		undo_.back() = data;
	}
}

void ClingoPropagator::registerUndoCheck(Solver& s) {
	if (uint32 dl = s.decisionLevel()) {
		registerUndo(s, set_bit(s.decision(dl).var(), CHECK_BIT));
	}
}

Constraint::PropResult ClingoPropagator::propagate(Solver& s, Literal p, uint32&) {
	registerUndo(s, static_cast<uint32>(trail_.size()));
	trail_.push_back(encodeLit(p));
	return PropResult(true, true);
}

void ClingoPropagator::undoLevel(Solver& s) {
	POTASSCO_REQUIRE(s.decisionLevel() == level_, "Invalid undo");
	uint32 beg = undo_.back();
	undo_.pop_back();

	if (test_bit(beg, CHECK_BIT) && call_->undoMode() == ClingoPropagatorUndo_t::Always) {
		assert(beg >= prop_);
		Potassco::LitSpan change = Potassco::toSpan<Potassco::Lit_t>();
		ScopedLock(call_->lock(), call_->propagator(), Inc(epoch_))->undo(Control(*this, s), change);
	}

	if (prop_ > beg) {
		Potassco::LitSpan change = Potassco::toSpan(&trail_[0] + beg, prop_ - beg);
		ScopedLock(call_->lock(), call_->propagator(), Inc(epoch_))->undo(Control(*this, s), change);
		prop_ = beg;
	}
	else if (level_ == propL_) {
		propL_ = UINT32_MAX;
		++epoch_;
	}

	if (front_ != INT32_MAX)
		front_ = -1;

	if (!test_bit(beg, CHECK_BIT))
		trail_.resize(beg);

	if (!undo_.empty()) {
		uint32 prev = undo_.back();
		if (test_bit(prev, CHECK_BIT)) {
			prev = clear_bit(prev, CHECK_BIT);
		}
		else {
			POTASSCO_ASSERT(prev < trail_.size());
			prev = decodeLit(trail_[prev]).var();
		}
		level_ = s.level(prev);
	}
	else {
		level_ = 0;
	}
}

bool ClingoPropagator::propagateFixpoint(Clasp::Solver& s, Clasp::PostPropagator*) {
	POTASSCO_REQUIRE(prop_ <= trail_.size(), "Invalid propagate");
	if (!s.sharedContext()->frozen())
		return true;

	for (Control ctrl(*this, s, state_prop); prop_ != trail_.size() || front_ < (int32)s.numAssignedVars();) {
		if (prop_ != trail_.size()) {
			// create copy because trail might change during call to user propagation
			temp_.assign(trail_.begin() + prop_, trail_.end());
			POTASSCO_REQUIRE(s.level(decodeLit(temp_[0]).var()) == s.decisionLevel(), "Propagate must be called on each level");
			prop_ = static_cast<uint32>(trail_.size());
			ScopedLock(call_->lock(), call_->propagator(), Inc(epoch_))->propagate(ctrl, Potassco::toSpan(temp_));
		}
		else {
			registerUndoCheck(s);
			front_ = (int32)s.numAssignedVars();
			ScopedLock(call_->lock(), call_->propagator(), Inc(epoch_))->check(ctrl);
		}
		if (!addClause(s, state_prop) || (s.queueSize() && !s.propagateUntil(this))) {
			return false;
		}
	}
	return true;
}

void ClingoPropagator::toClause(Solver& s, const Potassco::LitSpan& clause, Potassco::Clause_t prop) {
	POTASSCO_REQUIRE(todo_.empty(), "Assignment not propagated");
	Literal max;
	LitVec& mem = todo_.mem;
	for (const Potassco::Lit_t* it = Potassco::begin(clause); it != Potassco::end(clause); ++it) {
		Literal p = decodeLit(*it);
		if (max < p) { max = p; }
		mem.push_back(p);
	}
	if (aux_ < max) { aux_ = max; }
	if ((Potassco::Clause_t::isVolatile(prop) || s.auxVar(max.var())) && !isSentinel(s.sharedContext()->stepLiteral())) {
		mem.push_back(~s.sharedContext()->stepLiteral());
		POTASSCO_REQUIRE(s.value(mem.back().var()) != value_free || s.decisionLevel() == 0, "Step literal must be assigned on level 1");
	}
	todo_.clause = ClauseCreator::prepare(s, mem, Clasp::ClauseCreator::clause_force_simplify, Constraint_t::Other);
	todo_.flags  = ccFlags_s[int(Potassco::Clause_t::isStatic(prop))];
	if (mem.empty()) {
		mem.push_back(lit_false());
	}
}
bool ClingoPropagator::addClause(Solver& s, uint32 st) {
	if (s.hasConflict()) {
		POTASSCO_REQUIRE(todo_.empty(), "Assignment not propagated");
		return false;
	}
	if (todo_.empty())   { return true; }
	const ClauseRep& clause = todo_.clause;
	Literal w0 = clause.size > 0 ? clause.lits[0] : lit_false();
	Literal w1 = clause.size > 1 ? clause.lits[1] : lit_false();
	uint32  cs = ClauseCreator::status(s, clause);
	if (cs & (ClauseCreator::status_unit|ClauseCreator::status_unsat)) {
		uint32 dl = (cs & ClauseCreator::status_unsat) ? s.level(w0.var()) : s.level(w1.var());
		if (dl < s.decisionLevel() && s.isUndoLevel()) {
			if ((st & state_ctrl) != 0u) { return false; }
			if ((st & state_prop) != 0u) { ClingoPropagator::reset(); cancelPropagation(); }
			s.undoUntil(dl);
		}
	}
	bool local = (todo_.flags & ClauseCreator::clause_no_add) != 0;
	if (!s.isFalse(w0) || local || s.force(w0, this)) {
		ClauseCreator::Result res = ClauseCreator::create(s, clause, todo_.flags);
		if (res.local && local) { db_.push_back(res.local); }
	}
	todo_.clear();
	return !s.hasConflict();
}

void ClingoPropagator::reason(Solver&, Literal p, LitVec& r) {
	if (!todo_.empty() && todo_.mem[0] == p) {
		for (LitVec::const_iterator it = todo_.mem.begin() + 1, end = todo_.mem.end(); it != end; ++it) {
			r.push_back(~*it);
		}
	}
}

bool ClingoPropagator::simplify(Solver& s, bool) {
	if (!s.validVar(aux_.var())) {
		ClauseDB::size_type i, j, end = db_.size();
		LitVec cc;
		Var last = s.numVars();
		aux_ = lit_true();
		for (i = j = 0; i != end; ++i) {
			db_[j++] = db_[i];
			ClauseHead* c = db_[i]->clause();
			if (c && c->aux()) {
				cc.clear();
				c->toLits(cc);
				Literal x = *std::max_element(cc.begin(), cc.end());
				if (x.var() > last) {
					c->destroy(&s, true);
					--j;
				}
				else if (aux_ < x) { aux_ = x; }
			}
		}
		db_.erase(db_.begin()+j, db_.end());
	}
	simplifyDB(s, db_, false);
	return false;
}

bool ClingoPropagator::isModel(Solver& s) {
	POTASSCO_REQUIRE(prop_ == trail_.size(), "Assignment not propagated");
	if (call_->checkMode() & ClingoPropagatorCheck_t::Total) {
		front_ = -1;
		s.propagateFrom(this);
		front_ = (call_->checkMode() & ClingoPropagatorCheck_t::Fixpoint) ? front_ : INT32_MAX;
		return !s.hasConflict() && s.numFreeVars() == 0;
	}
	return true;
}
/////////////////////////////////////////////////////////////////////////////////////////
// ClingoPropagatorInit
/////////////////////////////////////////////////////////////////////////////////////////
ClingoPropagatorInit::Change::Change(Lit_t p, Action a)
	: lit(p), sId(-1), action(static_cast<int16>(a)) {}
ClingoPropagatorInit::Change::Change(Lit_t p, Action a, uint32 sId)
	: lit(p), sId(static_cast<int16>(sId)), action(static_cast<int16>(a)) {}
bool ClingoPropagatorInit::Change::operator<(const Change& rhs) const {
	int cmp = std::abs(lit) - std::abs(rhs.lit);
	return cmp != 0 ? cmp < 0 : lit < rhs.lit;
}
uint64 ClingoPropagatorInit::Change::solverMask() const {
	return static_cast<uint32_t>(sId) > 63 ? UINT64_MAX : bit_mask<uint64>(static_cast<uint32>(sId));
}
void ClingoPropagatorInit::Change::apply(Potassco::AbstractSolver& s) const {
	switch (action) {
		case AddWatch:    s.addWatch(lit);    break;
		case RemoveWatch: s.removeWatch(lit); break;
		default: break;
	}
}

struct ClingoPropagatorInit::History : POTASSCO_EXT_NS::unordered_map<Potassco::Lit_t, uint64>{
	void add(const Change& change) {
		if (change.action == AddWatch) {
			std::pair<iterator, bool> x = insert(value_type(change.lit, 0));
			x.first->second |= change.solverMask();
		}
		else if (change.action == RemoveWatch) {
			iterator it = find(change.lit);
			if (it != end() && (it->second &= ~change.solverMask()) == 0) {
				erase(it);
			}
		}
	}
};

ClingoPropagatorInit::ClingoPropagatorInit(Potassco::AbstractPropagator& cb, ClingoPropagatorLock* lock, CheckType m)
	: prop_(&cb), lock_(lock), history_(0), step_(1), check_(m), undo_(ClingoPropagatorUndo_t::Default) {
}
ClingoPropagatorInit::~ClingoPropagatorInit()         { delete history_; }
bool ClingoPropagatorInit::applyConfig(Solver& s)     { return s.addPost(new ClingoPropagator(this)); }
void ClingoPropagatorInit::prepare(SharedContext& ctx){
	std::stable_sort(changes_.begin(), changes_.end());
	for (ChangeList::const_iterator it = changes_.begin(), end = changes_.end(); it != end;) {
		Lit_t lit = it->lit;
		uint64_t addWatch = 0;
		bool     freeze   = false;
		do {
			switch (it->action) {
				case AddWatch:    addWatch |=  it->solverMask(); break;
				case RemoveWatch: addWatch &= ~it->solverMask(); break;
				case FreezeLit:   freeze = true; break;
				default: break;
			}
		} while (++it != end && it->lit == lit);
		if (freeze || addWatch)
			ctx.setFrozen(decodeVar(lit), true);
	}
}
void ClingoPropagatorInit::unfreeze(SharedContext&)   {
	if (history_) {
		for (ChangeList::const_iterator it = changes_.begin(), end = changes_.end(); it != end; ++it) {
			history_->add(*it);
		}
	}
	ChangeList().swap(changes_);
	++step_;
}

void ClingoPropagatorInit::freezeLit(Literal lit) {
	changes_.push_back(Change(encodeLit(lit), FreezeLit, 64));
}

Potassco::Lit_t ClingoPropagatorInit::addWatch(Literal lit) {
	changes_.push_back(Change(encodeLit(lit), AddWatch));
	return changes_.back().lit;
}

Potassco::Lit_t ClingoPropagatorInit::addWatch(uint32 sId, Literal lit) {
	POTASSCO_REQUIRE(sId < 64, "Invalid solver id");
	changes_.push_back(Change(encodeLit(lit), AddWatch, sId));
	return changes_.back().lit;
}

void ClingoPropagatorInit::removeWatch(Literal lit) {
	changes_.push_back(Change(encodeLit(lit), RemoveWatch));
}

void ClingoPropagatorInit::removeWatch(uint32 sId, Literal lit) {
	POTASSCO_REQUIRE(sId < 64, "Invalid solver id");
	changes_.push_back(Change(encodeLit(lit), RemoveWatch, sId));
}

uint32 ClingoPropagatorInit::init(uint32 lastStep, Potassco::AbstractSolver& s) {
	POTASSCO_REQUIRE(s.id() < 64, "Invalid solver id");
	int16 sId = static_cast<int16>(s.id());
	if (history_ && (step_ - lastStep) > 1) {
		for (History::const_iterator it = history_->begin(), end = history_->end(); it != end; ++it) {
			if (test_bit(it->second, sId)) { Change(it->first, AddWatch, sId).apply(s); }
		}
	}
	ChangeList changesForSolver;
	bool isSorted = true;
	for (ChangeList::const_iterator it = changes_.begin(), end = changes_.end(); it != end; ++it) {
		if (it->sId < 0 || it->sId == sId) {
			isSorted = isSorted && (changesForSolver.empty() || !(*it < changesForSolver.back()));
			changesForSolver.push_back(*it);
		}
	}
	if (!isSorted)
		std::stable_sort(changesForSolver.begin(), changesForSolver.end());
	for (ChangeList::const_iterator it = changesForSolver.begin(), end = changesForSolver.end(); it != end; ++it) {
		Lit_t lit = it->lit;
		// skip all but the last change for a given literal
		while ((it + 1) != end && (it + 1)->lit == lit) { ++it; }
		it->apply(s);
	}
	return step_;
}

void ClingoPropagatorInit::enableClingoPropagatorCheck(CheckType checkMode) {
	check_ = checkMode;
}

void ClingoPropagatorInit::enableClingoPropagatorUndo(UndoType undoMode) {
	undo_ = undoMode;
}

void ClingoPropagatorInit::enableHistory(bool b) {
	if (!b)             { delete history_; history_ = 0; }
	else if (!history_) { history_ = new History(); }
}

/////////////////////////////////////////////////////////////////////////////////////////
// ClingoHeuristic
/////////////////////////////////////////////////////////////////////////////////////////
ClingoHeuristic::ClingoHeuristic(Potassco::AbstractHeuristic& clingoHeuristic, DecisionHeuristic* claspHeuristic, ClingoPropagatorLock* lock)
	: clingo_(&clingoHeuristic)
	, clasp_(claspHeuristic)
	, lock_(lock) {}

Literal ClingoHeuristic::doSelect(Solver& s) {
	typedef Scoped<Potassco::AbstractHeuristic, ClingoPropagatorLock, &ClingoPropagatorLock::lock, &ClingoPropagatorLock::unlock, Nop> ScopedLock;
	Literal fallback = clasp_->doSelect(s);
	if (s.hasConflict())
		return fallback;

	ClingoAssignment assignment(s);
	Potassco::Lit_t lit = ScopedLock(lock_, clingo_)->decide(s.id(), assignment, encodeLit(fallback));
	Literal decision = lit != 0 ? decodeLit(lit) : fallback;
	return s.validVar(decision.var()) && !s.isFalse(decision) ? decision : fallback;
}

void ClingoHeuristic::startInit(const Solver& s)    { clasp_->startInit(s); }
void ClingoHeuristic::endInit(Solver& s)            { clasp_->endInit(s); }
void ClingoHeuristic::detach(Solver& s)             { if (clasp_.is_owner()) { clasp_->detach(s); } }
void ClingoHeuristic::setConfig(const HeuParams& p) { clasp_->setConfig(p); }
void ClingoHeuristic::newConstraint(const Solver& s, const Literal* p, LitVec::size_type sz, ConstraintType t) {
	clasp_->newConstraint(s, p, sz, t);
}

void ClingoHeuristic::updateVar(const Solver& s, Var v, uint32 n)                   { clasp_->updateVar(s, v, n); }
void ClingoHeuristic::simplify(const Solver& s, LitVec::size_type st)               { clasp_->simplify(s, st); }
void ClingoHeuristic::undoUntil(const Solver& s, LitVec::size_type st)              { clasp_->undoUntil(s, st); }
void ClingoHeuristic::updateReason(const Solver& s, const LitVec& x, Literal r)     { clasp_->updateReason(s, x, r); }
bool ClingoHeuristic::bump(const Solver& s, const WeightLitVec& w, double d)        { return clasp_->bump(s, w, d); }
Literal ClingoHeuristic::selectRange(Solver& s, const Literal* f, const Literal* l) { return clasp_->selectRange(s, f, l); }


DecisionHeuristic* ClingoHeuristic::fallback() const { return clasp_.get();  }

ClingoHeuristic::Factory::Factory(Potassco::AbstractHeuristic& clingoHeuristic, ClingoPropagatorLock* lock)
	: clingo_(&clingoHeuristic)
	, lock_(lock) {}

DecisionHeuristic* ClingoHeuristic::Factory::create(Heuristic_t::Type t, const HeuParams& p) {
	return new ClingoHeuristic(*clingo_, Heuristic_t::create(t, p), lock_);
}

}
