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
#include <climits>
#include <clingo/astv2.hh>
#include <clingo/control.hh>
#include <clingo/gringo_options.hh>
#include <clingo/incmode.hh>
#include <clingo/scripts.hh>
#include <gringo/ground/program.hh>
#include <gringo/input/groundtermparser.hh>
#include <gringo/input/nongroundparser.hh>
#include <gringo/input/program.hh>
#include <gringo/input/programbuilder.hh>
#include <gringo/logger.hh>
#include <iostream>
#include <potassco/application.h>
#include <potassco/program_opts/typed_value.h>
#include <stdexcept>

namespace Gringo {

#define LOG                                                                                                            \
    if (opts.verbose)                                                                                                  \
    std::cerr
struct IncrementalControl : Control, private Output::ASPIFOutBackend {
    IncrementalControl(Output::OutputBase &out, std::vector<std::string> const &files, GringoOptions const &opts)
        : out(out), scripts(g_scripts()), pb(scripts, prg, out.outPreds, defs, opts.rewriteMinimize),
          parser(pb, *this, incmode), opts(opts) {
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
    Backend &getASPIFBackend() override { return *this; }
    Output::OutputBase &beginOutput() override {
        beginAddBackend();
        return out;
    }
    void endOutput() override { endAddBackend(); }

    Logger &logger() override { return logger_; }
    void update() {
        // This function starts a new step and has to be called at least once
        // before anything that causes output at the beginning of execution or
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
        for (auto &x : params) {
            idVec.emplace_back(loc, x);
        }
        parser.pushBlock(name, std::move(idVec), part, logger_);
        parse();
    }
    Symbol getConst(std::string const &name) const override {
        auto ret = defs.defs().find(name.c_str());
        if (ret != defs.defs().end()) {
            bool undefined = false;
            Symbol val = std::get<2>(ret->second)->eval(undefined, const_cast<Logger &>(logger_));
            if (!undefined) {
                return val;
            }
        }
        return Symbol();
    }
    void load(std::string const &filename) override {
        parser.pushFile(std::string(filename), logger_);
        parse();
    }
    void load_aspif(Potassco::Span<char const *> files) override {
        for (auto it = end(files), ib = begin(files); it != ib; --it) {
            parser.pushFile(std::string{*(it - 1)}, logger_);
        }
        if (!parser.empty()) {
            parser.parse_aspif(logger_);
        }
        if (logger_.hasError()) {
            throw std::runtime_error("parsing failed");
        }
    }
    bool blocked() override { return false; }
    USolveFuture solve(Assumptions ass, clingo_solve_mode_bitset_t, USolveEventHandler cb) override {
        update();
        grounded = false;
        out.endStep(ass);
        out.reset(true);
        return gringo_make_unique<DefaultSolveFuture>(std::move(cb));
    }
    void interrupt() override {}
    void *claspFacade() override { return nullptr; }
    void beginAdd() override { parse(); }
    void add(clingo_ast_t const &ast) override { Input::parse(pb, logger_, ast.ast); }
    void endAdd() override { defs.init(logger_); }
    void registerObserver(UBackend prg, bool replace) override { out.registerObserver(std::move(prg), replace); }
    Potassco::AbstractStatistics const *statistics() const override {
        throw std::runtime_error("statistics not supported (yet)");
    }
    bool isConflicting() const noexcept override { return false; }
    void assignExternal(Potassco::Atom_t ext, Potassco::Value_t val) override {
        update();
        if (auto *b = out.backend()) {
            b->external(ext, val);
        }
    }
    void assignExternal(Symbol ext, Potassco::Value_t val) override {
        update();
        auto res = out.find(ext);
        if (res.second != nullptr && res.first != res.second->end() && res.first->hasUid()) {
            assignExternal(res.first->uid(), val);
        }
    }
    void updateProject(Potassco::AtomSpan project, bool append) override {
        if (append) {
            update();
            if (auto *b = out.backend()) {
                b->project(project);
            }
        } else {
            throw std::runtime_error("replacing projection atoms is not supported");
        }
    }
    void removeMinimize() override { throw std::runtime_error("removing minimize constraints is not supported"); }
    SymbolicAtoms const &getDomain() const override { throw std::runtime_error("domain introspection not supported"); }
    ConfigProxy &getConf() override { throw std::runtime_error("configuration not supported"); }
    void registerPropagator(UProp, bool) override { throw std::runtime_error("theory propagators not supported"); }
    void useEnumAssumption(bool) override {}
    bool useEnumAssumption() const override { return false; }
    void cleanup() override {}
    void enableCleanup(bool) override {}
    bool enableCleanup() const override { return false; }
    ~IncrementalControl() override {}
    Output::DomainData const &theory() const override { return out.data; }
    bool beginAddBackend() override {
        update();
        backend_prg_ = std::make_unique<Ground::Program>(prg.toGround({}, out.data, logger_));
        backend_prg_->prepare({}, out, logger_);
        backend_ = out.backend();
        return backend_ != nullptr;
    }
    Backend *getBackend() override {
        if (!backend_) {
            throw std::runtime_error("backend not available");
        }
        return backend_;
    }
    Output::TheoryData &theoryData() override { return out.data.theory(); }
    Id_t addAtom(Symbol sym) override {
        bool added = false;
        auto atom = out.addAtom(sym, &added);
        if (added) {
            added_atoms_.emplace_back(sym);
        }
        return atom;
    }
    void addFact(Potassco::Atom_t uid) override { added_facts_.emplace(uid); }
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

    Input::GroundTermParser termParser;
    Output::OutputBase &out;
    Scripts &scripts;
    Defines defs;
    Input::Program prg;
    Input::NongroundProgramBuilder pb;
    Input::NonGroundParser parser;
    GringoOptions const &opts;
    Logger logger_;
    std::vector<Symbol> added_atoms_;
    std::unordered_set<Potassco::Atom_t> added_facts_;
    Backend *backend_ = nullptr;
    std::unique_ptr<Ground::Program> backend_prg_;
    std::unique_ptr<Input::NongroundProgramBuilder> builder;
    bool incmode = false;
    bool parsed = false;
    bool grounded = false;
    bool initialized_ = false;
    bool incremental_ = false;
};
#undef LOG

struct GringoApp : public Potassco::Application {
    using StringSeq = std::vector<std::string>;
    const char *getName() const override { return "gringo"; }
    const char *getVersion() const override { return clingo_version_string(); }
    HelpOpt getHelpOption() const override { return HelpOpt("Print (<n {1=default|2=advanced}) help and exit", 2); }
    void initOptions(Potassco::ProgramOptions::OptionContext &root) override {
        using namespace Potassco::ProgramOptions;
        OptionGroup gringo("Gringo Options");
        registerOptions(gringo, grOpts_, GringoOptions::AppType::Gringo);
        root.add(gringo);
        OptionGroup basic("Basic Options");
        basic.addOptions()("file,f,@2", storeTo(input_)->composing(), "Input files");
        root.add(basic);
    }
    void validateOptions(const Potassco::ProgramOptions::OptionContext &,
                         const Potassco::ProgramOptions::ParsedOptions &,
                         const Potassco::ProgramOptions::ParsedValues &) override {}
    void setup() override {}
    static bool parsePositional(std::string const &, std::string &out) {
        out = "file";
        return true;
    }
    Potassco::ProgramOptions::PosOption getPositional() const override { return parsePositional; }

    void printHelp(const Potassco::ProgramOptions::OptionContext &root) override {
        printf("%s version %s\n", getName(), getVersion());
        printUsage();
        Potassco::ProgramOptions::FileOut out(stdout);
        root.description(out);
        printf("\nType '%s --help=2' for further options.\n", getName());
        printf("\n");
        printUsage();
    }

    void printVersion() override {
        char const *py_version = clingo_script_version("python");
        char const *lua_version = clingo_script_version("lua");
        Potassco::Application::printVersion();
        printf("\n");
        printf("libgringo version " CLINGO_VERSION "\n");
        printf("Configuration: %s%s, %s%s\n", py_version ? "with Python " : "without Python",
               py_version ? py_version : "", lua_version ? "with Lua " : "without Lua", lua_version ? lua_version : "");
        printf("License: The MIT License <https://opensource.org/licenses/MIT>\n");
        fflush(stdout);
    }

    void ground(Output::OutputBase &out) {
        using namespace Gringo;
        IncrementalControl inc(out, input_, grOpts_);
        if (inc.scripts.callable("main")) {
            inc.incremental_ = !grOpts_.singleShot;
            inc.scripts.main(inc);
        } else if (inc.incmode) {
            inc.incremental_ = !grOpts_.singleShot;
            incmode(inc);
        } else {
            Control::GroundVec parts;
            parts.emplace_back("base", SymVec{});
            inc.incremental_ = false;
            inc.ground(parts, nullptr);
            inc.solve({nullptr, 0}, 0, nullptr)->get();
        }
    }

    void run() override {
        try {
            using namespace Gringo;
            grOpts_.verbose = verbose() == UINT_MAX;
            Output::OutputPredicates outPreds;
            for (auto &x : grOpts_.sigvec) {
                outPreds.add(Location("<cmd>", 1, 1, "<cmd>", 1, 1), x, false);
            }
            Potassco::TheoryData data;
            data.update();
            Output::OutputBase out(data, std::move(outPreds), std::cout, grOpts_.outputFormat, grOpts_.outputOptions);
            ground(out);
        } catch (GringoError const &e) {
            std::cerr << e.what() << std::endl;
            throw std::runtime_error("fatal error");
        } catch (...) {
            throw;
        }
    }

  private:
    StringSeq input_;
    GringoOptions grOpts_;
};

} // namespace Gringo

extern "C" CLINGO_VISIBILITY_DEFAULT int gringo_main_(int argc, char *argv[]) {
    Gringo::GringoApp app;
    return app.main(argc, argv);
}
