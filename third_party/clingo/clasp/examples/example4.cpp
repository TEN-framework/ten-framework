//
// Copyright (c) 2009-2017 Benjamin Kaufmann
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

// Add the libclasp directory to the list of
// include directoies of your build system.
#include <clasp/clasp_facade.h>
#include <clasp/solver.h>
#include "example.h"

// This example demonstrates how user code can influence model enumeration.

static void excludeModel(const Clasp::Solver& s, const Clasp::Model& m) {
	Clasp::LitVec clause;
	for (uint32_t i = 1; i <= s.decisionLevel(); ++i) {
		clause.push_back(~s.decision(i));
	}
	m.ctx->commitClause(clause);
}

void example4() {
	Clasp::ClaspConfig config;
	config.solve.enumMode  = Clasp::EnumOptions::enum_user;
	config.solve.numModels = 0;

	// The "interface" to the clasp library.
	Clasp::ClaspFacade libclasp;

	Clasp::Asp::LogicProgram& asp = libclasp.startAsp(config);
	addSimpleProgram(asp);

	libclasp.prepare();

	// Start the actual solving process.
	for (Clasp::ClaspFacade::SolveHandle h = libclasp.solve(Clasp::SolveMode_t::Yield); h.next(); ) {
		// print the model
		printModel(libclasp.ctx.output, *h.model());
		// exclude this model
		excludeModel(*libclasp.ctx.solver(h.model()->sId), *h.model());
	}
	std::cout << "No more models!" << std::endl;
}
