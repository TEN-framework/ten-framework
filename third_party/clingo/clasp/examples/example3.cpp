//
// Copyright (c) 2014-2017 Benjamin Kaufmann
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
// This example uses the ClaspFacade to compute
// the stable models of the program
//    a :- not b.
//    b :- not a.
//
// It is similar to example2() but uses generator based solving.
void example3(Clasp::SolveMode_t mode) {
	// See example2()
	Clasp::ClaspConfig config;
	config.solve.numModels = 0;

	// The "interface" to the clasp library.
	Clasp::ClaspFacade libclasp;

	// See example2()
	Clasp::Asp::LogicProgram& asp = libclasp.startAsp(config);
	addSimpleProgram(asp);

	// We are done with problem setup.
	// Prepare the problem for solving.
	libclasp.prepare();

	// Start the solving process.
	std::cout << "With Clasp::" << (((mode & Clasp::SolveMode_t::Async) != 0) ? "AsyncYield" : "Yield") << "\n";
	Clasp::ClaspFacade::SolveHandle it = libclasp.solve(mode|Clasp::SolveMode_t::Yield);
	// Get models one by one until iterator is exhausted.
	while (it.model()) {
		printModel(libclasp.ctx.output, *it.model());
		// Resume search for next model.
		it.resume();
	}
	std::cout << "No more models!" << std::endl;
}

void example3() {
	example3(Clasp::SolveMode_t::Default);
#if CLASP_HAS_THREADS
	example3(Clasp::SolveMode_t::Async);
#endif
}

