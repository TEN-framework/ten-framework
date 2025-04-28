//
// Copyright (c) 2009-2017 Benjamin Kaufmann
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
#ifndef CLASP_LOOKAHEAD_H_INCLUDED
#define CLASP_LOOKAHEAD_H_INCLUDED

#ifdef _MSC_VER
#pragma once
#endif

/*!
 * \file
 * \brief Defines lookahead related types.
 *
 * Lookahead can be used as a post propagator (e.g. failed-literal detection) and
 * optionally as an heuristic.
 */

#include <clasp/solver.h>
#include <clasp/constraint.h>
namespace Clasp {
/*!
 * \addtogroup propagator
 */
//@{

//! Type used to store lookahead-information for one variable.
struct VarScore {
	VarScore() { clear(); }
	void clear() { std::memset(this, 0, sizeof(VarScore)); }
	//! Mark literal p as dependent.
	void setSeen( Literal p )    { seen_ |= uint32(p.sign()) + 1; }
	//! Is literal p dependent?
	bool seen(Literal p) const   { return (seen_ & (uint32(p.sign())+1)) != 0; }
	//! Is this var dependent?
	bool seen()          const   { return seen_ != 0; }
	//! Mark literal p as tested during lookahead.
	void setTested( Literal p )  { tested_ |= uint32(p.sign()) + 1; }
	//! Was literal p tested during lookahead?
	bool tested(Literal p) const { return (tested_ & (uint32(p.sign())+1)) != 0; }
	//! Was some literal of this var tested?
	bool tested()          const { return tested_ != 0; }
	//! Were both literals of this var tested?
	bool testedBoth()      const { return tested_  == 3; }

	//! Sets the score for literal p to value and marks p as tested.
	void setScore(Literal p, LitVec::size_type value) {
		if (value > (1U<<14)-1) value = (1U<<14)-1;
		if (p.sign()) nVal_ = uint32(value);
		else          pVal_ = uint32(value);
		setTested(p);
	}
	//! Sets the score of a dependent literal p to min(sc, current score).
	void setDepScore(Literal p, uint32 sc) {
		if (!seen(p) || score(p) > sc) {
			if (sc > (1U<<14)-1) sc = (1U<<14)-1;
			if (p.sign()) nVal_ = std::min(uint32(nVal_-(nVal_==0)), sc);
			else          pVal_ = std::min(uint32(pVal_-(pVal_==0)), sc);
		}
	}
	//! Returns the score for literal p.
	uint32 score(Literal p) const { return p.sign() ? nVal_ : pVal_; }
	//! Returns the scores of the two literals of a variable.
	/*!
	 * \param[out] mx The maximum score.
	 * \param[out] mn The minimum score.
	 */
	void score(uint32& mx, uint32& mn) const {
		if (nVal_ > pVal_) {
			mx = nVal_;
			mn = pVal_;
		}
		else {
			mx = pVal_;
			mn = nVal_;
		}
	}
	//! Returns the sign of the literal that has the higher score.
	bool prefSign() const { return nVal_ > pVal_; }

	uint32 nVal() const { return nVal_; }
	uint32 pVal() const { return pVal_; }
private:
	uint32 pVal_  : 14;
	uint32 nVal_  : 14;
	uint32 seen_  : 2;
	uint32 tested_: 2;
};

//! A small helper class used to score the result of a lookahead operation.
struct ScoreLook {
	enum Mode { score_max, score_max_min };
	typedef PodVector<VarScore>::type VarScores; /**< A vector of variable-scores */
	ScoreLook() : best(0), mode(score_max), addDeps(true), nant(false) {}
	bool    validVar(Var v) const { return v < score.size(); }
	void    scoreLits(const Solver& s, const Literal* b, const Literal *e);
	void    clearDeps();
	uint32  countNant(const Solver& s, const Literal* b, const Literal *e) const;
	bool    greater(Var lhs, Var rhs)const;
	bool    greaterMax(Var x, uint32 max) const {
		return score[x].nVal() > max || score[x].pVal() > max;
	}
	bool    greaterMaxMin(Var lhs, uint32 max, uint32 min) const {
		uint32 lhsMin, lhsMax;
		score[lhs].score(lhsMax, lhsMin);
		return lhsMin > min || (lhsMin == min && lhsMax > max);
	}
	VarScores score;  //!< score[v] stores lookahead score of v
	VarVec    deps;   //!< Tested vars and those that follow from them.
	VarType   types;  //!< Var types to consider.
	Var       best;   //!< Var with best score among those in deps.
	Mode      mode;   //!< Score mode to apply.
	bool      addDeps;//!< Add/score dependent vars?
	bool      nant;   //!< Score only atoms in NegAnte(P)?
};

class UnitHeuristic;

//! Lookahead extends propagation with failed-literal detection.
/*!
 * The class provides different kinds of one-step lookahead.
 * Atom- and body-lookahead are uniform lookahead types, where
 * either only atoms or bodies are tested. Hybrid-lookahead tests
 * both types of vars but each only in a single phase. I.e. atoms
 * are only tested negatively while bodies are tested positively.
 */
class Lookahead : public PostPropagator {
public:
	//! Set of parameters to configure lookahead.
	struct Params {
		Params(VarType t = Var_t::Atom) : type(t), lim(0), topLevelImps(true), restrictNant(false) {}
		Params& lookahead(VarType t){ type         = t; return *this; }
		Params& addImps(bool b)     { topLevelImps = b; return *this; }
		Params& nant(bool b)        { restrictNant = b; return *this; }
		Params& limit(uint32 x)     { lim = x;          return *this; }
		VarType type;
		uint32  lim;
		bool    topLevelImps;
		bool    restrictNant;
	};
	static bool isType(uint32 t) { return t != 0 && t <= Var_t::Hybrid; }
	/*!
	 * \param p Lookahead parameters to use.
	 */
	explicit Lookahead(const Params& p);
	~Lookahead();

	bool    init(Solver& s);
	//! Clears the lookahead list.
	void    clear();
	//! Returns true if lookahead list is empty.
	bool    empty() const { return head()->next == head_id; }
	//! Adds literal p to the lookahead list.
	void    append(Literal p, bool testBoth);
	//! Executes a single-step lookahead on all vars in the loookahead list.
	bool    propagateFixpoint(Solver& s, PostPropagator*);
	//! Returns PostPropagator::priority_reserved_look.
	uint32  priority() const;
	void    destroy(Solver* s, bool detach);
	ScoreLook score; //!< State of last lookahead operation.
	//! Returns "best" literal w.r.t scoring of last lookahead or lit_true() if no such literal exists.
	Literal heuristic(Solver& s);
	void    detach(Solver& s);
	bool    hasLimit() const { return limit_ != 0; }
protected:
	bool propagateLevel(Solver& s); // called by propagate
	void undoLevel(Solver& s);
	bool test(Solver& s, Literal p);
private:
	typedef uint32 NodeId;
	enum { head_id = NodeId(0), undo_id = NodeId(1) };
	struct LitNode {
		LitNode(Literal x) : lit(x), next(UINT32_MAX) {}
		Literal  lit;
		NodeId   next;
	};
	typedef PodVector<NodeId>::type  UndoStack;
	typedef PodVector<LitNode>::type LookList;
	typedef UnitHeuristic*           HeuPtr;
	void splice(NodeId n);
	LitNode* node(NodeId n) { return &nodes_[n]; }
	LitNode* head()         { return &nodes_[head_id]; } // head of circular candidate list
	LitNode* undo()         { return &nodes_[undo_id]; } // head of undo list
	bool     checkImps(Solver& s, Literal p);
	const LitNode* head() const { return &nodes_[head_id]; }
	LookList   nodes_; // list of literals to test
	UndoStack  saved_; // stack of undo lists
	LitVec     imps_;  // additional top-level implications
	NodeId     last_;  // last candidate in list; invariant: node(last_)->next == head_id;
	NodeId     pos_;   // current lookahead start position
	uint32     top_;   // size of top-level
	uint32     limit_; // stop lookahead after this number of applications
};
//@}

//! Heuristic that uses the results of lookahead.
/*!
 * \ingroup heuristic
 * The heuristic uses a Lookahead post propagator to select a literal with the highest score,
 * where the score is determined by counting assignments made during
 * failed-literal detection. For hybrid_lookahead, the heuristic selects the literal that
 * derived the most literals. On the other hand, for uniform_lookahead the heuristic is similar to
 * the smodels lookahead heuristic and selects the literal that maximizes the minimum.
 * \see Patrik Simons: "Extending and Implementing the Stable Model Semantics" for a
 * detailed description of the lookahead heuristic.
 *
 * \note The heuristic might itself apply some lookahead but only on variables that
 *       did not fail in a previous call to Lookahead::propagateFixpoint(). I.e. if
 *       priorities are correct for all post propagators in s, the lookahead operations can't fail.
 *
 * \note If no Lookahead post propagator exists in the solver, the heuristic selects the first free variable!
 */
class UnitHeuristic : public SelectFirst {
public:
	UnitHeuristic();
	//! Decorates the heuristic given in other with temporary lookahead.
	static UnitHeuristic* restricted(DecisionHeuristic* other);
	void    endInit(Solver& /* s */);
	Literal doSelect(Solver& s);
};

}
#endif
