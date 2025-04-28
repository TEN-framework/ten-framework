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

#ifndef CLASP_LOGIC_PROGRAM_TYPES_H_INCLUDED
#define CLASP_LOGIC_PROGRAM_TYPES_H_INCLUDED
#ifdef _MSC_VER
#pragma once
#endif
/*!
 * \file
 * \brief Basic types for working with a logic program.
 */
#include <clasp/claspfwd.h>
#include <clasp/literal.h>
#include <potassco/rule_utils.h>
#include <functional>
namespace Potassco {
	typedef Clasp::PodVector<Lit_t>::type       LitVec;
	typedef Clasp::PodVector<WeightLit_t>::type WLitVec;
}
namespace Clasp {
class ClauseCreator;
using Potassco::idMax;
namespace Asp {
typedef PodVector<PrgAtom*>::type AtomList;
typedef PodVector<PrgBody*>::type BodyList;
typedef PodVector<PrgDisj*>::type DisjList;
const   ValueRep value_weak_true = 3; /**< true but no proof */
using Potassco::Atom_t;
using Potassco::Id_t;
/*!
 * \addtogroup asp
 */
//@{
//! A node of a program-dependency graph.
/*!
 * A node represents a relevant part in a logic program. Each node
 * has at least a literal and a value.
 */
class PrgNode {
public:
	//! Supported node types.
	enum Type { Atom = 0u, Body = 1u, Disj = 2u };
	static const uint32 noScc  = (1u << 27)-1;
	static const uint32 noNode = (1u << 28)-1;
	static const uint32 noLit  = 1;
	//! Creates a new node that corresponds to a literal that is false.
	explicit PrgNode(uint32 id, bool checkScc = true);
	//! Is the node still relevant or removed() resp. eq()?
	bool relevant() const { return eq_ == 0; }
	//! Was the node removed?
	bool removed()  const { return eq_ != 0 && id_ == noNode; }
	//! Ignore the node during scc checking?
	bool ignoreScc()const { return noScc_ != 0;  }
	//! Returns true if this node is equivalent to some other node.
	/*!
	 * If eq() is true, the node is no longer relevant and must not be used any further.
	 * The only sensible operation is to call id() in order to get the id of the node
	 * that is equivalent to this node.
	 */
	bool eq()       const { return eq_ != 0 && id_ != noNode; }
	bool seen()     const { return seen_ != 0; }
	//! Returns true if node has an associated variable in a solver.
	bool hasVar()   const { return litId_ != noLit; }
	//! Returns the variable associated with this node or sentVar if no var is associated with this node.
	Var  var()      const { return litId_ >> 1; }
	//! Returns the literal associated with this node or a sentinel literal if no var is associated with this node.
	Literal  literal()   const { return Literal::fromId(litId_); }
	//! Returns the value currently assigned to this node.
	ValueRep value()     const { return val_; }
	//! Returns the current id of this node.
	uint32   id()        const { return id_;  }
	//! Returns the literal that must be true in order to fulfill the truth-value of this node.
	Literal   trueLit()  const {
		return value() == value_free
			? lit_true()
			: literal() ^ (value() == value_false);
	}

	/*!
	 * \name implementation functions
	 * Low-level implementation functions. Use with care and only if you
	 * know what you are doing!
	 */
	//@{
	void setLiteral(Literal x)   { litId_ = x.id(); }
	void clearLiteral(bool clVal){ litId_ = noLit; if (clVal) val_ = value_free; }
	void setValue(ValueRep v)    { val_   = v; }
	void setEq(uint32 eqId)      { id_    = eqId; eq_ = 1; seen_ = 1; }
	void setIgnoreScc(bool b)    { noScc_ = (uint32)b; }
	void markRemoved()           { if (!eq()) setEq(noNode); }
	void setSeen(bool b)         { seen_  = uint32(b); }
	void resetId(uint32 id, bool seen) {
		id_ = id;
		eq_ = 0;
		seen_ = (uint32)seen;
	}
	bool assignValueImpl(ValueRep v, bool noWeak) {
		if (v == value_weak_true && noWeak) { v = value_true; }
		if (value() == value_free || v == value() || (value() == value_weak_true && v == value_true)) {
			setValue(v);
			return true;
		}
		return v == value_weak_true && value() == value_true;
	}
	//@}
protected:
	uint32 litId_ : 31; // literal-id in solver
	uint32 noScc_ :  1; // ignore during scc checks?
	uint32 id_    : 28; // own id/eq-id/root-id/ufs-id
	uint32 val_   :  2; // assigned value
	uint32 eq_    :  1; // removed or eq to some other node?
	uint32 seen_  :  1; // marked as seen?
private:
	PrgNode(const PrgNode&);
	PrgNode& operator=(const PrgNode&);
};
typedef PrgNode::Type NodeType;
//! An edge of a program-dependency graph.
/*!
 * Currently, clasp distinguishes four types of edges:
 *  - a Normal edge stipulates an implication between body and head,
 *    i.e. tableau-rules FTA and BFA for atoms.
 *  - a Choice edge only stipulates a support.
 *  - a Gamma edge is like a Normal edge that is only considered during
 *    nogood creation but ignored in the dependency graph.
 *  - a GammaChoice edge is like a Gamma edge but only stipulates a support.
 * The head of a rule is either an atom or a disjunction.
 */
struct PrgEdge {
	//! Type of edge.
	enum Type { Normal = 0, Gamma = 1, Choice = 2, GammaChoice = 3 };
	static PrgEdge noEdge() { PrgEdge x; x.rep = UINT32_MAX; return x; }

	template <class NT>
	static PrgEdge newEdge(const NT& n, Type eType) {
		// 28-bit node id, 2-bit node type, 2-bit edge type
		PrgEdge x = { (n.id() << 4) | (static_cast<uint32>(n.nodeType()) << 2) | eType };
		return x;
	}
	//! Returns the id of the adjacent node.
	uint32   node()     const { return rep >> 4; }
	//! Returns the type of this edge.
	Type     type()     const { return Type(rep & 3u); }
	//! Returns the type of adjacent node.
	NodeType nodeType() const { return NodeType((rep >> 2) & 3u); }
	//! Returns true if edge has normal semantic (normal edge or gamma edge).
	bool     isNormal() const { return (rep & 2u) == 0; }
	//! Returns true if edge has choice semantic.
	bool     isChoice() const { return (rep & 2u) != 0; }
	//! Returns true if the edge is a gamma (aux normal) edge.
	bool     isGamma()  const { return (rep & 1u) != 0; }
	//! Returns true if the adjacent node is a body.
	bool     isBody()   const { return nodeType() == PrgNode::Body; }
	//! Returns true if the adjacant node is an atom.
	bool     isAtom()   const { return nodeType() == PrgNode::Atom; }
	//! Returns true if the adjacent node is a disjunction.
	bool     isDisj()   const { return nodeType() == PrgNode::Disj; }
	bool     operator< (PrgEdge rhs) const { return rep < rhs.rep; }
	bool     operator==(PrgEdge rhs) const { return rep == rhs.rep; }
	uint32 rep;
};

typedef PrgEdge::Type               EdgeType;
typedef const PrgEdge*              EdgeIterator;
typedef bk_lib::pod_vector<PrgEdge> EdgeVec;
inline bool isChoice(EdgeType t) { return t >= PrgEdge::Choice;  }

using Potassco::Body_t;
using Potassco::Head_t;
using Potassco::WeightLitSpan;
typedef Potassco::Rule_t Rule;
//! A class for translating extended rules to normal rules.
class RuleTransform {
public:
	//! Interface that must be implemented to get the result of a transformation.
	struct ProgramAdapter {
		virtual Atom_t newAtom() = 0;
		virtual void addRule(const Rule& r) = 0;
	protected: ~ProgramAdapter() {}
	};
	//! Supported transformation strategies.
	enum Strategy { strategy_default, strategy_no_aux, strategy_allow_aux };
	RuleTransform(ProgramAdapter& prg);
	RuleTransform(LogicProgram& prg);
	~RuleTransform();
	//! Transforms the given (extended) rule to a set of normal rules.
	uint32 transform(const Rule& r, Strategy s = strategy_default);
private:
	RuleTransform(const RuleTransform&);
	RuleTransform& operator=(const RuleTransform&);
	struct Impl;
	Impl* impl_;
};

//! A set of flags used during rule simplification.
class AtomState {
public:
	static const uint8 pos_flag    = 0x1u; //!< In positive body of active rule
	static const uint8 neg_flag    = 0x2u; //!< In negative body of active rule
	static const uint8 head_flag   = 0x4u; //!< In normal head of active rule
	static const uint8 choice_flag = 0x8u; //!< In choice head of active rule
	static const uint8 disj_flag   = 0x10u;//!< In disjunctive head of active rule
	static const uint8 rule_mask   = 0x1Fu;//!< In active rule
	static const uint8 fact_flag   = 0x20u;//!< Atom is a fact (sticky)
	static const uint8 false_flag  = 0x40u;//!< Atom is false  (sticky)
	static const uint8 simp_mask   = 0x7fu;//!< In active rule or assigned
	static const uint8 dom_flag    = 0x80u;//!< Var of atom is a dom var (sticky)
	AtomState() {}
	void  swap(AtomState& o) { state_.swap(o.state_); }
	//! Does t.node() appear in the head of the active rule?
	bool  inHead(PrgEdge t)         const { return isSet(t.node(), headFlag(t)); }
	bool  inHead(Atom_t atom)       const { return isSet(atom, head_flag); }
	//! Does p appear in the body of the active rule?
	bool  inBody(Literal p)         const { return isSet(p.var(), pos_flag+p.sign()); }
	bool  isSet(Var v, uint8 f)     const { return v < state_.size() && (state_[v] & f) != 0; }
	bool  isFact(Var v)             const { return isSet(v, fact_flag); }
	//! Mark v as a head of the active rule.
	void  addToHead(Atom_t v)             { set(v, head_flag); }
	void  addToHead(PrgEdge t)            { set(t.node(), headFlag(t)); }
	//! Mark p as a literal contained in the active rule.
	void  addToBody(Literal p)            { set(p.var(), pos_flag+p.sign()); }

	void  set(Var v, uint8 f)             { grow(v); state_[v] |= f; }
	void  clear(Var v, uint8 f)           { if (v < state_.size()) { state_[v] &= ~f; } }
	void  clearRule(Var v)                { clear(v, rule_mask); }
	void  clearHead(PrgEdge t)            { clear(t.node(), headFlag(t)); }
	void  clearBody(Literal p)            { clear(p.var(), pos_flag+p.sign()); }
	void  resize(uint32 sz)               { state_.resize(sz); }

	template <class IT>
	bool  allMarked(IT first, IT last, uint8 f) const {
		for (; first != last; ++first) {
			if (!isSet(*first, f)) return false;
		}
		return true;
	}
	bool  inBody(const Literal* first, const Literal* last) const {
		bool all = true;
		for (; first != last && (all = inBody(*first)) == true; ++first) { ; }
		return all;
	}
private:
	typedef PodVector<uint8>::type StateVec;
	void  grow(Var v)                { if (v >= state_.size()) { state_.resize(v+1); } }
	uint8 headFlag(PrgEdge t) const  {
		return t.isAtom() ? (head_flag << uint8(t.isChoice())) : disj_flag;
	}
	StateVec state_;
};

//! A head node of a program-dependency graph.
/*!
 * A head node is either an atom or a disjunction
 * and stores its possible supports.
 */
class PrgHead : public PrgNode {
public:
	enum Simplify { no_simplify = 0, force_simplify = 1 };
	typedef EdgeIterator sup_iterator;

	//! Is the head part of the (simplified) program?
	bool         inUpper()    const  { return relevant() && upper_ != 0;  }
	//! Is this head an atom?
	bool         isAtom()     const  { return isAtom_ != 0; }
	//! Number of supports (rules) for this head.
	uint32       supports()   const  { return supports_.size();  }
	sup_iterator supps_begin()const  { return supports_.begin(); }
	sup_iterator supps_end()  const  { return supports_.end();   }
	//! External atom (or defined in a later incremental step)?
	bool         frozen()     const  { return freeze_ != uint32(freeze_no); }
	//! If frozen(), value to assume during solving.
	ValueRep     freezeValue()const  { return static_cast<ValueRep>(freeze_ - uint32(freeze_ != 0)); }
	//! If frozen(), literal to assume during solving.
	Literal      assumption() const  { return freeze_ > uint32(freeze_free) ? literal() ^ (freeze_ == freeze_false) : lit_true(); }
	//! Adds r as support edge for this node.
	void         addSupport(PrgEdge r){ addSupport(r, force_simplify); }
	void         addSupport(PrgEdge r, Simplify s);
	//! Removes r from the head's list of supports.
	void         removeSupport(PrgEdge r);
	void         clearSupports();
	void         clearSupports(EdgeVec& to) { to.swap(supports_); clearSupports(); }
	//! Removes any superfluous/irrelevant supports.
	bool         simplifySupports(LogicProgram& prg, bool strong, uint32* numDiffSupps = 0);
	//! Assigns the value v to this head.
	bool         assignValue(ValueRep v) { return assignValueImpl(v, ignoreScc() && !frozen()); }
	/*!
	 * \name implementation functions
	 * Low-level implementation functions. Use with care and only if you
	 * know what you are doing!
	 */
	//@{
	void         setInUpper(bool b)   { upper_ = (uint32)b; }
	void         markDirty()          { dirty_ = 1; }
	void         assignVar(LogicProgram& prg, PrgEdge it, bool allowEq = true);
	NodeType     nodeType()  const    { return isAtom() ? PrgNode::Atom : PrgNode::Disj; }
	//@}
protected:
	enum FreezeState { freeze_no = 0u, freeze_free = 1u, freeze_true = 2u, freeze_false = 3u };
	//! Creates a new node that corresponds to a literal that is false.
	explicit PrgHead(uint32 id, NodeType t, uint32 data = 0, bool checkScc = true);
	bool      backpropagate(LogicProgram& prg, ValueRep val, bool bpFull);
	EdgeVec supports_;  // possible supports (body or disjunction)
	uint32 data_  : 27; // number of atoms in disjunction or scc of atom
	uint32 upper_ :  1; // in (simplified) program?
	uint32 dirty_ :  1; // is list of supports dirty?
	uint32 freeze_:  2; // incremental freeze state
	uint32 isAtom_:  1; // is this head an atom?
};

//! An atom in a logic program.
/*!
 * An atom stores the list of bodies depending on it.
 * Furthermore, once strongly-connected components are identified,
 * atoms store their SCC-number. All trivial SCCs are represented
 * with the special SCC-number PrgNode::noScc.
 */
class PrgAtom : public PrgHead {
public:
	enum Dependency { dep_pos = 0, dep_neg = 1, dep_all = 2 };
	typedef LitVec::const_iterator dep_iterator;
	explicit PrgAtom(uint32 id, bool checkScc = true);
	NodeType     nodeType() const { return PrgNode::Atom; }
	//! Strongly connected component of this node.
	uint32       scc() const { return data_; }
	//! If eq(), stores the literal that is eq to this atom.
	Literal      eqGoal(bool sign) const;
	//! Returns true if atom belongs to a disjunctive head.
	bool         inDisj()   const;
	/*!
	 * \name forward dependencies (bodies containing this atom)
	 */
	//@{
	dep_iterator deps_begin()                   const { return deps_.begin(); }
	dep_iterator deps_end()                     const { return deps_.end();   }
	bool         hasDep(Dependency d)           const;
	void         addDep(Id_t bodyId, bool pos);
	void         removeDep(Id_t bodyId, bool pos);
	void         clearDeps(Dependency d);
	//@}

	/*!
	 * \name implementation functions
	 * Low-level implementation functions. Use with care and only if you
	 * know what you are doing!
	 */
	//@{
	void setEqGoal(Literal x);
	bool propagateValue(LogicProgram& prg, bool backprop);
	bool addConstraints(const LogicProgram& prg, ClauseCreator& c);
	void setScc(uint32 scc)    { data_ = scc; }
	void markFrozen(ValueRep v){ freeze_ = v + freeze_free; }
	void clearFrozen()         { freeze_ = freeze_no; markDirty(); }
	//@}
private:
	LitVec deps_; // bodies depending on this atom
};

//! A (rule) body in a logic program.
class PrgBody : public PrgNode {
public:
	typedef EdgeIterator   head_iterator;
	typedef const Literal* goal_iterator;

	//! Creates a new body node and (optionally) connects it to its predecessors (i.e. atoms).
	/*!
	 * \param prg     The program in which the new body is used.
	 * \param id      The id for the new body node.
	 * \param rule    The rule for which a body node is to be created.
	 * \param pos     Positive body size.
	 * \param addDeps If true, add an edge between each atom subgoal and the new node.
	 */
	static PrgBody* create(LogicProgram& prg, uint32 id, const Rule& rule, uint32 pos, bool addDeps);
	//! Destroys a body node created via create().
	void     destroy();
	Body_t   type()  const { return Body_t(static_cast<Body_t>(type_)); }
	//! Returns the number of atoms in the body.
	uint32   size()  const { return size_; }
	bool     noScc() const { return size() == 0 || goal(0).sign(); }
	//! Returns the bound of this body, or size() if the body is a normal body.
	weight_t bound() const { if (type() == Body_t::Normal) return (weight_t)size(); else return hasWeights() ? sumData()->bound : aggData().bound; }
	//! Returns the sum of the subgoals weights, or size() if the body is not a sum with weights.
	weight_t sumW()  const { return static_cast<weight_t>(!hasWeights() ? (weight_t)size() : sumData()->sumW); }
	//! Returns the idx'th subgoal as a literal.
	Literal  goal(uint32 idx)  const { assert(idx < size()); return *(goals_begin()+idx); }
	//! Returns the weight of the idx'th subgoal.
	weight_t weight(uint32 idx)const { assert(idx < size()); return !hasWeights() ? 1 : sumData()->weights[idx]; }
	//! Returns true if the body node is supported.
	/*!
	 * A normal body is supported, iff all of its positive subgoals are supported.
	 * A count/sum body is supported if the sum of the weights of the supported positive +
	 * the sum of the negative weights is >= lowerBound().
	 */
	bool     isSupported() const { return unsupp_ <= 0; }
	//! Returns true if this body defines any head.
	bool     hasHeads()    const { return isSmallHead() ? head_ != 0 : !largeHead()->empty(); }
	bool     inRule()      const { return hasHeads() || freeze_; }

	head_iterator heads_begin() const { return isSmallHead() ? smallHead()       : largeHead()->begin(); }
	head_iterator heads_end()   const { return isSmallHead() ? smallHead()+head_ : largeHead()->end(); }
	goal_iterator goals_begin() const { return const_cast<PrgBody*>(this)->goals_begin(); }
	goal_iterator goals_end()   const { return goals_begin() + size(); }
	//! Adds a rule edge between this body and the given head.
	/*!
	 * \note
	 *   The function also adds a corresponding back edge to the head.
	 * \note
	 *   Adding a head invalidates the set property for the heads of this body.
	 *   To restore it, call simplifyHeads()
	 */
	void     addHead(PrgHead* h, EdgeType t);
	//! Simplifies the heads of this body and establishes set property.
	/*!
	 * Removes superfluous heads and sets the body to false if for some atom a
	 * in the head of this body B, Ta -> FB. In that case, all heads atoms are removed, because
	 * a false body can't define any atom.
	 * If strong is true, removes head atoms that are not associated with a variable.
	 * \return
	 *    setValue(value_false) if setting a head of this body to true would
	 *    make the body false (i.e. the body is a selfblocker). Otherwise, true.
	 */
	bool     simplifyHeads(LogicProgram& prg, bool strong);
	bool     mergeHeads(LogicProgram& prg, PrgBody& heads, bool strong, bool simplify = true);
	void     removeHead(PrgHead* h, EdgeType t);
	bool     hasHead(PrgHead* h, EdgeType t) const;
	//! Simplifies the body, i.e. its predecessors-lists.
	/*!
	 * - removes true/false atoms from B+/B- resp.
	 * - removes/merges duplicate subgoals
	 * - checks whether body must be false (e.g. contains false/true atoms in B+/B- resp. or contains p and ~p)
	 *
	 * \pre prg.getBody(id()) == this
	 *
	 * \param[in] prg    The program containing this body.
	 * \param[in] strong If true, treats atoms that have no variable associated as false.
	 * \param[out] eqId  The id of a body in prg that is equivalent to this body.
	 *
	 * \return
	 *  - true if simplification was successful
	 *  - false if simplification detected a conflict
	 */
	bool     simplifyBody(LogicProgram& prg, bool strong, uint32* eqId = 0);
	//! Calls simplifyBody() and/or simplifyHeads() if necessary.
	bool     simplify(LogicProgram& prg, bool strong, uint32* eqId = 0) {
		return simplifyBody(prg, strong, eqId) && simplifyHeads(prg, strong);
	}
	bool     toData(const LogicProgram& prg, Potassco::RuleBuilder& out) const;
	//! Notifies the body node about the fact that its positive subgoal v is supported.
	/*!
	 * \return true if the body is now also supported, false otherwise.
	 */
	bool     propagateSupported(Var /* v */);
	//! Propagates the assignment of subgoal p.
	bool     propagateAssigned(LogicProgram& prg, Literal p, ValueRep v);
	//! Propagates the assignment of a head.
	bool     propagateAssigned(LogicProgram& prg, PrgHead* h, EdgeType t);
	//! Propagates the value of this body.
	bool     propagateValue(LogicProgram& prg, bool backprop);
	bool     propagateValue(LogicProgram& prg);
	bool     addConstraints(const LogicProgram& prg, ClauseCreator& c);
	void     markDirty()      { sBody_ = 1; }
	void     markHeadsDirty() { sHead_ = 1; }
	void     markFrozen()     { freeze_= 1; }
	void     clearHeads();
	bool     resetSupported();
	void     assignVar(LogicProgram& prg);
	bool     assignValue(ValueRep v) { return assignValueImpl(v, noScc()); }
	uint32   scc(const LogicProgram& prg) const;
	bool     hasWeights()  const { return type() == Body_t::Sum; }
	void     clearRule(AtomState& rs) const {
		for (head_iterator it = heads_begin(), end = heads_end(); it != end; ++it) {
			rs.clearRule(it->node());
		}
		for (const Literal* it = goals_begin(), *end = it + size(); it != end; ++it) {
			rs.clearRule(it->var());
		}
	}
	NodeType nodeType() const  { return PrgNode::Body; }
private:
	static const uint32 maxSize = (1u<<25)-1;
	typedef unsigned char byte_t;
POTASSCO_WARNING_BEGIN_RELAXED
	struct SumData {
		enum { LIT_OFFSET = sizeof(void*)/sizeof(uint32) };
		static SumData* create(uint32 size, weight_t bnd, weight_t ws);
		void     destroy();
		weight_t bound;
		weight_t sumW;
		weight_t weights[0];
	};
	struct Agg {
		union {
			SumData* sum;
			weight_t bound;
		};
		Literal lits[0];
	};
	struct Norm { Literal lits[0]; };
	PrgBody(uint32 id, LogicProgram& prg, const Potassco::LitSpan& lits, uint32 pos, bool addDeps);
	PrgBody(uint32 id, LogicProgram& prg, const Potassco::Sum_t& sum, bool hasWeights, uint32 pos, bool addDeps);
	void init(Body_t t, uint32 sz);
	~PrgBody();
	uint32   findLit(const LogicProgram& prg, Literal p) const;
	bool     normalize(const LogicProgram& prg, weight_t bound, weight_t sumW, weight_t reachW, uint32& hashOut);
	void     prepareSimplifyHeads(LogicProgram& prg, AtomState& rs);
	bool     simplifyHeadsImpl(LogicProgram& prg, PrgBody& target, AtomState& rs, bool strong);
	bool     superfluousHead(const LogicProgram& prg, const PrgHead* head, PrgEdge it, const AtomState& rs) const;
	bool     blockedHead(PrgEdge it, const AtomState& rs) const;
	void     addHead(PrgEdge h);
	bool     eraseHead(PrgEdge h);
	bool     isSmallHead() const { return head_ != 3u; }
	byte_t*  data()        const { return const_cast<byte_t*>(data_); }
	PrgEdge* smallHead()   const { return const_cast<PrgEdge*>(headData_.sm); }
	EdgeVec* largeHead()   const { return headData_.ext; }
	SumData* sumData()     const { return aggData().sum;}
	Agg&     aggData()     const { return *reinterpret_cast<Agg*>(data()); }
	Literal* goals_begin()       { return type() == Body_t::Normal ? reinterpret_cast<Norm*>(data())->lits  : aggData().lits; }
	Literal* goals_end()         { return goals_begin() + size(); }

	uint32    size_   : 25; // |B|
	uint32    head_   :  2; // simple or extended head?
	uint32    type_   :  2; // body type
	uint32    sBody_  :  1; // simplify body?
	uint32    sHead_  :  1; // simplify head?
	uint32    freeze_ :  1; // keep body even if it does not occur in a rule?
	weight_t  unsupp_;      // <= 0 -> body is supported
	union Head {
		PrgEdge  sm[2];
		EdgeVec* ext;
	}         headData_;    // successors of this body
	byte_t    data_[0];     // empty or one of Agg|Norm
POTASSCO_WARNING_END_RELAXED
};
//! The head of a disjunctive rule.
class PrgDisj : public PrgHead {
public:
	typedef const Var* atom_iterator;
	//! Constructor for disjunctions.
	static PrgDisj* create(uint32 id, const Potassco::AtomSpan& head);
	//! Destroys a disjunction created via create().
	void          destroy();
	void          detach(LogicProgram& prg, bool full = true);
	//! Number of atoms in disjunction.
	uint32        size()               const { return data_; }
	atom_iterator begin()              const { return atoms_; }
	atom_iterator end()                const { return atoms_ + size(); }
	//! Propagates the assignment of an atom in this disjunction.
	bool          propagateAssigned(LogicProgram& prg, PrgHead* at, EdgeType t);
private:
	explicit PrgDisj(uint32 id, const Potassco::AtomSpan& head);
	~PrgDisj();
POTASSCO_WARNING_BEGIN_RELAXED
	Var atoms_[0]; // atoms in disjunction
POTASSCO_WARNING_END_RELAXED
};

inline ValueRep getMergeValue(const PrgNode* lhs, const PrgNode* rhs) {
	return static_cast<ValueRep>(std::min(static_cast<ValueRep>(lhs->value()-1), static_cast<ValueRep>(rhs->value()-1)) + 1);
}

template <class NT>
bool mergeValue(NT* lhs, NT* rhs){
	ValueRep mv = getMergeValue(lhs, rhs);
	return (lhs->value() == mv || lhs->assignValue(mv))
	  &&   (rhs->value() == mv || rhs->assignValue(mv));
}

//! A class for computing strongly connected components of the positive atom-body dependency graph.
class SccChecker {
public:
	SccChecker(LogicProgram& prg, AtomList& sccAtoms, uint32 startScc);
	uint32 sccs()              const { return sccs_; }
	void  visit(PrgBody* body)       { visitDfs(body, PrgNode::Body); }
	void  visit(PrgAtom* atom)       { visitDfs(atom, PrgNode::Atom); }
	void  visit(PrgDisj* disj)       { visitDfs(disj, PrgNode::Disj); }
private:
	struct Call {
		uintp    node;
		uint32   min;
		uint32   next;
	};
	typedef PodVector<Call>::type  CallStack;
	typedef PodVector<uintp>::type NodeStack;
	static uintp    packNode(PrgNode* n, NodeType t)  { return reinterpret_cast<uintp>(n) + uintp(t); }
	static PrgNode* unpackNode(uintp n)               { return reinterpret_cast<PrgNode*>(n & ~uintp(3u));}
	static bool     isNode(uintp n, NodeType t)       { return (n & 3u) == uintp(t); }
	bool  doVisit(PrgNode* n, bool seen = true) const { return !n->ignoreScc() && n->relevant() && n->hasVar() && (!seen || !n->seen()); }
	void  visitDfs(PrgNode* n, NodeType t);
	bool  recurse(Call& c);
	bool  onNode(PrgNode* n, NodeType t, Call& c, uint32 data);
	void  addCall(PrgNode* n, NodeType t, uint32 next, uint32 min = 0) {
		Call c = {packNode(n, t), min, next};
		callStack_.push_back(c);
	}
	CallStack     callStack_;
	NodeStack     nodeStack_;
	LogicProgram* prg_;
	AtomList*     sccAtoms_;
	uint32        count_;
	uint32        sccs_;
};
//! A set of ids of strongly connected components having at least one head-cycle.
struct NonHcfSet : private PodVector<uint32>::type {
public:
	typedef PodVector<uint32>::type base_type;
	typedef base_type::const_iterator const_iterator;
	using base_type::begin;
	using base_type::end;
	using base_type::size;
	NonHcfSet() : config(0) {}
	void add(uint32 scc) {
		iterator it = std::lower_bound(begin(), end(), scc);
		if (it == end() || *it != scc) { insert(it, scc); }
	}
	bool find(uint32 scc) const {
		const_iterator it = scc != PrgNode::noScc ? std::lower_bound(begin(), end(), scc) : end();
		return it != end() && *it == scc;
	}
	Configuration* config;
};
//@}
} }
#endif
