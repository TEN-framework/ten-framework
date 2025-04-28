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
#ifndef CLASP_CONSTRAINT_H_INCLUDED
#define CLASP_CONSTRAINT_H_INCLUDED

#ifdef _MSC_VER
#pragma once
#endif

#include <clasp/literal.h>
#include <clasp/util/misc_types.h> // bits stuff
#include <cassert>

/*!
 * \file
 * \brief Defines the base classes for boolean constraints.
 */
namespace Clasp {

class  SharedContext;
class  Solver;
class  ClauseHead;
struct CCMinRecursive;

/**
 * \defgroup constraint Constraints
 * \brief Boolean Constraints, post propagators, and related types.
 * @{ */

//! Constraint types distinguished by a Solver.
struct Constraint_t {
	enum Type {
		Static     = 0, //!< An unremovable constraint (e.g. a problem constraint).
		Conflict   = 1, //!< A removable constraint derived from conflict analysis.
		Loop       = 2, //!< A removable constraint derived from unfounded set checking.
		Other      = 3, //!< A removable constraint learnt by some other means.
		Type__max  = Other
	};
	struct Set {
		Set() : m(0) {}
		bool inSet(Type t) const { return (m & (uint32(1)<<t)) != 0; }
		void addSet(Type t)      { m |= (uint32(1)<<t); }
		uint32 m;
	};
};
typedef Constraint_t::Type ConstraintType;
typedef Constraint_t::Set  TypeSet;

struct ConstraintScore;
class  ConstraintInfo;

//! Base class for (boolean) constraints to be used in a Solver.
/*!
 * Base class for (boolean) constraints like e.g. clauses. Concrete constraint classes define
 * representations for constraints over boolean variables.
 * Each constraint class must define a method for inference (derive information from an assignment),
 * it must be able to detect conflicts (i.e. detect when the current assignment makes the constraint unsatisfiable)
 * and to return a reason for inferred literals as well as conflicts (as a set of literals).
 */
class Constraint {
public:
	//! Type used as return type for Constraint::propagate.
	struct PropResult {
		explicit PropResult(bool a_ok = true, bool a_keepWatch = true) : ok(a_ok), keepWatch(a_keepWatch) {}
		bool ok;         //!< true if propagation completes without conflict.
		bool keepWatch;  //!< true if constraint wants to keep the current watch.
	};
	Constraint();

	/*!
	 * \name Mandatory functions
	 * Functions that must be implemented by all constraints.
	 * @{ */

	/*!
	 * Propagate is called for each constraint that watches p. It shall enqueue
	 * all consequences of p becoming true.
	 * \pre p is true in s
	 * \param s The solver in which p became true.
	 * \param p A literal watched by this constraint that recently became true.
	 * \param data The data-blob that this constraint passed when the watch for p was registered.
	 */
	virtual PropResult propagate(Solver& s, Literal p, uint32& data) = 0;

	/*!
	 * \pre This constraint is the reason for p being true in s.
	 * \post The literals implying p were added to lits.
	 */
	virtual void reason(Solver& s, Literal p, LitVec& lits) = 0;

	//! Returns a clone of this and adds necessary watches to the given solver.
	/*!
	 * The function shall create and return a copy of this constraint
	 * to be used in the given solver. Furthermore, it shall add
	 * necessary watches to the given solver.
	 * \note Return 0 to indicate that cloning is not supported.
	 */
	virtual Constraint* cloneAttach(Solver& other) = 0;
	//@}

	/*!
	 * \name Additional functions
	 * Functions that can be implemented by constraints.
	 * @{ */
	//! Called when the given solver removes a decision level watched by this constraint.
	virtual void undoLevel(Solver& s);

	/*!
	 * \brief Simplify this constraint.
	 *
	 * \pre s.decisionLevel() == 0 and the current assignment is fully propagated.
	 * \return
	 *  true if this constraint can be ignored (e.g. is satisfied),
	 *  false otherwise.
	 * \post
	 * If simplify returned true, this constraint has previously removed all its watches
	 * from the solver.
	 *
	 * \note The default implementation simply returns false.
	 */
	virtual bool simplify(Solver& s, bool reinit = false);

	/*!
	 * \brief Delete this constraint.
	 * \note The default implementation simply calls delete this.
	 * \param s The solver in which this constraint is used (can be 0).
	 * \param detach Whether the constraint shall detach itself from the given solver.
	 */
	virtual void destroy(Solver* s = 0, bool detach = false);

	//! Shall return whether the constraint is valid (i.e. not conflicting) w.r.t the current assignment in s.
	/*!
	 * \pre The assignment in s is not conflicting and fully propagated.
	 * \post A changed assignment if the assignment was not valid.
	 * \note The default implementation always returns true and assumes
	 *       that conflicts are detected by Constraint::propagate().
	 */
	virtual bool valid(Solver& s);

	//! Called during minimization of learnt clauses if this is the reason for p being true in s.
	/*!
	 * \return true if p can be removed from the current learnt clause, false otherwise.
	 * \note The default implementation uses the following inefficient algorithm
	 * \code
	 *   LitVec temp;
	 *   reason(s, p, temp);
	 *   for each x in temp
	 *     if (!s.ccMinimize(p, rec)) return false;
	 *   return true;
	 * \endcode
	 */
	virtual bool minimize(Solver& s, Literal p, CCMinRecursive* rec);

	//! Returns an estimate of the constraint's complexity relative to a clause (complexity = 1).
	virtual uint32 estimateComplexity(const Solver& s) const;

	//! Shall return this if constraint is a clause, otherwise 0.
	/*!
	 * The default implementation returns 0.
	 */
	virtual ClauseHead* clause();
	//@}

	/*!
	 * \name Functions for learnt constraints
	 *
	 * Learnt constraints can be created and deleted dynamically during the search-process and
	 * are subject to nogood-deletion.
	 * A learnt constraint shall at least define the methods type() and locked().
	 * @{ */

	typedef ConstraintType  Type;
	typedef ConstraintScore ScoreType;
	typedef ConstraintInfo  InfoType;

	//! Returns the type of this (learnt) constraint.
	virtual Type type() const;

	/*!
	 * Shall return true if this constraint can't be deleted because it
	 * currently implies one or more literals and false otherwise.
	 * The default implementation returns true.
	 */
	virtual bool locked(const Solver& s) const;

	//! Returns the activity of the constraint.
	/*!
	 * \note A solver uses the activity-value in order to establish an ordering
	 * of learnt constraints. Whenever a solver needs to delete some learnt constraints it
	 * selects from the unlocked ones those with a low activity-value.
	 * \note The default-implementation always returns the minimal activity.
	 */
	virtual ScoreType activity() const;
	//! Asks the constraint to decrease its activity.
	virtual void decreaseActivity();
	//! Asks the constraint to reset its activity.
	virtual void resetActivity();
	//! Shall return 0 if either !t.inSet(type()) or this constraint is satisfied w.r.t the current assignment.
	/*!
	 * If this constraint is currently not satisfied and t.inSet(type()), shall return type()
	 * and freeLits shall contain all currently unassigned literals of this constraint.
	 *
	 * The default implementation always returns 0.
	 */
	virtual uint32 isOpen(const Solver& s, const TypeSet& t, LitVec& freeLits);
	//@}
protected:
	virtual ~Constraint();
private:
	Constraint(const Constraint&);
	Constraint& operator=(const Constraint&);
};
//@}

/**
* \defgroup propagator Propagators
* \brief Post propagators and related types.
* \ingroup constraint
* @{
*/

//! Base class for post propagators.
/*!
 * Post propagators are called after unit propagation on each decision level and
 * once after a total assignment is found.
 *
 * They extend a solver's unit-propagation with more elaborate propagation mechanisms.
 * The typical asp example is an unfounded set check.
 *
 * \note Currently, the solver distinguishes \b two classes of post propagators:
 *       - class_simple: deterministic post propagators that only extend
 *         the current decision level. That is, these post propagators shall neither
 *         backtrack below the current decision level nor permanently add new decision levels.
 *         Deterministic post propagators are called in priority order. For this,
 *         the function PostPropagator::priority() is used and shall return a priority in the range: <tt>[priority_class_simple, priority_class_general)</tt>
 *       - class_general: post propagators that are non-deterministic or those that are not limited to extending
 *         the current decision level shall have a priority of priority_class_general. They are called in FIFO order
 *         after \b all simple post propagators have reached a fixpoint.
 *
 * \note There are currently three reserved priority values, namely
 *  - priority_reserved_msg for message and termination handler (if any),
 *  - priority_reserved_ufs for the default unfounded set checker (if any),
 *  - and priority_reserved_look for the default lookahead operator (if any).
 *  .
 */
class PostPropagator : public Constraint {
public:
	PostPropagator();
	virtual ~PostPropagator();
	using Constraint::propagate;  // Enable overloading!

	PostPropagator* next; // main propagation lists of post propagators
	//! Default priorities for post propagators.
	enum Priority {
		priority_class_simple    = 0,    //!< Starting priority of simple post propagators.
		priority_reserved_msg    = 0,    //!< Reserved priority for message/termination handlers (if any).
		priority_reserved_ufs    = 10,   //!< Reserved priority for the default unfounded set checker (if any).
		priority_reserved_look   = 1023, //!< Reserved priority for the default lookahead operator (if any).
		priority_class_general   = 1024, //!< Priortiy of extended post propagators.
	};

	//! Shall return a value representing the priority of this propagator.
	/*!
	 * The priority is used to order sequences of post propagators and to
	 * classify post propagators w.r.t the classes: class_simple and class_general.
	 * \note See class description for an overview of the two priority classes.
	 */
	virtual uint32 priority() const = 0;

	//! Called during initialization of s.
	/*!
	 * \note During initialization a post propagator may assign variables
	 *       but it must not yet propagate them.
	 */
	virtual bool   init(Solver& s);

	//! Shall enqueue and propagate new assignments implied by this propagator.
	/*!
	 * This function shall enqueue and propagate all assignments currently implied by
	 * this propagator until a fixpoint is reached w.r.t this post propagator or
	 * a conflict is detected.
	 *
	 * \pre   The assignment is fully propagated w.r.t any previous post propagator.
	 * \param s    The solver in which this post propagator is used.
	 * \param ctx  The post propagator from which this post propagator is called or
	 *             0 if no other post propagator is currently active.
	 * \post  s.queueSize() == 0 || s.hasConflict()
	 * \return false if propagation led to conflict, true otherwise
	 *
	 * \note
	 *   The function shall not call Solver::propagate()
	 *   or any other function that would result in a recursive chain
	 *   of propagate() calls. On the other hand, it shall call
	 *   Solver::propagateUntil(this) after enqueuing any new assignments
	 *   to initiate propagation up to this propagator.
	 *
	 * Typically, propagateFixpoint() should implemet a loop like this:
	 * \code
	 * for (;;) {
	 *   if (!assign_newly_implied_literals(s)){ return false; }
	 *   if (s.queueSize() == 0)               { return true;  }
	 *   if (!s.propagateUntil(this))          { return false; }
	 * }
	 * \endcode
	 */
	virtual bool   propagateFixpoint(Solver& s, PostPropagator* ctx) = 0;

	//! Aborts an active propagation operation.
	/*!
	 * The function reset() is called whenever propagation on the
	 * current decision level is stopped before a fixpoint is reached.
	 * In particular, a solver calls reset() when a conflict is detected
	 * during propagation.
	 *
	 * \note The default implementation is a noop.
	 */
	virtual void   reset();

	//! Is the current total assignment a model?
	/*!
	 * \pre The assignment is total and not conflicting.
	 * \return
	 *  - true if the assignment is a model w.r.t this post propagator
	 *  - false otherwise
	 * \post If the function returned true:  s.numFreeVars() == 0 && !s.hasConflict().
	 *       If the function returned false: s.numFreeVars() > 0 || s.hasConflict().
	 * \note The default implementation returns Constraint::valid(s);
	 */
	virtual bool   isModel(Solver& s);
protected:
	//! Calls reset on post propagators following this.
	void cancelPropagation();

	//! PostPropagators are not clonable by default.
	Constraint* cloneAttach(Solver&) { return 0; }
	// Constraint interface - noops
	PropResult  propagate(Solver&, Literal, uint32&);
	void        reason(Solver&, Literal, LitVec& );
private:
	PostPropagator(const PostPropagator&);
	PostPropagator& operator=(const PostPropagator&);
};

//! A special post propagator used to handle messages and signals.
class MessageHandler : public PostPropagator {
public:
	MessageHandler();
	virtual bool   handleMessages() = 0;
	virtual uint32 priority()const { return PostPropagator::priority_reserved_msg; }
	virtual bool   propagateFixpoint(Solver&, PostPropagator*) { return handleMessages(); }
};

//! An intrusive list of post propagators ordered by priority.
/*!
 * Propagators in the list are owned by the list.
 */
class PropagatorList {
public:
	PropagatorList();
	~PropagatorList();
	void add(PostPropagator* p);
	void remove(PostPropagator* p);
	void clear();
	PostPropagator* find(uint32 prio) const;
	PostPropagator*const* head() const { return &head_; }
	PostPropagator**      head() { return &head_; }
private:
	PropagatorList(const PropagatorList&);
	PropagatorList& operator=(const PropagatorList&);
	PostPropagator* head_;// head of pp-list
};
//@}

/**
* \addtogroup constraint
* @{ */

//! Stores a reference to the constraint that implied a literal.
/*!
 * Stores a reference to the constraint that implied a certain literal or
 * null if the literal has no antecedent (i.e. is a decision literal or a top-level fact).
 *
 * \note
 * The constraint that implied a literal can have three different representations:
 * - it can be a single literal (binary clause constraint)
 * - it can be two literals (ternary clause constraint)
 * - it can be a pointer to a constraint (generic constraint)
 * .
 *
 * \par Implementation:
 *
 * The class stores all three representations in one tagged 64-bit integer, i.e.
 * from the 64-bits the 2 LSBs encode the type stored:
 *  - 00: Pointer to constraint
 *  - 01: ternary constraint (i.e. two literals stored in the remaining 62 bits).
 *  - 10: binary constraint (i.e. one literal stored in the highest 31 bits)
 *  .
 */
class Antecedent {
public:
	enum Type { Generic = 0, Ternary = 1, Binary = 2};
	//! Creates a null Antecedent.
	/*!
	 * \post: isNull() == true && type == Generic
	 */
	Antecedent() : data_(0) {}

	//! Creates an Antecedent from the literal p.
	/*!
	 * \post: type() == Binary && firstLiteral() == p
	 */
	Antecedent(const Literal& p) {
		// first lit is stored in high dword
		data_ = (((uint64)p.id()) << 33) + Binary;
		assert(type() == Binary && firstLiteral() == p);
	}

	//! Creates an Antecedent from the literals p and q.
	/*!
	 * \post type() == Ternary && firstLiteral() == p && secondLiteral() == q
	 */
	Antecedent(const Literal& p, const Literal& q) {
		// first lit is stored in high dword
		// second lit is stored in low dword
		data_ = (((uint64)p.id()) << 33) + (((uint64)q.id()) << 2) + Ternary;
		assert(type() == Ternary && firstLiteral() == p && secondLiteral() == q);
	}

	//! Creates an Antecedent from the Constraint con.
	/*!
	 * \post type() == Generic && constraint() == con
	 */
	Antecedent(Constraint* con) : data_((uintp)con) {
		static_assert(sizeof(Constraint*) <= sizeof(uint64), "unsupported pointer size");
		assert(type() == Generic && constraint() == con);
	}

	//! Returns true if this antecedent does not refer to any constraint.
	bool isNull() const { return data_ == 0; }
	//! Returns the antecedent's type.
	Type type()   const { return Type( data_ & 3 ); }
	//! Returns true if the antecedent is a learnt nogood.
	bool learnt() const { return data_ && (data_ & 3u) == 0 && constraint()->type() != Constraint_t::Static; }

	//! Extracts the constraint-pointer stored in this object.
	/*!
	 * \pre type() == Generic
	 */
	Constraint* constraint() const {
		assert(type() == Generic);
		return (Constraint*)(uintp)data_;
	}

	//! Extracts the first literal stored in this object.
	/*!
	 * \pre type() != Generic
	 */
	Literal firstLiteral() const {
		assert(type() != Generic);
		return Literal::fromId(static_cast<uint32>(data_ >> 33));
	}

	//! Extracts the second literal stored in this object.
	/*!
	 * \pre type() == Ternary
	 */
	Literal secondLiteral() const {
		assert(type() == Ternary);
		return Literal::fromId( static_cast<uint32>(data_>>1) >> 1 );
	}

	//! Returns the reason for p.
	/*!
	 * \pre !isNull()
	 */
	void reason(Solver& s, Literal p, LitVec& lits) const {
		assert(!isNull());
		Type t = type();
		if (t == Generic) {
			constraint()->reason(s, p, lits);
			return;
		}
		lits.push_back(firstLiteral());
		if (t == Ternary) { lits.push_back(secondLiteral()); }
	}
	template <class S>
	bool minimize(S& s, Literal p, CCMinRecursive* rec) const {
		assert(!isNull());
		Type t = type();
		if (t == Generic) {
			return constraint()->minimize(s, p, rec);
		}
		return s.ccMinimize(firstLiteral(), rec) && (t != Ternary || s.ccMinimize(secondLiteral(), rec));
	}

	//! Returns true iff the antecedent refers to the constraint con.
	bool operator==(const Constraint* con) const {
		return static_cast<uintp>(data_) == reinterpret_cast<uintp>(con);
	}
	uint64  asUint() const { return data_; }
	uint64& asUint()       { return data_; }
private:
	uint64 data_;
};

enum { LBD_MAX = 127u, ACT_MAX = (1u << 20) - 1 };
//! Type storing a constraint's activity.
struct ConstraintScore {
	typedef ConstraintScore Score;
	enum { LBD_SHIFT = 20, BMP_BIT = 27, BITS_USED = 28, LBD_MASK = LBD_MAX<<LBD_SHIFT, BITS_MASK = (1u << BITS_USED) - 1  };
	void reset(uint32 act = 0, uint32 lbd = 0) {
		rep &= ~uint32(BITS_MASK);
		rep |= std::min(lbd, uint32(LBD_MAX)) << LBD_SHIFT;
		rep |= std::min(act, uint32(ACT_MAX));
	}
	uint32 activity() const  { return rep & uint32(ACT_MAX); }
	uint32 lbd()      const  { return hasLbd() ? uint32((rep & LBD_MASK) >> LBD_SHIFT) : uint32(LBD_MAX); }
	bool   hasLbd()   const  { return (rep & LBD_MASK) != 0; }
	bool   bumped()   const  { return test_bit(rep, BMP_BIT); }
	void   bumpActivity()    { if (activity() < uint32(ACT_MAX)) ++rep; }
	void   bumpLbd(uint32 x) { if (x < lbd()) { rep &= ~uint32(LBD_MASK); rep |= (x << LBD_SHIFT) | bit_mask<uint32>(BMP_BIT); } }
	void   clearBumped()     { store_clear_bit(rep, BMP_BIT); }
	void   reduce()          { clearBumped(); if (uint32 a = activity()) { rep &= ~uint32(ACT_MAX); rep |= (a>>1); } }
	void   assign(Score o)   { rep &= ~uint32(BITS_MASK); rep |= (o.rep & BITS_MASK); }
	uint32 rep;
};
inline ConstraintScore makeScore(uint32 act = 0, uint32 lbd = 0) {
	ConstraintScore sc = {0}; sc.reset(act, lbd);
	return sc;
}
//! Type storing meta information about a constraint.
class ConstraintInfo : private ConstraintScore {
public:
	typedef ConstraintInfo  Info;
	typedef ConstraintScore Score;
	ConstraintInfo(ConstraintType t = Constraint_t::Static) {
		static_assert(ConstraintScore::BITS_USED <= 28, "invalid score");
		rep = uint32(t) << TYPE_SHIFT;
	}
	using ConstraintScore::lbd;
	using ConstraintScore::activity;
	ConstraintType type()     const { return static_cast<ConstraintType>( (rep & uint32(TYPE_MASK)) >> TYPE_SHIFT ); }
	bool           tagged()   const { return test_bit(rep, TAG_BIT); }
	bool           aux()      const { return tagged() || test_bit(rep, AUX_BIT); }
	bool           learnt()   const { return type() != Constraint_t::Static; }
	const Score&   score()    const { return *this; }
	Score&         score()          { return *this; }

	Info& setType(ConstraintType t) { rep &= ~uint32(TYPE_MASK); rep |= (uint32(t) << TYPE_SHIFT); return *this; }
	Info& setScore(Score sc)        { assign(sc); return *this; }
	Info& setActivity(uint32 a)     { assign(makeScore(a, lbd())); return *this; }
	Info& setLbd(uint32 lbd)        { assign(makeScore(activity(), lbd)); return *this; }
	Info& setTagged(bool b)         { if (test_bit(rep, TAG_BIT) != b) store_toggle_bit(rep, TAG_BIT); return *this; }
	Info& setAux(bool b)            { if (test_bit(rep, AUX_BIT) != b) store_toggle_bit(rep, AUX_BIT); return *this; }
private:
	enum { TYPE_SHIFT = 28, AUX_BIT = 30, TAG_BIT = 31, TYPE_MASK = (3u << TYPE_SHIFT) };
};
//@}

/**
* \defgroup shared_con Shared
* \brief %Constraint data that can safely be shared between solvers.
* \ingroup constraint
*/

}
#endif
