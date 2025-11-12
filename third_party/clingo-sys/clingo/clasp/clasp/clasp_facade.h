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
#ifndef CLASP_CLASP_FACADE_H_INCLUDED
#define CLASP_CLASP_FACADE_H_INCLUDED
#ifdef _MSC_VER
#pragma once
#endif

#include <clasp/config.h>
#include <clasp/program_builder.h>
#include <clasp/parser.h>
#include <clasp/logic_program.h>
#include <clasp/shared_context.h>
#include <clasp/enumerator.h>
#include <clasp/solver_types.h>
#include <clasp/solve_algorithms.h>
#if CLASP_HAS_THREADS
#include <clasp/mt/parallel_solve.h>
namespace Clasp {
	//! Options for controlling enumeration and solving.
	struct SolveOptions : Clasp::mt::ParallelSolveOptions, EnumOptions {};
}
#else
namespace Clasp {
	struct SolveOptions : Clasp::BasicSolveOptions, EnumOptions {};
}
#endif

/*!
 * \file
 * \brief High-level API
 *
 * This file provides a facade around the clasp library.
 * I.e. a simplified interface for (multishot) solving a problem using
 * some configuration (set of parameters).
 * \ingroup facade
 */
namespace Clasp {
/////////////////////////////////////////////////////////////////////////////////////////
// Configuration
/////////////////////////////////////////////////////////////////////////////////////////
/*!
 * \defgroup facade Facade
 * \brief Simplified interface for (multishot) solving.
 *
 * @{
 */
//! Configuration object for configuring solving via the ClaspFacade.
class ClaspConfig : public BasicSatConfig {
public:
	//! Interface for injecting user-provided configurations.
	class Configurator {
	public:
		virtual ~Configurator();
		virtual void prepare(SharedContext&);
		virtual bool applyConfig(Solver& s) = 0;
		virtual void unfreeze(SharedContext&);
	};
	typedef BasicSatConfig UserConfig;
	typedef Solver**       SolverIt;
	typedef Asp::LogicProgram::AspOptions AspOptions;
	ClaspConfig();
	~ClaspConfig();
	// Base interface
	void           prepare(SharedContext&);
	void           reset();
	Configuration* config(const char*);
	//! Adds an unfounded set checker to the given solver if necessary.
	/*!
	 * If asp.suppMod is false and the problem in s is a non-tight asp-problem,
	 * the function adds an unfounded set checker to s.
	 */
	bool           addPost(Solver& s) const;
	// own interface
	UserConfig*    testerConfig() const { return tester_; }
	UserConfig*    addTesterConfig();
	//! Registers c as additional callback for when addPost() is called.
	/*!
	 * \param c         Additional configuration to apply.
	 * \param ownership If Ownership_t::Acquire, ownership of c is transferred to the configuration object.
	 * \param once      Whether c should be called once in the first step or also in each subsequent step.
	 */
	void           addConfigurator(Configurator* c, Ownership_t::Type ownership = Ownership_t::Retain, bool once = true);
	void           unfreeze(SharedContext&);
	SolveOptions   solve; /*!< Options for solve algorithm and enumerator. */
	AspOptions     asp;   /*!< Options for asp preprocessing.      */
	ParserOptions  parse; /*!< Options for input parser. */
private:
	struct Impl;
	ClaspConfig(const ClaspConfig&);
	ClaspConfig& operator=(const ClaspConfig&);
	UserConfig* tester_;
	Impl*       impl_;
};
/////////////////////////////////////////////////////////////////////////////////////////
// ClaspFacade
/////////////////////////////////////////////////////////////////////////////////////////
//! Result of a solving step.
struct SolveResult {
	//! Possible solving results.
	enum Base {
		UNKNOWN  = 0, //!< Satisfiability unknown - a given solve limit was hit.
		SAT      = 1, //!< Problem is satisfiable (a model was found).
		UNSAT    = 2, //!< Problem is unsatisfiable.
	};
	//! Additional flags applicable to a solve result.
	enum Ext {
		EXT_EXHAUST  = 4, //!< Search space is exhausted.
		EXT_INTERRUPT= 8, //!< The run was interrupted from outside.
	};
	bool sat()        const { return *this == SAT; }
	bool unsat()      const { return *this == UNSAT; }
	bool unknown()    const { return *this == UNKNOWN; }
	bool exhausted()  const { return (flags & EXT_EXHAUST)   != 0; }
	bool interrupted()const { return (flags & EXT_INTERRUPT) != 0; }
	operator Base()   const { return static_cast<Base>(flags & 3u);}
	uint8 flags;  //!< Set of Base and Ext flags.
	uint8 signal; //!< Term signal or 0.
};

//! A bitmask type for representing supported solve modes.
struct SolveMode_t {
	//! Named constants.
	POTASSCO_ENUM_CONSTANTS(SolveMode_t,
		Default = 0, /**< Solve synchronously in current thread. */
		Async   = 1, /**< Solve asynchronously in worker thread. */
		Yield   = 2, /**< Yield models one by one via handle.    */
		AsyncYield);
	friend inline SolveMode_t operator|(SolveMode_t::E x, SolveMode_t::E y) { return SolveMode_t(static_cast<uint32>(x)|static_cast<uint32>(y)); }
	friend inline SolveMode_t operator|(SolveMode_t x, SolveMode_t::E y)    { return SolveMode_t(static_cast<uint32>(x)|static_cast<uint32>(y)); }
	friend inline SolveMode_t operator|(SolveMode_t::E x, SolveMode_t y)    { return SolveMode_t(static_cast<uint32>(x)|static_cast<uint32>(y)); }
};
//! Provides a simplified interface to the services of the clasp library.
class ClaspFacade : public ModelHandler {
	struct SolveData;
	struct SolveStrategy;
public:
	//! A handle to a possibly asynchronously computed SolveResult.
	class SolveHandle {
	public:
		typedef SolveResult   Result;
		typedef const Model*  ModelRef;
		typedef const LitVec* CoreRef;
		explicit SolveHandle(SolveStrategy*);
		SolveHandle(const SolveHandle&);
		~SolveHandle();
		SolveHandle& operator=(SolveHandle temp)             { swap(*this, temp); return *this; }
		friend void swap(SolveHandle& lhs, SolveHandle& rhs) { std::swap(lhs.strat_, rhs.strat_); }
		/*!
		 * \name Blocking functions
		 * @{ */
		//! Waits until a result is ready and returns it.
		Result   get()              const;
		//! Returns an unsat core if get() returned unsat under assumptions.
		CoreRef  unsatCore()        const;
		//! Waits until a result is ready and returns it if it is a model.
		/*!
		 * \note If the corresponding solve operation was not started with
		 * SolveMode_t::Yield, the function always returns 0.
		 * \note A call to resume() invalidates the returned model and starts
		 * the search for the next model.
		 */
		ModelRef model()            const;
		//! Waits until a result is ready.
		void     wait()             const;
		//! Waits for a result but for at most sec seconds.
		bool     waitFor(double sec)const;
		//! Tries to cancel the active operation.
		void     cancel()           const;
		//! Behaves like resume() followed by return model() != 0.
		bool     next()             const;
		//@}
		/*!
		 * \name Non-blocking functions
		 * @{ */
		//! Tests whether a result is ready.
		bool     ready()            const;
		//! Tests whether the operation was interrupted and if so returns the interruption signal.
		int      interrupted()      const;
		//! Tests whether a result is ready and has a stored exception.
		bool     error()            const;
		//! Tests whether the operation is still active.
		bool     running()          const;
		//! Releases ownership of the active model and schedules search for the next model.
		void     resume()           const;
		//@}
	private:
		SolveStrategy* strat_;
	};
	typedef SolveResult Result;
	typedef Potassco::AbstractStatistics AbstractStatistics;
	//! Type summarizing one or more solving steps.
	struct Summary {
		typedef const ClaspFacade* FacadePtr;
		void init(ClaspFacade& f);
		//! Logic program elements added in the current step or 0 if not an asp problem.
		const Asp::LpStats*  lpStep()       const;
		//! Logic program stats or 0 if not an asp problem.
		const Asp::LpStats*  lpStats()      const;
		//! Active problem.
		const SharedContext& ctx()          const { return facade->ctx; }
		/*!
		 * \name Result functions
		 * Solve and enumeration result - not accumulated.
		 * @{
		 */
		bool                 sat()          const { return result.sat(); }
		bool                 unsat()        const { return result.unsat(); }
		bool                 complete()     const { return result.exhausted(); }
		bool                 optimum()      const { return costs() && (complete() || model()->opt); }
		const Model*         model()        const;
		const LitVec*        unsatCore()    const;
		const char*          consequences() const; /**< Cautious/brave reasoning active? */
		bool                 optimize()     const; /**< Optimization active? */
		const SumVec*        costs()        const; /**< Models have associated costs? */
		uint64               optimal()      const; /**< Number of optimal models found. */
		bool                 hasLower()     const;
		SumVec               lower()        const;
		//@}
		//! Visits this summary object.
		void accept(StatsVisitor& out) const;
		FacadePtr facade;    //!< Facade object of this run.
		double    totalTime; //!< Total wall clock time.
		double    cpuTime;   //!< Total cpu time.
		double    solveTime; //!< Wall clock time for solving.
		double    unsatTime; //!< Wall clock time to prove unsat.
		double    satTime;   //!< Wall clock time to first model.
		uint64    numEnum;   //!< Total models enumerated.
		uint64    numOptimal;//!< Optimal models enumerated.
		uint32    step;      //!< Step number (multishot solving).
		Result    result;    //!< Result of step.
	};
	ClaspFacade();
	~ClaspFacade();

	/*!
	 * \name Query functions
	 * Functions for checking the state of this object.
	 * @{ */
	//! Returns whether the problem is still valid.
	bool               ok()                  const { return program() ? program()->ok() : ctx.ok(); }
	//! Returns whether the active step is ready for solving.
	bool               prepared()            const;
	//! Returns whether the active step is currently being solved.
	bool               solving()             const;
	//! Returns whether the active step has been solved, i.e., has a result.
	bool               solved()              const;
	//! Returns whether solving of the active step was interrupted.
	bool               interrupted()         const;
	//! Returns the summary of the active step.
	const Summary&     summary()             const { return step_; }
	//! Returns the summary of the active (accu = false) or all steps.
	const Summary&     summary(bool accu)    const;
	//! Returns solving statistics or throws std::logic_error if solving() is true.
	AbstractStatistics*getStats()            const;
	//! Returns the active configuration.
	const ClaspConfig* config()              const { return config_;}
	//! Returns the current solving step (starts at 0).
	int                step()                const { return (int)step_.step;}
	//! Returns the result of the active step (unknown if run is not yet completed).
	Result             result()              const { return step_.result; }
	//! Returns the active program or 0 if it was already released.
	ProgramBuilder*    program()             const { return builder_.get(); }
	//! Returns whether program updates are enabled.
	bool               incremental()         const;
	//! Returns the active enumerator or 0 if there is none.
	Enumerator*        enumerator()          const;
	//@}

	//! Event type used to signal that a new step has started.
	struct StepStart : Event_t<StepStart> {
		explicit StepStart(const ClaspFacade& f) : Event_t<StepStart>(subsystem_facade, verbosity_quiet), facade(&f) {}
		const ClaspFacade* facade;
	};
	//! Event type used to signal that a solve step has terminated.
	struct StepReady : Event_t<StepReady> {
		explicit StepReady(const Summary& x) : Event_t<StepReady>(subsystem_facade, verbosity_quiet), summary(&x) {}
		const Summary* summary;
	};

	SharedContext ctx; //!< Context-object used to store problem.

	/*!
	 * \name Start functions
	 * Functions for defining a problem.
	 * Calling one of the start functions discards any previous problem
	 * and emits a StepStart event.
	 * @{ */
	//! Starts definition of an ASP-problem.
	Asp::LogicProgram& startAsp(ClaspConfig& config, bool enableProgramUpdates = false);
	//! Starts definition of a SAT-problem.
	SatBuilder&        startSat(ClaspConfig& config);
	//! Starts definition of a PB-problem.
	PBBuilder&         startPB(ClaspConfig& config);
	//! Starts definition of a problem of type t.
	ProgramBuilder&    start(ClaspConfig& config, ProblemType t);
	//! Starts definition of a problem given in stream.
	ProgramBuilder&    start(ClaspConfig& config, std::istream& stream);
	//! Enables support for program updates if supported by the program.
	/*!
	 * \pre program() != 0 and not prepared().
	 * \return true if program updates are supported. Otherwise, false.
	 */
	bool               enableProgramUpdates();
	//! Enables support for (asynchronous) solve interrupts.
	void               enableSolveInterrupts();
	//! Disables program disposal in non-incremental mode after problem has been prepared for solving.
	/*!
	 * \pre program() != 0 and not prepared().
	 */
	void               keepProgram();
	//! Tries to detect the problem type from the given input stream.
	static ProblemType detectProblemType(std::istream& str);
	//! Tries to read the next program part from the stream passed to start().
	/*!
	 * \return false if nothing was read because the stream is exhausted, solving was interrupted,
	 * or the problem is unconditionally unsat.
	 */
	bool               read();

	//@}

	/*!
	 * \name Solve functions
	 * Functions for solving a problem.
	 * @{ */

	enum EnumMode  { enum_volatile, enum_static };

	//! Finishes the definition of a problem and prepares it for solving.
	/*!
	 * \pre !solving()
	 * \post prepared() || !ok()
	 * \param m Mode to be used for handling enumeration-related knowledge.
	 *          If m is enum_volatile, enumeration knowledge is learnt under an
	 *          assumption that is retracted on program update. Otherwise,
	 *          no special assumption is used and enumeration-related knowledge
	 *          might become unretractable.
	 * \note If solved() is true, prepare() first starts a new solving step.
	 */
	void               prepare(EnumMode m = enum_volatile);

	//! Solves the current problem.
	/*!
	 * If prepared() is false, the function first calls prepare() to prepare the problem for solving.
	 * \pre !solving()
	 * \post solved()
	 * \param a A list of unit-assumptions under which solving should operate.
	 * \param eh An optional event handler that is notified on each model and
	 *           once the solve operation has completed.
	 */
	Result             solve(const LitVec& a = LitVec(), EventHandler* eh = 0);
	Result             solve(EventHandler* eh) { return solve(LitVec(), eh); }

	//! Solves the current problem using the given solve mode.
	/*!
	 * If prepared() is false, the function first calls prepare() to prepare the problem for solving.
	 * \pre !solving()
	 * \param mode The solve mode to use.
	 * \param a A list of unit-assumptions under which solving should operate.
	 * \param eh An optional event handler that is notified on each model and
	 *           once the solve operation has completed.
	 * \throws std::logic_error   if mode contains SolveMode_t::Async but thread support is disabled.
	 * \throws std::runtime_error if mode contains SolveMode_t::Async but solve is unable to start a thread.
	 *
	 * \note If mode contains SolveMode_t::Async, the optional event handler is notified in the
	 *       context of the asynchronous thread.
	 *
	 * \note If mode contains SolveMode_t::Yield, models are signaled one by one via the
	 *       returned handle object.
	 *       It is the caller's responsibility to finish the solve operation,
	 *       either by extracting models until SolveHandle::model() returns 0, or
	 *       by calling SolveHandle::cancel().
	 *
	 * To iterate over models one by one use a loop like:
	 * \code
	 * SolveMode_t p = ...
	 * for (auto it = facade.solve(p|SolveMode_t::Yield); it.model(); it.resume()) {
	 *   printModel(*it.model());
	 * }
	 * \endcode
	 */
	SolveHandle        solve(SolveMode_t mode, const LitVec& a = LitVec(), EventHandler* eh = 0);

	//! Tries to interrupt the active solve operation.
	/*!
	 * The function sends the given signal to the active solve operation.
	 * If no solve operation is active (i.e. solving() is false), the signal
	 * is queued and applied to the next solve operation.
	 *
	 * \param sig The signal to raise or 0, to re-raises a previously queued signal.
	 * \return false if no operation was interrupted, because
	 *         there is no active solve operation,
	 *         or the operation does not support interrupts,
	 *         or sig was 0 and there was no queued signal.
	 *
	 * \see enableSolveInterrupts()
	 */
	bool               interrupt(int sig);

	//! Forces termination of the current solving step.
	/*!
	 * \post solved()
	 * \return summary(true)
	 */
	const Summary&     shutdown();

	//! Starts update of the active problem.
	/*!
	 * \pre solving() is false and program updates are enabled (incremental() is true).
	 * \post !solved()
	 * \param updateConfig If true, the function applies any configuration changes.
	 * \param sigQ An action to be performed for any queued signal.
	 *        The default is to apply the signal to the next solve operation, while
	 *        SIGN_IGN can be used to discard queued signals.
	 */
	ProgramBuilder&    update(bool updateConfig, void (*sigQ)(int));
	ProgramBuilder&    update(bool updateConfig = false);
	//@}
private:
	struct Statistics;
	typedef SingleOwnerPtr<ProgramBuilder> BuilderPtr;
	typedef SingleOwnerPtr<SolveData>      SolvePtr;
	typedef SingleOwnerPtr<Summary>        SummaryPtr;
	typedef SingleOwnerPtr<Statistics>     StatsPtr;
	void   init(ClaspConfig& cfg, bool discardProblem);
	void   initBuilder(ProgramBuilder* in);
	bool   isAsp() const { return program() && type_ == Problem_t::Asp; }
	void   discardProblem();
	void   startStep(uint32 num);
	Result stopStep(int signal, bool complete);
	void   updateStats();
	bool   onModel(const Solver& s, const Model& m);
	void   doUpdate(ProgramBuilder* p, bool updateConfig, void (*sig)(int));
	ProblemType  type_;
	Summary      step_;
	LitVec       assume_;
	ClaspConfig* config_;
	BuilderPtr   builder_;
	SummaryPtr   accu_;
	StatsPtr     stats_; // statistics: only if requested
	SolvePtr     solve_; // NOTE: last so that it is destroyed first;
};

/**
 * \example example2.cpp
 * This is an example of how to use the ClaspFacade class for basic solving.
 *
 * \example example3.cpp
 * This is an example of how to use the ClaspFacade class for generator-based solving.
 */

//!@}

}
#endif
