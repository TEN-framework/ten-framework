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
#ifndef CLASP_HEURISTICS_H_INCLUDED
#define CLASP_HEURISTICS_H_INCLUDED

#ifdef _MSC_VER
#pragma once
#endif

/*!
 * \file
 * \brief Defines various decision heuristics to be used in clasp.
 */

#include <clasp/solver.h>
#include <clasp/pod_vector.h>
#include <clasp/util/indexed_priority_queue.h>
#include <list>
namespace Clasp {

//! Computes a moms-like score for var v.
uint32 momsScore(const Solver& s, Var v);

//! A variant of the BerkMin decision heuristic from the BerkMin Sat-Solver.
/*!
 * \ingroup heuristic
 * \see E. Goldberg, Y. Navikov: "BerkMin: a Fast and Robust Sat-Solver"
 *
 * \note
 * This version of the BerkMin heuristic varies mostly in the following points from the original BerkMin:
 *  -# it considers loop-nogoods if requested (this is the default)
 *  -# it uses a MOMS-like heuristic as long as there are no conflicts (and therefore no activities)
 *  -# it uses a MOMS-like score to break ties whenever multiple variables from an unsatisfied learnt constraint have equal activities
 *  -# it uses a lazy decaying scheme that only touches active variables
 */
class ClaspBerkmin : public DecisionHeuristic {
public:
	/*!
	 * \note Checks at most params.param candidates when searching for not yet
	 * satisfied learnt constraints. If param is 0, all candidates are checked.
	 */
	explicit ClaspBerkmin(const HeuParams& params = HeuParams());
	void setConfig(const HeuParams& params);
	void startInit(const Solver& s);
	void endInit(Solver& s);
	void newConstraint(const Solver& s, const Literal* first, LitVec::size_type size, ConstraintType t);
	void updateReason(const Solver& s, const LitVec& lits, Literal resolveLit);
	bool bump(const Solver& s, const WeightLitVec& lits, double adj);
	void undoUntil(const Solver&, LitVec::size_type);
	void updateVar(const Solver& s, Var v, uint32 n);
protected:
	Literal doSelect(Solver& s);
private:
	Literal selectLiteral(Solver& s, Var v, bool vsids) const;
	Literal selectRange(Solver& s, const Literal* first, const Literal* last);
	bool    initHuang() const { return order_.score[0].occ == 1; }
	void    initHuang(bool b) { order_.score[0].occ = b; }
	bool    hasActivities() const { return order_.score[0].act != 0; }
	void    hasActivities(bool b) { order_.score[0].act = b; }
	Var  getMostActiveFreeVar(const Solver& s);
	Var  getTopMoms(const Solver& s);
	bool hasTopUnsat(Solver& s);
	// Gathers heuristic information for one variable v.
	struct HScore {
		HScore(uint32 d = 0) :  occ(0), act(0), dec(uint16(d)) {}
		void incAct(uint32 gd, bool h, bool sign) {
			occ += int(h) * (1 - (2*int(sign)));
			decay(gd, h);
			++act;
		}
		void incOcc(bool sign) { occ += 1 - (2*int(sign)); }
		uint32 decay(uint32 gd, bool h) {
			if (uint32 x = (gd-dec)) {
				// NOTE: shifts might overflow, i.e.
				// activity is actually shifted by x%32.
				// We deliberately ignore this "logical inaccuracy"
				// and treat it as random noise ;)
				act >>= x;
				dec   = uint16(gd);
				occ  /= (1<<(x*int(h)));
			}
			return act;
		}
		int32  occ;
		uint16 act;
		uint16 dec;
	};
	typedef PodVector<HScore>::type   Scores;
	typedef VarVec::iterator Pos;

	struct Order {
		explicit Order() : decay(0), huang(false), resScore(3u) {}
		struct Compare {
			explicit Compare(Order* o) : self(o) {}
			bool operator()(Var v1, Var v2) const {
				return self->decayedScore(v1) > self->decayedScore(v2)
					|| (self->score[v1].act == self->score[v2].act && v1 < v2);
			}
			Order* self;
		};
		uint32  decayedScore(Var v) { return score[v].decay(decay, huang); }
		int32   occ(Var v)   const  { return score[v].occ; }
		void    inc(Literal p, bool inNant) {
			if (!this->nant || inNant) {
				score[p.var()].incAct(decay, huang, p.sign());
			}
		}
		void    incOcc(Literal p) { score[p.var()].incOcc(p.sign()); }
		int     compare(Var v1, Var v2) {
			return int(decayedScore(v1)) - int(decayedScore(v2));
		}
		void    resetDecay();
		Scores  score;        // For each var v score_[v] stores heuristic score of v
		uint32  decay;        // "global" decay counter. Increased every decP_ decisions
		bool    huang;        // Use Huang's scoring scheme (see: Jinbo Huang: "A Case for Simple SAT Solvers")
		bool    nant;         // only score vars v with varInfo(v).nant()?
		uint8   resScore;
	private:
		Order(const Order&);
		Order& operator=(const Order&);
	}       order_;
	VarVec  cache_;         // Caches the most active variables
	LitVec  freeLits_;      // Stores free variables of the last learnt conflict clause that is not sat
	LitVec  freeOtherLits_; // Stores free variables of the last other learnt nogood that is not sat
	uint32  topConflict_;   // Index into the array of learnt nogoods used when searching for conflict clauses that are not sat
	uint32  topOther_;      // Index into the array of learnt nogoods used when searching for other learnt nogoods that are not sat
	Var     front_;         // First variable whose truth-value is not already known - reset on backtracking
	Pos     cacheFront_;    // First unprocessed cache position - reset on backtracking
	uint32  cacheSize_;     // Cache at most cacheSize_ variables
	uint32  numVsids_;      // Number of consecutive vsids-based decisions
	uint32  maxBerkmin_;    // When searching for an open learnt constraint, check at most maxBerkmin_ candidates.
	TypeSet types_;         // When searching for an open learnt constraint, consider these types of nogoods.
	RNG     rng_;
};

//! Variable Move To Front decision strategies inspired by Siege.
/*!
 * \ingroup heuristic
 * \see Lawrence Ryan: "Efficient Algorithms for Clause Learning SAT-Solvers"
 *
 * \note This implementation of VMTF differs from the original implementation in three points:
 *  - it optionally moves to the front a selection of variables from learnt loop nogoods
 *  - it measures variable activity by using a BerkMin-like score scheme
 *  - the initial order of the var list is determined using a MOMs-like score scheme
 */
class ClaspVmtf : public DecisionHeuristic {
public:
	/*!
	 * \note Moves at most params.param literals from constraints used during
	 *  conflict resolution to the front. If params.param is 0, the default is
	 *  to move up to 8 literals.
	 */
	explicit ClaspVmtf(const HeuParams& params = HeuParams());
	void setConfig(const HeuParams& params);
	void startInit(const Solver& s);
	void endInit(Solver&);
	void newConstraint(const Solver& s, const Literal* first, LitVec::size_type size, ConstraintType t);
	void updateReason(const Solver& s, const LitVec& lits, Literal resolveLit);
	bool bump(const Solver& s, const WeightLitVec& lits, double adj);
	void simplify(const Solver&, LitVec::size_type);
	void undoUntil(const Solver&, LitVec::size_type);
	void updateVar(const Solver& s, Var v, uint32 n);
protected:
	Literal doSelect(Solver& s);
private:
	Literal selectRange(Solver& s, const Literal* first, const Literal* last);
	typedef std::list<Var> VarList;
	typedef VarList::iterator VarPos;
	struct VarInfo {
		VarInfo(VarPos it) : pos_(it), activity_(0), occ_(0), decay_(0) { }
		VarPos  pos_;       // position of var in var list
		uint32  activity_;  // activity of var - initially 0
		int32   occ_;       // which literal of var occurred more often in learnt constraints?
		uint32  decay_;     // counter for lazy decaying activity
		uint32& activity(uint32 globalDecay) {
			if (uint32 x = (globalDecay - decay_)) {
				activity_ >>= (x<<1);
				decay_ = globalDecay;
			}
			return activity_;
		}
	};
	typedef PodVector<VarInfo>::type Score;

	struct LessLevel {
		LessLevel(const Solver& s, const Score& sc) : s_(s), sc_(sc) {}
		bool operator()(Var v1, Var v2) const {
			return s_.level(v1) < s_.level(v2)
				|| (s_.level(v1) == s_.level(v2) && sc_[v1].activity_ > sc_[v2].activity_);
		}
		bool operator()(Literal l1, Literal l2) const {
			return (*this)(l1.var(), l2.var());
		}
	private:
		LessLevel& operator=(const LessLevel&);
		const Solver& s_;
		const Score&  sc_;
	};
	Score   score_; // For each var v score_[v] stores heuristic score of v
	VarList vars_;  // List of possible choices, initially ordered by MOMs-like score
	VarVec  mtf_;   // Vars to be moved to the front of vars_
	VarPos  front_; // Current front-position in var list - reset on backtracking
	uint32  decay_; // "Global" decay counter. Increased every 512 decisions
	uint32  nMove_; // Limit on number of vars to move
	TypeSet types_; // Type of nogoods to score during resolution
	uint32  scType_;// Type of scoring
	bool    nant_;  // only move vars v with varInfo(v).nant()?
};

//! Score type for VSIDS heuristic.
/*!
 * \see ClaspVsids
 */
struct VsidsScore {
	typedef VsidsScore SC;
	VsidsScore(double sc = 0.0) : value(sc) {}
	double get()                  const { return value; }
	bool   operator>(const SC& o) const { return value > o.value; }
	void   set(double f)                { value = f; }
	template <class C>
	static double applyFactor(C&, Var, double f) { return f; }
	double value; // activity
};

//! A variable state independent decision heuristic favoring variables that were active in recent conflicts.
/*!
 * \ingroup heuristic
 * \see M. W. Moskewicz, C. F. Madigan, Y. Zhao, L. Zhang, and S. Malik:
 * "Chaff: Engineering an Efficient SAT Solver"
 *
 * \note By default, the implementation uses the exponential VSIDS scheme from MiniSAT and
 * applies a MOMs-like score scheme to get an initial var order.
 */
template <class ScoreType>
class ClaspVsids_t : public DecisionHeuristic {
public:
	/*!
	 * \note Uses params.param to init the decay value d and inc factor 1.0/d.
	 * If params.param is 0, d is set 0.95. Otherwise, d is set to 0.x, where x is params.param.
	 */
	explicit ClaspVsids_t(const HeuParams& params = HeuParams());
	virtual void setConfig(const HeuParams& params);
	virtual void startInit(const Solver& s);
	virtual void endInit(Solver&);
	virtual void newConstraint(const Solver& s, const Literal* first, LitVec::size_type size, ConstraintType t);
	virtual void updateReason(const Solver& s, const LitVec& lits, Literal resolveLit);
	virtual bool bump(const Solver& s, const WeightLitVec& lits, double adj);
	virtual void undoUntil(const Solver&, LitVec::size_type);
	virtual void simplify(const Solver&, LitVec::size_type);
	virtual void updateVar(const Solver& s, Var v, uint32 n);
protected:
	virtual Literal doSelect(Solver& s);
	virtual Literal selectRange(Solver& s, const Literal* first, const Literal* last);
	virtual void    initScores(Solver& s, bool moms);
	typedef typename PodVector<ScoreType>::type ScoreVec;
	typedef PodVector<int32>::type              OccVec;
	void updateVarActivity(const Solver& s, Var v, double f = 1.0);
	void incOcc(Literal p) { occ_[p.var()] += 1 - (int(p.sign()) << 1); }
	int  occ(Var v) const  { return occ_[v]; }
	void normalize();
	struct CmpScore {
		explicit CmpScore(const ScoreVec& s) : sc_(s) {}
		bool operator()(Var v1, Var v2) const { return sc_[v1] > sc_[v2]; }
	private:
		CmpScore& operator=(const CmpScore&);
		const ScoreVec& sc_;
	};
	typedef bk_lib::indexed_priority_queue<CmpScore> VarOrder;
	struct Decay : Range<double>{
		Decay(double x = 0.0, double y = 0.95, uint32 b = 0, uint32 f = 0) : Range<double>(x, y), bump(b), freq(f), next(f) {
			this->df = 1.0 / (freq && lo > 0.0 ? lo : hi);
		}
		double df; // active decay factor for evsids (>= 1.0)
		uint32 bump;
		uint32 freq : 16;
		uint32 next : 16;
	};
	ScoreVec score_; // vsids score for each variable
	OccVec   occ_;   // occurrence balance of each variable
	VarOrder vars_;  // priority heap of variables
	Decay    decay_; // (dynamic) decaying strategy
	double   inc_;   // var bump for evsids or conflict index for acids (increased on conflict)
	TypeSet  types_; // set of constraints to score
	uint32   scType_;// score type (one of HeuParams::Score)
	bool     acids_; // whether to use acids instead if evsids scoring
	bool     nant_;  // whether bumps are restricted to vars v with varInfo(v).nant()
};
typedef ClaspVsids_t<VsidsScore> ClaspVsids;

//! Score type for DomainHeuristic.
/*!
 * \see DomainHeuristic
 */
struct DomScore : VsidsScore {
	static const uint32 domMax = (1u << 30) - 1;
	typedef DomScore SC;
	DomScore(double v = 0.0) : VsidsScore(v), level(0), factor(1), domP(domMax), sign(0), init(0) {}
	bool   operator>(const SC& o) const { return (level > o.level) || (level == o.level && value > o.value); }
	bool   isDom()                const { return domP != domMax; }
	void   setDom(uint32 key)           { domP = key; }
	template <class C>
	static double applyFactor(C& sc, Var v, double f) {
		int16 df = sc[v].factor;
		return df == 1 ? f : static_cast<double>(df) * f;
	}
	int16  level;     // priority level
	int16  factor;    // factor used when bumping activity
	uint32 domP : 30; // index into dom-table if dom var
	uint32 sign :  1; // whether v has a sign modification
	uint32 init :  1; // whether value is from init modification
};

//! A VSIDS heuristic supporting additional domain knowledge.
/*!
 * \ingroup heuristic
 *
 * \see M. Gebser, B. Kaufmann, R. Otero, J. Romero, T. Schaub, P. Wanko:
 *      "Domain-specific Heuristics in Answer Set Programming",
 *      http://www.cs.uni-potsdam.de/wv/pdfformat/gekaotroscwa13a.pdf
 */
class DomainHeuristic : public ClaspVsids_t<DomScore>
                      , private Constraint {
public:
	typedef ClaspVsids_t<DomScore>  BaseType;
	explicit DomainHeuristic(const HeuParams& params = HeuParams());
	~DomainHeuristic();
	void setDefaultMod(HeuParams::DomMod mod, uint32 prefSet);
	virtual void setConfig(const HeuParams& params);
	virtual void startInit(const Solver& s);
	const DomScore& score(Var v) const { return score_[v]; }
protected:
	// base interface
	virtual Literal     doSelect(Solver& s);
	virtual void        initScores(Solver& s, bool moms);
	virtual void        detach(Solver& s);
	// Constraint interface
	virtual Constraint* cloneAttach(Solver&) { return 0; }
	virtual void        reason(Solver&, Literal, LitVec&){}
	virtual PropResult  propagate(Solver&, Literal, uint32&);
	virtual void        undoLevel(Solver& s);
private:
	struct DomAction {
		static const uint32 UNDO_NIL = (1u << 31) - 1;
		uint32 var:30; // dom var to be modified
		uint32 mod: 2; // modification to apply
		uint32 undo:31;// next action in undo list
		uint32 next: 1;// next action belongs to same condition?
		int16  bias;   // value to apply
		uint16 prio;   // prio of modification
	};
	struct DomPrio {
		void clear() { prio[0] = prio[1] = prio[2] = prio[3] = 0; }
		uint16  operator[](unsigned i) const { return prio[i]; }
		uint16& operator[](unsigned i)       { return prio[i]; }
		uint16  prio[4];
	};
	struct Frame {
		Frame(uint32 lev, uint32 h) : dl(lev), head(h) {}
		uint32 dl;
		uint32 head;
	};
	typedef PodVector<DomAction>::type ActionVec;
	typedef PodVector<DomPrio>::type   PrioVec;
	typedef PodVector<Frame>::type     FrameVec;
	typedef DomainTable::ValueType     DomMod;
	typedef PodVector<std::pair<Var, double> >::type VarScoreVec;

	uint32  addDomAction(const DomMod& e, Solver& s,  VarScoreVec& outInit, Literal& lastW);
	void    addDefAction(Solver& s, Literal x, int16 lev, uint32 domKey);
	void    pushUndo(uint32& head, uint32 actionId);
	void    applyAction(Solver& s, DomAction& act, uint16& oldPrio);
	uint16& prio(Var v, uint32 mod) { return prios_[score_[v].domP][mod]; }
	PrioVec   prios_;    // priorities for domain vars
	ActionVec actions_;  // dynamic modifications
	FrameVec  frames_;   // dynamic undo information
	uint32    domSeen_;  // offset into domain table
	uint32    defMax_;   // max var with default modification
	uint16    defMod_;   // default modifier
	uint16    defPref_;  // default preferences
};
}
#endif
