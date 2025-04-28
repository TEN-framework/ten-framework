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
#ifndef CLASP_MINIMIZE_CONSTRAINT_H_INCLUDED
#define CLASP_MINIMIZE_CONSTRAINT_H_INCLUDED
#ifdef _MSC_VER
#pragma once
#endif
/*!
* \file
* \brief Types and functions for implementing minimization constraints.
*/
#include <clasp/constraint.h>
#include <clasp/solver_strategies.h>
#include <cassert>

namespace Clasp {
class MinimizeConstraint;
class WeightConstraint;

//! Supported minimization modes.
/*!
 * \ingroup constraint
 * Defines the possible minimization modes used during solving.
 * If optimize is used, a valid candidate model is a solution that is
 * strictly smaller than all previous solutions. Otherwise,
 * solutions with costs no greater than a fixed bound are considered valid.
 */
struct MinimizeMode_t {
	enum Mode {
		ignore    = 0, //!< Ignore optimize statements during solving.
		optimize  = 1, //!< Optimize via a decreasing bound.
		enumerate = 2, //!< Enumerate models with cost less or equal to a fixed bound.
		enumOpt   = 3, //!< Enumerate models with cost equal to optimum.
	};
};
typedef MinimizeMode_t::Mode MinimizeMode;

//! A type holding data (possibly) shared between a set of minimize constraints.
/*!
 * \ingroup shared_con
 */
class SharedMinimizeData {
public:
	typedef SharedMinimizeData ThisType;
	//! A type to represent a weight at a certain level.
	/*!
	 * Objects of this type are used to create sparse vectors of weights. E.g.
	 * a weight vector (w1\@L1, w2\@L3, w3\@L5) is represented as \[\<L1,1,w1\>\<L3,1,w2\>\<L5,0,w3\>\],
	 * where each \<level, next, weight\>-tuple is an object of type LevelWeight.
	 */
	struct LevelWeight {
		LevelWeight(uint32 l, weight_t w) : level(l), next(0), weight(w) {}
		uint32   level : 31; //!< The level of this weight.
		uint32   next  :  1; //!< Does this weight belong to a sparse vector of weights?
		weight_t weight;     //!< The weight at this level.
	};
	//! A type for holding sparse vectors of level weights of a multi-level constraint.
	typedef PodVector<LevelWeight>::type WeightVec;
	typedef PodVector<weight_t>::type    PrioVec;
	explicit SharedMinimizeData(const SumVec& lhsAdjust, MinimizeMode m = MinimizeMode_t::optimize);
	//! Increases the reference count of this object.
	ThisType*      share()               { ++count_; return this; }
	//! Decreases the object's reference count and destroys it if reference count drops to 0.
	void           release()             { if (--count_ == 0) destroy(); }
	//! Number of minimize statements contained in this constraint.
	uint32         numRules()       const{ return static_cast<uint32>(adjust_.size()); }
	uint32         maxLevel()       const{ return numRules()-1; }
	static wsum_t  maxBound()            { return INT64_MAX; }
	//! Returns the active minimization mode.
	MinimizeMode   mode()           const{ return mode_; }
	//! Returns true if optimization is active.
	bool           optimize()       const{ return optGen_ ? checkNext() : mode_ != MinimizeMode_t::enumerate; }
	//! Returns the lower bound of level x.
	wsum_t         lower(uint32 x)  const;
	//! Returns the upper bound of level x.
	wsum_t         upper(uint32 x)  const{ return upper()[x]; }
	const wsum_t*  upper()          const{ return &(up_ + (gCount_ & 1u))->front(); }
	//! Returns the sum of level x in the most recent model.
	wsum_t         sum(uint32 x)    const{ return sum()[x]; }
	const wsum_t*  sum()            const{ return (mode_ != MinimizeMode_t::enumerate) ? upper() : &up_[1][0]; }
	//! Returns the adjustment for level x.
	wsum_t         adjust(uint32 x) const{ return adjust_[x]; }
	const wsum_t*  adjust()         const{ return &adjust_[0]; }
	//! Returns the current (ajusted and possibly tentative) optimum for level x.
	wsum_t         optimum(uint32 x)const;
	//! Returns the highest level of the literal with the given index i.
	uint32         level(uint32 i)  const{ return numRules() == 1 ? 0 : weights[lits[i].second].level; }
	//! Returns the most important weight of the literal with the given index i.
	weight_t       weight(uint32 i) const{ return numRules() == 1 ? lits[i].second : weights[lits[i].second].weight; }
	uint32         generation()     const{ return gCount_; }
	//! Returns whether minimization should search for solutions with current or next smaller upper bound.
	bool           checkNext()      const{ return mode() != MinimizeMode_t::enumerate && generation() != optGen_; }
	/*!
	 * \name interface for optimization
	 * If not otherwise specified, the following functions shall not be called concurrently.
	 */
	//@{

	//! Sets the enumeration mode and (optionally) an initial bound.
	/*!
	 * \note If m is MinimizeMode::enumerate, the caller should always
	 * set a bound. Otherwise, *all* solutions are considered valid.
	 */
	bool setMode(MinimizeMode m, const wsum_t* bound = 0, uint32 boundSize = 0);
	bool setMode(MinimizeMode m, const SumVec& bound) { return setMode(m, bound.empty() ? 0 : &bound[0], (uint32)bound.size()); }
	void resetBounds();

	//! Attaches a new minimize constraint to this data object.
	/*!
	 * \param s      Solver in which the new minimize constraint should apply.
	 * \param params Parameters to pass to the optimization strategy.
	 * \param addRef If true, the ref count of the shared object is increased.
	 *               Otherwise, the new minimize constraint inherits the reference to the shared object.
	 */
	MinimizeConstraint* attach(Solver& s, const OptParams& params, bool addRef = true);

	//! Makes opt the new (tentative) optimum.
	/*!
	 * \pre opt is a pointer to an array of size numRules()
	 */
	const SumVec* setOptimum(const wsum_t* opt);
	//! Marks the current tentative optimum as the final optimum.
	/*!
	 * \note Once a final optimum is set, further calls to setOptimum()
	 * are ignored until resetBounds() is called.
	 */
	void          markOptimal();
	//! Sets the lower bound of level lev to low.
	void          setLower(uint32 lev, wsum_t low);
	//! Sets the lower bound of level lev to the maximum of low and the existing value lower(lev).
	/*!
	 * \note This function is thread-safe, i.e., can be called safely from multiple threads.
	 */
	wsum_t        incLower(uint32 lev, wsum_t low);
	//@}

	/*!
	 * \name Arithmetic functions on weights.
	 */
	//@{
	//! Computes lhs += weight(lit).
	void add(wsum_t* lhs, const WeightLiteral& lit) const { if (weights.empty()) *lhs += lit.second; else add(lhs, &weights[lit.second]); }
	void add(wsum_t* lhs, const LevelWeight* w)     const { do { lhs[w->level] += w->weight; } while (w++->next); }
	//! Computes lhs -= weight(lit).
	void sub(wsum_t* lhs, const WeightLiteral& lit, uint32& aLev) const { if (weights.empty()) *lhs -= lit.second; else sub(lhs, &weights[lit.second], aLev); }
	void sub(wsum_t* lhs, const LevelWeight* w, uint32& aLev)     const;
	//! Returns (lhs + weight(lit)) > rhs
	bool imp(wsum_t* lhs, const WeightLiteral& lit, const wsum_t* rhs, uint32& lev) const {
		return weights.empty() ? (*lhs+lit.second) > *rhs : imp(lhs, &weights[lit.second],  rhs, lev);
	}
	bool imp(wsum_t* lhs, const LevelWeight* w, const wsum_t* rhs, uint32& lev) const;
	//! Returns the weight of lit at level lev.
	weight_t weight(const WeightLiteral& lit, uint32 lev) const {
		if (numRules() == 1) { return lit.second * (lev == 0); }
		const LevelWeight* w = &weights[lit.second];
		do { if (w->level == lev) return w->weight; } while (w++->next);
		return 0;
	}
	//@}
private:
	typedef Clasp::Atomic_t<uint32>::type CounterType;
	typedef Clasp::Atomic_t<wsum_t>::type LowerType;
	SumVec       adjust_;  // initial bound adjustments
	SumVec       up_[2];   // buffers for update via "double buffering"
	LowerType*   lower_;   // (unadjusted) lower bound of constraint
	MinimizeMode mode_;    // how to compare assignments?
	CounterType  count_;   // number of refs to this object
	CounterType  gCount_;  // generation count - used when updating optimum
	uint32       optGen_;  // generation of optimal bound
public:
	WeightVec     weights; // sparse vectors of weights - only used for multi-level constraints
	PrioVec       prios;   // (optional): maps levels to original priorities
POTASSCO_WARNING_BEGIN_RELAXED
	WeightLiteral lits[0]; // (shared) literals - terminated with lit_true()
POTASSCO_WARNING_END_RELAXED
private:
	~SharedMinimizeData();
	void destroy() const;
	SharedMinimizeData(const SharedMinimizeData&);
	SharedMinimizeData& operator=(const SharedMinimizeData&);
};
//! Helper class for creating minimize constraints.
/*!
 * \ingroup constraint
 */
class MinimizeBuilder {
public:
	typedef SharedMinimizeData SharedData;
	MinimizeBuilder();

	MinimizeBuilder& add(weight_t prio, const WeightLitVec& lits);
	MinimizeBuilder& add(weight_t prio, WeightLiteral lit);
	MinimizeBuilder& add(weight_t prio, weight_t adjust);
	MinimizeBuilder& add(const SharedData& minCon);

	bool empty() const;

	//! Creates a new data object from previously added minimize literals.
	/*!
	 * The function creates a new minimize data object from
	 * the previously added literals to minimize. The returned
	 * object can be used to attach one or more MinimizeConstraints.
	 * \param ctx A ctx object to be associated with the new minmize constraint.
	 * \return A data object representing previously added minimize statements or 0 if empty().
	 * \pre !ctx.frozen()
	 * \post empty()
	 */
	SharedData* build(SharedContext& ctx);

	//! Discards any previously added minimize literals.
	void clear();
private:
	struct MLit {
		MLit(const WeightLiteral& wl, weight_t at) : lit(wl.first), prio(at), weight(wl.second) {}
		Literal  lit;
		weight_t prio;
		weight_t weight;
	};
	struct CmpPrio  { bool operator()(const MLit& lhs, const MLit& rhs) const; };
	struct CmpLit   { bool operator()(const MLit& lhs, const MLit& rhs) const; };
	struct CmpWeight{
		CmpWeight(const SharedData::WeightVec* w) : weights(w) {}
		bool operator()(const MLit& lhs, const MLit& rhs) const;
		const SharedData::WeightVec* weights;
	};
	typedef PodVector<MLit>::type LitVec;
	void prepareLevels(const Solver& s, SumVec& adjustOut, WeightVec& priosOut);
	void mergeLevels(SumVec& adjust, SharedData::WeightVec& weightsOut);
	SharedData* createShared(SharedContext& ctx, const SumVec& adjust, const CmpWeight& cmp);
	LitVec lits_;
};

//! Base class for implementing (mulit-level) minimize statements.
/*!
 * \ingroup constraint
 * A solver contains at most one minimize constraint, but a minimize constraint
 * may represent several minimize statements. In that case, each minimize statement
 * has a unique level (starting at 0) and minimize statements with a lower level
 * have higher priority. Priorities are handled like in smodels, i.e. statements
 * with lower priority become relevant only if all statements with higher priority
 * currently have an optimal assignment.
 *
 * MinimizeConstraint supports two modes of operation: if mode is set to
 * optimize, solutions are considered optimal only if they are strictly smaller
 * than previous solutions. Otherwise, if mode is set to enumerate a
 * solution is valid if it is not greater than the initially set optimum.
 * Example:
 *  m0: {a, b}
 *  m1: {c, d}
 *  All models: {a, c,...}, {a, d,...} {b, c,...}, {b, d,...} {a, b,...}
 *  Mode = optimize: {a, c, ...} (m0 = 1, m1 = 1}
 *  Mode = enumerate and initial opt=1,1: {a, c, ...}, {a, d,...}, {b, c,...}, {b, d,...}
 *
 */
class MinimizeConstraint : public Constraint {
public:
	typedef SharedMinimizeData SharedData;
	typedef const SharedData*  SharedDataP;
	//! Returns a pointer to the shared representation of this constraint.
	SharedDataP shared()  const { return shared_; }
	//! Attaches this object to the given solver.
	virtual bool attach(Solver& s)    = 0;
	//! Shall activate the minimize constraint by integrating bounds stored in the shared data object.
	virtual bool integrate(Solver& s) = 0;
	//! Shall relax this constraint (i.e. remove any bounds).
	/*!
	 * If reset is true, shall also remove search-path related state.
	 */
	virtual bool relax(Solver& s, bool reset) = 0;
	//! Shall commit the model in s to the shared data object.
	/*!
	 * The return value indicates whether the model is valid w.r.t the
	 * costs stored in the shared data object.
	 */
	virtual bool handleModel(Solver& s) = 0;
	//! Shall handle the unsatisfiable path in s.
	virtual bool handleUnsat(Solver& s, bool upShared, LitVec& restore) = 0;
	virtual bool supportsSplitting() const { return true; }
	// base interface
	void         destroy(Solver*, bool);
	Constraint*  cloneAttach(Solver&) { return 0; }
protected:
	MinimizeConstraint(SharedData* s);
	~MinimizeConstraint();
	void reportLower(Solver& s, uint32 level, wsum_t low) const;
	bool prepare(Solver& s, bool useTag);
	SharedData* shared_; // common shared data
	Literal     tag_;    // (optional) literal for tagging reasons
};

//! Minimization via branch and bound.
/*!
 * \ingroup constraint
 * The class supports both basic branch and bound as well as
 * hierarchical branch and bound (with or without varying step sizes).
 */
class DefaultMinimize : public MinimizeConstraint {
public:
	explicit DefaultMinimize(SharedData* d, const OptParams& params);
	// base interface
	//! Attaches the constraint to the given solver.
	/*!
	 * \pre s.decisionLevel() == 0
	 * \note If either MinimizeMode_t::enumOpt or hierarchical optimization
	 * is active, s.sharedContext()->tagLiteral() shall be an unassigned literal.
	 */
	bool       attach(Solver& s);
	bool       integrate(Solver& s)           { return integrateBound(s); }
	bool       relax(Solver&, bool reset)     { return relaxBound(reset); }
	bool       handleModel(Solver& s)         { commitUpperBound(s); return true; }
	bool       handleUnsat(Solver& s, bool up, LitVec& out);
	// constraint interface
	PropResult propagate(Solver& s, Literal p, uint32& data);
	void       undoLevel(Solver& s);
	void       reason(Solver& s, Literal p, LitVec& lits);
	bool       minimize(Solver& s, Literal p, CCMinRecursive* r);
	void       destroy(Solver*, bool);
	// own interface
	bool       active()  const { return *opt() != SharedData::maxBound(); }
	//! Number of minimize statements contained in this constraint.
	uint32     numRules()const { return size_; }
	//! Tries to integrate the next tentative bound into this constraint.
	/*!
	 * Starting from the current optimum stored in the shared data object,
	 * the function tries to integrate the next candidate bound into
	 * this constraint.
	 *
	 * \return The function returns true if integration succeeded. Otherwise
	 * false is returned and s.hasConflict() is true.
	 *
	 * \note If integrateBound() failed, the bound of this constraint
	 *       is relaxed. The caller has to resolve the conflict first
	 *       and then integrateBound() shall be called again.
	 *
	 * \note The caller has to call s.propagate() to propagate any new information
	 *       from the new bound.
	 *
	 * \note If the tag literal (if any) is not true, the minimize constraint first assumes it.
	 */
	bool       integrateBound(Solver& s);

	//! Sets the current local sum as the global optimum (upper bound).
	/*!
	 * commitUpperBound() shall be called whenever the solver finds a model.
	 * The current local sum is recorded as new optimum in the shared data object.
	 * Once the local bound is committed, the function integrateBound() has to be
	 * called in order to continue optimization.
	 */
	void       commitUpperBound(const Solver& s);
	//! Sets the current local upper bound as the lower bound of this constraint.
	/*!
	 * commitLowerBound() shall be called on unsat. The function stores
	 * the local upper bound as new lower bound in this constraint. If upShared is true,
	 * the lower bound is also copied to the shared data object.
	 *
	 * Once the local bound is committed, the function integrateBound() has to be
	 * called in order to continue optimization.
	 * \return false if search-space is exceeded w.r.t this constraint.
	 */
	bool       commitLowerBound(Solver& s, bool upShared);

	//! Removes the local upper bound of this constraint and therefore disables it.
	/*!
	 * If full is true, also removes search-path related state.
	 */
	bool       relaxBound(bool full = false);

	bool       more() const { return step_.lev != size_; }

	// FOR TESTING ONLY!
	wsum_t sum(uint32 i, bool adjust) const { return sum()[i] + (adjust ? shared_->adjust(i):0); }
private:
	enum PropMode  { propagate_new_sum, propagate_new_opt };
	union UndoInfo;
	typedef const WeightLiteral* Iter;
	~DefaultMinimize();
	// bound operations
	wsum_t* opt() const { return bounds_; }
	wsum_t* sum() const { return bounds_ + size_; }
	wsum_t* temp()const { return bounds_ + (size_*2); }
	wsum_t* end() const { return bounds_ + (size_*3); }
	void    assign(wsum_t* lhs, wsum_t* rhs)  const;
	bool    greater(wsum_t* lhs, wsum_t* rhs, uint32 len, uint32& aLev) const;
	// propagation & undo
	uint32  lastUndoLevel(const Solver& s) const;
	bool    litSeen(uint32 i) const;
	bool    propagateImpl(Solver& s, PropMode m);
	uint32  computeImplicationSet(const Solver& s, const WeightLiteral& it, uint32& undoPos);
	void    pushUndo(Solver& s, uint32 litIdx);
	bool    updateBounds(bool applyStep);
	// step
	wsum_t& stepLow() const { return *(end() + step_.lev); }
	void    stepInit(uint32 n);
	wsum_t*      bounds_;  // [upper,sum,temp[,lower]]
	Iter         pos_;     // position of literal to look at next
	UndoInfo*    undo_;    // one "seen" flag for each literal +
	uint32       undoTop_; // undo stack holding assigned literals
	uint32       posTop_;  // stack of saved "look at" positions
	const uint32 size_;    // number of rules
	uint32       actLev_;  // first level to look at when comparing bounds
	struct Step {          // how to reduce next tentative bound
	uint32 size;           //   size of step
	uint32 lev : 30;       //   level on which step is applied
	uint32 type:  2;       //   type of step (one of OptParams::BBAlgo)
	}            step_;
};

//! Minimization via unsat cores.
/*!
 * \ingroup constraint
 */
class UncoreMinimize : public MinimizeConstraint {
public:
	// constraint interface
	PropResult propagate(Solver& s, Literal p, uint32& data);
	void       reason(Solver& s, Literal p, LitVec& lits);
	void       destroy(Solver*, bool);
	bool       simplify(Solver& s, bool reinit = false);
	// base interface
	bool       attach(Solver& s);
	bool       integrate(Solver& s);
	bool       relax(Solver&, bool reset);
	bool       valid(Solver& s);
	bool       handleModel(Solver& s);
	bool       handleUnsat(Solver& s, bool up, LitVec& out);
	bool       supportsSplitting() const { return false; }
private:
	friend class SharedMinimizeData;
	explicit UncoreMinimize(SharedData* d, const OptParams& params);
	typedef DefaultMinimize* EnumPtr;
	struct LitData {
		LitData(weight_t w, bool as, uint32 c) : weight(w), coreId(c), assume((uint32)as), flag(0u) {}
		weight_t weight;
		uint32   coreId : 30;
		uint32   assume :  1;
		uint32   flag   :  1;
	};
	struct LitPair {
		LitPair(Literal p, uint32 dataId) : lit(p), id(dataId) {}
		Literal lit;
		uint32  id;
	};
	struct Core {
		Core(WeightConstraint* c, weight_t b, weight_t w) : con(c), bound(b), weight(w) {}
		uint32  size()       const;
		Literal at(uint32 i) const;
		Literal tag()        const;
		WeightConstraint* con;
		weight_t          bound;
		weight_t          weight;
	};
	struct WCTemp {
		typedef WeightLitVec WLitVec;
		typedef WeightLiteral* Ptr;
		void     start(weight_t b){ lits.clear(); bound = b; }
		void     add(Solver& s, Literal p);
		bool     unsat() const { return bound > 0 && static_cast<uint32>(bound) > static_cast<uint32>(lits.size()); }
		uint32   size()  const { return sizeVec(lits); }
		Ptr      begin() { return size() ? &lits[0] : 0; }
		weight_t bound;
		WLitVec  lits;
	};
	typedef PodVector<LitData>::type     LitTable;
	typedef PodVector<Core>::type        CoreTable;
	typedef PodVector<Constraint*>::type ConTable;
	typedef PodVector<LitPair>::type     LitSet;
	class Todo {
	public:
		typedef LitSet::const_iterator const_iterator;
		Todo() { clear(); }
		const_iterator begin()  const { return lits_.begin(); }
		const_iterator end()    const { return lits_.end(); }
		uint32         size()   const { return sizeVec(lits_); }
		weight_t       weight() const { return minW_; }
		bool           shrink() const { return next_ != 0u; }
		void clear(bool resetShrink = true);
		void add(const LitPair& x, weight_t w);
		void terminate();
		bool shrinkNext(UncoreMinimize& self, ValueRep result);
		void shrinkPush(UncoreMinimize& self, Solver& s);
		void shrinkReset();
	private:
		bool subsetNext(UncoreMinimize& self, ValueRep result);
		LitSet   lits_;
		weight_t minW_;
		// shrinking
		uint32   last_;
		uint32   next_;
		uint32   step_;
		LitSet   core_;
	};
	// literal and core management
	bool     hasCore(const LitData& x) const { return x.coreId != 0; }
	bool     flagged(uint32 id)        const { return litData_[id-1].flag != 0u; }
	void     setFlag(uint32 id, bool f)      { litData_[id-1].flag = uint32(f); }
	LitData& getData(uint32 id)              { return litData_[id-1];}
	Core&    getCore(const LitData& x)       { return open_[x.coreId-1]; }
	LitPair  newAssumption(Literal p, weight_t w);
	Literal  newLit(Solver& s);
	void     releaseLits();
	bool     addCore(Solver& s, const LitPair* lits, uint32 size, weight_t w, bool updateLower);
	uint32   allocCore(WeightConstraint* con, weight_t bound, weight_t weight, bool open);
	bool     closeCore(Solver& s, LitData& x, bool sat);
	bool     addOll(Solver& s, const LitPair* lits, uint32 size, weight_t w);
	bool     addOllCon(Solver& s, const WCTemp& wc, weight_t w);
	bool     addK(Solver& s, uint32 K, const LitPair* lits, uint32 size, weight_t w);
	enum CompType { comp_disj = 0, comp_conj = 1 };
	bool     addPmr(Solver& s, const LitPair* lits, uint32 size, weight_t w);
	bool     addPmrCon(CompType t, Solver& s, Literal head, Literal body1, Literal body2);
	bool     addConstraint(Solver& s, WeightLiteral* lits, uint32 size, weight_t bound);
	bool     addImplication(Solver& s, Literal a, Literal b, bool concise);
	// algorithm
	void     init();
	uint32   initRoot(Solver& s);
	bool     initLevel(Solver& s);
	uint32   analyze(Solver& s);
	bool     addNext(Solver& s, bool allowInit = true);
	bool     pushPath(Solver& s);
	bool     popPath(Solver& s, uint32 dl);
	bool     fixLit(Solver& s, Literal p);
	bool     fixLevel(Solver& s);
	void     detach(Solver* s, bool b);
	bool     pushTrim(Solver& s);
	void     resetTrim(Solver& s);
	bool     push(Solver& s, Literal p, uint32 id);
	wsum_t*  computeSum(const Solver& s) const;
	bool     validLowerBound() const {
		wsum_t cmp = lower_ - upper_;
		return cmp < 0 || (cmp == 0 && level_ == shared_->maxLevel() && !shared_->checkNext());
	}
	// data
	EnumPtr   enum_;      // for supporting (optimal) model enumeration in parallel mode
	wsum_t*   sum_;       // costs of active model
	LitTable  litData_;   // data for active literals (tag lits for cores + lits from active minimize)
	CoreTable open_;      // open cores, i.e. relaxable and referenced by an assumption
	ConTable  closed_;    // closed cores represented as weight constraints
	LitSet    assume_;    // current set of assumptions
	Todo      todo_;      // core(s) not yet represented as constraint
	LitVec    fix_;       // set of fixed literals
	LitVec    conflict_;  // temporary: conflicting set of assumptions
	WCTemp    temp_;      // temporary: used for creating weight constraints
	wsum_t    lower_;     // lower bound of active level
	wsum_t    upper_;     // upper bound of active level
	uint32    auxInit_;   // number of solver aux vars on attach
	uint32    auxAdd_;    // number of aux vars added for cores
	uint32    gen_;       // active generation
	uint32    level_ : 28;// active level
	uint32    next_  :  1;// update because of model
	uint32    disj_  :  1;// preprocessing active?
	uint32    path_  :  1;// push path?
	uint32    init_  :  1;// init constraint?
	weight_t  actW_;      // active weight limit (only weighted minimization with stratification)
	weight_t  nextW_;     // next weight limit   (only weighted minimization with stratification)
	uint32    eRoot_;     // saved root level of solver (initial gp)
	uint32    aTop_;      // saved assumption level (added by us)
	uint32    freeOpen_;  // head of open core free list
	OptParams options_;   // active options
};

} // end namespace Clasp

#endif
