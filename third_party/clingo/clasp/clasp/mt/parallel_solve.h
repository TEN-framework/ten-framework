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
#ifndef CLASP_PARALLEL_SOLVE_H_INCLUDED
#define CLASP_PARALLEL_SOLVE_H_INCLUDED

#ifdef _MSC_VER
#pragma once
#endif
#include <clasp/config.h>
#if !CLASP_HAS_THREADS
#error Thread support required for parallel solving
#endif
#include <clasp/solve_algorithms.h>
#include <clasp/constraint.h>
#include <clasp/shared_context.h>
#include <clasp/solver_types.h>
#include <clasp/util/multi_queue.h>
#include <clasp/mt/thread.h>

/*!
 * \file
 * \brief Defines classes controlling multi-threaded parallel solving.
 */
namespace Clasp {
//! Namespace for types and functions needed for implementing multi-threaded parallel solving.
namespace mt {

/**
 * \defgroup mt Multi-threading
 * \brief Parallel solving and related classes.
 * \ingroup enumerator
 */
//@{

class ParallelHandler;
class ParallelSolve;

//! Options for controlling parallel solving.
struct ParallelSolveOptions : BasicSolveOptions {
	//! Nogood distribution options.
	struct Distribution : Distributor::Policy {
		enum Mode { mode_global = 0, mode_local = 1 };
		Distribution(Mode m = mode_global) : Distributor::Policy(), mode(m) {}
		Distributor::Policy& policy() { return *this; }
		uint32 mode;
	};
	ParallelSolveOptions() {}
	//! Algorithm options.
	struct Algorithm {
		//! Possible search strategies.
		enum SearchMode { mode_split = 0, mode_compete  = 1 };
		Algorithm() : threads(1), mode(mode_compete) {}
		uint32     threads;
		SearchMode mode;
	};
	//! Nogood integration options.
	struct Integration {
		static const uint32 GRACE_MAX = (1u<<28)-1;
		Integration() :  grace(1024), filter(filter_gp), topo(topo_all) {}
		enum Filter   { filter_no = 0, filter_gp = 1, filter_sat = 2, filter_heuristic = 3 };
		enum Topology { topo_all  = 0, topo_ring = 1, topo_cube  = 2, topo_cubex = 3       };
		uint32 grace : 28; /**< Lower bound on number of shared nogoods to keep. */
		uint32 filter: 2;  /**< Filter for integrating shared nogoods (one of Filter). */
		uint32 topo  : 2;  /**< Integration topology */
	};
	//! Global restart options.
	struct GRestarts {
		GRestarts():maxR(0) {}
		uint32           maxR;
		ScheduleStrategy sched;
	};
	Integration  integrate; //!< Nogood integration options to apply during search.
	Distribution distribute;//!< Nogood distribution options to apply during search.
	GRestarts    restarts;  //!< Global restart strategy to apply during search.
	Algorithm    algorithm; //!< Parallel algorithm to use.
	//! Allocates a new solve object.
	SolveAlgorithm* createSolveObject() const;
	//! Returns the number of threads that can run concurrently on the current hardware.
	static uint32   recommendedSolvers()     { return Clasp::mt::thread::hardware_concurrency(); }
	//! Returns number of maximal number of supported threads.
	static uint32   supportedSolvers()       { return 64; }
	//! Returns the peers of the solver with the given id assuming the given topology.
	static uint64   initPeerMask(uint32 sId, Integration::Topology topo, uint32 numThreads);
	uint32          numSolver()        const { return algorithm.threads; }
	void            setSolvers(uint32 i)     { algorithm.threads = std::max(uint32(1), i); }
	bool            defaultPortfolio() const { return algorithm.mode == Algorithm::mode_compete; }
};

//! A parallel algorithm for multi-threaded solving with and without search-space splitting.
/*!
 * The class adapts clasp's basic solve algorithm
 * to a parallel solve algorithm that solves
 * a problem using a given number of threads.
 * It supports guiding path based solving, portfolio based solving, as well
 * as a combination of these two approaches.
 */
class ParallelSolve : public SolveAlgorithm {
public:
	explicit ParallelSolve(const ParallelSolveOptions& opts);
	~ParallelSolve();
	// base interface
	virtual bool interrupted() const;
	virtual void resetSolve();
	virtual void enableInterrupts();
	// own interface
	//! Returns the number of active threads.
	uint32 numThreads()            const;
	bool   integrateUseHeuristic() const { return test_bit(intFlags_, 31); }
	uint32 integrateGrace()        const { return intGrace_; }
	uint32 integrateFlags()        const { return intFlags_; }
	uint64 hasErrors()             const;
	//! Requests a global restart.
	void   requestRestart();
	bool   handleMessages(Solver& s);
	bool   integrateModels(Solver& s, uint32& mCount);
	void   pushWork(LitVec* gp);
	bool   commitModel(Solver& s);
	bool   commitUnsat(Solver& s);
	enum GpType { gp_none = 0, gp_split = 1, gp_fixed = 2 };
private:
	ParallelSolve(const ParallelSolve&);
	ParallelSolve& operator=(const ParallelSolve&);
	typedef SingleOwnerPtr<const LitVec> PathPtr;
	enum ErrorCode { LogicError = 1, RuntimeError = 2, OutOfMemory = 3, UnknownError = 4 };
	enum           { masterId = 0 };
	// -------------------------------------------------------------------------------------------
	// Thread setup
	struct EntryPoint;
	void   destroyThread(uint32 id);
	void   allocThread(uint32 id, Solver& s);
	int    joinThreads();
	// -------------------------------------------------------------------------------------------
	// Algorithm steps
	void   setIntegrate(uint32 grace, uint8 filter);
	void   setRestarts(uint32 maxR, const ScheduleStrategy& rs);
	bool   beginSolve(SharedContext& ctx, const LitVec& assume);
	bool   doSolve(SharedContext& ctx, const LitVec& assume);
	void   doStart(SharedContext& ctx, const LitVec& assume);
	int    doNext(int last);
	void   doStop();
	void   doDetach();
	bool   doInterrupt();
	void   solveParallel(uint32 id);
	void   initQueue();
	bool   requestWork(Solver& s, PathPtr& out);
	void   terminate(Solver& s, bool complete);
	bool   waitOnSync(Solver& s);
	void   exception(uint32 id, PathPtr& path, ErrorCode e, const char* what);
	void   reportProgress(const Event& ev) const;
	void   reportProgress(const Solver& s, const char* msg) const;
	// -------------------------------------------------------------------------------------------
	typedef ParallelSolveOptions::Distribution Distribution;
	struct SharedData;
	// SHARED DATA
	SharedData*       shared_;       // Shared control data
	ParallelHandler** thread_;       // Thread-locl control data
	// READ ONLY
	Distribution      distribution_; // distribution options
	uint32            maxRestarts_;  // disable global restarts once reached
	uint32            intGrace_ : 30;// grace period for clauses to integrate
	uint32            intTopo_  :  2;// integration topology
	uint32            intFlags_;     // bitset controlling clause integration
	bool              modeSplit_;
};
//! An event type for debugging messages sent between threads.
struct MessageEvent : SolveEvent<MessageEvent> {
	enum Action { sent, received, completed };
	MessageEvent(const Solver& s, const char* message, Action a, double t = 0.0)
		: SolveEvent<MessageEvent>(s, verbosity_high), msg(message), time(t) { op = (uint32)a; }
	const char* msg;    // name of message
	double      time;   // only for action completed
};
//! A per-solver (i.e. thread) class that implements message handling and knowledge integration.
/*!
 * The class adds itself as a post propagator to the given solver. During propagation
 * it checks for new messages and lemmas to integrate.
 */
class ParallelHandler : public MessageHandler {
public:
	typedef ParallelSolve::GpType GpType;

	//! Creates a new parallel handler to be used in the given solve group.
	/*!
	 * \param ctrl The object controlling the parallel solve operation.
	 * \param s    The solver that is to be controlled by this object.
	 */
	explicit ParallelHandler(ParallelSolve& ctrl, Solver& s);
	~ParallelHandler();
	//! Attaches the object's solver to ctx and adds this object as a post propagator.
	bool attach(SharedContext& ctx);
	//! Removes this object from the list of post propagators of its solver and detaches the solver from ctx.
	void detach(SharedContext& ctx, bool fastExit);

	bool setError(int e);
	int  error() const   { return (int)error_; }
	void setWinner()     { win_ = 1; }
	bool winner() const  { return win_ != 0; }
	void setThread(Clasp::mt::thread& x) { assert(!joinable()); x.swap(thread_); assert(joinable()); }

	//! True if *this has an associated thread of execution, false otherwise.
	bool joinable() const { return thread_.joinable(); }
	//! Waits for the thread of execution associated with *this to finish.
	int join() { if (joinable()) { thread_.join(); } return error(); }

	// overridden methods

	//! Integrates new information.
	bool propagateFixpoint(Solver& s, PostPropagator*);
	bool handleMessages() { return ctrl_->handleMessages(solver()); }
	void reset() { up_ = 1; }
	bool simplify(Solver& s, bool re);
	//! Checks whether new information has invalidated current model.
	bool isModel(Solver& s);

	// own interface
	bool isModelLocked(Solver& s);

	// TODO: make functions virtual once necessary

	//! Returns true if handler's guiding path is disjoint from all others.
	bool disjointPath() const { return gp_.type == ParallelSolve::gp_split; }
	//! Returns true if handler has a guiding path.
	bool hasPath()      const { return gp_.type != ParallelSolve::gp_none; }
	void setGpType(GpType t)  { gp_.type = t; }

	//! Entry point for solving the given guiding path.
	/*!
	 * \param solve   The object used for solving.
	 * \param type    The guiding path's type.
	 * \param restart Request restart after restart number of conflicts.
	 */
	ValueRep solveGP(BasicSolve& solve, GpType type, uint64 restart);

	/*!
	 * \name Message handlers
	 * \note
	 *   Message handlers are intended as callbacks for ParallelSolve::handleMessages().
	 *   They shall not change the assignment of the solver object.
	 */
	//@{

	//! Algorithm is about to terminate.
	/*!
	 * Removes this object from the solver's list of post propagators.
	 */
	void handleTerminateMessage();

	//! Request for split.
	/*!
	 * Splits off a new guiding path and adds it to the control object.
	 * \pre The guiding path of this object is "splittable"
	 */
	void handleSplitMessage();

	//! Request for (global) restart.
	/*!
	 * \return true if restart is valid, else false.
	 */
	bool handleRestartMessage();

	Solver& solver() { return *solver_; }
	//@}
private:
	void add(ClauseHead* h);
	void clearDB(Solver* s);
	bool integrate(Solver& s);
	typedef PodVector<Constraint*>::type ClauseDB;
	typedef SharedLiterals**             RecBuffer;
	struct GP {
		uint64 restart;  // don't give up before restart number of conflicts
		uint32 modCount; // integration counter for synchronizing models
		GpType type;     // type of gp
		void reset(uint64 r = UINT64_MAX, GpType t = ParallelSolve::gp_none) {
			restart  = r;
			modCount = 0;
			type     = t;
		}
	};
	enum { RECEIVE_BUFFER_SIZE = 32 };
	Clasp::mt::thread thread_;     // active thread or empty for master
	GP                gp_;         // active guiding path
	ParallelSolve*    ctrl_;       // message source
	Solver*           solver_;     // associated solver
	RecBuffer         received_;   // received clauses not yet integrated
	ClauseDB          integrated_; // integrated clauses
	uint32            recEnd_;     // where to put next received clause
	uint32            intEnd_;     // where to put next clause
	uint32            error_:28;   // error code or 0 if ok
	uint32            win_  : 1;   // 1 if thread was the first to terminate the search
	uint32            up_   : 1;   // 1 if next propagate should check for new lemmas/models
	uint32            act_  : 1;   // 1 if gp is active
	uint32            lbd_  : 1;   // 1 if integrate should compute lbds
};
//! A class that uses a global list to exchange nogoods between threads.
class GlobalDistribution : public Distributor {
public:
	explicit GlobalDistribution(const Policy& p, uint32 maxShare, uint32 topo);
	~GlobalDistribution();
	uint32  receive(const Solver& in, SharedLiterals** out, uint32 maxOut);
	void    publish(const Solver& source, SharedLiterals* n);
private:
	void release();
	struct ClausePair {
		ClausePair(uint32 sId = UINT32_MAX, SharedLiterals* x = 0) : sender(sId), lits(x) {}
		uint32          sender;
		SharedLiterals* lits;
	};
	class Queue : public MultiQueue<ClausePair> {
	public:
		typedef MultiQueue<ClausePair> base_type;
		using base_type::publish;
		Queue(uint32 m) : base_type(m) {}
	};
	struct ThreadInfo {
		uint64          peerMask;
		union {
			Queue::ThreadId id;
			uint64          rep;
		};
		char            pad[64 - (sizeof(uint64)*2)];
	};
	Queue::ThreadId& getThreadId(uint32 sId) const { return threadId_[sId].id; }
	uint64           getPeerMask(uint32 sId) const { return threadId_[sId].peerMask; }
	Queue*           queue_;
	ThreadInfo*      threadId_;
};
//! A class that uses thread-local lists to exchange nogoods between threads.
class LocalDistribution : public Distributor {
public:
	explicit LocalDistribution(const Policy& p, uint32 maxShare, uint32 topo);
	~LocalDistribution();
	uint32  receive(const Solver& in, SharedLiterals** out, uint32 maxOut);
	void    publish(const Solver& source, SharedLiterals* n);
private:
	typedef Detail::RawStack   RawStack;
	typedef MPSCPtrQueue::Node QNode;
	enum { BLOCK_CAP = 128 };
	QNode* allocNode(uint32 tId, SharedLiterals* clause);
	void   freeNode(uint32  tId, QNode* n) const;
	struct ThreadData {
		MPSCPtrQueue received; // queue holding received clauses
		uint64       peers;    // set of peers from which this thread receives clauses
		QNode        sentinal; // sentinal node for simplifying queue impl
		QNode*       free;     // local free list - only accessed by this thread
	}**            thread_;    // one entry for each thread
	RawStack       blocks_;    // allocated node blocks
	uint32         numThread_; // number of threads, i.e. size of array thread_
};
//@}
} }
#endif

