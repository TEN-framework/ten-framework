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
//! \file
//! \brief Model enumeration with minimization and projection.
#ifndef CLASP_MODEL_ENUMERATORS_H
#define CLASP_MODEL_ENUMERATORS_H

#ifdef _MSC_VER
#pragma once
#endif

#include <clasp/enumerator.h>
#include <clasp/clause.h>

namespace Clasp {

//! Class for model enumeration with minimization and projection.
/*!
 * This class implements algorithms for enumerating models with or without optimization
 * and/or projection. It supports two different algorithms (strategies). First, enumeration
 * via restricted backjumping, and second, enumeration via recording of solution nogoods.
 *
 * The first strategy, strategy_backtrack, maintains a special backtracking level to
 * suppress certain backjumps that could otherwise "re-open" search spaces already visited.
 * If projection is not active, no extra nogoods are created.
 * Otherwise, the number of additional solution nogoods is linear in the number of projection atoms.
 *
 * The second strategy, strategy_record, enumerates models by recording nogoods for found solutions.
 * In general, this strategy is exponential in space (bounded by the number of solutions).
 * On the other hand, if optimization is active, additional nogoods are not needed
 * because the optimization constraint already serves as solution nogood.
 *
 * There is also a third strategy, strategy_auto, provided for convenience. This
 * strategy automatically selects between strategy_backtrack and strategy_record
 * based on the problem at hand. It uses strategy_record, if one of the following holds:
 *  - optimization is active, or
 *  - only one model is requested, or
 *  - both parallel search as well as projection are active
 *  .
 * In all other cases, strategy_auto selects strategy_backtrack.
 *
 * \ingroup enumerator
 */
class ModelEnumerator : public Enumerator {
public:
	//! Enumeration algorithms.
	enum Strategy {
		strategy_auto      = 0, //!< Use strategy best suited to problem.
		strategy_backtrack = 1, //!< Use backtrack-based enumeration.
		strategy_record    = 2  //!< Use nogood-based enumeration.
	};
	//! Projective solution enumeration and options.
	enum ProjectOptions {
		project_enable_simple = 1, //!< Enable projective solution enumeration.
		project_use_heuristic = 2, //!< Use heuristic when selecting a literal from a projection nogood.
		project_save_progress = 4, //!< Enable progress saving after the first solution was found.
		project_enable_full   = 6, //!< Enable projective solution enumeration with heuristic and progress saving.
		project_dom_lits      = 8, //!< In strategy record, project only on true domain literals.
	};
	/*!
	 * \param st Enumeration strategy to apply.
	 */
	explicit ModelEnumerator(Strategy st = strategy_auto);
	~ModelEnumerator();

	//! Configure strategy.
	/*!
	 * \param st         Enumeration strategy to use.
	 * \param projection The set of ProjectOptions to be applied or 0 to disable projective enumeration.
	 * \param filter     Ignore output predicates starting with filter in projective enumeration.
	 */
	void     setStrategy(Strategy st = strategy_auto, uint32 projection = 0, char filter = '_');
	bool     projectionEnabled()const { return projectOpts() != 0; }
	bool     domRec()           const { return (projectOpts() &  project_dom_lits) != 0; }
	Strategy strategy()         const { return static_cast<Strategy>(opts_.algo); }
	bool     project(Var v)     const;
protected:
	bool   supportsRestarts() const { return optimize() || strategy() == strategy_record; }
	bool   supportsParallel() const { return !projectionEnabled() || strategy() != strategy_backtrack; }
	bool   supportsSplitting(const SharedContext& problem) const {
		return (strategy() == strategy_backtrack || !domRec()) && Enumerator::supportsSplitting(problem);
	}
	ConPtr doInit(SharedContext& ctx, SharedMinimizeData* m, int numModels);
private:
	class ModelFinder;
	class BacktrackFinder;
	class RecordFinder;
	typedef PodVector<uint32>::type WordVec;
	void    initProjection(SharedContext& ctx);
	void    addProject(SharedContext& ctx, Var v);
	uint32  projectOpts()    const { return opts_.proj; }
	bool    trivial()        const { return trivial_; }
	WordVec project_;
	char    filter_;
	struct Options {
		uint8 proj : 4;
		uint8 algo : 2;
	} opts_, saved_;
	bool    trivial_;
};
}
#endif
