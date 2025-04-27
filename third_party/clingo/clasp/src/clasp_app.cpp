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
#include <clasp/cli/clasp_app.h>
#include <clasp/solver.h>
#include <clasp/dependency_graph.h>
#include <clasp/parser.h>
#include <clasp/clause.h>
#include <potassco/aspif.h>
#include <potassco/string_convert.h>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <climits>
#include <signal.h>
#ifdef _MSC_VER
#pragma warning (disable : 4996)
#endif
#if defined(__GLIBC__) || defined(__GNU_LIBRARY__)
#include <fpu_control.h>
#if defined(_FPU_EXTENDED) && defined(_FPU_SINGLE) && defined(_FPU_DOUBLE) && defined(_FPU_GETCW) && defined(_FPU_SETCW)
#define CLASP_HAS_FPU_CONTROL
inline unsigned fpuReset(unsigned m) { _FPU_SETCW(m); return m; }
inline unsigned fpuInit()            { unsigned r; _FPU_GETCW(r); fpuReset((r & ~_FPU_EXTENDED & ~_FPU_SINGLE) | _FPU_DOUBLE); return r; }
#endif
#elif defined (_MSC_VER) && !defined(_WIN64)
#include <float.h>
#define CLASP_HAS_FPU_CONTROL
inline unsigned fpuReset(unsigned m) { _controlfp(m, _MCW_PC); return m; }
inline unsigned fpuInit()            { unsigned r = _controlfp(0, 0);  fpuReset(_PC_53); return r; }
#pragma fenv_access (on)
#endif
#if !defined(CLASP_HAS_FPU_CONTROL)
inline unsigned fpuReset(unsigned) { return 0u; }
inline unsigned fpuInit()          { return 0u; }
#endif
inline bool setFpuMode() { return (sizeof(void*)*CHAR_BIT) < size_t(64u); }
namespace Clasp {
/////////////////////////////////////////////////////////////////////////////////////////
// Some helpers
/////////////////////////////////////////////////////////////////////////////////////////
static double shutdownTime_g;
static const std::string stdinStr  = "stdin";
static const std::string stdoutStr = "stdout";
inline bool isStdIn(const std::string& in)   { return in == "-" || in == stdinStr; }
inline bool isStdOut(const std::string& out) { return out == "-" || out == stdoutStr; }
/////////////////////////////////////////////////////////////////////////////////////////
// ClaspAppOptions
/////////////////////////////////////////////////////////////////////////////////////////
namespace Cli {
ClaspAppOptions::ClaspAppOptions() : outf(0), compute(0), ifs(' '), hideAux(false), onlyPre(0), printPort(false) {
	quiet[0] = quiet[1] = quiet[2] = static_cast<uint8>(UCHAR_MAX);
}
void ClaspAppOptions::initOptions(Potassco::ProgramOptions::OptionContext& root) {
	using namespace Potassco::ProgramOptions;
	OptionGroup basic("Basic Options");
	basic.addOptions()
		("print-portfolio,@1", flag(printPort), "Print default portfolio and exit")
		("quiet,q"    , notify(this, &ClaspAppOptions::mappedOpts)->implicit("2,2,2")->arg("<levels>"),
		 "Configure printing of models, costs, and calls\n"
		 "      %A: <mod>[,<cost>][,<call>]\n"
		 "        <mod> : print {0=all|1=last|2=no} models\n"
		 "        <cost>: print {0=all|1=last|2=no} optimize values [<mod>]\n"
		 "        <call>: print {0=all|1=last|2=no} call steps      [2]")
		("pre", notify(this, &ClaspAppOptions::mappedOpts)->arg("<fmt>")->implicit("aspif"), "Print simplified program and exit\n"
		 "      %A: Set output format to {aspif|smodels} (implicit: %I)")
		("outf,@1", storeTo(outf)->arg("<n>"), "Use {0=default|1=competition|2=JSON|3=no} output")
		("out-atomf,@2" , storeTo(outAtom), "Set atom format string (<Pre>?%%0<Post>?)")
		("out-ifs,@2"   , notify(this, &ClaspAppOptions::mappedOpts), "Set internal field separator")
		("out-hide-aux,@1" , flag(hideAux), "Hide auxiliary atoms in answers")
		("lemma-in,@1"     , storeTo(lemmaIn)->arg("<file>"), "Read additional lemmas from %A")
		("lemma-out,@1"    , storeTo(lemmaLog)->arg("<file>"), "Log learnt lemmas to %A")
		("lemma-out-lbd,@2", storeTo(lemma.lbdMax)->arg("<n>"), "Only log lemmas with lbd <= %A")
		("lemma-out-max,@2", storeTo(lemma.logMax)->arg("<n>"), "Stop logging after %A lemmas")
		("lemma-out-dom,@2", notify(this, &ClaspAppOptions::mappedOpts), "Log lemmas over <arg {input|output}> variables")
		("lemma-out-txt,@2", flag(lemma.logText), "Log lemmas as ground integrity constraints")
		("hcc-out,@2", storeTo(hccOut)->arg("<file>"), "Write non-hcf programs to %A.#scc")
		("file,f,@3" , storeTo(input)->composing(), "Input files")
		("compute,@2", storeTo(compute)->arg("<lit>"), "Force given literal to true")
	;
	root.add(basic);
}
bool ClaspAppOptions::mappedOpts(ClaspAppOptions* this_, const std::string& name, const std::string& value) {
	if (name == "quiet") {
		const char* err = 0;
		uint32      q[3]= {uint32(UCHAR_MAX),uint32(UCHAR_MAX),uint32(UCHAR_MAX)};
		int      parsed = Potassco::xconvert(value.c_str(), q, &err);
		for (int i = 0; i != parsed; ++i) { this_->quiet[i] = static_cast<uint8>(q[i]); }
		return parsed && *err == 0;
	}
	else if (name == "out-ifs") {
		if (value.empty() || value.size() > 2) { return false;}
		if (value.size() == 1) { this_->ifs = value[0]; return true; }
		if (value[1] == 't')   { this_->ifs = '\t'; return true; }
		if (value[1] == 'n')   { this_->ifs = '\n'; return true; }
		if (value[1] == 'v')   { this_->ifs = '\v'; return true; }
		if (value[1] == '\\')  { this_->ifs = '\\'; return true; }
	}
	else if (name == "lemma-out-dom") {
		return (this_->lemma.domOut = (strcasecmp(value.c_str(), "output") == 0)) == true || strcasecmp(value.c_str(), "input") == 0;
	}
	else if (name == "pre") {
		if      (strcasecmp(value.c_str(), "aspif")   == 0) { this_->onlyPre = (int8)AspParser::format_aspif; return true; }
		else if (strcasecmp(value.c_str(), "smodels") == 0) { this_->onlyPre = (int8)AspParser::format_smodels; return true; }
	}
	return false;
}
bool ClaspAppOptions::validateOptions(const Potassco::ProgramOptions::ParsedOptions&) {
	if (quiet[1] == static_cast<uint8>(UCHAR_MAX)) { quiet[1] = quiet[0]; }
	return true;
}
/////////////////////////////////////////////////////////////////////////////////////////
// ClaspAppBase
/////////////////////////////////////////////////////////////////////////////////////////
ClaspAppBase::ClaspAppBase() { }
ClaspAppBase::~ClaspAppBase(){ }
const int* ClaspAppBase::getSignals() const {
	static const int signals[] = {
		SIGINT, SIGTERM
#if !defined (_WIN32)
		, SIGUSR1, SIGUSR2, SIGQUIT, SIGHUP, SIGXCPU, SIGXFSZ
#endif
		, 0};
		return signals;
}
bool ClaspAppBase::parsePositional(const std::string& t, std::string& out) {
	int num;
	if (Potassco::string_cast(t, num)) { out = "number"; }
	else                               { out = "file";   }
	return true;
}
void ClaspAppBase::initOptions(Potassco::ProgramOptions::OptionContext& root) {
	claspConfig_.addOptions(root);
	claspAppOpts_.initOptions(root);
	root.find("verbose")->get()->value()->defaultsTo("1");
}

void ClaspAppBase::validateOptions(const Potassco::ProgramOptions::OptionContext&, const Potassco::ProgramOptions::ParsedOptions& parsed, const Potassco::ProgramOptions::ParsedValues& values) {
	if (claspAppOpts_.printPort) {
		printTemplate();
		exit(E_UNKNOWN);
	}
	setExitCode(E_NO_RUN);
	ProblemType pt = getProblemType();
	POTASSCO_REQUIRE(claspAppOpts_.validateOptions(parsed) && claspConfig_.finalize(parsed, pt, true), "command-line error!");
	ClaspAppOptions& app = claspAppOpts_;
	POTASSCO_REQUIRE(app.lemmaLog.empty() || isStdOut(app.lemmaLog) || (std::find(app.input.begin(), app.input.end(), app.lemmaLog) == app.input.end() && app.lemmaIn != app.lemmaLog),
		"'lemma-out': cowardly refusing to overwrite input file!");
	POTASSCO_REQUIRE(app.lemmaIn.empty() || isStdIn(app.lemmaIn) || std::ifstream(app.lemmaIn.c_str()).is_open(),
		"'lemma-in': could not open file!");
	for (std::size_t i = 1; i < app.input.size(); ++i) {
		POTASSCO_EXPECT(isStdIn(app.input[i]) || std::ifstream(app.input[i].c_str()).is_open(),
			"'%s': could not open input file!", app.input[i].c_str());
	}
	POTASSCO_REQUIRE(!app.onlyPre || pt == Problem_t::Asp, "Option '--pre' only supported for ASP!");
	setExitCode(0);
	storeCommandArgs(values);
}
void ClaspAppBase::setup() {
	ProblemType pt = getProblemType();
	clasp_         = new ClaspFacade();
	if (setFpuMode()) { fpuMode_ = fpuInit(); }
	if (!claspAppOpts_.onlyPre) {
		out_ = createOutput(pt);
		Event::Verbosity verb	= (Event::Verbosity)std::min(verbose(), (uint32)Event::verbosity_max);
		if (out_.get() && out_->verbosity() < (uint32)verb) { verb = (Event::Verbosity)out_->verbosity(); }
		if (!claspAppOpts_.lemmaLog.empty()) {
			logger_ = new LemmaLogger(claspAppOpts_.lemmaLog.c_str(), claspAppOpts_.lemma);
		}
		EventHandler::setVerbosity(Event::subsystem_facade , verb);
		EventHandler::setVerbosity(Event::subsystem_load   , verb);
		EventHandler::setVerbosity(Event::subsystem_prepare, verb);
		EventHandler::setVerbosity(Event::subsystem_solve  , verb);
		clasp_->ctx.setEventHandler(this, logger_.get() == 0 ? SharedContext::report_default : SharedContext::report_conflict);
	}
}

void ClaspAppBase::shutdown() {
	if (!clasp_.get()) { return; }
	if (logger_.get()) { logger_->close(); }
	lemmaIn_ = 0;
	const ClaspFacade::Summary& result = clasp_->shutdown();
	if (shutdownTime_g) {
		shutdownTime_g += RealTime::getTime();
		info(POTASSCO_FORMAT("Shutdown completed in %.3f seconds", shutdownTime_g));
	}
	if (out_.get())  { out_->shutdown(result); }
	setExitCode(getExitCode() | exitCode(result));
	if (setFpuMode()){ fpuReset(fpuMode_); }
}

void ClaspAppBase::run() {
	if (out_.get()) {
		Potassco::Span<std::string> in = !claspAppOpts_.input.empty() ? Potassco::toSpan(claspAppOpts_.input) : Potassco::toSpan(&stdinStr, 1);
		out_->run(getName(), getVersion(), Potassco::begin(in), Potassco::end(in));
	}
	try        { run(*clasp_); }
	catch(...) {
		try { blockSignals(); setExitCode(E_ERROR); throw; }
		catch (const std::bad_alloc&  ) { setExitCode(E_MEMORY); error("std::bad_alloc"); }
		catch (const std::exception& e) { error(e.what()); }
		catch (...)                     { ; }
	}
}

bool ClaspAppBase::onSignal(int sig) {
	if (!clasp_.get() || !clasp_->interrupt(sig)) {
		info("INTERRUPTED by signal!");
		setExitCode(E_INTERRUPT);
		shutdown();
		exit(getExitCode());
	}
	else {
		// multiple threads are active - shutdown was initiated
		shutdownTime_g = -RealTime::getTime();
		info("Sending shutdown signal...");
	}
	return false; // ignore all future signals
}

void ClaspAppBase::onEvent(const Event& ev) {
	const LogEvent* log = event_cast<LogEvent>(ev);
	if (log && log->isWarning()) {
		warn(log->msg);
		return;
	}
	else if (const NewConflictEvent* cfl = event_cast<NewConflictEvent>(ev)) {
		if (logger_.get()) { logger_->add(*cfl->solver, *cfl->learnt, cfl->info); }
		return;
	}
	if (out_.get()) {
		blockSignals();
		out_->onEvent(ev);
		unblockSignals(true);
	}
}

bool ClaspAppBase::onModel(const Solver& s, const Model& m) {
	bool ret = true;
	if (out_.get() && !out_->quiet()) {
		blockSignals();
		ret = out_->onModel(s, m);
		unblockSignals(true);
	}
	return ret;
}
bool ClaspAppBase::onUnsat(const Solver& s, const Model& m) {
	bool ret = true;
	if (out_.get() && !out_->quiet()) {
		blockSignals();
		ret = out_->onUnsat(s, m);
		unblockSignals(true);
	}
	return ret;
}

int ClaspAppBase::exitCode(const RunSummary& run) const {
	int ec = 0;
	if (run.sat())               { ec |= E_SAT;       }
	if (run.complete())          { ec |= E_EXHAUST;   }
	if (run.result.interrupted()){ ec |= E_INTERRUPT; }
	return ec;
}

void ClaspAppBase::printTemplate() const {
	printf(
		"# clasp %s configuration file\n"
		"# A configuration file contains a (possibly empty) list of configurations.\n"
		"# Each of which must have the following format:\n"
		"#   <name>[(<base>)]: <cmd>\n"
		"# where\n"
		"# <name> is an alphanumeric identifier optionally enclosed in brackets,\n"
		"# <base> is the name of one of clasp's default configs and optional, and\n"
		"# <cmd>  is a command-line string of clasp options in long-format, e.g.\n"
		"# ('--heuristic=vsids --restarts=L,100').\n"
		"#\n"
		"# SEE: clasp --help=3\n"
		"#\n"
		"# NOTE: The options '--configuration' and '--tester' must not occur in a\n"
		"#       configuration file. All other global options are ignored unless\n"
		"#       explicitly given in the very first configuration after the colon.\n"
		"#       In particular, global options from base configurations are ignored.\n"
		"#\n"
		"# NOTE: Options given on the command-line are added to all configurations in a\n"
		"#       configuration file. If an option is given both on the command-line and\n"
		"#       in a configuration file, the one from the command-line is preferred.\n"
		"#\n"
		"# NOTE: If, after adding command-line options, a configuration\n"
		"#       contains mutually exclusive options an error is raised.\n"
		"#\n"
		"# EXAMPLE: To create a new config based on clasp's inbuilt tweety configuration\n"
		"#          with global options but a different heuristic one could write:\n"
		"#\n"
		"#            'Config1(tweety): --eq=3 --trans-ext=dynamic --heuristic=domain'\n"
		"#\n"
		"#          'Config1' is the purely descriptive name of the configuration and could\n"
		"#          also be written as '[Config1]'. The following '(tweety)' indicates that\n"
		"#          our configuration should be based on clasp's tweety configuration. Finally,\n"
		"#          since global options from base configurations are ignored, we explicitly add\n"
		"#          tweety's global options '--eq=3 --trans-ext=dynamic' after the colon.\n"
		"#\n", CLASP_VERSION);
	for (ConfigIter it = ClaspCliConfig::getConfig(Clasp::Cli::config_many); it.valid(); it.next()) {
		printf("%s: %s\n", it.name(), it.args());
	}
}
void ClaspAppBase::printVersion() {
	Potassco::Application::printVersion();
	printLibClaspVersion();
	printLicense();
}
void ClaspAppBase::printLicense() const {
	printf("License: The MIT License <https://opensource.org/licenses/MIT>\n");
}
void ClaspAppBase::printLibClaspVersion() const {
	printf("libclasp version %s (libpotassco version %s)\n", CLASP_VERSION, LIB_POTASSCO_VERSION);
	printf("Configuration: WITH_THREADS=%d\n", CLASP_HAS_THREADS);
	printf("%s\n", CLASP_LEGAL);
	fflush(stdout);
}

void ClaspAppBase::printHelp(const Potassco::ProgramOptions::OptionContext& root) {
	Potassco::Application::printHelp(root);
	if (root.getActiveDescLevel() >= Potassco::ProgramOptions::desc_level_e1) {
		printf("[asp] %s\n", ClaspCliConfig::getDefaults(Problem_t::Asp));
		printf("[cnf] %s\n", ClaspCliConfig::getDefaults(Problem_t::Sat));
		printf("[opb] %s\n", ClaspCliConfig::getDefaults(Problem_t::Pb));
	}
	if (root.getActiveDescLevel() >= Potassco::ProgramOptions::desc_level_e2) {
		printf("\nDefault configurations:\n");
		printDefaultConfigs();
	}
	else {
		const char* ht3 = "\nType ";
		if (root.getActiveDescLevel() == Potassco::ProgramOptions::desc_level_default) {
			printf("\nType '%s --help=2' for more options and defaults\n", getName());
			ht3 = "and ";
		}
		printf("%s '%s --help=3' for all options and configurations.\n", ht3, getName());
	}
	fflush(stdout);
}
void ClaspAppBase::printConfig(ConfigKey k) const {
	uint32 minW = 2, maxW = 80;
	ConfigIter it = ClaspCliConfig::getConfig(k);
	printf("%s:\n%*c", it.name(), minW-1, ' ');
	const char* opts = it.args();
	for (std::size_t size = std::strlen(opts), n = maxW - minW; n < size;) {
		while (n && opts[n] != ' ') { --n; }
		if (!n) { break; }
		printf("%.*s\n%*c", static_cast<int>(n), opts, static_cast<int>(minW - 1), ' ');
		size -= n + 1;
		opts += n + 1;
		n = (maxW - minW);
	}
	printf("%s\n", opts);
}
void ClaspAppBase::printDefaultConfigs() const {
	for (int i = Clasp::Cli::config_default+1; i != Clasp::Cli::config_default_max_value; ++i) {
		printConfig(static_cast<Clasp::Cli::ConfigKey>(i));
	}
}
void ClaspAppBase::writeNonHcfs(const PrgDepGraph& graph) const {
	Potassco::StringBuilder buf;
	for (PrgDepGraph::NonHcfIter it = graph.nonHcfBegin(), end = graph.nonHcfEnd(); it != end; ++it) {
		buf.appendFormat(".%u", (*it)->id());
		WriteCnf cnf(claspAppOpts_.hccOut + buf.c_str());
		const SharedContext& ctx = (*it)->ctx();
		cnf.writeHeader(ctx.numVars(), ctx.numConstraints());
		cnf.write(ctx.numVars(), ctx.shortImplications());
		Solver::DBRef db = ctx.master()->constraints();
		for (uint32 i = 0; i != db.size(); ++i) {
			if (ClauseHead* x = db[i]->clause()) { cnf.write(x); }
		}
		for (uint32 i = 0; i != ctx.master()->trail().size(); ++i) {
			cnf.write(ctx.master()->trail()[i]);
		}
		cnf.close();
		buf.clear();
	}
}
std::istream& ClaspAppBase::getStream(bool reopen) const {
	static std::ifstream file;
	static bool isOpen = false;
	if (!isOpen || reopen) {
		file.close();
		isOpen = true;
		if (!claspAppOpts_.input.empty() && !isStdIn(claspAppOpts_.input[0])) {
			file.open(claspAppOpts_.input[0].c_str());
			POTASSCO_EXPECT(file.is_open(), "Can not read from '%s'!", claspAppOpts_.input[0].c_str());
		}
	}
	return file.is_open() ? file : std::cin;
}

// Creates output object suitable for given input format
Output* ClaspAppBase::createOutput(ProblemType f) {
	SingleOwnerPtr<Output> out;
	if (claspAppOpts_.outf == ClaspAppOptions::out_none) {
		return 0;
	}
	if (claspAppOpts_.outf != ClaspAppOptions::out_json || claspAppOpts_.onlyPre) {
		TextOutput::Format outFormat = TextOutput::format_asp;
		if      (f == Problem_t::Sat){ outFormat = TextOutput::format_sat09; }
		else if (f == Problem_t::Pb) { outFormat = TextOutput::format_pb09;  }
		else if (f == Problem_t::Asp && claspAppOpts_.outf == ClaspAppOptions::out_comp) {
			outFormat = TextOutput::format_aspcomp;
		}
		out.reset(new TextOutput(verbose(), outFormat, claspAppOpts_.outAtom.c_str(), claspAppOpts_.ifs));
		if (claspConfig_.parse.isEnabled(ParserOptions::parse_maxsat) && f == Problem_t::Sat) {
			static_cast<TextOutput*>(out.get())->result[TextOutput::res_sat] = "UNKNOWN";
		}
	}
	else {
		out.reset(new JsonOutput(verbose()));
	}
	if (claspAppOpts_.quiet[0] != static_cast<uint8>(UCHAR_MAX)) {
		out->setModelQuiet((Output::PrintLevel)std::min(uint8(Output::print_no), claspAppOpts_.quiet[0]));
	}
	if (claspAppOpts_.quiet[1] != static_cast<uint8>(UCHAR_MAX)) {
		out->setOptQuiet((Output::PrintLevel)std::min(uint8(Output::print_no), claspAppOpts_.quiet[1]));
	}
	if (claspAppOpts_.quiet[2] != static_cast<uint8>(UCHAR_MAX)) {
		out->setCallQuiet((Output::PrintLevel)std::min(uint8(Output::print_no), claspAppOpts_.quiet[2]));
	}
	if (claspAppOpts_.hideAux && clasp_.get()) {
		clasp_->ctx.output.setFilter('_');
	}
	return out.release();
}
void ClaspAppBase::storeCommandArgs(const Potassco::ProgramOptions::ParsedValues&) {
	/* We don't need the values */
}
void ClaspAppBase::handleStartOptions(ClaspFacade& clasp) {
	if (!clasp.incremental()) {
		claspConfig_.releaseOptions();
	}
	if (claspAppOpts_.compute && clasp.program()->type() == Problem_t::Asp) {
		Potassco::Lit_t lit = Potassco::neg(claspAppOpts_.compute);
		static_cast<Asp::LogicProgram*>(clasp.program())->addRule(Potassco::Head_t::Disjunctive, Potassco::toSpan<Potassco::Atom_t>(), Potassco::toSpan(&lit, 1));
	}
	if (!claspAppOpts_.lemmaIn.empty()) {
		class LemmaIn : public Potassco::AspifInput {
		public:
			typedef Potassco::AbstractProgram PrgAdapter;
			LemmaIn(const std::string& fn, PrgAdapter* prg) : Potassco::AspifInput(*prg), prg_(prg) {
				if (!isStdIn(fn)) { file_.open(fn.c_str()); }
				POTASSCO_REQUIRE(accept(getStream()), "'lemma-in': invalid input file!");
			}
			~LemmaIn() { delete prg_; }
		private:
			std::istream& getStream() { return file_.is_open() ? file_ : std::cin; }
			PrgAdapter*   prg_;
			std::ifstream file_;
		};
		SingleOwnerPtr<Potassco::AbstractProgram> prgTemp;
		if (clasp.program()->type() == Problem_t::Asp) { prgTemp = new Asp::LogicProgramAdapter(*static_cast<Asp::LogicProgram*>(clasp.program())); }
		else { prgTemp = new BasicProgramAdapter(*clasp.program()); }
		lemmaIn_ = new LemmaIn(claspAppOpts_.lemmaIn, prgTemp.release());
	}
}
bool ClaspAppBase::handlePostGroundOptions(ProgramBuilder& prg) {
	if (!claspAppOpts_.onlyPre) {
		if (lemmaIn_.get()) { lemmaIn_->parse(); }
		if (logger_.get())  { logger_->startStep(prg, clasp_->incremental()); }
		return true;
	}
	prg.endProgram();
	if (prg.type() == Problem_t::Asp) {
		Asp::LogicProgram& asp = static_cast<Asp::LogicProgram&>(prg);
		AspParser::Format outf = static_cast<AspParser::Format>(claspAppOpts_.onlyPre);
		if (outf == AspParser::format_smodels && !asp.supportsSmodels()) {
			std::ofstream null;
			try { AspParser::write(asp, null, outf); }
			catch (const std::logic_error& e) {
				error("Option '--pre': unsupported input format!");
				info(std::string(e.what()).append(" in 'smodels' format").c_str());
				info("Try '--pre=aspif' to print in 'aspif' format");
				setExitCode(E_ERROR);
				return false;
			}
		}
		AspParser::write(asp, std::cout, outf);
	}
	else {
		error("Option '--pre': unsupported input format!");
		setExitCode(E_ERROR);
	}
	return false;
}
bool ClaspAppBase::handlePreSolveOptions(ClaspFacade& clasp) {
	if (!claspAppOpts_.hccOut.empty() && clasp.ctx.sccGraph.get()){ writeNonHcfs(*clasp.ctx.sccGraph); }
	return true;
}
void ClaspAppBase::run(ClaspFacade& clasp) {
	clasp.start(claspConfig_, getStream());
	handleStartOptions(clasp);
	while (clasp.read()) {
		if (handlePostGroundOptions(*clasp.program())) {
			clasp.prepare();
			if (handlePreSolveOptions(clasp)) { clasp.solve(); }
		}
	}
}
/////////////////////////////////////////////////////////////////////////////////////////
// ClaspApp
/////////////////////////////////////////////////////////////////////////////////////////
ClaspApp::ClaspApp() {}

ProblemType ClaspApp::getProblemType() {
	return ClaspFacade::detectProblemType(getStream());
}

void ClaspApp::run(ClaspFacade& clasp) {
	ClaspAppBase::run(clasp);
}

void ClaspApp::printHelp(const Potassco::ProgramOptions::OptionContext& root) {
	ClaspAppBase::printHelp(root);
	printf("\nclasp is part of Potassco: %s\n", "http://potassco.org/clasp");
	printf("Get help/report bugs via : %s\n"  , "http://potassco.org/support\n");
	fflush(stdout);
}
/////////////////////////////////////////////////////////////////////////////////////////
// LemmaLogger
/////////////////////////////////////////////////////////////////////////////////////////
LemmaLogger::LemmaLogger(const std::string& to, const Options& o)
	: str_(isStdOut(to) ? stdout : fopen(to.c_str(), "w"))
	, inputType_(Problem_t::Asp)
	, options_(o)
	, step_(0) {
	POTASSCO_EXPECT(str_, "Could not open lemma log file '%s'!", to.c_str());
}
LemmaLogger::~LemmaLogger() { close(); }
void LemmaLogger::startStep(ProgramBuilder& prg, bool inc) {
	logged_ = 0;
	++step_;
	if (!options_.logText) {
		if (step_ == 1) { fprintf(str_, "asp 1 0 0%s\n", inc ? " incremental" : ""); }
		else            { fprintf(str_, "0\n"); }
	}
	if ((inputType_ = static_cast<Problem_t::Type>(prg.type())) == Problem_t::Asp && prg.endProgram()) {
		// create solver variable to potassco literal mapping
		Asp::LogicProgram& asp = static_cast<Asp::LogicProgram&>(prg);
		for (Asp::Atom_t a = asp.startAtom(); a != asp.startAuxAtom(); ++a) {
			Literal sLit = asp.getLiteral(a);
			if (sLit.var() >= solver2asp_.size()) {
				solver2asp_.resize(sLit.var() + 1, 0);
			}
			Potassco::Lit_t& p = solver2asp_[sLit.var()];
			if (!p || (!sLit.sign() && p < 0)) {
				p = !sLit.sign() ? Potassco::lit(a) : Potassco::neg(a);
			}
		}
	}
	solver2NameIdx_.clear();
	if (options_.logText && prg.endProgram()) {
		const SharedContext& ctx = *prg.ctx();
		for (OutputTable::pred_iterator beg = ctx.output.pred_begin(), it = beg, end = ctx.output.pred_end(); it != end; ++it) {
			Var v = it->cond.var();
			if (ctx.varInfo(v).output()) {
				if (solver2NameIdx_.size() <= v) { solver2NameIdx_.resize(v + 1, UINT32_MAX); }
				solver2NameIdx_[v] = static_cast<uint32>(it - beg);
			}
		}
	}
}
void LemmaLogger::add(const Solver& s, const LitVec& cc, const ConstraintInfo& info) {
	LitVec temp;
	const LitVec* out = &cc;
	uint32 lbd = info.lbd();
	if (lbd > options_.lbdMax || logged_ >= options_.logMax) { return; }
	if (info.aux() || options_.domOut || std::find_if(cc.begin(), cc.end(), std::not1(std::bind1st(std::mem_fun(&Solver::inputVar), &s))) != cc.end()) {
		uint8 vf = options_.domOut ? VarInfo::Input|VarInfo::Output : VarInfo::Input;
		if (!s.resolveToFlagged(cc, vf, temp, lbd) || lbd > options_.lbdMax) { return; }
		out = &temp;
	}
	char buffer[1024];
	Potassco::StringBuilder str(buffer, sizeof(buffer), Potassco::StringBuilder::Dynamic);
	if (options_.logText) { formatText(*out, s.sharedContext()->output, lbd, str); }
	else                  { formatAspif(*out, lbd, str); }
	fwrite(str.c_str(), sizeof(char), str.size(), str_);
	++logged_;
}
void LemmaLogger::formatAspif(const LitVec& cc, uint32, Potassco::StringBuilder& out) const {
	out.appendFormat("1 0 0 0 %u", (uint32)cc.size());
	for (LitVec::const_iterator it = cc.begin(), end = cc.end(); it != end; ++it) {
		Literal sLit = ~*it; // clause -> constraint
		Potassco::Lit_t a = toInt(sLit);
		if (inputType_ == Problem_t::Asp) {
			a = sLit.var() < solver2asp_.size() ? solver2asp_[sLit.var()] : 0;
			if (!a) { return; }
			if (sLit.sign() != (a < 0)) { a = -a; }
		}
		out.appendFormat(" %d", a);
	}
	out.append("\n");
}
void LemmaLogger::formatText(const LitVec& cc, const OutputTable& tab, uint32 lbd, Potassco::StringBuilder& out) const {
	out.append(":-");
	const char* sep = " ";
	for (LitVec::const_iterator it = cc.begin(), end = cc.end(); it != end; ++it) {
		Literal sLit = ~*it; // clause -> constraint
		uint32 idx = sLit.var() < solver2NameIdx_.size() ? solver2NameIdx_[sLit.var()] : UINT32_MAX;
		if (idx != UINT32_MAX) {
			const OutputTable::PredType& p = *(tab.pred_begin() + idx);
			assert(sLit.var() == p.cond.var());
			out.appendFormat("%s%s%s", sep, sLit.sign() != p.cond.sign() ? "not " : "", p.name.c_str());
		}
		else {
			if (inputType_ == Problem_t::Asp) {
				Potassco::Lit_t a = sLit.var() < solver2asp_.size() ? solver2asp_[sLit.var()] : 0;
				if (!a) { return; }
				if (sLit.sign() != (a < 0)) { a = -a; }
				sLit = Literal(Potassco::atom(a), a < 0);
			}
			out.appendFormat("%s%s__atom(%u)", sep, sLit.sign() ? "not " : "", sLit.var());
		}
		sep = ", ";
	}
	out.appendFormat(".  %%lbd = %u\n", lbd);
}
void LemmaLogger::close() {
	if (!str_) { return; }
	if (!options_.logText) { fprintf(str_, "0\n"); }
	fflush(str_);
	if (str_ != stdout) { fclose(str_); }
	str_ = 0;
	solver2asp_.clear();
}
/////////////////////////////////////////////////////////////////////////////////////////
// WriteCnf
/////////////////////////////////////////////////////////////////////////////////////////
WriteCnf::WriteCnf(const std::string& outFile) : str_(fopen(outFile.c_str(), "w")) {
	POTASSCO_EXPECT(str_, "Could not open cnf file '%s'!", outFile.c_str());
}
WriteCnf::~WriteCnf() { close(); }
void WriteCnf::writeHeader(uint32 numVars, uint32 numCons) {
	fprintf(str_, "p cnf %u %u\n", numVars, numCons);
}
void WriteCnf::write(ClauseHead* h) {
	lits_.clear();
	h->toLits(lits_);
	for (LitVec::const_iterator it = lits_.begin(), end = lits_.end(); it != end; ++it) {
		fprintf(str_, "%d ", toInt(*it));
	}
	fprintf(str_, "%d\n", 0);
}
void WriteCnf::write(Var maxVar, const ShortImplicationsGraph& g) {
	for (Var v = 1; v <= maxVar; ++v) {
		g.forEach(posLit(v), *this);
		g.forEach(negLit(v), *this);
	}
}
void WriteCnf::write(Literal u) {
	fprintf(str_, "%d 0\n", toInt(u));
}
bool WriteCnf::unary(Literal p, Literal x) const {
	return p.rep() >= x.rep() || fprintf(str_, "%d %d 0\n", toInt(~p), toInt(x)) > 0;
}
bool WriteCnf::binary(Literal p, Literal x, Literal y) const {
	return p.rep() >= x.rep() || p.rep() >= y.rep() || fprintf(str_, "%d %d %d 0\n", toInt(~p), toInt(x), toInt(y)) > 0;
}
void WriteCnf::close() {
	if (str_) {
		fflush(str_);
		fclose(str_);
		str_ = 0;
	}
}

}} // end of namespace Clasp::Cli

