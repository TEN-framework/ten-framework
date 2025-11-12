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
#ifndef CLASP_CLAUSE_H_INCLUDED
#define CLASP_CLAUSE_H_INCLUDED

#ifdef _MSC_VER
#pragma once
#endif

#include <clasp/solver_types.h>
namespace Clasp {

//! An array of literals that can be shared between threads.
/*!
 * \ingroup shared_con
 */
class SharedLiterals {
public:
	//! Creates a shareable (ref-counted) object containing the literals in lits.
	/*!
	 * \note The reference count is set to numRefs.
	 */
	static SharedLiterals* newShareable(const LitVec& lits, ConstraintType t, uint32 numRefs = 1) {
		return newShareable(!lits.empty() ? &lits[0]:0, static_cast<uint32>(lits.size()), t, numRefs);
	}
	static SharedLiterals* newShareable(const Literal* lits, uint32 size, ConstraintType t, uint32 numRefs = 1);

	//! Returns a pointer to the beginning of the literal array.
	const Literal* begin() const { return lits_; }
	//! Returns a pointer to the end of the literal array.
	const Literal* end()   const { return lits_+size(); }
	//! Returns the number of literals in the array.
	uint32         size()  const { return size_type_ >> 2; }
	//! Returns the type of constraint from which the literals originated.
	ConstraintType type()  const { return ConstraintType( size_type_ & uint32(3) ); }
	//! Simplifies the literals w.r.t to the assignment in s.
	/*!
	 * Returns the number of non-false literals in this object or 0 if
	 * the array contains a true literal.
	 * \note If the object is currently not shared, simplify() removes
	 * all false literals from the array.
	 */
	uint32 simplify(Solver& s);

	void            release() { release(1); }
	void            release(uint32 numRefs);
  SharedLiterals* share();
	bool            unique()   const { return refCount_ <= 1; }
	uint32          refCount() const { return refCount_; }
private:
	void destroy();
	SharedLiterals(const Literal* lits, uint32 size, ConstraintType t, uint32 numRefs);
	SharedLiterals(const SharedLiterals&);
	SharedLiterals& operator=(const SharedLiterals&);
	typedef Clasp::Atomic_t<int32>::type RCType;
	RCType  refCount_;
	uint32  size_type_;
POTASSCO_WARNING_BEGIN_RELAXED
	Literal lits_[0];
POTASSCO_WARNING_END_RELAXED
};

//! A helper-class for creating/adding clauses.
/*!
 * \ingroup constraint
 * This class simplifies clause creation. It hides the special handling of
 * short, and shared clauses. It also makes sure that learnt clauses watch
 * the literals from the highest decision levels.
 */
class ClauseCreator {
public:
	typedef ConstraintInfo ClauseInfo;
	//! Creates a new ClauseCreator object.
	/*!
	 * \param s the Solver in which to store created clauses.
	 */
	explicit ClauseCreator(Solver* s = 0);
	//! Sets the solver in which created clauses are stored.
	void setSolver(Solver& s)         { solver_ = &s; }
	//! Adds additional flags to be applied in end().
	void addDefaultFlags(uint32 f)    { flags_ |= f; }
	//! Reserve space for a clause of size s.
	void reserve(LitVec::size_type s) { literals_.reserve(s); }
	//! Discards the current clause.
	void clear()                      { literals_.clear(); }

	//! Status of a clause.
	/*!
	 * For a clause with literals [l1,...,ln], status is one of:
	 */
	enum Status {
		// BASE STATUS
		status_open         = 0,  //!< Clause is neither sat, unsat, or unit.
		status_sat          = 1,  //!< At least one literal is true.
		status_unsat        = 2,  //!< All literals are false.
		status_unit         = 4,  //!< All but one literal false.
		// COMPLEX STATUS
		status_sat_asserting= 5,  //!< Sat but literal is implied on lower dl.
		status_asserting    = 6,  //!< Unsat but literal is implied on second highest dl.
		status_subsumed     = 9,  //!< Sat and one literal is true on level 0.
		status_empty        = 10, //!< Unsat and all literals are false on level 0.
	};
	//! A type for storing the result of a clause insertion operation.
	struct Result {
		explicit Result(ClauseHead* loc = 0, Status st = status_open)
			: local(loc)
			, status(st) {}
		ClauseHead*     local;
		Status          status;
		//! Returns false is clause is conflicting w.r.t current assignment.
		bool     ok()   const { return (status & status_unsat) == 0; }
		//! Returns true if the clause implies a literal (possibly after backtracking).
		bool     unit() const { return (status & status_unit) != 0; }
		operator bool() const { return ok(); }
	};
	//! Starts the creation of a new clause.
	/*!
	 * \pre s.decisionLevel() == 0 || t != Constraint_t::Static
	 */
	ClauseCreator& start(ConstraintType t = Constraint_t::Static);
	//! Sets the initial activity of the clause under construction.
	ClauseCreator& setActivity(uint32 a)      { extra_.setActivity(a);  return *this; }
	//! Sets the initial literal block distance of the clause under construction.
	ClauseCreator& setLbd(uint32 lbd)         { extra_.setLbd(lbd); return *this; }
	//! Adds the literal p to the clause under construction.
	ClauseCreator& add(const Literal& p)      { literals_.push_back(p); return *this; }
	//! Removes subsumed lits and orders first lits w.r.t watch order.
	ClauseRep      prepare(bool fullSimplify);
	//! Returns the current size of the clause under construction.
	uint32         size()    const { return (uint32)literals_.size(); }
	//! Returns the literal at the given idx.
	Literal&       operator[](uint32 i)       { return literals_[i]; }
	Literal        operator[](uint32 i) const { return literals_[i]; }
	//! Returns the literals of the clause under construction.
	const LitVec&  lits()    const { return literals_; }
	LitVec&        lits()          { return literals_; }
	//! Returns the clause's type.
	ConstraintType type()    const { return extra_.type(); }
	//! Returns the aux info of the clause under construction.
	ConstraintInfo info()    const { return extra_; }
	//! Creates a new clause object for the clause under construction.
	/*!
	 * \pre The clause does not contain duplicate/complementary literals or
	 *      flags contains clause_force_simplify.
	 *
	 * \note If the clause to be added is empty, end() fails and s.hasConflict() is set to true.
	 * \see Result ClauseCreator::create(Solver& s, LitVec& lits, uint32 flags, const ClauseInfo& info);
	 */
	Result end(uint32 flags = clause_not_sat | clause_not_conflict);

	/*!
	 * \name Factory functions
	 * Functions for creating and integrating clauses.
	 */
	//@{
	//! Flags controlling clause creation and integration.
	enum CreateFlag {
		// REPRESENTATION
		clause_no_add        = 1,  //!< Do not add clause to solver db.
		clause_explicit      = 2,  //!< Force creation of explicit clause even if size <= 3.
		// STATUS
		clause_not_sat       = 4,  //!< Do not add clause if it is satisfied (but not asserting) w.r.t current assignment.
		clause_not_root_sat  = 8,  //!< Do not add clause if it is satisfied w.r.t the root assignment.
		clause_not_conflict  = 16, //!< Do not add clause if it is conflicting w.r.t the current assignment.
		// INTEGRATE
		clause_no_release    = 32, //!< Do not call release on shared literals.
		clause_int_lbd       = 64, //!< Compute lbd when integrating asserting clauses.
		// PREPARE
		clause_no_prepare    = 128,//!< Assume clause is already ordered w.r.t watches.
		clause_force_simplify= 256,//!< Call simplify() on create.
		clause_no_heuristic  = 512,//!< Do not notify heuristic about new clause.
		// WATCH MODE - only for problem clauses
		clause_watch_first   =1024,//!< Watch first free literals.
		clause_watch_rand    =2048,//!< Watch rand literals.
		clause_watch_least   =4096,//!< Watch least watched literals.
	};
	//! Returns the status of the given clause w.r.t s.
	static Status    status(const Solver& s, const Literal* clause_begin, const Literal* clause_end);
	static Status    status(const Solver& s, const ClauseRep& c);

	//! Returns an abstraction of p's decision level that can be used to order literals.
	/*!
	 * The function returns a value, s.th
	 * order(any true literal) > order(any free literal) > order(any false literal).
	 * Furthermore, for equally assigned literals p and q, order(p) > order(q), iff
	 * level(p) > level(q).
	 */
	static uint32    watchOrder(const Solver& s, Literal p);

	//! Prepares the clause given in lits.
	/*!
	 * A prepared clause [l1...ln] with n >= 2 is a clause that,
	 *  - does not contain any duplicate or complementary literals, and
	 *  - does not contain any subsumed literals (i.e. literals assigned on decision level 0), and
	 *  - is partially ordered w.r.t watchOrder(), i.e., watchOrder(l1) >= watchOrder(l2), and there
	 *    is no lj, j > 2, s.th. watchOrder(lj) > watchOrder(l2)
	 *  .
	 *
	 * Removes subsumed literals from lits and reorders lits s.th.
	 * the first literals are valid watches. Furthermore,
	 * if flags contains clause_force_simplify,
	 * duplicate literals are removed from lits and tautologies are
	 * replaced with the single literal True.
	 */
	static ClauseRep prepare(Solver& s, LitVec& lits, uint32 flags, const ClauseInfo& info = ClauseInfo());

	//! Creates a clause from the literals given in lits.
	/*!
	 * \param s     The solver to which the clause should be added.
	 * \param lits  The literals of the clause.
	 * \param flags Flag set controlling creation (see ClauseCreator::CreateFlag).
	 * \param info  Initial information (e.g. type) for the new clause.
	 *
	 * \pre !s.hasConflict() and s.decisionLevel() == 0 or extra.learnt()
	 * \pre lits is fully prepared or flags contains suitable prepare flags.
	 *
	 * \note
	 *   If the given clause is unit (or asserting), the unit-resulting literal is
	 *   asserted on the (numerical) lowest level possible but the new information
	 *   is not immediately propagated, i.e. on return queueSize() may be greater than 0.
	 *
	 * \note
	 *   The local representation of the clause is always attached to the solver
	 *   but only added to the solver if clause_no_add is not contained in flags.
	 *   Otherwise, the returned clause is owned by the caller
	 *   and it is the caller's responsibility to manage it. Furthermore,
	 *   learnt statistics are *not* updated automatically in that case.
	 *
	 * \see prepare()
	 */
	static Result create(Solver& s, LitVec& lits, uint32 flags, const ClauseInfo& info = ClauseInfo());

	/*!
	 * \overload
	 */
	static Result create(Solver& s, const ClauseRep& rep, uint32 flags);

	//! Integrates the given clause into the current search of s.
	/*!
	 * \pre the assignment in s is not conflicting
	 * \param s      The solver in which the clause should be integrated.
	 * \param clause The clause to be integrated.
	 * \param flags  A set of flags controlling integration (see ClauseCreator::CreateFlag).
	 * \param t      Constraint type to use for the local representation.
	 *
	 * \note
	 *   The function behaves similar to ClauseCreator::create() with the exception that
	 *   it does not add local representations for implicit clauses (i.e. size <= 3)
	 *   unless flags contains clause_explicit.
	 *   In that case, an explicit representation is created.
	 *   Implicit representations can only be created via ClauseCreator::create().
	 *
	 * \note
	 *   The function acts as a sink for the given clause (i.e. it decreases its reference count)
	 *   unless flags contains clause_no_release.
	 *
	 * \note integrate() is intended to be called in a post propagator.
	 *   To integrate a set of clauses F, one would use a loop like this:
	 *   \code
	 *   bool MyPostProp::propagate(Solver& s) {
	 *     bool r = true;
	 *     while (!F.empty() && r) {
	 *       SharedLiterals* C = f.pop();
	 *       r = integrate(s, C, ...).ok;
	 *     }
	 *     return r;
	 *   \endcode
	 */
	static Result integrate(Solver& s, SharedLiterals* clause, uint32 flags, ConstraintType t);

	/*!
	 * \overload
	 */
	static Result integrate(Solver& s, SharedLiterals* clause, uint32 flags);
	//@}
private:
	static ClauseRep   prepare(Solver& s, const Literal* in, uint32 inSize, const ClauseInfo& e, uint32 flags, Literal* out, uint32 outMax = UINT32_MAX);
	static Result      create_prepared(Solver& s, const ClauseRep& pc, uint32 flags);
	static ClauseHead* newProblemClause(Solver& s, const ClauseRep& clause, uint32 flags);
	static ClauseHead* newLearntClause(Solver& s, const ClauseRep& clause, uint32 flags);
	static ClauseHead* newUnshared(Solver& s, SharedLiterals* clause, const Literal* w, const ClauseInfo& e);
	static bool        ignoreClause(const Solver& s, const ClauseRep& cl, Status st, uint32 modeFlags);
	Solver*    solver_;   // solver in which new clauses are stored
	LitVec     literals_; // literals of the new clause
	ClauseInfo extra_;    // extra info
	uint32     flags_;    // default flags to be used in end()
};

//! Class for representing a clause in a solver.
/*!
 * \ingroup constraint
 */
class Clause : public ClauseHead {
public:
	typedef Constraint::PropResult PropResult;

	//! Allocates memory for storing a (learnt) clause with nLits literals.
	static void* alloc(Solver& s, uint32 mLits, bool learnt);

	//! Creates a new clause from the clause given in rep.
	/*!
	 * \param s   Solver in which the new clause is to be used.
	 * \param rep The raw representation of the clause.
	 *
	 * \pre The clause given in lits is prepared and contains at least two literals.
	 * \note The clause must be destroyed using Clause::destroy.
	 * \see ClauseCreator::prepare()
	 */
	static ClauseHead*  newClause(Solver& s, const ClauseRep& rep) {
		return newClause(alloc(s, rep.size, rep.info.learnt()), s, rep);
	}
	//! Creates a new clause object in mem.
	/*!
	 * \pre mem points to a memory block that was allocated via Clause::alloc().
	 */
	static ClauseHead*  newClause(void* mem, Solver& s, const ClauseRep& rep);

	//! Creates a new contracted clause from the clause given in rep.
	/*!
	 * A contracted clause consists of an active head and a tail of false literals.
	 * Propagation is restricted to the head.
	 * The tail is only needed to compute reasons from assignments.
	 *
	 * \param s       Solver in which the new clause is to be used.
	 * \param rep     The raw representation of the clause.
	 * \param tailPos The starting index of the tail (first literal that should be temporarily removed from the clause).
	 * \param extend  Extend head part of clause as tail literals become free?
	 */
	static ClauseHead*  newContractedClause(Solver& s, const ClauseRep& rep, uint32 tailPos, bool extend);

	//! Creates a new local surrogate for shared_lits to be used in the given solver.
	/*!
	 * \param s      The solver in which this clause will be used.
	 * \param lits   The shared literals of this clause.
	 * \param e      Initial meta data for the new (local) clause.
	 * \param head   Watches and cache literal for the new (local) clause.
	 * \param addRef Increment ref count of lits.
	 */
	static ClauseHead* newShared(Solver& s, SharedLiterals* lits, const InfoType& e, const Literal head[3], bool addRef = true);

	// Constraint-Interface

	Constraint* cloneAttach(Solver& other);

	/*!
	 * For a clause [x y p] the reason for p is ~x and ~y.
	 * \pre *this previously asserted p
	 * \note if the clause is a learnt clause, calling reason increases
	 * the clause's activity.
	 */
	void reason(Solver& s, Literal p, LitVec& lits);

	bool minimize(Solver& m, Literal p, CCMinRecursive* r);

	bool isReverseReason(const Solver& s, Literal p, uint32 maxL, uint32 maxN);

	//! Returns true if clause is SAT.
	/*!
	 * Removes from the clause all literals that are false.
	 */
	bool simplify(Solver& s, bool = false);

	//! Destroys the clause and frees its memory.
	void destroy(Solver* s = 0, bool detach = false);

	// LearntConstraint interface

	//! Returns type() if the clause is currently not satisfied and t.inSet(type()).
	uint32 isOpen(const Solver& s, const TypeSet& t, LitVec& freeLits);

	// clause interface
	BoolPair strengthen(Solver& s, Literal p, bool allowToShort);
	void     detach(Solver&);
	uint32   size()                 const;
	void     toLits(LitVec& out)    const;
	bool     contracted()           const;
	bool     isSmall()              const;
	bool     strengthened()         const;
	uint32   computeAllocSize()     const;
private:
	Clause(Solver& s, const ClauseRep& rep, uint32 tail = UINT32_MAX, bool extend = false);
	Clause(Solver& s, const Clause& other);
	typedef std::pair<Literal*, Literal*> LitRange;
	void     undoLevel(Solver& s);
	bool     updateWatch(Solver& s, uint32 pos);
	Literal* end() { return head_+local_.size(); }
	Literal* removeFromTail(Solver& s, Literal* it, Literal* end);
	Literal* small();
	LitRange tail();
};

//! Constraint for Loop-Formulas.
/*!
 * \ingroup constraint
 * Special purpose constraint for loop formulas of the form: L v B1 v ... v Bm,
 * where L is an unfounded set represented as a set of atom literals {~a1, ..., ~an}.
 * Representing such a loop formula explicitly as n clauses
 *   - (1) ~a1 v B1 v ... v Bm
 *   - ...
 *   - (n) ~an v B1 v ... v Bm
 *   .
 * is wasteful because each clause contains the same set of bodies.
 *
 * The idea behind LoopFormula is to treat L as a "macro-literal"
 * with the following properties:
 * - isTrue(L), iff for all ai isTrue(~ai)
 * - isFalse(L), iff for some ai isFalse(~ai)
 * - L is watchable, iff not isFalse(L)
 * - Watching L means watching all ai.
 * - setting L to true means setting all ai to false.
 * Using this convention the TWL-algo can be implemented as in a clause.
 *
 * \par Implementation:
 * - The literal-array is divided into two parts, an "active clause" part and an atom part.
 * - The "active clause" contains one atom and all bodies: [~ai B1 ... Bj]
 * - The atom part contains all atoms: [~a1 ... ~an]
 * - Two of the literals of the "active clause" are watched (again: watching an atom means watching all atoms)
 * - If a watched atom becomes true, it is copied into the "active clause" and the TWL-algo starts.
 */
class LoopFormula : public Constraint {
public:
	//! Creates a new loop-constraint for the given atoms.
	/*!
	* \param s      Solver in which the new constraint is to be used.
	* \param c1     First clause of the new constraint.
	* \param atoms  Set of atoms in the loop.
	* \param nAtoms Number of atoms in the loop.
	* \param updateHeuristic Whether to notify heuristic about new constraint.
	*
	* \pre The clause given in c1 is prepared and c1.size > 1 and c1.lits[0] is a literal in atoms.
	* \see ClauseCreator::prepare()
	*/
	static LoopFormula* newLoopFormula(Solver& s, const ClauseRep& c1, const Literal* atoms, uint32 nAtoms, bool updateHeuristic = true);

	//! Returns the number of literals in the loop-formula.
	uint32 size() const;

	// Constraint interface
	Constraint* cloneAttach(Solver&) { return 0; }
	PropResult  propagate(Solver& s, Literal p, uint32& data);
	void reason(Solver&, Literal p, LitVec& lits);
	bool minimize(Solver& s, Literal p, CCMinRecursive* ccMin);
	bool simplify(Solver& s, bool = false);
	void destroy(Solver* = 0, bool = false);

	// LearntConstraint interface
	bool locked(const Solver& s) const;

	uint32 isOpen(const Solver& s, const TypeSet& t, LitVec& freeLits);

	//! Returns the loop-formula's activity.
	/*!
	 * The activity of a loop-formula is increased, whenever reason() is called.
	 */
	ScoreType activity() const { return act_; }

	//! Halves the loop-formula's activity.
	void decreaseActivity() { act_.reduce(); }
	void resetActivity()    { act_.reset(); }

	//! Returns Constraint_t::Loop.
	ConstraintType type() const { return Constraint_t::Loop; }
private:
	LoopFormula(Solver& s, const ClauseRep& c1, const Literal* atoms, uint32 nAtoms, bool heu);
	void detach(Solver& s);
	bool otherIsSat(const Solver& s);
	Literal* begin() { return lits_ + 1; }
	Literal* xBegin(){ return lits_ + end_ + 1; }
	Literal* xEnd()  { return lits_ + size_; }
	ScoreType act_;     // activity of constraint
	uint32    end_;     // position of second sentinel
	uint32    size_:30; // size of lits_
	uint32    str_ : 1; // removed literal(s) during simplify?
	uint32    xPos_: 1; // position of ai in lits_ or 0 if no atom
	uint32    other_;   // stores the position of a literal that was recently true
POTASSCO_WARNING_BEGIN_RELAXED
	Literal   lits_[0]; // S ai B1...Bm S a1...an
POTASSCO_WARNING_END_RELAXED
};

namespace mt {

//! Stores the local part of a shared clause.
/*!
 * \ingroup constraint
 * The local part of a shared clause consists of a
 * clause head and and a pointer to the shared literals.
 * Since the local part is owned by a particular solver
 * it can be safely modified. Destroying a SharedLitsClause
 * means destroying the local part and decreasing the
 * shared literals' reference count.
 */
class SharedLitsClause : public ClauseHead {
public:
	//! Creates a new SharedLitsClause to be used in the given solver.
	/*!
	 * \param s The solver in which this clause will be used.
	 * \param shared_lits The shared literals of this clause.
	 * \param e Initial meta data for the new (local) clause.
	 * \param lits Watches and cache literal for the new (local) clause.
	 * \param addRef Increment ref count of shared_lits.
	 */
	static ClauseHead* newClause(Solver& s, SharedLiterals* shared_lits, const InfoType& e, const Literal* lits, bool addRef = true);

	Constraint* cloneAttach(Solver& other);
	void        reason(Solver& s, Literal p, LitVec& out);
	bool        minimize(Solver& s, Literal p, CCMinRecursive* rec);
	bool        isReverseReason(const Solver& s, Literal p, uint32 maxL, uint32 maxN);
	bool        simplify(Solver& s, bool);
	void        destroy(Solver* s, bool detach);
	uint32      isOpen(const Solver& s, const TypeSet& t, LitVec& freeLits);
	uint32      size() const;
	void        toLits(LitVec& out) const;
private:
	SharedLitsClause(Solver& s, SharedLiterals* x, const Literal* lits, const InfoType&, bool addRef);
	bool     updateWatch(Solver& s, uint32 pos);
	BoolPair strengthen(Solver& s, Literal p, bool allowToShort);
};
}

}
#endif
