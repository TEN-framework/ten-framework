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

#include "gringo/input/literals.hh"
#include "gringo/ground/literals.hh"
#include "gringo/output/output.hh"
#include "gringo/utility.hh"

namespace Gringo { namespace Input {

// {{{ definition of Literal::

Symbol Literal::isEDB() const {
    return {};
}

// }}}
// {{{ definition of PredicateLiteral

PredicateLiteral::PredicateLiteral(NAF naf, UTerm &&repr, bool auxiliary)
: naf_(naf)
, auxiliary_(auxiliary)
, repr_(std::move(repr)) {
    if (!this->repr_->isAtom()) {
        throw std::runtime_error("atom expected");
    }
}

unsigned PredicateLiteral::projectScore() const {
    return 1 + repr_->projectScore();
}

void PredicateLiteral::print(std::ostream &out) const {
    if (auxiliary()) {
        out << "[";
    }
    out << naf_ << *repr_;
    if (auxiliary()) {
        out << "]";
    }
}

PredicateLiteral *PredicateLiteral::clone() const {
    return make_locatable<PredicateLiteral>(loc(), naf_, get_clone(repr_)).release();
}

bool PredicateLiteral::simplify(Logger &log, Projections &project, SimplifyState &state, bool positional, bool singleton) {
    if (singleton && positional && naf_ == NAF::POS) {
        positional = false;
    }
    auto ret(repr_->simplify(state, positional, false, log));
    ret.update(repr_, false);
    if (ret.undefined()) {
        return false;
    }
    if (repr_->simplify(state, positional, false, log).update(repr_, false).project()) {
        auto rep(project.add(*repr_));
        Term::replace(repr_, std::move(rep));
    }
    return true;
}

void PredicateLiteral::collect(VarTermBoundVec &vars, bool bound) const {
    repr_->collect(vars, bound && naf_ == NAF::POS);
}

bool PredicateLiteral::operator==(Literal const &other) const {
    const auto *t = dynamic_cast<PredicateLiteral const *>(&other);
    return t != nullptr &&
           naf_ == t->naf_ &&
           is_value_equal_to(repr_, t->repr_) &&
           (auxiliary_ == t->auxiliary_);
}

void PredicateLiteral::rewriteArithmetics(Term::ArithmeticsMap &arith, RelationVec &assign, AuxGen &auxGen) {
    if (naf_ == NAF::POS) {
        Term::replace(repr_, repr_->rewriteArithmetics(arith, auxGen));
    }
}

size_t PredicateLiteral::hash() const {
    return get_value_hash(typeid(PredicateLiteral).hash_code(), size_t(naf_), repr_);
}

ULitVec PredicateLiteral::unpool(bool head) const {
    ULitVec value;
    auto f = [&](UTerm &&y){ value.emplace_back(make_locatable<PredicateLiteral>(loc(), naf_, std::move(y))); };
    Term::unpool(repr_, Gringo::unpool, f);
    return  value;
}

void PredicateLiteral::toTuple(UTermVec &tuple, int &id) const {
    int naf_id = 0;
    switch (naf_) {
        case NAF::POS:    { naf_id = 0; break; }
        case NAF::NOT:    { naf_id = 1; break; }
        case NAF::NOTNOT: { naf_id = 2; break; }
    }
    tuple.emplace_back(make_locatable<ValTerm>(loc(), Symbol::createNum(naf_id)));
    tuple.emplace_back(get_clone(repr_));
}

Symbol PredicateLiteral::isEDB() const {
    return naf_ == NAF::POS ? repr_->isEDB() : Symbol();
}

bool PredicateLiteral::hasPool(bool head) const {
    return repr_->hasPool();
}

void PredicateLiteral::replace(Defines &x) {
    Term::replace(repr_, repr_->replace(x, false));
}

Ground::ULit PredicateLiteral::toGround(DomainData &x, bool auxiliary) const {
    return gringo_make_unique<Ground::PredicateLiteral>(auxiliary_ || auxiliary, x.add(repr_->getSig()), naf_, get_clone(repr_));
}

ULit PredicateLiteral::shift(bool negate) {
    if (naf_ == NAF::POS) {
        return nullptr;
    }
    NAF inv = (naf_ == NAF::NOT) == negate ? NAF::NOTNOT : NAF::NOT;
    return make_locatable<PredicateLiteral>(loc(), inv, std::move(repr_));
}

UTerm PredicateLiteral::headRepr() const {
    assert(naf_ == NAF::POS);
    return get_clone(repr_);
}

bool PredicateLiteral::auxiliary() const {
    return auxiliary_;
}

void PredicateLiteral::auxiliary(bool auxiliary) {
    auxiliary_ = auxiliary;
}

// }}}
// {{{ definition of ProjectionLiteral

ProjectionLiteral::ProjectionLiteral(UTerm &&repr)
: PredicateLiteral(NAF::POS, std::move(repr)), initialized_(false) { }

ProjectionLiteral *ProjectionLiteral::clone() const {
    throw std::logic_error("ProjectionLiteral::clone must not be called!!!");
}

ULitVec ProjectionLiteral::unpool(bool head) const {
    throw std::logic_error("ProjectionLiteral::unpool must not be called!!!");
}

Ground::ULit ProjectionLiteral::toGround(DomainData &x, bool auxiliary) const {
    bool initialized = initialized_;
    initialized_ = true;
    auto repr = headRepr();
    auto &dom = x.add(repr->getSig());

    return gringo_make_unique<Ground::ProjectionLiteral>(this->auxiliary() || auxiliary, dom, get_clone(repr), initialized);
}

ULit ProjectionLiteral::shift(bool negate) {
    throw std::logic_error("ProjectionLiteral::shift must not be called!!!");
}

// }}}
// {{{ definition of RelationLiteral

RelationLiteral::RelationLiteral(Relation rel, UTerm &&left, UTerm &&right)
: left_{std::move(left)}
, naf_{NAF::POS} {
    right_.emplace_back(rel, std::move(right));
}

RelationLiteral::RelationLiteral(NAF naf, Relation rel, UTerm &&left, UTerm &&right)
: left_{std::move(left)}
, naf_{NAF::POS} {
    right_.emplace_back(naf == NAF::NOT ? neg(rel) : rel, std::move(right));
}

RelationLiteral::RelationLiteral(NAF naf, UTerm &&left, Terms &&right)
: left_(std::move(left))
, right_(std::move(right))
, naf_{naf == NAF::NOT ? NAF::NOT : NAF::POS} {
    if (naf_ == NAF::NOT && right_.size() == 1) {
        naf_ = NAF::POS;
        right_.front().first = neg(right_.front().first);
    }
}

ULit RelationLiteral::make(Term::LevelMap::value_type &x) {
    Location loc(x.first->loc());
    return make_locatable<RelationLiteral>(loc, NAF::POS, Relation::EQ, std::move(x.second), get_clone(x.first));
}

ULit RelationLiteral::make(Literal::RelationVec::value_type &x) {
    Location loc(std::get<1>(x)->loc() + std::get<2>(x)->loc());
    return make_locatable<RelationLiteral>(loc, NAF::POS, std::get<0>(x), std::move(std::get<1>(x)), get_clone(std::get<2>(x)));
}

void RelationLiteral::addToSolver(IESolver &solver, bool invert) const {
    if (right_.size() != 1) {
        return;
    }
    auto rel = right_.front().first;
    if (invert) {
        rel = neg(rel);
    }
    if (naf_ == NAF::NOT) {
        rel = neg(rel);
    }
    if (rel == Relation::NEQ) {
        return;
    }
    IETermVec left;
    if (!left_->addToLinearTerm(left)) {
        return;
    }
    IETermVec right;
    if (!right_.front().second->addToLinearTerm(right)) {
        return;
    }
    // TODO: it should be possible to make this nicer!!!
    switch (rel) {
        case Relation::NEQ: {
            return;
        }
        case Relation::EQ: {
            // ignore assignements of X=number
            bool ignoreIfFixed =
                dynamic_cast<VarTerm*>(left_.get()) != nullptr &&
                dynamic_cast<ValTerm*>(right_.front().second.get()) != nullptr;
            auto temp = right;
            for (auto const &term : left) {
                subIETerm(right, term);
            }
            solver.add({right, 0}, ignoreIfFixed);
            for (auto const &term : temp) {
                subIETerm(left, term);
            }
            solver.add({left, 0}, ignoreIfFixed);
            return;
        }
        case Relation::LT: {
            addIETerm(left, {1, nullptr});
        }
        case Relation::LEQ: {
            // right - left >= 0
            for (auto const &term : left) {
                subIETerm(right, term);
            }
            solver.add({right, 0}, false);
            return;
        }
        case Relation::GT: {
            addIETerm(right, {1, nullptr});
        }
        case Relation::GEQ: {
            // left - right >= 0
            for (auto const &term : right) {
                subIETerm(left, term);
            }
            solver.add({left, 0}, false);
            return;
        }
    }
}

bool RelationLiteral::needSetShift() const {
    return true;
}

unsigned RelationLiteral::projectScore() const {
    auto score = left_->projectScore();
    for (const auto &term : right_) {
        score += term.second->projectScore();
    }
    return score;
}

void RelationLiteral::print(std::ostream &out) const {
    assert(!right_.empty());
    out << naf_ << *left_;
    for (const auto &term: right_) {
        out << term.first << *term.second;
    }
}

RelationLiteral *RelationLiteral::clone() const {
    return make_locatable<RelationLiteral>(loc(), naf_, get_clone(left_), get_clone(right_)).release();
}

bool RelationLiteral::simplify(Logger &log, Projections &project, SimplifyState &state, bool positional, bool singleton) {
    static_cast<void>(project);
    static_cast<void>(positional);
    static_cast<void>(singleton);
    auto handle_fail = [&]() {
        if (naf_ == NAF::NOT) {
            // the relation literal is trivially satisfied in this case
            naf_ = NAF::POS;
            left_ = make_locatable<ValTerm>(loc(), Symbol::createNum(0));
            right_.clear();
            right_.emplace_back(Relation::EQ, make_locatable<ValTerm>(loc(), Symbol::createNum(0)));
            return true;
        }
        return false;
    };
    if (left_->simplify(state, false, false, log).update(left_, false).undefined()) {
        return handle_fail();
    }
    for (auto &term : right_) {
        if (term.second->simplify(state, false, false, log).update(term.second, false).undefined()) {
            return handle_fail();
        }
    }
    return true;
}

void RelationLiteral::collect(VarTermBoundVec &vars, bool bound) const {
    left_->collect(vars, bound && naf_ == NAF::POS && right_.front().first == Relation::EQ);
    for (const auto &term : right_) {
        term.second->collect(vars, false);
    }
}

bool RelationLiteral::operator==(Literal const &other) const {
    const auto *t = dynamic_cast<RelationLiteral const *>(&other);
    return t != nullptr &&
           naf_ == t->naf_ &&
           is_value_equal_to(left_, t->left_) &&
           is_value_equal_to(right_, t->right_);
}

void RelationLiteral::rewriteArithmetics(Term::ArithmeticsMap &arith, RelationVec &assign, AuxGen &auxGen) {
    assert(naf_ == NAF::POS);
    UTerm *prev = &left_;
    for (auto &term : right_) {
        if (term.first == Relation::EQ) {
            if (term.second->hasVar()) {
                assign.emplace_back(Relation::EQ, get_clone(term.second), get_clone(*prev));
                Term::replace(std::get<1>(assign.back()), std::get<1>(assign.back())->rewriteArithmetics(arith, auxGen));
            }
            Term::replace(*prev, prev->get()->rewriteArithmetics(arith, auxGen));
        }
        prev = &term.second;
    }
    while (right_.size() > 1) {
        assign.emplace_back(right_.back().first, get_clone((right_.end() - 2)->second), std::move(right_.back().second));
        right_.pop_back();
    }
}

size_t RelationLiteral::hash() const {
    return get_value_hash(typeid(RelationLiteral).hash_code(), naf_, left_, right_);
}

ULitVec RelationLiteral::unpool(bool head) const {
    // unpool a term with a relation
    auto unpoolTerm = [&](Terms::value_type const &term) -> Terms {
        Terms ret;
        for (auto &x : Gringo::unpool(term.second)) {
            ret.emplace_back(term.first, std::move(x));
        }
        return ret;
    };
    // unpool a vector of terms with relations
    auto unpoolTerms = [&](Terms const &terms) -> std::vector<Terms> {
        std::vector<Terms> termsPool;
        Term::unpool(terms.begin(), terms.end(), unpoolTerm, [&termsPool](Terms &&unpooled) {
            termsPool.emplace_back(std::move(unpooled));
        });
        return termsPool;
    };
    // unpool the left hand side together with the terms and relations
    ULitVec unpooled;
    auto appendRelN = [&](UTerm &&left, Terms &&right) {
        unpooled.emplace_back(make_locatable<RelationLiteral>(loc(), naf_, std::move(left), std::move(right)));
    };
    Term::unpool(left_, right_, Gringo::unpool, unpoolTerms, appendRelN);
    return unpooled;
}

ULitVecVec RelationLiteral::unpoolComparison() const {
    ULitVecVec ret;
    if (naf_ != NAF::NOT) {
        ret.emplace_back();
        UTerm prev = get_clone(left_);
        for (const auto &term : right_) {
            ret.back().emplace_back(make_locatable<RelationLiteral>(loc(), NAF::POS, term.first, std::move(prev), get_clone(term.second)));
            prev = get_clone(term.second);
        }
    }
    else {
        UTerm prev = get_clone(left_);
        for (const auto &term : right_) {
            ret.emplace_back();
            ret.back().emplace_back(make_locatable<RelationLiteral>(loc(), NAF::POS, neg(term.first), std::move(prev), get_clone(term.second)));
            prev = get_clone(term.second);
        }
    }
    return ret;
}

bool RelationLiteral::hasUnpoolComparison() const {
    return right_.size() > 1;
}

void RelationLiteral::toTuple(UTermVec &tuple, int &id) const {
    VarTermBoundVec vars;
    left_->collect(vars, false);
    for (auto const &term : right_) {
        term.second->collect(vars, false);
    }
    tuple.emplace_back(make_locatable<ValTerm>(loc(), Symbol::createNum(id+3)));
    for (auto const &var : vars) {
        tuple.emplace_back(std::unique_ptr<VarTerm>(var.first->clone()));
    }
    ++id;
}

bool RelationLiteral::hasPool(bool head) const  {
    static_cast<void>(head);
    if (left_->hasPool()) {
        return true;
    }
    for (const auto &term : right_) {
        if (term.second->hasPool()) {
            return true;
        }
    }
    return  false;
}

void RelationLiteral::replace(Defines &x) {
    Term::replace(left_, left_->replace(x, true));
    for (auto &term : right_) {
        Term::replace(term.second, term.second->replace(x, true));
    }
}

Ground::ULit RelationLiteral::toGround(DomainData &x, bool auxiliary) const {
    static_cast<void>(x);
    static_cast<void>(auxiliary);
    assert(right_.size() == 1 && naf_ == NAF::POS);
    return gringo_make_unique<Ground::RelationLiteral>(right_.front().first, get_clone(left_), get_clone(right_.front().second));
}

ULit RelationLiteral::shift(bool negate) {
    if (negate) {
        if (naf_ == NAF::NOT) {
            naf_ = NAF::POS;
        }
        else if (right_.size() == 1) {
            naf_ = NAF::POS;
            right_.front().first = neg(right_.front().first);
        }
        else {
            naf_ = NAF::NOT;
        }
    }
    return make_locatable<RelationLiteral>(loc(), std::move(*this));
}

UTerm RelationLiteral::headRepr() const {
    throw std::logic_error("RelationLiteral::toTuple: a relation literal always has to be shifted");
}

bool RelationLiteral::auxiliary() const {
    return true;
}

void RelationLiteral::auxiliary(bool aux) {
    static_cast<void>(aux);
}

// }}}
// {{{ definition of RangeLiteral

RangeLiteral::RangeLiteral(UTerm &&assign, UTerm &&lower, UTerm &&upper)
: assign_(std::move(assign))
, lower_(std::move(lower))
, upper_(std::move(upper)) { }

ULit RangeLiteral::make(SimplifyState::DotsMap::value_type &dot) {
    Location loc(std::get<0>(dot)->loc());
    return make_locatable<RangeLiteral>(loc, std::move(std::get<0>(dot)), std::move(std::get<1>(dot)), std::move(std::get<2>(dot)));
}

ULit RangeLiteral::make(VarTerm const &var, IEBound const &bound) {
    auto loc = var.loc();
    return make_locatable<RangeLiteral>(
        loc,
        UTerm{var.clone()},
        make_locatable<ValTerm>(loc, Symbol::createNum(bound.get(IEBound::Lower))),
        make_locatable<ValTerm>(loc, Symbol::createNum(bound.get(IEBound::Upper))));
}

void RangeLiteral::addToSolver(IESolver &solver, bool invert) const {
    if (invert) {
        return;
    }
    IETermVec assign;
    if (!assign_->addToLinearTerm(assign)) {
        return;
    }
    IETermVec upper;
    if (upper_->addToLinearTerm(upper)) {
        // we get constraint upper - assign >= 0
        for (auto const &term : assign) {
            subIETerm(upper, term);
        }
        solver.add({std::move(upper), 0}, true);
    }
    IETermVec lower;
    if (lower_->addToLinearTerm(lower)) {
        // we get constraint assign - lower >= 0
        for (auto const &term : lower) {
            subIETerm(assign, term);
        }
        solver.add({std::move(assign), 0}, true);
    }
}

void RangeLiteral::print(std::ostream &out) const {
    out << "#range(" << *assign_ << "," << *lower_ << "," << *upper_ << ")";
}

RangeLiteral *RangeLiteral::clone() const {
    return make_locatable<RangeLiteral>(loc(), get_clone(assign_), get_clone(lower_), get_clone(upper_)).release();
}

bool RangeLiteral::simplify(Logger &log, Projections &project, SimplifyState &state, bool positional, bool singleton) {
    throw std::logic_error("RangeLiteral::simplify should never be called  if used properly");
}

void RangeLiteral::collect(VarTermBoundVec &vars, bool bound) const {
    assign_->collect(vars, bound);
    lower_->collect(vars, false);
    upper_->collect(vars, false);
}

bool RangeLiteral::operator==(Literal const &other) const {
    const auto *t = dynamic_cast<RangeLiteral const *>(&other);
    return t != nullptr &&
           is_value_equal_to(assign_, t->assign_) &&
           is_value_equal_to(lower_, t->lower_) &&
           is_value_equal_to(upper_, t->upper_);
}

void RangeLiteral::rewriteArithmetics(Term::ArithmeticsMap &arith, RelationVec &assign, AuxGen &auxGen) {
    Term::replace(this->assign_, this->assign_->rewriteArithmetics(arith, auxGen));
}

size_t RangeLiteral::hash() const {
    return get_value_hash(typeid(RangeLiteral).hash_code(), assign_, lower_, upper_);
}

ULitVec RangeLiteral::unpool(bool head) const {
    ULitVec value;
    value.emplace_back(ULit(clone()));
    return value;
}

void RangeLiteral::toTuple(UTermVec &tuple, int &id) const {
    throw std::logic_error("RangeLiteral::toTuple should never be called if used properly");
}

bool RangeLiteral::hasPool(bool head) const {
    return false;
}

void RangeLiteral::replace(Defines &x) {
    Term::replace(assign_, assign_->replace(x, true));
    Term::replace(lower_, lower_->replace(x, true));
    Term::replace(upper_, upper_->replace(x, true));
}

Ground::ULit RangeLiteral::toGround(DomainData &x, bool auxiliary) const {
    return gringo_make_unique<Ground::RangeLiteral>(get_clone(assign_), get_clone(lower_), get_clone(upper_));
}

ULit RangeLiteral::shift(bool negate) {
    throw std::logic_error("RangeLiteral::shift should never be called  if used properly");
}

UTerm RangeLiteral::headRepr() const {
    throw std::logic_error("RangeLiteral::toTuple should never be called if used properly");
}

bool RangeLiteral::auxiliary() const {
    return true;
}

void RangeLiteral::auxiliary(bool aux) {
    static_cast<void>(aux);
}

// }}}
// {{{ definition of ScriptLiteral

ScriptLiteral::ScriptLiteral(UTerm &&assign, String name, UTermVec &&args)
: assign_(std::move(assign))
, name_(name)
, args_(std::move(args)) { }

ULit ScriptLiteral::make(SimplifyState::ScriptMap::value_type &script) {
    Location loc(std::get<0>(script)->loc());
    return make_locatable<ScriptLiteral>(loc, std::move(std::get<0>(script)), std::get<1>(script), std::move(std::get<2>(script)));
}

void ScriptLiteral::print(std::ostream &out) const {
    out << "#script(" << *assign_ << "," << name_ << "(";
    print_comma(out, args_, ",", [](std::ostream &out, UTerm const &term) { out << *term; });
    out << ")";
}

ScriptLiteral *ScriptLiteral::clone() const {
    return make_locatable<ScriptLiteral>(loc(), get_clone(assign_), name_, get_clone(args_)).release();
}

bool ScriptLiteral::simplify(Logger &log, Projections &project, SimplifyState &state, bool positional, bool singleton) {
    throw std::logic_error("ScriptLiteral::simplify should never be called  if used properly");
}

void ScriptLiteral::collect(VarTermBoundVec &vars, bool bound) const {
    assign_->collect(vars, bound);
    for (const auto &x : args_) {
        x->collect(vars, false);
    }
}

bool ScriptLiteral::operator==(Literal const &other) const {
    const auto *t = dynamic_cast<ScriptLiteral const *>(&other);
    return t != nullptr &&
           is_value_equal_to(assign_, t->assign_) &&
           name_ == t->name_ &&
           is_value_equal_to(args_, t->args_);
}

void ScriptLiteral::rewriteArithmetics(Term::ArithmeticsMap &arith, RelationVec &assign, AuxGen &auxGen) {
    Term::replace(this->assign_, this->assign_->rewriteArithmetics(arith, auxGen));
}

size_t ScriptLiteral::hash() const {
    return get_value_hash(typeid(RangeLiteral).hash_code(), assign_, name_, args_);
}

ULitVec ScriptLiteral::unpool(bool head) const {
    ULitVec value;
    value.emplace_back(ULit(clone()));
    return value;
}

void ScriptLiteral::toTuple(UTermVec &tuple, int &id) const {
    throw std::logic_error("ScriptLiteral::toTuple should never be called if used properly");
}

bool ScriptLiteral::hasPool(bool head) const {
    return false;
}

void ScriptLiteral::replace(Defines &x) {
    Term::replace(assign_, assign_->replace(x, true));
    for (auto &y : args_) {
        Term::replace(y, y->replace(x, true));
    }
}

Ground::ULit ScriptLiteral::toGround(DomainData &x, bool auxiliary) const {
    return gringo_make_unique<Ground::ScriptLiteral>(get_clone(assign_), name_, get_clone(args_));
}

ULit ScriptLiteral::shift(bool negate) {
    throw std::logic_error("ScriptLiteral::shift should never be called  if used properly");
}

UTerm ScriptLiteral::headRepr() const {
    throw std::logic_error("ScriptLiteral::toTuple should never be called if used properly");
}

bool ScriptLiteral::auxiliary() const {
    return true;
}

void ScriptLiteral::auxiliary(bool aux) {
    static_cast<void>(aux);
}

// }}}
// {{{ definition of VoidLiteral

void VoidLiteral::print(std::ostream &out) const {
    out << "#void";
}

VoidLiteral *VoidLiteral::clone() const {
    return make_locatable<VoidLiteral>(loc()).release();
}

bool VoidLiteral::simplify(Logger &log, Projections &project, SimplifyState &state, bool positional, bool singleton) {
    return true;
}

void VoidLiteral::collect(VarTermBoundVec &vars, bool bound) const { }

bool VoidLiteral::operator==(Literal const &other) const {
    return dynamic_cast<VoidLiteral const *>(&other) != nullptr;
}

void VoidLiteral::rewriteArithmetics(Term::ArithmeticsMap &arith, RelationVec &assign, AuxGen &auxGen) { }

size_t VoidLiteral::hash() const {
    return get_value_hash(typeid(VoidLiteral).hash_code());
}

ULitVec VoidLiteral::unpool(bool head) const {
    ULitVec value;
    value.emplace_back(ULit(clone()));
    return value;
}

void VoidLiteral::toTuple(UTermVec &tuple, int &id) const {
    tuple.emplace_back(make_locatable<ValTerm>(loc(), Symbol::createNum(id+3)));
    ++id;
}

bool VoidLiteral::hasPool(bool head) const {
    return false;
}

void VoidLiteral::replace(Defines &dx) { }

Ground::ULit VoidLiteral::toGround(DomainData &x, bool auxiliary) const {
    throw std::logic_error("VoidLiteral::toGround: must not happen");
}

ULit VoidLiteral::shift(bool negate) {
    return nullptr;
}

UTerm VoidLiteral::headRepr() const {
    return nullptr;
}

bool VoidLiteral::auxiliary() const {
    return true;
}

void VoidLiteral::auxiliary(bool aux) {
    static_cast<void>(aux);
}

// }}}

} } // namespace Input Gringo

