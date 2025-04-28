//
// Copyright (c) 2010-2017 Benjamin Kaufmann
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
#include <clasp/shared_context.h>
#include <clasp/solver.h>
#include <clasp/clause.h>
#include <clasp/statistics.h>
#include <clasp/minimize_constraint.h>
#include <clasp/dependency_graph.h>
#include <potassco/basic_types.h>
#if CLASP_HAS_THREADS
#include <clasp/mt/thread.h>
#endif
namespace Clasp {
#define PS_STATS( APPLY ) \
	APPLY(vars               , VALUE(vars.num))            \
	APPLY(vars_eliminated    , VALUE(vars.eliminated))     \
	APPLY(vars_frozen        , VALUE(vars.frozen))         \
	APPLY(constraints        , VALUE(constraints.other))   \
	APPLY(constraints_binary , VALUE(constraints.binary))  \
	APPLY(constraints_ternary, VALUE(constraints.ternary)) \
	APPLY(acyc_edges         , VALUE(acycEdges))           \
	APPLY(complexity         , VALUE(complexity))

static const char* const stats_s[] = {
#define KEY(X,Y) #X,
	PS_STATS(KEY)
#undef KEY
	"ctx"
};
uint32 ProblemStats::size()             { return (sizeof(stats_s)/sizeof(stats_s[0])) - 1; }
const char* ProblemStats::key(uint32 i) { return i < size() ? stats_s[i] : throw std::out_of_range(POTASSCO_FUNC_NAME); }
StatisticObject ProblemStats::at(const char* key) const {
#define VALUE(X) StatisticObject::value(&X)
#define APPLY(x, y) if (std::strcmp(key, #x) == 0) return y;
	PS_STATS(APPLY)
	throw std::out_of_range(POTASSCO_FUNC_NAME);
#undef VALUE
#undef APPLY
}
#undef PS_STATS
/////////////////////////////////////////////////////////////////////////////////////////
// EventHandler
/////////////////////////////////////////////////////////////////////////////////////////
uint32 Event::nextId() { static uint32 id_s = 0; return id_s++; }
EventHandler::EventHandler(Event::Verbosity verbosity) : verb_(0), sys_(0){
	if (uint32 x = verbosity) {
		uint32 r = (x | (x<<4) | (x<<8) | (x<<12));
		verb_ = static_cast<uint16>(r);
	}
}
EventHandler::~EventHandler() {}
void EventHandler::setVerbosity(Event::Subsystem sys, Event::Verbosity verb) {
	uint32 s = (uint32(sys)<<VERB_SHIFT);
	uint32 r = verb_;
	r &= ~(uint32(VERB_MAX) << s);
	r |=  (uint32(verb) << s);
	verb_ = static_cast<uint16>(r);
}
bool EventHandler::setActive(Event::Subsystem sys) {
	if (sys == static_cast<Event::Subsystem>(sys_)) { return false; }
	sys_ = static_cast<uint16>(sys);
	return true;
}
Event::Subsystem EventHandler::active() const {
	return static_cast<Event::Subsystem>(sys_);
}
/////////////////////////////////////////////////////////////////////////////////////////
// ShortImplicationsGraph::ImplicationList
/////////////////////////////////////////////////////////////////////////////////////////
#if CLASP_HAS_THREADS
ShortImplicationsGraph::Block::Block() {
	for (int i = 0; i != block_cap; ++i) { data[i] = lit_true(); }
	size_lock = 0;
	next      = 0;
}
void ShortImplicationsGraph::Block::addUnlock(uint32 lockedSize, const Literal* x, uint32 xs) {
	std::copy(x, x+xs, data+lockedSize);
	size_lock = ((lockedSize+xs) << 1);
}
bool ShortImplicationsGraph::Block::tryLock(uint32& size) {
	uint32 s = size_lock.fetch_or(1);
	if ((s & 1) == 0) {
		size = s >> 1;
		return true;
	}
	return false;
}

#define FOR_EACH_LEARNT(x, Y) \
	for (Block* b = (x).learnt; b ; b = b->next) \
		for (const Literal* Y = b->begin(), *endof = b->end(); Y != endof; Y += 2 - Y->flagged())


ShortImplicationsGraph::ImplicationList::~ImplicationList() {
	clear(true);
}

void ShortImplicationsGraph::ImplicationList::clear(bool b) {
	ImpListBase::clear(b);
	for (Block* x = learnt; x; ) {
		Block* t = x;
		x = x->next;
		delete t;
	}
	learnt = 0;
}
void ShortImplicationsGraph::ImplicationList::simplifyLearnt(const Solver& s) {
	Block* x = learnt;
	learnt   = 0;
	while (x) {
		for (const Literal* Y = x->begin(), *endof = x->end(); Y != endof; Y += 2 - Y->flagged()) {
			Literal p = Y[0], q = !Y->flagged() ? Y[1] : lit_false();
			if (!s.isTrue(p) && !s.isTrue(q)) {
				addLearnt(p, q);
			}
		}
		Block* t = x;
		x = x->next;
		delete t;
	}
}
void ShortImplicationsGraph::ImplicationList::addLearnt(Literal p, Literal q) {
	Literal nc[2] = {p, q};
	uint32  ns    = 1 + !isSentinel(q);
	if (ns == 1) { nc[0].flag(); }
	for (Block* x;;) {
		if ((x = learnt) != 0) {
			uint32 lockedSize;
			if (x->tryLock(lockedSize)) {
				if ( (lockedSize + ns) <=  Block::block_cap ) {
					x->addUnlock(lockedSize, nc, ns);
				}
				else {
					Block* t = new Block();
					t->addUnlock(0, nc, ns);
					t->next   = x; // x is full and remains locked forever
					x = learnt= t; // publish new block - unlocks x and learnt
				}
				return;
			}
			else {
				Clasp::mt::this_thread::yield();
			}
		}
		else {
			x = new Block();
			if (compare_and_swap(learnt, static_cast<Block*>(0), x) != 0) {
				delete x;
			}
		}
	}
}

bool ShortImplicationsGraph::ImplicationList::hasLearnt(Literal q, Literal r) const {
	const bool binary = isSentinel(r);
	FOR_EACH_LEARNT(*this, imp) {
		if (imp[0] == q || imp[0] == r) {
			// binary clause subsumes new bin/tern clause
			if (imp->flagged())                          { return true; }
			// existing ternary clause subsumes new tern clause
			if (!binary && (imp[1] == q || imp[1] == r)) { return true; }
		}
	}
	return false;
}

void ShortImplicationsGraph::ImplicationList::move(ImplicationList& other) {
	ImpListBase::move(other);
	delete static_cast<Block*>(learnt);
	learnt = static_cast<Block*>(other.learnt);
	other.learnt = 0;
}
#endif
/////////////////////////////////////////////////////////////////////////////////////////
// ShortImplicationsGraph
/////////////////////////////////////////////////////////////////////////////////////////
ShortImplicationsGraph::ShortImplicationsGraph() {
	bin_[0]  = bin_[1]  = 0;
	tern_[0] = tern_[1] = 0;
	shared_  = false;
}
ShortImplicationsGraph::~ShortImplicationsGraph() {
	PodVector<ImplicationList>::destruct(graph_);
}
void ShortImplicationsGraph::resize(uint32 nodes) {
	if (nodes <= graph_.size()) {
		while (graph_.size() != nodes) {
			graph_.back().clear(true);
			graph_.pop_back();
		}
	}
	else if (graph_.capacity() >= nodes) {
		graph_.resize(nodes);
	}
	else {
		ImpLists temp; temp.resize(nodes);
		for (ImpLists::size_type i = 0; i != graph_.size(); ++i) {
			temp[i].move(graph_[i]);
		}
		graph_.swap(temp);
	}
}

uint32 ShortImplicationsGraph::numEdges(Literal p) const { return graph_[p.id()].size(); }

bool ShortImplicationsGraph::add(ImpType t, bool learnt, const Literal* lits) {
	uint32& stats= (t == ternary_imp ? tern_ : bin_)[learnt];
	Literal p = lits[0], q = lits[1], r = (t == ternary_imp ? lits[2] : lit_false());
	p.unflag(), q.unflag(), r.unflag();
	if (!shared_) {
		if (learnt) { p.flag(), q.flag(), r.flag(); }
		if (t == binary_imp) {
			getList(~p).push_left(q);
			getList(~q).push_left(p);
		}
		else {
			getList(~p).push_right(std::make_pair(q, r));
			getList(~q).push_right(std::make_pair(p, r));
			getList(~r).push_right(std::make_pair(p, q));
		}
		++stats;
		return true;
	}
#if CLASP_HAS_THREADS
	else if (learnt && !getList(~p).hasLearnt(q, r)) {
		getList(~p).addLearnt(q, r);
		getList(~q).addLearnt(p, r);
		if (t == ternary_imp) {
			getList(~r).addLearnt(p, q);
		}
		++stats;
		return true;
	}
#endif
	return false;
}

void ShortImplicationsGraph::remove_bin(ImplicationList& w, Literal p) {
	w.erase_left_unordered(std::find(w.left_begin(), w.left_end(), p));
	w.try_shrink();
}
void ShortImplicationsGraph::remove_tern(ImplicationList& w, Literal p) {
	w.erase_right_unordered(std::find_if(w.right_begin(), w.right_end(), PairContains<Literal>(p)));
	w.try_shrink();
}

// Removes all binary clauses containing p - those are now SAT.
// Binary clauses containing ~p are unit and therefore likewise SAT. Those
// are removed when their second literal is processed.
//
// Ternary clauses containing p are SAT and therefore removed.
// Ternary clauses containing ~p are now either binary or SAT. Those that
// are SAT are removed when the satisfied literal is processed.
// All conditional binary-clauses are replaced with real binary clauses.
// Note: clauses containing p watch ~p. Those containing ~p watch p.
void ShortImplicationsGraph::removeTrue(const Solver& s, Literal p) {
	assert(!shared_);
	typedef ImplicationList SWL;
	SWL& negPList = graph_[(~p).id()];
	SWL& pList    = graph_[ (p).id()];
	// remove every binary clause containing p -> clause is satisfied
	for (SWL::left_iterator it = negPList.left_begin(), end = negPList.left_end(); it != end; ++it) {
		--bin_[it->flagged()];
		remove_bin(graph_[(~*it).id()], p);
	}
	// remove every ternary clause containing p -> clause is satisfied
	for (SWL::right_iterator it = negPList.right_begin(), end = negPList.right_end(); it != end; ++it) {
		--tern_[it->first.flagged()];
		remove_tern(graph_[ (~it->first).id() ], p);
		remove_tern(graph_[ (~it->second).id() ], p);
	}
#if CLASP_HAS_THREADS
	FOR_EACH_LEARNT(negPList, imp) {
		graph_[(~imp[0]).id()].simplifyLearnt(s);
		if (!imp->flagged()){
			--tern_[1];
			graph_[(~imp[1]).id()].simplifyLearnt(s);
		}
		if (imp->flagged()) { --bin_[1]; }
	}
#endif
	// transform ternary clauses containing ~p to binary clause
	for (SWL::right_iterator it = pList.right_begin(), end = pList.right_end(); it != end; ++it) {
		Literal q = it->first;
		Literal r = it->second;
		--tern_[q.flagged()];
		remove_tern(graph_[(~q).id()], ~p);
		remove_tern(graph_[(~r).id()], ~p);
		if (s.value(q.var()) == value_free && s.value(r.var()) == value_free) {
			// clause is binary on dl 0
			Literal imp[2] = {q,r};
			add(binary_imp, false, imp);
		}
		// else: clause is SAT and removed when the satisfied literal is processed
	}
	graph_[(~p).id()].clear(true);
	graph_[ (p).id()].clear(true);
}
#undef FOR_EACH_LEARNT
struct ShortImplicationsGraph::Propagate {
	Propagate(Solver& a_s) : s(&a_s) {}
	bool unary(Literal p, Literal x) const { return s->isTrue(x) || s->force(x, Antecedent(p)); }
	bool binary(Literal p, Literal x, Literal y) const {
		ValueRep vx = s->value(x.var()), vy;
		if (vx != trueValue(x) && (vy=s->value(y.var())) != trueValue(y) && vx + vy) {
			return vx != 0 ? s->force(y, Antecedent(p, ~x)) : s->force(x, Antecedent(p, ~y));
		}
		return true;
	}
	Solver* s;
};
struct ShortImplicationsGraph::ReverseArc {
	ReverseArc(const Solver& a_s, uint32 m, Antecedent& o) : s(&a_s), out(&o), maxL(m) {}
	bool unary(Literal, Literal x) const {
		if (!isRevLit(*s, x, maxL)) { return true; }
		*out = Antecedent(~x);
		return false;
	}
	bool binary(Literal, Literal x, Literal y) const {
		if (!isRevLit(*s, x, maxL) || !isRevLit(*s, y, maxL)) { return true; }
		*out = Antecedent(~x, ~y);
		return false;
	}
	const Solver* s; Antecedent* out; uint32 maxL;
};
bool ShortImplicationsGraph::propagate(Solver& s, Literal p) const { return forEach(p, Propagate(s)); }
bool ShortImplicationsGraph::reverseArc(const Solver& s, Literal p, uint32 maxLev, Antecedent& out) const { return !forEach(p, ReverseArc(s, maxLev, out)); }
bool ShortImplicationsGraph::propagateBin(Assignment& out, Literal p, uint32 level) const {
	const ImplicationList& x = graph_[p.id()];
	Antecedent ante(p);
	for (ImplicationList::const_left_iterator it = x.left_begin(), end = x.left_end(); it != end; ++it) {
		if (!out.assign(*it, level, p)) { return false; }
	}
	return true;
}
/////////////////////////////////////////////////////////////////////////////////////////
// SatPreprocessor
/////////////////////////////////////////////////////////////////////////////////////////
SatPreprocessor::SatPreprocessor() : ctx_(0), opts_(0), elimTop_(0), seen_(1,1) {}
SatPreprocessor::~SatPreprocessor() {
	discardClauses(true);
}
void SatPreprocessor::discardClauses(bool full) {
	for (ClauseList::size_type i = 0; i != clauses_.size(); ++i) {
		if (clauses_[i]) { clauses_[i]->destroy(); }
	}
	ClauseList().swap(clauses_);
	if (Clause* r = (full ? elimTop_ : 0)) {
		do {
			Clause* t = r;
			 r = r->next();
			 t->destroy();
		} while (r);
		elimTop_ = 0;
	}
	if (full) { seen_ = Range32(1,1); }
}
void SatPreprocessor::cleanUp(bool full) {
	if (ctx_) { seen_.hi = ctx_->numVars() + 1; }
	doCleanUp();
	discardClauses(full);
}

bool SatPreprocessor::addClause(const Literal* lits, uint32 size) {
	if (size > 1) {
		clauses_.push_back( Clause::newClause(lits, size) );
	}
	else if (size == 1) {
		units_.push_back(lits[0]);
	}
	else {
		return false;
	}
	return true;
}

void SatPreprocessor::freezeSeen() {
	if (!ctx_->validVar(seen_.lo)) { seen_.lo = 1; }
	if (!ctx_->validVar(seen_.hi)) { seen_.hi = ctx_->numVars() + 1; }
	for (Var v = seen_.lo; v != seen_.hi; ++v) {
		if (!ctx_->eliminated(v)) { ctx_->setFrozen(v, true); }
	}
	seen_.lo = seen_.hi;
}

bool SatPreprocessor::preprocess(SharedContext& ctx, Options& opts) {
	ctx_  = &ctx;
	opts_ = &opts;
	Solver* s = ctx_->master();
	struct OnExit {
		SharedContext*   ctx;
		SatPreprocessor* self;
		SatPreprocessor* rest;
		OnExit(SatPreprocessor* s, SharedContext* c) : ctx(c), self(s), rest(0) {
			if (ctx && ctx->satPrepro.get() == s) { rest = ctx->satPrepro.release(); }
		}
		~OnExit() {
			if (self) self->cleanUp();
			if (rest) ctx->satPrepro.reset(rest);
		}
	} onExit(this, &ctx);
	for (LitVec::const_iterator it = units_.begin(), end = units_.end(); it != end; ++it) {
		if (!ctx.addUnary(*it)) return false;
	}
	units_.clear();
	// skip preprocessing if other constraints are UNSAT
	if (!s->propagate()) return false;
	if (ctx.preserveModels()) {
		opts.disableBce();
	}
	if (ctx.preserveShown()) {
		for (OutputTable::pred_iterator it = ctx.output.pred_begin(), end = ctx.output.pred_end(); it != end; ++it) {
			ctx.setFrozen(it->cond.var(), true);
		}
		for (OutputTable::range_iterator it = ctx.output.vars_begin(), end = ctx.output.vars_end(); it != end; ++it) {
			ctx.setFrozen(*it, true);
		}
	}
	// preprocess only if not too many vars are frozen or not too many clauses
	bool limFrozen = false;
	if (opts.limFrozen != 0 && ctx_->stats().vars.frozen) {
		uint32 varFrozen = ctx_->stats().vars.frozen;
		for (LitVec::const_iterator it = s->trail().begin(), end = s->trail().end(); it != end; ++it) {
 			varFrozen -= (ctx_->varInfo(it->var()).frozen());
 		}
		limFrozen = ((varFrozen / double(s->numFreeVars())) * 100.0) > double(opts.limFrozen);
	}
	// 1. remove SAT-clauses, strengthen clauses w.r.t false literals, attach
	if (opts.type != 0 && !opts.clauseLimit(numClauses()) && !limFrozen && initPreprocess(opts)) {
		ClauseList::size_type j = 0;
		for (ClauseList::size_type i = 0; i != clauses_.size(); ++i) {
			Clause* c   = clauses_[i]; assert(c);
			clauses_[i] = 0;
			c->simplify(*s);
			Literal x   = (*c)[0];
			if (s->value(x.var()) == value_free) {
				clauses_[j++] = c;
			}
			else {
				c->destroy();
				if (!ctx.addUnary(x)) { return false; }
			}
		}
		clauses_.erase(clauses_.begin()+j, clauses_.end());
		// 2. run preprocessing
		freezeSeen();
		if (!s->propagate() || !doPreprocess()) {
			return false;
		}
	}
	// simplify other constraints w.r.t any newly derived top-level facts
	if (!s->simplify()) return false;
	// 3. move preprocessed clauses to ctx
	for (ClauseList::size_type i = 0; i != clauses_.size(); ++i) {
		if (Clause* c = clauses_[i]) {
			if (!ClauseCreator::create(*s, ClauseRep::create(&(*c)[0], c->size()), 0)) {
				return false;
			}
			clauses_[i] = 0;
			c->destroy();
		}
	}
	ClauseList().swap(clauses_);
	return true;
}
bool SatPreprocessor::preprocess(SharedContext& ctx) {
	SatPreParams opts = ctx.configuration()->context().satPre;
	return preprocess(ctx, opts);
}
void SatPreprocessor::extendModel(ValueVec& m, LitVec& open) {
	if (!open.empty()) {
		// flip last unconstraint variable to get "next" model
		open.back() = ~open.back();
	}
	doExtendModel(m, open);
	// remove unconstraint vars already flipped
	while (!open.empty() && open.back().sign()) {
		open.pop_back();
	}
}
SatPreprocessor::Clause* SatPreprocessor::Clause::newClause(const Literal* lits, uint32 size) {
	assert(size > 0);
	void* mem = ::operator new( sizeof(Clause) + (size-1)*sizeof(Literal) );
	return new (mem) Clause(lits, size);
}
SatPreprocessor::Clause::Clause(const Literal* lits, uint32 size) : size_(size), inQ_(0), marked_(0) {
	std::memcpy(lits_, lits, size*sizeof(Literal));
}
void SatPreprocessor::Clause::strengthen(Literal p) {
	uint64 a = 0;
	uint32 i, end;
	for (i   = 0; lits_[i] != p; ++i) { a |= Clause::abstractLit(lits_[i]); }
	for (end = size_-1; i < end; ++i) { lits_[i] = lits_[i+1]; a |= Clause::abstractLit(lits_[i]); }
	--size_;
	data_.abstr = a;
}
void SatPreprocessor::Clause::simplify(Solver& s) {
	uint32 i;
	for (i = 0; i != size_ && s.value(lits_[i].var()) == value_free; ++i) {;}
	if      (i == size_)        { return; }
	else if (s.isTrue(lits_[i])){ std::swap(lits_[i], lits_[0]); return;  }
	uint32 j = i++;
	for (; i != size_; ++i) {
		if (s.isTrue(lits_[i]))   { std::swap(lits_[i], lits_[0]); return;  }
		if (!s.isFalse(lits_[i])) { lits_[j++] = lits_[i]; }
	}
	size_ = j;
}
void SatPreprocessor::Clause::destroy() {
	void* mem = this;
	this->~Clause();
	::operator delete(mem);
}
/////////////////////////////////////////////////////////////////////////////////////////
// ConstString
/////////////////////////////////////////////////////////////////////////////////////////
struct StrRef {
	typedef Atomic_t<uint32>::type RefCount;
	static uint64      empty() { return 0; }
	static uint64      create(const char* str, std::size_t len) {
		char*   mem = (char*)malloc(sizeof(RefCount) + len + 1);
		RefCount* p = new (mem) RefCount();
		std::memcpy(mem + sizeof(RefCount), str, len);
		mem[sizeof(RefCount) + len] = 0;
		*p = 1;
		return static_cast<uint64>(reinterpret_cast<uintp>(mem));
	}
	static RefCount*   asShared(uint64 ref) {
		return ref ? reinterpret_cast<RefCount*>(static_cast<uintp>(ref)) : 0;
	}
	static uint64      share(uint64 ref) {
		if (RefCount* p = asShared(ref)) { ++*p; }
		return ref;
	}
	static uint64      release(uint64 ref) {
		RefCount* p = asShared(ref);
		if (p && !--*p) {
			p->~RefCount();
			free((void*)p);
		}
		return 0;
	}
	static const char* get(uint64 ref) {
		return ref ? reinterpret_cast<const char*>(static_cast<uintp>(ref + sizeof(RefCount))) : "";
	}
};
ConstString::ConstString(const char* str) : ref_(str && *str ? StrRef::create(str, std::strlen(str)) : StrRef::empty()) {}
ConstString::ConstString(const StrView& str) : ref_(str.size ? StrRef::create(str.first, str.size) : StrRef::empty()) {}
ConstString::ConstString(const ConstString& other) : ref_(StrRef::share(other.ref_)) { }
ConstString::~ConstString() {  StrRef::release(ref_); }
void ConstString::swap(ConstString& rhs) { std::swap(ref_, rhs.ref_); }
ConstString& ConstString::operator=(const ConstString& rhs) {
	ConstString temp(rhs);
	swap(temp);
	return *this;
}
const char* ConstString::c_str() const { return StrRef::get(ref_); }
/////////////////////////////////////////////////////////////////////////////////////////
// OutputTable
/////////////////////////////////////////////////////////////////////////////////////////
OutputTable::OutputTable() : theory(0), vars_(0, 0), projMode_(0), hide_(0) {}
OutputTable::~OutputTable() {
	PodVector<NameType>::destruct(facts_);
	PodVector<PredType>::destruct(preds_);
}

void OutputTable::setFilter(char c) {
	hide_ = c;
}
bool OutputTable::filter(const NameType& n) const {
	char c = *n;
	return c == hide_ || !c;
}
bool OutputTable::add(const NameType& fact) {
	if (!filter(fact)) {
		facts_.push_back(fact);
		return true;
	}
	return false;
}

bool OutputTable::add(const NameType& n, Literal c, uint32 u) {
	if (!filter(n)) {
		PredType p = {n, c, u};
		preds_.push_back(p);
		return true;
	}
	return false;
}

void OutputTable::setVarRange(const RangeType& r) {
	POTASSCO_ASSERT(r.lo <= r.hi);
	vars_ = r;
}
void OutputTable::setProjectMode(ProjectMode m) {
	projMode_ = m;
}
void OutputTable::addProject(Literal x) {
	proj_.push_back(x);
}

uint32 OutputTable::size() const {
	return numFacts() + numPreds() + numVars();
}
OutputTable::Theory::~Theory() {}
/////////////////////////////////////////////////////////////////////////////////////////
// DomainTable
/////////////////////////////////////////////////////////////////////////////////////////
DomainTable::ValueType::ValueType(Var v, DomModType t, int16 bias, uint16 prio, Literal cond)
	: cond_(cond.id())
	, comp_(t == DomModType::True || t == DomModType::False)
	, var_(v)
	, type_(uint32(t) <= 3u ? t : uint32(t == DomModType::False))
	, bias_(bias)
	, prio_(prio) {
}
DomModType DomainTable::ValueType::type() const { return static_cast<DomModType>(comp_ == 0 ? type_ : uint32(DomModType::True + type_)); }
DomainTable::DomainTable() : assume(0), seen_(0) {}
void DomainTable::add(Var v, DomModType t, int16 b, uint16 p, Literal c) {
	if (c != lit_false() && (t != DomModType::Init || c == lit_true())) {
		entries_.push_back(ValueType(v, t, b, p, c));
	}
}
uint32 DomainTable::simplify() {
	if (seen_ >= size()) { return size(); }
	std::stable_sort(entries_.begin() + seen_, entries_.end(), cmp);
	DomVec::iterator j = entries_.begin() + seen_;
	for (DomVec::const_iterator it = j, end = entries_.end(), n; it != end; it = n) {
		Var     v = it->var();
		Literal c = it->cond();
		for (n = it + 1; n != end && n->var() == v && n->cond() == c; ) { ++n; }
		if ((n - it) == 1) {
			*j++ = *it;
		}
		else {
			static_assert(DomModType::Level == 0 && DomModType::Sign == 1 && DomModType::True == 4, "check enumeration constants");
			enum { n_simp = 4u };
			int const mod_level = DomModType::Level, mod_sign = DomModType::Sign;
			int16 const NO_BIAS = INT16_MAX;
			uint16 prio[n_simp] ={0, 0, 0, 0};
			int16  bias[n_simp] ={NO_BIAS, NO_BIAS, NO_BIAS, NO_BIAS};
			for (; it != n; ++it) {
				if (!it->comp() && it->prio() >= prio[it->type()]) {
					bias[it->type()] = it->bias();
					prio[it->type()] = it->prio();
				}
				else if (it->comp()) {
					if (it->prio() >= prio[mod_level]) {
						bias[mod_level] = it->bias();
						prio[mod_level] = it->prio();
					}
					if (it->prio() >= prio[mod_sign]) {
						bias[mod_sign] = it->type() == DomModType::True ? 1 : -1;
						prio[mod_sign] = it->prio();
					}
				}
			}
			int s = 0;
			if (bias[mod_level] != NO_BIAS && bias[mod_sign] != NO_BIAS && bias[mod_sign] && prio[mod_level] == prio[mod_sign]) {
				*j++ = ValueType(v, bias[mod_sign] > 0 ? DomModType::True : DomModType::False, bias[mod_level], prio[mod_level], c);
				s = mod_sign + 1;
			}
			for (int t = s; t != n_simp; ++t) {
				if (bias[t] != NO_BIAS) {
					*j++ = ValueType(v, static_cast<DomModType>(t), bias[t], prio[t], c);
				}
			}
		}
	}
	entries_.erase(j, entries_.end());
	if (entries_.capacity() > static_cast<std::size_t>(entries_.size() * 1.75)) {
		DomVec(entries_).swap(entries_);
	}
	return (seen_ = size());
}
void DomainTable::reset() {
	DomVec().swap(entries_);
	assume = 0;
}
DomainTable::DefaultAction::~DefaultAction() {}
void DomainTable::applyDefault(const SharedContext& ctx, DefaultAction& act, uint32 defFilter) {
	if ((defFilter & HeuParams::pref_show) != 0 || !defFilter) {
		const HeuParams::DomPref pref = defFilter ? HeuParams::pref_show : HeuParams::pref_atom;
		OutputTable::RangeType   vars = defFilter ? ctx.output.vars_range() : Range32(1, ctx.numVars()+1);
		for (OutputTable::pred_iterator it = ctx.output.pred_begin(), end = ctx.output.pred_end(); it != end; ++it) {
			if (defFilter || (it->cond.sign() && it->user && Potassco::atom(Potassco::lit(it->user)) < Asp::PrgNode::noNode)) {
				act.atom(it->cond, pref, pref);
			}
		}
		for (Var v = vars.lo; v != vars.hi; ++v) {
			if (Var_t::isAtom(ctx.varInfo(v).type())) { act.atom(posLit(v), pref, pref); }
		}
	}
	if ((defFilter & HeuParams::pref_min) != 0 && ctx.minimizeNoCreate()) {
		weight_t lastW = -1; uint32 strat = HeuParams::pref_show;
		for (const WeightLiteral* it = ctx.minimizeNoCreate()->lits; !isSentinel(it->first); ++it) {
			if (it->second != lastW && strat > HeuParams::pref_disj) { --strat; lastW = it->second; }
			act.atom(it->first, HeuParams::pref_min, strat);
		}
	}
	const uint32 gs = (uint32(HeuParams::pref_scc) | HeuParams::pref_hcc | HeuParams::pref_disj) & defFilter;
	if (ctx.sccGraph.get() && gs && ((gs & HeuParams::pref_scc) != 0 || ctx.sccGraph->numNonHcfs())) {
		for (uint32 i = 0; i != ctx.sccGraph->numAtoms(); ++i) {
			const PrgDepGraph::AtomNode& a = ctx.sccGraph->getAtom(i);
			if      ((gs & HeuParams::pref_disj) != 0 && a.inDisjunctive()) { act.atom(a.lit, HeuParams::pref_disj, 3u); }
			else if ((gs & HeuParams::pref_hcc)  != 0 && a.inNonHcf())      { act.atom(a.lit, HeuParams::pref_hcc, 2u);  }
			else if ((gs & HeuParams::pref_scc)  != 0)                      { act.atom(a.lit, HeuParams::pref_scc, 1u);  }
		}
	}
}
bool   DomainTable::empty() const { return entries_.empty(); }
uint32 DomainTable::size()  const { return static_cast<uint32>(entries_.size()); }
DomainTable::iterator DomainTable::begin() const { return entries_.begin(); }
DomainTable::iterator DomainTable::end()   const { return entries_.end(); }
/////////////////////////////////////////////////////////////////////////////////////////
// SharedContext::Minimize
/////////////////////////////////////////////////////////////////////////////////////////
struct SharedContext::Minimize {
	typedef SingleOwnerPtr<SharedMinimizeData, ReleaseObject> ProductPtr;
	void add(weight_t p, const WeightLiteral& lit) {
		builder.add(p, lit);
	}
	bool reset() const {
		if (product.get()) { product->resetBounds(); }
		return true;
	}
	SharedMinimizeData* get(SharedContext& ctx) {
		if (builder.empty()) { return product.get(); }
		if (product.get()) {
			builder.add(*product);
			product = 0;
		}
		return (product = builder.build(ctx)).get();
	}
	MinimizeBuilder builder;
	ProductPtr      product;
};
/////////////////////////////////////////////////////////////////////////////////////////
// SharedContext
/////////////////////////////////////////////////////////////////////////////////////////
static BasicSatConfig config_def_s;
SharedContext::SharedContext()
	: mini_(0), progress_(0), lastTopLevel_(0) {
	// sentinel always present
	setFrozen(addVar(Var_t::Atom, 0), true);
	stats_.vars.num = 0;
	config_ = &config_def_s;
	config_.release();
	pushSolver();
}

bool SharedContext::ok() const { return master()->decisionLevel() || !master()->hasConflict() || master()->hasStopConflict(); }
void SharedContext::enableStats(uint32 lev) {
	if (lev > 0) { master()->stats.enableExtended(); }
}
SharedContext::~SharedContext() {
	while (!solvers_.empty()) { delete solvers_.back(); solvers_.pop_back(); }
	delete mini_;
}

void SharedContext::reset() {
	this->~SharedContext();
	new (this) SharedContext();
}

void SharedContext::setConcurrency(uint32 n, ResizeMode mode) {
	if (n <= 1) { share_.count = 1; }
	else        { share_.count = n; solvers_.reserve(n); }
	while (solvers_.size() < share_.count && (mode & resize_push) != 0u) {
		pushSolver();
	}
	while (solvers_.size() > share_.count && (mode & resize_pop) != 0u) {
		delete solvers_.back();
		solvers_.pop_back();
	}
	if ((share_.shareM & ContextParams::share_auto) != 0) {
		setShareMode(ContextParams::share_auto);
	}
}

void SharedContext::setShareMode(ContextParams::ShareMode m) {
	if ( (share_.shareM = static_cast<uint32>(m)) == ContextParams::share_auto && share_.count > 1) {
		share_.shareM |= static_cast<uint32>(ContextParams::share_all);
	}
}
void SharedContext::setShortMode(ContextParams::ShortMode m) {
	share_.shortM = static_cast<uint32>(m);
}

void SharedContext::setPreproMode(uint32 m, bool b) {
	share_.satPreM &= ~m;
	if (b) { share_.satPreM |= m; }
}

Solver& SharedContext::pushSolver() {
	uint32 id    = (uint32)solvers_.size();
	share_.count = std::max(share_.count, id + 1);
	Solver* s    = new Solver(this, id);
	solvers_.push_back(s);
	return *s;
}

void SharedContext::setConfiguration(Configuration* c, Ownership_t::Type ownership) {
	bool own = ownership == Ownership_t::Acquire;
	if (c == 0) { c = &config_def_s; own = false; }
	report(Event::subsystem_facade);
	if (config_.get() != c) {
		config_ = c;
		if (!own) config_.release();
		config_->prepare(*this);
		const ContextParams& opts = config_->context();
		setShareMode(static_cast<ContextParams::ShareMode>(opts.shareMode));
		setShortMode(static_cast<ContextParams::ShortMode>(opts.shortMode));
		share_.seed   = opts.seed;
		if (satPrepro.get() == 0 && opts.satPre.type != SatPreParams::sat_pre_no) {
			satPrepro.reset(SatPreParams::create(opts.satPre));
		}
		enableStats(opts.stats);
		// force update on next call to Solver::startInit()
		for (uint32 i = 0; i != solvers_.size(); ++i) {
			solvers_[i]->resetConfig();
		}
	}
	else if (own != config_.is_owner()) {
		if (own) config_.acquire();
		else     config_.release();
	}
}

bool SharedContext::unfreeze() {
	if (frozen()) {
		share_.frozen = 0;
		share_.winner = 0;
		heuristic.assume = 0;
		btig_.markShared(false);
		return master()->popRootLevel(master()->rootLevel())
		  &&   btig_.propagate(*master(), lit_true()) // any newly learnt facts
		  &&   unfreezeStep()
		  &&   (!mini_ || mini_->reset());
	}
	return true;
}

bool SharedContext::unfreezeStep() {
	POTASSCO_ASSERT(!frozen());
	Var tag = step_.var();
	for (SolverVec::size_type i = solvers_.size(); i--;) {
		Solver& s = *solvers_[i];
		if (!s.validVar(tag)) { continue; }
		s.endStep(lastTopLevel_, configuration()->solver(s.id()));
	}
	if (tag) {
		varInfo_[tag] = VarInfo();
		step_ = lit_false();
		popVars(1);
		++stats_.vars.num;
	}
	return !master()->hasConflict();
}

Var SharedContext::addVars(uint32 nVars, VarType t, uint8 flags) {
	flags &= ~3u;
	flags |= VarInfo::flags(t);
	varInfo_.insert(varInfo_.end(), nVars, VarInfo(flags));
	stats_.vars.num += nVars;
	return static_cast<Var>(varInfo_.size() - nVars);
}

void SharedContext::popVars(uint32 nVars) {
	POTASSCO_REQUIRE(!frozen(), "Cannot pop vars from frozen program");
	POTASSCO_CHECK(nVars <= numVars(), EINVAL, POTASSCO_FUNC_NAME);
	uint32 newVars = numVars() - nVars;
	uint32 comVars = master()->numVars();
	if (newVars >= comVars) {
		// vars not yet committed
		varInfo_.erase(varInfo_.end() - nVars, varInfo_.end());
		stats_.vars.num -= nVars;
	}
	else {
		for (Var v = numVars(); v && nVars; --nVars, --v) {
			stats_.vars.eliminated -= eliminated(v);
			stats_.vars.frozen -= varInfo(v).frozen();
			--stats_.vars.num;
			varInfo_.pop_back();
		}
		btig_.resize((numVars()+1)<<1);
		for (SolverVec::size_type i = solvers_.size(); i--;) {
			solvers_[i]->updateVars();
		}
		lastTopLevel_ = std::min(lastTopLevel_, master()->assign_.front);
	}
}

void SharedContext::setSolveMode(SolveMode m) { share_.solveM = m; }
void SharedContext::requestStepVar() { if (step_ == lit_true()) { step_ = lit_false(); } }
void SharedContext::setFrozen(Var v, bool b) {
	assert(validVar(v));
	if (v && b != varInfo_[v].has(VarInfo::Frozen)) {
		varInfo_[v].toggle(VarInfo::Frozen);
		b ? ++stats_.vars.frozen : --stats_.vars.frozen;
	}
}

bool SharedContext::eliminated(Var v) const {
	assert(validVar(v));
	return !master()->assign_.valid(v);
}

void SharedContext::eliminate(Var v) {
	assert(validVar(v) && !frozen() && master()->decisionLevel() == 0);
	if (!eliminated(v)) {
		++stats_.vars.eliminated;
		// eliminate var from assignment - no longer a decision variable!
		master()->assign_.eliminate(v);
	}
}

Literal SharedContext::addStepLit() {
	VarInfo nv; nv.set(VarInfo::Frozen);
	varInfo_.push_back(nv);
	btig_.resize((numVars() + 1) << 1);
	return posLit(master()->pushAuxVar());
}
Solver& SharedContext::startAddConstraints(uint32 constraintGuess) {
	if (!unfreeze()) { return *master(); }
	btig_.resize((numVars() + 1 + uint32(step_ == lit_false() || solveMode() == solve_multi))<<1);
	master()->startInit(constraintGuess, configuration()->solver(0));
	return *master();
}
bool SharedContext::addUnary(Literal x) {
	POTASSCO_REQUIRE(!frozen() || !isShared());
	master()->acquireProblemVar(x.var());
	return master()->force(x);
}
bool SharedContext::addBinary(Literal x, Literal y) {
	POTASSCO_REQUIRE(allowImplicit(Constraint_t::Static));
	Literal lits[2] = {x, y};
	return ClauseCreator::create(*master(), ClauseRep(lits, 2), ClauseCreator::clause_force_simplify);
}
bool SharedContext::addTernary(Literal x, Literal y, Literal z) {
	POTASSCO_REQUIRE(allowImplicit(Constraint_t::Static));
	Literal lits[3] = {x, y, z};
	return ClauseCreator::create(*master(), ClauseRep(lits, 3), ClauseCreator::clause_force_simplify);
}
void SharedContext::add(Constraint* c) {
	POTASSCO_REQUIRE(!frozen());
	master()->add(c);
}
void SharedContext::addMinimize(WeightLiteral x, weight_t p) {
	if (!mini_) { mini_ = new Minimize(); }
	mini_->add(p, x);
}
bool SharedContext::hasMinimize() const {
	return mini_ != 0;
}
void SharedContext::removeMinimize() {
	delete mini_;
	mini_ = 0;
}
SharedMinimizeData* SharedContext::minimize() {
	return mini_ ? mini_->get(*this) : 0;
}
SharedMinimizeData* SharedContext::minimizeNoCreate() const {
	return mini_ ? mini_->product.get() : 0;
}
int SharedContext::addImp(ImpGraph::ImpType t, const Literal* lits, ConstraintType ct) {
	if (!allowImplicit(ct)) { return -1; }
	bool learnt = ct != Constraint_t::Static;
	if (!learnt && !frozen() && satPrepro.get()) {
		satPrepro->addClause(lits, static_cast<uint32>(t));
		return 1;
	}
	return int(btig_.add(t, learnt, lits));
}

uint32 SharedContext::numConstraints() const { return numBinary() + numTernary() + sizeVec(master()->constraints_); }

bool SharedContext::endInit(bool attachAll) {
	assert(!frozen());
	report(Event::subsystem_prepare);
	initStats(*master());
	heuristic.simplify();
	SatPrePtr temp;
	satPrepro.swap(temp);
	bool ok = !master()->hasConflict() && master()->preparePost() && (!temp.get() || temp->preprocess(*this)) && master()->endInit();
	satPrepro.swap(temp);
	master()->dbIdx_ = (uint32)master()->constraints_.size();
	lastTopLevel_    = (uint32)master()->assign_.front;
	stats_.constraints.other  = sizeVec(master()->constraints_);
	stats_.constraints.binary = btig_.numBinary();
	stats_.constraints.ternary= btig_.numTernary();
	stats_.acycEdges          = extGraph.get() ? extGraph->edges() : 0;
	stats_.complexity         = std::max(stats_.complexity, problemComplexity());
	if (ok && step_ == lit_false()) {
		step_ = addStepLit();
	}
	btig_.markShared(concurrency() > 1);
	share_.frozen = 1;
	if (ok && master()->getPost(PostPropagator::priority_class_general))
		ok = master()->propagate() && master()->simplify();
	for (uint32 i = ok && attachAll ? 1 : concurrency(); i != concurrency(); ++i) {
		if (!hasSolver(i)) { pushSolver(); }
		if (!attach(i))    { ok = false; break; }
	}
	return ok || (detach(*master(), false), master()->setStopConflict(), false);
}

bool SharedContext::attach(Solver& other) {
	assert(frozen() && other.shared_ == this);
	if (other.validVar(step_.var())) {
		if (!other.popRootLevel(other.rootLevel())){ return false; }
		if (&other == master())                    { return true;  }
	}
	initStats(other);
	// 1. clone vars & assignment
	Var lastVar = other.numVars();
	other.startInit(static_cast<uint32>(master()->constraints_.size()), configuration()->solver(other.id()));
	if (other.hasConflict()) { return false; }
	Antecedent null;
	for (LitVec::size_type i = 0, end = master()->trail().size(); i != end; ++i) {
		Literal x = master()->trail()[i];
		if (master()->auxVar(x.var())) { continue;  }
		if (!other.force(x, null))     { return false; }
	}
	for (Var v = satPrepro.get() ? lastVar+1 : varMax, end = master()->numVars(); v <= end; ++v) {
		if (eliminated(v) && other.value(v) == value_free) {
			other.assign_.eliminate(v);
		}
	}
	if (other.constraints_.empty()) { other.lastSimp_ = master()->lastSimp_; }
	// 2. clone & attach constraints
	if (!other.cloneDB(master()->constraints_)) { return false; }
	Constraint* c = master()->enumerationConstraint();
	other.setEnumerationConstraint( c ? c->cloneAttach(other) : 0 );
	// 3. endInit
	return (other.preparePost() && other.endInit()) || (detach(other, false), false);
}

void SharedContext::detach(Solver& s, bool reset) {
	assert(s.shared_ == this);
	if (reset) { s.reset(); }
	s.setEnumerationConstraint(0);
	s.popAuxVar();
}
void SharedContext::initStats(Solver& s) const {
	s.stats.enable(master()->stats);
	s.stats.reset();
}
SolverStats& SharedContext::solverStats(uint32 sId) const {
	POTASSCO_ASSERT(hasSolver(sId), "solver id out of range");
	return solver(sId)->stats;
}
const SolverStats& SharedContext::accuStats(SolverStats& out) const {
	for (uint32 i = 0; i != solvers_.size(); ++i) {
		out.accu(solvers_[i]->stats, true);
	}
	return out;
}
void SharedContext::warn(const char* what) const {
	if (progress_) {
		progress_->dispatch(LogEvent(progress_->active(), Event::verbosity_quiet, LogEvent::Warning, 0, what));
	}
}
void SharedContext::report(const char* what, const Solver* s) const {
	if (progress_) {
		progress_->dispatch(LogEvent(progress_->active(), Event::verbosity_high, LogEvent::Message, s, what));
	}
}
void SharedContext::report(Event::Subsystem sys) const {
	if (progress_ && progress_->setActive(sys)) {
		const char* m = "";
		Event::Verbosity v = Event::verbosity_high;
		switch(sys) {
			default: return;
			case Event::subsystem_load:    m = "Reading";       break;
			case Event::subsystem_prepare: m = "Preprocessing"; break;
			case Event::subsystem_solve:   m = "Solving"; v = Event::verbosity_low; break;
		}
		progress_->onEvent(LogEvent(sys, v, LogEvent::Message, 0, m));
	}
}
void SharedContext::simplify(LitVec::size_type trailStart, bool shuffle) {
	if (!isShared() && trailStart < master()->trail().size()) {
		for (const LitVec& trail = master()->trail(); trailStart != trail.size(); ++trailStart) {
			Literal p = trail[trailStart];
			if (p.id() < btig_.size()) { btig_.removeTrue(*master(), p); }
		}
	}
	Solver::ConstraintDB& db = master()->constraints_;
	if (concurrency() == 1 || master()->dbIdx_ == 0) {
		Clasp::simplifyDB(*master(), db, shuffle);
	}
	else {
		uint32 rem = 0;
		for (Solver::ConstraintDB::size_type i = 0, end = db.size(); i != end; ++i) {
			Constraint* c = db[i];
			if (c->simplify(*master(), shuffle)) { c->destroy(master(), false); db[i] = 0; ++rem; }
		}
		if (rem) {
			for (SolverVec::size_type s = 1; s != solvers_.size(); ++s) {
				Solver& x = *solvers_[s];
				POTASSCO_ASSERT(x.dbIdx_ <= db.size(), "Invalid DB idx!");
				if      (x.dbIdx_ == db.size()) { x.dbIdx_ -= rem; }
				else if (x.dbIdx_ != 0)         { x.dbIdx_ -= (uint32)std::count_if(db.begin(), db.begin()+x.dbIdx_, IsNull()); }
			}
			db.erase(std::remove_if(db.begin(), db.end(), IsNull()), db.end());
		}
	}
	master()->dbIdx_ = sizeVec(db);
}
void SharedContext::removeConstraint(uint32 idx, bool detach) {
	Solver::ConstraintDB& db = master()->constraints_;
	POTASSCO_REQUIRE(idx < db.size());
	Constraint* c = db[idx];
	for (SolverVec::size_type s = 1; s != solvers_.size(); ++s) {
		Solver& x = *solvers_[s];
		x.dbIdx_ -= (idx < x.dbIdx_);
	}
	db.erase(db.begin()+idx);
	master()->dbIdx_ = sizeVec(db);
	c->destroy(master(), detach);
}


uint32 SharedContext::problemComplexity() const {
	if (isExtended()) {
		uint32 r = numBinary() + numTernary();
		for (uint32 i = 0; i != master()->constraints_.size(); ++i) {
			r += master()->constraints_[i]->estimateComplexity(*master());
		}
		return r;
	}
	return numConstraints();
}
/////////////////////////////////////////////////////////////////////////////////////////
// Distributor
/////////////////////////////////////////////////////////////////////////////////////////
Distributor::Distributor(const Policy& p) : policy_(p)  {}
Distributor::~Distributor() {}

}
