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

#ifndef CLINGO_SCRIPTS_HH
#define CLINGO_SCRIPTS_HH

#include <clingo/control.hh>
#include <gringo/base.hh>

namespace Gringo {

class Script : public Context {
public:
    virtual void main(Control &ctl) = 0;
    virtual char const *version() = 0;
    ~Script() override = default;
};
using UScript = std::shared_ptr<Script>;
using UScriptVec = std::vector<std::tuple<String, bool, UScript>>;

class Scripts : public Context {
public:
    Scripts() = default;
    ~Scripts() override;

    bool callable(String name) override;
    SymVec call(Location const &loc, String name, SymSpan args, Logger &log) override;
    void main(Control &ctl);
    void registerScript(String type, UScript script);
    void exec(String type, Location loc, String code) override;
    char const *version(String type);

    template <class F>
    void withContext(Context *ctx, F f);
private:
    UScriptVec scripts_;
};

class ChainContext : public Context {
public:
    ChainContext(Context &a, Scripts &b)
    : a_{a}, b_{b} { }

    bool callable(String name) override {
        return a_.callable(name) || b_.callable(name);
    }

    SymVec call(Location const &loc, String name, SymSpan args, Logger &log) override {
        if (a_.callable(name)) {
            return a_.call(loc, name, args, log);
        }
        return b_.call(loc, name, args, log);
    }

    void exec(String type, Location loc, String code) override {
        b_.exec(type, loc, code);
    }

private:
    Context &a_;
    Scripts &b_;
};

Scripts &g_scripts();

template <class F>
inline void Scripts::withContext(Context *ctx, F f) {
    if (ctx == nullptr) {
        f(*this);
    }
    else {
        ChainContext cctx{*ctx, *this};
        f(cctx);
    }
}

} // namespace Gringo

#endif // CLINGO_SCRIPTS_HH

