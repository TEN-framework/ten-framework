//
// Copyright (c) 2013-2017 Benjamin Kaufmann
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

#ifndef CLASP_LOGIC_PROGRAM_H_INCLUDED
#define CLASP_LOGIC_PROGRAM_H_INCLUDED

#ifdef _MSC_VER
#pragma once
#endif
#include <clasp/logic_program_types.h>
#include <clasp/program_builder.h>
#include <clasp/statistics.h>
#include POTASSCO_EXT_INCLUDE(unordered_set)

namespace Clasp { namespace Asp {
/*!
 * \file
 * \defgroup asp Asp
 * \brief Classes and functions for defining logic programs.
 * \ingroup problem
 */
//@{

//! A struct for counting program rules and directives.
struct RuleStats {
	typedef uint32& Ref_t;
	typedef uint32 const& CRef_t;
	//! Rules and directives counted by this object.
	enum Key {
		Normal    = Head_t::Disjunctive,//!< Normal or disjunctive rules.
		Choice    = Head_t::Choice,     //!< Choice rules.
		Minimize  ,                     //!< Distinct minimize constraints.
		Acyc      ,                     //!< Edge directives.
		Heuristic ,                     //!< Heuristic directives.
		Key__num
	};
	//! Returns a string representation of the given key.
	static const char* toStr(int k);
	//! Returns the number of keys distinguished by this type.
	static uint32      numKeys()   { return Key__num; }
	//! Updates the number of rules of the given type.
	void up(Key k, int amount)     { key[k] += static_cast<uint32>(amount); }
	//! Returns the number of rules of the given type.
	Ref_t  operator[](int k)       { return key[k]; }
	//! @copydoc operator[](int k)
	CRef_t operator[](int k) const { return key[k]; }
	//! Returns the sum of all rules.
	uint32 sum()             const;
	uint32 key[Key__num]; //!< @private
};
//! A struct for counting distinct program bodies.
struct BodyStats {
	typedef uint32& Ref_t;
	typedef uint32 const& CRef_t;
	//! Body types distinguished by this object.
	typedef Body_t Key;
	//! Returns a string representation of the given key.
	static const char* toStr(int k);
	//! Returns the number of keys distinguished by this type.
	static uint32      numKeys()   { return Body_t::eMax + 1; }
	//! Updates the number of bodies of the given type.
	void up(Key k, int amount)     { key[k] += static_cast<uint32>(amount); }
	//! Returns the number of bodies of the given type.
	Ref_t  operator[](int k)       { return key[k]; }
	//! @copydoc operator[](int k)
	CRef_t operator[](int k) const { return key[k]; }
	//! Returns the sum of all bodies.
	uint32 sum()             const;
	uint32 key[Body_t::eMax + 1]; //!< @private
};

//! A type for maintaining a set of program statistics.
class LpStats {
public:
	LpStats() { reset(); }
	void   reset();
	//! Returns the sum of all equivalences.
	uint32 eqs()             const { return eqs(Var_t::Atom) + eqs(Var_t::Body) + eqs(Var_t::Hybrid); }
	//! Returns the number of equivalences of the given type.
	uint32 eqs(VarType t)    const { return eqs_[t-1]; }
	//! Increments the number of equivalences of the given type.
	void   incEqs(VarType t)       { ++eqs_[t-1]; }
	//! Computes *this += o.
	void   accu(const LpStats& o);
	RuleStats rules[2];        /**< RuleStats (initial, final). */
	BodyStats bodies[2];       /**< BodyStats (initial, final). */
	uint32    atoms;           /**< Number of program atoms.    */
	uint32    auxAtoms;        /**< Number of aux atoms created. */
	uint32    disjunctions[2]; /**< Number of disjunctions (initial, non-hcf). */
	uint32    sccs;            /**< How many strongly connected components? */
	uint32    nonHcfs;         /**< How many non head-cycle free components?*/
	uint32    gammas;          /**< How many non-hcf gamma rules. */
	uint32    ufsNodes;        /**< How many nodes in the positive dependency graph? */
	// StatisticObject
	static uint32 size();
	static const char* key(uint32 i);
	StatisticObject at(const char* k) const;
private:
	uint32 eqs_[3];
};
//! Exception type for signaling an invalid incremental program update.
class RedefinitionError : public std::logic_error {
public:
	explicit RedefinitionError(unsigned atomId, const char* atomName = "");
	unsigned atom() const { return atomId_; }
private:
	unsigned atomId_;
};
using Potassco::TheoryData;
struct MapLit_t {
	POTASSCO_ENUM_CONSTANTS(MapLit_t, Raw = 0, Refined = 1);
};

//! A class for defining a logic program.
/*!
 * Use this class to specify a logic program. Once the program is completly defined,
 * call endProgram() to load the logic program into a SharedContext object.
 */
class LogicProgram : public ProgramBuilder {
public:
	LogicProgram();
	~LogicProgram();
	//! Defines the possible modes for handling extended rules, i.e. choice, cardinality, and weight rules.
	enum ExtendedRuleMode {
		mode_native           = 0, //!< Handle extended rules natively.
		mode_transform        = 1, //!< Transform extended rules to normal rules.
		mode_transform_choice = 2, //!< Transform only choice rules to normal rules.
		mode_transform_card   = 3, //!< Transform only cardinality rules to normal rules.
		mode_transform_weight = 4, //!< Transform cardinality- and weight rules to normal rules.
		mode_transform_scc    = 5, //!< Transform recursive cardinality- and weight rules to normal rules.
		mode_transform_nhcf   = 6, //!< Transform cardinality- and weight rules in non-hcf components to normal rules.
		mode_transform_integ  = 7, //!< Transform cardinality-based integrity constraints.
		mode_transform_dynamic= 8  //!< Heuristically decide whether or not to transform a particular extended rule.
	};

	//! Options for the Asp-Preprocessor.
	struct AspOptions {
		static const uint32 MAX_EQ_ITERS = static_cast<uint32>( (1u<<25)-1 );
		typedef ExtendedRuleMode TrMode;
		AspOptions() {
			std::memset(this, 0, sizeof(AspOptions));
			iters = 5;
		}
		AspOptions& iterations(uint32 it)   { iters   = it;return *this;}
		AspOptions& depthFirst()            { dfOrder = 1; return *this;}
		AspOptions& backpropagate()         { backprop= 1; return *this;}
		AspOptions& noScc()                 { noSCC   = 1; return *this;}
		AspOptions& noEq()                  { iters   = 0; return *this;}
		AspOptions& disableGamma()          { noGamma = 1; return *this;}
		AspOptions& ext(ExtendedRuleMode m) { erMode  = m; return *this;}
		AspOptions& distinctTrue()          { distTrue= 1; return *this;}
		TrMode erMode;       //!< How to handle extended rules?
		uint32 iters    : 26;//!< Number of iterations in eq-preprocessing or 0 to disable.
		uint32 noSCC    :  1;//!< Disable scc checking?
		uint32 suppMod  :  1;//!< Disable scc checking and compute supported models.
		uint32 dfOrder  :  1;//!< Visit nodes in eq-preprocessing in depth-first order?
		uint32 backprop :  1;//!< Enable backpropagation during preprocessing?
		uint32 oldMap   :  1;//!< Use old and larger mapping for disjunctive programs.
		uint32 noGamma  :  1;//!< Disable creation of (shifted) gamma rules for non-hcf disjunctions?
		uint32 distTrue :  1;//!< Add distinct true var for each step instead of one for all steps.
	};

	/*!
	 * \name Step control functions
	 */
	//@{

	//! Starts the definition of a logic program.
	LogicProgram& start(SharedContext& ctx, const AspOptions& opts = AspOptions()) {
		startProgram(ctx);
		setOptions(opts);
		return *this;
	}
	//! Sets the mode for handling extended rules (default: mode_native).
	void setExtendedRuleMode(ExtendedRuleMode m) { opts_.ext(m); }
	//! Enable distinct true vars for incremental steps.
	void enableDistinctTrue();
	//! Sets preprocessing options.
	void setOptions(const AspOptions& opts);
	//! Sets the configuration to be used for checker solvers in disjunctive LP solving.
	void setNonHcfConfiguration(Configuration* c){ nonHcfs_.config = c; }

	//! Unfreezes a currently frozen program and starts an incremental step.
	/*!
	 * If a program is to be defined incrementally, this function must be called
	 * exactly once for each step before any new rules or atoms are added.
	 * \note Program update only works correctly under the following assumptions:
	 *  - Atoms introduced in step i are either:
	 *    - solely defined in step i OR,
	 *    - marked as frozen in step i and solely defined in step i+k OR,
	 *    - forced to false by a compute statement in step 0.
	 *
	 * \pre The program is either frozen or at step 0.
	 * \post The program is no longer frozen and calling program mutating functions is valid again.
	 * \throws std::logic_error precondition is violated.
	 * \note The function is an alias for ProgramBuilder::updateProgram().
	 */
	bool update() { return updateProgram(); }

	//! Finishes the definition of the logic program (or its current increment).
	/*!
	 * Applies program mutating operations issued in the current step and transforms
	 * the new program into the solver's internal representation.
	 *
	 * \return false if the program is conflicting, true otherwise.
	 *
	 * \post
	 *  - If true is returned, the program is considered to be "frozen" and calling
	 *    program mutating functions is invalid until the next call to update().
	 *  - If false is returned, the state of the object is undefined and start()
	 *    and dispose() are the only remaining valid operations.
	 *  .
	 * \note The function is an alias for ProgramBuilder::endProgram().
	 */
	bool end() { return endProgram(); }

	//! Visits the the simplified program by notifying out on its elements.
	void accept(Potassco::AbstractProgram& out);

	//! Disposes (parts of) the internal representation of the logic program.
	/*!
	 * \param forceFullDispose If set to true, the whole program is disposed. Otherwise,
	 *  only the rules (of the current step) are disposed but atoms and any incremental
	 *  control data remain.
	 */
	void dispose(bool forceFullDispose);

	//! Clones the program and adds it to the given ctx.
	/*
	 * \pre The program is currently frozen.
	 */
	bool clone(SharedContext& ctx);

	//@}

	/*!
	 * \name Program mutating functions
	 *
	 * Functions in this group shall only be called if the program is currently not
	 * frozen. That is, only between the call to start() (resp. update() if in
	 * incremental setting) and end(). A std::logic_error is raised if this precondition is violated.
	 *
	 */
	//@{

	//! Adds a new atom to the program and returns the new atom's id.
	Atom_t newAtom();

	//! Sets atomId as the last input atom of the current step.
	/*!
	* All (new or existing) atoms with a larger id than atomId
	* are considered to be auxiliary and automatically removed before
	* a new incremental step is started.
	*
	* \pre atomId >= startAtom()
	* \post startAuxAtom() == atomId + 1
	*/
	void   setMaxInputAtom(uint32 atomId);

	//! Adds a new conjunctive condition to the program.
	/*!
	 * \param cond A (possibly empty) list of atom literals.
	 * \return The id of the new condition, which can be later passed to
	 * extractCondition() or getLiteral().
	 */
	Id_t   newCondition(const Potassco::LitSpan& cond);

	//! Adds the given string to the problem's output table.
	/*!
	 * \param str The string to add.
	 * \param cond The condition under which str should be considered part of a model.
	 */
	LogicProgram& addOutput(const ConstString& str, const Potassco::LitSpan& cond);
	LogicProgram& addOutput(const ConstString& str, Id_t cond);

	//! Adds the given atoms to the set of projection variables.
	LogicProgram& addProject(const Potassco::AtomSpan& atoms);

	//! Protects an otherwise undefined atom from preprocessing.
	/*!
	 * If the atom is defined in this or a previous step, the operation has no effect.
	 * \note If atomId is not yet known, an atom with the given id is implicitly created.
	 * \note The second parameter defines the assumption that shall hold during solving, i.e.
	 *       - posLit(atomId), if value is value_true,
	 *       - negLit(atomId), if value is value_false, or
	 *       - no assumption, if value is value_free.
	 *
	 * \see ProgramBuilder::getAssumptions(LitVec&) const;
	 */
	LogicProgram& freeze(Atom_t atomId, ValueRep value = value_false);

	//! Removes any protection from the given atom.
	/*!
	 * If the atom is defined in this or a previous step, the operation has no effect.
	 * \note
	 *   - The effect is logically equivalent to adding a rule atomId :- false.
	 *   - A call to unfreeze() always overwrites a call to freeze() even if the
	 *     latter comes after the former
	 *   .
	 */
	LogicProgram& unfreeze(Atom_t atomId);

	//! Adds the given rule (or integrity constraint) to the program.
	/*!
	 * \pre The the rule does not define an atom from a previous incremental step.
	 *
	 * Simplifies the given rule and adds it to the program if it
	 * is neither tautological (e.g. a :- a) nor contradictory (e.g. a :- b, not b).
	 * Atoms in the simplified rule that are not yet known are implicitly created.
	 *
	 * \throws RedefinitionError if the precondition is violated.
	 * \note If the head of the simplified rule mentions an atom from a previous step,
	 *       that atom shall either be frozen or false. In the former case,
	 *       unfreeze() is implicitly called. In the latter case, the rule is interpreted
	 *       as an integrity constraint.
	 */
	LogicProgram& addRule(const Rule& rule);
	LogicProgram& addRule(Head_t ht, const Potassco::AtomSpan& head, const Potassco::LitSpan& body);
	LogicProgram& addRule(Head_t ht, const Potassco::AtomSpan& head, Potassco::Weight_t bound, const Potassco::WeightLitSpan& lits);
	LogicProgram& addRule(Potassco::RuleBuilder& rb);
	//! Adds the given minimize statement.
	/*!
	 * \param prio The priority of the minimize statement.
	 * \param lits The literals to minimize.
	 * \note All minimize statements of the same priority are merged into one.
	 */
	LogicProgram& addMinimize(weight_t prio, const Potassco::WeightLitSpan& lits);

	//! Adds an edge to the extended (user-defined) dependency graph.
	LogicProgram& addAcycEdge(uint32 n1, uint32 n2, const Potassco::LitSpan& condition) { return addAcycEdge(n1, n2, newCondition(condition)); }
	LogicProgram& addAcycEdge(uint32 n1, uint32 n2, Id_t cond);

	//! Adds a conditional domain heuristic directive to the program.
	LogicProgram& addDomHeuristic(Atom_t atom, DomModType t, int bias, unsigned prio, const Potassco::LitSpan& condition) { return addDomHeuristic(atom, t, bias, prio, newCondition(condition)); }
	LogicProgram& addDomHeuristic(Atom_t atom, DomModType t, int bias, unsigned prio, Id_t cond);
	//! Adds an unconditional domain heuristic directive to the program.
	LogicProgram& addDomHeuristic(Atom_t atom, DomModType t, int bias, unsigned prio);

	//! Forces the given literals to be true during solving.
	/*!
	 * Assumptions are retracted on the next program update.
	 */
	LogicProgram& addAssumption(const Potassco::LitSpan& cube);

	//! Adds or updates the given external atom.
	/*!
	* \see LogicProgram::freeze(Atom_t atomId, ValueRep value);
	* \see LogicProgram::unfreeze(Atom_t atomId);
	*/
	LogicProgram& addExternal(Atom_t atomId, Potassco::Value_t value);

	//! Returns an object for adding theory data to this program.
	TheoryData&   theoryData();
	//@}

	/*!
	 * \name Query functions
	 *
	 * Functions in this group are useful to query important information
	 * once the program is frozen, i.e. after end() was called.
	 * They do not throw exceptions.
	 */
	 //@{
	//! Returns whether the program can be represented in internal smodels format.
	bool   supportsSmodels() const;
	//! Returns whether the program is to be defined incrementally.
	bool   isIncremental()   const { return incData_ != 0; }
	//! Returns whether the program contains any minimize statements.
	bool   hasMinimize()     const { return !minimize_.empty(); }
	//! Returns whether the program contains any theory data.
	bool   hasTheoryData()   const { return theory_ != 0; }
	//! Returns the number of atoms in the logic program.
	uint32 numAtoms()        const { return (uint32)atoms_.size()-1; }
	//! Returns the number of bodies in the current (slice of the) logic program.
	uint32 numBodies()       const { return (uint32)bodies_.size(); }
	//! Returns the number of disjunctive heads.
	uint32 numDisjunctions() const { return (uint32)disjunctions_.size(); }
	//! Returns the id of the first atom of the current step.
	Atom_t startAtom()       const { return input_.lo; }
	//! Returns an id one past the last valid atom id in the program.
	Atom_t endAtom()         const { return numAtoms() + 1; }
	//! Returns the id of the first atom that is not an input atom or endAtom() if no such atoms exists.
	Atom_t startAuxAtom()    const;
	//! Returns whether a is an atom in the (simplified) program.
	bool   inProgram(Atom_t a)  const;
	//! Returns whether a is an external atom, i.e. is frozen in this step.
	bool   isExternal(Atom_t a) const;
	//! Returns whether a occurs in the head of a rule.
	bool   isDefined(Atom_t a)  const;
	//! Returns whether a is a fact.
	bool   isFact(Atom_t a)     const;
	//! Returns the solver literal that is associated with the given atom or condition.
	/*!
	 * \pre id is the id of a valid atom literal or was previously returned by newCondition().
	 * \note Until end() is called, the function returns lit_false() for
	 *       all atoms and conditions defined in the current step.
	 * \note For an atom literal x with Potassco::atom(x) == a,
	 *       getLiteral(Potassco::id(x)) returns
	 *        getLiteral(a), iff x ==  a, or
	 *       ~getLiteral(a), iff x == -a.
	 *
	 * \note If mode is MapLit_t::Raw, the function simply returns the literal that
	 *       was set during preprocessing. Otherwise, it also considers equivalences
	 *       induced by domain heuristic directives and/or step-local true vars.
	 *
	 * \see enableDistinctTrue()
	 */
	Literal getLiteral(Id_t id, MapLit_t mode = MapLit_t::Raw) const;
	//! Returns the atom literals belonging to the given condition.
	/*!
	 * \pre cId was previously returned by newCondition() in the current step.
	 */
	bool    extractCondition(Id_t cId, Potassco::LitVec& lits) const;


	//! Maps the given unsat core of solver literals to original program assumptions.
	/*!
	 * \param solverCore An unsat core found when solving under ProgramBuilder::getAssumptions().
	 * \param prgLits The given unsat core expressed in terms of program literals.
	 * \return Whether unsatCore was successfully mapped.
	 */
	bool extractCore(const LitVec& unsatCore, Potassco::LitVec& prgLits) const;

	LpStats stats; //!< Statistics of the current step.
	//@}

	/*!
	 * \name Implementation functions
	 * Low-level implementation functions. Use with care and only if you
	 * know what you are doing!
	 */
	//@{
	typedef VarVec::const_iterator                VarIter;
	typedef PrgHead*const*                        HeadIter;
	typedef std::pair<EdgeIterator, EdgeIterator> EdgeRange;
	typedef std::pair<HeadIter, HeadIter>         HeadRange;
	struct SRule {
		SRule() : hash(0), pos(0), bid(varMax) {}
		uint32 hash; // hash value of the body
		uint32 pos;  // positive size of body
		uint32 bid;  // id of existing body or varMax
	};
	const AspOptions& options()    const { return opts_; }
	bool       hasConflict()       const { return getTrueAtom()->literal() != lit_true(); }
	bool       ok()                const { return !hasConflict() && ProgramBuilder::ok(); }
	PrgAtom*   getAtom(Id_t atomId)const { return atoms_[atomId]; }
	PrgHead*   getHead(PrgEdge it) const { return it.isAtom() ? (PrgHead*)getAtom(it.node()) : (PrgHead*)getDisj(it.node()); }
	PrgNode*   getSupp(PrgEdge it) const { return it.isBody() ? (PrgNode*)getBody(it.node()) : (PrgNode*)getDisj(it.node()); }
	Id_t       getRootId(Id_t atom)const { return getEqNode(atoms_, atom); }
	PrgAtom*   getRootAtom(Id_t a) const { return getAtom(getRootId(a)); }
	PrgBody*   getBody(Id_t bodyId)const { return bodies_[bodyId]; }
	Id_t       getEqBody(Id_t b)   const { return getEqNode(bodies_, b); }
	PrgDisj*   getDisj(Id_t disjId)const { return disjunctions_[disjId]; }
	HeadIter   disj_begin()        const { return disjunctions_.empty() ? 0 : reinterpret_cast<HeadIter>(&disjunctions_[0]); }
	HeadIter   disj_end()          const { return disj_begin() + numDisjunctions(); }
	HeadIter   atom_begin()        const { return reinterpret_cast<HeadIter>(&atoms_[0]); }
	HeadIter   atom_end()          const { return atom_begin() + (numAtoms()+1); }
	VarIter    unfreeze_begin()    const { return incData_?incData_->unfreeze.begin() : propQ_.end(); }
	VarIter    unfreeze_end()      const { return incData_?incData_->unfreeze.end()   : propQ_.end(); }
	bool       validAtom(Id_t aId) const { return aId < (uint32)atoms_.size(); }
	bool       validBody(Id_t bId) const { return bId < numBodies(); }
	bool       validDisj(Id_t dId) const { return dId < numDisjunctions(); }
	bool       isFact(PrgAtom* a)  const;
	const char*findName(Atom_t x)  const;
	bool       simplifyRule(const Rule& r, Potassco::RuleBuilder& out, SRule& meta);
	Atom_t     falseAtom();
	VarVec&    getSupportedBodies(bool sorted);
	uint32     update(PrgBody* b, uint32 oldHash, uint32 newHash);
	bool       assignValue(PrgAtom* a, ValueRep v, PrgEdge reason);
	bool       assignValue(PrgHead* h, ValueRep v, PrgEdge reason);
	bool       propagate(bool backprop);
	PrgAtom*   mergeEqAtoms(PrgAtom* a, Id_t rootAtom);
	PrgBody*   mergeEqBodies(PrgBody* b, Id_t rootBody, bool hashEq, bool atomsAssigned);
	bool       propagate()               { return propagate(options().backprop != 0); }
	void       setConflict()             { getTrueAtom()->setLiteral(lit_false()); }
	AtomState& atomState()               { return atomState_; }
	void       addMinimize();
	// ------------------------------------------------------------------------
	// Statistics
	void incTrAux(uint32 n)   { stats.auxAtoms += n; }
	void incEqs(VarType t)    { stats.incEqs(t);     }
	void upStat(RuleStats::Key k, int n = 1){ stats.rules[statsId_].up(k, n); }
	void upStat(Body_t k, int n = 1) { stats.bodies[statsId_].up(k, n); }
	void upStat(Head_t k, int n = 1) { stats.rules[statsId_].up(static_cast<RuleStats::Key>(unsigned(k)), n); }
	// ------------------------------------------------------------------------
	//@}
private:
	LogicProgram(const LogicProgram&);
	LogicProgram& operator=(const LogicProgram&);
	struct DlpTr;
	struct AcycArc { Id_t cond; uint32 node[2]; };
	struct DomRule { uint32 atom : 29; uint32 type : 3; Id_t cond; int16 bias; uint16 prio; };
	struct Eq      { Atom_t var; Literal lit; };
	struct TFilter { bool operator()(const Potassco::TheoryAtom& atom) const; LogicProgram* self; };
	struct Min     { weight_t prio; Potassco::WLitVec lits; };
	struct CmpMin  { bool operator()(const Min* m1, const Min* m2) const { return m1->prio < m2->prio; } };
	typedef Potassco::RuleBuilder           RuleBuilder;
	typedef std::pair<Id_t, ConstString>    ShowPair;
	typedef PodVector<ShowPair>::type       ShowVec;
	typedef PodVector<DomRule>::type        DomRules;
	typedef PodVector<AcycArc>::type        AcycRules;
	typedef PodVector<RuleBuilder*>::type   RuleList;
	typedef PodVector<Min*>::type           MinList;
	typedef PodVector<uint8>::type          SccMap;
	typedef PodVector<Eq>::type             EqVec;
	typedef POTASSCO_EXT_NS::unordered_multimap<uint32, uint32> IndexMap;
	typedef POTASSCO_EXT_NS::unordered_set<Id_t> IdSet;
	typedef IndexMap::iterator              IndexIter;
	typedef std::pair<IndexIter, IndexIter> IndexRange;
	typedef Potassco::WLitVec               LpWLitVec;
	typedef Potassco::LitVec                LpLitVec;
	typedef Range<uint32>                   AtomRange;
	// ------------------------------------------------------------------------
	// virtual overrides
	bool doStartProgram();
	bool doUpdateProgram();
	bool doEndProgram();
	void doGetAssumptions(LitVec& out) const;
	ProgramParser* doCreateParser();
	int  doType() const { return Problem_t::Asp; }
	// ------------------------------------------------------------------------
	// Program definition
	bool     isNew(Atom_t atomId) const { return atomId >= startAtom(); }
	PrgAtom* resize(Atom_t atomId);
	void     pushFrozen(PrgAtom* atom, ValueRep v);
	void     addRule(const Rule& r, const SRule& meta);
	void     addFact(const Potassco::AtomSpan& head);
	void     addIntegrity(const Rule& b, const SRule& meta);
	bool     handleNatively(const Rule& r) const;
	bool     transformNoAux(const Rule& r) const;
	void     freezeTheory();
	void     transformExtended();
	void     transformIntegrity(uint32 nAtoms, uint32 maxAux);
	PrgBody* getBodyFor(const Rule& r, const SRule& m, bool addDeps = true);
	PrgBody* getTrueBody();
	PrgDisj* getDisjFor(const Potassco::AtomSpan& heads, uint32 headHash);
	PrgBody* assignBodyFor(const Rule& r, const SRule& m, EdgeType x, bool strongSimp);
	bool     equalLits(const PrgBody& b, const WeightLitSpan& lits) const;
	bool     simplifyNormal(Head_t ht, const Potassco::AtomSpan& head, const Potassco::LitSpan& body, RuleBuilder& out, SRule& info);
	bool     simplifySum(Head_t ht, const Potassco::AtomSpan& head, const Potassco::Sum_t& body, RuleBuilder& out, SRule& info);
	bool     pushHead(Head_t ht, const Potassco::AtomSpan& head, weight_t slack, RuleBuilder& out);
	ValueRep litVal(const PrgAtom* a, bool pos) const;
	uint32   findBody(uint32 hash, Body_t type, uint32 size, weight_t bound = -1, Potassco::WeightLit_t* wlits = 0);
	uint32   findEqBody(const PrgBody* b, uint32 hash);
	uint32   removeBody(PrgBody* b, uint32 oldHash);
	Literal  getEqAtomLit(Literal lit, const BodyList& supports, Preprocessor& p, const SccMap& x);
	bool     positiveLoopSafe(PrgBody* b, PrgBody* root) const;
	void     prepareExternals();
	void     updateFrozenAtoms();
	template <class C>
	Id_t getEqNode(C& vec, Id_t id)  const {
		if (!vec[id]->eq()) return id;
		PrgNode* n = vec[id], *r;
		Id_t root  = n->id();
		for (r = vec[root]; r->eq(); r = vec[root]) {
			// n == r and r == r' -> n == r'
			assert(root != r->id());
			n->setEq(root = r->id());
		}
		return root;
	}
	bool checkBody(const PrgBody& rhs, Body_t type, uint32 size, weight_t bound) const {
		return (rhs.relevant() || (rhs.eq() && getBody(getEqBody(rhs.id()))->relevant()))
			&& rhs.type() == type && rhs.size() == size && rhs.bound() == bound;
	}
	// ------------------------------------------------------------------------
	// Nogood creation
	void prepareProgram(bool checkSccs);
	void prepareOutputTable();
	void finalizeDisjunctions(Preprocessor& p, uint32 numSccs);
	void prepareComponents();
	bool addConstraints();
	void addAcycConstraint();
	void addDomRules();
	void freezeAssumptions();
	// ------------------------------------------------------------------------
	void deleteAtoms(uint32 start);
	PrgAtom* getTrueAtom() const { return atoms_[0]; }
	RuleBuilder rule_;        // temporary: active rule
	AtomState   atomState_;   // which atoms appear in the active rule?
	IndexMap    bodyIndex_;   // hash -> body id
	IndexMap    disjIndex_;   // hash -> disjunction id
	IndexMap    domEqIndex_;  // maps eq atoms modified by dom heuristic to aux vars
	BodyList    bodies_;      // all bodies
	AtomList    atoms_;       // all atoms
	DisjList    disjunctions_;// all (head) disjunctions
	MinList     minimize_;    // list of minimize rules
	RuleList    extended_;    // extended rules to be translated
	ShowVec     show_;        // shown atoms/conditions
	VarVec      initialSupp_; // bodies that are (initially) supported
	VarVec      propQ_;       // assigned atoms
	VarVec      frozen_;      // list of frozen atoms
	LpLitVec    assume_;      // set of assumptions
	NonHcfSet   nonHcfs_;     // set of non-hcf sccs
	TheoryData* theory_;      // optional map of theory data
	AtomRange   input_;       // input atoms of current step
	int         statsId_;     // which stats to update (0 or 1)
	struct Aux {
		AtomList  scc;          // atoms that are strongly connected
		DomRules  dom;          // list of domain heuristic directives
		AcycRules acyc;         // list of user-defined edges for acyclicity check
		VarVec    project;      // atoms in projection directives
		VarVec    external;     // atoms in external directives
		IdSet     skippedHeads; // heads of rules that have been removed during parsing
	}*          auxData_;     // additional state for handling extended constructs
	struct Incremental  {
		// first: last atom of step, second: true var
		typedef std::pair<uint32, uint32> StepTrue;
		typedef PodVector<StepTrue>::type TrueVec;
		Incremental();
		uint32    startScc;     // first valid scc number in this iteration
		VarVec    unfreeze;     // list of atoms to unfreeze in this step
		VarVec    doms;         // list of atom variables with domain modification
		TrueVec   steps;        // map of steps to true var
	}*          incData_;     // additional state for handling incrementally defined programs
	AspOptions  opts_;        // preprocessing
};
//! Returns the internal solver literal that is associated with the given atom literal.
/*!
 * \pre The prg is frozen and atomLit is a known atom in prg.
 */
inline Literal solverLiteral(const LogicProgram& prg, Potassco::Lit_t atomLit) {
	POTASSCO_REQUIRE(prg.frozen() && prg.validAtom(Potassco::atom(atomLit)));
	return prg.getLiteral(Potassco::id(atomLit));
}
//! Adapts a LogicProgram object to the Potassco::AbstractProgram interface.
class LogicProgramAdapter : public Potassco::AbstractProgram {
public:
	LogicProgramAdapter(LogicProgram& prg);
	void initProgram(bool inc);
	void beginStep();
	void endStep();
	void rule(Potassco::Head_t ht, const Potassco::AtomSpan& head, const Potassco::LitSpan& body);
	void rule(Potassco::Head_t ht, const Potassco::AtomSpan& head, Potassco::Weight_t bound, const Potassco::WeightLitSpan& body);
	void minimize(Potassco::Weight_t prio, const Potassco::WeightLitSpan& lits);
	void project(const Potassco::AtomSpan& atoms);
	void output(const Potassco::StringSpan& str, const Potassco::LitSpan& cond);
	void external(Potassco::Atom_t a, Potassco::Value_t v);
	void assume(const Potassco::LitSpan& lits);
	void heuristic(Potassco::Atom_t a, Potassco::Heuristic_t t, int bias, unsigned prio, const Potassco::LitSpan& cond);
	void acycEdge(int s, int t, const Potassco::LitSpan& cond);
	void theoryTerm(Potassco::Id_t termId, int number);
	void theoryTerm(Potassco::Id_t termId, const Potassco::StringSpan& name);
	void theoryTerm(Potassco::Id_t termId, int cId, const Potassco::IdSpan& args);
	void theoryElement(Potassco::Id_t elementId, const Potassco::IdSpan& terms, const Potassco::LitSpan& cond);
	void theoryAtom(Potassco::Id_t atomOrZero, Potassco::Id_t termId, const Potassco::IdSpan& elements);
	void theoryAtom(Potassco::Id_t atomOrZero, Potassco::Id_t termId, const Potassco::IdSpan& elements, Potassco::Id_t op, Potassco::Id_t rhs);
protected:
	Asp::LogicProgram* lp_;
	bool               inc_;
};
//@}

} } // end namespace Asp
#endif
