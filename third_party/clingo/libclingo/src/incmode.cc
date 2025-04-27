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

#include <clingo/incmode.hh>

namespace Gringo { namespace {

struct Incmode {
    Incmode(Control &ctl_) : ctl_(ctl_), istop(""), res(Gringo::SolveResult::Unknown, false, false) { }

    int get_max() {
        auto imax  = ctl_.getConst("imax");
        if (imax.type() == Gringo::SymbolType::Num) {
            return imax.num();
        }
        return std::numeric_limits<int>::max();
    }

    int get_min() {
        auto imin  = ctl_.getConst("imin");
        if (imin.type() == Gringo::SymbolType::Num) {
            return imin.num();
        }
        return 0;
    }

    String get_stop() {
        auto istop  = ctl_.getConst("istop");
        if (istop.type() == Gringo::SymbolType::Str) { return istop.string(); }
        else if (istop.type() == Gringo::SymbolType::Fun && istop.args().size == 0) { return istop.name(); }
        return "SAT";
    }

    bool check_run() {
        if (step >= imax) { return false; }
        if (step == 0 || step < imin) { return true; }
        auto sat = res.satisfiable();
        if (istop == "SAT" && sat == SolveResult::Satisfiable) { return false; }
        if (istop == "UNSAT" && sat == SolveResult::Unsatisfiable) { return false; }
        if (istop == "UNKNOWN" && sat == SolveResult::Unknown) { return false; }
        return true;
    }

    void assign_external_(Symbol sym, Potassco::Value_t val) {
        auto &dom = ctl_.getDomain();
        auto atm = dom.lookup(sym);
        if (!dom.eq(atm, dom.end())) { ctl_.assignExternal(dom.literal(atm), val); }
    }

    void run() {
        ctl_.add("check", {"t"}, "#external query(t).");
        imax = get_max();
        imin = get_min();
        istop = get_stop();

        while (check_run()) {
            Control::GroundVec parts;
            parts.reserve(2);
            parts.push_back({"check", {Symbol::createNum(step)}});
            if (step > 0) {
                assign_external_(Symbol::createFun("query", {Symbol::createNum(step - 1)}), Potassco::Value_t::Release);
                parts.push_back({"step", {Symbol::createNum(step)}});
            }
            else {
                parts.push_back({"base", {}});
            }
            ctl_.ground(parts, nullptr);
            assign_external_(Symbol::createFun("query", {Symbol::createNum(step)}), Potassco::Value_t::True);
            res = ctl_.solve({nullptr, 0}, 0)->get();
            step += 1;
        }
    }

    Control &ctl_;
    int imax = 0;
    int imin = 0;
    int step = 0;
    String istop;
    SolveResult res;
};

} // namespace

void incmode(Gringo::Control &ctl_) {
    Incmode m(ctl_);
    m.run();
}

} // namespace Gringo


