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

#include "gringo/ground/literals.hh"
#include "gringo/ground/binders.hh"
#include "gringo/ground/types.hh"
#include "gringo/logger.hh"
#include <cmath>
#include <cstring>

namespace Gringo { namespace Ground {

namespace {

// {{{ declaration of RangeBinder

class RangeBinder : public Binder {
public:
    RangeBinder(UTerm assign, RangeLiteralShared &range)
    : assign_(std::move(assign))
    , range_(range) { }

    IndexUpdater *getUpdater() override {
        return nullptr;
    }

    void match(Logger &log) override {
        bool undefined = false;
        Symbol l{range_.first->eval(undefined, log)};
        Symbol r{range_.second->eval(undefined, log)};
        if (!undefined && l.type() == SymbolType::Num && r.type() == SymbolType::Num) {
            current_ = l.num();
            end_     = r.num();
        }
        else {
            if (!undefined) {
                GRINGO_REPORT(log, Warnings::OperationUndefined)
                    << (range_.first->loc() + range_.second->loc()) << ": info: interval undefined:\n"
                    << "  " << *range_.first << ".." << *range_.second << "\n";
            }
            current_ = 1;
            end_     = 0;
        }
    }

    bool next() override {
        // Note: if assign does not match it is not a variable and will not match at all
        //       if assign is just a number, then this is handled in the corresponding Binder
        return current_ <= end_ && assign_->match(Symbol::createNum(current_++));
    }

    void print(std::ostream &out) const override {
        out << *assign_ << "=" << *range_.first << ".." << *range_.second;
    }

private:
    UTerm               assign_;
    RangeLiteralShared &range_;
    int                 current_ = 0;
    int                 end_     = 0;

};

// }}}
// {{{ declaration of RangeMatcher

class RangeMatcher : public Binder {
public:
    RangeMatcher(Term &assign, RangeLiteralShared &range)
    : assign_(assign)
    , range_(range) { }

    IndexUpdater *getUpdater() override {
        return nullptr;
    }

    void match(Logger &log) override {
        bool undefined = false;
        Symbol l{range_.first->eval(undefined, log)};
        Symbol r{range_.second->eval(undefined, log)};
        Symbol a{assign_.eval(undefined, log)};
        if (!undefined && l.type() == SymbolType::Num && r.type() == SymbolType::Num) {
            firstMatch_ = a.type() == SymbolType::Num && l.num() <= a.num() && a.num() <= r.num();
        }
        else {
            if (!undefined) {
                GRINGO_REPORT(log, Warnings::OperationUndefined)
                    << (range_.first->loc() + range_.second->loc()) << ": info: interval undefined:\n"
                    << "  " << *range_.first << ".." << *range_.second << "\n";
            }
            firstMatch_ = false;
        }
    }

    bool next() override {
        bool m = firstMatch_;
        firstMatch_ = false;
        return m;
    }

    void print(std::ostream &out) const override {
        out << assign_ << "=" << *range_.first << ".." << *range_.second;
    }

private:
    Term               &assign_;
    RangeLiteralShared &range_;
    bool                firstMatch_ = false;
};

// }}}

// {{{ declaration of ScriptBinder

class ScriptBinder : public Binder {
public:
    ScriptBinder(Context &context, UTerm assign, ScriptLiteralShared &shared)
    : context_(context)
    , assign_(std::move(assign))
    , shared_(shared) { }

    IndexUpdater *getUpdater() override {
        return nullptr;
    }

    void match(Logger &log) override {
        SymVec args;
        bool undefined = false;
        for (auto &x : std::get<1>(shared_)) {
            args.emplace_back(x->eval(undefined, log));
        }
        if (!undefined) {
            matches_ = context_.call(assign_->loc(), std::get<0>(shared_), Potassco::toSpan(args), log);
        }
        else {
            matches_ = {};
        }
        current_ = matches_.begin();
    }

    bool next() override {
        while (current_ != matches_.end()) {
            if (assign_->match(*current_++)) {
                return true;
            }
        }
        return false;
    }
    void print(std::ostream &out) const override {
        out << *assign_ << "=" << std::get<0>(shared_) << "(";
        print_comma(out, std::get<1>(shared_), ",", [](std::ostream &out, UTerm const &term) { out << *term; });
        out << ")";
    }

private:
    Context             &context_;
    UTerm                assign_;
    ScriptLiteralShared &shared_;
    SymVec               matches_;
    SymVec::iterator     current_;
};

// }}}

// {{{ declaration of RelationMatcher

class RelationMatcher : public Binder {
public:
    RelationMatcher(RelationShared &shared)
    : shared_(shared) { }

    IndexUpdater *getUpdater() override {
        return nullptr;
    }

    void match(Logger &log) override {
        bool undefined = false;
        Symbol l(std::get<1>(shared_)->eval(undefined, log));
        if (undefined) {
            firstMatch_ = false;
            return;
        }
        Symbol r(std::get<2>(shared_)->eval(undefined, log));
        if (undefined) {
            firstMatch_ = false;
            return;
        }
        switch (std::get<0>(shared_)) {
            case Relation::GT:  {
                firstMatch_ = l >  r;
                break;
            }
            case Relation::LT:  {
                firstMatch_ = l <  r;
                break;
            }
            case Relation::GEQ: {
                firstMatch_ = l >= r;
                break;
            }
            case Relation::LEQ: {
                firstMatch_ = l <= r;
                break;
            }
            case Relation::NEQ: {
                firstMatch_ = l != r;
                break;
            }
            case Relation::EQ:  {
                firstMatch_ = l == r;
                break;
            }
        }
    }

    bool next() override {
        bool ret = firstMatch_;
        firstMatch_ = false;
        return ret;
    }

    void print(std::ostream &out) const override {
        out << *std::get<1>(shared_) << std::get<0>(shared_) << *std::get<2>(shared_);
    }

private:
    RelationShared &shared_;
    bool firstMatch_ = false;
};

// }}}
// {{{ declaration of AssignBinder

class AssignBinder : public Binder {
public:
    AssignBinder(UTerm &&lhs, Term &rhs)
    : lhs_(std::move(lhs))
    , rhs_(rhs) { }

    IndexUpdater *getUpdater() override {
        return nullptr;
    }

    void match(Logger &log) override {
        bool undefined = false;
        Symbol valRhs = rhs_.eval(undefined, log);
        if (!undefined) {
            firstMatch_ = lhs_->match(valRhs);
        }
        else {
            firstMatch_ = false;
        }
    }

    bool next() override {
        bool ret = firstMatch_;
        firstMatch_ = false;
        return ret;
    }

    void print(std::ostream &out) const override {
        out << *lhs_ << "=" << rhs_;
    }

private:
    UTerm lhs_;
    Term &rhs_;
    bool firstMatch_ = false;
};


// }}}

} // namespace

// {{{1 definition of RangeLiteral

RangeLiteral::RangeLiteral(UTerm assign, UTerm left, UTerm right)
: assign_(std::move(assign))
, range_(std::move(left), std::move(right)) { }

void RangeLiteral::print(std::ostream &out) const {
    out << *assign_ << "=" << *range_.first << ".." << *range_.second;
}

bool RangeLiteral::isRecursive() const {
    return false;
}

BodyOcc *RangeLiteral::occurrence() {
    return nullptr;
}

void RangeLiteral::collect(VarTermBoundVec &vars) const {
    assign_->collect(vars, true);
    range_.first->collect(vars, false);
    range_.second->collect(vars, false);
}

UIdx RangeLiteral::index(Context &context, BinderType type, Term::VarSet &bound) {
    static_cast<void>(context);
    static_cast<void>(type);
    if (assign_->bind(bound)) {
        return gringo_make_unique<RangeBinder>(get_clone(assign_), range_);
    }
    return gringo_make_unique<RangeMatcher>(*assign_, range_);
}

Literal::Score RangeLiteral::score(Term::VarSet const &bound, Logger &log) {
    static_cast<void>(bound);
    if (range_.first->getInvertibility() == Term::CONSTANT && range_.second->getInvertibility() == Term::CONSTANT) {
        bool undefined = false;
        Symbol l(range_.first->eval(undefined, log));
        Symbol r(range_.second->eval(undefined, log));
        return (l.type() == SymbolType::Num && r.type() == SymbolType::Num) ? static_cast<double>(r.num()) - l.num() : -1.0;
    }
    return 0;
}
std::pair<Output::LiteralId, bool> RangeLiteral::toOutput(Logger &log) {
    return {Output::LiteralId(), true};
}

// {{{1 definition of ScriptLiteral


ScriptLiteral::ScriptLiteral(UTerm assign, String name, UTermVec args)
: assign_(std::move(assign))
, shared_(name, std::move(args)) { }

void ScriptLiteral::print(std::ostream &out) const    {
    out << *assign_ << "=" << std::get<0>(shared_) << "(";
    print_comma(out, std::get<1>(shared_), ",", [](std::ostream &out, UTerm const &term) { out << *term; });
    out << ")";
}

bool ScriptLiteral::isRecursive() const {
    return false;
}

BodyOcc *ScriptLiteral::occurrence() {
    return nullptr;
}

void ScriptLiteral::collect(VarTermBoundVec &vars) const {
    assign_->collect(vars, true);
    for (auto const &x : std::get<1>(shared_)) {
        x->collect(vars, false);
    }
}

UIdx ScriptLiteral::index(Context &context, BinderType type, Term::VarSet &bound) {
    static_cast<void>(type);
    UTerm clone(assign_->clone());
    clone->bind(bound);
    return gringo_make_unique<ScriptBinder>(context, std::move(clone), shared_);
}

Literal::Score ScriptLiteral::score(Term::VarSet const &bound, Logger &log) {
    static_cast<void>(bound);
    static_cast<void>(log);
    return 0;
}

std::pair<Output::LiteralId, bool> ScriptLiteral::toOutput(Logger &log) {
    static_cast<void>(log);
    return {Output::LiteralId(), true};
}

// {{{1 definition of PredicateLiteral

RelationLiteral::RelationLiteral(Relation rel, UTerm left, UTerm right)
: shared_(rel, std::move(left), std::move(right)) { }

void RelationLiteral::print(std::ostream &out) const  {
    out << *std::get<1>(shared_) << std::get<0>(shared_) << *std::get<2>(shared_);
}

bool RelationLiteral::isRecursive() const  {
    return false;
}

BodyOcc *RelationLiteral::occurrence()  {
    return nullptr;
}

void RelationLiteral::collect(VarTermBoundVec &vars) const {
    std::get<1>(shared_)->collect(vars, std::get<0>(shared_) == Relation::EQ);
    std::get<2>(shared_)->collect(vars, false);
}

UIdx RelationLiteral::index(Context &context, BinderType type, Term::VarSet &bound) {
    static_cast<void>(context);
    static_cast<void>(type);
    if (std::get<0>(shared_) == Relation::EQ) {
        UTerm clone(std::get<1>(shared_)->clone());
        VarTermVec occBound;
        if (clone->bind(bound)) {
            return gringo_make_unique<AssignBinder>(std::move(clone), *std::get<2>(shared_));
        }
    }
    return gringo_make_unique<RelationMatcher>(shared_);
}

Literal::Score RelationLiteral::score(Term::VarSet const &bound, Logger &log) {
    static_cast<void>(bound);
    static_cast<void>(log);
    return -1;
}

std::pair<Output::LiteralId,bool> RelationLiteral::toOutput(Logger &log)  {
    static_cast<void>(log);
    return {Output::LiteralId(), true};
}

// {{{1 definition of PredicateLiteral

PredicateLiteral::PredicateLiteral(bool auxiliary, PredicateDomain &domain, NAF naf, UTerm &&repr)
: auxiliary_(auxiliary)
, repr_(std::move(repr))
, domain_(domain)
, naf_(naf) { }

void PredicateLiteral::print(std::ostream &out) const {
    if (auxiliary()) {
        out << "[";
    }
    out << naf_ << *repr_;
    switch (type_) {
        case OccurrenceType::POSITIVELY_STRATIFIED: {
            break;
        }
        case OccurrenceType::STRATIFIED: {
            out << "!";
            break;
        }
        case OccurrenceType::UNSTRATIFIED: {
            out << "?";
            break;
        }
    }
    if (auxiliary()) {
        out << "]";
    }
}

bool PredicateLiteral::isRecursive() const {
    return type_ == OccurrenceType::UNSTRATIFIED;
}

Literal::Score PredicateLiteral::score(Term::VarSet const &bound, Logger &log) {
    static_cast<void>(log);
    return naf_ == NAF::POS ? estimate(domain_.size(), *repr_, bound) : 0;
}

void PredicateLiteral::collect(VarTermBoundVec &vars) const {
    repr_->collect(vars, naf_ == NAF::POS);
}

std::pair<Output::LiteralId, bool> PredicateLiteral::toOutput(Logger &log) {
    static_cast<void>(log);
    if (offset_ == InvalidId) {
        assert(naf_ == NAF::NOT);
        return {Output::LiteralId(), true};
    }
    auto &atom = domain_[offset_];
    if (static_cast<Symbol>(atom).name().startsWith("#inc_")) {
        return {Output::LiteralId(), true};
    }
    switch (naf_) {
        case NAF::POS:
        case NAF::NOTNOT: {
            return {Output::LiteralId{naf_, Output::AtomType::Predicate, offset_, domain_.domainOffset()}, atom.fact()};
        }
        case NAF::NOT: {
            if (atom.defined() || type_ == OccurrenceType::UNSTRATIFIED) {
                return {Output::LiteralId{naf_, Output::AtomType::Predicate, offset_, domain_.domainOffset()}, false};
            }
            return {Output::LiteralId(), true};
        }
    }
    assert(false);
    return {Output::LiteralId(),true};
}

UIdx PredicateLiteral::make_index(BinderType type, Term::VarSet &bound, bool initialized) {
    return make_binder(domain_, naf_, *repr_, offset_, type, isRecursive(), bound, initialized ? numeric_cast<int>(domain_.incOffset()) : 0);
}

UIdx PredicateLiteral::index(Context &context, BinderType type, Term::VarSet &bound) {
    static_cast<void>(context);
    return make_index(type, bound, false);
}

BodyOcc *PredicateLiteral::occurrence() {
    return this;
}

UGTerm PredicateLiteral::getRepr() const {
    return repr_->gterm();
}

bool PredicateLiteral::isPositive() const {
    return naf_ == NAF::POS;
}

bool PredicateLiteral::isNegative() const {
    return naf_ != NAF::POS;
}

void PredicateLiteral::setType(OccurrenceType x) {
    type_ = x;
}

OccurrenceType PredicateLiteral::getType() const {
    if (type_ == OccurrenceType::POSITIVELY_STRATIFIED && domain_.hasChoice()) {
        return OccurrenceType::STRATIFIED;
    }
    return type_;
}

BodyOcc::DefinedBy &PredicateLiteral::definedBy() {
    return defs_;
}

void PredicateLiteral::checkDefined(LocSet &done, SigSet const &edb, UndefVec &undef) const {
    if (!auxiliary_ && defs_.empty() && done.find(repr_->loc()) == done.end() && edb.find(repr_->getSig()) == edb.end() && domain_.empty()) {
        // accumulate warnings in array of printables ..
        done.insert(repr_->loc());
        undef.emplace_back(repr_->loc(), repr_.get());
    }
}

ProjectionLiteral::ProjectionLiteral(bool auxiliary, PredicateDomain &domain, UTerm repr, bool initialized)
: PredicateLiteral(auxiliary, domain, NAF::POS, std::move(repr))
, initialized_(initialized) { }

UIdx ProjectionLiteral::index(Context &context, BinderType type, Term::VarSet &bound) {
    static_cast<void>(context);
    assert(bound.empty());
    assert(type == BinderType::ALL || type == BinderType::NEW);
    return make_index(type, bound, initialized_);
}

// }}}1

} } // namespace Ground Gringo
