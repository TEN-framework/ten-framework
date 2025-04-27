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

#ifndef _GRINGO_GROUND_TEST_GROUNDER_HELPER_HH
#define _GRINGO_GROUND_TEST_GROUNDER_HELPER_HH

#include "gringo/logger.hh"
#include "gringo/ground/dependency.hh"
#include "gringo/input/nongroundparser.hh"
#include "gringo/input/program.hh"
#include "gringo/output/output.hh"

#include "tests/tests.hh"

#include <regex>

namespace Gringo { namespace Ground { namespace Test {

inline void ground(std::string const &str, Output::OutputFormat fmt, std::ostream &ss) {
    Gringo::Test::TestGringoModule module;
    Potassco::TheoryData td;
    Output::OutputBase out(td, {}, ss, fmt);
    Input::Program prg;
    Defines defs;
    Gringo::Test::TestContext context;
    NullBackend bck;
    Input::NongroundProgramBuilder pb{ context, prg, out.outPreds, defs };
    bool incmode;
    Input::NonGroundParser ngp{ pb, bck, incmode };
    ngp.pushStream("-", gringo_make_unique<std::stringstream>(str), module.logger);
    ngp.parse(module.logger);
    prg.rewrite(defs, module.logger);
    Ground::Program gPrg(prg.toGround({Sig{"base", 0, false}}, out.data, module.logger));
    Parameters params;
    params.add("base", {});
    gPrg.prepare(params, out, module);
    gPrg.ground(context, out, module);
    out.endStep({});
}

inline std::string groundText(std::string const &str, std::initializer_list<std::string> filter = {""}) {
    std::regex delayedDef("^#delayed\\(([0-9]+)\\) <=> (.*)$");
    std::regex delayedOcc("#delayed\\(([0-9]+)\\)");
    std::map<std::string, std::string> delayedMap;
    std::stringstream ss;

    ground(str, Output::OutputFormat::TEXT, ss);

    std::string line;
    std::vector<std::string> res;
    ss.seekg(0, std::ios::beg);
    while (std::getline(ss, line)) {
        std::smatch m;
        if (std::regex_match(line, m, delayedDef)) {
            delayedMap[m[1]] = m[2];
        }
        else if (!line.compare(0, 9, "#delayed(")) {
            res.emplace_back(std::move(line));
        }
        else {
            for (auto &x : filter) {
                if (!line.compare(0, x.size(), x)) {
                    res.emplace_back(std::move(line));
                    break;
                }
            }
        }
    }
    for (auto &x : res) {
        std::string r;
        auto st = x.cbegin();
        for (auto it = std::sregex_iterator(x.begin(), x.end(), delayedOcc), ie = std::sregex_iterator(); it != ie; ++it) {
            std::smatch match = *it;
            r.append(st, match.prefix().second);
            st = match.suffix().first;
            r.append(delayedMap[match[1]]);
        }
        r.append(st, x.cend());
        x = r;
    }
    std::stringstream oss;
    std::sort(res.begin(), res.end());
    for (auto &x : res) { oss << x << "\n"; }
    return oss.str();
}

inline std::string groundAspif(std::string const &str) {
    std::stringstream ss;
    ground(str, Output::OutputFormat::INTERMEDIATE, ss);
    return ss.str();
}

} } } // namespace Test Ground Gringo

#endif // _GRINGO_GROUND_TEST_GROUNDER_HELPER_HH
