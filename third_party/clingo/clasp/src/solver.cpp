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
#include <clasp/solver.h>
#include <clasp/clause.h>
#include POTASSCO_EXT_INCLUDE(unordered_set)
namespace Clasp {
namespace {
	typedef POTASSCO_EXT_NS::unordered_set<Constraint*> ConstraintSet;
	struct InSet {
		bool operator()(Constraint* c)        const { return set->find(c) != set->end(); }
		bool operator()(const ClauseWatch& w) const { return (*this)(w.head); }
		bool operator()(const GenericWatch&w) const { return (*this)(w.con);  }
		const ConstraintSet* set;
	};
}
DecisionHeuristic::~DecisionHeuristic() {}
static SelectFirst null_heuristic_g;
/////////////////////////////////////////////////////////////////////////////////////////
// CCMinRecursive
/////////////////////////////////////////////////////////////////////////////////////////
struct CCMinRecursive {
	enum State { state_open = 0, state_removable = 1, state_poison = 2 };
	uint32 encodeState(State st)     const { return open + uint32(st); }
	State  decodeState(uint32 epoch) const { return epoch <= open ? state_open : static_cast<State>(epoch - open); }
	void    push(Literal p) { todo.push_back(p); }
	Literal pop()           { Literal p = todo.back(); todo.pop_back(); return p; }
	LitVec todo;
	uint32 open;
};
/////////////////////////////////////////////////////////////////////////////////////////
// SelectFirst selection strategy
/////////////////////////////////////////////////////////////////////////////////////////
// selects the first free literal
Literal SelectFirst::doSelect(Solver& s) {
	for (Var i = 1; i <= s.numVars(); ++i) {
		if (s.value(i) == value_free) {
			return selectLiteral(s, i, 0);
		}
	}
	assert(!"SelectFirst::doSelect() - precondition violated!\n");
	return Literal();
}
/////////////////////////////////////////////////////////////////////////////////////////
// Dirty list
/////////////////////////////////////////////////////////////////////////////////////////
struct Solver::Dirty {
	static const std::size_t min_size = static_cast<std::size_t>(4);
	Dirty() : last(0) {}
	bool add(Literal p, WatchList& wl, Constraint* c) {
		if (wl.right_size() <= min_size) { return false; }
		uintp o = wl.left_size() > 0 ? reinterpret_cast<uintp>(wl.left_begin()->head) : 0;
		if (add(wl.right_begin()->con, o, c)) { dirty.push_left(p); }
		return true;
	}
	bool add(Literal p, WatchList& wl, ClauseHead* c) {
		if (wl.left_size() <= min_size) { return false; }
		uintp o = wl.right_size() > 0 ? reinterpret_cast<uintp>(wl.right_begin()->con) : 0;
		if (add(wl.left_begin()->head, o, c)) { dirty.push_left(p); }
		return true;
	}
	bool add(uint32 dl, ConstraintDB& wl, Constraint* c) {
		if (wl.size() <= min_size) { return false; }
		if (add(wl[0], 0, c)) { dirty.push_right(dl); }
		return true;
	}
	template <class T>
	bool add(T*& list, uintp other, Constraint* c) {
		other |= reinterpret_cast<uintp>(list);
		list = reinterpret_cast<T*>( set_bit(reinterpret_cast<uintp>(list), 0) );
		if (c != last) { cons.insert(last = c); }
		return !test_bit(other, 0);
	}
	template <class T>
	bool test_and_clear(T*& x) const {
		uintp old = reinterpret_cast<uintp>(x);
		return test_bit(old, 0) && (x = reinterpret_cast<T*>(clear_bit(old, 0))) != 0;
	}
	void cleanup(Watches& watches, DecisionLevels& levels) {
		InSet inCons = { &cons };
		const uint32 maxId = (uint32)watches.size();
		for (DirtyList::left_iterator it = dirty.left_begin(), end = dirty.left_end(); it != end; ++it) {
			uint32 id = it->id();
			if (id >= maxId)
				continue;
			WatchList& wl = watches[id];
			if (wl.left_size() && test_and_clear(wl.left_begin()->head)) { wl.shrink_left(std::remove_if(wl.left_begin(), wl.left_end(), inCons)); }
			if (wl.right_size()&& test_and_clear(wl.right_begin()->con)) { wl.shrink_right(std::remove_if(wl.right_begin(), wl.right_end(), inCons)); }
		}
		ConstraintDB* db = 0;
		for (DirtyList::right_iterator it = dirty.right_begin(), end = dirty.right_end(); it != end; ++it) {
			if (*it < levels.size() && !(db = levels[*it].undo)->empty() && test_and_clear(*db->begin())) {
				db->erase(std::remove_if(db->begin(), db->end(), inCons), db->end());
			}
		}
		dirty.clear();
		cons.clear();
		last = 0;
	}
	typedef bk_lib::left_right_sequence<Literal, uint32, 0> DirtyList;
	DirtyList     dirty;
	ConstraintSet cons;
	Constraint*   last;
};
/////////////////////////////////////////////////////////////////////////////////////////
// Solver: Construction/Destruction/Setup
/////////////////////////////////////////////////////////////////////////////////////////
#define FOR_EACH_POST(x, head) \
	for (PostPropagator** __r__ = (head), *x; (x = *__r__) != 0; __r__ = (x == *__r__) ? &x->next : __r__)

static PostPropagator* sent_list;
Solver::Solver(SharedContext* ctx, uint32 id)
	: shared_(ctx)
	, heuristic_(&null_heuristic_g, Ownership_t::Retain)
	, ccMin_(0)
	, postHead_(&sent_list)
	, undoHead_(0)
	, enum_(0)
	, memUse_(0)
	, lazyRem_(0)
	, ccInfo_(Constraint_t::Conflict)
	, dbIdx_(0)
	, lastSimp_(0)
	, shufSimp_(0)
	, initPost_(0){
	Var trueVar = assign_.addVar();
	assign_.setValue(trueVar, value_true);
	markSeen(trueVar);
	strategy_.id = id;
}

Solver::~Solver() {
	freeMem();
}

void Solver::freeMem() {
	std::for_each( constraints_.begin(), constraints_.end(), DestroyObject());
	std::for_each( learnts_.begin(), learnts_.end(), DestroyObject() );
	constraints_.clear();
	learnts_.clear();
	post_.clear();
	if (enum_) { enum_->destroy(); }
	resetHeuristic(0);
	PodVector<WatchList>::destruct(watches_);
	// free undo lists
	// first those still in use
	for (DecisionLevels::size_type i = 0; i != levels_.size(); ++i) {
		delete levels_[i].undo;
	}
	// then those in the free list
	for (ConstraintDB* x = undoHead_; x; ) {
		ConstraintDB* t = x;
		x = (ConstraintDB*)x->front();
		delete t;
	}
	delete ccMin_;
	ccMin_  = 0;
	memUse_ = 0;
}

SatPreprocessor*    Solver::satPrepro()     const { return shared_->satPrepro.get(); }
const SolveParams&  Solver::searchConfig()  const { return shared_->configuration()->search(id()); }

void Solver::reset() {
	SharedContext* myCtx = shared_;
	uint32         myId  = strategy_.id;
	this->~Solver();
	new (this) Solver(myCtx, myId);
}
void Solver::setHeuristic(DecisionHeuristic* h, Ownership_t::Type t) {
	POTASSCO_REQUIRE(h, "Heuristic must not be null");
	resetHeuristic(this, h, t);
}
void Solver::resetHeuristic(Solver* s, DecisionHeuristic* h, Ownership_t::Type t) {
	if (s && heuristic_.get()) { heuristic_->detach(*this); }
	if (!h) { h = &null_heuristic_g; t = Ownership_t::Retain; }
	HeuristicPtr(h, t).swap(heuristic_);
}
void Solver::resetConfig() {
	if (strategy_.hasConfig) {
		if (PostPropagator* pp = getPost(PostPropagator::priority_reserved_look)) { pp->destroy(this, true); }
		delete ccMin_;
		ccMin_ = 0;
	}
	strategy_.hasConfig = 0;
}
void Solver::startInit(uint32 numConsGuess, const SolverParams& params) {
	assert(!lazyRem_ && decisionLevel() == 0);
	if (watches_.empty()) {
		assign_.trail.reserve(shared_->numVars() + 2);
		watches_.reserve((shared_->numVars() + 2)<<1);
		assign_.reserve(shared_->numVars() + 2);
	}
	updateVars();
	// pre-allocate some memory
	constraints_.reserve(numConsGuess/2);
	levels_.reserve(25);
	if (undoHead_ == 0)  {
		for (uint32 i = 0; i != 25; ++i) {
			undoFree(new ConstraintDB(10));
		}
	}
	if (!popRootLevel(rootLevel())) { return; }
	if (!strategy_.hasConfig) {
		uint32 id           = this->id();
		uint32 hId          = strategy_.heuId; // remember active heuristic
		strategy_           = params;
		strategy_.id        = id; // keep id
		strategy_.hasConfig = 1;  // strategy is now "up to date"
		if      (!params.ccMinRec)  { delete ccMin_; ccMin_ = 0; }
		else if (!ccMin_)           { ccMin_ = new CCMinRecursive; }
		if (id == params.id || !shared_->seedSolvers()) {
			rng.srand(params.seed);
		}
		else {
			RNG x(14182940); for (uint32 i = 0; i != id; ++i) { x.rand(); }
			rng.srand(x.seed());
		}
		if (hId != params.heuId) { // heuristic has changed
			resetHeuristic(this);
		}
		else if (heuristic_.is_owner()) {
			heuristic_->setConfig(params.heuristic);
		}
	}
	if (heuristic_.get() == &null_heuristic_g) {
		heuristic_.reset(shared_->configuration()->heuristic(id()));
	}
	postHead_ = &sent_list; // disable post propagators during setup
	heuristic_->startInit(*this);
}

void Solver::updateVars() {
	if (numVars() > shared_->numVars()) {
		popVars(numVars() - shared_->numVars(), false, 0);
	}
	else {
		assign_.resize(shared_->numVars() + 1);
		watches_.resize(assign_.numVars()<<1);
	}
}

bool Solver::cloneDB(const ConstraintDB& db) {
	while (dbIdx_ < (uint32)db.size() && !hasConflict()) {
		if (Constraint* c = db[dbIdx_++]->cloneAttach(*this)) {
			constraints_.push_back(c);
		}
	}
	return !hasConflict();
}
bool Solver::preparePost() {
	if (hasConflict()) { return false; }
	if (initPost_ == 0){
		initPost_ = 1;
		FOR_EACH_POST(x, post_.head()) {
			if (!x->init(*this)) { return false; }
		}
	}
	return shared_->configuration()->addPost(*this);
}

bool Solver::endInit() {
	if (hasConflict()) { return false; }
	heuristic_->endInit(*this);
	if (strategy_.signFix) {
		for (Var v = 1; v <= numVars(); ++v) {
			Literal x = DecisionHeuristic::selectLiteral(*this, v, 0);
			setPref(v, ValueSet::user_value, x.sign() ? value_false : value_true);
		}
	}
	postHead_ = post_.head(); // enable all post propagators
	return propagate() && simplify();
}

bool Solver::endStep(uint32 top, const SolverParams& params) {
	initPost_ = 0; // defer calls to PostPropagator::init()
	if (!popRootLevel(rootLevel())) { return false; }
	popAuxVar();
	Literal x = shared_->stepLiteral();
	top = std::min(top, (uint32)lastSimp_);
	if (PostPropagator* pp = getPost(PostPropagator::priority_reserved_look)) {
		pp->destroy(this, true);
	}
	if ((value(x.var()) != value_free || force(~x)) && simplify() && this != shared_->master() && shared_->ok()) {
		Solver& m = *shared_->master();
		for (uint32 end = (uint32)assign_.trail.size(); top < end; ++top) {
			Literal u = assign_.trail[top];
			if (u.var() != x.var() && !m.force(u)) { break; }
		}
	}
	if (params.forgetLearnts())   { reduceLearnts(1.0f); }
	if (params.forgetHeuristic()) { resetHeuristic(this); }
	if (params.forgetSigns())     { resetPrefs(); }
	if (params.forgetActivities()){ resetLearntActivities(); }
	return true;
}

void Solver::add(Constraint* c) {
	constraints_.push_back(c);
}
bool Solver::add(const ClauseRep& c, bool isNew) {
	typedef ShortImplicationsGraph::ImpType ImpType;
	if (c.prep == 0) {
		return ClauseCreator::create(*this, c, ClauseCreator::clause_force_simplify).ok();
	}
	int added = 0;
	if (c.size > 1) {
		if (allowImplicit(c)) { added = shared_->addImp(static_cast<ImpType>(c.size), c.lits, c.info.type()); }
		else                  { return ClauseCreator::create(*this, c, ClauseCreator::clause_explicit).ok(); }
	}
	else {
		Literal u = c.size ? c.lits[0] : lit_false();
		uint32  ts= sizeVec(trail());
		force(u);
		added     = int(ts != trail().size());
	}
	if (added > 0 && isNew && c.info.learnt()) {
		stats.addLearnt(c.size, c.info.type());
		distribute(c.lits, c.size, c.info);
	}
	return !hasConflict();
}
bool Solver::addPost(PostPropagator* p, bool init) {
	post_.add(p);
	return !init || p->init(*this);
}
bool Solver::addPost(PostPropagator* p)   { return addPost(p, initPost_ != 0); }
void Solver::removePost(PostPropagator* p){ post_.remove(p); }
PostPropagator* Solver::getPost(uint32 prio) const { return post_.find(prio); }
uint32 Solver::receive(SharedLiterals** out, uint32 maxOut) const {
	if (shared_->distributor.get()) {
		return shared_->distributor->receive(*this, out, maxOut);
	}
	return 0;
}
SharedLiterals* Solver::distribute(const Literal* lits, uint32 size, const ConstraintInfo& extra) {
	if (shared_->distributor.get() && !extra.aux() && (size <= 3 || shared_->distributor->isCandidate(size, extra.lbd(), extra.type()))) {
		uint32 initialRefs = shared_->concurrency() - (size <= ClauseHead::MAX_SHORT_LEN || !shared_->physicalShare(extra.type()));
		SharedLiterals* x  = SharedLiterals::newShareable(lits, size, extra.type(), initialRefs);
		shared_->distributor->publish(*this, x);
		stats.addDistributed(extra.lbd(), extra.type());
		return initialRefs == shared_->concurrency() ? x : 0;
	}
	return 0;
}
void Solver::setEnumerationConstraint(Constraint* c) {
	if (enum_) enum_->destroy(this, true);
	enum_ = c;
}

uint32 Solver::numConstraints() const {
	return static_cast<uint32>(constraints_.size())
		+ (shared_ ? shared_->numBinary()+shared_->numTernary() : 0);
}

Var Solver::pushAuxVar() {
	assert(!lazyRem_);
	Var aux = assign_.addVar();
	setPref(aux, ValueSet::def_value, value_false);
	watches_.insert(watches_.end(), 2, WatchList());
	heuristic_->updateVar(*this, aux, 1);
	return aux;
}

void Solver::acquireProblemVar(Var var) {
	if (validVar(var) || shared_->frozen() || numProblemVars() <= numVars() || !shared_->ok())
		return;
	shared_->startAddConstraints();
}

void Solver::popAuxVar(uint32 num, ConstraintDB* auxCons) {
	num = numVars() >= shared_->numVars() ? std::min(numVars() - shared_->numVars(), num) : 0;
	if (!num) { return; }
	shared_->report("removing aux vars", this);
	Dirty dirty;
	lazyRem_ = &dirty;
	popVars(num, true, auxCons);
	lazyRem_ = 0;
	shared_->report("removing aux watches", this);
	dirty.cleanup(watches_, levels_);
}
Literal Solver::popVars(uint32 num, bool popLearnt, ConstraintDB* popAux) {
	Literal pop = posLit(assign_.numVars() - num);
	uint32  dl  = decisionLevel() + 1;
	for (ImpliedList::iterator it = impliedLits_.begin(); it != impliedLits_.end(); ++it) {
		if (!(it->lit < pop)) { dl = std::min(dl, it->level); }
	}
	for (Var v = pop.var(), end = pop.var()+num; v != end; ++v) {
		if (value(v) != value_free) { dl = std::min(dl, level(v)); }
	}
	// 1. remove aux vars from assignment and watch lists
	if (dl > rootLevel()) {
		undoUntil(dl-1, undo_pop_proj_level);
	}
	else {
		popRootLevel((rootLevel() - dl) + 1);
		if (dl == 0) { // top-level has aux vars - cleanup manually
			uint32 j = shared_->numUnary();
			uint32 nUnits = assign_.units(), nFront = assign_.front, nSimps = lastSimp_;
			for (uint32 i = j, end = sizeVec(assign_.trail), endUnits = nUnits, endFront = nFront, endSimps = lastSimp_; i != end; ++i) {
				if (assign_.trail[i] < pop) { assign_.trail[j++] = assign_.trail[i]; }
				else {
					nUnits -= (i < endUnits);
					nFront -= (i < endFront);
					nSimps -= (i < endSimps);
				}
			}
			shrinkVecTo(assign_.trail, j);
			assign_.front = nFront;
			assign_.setUnits(nUnits);
			lastSimp_ = nSimps;
		}
	}
	for (uint32 n = num; n--;) {
		watches_.back().clear(true);
		watches_.pop_back();
		watches_.back().clear(true);
		watches_.pop_back();
	}
	// 2. remove learnt constraints over aux
	if (popLearnt) {
		shared_->report("removing aux constraints", this);
		ConstraintDB::size_type i, j, end = learnts_.size();
		LitVec cc;
		for (i = j = 0; i != end; ++i) {
			learnts_[j++] = learnts_[i];
			ClauseHead* c = learnts_[i]->clause();
			if (c && c->aux()) {
				cc.clear();
				c->toLits(cc);
				if (std::find_if(cc.begin(), cc.end(), std::not1(std::bind2nd(std::less<Literal>(), pop))) != cc.end()) {
					c->destroy(this, true);
					--j;
				}
			}
		}
		learnts_.erase(learnts_.begin()+j, learnts_.end());
	}
	if (popAux) { destroyDB(*popAux); }
	// 3. remove vars from solver and heuristic
	assign_.resize(assign_.numVars()-num);
	if (!validVar(tag_.var())) { tag_ = lit_true(); }
	heuristic_->updateVar(*this, pop.var(), num);
	return pop;
}

bool Solver::pushRoot(const LitVec& path, bool pushStep) {
	// make sure we are on the current (fully propagated) root level
	if (!popRootLevel(0) || !simplify() || !propagate()) { return false; }
	// push path
	if (pushStep && !pushRoot(shared_->stepLiteral())) { return false; }
	stats.addPath(path.size());
	for (LitVec::const_iterator it = path.begin(), end = path.end(); it != end; ++it) {
		if (!pushRoot(*it)) { return false; }
	}
	ccInfo_.setActivity(1);
	return true;
}

bool Solver::pushRoot(Literal x) {
	if (hasConflict())                 { return false; }
	if (decisionLevel()!= rootLevel()) { popRootLevel(0);  }
	if (queueSize() && !propagate())   { return false;    }
	if (value(x.var()) != value_free)  { return isTrue(x);}
	assume(x); --stats.choices;
	pushRootLevel();
	return propagate();
}

bool Solver::popRootLevel(uint32 n, LitVec* popped, bool aux)  {
	clearStopConflict();
	uint32 newRoot = levels_.root - std::min(n, rootLevel());
	if (popped && newRoot < rootLevel()) {
		for (uint32 i = newRoot+1; i <= rootLevel(); ++i) {
			Literal x = decision(i);
			if (aux || !auxVar(x.var())) { popped->push_back(x); }
		}
	}
	if (n) { ccInfo_.setActivity(1); }
	levels_.root = newRoot;
	levels_.flip = rootLevel();
	levels_.mode = 0;
	impliedLits_.front = 0;
	bool tagActive     = isTrue(tagLiteral());
	// go back to new root level and re-assert still implied literals
	undoUntil(rootLevel(), undo_pop_proj_level);
	if (tagActive && !isTrue(tagLiteral())) {
		removeConditional();
	}
	return !hasConflict();
}

bool Solver::clearAssumptions()  {
	return popRootLevel(rootLevel())
		&& simplify();
}

void Solver::clearStopConflict() {
	if (hasStopConflict()) {
		levels_.root  = conflict_[1].rep();
		levels_.flip  = conflict_[2].rep();
		assign_.front = conflict_[3].rep();
		conflict_.clear();
	}
}

void Solver::setStopConflict() {
	if (!hasConflict()) {
		// we use the nogood {FALSE} to represent the unrecoverable conflict -
		// note that {FALSE} can otherwise never be a violated nogood because
		// TRUE is always true in every solver
		conflict_.push_back(lit_false());
		// remember the current root-level
		conflict_.push_back(Literal::fromRep(rootLevel()));
		// remember the current bt-level
		conflict_.push_back(Literal::fromRep(backtrackLevel()));
		// remember the current propagation queue
		conflict_.push_back(Literal::fromRep(assign_.front));
	}
	// artificially increase root level -
	// this way, the solver is prevented from resolving the conflict
	pushRootLevel(decisionLevel());
}

void Solver::copyGuidingPath(LitVec& gpOut) {
	uint32 aux = rootLevel()+1;
	gpOut.clear();
	for (uint32 i = 1, end = rootLevel()+1; i != end; ++i) {
		Literal x = decision(i);
		if      (!auxVar(x.var())) { gpOut.push_back(x); }
		else if (i < aux)          { aux = i; }
	}
	for (ImpliedList::iterator it = impliedLits_.begin(); it != impliedLits_.end(); ++it) {
		if (it->level <= rootLevel() && (it->ante.ante().isNull() || it->level < aux) && !auxVar(it->lit.var())) {
			gpOut.push_back(it->lit);
		}
	}
}
bool Solver::splittable() const {
	if (decisionLevel() == rootLevel() || frozenLevel(rootLevel()+1)) { return false; }
	if (numAuxVars()) { // check if gp would contain solver local aux var
		uint32 minAux = rootLevel() + 2;
		for (uint32 i = 1; i != minAux; ++i) {
			if (auxVar(decision(i).var()) && decision(i) != tag_) { return false; }
		}
		for (ImpliedList::iterator it = impliedLits_.begin(); it != impliedLits_.end(); ++it) {
			if (it->ante.ante().isNull() && it->level < minAux && auxVar(it->lit.var()) && it->lit != tag_) { return false; }
		}
	}
	return true;
}
bool Solver::split(LitVec& out) {
	if (!splittable()) { return false; }
	copyGuidingPath(out);
	pushRootLevel();
	out.push_back(~decision(rootLevel()));
	stats.addSplit();
	return true;
}
/////////////////////////////////////////////////////////////////////////////////////////
// Solver: Watch management
////////////////////////////////////////////////////////////////////////////////////////
uint32 Solver::numWatches(Literal p) const {
	assert( validVar(p.var()) );
	if (!validWatch(p)) return 0;
	uint32 n = static_cast<uint32>(watches_[p.id()].size());
	if (!auxVar(p.var())){
		n += shared_->shortImplications().numEdges(p);
	}
	return n;
}

bool Solver::hasWatch(Literal p, Constraint* c) const {
	return getWatch(p, c) != 0;
}

bool Solver::hasWatch(Literal p, ClauseHead* h) const {
	if (!validWatch(p)) return false;
	const WatchList& pList = watches_[p.id()];
	return !pList.empty() && std::find_if(pList.left_begin(), pList.left_end(), ClauseWatch::EqHead(h)) != pList.left_end();
}

GenericWatch* Solver::getWatch(Literal p, Constraint* c) const {
	if (!validWatch(p)) return 0;
	const WatchList& pList = watches_[p.id()];
	WatchList::const_right_iterator it = std::find_if(pList.right_begin(), pList.right_end(), GenericWatch::EqConstraint(c));
	return it != pList.right_end()
		? &const_cast<GenericWatch&>(*it)
		: 0;
}

void Solver::removeWatch(const Literal& p, Constraint* c) {
	if (!validWatch(p)) { return; }
	WatchList& pList = watches_[p.id()];
	if (!lazyRem_ || !lazyRem_->add(p, pList, c)) {
		pList.erase_right(std::find_if(pList.right_begin(), pList.right_end(), GenericWatch::EqConstraint(c)));
	}
}

void Solver::removeWatch(const Literal& p, ClauseHead* h) {
	if (!validWatch(p)) { return; }
	WatchList& pList = watches_[p.id()];
	if (!lazyRem_ || !lazyRem_->add(p, pList, h)) {
		pList.erase_left(std::find_if(pList.left_begin(), pList.left_end(), ClauseWatch::EqHead(h)));
	}
}

bool Solver::removeUndoWatch(uint32 dl, Constraint* c) {
	assert(dl != 0 && dl <= decisionLevel() );
	if (levels_[dl-1].undo) {
		ConstraintDB& uList = *levels_[dl-1].undo;
		if (!lazyRem_ || !lazyRem_->add(dl - 1, uList, c)) {
			ConstraintDB::iterator it = std::find(uList.begin(), uList.end(), c);
			if (it != uList.end()) {
				*it = uList.back();
				uList.pop_back();
				return true;
			}
		}
	}
	return false;
}
void Solver::destroyDB(ConstraintDB& db) {
	if (!db.empty()) {
		Dirty dirty;
		if (!lazyRem_) { lazyRem_ = &dirty; }
		for (ConstraintDB::const_iterator it = db.begin(), end = db.end(); it != end; ++it) {
			(*it)->destroy(this, true);
		}
		db.clear();
		if (lazyRem_ == &dirty) {
			lazyRem_ = 0;
			dirty.cleanup(watches_, levels_);
		}
	}
}
/////////////////////////////////////////////////////////////////////////////////////////
// Solver: Basic DPLL-functions
////////////////////////////////////////////////////////////////////////////////////////

// removes all satisfied binary and ternary clauses as well
// as all constraints for which Constraint::simplify returned true.
bool Solver::simplify() {
	if (decisionLevel() != 0) return true;
	if (hasConflict())        return false;
	if (lastSimp_ != (uint32)assign_.trail.size()) {
		uint32 old = lastSimp_;
		if (!simplifySAT()) { return false; }
		assert(lastSimp_ == (uint32)assign_.trail.size());
		heuristic_->simplify(*this, old);
	}
	if (shufSimp_) { simplifySAT(); }
	return true;
}
Var  Solver::pushTagVar(bool pushToRoot) {
	if (isSentinel(tag_)) { tag_ = posLit(pushAuxVar()); }
	if (pushToRoot)       { pushRoot(tag_); }
	return tag_.var();
}
void Solver::removeConditional() {
	Literal p = ~tagLiteral();
	if (!isSentinel(p)) {
		ConstraintDB::size_type i, j, end = learnts_.size();
		for (i = j = 0; i != end; ++i) {
			ClauseHead* c = learnts_[i]->clause();
			if (!c || !c->tagged()) {
				learnts_[j++] = learnts_[i];
			}
			else {
				c->destroy(this, true);
			}
		}
		learnts_.erase(learnts_.begin()+j, learnts_.end());
	}
}

void Solver::strengthenConditional() {
	Literal p = ~tagLiteral();
	if (!isSentinel(p)) {
		ConstraintDB::size_type i, j, end = learnts_.size();
		for (i = j = 0; i != end; ++i) {
			ClauseHead* c = learnts_[i]->clause();
			if (!c || !c->tagged() || !c->strengthen(*this, p, true).second) {
				learnts_[j++] = learnts_[i];
			}
			else {
				assert((decisionLevel() == rootLevel() || !c->locked(*this)) && "Solver::strengthenConditional(): must not remove locked constraint!");
				c->destroy(this, false);
			}
		}
		learnts_.erase(learnts_.begin()+j, learnts_.end());
	}
}

bool Solver::simplifySAT() {
	if (queueSize() > 0 && !propagate()) {
		return false;
	}
	assert(assign_.qEmpty());
	uint32 start  = lastSimp_;
	assign_.front = start;
	lastSimp_     = (uint32)assign_.trail.size();
	for (Literal p; !assign_.qEmpty(); ) {
		p = assign_.qPop();
		releaseVec(watches_[p.id()]);
		releaseVec(watches_[(~p).id()]);
	}
	bool shuffle = shufSimp_ != 0;
	shufSimp_    = 0;
	if (shuffle) {
		std::random_shuffle(constraints_.begin(), constraints_.end(), rng);
		std::random_shuffle(learnts_.begin(), learnts_.end(), rng);
	}
	if (isMaster()) { shared_->simplify(start, shuffle); }
	else            { simplifyDB(*this, constraints_, shuffle); }
	simplifyDB(*this, learnts_, shuffle);
	FOR_EACH_POST(x, postHead_) {
		if (x->simplify(*this, shuffle)) {
			post_.remove(x);
			x->destroy(this, false);
		}
	}
	if (enum_ && enum_->simplify(*this, shuffle)) {
		enum_->destroy(this, false);
		enum_ = 0;
	}
	return true;
}

void Solver::setConflict(Literal p, const Antecedent& a, uint32 data) {
	++stats.conflicts;
	conflict_.push_back(~p);
	if (searchMode() != SolverStrategies::no_learning && !a.isNull()) {
		if (data == UINT32_MAX) {
			a.reason(*this, p, conflict_);
		}
		else {
			// temporarily replace old data with new data
			uint32 saved = assign_.data(p.var());
			assign_.setData(p.var(), data);
			// extract conflict using new data
			a.reason(*this, p, conflict_);
			// restore old data
			assign_.setData(p.var(), saved);
		}
	}
}
bool Solver::force(const ImpliedLiteral& p) {
	// Already implied?
	if (isTrue(p.lit)) {
		if (level(p.lit.var()) <= p.level) { return true; }
		if (ImpliedLiteral* x = impliedLits_.find(p.lit)) {
			if (x->level > p.level) {
				*x = p;
				setReason(p.lit, p.ante.ante(), p.ante.data());
			}
			return true;
		}
	}
	if (undoUntil(p.level) != p.level) {
		// Logically the implication is on level p.level.
		// Store enough information so that p can be re-assigned once we backtrack.
		impliedLits_.add(decisionLevel(), p);
	}
	return (isTrue(p.lit) && setReason(p.lit, p.ante.ante(), p.ante.data())) || force(p.lit, p.ante.ante(), p.ante.data());
}

bool Solver::assume(const Literal& p) {
	if (value(p.var()) == value_free) {
		assert(decisionLevel() != assign_.maxLevel());
		++stats.choices;
		levels_.push_back(DLevel(numAssignedVars(), 0));
		return assign_.assign(p, decisionLevel(), Antecedent());
	}
	return isTrue(p);
}

void Solver::cancelPropagation() {
	assign_.qReset();
	for (PostPropagator* r = *postHead_; r; r = r->next) { r->reset(); }
}

bool Solver::propagate() {
	if (unitPropagate() && postPropagate(postHead_, 0)) {
		assert(queueSize() == 0);
		return true;
	}
	cancelPropagation();
	return false;
}

bool Solver::propagateFrom(PostPropagator* p) {
	assert((p && *postHead_) && "OP not allowed during init!");
	assert(queueSize() == 0);
	for (PostPropagator** r = postHead_; *r;) {
		if      (*r != p)             { r = &(*r)->next; }
		else if (postPropagate(r, 0)) { break; }
		else {
			cancelPropagation();
			return false;
		}
	}
	assert(queueSize() == 0);
	return true;
}

bool Solver::propagateUntil(PostPropagator* p) {
	assert((!p || *postHead_) && "OP not allowed during init!");
	return unitPropagate() && (p == *postHead_ || postPropagate(postHead_, p));
}

Constraint::PropResult ClauseHead::propagate(Solver& s, Literal p, uint32&) {
	Literal* head = head_;
	uint32 wLit   = (head[1] == ~p); // pos of false watched literal
	if (s.isTrue(head[1-wLit])) {
		return Constraint::PropResult(true, true);
	}
	else if (!s.isFalse(head[2])) {
		assert(!isSentinel(head[2]) && "Invalid ClauseHead!");
		head[wLit] = head[2];
		head[2]    = ~p;
		s.addWatch(~head[wLit], ClauseWatch(this));
		return Constraint::PropResult(true, false);
	}
	else if (updateWatch(s, wLit)) {
		assert(!s.isFalse(head_[wLit]));
		s.addWatch(~head[wLit], ClauseWatch(this));
		return Constraint::PropResult(true, false);
	}
	return PropResult(s.force(head_[1^wLit], this), true);
}

bool Solver::unitPropagate() {
	assert(!hasConflict());
	Literal p, q, r;
	uint32 idx, ignore, DL = decisionLevel();
	const ShortImplicationsGraph& btig = shared_->shortImplications();
	const uint32 maxIdx = btig.size();
	while ( !assign_.qEmpty() ) {
		p             = assign_.qPop();
		idx           = p.id();
		WatchList& wl = watches_[idx];
		// first: short clause BCP
		if (idx < maxIdx && !btig.propagate(*this, p)) {
			return false;
		}
		// second: clause BCP
		if (wl.left_size() != 0) {
			WatchList::left_iterator it, end, j = wl.left_begin();
			Constraint::PropResult res;
			for (it = wl.left_begin(), end = wl.left_end();  it != end;  ) {
				ClauseWatch& w = *it++;
				res = w.head->ClauseHead::propagate(*this, p, ignore);
				if (res.keepWatch) {
					*j++ = w;
				}
				if (!res.ok) {
					wl.shrink_left(std::copy(it, end, j));
					return false;
				}
			}
			wl.shrink_left(j);
		}
		// third: general constraint BCP
		if (wl.right_size() != 0) {
			WatchList::right_iterator it, end, j = wl.right_begin();
			Constraint::PropResult res;
			for (it = wl.right_begin(), end = wl.right_end(); it != end; ) {
				GenericWatch& w = *it++;
				res = w.propagate(*this, p);
				if (res.keepWatch) {
					*j++ = w;
				}
				if (!res.ok) {
					wl.shrink_right(std::copy(it, end, j));
					return false;
				}
			}
			wl.shrink_right(j);
		}
	}
	return DL || assign_.markUnits();
}

bool Solver::postPropagate(PostPropagator** start, PostPropagator* stop) {
	for (PostPropagator** r = start, *t; *r != stop;) {
		t = *r;
		if (!t->propagateFixpoint(*this, stop)) { return false; }
		assert(queueSize() == 0);
		if (t == *r) { r = &t->next; }
		// else: t was removed during propagate
	}
	return true;
}

bool Solver::test(Literal p, PostPropagator* c) {
	assert(value(p.var()) == value_free && !hasConflict());
	assume(p); --stats.choices;
	uint32 pLevel = decisionLevel();
	freezeLevel(pLevel); // can't split-off this level
	if (propagateUntil(c)) {
		assert(decisionLevel() == pLevel && "Invalid PostPropagators");
		if (c) c->undoLevel(*this);
		undoUntil(pLevel-1);
		return true;
	}
	assert(decisionLevel() == pLevel && "Invalid PostPropagators");
	unfreezeLevel(pLevel);
	cancelPropagation();
	return false;
}

bool Solver::resolveConflict() {
	assert(hasConflict());
	if (decisionLevel() > rootLevel()) {
		if (decisionLevel() != backtrackLevel() && searchMode() != SolverStrategies::no_learning) {
			uint32 uipLevel = analyzeConflict();
			stats.addConflict(decisionLevel(), uipLevel, backtrackLevel(), ccInfo_.lbd());
			if (shared_->reportMode()) {
				sharedContext()->report(NewConflictEvent(*this, cc_, ccInfo_));
			}
			undoUntil( uipLevel );
			return ClauseCreator::create(*this, cc_, ClauseCreator::clause_no_prepare, ccInfo_);
		}
		else {
			return backtrack();
		}
	}
	return false;
}

bool Solver::backtrack() {
	Literal lastChoiceInverted;
	do {
		if (decisionLevel() == rootLevel()) {
			setStopConflict();
			return false;
		}
		lastChoiceInverted = ~decision(decisionLevel());
		undoUntil(decisionLevel() - 1, undo_pop_proj_level);
		setBacktrackLevel(decisionLevel(), undo_pop_bt_level);
	} while (hasConflict() || !force(lastChoiceInverted, 0));
	// remember flipped literal for copyGuidingPath()
	impliedLits_.add(decisionLevel(), ImpliedLiteral(lastChoiceInverted, decisionLevel(), 0));
	return true;
}

bool ImpliedList::assign(Solver& s) {
	assert(front <= lits.size());
	bool ok             = !s.hasConflict();
	const uint32 DL     = s.decisionLevel();
	VecType::iterator j = lits.begin() + front;
	for (VecType::iterator it = j, end = lits.end(); it != end; ++it) {
		if(it->level <= DL) {
			ok = ok && s.force(it->lit, it->ante.ante(), it->ante.data());
			if (it->level < DL || it->ante.ante().isNull()) { *j++ = *it; }
		}
	}
	lits.erase(j, lits.end());
	level = DL * uint32(!lits.empty());
	front = level > s.rootLevel() ? front  : sizeVec(lits);
	return ok;
}
bool Solver::isUndoLevel() const {
	return decisionLevel() > backtrackLevel();
}
uint32 Solver::undoUntilImpl(uint32 level, bool forceSave) {
	level      = std::max( level, backtrackLevel() );
	if (level >= decisionLevel()) { return decisionLevel(); }
	uint32& n  = (levels_.jump = decisionLevel() - level);
	bool sp    = forceSave || (strategy_.saveProgress > 0 && ((uint32)strategy_.saveProgress) <= n);
	bool ok    = conflict_.empty() && levels_.back().freeze == 0;
	conflict_.clear();
	heuristic_->undoUntil( *this, levels_[level].trailPos);
	undoLevel(sp && ok);
	while (--n) { undoLevel(sp); }
	return level;
}
uint32 Solver::undoUntil(uint32 level, uint32 mode) {
	assert(backtrackLevel() >= rootLevel());
	if (level < backtrackLevel() && mode >= levels_.mode) {
		levels_.flip = std::max(rootLevel(), level);
	}
	level = undoUntilImpl(level, (mode & undo_save_phases) != 0);
	if (impliedLits_.active(level)) {
		impliedLits_.assign(*this);
	}
	return level;
}
uint32 Solver::estimateBCP(const Literal& p, int rd) const {
	if (value(p.var()) != value_free) return 0;
	LitVec::size_type first = assign_.assigned();
	LitVec::size_type i     = first;
	Solver& self            = const_cast<Solver&>(*this);
	self.assign_.setValue(p.var(), trueValue(p));
	self.assign_.trail.push_back(p);
	const ShortImplicationsGraph& btig = shared_->shortImplications();
	const uint32 maxIdx = btig.size();
	do {
		Literal x = assign_.trail[i++];
		if (x.id() < maxIdx && !btig.propagateBin(self.assign_, x, 0)) {
			break;
		}
	} while (i < assign_.assigned() && rd-- != 0);
	i = assign_.assigned()-first;
	while (self.assign_.assigned() != first) {
		self.assign_.undoLast();
	}
	return (uint32)i;
}

uint32 Solver::inDegree(WeightLitVec& out) {
	if (decisionLevel() == 0) { return 1; }
	assert(!hasConflict());
	out.reserve((numAssignedVars()-levelStart(1))/10);
	uint32 maxIn = 1;
	uint32 i = sizeVec(assign_.trail), stop = levelStart(1);
	for (LitVec temp; i-- != stop; ) {
		Literal x    = assign_.trail[i];
		uint32  xLev = assign_.level(x.var());
		uint32  xIn  = 0;
		Antecedent xAnte = assign_.reason(x.var());
		if (!xAnte.isNull() && xAnte.type() != Antecedent::Binary) {
			xAnte.reason(*this, x, temp);
			for (LitVec::const_iterator it = temp.begin(); it != temp.end(); ++it) {
				xIn += level(it->var()) != xLev;
			}
			if (xIn) {
				out.push_back(WeightLiteral(x, xIn));
				maxIn = std::max(xIn, maxIn);
			}
			temp.clear();
		}
	}
	return maxIn;
}
/////////////////////////////////////////////////////////////////////////////////////////
// Solver: Private helper functions
////////////////////////////////////////////////////////////////////////////////////////
Solver::ConstraintDB* Solver::allocUndo(Constraint* c) {
	if (undoHead_ == 0) {
		return new ConstraintDB(1, c);
	}
	assert(undoHead_->size() == 1);
	ConstraintDB* r = undoHead_;
	undoHead_ = (ConstraintDB*)undoHead_->front();
	r->clear();
	r->push_back(c);
	return r;
}
void Solver::undoFree(ConstraintDB* x) {
	// maintain a single-linked list of undo lists
	x->clear();
	x->push_back((Constraint*)undoHead_);
	undoHead_ = x;
}
// removes the current decision level
void Solver::undoLevel(bool sp) {
	assert(decisionLevel() != 0 && levels_.back().trailPos != assign_.trail.size() && "Decision Level must not be empty");
	assign_.undoTrail(levels_.back().trailPos, sp);
	if (levels_.back().undo) {
		const ConstraintDB& undoList = *levels_.back().undo;
		for (ConstraintDB::size_type i = 0, end = undoList.size(); i != end; ++i) {
			undoList[i]->undoLevel(*this);
		}
		undoFree(levels_.back().undo);
	}
	levels_.pop_back();
}

inline ClauseHead* clause(const Antecedent& ante) {
	return ante.isNull() || ante.type() != Antecedent::Generic ? 0 : ante.constraint()->clause();
}

bool Solver::resolveToFlagged(const LitVec& in, uint8 vf, LitVec& out, uint32& outLbd) const {
	return const_cast<Solver&>(*this).resolveToFlagged(in, vf, out, outLbd);
}
bool Solver::resolveToFlagged(const LitVec& in, const uint8 vf, LitVec& out, uint32& outLbd) {
	const LitVec& trail = assign_.trail;
	const LitVec* rhs   = &in;
	LitVec temp; out.clear();
	bool ok = true, first = true;
	for (LitVec::size_type tp = trail.size(), resolve = 0;; first = false) {
		Literal p; Var v;
		for (LitVec::const_iterator it = rhs->begin(), end = rhs->end(); it != end; ++it) {
			p = *it ^ first; v = p.var();
			if (!seen(v)) {
				markSeen(v);
				if      (varInfo(v).hasAll(vf)) { markLevel(level(v)); out.push_back(~p); }
				else if (!reason(p).isNull())   { ++resolve; }
				else                            { clearSeen(v); ok = false; break; }
			}
		}
		if (resolve-- == 0) { break; }
		// find next literal to resolve
		while (!seen(trail[--tp]) || varInfo(trail[tp].var()).hasAll(vf)) { ; }
		clearSeen((p = trail[tp]).var());
		reason(p, temp);
		rhs = &temp;
	}
	LitVec::size_type outSize = out.size();
	if (ok && !first) {
		const uint32 ccAct = strategy_.ccMinKeepAct;
		const uint32 antes = SolverStrategies::all_antes;
		strategy_.ccMinKeepAct = 1;
		if (ccMin_) { ccMinRecurseInit(*ccMin_); }
		for (LitVec::size_type i = 0; i != outSize;) {
			if (!ccRemovable(~out[i], antes, ccMin_)) { ++i; }
			else {
				std::swap(out[i], out[--outSize]);
			}
		}
		strategy_.ccMinKeepAct = ccAct;
	}
	POTASSCO_ASSERT(!ok || outSize != 0, "Invalid empty clause - was %u!\n", out.size());
	outLbd = 0;
	for (uint32 i = 0, dl, root = 0; i != outSize; ++i) {
		Var v = out[i].var();
		dl    = level(v);
		clearSeen(v);
		if (dl && hasLevel(dl)) {
			unmarkLevel(dl);
			outLbd += (dl > rootLevel()) || (++root == 1);
		}
	}
	for (Var v; outSize != out.size(); out.pop_back()) {
		clearSeen(v = out.back().var());
		unmarkLevel(level(v));
	}
	return ok;
}
void Solver::resolveToCore(LitVec& out) {
	POTASSCO_REQUIRE(hasConflict() && !hasStopConflict(), "Function requires valid conflict");
	// move conflict to cc_
	cc_.clear();
	cc_.swap(conflict_);
	if (searchMode() == SolverStrategies::no_learning) {
		for (uint32 i = 1, end = decisionLevel() + 1; i != end; ++i) { cc_.push_back(decision(i)); }
	}
	const LitVec& trail = assign_.trail;
	const LitVec* r = &cc_;
	// resolve all-last uip
	for (uint32 marked = 0, tPos = (uint32)trail.size();; r = &conflict_) {
		for (LitVec::const_iterator it = r->begin(), end = r->end(); it != end; ++it) {
			if (!seen(it->var())) {
				assert(level(it->var()) <= decisionLevel());
				markSeen(it->var());
				++marked;
			}
		}
		if (marked-- == 0) { break; }
		// search for the last marked literal
		while (!seen(trail[--tPos].var())) { ; }
		Literal p = trail[tPos];
		uint32 dl = level(p.var());
		assert(dl);
		clearSeen(p.var());
		conflict_.clear();
		if      (!reason(p).isNull()) { reason(p).reason(*this, p, conflict_); }
		else if (p == decision(dl))   { out.push_back(p); }
	}
	// restore original conflict
	cc_.swap(conflict_);
}

// computes the First-UIP clause and stores it in cc_, where cc_[0] is the asserting literal (inverted UIP)
// and cc_[1] is a literal from the asserting level (if > 0)
// RETURN: asserting level of the derived conflict clause
uint32 Solver::analyzeConflict() {
	// must be called here, because we unassign vars during analyzeConflict
	heuristic_->undoUntil( *this, levels_.back().trailPos );
	uint32 onLevel  = 0;        // number of literals from the current DL in resolvent
	uint32 resSize  = 0;        // size of current resolvent
	Literal p;                  // literal to be resolved out next
	cc_.assign(1, p);           // will later be replaced with asserting literal
	Antecedent lhs, rhs, last;  // resolve operands
	const bool doOtfs = strategy_.otfs > 0;
	for (bumpAct_.clear();;) {
		uint32 lhsSize = resSize;
		uint32 rhsSize = 0;
		heuristic_->updateReason(*this, conflict_, p);
		for (LitVec::size_type i = 0; i != conflict_.size(); ++i) {
			Literal& q = conflict_[i];
			uint32 cl  = level(q.var());
			rhsSize   += (cl != 0);
			if (!seen(q.var())) {
				++resSize;
				assert(isTrue(q) && "Invalid literal in reason set!");
				assert(cl > 0 && "Top-Level implication not marked!");
				markSeen(q.var());
				if (cl == decisionLevel()) {
					++onLevel;
				}
				else {
					cc_.push_back(~q);
					markLevel(cl);
				}
			}
		}
		if (resSize != lhsSize) { lhs = 0; }
		if (rhsSize != resSize) { rhs = 0; }
		if (doOtfs && (!rhs.isNull() || !lhs.isNull())) {
			// resolvent subsumes rhs and possibly also lhs
			otfs(lhs, rhs, p, onLevel == 1);
		}
		assert(onLevel > 0 && "CONFLICT MUST BE ANALYZED ON CONFLICT LEVEL!");
		// search for the last assigned literal that needs to be analyzed...
		while (!seen(assign_.last().var())) {
			assign_.undoLast();
		}
		p   = assign_.last();
		rhs = reason(p);
		clearSeen(p.var());
		if (--onLevel == 0) {
			break;
		}
		--resSize; // p will be resolved out next
		last = rhs;
		reason(p, conflict_);
	}
	cc_[0] = ~p; // store the 1-UIP
	assert(decisionLevel() == level(cc_[0].var()));
	ClauseHead* lastRes = 0;
	if (strategy_.otfs > 1 || !lhs.isNull()) {
		if (!lhs.isNull()) {
			lastRes = clause(lhs);
		}
		else if (cc_.size() <= (conflict_.size()+1)) {
			lastRes = clause(last);
		}
	}
	if (strategy_.bumpVarAct && reason(p).learnt()) {
		bumpAct_.push_back(WeightLiteral(p, reason(p).constraint()->activity().lbd()));
	}
	return simplifyConflictClause(cc_, ccInfo_, lastRes);
}

void Solver::otfs(Antecedent& lhs, const Antecedent& rhs, Literal p, bool final) {
	ClauseHead* cLhs = clause(lhs), *cRhs = clause(rhs);
	ClauseHead::BoolPair x;
	if (cLhs) {
		x = cLhs->strengthen(*this, ~p, !final);
		if (!x.first || x.second) {
			cLhs = !x.first ? 0 : otfsRemove(cLhs, 0);
		}
	}
	lhs = cLhs;
	if (cRhs) {
		x = cRhs->strengthen(*this, p, !final);
		if (!x.first || (x.second && otfsRemove(cRhs, 0) == 0)) {
			if (x.first && reason(p) == cRhs) { setReason(p, 0); }
			cRhs = 0;
		}
		if (cLhs && cRhs) {
			// lhs and rhs are now equal - only one of them is needed
			if (!cLhs->learnt()) {
				std::swap(cLhs, cRhs);
			}
			otfsRemove(cLhs, 0);
		}
		lhs = cRhs;
	}
}

ClauseHead* Solver::otfsRemove(ClauseHead* c, const LitVec* newC) {
	bool remStatic = !newC || (newC->size() <= 3 && shared_->allowImplicit(Constraint_t::Conflict));
	if (c->learnt() || remStatic) {
		ConstraintDB& db = (c->learnt() ? learnts_ : constraints_);
		ConstraintDB::iterator it;
		if ((it = std::find(db.begin(), db.end(), c)) != db.end()) {
			if (isMaster() && &db == &constraints_) {
				shared_->removeConstraint(static_cast<uint32>(it - db.begin()), true);
			}
			else {
				db.erase(it);
				c->destroy(this, true);
			}
			c = 0;
		}
	}
	return c;
}

// minimizes the conflict clause in cc w.r.t selected strategies.
// PRE:
//  - cc is a valid conflict clause and cc[0] is the UIP-literal
//  - all literals in cc except cc[0] are marked
//  - all decision levels of literals in cc are marked
//  - rhs is 0 or a clause that might be subsumed by cc
// RETURN: finalizeConflictClause(cc, info)
uint32 Solver::simplifyConflictClause(LitVec& cc, ConstraintInfo& info, ClauseHead* rhs) {
	// 1. remove redundant literals from conflict clause
	temp_.clear();
	uint32 onAssert = ccMinimize(cc, temp_, strategy_.ccMinAntes, ccMin_);
	uint32 jl       = cc.size() > 1 ? level(cc[1].var()) : 0;
	// clear seen flags of removed literals - keep levels marked
	for (LitVec::size_type x = 0, stop = temp_.size(); x != stop; ++x) {
		clearSeen(temp_[x].var());
	}
	// 2. check for inverse arcs
	if (onAssert == 1 && strategy_.reverseArcs > 0) {
		uint32 maxN = (uint32)strategy_.reverseArcs;
		if      (maxN > 2) maxN = UINT32_MAX;
		else if (maxN > 1) maxN = static_cast<uint32>(cc.size() / 2);
		markSeen(cc[0].var());
		Antecedent ante = ccHasReverseArc(cc[1], jl, maxN);
		if (!ante.isNull()) {
			// resolve with inverse arc
			conflict_.clear();
			ante.reason(*this, ~cc[1], conflict_);
			ccResolve(cc, 1, conflict_);
		}
		clearSeen(cc[0].var());
	}
	// 3. check if final clause subsumes rhs
	if (rhs) {
		conflict_.clear();
		rhs->toLits(conflict_);
		uint32 open   = (uint32)cc.size();
		markSeen(cc[0].var());
		for (LitVec::const_iterator it = conflict_.begin(), end = conflict_.end(); it != end && open; ++it) {
			// NOTE: at this point the DB might not be fully simplified,
			//       e.g. because of mt or lookahead, hence we must explicitly
			//       check for literals assigned on DL 0
			open -= level(it->var()) > 0 && seen(it->var());
		}
		rhs = open ? 0 : otfsRemove(rhs, &cc);
		if (rhs) { // rhs is subsumed by cc but could not be removed.
			// TODO: we could reuse rhs instead of learning cc
			//       but this would complicate the calling code.
			ClauseHead::BoolPair r(true, false);
			if (cc_.size() < conflict_.size()) {
				//     For now, we only try to strengthen rhs.
				for (LitVec::const_iterator it = conflict_.begin(), end = conflict_.end(); it != end && r.first; ++it) {
					if (!seen(it->var()) || level(it->var()) == 0) {
						r = rhs->strengthen(*this, *it, false);
					}
				}
				if (!r.first) { rhs = 0; }
			}
		}
		clearSeen(cc[0].var());
	}
	// 4. finalize
	uint32 repMode = cc.size() < std::max(strategy_.compress, decisionLevel()+1) ? 0 : strategy_.ccRepMode;
	jl = finalizeConflictClause(cc, info, repMode);
	// 5. bump vars implied by learnt constraints with small lbd
	if (!bumpAct_.empty()) {
		WeightLitVec::iterator j = bumpAct_.begin();
		weight_t newLbd = info.lbd();
		for (WeightLitVec::iterator it = bumpAct_.begin(), end = bumpAct_.end(); it != end; ++it) {
			if (it->second < newLbd) {
				it->second = 1 + (it->second <= 2);
				*j++ = *it;
			}
		}
		bumpAct_.erase(j, bumpAct_.end());
		heuristic_->bump(*this, bumpAct_, 1.0);
	}
	bumpAct_.clear();
	// 6. clear level flags of redundant literals
	for (LitVec::size_type x = 0, stop = temp_.size(); x != stop; ++x) {
		unmarkLevel(level(temp_[x].var()));
	}
	temp_.clear();
	return jl;
}

// conflict clause minimization
// PRE:
//  - cc is an asserting clause and cc[0] is the asserting literal
//  - all literals in cc are marked as seen
//  -  if ccMin != 0, all decision levels of literals in cc are marked
// POST:
//  - redundant literals were added to removed
//  - if (cc.size() > 1): cc[1] is a literal from the asserting level
// RETURN
//  - the number of literals from the asserting level
uint32 Solver::ccMinimize(LitVec& cc, LitVec& removed, uint32 antes, CCMinRecursive* ccMin) {
	if (ccMin) { ccMinRecurseInit(*ccMin); }
	// skip the asserting literal
	LitVec::size_type j = 1;
	uint32 assertLevel  = 0;
	uint32 assertPos    = 1;
	uint32 onAssert     = 0;
	uint32 varLevel     = 0;
	for (LitVec::size_type i = 1; i != cc.size(); ++i) {
		if (antes == SolverStrategies::no_antes || !ccRemovable(~cc[i], antes, ccMin)) {
			if ( (varLevel = level(cc[i].var())) > assertLevel ) {
				assertLevel = varLevel;
				assertPos   = static_cast<uint32>(j);
				onAssert    = 0;
			}
			onAssert += (varLevel == assertLevel);
			cc[j++] = cc[i];
		}
		else {
			removed.push_back(cc[i]);
		}
	}
	cc.erase(cc.begin()+j, cc.end());
	if (assertPos != 1) {
		std::swap(cc[1], cc[assertPos]);
	}
	return onAssert;
}
void Solver::ccMinRecurseInit(CCMinRecursive& ccMin) {
	ccMin.open = incEpoch(numVars() + 1, 2) - 2;
}
bool Solver::ccMinRecurse(CCMinRecursive& ccMin, Literal p) const {
	CCMinRecursive::State st = ccMin.decodeState(epoch_[p.var()]);
	if (st == CCMinRecursive::state_poison) { return false; }
	if (st == CCMinRecursive::state_open)   { ccMin.push(p.unflag()); }
	return true;
}

// returns true if p is redundant in current conflict clause
bool Solver::ccRemovable(Literal p, uint32 antes, CCMinRecursive* ccMin) {
	const Antecedent& ante = reason(p);
	if (ante.isNull() || !(antes <= (uint32)ante.type())) {
		return false;
	}
	if (!ccMin) { return ante.minimize(*this, p, 0); }
	// recursive minimization
	assert(ccMin->todo.empty());
	CCMinRecursive::State dfsState = CCMinRecursive::state_removable;
	ccMin->push(p.unflag());
	for (Literal x;; ) {
		x = ccMin->pop();
		assert(!seen(x.var()) || x == p);
		if (x.flagged()) {
			if (x == p) return dfsState == CCMinRecursive::state_removable;
			epoch_[x.var()] = ccMin->encodeState(dfsState);
		}
		else if (dfsState != CCMinRecursive::state_poison) {
			CCMinRecursive::State temp = ccMin->decodeState(epoch_[x.var()]);
			if (temp == CCMinRecursive::state_open) {
				assert(value(x.var()) != value_free && hasLevel(level(x.var())));
				ccMin->push(x.flag());
				const Antecedent& next = reason(x);
				if (next.isNull() || !(antes <= (uint32)next.type()) || !next.minimize(*this, x, ccMin)) {
					dfsState = CCMinRecursive::state_poison;
				}
			}
			else if (temp == CCMinRecursive::state_poison) {
				dfsState = temp;
			}
		}
	}
}

// checks whether there is a valid "inverse arc" for the given literal p that can be used
// to resolve p out of the current conflict clause
// PRE:
//  - all literals in the current conflict clause are marked
//  - p is a literal of the current conflict clause and level(p) == maxLevel
// RETURN
//  - An antecedent that is an "inverse arc" for p or null if no such antecedent exists.
Antecedent Solver::ccHasReverseArc(Literal p, uint32 maxLevel, uint32 maxNew) {
	assert(seen(p.var()) && isFalse(p) && level(p.var()) == maxLevel);
	const ShortImplicationsGraph& btig = shared_->shortImplications();
	Antecedent ante;
	if (p.id() < btig.size() && btig.reverseArc(*this, p, maxLevel, ante)) { return ante; }
	WatchList& wl = watches_[p.id()];
	for (WatchList::left_iterator it = wl.left_begin(), end = wl.left_end();  it != end;  ++it) {
		if (it->head->isReverseReason(*this, ~p, maxLevel, maxNew)) {
			return it->head;
		}
	}
	return ante;
}

// removes cc[pos] by resolving cc with reason
void Solver::ccResolve(LitVec& cc, uint32 pos, const LitVec& reason) {
	heuristic_->updateReason(*this, reason, cc[pos]);
	Literal x;
	for (LitVec::size_type i = 0; i != reason.size(); ++i) {
		x = reason[i];
		assert(isTrue(x));
		if (!seen(x.var())) {
			markLevel(level(x.var()));
			cc.push_back(~x);
		}
	}
	clearSeen(cc[pos].var());
	unmarkLevel(level(cc[pos].var()));
	cc[pos] = cc.back();
	cc.pop_back();
}

// computes asserting level and lbd of cc and clears flags.
// POST:
//  - literals and decision levels in cc are no longer marked
//  - if cc.size() > 1: cc[1] is a literal from the asserting level
// RETURN: asserting level of conflict clause.
uint32 Solver::finalizeConflictClause(LitVec& cc, ConstraintInfo& info, uint32 ccRepMode) {
	// 2. clear flags and compute lbd
	uint32  lbd         = 1;
	uint32  onRoot      = 0;
	uint32  varLevel    = 0;
	uint32  assertLevel = 0;
	uint32  assertPos   = 1;
	uint32  maxVar      = cc[0].var();
	Literal tagLit      = ~tagLiteral();
	bool    tagged      = false;
	for (LitVec::size_type i = 1; i != cc.size(); ++i) {
		Var v = cc[i].var();
		clearSeen(v);
		if (cc[i] == tagLit) { tagged = true; }
		if (v > maxVar)      { maxVar = v;    }
		if ( (varLevel = level(v)) > assertLevel ) {
			assertLevel = varLevel;
			assertPos   = static_cast<uint32>(i);
		}
		if (hasLevel(varLevel)) {
			unmarkLevel(varLevel);
			lbd += (varLevel > rootLevel()) || (++onRoot == 1);
		}
	}
	if (assertPos != 1) { std::swap(cc[1], cc[assertPos]); }
	if (ccRepMode == SolverStrategies::cc_rep_dynamic) {
		ccRepMode = double(lbd)/double(decisionLevel()) > .66 ? SolverStrategies::cc_rep_decision : SolverStrategies::cc_rep_uip;
	}
	if (ccRepMode) {
		maxVar = cc[0].var(), tagged = false, lbd = 1;
		if (ccRepMode == SolverStrategies::cc_rep_decision) {
			// replace cc with decision sequence
			cc.resize(assertLevel+1);
			for (uint32 i = assertLevel; i;){
				Literal x = ~decision(i--);
				cc[lbd++] = x;
				if (x == tagLit)     { tagged = true; }
				if (x.var() > maxVar){ maxVar = x.var(); }
			}
		}
		else {
			// replace cc with all uip clause
			uint32 marked = sizeVec(cc) - 1;
			while (cc.size() > 1) { markSeen(~cc.back()); cc.pop_back(); }
			for (LitVec::const_iterator tr = assign_.trail.end(), next, stop; marked;) {
				while (!seen(*--tr)) { ; }
				bool n = --marked != 0 && !reason(*tr).isNull();
				clearSeen(tr->var());
				if (n) { for (next = tr, stop = assign_.trail.begin() + levelStart(level(tr->var())); next-- != stop && !seen(*next);) { ; } }
				if (!n || level(next->var()) != level(tr->var())) {
					cc.push_back(~*tr);
					if (tr->var() == tagLit.var()){ tagged = true; }
					if (tr->var() > maxVar)       { maxVar = tr->var(); }
				}
				else {
					for (reason(*tr, conflict_); !conflict_.empty(); conflict_.pop_back()) {
						if (!seen(conflict_.back())) { ++marked; markSeen(conflict_.back()); }
					}
				}
			}
			lbd = sizeVec(cc);
		}
	}
	info.setScore(makeScore(ccInfo_.activity(), lbd));
	info.setTagged(tagged);
	info.setAux(auxVar(maxVar));
	return assertLevel;
}

// (inefficient) default implementation
bool Constraint::minimize(Solver& s, Literal p, CCMinRecursive* rec) {
	LitVec temp;
	reason(s, p, temp);
	for (LitVec::size_type i = 0; i != temp.size(); ++i) {
		if (!s.ccMinimize(temp[i], rec)) {
			return false;
		}
	}
	return true;
}

// Selects next branching literal
bool Solver::decideNextBranch(double f) {
	if (f <= 0.0 || rng.drand() >= f || numFreeVars() == 0) {
		return heuristic_->select(*this);
	}
	// select randomly
	Literal choice;
	uint32 maxVar = numVars() + 1;
	for (uint32 v = rng.irand(maxVar);;) {
		if (value(v) == value_free) {
			choice    = heuristic_->selectLiteral(*this, v, 0);
			break;
		}
		if (++v == maxVar) { v = 1; }
	}
	return assume(choice);
}
void Solver::resetLearntActivities() {
	for (ConstraintDB::size_type i = 0, end = learnts_.size(); i != end; ++i) {
		learnts_[i]->resetActivity();
	}
}
// Removes up to remFrac% of the learnt nogoods but
// keeps those that are locked or are highly active.
Solver::DBInfo Solver::reduceLearnts(float remFrac, const ReduceStrategy& rs) {
	uint32 oldS = numLearntConstraints();
	uint32 remM = static_cast<uint32>(oldS * std::max(0.0f, remFrac));
	DBInfo r    = {0,0,0};
	CmpScore cmp(learnts_, (ReduceStrategy::Score)rs.score, rs.glue, rs.protect);
	if (remM >= oldS || !remM || rs.algo == ReduceStrategy::reduce_sort) {
		r = reduceSortInPlace(remM, cmp, false);
	}
	else if (rs.algo == ReduceStrategy::reduce_stable) { r = reduceSort(remM, cmp);  }
	else if (rs.algo == ReduceStrategy::reduce_heap)   { r = reduceSortInPlace(remM, cmp, true);}
	else                                               { r = reduceLinear(remM, cmp); }
	stats.addDeleted(oldS - r.size);
	shrinkVecTo(learnts_, r.size);
	return r;
}
// Removes up to maxR of the learnt nogoods.
// Keeps those that are locked or have a high activity and
// does not reorder learnts_.
Solver::DBInfo Solver::reduceLinear(uint32 maxR, const CmpScore& sc) {
	// compute average activity
	uint64 scoreSum = 0;
	for (LitVec::size_type i = 0; i != learnts_.size(); ++i) {
		scoreSum += sc.score(learnts_[i]->activity());
	}
	double avgAct = (scoreSum / (double) numLearntConstraints());
	// constraints with socre > 1.5 times the average are "active"
	double scoreThresh = avgAct * 1.5;
	double scoreMax    = (double)sc.score(makeScore(Clasp::ACT_MAX, 1));
	if (scoreThresh > scoreMax) {
		scoreThresh = (scoreMax + (scoreSum / (double) numLearntConstraints())) / 2.0;
	}
	// remove up to maxR constraints but keep "active" and locked once
	DBInfo res = {0,0,0};
	typedef ConstraintScore ScoreType;
	for (LitVec::size_type i = 0; i != learnts_.size(); ++i) {
		Constraint* c = learnts_[i];
		ScoreType a   = c->activity();
		bool isLocked = c->locked(*this);
		bool isGlue   = sc.score(a) > scoreThresh || sc.isGlue(a);
		if (maxR == 0 || isLocked || isGlue || sc.isFrozen(a)) {
			res.pinned += isGlue;
			res.locked += isLocked;
			learnts_[res.size++] = c;
			c->decreaseActivity();
		}
		else {
			--maxR;
			c->destroy(this, true);
		}
	}
	return res;
}

// Sorts learnt constraints by score and removes the
// maxR constraints with the lowest score without
// reordering learnts_.
Solver::DBInfo Solver::reduceSort(uint32 maxR, const CmpScore& sc) {
	typedef PodVector<CmpScore::ViewPair>::type HeapType;
	DBInfo   res  = {0,0,0};
	HeapType heap;
	heap.reserve(maxR = std::min(maxR, (uint32)learnts_.size()));
	bool isGlue, isLocked;
	for (LitVec::size_type i = 0; i != learnts_.size(); ++i) {
		Constraint* c = learnts_[i];
		CmpScore::ViewPair vp(toU32(i), c->activity());
		res.pinned += (isGlue   = sc.isGlue(vp.second));
		res.locked += (isLocked = c->locked(*this));
		if (!isLocked && !isGlue && !sc.isFrozen(vp.second)) {
			if (maxR) { // populate heap with first maxR constraints
				heap.push_back(vp);
				if (--maxR == 0) { std::make_heap(heap.begin(), heap.end(), sc); }
			}
			else if (sc(vp, heap[0])) { // replace max element in heap
				std::pop_heap(heap.begin(), heap.end(), sc);
				heap.back() = vp;
				std::push_heap(heap.begin(), heap.end(), sc);
			}
		}
	}
	// Remove all constraints in heap - those are "inactive".
	for (HeapType::const_iterator it = heap.begin(), end = heap.end(); it != end; ++it) {
		learnts_[it->first]->destroy(this, true);
		learnts_[it->first] = 0;
	}
	// Cleanup db and decrease activity of remaining constraints.
	uint32 j = 0;
	for (LitVec::size_type i = 0; i != learnts_.size(); ++i) {
		if (Constraint* c = learnts_[i]) {
			c->decreaseActivity();
			learnts_[j++] = c;
		}
	}
	res.size = j;
	return res;
}

// Sorts the learnt db by score and removes the first
// maxR constraints (those with the lowest score).
Solver::DBInfo Solver::reduceSortInPlace(uint32 maxR, const CmpScore& sc, bool partial) {
	DBInfo res = {0,0,0};
	ConstraintDB::iterator nEnd = learnts_.begin();
	maxR = std::min(maxR, (uint32)learnts_.size());
	bool isGlue, isLocked;
	typedef ConstraintScore ScoreType;
	if (!partial) {
		// sort whole db and remove first maxR constraints
		if (maxR && maxR != learnts_.size()) std::stable_sort(learnts_.begin(), learnts_.end(), sc);
		for (ConstraintDB::iterator it = learnts_.begin(), end = learnts_.end(); it != end; ++it) {
			Constraint* c = *it;
			ScoreType a = c->activity();
			res.pinned += (isGlue = sc.isGlue(a));
			res.locked += (isLocked = c->locked(*this));
			if (!maxR || isLocked || isGlue || sc.isFrozen(a)) {
				c->decreaseActivity();
				*nEnd++ = c;
			}
			else {
				c->destroy(this, true);
				--maxR;
			}
		}
	}
	else {
		ConstraintDB::iterator hBeg = learnts_.begin();
		ConstraintDB::iterator hEnd = learnts_.begin();
		for (ConstraintDB::iterator it = learnts_.begin(), end = learnts_.end(); it != end; ++it) {
			Constraint* c = *it;
			ScoreType a = c->activity();
			res.pinned += (isGlue = sc.isGlue(a));
			res.locked += (isLocked = c->locked(*this));
			if      (isLocked || isGlue || sc.isFrozen(a)) { continue; }
			else if (maxR) {
				*it     = *hEnd;
				*hEnd++ = c;
				if (--maxR == 0) { std::make_heap(hBeg, hEnd, sc); }
			}
			else if (sc(c, learnts_[0])) {
				std::pop_heap(hBeg, hEnd, sc);
				*it      = *(hEnd-1);
				*(hEnd-1)= c;
				std::push_heap(hBeg, hEnd, sc);
			}
		}
		// remove all constraints in heap
		for (ConstraintDB::iterator it = hBeg; it != hEnd; ++it) {
			(*it)->destroy(this, true);
		}
		// copy remaining constraints down
		for (ConstraintDB::iterator it = hEnd, end = learnts_.end(); it != end; ++it) {
			Constraint* c = *it;
			c->decreaseActivity();
			*nEnd++ = c;
		}
	}
	res.size = static_cast<uint32>(std::distance(learnts_.begin(), nEnd));
	return res;
}
uint32 Solver::incEpoch(uint32 size, uint32 n) {
	if (size > epoch_.size())         { epoch_.resize(size, 0u); }
	if ((UINT32_MAX - epoch_[0]) < n) { epoch_.assign(epoch_.size(), 0u); }
	return epoch_[0] += n;
}
uint32 Solver::countLevels(const Literal* first, const Literal* last, uint32 maxLevel) {
	if (maxLevel < 2) { return uint32(maxLevel && first != last); }
	POTASSCO_ASSERT(!ccMin_ || ccMin_->todo.empty(), "Must not be called during minimization!");
	uint32 n = 0;
	for (uint32 epoch = incEpoch(sizeVec(levels_) + 1); first != last; ++first) {
		assert(value(first->var()) != value_free);
		uint32& levEpoch = epoch_[level(first->var())];
		if (levEpoch != epoch) {
			levEpoch = epoch;
			if (++n == maxLevel) { break; }
		}
	}
	return n;
}

void Solver::updateBranch(uint32 n) {
	int32 dl = (int32)decisionLevel(), xl = static_cast<int32>(cflStamp_.size())-1;
	if      (xl > dl) { do { n += cflStamp_.back(); cflStamp_.pop_back(); } while (--xl != dl); }
	else if (dl > xl) { cflStamp_.insert(cflStamp_.end(), dl - xl, 0); }
	cflStamp_.back() += n;
}
bool Solver::reduceReached(const SearchLimits& limits) const {
	return numLearntConstraints() > limits.learnts || memUse_ > limits.memory;
}
bool Solver::restartReached(const SearchLimits& limits) const {
	uint64 n = !limits.restart.local || cflStamp_.empty() ? limits.used : cflStamp_.back();
	return n >= limits.restart.conflicts || (limits.restart.dynamic && limits.restart.dynamic->reached());
}
/////////////////////////////////////////////////////////////////////////////////////////
// The basic DPLL-like search-function
/////////////////////////////////////////////////////////////////////////////////////////
ValueRep Solver::search(SearchLimits& limit, double rf) {
	assert(!isFalse(tagLiteral()));
	SearchLimits::BlockPtr block = limit.restart.block;
	rf = std::max(0.0, std::min(1.0, rf));
	lower.reset();
	if (limit.restart.local && decisionLevel() == rootLevel()) { cflStamp_.assign(decisionLevel()+1, 0); }
	do {
		for (bool conflict = hasConflict() || !propagate() || !simplify(), local = limit.restart.local;;) {
			if (conflict) {
				uint32 n = 1, ts;
				do {
					if (block && block->push(ts = numAssignedVars()) && ts > block->scaled()) {
						if (limit.restart.dynamic) { limit.restart.dynamic->resetRun(); }
						else                       { limit.restart.conflicts += block->inc; }
						block->next = block->n + block->inc;
					}
				} while (resolveConflict() && !propagate() && (++n, true));
				limit.used += n;
				if (local) { updateBranch(n); }
				if (hasConflict() || (decisionLevel() == 0 && !simplify())) { return value_false; }
				if (numFreeVars()) {
					if (limit.used >= limit.conflicts) { return value_free; }
					if (restartReached(limit))         { return value_free; }
					if (reduceReached(limit))          { return value_free; }
				}
			}
			if (decideNextBranch(rf)) { conflict = !propagate(); }
			else                      { break; }
		}
	} while (!isModel());
	temp_.clear();
	model.clear(); model.reserve(numVars()+1);
	for (Var v = 0; v <= numVars(); ++v) { model.push_back(value(v)); }
	if (satPrepro()) { satPrepro()->extendModel(model, temp_); }
	return value_true;
}
ValueRep Solver::search(uint64 maxC, uint32 maxL, bool local, double rp) {
	SearchLimits limit;
	limit.restart.conflicts = maxC;
	limit.restart.local     = local;
	limit.learnts = maxL;
	return search(limit, rp);
}
bool Solver::isModel() {
	if (hasConflict()) { return false; }
	FOR_EACH_POST(x, post_.head()) {
		if (!x->isModel(*this)) { return false; }
	}
	return !enumerationConstraint() || enumerationConstraint()->valid(*this);
}
/////////////////////////////////////////////////////////////////////////////////////////
// Free functions
/////////////////////////////////////////////////////////////////////////////////////////
void destroyDB(Solver::ConstraintDB& db, Solver* s, bool detach) {
	if (s && detach) {
		s->destroyDB(db);
		return;
	}
	while (!db.empty()) {
		db.back()->destroy(s, detach);
		db.pop_back();
	}
}
}
