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
#ifndef CLASP_SOLVER_H_INCLUDED
#define CLASP_SOLVER_H_INCLUDED
#ifdef _MSC_VER
#pragma once
#endif

#include <clasp/solver_types.h>
#include <clasp/solver_strategies.h>
#include <clasp/shared_context.h>

namespace Clasp {

/**
 * \file
 * \defgroup solver Solver
 * \brief %Solver and related classes.
 */
//@{

//! clasp's Solver class.
/*!
 * A Solver-object maintains the state and provides the functions
 * necessary to implement our CDNL-algorithm.
 *
 * The interface supports incremental solving (search under assumptions) as well as
 * solution enumeration. To make this possible the solver maintains two special
 * decision levels: the root-level and the backtrack-level.
 *
 * The root-level is the lowest decision level to which a search
 * can return. Conflicts on the root-level are non-resolvable and end a search - the
 * root-level therefore acts as an artificial top-level during search.
 * Incremental solving is then implemented by first adding a list of unit assumptions
 * and second initializing the root-level to the current decision level.
 * Once search terminates assumptions can be undone by calling clearAssumptions
 * and a new a search can be started using different assumptions.
 *
 * For model enumeration the solver maintains a backtrack-level that restricts
 * backjumping in order to prevent repeating already enumerated solutions.
 * The solver will never backjump above that level and conflicts on the backtrack-level
 * are resolved by backtracking, i.e. flipping the corresponding decision literal.
 *
 * \see "Conflict-Driven Answer Set Enumeration" for a detailed description of this approach.
 *
 */
class Solver {
public:
	typedef PodVector<Constraint*>::type      ConstraintDB;
	typedef const ConstraintDB&               DBRef;
	typedef SingleOwnerPtr<DecisionHeuristic> HeuristicPtr;
private:
	friend class SharedContext;
	// Creates an empty solver object with all strategies set to their default value.
	Solver(SharedContext* ctx, uint32 id);
	~Solver();
	// Resets a solver object to the state it had after construction.
	void reset();
	void resetConfig();
	void startInit(uint32 constraintGuess, const SolverParams& params);
	void updateVars();
	bool cloneDB(const ConstraintDB& db);
	bool preparePost();
	bool endInit();
	bool endStep(uint32 top, const SolverParams& params);
public:
	typedef SolverStrategies::SearchStrategy SearchMode;
	typedef SolverStrategies::UpdateMode     UpdateMode;
	typedef SolverStrategies::WatchInit      WatchInitMode;
	//! Returns a pointer to the shared context object of this solver.
	const SharedContext*    sharedContext() const { return shared_; }
	//! Returns a pointer to the sat-preprocessor used by this solver.
	SatPreprocessor*        satPrepro()     const;
	//! Returns the solver's solve parameters.
	const SolveParams&      searchConfig()  const;
	SearchMode              searchMode()    const { return static_cast<SearchMode>(strategy_.search); }
	UpdateMode              updateMode()    const { return static_cast<UpdateMode>(strategy_.upMode); }
	WatchInitMode           watchInitMode() const { return static_cast<WatchInitMode>(strategy_.initWatches); }
	uint32                  compressLimit() const { return strategy_.compress ? strategy_.compress : UINT32_MAX; }
	bool                    restartOnModel()const { return strategy_.restartOnModel; }
	DecisionHeuristic*      heuristic()     const { return heuristic_.get();}
	uint32                  id()            const { return strategy_.id; }
	VarInfo                 varInfo(Var v)  const { return shared_->validVar(v) ? shared_->varInfo(v) : VarInfo(); }
	const OutputTable&      outputTable()   const { return shared_->output; }
	Literal                 tagLiteral()    const { return tag_; }
	bool                    isMaster()      const { return this == sharedContext()->master(); }
	/*!
	 * \name Setup functions
	 * Functions in this group are typically used before a search is started.
	 * @{ */
	//! Adds the problem constraint c to the solver.
	/*!
	 * Problem constraints shall only be added to the master solver of
	 * a SharedContext object and only during the setup phase.
	 * \pre this == sharedContext()->master() && !sharedContext()->frozen().
	 */
	void                    add(Constraint* c);
	//! Adds a suitable representation of the given clause to the solver.
	/*!
	 * Depending on the type and size of the given clause, the function
	 * either adds a (learnt) constraint to this solver or an implication
	 * to the shared implication graph.
	 * \note If c is a problem clause, the precondition of add(Constraint* c) applies.
	 */
	bool                    add(const ClauseRep& c, bool isNew = true);
	//! Returns whether c can be stored in the shared short implication graph.
	bool                    allowImplicit(const ClauseRep& c) const {
		return c.isImp()
		  ? shared_->allowImplicit(c.info.type()) && !c.info.aux() && (c.prep == 1 || (!auxVar(c.lits[0].var()) && !auxVar(c.lits[1].var()) && (c.size == 2 || !auxVar(c.lits[2].var()))))
		  : c.size <= 1;
	}
	//! Returns the post propagator with the given priority or 0 if no such post propagator exists.
	PostPropagator*         getPost(uint32 prio) const;
	//! Adds p as post propagator to this solver.
	/*!
	 * \pre p was not added previously and is not part of any other solver.
	 * \note Post propagators are stored and called in priority order.
	 * \see PostPropagator::priority()
	 */
	bool                    addPost(PostPropagator* p);
	//! Removes p from the solver's list of post propagators.
	/*!
	 * \note The function shall not be called during propagation of any other post propagator.
	 */
	void                    removePost(PostPropagator* p);

	//! Adds path to the current root-path and adjusts the root-level accordingly.
	bool pushRoot(const LitVec& path, bool pushStep = false);
	bool pushRoot(Literal p);
	void setEnumerationConstraint(Constraint* c);
	//! Requests a special aux variable for tagging conditional knowledge.
	/*!
	 * Once a tag variable t is set, learnt clauses containing ~t are
	 * tagged as "conditional". Conditional clauses are removed once t becomes
	 * unassigned or Solver::removeConditional() is called. Furthermore, calling
	 * Solver::strengthenConditional() removes ~t from conditional clauses and
	 * transforms them to unconditional knowledge.
	 *
	 * \note Typically, the tag variable is a root assumption and hence true during
	 *       the whole search.
	 */
	Var  pushTagVar(bool pushToRoot);
	//@}

	/*!
	 * \name CDNL functions
	 * Top level functions that are important to the CDNL algorithm.
	 * @{ */

	//! Searches for a model as long as the given limit is not reached.
	/*!
	 * The function searches for a model as long as none of the limits given by limit
	 * is reached. The limits are updated during search.
	 *
	 * \param limit Imposed limit on conflicts and number of learnt constraints.
	 * \param randf Pick next decision variable randomly with a probability of randf.
	 * \return
	 *  - value_true: if a model was found.
	 *  - value_false: if the problem is unsatisfiable (under assumptions, if any).
	 *  - value_free: if search was stopped because limit was reached.
	 *  .
	 *
	 * \note search treats the root level as top-level, i.e. it will never backtrack below that level.
	 */
	ValueRep search(SearchLimits& limit, double randf = 0.0);
	ValueRep search(uint64 maxC, uint32 maxL, bool local = false, double rp  = 0.0);

	//! Moves the root-level i levels down (i.e. away from the top-level).
	/*!
	 * The root-level is similar to the top-level in that it cannot be
	 * undone during search, i.e. the solver will not resolve conflicts that are on or
	 * above the root-level.
	 */
	void pushRootLevel(uint32 i = 1) {
		levels_.root = std::min(decisionLevel(), levels_.root+i);
		levels_.flip = std::max(levels_.flip, levels_.root);
	}

	//! Moves the root-level i levels up (i.e. towards the top-level).
	/*!
	 * The function removes all levels between the new root level and the current decision level,
	 * resets the current backtrack-level, and re-assigns any implied literals.
	 * \param      i      Number of root decisions to pop.
	 * \param[out] popped Optional storage for popped root decisions.
	 * \param      aux    Whether or not aux variables should be added to popped.
	 * \post decisionLevel() == rootLevel()
	 * \note The function first calls clearStopConflict() to remove any stop conflicts.
	 * \note The function *does not* propagate any asserted literals. It is
	 *       the caller's responsibility to call propagate() after the function returned.
	 */
	bool popRootLevel(uint32 i = 1, LitVec* popped = 0, bool aux = true);

	//! Removes a previously set stop conflict and restores the root level.
	void clearStopConflict();

	//! Returns the current root level.
	uint32 rootLevel() const { return levels_.root; }

	//! Removes any implications made between the top-level and the root-level.
	/*!
	 * The function also resets the current backtrack-level and re-assigns learnt facts.
	 * \note
	 *   Equivalent to popRootLevel(rootLevel()) followed by simplify().
	 */
	bool clearAssumptions();

	//! Adds c as a learnt constraint to the solver.
	void addLearnt(Constraint* c, uint32 size, ConstraintType type) {
		learnts_.push_back(c);
		stats.addLearnt(size, type);
	}
	void addLearnt(Constraint* c, uint32 size) { addLearnt(c, size, c->type()); }
	//! Tries to receive at most maxOut clauses.
	/*!
	 * The function queries the distributor object for new clauses to be delivered to
	 * this solver. Clauses are stored in out.
	 * \return The number of clauses received.
	 */
	uint32          receive(SharedLiterals** out, uint32 maxOut) const;
	//! Distributes the clause in lits via the distributor.
	/*!
	 * The function first calls the distribution strategy
	 * to decides whether the clause is a valid candidate for distribution.
	 * If so and a distributor was set, it distributes the clause and returns a handle to the
	 * now shared literals of the clause. Otherwise, it returns 0.
	 *
	 * \param lits  The literals of the clause.
	 * \param size  The number of literals in the clause.
	 * \param extra Additional information about the clause.
	 * \note
	 *   If the return value is not null, it is the caller's
	 *   responsibility to release the returned handle (i.e. by calling release()).
	 * \note If the clause contains aux vars, it is not distributed.
	 */
	SharedLiterals* distribute(const Literal* lits, uint32 size, const ConstraintInfo& extra);


	//! Returns to the maximum of rootLevel() and backtrackLevel() and increases the number of restarts.
	void restart() {
		undoUntil(0);
		++stats.restarts;
		ccInfo_.score().bumpActivity();
	}
	enum UndoMode { undo_default = 0u, undo_pop_bt_level = 1u, undo_pop_proj_level = 2u, undo_save_phases = 4u };
	//! Sets the backtracking level to dl.
	/*!
	 * Depending on mode, the backtracking level either applies
	 * to normal or projective solution enumeration.
	 * \see "Solution Enumeration for Projected Boolean Search Problems".
	 */
	void setBacktrackLevel(uint32 dl, UndoMode mode = undo_pop_bt_level) {
		if (uint32(mode) >= levels_.mode) {
			levels_.flip = std::max(std::min(dl, decisionLevel()), rootLevel());
			levels_.mode = std::max(uint32(mode & 3u), uint32(undo_pop_bt_level));
		}
	}
	//! Returns the current backtracking level.
	uint32 backtrackLevel() const { return levels_.flip; }
	//! Returns the backjump level during an undo operation.
	uint32 jumpLevel()      const { return decisionLevel() - levels_.jump; }

	//! Returns whether the solver can split-off work.
	bool splittable() const;

	//! Tries to split-off disjoint work from the solver's currrent guiding path and returns it in out.
	/*!
	 * \return splittable()
	 */
	bool split(LitVec& out);

	//! Copies the solver's currrent guiding path to gp.
	/*!
	 * \note The solver's guiding path consists of:
	 *   - the decisions from levels [1, rootLevel()]
	 *   - any literals that are implied on a level <= rootLevel() because of newly learnt
	 *     information. This particularly includes literals that were flipped during model enumeration.
	 *
	 * \param[out] out Where to store the guiding path.
	 */
	void copyGuidingPath(LitVec& out);

	//! If called on top-level, removes SAT-clauses + Constraints for which Constraint::simplify returned true.
	/*!
	 * \note If this method is called on a decision-level > 0, it is a noop and will
	 * simply return true.
	 * \return false, if a top-level conflict is detected. Otherwise, true.
	 */
	bool simplify();
	//! Shuffle constraints upon next simplification.
	void shuffleOnNextSimplify(){ shufSimp_ = 1; }


	//! Removes all conditional knowledge, i.e. all previously tagged learnt clauses.
	/*!
	 * \see Solver::pushTagVar()
	 */
	void removeConditional();

	//! Resolves all tagged clauses with the tag literal and thereby strengthens the learnt db.
	/*!
	 * \see Solver::pushTagVar()
	 */
	void strengthenConditional();

	//! Sets the literal p to true and schedules p for propagation.
	/*!
	 * Setting a literal p to true means assigning the appropriate value to
	 * p's variable. That is: value_false if p is a negative literal and value_true
	 * if p is a positive literal.
	 * \param p The literal that should become true.
	 * \param a The reason for the literal to become true or 0 if no reason exists.
	 *
	 * \return
	 *  - false if p is already false
	 *  - otherwise true.
	 *
	 * \pre hasConflict() == false
	 * \pre a.isNull() == false || decisionLevel() <= rootLevel() || searchMode() == no_learning
	 * \post
	 *  p.var() == trueValue(p) || p.var() == falseValue(p) && hasConflict() == true
	 *
	 * \note if setting p to true leads to a conflict, the nogood that caused the
	 * conflict can be requested using the conflict() function.
	 */
	bool force(const Literal& p, const Antecedent& a) {
		assert(!hasConflict() || isTrue(p));
		if (assign_.assign(p, decisionLevel(), a)) return true;
		setConflict(p, a, UINT32_MAX);
		return false;
	}
	/*!
	 * \overload bool Solver::force(const Literal&, const Antecedent&)
	 */
	bool force(const Literal& p, const Antecedent& a, uint32 data) {
		return data != UINT32_MAX
			? assign_.assign(p, decisionLevel(), a.constraint(), data) || (setConflict(p, a, data), false)
			: force(p, a);
	}

	//! Assigns p at dl because of r.
	/*!
	 * \pre dl <= decisionLevel()
	 * \note
	 *   If dl < ul = max(rootLevel(), backtrackLevel()), p is actually assigned
	 *   at ul but the solver stores enough information to reassign
	 *   p on backtracking.
	 */
	bool force(Literal p, uint32 dl, const Antecedent& r, uint32 d = UINT32_MAX) {
		return dl == decisionLevel() ? force(p, r, d) : force(ImpliedLiteral(p, dl, r, d));
	}
	//! Assigns p as a fact at decision level 0.
	bool force(Literal p) { return force(p, 0, Antecedent(lit_true())); }

	//! Assumes the literal p if possible.
	/*!
	 * If p is currently unassigned, sets p to true and starts a new decision level.
	 * \pre validVar(p.var()) == true
	 * \param p The literal to assume.
	 * \return !isFalse(p)
	 */
	bool assume(const Literal& p);

	//! Selects and assumes the next branching literal by calling the installed decision heuristic.
	/*!
	 * \pre queueSize() == 0
	 * \note The next decision literal will be selected randomly with probability f.
	 * \return
	 *  - true if the assignment is not total and a literal was assumed (or forced).
	 *  - false otherwise
	 *  .
	 * \see DecisionHeuristic
	 */
	bool decideNextBranch(double f = 0.0);

	//! Sets a conflict that forces the solver to terminate its search.
	/*!
	 * \pre  !hasConflict()
	 * \post hasConflict()
	 *
	 * \note
	 *   To prevent the solver from resolving the stop conflict, the
	 *   function sets the root level to the current decision level.
	 *   Call clearStopConflict() to remove the conflict and to restore
	 *   the previous root-level.
	 */
	void setStopConflict();

	/*!
	 * Propagates all enqueued literals. If a conflict arises during propagation
	 * propagate returns false and the current conflict (as a set of literals)
	 * is stored in the solver's conflict variable.
	 * \pre !hasConflict()
	 * \see Solver::force
	 * \see Solver::assume
	 * \note Shall not be called recursively.
	 */
	bool propagate();

	/*!
	 * Does unit propagation and calls x->propagateFixpoint(*this)
	 * for all post propagators x up to but not including p.
	 * \note The function is meant to be called only in the context of p.
	 * \pre  p is a post propagator of this solver, i.e. was previously added via addPost().
	 * \pre  Post propagators are active, i.e. the solver is fully initialized.
	 */
	bool propagateUntil(PostPropagator* p);

	/*!
	 * Calls x->propagateFixpoint(*this) for all post propagators x starting from and including p.
	 * \note The function is meant to be called only in the context of p.
	 * \pre  p is a post propagator of this solver, i.e. was previously added via addPost().
	 * \pre  Post propagators are active, i.e. the solver is fully initialized.
	 * \pre  Assignment is fully (unit) propagated up to p.
	 */
	bool propagateFrom(PostPropagator* p);

	//! Executes a one-step lookahead on p.
	/*!
	 * Assumes p and propagates this assumption. If propagations leads to
	 * a conflict, false is returned. Otherwise the assumption is undone and
	 * the function returns true.
	 * \param p The literal to test.
	 * \param c The constraint that wants to test p (can be 0).
	 * \pre p is free
	 * \note If c is not null and testing p does not lead to a conflict,
	 *       c->undoLevel() is called *before* p is undone. Hence, the
	 *       range [s.levelStart(s.decisionLevel()), s.assignment().size())
	 *       contains p followed by all literals that were forced because of p.
	 * \note propagateUntil(c) is used to propagate p.
	 */
	bool test(Literal p, PostPropagator* c);

	//! Estimates the number of assignments following from setting p to true.
	/*!
	 * \note For the estimate only binary clauses are considered.
	 */
	uint32 estimateBCP(const Literal& p, int maxRecursionDepth = 5) const;

	//! Computes the number of in-edges for each assigned literal.
	/*!
	 * \pre  !hasConflict()
	 * \note For a literal p assigned on level level(p), only in-edges from
	 *       levels < level(p) are counted.
	 * \return The maximum number of in-edges.
	 */
	uint32 inDegree(WeightLitVec& out);

	struct DBInfo { uint32 size; uint32 locked; uint32 pinned; };
	//! Removes upto remMax percent of the learnt nogoods.
	/*!
	 * \param remMax  Fraction of nogoods to remove ([0.0,1.0]).
	 * \param rs      Strategy to apply during nogood deletion.
	 * \return        The number of locked and active/glue clauses currently exempt from deletion.
	 * \note
	 *   Nogoods that are the reason for a literal to be in the assignment
	 *   are said to be locked and won't be removed.
	 */
	DBInfo reduceLearnts(float remMax, const ReduceStrategy& rs = ReduceStrategy());

	//! Resolves the active conflict using the selected strategy.
	/*!
	 * If searchMode() is set to learning, resolveConflict implements
	 * First-UIP learning and backjumping. Otherwise, it simply applies
	 * chronological backtracking.
	 * \pre hasConflict()
	 * \return
	 *  - true if the conflict was successfully resolved
	 *  - false otherwise
	 * \note
	 *  If decisionLevel() == rootLevel() false is returned.
	 */
	bool resolveConflict();

	//! Backtracks the last decision and updates the backtrack-level if necessary.
	/*!
	 * \return
	 *  - true if backtracking was possible
	 *  - false if decisionLevel() == rootLevel()
	 */
	bool backtrack();

	//! Undoes all assignments up to (but not including) decision level dl.
	/*!
	 * \post decision level == max(min(decisionLevel(), dl), max(rootLevel(), backtrackLevel()))
	 * \return The decision level after undoing assignments.
	 * \note
	 *   undoUntil() stops at the current backtrack level unless undoMode includes the mode
	 *   that was used when setting the backtrack level.
	 * \note
	 *   If undoMode contains undo_save_phases, the functions saves the values of variables that are undone.
	 *   Otherwise, phases are only saved if indicated by the active strategy.
	 */
	uint32 undoUntil(uint32 dl, uint32 undoMode);
	//! Behaves like undoUntil(dl, undo_default).
	uint32 undoUntil(uint32 dl) { return undoUntilImpl(dl, false); }
	//! Returns whether undoUntil(decisionLevel()-1) is valid and would remove decisionLevel().
	bool   isUndoLevel() const;

	//! Adds a new auxiliary variable to this solver.
	/*!
	 * Auxiliary variables are local to one solver and are not considered
	 * as part of the problem. They shall be added/used only during solving, i.e.
	 * after problem setup is completed.
	 */
	Var  pushAuxVar();
	//! Pops the num most recently added auxiliary variables and destroys all constraints in auxCons.
	void popAuxVar(uint32 num = UINT32_MAX, ConstraintDB* auxCons = 0);
	//@}

	/*!
	 * \name State inspection
	 * Functions for inspecting the state of the solver & search.
	 * \note validVar(v) is a precondition for all functions that take a variable as
	 * parameter.
	 * @{ */
	//! Returns the number of problem variables.
	uint32   numProblemVars()       const { return shared_->numVars(); }
	//! Returns the number of active solver-local aux variables.
	uint32   numAuxVars()           const { return numVars() - numProblemVars(); }
	//! Returns the number of solver variables, i.e. numProblemVars() + numAuxVars()
	uint32   numVars()              const { return assign_.numVars() - 1; }
	//! Returns true if var represents a valid variable in this solver.
	bool     validVar(Var var)      const { return var <= numVars(); }
	//! Returns true if var is a solver-local aux var.
	bool     auxVar(Var var)        const { return shared_->numVars() < var; }
	//! Returns the number of assigned variables.
	uint32   numAssignedVars()      const { return assign_.assigned(); }
	//! Returns the number of free variables.
	/*!
	 * The number of free variables is the number of vars that are neither
	 * assigned nor eliminated.
	 */
	uint32   numFreeVars()          const { return assign_.free()-1; }
	//! Returns the value of v w.r.t the current assignment.
	ValueRep value(Var v)           const { assert(validVar(v)); return assign_.value(v); }
	//! Returns the value of v w.r.t the top level.
	ValueRep topValue(Var v)        const { return level(v) == 0 ? value(v) : value_free; }
	//! Returns the set of preferred values of v.
	ValueSet pref(Var v)            const { assert(validVar(v)); return assign_.pref(v);}
	//! Returns true if p is true w.r.t the current assignment.
	bool     isTrue(Literal p)      const { assert(validVar(p.var())); return assign_.value(p.var()) == trueValue(p); }
	//! Returns true if p is false w.r.t the current assignment.
	bool     isFalse(Literal p)     const { assert(validVar(p.var())); return assign_.value(p.var()) == falseValue(p); }
	//! Returns the literal of v being true in the current assignment.
	/*!
	 * \pre v is assigned a value in the current assignment
	 */
	Literal  trueLit(Var v)         const { assert(value(v) != value_free); return Literal(v, valSign(value(v))); }
	Literal  defaultLit(Var v)      const;
	//! Returns the decision level on which v was assigned.
	/*!
	 * \note The returned value is only meaningful if value(v) != value_free.
	 */
	uint32   level(Var v)           const { assert(validVar(v)); return assign_.level(v); }
	//! Returns true if v is currently marked as seen.
	/*!
	 * Note: variables assigned on level 0 are always marked.
	 */
	bool     seen(Var v)            const { assert(validVar(v)); return assign_.seen(v, 3u); }
	//! Returns true if the literal p is currently marked as seen.
	bool     seen(Literal p)        const { assert(validVar(p.var())); return assign_.seen(p.var(), uint8(1+p.sign())); }
	//! Returns the current decision level.
	uint32   decisionLevel()        const { return (uint32)levels_.size(); }
	bool     validLevel(uint32 dl)  const { return dl != 0 && dl <= decisionLevel(); }
	//! Returns the starting position of decision level dl in the trail.
	/*!
	 * \pre validLevel(dl)
	 */
	uint32   levelStart(uint32 dl)  const { assert(validLevel(dl)); return levels_[dl-1].trailPos; }
	//! Returns the decision literal of the decision level dl.
	/*!
	 * \pre validLevel(dl)
	 */
	Literal  decision(uint32 dl)    const { assert(validLevel(dl)); return assign_.trail[ levels_[dl-1].trailPos ]; }
	//! Returns true, if the current assignment is conflicting.
	bool     hasConflict()          const { return !conflict_.empty(); }
	bool     hasStopConflict()      const { return hasConflict() && conflict_[0] == lit_false(); }
	//! Returns the number of (unprocessed) literals in the propagation queue.
	uint32   queueSize()            const { return (uint32) assign_.qSize(); }
	//! Number of problem constraints in this solver.
	uint32   numConstraints()       const;
	//! Returns the number of constraints that are currently in the solver's learnt database.
	uint32   numLearntConstraints() const { return (uint32)learnts_.size(); }
	//! Returns the reason for p being true.
	/*!
	 * \pre p is true w.r.t the current assignment
	 */
	const Antecedent& reason(Literal p)              const { assert(isTrue(p)); return assign_.reason(p.var()); }
	//! Returns the additional reason data associated with p.
	uint32            reasonData(Literal p)          const { return assign_.data(p.var()); }
	//! Returns the current (partial) assignment as a set of true literals.
	/*!
	 * \note Although the special var 0 always has a value it is not considered to be
	 * part of the assignment.
	 */
	const LitVec&     trail()                        const { return assign_.trail; }
	const Assignment& assignment()                   const { return assign_; }
	//! Returns the current conflict as a set of literals.
	const LitVec&     conflict()                     const { return conflict_; }
	//! Returns the most recently derived conflict clause.
	const LitVec&     conflictClause()               const { return cc_; }
	//! Returns the set of eliminated literals that are unconstraint w.r.t the last model.
	const LitVec&     symmetric()                    const { return temp_; }
	//! Returns the enumeration constraint set by the enumerator used.
	Constraint*       enumerationConstraint()        const { return enum_; }
	DBRef             constraints()                  const { return constraints_; }
	//! Returns the idx'th learnt constraint.
	/*!
	 * \pre idx < numLearntConstraints()
	 */
	Constraint& getLearnt(uint32 idx) const {
		assert(idx < numLearntConstraints());
		return *learnts_[ idx ];
	}

	mutable RNG rng;   //!< Random number generator for this object.
	ValueVec    model; //!< Stores the last model (if any).
	LowerBound  lower; //!< Stores the last lower bound found (if any).
	SolverStats stats; //!< Stores statistics about the solving process.
	//@}

	/*!
	 * \name Watch management
	 * Functions for setting/removing watches.
	 * \pre validVar(v)
	 * @{ */
	//! Returns the number of constraints watching the literal p.
	uint32        numWatches(Literal p)              const;
	//! Returns true if the constraint c watches the literal p.
	bool          hasWatch(Literal p, Constraint* c) const;
	bool          hasWatch(Literal p, ClauseHead* c) const;
	//! Returns c's watch-structure associated with p.
	/*!
	 * \note returns 0, if hasWatch(p, c) == false
	 */
	GenericWatch* getWatch(Literal p, Constraint* c) const;
	//! Adds c to the watch-list of p.
	/*!
	 * When p becomes true, c->propagate(p, data, *this) is called.
	 * \post hasWatch(p, c) == true
	 */
	void addWatch(Literal p, Constraint* c, uint32 data = 0) {
		assert(validWatch(p));
		watches_[p.id()].push_right(GenericWatch(c, data));
	}
	//! Adds w to the clause watch-list of p.
	void addWatch(Literal p, const ClauseWatch& w) {
		assert(validWatch(p));
		watches_[p.id()].push_left(w);
	}
	//! Removes c from p's watch-list.
	/*!
	 * \post hasWatch(p, c) == false
	 */
	void removeWatch(const Literal& p, Constraint* c);
	void removeWatch(const Literal& p, ClauseHead* c);
	//! Adds c to the watch-list of decision-level dl.
	/*!
	 * Constraints in the watch-list of a decision level are
	 * notified when that decision level is about to be backtracked.
	 * \pre validLevel(dl)
	 */
	void addUndoWatch(uint32 dl, Constraint* c) {
		assert(validLevel(dl));
		if (levels_[dl-1].undo != 0) {
			levels_[dl-1].undo->push_back(c);
		}
		else {
			levels_[dl-1].undo = allocUndo(c);
		}
	}
	//! Removes c from the watch-list of the decision level dl.
	bool removeUndoWatch(uint32 dl, Constraint* c);
	//@}

	/*!
	 * \name Misc functions
	 * Low-level implementation functions. Use with care and only if you
	 * know what you are doing!
	 * @{ */
	bool addPost(PostPropagator* p, bool init);
	//! Updates the reason for p being true.
	/*!
	 * \pre p is true and x is a valid reason for p
	 */
	bool setReason(Literal p, const Antecedent& x, uint32 data = UINT32_MAX) {
		assert(isTrue(p) || shared_->eliminated(p.var()));
		assign_.setReason(p.var(), x);
		if (data != UINT32_MAX) { assign_.setData(p.var(), data); }
		return true;
	}
	void setPref(Var v, ValueSet::Value which, ValueRep to) {
		assert(validVar(v) && to <= value_false);
		assign_.requestPrefs();
		assign_.setPref(v, which, to);
	}
	void resetPrefs() { assign_.resetPrefs(); }
	void resetLearntActivities();
	//! Returns the reason for p being true as a set of literals.
	void reason(Literal p, LitVec& out) { assert(isTrue(p)); out.clear(); return assign_.reason(p.var()).reason(*this, p, out); }

	//! Helper function for updating antecedent scores during conflict resolution.
	/*!
	 * \param sc   The current score of the active antecedent.
	 * \param p    The literal implied by the active antecedent.
	 * \param lits The literals of the active antecedent.
	 * \return true if a score was updated.
	 *
	 * \note Depending on the active solver strategies, the function
	 *       increases the activity and/or updates the lbd of the given antecedent.
	 *
	 * \note If SolverStrategies::bumpVarAct is active, p's activity
	 *       is increased if the new lbd is smaller than the lbd of the
	 *       conflict clause that is currently being derived.
	 */
	bool updateOnReason(ConstraintScore& sc, Literal p, const LitVec& lits) {
		// update only during conflict resolution
		if (&lits != &conflict_) { return false; }
		sc.bumpActivity();
		uint32 up = strategy_.updateLbd;
		if (up != SolverStrategies::lbd_fixed && !lits.empty()) {
			uint32 lbd  = sc.lbd();
			uint32 inc  = uint32(up != SolverStrategies::lbd_updated_less);
			uint32 nLbd = countLevels(&lits[0], &lits[0] + lits.size(), lbd - inc);
			if ((nLbd + inc) < lbd) {
				sc.bumpLbd(nLbd + uint32(up == SolverStrategies::lbd_update_pseudo));
			}
		}
		if (strategy_.bumpVarAct && isTrue(p)) { bumpAct_.push_back(WeightLiteral(p, sc.lbd())); }
		return true;
	}

	//! Helper function for increasing antecedent activities during conflict clause minimization.
	bool updateOnMinimize(ConstraintScore& sc) {
		return !strategy_.ccMinKeepAct && (sc.bumpActivity(), true);
	}

	//! Helper function for antecedents to be called during conflict clause minimization.
	bool ccMinimize(Literal p, CCMinRecursive* rec) const {
		return seen(p.var()) || (rec && hasLevel(level(p.var())) && ccMinRecurse(*rec, p));
	}

	//! Allocates a small block (32-bytes) from the solver's small block pool.
	void* allocSmall()       { return smallAlloc_.allocate(); }
	//! Frees a small block previously allocated from the solver's small block pool.
	void  freeSmall(void* m) { smallAlloc_.free(m);    }

	void  addLearntBytes(uint32 bytes)  { memUse_ += bytes; }
	void  freeLearntBytes(uint64 bytes) { memUse_ -= (bytes < memUse_) ? bytes : memUse_; }

	bool  restartReached(const SearchLimits& limit) const;
	bool  reduceReached(const SearchLimits& limit)  const;

	//! simplifies cc and returns finalizeConflictClause(cc, info);
	uint32 simplifyConflictClause(LitVec& cc, ConstraintInfo& info, ClauseHead* rhs);
	uint32 finalizeConflictClause(LitVec& cc, ConstraintInfo& info, uint32 ccRepMode = 0);
	uint32 countLevels(const Literal* first, const Literal* last, uint32 maxLevels = Clasp::LBD_MAX);
	bool hasLevel(uint32 dl)    const { assert(validLevel(dl)); return levels_[dl-1].marked != 0; }
	bool frozenLevel(uint32 dl) const { assert(validLevel(dl)); return levels_[dl-1].freeze != 0; }
	bool inputVar(Literal p)    const { return varInfo(p.var()).input(); }
	void markLevel(uint32 dl)    { assert(validLevel(dl)); levels_[dl-1].marked = 1; }
	void freezeLevel(uint32 dl)  { assert(validLevel(dl)); levels_[dl-1].freeze = 1; }
	void unmarkLevel(uint32 dl)  { assert(validLevel(dl)); levels_[dl-1].marked = 0; }
	void unfreezeLevel(uint32 dl){ assert(validLevel(dl)); levels_[dl-1].freeze = 0; }
	void markSeen(Var v)         { assert(validVar(v)); assign_.setSeen(v, 3u); }
	void markSeen(Literal p)     { assert(validVar(p.var())); assign_.setSeen(p.var(), uint8(1+p.sign())); }
	void clearSeen(Var v)        { assert(validVar(v)); assign_.clearSeen(v);  }
	void setHeuristic(DecisionHeuristic* h, Ownership_t::Type own);
	void destroyDB(ConstraintDB& db);
	SolverStrategies&  strategies() { return strategy_; }
	bool resolveToFlagged(const LitVec& conflictClause, uint8 vflag, LitVec& out, uint32& lbd) const;
	void resolveToCore(LitVec& out);
	void acquireProblemVar(Var var);
	void acquireProblemVars() { acquireProblemVar(numProblemVars());  }
	//@}
private:
	struct DLevel {
		explicit DLevel(uint32 pos = 0, ConstraintDB* u = 0)
			: trailPos(pos)
			, marked(0)
			, freeze(0)
			, undo(u) {}
		uint32        trailPos : 30;
		uint32        marked   :  1;
		uint32        freeze   :  1;
		ConstraintDB* undo;
	};
	struct DecisionLevels : public PodVector<DLevel>::type {
		DecisionLevels() : root(0), flip(0), mode(0), jump(0) {}
		uint32 root;      // root level
		uint32 flip : 30; // backtrack level
		uint32 mode :  2; // type of backtrack level
		uint32 jump;      // length of active undo
	};
	typedef PodVector<Antecedent>::type ReasonVec;
	typedef PodVector<WatchList>::type  Watches;
	struct  Dirty;
	struct CmpScore {
		typedef std::pair<uint32, ConstraintScore> ViewPair;
		CmpScore(const ConstraintDB& learnts, ReduceStrategy::Score sc, uint32 g, uint32 f = 0) : db(learnts), rs(sc), glue(g), freeze(f) {}
		uint32 score(const ConstraintScore& act)                      const { return ReduceStrategy::asScore(rs, act); }
		bool operator()(uint32 lhsId, uint32 rhsId)                   const { return (*this)(db[lhsId], db[rhsId]); }
		bool operator()(const ViewPair& lhs, const ViewPair& rhs)     const { return this->operator()(lhs.second, rhs.second); }
		bool operator()(ConstraintScore lhs, ConstraintScore rhs)     const { return ReduceStrategy::compare(rs, lhs, rhs) < 0;}
		bool operator()(const Constraint* lhs, const Constraint* rhs) const { return this->operator()(lhs->activity(), rhs->activity()); }
		bool isFrozen(const ConstraintScore& a) const { return a.bumped() && a.lbd() <= freeze; }
		bool isGlue(const ConstraintScore& a)   const { return a.lbd() <= glue; }
		const ConstraintDB&   db;
		ReduceStrategy::Score rs;
		uint32                glue;
		uint32                freeze;
	private: CmpScore& operator=(const CmpScore&);
	};
	bool    validWatch(Literal p) const { return p.id() < (uint32)watches_.size(); }
	void    freeMem();
	void    resetHeuristic(Solver* detach, DecisionHeuristic* h = 0, Ownership_t::Type own = Ownership_t::Acquire);
	bool    simplifySAT();
	bool    unitPropagate();
	bool    postPropagate(PostPropagator** start, PostPropagator* stop);
	void    cancelPropagation();
	uint32  undoUntilImpl(uint32 dl, bool sp);
	void    undoLevel(bool sp);
	uint32  analyzeConflict();
	bool    isModel();
	bool    resolveToFlagged(const LitVec& conflictClause, uint8 vf, LitVec& out, uint32& lbd);
	void    otfs(Antecedent& lhs, const Antecedent& rhs, Literal p, bool final);
	ClauseHead* otfsRemove(ClauseHead* c, const LitVec* newC);
	uint32  ccMinimize(LitVec& cc, LitVec& removed, uint32 antes, CCMinRecursive* ccMin);
	void    ccMinRecurseInit(CCMinRecursive& ccMin);
	bool    ccMinRecurse(CCMinRecursive& ccMin, Literal p) const;
	bool    ccRemovable(Literal p, uint32 antes, CCMinRecursive* ccMin);
	Antecedent ccHasReverseArc(Literal p, uint32 maxLevel, uint32 maxNew);
	void    ccResolve(LitVec& cc, uint32 pos, const LitVec& reason);
	void    undoFree(ConstraintDB* x);
	void    setConflict(Literal p, const Antecedent& a, uint32 data);
	bool    force(const ImpliedLiteral& p);
	void    updateBranch(uint32 n);
	uint32  incEpoch(uint32 size, uint32 inc = 1);
	DBInfo  reduceLinear(uint32 maxR, const CmpScore& cmp);
	DBInfo  reduceSort(uint32 maxR, const CmpScore& cmp);
	DBInfo  reduceSortInPlace(uint32 maxR, const CmpScore& cmp, bool onlyPartialSort);
	Literal popVars(uint32 num, bool popLearnt, ConstraintDB* popAux);
	ConstraintDB* allocUndo(Constraint* c);
	SharedContext*    shared_;      // initialized by master thread - otherwise read-only!
	SolverStrategies  strategy_;    // strategies used by this object
	HeuristicPtr      heuristic_;   // active decision heuristic
	CCMinRecursive*   ccMin_;       // additional data for supporting recursive strengthen
	PostPropagator**  postHead_;    // head of post propagator list to propagate
	ConstraintDB*     undoHead_;    // free list of undo DBs
	Constraint*       enum_;        // enumeration constraint - set by enumerator
	uint64            memUse_;      // memory used by learnt constraints (estimate)
	Dirty*            lazyRem_;     // set of watch lists that contain invalid constraints
	SmallClauseAlloc  smallAlloc_;  // allocator object for small clauses
	Assignment        assign_;      // three-valued assignment.
	DecisionLevels    levels_;      // information (e.g. position in trail) on each decision level
	ConstraintDB      constraints_; // problem constraints
	ConstraintDB      learnts_;     // learnt constraints
	PropagatorList    post_;        // (possibly empty) list of post propagators
	Watches           watches_;     // for each literal p: list of constraints watching p
	LitVec            conflict_;    // conflict-literals for later analysis
	LitVec            cc_;          // temporary: conflict clause within analyzeConflict
	LitVec            temp_;        // temporary: redundant literals in simplifyConflictClause
	WeightLitVec      bumpAct_;     // temporary: lits from current dl whose activity might get an extra bump
	VarVec            epoch_;       // temporary vector for computing LBD
	VarVec            cflStamp_;    // temporary vector for computing number of conflicts in branch
	ImpliedList       impliedLits_; // lits that were asserted on current dl but are logically implied earlier
	ConstraintInfo    ccInfo_;      // temporary: information about conflict clause cc_
	Literal           tag_;         // aux literal for tagging learnt constraints
	uint32            dbIdx_;       // position of first new problem constraint in master db
	uint32            lastSimp_ :30;// number of top-level assignments on last call to simplify
	uint32            shufSimp_ : 1;// shuffle db on next simplify?
	uint32            initPost_ : 1;// initialize new post propagators?
};
inline bool isRevLit(const Solver& s, Literal p, uint32 maxL) {
	return s.isFalse(p) && (s.seen(p) || s.level(p.var()) < maxL);
}
//! Simplifies the constraints in db and removes those that are satisfied.
template <class C>
void simplifyDB(Solver& s, C& db, bool shuffle) {
	typename C::size_type j = 0;
	for (typename C::size_type i = j, end = db.size(); i != end; ++i) {
		Constraint* c = db[i];
		if (c->simplify(s, shuffle)){ c->destroy(&s, false); }
		else                        { db[j++] = c;  }
	}
	shrinkVecTo(db, j);
}
//! Destroys (and optionally detaches) all constraints in db.
void destroyDB(Solver::ConstraintDB& db, Solver* s, bool detach);
//! Returns the default decision literal of the given variable.
inline Literal Solver::defaultLit(Var v) const {
	switch(strategy_.signDef) {
		default: //
		case SolverStrategies::sign_atom: return Literal(v, !varInfo(v).has(VarInfo::Body));
		case SolverStrategies::sign_pos : return posLit(v);
		case SolverStrategies::sign_neg : return negLit(v);
		case SolverStrategies::sign_rnd : return Literal(v, rng.drand() < 0.5);
	}
}
//! Event type optionally emitted after a conflict.
struct NewConflictEvent : SolveEvent<NewConflictEvent> {
	NewConflictEvent(const Solver& s, const LitVec& c, const ConstraintInfo& i) : SolveEvent<NewConflictEvent>(s, verbosity_quiet), learnt(&c), info(i) {}
	const LitVec*  learnt; //!< Learnt conflict clause.
	ConstraintInfo info;   //!< Additional information associated with the conflict clause.
};
//@}

/**
 * \defgroup heuristic Decision Heuristics
 * \brief Decision heuristics and related classes.
 * \ingroup solver
 */
//@{
//! Base class for decision heuristics to be used in a Solver.
/*!
 * During search the decision heuristic is used whenever the DPLL-procedure must pick
 * a new variable to branch on. Each concrete decision heuristic can implement a
 * different algorithm for selecting the next decision variable.
 */
class DecisionHeuristic {
public:
	DecisionHeuristic() {}
	virtual ~DecisionHeuristic();
	/*!
	 * Called once after all problem variables are known to the solver.
	 * The default-implementation is a noop.
	 * \param s The solver in which this heuristic is used.
	 */
	virtual void startInit(const Solver& s) { (void)s; }

	/*!
	 * Called once after all problem constraints are known to the solver
	 * and the problem was simplified.
	 * The default-implementation is a noop.
	 * \param s The solver in which this heuristic is used.
	 */
	virtual void endInit(Solver& s) { (void)s; }

	//! Called once if s switches to a different heuristic.
	virtual void detach(Solver& s) { (void)s; }

	//! Called if configuration has changed.
	virtual void setConfig(const HeuParams& p) { (void)p; }

	/*!
	 * Called if the state of one or more variables changed.
	 * A state change is one of:
	 *   - A previously eliminated variable is resurrected.
	 *   - A new aux variable was added.
	 *   - An aux variable was removed.
	 *   .
	 * \param s Solver in which the state change occurred.
	 * \param v The first variable affected by the change.
	 * \param n The range of variables affected, i.e. [v, v+n).
	 * \note Use s.validVar(v) and s.auxVar(v) to determine the reason for the update.
	 */
	virtual void updateVar(const Solver& s, Var v, uint32 n) = 0;

	/*!
	 * Called on decision level 0. Variables that are assigned on this level
	 * may be removed from any decision heuristic.
	 * \note Whenever the solver returns to dl 0, simplify is only called again
	 * if the solver learnt new facts since the last call to simplify.
	 *
	 * \param s The solver that reached decision level 0.
	 * \param st The position in the trail of the first new learnt fact.
	 */
	virtual void simplify(const Solver& s, LitVec::size_type st) { (void)s; (void)st; }

	/*!
	 * Called whenever the solver backracks.
	 * Literals in the range [s.trail()[st], s.trail().size()) are subject to backtracking.
	 * The default-implementation is a noop.
	 * \param s The solver that is about to backtrack.
	 * \param st Position in the trail of the first literal that will be backtracked.
	 */
	virtual void undoUntil(const Solver& s, LitVec::size_type st) { (void)s; (void)st; }

	/*!
	 * Called whenever a new constraint is added to the solver s.
	 * The default-implementation is a noop.
	 * \param s The solver to which the constraint is added.
	 * \param first First literal of the new constraint.
	 * \param size Size of the new constraint.
	 * \param t Type of the new constraint.
	 * \note first points to an array of size size.
	 */
	virtual void newConstraint(const Solver& s, const Literal* first, LitVec::size_type size, ConstraintType t) { (void)s; (void)first; (void)size; (void)t; }

	/*!
	 * Called for each new reason-set that is traversed during conflict analysis.
	 * The default-implementation is a noop.
	 * \param s The solver in which the conflict is analyzed.
	 * \param lits The current reason-set under inspection.
	 * \param resolveLit The literal that is currently resolved.
	 * \note When a conflict is detected, the solver passes the conflicting literals
	 * in lits during the first call to updateReason. On that first call resolveLit
	 * is the sentinel-literal.
	 */
	virtual void updateReason(const Solver& s, const LitVec& lits, Literal resolveLit) { (void)s; (void)lits; (void)resolveLit; }

	//! Shall bump the activity of literals in lits by lits.second * adj.
	/*!
	 * The default-implementation is a noop and always returns false.
	 * \return true if heuristic supports activities, false otherwise.
	 */
	virtual bool bump(const Solver& s, const WeightLitVec& lits, double adj) { (void)s; (void)lits; (void)adj; return false; }

	/*!
	 * Called whenever the solver must pick a new variable to branch on.
	 * \param s The solver that needs a new decision variable.
	 * \return
	 *  - true  : if the decision heuristic assumed a literal
	 *  - false : if no decision could be made because assignment is total or there is a conflict
	 *  .
	 * \post
	 * If true is returned, the heuristic has asserted a literal.
	 */
	bool select(Solver& s) { return s.numFreeVars() != 0 && s.assume(doSelect(s)); }

	//! Implements the actual selection process.
	/*!
	 * \pre s.numFreeVars() > 0, i.e. there is at least one variable to branch on.
	 * \return
	 *  - a literal that is currently free or
	 *  - a sentinel literal. In that case, the heuristic shall have asserted a literal!
	 */
	virtual Literal doSelect(Solver& s) = 0;

	/*!
	 * Shall select one of the literals in the range [first, last).
	 * \param s     The solver that needs a new decision variable.
	 * \param first Pointer to first literal in range.
	 * \param last  Pointer to the end of the range.
	 * \pre [first, last) is not empty and all literals in the range are currently unassigned.
	 * \note The default implementation returns *first.
	 */
	virtual Literal selectRange(Solver& s, const Literal* first, const Literal* last) {
		(void)s; (void)last;
		return *first;
	}
	static Literal selectLiteral(Solver& s, Var v, int signScore) {
		ValueSet prefs = s.pref(v);
		if (signScore != 0 && !prefs.has(ValueSet::user_value | ValueSet::saved_value | ValueSet::pref_value)) {
			return Literal(v, signScore < 0);
		}
		else if (!prefs.empty()) {
			return Literal(v, prefs.sign());
		}
		return s.defaultLit(v);
	}
private:
	DecisionHeuristic(const DecisionHeuristic&);
	DecisionHeuristic& operator=(const DecisionHeuristic&);
};
//! Selects the first free literal w.r.t to the initial variable order.
class SelectFirst : public DecisionHeuristic {
public:
	void updateVar(const Solver&, Var, uint32) {}
protected:
	Literal doSelect(Solver& s);
};
//@}
}
#endif

