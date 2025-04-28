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

#include "clingo.hh"
#include <catch2/catch_test_macros.hpp>

namespace Clingo { namespace Test {

using S = std::string;
using ModelVec = std::vector<SymbolVector>;
using MessageVec = std::vector<std::pair<WarningCode, std::string>>;

struct MCB {
    MCB(ModelVec &models) : models(models) {
        models.clear();
    }
    bool operator()(Model const &m) {
        models.emplace_back();
        for (auto sym : m.symbols(ShowType::Shown)) {
            models.back().emplace_back(sym);
        }
        std::sort(models.back().begin(), models.back().end());
        return true;
    }
    ~MCB() {
        std::sort(models.begin(), models.end());
    }
    ModelVec &models;
};

template <class Handle>
inline SolveResult test_solve(Handle &&sh, ModelVec &models) {
    MCB cb(models);
    SolveResult ret;
    for (auto &m : sh) { cb(m); }
    return sh.get();
}

class LCB {
public:
    LCB(MessageVec &messages) : messages_(messages) { }
    void operator()(WarningCode code, char const *msg) { messages_.emplace_back(code, msg); }
private:
    MessageVec &messages_;
};

} } // namespace Test Clingo
