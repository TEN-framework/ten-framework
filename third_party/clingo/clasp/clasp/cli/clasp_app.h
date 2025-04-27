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
#ifndef CLASP_CLI_CLASP_APP_H_INCLUDED
#define CLASP_CLI_CLASP_APP_H_INCLUDED

#ifdef _MSC_VER
#pragma once
#endif
#include <potassco/program_opts/typed_value.h>
#include <potassco/application.h>
#include <clasp/util/timer.h>
#include <clasp/cli/clasp_options.h>
#include <clasp/cli/clasp_output.h>
#include <string>
#include <vector>
#include <iosfwd>
#include <memory>
namespace Potassco { class StringBuilder; }
namespace Clasp { namespace Cli {
/////////////////////////////////////////////////////////////////////////////////////////
// clasp exit codes
/////////////////////////////////////////////////////////////////////////////////////////
enum ExitCode {
	E_UNKNOWN   = 0,  /*!< Satisfiablity of problem not knwon; search not started.    */
	E_INTERRUPT = 1,  /*!< Run was interrupted.                                       */
	E_SAT       = 10, /*!< At least one model was found.                              */
	E_EXHAUST   = 20, /*!< Search-space was completely examined.                      */
	E_MEMORY    = 33, /*!< Run was interrupted by out of memory exception.            */
	E_ERROR     = 65, /*!< Run was interrupted by internal error.                     */
	E_NO_RUN    = 128 /*!< Search not started because of syntax or command line error.*/
};
/////////////////////////////////////////////////////////////////////////////////////////
// clasp app helpers
/////////////////////////////////////////////////////////////////////////////////////////
class WriteCnf {
public:
	WriteCnf(const std::string& outFile);
	~WriteCnf();
	void writeHeader(uint32 numVars, uint32 numCons);
	void write(Var maxVar, const ShortImplicationsGraph& g);
	void write(ClauseHead* h);
	void write(Literal unit);
	void close();
	bool unary(Literal, Literal) const;
	bool binary(Literal, Literal, Literal) const;
private:
	WriteCnf(const WriteCnf&);
	WriteCnf& operator=(const WriteCnf&);
	FILE*  str_;
	LitVec lits_;
};
class LemmaLogger {
public:
	struct Options {
		Options() : logMax(UINT32_MAX), lbdMax(UINT32_MAX), domOut(false), logText(false) {}
		uint32 logMax;  // log at most logMax lemmas
		uint32 lbdMax;  // only log lemmas with lbd <= lbdMax
		bool   domOut;  // only log lemmas that can be expressed over out variables
		bool   logText; // log lemmas in ground lp format
	};
	LemmaLogger(const std::string& outFile, const Options& opts);
	~LemmaLogger();
	void startStep(ProgramBuilder& prg, bool inc);
	void add(const Solver& s, const LitVec& cc, const ConstraintInfo& info);
	void close();
private:
	typedef PodVector<uint32>::type Var2Idx;
	typedef Atomic_t<uint32>::type Counter;
	LemmaLogger(const LemmaLogger&);
	LemmaLogger& operator=(const LemmaLogger&);
	void formatAspif(const LitVec& cc, uint32 lbd, Potassco::StringBuilder& out)  const;
	void formatText(const LitVec& cc, const OutputTable& tab, uint32 lbd, Potassco::StringBuilder& out) const;
	FILE*            str_;
	Potassco::LitVec solver2asp_;
	Var2Idx          solver2NameIdx_;
	ProblemType      inputType_;
	Options          options_;
	int              step_;
	Counter          logged_;
};
/////////////////////////////////////////////////////////////////////////////////////////
// clasp specific application options
/////////////////////////////////////////////////////////////////////////////////////////
struct ClaspAppOptions {
	typedef LemmaLogger::Options LogOptions;
	ClaspAppOptions();
	typedef std::vector<std::string>  StringSeq;
	static bool mappedOpts(ClaspAppOptions*, const std::string&, const std::string&);
	void initOptions(Potassco::ProgramOptions::OptionContext& root);
	bool validateOptions(const Potassco::ProgramOptions::ParsedOptions& parsed);
	StringSeq   input;     // list of input files - only first used!
	std::string lemmaLog;  // optional file name for writing learnt lemmas
	std::string lemmaIn;   // optional file name for reading learnt lemmas
	std::string hccOut;    // optional file name for writing scc programs
	std::string outAtom;   // optional format string for atoms
	uint32      outf;      // output format
	int         compute;   // force literal compute to true
	LogOptions  lemma;     // options for lemma logging
	char        ifs;       // output field separator
	bool        hideAux;   // output aux atoms?
	uint8       quiet[3];  // configure printing of models, optimization values, and call steps
	int8        onlyPre;   // run preprocessor and exit
	bool        printPort; // print portfolio and exit
	enum OutputFormat { out_def = 0, out_comp = 1, out_json = 2, out_none = 3 };
};
/////////////////////////////////////////////////////////////////////////////////////////
// clasp application base
/////////////////////////////////////////////////////////////////////////////////////////
// Base class for applications using the clasp library.
class ClaspAppBase : public Potassco::Application, public Clasp::EventHandler {
public:
	typedef ClaspFacade::Summary  RunSummary;
	typedef Potassco::ProgramOptions::PosOption PosOption;
protected:
	using Potassco::Application::run;
	ClaspAppBase();
	~ClaspAppBase();
	// -------------------------------------------------------------------------------------------
	// Functions to be implemented by subclasses
	virtual ProblemType   getProblemType()             = 0;
	virtual void          run(ClaspFacade& clasp)      = 0;
	virtual Output*       createOutput(ProblemType f);
	virtual void          storeCommandArgs(const Potassco::ProgramOptions::ParsedValues& values);
	// -------------------------------------------------------------------------------------------
	// Helper functions that subclasses might call during run
	void handleStartOptions(ClaspFacade& clasp);
	bool handlePostGroundOptions(ProgramBuilder& prg);
	bool handlePreSolveOptions(ClaspFacade& clasp);
	// -------------------------------------------------------------------------------------------
	// Application functions
	virtual const int*  getSignals()    const;
	virtual HelpOpt     getHelpOption() const { return HelpOpt("Print {1=basic|2=more|3=full} help and exit", 3); }
	virtual PosOption   getPositional() const { return parsePositional; }
	virtual void        initOptions(Potassco::ProgramOptions::OptionContext& root);
	virtual void        validateOptions(const Potassco::ProgramOptions::OptionContext& root, const Potassco::ProgramOptions::ParsedOptions& parsed, const Potassco::ProgramOptions::ParsedValues& values);
	virtual void        setup();
	virtual void        run();
	virtual void        shutdown();
	virtual bool        onSignal(int);
	virtual void        printHelp(const Potassco::ProgramOptions::OptionContext& root);
	virtual void        printVersion();
	static  bool        parsePositional(const std::string& s, std::string& out);
	// -------------------------------------------------------------------------------------------
	// Event handler
	virtual void onEvent(const Event& ev);
	virtual bool onModel(const Solver& s, const Model& m);
	virtual bool onUnsat(const Solver& s, const Model& m);
	// -------------------------------------------------------------------------------------------
	// Status information & output
	int  exitCode(const RunSummary& sol)    const;
	void printTemplate()                    const;
	void printDefaultConfigs()              const;
	void printConfig(ConfigKey k)           const;
	void printLibClaspVersion()             const;
	void printLicense()                     const;
	std::istream& getStream(bool reopen = false) const;
	// -------------------------------------------------------------------------------------------
	// Functions called in handlePreSolveOptions()
	void writeNonHcfs(const PrgDepGraph& graph) const;
	typedef Potassco::ProgramReader     LemmaReader;
	typedef SingleOwnerPtr<Output>      OutPtr;
	typedef SingleOwnerPtr<ClaspFacade> ClaspPtr;
	typedef SingleOwnerPtr<LemmaLogger> LogPtr;
	typedef SingleOwnerPtr<LemmaReader> LemmaPtr;
	ClaspCliConfig  claspConfig_;
	ClaspAppOptions claspAppOpts_;
	ClaspPtr        clasp_;
	OutPtr          out_;
	LogPtr          logger_;
	LemmaPtr        lemmaIn_;
	unsigned        fpuMode_;
};
/////////////////////////////////////////////////////////////////////////////////////////
// clasp application
/////////////////////////////////////////////////////////////////////////////////////////
// Standalone clasp application.
class ClaspApp : public ClaspAppBase {
public:
	ClaspApp();
	const char* getName()       const { return "clasp"; }
	const char* getVersion()    const { return CLASP_VERSION; }
	const char* getUsage()      const {
		return
			"[number] [options] [file]\n"
			"Compute at most <number> models (0=all) of the instance given in <file>";
	}
protected:
	virtual ProblemType getProblemType();
	virtual void        run(ClaspFacade& clasp);
	virtual void        printHelp(const Potassco::ProgramOptions::OptionContext& root);
private:
	ClaspApp(const ClaspApp&);
	ClaspApp& operator=(const ClaspApp&);
};
}}
#endif
