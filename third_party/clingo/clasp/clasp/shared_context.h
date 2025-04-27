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
#ifndef CLASP_SHARED_CONTEXT_H_INCLUDED
#define CLASP_SHARED_CONTEXT_H_INCLUDED
#ifdef _MSC_VER
#pragma once
#endif
#include <clasp/claspfwd.h>
#include <clasp/literal.h>
#include <clasp/constraint.h>
#include <clasp/util/left_right_sequence.h>
#include <clasp/solver_strategies.h>
/*!
 * \file
 * \brief Contains common types shared between different solvers.
 */
namespace Clasp {
class Assignment;
class SharedLiterals;
struct SolverStats;
class StatisticObject;
typedef Asp::PrgDepGraph PrgDepGraph;
//! An immutable string with efficient copying.
/*!
 * \ingroup misc
 */
class ConstString {
public:
	//! Creates a string from str.
	ConstString(const char* str = "");
	//! Creates a copy of str.
	ConstString(const StrView& str);
	ConstString(const ConstString& other);
	~ConstString();
	ConstString& operator=(const ConstString& rhs);
	const char* c_str()     const;
	operator const char* () const { return c_str(); }
	void swap(ConstString& o);
private:
	typedef uint64 RefType;
	RefType ref_;
};
/**
* \defgroup shared SharedContext
* \brief %SharedContext and related types.
*/

//! Base class for event handlers.
/*!
 * \ingroup shared
 */
class EventHandler : public ModelHandler {
public:
	//! Creates a handler for events with given verbosity or lower.
	explicit EventHandler(Event::Verbosity verbosity = Event::verbosity_quiet);
	virtual ~EventHandler();
	//! Sets the verbosity for the given event source.
	/*!
	 * Events with higher verbosity are not dispatched to this handler.
	 */
	void setVerbosity(Event::Subsystem sys, Event::Verbosity verb);
	//! Sets the active event source to sys if sys is not yet active.
	bool setActive(Event::Subsystem sys);
	Event::Subsystem active() const;
	uint32 verbosity(Event::Subsystem sys) const { return (uint32(verb_) >> (uint32(sys)<<VERB_SHIFT)) & uint32(VERB_MAX); }
	//! Calls onEvent(ev) if ev has acceptable verbosity.
	void dispatch(const Event& ev)               { if (ev.verb <= verbosity(static_cast<Event::Subsystem>(ev.system))) { onEvent(ev); } }
	virtual void onEvent(const Event& /* ev */)  {}
	virtual bool onModel(const Solver&, const Model&) { return true; }
private:
	enum { VERB_SHIFT = 2u, VERB_MAX = 15u };
	EventHandler(const EventHandler&);
	EventHandler& operator=(const EventHandler&);
	uint16 verb_;
	uint16 sys_;
};

//! Event type for log or warning messages.
/*!
 * \ingroup enumerator
 */
struct LogEvent : Event_t<LogEvent> {
	enum Type { Message = 'M', Warning = 'W' };
	LogEvent(Subsystem sys, Verbosity verb, Type t, const Solver* s, const char* what) : Event_t<LogEvent>(sys, verb), solver(s), msg(what) {
		op = static_cast<uint32>(t);
	}
	bool isWarning() const { return op == static_cast<uint32>(Warning); }
	const Solver* solver;
	const char*   msg;
};

//! Base class for preprocessors working on clauses only.
/*!
 * \ingroup shared
 */
class SatPreprocessor {
public:
	//! A clause class optimized for preprocessing.
	class Clause {
	public:
		static Clause*  newClause(const Literal* lits, uint32 size);
		static uint64   abstractLit(Literal p)      { return uint64(1) << ((p.var()-1) & 63);  }
		uint32          size()                const { return size_;       }
		const Literal&  operator[](uint32 x)  const { return lits_[x];    }
		bool            inQ()                 const { return inQ_ != 0;   }
		uint64          abstraction()         const { return data_.abstr; }
		Clause*         next()                const { return data_.next;  }
		bool            marked()              const { return marked_ != 0;}
		Literal&        operator[](uint32 x)        { return lits_[x];    }
		void            setInQ(bool b)              { inQ_    = (uint32)b;}
		void            setMarked(bool b)           { marked_ = (uint32)b;}
		uint64&         abstraction()               { return data_.abstr; }
		Clause*         linkRemoved(Clause* next)   { data_.next = next; return this; }
		void            strengthen(Literal p);
		void            simplify(Solver& s);
		void            destroy();
	private:
		Clause(const Literal* lits, uint32 size);
		union {
			uint64  abstr;      // abstraction of literals
			Clause* next;       // next removed clause
		}       data_;
		uint32  size_   : 30; // size of the clause
		uint32  inQ_    : 1;  // in todo-queue?
		uint32  marked_ : 1;  // a marker flag
		Literal lits_[1];     // literals of the clause: [lits_[0], lits_[size_])
	};

	SatPreprocessor();
	virtual ~SatPreprocessor();

	//! Creates a clone of this preprocessor.
	/*!
	 * \note The function does not clone any clauses already added to *this.
	 */
	virtual SatPreprocessor* clone() = 0;

	uint32 numClauses() const { return (uint32)clauses_.size(); }
	//! Adds a clause to the preprocessor.
	/*!
	 * \pre clause is not a tautology (i.e. does not contain both l and ~l)
	 * \pre clause is a set (i.e. does not contain l more than once)
	 * \return true if clause was added. False if adding the clause makes the problem UNSAT
	 */
	bool addClause(const LitVec& cl) { return addClause(!cl.empty() ? &cl[0] : 0, sizeVec(cl)); }
	bool addClause(const Literal* clause, uint32 size);
	//! Runs the preprocessor on all clauses that were previously added.
	/*!
	 * \pre A ctx.startAddConstraint() was called and has variables for all added clauses.
	 */
	bool preprocess(SharedContext& ctx, SatPreParams& opts);
	bool preprocess(SharedContext& ctx);

	//! Force removal of state & clauses.
	void cleanUp(bool discardEliminated = false);

	//! Extends the model in m with values for any eliminated variables.
	void extendModel(ValueVec& m, LitVec& open);
	struct Stats {
		Stats() : clRemoved(0), clAdded(0), litsRemoved(0) {}
		uint32 clRemoved;
		uint32 clAdded;
		uint32 litsRemoved;
	} stats;
	typedef SatPreParams Options;
protected:
	typedef PodVector<Clause*>::type  ClauseList;
	virtual bool  initPreprocess(SatPreParams& opts) = 0;
	virtual bool  doPreprocess() = 0;
	virtual void  doExtendModel(ValueVec& m, LitVec& open) = 0;
	virtual void  doCleanUp() = 0;
	Clause*       clause(uint32 clId)       { return clauses_[clId]; }
	const Clause* clause(uint32 clId) const { return clauses_[clId]; }
	void          freezeSeen();
	void          discardClauses(bool discardEliminated);
	void          setClause(uint32 clId, const LitVec& cl) {
		clauses_[clId] = Clause::newClause(&cl[0], (uint32)cl.size());
	}
	void          destroyClause(uint32 clId){
		clauses_[clId]->destroy();
		clauses_[clId] = 0;
		++stats.clRemoved;
	}
	void          eliminateClause(uint32 id){
		elimTop_     = clauses_[id]->linkRemoved(elimTop_);
		clauses_[id] = 0;
		++stats.clRemoved;
	}
	SharedContext* ctx_;     // current context
	const Options* opts_;    // active options
	Clause*        elimTop_; // stack of blocked/eliminated clauses
private:
	SatPreprocessor(const SatPreprocessor&);
	SatPreprocessor& operator=(const SatPreprocessor&);
	ClauseList clauses_; // initial non-unit clauses
	LitVec     units_;   // initial unit clauses
	Range32    seen_;    // vars seen in previous step
};

///////////////////////////////////////////////////////////////////////////////
// Problem statistics
///////////////////////////////////////////////////////////////////////////////
/*!
 * \addtogroup shared
 * @{
 */
//! A struct for aggregating basic problem statistics.
struct ProblemStats {
	ProblemStats() { reset(); }
	struct { uint32 num, eliminated, frozen; } vars;
	struct { uint32 other, binary, ternary; } constraints;
	uint32  acycEdges;
	uint32  complexity;
	void    reset() { std::memset(this, 0, sizeof(*this)); }
	uint32  numConstraints() const   { return constraints.other + constraints.binary + constraints.ternary; }
	void diff(const ProblemStats& o) {
		vars.num           = std::max(vars.num, o.vars.num)-std::min(vars.num, o.vars.num);
		vars.eliminated    = std::max(vars.eliminated, o.vars.eliminated)-std::min(vars.eliminated, o.vars.eliminated);
		vars.frozen        = std::max(vars.frozen, o.vars.frozen)-std::min(vars.frozen, o.vars.frozen);
		constraints.other  = std::max(constraints.other, o.constraints.other) - std::min(constraints.other, o.constraints.other);
		constraints.binary = std::max(constraints.binary, o.constraints.binary) - std::min(constraints.binary, o.constraints.binary);
		constraints.ternary= std::max(constraints.ternary, o.constraints.ternary) - std::min(constraints.ternary, o.constraints.ternary);
		acycEdges          = std::max(acycEdges, o.acycEdges) - std::min(acycEdges, o.acycEdges);
	}
	void accu(const ProblemStats& o) {
		vars.num            += o.vars.num;
		vars.eliminated     += o.vars.eliminated;
		vars.frozen         += o.vars.frozen;
		constraints.other   += o.constraints.other;
		constraints.binary  += o.constraints.binary;
		constraints.ternary += o.constraints.ternary;
		acycEdges           += o.acycEdges;
	}
	// StatisticObject
	static uint32 size();
	static const char* key(uint32 i);
	StatisticObject at(const char* k) const;
};

//! Stores static information about a variable.
struct VarInfo {
	//! Possible flags.
	enum Flag {
		Mark_p = 0x1u,  //!< Mark for positive literal.
		Mark_n = 0x2u,  //!< Mark for negative literal.
		Input  = 0x4u,  //!< Is this var an input variable?
		Body   = 0x8u,  //!< Is this var representing a body?
		Eq     = 0x10u, //!< Is this var representing both a body and an atom?
		Nant   = 0x20u, //!< Is this var in NAnt(P)?
		Frozen = 0x40u, //!< Is the variable frozen?
		Output = 0x80u  //!< Is the variable an output variable?
	};
	static uint8 flags(VarType t) {
		if (t == Var_t::Body)  { return VarInfo::Body; }
		if (t == Var_t::Hybrid){ return VarInfo::Eq; }
		return 0;
	}
	explicit VarInfo(uint8 r = 0) : rep(r) { }
	//! Returns the type of the variable (or Var_t::Hybrid if variable was created with parameter eq=true).
	VarType type()          const { return has(VarInfo::Eq) ? Var_t::Hybrid : VarType(Var_t::Atom + has(VarInfo::Body)); }
	//! Returns whether var is part of negative antecedents (occurs negatively or in the head of a choice rule).
	bool    nant()          const { return has(VarInfo::Nant); }
	//! Returns true if var is excluded from variable elimination.
	bool    frozen()        const { return has(VarInfo::Frozen); }
	//! Returns true if var is an input variable.
	bool    input()         const { return has(VarInfo::Input); }
	//! Returns true if var is marked as output variable.
	bool    output()        const { return has(VarInfo::Output); }
	//! Returns the preferred sign of this variable w.r.t its type.
	/*!
	 * \return false (i.e no sign) if var originated from body, otherwise true.
	 */
	bool    preferredSign() const { return !has(VarInfo::Body); }

	bool    has(Flag f)     const { return (rep & flag(f)) != 0; }
	bool    has(uint32 f)   const { return (rep & f) != 0;       }
	bool    hasAll(uint32 f)const { return (rep & f) == f; }
	void    set(Flag f)           { rep |= flag(f); }
	void    toggle(Flag f)        { rep ^= flag(f); }
	static uint8 flag(Flag x)     { return uint8(x); }
	uint8 rep;
};

//! A class for efficiently storing and propagating binary and ternary clauses.
/*!
 * \ingroup shared_con
 */
class ShortImplicationsGraph {
public:
	ShortImplicationsGraph();
	~ShortImplicationsGraph();
	enum ImpType { binary_imp = 2, ternary_imp = 3 };

	//! Makes room for nodes number of nodes.
	void resize(uint32 nodes);
	//! Mark the instance as shared/unshared.
	/*!
	 * A shared instance adds learnt binary/ternary clauses
	 * to specialized shared blocks of memory.
	 */
	void markShared(bool b) { shared_ = b; }
	//! Adds the given constraint to the implication graph.
	/*!
	 * \return true iff a new implication was added.
	 */
	bool add(ImpType t, bool learnt, const Literal* lits);

	//! Removes p and its implications.
	/*!
	 * \pre s.isTrue(p)
	 */
	void removeTrue(const Solver& s, Literal p);

	//! Propagates consequences of p following from binary and ternary clauses.
	/*!
	 * \pre s.isTrue(p)
	 */
	bool   propagate(Solver& s, Literal p) const;
	//! Propagates immediate consequences of p following from binary clauses only.
	bool   propagateBin(Assignment& out, Literal p, uint32 dl) const;
	//! Checks whether there is a reverse arc implying p and if so returns it in out.
	bool   reverseArc(const Solver& s, Literal p, uint32 maxLev, Antecedent& out) const;

	uint32 numBinary() const { return bin_[0]; }
	uint32 numTernary()const { return tern_[0]; }
	uint32 numLearnt() const { return bin_[1] + tern_[1]; }
	uint32 numEdges(Literal p) const;
	uint32 size()      const { return sizeVec(graph_); }
	//! Applies op on all unary- and binary implications following from p.
	/*!
	 * OP must provide two functions:
	 *  - bool unary(Literal, Literal)
	 *  - bool binary(Literal, Literal, Literal)
	 * The first argument will be p, the second (resp. third) the unary
	 * (resp. binary) clause implied by p.
	 * \note For learnt imps, at least one literal has its watch-flag set.
	 */
	template <class OP>
	bool forEach(Literal p, const OP& op) const {
		const ImplicationList& x = graph_[p.id()];
		if (x.empty()) return true;
		ImplicationList::const_right_iterator rEnd = x.right_end(); // prefetch
		for (ImplicationList::const_left_iterator it = x.left_begin(), end = x.left_end(); it != end; ++it) {
			if (!op.unary(p, *it)) { return false; }
		}
		for (ImplicationList::const_right_iterator it = x.right_begin(); it != rEnd; ++it) {
			if (!op.binary(p, it->first, it->second)) { return false; }
		}
#if CLASP_HAS_THREADS
		for (Block* b = (x).learnt; b ; b = b->next) {
			p.flag(); bool r = true;
			for (Block::const_iterator imp = b->begin(), endOf = b->end(); imp != endOf; ) {
				if (!imp->flagged()) { r = op.binary(p, imp[0], imp[1]); imp += 2; }
				else                 { r = op.unary(p, imp[0]);          imp += 1; }
				if (!r)              { return false; }
			}
		}
#endif
		return true;
	}
private:
	ShortImplicationsGraph(const ShortImplicationsGraph&);
	ShortImplicationsGraph& operator=(ShortImplicationsGraph&);
	struct Propagate;
	struct ReverseArc;
#if CLASP_HAS_THREADS
	struct Block {
		typedef Clasp::mt::atomic<uint32> atomic_size;
		typedef Clasp::mt::atomic<Block*> atomic_ptr;
		typedef const Literal*      const_iterator;
		typedef       Literal*      iterator;
		enum { block_cap = (64 - (sizeof(atomic_size)+sizeof(atomic_ptr)))/sizeof(Literal) };
		explicit Block();
		const_iterator  begin() const { return data; }
		const_iterator  end()   const { return data+size(); }
		iterator        end()         { return data+size(); }
		uint32          size()  const { return size_lock >> 1; }
		bool tryLock(uint32& lockedSize);
		void addUnlock(uint32 lockedSize, const Literal* x, uint32 xs);
		atomic_ptr  next;
		atomic_size size_lock;
		Literal     data[block_cap];
	};
	typedef Block::atomic_ptr SharedBlockPtr;
	typedef bk_lib::left_right_sequence<Literal, std::pair<Literal,Literal>, 64-sizeof(SharedBlockPtr)> ImpListBase;
	struct ImplicationList : public ImpListBase {
		ImplicationList() : ImpListBase() { learnt = 0; }
		ImplicationList(const ImplicationList& other) : ImpListBase(other), learnt() {
			learnt = static_cast<Block*>(other.learnt);
		}
		ImplicationList& operator=(const ImplicationList& other) {
			ImpListBase::operator=(other);
			learnt = static_cast<Block*>(other.learnt);
			return *this;
		}
		~ImplicationList();
		bool hasLearnt(Literal q, Literal r = lit_false()) const;
		void addLearnt(Literal q, Literal r = lit_false());
		void simplifyLearnt(const Solver& s);
		bool empty() const { return ImpListBase::empty() && learnt == static_cast<Block*>(0); }
		void move(ImplicationList& other);
		void clear(bool b);
		SharedBlockPtr learnt;
	};
#else
	typedef bk_lib::left_right_sequence<Literal, std::pair<Literal,Literal>, 64> ImplicationList;
#endif
	ImplicationList& getList(Literal p) { return graph_[p.id()]; }
	void remove_bin(ImplicationList& w, Literal p);
	void remove_tern(ImplicationList& w, Literal p);
	typedef PodVector<ImplicationList>::type ImpLists;
	ImpLists graph_;     // one implication list for each literal
	uint32   bin_[2];    // number of binary constraints (0: problem / 1: learnt)
	uint32   tern_[2];   // number of ternary constraints(0: problem / 1: learnt)
	bool     shared_;
};

//! Base class for distributing learnt knowledge between solvers.
class Distributor {
public:
	struct Policy {
		enum Types {
			no       = 0,
			conflict = Constraint_t::Conflict,
			loop     = Constraint_t::Loop,
			all      = conflict | loop,
			implicit = all + 1
		};
		Policy(uint32 a_sz = 0, uint32 a_lbd = 0, uint32 a_type = 0) : size(a_sz), lbd(a_lbd), types(a_type) {}
		uint32 size  : 22; /*!< Allow distribution up to this size only. */
		uint32 lbd   :  7; /*!< Allow distribution up to this lbd only.  */
		uint32 types :  3; /*!< Restrict distribution to these types.    */
	};
	static  uint64  mask(uint32 i)             { return uint64(1) << i; }
	static  uint32  initSet(uint32 sz)         { return (uint64(1) << sz) - 1; }
	static  bool    inSet(uint64 s, uint32 id) { return (s & mask(id)) != 0; }
	explicit Distributor(const Policy& p);
	virtual ~Distributor();
	bool            isCandidate(uint32 size, uint32 lbd, uint32 type) const {
		return size <= policy_.size && lbd <= policy_.lbd && ((type & policy_.types) != 0);
	}
	virtual void    publish(const Solver& source, SharedLiterals* lits) = 0;
	virtual uint32  receive(const Solver& in, SharedLiterals** out, uint32 maxOut) = 0;
private:
	Distributor(const Distributor&);
	Distributor& operator=(const Distributor&);
	Policy policy_;
};

//! Output table that contains predicates to be output on model.
class OutputTable {
public:
	typedef ConstString NameType;
	typedef Range32     RangeType;
	struct PredType {
		NameType name;
		Literal  cond;
		uint32   user;
	};
	typedef PodVector<NameType>::type FactVec;
	typedef PodVector<PredType>::type PredVec;
	typedef FactVec::const_iterator   fact_iterator;
	typedef PredVec::const_iterator   pred_iterator;
	typedef num_iterator<uint32>      range_iterator;
	typedef LitVec::const_iterator    lit_iterator;

	OutputTable();
	~OutputTable();
	//! Ignore predicates starting with c.
	void setFilter(char c);
	//! Adds a fact.
	bool add(const NameType& fact);
	//! Adds an output predicate, i.e. n is output if c is true.
	bool add(const NameType& n, Literal c, uint32 u = 0);
	//! Sets a range of output variables.
	void setVarRange(const RangeType& r);
	void setProjectMode(ProjectMode m);

	//! Returns whether n would be filtered out.
	bool filter(const NameType& n) const;

	fact_iterator  fact_begin() const { return facts_.begin(); }
	fact_iterator  fact_end()   const { return facts_.end();  }
	pred_iterator  pred_begin() const { return preds_.begin(); }
	pred_iterator  pred_end()   const { return preds_.end();  }
	range_iterator vars_begin() const { return range_iterator(vars_.lo); }
	range_iterator vars_end()   const { return range_iterator(vars_.hi); }
	RangeType      vars_range() const { return vars_; }

	ProjectMode    projectMode()const { return projMode_ ? static_cast<ProjectMode>(projMode_) : hasProject() ? ProjectMode_t::Explicit : ProjectMode_t::Output; }
	bool           hasProject() const { return !proj_.empty(); }
	lit_iterator   proj_begin() const { return proj_.begin(); }
	lit_iterator   proj_end()   const { return proj_.end(); }
	void           addProject(Literal x);

	//! Returns the number of output elements, counting each element in a var range.
	uint32 size()     const;
	uint32 numFacts() const { return static_cast<uint32>(facts_.size()); }
	uint32 numPreds() const { return static_cast<uint32>(preds_.size()); }
	uint32 numVars()  const { return static_cast<uint32>(vars_.hi - vars_.lo);  }

	//! An optional callback for getting additional theory output.
	class Theory {
	public:
		virtual ~Theory();
		//! Called once on new model m. Shall return 0 to indicate no output.
		virtual const char* first(const Model& m) = 0;
		//! Shall return 0 to indicate no output.
		virtual const char* next() = 0;
	}* theory;
private:
	FactVec facts_;
	PredVec preds_;
	LitVec  proj_;
	Range32 vars_;
	int     projMode_;
	char    hide_;
};
//! A type for storing information to be used in clasp's domain heuristic.
class DomainTable {
public:
	DomainTable();
	//! A type storing a single domain modification for a variable.
	class ValueType {
	public:
		ValueType(Var v, DomModType t, int16 bias, uint16 prio, Literal cond);
		bool       hasCondition() const { return cond_ != 0; }
		Literal    cond() const { return Literal::fromId(cond_); }
		Var        var()  const { return var_; }
		DomModType type() const;
		int16      bias() const { return bias_; }
		uint16     prio() const { return prio_; }
		bool       comp() const { return comp_ != 0; }
	private:
		uint32 cond_ : 31;
		uint32 comp_ :  1;
		uint32 var_  : 30;
		uint32 type_ :  2;
		int16  bias_;
		uint16 prio_;
	};
	typedef PodVector<ValueType>::type DomVec;
	typedef DomVec::const_iterator     iterator;

	void   add(Var v, DomModType t, int16 bias, uint16 prio, Literal cond);
	uint32 simplify();
	void   reset();
	bool     empty() const;
	uint32   size()  const;
	iterator begin() const;
	iterator end()   const;
	LitVec* assume;

	class DefaultAction {
	public:
		virtual ~DefaultAction();
		virtual void atom(Literal p, HeuParams::DomPref, uint32 strat) = 0;
	};
	static void applyDefault(const SharedContext& ctx, DefaultAction& action, uint32 prefSet = 0);
private:
	static bool cmp(const ValueType& lhs, const ValueType& rhs) {
		return lhs.cond() < rhs.cond() || (lhs.cond() == rhs.cond() && lhs.var() < rhs.var());
	}
	DomVec entries_;
	uint32 seen_;   // size of domain table after last simplify
};

//! Aggregates information to be shared between solver objects.
/*!
 * Among other things, SharedContext objects store
 * static information on variables, an output table, as well as the
 * binary and ternary implication graph of the input problem.
 *
 * Furthermore, a SharedContext object always stores a distinguished
 * master solver that is used to store and simplify problem constraints.
 * Additional solvers can be added via SharedContext::pushSolver().
 * Once initialization is completed, any additional solvers must be attached
 * to this object by calling SharedContext::attach().
 */
class SharedContext {
public:
	typedef PodVector<Solver*>::type       SolverVec;
	typedef SingleOwnerPtr<PrgDepGraph>    SccGraph;
	typedef SingleOwnerPtr<ExtDepGraph>    ExtGraph;
	typedef Configuration*                 ConfigPtr;
	typedef SingleOwnerPtr<Distributor>    DistrPtr;
	typedef const ProblemStats&            StatsCRef;
	typedef DomainTable                    DomTab;
	typedef OutputTable                    Output;
	typedef LitVec::size_type              size_type;
	typedef ShortImplicationsGraph         ImpGraph;
	typedef const ImpGraph&                ImpGraphRef;
	typedef EventHandler*                  LogPtr;
	typedef SingleOwnerPtr<SatPreprocessor>SatPrePtr;
	typedef SharedMinimizeData*            MinPtr;
	enum ResizeMode { resize_reserve = 0u, resize_push = 1u, resize_pop = 2u, resize_resize = 3u};
	enum PreproMode { prepro_preserve_models = 1u, prepro_preserve_shown  = 2u };
	enum ReportMode { report_default = 0u, report_conflict = 1u };
	enum SolveMode  { solve_once = 0u, solve_multi = 1u };
	/*!
	 * \name Configuration
	 * \brief Functions for creating and configuring a shared context.
	 * @{ */
	//! Creates a new object for sharing variables and the binary and ternary implication graph.
	explicit SharedContext();
	~SharedContext();
	//! Resets this object to the state after default construction.
	void       reset();
	//! Enables event reporting via the given event handler.
	void       setEventHandler(LogPtr r, ReportMode m = report_default) { progress_ = r; share_.report = uint32(m); }
	//! Sets solve mode, which can be used by other objects to query whether multi-shot solving is active.
	void       setSolveMode(SolveMode m);
	//! Sets how to handle physical sharing of constraints.
	void       setShareMode(ContextParams::ShareMode m);
	//! Sets whether the short implication graph should be used for storing short learnt constraints.
	void       setShortMode(ContextParams::ShortMode m);
	//! Sets maximal number of solvers sharing this object.
	void       setConcurrency(uint32 numSolver, ResizeMode m = resize_reserve);
	//! If b is true, sets preprocessing mode to model-preserving operations only.
	void       setPreserveModels(bool b = true) { setPreproMode(prepro_preserve_models, b); }
	//! If b is true, excludes all shown variables from variable elimination.
	void       setPreserveShown(bool b = true)  { setPreproMode(prepro_preserve_shown, b); }

	//! Adds an additional solver to this object and returns it.
	Solver&    pushSolver();
	//! Configures the statistic object of attached solvers.
	/*!
	 * The level determines the amount of extra statistics.
	 * \see ExtendedStats
	 * \see JumpStats
   */
	void       enableStats(uint32 level);
	//! Sets the configuration for this object and its attached solvers.
	/*!
	 * \note If ownership is Ownership_t::Acquire, ownership of c is transferred.
	 */
	void       setConfiguration(Configuration* c, Ownership_t::Type ownership);
	SatPrePtr  satPrepro;  /*!< Preprocessor for simplifying the problem.              */
	SccGraph   sccGraph;   /*!< Program dependency graph - only used for ASP-problems. */
	ExtGraph   extGraph;   /*!< External dependency graph - given by user.             */

	//! Returns the current configuration used in this object.
	ConfigPtr  configuration()      const { return config_.get(); }
	//! Returns the active event handler or 0 if none was set.
	LogPtr     eventHandler()       const { return progress_; }
	//! Returns whether this object seeds the RNG of new solvers.
	bool       seedSolvers()        const { return share_.seed != 0; }
	//! Returns the number of solvers that can share this object.
	uint32     concurrency()        const { return share_.count; }
	bool       preserveModels()     const { return (share_.satPreM & prepro_preserve_models) != 0; }
	bool       preserveShown()      const { return (share_.satPreM & prepro_preserve_shown) != 0; }
	//! Returns whether physical sharing is enabled for constraints of type t.
	bool       physicalShare(ConstraintType t) const { return (share_.shareM & (1 + (t != Constraint_t::Static))) != 0; }
	//! Returns whether physical sharing of problem constraints is enabled.
	bool       physicalShareProblem()          const { return (share_.shareM & ContextParams::share_problem) != 0; }
	//! Returns whether short constraints of type t can be stored in the short implication graph.
	bool       allowImplicit(ConstraintType t) const { return t != Constraint_t::Static ? share_.shortM != ContextParams::short_explicit : !isShared(); }
	//! Returns the configured solve mode.
	SolveMode  solveMode() const { return static_cast<SolveMode>(share_.solveM); }
	//@}

	/*!
	 * \name Problem introspection
	 * \brief Functions for querying information about the problem.
	 */
	//@{
	//! Returns true unless the master has an unresolvable top-level conflict.
	bool       ok()                 const;
	//! Returns whether the problem is currently frozen and therefore ready for being solved.
	bool       frozen()             const { return share_.frozen;}
	//! Returns whether more than one solver is actively working on the problem.
	bool       isShared()           const { return frozen() && concurrency() > 1; }
	//! Returns whether the problem is more than a simple CNF.
	bool       isExtended()         const { return stats_.vars.frozen != 0; }
	//! Returns whether this object has a solver associcated with the given id.
	bool       hasSolver(uint32 id) const { return id < solvers_.size(); }
	//! Returns the master solver associated with this object.
	Solver*    master()             const { return solver(0);    }
	//! Returns the solver with the given id.
	Solver*    solver(uint32 id)    const { return solvers_[id]; }

	//! Returns the number of problem variables.
	/*!
	 * \note The special sentinel-var 0 is not counted, i.e. numVars() returns
	 * the number of problem-variables.
	 * To iterate over all problem variables use a loop like:
	 * \code
	 * for (Var i = 1; i <= numVars(); ++i) {...}
	 * \endcode
	 */
	uint32     numVars()            const { return static_cast<uint32>(varInfo_.size() - 1); }
	//! Returns the number of eliminated vars.
	uint32     numEliminatedVars()  const { return stats_.vars.eliminated; }
	//! Returns true if var represents a valid variable in this problem.
	/*!
	 * \note The range of valid variables is [1..numVars()]. The variable 0
	 * is a special sentinel variable.
	 */
	bool       validVar(Var var)    const { return var < static_cast<uint32>(varInfo_.size()); }
	//! Returns information about the given variable.
	VarInfo    varInfo(Var v)       const { assert(validVar(v)); return varInfo_[v]; }
	//! Returns true if v is currently eliminated, i.e. no longer part of the problem.
	bool       eliminated(Var v)    const;
	bool       marked(Literal p)    const { return varInfo(p.var()).has(VarInfo::Mark_p + p.sign()); }
	//! Returns the number of problem constraints.
	uint32     numConstraints()     const;
	//! Returns the number of binary constraints.
	uint32     numBinary()          const { return btig_.numBinary();  }
	//! Returns the number of ternary constraints.
	uint32     numTernary()         const { return btig_.numTernary(); }
	//! Returns the number of unary constraints.
	uint32     numUnary()           const { return lastTopLevel_; }
	//! Returns an estimate of the problem complexity based on the number and type of constraints.
	uint32     problemComplexity()  const;
	//! Returns whether the problem contains minimize (i.e. weak) constraints.
	bool       hasMinimize()        const;
	StatsCRef  stats()              const { return stats_; }
	//@}

	/*!
	 * \name Problem setup
	 * \brief Functions for specifying the problem.
	 *
	 * Problem specification is a four-stage process:
	 * -# Add variables to the SharedContext object.
	 * -# Call startAddConstraints().
	 * -# Add problem constraints.
	 * -# Call endInit() to finish the initialization process.
	 * .
	 * \note After endInit() was called, other solvers can be attached to this object.
	 * \note In incremental setting, the process must be repeated for each incremental step.
	 *
	 * \note Problem specification is *not* thread-safe, i.e. during initialization no other thread shall
	 * access the context.
	 *
	 * \note !frozen() is a precondition for all functions in this group!
	 * @{ */
	//! Unfreezes a frozen program and prepares it for updates.
	/*!
	 * The function also triggers forgetting of volatile knowledge and removes
	 * any auxiliary variables.
	 * \see requestStepVar()
	 * \see Solver::popAuxVar()
	 */
	bool    unfreeze();

	//! Adds a new variable and returns its numerical id.
	/*!
	 * \param type  Type of variable.
	 * \param flags Additional information associated with the new variable.
	 * \note Problem variables are numbered from 1 onwards!
	 */
	Var     addVar(VarType type, uint8 flags = VarInfo::Nant | VarInfo::Input) { return addVars(1, type, flags); }
	Var     addVars(uint32 nVars, VarType type, uint8 flags = VarInfo::Nant | VarInfo::Input);
	//! Removes the n most recently added problem variables.
	/*!
	 * \pre The variables have either not yet been committed by a call to startAddConstraints()
	 *      or they do not occur in any constraint.
	 */
	void    popVars(uint32 n = 1);
	//! Freezes/defreezes a variable (a frozen var is exempt from Sat-preprocessing).
	void    setFrozen(Var v, bool b);
	//! Marks/unmarks v as input variable.
	void    setInput(Var v, bool b) { set(v, VarInfo::Input, b); }
	//! Marks/unmarks v as output variable.
	void    setOutput(Var v, bool b) { set(v, VarInfo::Output, b); }
	//! Marks/unmarks v as part of negative antecedents.
	void    setNant(Var v, bool b)  { set(v, VarInfo::Nant, b); }
	void    setVarEq(Var v, bool b) { set(v, VarInfo::Eq, b); }
	void    set(Var v, VarInfo::Flag f, bool b) { if (b != varInfo(v).has(f)) varInfo_[v].toggle(f); }
	void    mark(Literal p)         { assert(validVar(p.var())); varInfo_[p.var()].rep |= (VarInfo::Mark_p + p.sign()); }
	void    unmark(Literal p)       { assert(validVar(p.var())); varInfo_[p.var()].rep &= ~(VarInfo::Mark_p + p.sign()); }
	void    unmark(Var v)           { assert(validVar(v)); varInfo_[v].rep &= ~(VarInfo::Mark_p|VarInfo::Mark_n); }
	//! Eliminates the variable v from the problem.
	/*!
	 * \pre v must not occur in any constraint and frozen(v) == false and value(v) == value_free
	 */
	void    eliminate(Var v);

	//! Prepares the master solver so that constraints can be added.
	/*!
	 * Must be called to publish previously added variables to master solver
	 * and before constraints over these variables can be added.
	 * \return The master solver associated with this object.
	 */
	Solver& startAddConstraints(uint32 constraintGuess = 100);

	//! A convenience method for adding facts to the master.
	bool    addUnary(Literal x);
	//! A convenience method for adding binary clauses.
	bool    addBinary(Literal x, Literal y);
	//! A convenience method for adding ternary clauses.
	bool    addTernary(Literal x, Literal y, Literal z);
	//! A convenience method for adding constraints to the master.
	void    add(Constraint* c);
	//! Add weak constraint :~ x.first \[x.second\@p\].
	void    addMinimize(WeightLiteral x, weight_t p);
	//! Returns a pointer to an optimized representation of all minimize constraints in this problem.
	MinPtr  minimize();
	//! List of output predicates and/or variables.
	Output  output;
	//! Set of heuristic modifications.
	DomTab  heuristic;
	//! Requests a special variable for tagging volatile knowledge in multi-shot solving.
	/*!
	 * The step variable is created on the next call to endInit() and removed on the next
	 * call to unfreeze().
	 * Once the step variable S is set, learnt constraints containing ~S are
	 * considered to be "volatile" and removed on the next call to unfreeze().
	 * For this to work correctly, S shall be a root assumption during search.
	 */
	void    requestStepVar();
	//! Finishes initialization of the master solver.
	/*!
	 * The function must be called once before search is started. After endInit()
	 * was called, previously added solvers can be attached to the
	 * shared context and learnt constraints may be added to solver.
	 * \param attachAll If true, also calls attach() for all solvers that were added to this object.
	 * \return If the constraints are initially conflicting, false. Otherwise, true.
	 * \note
	 * The master solver can't recover from top-level conflicts, i.e. if endInit()
	 * returned false, the solver is in an unusable state.
	 * \post frozen()
	 */
	bool    endInit(bool attachAll = false);
	//@}

	/*!
	 * \name (Parallel) solving
	 * Functions to be called during (parallel) solving.
	 *
	 * \note If not otherwise noted, the functions in this group can be safely called
	 * from multiple threads.
	 * @{ */
	//! Returns the active step literal (see requestStepVar()).
	Literal  stepLiteral() const { return step_; }
	//! Attaches the solver with the given id to this object.
	/*!
	 * \note It is safe to attach multiple solvers concurrently
	 * but the master solver shall not change during the whole operation.
	 *
	 * \pre hasSolver(id)
	 */
	bool     attach(uint32 id) { return attach(*solver(id)); }
	bool     attach(Solver& s);

	//! Detaches the solver with the given id from this object.
	/*!
	 * The function removes any tentative constraints from s.
	 * Shall be called once after search has stopped.
	 * \note The function is concurrency-safe w.r.t to different solver objects,
	 *       i.e. in a parallel search different solvers may call detach()
	 *       concurrently.
	 */
	void     detach(uint32 id, bool reset = false) { return detach(*solver(id), reset); }
	void     detach(Solver& s, bool reset = false);

	DistrPtr distributor;/*!< Distributor object to use for distribution of learnt constraints.*/

	uint32   winner()             const   { return share_.winner; }
	void     setWinner(uint32 sId)        { share_.winner = std::min(sId, concurrency()); }

	//! Simplifies the problem constraints w.r.t the master's assignment.
	void     simplify(LitVec::size_type trailStart, bool shuffle);
	//! Removes the constraint with the given idx from the master's db.
	void     removeConstraint(uint32 idx, bool detach);
	//! Removes all minimize constraints from this object.
	void     removeMinimize();

	//! Adds the given short implication to the short implication graph if possible.
	/*!
	 * \return
	 *   - > 0 if implication was added.
	 *   - < 0 if implication can't be added because allowImplicit() is false for ct.
	 *   - = 0 if implication is subsumed by some constraint in the short implication graph.
	 */
	int         addImp(ImpGraph::ImpType t, const Literal* lits, ConstraintType ct);
	//! Returns the number of learnt short implications.
	uint32      numLearntShort()     const { return btig_.numLearnt(); }
	ImpGraphRef shortImplications()  const { return btig_; }
	void               report(const Event& ev)                 const { if (progress_) progress_->dispatch(ev);  }
	bool               report(const Solver& s, const Model& m) const { return !progress_ || progress_->onModel(s, m); }
	void               report(const char* what, const Solver* s = 0) const;
	void               report(Event::Subsystem sys)            const;
	void               warn(const char* what)                  const;
	ReportMode         reportMode()                            const { return static_cast<ReportMode>(share_.report); }
	void               initStats(Solver& s)                    const;
	SolverStats&       solverStats(uint32 sId)                 const; // stats of solver i
	const SolverStats& accuStats(SolverStats& out)             const; // accumulates all solver stats in out
	MinPtr             minimizeNoCreate()                      const;
	//@}
private:
	SharedContext(const SharedContext&);
	SharedContext& operator=(const SharedContext&);
	bool    unfreezeStep();
	Literal addStepLit();
	typedef SingleOwnerPtr<Configuration> Config;
	typedef PodVector<VarInfo>::type      VarVec;
	void    setPreproMode(uint32 m, bool b);
	struct Minimize;
	ProblemStats stats_;         // problem statistics
	VarVec       varInfo_;       // varInfo[v] stores info about variable v
	ImpGraph     btig_;          // binary-/ternary implication graph
	Config       config_;        // active configuration
	SolverVec    solvers_;       // solvers associated with this context
	Minimize*    mini_;          // pointer to set of weak constraints
	LogPtr       progress_;      // event handler or 0 if not used
	Literal      step_;          // literal for tagging enumeration/step constraints
	uint32       lastTopLevel_;  // size of master's top-level after last init
	struct Share {               // Additional data
		uint32 count   :10;        //   max number of objects sharing this object
		uint32 winner  :10;        //   id of solver that terminated the search
		uint32 shareM  : 3;        //   physical sharing mode
		uint32 shortM  : 1;        //   short clause mode
		uint32 solveM  : 1;        //   solve mode
		uint32 frozen  : 1;        //   is adding of problem constraints allowed?
		uint32 seed    : 1;        //   set seed of new solvers
		uint32 satPreM : 2;        //   preprocessing mode
		uint32 report  : 2;        //   report mode
		uint32 reserved: 1;
		Share() : count(1), winner(0), shareM((uint32)ContextParams::share_auto), shortM(0), solveM(0), frozen(0), seed(0), satPreM(0), report(0) {}
	}            share_;
};
//@}
}
#endif
