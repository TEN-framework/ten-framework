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

#ifndef CLASP_PROGRAM_BUILDER_H_INCLUDED
#define CLASP_PROGRAM_BUILDER_H_INCLUDED

#ifdef _MSC_VER
#pragma once
#endif

#include <clasp/claspfwd.h>
#include <clasp/literal.h>
#include <clasp/util/misc_types.h>
#include <potassco/basic_types.h>
#include POTASSCO_EXT_INCLUDE(unordered_map)
#include <iosfwd>

namespace Clasp {

/**
 * \file
 * \defgroup problem Input
 * \brief Classes and functions for defining input programs.
 */
//@{

//! Interface for defining an input program.
class ProgramBuilder {
public:
	typedef SharedMinimizeData SharedMinimize;
	typedef SingleOwnerPtr<SharedMinimize, ReleaseObject> MinPtr;

	ProgramBuilder();
	virtual ~ProgramBuilder();
	//! Starts the definition of a program.
	/*!
	 * This function shall be called exactly once before a new program is defined.
	 * It discards any previously added program.
	 *
	 * \param ctx The context object in which the program should be stored.
	 */
	bool startProgram(SharedContext& ctx);
	//! Parses the given stream as a program of type() and adds it to this object.
	bool parseProgram(std::istream& prg);
	//! Unfreezes a currently frozen program.
	bool updateProgram();
	//! Loads the program into the shared context passed to startProgram().
	bool endProgram();
	//! Returns any assumptions that shall hold during solving.
	/*!
	 * \pre frozen()
	 */
	void             getAssumptions(LitVec& out) const;
	//! Returns bounds that shall hold during minimization.
	void             getWeakBounds(SumVec& out)  const;
	//! Returns the type of program that is created by this builder.
	int              type()   const { return doType(); }
	//! Returns true if the program is currently frozen.
	bool             frozen() const { return frozen_; }
	//! Returns true if the program is not conflicting.
	virtual bool     ok()     const;
	//! Returns the stored context object.
	SharedContext*   ctx()    const { return ctx_; }
	//! Returns a parser for this type of program associated with this object.
	ProgramParser&   parser();
protected:
	void addMinLit(weight_t prio, WeightLiteral x);
	void setFrozen(bool frozen)  { frozen_ = frozen; }
	void setCtx(SharedContext* x){ ctx_    = x; }
	void markOutputVariables() const;
private:
	typedef SingleOwnerPtr<MinimizeBuilder> MinBuildPtr;
	typedef SingleOwnerPtr<ProgramParser>   ParserPtr;
	ProgramBuilder(const ProgramBuilder&);
	ProgramBuilder& operator=(ProgramBuilder&);
	virtual bool doStartProgram()                    = 0;
	virtual bool doUpdateProgram()                   = 0;
	virtual bool doEndProgram()                      = 0;
	virtual void doGetWeakBounds(SumVec& out)  const;
	virtual void doGetAssumptions(LitVec& out) const = 0;
	virtual int  doType() const                      = 0;
	virtual ProgramParser* doCreateParser()          = 0;
	SharedContext* ctx_;
	ParserPtr      parser_;
	bool           frozen_;
};

//! A class for defining a SAT-problem in CNF.
class SatBuilder : public ProgramBuilder {
public:
	explicit SatBuilder();
	// program definition

	//! Creates necessary variables and prepares the problem.
	/*!
	 * \param numVars          Number of variables to create.
	 * \param hardClauseWeight Weight identifying hard clauses (0 means no weight).
	 *                         Clauses added with a weight != hardClauseWeight are
	 *                         considered soft clauses (see addClause()).
	 * \param clauseHint       A hint on how many clauses will be added.
	 */
	void prepareProblem(uint32 numVars, wsum_t hardClauseWeight = 0, uint32 clauseHint = 100);
	//! Returns the number of variables in the problem.
	Var numVars() const { return vars_; }
	//! Adds the given clause to the problem.
	/*!
	 * The SatBuilder supports the creation of (weighted) MaxSAT problems
   * via the creation of "soft clauses". For this, clauses
   * added to this object have an associated weight cw. If cw
	 * does not equal hardClauseWeight (typically 0), the clause is a
	 * soft clause and not satisfying it results in a penalty of cw.
	 *
	 * \pre v <= numVars(), for all variables v occuring in clause.
	 * \pre cw >= 0.
	 * \param clause The clause to add.
	 * \param cw     The weight associated with the clause.
	 */
	bool addClause(LitVec& clause, wsum_t cw = 0);
	//! Adds the given PB-constraint (sum(lits) >= bound) to the problem.
	bool addConstraint(WeightLitVec& lits, weight_t bound);
	//! Adds min as an objective function to the problem.
	bool addObjective(const WeightLitVec& min);
	//! Adds v to the set of projection vars.
	void addProject(Var v);
	//! Adds x to the set of initial assumptions.
	void addAssumption(Literal x);
private:
	typedef PodVector<uint8>::type VarState;
	bool doStartProgram();
	ProgramParser* doCreateParser();
	int  doType() const                    { return Problem_t::Sat; }
	bool doUpdateProgram()                 { return !frozen(); }
	void doGetAssumptions(LitVec& a) const { a.insert(a.end(), assume_.begin(), assume_.end()); }
	bool doEndProgram();
	bool satisfied(LitVec& clause);
	bool markAssigned();
	void markLit(Literal x) { varState_[x.var()] |= 1 + x.sign(); }
	VarState varState_;
	LitVec   softClauses_;
	LitVec   assume_;
	wsum_t   hardWeight_;
	Var      vars_;
	uint32   pos_;
};

//! A class for defining a PB-problem.
class PBBuilder : public ProgramBuilder {
public:
	PBBuilder();
	// program definition
	//! Creates necessary variables and prepares the problem.
	/*!
	 * \param numVars          Number of problem variables to create.
	 * \param maxProduct       Max number of products in the problem.
	 * \param maxSoft          Max number of soft constraints in the problem.
	 * \param constraintHint   A hint on how many clauses will be added.
	 */
	void    prepareProblem(uint32 numVars, uint32 maxProduct, uint32 maxSoft, uint32 constraintHint = 100);
	//! Returns the number of variables in the problem.
	uint32  numVars() const { return auxVar_ - 1; }
	//! Adds the given PB-constraint to the problem.
	/*!
	 * A PB-constraint consists of a list of weighted Boolean literals (lhs),
	 * a comparison operator (either >= or =), and an integer bound (rhs).
	 *
	 * \pre v <= numVars(), for all variables v occuring in lits.
	 *
	 * \param lits  The lhs of the PB-constraint.
	 * \param bound The rhs of the PB-constraint.
	 * \param eq    If true, use '=' instead of '>=' as comparison operator.
	 * \param cw    If > 0, treat constraint as soft constraint with weight cw.
	 */
	bool    addConstraint(WeightLitVec& lits, weight_t bound, bool eq = false, weight_t cw = 0);
	//! Adds the given product to the problem.
	/*!
	 * The function creates the equality x == l1 && ... && ln, where x is a new
	 * literal and each li is a literal in lits.
	 * \pre The number of products added so far is < maxProduct that was given in prepareProblem().
	 */
	Literal addProduct(LitVec& lits);
	//! Adds min as an objective function to the problem.
	bool    addObjective(const WeightLitVec& min);
	//! Adds v to the set of projection vars.
	void    addProject(Var v);
	//! Adds x to the set of initial assumptions.
	void    addAssumption(Literal x);
	//! Only allow solutions where the sum of violated soft constraint is less than bound.
	bool    setSoftBound(wsum_t bound);
private:
	struct PKey {
		LitVec lits;
		std::size_t operator()(const PKey& k)                    const { return k.lits[0].rep(); }
		bool        operator()(const PKey& lhs, const PKey& rhs) const { return lhs.lits == rhs.lits; }
	};
	typedef POTASSCO_EXT_NS::unordered_map<PKey, Literal, PKey, PKey> ProductIndex;
	bool doStartProgram();
	void doGetWeakBounds(SumVec& out) const;
	int  doType() const                    { return Problem_t::Pb; }
	bool doUpdateProgram()                 { return !frozen(); }
	void doGetAssumptions(LitVec& a) const { a.insert(a.end(), assume_.begin(), assume_.end()); }
	ProgramParser* doCreateParser();
	bool doEndProgram();
	bool productSubsumed(LitVec& lits, PKey& prod);
	void addProductConstraints(Literal eqLit, LitVec& lits);
	Var  getAuxVar();
	ProductIndex products_;
	PKey         prod_;
	LitVec       assume_;
	uint32       auxVar_;
	uint32       endVar_;
	wsum_t       soft_;
};

//! Adapts a Sat or PB builder to the Potassco::AbstractProgram interface.
class BasicProgramAdapter : public Potassco::AbstractProgram {
public:
	BasicProgramAdapter(ProgramBuilder& prg);
	void initProgram(bool inc);
	void beginStep();
	void rule(Potassco::Head_t ht, const Potassco::AtomSpan& head, const Potassco::LitSpan& body);
	void rule(Potassco::Head_t ht, const Potassco::AtomSpan& head, Potassco::Weight_t bound, const Potassco::WeightLitSpan& body);
	void minimize(Potassco::Weight_t prio, const Potassco::WeightLitSpan& lits);
protected:
	ProgramBuilder* prg_;
	LitVec          clause_;
	WeightLitVec    constraint_;
	bool            inc_;
};

}
#endif
