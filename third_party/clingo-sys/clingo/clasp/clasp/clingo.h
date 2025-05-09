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
#ifndef CLASP_CLINGO_H_INCLUDED
#define CLASP_CLINGO_H_INCLUDED
/*!
 * \file
 * \brief Types for implementing theory propagation from clingo.
 */
#include <potassco/clingo.h>
#include <clasp/clasp_facade.h>
#include <clasp/solver.h>
namespace Clasp {

/*!
 * \defgroup clingo Clingo
 * \brief Additional classes mainly used by clingo.
 * \ingroup facade
 * @{ */

//! Lock interface called by libclasp during (multi-threaded) theory propagation.
/*!
 * The interface may be implemented by the application to lock
 * certain global data structures. For example, in clingo,
 * this interface wraps python's global interpreter lock.
 */
class ClingoPropagatorLock {
public:
	virtual ~ClingoPropagatorLock();
	virtual void lock()   = 0;
	virtual void unlock() = 0;
};

//! Supported check modes for clingo propagators.
struct ClingoPropagatorCheck_t {
	enum Type {
		No       = 0u, //!< Never call AbstractPropagator::check().
		Total    = 1u, //!< Call AbstractPropagator::check() only on total assignment.
		Fixpoint = 2u, //!< Call AbstractPropagator::check() on every propagation fixpoint.
		Both     = 3u  //!< Call AbstractPropagator::check() on both fixpoints and total assignment.
	};
};

//! Supported undo modes for clingo propagators.
struct ClingoPropagatorUndo_t {
	enum Type {
		Default  = 0u, //!< Call AbstractPropagator::undo() only on levels with non-empty changelist.
		Always   = 1u  //!< Call AbstractPropagator::undo() on all levels that have been propagated or checked.
	};
};

//! Initialization adaptor for a Potassco::AbstractPropagator.
/*!
 * The class provides a function for registering watches for the propagator.
 * Furthermore, it can be added to a clasp configuration so that
 * a (suitably adapted) propagator is added to solvers that are attached to the configuration.
 */
class ClingoPropagatorInit : public ClaspConfig::Configurator {
public:
	typedef ClingoPropagatorCheck_t::Type CheckType;
	typedef ClingoPropagatorUndo_t::Type  UndoType;
	//! Creates a new adaptor.
	/*!
	 * \param cb The (theory) propagator that should be added to solvers.
	 * \param lock An optional lock that should be applied during theory propagation.
	 *
	 * If lock is not null, calls to cb are wrapped in a lock->lock()/lock->unlock() pair
	 */
	ClingoPropagatorInit(Potassco::AbstractPropagator& cb, ClingoPropagatorLock* lock = 0, CheckType check = ClingoPropagatorCheck_t::Total);
	~ClingoPropagatorInit();
	// base class
	virtual void prepare(SharedContext&);
	//! Adds a ClingoPropagator adapting the propagator() to s.
	virtual bool applyConfig(Solver& s);
	virtual void unfreeze(SharedContext&);

	// for clingo
	//! Sets the type of checks to enable during solving.
	/*!
	 * \param checkMode A set of ClingoPropagatorCheck_t::Type values.
	 */
	void enableClingoPropagatorCheck(CheckType checkMode);

	//! Sets the undo mode to use when checks are enabled.
	/*!
	 * \param undoMode The undo mode to use.
	 *
	 * \note By default, AbstractPropagator::undo() is only called for levels on which
	 *       at least one watched literal has been assigned. However, if undoMode is set
	 *       to "Always", AbstractPropagator::undo() is also called for levels L with an
	 *       empty change list if AbstractPropagator::check() has been called on L.
	 */
	void enableClingoPropagatorUndo(UndoType undoMode);

	void enableHistory(bool b);

	//! Adds a watch for lit to all solvers and returns encodeLit(lit).
	Potassco::Lit_t addWatch(Literal lit);
	//! Removes the watch for lit from all solvers.
	void            removeWatch(Literal lit);
	//! Adds a watch for lit to the solver with the given id and returns encodeLit(lit).
	Potassco::Lit_t addWatch(uint32 solverId, Literal lit);
	//! Removes the watch for lit from solver with the given id.
	void            removeWatch(uint32 solverId, Literal lit);
	//! Freezes the given literal making it exempt from Sat-preprocessing.
	/*!
	 * \note Watched literals are automatically frozen.
	 */
	void            freezeLit(Literal lit);

	//! Returns the propagator that was given on construction.
	Potassco::AbstractPropagator* propagator() const { return prop_; }
	ClingoPropagatorLock*         lock()       const { return lock_; }
	CheckType                     checkMode()  const { return check_; }
	UndoType                      undoMode()   const { return undo_; }

	uint32 init(uint32 lastStep, Potassco::AbstractSolver& s);
private:
	typedef Potassco::Lit_t Lit_t;
	enum Action { RemoveWatch = 0, AddWatch = 1, FreezeLit = 2 };
	struct History;
	struct Change {
		Change(Lit_t p, Action a);
		Change(Lit_t p, Action a, uint32 sId);
		bool operator<(const Change& rhs) const;
		void apply(Potassco::AbstractSolver& s) const;
		uint64 solverMask() const;
		Lit_t lit;
		int16 sId;
		int16 action;
	};
	typedef PodVector<Change>::type ChangeList;
	ClingoPropagatorInit(const ClingoPropagatorInit&);
	ClingoPropagatorInit& operator=(const ClingoPropagatorInit&);
	Potassco::AbstractPropagator* prop_;
	ClingoPropagatorLock* lock_;
	History*   history_;
	ChangeList changes_;
	uint32     step_;
	CheckType  check_;
	UndoType   undo_;
};

//! Adaptor for a Potassco::AbstractPropagator.
/*!
 * The class adapts a given Potassco::AbstractPropagator so that
 * it is usable as a PostPropagator within libclasp.
 * \note This class is meant to be a final class.
 */
class ClingoPropagator : public Clasp::PostPropagator {
public:
	typedef Potassco::AbstractPropagator::ChangeList ChangeList;
	typedef Clasp::PostPropagator::PropResult PPair;

	explicit ClingoPropagator(ClingoPropagatorInit* init);

	// PostPropagator
	virtual uint32 priority() const;
	virtual bool   init(Solver& s);
	virtual bool   propagateFixpoint(Clasp::Solver& s, Clasp::PostPropagator* ctx);
	virtual PPair  propagate(Solver&, Literal, uint32&);
	virtual bool   isModel(Solver& s);
	virtual void   reason(Solver&, Literal, LitVec&);
	virtual void   undoLevel(Solver& s);
	virtual bool   simplify(Solver& s, bool reinit);
	virtual void   destroy(Solver* s, bool detach);
private:
	typedef LitVec::size_type size_t;
	typedef Potassco::Lit_t Lit_t;
	class Control;
	enum State { state_ctrl = 1u, state_prop = 2u, state_init = 4u };
	struct ClauseTodo {
		bool empty() const { return mem.empty(); }
		void clear()       { mem.clear(); }
		LitVec    mem;
		ClauseRep clause;
		uint32    flags;
	};
	typedef PodVector<Lit_t>::type       AspifVec;
	typedef PodVector<Constraint*>::type ClauseDB;
	typedef ClingoPropagatorInit         Propagator;
	typedef ClingoPropagatorLock*        ClingoLock;
	bool addClause(Solver& s, uint32 state);
	void toClause(Solver& s, const Potassco::LitSpan& clause, Potassco::Clause_t prop);
	void registerUndoCheck(Solver& s);
	void registerUndo(Solver& s, uint32 data);
	bool inTrail(Literal p) const;
	Propagator* call_;  // wrapped theory propagator
	AspifVec    trail_; // assignment trail: watched literals that are true
	AspifVec    temp_;  // temporary buffer used to pass changes to user
	VarVec      undo_;  // offsets into trail marking beginnings of decision levels
	ClauseDB    db_;    // clauses added with flag static
	ClauseTodo  todo_;  // active clause to be added (received from theory propagator)
	size_t      prop_;  // offset into trail: literals [0, prop_) were propagated
	size_t      epoch_; // number of calls into callback
	uint32      level_; // highest undo level
	uint32      propL_; // decision level on handling propagate() from theory propagator
	int32       front_; // global assignment position for fixpoint checks
	uint32      init_;  // last time init() was called
	Literal     aux_;   // max active literal
};

class ClingoAssignment : public Potassco::AbstractAssignment {
public:
	typedef Potassco::AbstractAssignment BaseType;
	typedef BaseType::Value_t Value_t;
	typedef BaseType::Lit_t   Lit_t;

	ClingoAssignment(const Solver& s);

	virtual uint32_t size()               const;
	virtual uint32_t unassigned()         const;
	virtual bool     hasConflict()        const;
	virtual uint32_t level()              const;
	virtual uint32_t rootLevel()          const;
	virtual bool     hasLit(Lit_t lit)    const;
	virtual Value_t  value(Lit_t lit)     const;
	virtual uint32_t level(Lit_t lit)     const;
	virtual Lit_t    decision(uint32_t)   const;
	virtual bool     isTotal()            const;
	virtual uint32_t trailSize()          const;
	virtual Lit_t    trailAt(uint32_t)    const;
	virtual uint32_t trailBegin(uint32_t) const;

	const Solver& solver() const { return *solver_;  }
private:
	const Solver* solver_;
};

class ClingoHeuristic : public DecisionHeuristic {
public:
	class Factory : public BasicSatConfig::HeuristicCreator {
	public:
		/*!
		 * \param clingoHeuristic The heuristic that should be added to solvers.
		 * \param lock An optional lock that should be applied during calls to AbstractHeuristic::decide().
		 */
		explicit Factory(Potassco::AbstractHeuristic& clingoHeuristic, ClingoPropagatorLock* lock = 0);
		DecisionHeuristic* create(Heuristic_t::Type t, const HeuParams& p);
	private:
		Potassco::AbstractHeuristic* clingo_;
		ClingoPropagatorLock*        lock_;
	};

	explicit ClingoHeuristic(Potassco::AbstractHeuristic& clingoHeuristic, DecisionHeuristic* claspHeuristic, ClingoPropagatorLock* lock);
	virtual void startInit(const Solver& s);
	virtual void endInit(Solver& s);
	virtual void detach(Solver& s);
	virtual void setConfig(const HeuParams& p);
	virtual void updateVar(const Solver& s, Var v, uint32 n);
	virtual void simplify(const Solver& s, LitVec::size_type st);
	virtual void undoUntil(const Solver& s, LitVec::size_type st);
	virtual void newConstraint(const Solver& s, const Literal* first, LitVec::size_type size, ConstraintType t);
	virtual void updateReason(const Solver& s, const LitVec& lits, Literal resolveLit);
	virtual bool bump(const Solver& s, const WeightLitVec& lits, double adj);
	virtual Literal doSelect(Solver& s);
	virtual Literal selectRange(Solver& s, const Literal* first, const Literal* last);

	DecisionHeuristic* fallback() const;
private:
	typedef SingleOwnerPtr<DecisionHeuristic> HeuPtr;
	Potassco::AbstractHeuristic* clingo_;
	HeuPtr                       clasp_;
	ClingoPropagatorLock*        lock_;
};

///@}
}
#endif
