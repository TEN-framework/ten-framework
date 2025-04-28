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

#include <clingo/scripts.hh>
#include <gringo/logger.hh>

namespace Gringo {

bool Scripts::callable(String name) {
    for (auto &&script : scripts_) {
        if (std::get<1>(script) && std::get<2>(script)->callable(name)) {
            return true;
        }
    }
    return false;
}

void Scripts::main(Control &ctl) {
    for (auto &&script : scripts_) {
        if (std::get<1>(script) && std::get<2>(script)->callable("main")) {
            std::get<2>(script)->main(ctl);
            return;
        }
    }
}

SymVec Scripts::call(Location const &loc, String name, SymSpan args, Logger &log) {
    for (auto &&script : scripts_) {
        if (std::get<1>(script) && std::get<2>(script)->callable(name)) {
            return std::get<2>(script)->call(loc, name, args, log);
        }
    }
    GRINGO_REPORT(log, Warnings::OperationUndefined)
        << loc << ": info: operation undefined:\n"
        << "  function '" << name << "' not found\n"
        ;
    return {};
}

void Scripts::registerScript(String type, UScript script) {
    if (script) { scripts_.emplace_back(type, false, std::move(script)); }
}

void Scripts::exec(String type, Location loc, String code) {
    bool notfound = true;
    for (auto &&script : scripts_) {
        if (std::get<0>(script) == type) {
            std::get<1>(script) = true;
            std::get<2>(script)->exec(type, loc, code);
            notfound = false;
        }
    }
    if (notfound) {
        std::ostringstream oss;
        oss << loc << ": error: " << type << " support not available\n";
        throw GringoError(oss.str().c_str());
    }
}

char const *Scripts::version(String type) {
    for (auto &&script : scripts_) {
        if (std::get<0>(script) == type) {
            return std::get<2>(script)->version();
        }
    }
    return nullptr;
}

Scripts::~Scripts() = default;

Scripts &g_scripts() {
    static Scripts scripts;
    return scripts;
}

} // namespace Gringo
