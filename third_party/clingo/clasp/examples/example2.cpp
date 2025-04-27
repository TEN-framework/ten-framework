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

// This example uses the ClaspFacade to compute
// the stable models of the program
//    a :- not b.
//    b :- not a.
//
// The ClaspFacade is a convenient wrapper for the services of the clasp library.
// See clasp_facade.h for details.


// In order to get models from the ClaspFacade, we must provide a suitable
// event handler.
class ModelPrinter : public Clasp::EventHandler {
public:
	ModelPrinter() {}
	bool onModel(const Clasp::Solver& s, const Clasp::Model& m) {
		printModel(s.outputTable(), m);
		return true;
	}
};

void addSimpleProgram(Clasp::Asp::LogicProgram& prg) {
	Potassco::Atom_t a = prg.newAtom();
	Potassco::Atom_t b = prg.newAtom();
	Potassco::RuleBuilder rb;
	prg.addRule(rb.start().addHead(a).addGoal(Potassco::neg(b)));
	prg.addRule(rb.start().addHead(b).addGoal(Potassco::neg(a)));
	prg.addOutput("a", a);
	prg.addOutput("b", b);
}

void example2() {
	// Aggregates configuration options.
	// Using config, you can control many parts of the search, e.g.
	// - the amount and kind of preprocessing
	// - the enumerator to use and the number of models to compute
	// - the heuristic used for decision making
	// - the restart strategy
	// - ...
	Clasp::ClaspConfig config;
	// We want to compute all models but
	// otherwise we use the default configuration.
	config.solve.numModels = 0;

	// The "interface" to the clasp library.
	Clasp::ClaspFacade libclasp;

	// LogicProgram provides the interface for defining logic programs.
	// The returned object is already setup and ready to use.
	// See logic_program.h for details.
	Clasp::Asp::LogicProgram& asp = libclasp.startAsp(config);
	addSimpleProgram(asp);
	// We are done with problem setup.
	// Prepare the problem for solving.
	libclasp.prepare();

	// Start the actual solving process.
	ModelPrinter printer;
	libclasp.solve(&printer);
	std::cout << "No more models!" << std::endl;
}
