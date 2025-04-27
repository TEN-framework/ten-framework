// {{{ MIT License

// Copyright 2017 Roland Kaminski

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

// }}}

#include "clingo.h"
#include <clingo/incmode.hh>
#include <clingo/control.hh>
#include <clingo/astv2.hh>
#include <gringo/input/nongroundparser.hh>
#include <gringo/input/groundtermparser.hh>
#include <gringo/input/programbuilder.hh>
#include <gringo/input/program.hh>
#include <gringo/ground/program.hh>
#include <gringo/output/output.hh>
#include <gringo/output/statements.hh>
#include <gringo/logger.hh>
#include <clingo/scripts.hh>
#include <potassco/application.h>
#include <potassco/program_opts/typed_value.h>
#include <climits>
#include <iostream>
#include <stdexcept>

namespace Gringo {

using StrVec = std::vector<std::string>;

struct GringoOptions {
    using SigVec = std::vector<Sig>;
    StrVec                     defines;
    Output::OutputOptions outputOptions;
    Output::OutputFormat  outputFormat          = Output::OutputFormat::INTERMEDIATE;
    bool                          verbose               = false;
    bool                          wNoOperationUndefined = false;
    bool                          wNoAtomUndef          = false;
    bool                          wNoFileIncluded       = false;
    bool                          wNoGlobalVariable     = false;
    bool                          wNoOther              = false;
    bool                          rewriteMinimize       = false;
    bool                          keepFacts             = false;
    bool                          singleShot            = false;
    SigVec                        sigvec;
};

static inline std::vector<std::string> split(std::string const &source, char const *delimiter = " ", bool keepEmpty = false) {
    std::vector<std::string> results;
    size_t prev = 0;
    size_t next = 0;
    while ((next = source.find_first_of(delimiter, prev)) != std::string::npos) {
        if (keepEmpty || (next - prev != 0)) { results.push_back(source.substr(prev, next - prev)); }
        prev = next + 1;
    }
    if (prev < source.size()) { results.push_back(source.substr(prev)); }
    return results;
}

static inline bool parseSigVec(const std::string& str, GringoOptions::SigVec& sigvec) {
    for (auto &x : split(str, ",")) {
        auto y = split(x, "/");
        if (y.size() != 2) { return false; }
        unsigned a;
        if (!Potassco::string_cast<unsigned>(y[1], a)) { return false; }
        bool sign = !y[0].empty() && y[0][0] == '-';
        if (sign) { y[0] = y[0].substr(1); }
        sigvec.emplace_back(y[0].c_str(), a, sign);
    }
    return true;
}

#define LOG if (opts.verbose) std::cerr
struct IncrementalControl : Control, private Output::ASPIFOutBackend {
    IncrementalControl(Output::OutputBase &out, StrVec const &files, GringoOptions const &opts)
    : out(out)
    , scripts(g_scripts())
    , pb(scripts, prg, out.outPreds, defs, opts.rewriteMinimize)
    , parser(pb, *this, incmode)
    , opts(opts) {
        using namespace Gringo;
        // TODO: should go where python script is once refactored
        out.keepFacts = opts.keepFacts;
        logger_.enable(Warnings::OperationUndefined, !opts.wNoOperationUndefined);
        logger_.enable(Warnings::AtomUndefined, !opts.wNoAtomUndef);
        logger_.enable(Warnings::FileIncluded, !opts.wNoFileIncluded);
        logger_.enable(Warnings::GlobalVariable, !opts.wNoGlobalVariable);
        logger_.enable(Warnings::Other, !opts.wNoOther);
        for (auto &x : opts.defines) {
            LOG << "define: " << x << std::endl;
            parser.parseDefine(x, logger_);
        }
        for (auto &x : files) {
            LOG << "file: " << x << std::endl;
            parser.pushFile(std::string(x), logger_);
        }
        if (files.empty()) {
            LOG << "reading from stdin" << std::endl;
            parser.pushFile("-", logger_);
        }
        parse();
    }
    Backend &getASPIFBackend() override {
        return *this;
    }
    Output::OutputBase &beginOutput() override {
        beginAddBackend();
        return out;
    }
    void endOutput() override {
        endAddBackend();
    }

    Logger &logger() override {
        return logger_;
    }
    void update() {
        // This function starts a new step and has to be called at least once
        // before anything that causes output at the beginning of excecution or
        // after a solve step.
        if (!grounded) {
            if (!initialized_) {
                initialized_ = true;
                out.init(incremental_);
            }
            out.beginStep();
            grounded = true;
        }
    }
    void parse() {
        if (!parser.empty()) {
            switch (parser.parse(logger_)) {
                case Input::ParseResult::Gringo: {
                    defs.init(logger_);
                    parsed = true;
                    break;
                }
                case Input::ParseResult::ASPIF: {
                    break;
                }
            }
        }
        if (logger_.hasError()) {
            throw std::runtime_error("parsing failed");
        }
    }
    void ground(Control::GroundVec const &parts, Context *context) override {
        update();
        // NOTE: it would be cool to have assumptions in the lparse output
        if (parsed) {
            LOG << "************** parsed program **************" << std::endl << prg;
            prg.rewrite(defs, logger_);
            LOG << "************* rewritten program ************" << std::endl << prg;
            prg.check(logger_);
            if (logger_.hasError()) {
                throw std::runtime_error("grounding stopped because of errors");
            }
            parsed = false;
        }
        if (!parts.empty()) {
            Ground::Parameters params;
            std::set<Sig> sigs;
            for (auto &x : parts) {
                params.add(x.first, SymVec(x.second));
                sigs.emplace(x.first, numeric_cast<uint32_t>(x.second.size()), false);
            }
            Ground::Program gPrg(prg.toGround(sigs, out.data, logger_));
            LOG << "************* intermediate program *************" << std::endl << gPrg << std::endl;
            LOG << "*************** grounded program ***************" << std::endl;
            gPrg.prepare(params, out, logger_);
            scripts.withContext(context, [&, this](Context &ctx) { gPrg.ground(ctx, out, logger_); });
        }
    }
    void add(std::string const &name, StringVec const &params, std::string const &part) override {
        Location loc("<block>", 1, 1, "<block>", 1, 1);
        Input::IdVec idVec;
        for (auto &x : params) { idVec.emplace_back(loc, x); }
        parser.pushBlock(name, std::move(idVec), part, logger_);
        parse();
    }
    Symbol getConst(std::string const &name) const override {
        auto ret = defs.defs().find(name.c_str());
        if (ret != defs.defs().end()) {
            bool undefined = false;
            Symbol val = std::get<2>(ret->second)->eval(undefined, const_cast<Logger&>(logger_));
            if (!undefined) { return val; }
        }
        return Symbol();
    }
    void load(std::string const &filename) override {
        parser.pushFile(std::string(filename), logger_);
        parse();
    }
    bool blocked() override { return false; }
    USolveFuture solve(Assumptions ass, clingo_solve_mode_bitset_t, USolveEventHandler cb) override {
        update();
        grounded = false;
        out.endStep(ass);
        out.reset(true);
        return gringo_make_unique<DefaultSolveFuture>(std::move(cb));
    }
    void interrupt() override { }
    void *claspFacade() override {
        return nullptr;
    }
    void beginAdd() override {
        parse();
    }
    void add(clingo_ast_t const &ast) override {
        Input::parse(pb, logger_, ast.ast);
    }
    void endAdd() override {
        defs.init(logger_);
    }
    void registerObserver(UBackend prg, bool replace) override {
        out.registerObserver(std::move(prg), replace);
    }
    Potassco::AbstractStatistics const *statistics() const override { throw std::runtime_error("statistics not supported (yet)"); }
    bool isConflicting() const noexcept override { return false; }
    void assignExternal(Potassco::Atom_t ext, Potassco::Value_t val) override {
        update();
        if (auto *b = out.backend()) { b->external(ext, val); }
    }
    SymbolicAtoms const &getDomain() const override { throw std::runtime_error("domain introspection not supported"); }
    ConfigProxy &getConf() override { throw std::runtime_error("configuration not supported"); }
    void registerPropagator(UProp, bool) override { throw std::runtime_error("theory propagators not supported"); }
    void useEnumAssumption(bool) override { }
    bool useEnumAssumption() const override { return false; }
    void cleanup() override { }
    void enableCleanup(bool) override { }
    bool enableCleanup() const override { return false; }
    virtual ~IncrementalControl() { }
    Output::DomainData const &theory() const override { return out.data; }
    bool beginAddBackend() override {
        update();
        backend_prg_ = std::make_unique<Ground::Program>(prg.toGround({}, out.data, logger_));
        backend_prg_->prepare({}, out, logger_);
        backend_ = out.backend();
        return backend_ != nullptr;
    }
    Backend *getBackend() override {
        if (!backend_) { throw std::runtime_error("backend not available"); }
        return backend_;
    }
    Output::TheoryData &theoryData() override {
        return out.data.theory();
    }
    Id_t addAtom(Symbol sym) override {
        bool added = false;
        auto atom  = out.addAtom(sym, &added);
        if (added) { added_atoms_.emplace_back(sym); }
        return atom;
    }
    void addFact(Potassco::Atom_t uid) override {
        added_facts_.emplace(uid);
    }
    void endAddBackend() override {
        for (auto &sym : added_atoms_) {
            auto it = out.predDoms().find(sym.sig());
            assert(it != out.predDoms().end());
            auto jt = (*it)->find(sym);
            assert(jt != (*it)->end());
            assert(jt->hasUid());
            if (added_facts_.find(jt->uid()) != added_facts_.end()) {
                jt->setFact(true);
            }
        }
        added_atoms_.clear();
        added_facts_.clear();
        backend_prg_->ground(scripts, out, logger_);
        backend_prg_.reset(nullptr);
        backend_ = nullptr;
    }
    Potassco::Atom_t addProgramAtom() override { return out.data.newAtom(); }

    Input::GroundTermParser        termParser;
    Output::OutputBase            &out;
    Scripts                       &scripts;
    Defines                        defs;
    Input::Program                 prg;
    Input::NongroundProgramBuilder pb;
    Input::NonGroundParser         parser;
    GringoOptions const           &opts;
    Logger                         logger_;
    std::vector<Symbol>            added_atoms_;
    std::unordered_set<Potassco::Atom_t> added_facts_;
    Backend                       *backend_ = nullptr;
    std::unique_ptr<Ground::Program> backend_prg_;
    std::unique_ptr<Input::NongroundProgramBuilder> builder;
    bool incmode = false;
    bool parsed = false;
    bool grounded = false;
    bool initialized_ = false;
    bool incremental_ = false;
};
#undef LOG

static bool parseConst(const std::string& str, std::vector<std::string>& out) {
    out.push_back(str);
    return true;
}

inline void enableAll(GringoOptions& out, bool enable) {
    out.wNoAtomUndef          = !enable;
    out.wNoFileIncluded       = !enable;
    out.wNoOperationUndefined = !enable;
    out.wNoGlobalVariable     = !enable;
    out.wNoOther              = !enable;
}

inline bool parseWarning(const std::string& str, GringoOptions& out) {
    if (str == "none")                     { enableAll(out, false);             return true; }
    if (str == "all")                      { enableAll(out, true);              return true; }
    if (str == "no-atom-undefined")        { out.wNoAtomUndef          = true;  return true; }
    if (str ==    "atom-undefined")        { out.wNoAtomUndef          = false; return true; }
    if (str == "no-file-included")         { out.wNoFileIncluded       = true;  return true; }
    if (str ==    "file-included")         { out.wNoFileIncluded       = false; return true; }
    if (str == "no-operation-undefined")   { out.wNoOperationUndefined = true;  return true; }
    if (str ==    "operation-undefined")   { out.wNoOperationUndefined = false; return true; }
    if (str == "no-global-variable")       { out.wNoGlobalVariable     = true;  return true; }
    if (str ==    "global-variable")       { out.wNoGlobalVariable     = false; return true; }
    if (str == "no-other")                 { out.wNoOther              = true;  return true; }
    if (str ==    "other")                 { out.wNoOther              = false; return true; }
    return false;
}

inline bool parsePreserveFacts(const std::string& str, GringoOptions& out) {
    if (str == "none")   { out.keepFacts = false; out.outputOptions.preserveFacts = false; return true; }
    if (str == "body")   { out.keepFacts = true;  out.outputOptions.preserveFacts = false; return true; }
    if (str == "symtab") { out.keepFacts = false; out.outputOptions.preserveFacts = true;  return true; }
    if (str == "all")    { out.keepFacts = true;  out.outputOptions.preserveFacts = true;  return true; }
    return false;
}

static bool parseText(const std::string&, GringoOptions& out) {
    out.outputFormat = Output::OutputFormat::TEXT;
    return true;
}

struct GringoApp : public Potassco::Application {
    using StringSeq = std::vector<std::string>;
    virtual const char* getName() const    { return "gringo"; }
    virtual const char* getVersion() const { return clingo_version_string(); }
    virtual HelpOpt     getHelpOption() const { return HelpOpt("Print (<n {1=default|2=advanced}) help and exit", 2); }
    virtual void initOptions(Potassco::ProgramOptions::OptionContext& root) {
        using namespace Potassco::ProgramOptions;
        grOpts_.defines.clear();
        grOpts_.verbose = false;
        OptionGroup gringo("Gringo Options");
        gringo.addOptions()
            ("text,t", storeTo(grOpts_, parseText)->flag(), "Print plain text format")
            ("const,c", storeTo(grOpts_.defines, parseConst)->composing()->arg("<id>=<term>"), "Replace term occurrences of <id> with <term>")
            ("output,o,@1", storeTo(grOpts_.outputFormat = Output::OutputFormat::INTERMEDIATE, values<Output::OutputFormat>()
              ("intermediate", Output::OutputFormat::INTERMEDIATE)
              ("text", Output::OutputFormat::TEXT)
              ("reify", Output::OutputFormat::REIFY)
              ("smodels", Output::OutputFormat::SMODELS)), "Choose output format:\n"
             "      intermediate: print intermediate format\n"
             "      text        : print plain text format\n"
             "      reify       : print program as reified facts\n"
             "      smodels     : print smodels format\n"
             "                    (only supports basic features)")
            ("output-debug,@1", storeTo(grOpts_.outputOptions.debug = Output::OutputDebug::NONE, values<Output::OutputDebug>()
              ("none", Output::OutputDebug::NONE)
              ("text", Output::OutputDebug::TEXT)
              ("translate", Output::OutputDebug::TRANSLATE)
              ("all", Output::OutputDebug::ALL)), "Print debug information during output:\n"
             "      none     : no additional info\n"
             "      text     : print rules as plain text (prefix %%)\n"
             "      translate: print translated rules as plain text (prefix %%%%)\n"
             "      all      : combines text and translate")
            ("warn,W,@1", storeTo(grOpts_, parseWarning)->arg("<warn>")->composing(), "Enable/disable warnings:\n"
             "      none                    : disable all warnings\n"
             "      all                     : enable all warnings\n"
             "      [no-]atom-undefined     : a :- b.\n"
             "      [no-]file-included      : #include \"a.lp\". #include \"a.lp\".\n"
             "      [no-]operation-undefined: p(1/0).\n"
             "      [no-]global-variable    : :- #count { X } = 1, X = 1.\n"
             "      [no-]other              : uncategorized warnings")
            ("rewrite-minimize,@1", flag(grOpts_.rewriteMinimize = false), "Rewrite minimize constraints into rules")
            // for backward compatibility
            ("keep-facts,@4", flag(grOpts_.keepFacts = false), "Preserve facts in rule bodies.")
            ("preserve-facts,@1", storeTo(grOpts_, parsePreserveFacts),
             "Preserve facts in output:\n"
             "      none  : do not preserve\n"
             "      body  : do not preserve\n"
             "      symtab: do not preserve\n"
             "      all   : preserve all facts")
            ("reify-sccs,@1", flag(grOpts_.outputOptions.reifySCCs = false), "Calculate SCCs for reified output")
            ("reify-steps,@1", flag(grOpts_.outputOptions.reifySteps = false), "Add step numbers to reified output")
            ("show-preds,@1", storeTo(grOpts_.sigvec, parseSigVec), "Show the given signatures")
            ("single-shot,@2", flag(grOpts_.singleShot = false), "Force single-shot grounding mode")
            ;
        root.add(gringo);
        OptionGroup basic("Basic Options");
        basic.addOptions()
            ("file,f,@2", storeTo(input_)->composing(), "Input files")
            ;
        root.add(basic);
    }
    virtual void validateOptions(const Potassco::ProgramOptions::OptionContext&, const Potassco::ProgramOptions::ParsedOptions&, const Potassco::ProgramOptions::ParsedValues&) { }
    virtual void setup() { }
    static bool parsePositional(std::string const &, std::string& out) {
        out = "file";
        return true;
    }
    virtual Potassco::ProgramOptions::PosOption getPositional() const { return parsePositional; }

    virtual void printHelp(const Potassco::ProgramOptions::OptionContext& root) {
        printf("%s version %s\n", getName(), getVersion());
        printUsage();
        Potassco::ProgramOptions::FileOut out(stdout);
        root.description(out);
        printf("\nType '%s --help=2' for further options.\n", getName());
        printf("\n");
        printUsage();
    }

    virtual void printVersion() {
        char const *py_version = clingo_script_version("python");
        char const *lua_version = clingo_script_version("lua");
        Potassco::Application::printVersion();
        printf("\n");
        printf("libgringo version " CLINGO_VERSION "\n");
        printf("Configuration: %s%s, %s%s\n",
             py_version ? "with Python " : "without Python", py_version ?  py_version : "",
            lua_version ? "with Lua "    : "without Lua",   lua_version ? lua_version : "");
        printf("License: The MIT License <https://opensource.org/licenses/MIT>\n");
        fflush(stdout);
    }

    void ground(Output::OutputBase &out) {
        using namespace Gringo;
        IncrementalControl inc(out, input_, grOpts_);
        if (inc.scripts.callable("main")) {
            inc.incremental_ = !grOpts_.singleShot;
            inc.scripts.main(inc);
        }
        else if (inc.incmode) {
            inc.incremental_ = !grOpts_.singleShot;
            incmode(inc);
        }
        else {
            Control::GroundVec parts;
            parts.emplace_back("base", SymVec{});
            inc.incremental_ = false;
            inc.ground(parts, nullptr);
            inc.solve({nullptr, 0}, 0, nullptr)->get();
        }
    }

    virtual void run() {
        try {
            using namespace Gringo;
            grOpts_.verbose = verbose() == UINT_MAX;
            Output::OutputPredicates outPreds;
            for (auto &x : grOpts_.sigvec) {
                outPreds.add(Location("<cmd>",1,1,"<cmd>", 1,1), x, false);
            }
            Potassco::TheoryData data;
            data.update();
            Output::OutputBase out(data, std::move(outPreds), std::cout, grOpts_.outputFormat, grOpts_.outputOptions);
            ground(out);
        }
        catch (GringoError const &e) {
            std::cerr << e.what() << std::endl;
            throw std::runtime_error("fatal error");
        }
        catch (...) { throw; }
    }
private:
    StringSeq     input_;
    GringoOptions grOpts_;
};

} // namespace Gringo

extern "C" CLINGO_VISIBILITY_DEFAULT int gringo_main_(int argc, char *argv[]) {
    Gringo::GringoApp app;
    return app.main(argc, argv);
}

