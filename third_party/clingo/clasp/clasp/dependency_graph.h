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

#ifndef CLASP_DEPENDENCY_GRAPH_H_INCLUDED
#define CLASP_DEPENDENCY_GRAPH_H_INCLUDED

#ifdef _MSC_VER
#pragma once
#endif

#include <clasp/logic_program.h>
#include <clasp/solver_strategies.h>
#include <algorithm>
namespace Clasp {
class Solver;
class SharedContext;
struct SolverStats;
//! Event type used to signal a (partial) check in disjunctive solving.
struct SolveTestEvent : SolveEvent<SolveTestEvent> {
	SolveTestEvent(const Solver& s, uint32 hcc, bool partial);
	int    result;     //!< -1: before test, 0: unstable, 1: stable
	uint32 hcc     :31;//!< hcc under test
	uint32 partial : 1;//!< partial test?
	uint64 confDelta;  //!< conflicts before test
	uint64 choiceDelta;//!< choices before test
	double time;       //!< time for test

	uint64 conflicts() const;
	uint64 choices()   const;
};
struct LoopReason_t {
	enum Type { Explicit = 0u, Implicit = 1u, };
};
typedef LoopReason_t::Type LoopType;

namespace Asp {
//! (Positive) Body-Atom-Dependency Graph.
/*!
 * \ingroup shared_con
 *
 * Represents the PBADG of a logic program. Once initialized, the
 * PBDAG is static and read-only and thus can be shared between multiple solvers.
 *
 * \note Initialization is *not* thread-safe, i.e. must be done only once by one thread.
 */
class PrgDepGraph {
public:
	enum NonHcfMapType {
		map_old = 0,
		map_new = 1
	};
	explicit PrgDepGraph(NonHcfMapType m = map_old);
	~PrgDepGraph();
	typedef uint32 NodeId;
	//! Type for storing a non head-cycle-free component of a disjunctive program.
	class NonHcfComponent {
	public:
		explicit NonHcfComponent(uint32 id, const PrgDepGraph& dep, SharedContext& generator, Configuration* c, uint32 scc, const VarVec& atoms, const VarVec& bodies);
		~NonHcfComponent();
		void assumptionsFromAssignment(const Solver& generator, LitVec& assumptionsOut) const;
		bool test(const Solver& generator, const LitVec& assumptions, VarVec& unfoundedOut)  const;
		bool simplify(const Solver& generator) const;
		const SharedContext& ctx() const { return *prg_; }
		void update(const SharedContext& generator);
		uint32 id()  const { return id_; }
		uint32 scc() const { return scc_; }
	private:
		friend class PrgDepGraph;
		NonHcfComponent(const NonHcfComponent&);
		NonHcfComponent& operator=(const NonHcfComponent&);
		class ComponentMap;
		const PrgDepGraph* dep_;
		SharedContext*     prg_;
		ComponentMap*      comp_;
		uint32             id_;
		uint32             scc_;
	};
	//! A class for storing statistics on checking of non head-cycle-free components.
	class NonHcfStats {
	public:
		NonHcfStats(PrgDepGraph& g, uint32 level, bool inc);
		~NonHcfStats();
		void accept(StatsVisitor& out, bool final) const;
		void startStep(uint32 statsLevel);
		void endStep();
		void addTo(StatsMap& problem, StatsMap& solving, StatsMap* accu) const;
	private:
		friend class PrgDepGraph;
		void addHcc(const NonHcfComponent&);
		void removeHcc(const NonHcfComponent&);
		NonHcfStats(const NonHcfStats&);
		NonHcfStats& operator=(const NonHcfStats&);
		struct Data;
		PrgDepGraph* graph_;
		Data*        data_;
	};
	typedef PodVector<NonHcfComponent*>::type ComponentVec;
	typedef ComponentVec::const_iterator NonHcfIter;
	//! Base type for nodes.
	struct Node {
		Node(Literal l = Literal(0, false), uint32 sc = PrgNode::noScc)
			: lit(l), scc(sc), data(0), adj_(0), sep_(0) {}
		Literal lit;       // literal of this node
		uint32  scc   : 28;// scc of this node
		uint32  data  :  4;// additional atom/body data
		NodeId* adj_;      // list of adjacent nodes
		NodeId* sep_;      // separates successor/predecessor nodes
	};
	//! An atom node.
	/*!
	 * The PBDAG stores a node of type AtomNode for each non-trivially connected atom.
	 * The predecessors of an AtomNode are the bodies that define the atom. Its successors
	 * are those bodies from the same SCC that contain the atom positively.
	 */
	struct AtomNode : public Node {
		enum Property { property_in_choice = 1u, property_in_disj = 2u, property_in_ext = 4u, property_in_non_hcf = 8u };
		AtomNode() {}
		void  set(Property p)         { data |= (uint32)p; }
		void  setProperties(uint32 f) { assert(f < 8); data |= f; }
		//! Contained in the head of a choice rule?
		bool          inChoice()     const { return (data & property_in_choice) != 0; }
		//! Contained in the head of a non-hcf disjunctive rule?
		bool          inDisjunctive()const { return (data & property_in_disj) != 0; }
		//! Contained in an extended body?
		bool          inExtended()   const { return (data& property_in_ext) != 0; }
		//! Contained in a non-hcf SCC?
		bool          inNonHcf()     const { return (data & property_in_non_hcf) != 0; }
		//! Bodies (i.e. predecessors): bodies from other SCCs precede those from same SCC.
		const NodeId* bodies_begin() const { return adj_; }
		const NodeId* bodies_end()   const { return sep_; }
		NodeId        body(uint32 i) const { return bodies_begin()[i]; }
		//! Successors from same SCC [B1,...Bn, idMax].
		/*!
		 * \note If extended() is true, the atom is adjacent to some extended body.
		 * In that case, the returned list looks like this:
		 * [Bn1, ..., Bnj, idMax, Bext1, pos1, ..., Bextn, posn, idMax], where
		 * each Bni is a normal body, each Bexti is an extended body and posi is the
		 * position of this atom in Bexti.
		 */
		const NodeId* succs()        const { return sep_; }
		//! Calls the given function object p once for each body containing this atom.
		template <class P>
		void visitSuccessors(const P& p) const {
			const NodeId* s = succs();
			for (; *s != idMax; ++s) { p(*s); }
			if (inExtended()) {
				for (++s; *s != idMax; s += 2) { p(*s, *(s+1)); }
			}
		}
	};
	enum { sentinel_atom = 0u };

	//! A body node.
	/*!
	 * The PBDAG stores a node of type BodyNode for each body that defines
	 * a non-trivially connected atom.
	 * The predecessors of a BodyNode are the body's subgoals.
	 * Its successors are the heads that are defined by the body.
	 * \note Normal bodies only store the positive subgoals from the same SCC, while
	 * extended rule bodies store all subgoals. In the latter case, the positive subgoals from
	 * the same SCC are stored as AtomNodes. All other subgoals are stored as literals.
	 */
	struct BodyNode : public Node {
		enum Flag { flag_has_bound = 1u, flag_has_weights = 2u, flag_has_delta = 4u, flag_seen = 8u };
		explicit BodyNode(PrgBody* b, uint32 scc) : Node(b->literal(), scc) {
			if (scc == PrgNode::noScc || b->type() == Body_t::Normal) {
				data = 0;
			}
			else if (b->type() == Body_t::Count){ data = flag_has_bound; }
			else if (b->type() == Body_t::Sum)  { data = flag_has_bound | flag_has_weights;   }
			else                                { assert("UNKNOWN BODY TYPE!\n"); }
		}
		bool seen() const { return (data & flag_seen) != 0; }
		void seen(bool b) { if (b) data |= flag_seen; else data &= ~uint32(flag_seen); }

		//! Heads (i.e. successors): atoms from same SCC precede those from other SCCs.
		/*!
		 * \note Disjunctive heads are stored in flattened atom-lists, where the
		 *       lists are terminated on both ends with the special sentinal atom 0.
		 *       E.g. given
		 *        x :- B.
		 *        y :- B.
		 *       a|b:- B.
		 *       a|c:- B.
		 *       would result in: [x,y,0,a,b,0,0,a,c,0]
		 */
		const NodeId* heads_begin() const { return adj_; }
		const NodeId* heads_end()   const { return sep_ - extended(); }
		//! Any disjunctive heads?
		bool          delta()       const { return (data & flag_has_delta) != 0; }
		//! Predecessors from same SCC [a1,...an, idMax].
		/*!
		 * \note If extended() is true, the body stores all its subgoals and preds looks
		 * like this: [a1, [w1], ..., aj, [wj], idMax, l1, [w1], ..., lk, [wk], idMax], where
		 * each ai is an atom from the same SCC, each li is a literal of a subgoal from
		 * other SCCs and wi is an optional weight (only for weight rules).
		 */
		const NodeId* preds()       const { assert(scc != PrgNode::noScc); return sep_; }
		//! Returns idx of atomId in preds.
		uint32        get_pred_idx(NodeId atomId) const {
			const uint32 inc = pred_inc();
			uint32 idx = 0;
			for (const NodeId* x = preds(); *x != idMax; x += inc, ++idx) {
				if (*x == atomId) return idx;
			}
			return idMax;
		}
		NodeId        get_pred(uint32 idx) const { return *(preds() + (idx*pred_inc())); }
		//! Increment to jump from one pred to the next.
		uint32        pred_inc()           const { return 1 + sum(); }
		//! Weight of ith subgoal.
		/*!
		 * \pre i in [0, num_preds())
		 */
		uint32        pred_weight(uint32 i, bool ext) const {
			return !sum()
				? 1
				: *(preds() + (i*pred_inc()) + (1+uint32(ext)));
		}
		//! Number of predecessors (counting external subgoals).
		uint32        num_preds() const {
			if (scc == PrgNode::noScc) return 0;
			uint32 p = 0;
			const NodeId*  x = preds();
			const uint32 inc = pred_inc();
			for (; *x != idMax; x += inc) { ++p; }
			x += extended();
			for (; *x != idMax; x += inc) { ++p; }
			return p;
		}
		//! Is the body an extended body?
		bool          extended()const { return (data & flag_has_bound) != 0u; }
		//! Is the body a sum body?
		bool          sum()     const { return (data & flag_has_weights) != 0u; }
		//! Bound of extended body.
		weight_t      ext_bound() const { return sep_[-1]; }
	};
	//! Adds SCCs to the graph.
	/*!
	 * \param prg       The logic program for which the dependency graph is to be created.
	 * \param sccAtoms  Atoms of the logic program that are strongly connected.
	 * \param nonHcfs   Sorted list of non-hcf sccs
	 */
	void addSccs(LogicProgram& prg, const AtomList& sccAtoms, const NonHcfSet& nonHcfs);

	//! Removes inactive non-hcfs.
	void simplify(const Solver& s);
	//! Number of atoms in graph.
	uint32 numAtoms() const { return (uint32)atoms_.size(); }
	//! Number of bodies in graph.
	uint32 numBodies()const { return (uint32)bodies_.size(); }
	//! Sum of atoms and bodies.
	uint32 nodes()    const { return numAtoms()+numBodies(); }

	//! Returns AtomNode of atom with given id.
	const AtomNode& getAtom(NodeId atomId) const {
		assert(atomId < atoms_.size());
		return atoms_[atomId];
	}
	NodeId id(const AtomNode& n) const { return static_cast<uint32>(&n - &atoms_[0]); }
	//! Returns BodyNode of body with given id.
	const BodyNode& getBody(NodeId bodyId) const {
		assert(bodyId < bodies_.size());
		return bodies_[bodyId];
	}
	//! Calls the given function object p once for each body-literal.
	template <class P>
	void visitBodyLiterals(const BodyNode& n, const P& p) const {
		const NodeId*  x = n.preds();
		const uint32 inc = n.pred_inc();
		uint32         i = 0;
		for (; *x != idMax; x += inc, ++i) { p(getAtom(*x).lit, i, false); }
		x += n.extended();
		for (; *x != idMax; x += inc, ++i) { p(Literal::fromRep(*x), i, true); }
	}
	NonHcfIter   nonHcfBegin() const { return components_.begin(); }
	NonHcfIter   nonHcfEnd()   const { return components_.end(); }
	uint32       numNonHcfs()  const { return (uint32)components_.size(); }
	NonHcfStats* nonHcfStats() const { return stats_; }
	NonHcfStats* enableNonHcfStats(uint32 level, bool incremental);
private:
	typedef PodVector<AtomNode>::type AtomVec;
	typedef PodVector<BodyNode>::type BodyVec;
	PrgDepGraph(const PrgDepGraph&);
	PrgDepGraph& operator=(const PrgDepGraph&);
	inline bool    relevantPrgAtom(const Solver& s, PrgAtom* a) const;
	inline bool    relevantPrgBody(const Solver& s, PrgBody* b) const;
	NonHcfMapType  nonHcfMapType() const { return static_cast<NonHcfMapType>(mapType_); }
	NodeId         createBody(PrgBody* b, uint32 bScc);
	NodeId         createAtom(Literal lit, uint32 aScc);
	NodeId         addBody(const LogicProgram& prg, PrgBody*);
	NodeId         addDisj(const LogicProgram& prg, PrgDisj*);
	uint32         addHeads(const LogicProgram& prg, PrgBody*, VarVec& atoms) const;
	uint32         getAtoms(const LogicProgram& prg, PrgDisj*, VarVec& atoms) const;
	void           addPreds(const LogicProgram& prg, PrgBody*, uint32 bScc, VarVec& preds) const;
	void           initBody(uint32 id, const VarVec& preds, const VarVec& atHeads);
	void           initAtom(uint32 id, uint32 prop, const VarVec& adj, uint32 preds);
	void           addNonHcf(uint32 id, SharedContext& ctx, Configuration* c, uint32 scc);
	AtomVec        atoms_;
	BodyVec        bodies_;
	ComponentVec   components_;
	NonHcfStats*   stats_;
	uint32         seenComponents_ : 31;
	uint32         mapType_        :  1;
};
} // namespace Asp

//! External dependency graph.
/*!
 * \ingroup shared_con
 *
 * Represents external dependencies explicitly given by the user.
 * For example, via aspif edge directives or the graph block in extended dimacs format.
 * \note Initialization is *not* thread-safe, i.e. must be done only once by one thread.
 */
class ExtDepGraph {
public:
	struct Arc {
		Literal lit;
		uint32  node[2];
		uint32 tail() const { return node[0]; }
		uint32 head() const { return node[1]; }
		static Arc create(Literal x, uint32 nodeX, uint32 nodeY) { Arc a = {x, {nodeX, nodeY}}; return a; }
	};
	struct Inv {
		uint32  tail() const { return rep >> 1; }
		Literal lit;
		uint32  rep;
	};
	template <unsigned x>
	struct CmpArc {
		bool operator()(const Arc& lhs, uint32 n) const { return lhs.node[x] < n; }
		bool operator()(uint32 n, const Arc& rhs) const { return n < rhs.node[x]; }
		bool operator()(const Arc& lhs, const Arc& rhs) const {
			return lhs.node[x] < rhs.node[x]
			|| (lhs.node[x] == rhs.node[x]  && lhs.node[1-x] < rhs.node[1-x]);
		}
	};
	explicit ExtDepGraph(uint32 numNodeGuess = 0);
	~ExtDepGraph();
	void   addEdge(Literal lit, uint32 startNode, uint32 endNode);
	void   update();
	uint32 finalize(SharedContext& ctx);
	bool   frozen() const;
	uint64 attach(Solver& s, Constraint& p, uint64 genId);
	void   detach(Solver* s, Constraint& p);

	const Arc& arc(uint32 id)       const { return fwdArcs_[id]; }
	const Arc* fwdBegin(uint32 n)   const {
		uint32 X = nodes_[n].fwdOff;
		return validOff(X) ? &fwdArcs_[X] : 0;
	}
	const Arc* fwdNext(const Arc* a)const { assert(a); return a[0].node[0] == a[1].node[0] ? ++a : 0; }
	const Inv* invBegin(uint32 n)   const {
		uint32 X = nodes_[n].invOff;
		return validOff(X) ? &invArcs_[X] : 0;
	}
	const Inv* invNext(const Inv* a)const { assert(a); return (a->rep & 1u) == 1u ? ++a : 0; }
	uint32     nodes()              const { return maxNode_; }
	uint32     edges()              const { return comEdge_; }
	bool       validNode(uint32 n)  const { return n < maxNode_; }
private:
	ExtDepGraph(const ExtDepGraph&);
	ExtDepGraph& operator=(const ExtDepGraph&);
	struct Node {
		uint32 fwdOff;
		uint32 invOff;
	};
	typedef PodVector<Arc>::type  ArcVec;
	typedef PodVector<Inv>::type  InvVec;
	typedef PodVector<Node>::type NodeVec;
	bool validOff(uint32 n) const {
		return n != UINT32_MAX;
	}
	ArcVec  fwdArcs_; // arcs ordered by node id
	InvVec  invArcs_; // inverse arcs ordered by node id
	NodeVec nodes_;   // data for the nodes of this graph
	uint32  maxNode_; // nodes have ids in the range [0, maxNode_)
	uint32  comEdge_; // number of edges committed
	uint32  genCnt_;  // generation count (for incremental updates)
};

//! Acyclicity checker that operates on a ExtDepGraph.
/*!
 * \ingroup propagator
 * \see M. Gebser, T. Janhunen, and J. Rintanen: "SAT Modulo Graphs: Acyclicity"
 */
class AcyclicityCheck : public PostPropagator {
public:
	enum Strategy {
		prop_full     = 0, // forward and backward check with clause generation
		prop_full_imp = 1, // forward and backward check without clause generation
		prop_fwd      = 2, // only forward check
	};
	enum { PRIO = PostPropagator::priority_reserved_ufs + 1 };
	typedef ExtDepGraph DependencyGraph;
	explicit AcyclicityCheck(DependencyGraph* graph);
	~AcyclicityCheck();
	void   setStrategy(Strategy p);
	void   setStrategy(const SolverParams& opts);
	// base interface
	uint32 priority() const { return uint32(PRIO); }
	bool   init(Solver&);
	void   reset();
	bool   propagateFixpoint(Solver& s, PostPropagator* ctx);
	bool   isModel(Solver& s);
	bool   valid(Solver& s);
	void   destroy(Solver* s, bool detach);
private:
	AcyclicityCheck(const AcyclicityCheck&);
	AcyclicityCheck& operator=(const AcyclicityCheck&);
	struct Parent {
		static Parent create(Literal x, uint32 n) { Parent p = {x, n}; return p; }
		Literal lit;
		uint32  node;
	};
	enum { config_bit = 2 };
	struct ReasonStore;
	typedef DependencyGraph::Arc    Arc;
	typedef DependencyGraph::Inv     Inv;
	typedef PodQueue<Arc>            EdgeQueue;
	typedef PodVector<uint32>::type  TagVec;
	typedef PodVector<Parent>::type  ParentVec;
	bool   dfsForward(Solver& s,  const Arc& e);
	bool   dfsBackward(Solver& s, const Arc& e);
	void   setParent(Var node, const Parent& p){ parent_[node] = p; }
	void   pushVisit(Var node, uint32 tv)      { nStack_.push_back(node); tags_[node] = tv; }
	bool   visited(Var node, uint32 tv) const  { return tags_[node] == tv; }
	uint32 startSearch();
	void   addClauseLit(Solver& s, Literal p);
	void   setReason(Literal p, LitVec::const_iterator first, LitVec::const_iterator end);
	// -------------------------------------------------------------------------------------------
	// constraint interface
	PropResult propagate(Solver&, Literal, uint32& eId) {
		todo_.push(graph_->arc(eId));
		return PropResult(true, true);
	}
	void reason(Solver& s, Literal, LitVec&);
	Strategy strategy() const { return static_cast<Strategy>(strat_ & 3u); }
	DependencyGraph* graph_;  // my graph
	Solver*          solver_; // my solver
	ReasonStore*     nogoods_;// stores at most one reason per literal
	uint32           strat_;  // active propagation strategy
	uint32           tagCnt_; // generation counter for searches
	EdgeQueue        todo_;   // edges that recently became enabled
	TagVec           tags_;   // tag for each node
	ParentVec        parent_; // parents for each node
	VarVec           nStack_; // node stack for df-search
	LitVec           reason_; // reason for conflict/implication
	uint64           genId_;  // generation identifier
};

}
#endif

