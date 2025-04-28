//
// Copyright (c) 2010-2022 Benjamin Kaufmann
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
#include <clasp/mt/parallel_solve.h>
#include <clasp/solver.h>
#include <clasp/clause.h>
#include <clasp/enumerator.h>
#include <clasp/util/timer.h>
#include <clasp/minimize_constraint.h>
#include <clasp/mt/mutex.h>
#include <potassco/string_convert.h>
namespace Clasp { namespace mt {
/////////////////////////////////////////////////////////////////////////////////////////
// ParallelSolve::Impl
/////////////////////////////////////////////////////////////////////////////////////////
struct ParallelSolve::SharedData {
	typedef PodQueue<const LitVec*> Queue;
	typedef condition_variable      ConditionVar;
	enum MsgFlag {
		terminate_flag        = 1u, sync_flag  = 2u,  split_flag    = 4u,
		restart_flag          = 8u, complete_flag = 16u,
		interrupt_flag        = 32u, // set on terminate from outside
		allow_split_flag      = 64u, // set if splitting mode is active
		forbid_restart_flag   = 128u,// set if restarts are no longer allowed
		cancel_restart_flag   = 256u,// set if current restart request was cancelled by some thread
		restart_abandoned_flag= 512u,// set to signal that threads must not give up their gp
	};
	enum Message {
		msg_terminate      = (terminate_flag),
		msg_interrupt      = (terminate_flag | interrupt_flag),
		msg_sync_restart   = (sync_flag | restart_flag),
		msg_split          = split_flag
	};
	struct Generator {
		enum State { start = 0, search = 1, model = 2, done = 3 };
		Generator() : state(start) {}
		mutex genM;
		condition_variable cond;
		State waitWhile(State st) {
			State r;
			for (unique_lock<mutex> lock(genM); (r = state) == st;) { cond.wait(lock); }
			return r;
		}
		void pushModel() {
			notify(model);
			waitWhile(model);
		}
		void notify(State st) {
			unique_lock<mutex> lock(genM);
			state = st;
			cond.notify_one();
		}
		State state;
	};
	SharedData() : path(0) { reset(0); control = 0; }
	void reset(SharedContext* a_ctx) {
		clearQueue();
		syncT.reset();
		msg.clear();
		globalR.reset();
		maxConflict = globalR.current();
		threads     = a_ctx ? a_ctx->concurrency() : 0;
		waiting     = 0;
		errorSet    = 0;
		initVec     = 0;
		ctx         = a_ctx;
		path        = 0;
		nextId      = 1;
		workReq     = 0;
		restartReq  = 0;
		generator   = 0;
		errorCode   = 0;
	}
	void clearQueue() {
		while (!workQ.empty()) { delete workQ.pop_ret(); }
		workQ.clear();
	}
	const LitVec* requestWork(const Solver& s) {
		const uint64 m(uint64(1) << s.id());
		if ((initVec & m) == m) {
			if (!allowSplit()) {
				// portfolio mode - all solvers can start with initial path
				initVec -= m;
				return path;
			}
			else if (initVec.exchange(0) != 0) {
				// splitting mode - only one solver must start with initial path
				return path;
			}
		}
		if (!allowSplit()) { return 0; }
		// try to get work from split
		ctx->report(MessageEvent(s, "SPLIT", MessageEvent::sent));
		return waitWork();
	}
	void pushWork(const LitVec* v) {
		unique_lock<mutex> lock(workM);
		workQ.push(v);
		notifyWaitingThreads(&lock, 1);
	}
	const LitVec* waitWork(bool postSplit = true) {
		for (unique_lock<mutex> lock(workM); !hasControl(uint32(terminate_flag|sync_flag));) {
			if (!workQ.empty()) {
				const LitVec* res = workQ.pop_ret();
				if (workQ.empty()) { workQ.clear(); }
				return res;
			}
			postSplit = postSplit && !postMessage(SharedData::msg_split, false);
			if (!enterWait(lock))
				break;
		}
		return 0;
	}
	void notifyWaitingThreads(unique_lock<mutex>* lock = 0, int n = 0) {
		assert(!lock || lock->owns_lock());
		if (lock)
			lock->unlock();
		else
			unique_lock<mutex> preventLostWakeup(workM);
		n == 1 ? workCond.notify_one() : workCond.notify_all();
	}
	bool enterWait(unique_lock<mutex>& lock) {
		assert(lock.owns_lock());
		if ((waiting + 1) >= threads)
			return false;
		++waiting;
		workCond.wait(lock);
		--waiting;
		return true;
	}
	bool waitSync() {
		for (unique_lock<mutex> lock(workM); synchronize();) {
			if (!enterWait(lock)) {
				assert(synchronize());
				return true;
			}
		}
		return false;
	}
	uint32 leaveAlgorithm() {
		assert(threads);
		unique_lock<mutex> lock(workM);
		uint32 res = --threads;
		notifyWaitingThreads(&lock);
		return res;
	}
	// MESSAGES
	bool postMessage(Message m, bool notify);
	bool hasMessage()  const { return (control & uint32(7)) != 0; }
	bool synchronize() const { return (control & uint32(sync_flag))      != 0; }
	bool terminate()   const { return (control & uint32(terminate_flag)) != 0; }
	bool split()       const { return (control & uint32(split_flag))     != 0; }
	void aboutToSplit()      { if (--workReq == 0) updateSplitFlag();  }
	void updateSplitFlag();
	// CONTROL FLAGS
	bool hasControl(uint32 f) const { return (control & f) != 0;        }
	bool interrupt()          const { return hasControl(interrupt_flag);}
	bool complete()           const { return hasControl(complete_flag); }
	bool restart()            const { return hasControl(restart_flag);  }
	bool allowSplit()         const { return hasControl(allow_split_flag); }
	bool allowRestart()       const { return !hasControl(forbid_restart_flag); }
	bool setControl(uint32 flags)   { return (control.fetch_or(flags) & flags) != flags; }
	bool clearControl(uint32 flags) { return (control.fetch_and(~flags) & flags) == flags; }
	typedef SingleOwnerPtr<Generator> GeneratorPtr;
	Potassco::StringBuilder msg;  // global error message
	ScheduleStrategy globalR;     // global restart strategy
	uint64           maxConflict; // current restart limit
	atomic<uint64>   errorSet;    // bitmask of erroneous solvers
	SharedContext*   ctx;         // shared context object
	const LitVec*    path;        // initial guiding path - typically empty
	atomic<uint64>   initVec;     // vector of initial gp - represented as bitset
	GeneratorPtr     generator;   // optional data for model generation
	Timer<RealTime>  syncT;       // thread sync time
	mutex            modelM;      // model-mutex
	mutex            workM;       // work-mutex
	ConditionVar     workCond;    // work-condition
	Queue            workQ;       // work-queue (must be protected by workM)
	uint32           waiting;     // number of worker threads waiting on workCond
	uint32           nextId;      // next solver id to use
	LowerBound       lower;       // last reported lower bound (if any)
	atomic<uint32>   threads;     // number of threads in the algorithm
	atomic<int>      workReq;     // > 0: someone needs work
	atomic<uint32>   restartReq;  // == numThreads(): restart
	atomic<uint32>   control;     // set of active message flags
	atomic<uint32>   modCount;    // counter for synchronizing models
	uint32           errorCode;   // global error code
};

// post message to all threads
bool ParallelSolve::SharedData::postMessage(Message m, bool notifyWaiting) {
	if (m == msg_split) {
		if (++workReq == 1) { updateSplitFlag(); }
		return true;
	}
	else if (setControl(m)) {
		// control message - notify all if requested
		if (notifyWaiting) notifyWaitingThreads();
		if ((uint32(m) & uint32(terminate_flag|sync_flag)) != 0) {
			syncT.reset();
			syncT.start();
		}
		return true;
	}
	return false;
}

void ParallelSolve::SharedData::updateSplitFlag() {
	for (bool splitF;;) {
		splitF = (workReq > 0);
		if (split() == splitF) return;
		if (splitF) control.fetch_or(uint32(split_flag));
		else        control.fetch_and(~uint32(split_flag));
	}
}
/////////////////////////////////////////////////////////////////////////////////////////
// ParallelSolve
/////////////////////////////////////////////////////////////////////////////////////////
ParallelSolve::ParallelSolve(const ParallelSolveOptions& opts)
	: SolveAlgorithm(opts.limit)
	, shared_(new SharedData)
	, thread_(0)
	, distribution_(opts.distribute)
	, maxRestarts_(0)
	, intGrace_(1024)
	, intTopo_(opts.integrate.topo)
	, intFlags_(ClauseCreator::clause_not_root_sat | ClauseCreator::clause_no_add)
	, modeSplit_(opts.algorithm.mode == ParallelSolveOptions::Algorithm::mode_split) {
	setRestarts(opts.restarts.maxR, opts.restarts.sched);
	setIntegrate(opts.integrate.grace, opts.integrate.filter);
}

ParallelSolve::~ParallelSolve() {
	if (shared_->nextId > 1) {
		// algorithm was not started but there may be active threads -
		// force orderly shutdown
		ParallelSolve::doInterrupt();
		shared_->notifyWaitingThreads();
		joinThreads();
	}
	destroyThread(masterId);
	delete shared_;
}

bool ParallelSolve::beginSolve(SharedContext& ctx, const LitVec& path) {
	assert(ctx.concurrency() && "Illegal number of threads");
	if (shared_->terminate()) { return false; }
	shared_->reset(&ctx);
	if (!enumerator().supportsParallel() && numThreads() > 1) {
		ctx.warn("Selected reasoning mode implies #Threads=1.");
		shared_->threads = 1;
		modeSplit_       = false;
		ctx.setConcurrency(1, SharedContext::resize_reserve);
	}
	shared_->setControl(modeSplit_ ? SharedData::allow_split_flag : SharedData::forbid_restart_flag);
	shared_->modCount = uint32(enumerator().optimize());
	shared_->path = &path;
	if (distribution_.types != 0 && ctx.distributor.get() == 0 && numThreads() > 1) {
		if (distribution_.mode == ParallelSolveOptions::Distribution::mode_local) {
			ctx.distributor.reset(new mt::LocalDistribution(distribution_, ctx.concurrency(), intTopo_));
		}
		else {
			ctx.distributor.reset(new mt::GlobalDistribution(distribution_, ctx.concurrency(), intTopo_));
		}
	}
	shared_->setControl(SharedData::sync_flag); // force initial sync with all threads
	shared_->syncT.start();
	reportProgress(MessageEvent(*ctx.master(), "SYNC", MessageEvent::sent));
	assert(ctx.master()->id() == masterId);
	allocThread(masterId, *ctx.master());
	for (uint32 i = 1; i != ctx.concurrency(); ++i) {
		uint32 id = shared_->nextId++;
		allocThread(id, *ctx.solver(id));
		// start in new thread
		Clasp::mt::thread x(std::mem_fn(&ParallelSolve::solveParallel), this, id);
		thread_[id]->setThread(x);
	}
	return true;
}

void ParallelSolve::setIntegrate(uint32 grace, uint8 filter) {
	typedef ParallelSolveOptions::Integration Dist;
	intGrace_ = grace;
	intFlags_ = ClauseCreator::clause_no_add;
	if (filter == Dist::filter_heuristic) { store_set_bit(intFlags_, 31); }
	if (filter != Dist::filter_no)        { intFlags_ |= ClauseCreator::clause_not_root_sat; }
	if (filter == Dist::filter_sat)       { intFlags_ |= ClauseCreator::clause_not_sat; }
}

void ParallelSolve::setRestarts(uint32 maxR, const ScheduleStrategy& rs) {
	maxRestarts_         = maxR;
	shared_->globalR     = maxR ? rs : ScheduleStrategy::none();
	shared_->maxConflict = shared_->globalR.current();
}

uint32 ParallelSolve::numThreads() const { return shared_->threads; }

void ParallelSolve::allocThread(uint32 id, Solver& s) {
	if (!thread_) {
		uint32 n = numThreads();
		thread_  = new ParallelHandler*[n];
		std::fill(thread_, thread_+n, static_cast<ParallelHandler*>(0));
	}
	size_t sz   = ((sizeof(ParallelHandler)+63) / 64) * 64;
	thread_[id] = new (alignedAlloc(sz, 64)) ParallelHandler(*this, s);
}

void ParallelSolve::destroyThread(uint32 id) {
	if (thread_ && thread_[id]) {
		assert(!thread_[id]->joinable() && "Shutdown not completed!");
		thread_[id]->~ParallelHandler();
		alignedFree(thread_[id]);
		thread_[id] = 0;
		if (id == masterId) {
			delete [] thread_;
			thread_ = 0;
		}
	}
}

inline void ParallelSolve::reportProgress(const Event& ev) const {
	return shared_->ctx->report(ev);
}
inline void ParallelSolve::reportProgress(const Solver& s, const char* msg) const {
	return shared_->ctx->report(msg, &s);
}

// joins with and destroys all active threads
int ParallelSolve::joinThreads() {
	uint32 winner = thread_[masterId]->winner() ? uint32(masterId) : UINT32_MAX;
	// detach master only after all client threads are done
	for (uint32 i = 1, end = shared_->nextId; i != end; ++i) {
		thread_[i]->join();
		if (thread_[i]->winner() && i < winner) {
			winner = i;
		}
		Solver* s = &thread_[i]->solver();
		reportProgress(*s, "joined");
		destroyThread(i);
		reportProgress(*s, "destroyed");
	}
	if (shared_->complete()) {
		enumerator().commitComplete();
	}
	thread_[masterId]->handleTerminateMessage();
	shared_->ctx->setWinner(winner);
	shared_->nextId = 1;
	shared_->syncT.stop();
	reportProgress(MessageEvent(*shared_->ctx->master(), "TERMINATE", MessageEvent::completed, shared_->syncT.total()));
	return !shared_->interrupt() ? thread_[masterId]->error() : shared_->errorCode;
}

void ParallelSolve::doStart(SharedContext& ctx, const LitVec& assume) {
	if (beginSolve(ctx, assume)) {
		// start computation in new thread
		shared_->generator = new SharedData::Generator();
		Clasp::mt::thread x(std::mem_fn(&ParallelSolve::solveParallel), this, static_cast<uint32>(masterId));
		thread_[masterId]->setThread(x);
	}
}
int ParallelSolve::doNext(int) {
	POTASSCO_REQUIRE(shared_->generator.get(), "Invalid operation");
	int s = shared_->generator->state;
	if (s != SharedData::Generator::done) {
		shared_->generator->notify(SharedData::Generator::search);
		if (shared_->generator->waitWhile(SharedData::Generator::search) == SharedData::Generator::model) {
			return value_true;
		}
	}
	return shared_->complete() ? value_false : value_free;
}
void ParallelSolve::doStop() {
	if (shared_->nextId <= 1) { return; }
	reportProgress(*shared_->ctx->master(), "joining with other threads");
	if (shared_->generator.get()) {
		shared_->setControl(SharedData::terminate_flag);
		shared_->generator->notify(SharedData::Generator::done);
		thread_[masterId]->join();
	}
	int err = joinThreads();
	shared_->generator = 0;
	shared_->ctx->distributor.reset(0);
	switch(err) {
		case 0: break;
		case LogicError:   throw std::logic_error(shared_->msg.c_str());
		case RuntimeError: throw std::runtime_error(shared_->msg.c_str());
		case OutOfMemory:  throw std::bad_alloc();
		default:           throw std::runtime_error(shared_->msg.c_str());
	}
}

void ParallelSolve::doDetach() {
	// detach master only after all client threads are done
	thread_[masterId]->detach(*shared_->ctx, shared_->interrupt());
	destroyThread(masterId);
}

// Entry point for master solver
bool ParallelSolve::doSolve(SharedContext& ctx, const LitVec& path) {
	if (beginSolve(ctx, path)) {
		solveParallel(masterId);
		ParallelSolve::doStop();
	}
	return !shared_->complete();
}

// main solve loop executed by all threads
void ParallelSolve::solveParallel(uint32 id) {
	Solver& s = thread_[id]->solver();
	SolverStats agg;
	PathPtr a(0);
	if (id == masterId && shared_->generator.get()) {
		shared_->generator->waitWhile(SharedData::Generator::start);
	}
	try {
		// establish solver<->handler connection and attach to shared context
		// should this fail because of an initial conflict, we'll terminate
		// in requestWork.
		thread_[id]->attach(*shared_->ctx);
		BasicSolve solve(s, limits());
		agg.enable(s.stats);
		for (GpType t; requestWork(s, a);) {
			agg.accu(s.stats);
			s.stats.reset();
			thread_[id]->setGpType(t = ((a.is_owner() || modeSplit_) ? gp_split : gp_fixed));
			if (enumerator().start(s, *a, a.is_owner()) && thread_[id]->solveGP(solve, t, shared_->maxConflict) == value_free) {
				terminate(s, false);
			}
			s.clearStopConflict();
			s.undoUntil(0);
			enumerator().end(s);
		}
	}
	catch (const std::bad_alloc&)       { exception(id, a, OutOfMemory,  "bad alloc"); }
	catch (const std::logic_error& e)   { exception(id, a, LogicError,   e.what()); }
	catch (const std::exception& e)     { exception(id, a, RuntimeError, e.what()); }
	catch (...)                         { exception(id, a, UnknownError, "unknown");  }
	assert(shared_->terminate() || thread_[id]->error());
	int remaining = shared_->leaveAlgorithm();
	// update stats
	s.stats.accu(agg);
	if (id != masterId) {
		// remove solver<->handler connection and detach from shared context
		// note: since detach can change the problem, we must not yet detach the master
		// because some client might still be in the middle of an attach operation
		thread_[id]->detach(*shared_->ctx, shared_->interrupt());
		s.stats.addCpuTime(ThreadTime::getTime());
	}
	if (remaining == 0 && shared_->generator.get()) {
		shared_->generator->notify(SharedData::Generator::done);
	}
}

void ParallelSolve::exception(uint32 id, PathPtr& path, ErrorCode e, const char* what) {
	try {
		if (!thread_[id]->setError(e) || e != OutOfMemory || id == masterId) {
			ParallelSolve::doInterrupt();
			if (shared_->errorSet.fetch_or(bit_mask<uint64>(id)) == 0) {
				shared_->errorCode = e;
				shared_->msg.appendFormat("[%u]: %s", id, what);
			}
		}
		else if (path.get() && shared_->allowSplit()) {
			shared_->pushWork(path.release());
		}
		reportProgress(thread_[id]->solver(), e == OutOfMemory ? "Thread failed with out of memory" : "Thread failed with error");
	}
	catch(...) { ParallelSolve::doInterrupt(); }
}

// forced termination from outside
bool ParallelSolve::doInterrupt() {
	// do not notify blocked threads to avoid possible
	// deadlock in semaphore!
	shared_->postMessage(SharedData::msg_interrupt, false);
	return true;
}

// tries to get new work for the given solver
bool ParallelSolve::requestWork(Solver& s, PathPtr& out) {
	const LitVec* a = 0;
	for (int popped = 0; !shared_->terminate();) {
		// only clear path and stop conflict - we don't propagate() here
		// because we would then have to handle any eventual conflicts
		if (++popped == 1 && !s.popRootLevel(s.rootLevel())) {
			// s has a real top-level conflict - problem is unsat
			terminate(s, true);
		}
		else if (shared_->synchronize()) {
			// a synchronize request is active - we are fine with
			// this but did not yet had a chance to react on it
			waitOnSync(s);
		}
		else if (a || (a = shared_->requestWork(s)) != 0) {
			assert(s.decisionLevel() == 0);
			// got new work from work-queue
			out = a;
			// do not take over ownership of initial gp!
			if (a == shared_->path) { out.release(); }
			// propagate any new facts before starting new work
			if (s.simplify())       { return true; }
			// s now has a conflict - either an artificial stop conflict
			// or a real conflict - we'll handle it in the next iteration
			// via the call to popRootLevel()
			popped = 0;
		}
		else if (!shared_->synchronize()) {
			// no work left - quitting time?
			terminate(s, true);
		}
	}
	return false;
}

// terminated from inside of algorithm
// check if there is more to do
void ParallelSolve::terminate(Solver& s, bool complete) {
	if (!shared_->terminate()) {
		if (enumerator().tentative() && complete) {
			if (shared_->setControl(SharedData::sync_flag|SharedData::complete_flag)) {
				thread_[s.id()]->setWinner();
				reportProgress(MessageEvent(s, "SYNC", MessageEvent::sent));
			}
		}
		else {
			reportProgress(MessageEvent(s, "TERMINATE", MessageEvent::sent));
			shared_->postMessage(SharedData::msg_terminate, true);
			thread_[s.id()]->setWinner();
			if (complete) { shared_->setControl(SharedData::complete_flag); }
		}
	}
}

// handles an active synchronization request
// returns true to signal that s should restart otherwise false
bool ParallelSolve::waitOnSync(Solver& s) {
	if (!thread_[s.id()]->handleRestartMessage()) {
		shared_->setControl(SharedData::cancel_restart_flag);
	}
	bool hasPath  = thread_[s.id()]->hasPath();
	bool tentative= enumerator().tentative();
	if (shared_->waitSync()) {
		// last man standing - complete synchronization request
		shared_->workReq     = 0;
		shared_->restartReq  = 0;
		bool restart         = shared_->hasControl(SharedData::restart_flag);
		bool init            = true;
		if (restart) {
			init = shared_->allowRestart() && !shared_->hasControl(SharedData::cancel_restart_flag);
			if (init) { shared_->globalR.next(); }
			shared_->maxConflict = shared_->allowRestart() && shared_->globalR.idx < maxRestarts_
				? shared_->globalR.current()
				: UINT64_MAX;
		}
		else if (shared_->maxConflict != UINT64_MAX && !shared_->allowRestart()) {
			shared_->maxConflict = UINT64_MAX;
		}
		if (init) { initQueue();  }
		else      { shared_->setControl(SharedData::restart_abandoned_flag); }
		if (tentative && shared_->complete()) {
			if (enumerator().commitComplete()) { shared_->setControl(SharedData::terminate_flag); }
			else                               { shared_->modCount = uint32(0); shared_->clearControl(SharedData::complete_flag); }
		}
		shared_->clearControl(SharedData::msg_split | SharedData::msg_sync_restart | SharedData::restart_abandoned_flag | SharedData::cancel_restart_flag);
		shared_->syncT.lap();
		reportProgress(MessageEvent(s, "SYNC", MessageEvent::completed, shared_->syncT.elapsed()));
		assert(!shared_->synchronize());
		// wake up all blocked threads
		shared_->notifyWaitingThreads();
	}
	return shared_->terminate() || (hasPath && !shared_->hasControl(SharedData::restart_abandoned_flag));
}

// If guiding path scheme is active only one
// thread can start with gp (typically empty) and this
// thread must then split up search-space dynamically.
// Otherwise, all threads can start with initial gp
// TODO:
//  heuristic for initial splits?
void ParallelSolve::initQueue() {
	shared_->clearQueue();
	if (shared_->allowSplit() && modeSplit_ && !enumerator().supportsSplitting(*shared_->ctx)) {
		shared_->ctx->warn("Selected strategies imply Mode=compete.");
		shared_->clearControl(SharedData::allow_split_flag);
		shared_->setControl(SharedData::forbid_restart_flag);
		modeSplit_ = false;
	}
	shared_->initVec = UINT64_MAX;
	assert(shared_->allowSplit() || shared_->hasControl(SharedData::forbid_restart_flag));
}

// adds work to the work-queue
void ParallelSolve::pushWork(LitVec* v) {
	assert(v);
	shared_->pushWork(v);
}

// called whenever some solver proved unsat
bool ParallelSolve::commitUnsat(Solver& s) {
	const int supUnsat = enumerator().unsatType();
	if (supUnsat == Enumerator::unsat_stop || shared_->terminate() || shared_->synchronize()) {
		return false;
	}
	unique_lock<mutex> lock(shared_->modelM, defer_lock_t());
	if (supUnsat == Enumerator::unsat_sync) {
		lock.lock();
	}
	bool result = enumerator().commitUnsat(s);
	if (lock.owns_lock()) {
		lock.unlock();
	}
	if (!thread_[s.id()]->disjointPath()) {
		if (result) {
			++shared_->modCount;
			if (s.lower.bound > 0) {
				lock.lock();
				if (s.lower.bound > shared_->lower.bound || s.lower.level > shared_->lower.level) {
					shared_->lower = s.lower;
					reportUnsat(s);
					++shared_->modCount;
				}
				lock.unlock();
			}
		}
		else { terminate(s, true);  }
	}
	return result;
}

// called whenever some solver has found a model
bool ParallelSolve::commitModel(Solver& s) {
	// grab lock - models must be processed sequentially
	// in order to simplify printing and to avoid duplicates
	// in all non-trivial enumeration modes
	bool stop = false;
	{lock_guard<mutex> lock(shared_->modelM);
	// first check if the model is still valid once all
	// information is integrated into the solver
	if (thread_[s.id()]->isModelLocked(s) && (stop=shared_->terminate()) == false && enumerator().commitModel(s)) {
		if (enumerator().lastModel().num == 1 && !enumerator().supportsRestarts()) {
			// switch to backtracking based splitting algorithm
			// the solver's gp will act as the root for splitting and is
			// from now on disjoint from all other gps
			shared_->setControl(SharedData::forbid_restart_flag | SharedData::allow_split_flag);
			thread_[s.id()]->setGpType(gp_split);
			enumerator().setDisjoint(s, true);
		}
		if (shared_->generator.get()) {
			shared_->generator->pushModel();
		}
		else if ((stop = !reportModel(s)) == true) {
			// must be called while holding the lock - otherwise
			// we have a race condition with solvers that
			// are currently blocking on the mutex and we could enumerate
			// more models than requested by the user
			terminate(s, !moreModels(s));
		}
		++shared_->modCount;
	}}
	return !stop;
}

uint64 ParallelSolve::hasErrors() const {
	return shared_->errorSet != 0u;
}
bool ParallelSolve::interrupted() const {
	return shared_->interrupt();
}
void ParallelSolve::resetSolve() {
	shared_->control = 0;
}
void ParallelSolve::enableInterrupts() {}
// updates s with new messages and uses s to create a new guiding path
// if necessary and possible
bool ParallelSolve::handleMessages(Solver& s) {
	// check if there are new messages for s
	if (!shared_->hasMessage()) {
		// nothing to do
		return true;
	}
	ParallelHandler* h = thread_[s.id()];
	if (shared_->terminate()) {
		reportProgress(MessageEvent(s, "TERMINATE", MessageEvent::received));
		h->handleTerminateMessage();
		s.setStopConflict();
		return false;
	}
	if (shared_->synchronize()) {
		reportProgress(MessageEvent(s, "SYNC", MessageEvent::received));
		if (waitOnSync(s)) {
			s.setStopConflict();
			return false;
		}
		return true;
	}
	if (h->disjointPath() && s.splittable() && shared_->workReq > 0) {
		// First declare split request as handled
		// and only then do the actual split.
		// This way, we minimize the chance for
		// "over"-splitting, i.e. one split request handled
		// by more than one thread.
		shared_->aboutToSplit();
		reportProgress(MessageEvent(s, "SPLIT", MessageEvent::received));
		h->handleSplitMessage();
		enumerator().setDisjoint(s, true);
	}
	return true;
}

bool ParallelSolve::integrateModels(Solver& s, uint32& upCount) {
	uint32 gCount = shared_->modCount;
	return gCount == upCount || (enumerator().update(s) && (upCount = gCount) == gCount);
}

void ParallelSolve::requestRestart() {
	if (shared_->allowRestart() && ++shared_->restartReq == numThreads()) {
		shared_->postMessage(SharedData::msg_sync_restart, true);
	}
}

SolveAlgorithm* ParallelSolveOptions::createSolveObject() const {
	return numSolver() > 1 ? new ParallelSolve(*this) : BasicSolveOptions::createSolveObject();
}
////////////////////////////////////////////////////////////////////////////////////
// ParallelHandler
/////////////////////////////////////////////////////////////////////////////////////////
ParallelHandler::ParallelHandler(ParallelSolve& ctrl, Solver& s)
	: MessageHandler()
	, ctrl_(&ctrl)
	, solver_(&s)
	, received_(0)
	, recEnd_(0)
	, intEnd_(0)
	, error_(0)
	, win_(0)
	, up_(0) {
	this->next = this;
}

ParallelHandler::~ParallelHandler() { clearDB(0); delete [] received_; }

// adds this as post propagator to its solver and attaches the solver to ctx.
bool ParallelHandler::attach(SharedContext& ctx) {
	assert(solver_);
	gp_.reset();
	error_ = 0;
	win_   = 0;
	up_    = 0;
	act_   = 0;
	lbd_   = solver_->searchConfig().reduce.strategy.glue != 0;
	next   = 0;
	if (!received_ && ctx.distributor.get()) {
		received_ = new SharedLiterals*[RECEIVE_BUFFER_SIZE];
	}
	ctx.report("attach", solver_);
	solver_->addPost(this);
	return ctx.attach(solver_->id());
}

// removes this from the list of post propagators of its solver and detaches the solver from ctx.
void ParallelHandler::detach(SharedContext& ctx, bool) {
	handleTerminateMessage();
	ctx.report("detach", solver_);
	if (solver_->sharedContext() == &ctx) {
		clearDB(!error() ? solver_ : 0);
		ctx.report("detached db", solver_);
		ctx.detach(*solver_, error() != 0);
		ctx.report("detached ctx", solver_);
	}
}

bool ParallelHandler::setError(int code) {
	error_ = code;
	return thread_.joinable() && !winner();
}

void ParallelHandler::clearDB(Solver* s) {
	for (ClauseDB::iterator it = integrated_.begin(), end = integrated_.end(); it != end; ++it) {
		ClauseHead* c = static_cast<ClauseHead*>(*it);
		if (s) { s->addLearnt(c, c->size(), Constraint_t::Other); }
		else   { c->destroy(); }
	}
	integrated_.clear();
	intEnd_= 0;
	for (uint32 i = 0; i != recEnd_; ++i) { received_[i]->release(); }
	recEnd_= 0;
}

ValueRep ParallelHandler::solveGP(BasicSolve& solve, GpType t, uint64 restart) {
	ValueRep res = value_free;
	Solver&  s   = solve.solver();
	bool     fin = false;
	gp_.reset(restart, t);
	assert(act_ == 0);
	do {
		win_ = 0;
		ctrl_->integrateModels(s, gp_.modCount);
		up_ = act_ = 1; // activate enumerator and bounds
		res = solve.solve();
		up_ = act_ = 0; // de-activate enumerator and bounds
		fin = true;
		if      (res == value_true)  { if (ctrl_->commitModel(s)) { fin = false; } }
		else if (res == value_false) { if (ctrl_->commitUnsat(s)) { fin = false; gp_.reset(restart, gp_.type); } }
	} while (!fin);
	return res;
}

// detach from solver, i.e. ignore any further messages
void ParallelHandler::handleTerminateMessage() {
	if (this->next != this) {
		// mark removed propagator by creating "self-loop"
		solver_->removePost(this);
		this->next = this;
	}
}

// split-off new guiding path and add it to solve object
void ParallelHandler::handleSplitMessage() {
	assert(solver_ && "ParallelHandler::handleSplitMessage(): not attached!");
	assert(solver_->splittable());
	Solver& s = *solver_;
	SingleOwnerPtr<LitVec> newPath(new LitVec());
	s.split(*newPath);
	ctrl_->pushWork(newPath.release());
}

bool ParallelHandler::handleRestartMessage() {
	// TODO
	// we may want to implement some heuristic, like
	// computing a local var order.
	return true;
}

bool ParallelHandler::simplify(Solver& s, bool sh) {
	ClauseDB::size_type i, j, end = integrated_.size();
	for (i = j = 0; i != end; ++i) {
		Constraint* c = integrated_[i];
		if (c->simplify(s, sh)) {
			c->destroy(&s, false);
			intEnd_ -= (i < intEnd_);
		}
		else                    {
			integrated_[j++] = c;
		}
	}
	shrinkVecTo(integrated_, j);
	if (intEnd_ > sizeVec(integrated_)) intEnd_ = sizeVec(integrated_);
	return false;
}

bool ParallelHandler::propagateFixpoint(Solver& s, PostPropagator* ctx) {
	// Check for messages and integrate any new information from
	// models/lemma exchange but only if path is setup.
	// Skip updates if called from other post propagator so that we do not
	// disturb any active propagation.
	if (int up = (ctx == 0 && up_ != 0)) {
		up_ ^= (uint32)s.updateMode();
		up  += (act_ == 0 || (up_ && (s.stats.choices & 63) != 0));
		if (s.stats.conflicts >= gp_.restart)  { ctrl_->requestRestart(); gp_.restart *= 2; }
		for (uint32 cDL = s.decisionLevel();;) {
			bool ok = ctrl_->handleMessages(s) && (up > 1 ? integrate(s) : ctrl_->integrateModels(s, gp_.modCount));
			if (!ok)                         { return false; }
			if (cDL != s.decisionLevel())    { // cancel active propagation on cDL
				cancelPropagation();
				cDL = s.decisionLevel();
			}
			if      (!s.queueSize())         { if (++up == 3) return true; }
			else if (!s.propagateUntil(this)){ return false; }
		}
	}
	return ctrl_->handleMessages(s);
}

// checks whether s still has a model once all
// information from previously found models were integrated
bool ParallelHandler::isModel(Solver& s) {
	assert(s.numFreeVars() == 0);
	// either no unprocessed updates or still a model after
	// updates were integrated
	return ctrl_->integrateModels(s, gp_.modCount)
		&& s.numFreeVars() == 0
		&& s.queueSize()   == 0;
}

bool ParallelHandler::isModelLocked(Solver& s) {
	const uint32 current = gp_.modCount;
	if (!isModel(s))
		return false;
	if (current == gp_.modCount)
		return true;
	for (PostPropagator* p = s.getPost(PostPropagator::priority_class_general); p; p = p->next) {
		if (!p->isModel(s))
			return false;
	}
	return true;
}

bool ParallelHandler::integrate(Solver& s) {
	uint32 rec = recEnd_ + s.receive(received_ + recEnd_, RECEIVE_BUFFER_SIZE - recEnd_);
	if (!rec) { return true; }
	ClauseCreator::Result ret;
	uint32 dl       = s.decisionLevel(), added = 0, i = 0;
	uint32 intFlags = ctrl_->integrateFlags();
	recEnd_         = 0;
	if (lbd_) {
		intFlags |= ClauseCreator::clause_int_lbd;
	}
	do {
		ret    = ClauseCreator::integrate(s, received_[i++], intFlags, Constraint_t::Other);
		added += ret.status != ClauseCreator::status_subsumed;
		if (ret.local) { add(ret.local); }
		if (ret.unit()){ s.stats.addIntegratedAsserting(dl, s.decisionLevel()); dl = s.decisionLevel(); }
		if (!ret.ok()) { while (i != rec) { received_[recEnd_++] = received_[i++]; } }
	} while (i != rec);
	s.stats.addIntegrated(added);
	return !s.hasConflict();
}

void ParallelHandler::add(ClauseHead* h) {
	if (intEnd_ < integrated_.size()) {
		ClauseHead* o = (ClauseHead*)integrated_[intEnd_];
		integrated_[intEnd_] = h;
		assert(o);
		if (!ctrl_->integrateUseHeuristic() || o->locked(*solver_) || o->activity().activity() > 0) {
			solver_->addLearnt(o, o->size(), Constraint_t::Other);
		}
		else {
			o->destroy(solver_, true);
			solver_->stats.removeIntegrated();
		}
	}
	else {
		integrated_.push_back(h);
	}
	if (++intEnd_ >= ctrl_->integrateGrace()) {
		intEnd_ = 0;
	}
}
/////////////////////////////////////////////////////////////////////////////////////////
// Distribution
/////////////////////////////////////////////////////////////////////////////////////////
uint64 ParallelSolveOptions::initPeerMask(uint32 id, Integration::Topology topo, uint32 maxT)  {
	if (topo == Integration::topo_all) { return Distributor::initSet(maxT) ^ Distributor::mask(id); }
	if (topo == Integration::topo_ring){
		uint32 prev = id > 0 ? id - 1 : maxT - 1;
		uint32 next = (id + 1) % maxT;
		return Distributor::mask(prev) | Distributor::mask(next);
	}
	bool ext = topo == Integration::topo_cubex;
	uint32 n = maxT;
	uint32 k = 1;
	for (uint32 i = n / 2; i > 0; i /= 2, k *= 2) { }
	uint64 res = 0, x = 1;
	for (uint32 m = 1; m <= k; m *= 2) {
		uint32 i = m ^ id;
		if      (i < n)         { res |= (x << i);     }
		else if (ext && k != m) { res |= (x << (i^k)); }
	}
	if (ext) {
		uint32 s = k ^ id;
		for(uint32 m = 1; m < k && s >= n; m *= 2) {
			uint32 i = m ^ s;
			if (i < n) { res |= (x << i); }
		}
	}
	assert( (res & (x<<id)) == 0 );
	return res;
}
/////////////////////////////////////////////////////////////////////////////////////////
// GlobalDistribution
/////////////////////////////////////////////////////////////////////////////////////////
GlobalDistribution::GlobalDistribution(const Policy& p, uint32 maxT, uint32 topo) : Distributor(p), queue_(0) {
	typedef ParallelSolveOptions::Integration::Topology Topology;
	assert(maxT <= ParallelSolveOptions::supportedSolvers());
	Topology t = static_cast<Topology>(topo);
	queue_     = new Queue(maxT);
	threadId_  = (ThreadInfo*)alignedAlloc((maxT * sizeof(ThreadInfo)), 64);
	for (uint32 i = 0; i != maxT; ++i) {
		new (&threadId_[i]) ThreadInfo;
		threadId_[i].id       = queue_->addThread();
		threadId_[i].peerMask = ParallelSolveOptions::initPeerMask(i, t, maxT);
	}
}
GlobalDistribution::~GlobalDistribution() {
	static_assert(sizeof(ThreadInfo) == 64, "Invalid size");
	release();
}
void GlobalDistribution::release() {
	if (queue_) {
		for (uint32 i = 0; i != queue_->maxThreads(); ++i) {
			Queue::ThreadId& id = getThreadId(i);
			for (ClausePair n; queue_->tryConsume(id, n); ) {
				if (n.sender != i) { n.lits->release(); }
			}
			threadId_[i].~ThreadInfo();
		}
		delete queue_;
		queue_ = 0;
		alignedFree(threadId_);
	}
}
void GlobalDistribution::publish(const Solver& s, SharedLiterals* n) {
	assert(n->refCount() >= (queue_->maxThreads()-1));
	queue_->publish(ClausePair(s.id(), n), getThreadId(s.id()));
}
uint32 GlobalDistribution::receive(const Solver& in, SharedLiterals** out, uint32 maxn) {
	uint32 r = 0;
	Queue::ThreadId& id = getThreadId(in.id());
	uint64 peers = getPeerMask(in.id());
	for (ClausePair n; r != maxn && queue_->tryConsume(id, n); ) {
		if (n.sender != in.id()) {
			if (inSet(peers, n.sender))  { out[r++] = n.lits; }
			else if (n.lits->size() == 1){ out[r++] = n.lits; }
			else                         { n.lits->release(); }
		}
	}
	return r;
}

/////////////////////////////////////////////////////////////////////////////////////////
// LocalDistribution
/////////////////////////////////////////////////////////////////////////////////////////
LocalDistribution::LocalDistribution(const Policy& p, uint32 maxT, uint32 topo) : Distributor(p), thread_(0), numThread_(0) {
	typedef ParallelSolveOptions::Integration::Topology Topology;
	assert(maxT <= ParallelSolveOptions::supportedSolvers());
	Topology t = static_cast<Topology>(topo);
	thread_    = new ThreadData*[numThread_ = maxT];
	size_t sz  = ((sizeof(ThreadData) + 63) / 64) * 64;
	for (uint32 i = 0; i != maxT; ++i) {
		ThreadData* ti = new (alignedAlloc(sz, 64)) ThreadData;
		ti->received.init(&ti->sentinal);
		ti->peers = ParallelSolveOptions::initPeerMask(i, t, maxT);
		ti->free  = 0;
		thread_[i]= ti;
	}
}
LocalDistribution::~LocalDistribution() {
	while (numThread_) {
		ThreadData* ti      = thread_[--numThread_];
		thread_[numThread_] = 0;
		for (QNode* n; (n = ti->received.pop()) != 0; ) {
			static_cast<SharedLiterals*>(n->data)->release();
		}
		ti->~ThreadData();
		alignedFree(ti);
	}
	for (MPSCPtrQueue::RawNode* n; (n = blocks_.tryPop()) != 0; ) {
		alignedFree(n);
	}
	delete [] thread_;
}

void LocalDistribution::freeNode(uint32 tId, QNode* n) const {
	if (n != &thread_[tId]->sentinal) {
		n->next = thread_[tId]->free;
		thread_[tId]->free = n;
	}
}

LocalDistribution::QNode* LocalDistribution::allocNode(uint32 tId, SharedLiterals* clause) {
	for (ThreadData* td = thread_[tId];;) {
		if (QNode* n = td->free) {
			td->free = static_cast<QNode*>(static_cast<MPSCPtrQueue::RawNode*>(n->next));
			n->data  = clause;
			return n;
		}
		// alloc a new block of node;
		const uint32 nNodes = 128;
		QNode* raw = (QNode*)alignedAlloc(sizeof(QNode) * nNodes, 64);
		// add nodes [1, nNodes) to free list
		for (uint32 i = 1; i != nNodes-1; ++i) {
			raw[i].next = &raw[i+1];
		}
		raw[nNodes-1].next = 0;
		td->free = &raw[1];
		// use first node to link to block list
		blocks_.push(raw);
	}
}

void LocalDistribution::publish(const Solver& s, SharedLiterals* n) {
	assert(n->refCount() >= (numThread_-1));
	uint32 sender = s.id();
	uint32 size   = n->size();
	uint32 decRef = 0;
	for (uint32 i = 0; i != numThread_; ++i) {
		if (i == sender) { continue; }
		if (size > 1 && !inSet(thread_[i]->peers, sender)) { ++decRef; }
		else {
			QNode* node = allocNode(sender, n);
			thread_[i]->received.push(node);
		}
	}
	if (decRef) { n->release(decRef); }
}
uint32 LocalDistribution::receive(const Solver& in, SharedLiterals** out, uint32 maxn) {
	MPSCPtrQueue& q = thread_[in.id()]->received;
	QNode*        n = 0;
	uint32        r = 0;
	while (r != maxn && (n = q.pop()) != 0) {
		out[r++] = static_cast<SharedLiterals*>(n->data);
		freeNode(in.id(), n);
	}
	return r;
}

} } // namespace Clasp::mt

