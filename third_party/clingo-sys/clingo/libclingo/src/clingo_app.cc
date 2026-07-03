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

#include <clasp/parser.h>
#include <climits>
#include <clingo/clingo_app.hh>

#ifdef CLINGO_PROFILE
#include <gperftools/profiler.h>
#endif

namespace Gringo {

// {{{ declaration of ClingoApp

ClingoApp::ClingoApp(UIClingoApp app) : app_{std::move(app)} {}

void ClingoApp::initOptions(Potassco::ProgramOptions::OptionContext &root) {
    using namespace Potassco::ProgramOptions;
    BaseType::initOptions(root);
    OptionGroup gringo("Gringo Options");
    registerOptions(gringo, grOpts_, GringoOptions::AppType::Clingo);
    root.add(gringo);
    OptionGroup basic("Basic Options");
    basic.addOptions()(
        "mode",
        storeTo(mode_ = mode_clingo, values<Mode>()("clingo", mode_clingo)("clasp", mode_clasp)("gringo", mode_gringo)),
        "Run in {clingo|clasp|gringo} mode");
    root.add(basic);
    app_->register_options(*this);
    for (auto &group : optionGroups_) {
        root.add(group);
    }
}

void ClingoApp::validateOptions(const Potassco::ProgramOptions::OptionContext &root,
                                const Potassco::ProgramOptions::ParsedOptions &parsed,
                                const Potassco::ProgramOptions::ParsedValues &vals) {
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

void ClingoApp::addOption(char const *group, char const *option, char const *description, OptionParser parse,
                          char const *argument, bool multi) {
    using namespace Potassco::ProgramOptions;
    optionParsers_.emplace_front(parse);
    std::unique_ptr<Value> value{
        notify(&optionParsers_.front(),
               [](OptionParser *p, std::string const &, std::string const &value) { return (*p)(value.c_str()); })};
    if (argument) {
        value->arg(String(argument).c_str());
    }
    if (multi) {
        value->composing();
    }
    addGroup_(group).addOptions()(String(option).c_str(), value.release(), String(description).c_str());
}

void ClingoApp::addFlag(char const *group, char const *option, char const *description, bool &target) {
    using namespace Potassco::ProgramOptions;
    std::unique_ptr<Value> value{flag(target)};
    addGroup_(group).addOptions()(String(option).c_str(), value.release()->negatable(), String(description).c_str());
}

Clasp::ProblemType ClingoApp::getProblemType() {
    if (mode_ != mode_clasp)
        return Clasp::Problem_t::Asp;
    return Clasp::ClaspFacade::detectProblemType(getStream());
}

ClingoApp::ClaspOutput *ClingoApp::createTextOutput(const Clasp::Cli::ClaspAppBase::TextOptions &options) {
    if (mode_ == mode_gringo) {
        return nullptr;
    } else if (!app_->has_printer()) {
        return Clasp::Cli::ClaspAppBase::createTextOutput(options);
    } else {
        class CustomTextOutput : public Clasp::Cli::TextOutput {
          public:
            using BaseType = Clasp::Cli::TextOutput;
            CustomTextOutput(std::unique_ptr<ClingoControl> &ctl, IClingoApp &app,
                             const Clasp::Cli::ClaspAppBase::TextOptions &opts)
                : TextOutput(opts.verbosity, opts.format, opts.catAtom, opts.ifs), ctl_(ctl), app_(app) {}

          protected:
            void printModelValues(const Clasp::OutputTable &out, const Clasp::Model &m) override {
                if (ctl_) {
                    ClingoModel cm(*ctl_, &m);
                    std::lock_guard<decltype(ctl_->propLock_)> lock(ctl_->propLock_);
                    app_.print_model(&cm, [&]() { BaseType::printModelValues(out, m); });
                } else {
                    BaseType::printModelValues(out, m);
                }
            }

          private:
            std::unique_ptr<ClingoControl> &ctl_;
            IClingoApp &app_;
        };
        return new CustomTextOutput(grd, *app_, options);
    }
}

void ClingoApp::printHelp(const Potassco::ProgramOptions::OptionContext &root) {
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
    printf("Configuration: %s%s, %s%s\n", py_version ? "with Python " : "without Python", py_version ? py_version : "",
           lua_version ? "with Lua " : "without Lua", lua_version ? lua_version : "");
    printf("\n");
    BaseType::printLibClaspVersion();
    printf("\n");
    BaseType::printLicense();
}
bool ClingoApp::onModel(Clasp::Solver const &s, Clasp::Model const &m) {
    bool ret = !grd || grd->onModel(m);
    return BaseType::onModel(s, m) && ret;
}
void ClingoApp::onEvent(Clasp::Event const &ev) {
#if CLASP_HAS_THREADS
    Clasp::ClaspFacade::StepReady const *r = Clasp::event_cast<Clasp::ClaspFacade::StepReady>(ev);
    if (r && grd) {
        grd->onFinish(r->summary->result);
    }
#endif
    BaseType::onEvent(ev);
}
void ClingoApp::run(Clasp::ClaspFacade &clasp) {
#ifdef CLINGO_PROFILE
    struct P {
        P() { ProfilerStart("clingo.solve.prof"); }
        ~P() { ProfilerStop(); }
    } profile;
#endif
    try {
        using namespace std::placeholders;
        if (mode_ != mode_clasp) {
            ProblemType pt = getProblemType();
            Clasp::ProgramBuilder *prg = &clasp.start(claspConfig_, pt);
            grOpts_.verbose = verbose() == UINT_MAX;
            Clasp::Asp::LogicProgram *lp = mode_ != mode_gringo ? static_cast<Clasp::Asp::LogicProgram *>(prg) : 0;
            grd = Gringo::gringo_make_unique<ClingoControl>(
                g_scripts(), mode_ == mode_clingo, clasp_.get(), claspConfig_,
                std::bind(&ClingoApp::handlePostGroundOptions, this, _1),
                std::bind(&ClingoApp::handlePreSolveOptions, this, _1),
                app_->has_log() ? Logger::Printer{std::bind(&IClingoApp::log, app_.get(), _1, _2)} : nullptr,
                app_->message_limit());
            grd->main(*app_, claspAppOpts_.input, grOpts_, lp);
        } else {
            ClaspAppBase::run(clasp);
        }
    } catch (Gringo::GringoError const &e) {
        std::cerr << e.what() << std::endl;
        throw std::runtime_error("fatal error");
    } catch (...) {
        throw;
    }
}

// }}}

} // namespace Gringo
