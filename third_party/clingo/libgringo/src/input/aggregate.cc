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

#include "gringo/bug.hh"
#include "gringo/logger.hh"
#include "gringo/input/aggregate.hh"

namespace Gringo { namespace Input {

// {{{ definition of AssignLevel

void AssignLevel::add(VarTermBoundVec &vars) {
    for (auto &occ : vars) {
        occurr_[occ.first->ref].emplace_back(occ.first);
    }
}

AssignLevel &AssignLevel::subLevel() {
    children_.emplace_front();
    return children_.front();
}

void AssignLevel::assignLevels() {
    BoundSet bound;
    assignLevels(0, bound);
}

void AssignLevel::assignLevels(unsigned level, BoundSet const &bound) {
    BoundSet children(bound);
    for (auto &occs : occurr_) {
        unsigned l = children.emplace(occs.first, level).first->second;
        for (auto &occ : occs.second) { occ->level = l; }
    }
    for (auto &child : children_) {
        child.assignLevels(level + 1, children);
    }
}

// }}}
// {{{ definition of CheckLevel

bool CheckLevel::Ent::operator<(Ent const &x) const {
    return false;
}

CheckLevel::CheckLevel(Location const &loc, Printable const &p)
: loc(loc)
, p(p) { }

#if defined(_MSC_VER) && _MSCVER < 1920
CheckLevel::CheckLevel(CheckLevel &&other) noexcept
: loc{other.loc}
, p{other.p}
, dep{std::move(other.dep)}
, current{other.current}
, vars{std::move(other.vars)} { }
#else
CheckLevel::CheckLevel(CheckLevel &&other) noexcept = default;
#endif

CheckLevel::SC::VarNode &CheckLevel::var(VarTerm &var) {
    auto &node = vars[var.name];
    if (node == nullptr) {
        node = &dep.insertVar(&var);
    }
    return *node;
}

void CheckLevel::check(Logger &log) {
    dep.order();
    auto vars(dep.open());
    if (!vars.empty()) {
        auto cmp = [](SC::VarNode const *x, SC::VarNode const *y) -> bool{
            if (x->data->name != y->data->name) { return x->data->name < y->data->name; }
            return x->data->loc() < y->data->loc();
        };
        std::sort(vars.begin(), vars.end(), cmp);
        std::ostringstream msg;
        msg << loc << ": error: unsafe variables in:\n  " << p << "\n";
        for (auto &x : vars) { msg << x->data->loc() << ": note: '" << x->data->name << "' is unsafe\n"; }
        GRINGO_REPORT(log, Warnings::RuntimeError) << msg.str();
    }
}

void addVars(ChkLvlVec &levels, VarTermBoundVec &vars) {
    for (auto &x: vars) {
        auto &lvl(levels[x.first->level]);
        bool bind = x.second && levels.size() == x.first->level + 1;
        if (bind) { lvl.dep.insertEdge(*lvl.current, lvl.var(*x.first)); }
        else      { lvl.dep.insertEdge(lvl.var(*x.first), *lvl.current); }
    }
}

// }}}
// {{{ declaration of ToGroundArg

ToGroundArg::ToGroundArg(unsigned &auxNames, DomainData &domains)
: auxNames(auxNames)
, domains(domains) { }

String ToGroundArg::newId(bool increment) {
    unsigned inc = increment ? 1 : 0;
    auxNames += inc;
    return ("#d" + std::to_string(auxNames - inc)).c_str();
}

UTermVec ToGroundArg::getGlobal(VarTermBoundVec const &vars) { // NOLINT(readability-convert-member-functions-to-static)
    std::unordered_set<String> seen;
    UTermVec global;
    for (auto const &occ : vars) {
        if (occ.first->level == 0 && seen.emplace(occ.first->name).second) {
            global.emplace_back(occ.first->clone());
        }
    }
    return global;
}

UTermVec ToGroundArg::getLocal(VarTermBoundVec const &vars) { // NOLINT(readability-convert-member-functions-to-static)
    std::unordered_set<String> seen;
    UTermVec local;
    for (auto const &occ : vars) {
        if (occ.first->level != 0 && seen.emplace(occ.first->name).second) {
            local.emplace_back(occ.first->clone());
        }
    }
    return local;
}

UTerm ToGroundArg::newId(UTermVec &&global, Location const &loc, bool increment) {
    if (!global.empty()) {
        return make_locatable<FunctionTerm>(loc, newId(increment), std::move(global));
    }
    return make_locatable<ValTerm>(loc, Symbol::createId(newId(increment)));
}

// }}}

// {{{ definition of BodyAggregate

void BodyAggregate::addToSolver(IESolver &solver) {
    static_cast<void>(solver);
}

// }}}
// {{{ definition of HeadAggregate

void HeadAggregate::addToSolver(IESolver &solver) {
    static_cast<void>(solver);
}

void HeadAggregate::initTheory(TheoryDefs &def, bool hasBody, Logger &log) {
    static_cast<void>(def);
    static_cast<void>(hasBody);
    static_cast<void>(log);
}

Symbol HeadAggregate::isEDB() const {
    return {};
}

void HeadAggregate::printWithCondition(std::ostream &out, UBodyAggrVec const &condition) const {
    out << *this;
    if (!condition.empty()) {
        out << ":-";
        auto f = [](std::ostream &out, UBodyAggr const &x) { out << *x; };
        print_comma(out, condition, ";", f);
    }
    out << ".";
}

// }}}

std::vector<ULitVec> unpoolComparison_(ULitVec const &cond) {
    std::vector<ULitVec> conds;
    // compute the cross-product of the unpooled conditions
    Term::unpool(
        cond.begin(), cond.end(),
        [](ULit const &lit) {
            return lit->unpoolComparison();
        }, [&] (std::vector<ULitVec> cond) {
            conds.emplace_back();
            for (auto &lits : cond) {
                conds.back().insert(conds.back().end(),
                                    std::make_move_iterator(lits.begin()),
                                    std::make_move_iterator(lits.end()));
            }
        });
    return conds;
}

} } // namespace Input Gringo
