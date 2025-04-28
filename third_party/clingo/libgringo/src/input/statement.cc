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
#include "gringo/input/statement.hh"
#include "gringo/input/literals.hh"
#include "gringo/input/aggregates.hh"
#include "gringo/ground/statements.hh"
#include "gringo/safetycheck.hh"

#include <algorithm>
#include <numeric>

namespace Gringo { namespace Input {

Statement::Statement(UHeadAggr &&head, UBodyAggrVec &&body)
: head_(std::move(head))
, body_(std::move(body)) { }

void Statement::initTheory(TheoryDefs &def, Logger &log) {
    head_->initTheory(def, !body_.empty(), log);
    for (auto &lit : body_) {
        lit->initTheory(def, log);
    }
}

void Statement::add(ULit &&lit) {
    Location loc(lit->loc());
    body_.emplace_back(make_locatable<SimpleBodyLiteral>(loc, std::move(lit)));
}

void Statement::print(std::ostream &out) const {
    head_->printWithCondition(out, body_);
}

Symbol Statement::isEDB() const {
    return body_.empty() ? head_->isEDB() : Symbol();
}

UStmVec Statement::unpool() {
    std::vector<UBodyAggrVec> bodies;
    Term::unpool(body_.begin(), body_.end(),
        [] (UBodyAggr &x) -> UBodyAggrVec {
            UBodyAggrVec body;
            x->unpool(body);
            return body;
        }, [&bodies](UBodyAggrVec &&x) { bodies.push_back(std::move(x)); });
    UHeadAggrVec heads;
    head_->unpool(heads);
    UStmVec x;
    for (auto &body : bodies) {
        for (auto &head : heads) {
            x.emplace_back(make_locatable<Statement>(loc(), get_clone(head), get_clone(body)));
        }
    }
    return x;
}

bool Statement::hasPool() const {
    return std::any_of(body_.begin(), body_.end(), [](auto const &lit) { return lit->hasPool(); }) ||
           head_->hasPool();
}

UStmVec Statement::unpoolComparison() {
    Term::replace(head_, head_->unpoolComparison(body_));
    // check if the body needs unpooling
    bool hasPool = false;
    for (auto &x : body_) {
        if (x->hasUnpoolComparison()) {
            hasPool = true;
            break;
        }
    }
    // compute the cross-product of the unpooled bodies
    UStmVec ret;
    if (hasPool) {
        UBodyAggrVecVec bodies;
        Term::unpool(
            body_.begin(), body_.end(),
            [](UBodyAggr const &lit) {
                return lit->unpoolComparison();
            }, [&] (std::vector<UBodyAggrVec> body) {
                bodies.emplace_back();
                for (auto &lits : body) {
                    bodies.back().insert(bodies.back().end(),
                                         std::make_move_iterator(lits.begin()),
                                         std::make_move_iterator(lits.end()));
                }
            });
        // compute cross-product of head and body
        for (auto &body : bodies) {
            ret.emplace_back(make_locatable<Statement>(loc(), get_clone(head_), std::move(body)));
        }
    }
    else {
        ret.emplace_back(make_locatable<Statement>(loc(), std::move(head_), std::move(body_)));
    }
    return ret;
}

void Statement::assignLevels(VarTermBoundVec &bound) {
    AssignLevel c;
    head_->assignLevels(c);
    for (auto &y : body_) {
        y->assignLevels(c);
    }
    c.add(bound);
    c.assignLevels();
}

bool Statement::simplify(Projections &project, Logger &log) {
    SimplifyState state;
    if (!head_->simplify(project, state, log)) {
        return false;
    }
    bool singleton = std::accumulate(body_.begin(),
                                     body_.end(),
                                     0,
                                     [](unsigned x, UBodyAggr const &y) {
                                         return x + y->projectScore();
                                     }) == 1 && head_->isPredicate();
    for (auto &y : body_) {
        if (!y->simplify(project, state, singleton, log)) {
            return false;
        }
    }
    for (auto &y : state.dots()) {
        body_.emplace_back(gringo_make_unique<SimpleBodyLiteral>(RangeLiteral::make(y)));
    }
    for (auto &y : state.scripts()) {
        body_.emplace_back(gringo_make_unique<SimpleBodyLiteral>(ScriptLiteral::make(y)));
    }
    return true;
}

namespace {

void _rewriteAggregates(UBodyAggrVec &body) {
    UBodyAggrVec aggr;
    auto jt = body.begin();
    for (auto it = jt, ie = body.end(); it != ie; ++it) {
        if ((*it)->rewriteAggregates(aggr)) {
            if (it != jt) {
                *jt = std::move(*it);
            }
            ++jt;
        }
    }
    body.erase(jt, body.end());
    std::move(aggr.begin(), aggr.end(), std::back_inserter(body));
}

// TODO: this code has to be adjusted
//       the toGround code then has to take an additional argument
//       to only create aggregate literals up to an index
//       EXAMPLE:
//         h :- B, { c1 }, { c2 }.
//         a1 :- B*, c1.       % accumulation rule
//         a2 :- B*, a1*, c2.  % accumulation rule
//         % TODO: add complete rules...
//         h :- B, a1, a2.
//

void _rewriteAssignments(UBodyAggrVec &body) {
    using LitDep = SafetyChecker<VarTerm*, UBodyAggr>;
    using VarMap = std::unordered_map<String, LitDep::VarNode*>;
    LitDep dep;
    VarMap map;
    for (auto &y : body) {
        auto &ent = dep.insertEnt(std::move(y));
        VarTermBoundVec vars;
        ent.data->collect(vars);
        for (auto &occ : vars) {
            if (occ.first->level == 0) {
                auto &var(map[occ.first->name]);
                if (var == nullptr) {
                    var = &dep.insertVar(occ.first);
                }
                if (occ.second) { dep.insertEdge(ent, *var); }
                else            { dep.insertEdge(*var, ent); }
            }
        }
    }
    LitDep::VarVec bound;
    LitDep::EntVec open;
    open.reserve(body.size()); // Note: keeps iterators valid
    body.clear();
    dep.init(open);
    UBodyAggrVec sorted;
    for (std::size_t it = 0; it != open.size(); ) {
        LitDep::EntVec assign;
        for (; it != open.size(); ++it) {
            if (!(open[it])->data->isAssignment()) {
                dep.propagate(open[it], open, &bound);
                sorted.emplace_back(std::move((open[it])->data));
            }
            else { assign.emplace_back(open[it]); }
        }
        LitDep::EntVec nextAssign;
        while (!assign.empty()) {
            for (auto &x : assign) {
                bool allBound = true;
                for (auto &y : x->provides) {
                    if (!y->bound) {
                        allBound = false;
                        break;
                    }
                }
                if (!allBound) {
                    nextAssign.emplace_back(x);
                }
                else {
                    x->data->removeAssignment();
                    dep.propagate(x, open, &bound);
                    sorted.emplace_back(std::move(x->data));
                }
            }
            if (!nextAssign.empty()) {
                dep.propagate(nextAssign.back(), open, &bound);
                sorted.emplace_back(std::move(nextAssign.back()->data));
                nextAssign.pop_back();
            }
            std::swap(assign, nextAssign);
            nextAssign.clear();
        }
    }
    for (auto &x : dep.entNodes_) {
        if (x.depends > 0) {
            sorted.emplace_back(std::move(x.data));\
        }
    }
    body = std::move(sorted);
}

} // namespace

void Statement::gatherIEs(IESolver &solver) const {
    head_->addToSolver(solver);
    for (auto const &lit : body_) {
        lit->addToSolver(solver);
    }
}

void Statement::addIEBound(VarTerm const &var, IEBound const &bound) {
    body_.emplace_back(gringo_make_unique<SimpleBodyLiteral>(RangeLiteral::make(var, bound)));
}

void Statement::rewrite() {
    AuxGen auxGen;
    { // rewrite aggregates
        Term::replace(head_, head_->rewriteAggregates(body_));
        _rewriteAggregates(body_);
    }
    { // rewrite arithmetics
        Term::ArithmeticsMap arith;
        Literal::RelationVec assign;
        arith.emplace_back(gringo_make_unique<Term::LevelMap>());
        head_->rewriteArithmetics(arith, auxGen);
        for (auto &y : body_) {
            y->rewriteArithmetics(arith, assign, auxGen);
        }
        for (auto &y : *arith.back()) {
            body_.emplace_back(gringo_make_unique<SimpleBodyLiteral>(RelationLiteral::make(y)));
        }
        for (auto &y : assign) {
            body_.emplace_back(gringo_make_unique<SimpleBodyLiteral>(RelationLiteral::make(y)));
        }
        arith.pop_back();
    }
    {
        // TODO: add an option
        IESolver{*this}.compute();
    }
    _rewriteAssignments(body_);
}

void Statement::check(Logger &log) const {
    ChkLvlVec levels;
    levels.emplace_back(loc(), *this);
    head_->check(levels, log);
    for (auto const &y : body_) {
        y->check(levels, log);
    }
    levels.back().check(log);
}

void Statement::replace(Defines &x) {
    head_->replace(x);
    for (auto &y : body_) {
        y->replace(x);
    }
}

namespace {

void toGround(CreateHead &&head, UBodyAggrVec const &body, ToGroundArg &x, Ground::UStmVec &stms) {
    CreateBodyVec createVec;
    for (auto const &y : body) {
        createVec.emplace_back(y->toGround(x, stms));
    }
    Ground::ULitVec lits;
    for (auto current = createVec.begin(), end = createVec.end(); current != end; ++current) {
        current->first(lits, false);
        for (auto &z : current->second) {
            Ground::ULitVec splitLits;
            for (auto it = createVec.begin(); it < current; ++it) {
                it->first(splitLits, true);
            }
            stms.emplace_back(z(std::move(splitLits)));
        }
    }
    stms.emplace_back(head(std::move(lits)));
}

} // namespace

void Statement::toGround(ToGroundArg &x, Ground::UStmVec &stms) const {
    Gringo::Input::toGround(head_->toGround(x, stms), body_, x, stms);
}

} } // namespace Input Gringo
