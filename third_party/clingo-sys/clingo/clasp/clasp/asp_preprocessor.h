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

#ifndef CLASP_PREPROCESSOR_H_INCLUDED
#define CLASP_PREPROCESSOR_H_INCLUDED

#ifdef _MSC_VER
#pragma once
#endif
#include <clasp/claspfwd.h>
#include <clasp/literal.h>
namespace Clasp { namespace Asp {

/**
 * \addtogroup asp
 */
//@{

//! Preprocesses (i.e. simplifies) a logic program.
/*!
 * Preprocesses (i.e. simplifies) a logic program and associates variables with
 * the nodes of the simplified logic program.
 */
class Preprocessor {
public:
	Preprocessor() : prg_(0), dfs_(true) {}
	//! Possible eq-preprocessing types.
	enum EqType {
		no_eq,    //!< No eq-preprocessing, associate a new var with each supported atom and body.
		full_eq   //!< Check for all kinds of equivalences between atoms and bodies.
	};

	const LogicProgram* program() const  { return prg_; }
	      LogicProgram* program()        { return prg_; }

	//! Starts preprocessing of the logic program.
	/*!
	 * Computes the maximum consequences of prg and associates a variable
	 * with each supported atom and body.
	 * \param prg The logic program to preprocess.
	 * \param t   Type of eq-preprocessing.
	 * \param maxIters If t == full_eq, maximal number of iterations during eq preprocessing.
	 * \param dfs If t == full_eq, classify in df-order (true) or bf-order (false).
	 */
	bool preprocess(LogicProgram& prg, EqType t, uint32 maxIters, bool dfs = true) {
		prg_  = &prg;
		dfs_  = dfs;
		type_ = t;
		return t == full_eq
			? preprocessEq(maxIters)
			: preprocessSimple();
	}

	bool eq() const { return type_ == full_eq; }
	Var  getRootAtom(Literal p) const { return p.id() < litToNode_.size() ? litToNode_[p.id()] : varMax; }
	void setRootAtom(Literal p, uint32 atomId) {
		if (p.id() >= litToNode_.size()) litToNode_.resize(p.id()+1, varMax);
		litToNode_[p.id()] = atomId;
	}
private:
	Preprocessor(const Preprocessor&);
	Preprocessor& operator=(const Preprocessor&);
	bool    preprocessEq(uint32 maxIters);
	bool    preprocessSimple();
	// ------------------------------------------------------------------------
	typedef PrgHead* const *             HeadIter;
	typedef std::pair<HeadIter,HeadIter> HeadRange;
	// Eq-Preprocessing
	struct BodyExtra {
		BodyExtra() : known(0), mBody(0), bSeen(0) {}
		uint32 known  :30;  // Number of predecessors already classified, only used for bodies
		uint32 mBody  : 1;  // A flag for marking bodies
		uint32 bSeen  : 1;  // First time we see this body?
	};
	bool     classifyProgram(const VarVec& supportedBodies);
	ValueRep simplifyClassifiedProgram(const HeadRange& atoms, bool more, VarVec& supported);
	PrgBody* addBodyVar(uint32 bodyId);
	bool     addHeadsToUpper(PrgBody* body);
	bool     addHeadToUpper(PrgHead* head, PrgEdge support);
	bool     propagateAtomVar(PrgAtom*, PrgEdge source);
	bool     propagateAtomValue(PrgAtom*, ValueRep val, PrgEdge source);
	bool     mergeEqBodies(PrgBody* b, Var rootId, bool equalLits);
	bool     hasRootLiteral(PrgBody* b) const;
	bool     superfluous(PrgBody* b) const;
	ValueRep simplifyHead(PrgHead* h, bool reclassify);
	ValueRep simplifyBody(PrgBody* b, bool reclassify, VarVec& supported);
	uint32   nextBodyId(VarVec::size_type& idx) {
		if (follow_.empty() || idx == follow_.size()) { return varMax; }
		if (dfs_) {
			uint32 id = follow_.back();
			follow_.pop_back();
			return id;
		}
		return follow_[idx++];;
	}
	// ------------------------------------------------------------------------
	typedef PodVector<BodyExtra>::type BodyData;
	LogicProgram* prg_;      // program to preprocess
	VarVec        follow_;   // bodies yet to classify
	BodyData      bodyInfo_; // information about the program nodes
	VarVec        litToNode_;// the roots of our equivalence classes
	uint32        pass_;     // current iteration number
	uint32        maxPass_;  // force stop after maxPass_ iterations
	EqType        type_;     // type of eq-preprocessing
	bool          dfs_;      // classify bodies in DF or BF order
};
//@}
} }
#endif

