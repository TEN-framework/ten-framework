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

#include <clingo/clingo_app.hh>
#include <clasp/parser.h>
#include <climits>

namespace Gringo {

// {{{ declaration of ClingoApp

ClingoApp::ClingoApp(UIClingoApp app)
: app_{std::move(app)} { }

static bool parseConst(const std::string& str, std::vector<std::string>& out) {
    out.push_back(str);
    return true;
}

static bool parseText(const std::string&, ClingoOptions& out) {
    out.outputFormat = Gringo::Output::OutputFormat::TEXT;
    return true;
}

void ClingoApp::initOptions(Potassco::ProgramOptions::OptionContext& root) {
    using namespace Potassco::ProgramOptions;
    BaseType::initOptions(root);
    grOpts_.defines.clear();
    grOpts_.verbose = false;
    OptionGroup gringo("Gringo Options");
    gringo.addOptions()
        ("text", storeTo(grOpts_, parseText)->flag(), "Print plain text format")
        ("const,c", storeTo(grOpts_.defines, parseConst)->composing()->arg("<id>=<term>"), "Replace term occurrences of <id> with <term>")
        ("output,o,@1", storeTo(grOpts_.outputFormat = Gringo::Output::OutputFormat::INTERMEDIATE, values<Gringo::Output::OutputFormat>()
          ("intermediate", Gringo::Output::OutputFormat::INTERMEDIATE)
          ("text", Gringo::Output::OutputFormat::TEXT)
          ("reify", Gringo::Output::OutputFormat::REIFY)
          ("smodels", Gringo::Output::OutputFormat::SMODELS)), "Choose output format:\n"
             "      intermediate: print intermediate format\n"
             "      text        : print plain text format\n"
             "      reify       : print program as reified facts\n"
             "      smodels     : print smodels format\n"
             "                    (only supports basic features)")
        ("output-debug,@1", storeTo(grOpts_.outputOptions.debug = Gringo::Output::OutputDebug::NONE, values<Gringo::Output::OutputDebug>()
          ("none", Gringo::Output::OutputDebug::NONE)
          ("text", Gringo::Output::OutputDebug::TEXT)
          ("translate", Gringo::Output::OutputDebug::TRANSLATE)
          ("all", Gringo::Output::OutputDebug::ALL)), "Print debug information during output:\n"
         "      none     : no additional info\n"
         "      text     : print rules as plain text (prefix %%)\n"
         "      translate: print translated rules as plain text (prefix %%%%)\n"
         "      all      : combines text and translate")
        ("warn,W,@1"                   , storeTo(grOpts_, parseWarning)->arg("<warn>")->composing(), "Enable/disable warnings:\n"
         "      none                    : disable all warnings\n"
         "      all                     : enable all warnings\n"
         "      [no-]atom-undefined     : a :- b.\n"
         "      [no-]file-included      : #include \"a.lp\". #include \"a.lp\".\n"
         "      [no-]operation-undefined: p(1/0).\n"
         "      [no-]global-variable    : :- #count { X } = 1, X = 1.\n"
         "      [no-]other              : clasp related and uncategorized warnings")
        ("rewrite-minimize,@1"      , flag(grOpts_.rewriteMinimize = false), "Rewrite minimize constraints into rules")
        // for backward compatibility
        ("keep-facts,@3"            , flag(grOpts_.keepFacts = false), "Do not remove facts from normal rules")
        ("preserve-facts,@1"        , storeTo(grOpts_, parsePreserveFacts),
         "Preserve facts in output:\n"
         "      none  : do not preserve\n"
         "      body  : do not preserve\n"
         "      symtab: do not preserve\n"
         "      all   : preserve all facts")
        ("reify-sccs,@1"            , flag(grOpts_.outputOptions.reifySCCs = false), "Calculate SCCs for reified output")
        ("reify-steps,@1"           , flag(grOpts_.outputOptions.reifySteps = false), "Add step numbers to reified output")
        ("show-preds,@1"            , storeTo(grOpts_.sigvec, parseSigVec), "Show the given signatures")
        ("single-shot,@2"           , flag(grOpts_.singleShot = false), "Force single-shot solving mode")
        ;
    root.add(gringo);

    OptionGroup basic("Basic Options");
    basic.addOptions()
        ("mode", storeTo(mode_ = mode_clingo, values<Mode>()
            ("clingo", mode_clingo)
            ("clasp", mode_clasp)
            ("gringo", mode_gringo)),
         "Run in {clingo|clasp|gringo} mode")
        ;
    root.add(basic);
    app_->register_options(*this);
    for (auto &group : optionGroups_) { root.add(group); }
}

void ClingoApp::validateOptions(const Potassco::ProgramOptions::OptionContext& root, const Potassco::ProgramOptions::ParsedOptions& parsed, const Potassco::ProgramOptions::ParsedValues& vals) {
    BaseType::validateOptions(root, parsed, vals);
    if (parsed.count("text") > 0) {
        if (parsed.count("output") > 0) {
            error("'--text' and '--output' are mutually exclusive!");
            exit(Clasp::Cli::E_NO_RUN);
        }
        if (parsed.count("mode") > 0 && mode_ != mode_gringo) {
            error("'--text' can only be used with '--mode=gringo'!");
            exit(Clasp::Cli::E_NO_RUN);
        }
        mode_ = mode_gringo;
    }
    if (parsed.count("output") > 0) {
        if (parsed.count("mode") > 0 && mode_ != mode_gringo) {
            error("'--output' can only be used with '--mode=gringo'!");
            exit(Clasp::Cli::E_NO_RUN);
        }
        mode_ = mode_gringo;
    }
    app_->validate_options();
}

Potassco::ProgramOptions::OptionGroup &ClingoApp::addGroup_(char const *group_name) {
    using namespace Potassco::ProgramOptions;
    OptionGroup *group = nullptr;
    for (auto &x : optionGroups_) {
        if (x.caption() == group_name) {
            group = &x;
            break;
        }
    }
    if (!group) {
        optionGroups_.emplace_back(group_name);
        group = &optionGroups_.back();
    }
    return *group;
}

void ClingoApp::addOption(char const *group, char const *option, char const *description, OptionParser parse, char const *argument, bool multi) {
    using namespace Potassco::ProgramOptions;
    optionParsers_.emplace_front(parse);
    std::unique_ptr<Value> value{notify(&optionParsers_.front(), [](OptionParser *p, std::string const &, std::string const &value){ return (*p)(value.c_str()); })};
    if (argument) { value->arg(String(argument).c_str()); }
    if (multi) { value->composing(); }
    addGroup_(group).addOptions()(String(option).c_str(), value.release(), String(description).c_str());
}

void ClingoApp::addFlag(char const *group, char const *option, char const *description, bool &target) {
    using namespace Potassco::ProgramOptions;
    std::unique_ptr<Value> value{flag(target)};
    addGroup_(group).addOptions()(String(option).c_str(), value.release()->negatable(), String(description).c_str());
}

Clasp::ProblemType ClingoApp::getProblemType() {
    if (mode_ != mode_clasp) return Clasp::Problem_t::Asp;
    return Clasp::ClaspFacade::detectProblemType(getStream());
}

// TODO: the code below is annoying. There is too much copy and paste. The
// easiest way would be if the textoutput would already provide something to
// customize the output.
namespace {

class CustomTextOutput : public Clasp::Cli::TextOutput {
public:
    CustomTextOutput(std::unique_ptr<ClingoControl> &ctl, IClingoApp &app, Clasp::uint32 verbosity, Format f, const char* catAtom = 0, char ifs = ' ')
    : TextOutput(verbosity, f, catAtom, ifs), ctl_(ctl), app_(app) { }

protected:
    void printModel(const Clasp::OutputTable& out, const Clasp::Model& m, PrintLevel x) override {
        if (ctl_) {
            if (x == modelQ()) {
                comment(1, "%s: %" PRIu64"\n", !m.up ? "Answer" : "Update", m.num);
                ClingoModel cm(*ctl_, &m);
                std::lock_guard<decltype(ctl_->propLock_)> lock(ctl_->propLock_);
                app_.print_model(&cm, [&]() { printValues(out, m); });
            }
            if (x == optQ()) {
                printMeta(out, m);
            }
            fflush(stdout);
        }
        else { Clasp::Cli::TextOutput::printModel(out, m, x); }
    }
private:
    std::unique_ptr<ClingoControl> &ctl_;
    IClingoApp &app_;
};

} // namespace

ClingoApp::ClaspOutput* ClingoApp::createOutput(ProblemType f) {
    if (mode_ == mode_gringo) return 0;
    using namespace Clasp;
    using namespace Clasp::Cli;
    SingleOwnerPtr<ClaspOutput> out;
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
        if (app_->has_printer()) {
            out.reset(new CustomTextOutput(grd, *app_, verbose(), outFormat, claspAppOpts_.outAtom.c_str(), claspAppOpts_.ifs));
        }
        else {
            out.reset(new TextOutput(verbose(), outFormat, claspAppOpts_.outAtom.c_str(), claspAppOpts_.ifs));
        }
        if (claspConfig_.parse.isEnabled(ParserOptions::parse_maxsat) && f == Problem_t::Sat) {
            static_cast<TextOutput*>(out.get())->result[TextOutput::res_sat] = "UNKNOWN";
        }
    }
    else {
        out.reset(new JsonOutput(verbose()));
    }
    if (claspAppOpts_.quiet[0] != static_cast<uint8>(UCHAR_MAX)) {
        out->setModelQuiet((ClaspOutput::PrintLevel)std::min(uint8(ClaspOutput::print_no), claspAppOpts_.quiet[0]));
    }
    if (claspAppOpts_.quiet[1] != static_cast<uint8>(UCHAR_MAX)) {
        out->setOptQuiet((ClaspOutput::PrintLevel)std::min(uint8(ClaspOutput::print_no), claspAppOpts_.quiet[1]));
    }
    if (claspAppOpts_.quiet[2] != static_cast<uint8>(UCHAR_MAX)) {
        out->setCallQuiet((ClaspOutput::PrintLevel)std::min(uint8(ClaspOutput::print_no), claspAppOpts_.quiet[2]));
    }
    if (claspAppOpts_.hideAux && clasp_.get()) {
        clasp_->ctx.output.setFilter('_');
    }
    return out.release();
}

void ClingoApp::printHelp(const Potassco::ProgramOptions::OptionContext& root) {
    BaseType::printHelp(root);
    printf("\nclingo is part of Potassco: %s\n", "https://potassco.org/clingo");
    printf("Get help/report bugs via : https://potassco.org/support\n");
    fflush(stdout);
}

void ClingoApp::printVersion() {
    char const *py_version = clingo_script_version("python");
    char const *lua_version = clingo_script_version("lua");
    Potassco::Application::printVersion();
    printf("\n");
    printf("libclingo version " CLINGO_VERSION "\n");
    printf("Configuration: %s%s, %s%s\n",
         py_version ? "with Python " : "without Python", py_version ?  py_version : "",
        lua_version ? "with Lua "    : "without Lua",   lua_version ? lua_version : "");
    printf("\n");
    BaseType::printLibClaspVersion();
    printf("\n");
    BaseType::printLicense();
}
bool ClingoApp::onModel(Clasp::Solver const& s, Clasp::Model const& m) {
    bool ret = !grd || grd->onModel(m);
    return BaseType::onModel(s, m) && ret;
}
void ClingoApp::onEvent(Clasp::Event const& ev) {
#if CLASP_HAS_THREADS
    Clasp::ClaspFacade::StepReady const *r = Clasp::event_cast<Clasp::ClaspFacade::StepReady>(ev);
    if (r && grd) { grd->onFinish(r->summary->result); }
#endif
    BaseType::onEvent(ev);
}
void ClingoApp::run(Clasp::ClaspFacade& clasp) {
    try {
        using namespace std::placeholders;
        if (mode_ != mode_clasp) {
            ProblemType     pt  = getProblemType();
            Clasp::ProgramBuilder* prg = &clasp.start(claspConfig_, pt);
            grOpts_.verbose = verbose() == UINT_MAX;
            Clasp::Asp::LogicProgram* lp = mode_ != mode_gringo ? static_cast<Clasp::Asp::LogicProgram*>(prg) : 0;
            grd = Gringo::gringo_make_unique<ClingoControl>(g_scripts(), mode_ == mode_clingo, clasp_.get(), claspConfig_, std::bind(&ClingoApp::handlePostGroundOptions, this, _1), std::bind(&ClingoApp::handlePreSolveOptions, this, _1), app_->has_log() ? Logger::Printer{std::bind(&IClingoApp::log, app_.get(), _1, _2)} : nullptr, app_->message_limit());
            grd->main(*app_, claspAppOpts_.input, grOpts_, lp);
        }
        else {
            ClaspAppBase::run(clasp);
        }
    }
    catch (Gringo::GringoError const &e) {
        std::cerr << e.what() << std::endl;
        throw std::runtime_error("fatal error");
    }
    catch (...) { throw; }
}

// }}}

} // namespace Gringo

