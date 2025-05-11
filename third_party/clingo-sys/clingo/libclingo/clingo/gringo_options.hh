// {{{ MIT License

// Copyright 2024

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

#ifndef CLINGO_GRINGO_OPTIONS_HH
#define CLINGO_GRINGO_OPTIONS_HH

#include <gringo/output/output.hh>
#include <gringo/output/statements.hh>
#include <potassco/program_opts/program_options.h>
#include <vector>
namespace Gringo {

struct GringoOptions {
    enum class AppType { Gringo, Clingo, Lib };
    using SigVec = std::vector<Sig>;
    std::vector<std::string> defines;
    Output::OutputOptions outputOptions;
    Output::OutputFormat outputFormat = Output::OutputFormat::INTERMEDIATE;
    bool verbose = false;
    bool wNoOperationUndefined = false;
    bool wNoAtomUndef = false;
    bool wNoFileIncluded = false;
    bool wNoGlobalVariable = false;
    bool wNoOther = false;
    bool rewriteMinimize = false;
    bool keepFacts = false;
    bool singleShot = false;
    SigVec sigvec;
};

void registerOptions(Potassco::ProgramOptions::OptionGroup &group, GringoOptions &opts, GringoOptions::AppType type);

} // namespace Gringo

#endif // CLINGO_GRINGO_OPTIONS_HH
