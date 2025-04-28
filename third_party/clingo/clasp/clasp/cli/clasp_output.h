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
#ifndef CLASP_CLI_OUTPUT_H_INCLUDED
#define CLASP_CLI_OUTPUT_H_INCLUDED
#include <clasp/clasp_facade.h>
#include <clasp/dependency_graph.h>
#include <clasp/solver_types.h>
#include <string>
namespace Clasp { namespace Cli {
template <class T, class S>
void formatEvent(const T& eventType, S& str);
template <class T, class S>
void format(const Clasp::Event_t<T>& ev, S& str) {
	formatEvent(static_cast<const T&>(ev), str);
}

/*!
 * \addtogroup cli
 * @{ */
/*!
 * \brief Interface for printing status and input format dependent information,
 * like models, optimization values, and summaries.
 */
class Output : public EventHandler {
public:
	//! Supported levels for printing models, optimize values, and individual calls.
	enum PrintLevel {
		print_all  = 0, //!< Print all models, optimize values, or calls.
		print_best = 1, //!< Only print last model, optimize value, or call.
		print_no   = 2, //!< Do not print any models, optimize values, or calls.
	};
	explicit Output(uint32 verb = 1);
	virtual ~Output();
	//! Active verbosity level.
	uint32 verbosity()               const { return verbose_; }
	//! Do not output any models?
	bool   quiet()                   const { return modelQ() == 2 && optQ() == 2; }
	//! Print level for models.
	int    modelQ()                  const { return static_cast<int>(quiet_[0]); }
	//! Print level for optimization values.
	int    optQ()                    const { return static_cast<int>(quiet_[1]); }
	//! Print level for individual (solve) calls.
	int    callQ()                   const { return static_cast<int>(quiet_[2]); }

	using  EventHandler::setVerbosity;
	void   setVerbosity(uint32 verb);
	void   setModelQuiet(PrintLevel model);
	void   setOptQuiet(PrintLevel opt);
	void   setCallQuiet(PrintLevel call);

	//! Shall be called once on startup.
	virtual void run(const char* solver, const char* version, const std::string* begInput, const std::string* endInput) = 0;
	//! Shall be called once on shutdown.
	virtual void shutdown(const ClaspFacade::Summary& summary);
	virtual void shutdown() = 0;
	//! Handles ClaspFacade events by forwarding calls to startStep() and stopStep().
	virtual void onEvent(const Event& ev);
	//! Checks quiet-levels and forwards to printModel() if appropriate.
	virtual bool onModel(const Solver& s, const Model& m);
	//! Checks quiet-levels and forwards to printLower() if appropriate.
	virtual bool onUnsat(const Solver& s, const Model& m);
	//! Shall print the given model.
	virtual void printModel(const OutputTable& out, const Model& m, PrintLevel x) = 0;
	//! Called on unsat - may print new info.
	virtual void printUnsat(const OutputTable& out, const LowerBound* lower, const Model* prevModel);
	//! A solving step has started.
	virtual void startStep(const ClaspFacade&);
	//! A solving step has stopped.
	virtual void stopStep(const ClaspFacade::Summary& summary);
	//! Shall print the given summary.
	virtual void printSummary(const ClaspFacade::Summary& summary, bool final)    = 0;
	//! Shall print the given statistics.
	virtual void printStatistics(const ClaspFacade::Summary& summary, bool final) = 0;
protected:
	typedef std::pair<const char*, Literal> OutPair;
	typedef uintp UPtr;
	typedef std::pair<uint32, uint32> UPair;
	const Model* getModel() const { return saved_.values ? &saved_ : 0; }
	void         saveModel(const Model& m);
	void         clearModel() { saved_.reset(); }
	void         printWitness(const OutputTable& out, const Model& m, UPtr data);
	virtual UPtr doPrint(const OutPair& out, uintp data);
	UPair        numCons(const OutputTable& out, const Model& m) const;
	bool         stats(const ClaspFacade::Summary& summary) const;
private:
	Output(const Output&);
	Output& operator=(const Output&);
	typedef const ClaspFacade::Summary* SumPtr ;
	SumPtr    summary_ ; // summary of last step
	ValueVec  vals_    ; // saved values from most recent model
	SumVec    costs_   ; // saved costs from most recent model
	Model     saved_   ; // most recent model
	uint32    verbose_ ; // verbosity level
	uint8     quiet_[3]; // quiet levels for models, optimize, calls
};

//! Prints models and solving statistics in Json-format to stdout.
class JsonOutput : public Output, private StatsVisitor {
public:
	explicit JsonOutput(uint32 verb);
	~JsonOutput();
	virtual void run(const char* solver, const char* version, const std::string* begInput, const std::string* endInput);
	virtual void shutdown(const ClaspFacade::Summary& summary);
	virtual void shutdown();
	virtual void printSummary(const ClaspFacade::Summary&    summary, bool final);
	virtual void printStatistics(const ClaspFacade::Summary& summary, bool final);
private:
	virtual void startStep(const ClaspFacade&);
	virtual void stopStep(const ClaspFacade::Summary& summary);
	virtual bool visitThreads(Operation op);
	virtual bool visitTester(Operation op);
	virtual bool visitHccs(Operation op);

	virtual void visitThread(uint32, const SolverStats& stats);
	virtual void visitHcc(uint32, const ProblemStats& p, const SolverStats& s);
	virtual void visitLogicProgramStats(const Asp::LpStats& stats);
	virtual void visitProblemStats(const ProblemStats& stats);
	virtual void visitSolverStats(const SolverStats& stats);
	virtual void visitExternalStats(const StatisticObject& stats);
	virtual UPtr doPrint(const OutPair& out, UPtr data);
	void printChildren(const StatisticObject& s);
	enum ObjType { type_object, type_array };
	void pushObject(const char* k = 0, ObjType t = type_object);
	char popObject();
	void printKeyValue(const char* k, const char* v) ;
	void printKeyValue(const char* k, uint64 v);
	void printKeyValue(const char* k, uint32 v);
	void printKeyValue(const char* k, double d);
	void printKeyValue(const char* k, const StatisticObject& o);
	void printString(const char* s, const char* sep);
	void printKey(const char* k);
	void printModel(const OutputTable& out, const Model& m, PrintLevel x);
	void printCosts(const SumVec& costs, const char* name = "Costs");
	void printCons(const UPair& cons);
	void printCoreStats(const CoreStats&);
	void printExtStats(const ExtendedStats&, bool generator);
	void printJumpStats(const JumpStats&);
	void startModel();
	bool hasWitness() const { return !objStack_.empty() && *objStack_.rbegin() == '['; }
	uint32 indent()   const { return static_cast<uint32>(objStack_.size() * 2); }
	const char* open_;
	std::string objStack_;
};

//! Default clasp format printer.
/*!
 * Prints all output to stdout in given format:
 * - format_asp prints in clasp's default asp format
 * - format_aspcomp prints in in ASP competition format
 * - format_sat09 prints in SAT-competition format
 * - format_pb09 in PB-competition format
 * .
 * \see https://www.mat.unical.it/aspcomp2013/
 * \see http://www.satcompetition.org/2009/format-solvers2009.html
 * \see http://www.cril.univ-artois.fr/PB09/solver_req.html
 *
 */
class TextOutput : public Output, private StatsVisitor {
public:
	//! Supported text formats.
	enum Format      { format_asp, format_aspcomp, format_sat09, format_pb09 };
	enum ResultStr   { res_unknonw = 0, res_sat = 1, res_unsat = 2, res_opt = 3, num_str };
	enum CategoryKey { cat_comment, cat_value, cat_objective, cat_result, cat_value_term, cat_atom_name, cat_atom_var, num_cat };

	const char* result[num_str]; //!< Default result strings.
	const char* format[num_cat]; //!< Format strings.

	TextOutput(uint32 verbosity, Format f, const char* catAtom = 0, char ifs = ' ');
	~TextOutput();

	//! Prints a (comment) message containing the given solver and input.
	virtual void run(const char* solver, const char* version, const std::string* begInput, const std::string* endInput);
	virtual void shutdown();
	//! Prints the given model.
	/*!
	 * Prints format[cat_value] followed by the elements of the model. Individual
	 * elements e are printed as format[cat_atom] and separated by the internal field separator.
	 */
	virtual void printModel(const OutputTable& out, const Model& m, PrintLevel x);
	//! Prints the given lower bound and upper bounds that are known to be optimal.
	virtual void printUnsat(const OutputTable& out, const LowerBound* lower, const Model* prevModel);
	//! Called once a solving step has completed.
	/*!
	 * Always prints "format[cat_result] result[s.result()]".
	 * Furthermore, if verbosity() > 0, prints a summary consisting of
	 *   - the number of computed models m and whether the search space was exhausted
	 *   - the number of enumerated models e if e != m
	 *   - the state of any optimization and whether the last model was optimal
	 *   - the state of consequence computation and whether the last model corresponded to the consequences
	 *   - timing information
	 *   .
	 */
	virtual void printSummary(const ClaspFacade::Summary& s   , bool final);
	virtual void printStatistics(const ClaspFacade::Summary& s, bool final);
	//! Prints progress events (preprocessing/solving) if verbosity() > 1.
	virtual void onEvent(const Event& ev);
	//! A solving step has started.
	virtual void startStep(const ClaspFacade&);
	//! Prints a comment message.
	void comment(uint32 v, const char* fmt, ...) const;
protected:
	virtual bool visitThreads(Operation op);
	virtual bool visitTester(Operation op);

	virtual void visitThread(uint32, const SolverStats& stats);
	virtual void visitHcc(uint32, const ProblemStats& p, const SolverStats& s);
	virtual void visitLogicProgramStats(const Asp::LpStats& stats);
	virtual void visitProblemStats(const ProblemStats& stats);
	virtual void visitSolverStats(const SolverStats& stats);
	virtual void visitExternalStats(const StatisticObject& stats);

	virtual UPtr doPrint(const OutPair& out, UPtr data);
	const char* fieldSeparator() const;
	int         printSep(CategoryKey c) const;
	void        printCosts(const SumVec&) const;
	void        printBounds(const SumVec& upper, const SumVec& lower) const;
	void        printStats(const SolverStats& stats) const;
	void        printJumps(const JumpStats&) const;
	bool        startSection(const char* n)  const;
	void        startObject(const char* n, uint32 i) const;
	void        setState(uint32 state, uint32 verb, const char* st);
	void        printSolveProgress(const Event& ev);
	void        printValues(const OutputTable& out, const Model& m);
	void        printMeta(const OutputTable& out, const Model& m);
	void        printChildren(const StatisticObject& s, unsigned level = 0, const char* prefix = 0);
	int         printChildKey(unsigned level, const char* key, uint32 idx, const char* prefix = 0) const;
private:
	void        printCostsImpl(const SumVec&, char ifs, const char* ifsSuffix = "") const;
	const char* getIfsSuffix(char ifs, CategoryKey cat) const;
	const char* getIfsSuffix(CategoryKey cat) const;
	struct SolveProgress {
		int lines;
		int last;
		void clear() {
			lines = 0;
			last  = -1;
		}
	};
	std::string   fmt_;
	double        stTime_;  // time on state enter
	SolveProgress progress_;// for printing solve progress
	int           width_;   // output width
	uint32        state_;   // active state
	char          ifs_[2];  // field separator
	bool          accu_;
};
//@}

}}
#endif
