//
// Copyright (c) 2015-2017 Benjamin Kaufmann
//
// This file is part of Potassco.
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
#ifndef POTASSCO_CONVERT_H_INCLUDED
#define POTASSCO_CONVERT_H_INCLUDED

#include <potassco/smodels.h>
namespace Potassco {

//! Converts a given program so that it can be expressed in smodels format.
/*!
 * \ingroup WriteType
 */
class SmodelsConvert : public AbstractProgram {
public:
	//! Creates a new object that passes converted programs to out.
	/*!
	 * The parameter enableClaspExt determines how heuristic, edge, and external
	 * directives are handled.
	 * If true, heuristic and edge directives are converted to _heuristic and
	 * _edge predicates, while external directives passed to out.
	 * Otherwise, heuristic and edge directives are not converted but
	 * directly passed to out, while external directives are mapped to
	 * choice rules or integrity constraints.
	 */
	SmodelsConvert(AbstractProgram& out, bool enableClaspExt);
	~SmodelsConvert();
	//! Calls initProgram() on the associated output program.
	virtual void initProgram(bool incremental);
	//! Calls beginStep() on the associated output program.
	virtual void beginStep();
	//! Converts the given rule into one or more smodels rules.
	virtual void rule(Head_t t, const AtomSpan& head, const LitSpan& body);
	//! Converts the given rule into one or more smodels rules.
	virtual void rule(Head_t t, const AtomSpan& head, Weight_t bound, const WeightLitSpan& body);
	//! Converts literals associated with a priority to a set of corresponding smodels minimize rules.
	virtual void minimize(Weight_t prio, const WeightLitSpan& lits);
	//! Adds an atom named str that is equivalent to the condition to the symbol table.
	virtual void output(const StringSpan& str, const LitSpan& cond);
	//! Marks the atom that is equivalent to a as external.
	virtual void external(Atom_t a, Value_t v);
	//! Adds an _heuristic predicate over the given atom to the symbol table that is equivalent to condition.
	virtual void heuristic(Atom_t a, Heuristic_t t, int bias, unsigned prio, const LitSpan& condition);
	//! Adds an _edge(s,t) predicate to the symbol table that is equivalent to condition.
	virtual void acycEdge(int s, int t, const LitSpan& condition);

	//! Finalizes conversion and calls endStep() on the associated output program.
	virtual void endStep();

	//! Returns the output literal associated to in.
	Lit_t       get(Lit_t in) const;
	//! Returns the name associated with the given (output) smodels atom or 0 if no name exists.
	const char* getName(Atom_t a) const;
	//! Returns the max used smodels atom (valid atoms are [1..n]).
	unsigned    maxAtom() const;
protected:
	//! Creates a (named) atom that is equivalent to the given condition.
	Atom_t makeAtom(const LitSpan& lits, bool named);
	//! Processes all outstanding conversions.
	void flush();
	//! Converts external atoms.
	void flushExternal();
	//! Converts minimize statements.
	void flushMinimize();
	//! Converts heuristic directives to _heuristic predicates.
	void flushHeuristic();
	//! Converts (atom,name) pairs to output directives.
	void flushSymbols();
private:
	SmodelsConvert(const SmodelsConvert&);
	SmodelsConvert& operator=(const SmodelsConvert&);
	struct SmData;
	AbstractProgram& out_;
	SmData*          data_;
	bool             ext_;
};

} // namespace Potassco
#endif
