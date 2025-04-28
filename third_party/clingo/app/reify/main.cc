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

#include <fstream>
#include <potassco/application.h>
#include <potassco/program_opts/typed_value.h>
#include "reify/program.hh"
#include "clingo.h"

#define CLINGO_QUOTE_(name) #name
#define CLINGO_QUOTE(name) CLINGO_QUOTE_(name)
#ifdef CLINGO_BUILD_REVISION
#   define CLINGO_VERSION_STRING CLINGO_VERSION " (" CLINGO_QUOTE(CLINGO_BUILD_REVISION) ")"
#else
#   define CLINGO_VERSION_STRING CLINGO_VERSION
#endif

struct ReifyOptions {
    bool calculateSCCs = false;
    bool reifyStep     = false;
};

class ReifyApp : public Potassco::Application {
public:
    virtual const char* getName() const    { return "reify"; }

    virtual const char* getVersion() const { return CLINGO_VERSION_STRING; }

protected:
    virtual void initOptions(Potassco::ProgramOptions::OptionContext& root) {
        using namespace Potassco::ProgramOptions;
        OptionGroup reify("Reify Options");
        reify.addOptions()
            ("sccs,c", flag(opts_.calculateSCCs), "calculate strongly connected components\n")
            ("steps,s", flag(opts_.reifyStep), "add step numbers to generated facts\n");
        root.add(reify);
        OptionGroup basic("Basic Options");
        basic.addOptions()
            ("file,f,@2", storeTo(input_), "Input files")
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
        printf("\n");
        printUsage();
    }

    virtual void printVersion() {
        Application::printVersion();
        printf("License: The MIT License <https://opensource.org/licenses/MIT>\n");
        fflush(stdout);
    }

    virtual void run() {
        Reify::Reifier reify(std::cout, opts_.calculateSCCs, opts_.reifyStep);
        if (input_.empty() || input_ == "-") {
            reify.parse(std::cin);
        }
        else {
            std::ifstream ifs(input_);
            reify.parse(ifs);
        }
    }
private:
    std::string input_;
    ReifyOptions opts_;
};

int main(int argc, char **argv) {
    ReifyApp app;
    return app.main(argc, argv);
}

