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
#include <clasp/enumerator.h>
#include <clasp/solver.h>
#include <clasp/util/multi_queue.h>
#include <clasp/clause.h>
namespace Clasp {
/////////////////////////////////////////////////////////////////////////////////////////
// Enumerator - Shared Queue / Thread Queue
/////////////////////////////////////////////////////////////////////////////////////////
class Enumerator::SharedQueue : public mt::MultiQueue<SharedLiterals*, void (*)(SharedLiterals*)> {
public:
	typedef mt::MultiQueue<SharedLiterals*, void (*)(SharedLiterals*)> BaseType;
	explicit SharedQueue(uint32 m) : BaseType(m, releaseLits) { reserve(m + 1); }
	bool pushRelaxed(SharedLiterals* clause)  { unsafePublish(clause); return true; }
	static void releaseLits(SharedLiterals* x){ x->release(); }
};
class Enumerator::ThreadQueue {
public:
	explicit ThreadQueue(SharedQueue& q) : queue_(&q) { tail_ = q.addThread(); }
	bool         pop(SharedLiterals*& out){ return queue_->tryConsume(tail_, out); }
	ThreadQueue* clone()                  { return new ThreadQueue(*queue_); }
private:
	Enumerator::SharedQueue*          queue_;
	Enumerator::SharedQueue::ThreadId tail_;
};
/////////////////////////////////////////////////////////////////////////////////////////
// EnumerationConstraint
/////////////////////////////////////////////////////////////////////////////////////////
EnumerationConstraint::EnumerationConstraint() : mini_(0), root_(0), state_(0), upMode_(0), heuristic_(0), disjoint_(false)  {
	setDisjoint(false);
}
EnumerationConstraint::~EnumerationConstraint() { }
void EnumerationConstraint::init(Solver& s, SharedMinimizeData* m, QueuePtr p) {
	mini_ = 0;
	queue_ = p;
	upMode_ = value_false;
	heuristic_ = 0;
	if (m) {
		OptParams opt = s.sharedContext()->configuration()->solver(s.id()).opt;
		mini_ = m->attach(s, opt);
		if (optimize()) {
			if   (opt.type != OptParams::type_bb) { upMode_ |= value_true; }
			else { heuristic_ |= 1; }
		}
		if (opt.hasOption(OptParams::heu_sign)) {
			for (const WeightLiteral* it = m->lits; !isSentinel(it->first); ++it) {
				s.setPref(it->first.var(), ValueSet::pref_value, falseValue(it->first));
			}
		}
		if (opt.hasOption(OptParams::heu_model)) { heuristic_ |= 2; }
	}
}
bool EnumerationConstraint::valid(Solver& s)         { return !optimize() || mini_->valid(s); }
void EnumerationConstraint::add(Constraint* c)       { if (c) { nogoods_.push_back(c); } }
bool EnumerationConstraint::integrateBound(Solver& s){ return !mini_ || mini_->integrate(s); }
bool EnumerationConstraint::optimize() const         { return mini_ && mini_->shared()->optimize(); }
void EnumerationConstraint::setDisjoint(bool x)      { disjoint_ = x; }
Constraint* EnumerationConstraint::cloneAttach(Solver& s) {
	EnumerationConstraint* c = clone();
	POTASSCO_REQUIRE(c != 0, "Cloning not supported by Enumerator");
	c->init(s, mini_ ? const_cast<SharedMinimizeData*>(mini_->shared()) : 0, queue_.get() ? queue_->clone() : 0);
	return c;
}
void EnumerationConstraint::end(Solver& s) {
	if (mini_) { mini_->relax(s, disjointPath()); }
	state_ = 0;
	setDisjoint(false);
	next_.clear();
	if (s.rootLevel() > root_) { s.popRootLevel(s.rootLevel() - root_); }
}
bool EnumerationConstraint::start(Solver& s, const LitVec& path, bool disjoint) {
	state_ = 0;
	root_  = s.rootLevel();
	setDisjoint(disjoint);
	if (s.pushRoot(path, true)) {
		integrateBound(s);
		integrateNogoods(s);
		return true;
	}
	return false;
}
bool EnumerationConstraint::update(Solver& s) {
	ValueRep st = state();
	if (st == value_true) {
		if (s.restartOnModel()) { s.undoUntil(0); }
		if (optimize())         { s.strengthenConditional(); }
	}
	else if (st == value_false && !s.pushRoot(next_)) {
		if (!s.hasConflict()) { s.setStopConflict(); }
		return false;
	}
	state_ = 0;
	next_.clear();
	do {
		if (!s.hasConflict() && doUpdate(s) && integrateBound(s) && integrateNogoods(s)) {
			if (st == value_true) { modelHeuristic(s); }
			return true;
		}
	} while (st != value_free && s.hasConflict() && s.resolveConflict());
	return false;
}
bool EnumerationConstraint::integrateNogoods(Solver& s) {
	if (!queue_.get() || s.hasConflict()) { return !s.hasConflict(); }
	const uint32 f = ClauseCreator::clause_no_add | ClauseCreator::clause_no_release | ClauseCreator::clause_explicit;
	for (SharedLiterals* clause; queue_->pop(clause); ) {
		ClauseCreator::Result res = ClauseCreator::integrate(s, clause, f);
		if (res.local) { add(res.local);}
		if (!res.ok()) { return false;  }
	}
	return true;
}
void EnumerationConstraint::destroy(Solver* s, bool x) {
	if (mini_) { mini_->destroy(s, x); mini_ = 0; }
	queue_ = 0;
	Clasp::destroyDB(nogoods_, s, x);
	Constraint::destroy(s, x);
}
bool EnumerationConstraint::simplify(Solver& s, bool reinit) {
	if (mini_) { mini_->simplify(s, reinit); }
	simplifyDB(s, nogoods_, reinit);
	return false;
}

bool EnumerationConstraint::commitModel(Enumerator& ctx, Solver& s) {
	if (state() == value_true)          { return !next_.empty() && (s.satPrepro()->extendModel(s.model, next_), true); }
	if (mini_ && !mini_->handleModel(s)){ return false; }
	if (!ctx.tentative())               { doCommitModel(ctx, s); }
	next_ = s.symmetric();
	state_ |= value_true;
	return true;
}
bool EnumerationConstraint::commitUnsat(Enumerator& ctx, Solver& s) {
	next_.clear();
	state_ |= value_false;
	if (mini_) {
		mini_->handleUnsat(s, !disjointPath(), next_);
	}
	if (!ctx.tentative()) {
		doCommitUnsat(ctx, s);
	}
	return !s.hasConflict() || s.decisionLevel() != s.rootLevel();
}
void EnumerationConstraint::modelHeuristic(Solver& s) {
	const bool full = heuristic_ > 1;
	const bool heuristic = full || (heuristic_ == 1 && s.queueSize() == 0 && s.decisionLevel() == s.rootLevel());
	if (optimize() && heuristic && s.propagate()) {
		for (const WeightLiteral* w = mini_->shared()->lits; !isSentinel(w->first); ++w) {
			if (s.value(w->first.var()) == value_free) {
				s.assume(~w->first);
				if (!full || !s.propagate()) { break; }
			}
		}
	}
}
/////////////////////////////////////////////////////////////////////////////////////////
// Enumerator
/////////////////////////////////////////////////////////////////////////////////////////
void Model::reset() { std::memset(this, 0, sizeof(Model)); }
Enumerator::Enumerator() : mini_(0), queue_(0) { model_.reset(); }
Enumerator::~Enumerator()                            { delete queue_; }
void Enumerator::setDisjoint(Solver& s, bool b)const { constraintRef(s).setDisjoint(b); }
void Enumerator::setIgnoreSymmetric(bool b)          { model_.sym = static_cast<uint32>(b == false); }
void Enumerator::end(Solver& s)                const { constraintRef(s).end(s); }
void Enumerator::doReset()                           {}
void Enumerator::reset() {
	if (mini_) { mini_ = 0; }
	if (queue_){ delete queue_; queue_ = 0; }
	model_.reset();
	model_.ctx  = this;
	model_.sym  = 1;
	model_.type = uint32(modelType());
	model_.sId  = 0;
	doReset();
}
int  Enumerator::init(SharedContext& ctx, OptMode oMode, int limit)  {
	ctx.master()->setEnumerationConstraint(0);
	reset();
	if (oMode != MinimizeMode_t::ignore){ mini_ = ctx.minimize(); }
	limit      = limit >= 0 ? limit : 1 - int(exhaustive());
	if (limit != 1) { ctx.setPreserveModels(true); }
	queue_     = new SharedQueue(ctx.concurrency());
	ConPtr c   = doInit(ctx, mini_, limit);
	bool cons  = model_.consequences();
	if      (tentative())        { model_.type = Model::Sat; }
	else if (cons && optimize()) { ctx.warn("Optimization: Consequences may depend on enumeration order."); }
	c->init(*ctx.master(), mini_, new ThreadQueue(*queue_));
	ctx.master()->setEnumerationConstraint(c);
	return limit;
}
Enumerator::ConRef Enumerator::constraintRef(const Solver& s) const {
	POTASSCO_ASSERT(s.enumerationConstraint(), "Solver not attached");
	return static_cast<ConRef>(*s.enumerationConstraint());
}
Enumerator::ConPtr Enumerator::constraint(const Solver& s) const {
	return static_cast<ConPtr>(s.enumerationConstraint());
}
bool Enumerator::start(Solver& s, const LitVec& path, bool disjointPath) const {
	return constraintRef(s).start(s, path, disjointPath);
}
ValueRep Enumerator::commit(Solver& s) {
	if      (s.hasConflict() && s.decisionLevel() == s.rootLevel())         { return commitUnsat(s) ? value_free : value_false; }
	else if (s.numFreeVars() == 0 && s.queueSize() == 0 && !s.hasConflict()){ return commitModel(s) ? value_true : value_free;  }
	return value_free;
}
bool Enumerator::commitModel(Solver& s) {
	assert(s.numFreeVars() == 0 && !s.hasConflict() && s.queueSize() == 0);
	if (constraintRef(s).commitModel(*this, s)) {
		s.stats.addModel(s.decisionLevel());
		++model_.num;
		model_.sId    = s.id();
		model_.values = &s.model;
		model_.costs  = 0;
		model_.up     = 0;
		if (minimizer()) {
			costs_.resize(minimizer()->numRules());
			std::transform(minimizer()->adjust(), minimizer()->adjust()+costs_.size(), minimizer()->sum(), costs_.begin(), std::plus<wsum_t>());
			model_.costs = &costs_;
		}
		return true;
	}
	return false;
}
bool Enumerator::commitSymmetric(Solver& s){ return model_.sym && !optimize() && commitModel(s); }
bool Enumerator::commitUnsat(Solver& s)    { return constraintRef(s).commitUnsat(*this, s); }
bool Enumerator::commitClause(const LitVec& clause)  const {
	return queue_ && queue_->pushRelaxed(SharedLiterals::newShareable(clause, Constraint_t::Other));
}
bool Enumerator::commitComplete() {
	if (enumerated()) {
		if (tentative()) {
			mini_->markOptimal();
			model_.opt = 1;
			model_.num = 0;
			model_.type= uint32(modelType());
			return false;
		}
		else if (model_.consequences() || (!model_.opt && optimize())) {
			model_.opt = uint32(optimize());
			model_.def = uint32(model_.consequences());
			model_.num = 1;
		}
	}
	return true;
}
bool Enumerator::update(Solver& s) const {
	return constraintRef(s).update(s);
}
bool Enumerator::supportsSplitting(const SharedContext& ctx) const {
	if (!optimize()) { return true; }
	const Configuration* config = ctx.configuration();
	bool ok = true;
	for (uint32 i = 0; i != ctx.concurrency() && ok; ++i) {
		if      (ctx.hasSolver(i) && constraint(*ctx.solver(i))){ ok = constraint(*ctx.solver(i))->minimizer()->supportsSplitting(); }
		else if (config && i < config->numSolver())             { ok = config->solver(i).opt.supportsSplitting(); }
	}
	return ok;
}
int Enumerator::unsatType() const {
	return !optimize() ? unsat_stop : unsat_cont;
}
Model& Enumerator::model() {
	return model_;
}
/////////////////////////////////////////////////////////////////////////////////////////
// EnumOptions
/////////////////////////////////////////////////////////////////////////////////////////
Enumerator* EnumOptions::createEnumerator(const EnumOptions& opts) {
	if      (opts.models())      { return createModelEnumerator(opts);}
	else if (opts.consequences()){ return createConsEnumerator(opts); }
	else                         { return nullEnumerator(); }
}
Enumerator* EnumOptions::nullEnumerator() {
	struct NullEnum : Enumerator {
		ConPtr doInit(SharedContext&, SharedMinimizeData*, int) {
			struct Constraint : public EnumerationConstraint {
				Constraint() : EnumerationConstraint() {}
				ConPtr      clone()          { return new Constraint(); }
				bool        doUpdate(Solver&){ return true; }
			};
			return new Constraint();
		}
	};
	return new NullEnum;
}

const char* modelType(const Model& m) {
	switch (m.type) {
		case Model::Sat     : return "Model";
		case Model::Brave   : return "Brave";
		case Model::Cautious: return "Cautious";
		case Model::User    : return "User";
		default: return 0;
	}
}

}
