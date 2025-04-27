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

#ifndef CLINGO_CLINGOAPP_HH
#define CLINGO_CLINGOAPP_HH

#include <clasp/cli/clasp_app.h>
#include "clingo/clingocontrol.hh"

namespace Gringo {

// Standalone clingo application.
class ClingoApp : public Clasp::Cli::ClaspAppBase {
    using ClaspOutput = Clasp::Cli::Output;
    using ProblemType = Clasp::ProblemType;
    using BaseType    = Clasp::Cli::ClaspAppBase;
    using OptionParser = std::function<bool (char const *)>;
    enum class ConfigUpdate { KEEP, REPLACE };
public:
    ClingoApp(UIClingoApp app = gringo_make_unique<IClingoApp>());
    const char* getName()    const override { return app_->program_name(); }
    const char* getVersion() const override { return app_->version(); }
    const char* getUsage()   const override { return "[number] [options] [files]"; }

    void addOption(char const *group, char const *option, char const *description, OptionParser parse, char const *argument = nullptr, bool multi = false);
    void addFlag(char const *group, char const *option, char const *description, bool &target);
protected:
    enum Mode { mode_clingo = 0, mode_clasp = 1, mode_gringo = 2 };
    void        initOptions(Potassco::ProgramOptions::OptionContext& root) override;
    void        validateOptions(const Potassco::ProgramOptions::OptionContext& root, const Potassco::ProgramOptions::ParsedOptions& parsed, const Potassco::ProgramOptions::ParsedValues& vals) override;

    ProblemType getProblemType() override;
    void        run(Clasp::ClaspFacade& clasp) override;
    ClaspOutput* createOutput(ProblemType f) override;
    void        printHelp(const Potassco::ProgramOptions::OptionContext& root) override;
    void        printVersion() override;

    // -------------------------------------------------------------------------------------------
    // Event handler
    void onEvent(const Clasp::Event& ev) override;
    bool onModel(const Clasp::Solver& s, const Clasp::Model& m) override;
    // -------------------------------------------------------------------------------------------
private:
    Potassco::ProgramOptions::OptionGroup &addGroup_(char const *group_name);
private:
    ClingoApp(const ClingoApp&);
    ClingoApp& operator=(const ClingoApp&);
    ClingoOptions grOpts_;
    Mode mode_;
    std::unique_ptr<ClingoControl> grd;
    UIClingoApp app_;
    std::forward_list<OptionParser> optionParsers_;
    std::vector<Potassco::ProgramOptions::OptionGroup> optionGroups_;
};

} // namespace Gringo

#endif // CLINGO_CLINGOAPP_HH
