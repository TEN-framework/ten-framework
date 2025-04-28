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
#include <clasp/clasp_facade.h>
#include <clasp/lookahead.h>
#include <clasp/dependency_graph.h>
#include <clasp/minimize_constraint.h>
#include <clasp/unfounded_check.h>
#include <clasp/parser.h>
#include <clasp/clingo.h>
#include <clasp/util/timer.h>
#include <potassco/string_convert.h>
#include <cstdio>
#include <cstdlib>
#include <signal.h>
#include <climits>
#include <utility>
#if CLASP_HAS_THREADS
#include <clasp/mt/mutex.h>
#include <clasp/mt/thread.h>
#endif
namespace Clasp {
/////////////////////////////////////////////////////////////////////////////////////////
// ClaspConfig
/////////////////////////////////////////////////////////////////////////////////////////
struct ClaspConfig::Impl {
	struct ConfiguratorProxy {
		enum { OnceBit = 62, AcquireBit = 61 };
		ConfiguratorProxy(Configurator* c, Ownership_t::Type own, bool once) : cfg(reinterpret_cast<uintp>(c)), set(0) {
			if (once) { store_set_bit(cfg, OnceBit); }
			if (own == Ownership_t::Acquire) { store_set_bit(cfg, AcquireBit); }
			assert(ptr() == c);
		}
		bool applyConfig(Solver& s) {
			POTASSCO_ASSERT(s.id() < 64, "invalid solver id!");
			if (test_bit(set, s.id()))  { return true;  }
			if (test_bit(cfg, OnceBit)) { store_set_bit(set, s.id()); }
			return this->ptr()->applyConfig(s);
		}
		void prepare(SharedContext& ctx) {
			if (ctx.concurrency() < 64) {
				set &= (bit_mask<uint64>(ctx.concurrency()) - 1);
			}
			ptr()->prepare(ctx);
		}
		void unfreeze(SharedContext& ctx) {
			ptr()->unfreeze(ctx);
		}
		void destroy() { if (test_bit(cfg, AcquireBit)) { delete ptr(); } }
		Configurator* ptr() const {
			static const uint64 ptrMask = ~(bit_mask<uint64>(OnceBit) | bit_mask<uint64>(AcquireBit));
			return reinterpret_cast<Configurator*>(static_cast<uintp>(cfg & ptrMask));
		}
		uint64 cfg;
		uint64 set;
	};
	typedef PodVector<ConfiguratorProxy>::type PPVec;
	Impl()  { acycSet = 0; }
	~Impl() { reset(); }
	void reset();
	void prepare(SharedContext& ctx);
	bool addPost(Solver& s, const SolverParams& opts);
	void add(Configurator* c, Ownership_t::Type t, bool once) { pp.push_back(ConfiguratorProxy(c, t, once)); }
	void unfreeze(SharedContext& ctx);
	PPVec   pp;
	uint64  acycSet;
#if CLASP_HAS_THREADS
	Clasp::mt::mutex mutex;
#endif
};
void ClaspConfig::Impl::reset() {
	for (; !pp.empty(); pp.pop_back()) { pp.back().destroy(); }
}

void ClaspConfig::Impl::prepare(SharedContext& ctx) {
	if (ctx.concurrency() < 64) {
		acycSet &= (bit_mask<uint64>(ctx.concurrency()) - 1);
	}
	for (PPVec::iterator it = pp.begin(), end = pp.end(); it != end; ++it) {
		it->prepare(ctx);
	}
}
bool ClaspConfig::Impl::addPost(Solver& s, const SolverParams& opts) {
#if CLASP_HAS_THREADS
#define LOCKED() for (Clasp::mt::unique_lock<Clasp::mt::mutex> lock(mutex); lock.owns_lock(); lock.unlock())
#else
#define LOCKED()
#endif
	POTASSCO_ASSERT(s.sharedContext() != 0, "Solver not attached!");
	if (s.sharedContext()->sccGraph.get()) {
		if (DefaultUnfoundedCheck* ufs = static_cast<DefaultUnfoundedCheck*>(s.getPost(PostPropagator::priority_reserved_ufs))) {
			ufs->setReasonStrategy(static_cast<DefaultUnfoundedCheck::ReasonStrategy>(opts.loopRep));
		}
		else if (!s.addPost(new DefaultUnfoundedCheck(*s.sharedContext()->sccGraph, static_cast<DefaultUnfoundedCheck::ReasonStrategy>(opts.loopRep)))) {
			return false;
		}
	}
	if (s.sharedContext()->extGraph.get()) {
		bool addAcyc = false;
		// protect access to acycSet
		LOCKED() { addAcyc = !test_bit(acycSet, s.id()) && store_set_bit(acycSet, s.id()); }
		if (addAcyc && !s.addPost(new AcyclicityCheck(s.sharedContext()->extGraph.get()))) {
			return false;
		}
	}
	for (PPVec::iterator it = pp.begin(), end = pp.end(); it != end; ++it) {
		// protect call to user code
		LOCKED() { if (!it->applyConfig(s)) { return false; } }
	}
	return true;
#undef LOCKED
}
void ClaspConfig::Impl::unfreeze(SharedContext& ctx) {
	for (PPVec::iterator it = pp.begin(), end = pp.end(); it != end; ++it) {
		it->unfreeze(ctx);
	}
}

ClaspConfig::ClaspConfig() : tester_(0), impl_(new Impl()) {}
ClaspConfig::~ClaspConfig() {
	delete impl_;
	delete tester_;
}

void ClaspConfig::reset() {
	if (tester_) { tester_->reset(); }
	impl_->reset();
	BasicSatConfig::reset();
	solve = SolveOptions();
	asp   = AspOptions();
}

BasicSatConfig* ClaspConfig::addTesterConfig() {
	if (!tester_) { tester_ = new BasicSatConfig(); }
	return tester_;
}

void ClaspConfig::prepare(SharedContext& ctx) {
	BasicSatConfig::prepare(ctx);
	uint32 numS = solve.numSolver();
	if (numS > solve.supportedSolvers()) {
		ctx.warn("Too many solvers.");
		numS = solve.supportedSolvers();
	}
	if (numS > solve.recommendedSolvers()) {
		ctx.warn(POTASSCO_FORMAT("Oversubscription: #Threads=%u exceeds logical CPUs=%u.", numS, solve.recommendedSolvers()));
	}
	for (uint32 i = 0; i != numS; ++i) {
		if (solver(i).heuId == Heuristic_t::Domain) {
			parse.enableHeuristic();
			break;
		}
	}
	solve.setSolvers(numS);
	if (std::abs(static_cast<int>(solve.numModels)) != 1 || !solve.models()) {
		ctx.setPreserveModels(true);
	}
	ctx.setConcurrency(solve.numSolver(), SharedContext::resize_resize);
	impl_->prepare(ctx);
}

Configuration* ClaspConfig::config(const char* n) {
	return (n && std::strcmp(n, "tester") == 0) ? testerConfig() : BasicSatConfig::config(n);
}

void ClaspConfig::addConfigurator(Configurator* c, Ownership_t::Type t, bool once) {
	impl_->add(c, t, once);
}

bool ClaspConfig::addPost(Solver& s) const {
	return impl_->addPost(s, solver(s.id())) && BasicSatConfig::addPost(s);
}

void ClaspConfig::unfreeze(SharedContext& ctx) {
	impl_->unfreeze(ctx);
}

ClaspConfig::Configurator::~Configurator() {}
void ClaspConfig::Configurator::prepare(SharedContext&) {}
void ClaspConfig::Configurator::unfreeze(SharedContext&){}
/////////////////////////////////////////////////////////////////////////////////////////
// ClaspFacade::SolveStrategy
/////////////////////////////////////////////////////////////////////////////////////////
struct ClaspFacade::SolveStrategy {
public:
	enum { SIGCANCEL = 9, SIGERROR = 128};
	enum State { state_run = 1u, state_model = 2u, state_done = 4u };
	enum Event { event_attach, event_model, event_resume, event_detach };
	virtual ~SolveStrategy() {}
	static SolveStrategy* create(SolveMode_t m, ClaspFacade& f, SolveAlgorithm& algo);
	void start(EventHandler* h, const LitVec& a);
	bool running()  const { return (state_ & uint32(state_done-1)) != 0u; }
	bool error()    const { return signal_ == SIGERROR; }
	bool ready()    const { return state_ != uint32(state_run); }
	int  signal()   const { return static_cast<int>(signal_); }
	bool interrupt(int sig) {
		bool stopped = running() && compare_and_swap(signal_, uint32(0), uint32(sig)) == 0u && algo_->interrupt();
		if (sig == SIGCANCEL) { wait(-1.0); }
		return stopped;
	}
	bool wait(double s) { return doWait(s); }
	void resume() { doNotify(event_resume); }
	bool setModel(const Solver& s, const Model& m) {
		result_.flags |= SolveResult::SAT;
		bool ok = !handler_ || handler_->onModel(s, m);
		ok      = s.sharedContext()->report(s, m) && ok;
		if ((mode_ & SolveMode_t::Yield) != 0) { doNotify(event_model); }
		return ok && !signal();
	}
	Result result() {
		wait(-1.0);
		if (error()) { throw std::runtime_error(error_.c_str()); }
		return result_;
	}
	const Model* model() {
		return state_ == state_model || (result().sat() && state_ == state_model)
			? &algo_->model()
			: 0;
	}
	const LitVec* unsatCore() {
		return result().unsat()
			? algo_->unsatCore()
			: 0;
	}
	bool next() {
		return running() && (state_ != state_model || (resume(), true)) && model() != 0;
	}
	void release() {
		if      (--nrefs_ == 1) { interrupt(SIGCANCEL); }
		else if (nrefs_   == 0) { delete this; }
	}
	SolveStrategy* share() { ++nrefs_; return this; }
protected:
	SolveStrategy(SolveMode_t m, ClaspFacade& f, SolveAlgorithm* algo);
	ClaspFacade*    facade_;
	SolveAlgorithm* algo_;
	void startAlgo(SolveMode_t m);
	void continueAlgo() {
		bool detach = true;
		try {
			detach = (signal() && running()) || (state_ == state_run && !algo_->next());
			if (detach) { detach = false; detachAlgo(algo_->more(), 0); }
		}
		catch (...) {
			if (detach) { detachAlgo(algo_->more(), 1); }
			else        { throw; }
		}
	}
	void detachAlgo(bool more, int nException, int state = 0);
private:
	struct Async;
	virtual void doStart() { startAlgo(mode_);  }
	virtual bool doWait(double maxTime) {
		POTASSCO_REQUIRE(maxTime < 0.0, "Timed wait not supported!");
		if (mode_ == SolveMode_t::Yield) { continueAlgo(); }
		return true;
	}
	virtual void doNotify(Event event) {
		switch (event) {
			case event_attach: state_ = state_run;   break;
			case event_model : state_ = state_model; break;
			case event_resume: compare_and_swap(state_, uint32(state_model), uint32(state_run)); break;
			case event_detach: state_ = state_done; break;
		};
	}
	typedef Clasp::Atomic_t<uint32>::type SafeIntType;
	std::string   error_;
	EventHandler* handler_;
	SafeIntType   nrefs_;   // Facade + #Handle objects
	SafeIntType   state_;
	SafeIntType   signal_;
	Result        result_;
	SolveMode_t   mode_;
	uint32        aTop_;
};
ClaspFacade::SolveStrategy::SolveStrategy(SolveMode_t m, ClaspFacade& f, SolveAlgorithm* algo)
	: facade_(&f)
	, algo_(algo)
	, handler_(0)
	, mode_(m) {
	nrefs_ = 1;
	state_ = signal_ = 0;
}
void ClaspFacade::SolveStrategy::start(EventHandler* h, const LitVec& a) {
	ClaspFacade& f = *facade_;
	aTop_ = (uint32)f.assume_.size();
	f.assume_.insert(f.assume_.end(), a.begin(), a.end());
	if (!isSentinel(f.ctx.stepLiteral())) {
		f.assume_.push_back(f.ctx.stepLiteral());
	}
	handler_ = h;
	std::memset(&result_, 0, sizeof(SolveResult));
	// We forward models to the SharedContext ourselves.
	algo_->setReportModels(false);
	doStart();
	assert(running() || ready());
}
void ClaspFacade::SolveStrategy::startAlgo(SolveMode_t m) {
	bool more = true, detach = true;
	doNotify(event_attach);
	try {
		facade_->interrupt(0); // handle pending interrupts
		if (!signal_ && !facade_->ctx.master()->hasConflict()) {
			facade_->step_.solveTime = facade_->step_.unsatTime = RealTime::getTime();
			if ((m & SolveMode_t::Yield) == 0) {
				more = algo_->solve(facade_->ctx, facade_->assume_, facade_);
			}
			else {
				algo_->start(facade_->ctx, facade_->assume_, facade_);
				detach = false;
			}
		}
		else {
			facade_->ctx.report(Clasp::Event::subsystem_solve);
			more = facade_->ctx.ok();
		}
		if (detach) { detach = false; detachAlgo(more, 0); }
	}
	catch (...) {
		if (detach) { detachAlgo(more, 1); }
		else        { throw; }
	}
}
void ClaspFacade::SolveStrategy::detachAlgo(bool more, int nException, int state) {
#define PROTECT(ec, X) if ((ec)) try { X; } catch (...) {} else X
	try {
		if (nException == 1) { throw; }
		switch (state) {
			case 0: ++state; PROTECT(nException, algo_->stop());  // FALLTHRU
			case 1: ++state; PROTECT(nException, facade_->stopStep(signal_, !more));  // FALLTHRU
			case 2: ++state; if (handler_) { PROTECT(nException, handler_->onEvent(StepReady(facade_->summary()))); }   // FALLTHRU
			case 3: state = -1;
				result_ = facade_->result();
				facade_->assume_.resize(aTop_);
				doNotify(event_detach);
			default:	break;
		}
	}
	catch (...) {
		error_ = "Operation failed: ";
		if (!signal_)    { signal_ = SIGERROR; }
		if (state != -1) { detachAlgo(more, 2, state); }
		if ((mode_ & SolveMode_t::Async) == 0) {
			error_ += "exception thrown";
			throw;
		}
		try { throw; }
		catch (const std::exception& e) { error_ = e.what(); }
		catch (...)                     { error_ = "unknown error"; }
	}
}
#if CLASP_HAS_THREADS
struct ClaspFacade::SolveStrategy::Async : public ClaspFacade::SolveStrategy {
	enum { state_async = (state_done << 1), state_next = state_model | state_async, state_join = state_done | state_async };
	Async(SolveMode_t m, ClaspFacade& f, SolveAlgorithm* algo) : SolveStrategy(m, f, algo) {}
	virtual void doStart() {
		algo_->enableInterrupts();
		Clasp::mt::thread(std::mem_fun(&SolveStrategy::startAlgo), this, SolveMode_t::Async).swap(task_);
		for (mt::unique_lock<Clasp::mt::mutex> lock(mqMutex_); state_ == 0u;) {
			mqCond_.wait(lock);
		}
	}
	virtual bool doWait(double t) {
		for (mt::unique_lock<Clasp::mt::mutex> lock(mqMutex_);;) {
			if (signal() && running()) { // propagate signal to async thread and force wait
				mqCond_.notify_all();
				mqCond_.wait(lock);
			}
			else if (ready()) { break; }
			else if (t < 0.0) { mqCond_.wait(lock); }
			else if (t > 0.0) { mqCond_.wait_for(lock, t); t = 0.0; }
			else              { return false; }
		}
		assert(ready());
		// acknowledge current model or join if first to see done
		if (compare_and_swap(state_, uint32(state_next), uint32(state_model)) == state_done
			&& compare_and_swap(state_, uint32(state_done), uint32(state_join)) == state_done) {
			task_.join();
		}
		return true;
	}
	virtual void doNotify(Event event) {
		mt::unique_lock<Clasp::mt::mutex> lock(mqMutex_);
		switch (event) {
			case event_attach: state_ = state_run;  break;
			case event_model : state_ = state_next; break;
			case event_resume: if (state_ == state_model) { state_ = state_run; break; } else { return; }
			case event_detach: state_ = state_done;  break;
		};
		lock.unlock(); // synchronize-with other threads but no need to notify under lock
		mqCond_.notify_all();
		if (event == event_model) {
			for (lock.lock(); state_ != state_run && !signal();) {
				mqCond_.wait(lock);
			}
		}
	}
	typedef Clasp::mt::condition_variable ConditionVar;
	Clasp::mt::thread task_;   // async solving thread
	Clasp::mt::mutex  mqMutex_;// protects mqCond
	ConditionVar      mqCond_; // for iterating over models one by one
};
#endif
ClaspFacade::SolveStrategy* ClaspFacade::SolveStrategy::create(SolveMode_t m, ClaspFacade& f, SolveAlgorithm& algo) {
	if ((m & SolveMode_t::Async) == 0) { return new SolveStrategy(m, f, &algo); }
#if CLASP_HAS_THREADS
	return new SolveStrategy::Async(m, f, &algo);
#else
	throw std::logic_error("Solve mode not supported!");
#endif
}
/////////////////////////////////////////////////////////////////////////////////////////
// ClaspFacade::SolveData
/////////////////////////////////////////////////////////////////////////////////////////
struct ClaspFacade::SolveData {
	struct BoundArray {
		enum Type { Lower, Costs };
		BoundArray(SolveData* d, Type t) : data(d), type(t) {}
		~BoundArray() { while (!refs.empty()) { delete refs.back(); refs.pop_back(); } }
		struct LevelRef {
			LevelRef(const BoundArray* a, uint32 l) : arr(a), at(l) {}
			static double value(const LevelRef* ref) { return ref->arr->_at(ref->at); }
			const BoundArray* arr;
			uint32            at;
		};
		typedef typename PodVector<LevelRef*>::type ElemVec;
		uint32 size() const { return data->numBounds(); }
		StatisticObject at(uint32 i) const {
			POTASSCO_REQUIRE(i < size(), "invalid key");
			while (i >= refs.size()) { refs.push_back(new LevelRef(this, sizeVec(refs))); }
			return StatisticObject::value<LevelRef, &LevelRef::value>(refs[i]);
		}
		double _at(uint32_t idx) const {
			POTASSCO_REQUIRE(idx < size(), "expired key");
			const wsum_t bound = data->_bound(type, idx);
			return bound != SharedMinimizeData::maxBound()
				? static_cast<double>(bound)
				: std::numeric_limits<double>::infinity();
		}
		const SolveData* data;
		mutable ElemVec  refs;
		Type             type;
	};
	typedef SingleOwnerPtr<SolveAlgorithm> AlgoPtr;
	typedef SingleOwnerPtr<Enumerator>     EnumPtr;
	typedef Clasp::Atomic_t<int>::type     SafeIntType;
	typedef const SharedMinimizeData*      MinPtr;

	SolveData() : en(0), algo(0), active(0), costs(this, BoundArray::Costs), lower(this, BoundArray::Lower), keepPrg(false), prepared(false), solved(false), interruptible(false) { qSig = 0; }
	~SolveData() { reset(); }
	void init(SolveAlgorithm* algo, Enumerator* en);
	void reset();
	void prepareEnum(SharedContext& ctx, int64 numM, EnumOptions::OptMode opt, EnumMode mode, ProjectMode prj);
	bool interrupt(int sig) {
		if (solving()) { return active->interrupt(sig); }
		if (!qSig && sig != SolveStrategy::SIGCANCEL) { qSig = sig; }
		return false;
	}
	bool onModel(const Solver& s, const Model& m) {
		return !active || active->setModel(s, m);
	}
	bool         solving()   const { return active && active->running(); }
	const Model* lastModel() const { return en.get() ? &en->lastModel() : 0; }
	const LitVec*unsatCore() const { return active ? active->unsatCore() : 0; }
	MinPtr       minimizer() const { return en.get() ? en->minimizer() : 0; }
	Enumerator*  enumerator()const { return en.get(); }
	int          modelType() const { return en.get() ? en->modelType() : 0; }
	int          signal()    const { return solving() ? active->signal() : static_cast<int>(qSig); }
	uint32       numBounds() const { return minimizer() ? minimizer()->numRules() : 0; }

	wsum_t _bound(BoundArray::Type type, uint32 idx) const {
		const Model* m = lastModel();
		if (m && m->costs && (m->opt || type == BoundArray::Costs)) {
			return m->costs->at(idx);
		}
		const wsum_t b = type == BoundArray::Costs ? minimizer()->sum(idx) : minimizer()->lower(idx);
		return b + (b != SharedMinimizeData::maxBound() ? minimizer()->adjust(idx) : 0);
	}

	EnumPtr        en;
	AlgoPtr        algo;
	SolveStrategy* active;
	BoundArray     costs;
	BoundArray     lower;
	SafeIntType    qSig;
	bool           keepPrg;
	bool           prepared;
	bool           solved;
	bool           interruptible;
};
void ClaspFacade::SolveData::init(SolveAlgorithm* a, Enumerator* e) {
	en = e;
	algo = a;
	algo->setEnumerator(*en);
	if (interruptible) {
		this->algo->enableInterrupts();
	}
}
void ClaspFacade::SolveData::reset() {
	if (active)     { active->interrupt(SolveStrategy::SIGCANCEL); active->release(); active = 0; }
	if (algo.get()) { algo->resetSolve(); }
	if (en.get())   { en->reset(); }
	prepared = solved = false;
}
void ClaspFacade::SolveData::prepareEnum(SharedContext& ctx, int64 numM, EnumOptions::OptMode opt, EnumMode mode, ProjectMode proj) {
	POTASSCO_REQUIRE(!active, "Solve operation still active");
	if (ctx.ok() && !ctx.frozen() && !prepared) {
		if (mode == enum_volatile && ctx.solveMode() == SharedContext::solve_multi) {
			ctx.requestStepVar();
		}
		ctx.output.setProjectMode(proj);
		int lim = en->init(ctx, opt, (int)Range<int64>(-1, INT_MAX).clamp(numM));
		if (lim == 0 || numM < 0) {
			numM = lim;
		}
		algo->setEnumLimit(numM ? static_cast<uint64>(numM) : UINT64_MAX);
		prepared = true;
	}
}
ClaspFacade::SolveHandle::SolveHandle(SolveStrategy* s) : strat_(s->share()) {}
ClaspFacade::SolveHandle::~SolveHandle() { strat_->release(); }
ClaspFacade::SolveHandle::SolveHandle(const SolveHandle& o) : strat_(o.strat_->share()) {}
int  ClaspFacade::SolveHandle::interrupted()        const { return strat_->signal(); }
bool ClaspFacade::SolveHandle::error()              const { return ready() && strat_->error(); }
bool ClaspFacade::SolveHandle::ready()              const { return strat_->ready(); }
bool ClaspFacade::SolveHandle::running()            const { return strat_->running(); }
void ClaspFacade::SolveHandle::cancel()             const { strat_->interrupt(SolveStrategy::SIGCANCEL); }
void ClaspFacade::SolveHandle::wait()               const { strat_->wait(-1.0); }
bool ClaspFacade::SolveHandle::waitFor(double s)    const { return strat_->wait(s); }
void ClaspFacade::SolveHandle::resume()             const { strat_->resume(); }
SolveResult ClaspFacade::SolveHandle::get()         const { return strat_->result(); }
const Model*  ClaspFacade::SolveHandle::model()     const { return strat_->model(); }
const LitVec* ClaspFacade::SolveHandle::unsatCore() const { return strat_->unsatCore(); }
bool ClaspFacade::SolveHandle::next()               const { return strat_->next(); }
/////////////////////////////////////////////////////////////////////////////////////////
// ClaspFacade::Statistics
/////////////////////////////////////////////////////////////////////////////////////////
namespace {
struct KV { const char* key; StatisticObject(*get)(const ClaspFacade::Summary*); };
template <double ClaspFacade::Summary::*time>
StatisticObject _getT(const ClaspFacade::Summary* x) { return StatisticObject::value(&static_cast<const double&>((x->*time))); }
template <uint64 ClaspFacade::Summary::*model>
StatisticObject _getM(const ClaspFacade::Summary* x) { return StatisticObject::value(&static_cast<const uint64&>((x->*model))); }
static const KV sumKeys_s[] = {
	{"total"     , _getT<&ClaspFacade::Summary::totalTime>},
	{"cpu"       , _getT<&ClaspFacade::Summary::cpuTime>},
	{"solve"     , _getT<&ClaspFacade::Summary::solveTime>},
	{"unsat"     , _getT<&ClaspFacade::Summary::unsatTime>},
	{"sat"       , _getT<&ClaspFacade::Summary::satTime>},
	{"enumerated", _getM<&ClaspFacade::Summary::numEnum>},
	{"optimal"   , _getM<&ClaspFacade::Summary::numOptimal>},
};
struct SummaryStats {
	SummaryStats() : stats_(0), range_(0,0) {}
	void bind(const ClaspFacade::Summary& x, Range32 r) {
		stats_ = &x;
		range_ = r;
	}
	uint32          size()        const { return range_.hi - range_.lo; }
	const char*     key(uint32 i) const { return i < size() ? sumKeys_s[i + range_.lo].key : throw std::out_of_range(POTASSCO_FUNC_NAME); }
	StatisticObject at(const char* key) const {
		for (const KV* x = sumKeys_s + range_.lo, *end = sumKeys_s + range_.hi; x != end; ++x) {
			if (std::strcmp(x->key, key) == 0) { return x->get(stats_); }
		}
		throw std::out_of_range(POTASSCO_FUNC_NAME);
	}
	StatisticObject toStats() const { return StatisticObject::map(this); }
	const ClaspFacade::Summary* stats_;
	Range32 range_;
};

double _getConcurrency(const SharedContext* ctx) { return ctx->concurrency(); }
double _getWinner(const SharedContext* ctx) { return ctx->winner(); }
double _getResult(const SolveResult* r) { return static_cast<double>(r->operator Clasp::SolveResult::Base()); }
double _getSignal(const SolveResult* r) { return static_cast<double>(r->signal); }
double _getExhausted(const SolveResult* r) { return static_cast<double>(r->exhausted()); }
}

struct ClaspFacade::Statistics {
	Statistics(ClaspFacade& f) : self_(&f), tester_(0), level_(0), clingo_(0) {}
	~Statistics() { delete clingo_; delete solvers_.multi; }
	void start(uint32 level);
	void initLevel(uint32 level);
	void end();
	void addTo(StatsMap& solving, StatsMap* accu) const;
	void accept(StatsVisitor& out, bool final) const;
	bool incremental() const { return self_->incremental(); }
	typedef StatsVec<SolverStats>        SolverVec;
	typedef SingleOwnerPtr<Asp::LpStats> LpStatsPtr;
	typedef PrgDepGraph::NonHcfStats     TesterStats;
	ClaspFacade* self_;
	LpStatsPtr   lp_;      // level 0 and asp
	SolverStats  solvers_; // level 0
	SolverVec    solver_;  // level > 1
	SolverVec    accu_;    // level > 1 and incremental
	TesterStats* tester_;  // level > 0 and nonhcfs
	uint32       level_;   // active stats level
	// For clingo stats interface
	class ClingoView : public ClaspStatistics {
	public:
		explicit ClingoView(const ClaspFacade& f);
		void update(const Statistics& s);
		Key_t user(bool final) const;
	private:
		struct StepStats {
			SummaryStats times;
			SummaryStats models;
			void bind(const ClaspFacade::Summary& x) {
				times.bind(x, Range32(0, 5));
				models.bind(x, Range32(5, 7));
			}
			void addTo(StatsMap& summary) {
				summary.add("times", times.toStats());
				summary.add("models", models.toStats());
			}
		};
		StatsMap*   keys_;
		StatsMap    problem_;
		StatsMap    solving_;
		struct Summary : StatsMap { StepStats step; } summary_;
		struct Accu    : StatsMap { StepStats step; StatsMap solving_; };
		typedef SingleOwnerPtr<Accu> AccuPtr;
		AccuPtr accu_;
	}* clingo_; // new clingo stats interface
	ClingoView* getClingo();
};
void ClaspFacade::Statistics::initLevel(uint32 level) {
	if (level_ < level) {
		if (incremental() && !solvers_.multi) { solvers_.multi = new SolverStats(); }
		level_ = level;
	}
	if (self_->ctx.sccGraph.get() && self_->ctx.sccGraph->numNonHcfs() && !tester_) {
		tester_ = self_->ctx.sccGraph->nonHcfStats();
	}
}

void ClaspFacade::Statistics::start(uint32 level) {
	// cleanup previous state
	solvers_.reset();
	solver_.reset();
	if (tester_) { tester_->startStep(self_->config()->testerConfig() ? self_->config()->testerConfig()->context().stats : 0); }
	// init next step
	initLevel(level);
	if (lp_.get() && self_->step_.lpStep()) {
		lp_->accu(*self_->step_.lpStep());
	}
	if (level > 1 && solver_.size() < self_->ctx.concurrency()) {
		uint32 sz = sizeVec(solver_);
		solver_.growTo(self_->ctx.concurrency());
		for (const bool inc = incremental() && (accu_.growTo(sizeVec(solver_)), true); sz != sizeVec(solver_); ++sz) {
			if (!inc) { solver_[sz] = &self_->ctx.solverStats(sz); }
			else      { (solver_[sz] = new SolverStats())->multi = (accu_[sz] = new SolverStats()); }
		}
		if (!incremental()) { solver_.release(); }
	}
}
void ClaspFacade::Statistics::end() {
	self_->ctx.accuStats(solvers_); // compute solvers = sum(solver[1], ... , solver[n])
	solvers_.flush();
	for (uint32 i = incremental() ? 0 : sizeVec(solver_), end = sizeVec(solver_); i != end && self_->ctx.hasSolver(i); ++i) {
		solver_[i]->accu(self_->ctx.solverStats(i), true);
		solver_[i]->flush();
	}
	if (tester_) { tester_->endStep(); }
	if (clingo_) { clingo_->update(*this); }
}
void ClaspFacade::Statistics::addTo(StatsMap& solving, StatsMap* accu) const {
	solvers_.addTo("solvers", solving, accu);
	if (solver_.size())       { solving.add("solver", solver_.toStats()); }
	if (accu && accu_.size()) { accu->add("solver", accu_.toStats()); }
}
void ClaspFacade::Statistics::accept(StatsVisitor& out, bool final) const {
	final = final && solvers_.multi;
	if (out.visitGenerator(StatsVisitor::Enter)) {
		out.visitSolverStats(final ? *solvers_.multi : solvers_);
		if (lp_.get()) { out.visitLogicProgramStats(*lp_); }
		out.visitProblemStats(self_->ctx.stats());
		const SolverVec& solver = final ? accu_ : solver_;
		const uint32 nThreads = final ? (uint32)accu_.size() : self_->ctx.concurrency();
		const uint32 nSolver  = (uint32)solver.size();
		if (const AbstractStatistics::Key_t userKey = clingo_ ? clingo_->user(final) : 0) {
			out.visitExternalStats(clingo_->getObject(userKey));
		}
		if (nThreads > 1 && nSolver > 1 && out.visitThreads(StatsVisitor::Enter)) {
			for (uint32 i = 0, end = std::min(nSolver, nThreads); i != end; ++i) {
				out.visitThread(i, *solver[i]);
			}
			out.visitThreads(StatsVisitor::Leave);
		}
		out.visitGenerator(StatsVisitor::Leave);
	}
	if (tester_ && out.visitTester(StatsVisitor::Enter)) {
		tester_->accept(out, final);
		out.visitTester(StatsVisitor::Leave);
	}
}
ClaspFacade::Statistics::ClingoView* ClaspFacade::Statistics::getClingo() {
	if (!clingo_) {
		clingo_ = new ClingoView(*this->self_);
		clingo_->update(*this);
	}
	return clingo_;
}
ClaspFacade::Statistics::ClingoView::ClingoView(const ClaspFacade& f) {
	keys_ = makeRoot();
	summary_.add("call"       , StatisticObject::value(&f.step_.step));
	summary_.add("result"     , StatisticObject::value<SolveResult, _getResult>(&f.step_.result));
	summary_.add("signal"     , StatisticObject::value<SolveResult, _getSignal>(&f.step_.result));
	summary_.add("exhausted"  , StatisticObject::value<SolveResult, _getExhausted>(&f.step_.result));
	summary_.add("costs"      , StatisticObject::array(&f.solve_->costs));
	summary_.add("lower"      , StatisticObject::array(&f.solve_->lower));
	summary_.add("concurrency", StatisticObject::value<SharedContext, _getConcurrency>(&f.ctx));
	summary_.add("winner"     , StatisticObject::value<SharedContext, _getWinner>(&f.ctx));
	summary_.step.bind(f.step_);
	summary_.step.addTo(summary_);
	if (f.step_.lpStats()) {
		problem_.add("lp", StatisticObject::map(f.step_.lpStats()));
		if (f.incremental()) { problem_.add("lpStep", StatisticObject::map(f.step_.lpStep())); }
	}
	problem_.add("generator", StatisticObject::map(&f.ctx.stats()));
	keys_->add("problem", problem_.toStats());
	keys_->add("solving", solving_.toStats());
	keys_->add("summary", summary_.toStats());

	if (f.incremental()) {
		accu_ = new Accu();
		accu_->step.bind(*f.accu_.get());
	}
}
Potassco::AbstractStatistics::Key_t ClaspFacade::Statistics::ClingoView::user(bool final) const {
	Key_t key = 0;
	find(root(), final ? "user_accu" : "user_step", &key);
	return key;
}
void ClaspFacade::Statistics::ClingoView::update(const ClaspFacade::Statistics& stats) {
	if (stats.level_ > 0 && accu_.get() && keys_->add("accu", accu_->toStats())) {
		accu_->step.addTo(*accu_);
		accu_->add("solving", accu_->solving_.toStats());
	}
	stats.addTo(solving_, stats.level_ > 0 && accu_.get() ? &accu_->solving_ : 0);
	if (stats.tester_) {
		stats.tester_->addTo(problem_, solving_, stats.level_ > 0 && accu_.get() ? &accu_->solving_ : 0);
	}
}
/////////////////////////////////////////////////////////////////////////////////////////
// ClaspFacade
/////////////////////////////////////////////////////////////////////////////////////////
ClaspFacade::ClaspFacade() : config_(0) { step_.init(*this); }
ClaspFacade::~ClaspFacade() {}
bool ClaspFacade::prepared() const {
	return solve_.get() && solve_->prepared;
}
bool ClaspFacade::solving() const {
	return solve_.get() && solve_->solving();
}
bool ClaspFacade::solved() const {
	return solve_.get() && solve_->solved;
}
bool ClaspFacade::interrupted() const {
	return result().interrupted();
}
bool ClaspFacade::incremental() const {
	return accu_.get() != 0;
}
ProblemType ClaspFacade::detectProblemType(std::istream& str) {
	return Clasp::detectProblemType(str);
}
const ClaspFacade::Summary&  ClaspFacade::summary(bool accu) const {
	return accu && accu_.get() ? *accu_ : step_;
}

void ClaspFacade::discardProblem() {
	config_  = 0;
	builder_ = 0;
	stats_   = 0;
	solve_   = 0;
	accu_    = 0;
	step_.init(*this);
	if (ctx.frozen() || ctx.numVars()) { ctx.reset(); }
}
void ClaspFacade::init(ClaspConfig& config, bool discard) {
	if (discard) { discardProblem(); }
	ctx.setConfiguration(0, Ownership_t::Retain); // force reload of configuration once done
	config_ = &config;
	if (config_->solve.enumMode == EnumOptions::enum_dom_record && config_->solver(0).heuId != Heuristic_t::Domain) {
		ctx.warn("Reasoning mode requires domain heuristic and is ignored.");
		config_->solve.enumMode = EnumOptions::enum_auto;
	}
	SolveData::EnumPtr e(config.solve.createEnumerator(config.solve));
	if (e.get() == 0) { e = EnumOptions::nullEnumerator(); }
	if (config.solve.numSolver() > 1 && !e->supportsParallel()) {
		ctx.warn("Selected reasoning mode implies #Threads=1.");
		config.solve.setSolvers(1);
	}
	ctx.setConfiguration(&config, Ownership_t::Retain); // prepare and apply config
	if (program() && type_ == Problem_t::Asp) {
		Asp::LogicProgram* p = static_cast<Asp::LogicProgram*>(program());
		p->setOptions(config.asp);
		p->setNonHcfConfiguration(config.testerConfig());
	}
	if (!solve_.get()) { solve_ = new SolveData(); }
	SolveData::AlgoPtr a(config.solve.createSolveObject());
	solve_->init(a.release(), e.release());
	if (discard) { startStep(0); }
}

void ClaspFacade::initBuilder(ProgramBuilder* in) {
	builder_ = in;
	assume_.clear();
	builder_->startProgram(ctx);
}
ProgramBuilder& ClaspFacade::start(ClaspConfig& config, ProblemType t) {
	if      (t == Problem_t::Sat) { return startSat(config); }
	else if (t == Problem_t::Pb)  { return startPB(config);  }
	else if (t == Problem_t::Asp) { return startAsp(config); }
	else                          { throw std::domain_error("Unknown problem type!"); }
}

ProgramBuilder& ClaspFacade::start(ClaspConfig& config, std::istream& str) {
	ProgramParser& p = start(config, detectProblemType(str)).parser();
	POTASSCO_REQUIRE(p.accept(str, config_->parse), "Auto detection failed!");
	if (p.incremental()) { enableProgramUpdates(); }
	return *program();
}

SatBuilder& ClaspFacade::startSat(ClaspConfig& config) {
	init(config, true);
	initBuilder(new SatBuilder());
	type_ = Problem_t::Sat;
	return static_cast<SatBuilder&>(*builder_.get());
}

PBBuilder& ClaspFacade::startPB(ClaspConfig& config) {
	init(config, true);
	initBuilder(new PBBuilder());
	type_ = Problem_t::Sat;
	return static_cast<PBBuilder&>(*builder_.get());
}

Asp::LogicProgram& ClaspFacade::startAsp(ClaspConfig& config, bool enableUpdates) {
	init(config, true);
	Asp::LogicProgram* p = new Asp::LogicProgram();
	initBuilder(p);
	p->setOptions(config.asp);
	p->setNonHcfConfiguration(config.testerConfig());
	type_ = Problem_t::Asp;
	stats_->lp_ = new Asp::LpStats();
	if (enableUpdates) { enableProgramUpdates(); }
	return *p;
}

bool ClaspFacade::enableProgramUpdates() {
	POTASSCO_REQUIRE(program(), "Program was already released!");
	POTASSCO_REQUIRE(!solving() && !program()->frozen());
	if (!accu_.get()) {
		keepProgram();
		builder_->updateProgram();
		ctx.setSolveMode(SharedContext::solve_multi);
		enableSolveInterrupts();
		accu_ = new Summary();
		accu_->init(*this);
		accu_->step = UINT32_MAX;
	}
	return isAsp(); // currently only ASP supports program updates
}
void ClaspFacade::enableSolveInterrupts() {
	POTASSCO_REQUIRE(!solving(), "Solving is already active!");
	POTASSCO_ASSERT(solve_.get(), "Active program required!");
	if (!solve_->interruptible) {
		solve_->interruptible = true;
		solve_->algo->enableInterrupts();
	}
}

void Clasp::ClaspFacade::keepProgram() {
	POTASSCO_REQUIRE(program(), "Program was already released!");
	POTASSCO_ASSERT(solve_.get(), "Active program required!");
	solve_->keepPrg = true;
}

void ClaspFacade::startStep(uint32 n) {
	step_.init(*this);
	step_.totalTime = RealTime::getTime();
	step_.cpuTime   = ProcessTime::getTime();
	step_.step      = n;
	solve_->solved  = false;
	if (!stats_.get()) { stats_ = new Statistics(*this); }
	ctx.report(StepStart(*this));
}

ClaspFacade::Result ClaspFacade::stopStep(int signal, bool complete) {
	if (!solved()) {
		double t = RealTime::getTime();
		solve_->solved  = true;
		step_.totalTime = diffTime(t, step_.totalTime);
		step_.cpuTime   = diffTime(ProcessTime::getTime(), step_.cpuTime);
		if (step_.solveTime) {
			step_.solveTime = diffTime(t, step_.solveTime);
			step_.unsatTime = complete ? diffTime(t, step_.unsatTime) : 0;
		}
		Result res = {uint8(0), uint8(signal)};
		if (complete) { res.flags = uint8(step_.numEnum ? Result::SAT : Result::UNSAT) | Result::EXT_EXHAUST; }
		else          { res.flags = uint8(step_.numEnum ? Result::SAT : Result::UNKNOWN); }
		if (signal)   { res.flags|= uint8(Result::EXT_INTERRUPT); }
		step_.result = res;
		if (res.sat() && step_.model()->opt && !step_.numOptimal) {
			step_.numOptimal = 1;
		}
		updateStats();
		ctx.report(StepReady(step_));
		ctx.report(Event::subsystem_facade);
	}
	return result();
}

void ClaspFacade::updateStats() {
	if (stats_.get()) {
		stats_->end();
	}
	if (accu_.get() && accu_->step != step_.step) {
		accu_->totalTime  += step_.totalTime;
		accu_->cpuTime    += step_.cpuTime;
		accu_->solveTime  += step_.solveTime;
		accu_->unsatTime  += step_.unsatTime;
		accu_->satTime    += step_.satTime;
		accu_->numEnum    += step_.numEnum;
		accu_->numOptimal += step_.numOptimal;
		// no aggregation
		accu_->step   = step_.step;
		accu_->result = step_.result;
	}
}

bool ClaspFacade::interrupt(int signal) {
	return solve_.get() && (signal || (signal = solve_->qSig.exchange(0)) != 0) && solve_->interrupt(signal);
}

const ClaspFacade::Summary& ClaspFacade::shutdown() {
	if (solve_.get()) {
		solve_->interrupt(SolveStrategy::SIGCANCEL);
		stopStep(solve_->signal(), !ok());
	}
	return summary(true);
}

bool ClaspFacade::read() {
	POTASSCO_REQUIRE(solve_.get());
	if (!program() || interrupted()) { return false; }
	ProgramParser& p = program()->parser();
	if (!p.isOpen() || (solved() && !update().ok())) { return false; }
	POTASSCO_REQUIRE(p.parse(), "Invalid input stream!");
	if (!p.more()) { p.reset(); }
	return true;
}

void ClaspFacade::prepare(EnumMode enumMode) {
	POTASSCO_REQUIRE(solve_.get() && !solving());
	POTASSCO_REQUIRE(!solved() || ctx.solveMode() == SharedContext::solve_multi);
	EnumOptions& en = config_->solve;
	if (solved()) {
		doUpdate(0, false, SIG_DFL);
		solve_->prepareEnum(ctx, en.numModels, en.optMode, enumMode, en.proMode);
		ctx.endInit();
	}
	if (prepared()) { return; }
	SharedMinimizeData* m = 0;
	ProgramBuilder*   prg = program();
	if (prg && prg->endProgram()) {
		assume_.clear();
		prg->getAssumptions(assume_);
		prg->getWeakBounds(en.optBound);
	}
	stats_->start(uint32(config_->context().stats));
	if (ctx.ok() && en.optMode != MinimizeMode_t::ignore && (m = ctx.minimize()) != 0) {
		if (!m->setMode(en.optMode, en.optBound)) {
			assume_.push_back(lit_false());
		}
		if (en.optMode == MinimizeMode_t::enumerate && en.optBound.empty()) {
			ctx.warn("opt-mode=enum: No bound given, optimize statement ignored.");
		}
	}
	POTASSCO_REQUIRE(!ctx.ok() || !ctx.frozen());
	solve_->prepareEnum(ctx, en.numModels, en.optMode, enumMode, en.proMode);
	if      (!solve_->keepPrg) { builder_ = 0; }
	else if (isAsp())          { static_cast<Asp::LogicProgram*>(builder_.get())->dispose(false); }
	if (!builder_.get() && !ctx.heuristic.empty()) {
		bool keepDom = false;
		for (uint32 i = 0; i != config_->solve.numSolver() && !keepDom; ++i) {
			keepDom = config_->solver(i).heuId == Heuristic_t::Domain;
		}
		if (!keepDom) { ctx.heuristic.reset(); }
	}
	if (ctx.ok()) { ctx.endInit(); }
}

ClaspFacade::SolveHandle ClaspFacade::solve(SolveMode_t p, const LitVec& a, EventHandler* eh) {
	prepare();
	solve_->active = SolveStrategy::create(p, *this, *solve_->algo.get());
	solve_->active->start(eh, a);
	return SolveHandle(solve_->active);
}
ClaspFacade::Result ClaspFacade::solve(const LitVec& a, EventHandler* handler) {
	return solve(SolveMode_t::Default, a, handler).get();
}

ProgramBuilder& ClaspFacade::update(bool updateConfig, void (*sigAct)(int)) {
	POTASSCO_REQUIRE(config_ && program() && !solving(), "Program updates not supported!");
	POTASSCO_REQUIRE(!program()->frozen() || incremental(), "Program updates not supported!");
	doUpdate(program(), updateConfig, sigAct);
	return *program();
}
ProgramBuilder& ClaspFacade::update(bool updateConfig) {
	return update(updateConfig, SIG_DFL);
}

void ClaspFacade::doUpdate(ProgramBuilder* p, bool updateConfig, void(*sigAct)(int)) {
	if (updateConfig) {
		init(*config_, false);
	}
	if (solved()) {
		startStep(step() + 1);
	}
	if (p && p->frozen()) {
		p->updateProgram();
	}
	if (ctx.frozen()) {
		ctx.unfreeze();
	}
	solve_->reset();
	config_->unfreeze(ctx);
	int sig = sigAct == SIG_DFL ? 0 : solve_->qSig.exchange(0);
	if (sig && sigAct != SIG_IGN) { sigAct(sig); }
}

bool ClaspFacade::onModel(const Solver& s, const Model& m) {
	step_.unsatTime = RealTime::getTime();
	if (++step_.numEnum == 1) { step_.satTime = diffTime(step_.unsatTime, step_.solveTime); }
	if (m.opt) { ++step_.numOptimal; }
	return solve_->onModel(s, m);
}
Enumerator* ClaspFacade::enumerator() const { return solve_.get() ? solve_->enumerator() : 0; }
Potassco::AbstractStatistics* ClaspFacade::getStats() const {
	POTASSCO_REQUIRE(stats_.get() && !solving(), "statistics not (yet) available");
	return stats_->getClingo();
}
/////////////////////////////////////////////////////////////////////////////////////////
// ClaspFacade::Summary
/////////////////////////////////////////////////////////////////////////////////////////
void ClaspFacade::Summary::init(ClaspFacade& f)  { std::memset(this, 0, sizeof(Summary)); facade = &f;}
const Model* ClaspFacade::Summary::model() const { return facade->solve_.get() ? facade->solve_->lastModel() : 0; }
const SumVec* ClaspFacade::Summary::costs()const { return model() ? model()->costs : 0; }
uint64  ClaspFacade::Summary::optimal()    const { return facade->step_.numOptimal; }
bool ClaspFacade::Summary::optimize()      const {
	if (const Enumerator* e = facade->enumerator()){
		return e->optimize() || e->lastModel().opt;
	}
	return false;
}
const LitVec* ClaspFacade::Summary::unsatCore() const { return facade->solve_.get() ? facade->solve_->unsatCore() : 0; }
const Asp::LpStats* ClaspFacade::Summary::lpStep() const {
	return facade->isAsp() ? &static_cast<const Asp::LogicProgram*>(facade->program())->stats : 0;
}
const Asp::LpStats* ClaspFacade::Summary::lpStats() const {
	return facade->stats_.get() ? facade->stats_->lp_.get() : lpStep();
}
const char* ClaspFacade::Summary::consequences() const {
	const Model* m = model();
	return m && m->consequences() ? modelType(*m) : 0;
}

bool ClaspFacade::Summary::hasLower() const {
	const SharedMinimizeData* m = optimize() ? facade->enumerator()->minimizer() : 0;
	return m && m->lower(0) != 0;
}
SumVec ClaspFacade::Summary::lower() const {
	if (hasLower()) {
		const SharedMinimizeData* m = facade->enumerator()->minimizer();
		SumVec ret(m->numRules());
		for (uint32 i = 0; i != m->numRules(); ++i) {
			ret[i] = m->lower(i) + m->adjust(i);
		}
		return ret;
	}
	return SumVec();
}
void ClaspFacade::Summary::accept(StatsVisitor& out) const {
	if (facade->solved()) { facade->stats_->accept(out, this == facade->accu_.get()); }
}

}

