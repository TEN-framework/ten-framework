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
#include "gringo/input/aggregates.hh"
#include "gringo/input/literals.hh"
#include "gringo/ground/statements.hh"
#include "gringo/ground/literals.hh"
#include "gringo/logger.hh"
#include "gringo/term.hh"
#include "gringo/utility.hh"
#include <algorithm>
#include <cmath>
#include <ostream>
#include <tuple>
#ifdef _MSC_VER
#pragma warning (disable : 4503) // decorated name length exceeded
#endif
namespace Gringo { namespace Input {

namespace {

// {{{ definition of auxiliary functions

template <class T, class U, class V>
void printAggr_(std::ostream &out, AggregateFunction fun, T const &x, U const &y, V const &f) {
    auto it = std::begin(x);
    auto ie = std::end(x);
    if (it != ie) {
        it->bound->print(out);
        out << inv(it->rel);
        ++it;
    }
    out << fun << "{";
    print_comma(out, y, ";", f);
    out << "}";
    for (; it != ie; ++it) {
        out << it->rel;
        it->bound->print(out);
    }
}

struct Printer {
    template <class T>
    void operator()(std::ostream &out, std::unique_ptr<T> const &prt) const {
        out << *prt;
    };
    template <class T>
    void operator()(std::ostream &out, T const &prt) const {
        out << prt;
    };
};

void printCond_(std::ostream &out, CondLit const &x) {
    x.first->print(out);
    out << ":";
    print_comma(out, x.second, ",", Printer{});
};

std::function<ULitVec(ULit const &)> _unpool_lit(bool head) {
    return [head](ULit const &x) {
        return x->unpool(head);
    };
}

BoundVec _unpool_bound(Bound &x) {
    return x.unpool();
};

void _add(ChkLvlVec &levels, ULit const &lit, bool bind) {
    VarTermBoundVec vars;
    levels.back().current = &levels.back().dep.insertEnt();
    lit->collect(vars, bind);
    addVars(levels, vars);
}

void _add(ChkLvlVec &levels, UTermVec const &terms) {
    VarTermBoundVec vars;
    levels.back().current = &levels.back().dep.insertEnt();
    for (auto const &x : terms)  {
        x->collect(vars, false);
    }
    addVars(levels, vars);
}

void _add(ChkLvlVec &levels, ULitVec const &cond) {
    for (auto const &x : cond) {
        _add(levels, x, true);
    }
}

template <class T>
void _aggr(ChkLvlVec &levels, BoundVec const &bounds, T const &f, bool bind) {
    bool assign = false;
    CheckLevel::SC::EntNode *depend = nullptr;
    for (auto const &y : bounds) {
        if (bind && y.rel == Relation::EQ) {
            levels.back().current = &levels.back().dep.insertEnt();
            VarTermBoundVec vars;
            y.bound->collect(vars, true);
            addVars(levels, vars);
            f();
            assign = true;
        }
        else {
            if (depend == nullptr) {
                depend = &levels.back().dep.insertEnt();
            }
            levels.back().current = depend;
            VarTermBoundVec vars;
            y.bound->collect(vars, false);
            addVars(levels, vars);
        }
    }
    if (depend == nullptr && !assign) {
        depend = &levels.back().dep.insertEnt();
    }
    if (depend) {
        levels.back().current = depend;
        f();
    }
}

void warnGlobal(VarTermBoundVec &vars, bool warn, Logger &log) {
    if (warn) {
        auto ib = vars.begin();
        auto ie = vars.end();
        ie = std::remove_if(ib, ie, [](auto const &a) { return a.first->level > 0; });
        std::sort(ib, ie, [](auto const &a, auto const &b) { return a.first->name < b.first->name; });
        ie = std::unique(ib, ie, [](auto const &a, auto const &b) { return a.first->name == b.first->name; });
        for (auto it = ib; it != ie; ++it) {
            GRINGO_REPORT(log, Warnings::GlobalVariable)
                << it->first->loc() << ": info: global variable in tuple of aggregate element:\n"
                << "  " << it->first->name << "\n"
                ;
        }
    }
}

// }}}1

} // namespace

// {{{1 definition of AggregateElement

bool BodyAggrElem::hasPool() const {
    for (auto const &term : tuple_) {
        if (term->hasPool()) {
            return true;
        }
    }
    for (auto const &lit : condition_) {
        if (lit->hasPool(false)) {
            return true;
        }
    }
    return false;
}

void BodyAggrElem::unpool(BodyAggrElemVec &pool) {
    auto unpoolCond = [&](UTermVec &&tuple) {
        auto append = [&](ULitVec &&cond) {
            pool.emplace_back(get_clone(tuple), std::move(cond));
        };
        Term::unpool(condition_.begin(), condition_.end(), _unpool_lit(false), append);
    };
    Term::unpool(tuple_.begin(), tuple_.end(), Gringo::unpool, unpoolCond);
}

bool BodyAggrElem::hasUnpoolComparison() const {
    return std::any_of(condition_.begin(),
                       condition_.end(),
                       [](auto const &lit) { return lit->hasUnpoolComparison(); });
}

void BodyAggrElem::unpoolComparison(BodyAggrElemVec &elems) const {
    for (auto &cond : unpoolComparison_(condition_)) {
        elems.emplace_back(get_clone(tuple_), std::move(cond));
    }
}

void BodyAggrElem::collect(VarTermBoundVec &vars, bool tupleOnly) const {
    for (auto const &term : tuple_) {
        term->collect(vars, false);
    }
    if (!tupleOnly) {
        for (auto const &lit : condition_) {
            lit->collect(vars, false);
        }
    }
}

void BodyAggrElem::gatherIEs(IESolver &solver) const {
    for (auto const &lit : condition_) {
        lit->addToSolver(solver, false);
    }
}

void BodyAggrElem::addIEBound(VarTerm const &var, IEBound const &bound) {
    condition_.emplace_back(RangeLiteral::make(var, bound));
}

bool BodyAggrElem::simplify(Projections &project, SimplifyState &state, Logger &log) {
    for (auto &term : tuple_) {
        if (term->simplify(state, false, false, log).update(term, false).undefined()) {
            return false;
        }
    }
    for (auto &lit : condition_) {
        // NOTE: projection disabled with singelton=true
        if (!lit->simplify(log, project, state, true, true)) {
            return false;
        }
    }
    for (auto &dot : state.dots()) {
        condition_.emplace_back(RangeLiteral::make(dot));
    }
    for (auto &script : state.scripts()) {
        condition_.emplace_back(ScriptLiteral::make(script));
    }
    return true;
}

void BodyAggrElem::rewriteArithmetics(Term::ArithmeticsMap &arith, Literal::RelationVec &assign, AuxGen &auxGen) {
    for (auto &y : condition_) {
        y->rewriteArithmetics(arith, assign, auxGen);
    }
    for (auto &y : *arith.back()) {
        condition_.emplace_back(RelationLiteral::make(y));
    }
    for (auto &y : assign) {
        condition_.emplace_back(RelationLiteral::make(y));
    }
}

void BodyAggrElem::check(ChkLvlVec &levels) const {
    _add(levels, tuple_);
    _add(levels, condition_);
}

void BodyAggrElem::replace(Defines &defs) {
    for (auto &y : tuple_) {
        Term::replace(y, y->replace(defs, true));
    }
    for (auto &y : condition_) {
        y->replace(defs);
    }
}

template <class T, class C>
std::unique_ptr<T> BodyAggrElem::toGround(ToGroundArg &x, C &completeRef, Ground::ULitVec &&lits) const {
    for (auto const &z : condition_) {
        lits.emplace_back(z->toGround(x.domains, false));
    }
    auto ret = gringo_make_unique<T>(completeRef, get_clone(tuple_), std::move(lits));
    completeRef.addAccuDom(*ret);
    return ret;
}

std::ostream &operator<<(std::ostream &out, BodyAggrElem const &elem) {
    print_comma(out, elem.tuple_, ",", Printer{});
    out << ":";
    print_comma(out, elem.condition_, ",", Printer{});
    return out;
}

bool operator==(BodyAggrElem const &a, BodyAggrElem const &b) {
    return is_value_equal_to(a.tuple_, b.tuple_) && is_value_equal_to(a.condition_, b.condition_);
}

size_t get_value_hash(BodyAggrElem const &elem) {
    return get_value_hash(typeid(BodyAggrElem).hash_code(), elem.tuple_, elem.condition_);
}

BodyAggrElem get_clone(BodyAggrElem const &elem) {
    return {get_clone(elem.tuple_), get_clone(elem.condition_)};
}

// {{{1 definition of TupleBodyAggregate

TupleBodyAggregate::TupleBodyAggregate(NAF naf, AggregateFunction fun, BoundVec &&bounds, BodyAggrElemVec &&elems)
: TupleBodyAggregate(naf, false, false, fun, std::move(bounds), std::move(elems)) { }

TupleBodyAggregate::TupleBodyAggregate(NAF naf, bool removedAssignment, bool translated, AggregateFunction fun, BoundVec &&bounds, BodyAggrElemVec &&elems)
: naf_(naf)
, removedAssignment_(removedAssignment)
, translated_(translated)
, fun_(fun)
, bounds_(std::move(bounds))
, elems_(std::move(elems)) { }

void TupleBodyAggregate::print(std::ostream &out) const {
    out << naf_;
    printAggr_(out, fun_, bounds_, elems_, Printer{});
}

size_t TupleBodyAggregate::hash() const {
    return get_value_hash(typeid(TupleBodyAggregate).hash_code(), size_t(naf_), size_t(fun_), bounds_, elems_);
}

bool TupleBodyAggregate::operator==(BodyAggregate const &other) const {
    const auto *t = dynamic_cast<TupleBodyAggregate const *>(&other);
    return t != nullptr &&
           naf_ == t->naf_ &&
           fun_ == t->fun_ &&
           is_value_equal_to(bounds_, t->bounds_) &&
           is_value_equal_to(elems_, t->elems_);
}

TupleBodyAggregate *TupleBodyAggregate::clone() const {
    return make_locatable<TupleBodyAggregate>(loc(), naf_, removedAssignment_, translated_, fun_, get_clone(bounds_), get_clone(elems_)).release();
}

bool TupleBodyAggregate::hasPool() const {
    return std::any_of(bounds_.begin(),
                       bounds_.end(),
                       [](auto const &bound) { return bound.bound->hasPool(); }) ||
           std::any_of(elems_.begin(),
                       elems_.end(),
                       [](auto const &elem) { return elem.hasPool(); });
}

void TupleBodyAggregate::unpool(UBodyAggrVec &x) {
    BodyAggrElemVec elems;
    for (auto &elem : elems_) {
        elem.unpool(elems);
    }
    auto f = [&](BoundVec &&y) { x.emplace_back(make_locatable<TupleBodyAggregate>(loc(), naf_, removedAssignment_, translated_, fun_, std::move(y), get_clone(elems))); };
    Term::unpool(bounds_.begin(), bounds_.end(), _unpool_bound, f);
}

bool TupleBodyAggregate::hasUnpoolComparison() const {
    return std::any_of(elems_.begin(),
                       elems_.end(),
                       [](auto const &elem) { return elem.hasUnpoolComparison(); });
}

UBodyAggrVecVec TupleBodyAggregate::unpoolComparison() const {
    BodyAggrElemVec elems;
    for (auto const &elem : elems_) {
        elem.unpoolComparison(elems);
    }
    UBodyAggrVecVec ret;
    ret.emplace_back();
    ret.back().emplace_back(make_locatable<TupleBodyAggregate>(loc(),
                                                               naf_,
                                                               removedAssignment_,
                                                               translated_,
                                                               fun_,
                                                               get_clone(bounds_),
                                                               std::move(elems)));
    return ret;
}

void TupleBodyAggregate::collect(VarTermBoundVec &vars) const {
    for (auto const &bound : bounds_) {
        bound.bound->collect(vars, bound.rel == Relation::EQ && naf_ == NAF::POS);
    }
    for (auto const &elem : elems_) {
        elem.collect(vars);
    }
}

void TupleBodyAggregate::addToSolver(IESolver &solver) {
    for (auto &elem: elems_) {
        solver.add(elem);
    }
}

bool TupleBodyAggregate::rewriteAggregates(UBodyAggrVec &aggr) {
    BoundVec assign;
    auto jt(bounds_.begin());
    for (auto it = jt, ie = bounds_.end(); it != ie; ++it) {
        if (it->rel == Relation::EQ && naf_ == NAF::POS) {
            assign.emplace_back(std::move(*it));
        }
        else {
            if (it != jt) {
                *jt = std::move(*it);
            }
            ++jt;
        }
    }
    bounds_.erase(jt, bounds_.end());
    bool skip = bounds_.empty() && !assign.empty();
    for (auto it = assign.begin(), ie = assign.end() - (skip ? 1 : 0); it != ie; ++it) {
        BoundVec bound;
        bound.emplace_back(std::move(*it));
        aggr.emplace_back(make_locatable<TupleBodyAggregate>(loc(), naf_, removedAssignment_, translated_, fun_, std::move(bound), get_clone(elems_)));
    }
    if (skip) {
        bounds_.emplace_back(std::move(assign.back()));
    }
    if (bounds_.empty() && naf_ == NAF::NOT) {
        aggr.emplace_back(gringo_make_unique<SimpleBodyLiteral>(make_locatable<RelationLiteral>(
            loc(), Relation::NEQ,
            make_locatable<ValTerm>(loc(), Symbol::createNum(0)),
            make_locatable<ValTerm>(loc(), Symbol::createNum(0)))));
    }
    return !bounds_.empty();
}

bool TupleBodyAggregate::simplify(Projections &project, SimplifyState &state, bool singleton, Logger &log) {
    static_cast<void>(singleton);
    for (auto &bound : bounds_) {
        if (!bound.simplify(state, log)) {
            return false;
        }
    }
    elems_.erase(std::remove_if(elems_.begin(), elems_.end(), [&](BodyAggrElemVec::value_type &elem) {
        auto elemState = SimplifyState::make_substate(state);
        return !elem.simplify(project, elemState, log);
    }), elems_.end());
    return true;
}

void TupleBodyAggregate::rewriteArithmetics(Term::ArithmeticsMap &arith, Literal::RelationVec &assign, AuxGen &auxGen) {
    static_cast<void>(assign);
    for (auto &bound : bounds_) {
        bound.rewriteArithmetics(arith, auxGen);
    }
    for (auto &elem : elems_) {
        Literal::RelationVec assign;
        arith.emplace_back(gringo_make_unique<Term::LevelMap>());
        elem.rewriteArithmetics(arith, assign, auxGen);
        arith.pop_back();
    }
}

void TupleBodyAggregate::assignLevels(AssignLevel &lvl) {
    VarTermBoundVec vars;
    for (auto &bound : bounds_) {
        bound.bound->collect(vars, false);
    }
    lvl.add(vars);
    for (auto &elem : elems_) {
        AssignLevel &local(lvl.subLevel());
        VarTermBoundVec vars;
        elem.collect(vars);
        local.add(vars);
    }
}

void TupleBodyAggregate::check(ChkLvlVec &levels, Logger &log) const {
    auto f = [&]() {
        VarTermBoundVec vars;
        for (auto const &elem : elems_) {
            levels.emplace_back(loc(), *this);
            elem.check(levels);
            levels.back().check(log);
            levels.pop_back();
            elem.collect(vars, true);
        }
        warnGlobal(vars, !translated_, log);
    };
    return _aggr(levels, bounds_, f, naf_ == NAF::POS);
}

void TupleBodyAggregate::replace(Defines &x) {
    for (auto &bound : bounds_) {
        Term::replace(bound.bound, bound.bound->replace(x, true));
    }
    for (auto &elem : elems_) {
        elem.replace(x);
    }
}

CreateBody TupleBodyAggregate::toGround(ToGroundArg &x, Ground::UStmVec &stms) const {
    if (!isAssignment()) {
        stms.emplace_back(gringo_make_unique<Ground::BodyAggregateComplete>(x.domains, x.newId(*this), fun_, get_clone(bounds_)));
        auto &completeRef = static_cast<Ground::BodyAggregateComplete&>(*stms.back()); // NOLINT
        CreateStmVec split;
        split.emplace_back([&completeRef, this](Ground::ULitVec &&lits) -> Ground::UStm {
            UTermVec tuple;
            tuple.emplace_back(make_locatable<ValTerm>(loc(), Symbol()));
            // Note: should this become a function?
            UTerm neutral;
            switch (fun_) {
                case AggregateFunction::MIN: { neutral = make_locatable<ValTerm>(loc(), Symbol::createSup());  break; }
                case AggregateFunction::MAX: { neutral = make_locatable<ValTerm>(loc(), Symbol::createInf());  break; }
                default:                     { neutral = make_locatable<ValTerm>(loc(), Symbol::createNum(0)); break; }
            }
            for (auto const &y : bounds_) {
                lits.emplace_back(gringo_make_unique<Ground::RelationLiteral>(y.rel, get_clone(neutral), get_clone(y.bound)));
            }
            auto ret = gringo_make_unique<Ground::BodyAggregateAccumulate>(completeRef, get_clone(tuple), std::move(lits));
            completeRef.addAccuDom(*ret);
            return std::move(ret);
        });
        for (auto const &elem : elems_) {
            split.emplace_back([&completeRef, &elem, &x](Ground::ULitVec &&lits) -> Ground::UStm {
                return elem.toGround<Ground::BodyAggregateAccumulate>(x, completeRef, std::move(lits));
            });
        }
        return {[&completeRef, this](Ground::ULitVec &lits, bool auxiliary) {
            lits.emplace_back(gringo_make_unique<Ground::BodyAggregateLiteral>(completeRef, naf_, auxiliary));
        }, std::move(split)};
    }
    assert(bounds_.size() == 1 && naf_ == NAF::POS);
    VarTermBoundVec vars;
    for (auto const &elem : elems_) {
        elem.collect(vars);
    }
    UTermVec global(x.getGlobal(vars));
    global.emplace_back(get_clone(bounds_.front().bound));
    UTermVec globalSpecial(x.getGlobal(vars));
    UTerm repr(x.newId(std::move(global), loc(), false));
    UTerm dataRepr(x.newId(std::move(globalSpecial), loc()));

    stms.emplace_back(gringo_make_unique<Ground::AssignmentAggregateComplete>(x.domains, get_clone(repr), get_clone(dataRepr), fun_));
    auto &completeRef = static_cast<Ground::AssignmentAggregateComplete&>(*stms.back()); // NOLINT
    // NOTE: for assignment aggregates this does not make much sense
    //       the empty aggregate always matches and hence the elements
    //       should be grounded without auxLits
    CreateStmVec split;
    split.emplace_back([&completeRef, this](Ground::ULitVec &&lits) -> Ground::UStm {
        UTermVec tuple;
        tuple.emplace_back(make_locatable<ValTerm>(loc(), Symbol()));
        auto ret = gringo_make_unique<Ground::AssignmentAggregateAccumulate>(completeRef, get_clone(tuple), std::move(lits));
        completeRef.addAccuDom(*ret);
        return std::move(ret);
    });
    for (auto const &elem : elems_) {
        split.emplace_back([&completeRef, &elem, &x](Ground::ULitVec &&lits) -> Ground::UStm {
            return elem.toGround<Ground::AssignmentAggregateAccumulate>(x, completeRef, std::move(lits));
        });
    }
    return {[&completeRef](Ground::ULitVec &lits, bool auxiliary) {
        lits.emplace_back(gringo_make_unique<Ground::AssignmentAggregateLiteral>(completeRef, auxiliary));
    }, std::move(split)};
}

bool TupleBodyAggregate::isAssignment() const {
    return !removedAssignment_ &&
           bounds_.size() == 1 &&
           naf_ == NAF::POS && bounds_.front().rel == Relation::EQ &&
           bounds_.front().bound->getInvertibility() == Term::INVERTIBLE;
}

void TupleBodyAggregate::removeAssignment() {
    assert (isAssignment());
    removedAssignment_ = true;
}

// {{{1 definition of LitBodyAggregate

LitBodyAggregate::LitBodyAggregate(NAF naf, AggregateFunction fun, BoundVec &&bounds, CondLitVec &&elems)
: naf_(naf)
, fun_(fun)
, bounds_(std::move(bounds))
, elems_(std::move(elems)) { }

void LitBodyAggregate::print(std::ostream &out) const {
    out << naf_;
    printAggr_(out, fun_, bounds_, elems_, printCond_);
}

size_t LitBodyAggregate::hash() const {
    return get_value_hash(typeid(LitBodyAggregate).hash_code(), size_t(naf_), size_t(fun_), bounds_, elems_);
}

bool LitBodyAggregate::operator==(BodyAggregate const &other) const {
    const auto *t = dynamic_cast<LitBodyAggregate const *>(&other);
    return t != nullptr &&
           naf_ == t->naf_ &&
           fun_ == t->fun_ &&
           is_value_equal_to(bounds_, t->bounds_) &&
           is_value_equal_to(elems_, t->elems_);
}

LitBodyAggregate *LitBodyAggregate::clone() const {
    return make_locatable<LitBodyAggregate>(loc(), naf_, fun_, get_clone(bounds_), get_clone(elems_)).release();
}

void LitBodyAggregate::unpool(UBodyAggrVec &x) {
    CondLitVec e;
    for (auto &elem : elems_) {
        auto f = [&](ULit &&y) { e.emplace_back(std::move(y), get_clone(elem.second)); };
        Term::unpool(elem.first, _unpool_lit(true), f);
    }
    elems_ = std::move(e);
    e.clear();
    for (auto &elem : elems_) {
        auto f = [&](ULitVec &&y) { e.emplace_back(get_clone(elem.first), std::move(y)); };
        Term::unpool(elem.second.begin(), elem.second.end(), _unpool_lit(false), f);
    }
    auto f = [&](BoundVec &&y) { x.emplace_back(make_locatable<LitBodyAggregate>(loc(), naf_, fun_, std::move(y), get_clone(e))); };
    Term::unpool(bounds_.begin(), bounds_.end(), _unpool_bound, f);
}

bool LitBodyAggregate::hasUnpoolComparison() const {
    return true;
}

UBodyAggrVecVec LitBodyAggregate::unpoolComparison() const {
    int id = 0;
    BodyAggrElemVec elems;
    for (auto const &elem : elems_) {
        UTermVec tuple;
        elem.first->toTuple(tuple, id);
        ULitVec lits(get_clone(elem.second));
        if (!elem.first->triviallyTrue()) {
            lits.emplace_back(get_clone(elem.first));
        }
        elems.emplace_back(std::move(tuple), std::move(lits));
    }
    UBodyAggr ret(make_locatable<TupleBodyAggregate>(loc(), naf_, false, true, fun_, get_clone(bounds_), std::move(elems)));
    return ret->unpoolComparison();
}

void LitBodyAggregate::collect(VarTermBoundVec &vars) const {
    for (auto const &bound : bounds_) {
        bound.bound->collect(vars, bound.rel == Relation::EQ && naf_ == NAF::POS);
    }
    for (auto const &elem : elems_) {
        elem.first->collect(vars, false);
        for (auto const &lit : elem.second) {
            lit->collect(vars, false);
        }
    }
}

bool LitBodyAggregate::rewriteAggregates(UBodyAggrVec &aggr) {
    static_cast<void>(aggr);
    throw  std::runtime_error("LitHeadAggregate::rewriteAggregate: must not be called");
}

bool LitBodyAggregate::simplify(Projections &project, SimplifyState &state, bool singleton, Logger &log) {
    static_cast<void>(singleton);
    for (auto &bound : bounds_) {
        if (!bound.simplify(state, log)) {
            return false;
        }
    }
    elems_.erase(std::remove_if(elems_.begin(), elems_.end(), [&](CondLitVec::value_type &elem) {
        auto elemState = SimplifyState::make_substate(state);
        if (!std::get<0>(elem)->simplify(log, project, elemState, true, true)) {
            return true;
        }
        for (auto &lit : std::get<1>(elem)) {
            // NOTE: projection disabled with singelton=true
            if (!lit->simplify(log, project, elemState, true, true)) {
                return true;
            }
        }
        for (auto &dot : elemState.dots()) {
            std::get<1>(elem).emplace_back(RangeLiteral::make(dot));
        }
        for (auto &script : elemState.scripts()) {
            std::get<1>(elem).emplace_back(ScriptLiteral::make(script));
        }
        return false;
    }), elems_.end());
    return true;
}

void LitBodyAggregate::rewriteArithmetics(Term::ArithmeticsMap &arith, Literal::RelationVec &assign, AuxGen &auxGen) {
    static_cast<void>(assign);
    for (auto &bound : bounds_) {
        bound.rewriteArithmetics(arith, auxGen);
    }
    for (auto &elem : elems_) {
        Literal::RelationVec assign;
        arith.emplace_back(gringo_make_unique<Term::LevelMap>());
        for (auto &y : std::get<1>(elem)) {
            y->rewriteArithmetics(arith, assign, auxGen);
        }
        for (auto &y : *arith.back()) {
            std::get<1>(elem).emplace_back(RelationLiteral::make(y));
        }
        for (auto &y : assign) {
            std::get<1>(elem).emplace_back(RelationLiteral::make(y));
        }
        arith.pop_back();
    }
}

void LitBodyAggregate::assignLevels(AssignLevel &lvl) {
    VarTermBoundVec vars;
    for (auto &bound : bounds_) {
        bound.bound->collect(vars, false);
    }
    lvl.add(vars);
    for (auto &elem : elems_) {
        AssignLevel &local(lvl.subLevel());
        VarTermBoundVec vars;
        std::get<0>(elem)->collect(vars, false);
        for (auto &lit : std::get<1>(elem)) {
            lit->collect(vars, false);
        }
        local.add(vars);
    }
}

void LitBodyAggregate::check(ChkLvlVec &levels, Logger &log) const {
    auto f = [&]() {
        for (auto const &y : elems_) {
            levels.emplace_back(loc(), *this);
            _add(levels, y.first, false);
            _add(levels, y.second);
            levels.back().check(log);
            levels.pop_back();
        }
    };
    _aggr(levels, bounds_, f, naf_ == NAF::POS);
}

bool LitBodyAggregate::hasPool() const {
    for (auto const &bound : bounds_) {
        if (bound.bound->hasPool()) {
            return true;
        }
    }
    for (auto const &elem : elems_) {
        if (elem.first->hasPool(false)) {
            return true;
        }
        for (auto const &lit : elem.second) {
            if (lit->hasPool(false)) {
                return true;
            }
        }
    }
    return false;
}

void LitBodyAggregate::replace(Defines &x) {
    for (auto &bound : bounds_) {
        Term::replace(bound.bound, bound.bound->replace(x, true));
    }
    for (auto &elem : elems_) {
        elem.first->replace(x);
        for (auto &y : elem.second) {
            y->replace(x);
        }
    }
}

CreateBody LitBodyAggregate::toGround(ToGroundArg &x, Ground::UStmVec &stms) const {
    static_cast<void>(x);
    static_cast<void>(stms);
    throw std::logic_error("Aggregate::rewriteAggregates must be called before LitAggregate::toGround");
}

bool LitBodyAggregate::isAssignment() const {
    return false;
}

void LitBodyAggregate::removeAssignment()  { }

// {{{1 definition of ConjunctionElem

void ConjunctionElem::print(std::ostream &out) const {
    print_comma(out, head_, "|", [&](std::ostream &out, ULitVec const &clause) {
        print_comma(out, clause, "&", Printer{});
    });
    out << ":";
    print_comma(out, cond_, ",", Printer{});
}

bool ConjunctionElem::hasPool() const {
    for (auto const &clause : head_) {
        for (auto const &lit : clause) {
            if (lit->hasPool(false)) {
                return true;
            }
        }
    }
    for (auto const &lit : cond_) {
        if (lit->hasPool(false)) {
            return true;
        }
    }
    return false;
}

void ConjunctionElem::unpool(ConjunctionElemVec &elems) const {
    ULitVecVec head;
    for (auto const &clause : head_) {
        Term::unpool(clause.begin(), clause.end(), _unpool_lit(false), [&head](ULitVec &&clause) {
            head.emplace_back(std::move(clause));
        });
    }
    Term::unpool(cond_.begin(), cond_.end(), _unpool_lit(false), [&](ULitVec &&cond) {
        elems.emplace_back(get_clone(head), std::move(cond));
    });
}

bool ConjunctionElem::hasUnpoolComparison() const {
    for (auto const &clause : head_) {
        for (auto const &lit : clause) {
            if (lit->hasUnpoolComparison()) {
                return true;
            }
        }
    }
    for (auto const &lit : cond_) {
        if (lit->hasUnpoolComparison()) {
            return true;
        }
    }
    return false;
}

ConjunctionElemVecVec ConjunctionElem::unpoolComparison() const {
    ConjunctionElemVecVec ret;
    ULitVecVec heads;
    for (auto const &head : head_) {
        auto unpooled = unpoolComparison_(head);
        std::move(unpooled.begin(), unpooled.end(), std::back_inserter(heads));
    }
    for (auto &cond : unpoolComparison_(cond_)) {
        ret.emplace_back();
        ret.back().emplace_back(get_clone(heads), std::move(cond));
    }
    return ret;
}

void ConjunctionElem::collect(VarTermBoundVec &vars) const {
    for (auto const &clause : head_) {
        for (auto const &lit : clause) {
            lit->collect(vars, false);
        }
    }
    for (auto const &lit : cond_) {
        lit->collect(vars, false);
    }
}

bool ConjunctionElem::simplify(Projections &project, SimplifyState &state, Logger &log) {
    head_.erase(std::remove_if(head_.begin(), head_.end(), [&](ULitVec &clause) {
        auto elemState = SimplifyState::make_substate(state);
        for (auto &lit : clause) {
            if (!lit->simplify(log, project, elemState)) {
                return true;
            }
        }
        for (auto &dot : elemState.dots()) {
            clause.emplace_back(RangeLiteral::make(dot));
        }
        for (auto &script : elemState.scripts()) {
            clause.emplace_back(ScriptLiteral::make(script));
        }
        return false;
    }), head_.end());

    auto elemState = SimplifyState::make_substate(state);
    for (auto &lit : cond_) {
        // NOTE: projection disabled with singelton=true
        if (!lit->simplify(log, project, elemState, true, true)) {
            return false;
        }
    }
    for (auto &dot : elemState.dots()) {
        cond_.emplace_back(RangeLiteral::make(dot));
    }
    for (auto &script : elemState.scripts()) {
        cond_.emplace_back(ScriptLiteral::make(script));
    }
    return true;
}

void ConjunctionElem::rewriteArithmetics(Term::ArithmeticsMap &arith, AuxGen &auxGen) {
    for (auto &clause : head_) {
        Literal::RelationVec assign;
        arith.emplace_back(gringo_make_unique<Term::LevelMap>());
        for (auto &lit : clause) {
            lit->rewriteArithmetics(arith, assign, auxGen);
        }
        for (auto &x : *arith.back()) {
            clause.emplace_back(RelationLiteral::make(x));
        }
        for (auto &x : assign) {
            clause.emplace_back(RelationLiteral::make(x));
        }
        arith.pop_back();
    }
    Literal::RelationVec assign;
    arith.emplace_back(gringo_make_unique<Term::LevelMap>());
    for (auto &lit : cond_) {
        lit->rewriteArithmetics(arith, assign, auxGen);
    }
    for (auto &x : *arith.back()) {
        cond_.emplace_back(RelationLiteral::make(x));
    }
    for (auto &x : assign) {
        cond_.emplace_back(RelationLiteral::make(x));
    }
    arith.pop_back();
}

void ConjunctionElem::assignLevels(AssignLevel &lvl) const {
    AssignLevel &local(lvl.subLevel());
    VarTermBoundVec vars;
    collect(vars);
    local.add(vars);
}

void ConjunctionElem::check(BodyAggregate const &parent, ChkLvlVec &levels, Logger &log) const {
    levels.emplace_back(parent.loc(), parent);
    _add(levels, cond_);
    // check safety of condition
    levels.back().check(log);
    levels.pop_back();
    for (auto const &disj : head_) {
        levels.emplace_back(parent.loc(), parent);
        _add(levels, disj);
        _add(levels, cond_);
        // check safty of head it can contain pure variables!
        levels.back().check(log);
        levels.pop_back();
    }
}

void ConjunctionElem::replace(Defines &x) {
    for (auto &clause : head_) {
        for (auto &lit : clause) {
            lit->replace(x);
        }
    }
    for (auto &lit : cond_) {
        lit->replace(x);
    }
}

std::ostream &operator<<(std::ostream &out, ConjunctionElem const &elem) {
    elem.print(out);
    return out;
}

bool operator==(ConjunctionElem const &a, ConjunctionElem const &b) {
    return is_value_equal_to(a.head_, b.head_) && is_value_equal_to(a.cond_, b.cond_);
}

size_t get_value_hash(ConjunctionElem const &elem) {
    return get_value_hash(typeid(ConjunctionElem).hash_code(), elem.head_, elem.cond_);
}

ConjunctionElem get_clone(ConjunctionElem const &elem) {
    return {get_clone(elem.head_), get_clone(elem.cond_)};
}

CreateBody ConjunctionElem::toGround(UTerm id, ToGroundArg &x, Ground::UStmVec &stms) const {
    UTermVec local;
    std::unordered_set<String> seen;
    VarTermBoundVec varsHead;
    VarTermBoundVec varsBody;
    for (auto const &clause : head_) {
        for (auto const &lit : clause) {
            lit->collect(varsHead, false);
        }
    }
    for (auto const &lit : cond_) {
        lit->collect(varsBody, false);
    }
    for (auto &occ : varsBody) {
        if (occ.first->level != 0) {
            seen.emplace(occ.first->name);
        }
    }
    for (auto &occ : varsHead) {
        if (occ.first->level != 0 && seen.find(occ.first->name) != seen.end()) {
            local.emplace_back(occ.first->clone());
        }
    }
    stms.emplace_back(gringo_make_unique<Ground::ConjunctionComplete>(x.domains, std::move(id), std::move(local)));
    auto &completeRef = static_cast<Ground::ConjunctionComplete&>(*stms.back()); // NOLINT

    Ground::ULitVec condLits;
    for (auto const &lit : cond_) {
        condLits.emplace_back(lit->toGround(x.domains, false));
    }
    stms.emplace_back(gringo_make_unique<Ground::ConjunctionAccumulateCond>(completeRef, std::move(condLits)));

    for (auto const &clause : head_)  {
        Ground::ULitVec headLits;
        for (auto const &lit : clause) {
            headLits.emplace_back(lit->toGround(x.domains, false));
        }
        stms.emplace_back(gringo_make_unique<Ground::ConjunctionAccumulateHead>(completeRef, std::move(headLits)));
    }

    CreateStmVec split;
    split.emplace_back([&completeRef](Ground::ULitVec &&lits) -> Ground::UStm {
        auto ret = gringo_make_unique<Ground::ConjunctionAccumulateEmpty>(completeRef, std::move(lits));
        return std::move(ret);
    });

    return {[&completeRef](Ground::ULitVec &lits, bool auxiliary) {
        lits.emplace_back(gringo_make_unique<Ground::ConjunctionLiteral>(completeRef, auxiliary));
    }, std::move(split)};
}

void ConjunctionElem::gatherIEs(IESolver &solver) const {
    for (auto const &lit : cond_) {
        lit->addToSolver(solver, false);
    }
}

void ConjunctionElem::addIEBound(VarTerm const &var, IEBound const &bound) {
    cond_.emplace_back(RangeLiteral::make(var, bound));
}

// {{{1 definition of Conjunction

void Conjunction::addToSolver(IESolver &solver) {
    for (auto &elem : elems_) {
        solver.add(elem);
    }
}

void Conjunction::print(std::ostream &out) const {
    print_comma(out, elems_, ";", Printer{});
}

size_t Conjunction::hash() const {
    using Gringo::get_value_hash;
    return get_value_hash(typeid(Conjunction).hash_code(), elems_);
}

bool Conjunction::operator==(BodyAggregate const &other) const {
    const auto *t = dynamic_cast<Conjunction const *>(&other);
    return t != nullptr &&
           is_value_equal_to(elems_, t->elems_);
}

Conjunction *Conjunction::clone() const {
    using Gringo::get_clone;
    return make_locatable<Conjunction>(loc(), get_clone(elems_)).release();
}

bool Conjunction::hasPool() const {
    return std::any_of(elems_.begin(), elems_.end(), [](ConjunctionElem const &elem) { return elem.hasPool(); });
}

void Conjunction::unpool(UBodyAggrVec &x) {
    ConjunctionElemVec elems;
    for (auto &elem : elems_) {
        elem.unpool(elems);
    }
    x.emplace_back(make_locatable<Conjunction>(loc(), std::move(elems)));
}

bool Conjunction::hasUnpoolComparison() const {
    return std::any_of(elems_.begin(), elems_.end(), [](ConjunctionElem const &elem) { return elem.hasUnpoolComparison(); });
}

UBodyAggrVecVec Conjunction::unpoolComparison() const {
    // a conjunction has form:
    //   Condiditon -> Clause & ... & Clause
    // clauses have form:
    //   Lit | ... | Lit
    UBodyAggrVecVec ret;
    ret.emplace_back();
    for (auto const &elem : elems_) {
        for (auto &elems : elem.unpoolComparison()) {
            ret.back().emplace_back(make_locatable<Conjunction>(loc(), std::move(elems)));
        }
    }
    return ret;
}

void Conjunction::collect(VarTermBoundVec &vars) const {
    for (auto const &elem : elems_) {
        elem.collect(vars);
    }
}

bool Conjunction::rewriteAggregates(UBodyAggrVec &aggr) {
    while (elems_.size() > 1) {
        ConjunctionElemVec elems;
        elems.emplace_back(std::move(elems_.back()));
        aggr.emplace_back(make_locatable<Conjunction>(loc(), std::move(elems)));
        elems_.pop_back();
    }
    return !elems_.empty();
}

bool Conjunction::simplify(Projections &project, SimplifyState &state, bool singleton, Logger &log) {
    static_cast<void>(singleton);
    elems_.erase(std::remove_if(elems_.begin(), elems_.end(), [&](ConjunctionElem &elem) {
        return !elem.simplify(project, state, log);
    }), elems_.end());
    return true;
}

void Conjunction::rewriteArithmetics(Term::ArithmeticsMap &arith, Literal::RelationVec &assign, AuxGen &auxGen) {
    static_cast<void>(assign);
    for (auto &elem : elems_) {
        elem.rewriteArithmetics(arith, auxGen);
    }
}

void Conjunction::assignLevels(AssignLevel &lvl) {
    for (auto &elem : elems_) {
        elem.assignLevels(lvl);
    }
}

void Conjunction::check(ChkLvlVec &levels, Logger &log) const {
    levels.back().current = &levels.back().dep.insertEnt();
    for (auto const &elem : elems_) {
        elem.check(*this, levels, log);
    }
}

void Conjunction::replace(Defines &x) {
    for (auto &elem : elems_) {
        elem.replace(x);
    }
}

CreateBody Conjunction::toGround(ToGroundArg &x, Ground::UStmVec &stms) const {
    assert(elems_.size() == 1);

    return elems_.front().toGround(x.newId(*this), x, stms);
}

bool Conjunction::isAssignment() const {
    return false;
}

void Conjunction::removeAssignment() { }

// {{{1 definition of SimpleBodyLiteral

SimpleBodyLiteral::SimpleBodyLiteral(ULit &&lit)
: lit_(std::move(lit)) { }

Location const &SimpleBodyLiteral::loc() const {
    return lit_->loc();
}

void SimpleBodyLiteral::loc(Location const &loc) {
    lit_->loc(loc);
}

void SimpleBodyLiteral::addToSolver(IESolver &solver) {
    lit_->addToSolver(solver, false);
}

void SimpleBodyLiteral::print(std::ostream &out) const {
    lit_->print(out);
}

size_t SimpleBodyLiteral::hash() const {
    return get_value_hash(typeid(SimpleBodyLiteral).hash_code(), lit_);
}

bool SimpleBodyLiteral::operator==(BodyAggregate const &other) const {
    const auto *t = dynamic_cast<SimpleBodyLiteral const *>(&other);
    return t != nullptr &&
           is_value_equal_to(lit_, t->lit_);
}

SimpleBodyLiteral *SimpleBodyLiteral::clone() const {
    return gringo_make_unique<SimpleBodyLiteral>(get_clone(lit_)).release();
}

void SimpleBodyLiteral::unpool(UBodyAggrVec &x) {
    for (auto &y : lit_->unpool(false)) {
        x.emplace_back(gringo_make_unique<SimpleBodyLiteral>(std::move(y)));
    }
}

bool SimpleBodyLiteral::hasUnpoolComparison() const {
    return lit_->hasUnpoolComparison();
}

UBodyAggrVecVec SimpleBodyLiteral::unpoolComparison() const {
    UBodyAggrVecVec ret;
    for (auto &lits : lit_->unpoolComparison()) {
        ret.emplace_back();
        for (auto &lit : lits) {
            ret.back().emplace_back(gringo_make_unique<SimpleBodyLiteral>(std::move(lit)));
        }
    }
    return ret;
}

void SimpleBodyLiteral::collect(VarTermBoundVec &vars) const {
    lit_->collect(vars, true);
}

bool SimpleBodyLiteral::rewriteAggregates(UBodyAggrVec &aggr) {
    static_cast<void>(aggr);
    return true;
}

bool SimpleBodyLiteral::simplify(Projections &project, SimplifyState &state, bool singleton, Logger &log) {
    return lit_->simplify(log, project, state, true, singleton);
}

void SimpleBodyLiteral::rewriteArithmetics(Term::ArithmeticsMap &arith, Literal::RelationVec &assign, AuxGen &auxGen) {
    lit_->rewriteArithmetics(arith, assign, auxGen);
}

void SimpleBodyLiteral::assignLevels(AssignLevel &lvl) {
    VarTermBoundVec vars;
    lit_->collect(vars, false);
    lvl.add(vars);
}

void SimpleBodyLiteral::check(ChkLvlVec &levels, Logger &log) const {
    static_cast<void>(log);
    levels.back().current = &levels.back().dep.insertEnt();
    _add(levels, lit_, true);
}

bool SimpleBodyLiteral::hasPool() const {
    return lit_->hasPool(false);
}

void SimpleBodyLiteral::replace(Defines &x) {
    lit_->replace(x);
}

CreateBody SimpleBodyLiteral::toGround(ToGroundArg &x, Ground::UStmVec &stms) const {
    static_cast<void>(stms);
    return {[&](Ground::ULitVec &lits, bool auxiliary) -> void {
        lits.emplace_back(lit_->toGround(x.domains, auxiliary));
    }, CreateStmVec()};
}

bool SimpleBodyLiteral::isAssignment() const  {
    return false;
}

void SimpleBodyLiteral::removeAssignment() { }

// }}}1

// {{{1 definition of HeadAggrElem

bool HeadAggrElem::hasPool() const {
    for (auto const &term : tuple_) {
        if (term->hasPool()) {
            return true;
        }
    }
    if (lit_->hasPool(false)) {
        return true;
    }
    for (auto const &lit : condition_) {
        if (lit->hasPool(false)) {
            return true;
        }
    }
    return false;
}

void HeadAggrElem::unpool(HeadAggrElemVec &pool) {
    Term::unpool(tuple_.begin(), tuple_.end(), Gringo::unpool, [&](UTermVec &&tuple) {
        Term::unpool(condition_.begin(), condition_.end(), _unpool_lit(false), [&](ULitVec &&cond) {
            Term::unpool(lit_, _unpool_lit(false), [&](ULit &&lit) {
                pool.emplace_back(get_clone(tuple), std::move(lit), get_clone(cond));
            });
        });
    });
}

bool HeadAggrElem::hasUnpoolComparison() const {
    return lit_->hasUnpoolComparison() ||
           std::any_of(condition_.begin(),
                       condition_.end(),
                       [](auto const &lit) { return lit->hasUnpoolComparison(); });
}

void HeadAggrElem::shiftLit() {
    if (ULit shifted = lit_->shift(false)) {
        lit_ = make_locatable<VoidLiteral>(lit_->loc());
        condition_.emplace_back(std::move(shifted));
    }
}

void HeadAggrElem::unpoolComparison(HeadAggrElemVec &elems) const {
    // Note: requires that comparisons in the head have been shifted
    for (auto &unpooled : unpoolComparison_(condition_)) {
        elems.emplace_back(get_clone(tuple_), get_clone(lit_), std::move(unpooled));
    }
}

void HeadAggrElem::collect(VarTermBoundVec &vars, bool tupleOnly) const {
    for (auto const &term : tuple_) {
        term->collect(vars, false);
    }
    if (!tupleOnly) {
        lit_->collect(vars, false);
        for (auto const &lit : condition_) {
            lit->collect(vars, false);
        }
    }
}

template <class T>
void HeadAggrElem::zeroLevel_(VarTermBoundVec &bound, T const &x) {
    bound.clear();
    x->collect(bound, false);
    for (auto &var : bound) {
        var.first->level = 0;
    }
}

void HeadAggrElem::shiftCondition(UBodyAggrVec &aggr, bool weight) {
    VarTermBoundVec bound;
    // Note: to handle undefinedness in tuples
    Location l = tuple_.empty() ? lit_->loc() : tuple_.front()->loc();
    for (auto &term : tuple_) {
        // NOTE: there could be special predicates for is_integer and is_defined
        zeroLevel_(bound, term);
        UTerm first = get_clone(term);
        if (weight) {
            first = make_locatable<BinOpTerm>(l, BinOp::ADD, std::move(first), make_locatable<ValTerm>(l, Symbol::createNum(0)));
            weight = false;
        }
        aggr.emplace_back(gringo_make_unique<SimpleBodyLiteral>(make_locatable<RelationLiteral>(l, Relation::LEQ, std::move(first), std::move(term))));
    }
    tuple_.clear();
    tuple_.emplace_back(make_locatable<ValTerm>(l, Symbol::createNum(0)));
    for (auto &lit : condition_) {
        zeroLevel_(bound, lit);
        aggr.emplace_back(gringo_make_unique<SimpleBodyLiteral>(std::move(lit)));
    }
    condition_.clear();
    zeroLevel_(bound, lit_);
}

void HeadAggrElem::gatherIEs(IESolver &solver) const {
    for (auto const &lit : condition_) {
        lit->addToSolver(solver, false);
    }
}

void HeadAggrElem::addIEBound(VarTerm const &var, IEBound const &bound) {
    condition_.emplace_back(RangeLiteral::make(var, bound));
}

bool HeadAggrElem::simplify(Projections &project, SimplifyState &state, Logger &log) {
    for (auto &term : tuple_) {
        if (term->simplify(state, false, false, log).update(term, false).undefined()) {
            return false;
        }
    }
    if (!lit_->simplify(log, project, state, false)) {
        return false;
    }
    for (auto &lit : condition_) {
        if (!lit->simplify(log, project, state)) {
            return false;
        }
    }
    for (auto &dot : state.dots()) {
        condition_.emplace_back(RangeLiteral::make(dot));
    }
    for (auto &script : state.scripts()) {
        condition_.emplace_back(ScriptLiteral::make(script));
    }
    return true;
}

void HeadAggrElem::rewriteArithmetics(Term::ArithmeticsMap &arith, Literal::RelationVec &assign, AuxGen &auxGen) {
    for (auto &y : condition_) {
        y->rewriteArithmetics(arith, assign, auxGen);
    }
    for (auto &y : *arith.back()) {
        condition_.emplace_back(RelationLiteral::make(y));
    }
    for (auto &y : assign) {
        condition_.emplace_back(RelationLiteral::make(y));
    }
}

void HeadAggrElem::check(ChkLvlVec &levels) const {
    _add(levels, tuple_);
    _add(levels, lit_, false);
    _add(levels, condition_);
}

void HeadAggrElem::replace(Defines &defs) {
    for (auto &y : tuple_) {
        Term::replace(y, y->replace(defs, true));
    }
    lit_->replace(defs);
    for (auto &y : condition_) {
        y->replace(defs);
    }
}

bool HeadAggrElem::isSimple() const {
    return condition_.empty();
}

UTerm HeadAggrElem::headRepr() const {
    return lit_->headRepr();
}

template <class T, class C>
std::unique_ptr<T> HeadAggrElem::toGround(ToGroundArg &x, C &completeRef) const {
    Ground::ULitVec lits;
    for (auto const &lit : condition_) {
        lits.emplace_back(lit->toGround(x.domains, false));
    }
    lits.emplace_back(gringo_make_unique<Ground::HeadAggregateLiteral>(completeRef));
    UTerm headRep(headRepr());
    PredicateDomain *predDom = headRep ? &x.domains.add(headRep->getSig()) : nullptr;
    auto ret = gringo_make_unique<T>(completeRef,
                                     get_clone(tuple_),
                                     predDom,
                                     std::move(headRep),
                                     std::move(lits));
    completeRef.addAccuDom(*ret);
    return ret;

}

std::ostream &operator<<(std::ostream &out, HeadAggrElem const &elem) {
    print_comma(out, elem.tuple_, ",", Printer{});
    out << ":";
    elem.lit_->print(out);
    out << ":";
    print_comma(out, elem.condition_, ",", Printer{});
    return out;
}

bool operator==(HeadAggrElem const &a, HeadAggrElem const &b) {
    return is_value_equal_to(a.tuple_, b.tuple_) && is_value_equal_to(a.lit_, b.lit_) && is_value_equal_to(a.condition_, b.condition_);
}

size_t get_value_hash(HeadAggrElem const &elem) {
    return get_value_hash(typeid(HeadAggrElem).hash_code(), elem.tuple_, elem.lit_, elem.condition_);
}

HeadAggrElem get_clone(HeadAggrElem const &elem) {
    return {get_clone(elem.tuple_), get_clone(elem.lit_), get_clone(elem.condition_)};
}

// {{{1 definition of TupleHeadAggregate

TupleHeadAggregate::TupleHeadAggregate(AggregateFunction fun, BoundVec &&bounds, HeadAggrElemVec &&elems)
: TupleHeadAggregate(fun, false, std::move(bounds), std::move(elems)) { }

TupleHeadAggregate::TupleHeadAggregate(AggregateFunction fun, bool translated, BoundVec &&bounds, HeadAggrElemVec &&elems)
: fun_(fun)
, translated_(translated)
, bounds_(std::move(bounds))
, elems_(std::move(elems)) { }

void TupleHeadAggregate::print(std::ostream &out) const {
    printAggr_(out, fun_, bounds_, elems_, Printer{});
}

size_t TupleHeadAggregate::hash() const {
    return get_value_hash(typeid(TupleHeadAggregate).hash_code(), size_t(fun_), bounds_, elems_);
}

bool TupleHeadAggregate::operator==(HeadAggregate const &other) const {
    const auto *t = dynamic_cast<TupleHeadAggregate const *>(&other);
    return t != nullptr &&
           fun_ == t->fun_ &&
           is_value_equal_to(bounds_, t->bounds_) &&
           is_value_equal_to(elems_, t->elems_);
}

TupleHeadAggregate *TupleHeadAggregate::clone() const {
    return make_locatable<TupleHeadAggregate>(loc(), fun_, translated_, get_clone(bounds_), get_clone(elems_)).release();
}

void TupleHeadAggregate::unpool(UHeadAggrVec &x) {
    HeadAggrElemVec elems;
    for (auto &elem : elems_) {
        elem.unpool(elems);
    }
    Term::unpool(bounds_.begin(), bounds_.end(), _unpool_bound, [&](BoundVec &&bounds) {
        x.emplace_back(make_locatable<TupleHeadAggregate>(loc(), fun_, translated_, std::move(bounds), get_clone(elems)));
    });
}

UHeadAggr TupleHeadAggregate::unpoolComparison(UBodyAggrVec &body) {
    static_cast<void>(body);
    HeadAggrElemVec elems;
    for (auto &elem : elems_) {
        elem.shiftLit();
        if (elem.hasUnpoolComparison()) {
            elem.unpoolComparison(elems);
        }
        else {
            elems.emplace_back(std::move(elem));
        }
    }
    elems_.swap(elems);
    return nullptr;
}

void TupleHeadAggregate::addToSolver(IESolver &solver) {
    for (auto &elem: elems_) {
        solver.add(elem);
    }
}

void TupleHeadAggregate::collect(VarTermBoundVec &vars) const {
    for (auto const &bound : bounds_) {
        bound.bound->collect(vars, false);
    }
    for (auto const &elem : elems_) {
        elem.collect(vars, false);
    }
}

UHeadAggr TupleHeadAggregate::rewriteAggregates(UBodyAggrVec &aggr) {
    // NOTE: if it were possible to add further rules here,
    // then also aggregates with more than one element could be supported
    if (elems_.size() == 1 && bounds_.empty()) {
        elems_.front().shiftCondition(aggr, fun_ == AggregateFunction::SUM || fun_ == AggregateFunction::SUMP);
    }
    return nullptr;
}

bool TupleHeadAggregate::simplify(Projections &project, SimplifyState &state, Logger &log) {
    for (auto &bound : bounds_) {
        if (!bound.simplify(state, log)) {
            return false;
        }
    }
    elems_.erase(std::remove_if(elems_.begin(), elems_.end(), [&](HeadAggrElemVec::value_type &elem) {
        auto elemState = SimplifyState::make_substate(state);
        return !elem.simplify(project, elemState, log);
    }), elems_.end());
    return true;
}

void TupleHeadAggregate::rewriteArithmetics(Term::ArithmeticsMap &arith, AuxGen &auxGen) {
    for (auto &bound : bounds_) {
        bound.rewriteArithmetics(arith, auxGen);
    }
    for (auto &elem : elems_) {
        Literal::RelationVec assign;
        arith.emplace_back(gringo_make_unique<Term::LevelMap>());
        elem.rewriteArithmetics(arith, assign, auxGen);
        arith.pop_back();
    }
}

void TupleHeadAggregate::assignLevels(AssignLevel &lvl) {
    VarTermBoundVec vars;
    for (auto &bound : bounds_) {
        bound.bound->collect(vars, false);
    }
    lvl.add(vars);
    for (auto &elem : elems_) {
        AssignLevel &local(lvl.subLevel());
        VarTermBoundVec vars;
        elem.collect(vars);
        local.add(vars);
    }
}

void TupleHeadAggregate::check(ChkLvlVec &levels, Logger &log) const {
    auto f = [&]() {
        VarTermBoundVec vars;
        for (auto const &elem : elems_) {
            levels.emplace_back(loc(), *this);
            elem.check(levels);
            levels.back().check(log);
            levels.pop_back();
            elem.collect(vars, true);
        }
        warnGlobal(vars, !translated_, log);
    };
    return _aggr(levels, bounds_, f, false);
}

bool TupleHeadAggregate::hasPool() const {
    return std::any_of(bounds_.begin(),
                       bounds_.end(),
                       [](auto const &bound) { return bound.bound->hasPool(); }) ||
           std::any_of(elems_.begin(),
                       elems_.end(),
                       [](auto const &elem) { return elem.hasPool(); });
}

void TupleHeadAggregate::replace(Defines &x) {
    for (auto &bound : bounds_) {
        Term::replace(bound.bound, bound.bound->replace(x, true));
    }
    for (auto &elem : elems_) {
        elem.replace(x);
    }
}

CreateHead TupleHeadAggregate::toGround(ToGroundArg &x, Ground::UStmVec &stms) const {
    bool isSimple = bounds_.empty() && std::all_of(elems_.begin(), elems_.end(), [](auto const &elem) { return elem.isSimple(); });
    if (isSimple) {
        DomainData &data = x.domains;
        return [&](Ground::ULitVec &&lits) {
            Ground::AbstractRule::HeadVec heads;
            for (auto const &elem : elems_) {
                if (UTerm headRepr = elem.headRepr()) {
                    PredicateDomain *headDom = &data.add(headRepr->getSig());
                    heads.emplace_back(std::move(headRepr), headDom);
                }
            }
            return gringo_make_unique<Ground::Rule<false>>(std::move(heads), std::move(lits));
        };
    }

    stms.emplace_back(gringo_make_unique<Ground::HeadAggregateComplete>(x.domains, x.newId(*this), fun_, get_clone(bounds_)));
    auto &completeRef = static_cast<Ground::HeadAggregateComplete&>(*stms.back()); // NOLINT
    for (auto const &elem : elems_) {
        stms.emplace_back(elem.toGround<Ground::HeadAggregateAccumulate>(x, completeRef));
    }
    return [&completeRef](Ground::ULitVec &&lits) {
        return gringo_make_unique<Ground::HeadAggregateRule>(completeRef, std::move(lits));
    };
}

// {{{1 definition of LitHeadAggregate

LitHeadAggregate::LitHeadAggregate(AggregateFunction fun, BoundVec &&bounds, CondLitVec &&elems)
: fun_(fun)
, bounds_(std::move(bounds))
, elems_(std::move(elems)) { }

void LitHeadAggregate::print(std::ostream &out) const {
    printAggr_(out, fun_, bounds_, elems_, printCond_);
}

size_t LitHeadAggregate::hash() const {
    return get_value_hash(typeid(LitHeadAggregate).hash_code(), size_t(fun_), bounds_, elems_);
}

bool LitHeadAggregate::operator==(HeadAggregate const &other) const {
    const auto *t = dynamic_cast<LitHeadAggregate const *>(&other);
    return t != nullptr &&
           fun_ == t->fun_ &&
           is_value_equal_to(bounds_, t->bounds_) &&
           is_value_equal_to(elems_, t->elems_);
}

LitHeadAggregate *LitHeadAggregate::clone() const {
    return make_locatable<LitHeadAggregate>(loc(), fun_, get_clone(bounds_), get_clone(elems_)).release();
}

void LitHeadAggregate::unpool(UHeadAggrVec &x) {
    CondLitVec e;
    for (auto &elem : elems_) {
        auto f = [&](ULit &&y) { e.emplace_back(std::move(y), get_clone(elem.second)); };
        Term::unpool(elem.first, _unpool_lit(true), f);
    }
    elems_.clear();
    for (auto &elem : e) {
        auto f = [&](ULitVec &&y) { elems_.emplace_back(get_clone(elem.first), std::move(y)); };
        Term::unpool(elem.second.begin(), elem.second.end(), _unpool_lit(false), f);
    }
    e.clear();
    auto f = [&](BoundVec &&y) { x.emplace_back(make_locatable<LitHeadAggregate>(loc(), fun_, std::move(y), get_clone(elems_))); };
    Term::unpool(bounds_.begin(), bounds_.end(), _unpool_bound, f);
}

UHeadAggr LitHeadAggregate::unpoolComparison(UBodyAggrVec &body) {
    static_cast<void>(body);
    int id = 0;
    HeadAggrElemVec elems;
    for (auto &x : elems_) {
        UTermVec tuple;
        x.first->toTuple(tuple, id);
        elems.emplace_back(std::move(tuple), get_clone(x.first), get_clone(x.second));
    }
    UHeadAggr x(make_locatable<TupleHeadAggregate>(loc(), fun_, true, get_clone(bounds_), std::move(elems)));
    Term::replace(x, x->unpoolComparison(body));
    return x;
}

void LitHeadAggregate::collect(VarTermBoundVec &vars) const {
    for (auto const &bound : bounds_) {
        bound.bound->collect(vars, false);
    }
    for (auto const &elem : elems_) {
        elem.first->collect(vars, false);
        for (auto const &lit : elem.second) {
            lit->collect(vars, false);
        }
    }
}

UHeadAggr LitHeadAggregate::rewriteAggregates(UBodyAggrVec &aggr) {
    static_cast<void>(aggr);
    throw  std::runtime_error("LitHeadAggregate::rewriteAggregate: must not be called");
}

bool LitHeadAggregate::simplify(Projections &project, SimplifyState &state, Logger &log) {
    for (auto &bound : bounds_) {
        if (!bound.simplify(state, log)) {
            return false;
        }
    }
    elems_.erase(std::remove_if(elems_.begin(), elems_.end(), [&](CondLitVec::value_type &elem) {
        auto elemState = SimplifyState::make_substate(state);
        if (!std::get<0>(elem)->simplify(log, project, elemState, false)) {
            return true;
        }
        for (auto &lit : std::get<1>(elem)) {
            if (!lit->simplify(log, project, elemState)) {
                return true;
            }
        }
        for (auto &dot : elemState.dots()) {
            std::get<1>(elem).emplace_back(RangeLiteral::make(dot));
        }
        for (auto &script : elemState.scripts()) {
            std::get<1>(elem).emplace_back(ScriptLiteral::make(script));
        }
        return false;
    }), elems_.end());
    return true;
}

void LitHeadAggregate::rewriteArithmetics(Term::ArithmeticsMap &arith, AuxGen &auxGen) {
    for (auto &bound : bounds_) {
        bound.rewriteArithmetics(arith, auxGen);
    }
    for (auto &elem : elems_) {
        Literal::RelationVec assign;
        arith.emplace_back(gringo_make_unique<Term::LevelMap>());
        for (auto &y : std::get<1>(elem)) {
            y->rewriteArithmetics(arith, assign, auxGen);
        }
        for (auto &y : *arith.back()) {
            std::get<1>(elem).emplace_back(RelationLiteral::make(y));
        }
        for (auto &y : assign) {
            std::get<1>(elem).emplace_back(RelationLiteral::make(y));
        }
        arith.pop_back();
    }
}

void LitHeadAggregate::assignLevels(AssignLevel &lvl) {
    VarTermBoundVec vars;
    for (auto &bound : bounds_) {
        bound.bound->collect(vars, false);
    }
    lvl.add(vars);
    for (auto &elem : elems_) {
        AssignLevel &local(lvl.subLevel());
        VarTermBoundVec vars;
        std::get<0>(elem)->collect(vars, false);
        for (auto &lit : std::get<1>(elem)) {
            lit->collect(vars, false);
        }
        local.add(vars);
    }
}

void LitHeadAggregate::check(ChkLvlVec &levels, Logger &log) const {
    auto f = [&]() {
        for (auto const &y : elems_) {
            levels.emplace_back(loc(), *this);
            _add(levels, y.first, false);
            _add(levels, y.second);
            levels.back().check(log);
            levels.pop_back();
        }
    };
    _aggr(levels, bounds_, f, false);
}

bool LitHeadAggregate::hasPool() const {
    for (auto const &bound : bounds_) {
        if (bound.bound->hasPool()) {
            return true;
        }
    }
    for (auto const &elem : elems_) {
        if (elem.first->hasPool(true)) {
            return true;
        }
        for (auto const &lit : elem.second) {
            if (lit->hasPool(false)) {
                return true;
            }
        }
    }
    return false;
}

void LitHeadAggregate::replace(Defines &x) {
    for (auto &bound : bounds_) {
        Term::replace(bound.bound, bound.bound->replace(x, true));
    }
    for (auto &elem : elems_) {
        elem.first->replace(x);
        for (auto &y : elem.second) {
            y->replace(x);
        }
    }
}

CreateHead LitHeadAggregate::toGround(ToGroundArg &x, Ground::UStmVec &stms) const {
    static_cast<void>(x);
    static_cast<void>(stms);
    throw std::logic_error("Aggregate::rewriteAggregates must be called before LitAggregate::toGround");
}

// {{{1 definition of DisjunctionElem

void DisjunctionElem::gatherIEs(IESolver &solver) const {
    for (auto const &lit : cond_) {
        lit->addToSolver(solver, false);
    }
}

void DisjunctionElem::addIEBound(VarTerm const &var, IEBound const &bound) {
    cond_.emplace_back(RangeLiteral::make(var, bound));
}

void DisjunctionElem::print(std::ostream &out) const {
    print_comma(out, heads_, "&", [](std::ostream &out, Head const &head) {
        out << *head.first << ":";
        print_comma(out, head.second, ",", Printer{});
    });
    out << ":";
    print_comma(out, cond_, ",", Printer{});
}

void DisjunctionElem::unpool(DisjunctionElemVec &elems) {
    HeadVec heads;
    for (auto &head : heads_) {
        for (auto &lit : head.first->unpool(true)) {
            Term::unpool(head.second.begin(), head.second.end(), _unpool_lit(false), [&](ULitVec &&expand) {
                heads.emplace_back(get_clone(lit), std::move(expand));
            });
        }
    }
    Term::unpool(cond_.begin(), cond_.end(), _unpool_lit(false), [&](ULitVec &&cond) {
        elems.emplace_back(get_clone(heads), std::move(cond));
    });
}

bool DisjunctionElem::hasUnpoolComparison() const {
    for (auto const &head : heads_) {
        for (auto const &lit : head.second) {
            if (lit->hasUnpoolComparison()) {
                return true;
            }
        }
    }
    for (auto const &lit : cond_) {
        if (lit->hasUnpoolComparison()) {
            return true;
        }
    }
    return false;
}

void DisjunctionElem::unpoolComparison(DisjunctionElemVec &elems) {
    // shift comparisons
    for (auto &head : heads_) {
        if (ULit shifted = head.first->shift(true)) {
            head.first = make_locatable<VoidLiteral>(head.first->loc());
            head.second.emplace_back(std::move(shifted));
        }
    }
    // skip unpooling if there are no comparisons
    if (!hasUnpoolComparison()) {
        elems.emplace_back(std::move(*this));
        return;
    }
    // unpool comparisons
    HeadVec heads;
    for (auto const &head : heads_) {
        for (auto &cond : unpoolComparison_(head.second)) {
            heads.emplace_back(get_clone(head.first), std::move(cond));
        }
    }
    // compute cross-product of the unpooled conditions
    for (auto &cond : unpoolComparison_(cond_)) {
        elems.emplace_back(get_clone(heads), std::move(cond));
    }
}

void DisjunctionElem::collect(VarTermBoundVec &vars) const {
    for (auto const &head : heads_) {
        head.first->collect(vars, false);
        for (auto const &lit : head.second) {
            lit->collect(vars, false);
        }
    }
    for (auto const &lit : cond_) {
        lit->collect(vars, false);
    }
}

void DisjunctionElem::rewriteAggregates(Location const &loc, UBodyAggrVec &aggr) {
    for (auto &head : heads_) {
        if (ULit shifted = head.first->shift(true)) {
            head.first = make_locatable<VoidLiteral>(head.first->loc());
            if (!shifted->triviallyTrue()) {
                head.second.emplace_back(std::move(shifted));
            }
        }
    }
    if (cond_.empty() && heads_.size() == 1) {
        auto &head = heads_.front();
        VarTermBoundVec vars;
        head.first->collect(vars, false);
        for (auto &var : vars) {
            var.first->level = 0;
        }
        vars.clear();
        for (auto &lit : head.second) {
            lit->collect(vars, false);
            for (auto &var : vars) {
                var.first->level = 0;
            }
            vars.clear();
            aggr.emplace_back(make_locatable<SimpleBodyLiteral>(loc, std::move(lit)));
        }
        head.second.clear();
    }
}

bool DisjunctionElem::simplify(Projections &project, SimplifyState &state, Logger &log) {
    for (auto &head : heads_) {
        bool replace = false;
        auto elemState = SimplifyState::make_substate(state);
        if (head.first->simplify(log, project, elemState)) {
            for (auto &lit : head.second) {
                if (!lit->simplify(log, project, elemState)) {
                    replace = true;
                    break;
                }
            }
        }
        else {
            replace = true;
        }
        if (replace) {
            auto loc = head.first->loc();
            head.first = Gringo::make_locatable<RelationLiteral>(loc, Relation::EQ, make_locatable<ValTerm>(
                loc, Symbol::createNum(0)), make_locatable<ValTerm>(loc, Symbol::createNum(0)));
            head.second.clear();
        }
        else {
            for (auto &dot : elemState.dots()) {
                head.second.emplace_back(RangeLiteral::make(dot));
            }
            for (auto &script : elemState.scripts()) {
                head.second.emplace_back(ScriptLiteral::make(script));
            }
        }
    }
    auto elemState = SimplifyState::make_substate(state);
    for (auto &lit : cond_) {
        if (!lit->simplify(log, project, elemState)) {
            return false;
        }
    }
    for (auto &dot : elemState.dots()) {
        cond_.emplace_back(RangeLiteral::make(dot));
    }
    for (auto &script : elemState.scripts()) {
        cond_.emplace_back(ScriptLiteral::make(script));
    }
    return true;
}

void DisjunctionElem::rewriteArithmetics(Term::ArithmeticsMap &arith, AuxGen &auxGen) {
    for (auto &head : heads_) {
        Literal::RelationVec assign;
        arith.emplace_back(gringo_make_unique<Term::LevelMap>());
        for (auto &lit : head.second) {
            lit->rewriteArithmetics(arith, assign, auxGen);
        }
        for (auto &y : *arith.back()) {
            head.second.emplace_back(RelationLiteral::make(y));
        }
        for (auto &y : assign) {
            head.second.emplace_back(RelationLiteral::make(y));
        }
        arith.pop_back();
    }
    Literal::RelationVec assign;
    arith.emplace_back(gringo_make_unique<Term::LevelMap>());
    for (auto &lit : cond_) {
        lit->rewriteArithmetics(arith, assign, auxGen);
    }
    for (auto &y : *arith.back()) {
        cond_.emplace_back(RelationLiteral::make(y));
    }
    for (auto &y : assign) {
        cond_.emplace_back(RelationLiteral::make(y));
    }
    arith.pop_back();
}

void DisjunctionElem::assignLevels(AssignLevel &lvl) {
    AssignLevel &local(lvl.subLevel());
    VarTermBoundVec vars;
    for (auto &head : heads_) {
        head.first->collect(vars, false);
        for (auto &lit : head.second) {
            lit->collect(vars, false);
        }
    }
    for (auto &lit : cond_) {
        lit->collect(vars, false);
    }
    local.add(vars);
}

void DisjunctionElem::check(HeadAggregate const &parent, ChkLvlVec &levels, Logger &log) const {
    levels.emplace_back(parent.loc(), parent);
    _add(levels, cond_);
    // check condition first
    // (although by construction it cannot happen that something in expand binds something in condition)
    levels.back().check(log);
    levels.pop_back();
    for (auto const &head : heads_) {
        levels.emplace_back(parent.loc(), parent);
        _add(levels, head.first, false);
        _add(levels, head.second);
        _add(levels, cond_);
        levels.back().check(log);
        levels.pop_back();
    }
}

bool DisjunctionElem::hasPool() const {
    for (auto const &head : heads_) {
        if (head.first->hasPool(true)) {
            return true;
        }
        for (auto const &lit : head.second) {
            if (lit->hasPool(false)) {
                return true;
            }
        }
    }
    for (auto const &lit : cond_) {
        if (lit->hasPool(false)) {
            return true;
        }
    }
    return false;
}

void DisjunctionElem::replace(Defines &x) {
    for (auto &head : heads_) {
        head.first->replace(x);
        for (auto &lit : head.second) {
            lit->replace(x);
        }
    }
    for (auto &lit : cond_) {
        lit->replace(x);
    }
}

bool DisjunctionElem::isSimple() const {
    return heads_.size() <= 1 && cond_.empty();
}

template <class T>
void DisjunctionElem::toGroundSimple(ToGroundArg &x, T &heads) const {
    DomainData &data = x.domains;
    for (auto const &head : heads_) {
        if (UTerm headRepr = head.first->headRepr()) {
            PredicateDomain *headDom = &data.add(headRepr->getSig());
            heads.emplace_back(std::move(headRepr), headDom);
        }
    }
}

template <class T>
void DisjunctionElem::toGround(Location const &loc, T &complete, ToGroundArg &x, Ground::UStmVec &stms) const {
    UTermVec elemVars;
    std::unordered_set<String> seen;
    VarTermBoundVec vars;
    for (auto const &x : cond_) {
        x->collect(vars, false);
    }
    for (auto &occ : vars) {
        if (occ.first->level != 0) {
            seen.emplace(occ.first->name);
        }
    }
    vars.clear();
    for (auto const &head : heads_) {
        head.first->collect(vars, false);
        for (auto const &x : head.second) {
            x->collect(vars, false);
        }
    }
    for (auto &occ : vars) {
        if (occ.first->level != 0 && seen.find(occ.first->name) != seen.end()) {
            elemVars.emplace_back(occ.first->clone());
            seen.emplace(occ.first->name);
        }
    }
    UTerm elemRepr = x.newId(std::move(elemVars), loc, true);
    if (heads_.empty()) {
        Ground::ULitVec elemCond;
        for (auto const &z : cond_) {
            elemCond.emplace_back(z->toGround(x.domains, false));
        }
        Ground::ULitVec headCond;
        headCond.emplace_back(make_locatable<RelationLiteral>(loc,
                                                              Relation::NEQ,
                                                              make_locatable<ValTerm>(loc, Symbol::createNum(0)),
                                                              make_locatable<ValTerm>(loc, Symbol::createNum(0)))->toGround(x.domains, true));
        stms.emplace_back(gringo_make_unique<Ground::DisjunctionAccumulate>(complete,
                                                                            nullptr,
                                                                            nullptr,
                                                                            std::move(headCond),
                                                                            std::move(elemRepr),
                                                                            std::move(elemCond)));
    }
    else {
        for (auto const &head : heads_) {
            Ground::ULitVec elemCond;
            for (auto const &z : cond_) {
                elemCond.emplace_back(z->toGround(x.domains, false));
            }
            Ground::ULitVec headCond;
            for (auto const &z : head.second) {
                headCond.emplace_back(z->toGround(x.domains, false));
            }
            UTerm headRepr = head.first->headRepr();
            PredicateDomain *headDom = headRepr ? &x.domains.add(headRepr->getSig()) : nullptr;
            stms.emplace_back(gringo_make_unique<Ground::DisjunctionAccumulate>(complete,
                                                                                headDom,
                                                                                std::move(headRepr),
                                                                                std::move(headCond),
                                                                                get_clone(elemRepr),
                                                                                std::move(elemCond)));
        }
    }
}

std::ostream &operator<<(std::ostream &out, DisjunctionElem const &elem) {
    elem.print(out);
    return out;
}

bool operator==(DisjunctionElem const &a, DisjunctionElem const &b) {
    return is_value_equal_to(a.heads_, b.heads_) && is_value_equal_to(a.cond_, b.cond_);
}

size_t get_value_hash(DisjunctionElem const &elem) {
    return get_value_hash(typeid(DisjunctionElem).hash_code(), elem.heads_, elem.cond_);
}

DisjunctionElem get_clone(DisjunctionElem const &elem) {
    return {get_clone(elem.heads_), get_clone(elem.cond_)};
}

// {{{1 definition of Disjunction

void Disjunction::addToSolver(IESolver &solver) {
    for (auto &elem : elems_) {
        solver.add(elem);
    }
}

Disjunction::Disjunction(CondLitVec elems) {
    for (auto &lit : elems) {
        elems_.emplace_back(std::move(lit));
    }
}

Disjunction::Disjunction(DisjunctionElemVec elems)
: elems_(std::move(elems)) { }

void Disjunction::print(std::ostream &out) const {
    print_comma(out, elems_, ";", Printer{});
}

size_t Disjunction::hash() const {
    using Gringo::get_value_hash;
    return get_value_hash(typeid(Disjunction).hash_code(), elems_);
}

bool Disjunction::operator==(HeadAggregate const &other) const {
    const auto *t = dynamic_cast<Disjunction const *>(&other);
    return t != nullptr &&
           is_value_equal_to(elems_, t->elems_);
}

Disjunction *Disjunction::clone() const {
    using Gringo::get_clone;
    return make_locatable<Disjunction>(loc(), get_clone(elems_)).release();
}

void Disjunction::unpool(UHeadAggrVec &x) {
    DisjunctionElemVec elems;
    for (auto &elem : elems_) {
        elem.unpool(elems);
    }
    x.emplace_back(make_locatable<Disjunction>(loc(), std::move(elems)));
}

UHeadAggr Disjunction::unpoolComparison(UBodyAggrVec &body) {
    static_cast<void>(body);
    // unpool elements
    DisjunctionElemVec elems;
    for (auto &elem : elems_) {
        // compute cross-product of unpooled head conditions
        elem.unpoolComparison(elems);
    }
    elems_ = std::move(elems);
    return nullptr;
}

void Disjunction::collect(VarTermBoundVec &vars) const {
    for (auto const &elem : elems_) {
        elem.collect(vars);
    }
}

UHeadAggr Disjunction::rewriteAggregates(UBodyAggrVec &aggr) {
    for (auto &elem : elems_) {
        elem.rewriteAggregates(loc(), aggr);
    }
    return nullptr;
}

bool Disjunction::simplify(Projections &project, SimplifyState &state, Logger &log) {
    elems_.erase(std::remove_if(elems_.begin(), elems_.end(), [&](DisjunctionElem &elem) -> bool {
        return !elem.simplify(project, state, log);
    }), elems_.end());
    return true;
}

void Disjunction::rewriteArithmetics(Term::ArithmeticsMap &arith, AuxGen &auxGen) {
    for (auto &elem : elems_) {
        elem.rewriteArithmetics(arith, auxGen);
    }
}

void Disjunction::assignLevels(AssignLevel &lvl) {
    for (auto &elem : elems_) {
        elem.assignLevels(lvl);
    }
}

void Disjunction::check(ChkLvlVec &levels, Logger &log) const {
    levels.back().current = &levels.back().dep.insertEnt();
    for (auto const &elem : elems_) {
        elem.check(*this, levels, log);
    }
}

bool Disjunction::hasPool() const {
    return std::any_of(elems_.begin(), elems_.end(), [](auto const &elem) { return elem.hasPool(); });
}

void Disjunction::replace(Defines &x) {
    for (auto &elem : elems_) {
        elem.replace(x);
    }
}

CreateHead Disjunction::toGround(ToGroundArg &x, Ground::UStmVec &stms) const {
    if (std::all_of(elems_.begin(), elems_.end(), [](auto const &elem) { return elem.isSimple(); })) {
        return [&](Ground::ULitVec &&lits) {
            Ground::AbstractRule::HeadVec heads;
            for (auto const &elem : elems_) {
                elem.toGroundSimple(x, heads);
            }
            return gringo_make_unique<Ground::Rule<true>>(std::move(heads), std::move(lits));
        };
    }
    stms.emplace_back(gringo_make_unique<Ground::DisjunctionComplete>(x.domains, x.newId(*this)));
    auto &complete = static_cast<Ground::DisjunctionComplete&>(*stms.back()); // NOLINT
    for (auto const &elem : elems_) {
        elem.toGround(loc(), complete, x, stms);
    }
    return [&](Ground::ULitVec &&lits) {
        return gringo_make_unique<Ground::DisjunctionRule>(complete, std::move(lits));
    };
}

// {{{1 definition of SimpleHeadLiteral

SimpleHeadLiteral::SimpleHeadLiteral(ULit &&lit)
: lit_(std::move(lit)) { }

Location const &SimpleHeadLiteral::loc() const {
    return lit_->loc();
}

void SimpleHeadLiteral::loc(Location const &loc) {
    lit_->loc(loc);
}

void SimpleHeadLiteral::addToSolver(IESolver &solver) {
    lit_->addToSolver(solver, true);
}

void SimpleHeadLiteral::print(std::ostream &out) const {
    lit_->print(out);
}

size_t SimpleHeadLiteral::hash() const {
    return get_value_hash(typeid(SimpleHeadLiteral).hash_code(), lit_);
}

bool SimpleHeadLiteral::operator==(HeadAggregate const &other) const {
    const auto *t = dynamic_cast<SimpleHeadLiteral const *>(&other);
    return t != nullptr &&
           is_value_equal_to(lit_, t->lit_);
}

SimpleHeadLiteral *SimpleHeadLiteral::clone() const {
    return gringo_make_unique<SimpleHeadLiteral>(get_clone(lit_)).release();
}

bool SimpleHeadLiteral::hasPool() const {
    return lit_->hasPool(true);
}

void SimpleHeadLiteral::unpool(UHeadAggrVec &x) {
    for (auto &y : lit_->unpool(true)) {
        x.emplace_back(gringo_make_unique<SimpleHeadLiteral>(std::move(y)));
    }
}

UHeadAggr SimpleHeadLiteral::unpoolComparison(UBodyAggrVec &body) {
    ULit shifted(lit_->shift(true));
    if (shifted) {
        body.emplace_back(gringo_make_unique<SimpleBodyLiteral>(std::move(shifted)));
        return gringo_make_unique<SimpleHeadLiteral>(make_locatable<VoidLiteral>(lit_->loc()));
    }
    return nullptr;
}

void SimpleHeadLiteral::collect(VarTermBoundVec &vars) const {
    lit_->collect(vars, false);
}

UHeadAggr SimpleHeadLiteral::rewriteAggregates(UBodyAggrVec &aggr) {
    static_cast<void>(aggr);
    return nullptr;
}

bool SimpleHeadLiteral::simplify(Projections &project, SimplifyState &state, Logger &log) {
    return lit_->simplify(log, project, state, false);
}

void SimpleHeadLiteral::rewriteArithmetics(Term::ArithmeticsMap &arith, AuxGen &auxGen) {
    // Note: nothing to do
    static_cast<void>(arith);
    static_cast<void>(auxGen);
}

void SimpleHeadLiteral::assignLevels(AssignLevel &lvl) {
    VarTermBoundVec vars;
    lit_->collect(vars, false);
    lvl.add(vars);
}

Symbol SimpleHeadLiteral::isEDB() const {
    return lit_->isEDB();
}

void SimpleHeadLiteral::check(ChkLvlVec &levels, Logger &log) const {
    static_cast<void>(log);
    _add(levels, lit_, false);
}

void SimpleHeadLiteral::replace(Defines &x) {
    lit_->replace(x);
}

CreateHead SimpleHeadLiteral::toGround(ToGroundArg &x, Ground::UStmVec &stms) const {
    static_cast<void>(stms);
    return [this, &x](Ground::ULitVec &&lits) -> Ground::UStm {
        Ground::AbstractRule::HeadVec heads;
        if (UTerm headRepr = lit_->headRepr()) {
            Sig sig(headRepr->getSig());
            heads.emplace_back(std::move(headRepr), &x.domains.add(sig));
        }
        return gringo_make_unique<Ground::Rule<true>>(std::move(heads), std::move(lits));
    };
}

// {{{1 definition of MinimizeHeadLiteral

MinimizeHeadLiteral::MinimizeHeadLiteral(UTerm &&weight, UTerm &&priority, UTermVec &&tuple)
: tuple_(std::move(tuple)) {
    tuple_.emplace_back(std::move(weight));
    tuple_.emplace_back(std::move(priority));
    std::rotate(tuple_.begin(), tuple_.end() - 2, tuple_.end());
}

MinimizeHeadLiteral::MinimizeHeadLiteral(UTermVec &&tuple)
: tuple_(std::move(tuple)) {
    assert(tuple_.size() >= 2);
}

void MinimizeHeadLiteral::print(std::ostream &out) const {
    out << "[";
    weight().print(out);
    out << "@";
    priority().print(out);
    for (auto it = tuple_.begin() + 2, ie = tuple_.end(); it != ie; ++it) {
        out << ",";
        (*it)->print(out);
    }
    out << "]";
}

void MinimizeHeadLiteral::printWithCondition(std::ostream &out, UBodyAggrVec const &condition) const {
    out << ":~";
    print_comma(out, condition, ";", Printer{});
    out << "." << *this;
}

size_t MinimizeHeadLiteral::hash() const {
    return get_value_hash(typeid(MinimizeHeadLiteral).hash_code(), tuple_);
}

bool MinimizeHeadLiteral::operator==(HeadAggregate const &other) const {
    const auto *t = dynamic_cast<MinimizeHeadLiteral const *>(&other);
    return t != nullptr &&
           is_value_equal_to(tuple_, t->tuple_);
}

MinimizeHeadLiteral *MinimizeHeadLiteral::clone() const {
    return make_locatable<MinimizeHeadLiteral>(loc(), get_clone(tuple_)).release();
}

void MinimizeHeadLiteral::unpool(UHeadAggrVec &x) {
    auto f = [&](UTermVec &&tuple) { x.emplace_back(make_locatable<MinimizeHeadLiteral>(loc(), std::move(tuple))); };
    Term::unpool(tuple_.begin(), tuple_.end(), Gringo::unpool, f);
}

UHeadAggr MinimizeHeadLiteral::unpoolComparison(UBodyAggrVec &body) {
    static_cast<void>(body);
    return nullptr;
}

void MinimizeHeadLiteral::collect(VarTermBoundVec &vars) const {
    for (auto const &term : tuple_) {
        term->collect(vars, false);
    }
}

UHeadAggr MinimizeHeadLiteral::rewriteAggregates(UBodyAggrVec &aggr) {
    static_cast<void>(aggr);
    return nullptr;
}

bool MinimizeHeadLiteral::simplify(Projections &project, SimplifyState &state, Logger &log) {
    static_cast<void>(project);
    for (auto &term : tuple_) {
        if (term->simplify(state, false, false, log).update(term, false).undefined()) {
            return false;
        }
    }
    return true;
}

void MinimizeHeadLiteral::rewriteArithmetics(Term::ArithmeticsMap &arith, AuxGen &auxGen) {
    // Note: nothing to do
    static_cast<void>(arith);
    static_cast<void>(auxGen);
}

void MinimizeHeadLiteral::assignLevels(AssignLevel &lvl) {
    VarTermBoundVec vars;
    for (auto &term : tuple_) {
        term->collect(vars, false);
    }
    lvl.add(vars);
}

void MinimizeHeadLiteral::check(ChkLvlVec &levels, Logger &log) const {
    static_cast<void>(log);
    levels.back().current = &levels.back().dep.insertEnt();
    VarTermBoundVec vars;
    collect(vars);
    addVars(levels, vars);
}

bool MinimizeHeadLiteral::hasPool() const {
    for (auto const &term : tuple_) {
        if (term->hasPool()) {
            return true;
        }
    }
    return false;
}

void MinimizeHeadLiteral::replace(Defines &x) {
    for (auto &term : tuple_) {
        Term::replace(term, term->replace(x, true));
    }
}

CreateHead MinimizeHeadLiteral::toGround(ToGroundArg &x, Ground::UStmVec &stms) const {
    static_cast<void>(x);
    static_cast<void>(stms);
    return [&](Ground::ULitVec &&lits) {
        return gringo_make_unique<Ground::WeakConstraint>(get_clone(tuple_), std::move(lits));
    };
}

Term &MinimizeHeadLiteral::weight() const {
    return *tuple_[0];
}

Term &MinimizeHeadLiteral::priority() const {
    return *tuple_[1];
}

// {{{1 definition of EdgeHeadAtom

EdgeHeadAtom::EdgeHeadAtom(UTerm &&u, UTerm &&v)
: u_(std::move(u))
, v_(std::move(v))
{ }

void EdgeHeadAtom::print(std::ostream &out) const {
    out << "#edge(" << *u_ << "," << *v_ << ")";
}

size_t EdgeHeadAtom::hash() const {
    return get_value_hash(typeid(EdgeHeadAtom).hash_code(), u_, v_);
}

bool EdgeHeadAtom::operator==(HeadAggregate const &other) const {
    const auto *t = dynamic_cast<EdgeHeadAtom const *>(&other);
    return t != nullptr &&
           is_value_equal_to(u_, t->u_) &&
           is_value_equal_to(v_, t->v_);;
}

EdgeHeadAtom *EdgeHeadAtom::clone() const {
    return make_locatable<EdgeHeadAtom>(loc(), get_clone(u_), get_clone(v_)).release();
}

void EdgeHeadAtom::unpool(UHeadAggrVec &x) {
    for (auto &u : Gringo::unpool(u_)) {
        for (auto &v : Gringo::unpool(v_)) {
            x.emplace_back(make_locatable<EdgeHeadAtom>(loc(), get_clone(u), std::move(v)));
        }
    }
}

UHeadAggr EdgeHeadAtom::unpoolComparison(UBodyAggrVec &body) {
    static_cast<void>(body);
    return nullptr;
}

void EdgeHeadAtom::collect(VarTermBoundVec &vars) const {
    u_->collect(vars, false);
    v_->collect(vars, false);
}

UHeadAggr EdgeHeadAtom::rewriteAggregates(UBodyAggrVec &aggr) {
    static_cast<void>(aggr);
    return nullptr;
}

bool EdgeHeadAtom::simplify(Projections &project, SimplifyState &state, Logger &log) {
    static_cast<void>(project);
    return !u_->simplify(state, false, false, log).update(u_, false).undefined() &&
           !v_->simplify(state, false, false, log).update(v_, false).undefined();
}

void EdgeHeadAtom::rewriteArithmetics(Term::ArithmeticsMap &arith, AuxGen &auxGen) {
    // Note: nothing to do
    static_cast<void>(arith);
    static_cast<void>(auxGen);
}

void EdgeHeadAtom::assignLevels(AssignLevel &lvl) {
    VarTermBoundVec vars;
    collect(vars);
    lvl.add(vars);
}

void EdgeHeadAtom::check(ChkLvlVec &levels, Logger &log) const {
    static_cast<void>(log);
    levels.back().current = &levels.back().dep.insertEnt();
    VarTermBoundVec vars;
    collect(vars);
    addVars(levels, vars);
}

bool EdgeHeadAtom::hasPool() const {
    return u_->hasPool() || v_->hasPool();
}

void EdgeHeadAtom::replace(Defines &x) {
    Term::replace(u_, u_->replace(x, true));
    Term::replace(v_, v_->replace(x, true));
}

CreateHead EdgeHeadAtom::toGround(ToGroundArg &x, Ground::UStmVec &stms) const {
    static_cast<void>(x);
    static_cast<void>(stms);
    return [&](Ground::ULitVec &&lits) {
        return gringo_make_unique<Ground::EdgeStatement>(get_clone(u_), get_clone(v_), std::move(lits));
    };
}

// {{{1 definition of ProjectHeadAtom

ProjectHeadAtom::ProjectHeadAtom(UTerm &&atom)
: atom_(std::move(atom)) { }

void ProjectHeadAtom::print(std::ostream &out) const {
    out << "#project " << *atom_;
}

size_t ProjectHeadAtom::hash() const {
    return get_value_hash(typeid(ProjectHeadAtom).hash_code(), atom_);
}

bool ProjectHeadAtom::operator==(HeadAggregate const &other) const {
    const auto *t = dynamic_cast<ProjectHeadAtom const *>(&other);
    return t != nullptr &&
           is_value_equal_to(atom_, t->atom_);
}

ProjectHeadAtom *ProjectHeadAtom::clone() const {
    return make_locatable<ProjectHeadAtom>(loc(), get_clone(atom_)).release();
}

void ProjectHeadAtom::unpool(UHeadAggrVec &x) {
    for (auto &atom : Gringo::unpool(atom_)) {
        x.emplace_back(make_locatable<ProjectHeadAtom>(loc(), std::move(atom)));
    }
}

UHeadAggr ProjectHeadAtom::unpoolComparison(UBodyAggrVec &body) {
    static_cast<void>(body);
    return nullptr;
}

void ProjectHeadAtom::collect(VarTermBoundVec &vars) const {
    atom_->collect(vars, false);
}

UHeadAggr ProjectHeadAtom::rewriteAggregates(UBodyAggrVec &aggr) {
    aggr.emplace_back(gringo_make_unique<SimpleBodyLiteral>(make_locatable<PredicateLiteral>(atom_->loc(), NAF::POS, get_clone(atom_), true)));
    return nullptr;
}

bool ProjectHeadAtom::simplify(Projections &project, SimplifyState &state, Logger &log) {
    static_cast<void>(project);
    return !atom_->simplify(state, false, false, log).update(atom_, false).undefined();
}

void ProjectHeadAtom::rewriteArithmetics(Term::ArithmeticsMap &arith, AuxGen &auxgen) {
    atom_->rewriteArithmetics(arith, auxgen);
}

void ProjectHeadAtom::assignLevels(AssignLevel &lvl) {
    VarTermBoundVec vars;
    collect(vars);
    lvl.add(vars);
}

void ProjectHeadAtom::check(ChkLvlVec &levels, Logger &log) const {
    static_cast<void>(log);
    levels.back().current = &levels.back().dep.insertEnt();
    VarTermBoundVec vars;
    collect(vars);
    addVars(levels, vars);
}

bool ProjectHeadAtom::hasPool() const {
    return atom_->hasPool();
}

void ProjectHeadAtom::replace(Defines &x) {
    atom_->replace(x, false);
}

CreateHead ProjectHeadAtom::toGround(ToGroundArg &x, Ground::UStmVec &stms) const {
    static_cast<void>(x);
    static_cast<void>(stms);
    return [&](Ground::ULitVec &&lits) {
        return gringo_make_unique<Ground::ProjectStatement>(get_clone(atom_), std::move(lits));
    };
}

// {{{1 definition of ExternalHeadAtom

ExternalHeadAtom::ExternalHeadAtom(UTerm &&atom, UTerm &&type)
: atom_(std::move(atom))
, type_(std::move(type))
{ }

void ExternalHeadAtom::print(std::ostream &out) const {
    out << "#external " << *atom_;
}

void ExternalHeadAtom::printWithCondition(std::ostream &out, UBodyAggrVec const &condition) const {
    out << *this;
    if (!condition.empty()) {
        out << ":";
        print_comma(out, condition, ";", Printer{});
    }
    out << "." << "[" << *type_ << "]";
}

size_t ExternalHeadAtom::hash() const {
    return get_value_hash(typeid(ExternalHeadAtom).hash_code(), atom_, type_);
}

bool ExternalHeadAtom::operator==(HeadAggregate const &other) const {
    const auto *t = dynamic_cast<ExternalHeadAtom const *>(&other);
    return t != nullptr &&
           is_value_equal_to(atom_, t->atom_) &&
           is_value_equal_to(type_, t->type_);
}

ExternalHeadAtom *ExternalHeadAtom::clone() const {
    return make_locatable<ExternalHeadAtom>(loc(), get_clone(atom_), get_clone(type_)).release();
}

void ExternalHeadAtom::unpool(UHeadAggrVec &x) {
    for (auto &atom : Gringo::unpool(atom_)) {
        for (auto &type : Gringo::unpool(type_)) {
            x.emplace_back(make_locatable<ExternalHeadAtom>(loc(), get_clone(atom), std::move(type)));
        }
    }
}

UHeadAggr ExternalHeadAtom::unpoolComparison(UBodyAggrVec &body) {
    static_cast<void>(body);
    return nullptr;
}

void ExternalHeadAtom::collect(VarTermBoundVec &vars) const {
    atom_->collect(vars, false);
    type_->collect(vars, false);
}

UHeadAggr ExternalHeadAtom::rewriteAggregates(UBodyAggrVec &aggr) {
    static_cast<void>(aggr);
    return nullptr;
}

bool ExternalHeadAtom::simplify(Projections &project, SimplifyState &state, Logger &log) {
    static_cast<void>(project);
    return !atom_->simplify(state, false, false, log).update(atom_, false).undefined() &&
           !type_->simplify(state, false, false, log).update(type_, false).undefined();
}

void ExternalHeadAtom::rewriteArithmetics(Term::ArithmeticsMap &arith, AuxGen &auxgen) {
    atom_->rewriteArithmetics(arith, auxgen);
    type_->rewriteArithmetics(arith, auxgen);
}

void ExternalHeadAtom::assignLevels(AssignLevel &lvl) {
    VarTermBoundVec vars;
    collect(vars);
    lvl.add(vars);
}

void ExternalHeadAtom::check(ChkLvlVec &levels, Logger &log) const {
    static_cast<void>(log);
    levels.back().current = &levels.back().dep.insertEnt();
    VarTermBoundVec vars;
    collect(vars);
    addVars(levels, vars);
}

bool ExternalHeadAtom::hasPool() const {
    return atom_->hasPool() || type_->hasPool();
}

void ExternalHeadAtom::replace(Defines &x) {
    atom_->replace(x, false);
    type_->replace(x, true);
}

CreateHead ExternalHeadAtom::toGround(ToGroundArg &x, Ground::UStmVec &stms) const {
    static_cast<void>(stms);
    return [this, &x](Ground::ULitVec &&lits) -> Ground::UStm {
        Ground::AbstractRule::HeadVec heads;
        Sig sig(atom_->getSig());
        heads.emplace_back(get_clone(atom_), &x.domains.add(sig));
        return gringo_make_unique<Ground::ExternalStatement>(std::move(heads), std::move(lits), get_clone(type_));
    };
}

// {{{1 definition of HeuristicHeadAtom

HeuristicHeadAtom::HeuristicHeadAtom(UTerm &&atom, UTerm &&bias, UTerm &&priority, UTerm &&mod)
: atom_(std::move(atom))
, value_(std::move(bias))
, priority_(std::move(priority))
, mod_(std::move(mod))
{ }

void HeuristicHeadAtom::print(std::ostream &out) const {
    out << "#heuristic " << *atom_ << "[" << *value_ << "@" << *priority_ << "," << *mod_ << "]";
}

size_t HeuristicHeadAtom::hash() const {
    return get_value_hash(typeid(HeuristicHeadAtom).hash_code(), atom_, value_, priority_, mod_);
}

bool HeuristicHeadAtom::operator==(HeadAggregate const &other) const {
    const auto *t = dynamic_cast<HeuristicHeadAtom const *>(&other);
    return t != nullptr &&
           is_value_equal_to(atom_, t->atom_) &&
           is_value_equal_to(value_, t->value_) &&
           is_value_equal_to(priority_, t->priority_) &&
           is_value_equal_to(mod_, t->mod_);
}

HeuristicHeadAtom *HeuristicHeadAtom::clone() const {
    return make_locatable<HeuristicHeadAtom>(loc(), get_clone(atom_), get_clone(value_), get_clone(priority_), get_clone(mod_)).release();
}

void HeuristicHeadAtom::unpool(UHeadAggrVec &x) {
    for (auto &atom : Gringo::unpool(atom_)) {
        for (auto &bias : Gringo::unpool(value_)) {
            for (auto &priority : Gringo::unpool(priority_)) {
                for (auto &mod : Gringo::unpool(mod_)) {
                    x.emplace_back(make_locatable<HeuristicHeadAtom>(loc(), get_clone(atom), get_clone(bias), get_clone(priority), get_clone(mod)));
                }
            }
        }
    }
}

UHeadAggr HeuristicHeadAtom::unpoolComparison(UBodyAggrVec &body) {
    static_cast<void>(body);
    return nullptr;
}

void HeuristicHeadAtom::collect(VarTermBoundVec &vars) const {
    atom_->collect(vars, false);
    value_->collect(vars, false);
    priority_->collect(vars, false);
    mod_->collect(vars, false);
}

UHeadAggr HeuristicHeadAtom::rewriteAggregates(UBodyAggrVec &aggr) {
    aggr.emplace_back(gringo_make_unique<SimpleBodyLiteral>(make_locatable<PredicateLiteral>(atom_->loc(), NAF::POS, get_clone(atom_), true)));
    return nullptr;
}

bool HeuristicHeadAtom::simplify(Projections &project, SimplifyState &state, Logger &log) {
    static_cast<void>(project);
    return
        !atom_->simplify(state, false, false, log).update(atom_, false).undefined() &&
        !value_->simplify(state, false, false, log).update(value_, false).undefined() &&
        !priority_->simplify(state, false, false, log).update(priority_, false).undefined() &&
        !mod_->simplify(state, false, false, log).update(mod_, false).undefined();
}

void HeuristicHeadAtom::rewriteArithmetics(Term::ArithmeticsMap &arith, AuxGen &auxgen) {
    atom_->rewriteArithmetics(arith, auxgen);
}

void HeuristicHeadAtom::assignLevels(AssignLevel &lvl) {
    VarTermBoundVec vars;
    collect(vars);
    lvl.add(vars);
}

void HeuristicHeadAtom::check(ChkLvlVec &levels, Logger &log) const {
    static_cast<void>(log);
    levels.back().current = &levels.back().dep.insertEnt();
    VarTermBoundVec vars;
    collect(vars);
    addVars(levels, vars);
}

bool HeuristicHeadAtom::hasPool() const {
    return atom_->hasPool() || value_->hasPool() || priority_->hasPool();
}

void HeuristicHeadAtom::replace(Defines &x) {
    Term::replace(atom_, atom_->replace(x, false));
    Term::replace(value_, value_->replace(x, true));
    Term::replace(priority_, priority_->replace(x, true));
    Term::replace(mod_, mod_->replace(x, true));
}

CreateHead HeuristicHeadAtom::toGround(ToGroundArg &x, Ground::UStmVec &stms) const {
    static_cast<void>(x);
    static_cast<void>(stms);
    return [&](Ground::ULitVec &&lits) {
        return gringo_make_unique<Ground::HeuristicStatement>(get_clone(atom_), get_clone(value_), get_clone(priority_), get_clone(mod_), std::move(lits));
    };
}

// {{{1 definition of ShowHeadLiteral

ShowHeadLiteral::ShowHeadLiteral(UTerm &&term)
: term_(std::move(term)) { }

void ShowHeadLiteral::print(std::ostream &out) const {
    out << "#show " << *term_;
}

size_t ShowHeadLiteral::hash() const {
    return get_value_hash(typeid(ShowHeadLiteral).hash_code(), term_);
}

bool ShowHeadLiteral::operator==(HeadAggregate const &other) const {
    const auto *t = dynamic_cast<ShowHeadLiteral const *>(&other);
    return t != nullptr &&
           is_value_equal_to(term_, t->term_);
}

ShowHeadLiteral *ShowHeadLiteral::clone() const {
    return make_locatable<ShowHeadLiteral>(loc(), get_clone(term_)).release();
}

void ShowHeadLiteral::unpool(UHeadAggrVec &x) {
    for (auto &term : Gringo::unpool(term_)) {
        x.emplace_back(make_locatable<ShowHeadLiteral>(loc(), std::move(term)));
    }
}

UHeadAggr ShowHeadLiteral::unpoolComparison(UBodyAggrVec &body) {
    static_cast<void>(body);
    return nullptr;
}

void ShowHeadLiteral::collect(VarTermBoundVec &vars) const {
    term_->collect(vars, false);
}

UHeadAggr ShowHeadLiteral::rewriteAggregates(UBodyAggrVec &aggr) {
    static_cast<void>(aggr);
    return nullptr;
}

bool ShowHeadLiteral::simplify(Projections &project, SimplifyState &state, Logger &log) {
    static_cast<void>(project);
    return !term_->simplify(state, false, false, log).update(term_, false).undefined();
}

void ShowHeadLiteral::rewriteArithmetics(Term::ArithmeticsMap &arith, AuxGen &auxGen) {
    // Note: nothing to do
    static_cast<void>(arith);
    static_cast<void>(auxGen);
}

void ShowHeadLiteral::assignLevels(AssignLevel &lvl) {
    VarTermBoundVec vars;
    collect(vars);
    lvl.add(vars);
}

void ShowHeadLiteral::check(ChkLvlVec &levels, Logger &log) const {
    static_cast<void>(log);
    levels.back().current = &levels.back().dep.insertEnt();
    VarTermBoundVec vars;
    collect(vars);
    addVars(levels, vars);
}

bool ShowHeadLiteral::hasPool() const {
    return term_->hasPool();
}

void ShowHeadLiteral::replace(Defines &x) {
    Term::replace(term_, term_->replace(x, true));
}

CreateHead ShowHeadLiteral::toGround(ToGroundArg &x, Ground::UStmVec &stms) const {
    static_cast<void>(x);
    static_cast<void>(stms);
    return [&](Ground::ULitVec &&lits) {
        return gringo_make_unique<Ground::ShowStatement>(get_clone(term_), std::move(lits));
    };
}

// }}}1

} } // namespace Input Gringo
