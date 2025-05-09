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
#ifndef CLASP_CB_ENUMERATOR_H
#define CLASP_CB_ENUMERATOR_H

#ifdef _MSC_VER
#pragma once
#endif

#include <clasp/enumerator.h>

namespace Clasp {

//! Enumerator for computing the brave/cautious consequences of a logic program.
/*!
 * \ingroup enumerator
 */
class CBConsequences : public Enumerator {
public:
	enum Type {
		Brave    = Model::Brave,
		Cautious = Model::Cautious,
	};
	enum Algo { Default, Query };
	/*!
	 * \param type Type of consequences to compute.
	 * \param a Type of algorithm to apply if type is Cautious.
	 */
	explicit CBConsequences(Type type, Algo a = Default);
	~CBConsequences();
	int  modelType() const { return type_; }
	bool exhaustive()const { return true; }
	bool supportsSplitting(const SharedContext& problem) const;
	int  unsatType() const;
private:
	class  CBFinder;
	class  QueryFinder;
	class  SharedConstraint;
	ConPtr doInit(SharedContext& ctx, SharedMinimizeData* m, int numModels);
	void   addLit(SharedContext& ctx, Literal p);
	void   addCurrent(Solver& s, LitVec& con, ValueVec& m, uint32 rootL = 0);
	LitVec            cons_;
	SharedConstraint* shared_;
	Type              type_;
	Algo              algo_;
};

}
#endif
