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
#ifndef CLASP_SMODELS_CONSTRAINTS_H_INCLUDED
#define CLASP_SMODELS_CONSTRAINTS_H_INCLUDED

#ifdef _MSC_VER
#pragma once
#endif

#include <clasp/constraint.h>

namespace Clasp {

//! Primitive representation of weight constraint literals in normal form.
struct WeightLitsRep {
	//! Transforms the given literals to the normal form expected by WeightConstraint.
	/*!
	 * The function simplifies lits and bound by removing assigned and
	 * merging duplicate/complementary literals. Furthermore, negative weights and
	 * their literals are inverted, bound is updated accordingly, and literals
	 * are sorted by decreasing weight.
	 */
	static WeightLitsRep create(Solver& s, WeightLitVec& lits, weight_t bound);
	//! Propagates the constraint W == *this.
	/*!
	 * If *this is always satisfied (bound <= 0) or unsatisfied (bound > reach),
	 * the function forward propagates W. Otherwise, if W is not free, it assigns
	 * (and removes) literals from *this that must hold.
	 */
	bool propagate(Solver& s, Literal W);
	bool sat()        const { return bound <= 0; }
	bool unsat()      const { return reach < bound; }
	bool open()       const { return bound > 0 && bound <= reach;}
	bool hasWeights() const { return size && lits[0].second > 1;  }
	WeightLiteral* lits;  /*!< Literals sorted by decreasing weight. */
	uint32         size;  /*!< Number of literals in lits.  */
	weight_t       bound; /*!< Rhs of linear constraint.    */
	weight_t       reach; /*!< Sum of weights of lits.      */
};

//! Class implementing smodels-like cardinality- and weight constraints.
/*!
 * \ingroup constraint
 * This class represents a constraint of type W == w1*x1 ... wn*xn >= B,
 * where W and each xi are literals and B and each wi are strictly positive integers.
 *
 * The class is used to represent smodels-like weight constraint, i.e.
 * the body of a basic weight rule. In this case W is the literal associated with the body.
 * A cardinality constraint is handled like a weight constraint where all weights are equal to 1.
 *
 * <b>Given a WeightConstraint with bound \p B and set of literals \p L</b>,
 * -  let \p sumTrue be the sum of the weights of all literals \p l in \p L that are currently true,
 * - \p sumReach be the sum of the weights of all literals \p l in \p L that are currently not false, and
 * - \p U be the set of literals \p l in \p L that are currently unassigned
 * .
 * <b>the class implements the following four inference rules</b>:
 * - \b FTB: If \p sumTrue >= \p B: assign \p W to true.
 * - \b BFB: If \p W is false: set false all literals \p l in \p U for which \p sumTrue + weight(\p l) >= \p B.
 * - \b FFB: If \p sumReach < \p B: assign \p W to false.
 * - \b BTB: If \p W is true: set true all literals \p l in \p U for which \p sumReach - weight(\p l) < \p B.
 * .
 */
class WeightConstraint : public Constraint {
public:
	//! Flags controlling weight constraint creation.
	enum CreationFlags {
		create_explicit  = 1u, //!< Force creation of explicit constraint even if size/bound is small.
		create_no_add    = 3u, //!< Do not add constraint to solver db.
		create_sat       = 4u, //!< Force creation even if constraint is always satisfied.
		create_no_freeze = 8u, //!< Do not freeze variables in constraint.
		create_no_share  =16u, //!< Do not allow sharing of literals between threads.
		create_eq_bound  =32u, //!< Create equality instead of greater-or-equal constraint.
		create_only_btb  =64u, //!< Only create FFB_BTB constraint.
		create_only_bfb  =128u,//!< Only create FTB_BFB constraint.
	};
	//! Type used to communicate result of create().
	class CPair {
	public:
		CPair() { con[0] = con[1] = 0; }
		bool              ok()    const { return con[0] != (WeightConstraint*)0x1 && con[1] != (WeightConstraint*)0x1; }
		WeightConstraint* first() const { return con[0]; }
		WeightConstraint* second()const { return con[1]; }
	private:
		friend class WeightConstraint;
		WeightConstraint* con[2];
	};
	//! Creates a new weight constraint from the given weight literals.
	/*!
	 * If the right hand side of the weight constraint is initially true/false (FTB/FFB),
	 * W is assigned appropriately but no constraint is created. Otherwise,
	 * the new weight constraint is added to s unless creationFlags contains create_no_add.
	 *
	 * \param s Solver in which the new constraint is to be used.
	 * \param W The literal that is associated with the constraint.
	 * \param lits The literals of the weight constraint.
	 * \param bound The lower bound of the weight constraint.
	 * \param creationFlags Set of CreationFlags to apply.
	 * \note Cardinality constraint are represented as weight constraints with all weights equal to 1.
	 * \note If creationFlags contains create_eq_bound, a constraint W == (lits == bound) is created that
	 * is represented by up to two weight constraints.
	 */
	static CPair create(Solver& s, Literal W, WeightLitVec& lits, weight_t bound, uint32 creationFlags = 0);

	//! Low level creation function.
	/*!
	 * \note flag create_eq_bound is ignored by this function, that is, this function always creates
	 * a single >= constraint.
	 */
	static CPair create(Solver& s, Literal W, WeightLitsRep& rep, uint32 flags);
	// constraint interface
	Constraint* cloneAttach(Solver&);
	bool simplify(Solver& s, bool = false);
	void destroy(Solver*, bool);
	PropResult propagate(Solver& s, Literal p, uint32& data);
	void reason(Solver&, Literal p, LitVec& lits);
	bool minimize(Solver& s, Literal p, CCMinRecursive* r);
	void undoLevel(Solver& s);
	uint32 estimateComplexity(const Solver& s) const;
	/*!
	 * Logically, we distinguish two constraints:
	 * - FFB_BTB for handling forward false body and backward true body and
	 * - FTB_BFB for handling forward true body and backward false body.
	 * .
	 * Physically, we store the literals in one array: ~W=1, l0=w0,...,ln-1=wn-1.
	 */
	enum ActiveConstraint {
		FFB_BTB   = 0, //!< (\p SumW - \p B)+1 [~W=1, l0=w0,..., ln-1=wn-1]
		FTB_BFB   = 1, //!< \p B [ W=1,~l0=w0,...,~ln-1=wn-1]
	};
	/*!
	 * Returns the i'th literal of constraint c, i.e.
	 * - li, iff c == FFB_BTB
	 * - ~li, iff c == FTB_BFB.
	 */
	Literal  lit(uint32 i, ActiveConstraint c) const { return Literal::fromId( lits_->lit(i).id() ^ c ); }
	//! Returns the weight of the i'th literal or 1 if constraint is a cardinality constraint.
	weight_t weight(uint32 i)                  const { return lits_->weight(i); }
	//! Returns the number of literals in this constraint (including W).
	uint32   size()                            const { return lits_->size();    }
	//! Returns false if constraint is a cardinality constraint.
	bool     isWeight()                        const { return lits_->weights(); }
	// Returns the index of next literal to look at during backward propagation.
	uint32   getBpIndex()                      const { return !isWeight() ? 1 : undo_[0].data>>1; }
private:
	static WeightConstraint* doCreate(Solver& s, Literal W, WeightLitsRep& rep, uint32 flags);
	bool                     integrateRoot(Solver& s);
	struct WL {
		WL(uint32 s, bool shared, bool w);
		bool     shareable()      const { return rc != 0; }
		bool     unique()         const { return rc == 0 || refCount() == 1; }
		bool     weights()        const { return w != 0; }
		uint32   size()           const { return sz; }
		Literal  lit(uint32 i)    const { return lits[(i<<w)]; }
		Var      var(uint32 i)    const { return lits[(i<<w)].var(); }
		weight_t weight(uint32 i) const { return !weights() ? weight_t(1) : (weight_t)lits[(i<<1)+1].rep(); }
		uint32   refCount()       const;
		WL*      clone();
		void     release();
		uint8*   address();
		uint32  sz : 30; // number of literals
		uint32  rc : 1;  // ref counted?
		uint32  w  : 1;  // has weights?
POTASSCO_WARNING_BEGIN_RELAXED
		Literal lits[0]; // Literals of constraint: ~B [Bw], l1 [w1], ..., ln-1 [Wn-1]
POTASSCO_WARNING_END_RELAXED
	};
	WeightConstraint(Solver& s, SharedContext* ctx, Literal W, const WeightLitsRep& , WL* out, uint32 act = 3u);
	WeightConstraint(Solver& s, const WeightConstraint& other);
	~WeightConstraint();

	static const uint32 NOT_ACTIVE = 3u;

	// Represents a literal on the undo stack.
	// idx()        returns the index of the literal.
	// constraint() returns the constraint that added the literal to the undo stack.
	// Note: Only 31-bits are used for undo info.
	// The remaining bit is used as a flag for marking processed literals.
	struct UndoInfo {
		explicit UndoInfo(uint32 d = 0) : data(d) {}
		uint32           idx()        const { return data >> 2; }
		ActiveConstraint constraint() const { return static_cast<ActiveConstraint>((data&2) != 0); }
		uint32 data;
	};
	// Is literal idx contained as reason lit in the undo stack?
	bool litSeen(uint32 idx) const { return (undo_[idx].data & 1) != 0; }
	// Mark/unmark literal idx.
	void toggleLitSeen(uint32 idx) { undo_[idx].data ^= 1; }
	// Add watch for idx'th literal of c to the solver.
	void addWatch(Solver& s, uint32 idx, ActiveConstraint c);
	// Updates bound_[c] and adds an undo watch to the solver if necessary.
	// Then adds the literal at position idx to the reason set (and the undo stack).
	void updateConstraint(Solver& s, uint32 level, uint32 idx, ActiveConstraint c);
	// Returns the starting index of the undo stack.
	uint32   undoStart()       const { return isWeight(); }
	UndoInfo undoTop()         const { assert(up_ != undoStart()); return undo_[up_-1]; }
	// Returns the decision level of the last assigned literal
	// or 0 if no literal was assigned yet.
	uint32   highestUndoLevel(Solver&) const;
	void     setBpIndex(uint32 n);
	WL*      lits_;        // literals of constraint
	uint32   up_     : 27; // undo position; [undoStart(), up_) is the undo stack
	uint32   ownsLit_:  1; // owns lits_?
	uint32   active_ :  2; // which of the two sub-constraints is currently unit?
	uint32   watched_:  2; // which constraint is watched (3 both, 2 ignore, FTB_BFB, FFB_BTB)
	weight_t bound_[2];    // FFB_BTB: (sumW-bound)+1 / FTB_BFB: bound
POTASSCO_WARNING_BEGIN_RELAXED
	UndoInfo undo_[0];     // undo stack + seen flag for each literal
POTASSCO_WARNING_END_RELAXED
};
}

#endif
