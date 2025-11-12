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
#ifndef POTASSCO_SMODELS_H_INCLUDED
#define POTASSCO_SMODELS_H_INCLUDED
#include <potassco/match_basic_types.h>
namespace Potassco {
/*!
 * \addtogroup ParseType
 */
///@{

//! Interface representing an smodels-style symbol table.
class AtomTable {
public:
	virtual ~AtomTable();
	//! Associate a name with the given (output) atom.
	virtual void   add(Atom_t id, const StringSpan& name, bool output) = 0;
	//! Return the atom with the given name or 0 if no such atom was previously added.
	virtual Atom_t find(const StringSpan& name) = 0;
};

//! Class for parsing logic programs in (extended) smodels format.
class SmodelsInput : public ProgramReader {
public:
	//! Options for configuring reading of smodels format.
	struct Options {
		Options() : claspExt(false), cEdge(false), cHeuristic(false), filter(false) {}
		//! Enable clasp extensions for handling incremental programs.
		Options& enableClaspExt() { claspExt = true; return *this; }
		//! Convert _edge/_acyc_ atoms to edge directives.
		Options& convertEdges() { cEdge = true; return *this; }
		//! Convert _heuristic atoms to heuristic directives.
		Options& convertHeuristic() { cHeuristic = true; return *this; }
		//! Remove converted atoms from output.
		Options& dropConverted() { filter = true; return *this; }
		bool claspExt;
		bool cEdge;
		bool cHeuristic;
		bool filter;
	};
	//! Creates a new parser object that calls out on each parsed element.
	SmodelsInput(AbstractProgram& out, const Options& opts, AtomTable* symTab = 0);
	virtual ~SmodelsInput();
protected:
	//! Checks whether stream starts with a valid smodels token.
	virtual bool doAttach(bool& inc);
	//! Parses the current step and throws exception on error.
	/*!
	 * The function calls beginStep()/endStep() on the associated
	 * output object before/after parsing the current step.
	 */
	virtual bool doParse();
	//! Resets internal parsing state.
	virtual void doReset();
	//! Reads the current rule block.
	virtual bool readRules();
	//! Reads the current smodels symbol table block.
	virtual bool readSymbols();
	//! Reads the current part of the compute statement.
	virtual bool readCompute(const char* sec, bool val);
	//! Reads an optional external block and the number of models.
	virtual bool readExtra();
private:
	struct NodeTab;
	struct SymTab;
	void matchBody(RuleBuilder& rule);
	void matchSum(RuleBuilder& rule, bool weights);
	AbstractProgram& out_;
	AtomTable*       atoms_;
	NodeTab*         nodes_;
	Options          opts_;
	bool             delSyms_;
};

/*!
 * Parses the given program in smodels format and calls out on each parsed element.
 * The error handler h is called on error. If h is 0, std::exceptions are used to signal errors.
 */
int readSmodels(std::istream& prg, AbstractProgram& out, ErrorHandler h = 0, const SmodelsInput::Options& opts = SmodelsInput::Options());

///@}

/*!
 * \addtogroup WriteType
 */
///@{

//! Returns a non-zero value if head can be represented in smodels format (i.e. is not empty).
int isSmodelsHead(Head_t ht, const AtomSpan& head);
//! Returns a non-zero value if rule can be represented in smodels format.
int isSmodelsRule(Head_t ht, const AtomSpan& head, Weight_t bound, const WeightLitSpan& body);

//! Writes a program in smodels numeric format to the given output stream.
/*!
 * \note The class only supports program constructs that can be directly
 * expressed in smodels numeric format.
 */
class SmodelsOutput : public AbstractProgram {
public:
	//! Creates a new object and associates it with the given output stream.
	/*!
	 * If enableClaspExt is true, rules with numbers 90, 91, and 92
	 * are used to enable incremental programs and external atoms.
	 *
	 * The falseAtom is used to write integrity constraints and can be
	 * set to 0 if integrity constraints are not used.
	 */
	SmodelsOutput(std::ostream& os, bool enableClaspExt, Atom_t falseAtom);
	//! Prepares the object for a new program.
	/*!
	 * Requires enableClaspExt or inc must be false.
	 */
	virtual void initProgram(bool inc);
	//! Starts a new step.
	virtual void beginStep();
	//! Writes the given rule provided that isSmodelsHead(head) returns a non-zero value.
	virtual void rule(Head_t t, const AtomSpan& head, const LitSpan& body);
	//! Writes the given rule provided that isSmodelsRule(head, bound, body) returns a non-zero value.
	virtual void rule(Head_t t, const AtomSpan& head, Weight_t bound, const WeightLitSpan& body);
	//! Writes the given minimize rule while ignoring its priority.
	virtual void minimize(Weight_t prio, const WeightLitSpan& lits);
	//! Writes the entry (a, str) to the symbol table provided that condition equals a.
	/*!
	 * \note Symbols shall only be added once after all rules were added.
	 */
	virtual void output(const StringSpan& str, const LitSpan& cond);
	//! Writes lits as a compute statement.
	/*!
	 * \note The function shall be called at most once per step and only after all rules and symbols were added.
	 */
	virtual void assume(const LitSpan& lits);
	//! Requires enableClaspExt or throws exception.
	virtual void external(Atom_t a, Value_t v);
	//! Terminates the current step.
	virtual void endStep();
protected:
	//! Starts writing an smodels-rule of type rt.
	SmodelsOutput& startRule(int rt);
	//! Writes the given head.
	SmodelsOutput& add(Head_t ht, const AtomSpan& head);
	//! Writes the given normal body in smodels format, i.e. size(lits) size(B-) atoms in B- atoms in B+
	SmodelsOutput& add(const LitSpan&  lits);
	//! Writes the given extended body in smodels format.
	SmodelsOutput& add(Weight_t bound, const WeightLitSpan& lits, bool card);
	//! Writes i.
	SmodelsOutput& add(unsigned i);
	//! Terminates the active rule by writing a newline.
	SmodelsOutput& endRule();
	//! Returns whether the current program is incremental.
	bool incremental() const { return inc_; }
	//! Returns whether clasp extensions are enabled.
	bool extended()    const { return ext_; }
private:
	SmodelsOutput(const SmodelsOutput&);
	SmodelsOutput& operator=(const SmodelsOutput&);
	std::ostream& os_;
	Atom_t       false_;
	int          sec_;
	bool         ext_;
	bool         inc_;
	bool         fHead_;
};
///@}

} // namespace Potassco
#endif
