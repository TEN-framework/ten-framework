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
#ifndef CLASP_SOLVER_STRATEGIES_H_INCLUDED
#define CLASP_SOLVER_STRATEGIES_H_INCLUDED
#ifdef _MSC_VER
#pragma once
#endif

#include <clasp/constraint.h>
#include <clasp/util/misc_types.h>

#if !defined(CLASP_ALIGN_BITFIELD)
#	if defined(EMSCRIPTEN)
// Force alignment of bitfield to T in order to prevent
// code-generation bug in emcc
// see: https://github.com/kripken/emscripten/issues/4540
#	define CLASP_ALIGN_BITFIELD(T) T : 0;
#	else
#	define CLASP_ALIGN_BITFIELD(T)
#	endif
#endif

/*!
 * \file
 * \brief Contains strategies and options used to configure solvers and search.
 */
namespace Clasp {
//! Implements clasp's configurable schedule-strategies.
/*!
 * clasp currently supports the following basic strategies:
 *  - geometric sequence  : X = n1 * n2^k   (k >= 0)
 *  - arithmetic sequence : X = n1 + (n2*k) (k >= 0)
 *  - fixed sequence      : X = n1 + (0*k)  (k >= 0)
 *  - luby's sequence     : X = n1 * luby(k)(k >= 0)
 *  .
 * Furthermore, an inner-outer scheme can be applied to the selected sequence.
 * In that case, the sequence is repeated every \<limit\>+j restarts, where
 * \<limit\> is the initial outer-limit and j is the number of times the
 * sequence was already repeated.
 *
 * \note For luby's seqeuence, j is not a repetition counter
 * but the index where the sequence grows to the next power of two.
 *
 * \see Luby et al. "Optimal speedup of las vegas algorithms."
 *
 */
struct ScheduleStrategy {
public:
	//! Supported strategies.
	enum Type { Geometric = 0, Arithmetic = 1, Luby = 2, User = 3 };

	ScheduleStrategy(Type t = Geometric, uint32 b = 100, double g = 1.5, uint32 o = 0);
	//! Creates luby's sequence with unit-length unit and optional outer limit.
	static ScheduleStrategy luby(uint32 unit, uint32 limit = 0)              { return ScheduleStrategy(Luby, unit, 0, limit);  }
	//! Creates geometric sequence base * (grow^k) with optional outer limit.
	static ScheduleStrategy geom(uint32 base, double grow, uint32 limit = 0) { return ScheduleStrategy(Geometric, base, grow, limit);  }
	//! Creates arithmetic sequence base + (add*k) with optional outer limit.
	static ScheduleStrategy arith(uint32 base, double add, uint32 limit = 0) { return ScheduleStrategy(Arithmetic, base, add, limit);  }
	//! Creates fixed sequence with length base.
	static ScheduleStrategy fixed(uint32 base)                               { return ScheduleStrategy(Arithmetic, base, 0, 0);  }
	static ScheduleStrategy none()                                           { return ScheduleStrategy(Geometric, 0); }
	static ScheduleStrategy def()                                            { return ScheduleStrategy(User, 0, 0.0); }
	uint64 current()  const;
	bool   disabled() const { return base == 0; }
	bool   defaulted()const { return base == 0 && type == User; }
	void   reset()          { idx  = 0;         }
	uint64 next();
	void   advanceTo(uint32 idx);
	uint32 base : 30; // base of sequence (n1)
	uint32 type :  2; // type of basic sequence
	uint32 idx;       // current index into sequence
	uint32 len;       // length of sequence (0 if infinite) (once reached, sequence is repeated and len increased)
	float  grow;      // update parameter n2
};
//! Returns the idx'th value of the luby sequence.
uint32 lubyR(uint32 idx);
//! Returns the idx'th value of the geometric sequence with the given growth factor.
double growR(uint32 idx, double g);
//! Returns the idx'th value of the arithmetic sequence with the given addend.
double addR(uint32 idx, double a);

class DecisionHeuristic;
//! Parameter-Object for grouping solver strategies.
struct SolverStrategies {
	//! Clasp's two general search strategies.
	enum SearchStrategy {
		use_learning = 0, //!< Analyze conflicts and learn First-1-UIP-clause.
		no_learning  = 1  //!< Don't analyze conflicts - chronological backtracking.
	};
	//! Default sign heuristic.
	enum SignHeu {
		sign_atom = 0, //!< Prefer negative literal for atoms.
		sign_pos  = 1, //!< Prefer positive literal.
		sign_neg  = 2, //!< Prefer negative literal.
		sign_rnd  = 3, //!< Prefer random literal.
	};
	//! Conflict clause minimization strategy.
	enum CCMinType {
		cc_local     = 0, //!< Basic algorithm.
		cc_recursive = 1, //!< Extended algorithm.
	};
	//! Antecedents to consider during conflict clause minimization.
	enum CCMinAntes {
		all_antes    = 0, //!< Consider all antecedents.
		short_antes  = 1, //!< Consider only short antecedents.
		binary_antes = 2, //!< Consider only binary antecedents.
		no_antes     = 3, //!< Don't minimize conflict clauses.
	};
	//! Simplifications for long conflict clauses.
	enum CCRepMode {
		cc_no_replace  = 0,//!< Don't replace literals in conflict clauses.
		cc_rep_decision= 1,//!< Replace conflict clause with decision sequence.
		cc_rep_uip     = 2,//!< Replace conflict clause with all uip clause.
		cc_rep_dynamic = 3,//!< Dynamically select between cc_rep_decision and cc_rep_uip.
	};
	//! Strategy for initializing watched literals in clauses.
	enum WatchInit  { watch_rand = 0, watch_first = 1, watch_least = 2 };
	//! Strategy for integrating new information in parallel solving.
	enum UpdateMode { update_on_propagate = 0, update_on_conflict  = 1 };
	enum LbdMode    { lbd_fixed = 0, lbd_updated_less = 1, lbd_update_glucose = 2, lbd_update_pseudo = 3 };

	SolverStrategies();
	void prepare();
	//----- 32 bit ------------
	uint32    compress      : 16; /*!< If > 0, enable compression for learnt clauses of size > compress. */
	uint32    saveProgress  : 16; /*!< Enable progress saving if > 0. */
	//----- 32 bit ------------
	uint32    heuId         : 3;  /*!< Type of decision heuristic.   */
	uint32    reverseArcs   : 2;  /*!< Use "reverse-arcs" during learning if > 0. */
	uint32    otfs          : 2;  /*!< Enable "on-the-fly" subsumption if > 0. */
	uint32    updateLbd     : 2;  /*!< Update lbds of antecedents during conflict analysis (one of LbdMode). */
	uint32    ccMinAntes    : 2;  /*!< Antecedents to look at during conflict clause minimization. */
	uint32    ccRepMode     : 2;  /*!< One of CCRepMode. */
	uint32    ccMinRec      : 1;  /*!< If 1, use more expensive recursive nogood minimization.  */
	uint32    ccMinKeepAct  : 1;  /*!< Do not increase nogood activities during nogood minimization? */
	uint32    initWatches   : 2;  /*!< Initialize watches randomly in clauses. */
	uint32    upMode        : 1;  /*!< One of UpdateMode. */
	uint32    bumpVarAct    : 1;  /*!< Bump activities of vars implied by learnt clauses with small lbd. */
	uint32    search        : 1;  /*!< Current search strategy. */
	uint32    restartOnModel: 1;  /*!< Do a restart after each model. */
	uint32    signDef       : 2;  /*!< Default sign heuristic.        */
	uint32    signFix       : 1;  /*!< Disable all sign heuristics and always use default sign. */
	uint32    reserved      : 1;
	uint32    hasConfig     : 1;  /*!< Config applied to solver? */
	uint32    id            : 6;  /*!< Solver id - SHALL ONLY BE SET BY Shared Context! */
};
//! Parameter-Object for grouping additional heuristic options.
struct HeuParams {
	//! Strategy for scoring clauses not learnt by conflict analysis.
	enum ScoreOther { other_auto = 0u, other_no   = 1u, other_loop = 2u, other_all = 3u };
	//! Strategy for scoring during conflict analysis.
	enum Score      { score_auto = 0u, score_min  = 1u, score_set = 2u, score_multi_set = 3u };
	//! Global preference for domain heuristic.
	enum DomPref    { pref_atom  = 0u, pref_scc   = 1u, pref_hcc  = 2u, pref_disj = 4u, pref_min  = 8u, pref_show = 16u };
	//! Global modification for domain heuristic.
	enum DomMod     { mod_none   = 0u, mod_level  = 1u, mod_spos  = 2u, mod_true  = 3u, mod_sneg  = 4u, mod_false = 5u, mod_init = 6u, mod_factor = 7u };
	//! Values for dynamic decaying scheme.
	struct VsidsDecay {
		uint32 init: 10; /*!< Starting decay factor: 1/0.\<init\>. */
		uint32 bump:  7; /*!< Decay decrease value : \<bump\>/100. */
		uint32 freq: 15; /*!< Update decay factor every \<freq\> conflicts. */
	};
	HeuParams();
	uint32 param    : 16; /*!< Extra parameter for heuristic with meaning depending on type. */
	uint32 score    : 2;  /*!< Type of scoring during resolution. */
	uint32 other    : 2;  /*!< Consider other learnt nogoods in heuristic. */
	uint32 moms     : 1;  /*!< Use MOMS-score as top-level heuristic. */
	uint32 nant     : 1;  /*!< Prefer elements in NegAnte(P).      */
	uint32 huang    : 1;  /*!< Only for Berkmin.   */
	uint32 acids    : 1;  /*!< Only for Vsids/Dom. */
	uint32 domPref  : 5;  /*!< Default pref for doamin heuristic (set of DomPref). */
	uint32 domMod   : 3;  /*!< Default mod for domain heuristic (one of DomMod). */
	union {
		uint32     extra;
		VsidsDecay decay; /*!< Only for Vsids/Dom. */
	};
};

struct OptParams {
	//! Strategy to use for optimization.
	enum Type {
		type_bb = 0, //!< Branch and bound based (model-guided) optimization.
		type_usc= 1, //!< Unsatisfiable-core based (core-guided) optimization.
	};
	//! Algorithm for model-guided optimization.
	enum BBAlgo {
		bb_lin  = 0u, //!< Linear branch and bound with fixed step of size 1.
		bb_hier = 1u, //!< Hierarchical branch and bound with fixed step of size 1.
		bb_inc  = 2u, //!< Hierarchical branch and bound with increasing steps.
		bb_dec  = 3u, //!< Hierarchical branch and bound with decreasing steps.
	};
	//! Algorithm for core-guided optimization.
	enum UscAlgo {
		usc_oll  = 0u, //!< OLL with possibly multiple cardinality constraints per core.
		usc_one  = 1u, //!< ONE with one cardinality constraints per core.
		usc_k    = 2u, //!< K with bounded cardinality constraints of size 2 * (K+1).
		usc_pmr  = 3u, //!< PMRES with clauses.
	};
	//! Additional tactics for core-guided optimization.
	enum UscOption {
		usc_disjoint = 1u, //!< Enable (disjoint) preprocessing.
		usc_succinct = 2u, //!< Do not add redundant constraints.
		usc_stratify = 4u, //!< Enable stratification for weighted optimization.
	};
	//! Strategy for unsatisfiable-core shrinking.
	enum UscTrim {
		usc_trim_lin = 1u, //!< Shrinking with linear search SAT->UNSAT.
		usc_trim_inv = 2u, //!< Shrinking with inverse linear search UNSAT->SAT.
		usc_trim_bin = 3u, //!< Shrinking with binary search SAT->UNSAT.
		usc_trim_rgs = 4u, //!< Shrinking with repeated geometric sequence until UNSAT.
		usc_trim_exp = 5u, //!< Shrinking with exponential search until UNSAT.
		usc_trim_min = 6u, //!< Shrinking with linear search for subset minimal core.
	};
	//! Heuristic options common to all optimization strategies.
	enum Heuristic {
		heu_sign  = 1,  //!< Use optimize statements in sign heuristic.
		heu_model = 2,  //!< Apply model heuristic when optimizing.
	};
	OptParams(Type st = type_bb);
	bool supportsSplitting()    const { return type != type_usc; }
	bool hasOption(UscOption o) const { return (opts & uint32(o)) != 0u; }
	bool hasOption(Heuristic h) const { return (heus & uint32(h)) != 0u; }
	uint32 type : 1; /*!< Optimization strategy (see Type).*/
	uint32 heus : 2; /*!< Set of Heuristic values. */
	uint32 algo : 2; /*!< Optimization algorithm (see BBAlgo/UscAlgo). */
	uint32 trim : 3; /*!< Unsatisfiable-core shrinking (0=no shrinking). */
	uint32 opts : 4; /*!< Set of usc options. */
	uint32 tLim : 5; /*!< Limit core shrinking to 2^tLim conflicts (0=no limit). */
	uint32 kLim :15; /*!< Limit for algorithm K (0=dynamic limit). */
};

//! Parameter-Object for configuring a solver.
struct SolverParams : SolverStrategies  {
	//! Supported forget options.
	enum Forget { forget_heuristic = 1u, forget_signs = 2u, forget_activities = 4u, forget_learnts = 8u };
	SolverParams();
	uint32 prepare();
	inline bool forgetHeuristic() const { return (forgetSet & uint32(forget_heuristic))  != 0; }
	inline bool forgetSigns()     const { return (forgetSet & uint32(forget_signs))      != 0; }
	inline bool forgetActivities()const { return (forgetSet & uint32(forget_activities)) != 0; }
	inline bool forgetLearnts()   const { return (forgetSet & uint32(forget_learnts))    != 0; }
	SolverParams& setId(uint32 sId)     { id = sId; return *this; }
	HeuParams heuristic;  /*!< Parameters for decision heuristic. */
	OptParams opt;        /*!< Parameters for optimization.       */
	// 64-bit
	uint32 seed;           /*!< Seed for the random number generator.  */
	uint32 lookOps   : 16; /*!< Max. number of lookahead operations (0: no limit). */
	uint32 lookType  : 2;  /*!< Type of lookahead operations. */
	uint32 loopRep   : 2;  /*!< How to represent loops? */
	uint32 acycFwd   : 1;  /*!< Disable backward propagation in acyclicity checker. */
	uint32 forgetSet : 4;  /*!< What to forget on (incremental step). */
	uint32 reserved  : 7;
};

typedef Range<uint32> Range32;

//! Aggregates restart-parameters to configure restarts during search.
/*!
 * \see ScheduleStrategy
 */
struct RestartParams {
	RestartParams();
	enum SeqUpdate { seq_continue = 0, seq_repeat = 1, seq_disable = 2 };
	uint32    prepare(bool withLookback);
	void      disable();
	bool      dynamic() const { return dynRestart != 0; }
	bool      local()   const { return cntLocal   != 0; }
	SeqUpdate update()  const { return static_cast<SeqUpdate>(upRestart); }
	ScheduleStrategy sched;  /**< Restart schedule to use. */
	float  blockScale;       /**< Scaling factor for blocking restarts. */
	uint32 blockWindow: 16;  /**< Size of moving assignment average for blocking restarts (0: disable). */
	uint32 blockFirst : 16;  /**< Enable blocking restarts after blockFirst conflicts. */
	CLASP_ALIGN_BITFIELD(uint32)
	uint32 counterRestart:16;/**< Apply counter implication bump every counterRestart restarts (0: disable). */
	uint32 counterBump:16;   /**< Bump factor for counter implication restarts. */
	CLASP_ALIGN_BITFIELD(uint32)
	uint32 shuffle    :14;   /**< Shuffle program after shuffle restarts (0: disable). */
	uint32 shuffleNext:14;   /**< Re-Shuffle program every shuffleNext restarts (0: disable). */
	uint32 upRestart  : 2;   /**< How to update restart sequence after a model was found (one of SeqUpdate). */
	uint32 cntLocal   : 1;   /**< Count conflicts globally or relative to current branch? */
	uint32 dynRestart : 1;   /**< Dynamic restarts enabled? */
};

//! Reduce strategy used during solving.
/*!
 * A reduce strategy mainly consists of an algorithm and a scoring scheme
 * for measuring "activity" of learnt constraints.
 */
struct ReduceStrategy {
	//! Reduction algorithm to use during solving.
	enum Algorithm {
		reduce_linear = 0, //!< Linear algorithm from clasp-1.3.x.
		reduce_stable = 1, //!< Sort constraints by score but keep order in learnt db.
		reduce_sort   = 2, //!< Sort learnt db by score and remove fraction with lowest score.
		reduce_heap   = 3  //!< Similar to reduce_sort but only partially sorts learnt db.
	};
	//! Score to measure "activity" of learnt constraints.
	enum Score {
		score_act  = 0, //!< Activity only: how often constraint is used during conflict analysis.
		score_lbd  = 1, //!< Use literal block distance as activity.
		score_both = 2  //!< Use activity and lbd together.
	};
	//! Strategy for estimating size of problem.
	enum EstimateSize {
		est_dynamic         = 0, //!< Dynamically decide whether to use number of variables or constraints.
		est_con_complexity  = 1, //!< Measure size in terms of constraint complexities.
		est_num_constraints = 2, //!< Measure size in terms of number constraints.
		est_num_vars        = 3  //!< Measure size in terms of number variable.
	};
	static uint32 scoreAct(const ConstraintScore& sc)  { return sc.activity(); }
	static uint32 scoreLbd(const ConstraintScore& sc)  { return uint32(LBD_MAX+1)-sc.lbd(); }
	static uint32 scoreBoth(const ConstraintScore& sc) { return (sc.activity()+1) * scoreLbd(sc); }
	static int    compare(Score sc, const ConstraintScore& lhs, const ConstraintScore& rhs) {
		int fs = 0;
		if      (sc == score_act) { fs = ((int)scoreAct(lhs)) - ((int)scoreAct(rhs)); }
		else if (sc == score_lbd) { fs = ((int)scoreLbd(lhs)) - ((int)scoreLbd(rhs)); }
		return fs != 0 ? fs : ((int)scoreBoth(lhs)) - ((int)scoreBoth(rhs));
	}
	static uint32 asScore(Score sc, const Clasp::ConstraintScore& act) {
		if (sc == score_act)  { return scoreAct(act); }
		if (sc == score_lbd)  { return scoreLbd(act); }
		/*  sc == score_both*/{ return scoreBoth(act);}
	}
	ReduceStrategy() : protect(0), glue(0), fReduce(75), fRestart(0), score(0), algo(0), estimate(0), noGlue(0) {
		static_assert(sizeof(ReduceStrategy) == sizeof(uint32), "invalid bitset");
	}
	uint32 protect : 7; /*!< Protect nogoods whose lbd was reduced and is now <= freeze. */
	uint32 glue    : 4; /*!< Don't remove nogoods with lbd <= glue.    */
	uint32 fReduce : 7; /*!< Fraction of nogoods to remove in percent. */
	uint32 fRestart: 7; /*!< Fraction of nogoods to remove on restart. */
	uint32 score   : 2; /*!< One of Score.                             */
	uint32 algo    : 2; /*!< One of Algorithm.                         */
	uint32 estimate: 2; /*!< How to estimate problem size in init.     */
	uint32 noGlue  : 1; /*!< Do not count glue clauses in limit.       */
};

//! Aggregates parameters for the nogood deletion heuristic used during search.
/*!
 * - S:delCond {yes,no}
 * - no:del {0}[0]
 * - no:del | delCond in {no}
 * - deletion | delCond in {yes}
 * - del-* | delCond in {yes}
 * - {delCond=yes, del-grow=no, del-cfl=no}
 * .
 */
struct ReduceParams {
	ReduceParams() : cflSched(ScheduleStrategy::none()), growSched(ScheduleStrategy::def())
		, fInit(1.0f/3.0f)
		, fMax(3.0f)
		, fGrow(1.1f)
		, initRange(10, UINT32_MAX)
		, maxRange(UINT32_MAX)
		, memMax(0) {}
	void    disable();
	uint32  prepare(bool withLookback);
	Range32 sizeInit(const SharedContext& ctx) const;
	uint32  cflInit(const SharedContext& ctx)  const;
	uint32  getBase(const SharedContext& ctx)  const;
	float   fReduce()  const { return strategy.fReduce / 100.0f; }
	float   fRestart() const { return strategy.fRestart/ 100.0f; }
	static uint32 getLimit(uint32 base, double f, const Range<uint32>& r);
	ScheduleStrategy cflSched;   /**< Conflict-based deletion schedule.               */
	ScheduleStrategy growSched;  /**< Growth-based deletion schedule.                 */
	ReduceStrategy   strategy;   /**< Strategy to apply during nogood deletion.       */
	float            fInit;      /**< Initial limit. X = P*fInit clamped to initRange.*/
	float            fMax;       /**< Maximal limit. X = P*fMax  clamped to maxRange. */
	float            fGrow;      /**< Growth factor for db.                           */
	Range32          initRange;  /**< Allowed range for initial limit.                */
	uint32           maxRange;   /**< Allowed range for maximal limit: [initRange.lo,maxRange]*/
	uint32           memMax;     /**< Memory limit in MB (0 = no limit).              */
};

//! Parameter-Object for grouping solve-related options.
/*!
 * \ingroup enumerator
 */
struct SolveParams {
	//! Creates a default-initialized object.
	/*!
	 * The following parameters are used:
	 * - restart      : quadratic: 100*1.5^k / no restarts after first solution
	 * - deletion     : initial size: vars()/3, grow factor: 1.1, max factor: 3.0, do not reduce on restart
	 * - randomization: disabled
	 * - randomProp   : 0.0 (disabled)
	 * .
	 */
	SolveParams();
	uint32  prepare(bool withLookback);
	bool    randomize(Solver& s) const;
	RestartParams restart;
	ReduceParams  reduce;
	uint32        randRuns:16; /*!< Number of initial randomized-runs. */
	uint32        randConf:16; /*!< Number of conflicts comprising one randomized-run. */
	float         randProb;    /*!< Use random heuristic with given probability ([0,1]) */
	struct FwdCheck {          /*!< Options for (partial checks in) DLP-solving; */
		uint32 highStep : 24;    /*!< Init/inc high level when reached. */
		uint32 highPct  :  7;    /*!< Check on low + (high - low) * highPct/100  */
		uint32 signDef  :  2;    /*!< Default sign heuristic for atoms in disjunctions. */
		FwdCheck() { std::memset(this, 0, sizeof(*this)); }
	}             fwdCheck;
};

class SharedContext;
class SatPreprocessor;

//! Parameters for (optional) Sat-preprocessing.
struct SatPreParams {
	enum Algo {
		sat_pre_no     = 0, /**< Disable sat-preprocessing.                            */
		sat_pre_ve     = 1, /**< Run variable elimination.                             */
		sat_pre_ve_bce = 2, /**< Run variable- and limited blocked clause elimination. */
		sat_pre_full   = 3, /**< Run variable- and full blocked clause elimination.    */
	};
	SatPreParams() : type(0u), limIters(0u), limTime(0u), limFrozen(0u), limClause(4000u), limOcc(0u) {}
	uint32 type     :  2; /**< One of algo. */
	uint32 limIters : 11; /**< Max. number of iterations.                         (0=no limit)*/
	uint32 limTime  : 12; /**< Max. runtime in sec, checked after each iteration. (0=no limit)*/
	uint32 limFrozen:  7; /**< Run only if percent of frozen vars < maxFrozen.    (0=no limit)*/
	uint32 limClause: 16; /**< Run only if \#clauses \< (limClause*1000)          (0=no limit)*/
	uint32 limOcc   : 16; /**< Skip v, if \#occ(v) \>= limOcc && \#occ(~v) \>= limOcc.(0=no limit) */
	bool clauseLimit(uint32 nc)           const { return limClause && nc > (limClause*1000u); }
	bool occLimit(uint32 pos, uint32 neg) const { return limOcc && pos > (limOcc-1u) && neg > (limOcc-1u); }
	uint32 bce()                          const { return type != sat_pre_no ? type - 1 : 0; }
	void   disableBce()                         { type = std::min(type, uint32(sat_pre_ve));}
	static SatPreprocessor* create(const SatPreParams&);
};

//! Parameters for a SharedContext object.
struct ContextParams {
	//! How to handle short learnt clauses.
	enum ShortMode  {
		short_implicit = 0, /*!< Share short learnt clauses via short implication graph. */
		short_explicit = 1, /*!< Do not use short implication graph. */
	};
	//! How to handle physical sharing of (explicit) constraints.
	enum ShareMode  {
		share_no      = 0, /*!< Do not physically share constraints (use copies instead). */
		share_problem = 1, /*!< Share problem constraints but copy learnt constraints.    */
		share_learnt  = 2, /*!< Copy problem constraints but share learnt constraints.    */
		share_all     = 3, /*!< Share all constraints.                                    */
		share_auto    = 4, /*!< Use share_no or share_all depending on number of solvers. */
	};
	ContextParams() : shareMode(share_auto), stats(0), shortMode(short_implicit), seed(1), hasConfig(0), cliConfig(0), cliId(0), cliMode(0) {}
	SatPreParams satPre;        /*!< Preprocessing options.                    */
	uint8        shareMode : 3; /*!< Physical sharing mode (one of ShareMode). */
	uint8        stats     : 2; /*!< See SharedContext::enableStats().         */
	uint8        shortMode : 1; /*!< One of ShortMode.                         */
	uint8        seed      : 1; /*!< Apply new seed when adding solvers.       */
	uint8        hasConfig : 1; /*!< Reserved for command-line interface.      */
	uint8        cliConfig;     /*!< Reserved for command-line interface.      */
	uint8        cliId;         /*!< Reserved for command-line interface.      */
	uint8        cliMode;       /*!< Reserved for command-line interface.      */
};

//! Interface for configuring a SharedContext object and its associated solvers.
class Configuration {
public:
	typedef SolverParams  SolverOpts;
	typedef SolveParams   SearchOpts;
	typedef ContextParams CtxOpts;
	virtual ~Configuration();
	//! Prepares this configuration for the usage in the given context.
	virtual void               prepare(SharedContext&)    = 0;
	//! Returns the options for the shared context.
	virtual const CtxOpts&     context()            const = 0;
	//! Returns the number of solver options in this config.
	virtual uint32             numSolver()          const = 0;
	//! Returns the number of search options in this config.
	virtual uint32             numSearch()          const = 0;
	//! Returns the solver options for the i'th solver to be attached to the SharedContext.
	virtual const SolverOpts&  solver(uint32 i)     const = 0;
	//! Returns the search options for the i'th solver of the SharedContext.
	virtual const SearchOpts&  search(uint32 i)     const = 0;
	//! Returns the heuristic to be used in the i'th solver.
	/*!
	 * The function is called in Solver::startInit().
	 * \note The returned object is owned by the caller.
	 */
	virtual DecisionHeuristic* heuristic(uint32 i)  const = 0;
	//! Adds post propagators to the given solver.
	virtual bool               addPost(Solver& s)   const = 0;
	//! Returns the configuration with the given name or 0 if no such config exists.
	/*!
	 * The default implementation returns this
	 * if n is empty or one of "." or "/".
	 * Otherwise, 0 is returned.
	 */
	virtual Configuration*     config(const char* n);
};

//! Base class for user-provided configurations.
class UserConfiguration : public Configuration {
public:
	//! Adds a lookahead post propagator to the given solver if requested.
	/*!
	 * The function adds a lookahead post propagator if indicated by
	 * the solver's SolverParams.
	 */
	virtual bool            addPost(Solver& s)   const;
	//! Returns the (modifiable) solver options for the i'th solver.
	virtual SolverOpts&     addSolver(uint32 i) = 0;
	//! Returns the (modifiable) search options for the i'th solver.
	virtual SearchOpts&     addSearch(uint32 i) = 0;
};

//! Simple factory for decision heuristics.
struct Heuristic_t {
	enum Type { Default = 0, Berkmin = 1, Vsids = 2, Vmtf = 3, Domain = 4, Unit = 5, None = 6, User = 7  };
	static inline bool        isLookback(uint32 type) { return type >= (uint32)Berkmin && type < (uint32)Unit; }
	//! Default callback for creating decision heuristics.
	static DecisionHeuristic* create(Type t, const HeuParams& p);
};

struct ProjectMode_t {
	enum Mode { Implicit = 0u, Output = 1u, Explicit = 2u };
};
typedef ProjectMode_t::Mode ProjectMode;

//! Basic configuration for one or more SAT solvers.
class BasicSatConfig : public UserConfiguration, public ContextParams {
public:
	struct HeuristicCreator {
		virtual ~HeuristicCreator();
		virtual DecisionHeuristic* create(Heuristic_t::Type t, const HeuParams& p) = 0;
	};

	BasicSatConfig();
	void               prepare(SharedContext&);
	const CtxOpts&     context()            const { return *this; }
	uint32             numSolver()          const { return sizeVec(solver_); }
	uint32             numSearch()          const { return sizeVec(search_); }
	const SolverOpts&  solver(uint32 i)     const { return solver_[i % solver_.size() ]; }
	const SearchOpts&  search(uint32 i)     const { return search_[i % search_.size() ]; }
	DecisionHeuristic* heuristic(uint32 i)  const;
	SolverOpts&        addSolver(uint32 i);
	SearchOpts&        addSearch(uint32 i);

	virtual void       reset();
	virtual void       resize(uint32 numSolver, uint32 numSearch);
	void               setHeuristicCreator(HeuristicCreator* hc, Ownership_t::Type = Ownership_t::Acquire);
private:
	typedef PodVector<SolverOpts>::type SolverVec;
	typedef PodVector<SearchOpts>::type SearchVec;
	typedef SingleOwnerPtr<HeuristicCreator> HeuFactory;
	SolverVec  solver_;
	SearchVec  search_;
	HeuFactory heu_;
};

//! Base class for solving related events.
template <class T>
struct SolveEvent : Event_t<T> {
	SolveEvent(const Solver& s, Event::Verbosity verb) : Event_t<T>(Event::subsystem_solve, verb), solver(&s) {}
	const Solver* solver;
};
struct Model;
//! Base class for handling results of a solve operation.
class ModelHandler {
public:
	virtual ~ModelHandler();
	virtual bool onModel(const Solver&, const Model&) = 0;
	virtual bool onUnsat(const Solver&, const Model&);
};
//! Type for storing the lower bound of a minimize statement.
struct LowerBound {
	LowerBound() : level(0), bound(CLASP_WEIGHT_SUM_MIN) {}
	void reset() { *this = LowerBound(); }
	bool active() const { return bound != CLASP_WEIGHT_SUM_MIN; }
	uint32 level;
	wsum_t bound;
};

}
#endif
