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
#include <clingo/gringo_options.hh>

#include <potassco/program_opts/typed_value.h>
#include <potassco/string_convert.h>

namespace Gringo {

static std::vector<std::string> split(std::string const &source, char const *delimiter = " ") {
    std::vector<std::string> results;
    size_t prev = 0;
    size_t next = 0;
    while ((next = source.find_first_of(delimiter, prev)) != std::string::npos) {
        if (next - prev != 0) {
            results.push_back(source.substr(prev, next - prev));
        }
        prev = next + 1;
    }
    if (prev < source.size()) {
        results.push_back(source.substr(prev));
    }
    return results;
}

static bool parseSigVec(const std::string &str, GringoOptions::SigVec &sigvec) {
    for (auto &x : split(str, ",")) {
        auto y = split(x, "/");
        if (y.size() != 2) {
            return false;
        }
        unsigned a;
        if (!Potassco::string_cast<unsigned>(y[1], a)) {
            return false;
        }
        bool sign = !y[0].empty() && y[0][0] == '-';
        if (sign) {
            y[0] = y[0].substr(1);
        }
        sigvec.emplace_back(y[0].c_str(), a, sign);
    }
    return true;
}

static void enableAll(GringoOptions &out, bool enable) {
    out.wNoAtomUndef = !enable;
    out.wNoFileIncluded = !enable;
    out.wNoOperationUndefined = !enable;
    out.wNoGlobalVariable = !enable;
    out.wNoOther = !enable;
}

static bool parseWarning(const std::string &str, GringoOptions &out) {
    if (str == "none") {
        enableAll(out, false);
        return true;
    }
    if (str == "all") {
        enableAll(out, true);
        return true;
    }
    if (str == "no-atom-undefined") {
        out.wNoAtomUndef = true;
        return true;
    }
    if (str == "atom-undefined") {
        out.wNoAtomUndef = false;
        return true;
    }
    if (str == "no-file-included") {
        out.wNoFileIncluded = true;
        return true;
    }
    if (str == "file-included") {
        out.wNoFileIncluded = false;
        return true;
    }
    if (str == "no-operation-undefined") {
        out.wNoOperationUndefined = true;
        return true;
    }
    if (str == "operation-undefined") {
        out.wNoOperationUndefined = false;
        return true;
    }
    if (str == "no-global-variable") {
        out.wNoGlobalVariable = true;
        return true;
    }
    if (str == "global-variable") {
        out.wNoGlobalVariable = false;
        return true;
    }
    if (str == "no-other") {
        out.wNoOther = true;
        return true;
    }
    if (str == "other") {
        out.wNoOther = false;
        return true;
    }
    return false;
}

static bool parsePreserveFacts(const std::string &str, GringoOptions &out) {
    if (str == "none") {
        out.keepFacts = false;
        out.outputOptions.preserveFacts = false;
        return true;
    }
    if (str == "body") {
        out.keepFacts = true;
        out.outputOptions.preserveFacts = false;
        return true;
    }
    if (str == "symtab") {
        out.keepFacts = false;
        out.outputOptions.preserveFacts = true;
        return true;
    }
    if (str == "all") {
        out.keepFacts = true;
        out.outputOptions.preserveFacts = true;
        return true;
    }
    return false;
}

static bool parseText(const std::string &, GringoOptions &out) {
    out.outputFormat = Output::OutputFormat::TEXT;
    return true;
}

void registerOptions(Potassco::ProgramOptions::OptionGroup &group, GringoOptions &opts, GringoOptions::AppType type) {
    using namespace Potassco::ProgramOptions;
    auto level = [&](int i) {
        return type != GringoOptions::AppType::Lib ? DescriptionLevel(i) : DescriptionLevel::desc_level_default;
    };
    auto name = [](const char *n, char a = 0) { return std::pair<const char *, char>(n, a); };
    auto push = [&](std::pair<const char *, char> n, Value *v, const char *desc,
                    DescriptionLevel l = DescriptionLevel::desc_level_default) {
        group.addOption(SharedOptPtr(new Option(n.first, n.second, desc, v->level(l))));
    };
    opts.defines.clear();
    opts.verbose = false;
    if (type != GringoOptions::AppType::Lib) {
        auto alias = char(type == GringoOptions::AppType::Gringo ? 't' : 0);
        push(name("text", alias), storeTo(opts, parseText)->flag(), "Print plain text format");
    } else {
        push(name("verbose", 'V'), flag(opts.verbose = false), "Enable verbose output");
    }
    push(name("const", 'c'),
         storeTo(
             opts.defines,
             +[](const std::string &str, std::vector<std::string> &out) {
                 out.push_back(str);
                 return true;
             })
             ->composing()
             ->arg("<id>=<term>"),
         "Replace term occurrences of <id> with <term>");
    if (type != GringoOptions::AppType::Lib) {
        push(name("output", 'o'),
             storeTo(opts.outputFormat = Gringo::Output::OutputFormat::INTERMEDIATE,
                     values<Gringo::Output::OutputFormat>()("intermediate", Gringo::Output::OutputFormat::INTERMEDIATE)(
                         "text", Gringo::Output::OutputFormat::TEXT)("reify", Gringo::Output::OutputFormat::REIFY)(
                         "smodels", Gringo::Output::OutputFormat::SMODELS)),
             "Choose output format:\n"
             "      intermediate: print intermediate format\n"
             "      text        : print plain text format\n"
             "      reify       : print program as reified facts\n"
             "      smodels     : print smodels format\n"
             "                    (only supports basic features)",
             level(1));
    }
    push(name("output-debug"),
         storeTo(opts.outputOptions.debug = Output::OutputDebug::NONE,
                 values<Output::OutputDebug>()("none", Output::OutputDebug::NONE)("text", Output::OutputDebug::TEXT)(
                     "translate", Output::OutputDebug::TRANSLATE)("all", Output::OutputDebug::ALL)),
         "Print debug information during output:\n"
         "      none     : no additional info\n"
         "      text     : print rules as plain text (prefix %%)\n"
         "      translate: print translated rules as plain text (prefix %%%%)\n"
         "      all      : combines text and translate",
         level(1));
    push(name("warn", 'W'), storeTo(opts, parseWarning)->arg("<warn>")->composing(),
         "Enable/disable warnings:\n"
         "      none                    : disable all warnings\n"
         "      all                     : enable all warnings\n"
         "      [no-]atom-undefined     : a :- b.\n"
         "      [no-]file-included      : #include \"a.lp\". #include \"a.lp\".\n"
         "      [no-]operation-undefined: p(1/0).\n"
         "      [no-]global-variable    : :- #count { X } = 1, X = 1.\n"
         "      [no-]other              : uncategorized warnings",
         level(1));
    push(name("rewrite-minimize"), flag(opts.rewriteMinimize = false), "Rewrite minimize constraints into rules",
         level(1));
    // for backward compatibility
    push(name("keep-facts"), flag(opts.keepFacts = false), "Preserve facts in rule bodies.", level(5));
    push(name("preserve-facts"), storeTo(opts, parsePreserveFacts),
         "Preserve facts in output:\n"
         "      none  : do not preserve\n"
         "      body  : do not preserve\n"
         "      symtab: do not preserve\n"
         "      all   : preserve all facts",
         level(1));
    if (type != GringoOptions::AppType::Lib) {
        push(name("reify-sccs"), flag(opts.outputOptions.reifySCCs = false), "Calculate SCCs for reified output",
             level(1));
        push(name("reify-steps"), flag(opts.outputOptions.reifySteps = false), "Add step numbers to reified output",
             level(1));
    }
    push(name("show-preds"), storeTo(opts.sigvec, parseSigVec), "Show the given signatures", level(1));
    push(name("single-shot"), flag(opts.singleShot = false), "Force single-shot solving mode", level(2));
}
} // namespace Gringo
