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

#include <gringo/output/statement.hh>
#include <gringo/utility.hh>
#include <gringo/output/literal.hh>
#include <gringo/output/statements.hh>
#include <gringo/logger.hh>
#include <gringo/output/aggregates.hh>
#include <gringo/output/theory.hh>

namespace Gringo { namespace Output {

// {{{1 definition of functions to work with BodyAggregates

namespace {

void printLit(PrintPlain out, LiteralId lit) {
    call(out.domain, lit, &Literal::printPlain, out);
}

void printCond(PrintPlain out, TupleId tuple, Formula::value_type cond) {
    print_comma(out, out.domain.tuple(tuple), ",");
    out << ":";
    auto rng = out.domain.clause(cond.first, cond.second);
    print_comma(out, rng, ",", [](PrintPlain out, LiteralId lit) { printLit(out, lit); });
}

void printCond(PrintPlain out, TupleId tuple, HeadFormula::value_type const &cond) {
    print_comma(out, out.domain.tuple(tuple), ",");
    out << ":";
    if (cond.first.valid()) {
        printLit(out, cond.first);
    }
    else {
        out << "#true";
    }
    if (cond.second.second > 0) {
        out << ":";
        auto rng = out.domain.clause(cond.second.first, cond.second.second);
        print_comma(out, rng, ",", [](PrintPlain out, LiteralId lit) { printLit(out, lit); });
    }
}

void printBodyElem(PrintPlain out, BodyAggregateElements::value_type const &x) {
    if (x.second.empty()) {
        print_comma(out, out.domain.tuple(x.first), ",");
    }
    else {
        print_comma(out, x.second, ";", [&x](PrintPlain out, Formula::value_type cond) { printCond(out, x.first, cond); });
    }
}

void printHeadElem(PrintPlain out, HeadAggregateElements::value_type const &x) {
    print_comma(out, x.second, ";", [&x](PrintPlain out, HeadFormula::value_type const &cond) { printCond(out, x.first, cond); });
}

template <class Atom>
void makeFalse(DomainData &data, Translator &trans, NAF naf, Atom &atm) {
    LiteralId atomlit;
    switch (naf) {
        case NAF::POS: {
            atomlit = data.getTrueLit().negate();
            break;
        }
        case NAF::NOT: {
            atomlit = data.getTrueLit();
            break;
        }
        case NAF::NOTNOT: {
            atomlit = data.getTrueLit().negate();
            break;
        }
    }
    auto lit = atm.lit();
    if (lit) {
        assert(lit.sign() == NAF::POS);
        Rule().addHead(lit).addBody(atomlit).translate(data, trans);
    }
    else {
        atm.setLit(atomlit);
    }
}

} // namespace

int clamp(int64_t x) {
    if (x > std::numeric_limits<int>::max()) {
        return std::numeric_limits<int>::max();
    }
    if (x < std::numeric_limits<int>::min()) {
        return std::numeric_limits<int>::min();
    }
    return int(x);
}

bool defined(SymVec const &tuple, AggregateFunction fun, Location const &loc, Logger &log) {
    if (tuple.empty()) {
        if (fun == AggregateFunction::COUNT) {
            return true;
        }
        GRINGO_REPORT(log, Warnings::OperationUndefined)
            << loc << ": info: empty tuple ignored\n";
        return false;
    }
    if (tuple.front().type() == SymbolType::Special) {
        return true;
    }
    switch (fun) {
        case AggregateFunction::MIN:
        case AggregateFunction::MAX:
        case AggregateFunction::COUNT: {
            return true;
        }
        case AggregateFunction::SUM:
        case AggregateFunction::SUMP: {
            if (tuple.front().type() == SymbolType::Num) {
                return true;
            }
            std::ostringstream s;
            print_comma(s, tuple, ",");
            GRINGO_REPORT(log, Warnings::OperationUndefined)
                << loc << ": info: tuple ignored:\n"
                << "  " << s.str() << "\n";
            return false;
        }
    }
    return true;
}


bool neutral(SymVec const &tuple, AggregateFunction fun, Location const &loc, Logger &log) {
    if (tuple.empty()) {
        if (fun == AggregateFunction::COUNT) {
            return false;
        }
        GRINGO_REPORT(log, Warnings::OperationUndefined)
            << loc << ": info: empty tuple ignored\n";
        return true;
    }
    if (tuple.front().type() != SymbolType::Special) {
        bool ret = true;
        switch (fun) {
            case AggregateFunction::MIN: {
                return tuple.front() == Symbol::createSup();
            }
            case AggregateFunction::MAX: {
                return tuple.front() == Symbol::createInf();
            }
            case AggregateFunction::COUNT: {
                return false;
            }
            case AggregateFunction::SUM: {
                ret = tuple.front().type() != SymbolType::Num || tuple.front() == Symbol::createNum(0);
                break;
            }
            case AggregateFunction::SUMP: {
                ret = tuple.front().type() != SymbolType::Num || tuple.front() <= Symbol::createNum(0);
                break;
            }
        }
        if (ret && tuple.front() != Symbol::createNum(0)) {
            std::ostringstream s;
            print_comma(s, tuple, ",");
            GRINGO_REPORT(log, Warnings::OperationUndefined)
                << loc << ": info: tuple ignored:\n"
                << "  " << s.str() << "\n";
        }
        return ret;
    }
    return true;
}

int toInt(IntervalSet<Symbol>::LBound const &x) {
    if (x.bound.type() == SymbolType::Num) {
        return x.inclusive ? x.bound.num() : x.bound.num() + 1;
    }
    if (x.bound < Symbol::createNum(0)) {
        return std::numeric_limits<int>::min();
    }
    return std::numeric_limits<int>::max();
}

int toInt(IntervalSet<Symbol>::RBound const &x) {
    if (x.bound.type() == SymbolType::Num) {
        return x.inclusive ? x.bound.num() : x.bound.num() - 1;
    }
    if (x.bound < Symbol::createNum(0)) {
        return std::numeric_limits<int>::min();
    }
    return std::numeric_limits<int>::max();
}

Symbol getWeight(AggregateFunction fun, SymVec const &x) {
    return fun == AggregateFunction::COUNT ? Symbol::createNum(1) : x.front();
}

Symbol getWeight(AggregateFunction fun, Potassco::Span<Symbol> rng) {
    return fun == AggregateFunction::COUNT ? Symbol::createNum(1) : *rng.first;
}

// {{{1 definition of AggregateAtomRange

void AggregateAtomRange::init(AggregateFunction fun, DisjunctiveBounds &&bounds) {
    switch (fun) {
        case AggregateFunction::MIN: {
            valMin() = Symbol::createSup();
            valMax() = Symbol::createSup();
            break;
        }
        case AggregateFunction::MAX: {
            valMin() = Symbol::createInf();
            valMax() = Symbol::createInf();
            break;
        }
        default: {
            intMin() = 0;
            intMax() = 0;
            break;
        }
    }
    this->fun = fun;
    this->bounds = std::move(bounds);
}

Interval AggregateAtomRange::range() const {
    if (fun != AggregateFunction::MIN && fun != AggregateFunction::MAX) {
        return {{Symbol::createNum(clamp(intMin())), true}, {Symbol::createNum(clamp(intMax())), true}};
    }
    return {{valMin(), true}, {valMax(), true}};
}

PlainBounds AggregateAtomRange::plainBounds() {
    // There are 3 cases:
    // (a) disjunctive bounds are open to the left: > or >=
    // (b) disjunctive bounds are open to the right: < or <=
    // (c) < and > follow each other: !=
    // Case (c) can furthermore correspond to one or two != occurrences.
    PlainBounds cBounds;
    auto rng = range();
    for (auto part : bounds) {
        if (rng.left < part.left) {
            // (a)
            if (cBounds.empty()) {
                cBounds.emplace_back(part.left.inclusive ? Relation::GEQ : Relation::GT, part.left.bound);
            }
            // (c)
            else {
                assert(cBounds.back().first == Relation::LT || cBounds.back().first == Relation::LEQ);
                Symbol left = cBounds.back().second;
                if (cBounds.back().first == Relation::LEQ) {
                    assert(left.type() == SymbolType::Num);
                    left = Symbol::createNum(left.num() + 1);
                }
                Symbol right = part.left.bound;
                if (part.left.inclusive) {
                    assert(right.type() == SymbolType::Num);
                    right = Symbol::createNum(right.num() - 1);
                }
                cBounds.back() = {Relation::NEQ, left};
                if (left != right) {
                    cBounds.back() = {Relation::NEQ, right};
                }
            }
        }
        if (part.right < rng.right) {
            // (b)
            cBounds.emplace_back(part.right.inclusive ? Relation::LEQ : Relation::LT, part.right.bound);
        }
    }
    assert(cBounds.size() < 3);
    return cBounds;

}

void AggregateAtomRange::accumulate(SymVec const &tuple, bool fact, bool remove) {
    switch (fun) {
        case AggregateFunction::MIN: {
            Symbol val = tuple.front();
            if (fact) {
                valMax() = std::min<Symbol>(valMax(), val);
            }
            valMin() = std::min<Symbol>(valMin(), val);
            break;
        }
        case AggregateFunction::MAX: {
            Symbol val = tuple.front();
            if (fact) {
                valMin() = std::max<Symbol>(valMin(), val);
            }
            valMax() = std::max<Symbol>(valMax(), val);
            break;
        }
        default: {
            int64_t val = fun == AggregateFunction::COUNT ? 1 : tuple.front().num();
            if (fact) {
                if (remove) {
                    if (val < 0) {
                        intMax() += val;
                    }
                    else {
                        intMin()+= val;
                    }
                }
                else {
                    intMin()+= val;
                    intMax()+= val;
                }
            }
            else {
                if (val < 0) {
                    intMin()+= val;
                }
                else {
                    intMax()+= val;
                }
            }
            break;
        }
    }
}

// {{{1 definition of BodyAggregateAtom

template <class F>
void BodyAggregateElements_::visitClause(F f) const {
    for (auto it = conditions_.begin(), ie = conditions_.end(); it != ie; ) {
        // get compressedd tuple offset (the size stores the fact bit)
        auto ct_offset = CompressedOffset{*it++};
        Id_t t_offset = ct_offset.offset();
        Id_t t_size_fact = ct_offset.has_size() ? ct_offset.size() : *it++;
        bool t_fact = (t_size_fact & 1) == 0;
        auto t_size = t_size_fact >> 1;
        // get compressed clause offset (empty conditions are not stored)
        Id_t c_offset = 0;
        Id_t c_size = 0;
        if (!t_fact) {
            auto cc_offset = CompressedOffset{*it++};
            c_offset = cc_offset.offset();
            c_size = cc_offset.has_size() ? (cc_offset.size() + 1) : *it++;
        }
        f(TupleId{t_offset, t_size}, ClauseId(c_offset, c_size));
    }
}

void BodyAggregateElements_::accumulate(DomainData &data, TupleId tuple, LitVec &lits, bool &inserted, bool &fact, bool &remove) {
    TupleOffset offset{tuple.offset, tuple.size, lits.empty()};
    auto ret = tuples_.insert(offset);
    inserted = ret.second;
    remove = false;
    if (!inserted) {
        auto old_offset = *ret.first;
        if (!old_offset.fact() && offset.fact()) {
            tuples_.erase(old_offset);
            tuples_.insert(offset);
            remove = true;
        }
        else {
            offset = old_offset;
        }
    }
    fact = offset.fact();
    if (!fact || inserted || remove) {
        auto clause = data.clause(std::move(lits));
        // add compressed tuple offset with fact bit to condition vector
        uint32_t t_size_fact = (offset.size() << 1) | (fact ? 0U : 1U);
        CompressedOffset ct_offset{offset.offset(), t_size_fact};
        conditions_.emplace_back(ct_offset.repr());
        if (!ct_offset.has_size()) {
            conditions_.emplace_back(t_size_fact);
        }
        if (!fact) {
            // add compressed clause offset to condition vector
            CompressedOffset cc_offset(clause.first, clause.second - 1);
            conditions_.emplace_back(cc_offset.repr());
            if (!cc_offset.has_size()) {
                conditions_.emplace_back(clause.second);
            }
        }
    }
}

BodyAggregateElements BodyAggregateElements_::elems() const {
    BodyAggregateElements elems;
    visitClause([&](TupleId tuple, ClauseId cond) {
        auto ret = elems.try_emplace(tuple);
        if (cond.second == 0) {
            ret.first.value().clear();
        }
        ret.first.value().emplace_back(cond);
    });
    return elems;
}

void BodyAggregateAtom::init(AggregateFunction fun, DisjunctiveBounds &&bounds, bool monotone) {
    data_->range.init(fun, std::move(bounds));
    data_->fact = data_->range.fact();
    data_->monotone = monotone;
    data_->initialized = true;
}

BodyAggregateElements BodyAggregateAtom::elems() const {
    return data_->elems.elems();
}

void BodyAggregateAtom::accumulate(DomainData &data, Location const &loc, SymVec const &tuple, LitVec &cond, Logger &log) {
    if (neutral(tuple, data_->range.fun, loc, log)) {
        return;
    }
    bool inserted{false};
    bool fact{false};
    bool remove{false};
    data_->elems.accumulate(data, data.tuple(tuple), cond, inserted, fact, remove);
    if (!fact || inserted || remove) {
        data_->range.accumulate(tuple, fact, remove);
        data_->fact = data_->range.fact();
    }
}

BodyAggregateAtom::~BodyAggregateAtom() noexcept = default;

// {{{1 definition of AssignmentAggregateAtom

void AssignmentAggregateData::accumulate(DomainData &data, Location const &loc, SymVec const &tuple, LitVec &cond, Logger &log) {
    if (neutral(tuple, fun_, loc, log)) {
        return;
    }
    auto ret(elems_.try_emplace(data.tuple(tuple)));
    auto &elem = ret.first.value();
    // the tuple was fact
    if (elem.size() == 1 && elem.front().second == 0) {
        return;
    }
    bool fact = false;
    bool remove = false;
    if (cond.empty()) {
        elem.clear();
        fact = true;
        remove = !ret.second;
    }
    elem.emplace_back(data.clause(cond));
    if (!ret.second && !remove) {
        return;
    }
    switch (fun_) {
        case AggregateFunction::MIN: {
            Symbol val = tuple.front();
            if (fact) {
                values_.erase(std::remove_if(values_.begin() + 1, values_.end(), [val](Symbol x) { return x >= val; }), values_.end());
                if (values_.front() > val) {
                    values_.front() = val;
                }
            }
            else if (values_.front() > val) {
                values_.push_back(val);
            }
            break;
        }
        case AggregateFunction::MAX: {
            Symbol val = tuple.front();
            if (fact) {
                values_.erase(std::remove_if(values_.begin() + 1, values_.end(), [val](Symbol x) { return x <= val; }), values_.end());
                if (values_.front() < val) {
                    values_.front() = val;
                }
            }
            else if (values_.front() < val) {
                values_.push_back(val);
            }
            break;
        }
        default: {
            Symbol val = fun_ == AggregateFunction::COUNT ? Symbol::createNum(1) : tuple.front();
            if (fact) {
                if (remove) {
                    values_.erase(std::find(values_.begin() + 1, values_.end(), val));
                }
                values_.front() = Symbol::createNum(values_.front().num() + val.num());
            }
            else {
                values_.push_back(val);
            }
            break;
        }
    }
}

AssignmentAggregateData::Values AssignmentAggregateData::values() const {
    switch (fun_) {
        case AggregateFunction::MIN:
        case AggregateFunction::MAX: {
            Values values = values_;
            sort_unique(values);
            return values;
        }
        default: {
            ordered_set<Symbol> values;
            auto it = values_.begin();
            values.insert(*it++);
            for (auto ie = values_.end(); it != ie; ++it) {
                for (Id_t jt = 0, je = values.size(); jt != je; ++jt) {
                    values.insert(Symbol::createNum(values.nth(jt)->num() + it->num()));
                }
            }
            // Note: a release function would be nice
            // NOLINTNEXTLINE
            return std::move(const_cast<Values&>(values.values_container()));
        }
    }
}

Interval AssignmentAggregateData::range() const {
    switch (fun_) {
        case AggregateFunction::MAX:
        case AggregateFunction::MIN: {
            Symbol valMin = values_.front();
            Symbol valMax = valMin;
            for (auto it = values_.begin() + 1, ie = values_.end(); it != ie; ++it) {
                valMin = std::min(valMin, *it);
                valMax = std::max(valMax, *it);
            }
            return Interval{{valMin,true},{valMax,true}};
        }
        default: {
            int64_t intMin = values_.front().num();
            int64_t intMax = intMin;
            for (auto it = values_.begin() + 1, ie = values_.end(); it != ie; ++it) {
                int val = it->num();
                if (val < 0) {
                    intMin+= val;
                }
                else {
                    intMax+= val;
                }
            }
            // NOTE: nonsense, proper handling is not achieved like this...
            return {{Symbol::createNum(clamp(intMin)), true}, {Symbol::createNum(clamp(intMax)), true}};
        }
    }
}

// {{{1 definition of ConjunctionAtom

// Example:
//   H :- p(G), q(G,X,Y) : r(G,X,Z).
//   global variables: G
//   local variables : X, Y, Z
//   value of the conjunction: c(G)
//   value of the element    : e(X)
//   disjunction of bodies is spanned by Z
//   disjunction of heads is spanned by Y

bool ConjunctionElement::isSimple(DomainData &data) const {
    return (heads_.empty() && bodies_.size() == 1 && bodies_.front().second == 1 && data.clause(bodies_.front()).first->invertible()) ||
           (bodies_.size() == 1 && bodies_.front().second == 0 && heads_.size() <= 1);
}

bool ConjunctionElement::needsSemicolon() const {
    return !bodies_.empty() && bodies_.front().second > 0;
}

void ConjunctionElement::print(PrintPlain out) const {
    using namespace std::placeholders;
    if (bodies_.empty()) {
        out << "#true";
    }
    else {
        if (heads_.empty()) {
            out << "#false";
        }
        else {
            auto pc = [](PrintPlain out, ClauseId id) {
                if (id.second == 0) {
                    out << "#true";
                }
                else {
                    print_comma(out, out.domain.clause(id), "&", printLit);
                }
            };
            print_comma(out, heads_, "|", pc);
        }
        if (bodies_.front().second > 0) {
            out << ":";
            auto pc = [](PrintPlain out, ClauseId id) {
                if (id.second == 0) {
                    out << "#true";
                }
                else {
                    print_comma(out, out.domain.clause(id), ",", printLit);
                }
            };
            print_comma(out, bodies_, "|", pc);
        }
    }
}
void ConjunctionElement::accumulateCond(DomainData &data, LitVec &cond, Id_t &blocked, Id_t &fact) {
    if (bodies_.empty()) {
         // there can only be a head if there is at least one body
        assert(heads_.empty());
        ++fact;
    }
    if (bodies_.size() != 1 || bodies_.front().second > 0) {
        if (cond.empty()) {
            bodies_.clear();
            if (heads_.empty()) {
                ++blocked;
            }
        }
        bodies_.emplace_back(data.clause(cond));
    }
}

void ConjunctionElement::accumulateHead(DomainData &data, LitVec &cond, Id_t &blocked, Id_t &fact) {
    // NOTE: returns true for newly satisfiable elements
    if (heads_.empty() && bodies_.size() == 1 && bodies_.front().second == 0) {
        assert(blocked > 0);
        --blocked;
    }
    if (heads_.size() != 1 || heads_.front().second > 0) {
        if (cond.empty()) {
            heads_.clear();
            assert(fact > 0);
            --fact;
        }
        heads_.emplace_back(data.clause(cond));
    }
}

void ConjunctionAtom::accumulateCond(DomainData &data, Symbol elem, LitVec &cond) {
    elems_.try_emplace(elem).first.value().accumulateCond(data, cond, blocked_, fact_);
}

void ConjunctionAtom::accumulateHead(DomainData &data, Symbol elem, LitVec &cond) {
    elems_.try_emplace(elem).first.value().accumulateHead(data, cond, blocked_, fact_);
}

bool ConjunctionAtom::recursive() const {
    return (headRecursive_ || condRecursive_) && !fact();
}

bool ConjunctionAtom::nonmonotone() const {
    return !fact() && condRecursive_;
}

void ConjunctionAtom::init(bool headRecursive, bool condRecursive) {
    headRecursive_ = headRecursive;
    condRecursive_ = condRecursive;
}

// {{{1 definition of DisjunctionAtom

// Example:
//   p(1..10,X) : q(X,Y,G) :- r(G).
//   global variables: G
//   local variables : X, Y
//   value of the disjunction: c(G)
//   value of the element    : e(X)
//   disjunction of bodies is spanned by Y
//   conjunction of heads is spanned by 1..10

void DisjunctionElement::print(PrintPlain out) const {
    using namespace std::placeholders;
    if (bodies_.empty()) {
        out << "#false";
    }
    else {
        if (heads_.empty()) {
            out << "#true";
        }
        else {
            auto pc = [](PrintPlain out, ClauseId id) {
                if (id.second == 0) {
                    out << "#false";
                }
                else {
                    print_comma(out, out.domain.clause(id), "|", printLit);
                }
            };
            print_comma(out, heads_, "&", pc);
        }
        if (bodies_.front().second > 0) {
            out << ":";
            auto pc = [](PrintPlain out, ClauseId id) {
                if (id.second == 0) {
                    out << "#true";
                }
                else {
                    print_comma(out, out.domain.clause(id), "&", printLit);
                }
            };
            print_comma(out, bodies_, "|", pc);
        }
    }
}

// Note: this are general functions on formulas
bool DisjunctionElement::bodyIsTrue() const {
    return bodies_.size() == 1 && bodies_.front().second == 0;
}

bool DisjunctionElement::headIsTrue() const {
    return heads_.empty();
}

bool DisjunctionElement::bodyIsFalse() const {
    return bodies_.empty();
}

bool DisjunctionElement::headIsFalse() const {
    return heads_.size() == 1 && heads_.front().second == 0;
}

void DisjunctionElement::accumulateCond(DomainData &data, LitVec &cond, Id_t &fact) {
    if (!bodyIsTrue()) {
        if (cond.empty()) {
            bodies_.clear();
            if (headIsTrue()) {
                ++fact;
            }
        }
        bodies_.emplace_back(data.clause(cond));
    }
}

void DisjunctionElement::accumulateHead(DomainData &data, LitVec &cond, Id_t &fact) {
    if (!headIsFalse()) {
        if (bodyIsTrue() && headIsTrue()) {
            --fact;
        }
        if (cond.empty()) {
            heads_.clear();
        }
        heads_.emplace_back(data.clause(cond));
    }
}

void DisjunctionAtom::simplify(bool &headFact) {
    headFact_ = 0;
    for (auto it = elems_.begin(); it != elems_.end();) {
        if (it.value().headIsTrue() && it.value().bodyIsTrue()) {
            ++headFact_;
        }
        if (it.value().bodyIsFalse() || it.value().headIsFalse()) {
            it = elems_.unordered_erase(it);
        }
        else {
            ++it;
        }
    }
    headFact = headFact_ > 0;
}

void DisjunctionAtom::accumulateCond(DomainData &data, Symbol elem, LitVec &cond) {
    elems_.try_emplace(elem).first.value().accumulateCond(data, cond, headFact_);
}

void DisjunctionAtom::accumulateHead(DomainData &data, Symbol elem, LitVec &cond) {
    elems_.try_emplace(elem).first.value().accumulateHead(data, cond, headFact_);
}

// {{{1 definition of TheoryAtom

void TheoryAtom::simplify(TheoryData const &data) {
    if (!simplified_) {
        // NOTE: tuples with non-factual conditions can be removed if there
        //       is an element with the same tuple and a factual condition
        std::sort(elems_.begin(), elems_.end());
        elems_.erase(std::unique(elems_.begin(), elems_.end()), elems_.end());
        elems_.shrink_to_fit();
        simplified_ = true;
    }
}

// {{{1 definition of HeadAggregateAtom

void HeadAggregateAtom::init(AggregateFunction fun, DisjunctiveBounds &&bounds) {
    range_.init(fun, std::move(bounds));
    fact_ = range_.fact();
    initialized_ = true;
}

void HeadAggregateAtom::accumulate(DomainData &data, Location const &loc, SymVec const &tuple, LiteralId head, LitVec &cond, Logger &log) {
    // Elements are grouped by their tuples.
    // Each tuple is associated with a vector of pairs of head literals and a condition.
    // If the head is a fact, this is represented with an invalid literal.
    // If a tuple is a fact, this is represented with the first element of the vector being having an invalid head and an empty condition.
    if (!Gringo::Output::defined(tuple, range_.fun, loc, log)) {
        return;
    }
    auto ret(elems_.try_emplace(data.tuple(tuple)));
    auto &elem = ret.first.value();
    bool fact = cond.empty() && !head.valid();
    bool wasFact = !elem.empty() && !elem.front().first.valid() && elem.front().second.second == 0;
    if (wasFact && fact) {
        return;
    }
    elem.emplace_back(head, data.clause(cond));
    bool remove = false;
    if (fact) {
        std::swap(elem.front(), elem.back());
        remove = !ret.second;
    }
    if ((!ret.second && !remove) || neutral(tuple, range_.fun, loc, log)) {
        return;
    }
    range_.accumulate(tuple, fact, remove);
    fact_ = range_.fact();
}

// }}}1

// {{{1 definition of PredicateDomain

std::pair<Id_t, Id_t> PredicateDomain::cleanup(AssignmentLookup assignment, Mapping &map) {
    Id_t facts = 0;
    Id_t deleted = 0;
    //std::cerr << "cleaning " << sig_ << std::endl;
    cleanup_([&](PredicateAtom &atom, Id_t oldOffset, Id_t newOffset) {
        if (!atom.defined()) {
            ++deleted;
            return true;
        }
        if (atom.hasUid()) {
            auto value = assignment(atom.uid());
            if (!value.first) {
                switch (value.second) {
                    case Potassco::Value_t::True: {
                        // NOTE: externals cannot become facts here
                        //       because they might get new definitions while grounding
                        //       because there is no distinction between true and weak true
                        //       these definitions might be skipped if a weak true external
                        //       is made a fact here
                        if (!atom.fact()) {
                            ++facts;
                        }
                        atom.setFact(true);
                        break;
                    }
                    case Potassco::Value_t::False: {
                        ++deleted;
                        return true;
                    }
                    default: {
                        break;
                    }
                }
            }
        }
        //std::cerr << "  mapping " << static_cast<Symbol>(atom) << " from " << oldOffset << " to " << newOffset << std::endl;
        atom.setGeneration(0);
        atom.unmarkDelayed();
        map.add(oldOffset, newOffset);
        return false;
    });
    //std::cerr << "remaining atoms: ";
    //for (auto &atom : atoms_) {
    //    std::cerr << "  " << static_cast<Symbol>(atom) << "=" << (atoms_.find(static_cast<Symbol>(atom)) != atoms_.end()) << "/" << atom.generation() << "/" << atom.defined() << "/" << atom.delayed() << std::endl;
    //}
    incOffset_ = size();
    showOffset_ = size();
    return {facts, deleted};
}


// }}}1

// {{{1 definition of AuxLiteral

bool AuxLiteral::isHeadAtom() const {
    return id_.sign() == NAF::POS;
}

LiteralId AuxLiteral::translate(Translator &x) {
    return id_.sign() != NAF::NOTNOT ? id_ : x.removeNotNot(data_, id_);
}

void AuxLiteral::printPlain(PrintPlain out) const {
    out << id_.sign() << (id_.domain() == 0 ? "#aux" : "#delayed") << "(" << id_.offset() << ")";
}

bool AuxLiteral::isIncomplete() const {
    return false;
}

int AuxLiteral::uid() const {
    switch (id_.sign()) {
        case NAF::POS: {
            return static_cast<Potassco::Lit_t>(id_.offset());
        }
        case NAF::NOT: {
            return -static_cast<Potassco::Lit_t>(id_.offset());
        }
        case NAF::NOTNOT: {
            throw std::logic_error("AuxLiteral::uid: translate must be called before!");
        }
    }
    throw std::logic_error("AuxLiteral::uid: must not happen");
}

LiteralId AuxLiteral::simplify(Mappings &mappings, AssignmentLookup const &lookup) const {
    auto value = lookup(id_.offset());
    if (value.second == Potassco::Value_t::Free) {
        return id_;
    }
    auto ret = data_.getTrueLit();
    if (value.second == Potassco::Value_t::False) {
        ret = ret.negate(false);
    }
    if (id_.sign() == NAF::NOT) {
        ret = ret.negate(false);
    }
    return id_;
}

bool AuxLiteral::isTrue(IsTrueLookup const &lookup) const {
    assert(id_.offset() > 0);
    return (id_.sign() == NAF::NOT) ^ lookup(id_.offset());
}

LiteralId AuxLiteral::toId() const {
    return id_;
}

// {{{1 definition of PredicateLiteral

bool PredicateLiteral::isHeadAtom() const {
    return id_.sign() == NAF::POS;
}

bool PredicateLiteral::isAtomFromPreviousStep() const {
    return id_.offset() < data_.predDom(id_.domain()).incOffset();
}

void PredicateLiteral::printPlain(PrintPlain out) const {
    auto &atom = data_.predDom(id_.domain())[id_.offset()];
    out << id_.sign() << static_cast<Symbol>(atom);
}

bool PredicateLiteral::isIncomplete() const {
    return false;
}

LiteralId PredicateLiteral::toId() const {
    return id_;
}

LiteralId PredicateLiteral::translate(Translator &x) {
    return id_.sign() != NAF::NOTNOT ? id_ : x.removeNotNot(data_, id_);
}

int PredicateLiteral::uid() const {
    auto &atom = data_.predDom(id_.domain())[id_.offset()];
    if (!atom.hasUid()) {
        atom.setUid(data_.newAtom());
    }
    switch (id_.sign()) {
        case NAF::POS: {
            return static_cast<Potassco::Lit_t>(atom.uid());
        }
        case NAF::NOT: {
            return -static_cast<Potassco::Lit_t>(atom.uid());
        }
        case NAF::NOTNOT: {
            throw std::logic_error("PredicateLiteral::uid: translate must be called before!");
        }
    }
    assert(false);
    return 0;
}

LiteralId PredicateLiteral::simplify(Mappings &mappings, AssignmentLookup const &lookup) const {
    auto offset = mappings[id_.domain()].get(id_.offset());
    if (offset == InvalidId) {
        auto ret = data_.getTrueLit();
        if (id_.sign() != NAF::NOT) {
            ret = ret.negate(false);
        }
        return ret;
    }
    auto &atom = data_.predDom(id_.domain())[offset];
    if (!atom.defined()) {
        return data_.getTrueLit().negate();
    }
    if (atom.hasUid()) {
        auto value = lookup(atom.uid());
        if (value.second != Potassco::Value_t::Free) {
            auto ret = data_.getTrueLit();
            if (value.second == Potassco::Value_t::False) {
                ret = ret.negate(false);
            }
            if (id_.sign() == NAF::NOT) {
                ret = ret.negate(false);
            }
            return ret;
        }
    }
    return id_.withOffset(offset);
}

bool PredicateLiteral::isTrue(IsTrueLookup const &lookup) const {
    auto &atom = data_.predDom(id_.domain())[id_.offset()];
    assert(atom.hasUid());
    return (id_.sign() == NAF::NOT) ^ lookup(atom.uid());
}

// {{{1 definition of TheoryLiteral

void TheoryLiteral::printPlain(PrintPlain out) const {
    auto &atm = data_.getAtom<TheoryDomain>(id_);
    if (atm.defined()) {
        atm.simplify(data_.theory());
        out << id_.sign();
        out << "&";
        out.printTerm(atm.name());
        out << "{";
        print_comma(out, atm.elems(), "; ", [](PrintPlain out, Potassco::Id_t elemId) { out.printElem(elemId); });
        out << "}";
        if (atm.hasGuard()) {
            out.printTerm(atm.op());
            out << "(";
            out.printTerm(atm.guard());
            out << ")";
        }
    }
    else {
        out << (id_.sign() == NAF::NOT ? "#true" : "#false");
    }
}

bool TheoryLiteral::isHeadAtom() const {
    auto &atm = data_.getAtom<TheoryDomain>(id_);
    return atm.defined() && atm.type() != TheoryAtomType::Body;
}

bool TheoryLiteral::isIncomplete() const {
    return data_.getAtom<TheoryDomain>(id_).recursive();
}

std::pair<LiteralId,bool> TheoryLiteral::delayedLit() {
    auto &atm = data_.getAtom<TheoryDomain>(id_);
    bool found = atm.lit();
    if (!found) {
        atm.setLit(data_.newDelayed());
    }
    return {atm.lit().withSign(id_.sign()), !found};
}

LiteralId TheoryLiteral::toId() const {
    return id_;
}

LiteralId TheoryLiteral::translate(Translator &x) {
    auto &atm = data_.getAtom<TheoryDomain>(id_);
    if (!atm.translated()) {
        atm.setTranslated();
        if (atm.defined()) {
            atm.simplify(data_.theory());
            for (auto const &elemId : atm.elems()) {
                data_.theory().updateCondition(elemId, [&x, this](LitVec &cond) {
                    Gringo::Output::translate(data_, x, cond);
                });
            }
            auto newAtom = [&]() -> Atom_t {
                if (atm.type() == TheoryAtomType::Directive) {
                    return 0;
                }
                if (!atm.lit()) {
                    atm.setLit(data_.newAux());
                }
                assert(atm.lit().type() == AtomType::Aux);
                return atm.lit().offset();
            };
            TheoryData &data = data_.theory();
            auto ret = atm.hasGuard()
                ? data.addAtom(newAtom, atm.name(), Potassco::toSpan(atm.elems()), atm.op(), atm.guard())
                : data.addAtom(newAtom, atm.name(), Potassco::toSpan(atm.elems()));
            if (ret.first.atom() != 0) {
                // assign the literal of the theory atom
                if (!atm.lit()) {
                    atm.setLit({NAF::POS, AtomType::Aux, ret.first.atom(), 0});
                }
                // connect the theory atom with an existing one
                else if (ret.first.atom() != atm.lit().offset()) {
                    LiteralId head = atm.lit();
                    LiteralId body{NAF::POS, AtomType::Aux, ret.first.atom(), 0};
                    if (atm.type() == TheoryAtomType::Head) {
                        std::swap(head, body);
                    }
                    Rule().addHead(head).addBody(body).translate(data_, x);
                }
            }
        }
        else {
            makeFalse(data_, x, id_.sign(), atm);
        }
    }
    auto lit = atm.lit();
    return !lit ? lit : x.removeNotNot(data_, lit.withSign(id_.sign()));
}

int TheoryLiteral::uid() const {
    throw std::logic_error("TheoryLiteral::uid: translate must be called before!");
}

// {{{1 definition of BodyAggregateLiteral

void BodyAggregateLiteral::printPlain(PrintPlain out) const {
    auto &dom = data_.getDom<BodyAggregateDomain>(id_.domain());
    auto &atm = dom[id_.offset()];
    if (atm.defined()) {
        auto bounds = atm.plainBounds();
        out << id_.sign();
        auto it = bounds.begin();
        auto ie = bounds.end();
        if (it != ie) {
            out << it->second << inv(it->first); ++it;
        }
        out << atm.fun() << "{";
        print_comma(out, atm.elems(), ";", printBodyElem);
        out << "}";
        for (; it != ie; ++it) {
            out << it->first << it->second;
        }
    }
    else {
        out << (id_.sign() == NAF::NOT ? "#true" : "#false");
    }
}

LiteralId BodyAggregateLiteral::toId() const {
    return id_;
}

LiteralId BodyAggregateLiteral::translate(Translator &x) {
    auto &dom = data_.getDom<BodyAggregateDomain>(id_.domain());
    auto &atm = dom[id_.offset()];
    if (!atm.translated()) {
        atm.setTranslated();
        if (atm.defined()) {
            auto aggrLit = getEqualAggregate(data_, x, atm.fun(), id_.sign(), atm.bounds(), atm.range(), atm.elems(), atm.recursive());
            auto lit = atm.lit();
            if (lit) {
                Rule().addHead(lit).addBody(aggrLit).translate(data_, x);
            }
            else {
                atm.setLit(aggrLit);
            }
        }
        else {
            makeFalse(data_, x, id_.sign(), atm);
        }
    }
    return atm.lit();
}
int BodyAggregateLiteral::uid() const {
    throw std::logic_error("BodyAggregateLiteral::uid must be called after BodyAggregateLiteral::translate");
}

bool BodyAggregateLiteral::isIncomplete() const {
    return data_.getAtom<BodyAggregateDomain>(id_.domain(), id_.offset()).recursive();
}

std::pair<LiteralId,bool> BodyAggregateLiteral::delayedLit() {
    auto &atm = data_.getAtom<BodyAggregateDomain>(id_.domain(), id_.offset());
    bool found = atm.lit();
    if (!found) {
        atm.setLit(data_.newDelayed());
    }
    return {atm.lit(), !found};
}

// {{{1 definition of AssignmentAggregateLiteral

AssignmentAggregateDomain &AssignmentAggregateLiteral::dom() const {
    return data_.getDom<AssignmentAggregateDomain>(id_.domain());
}

void AssignmentAggregateLiteral::printPlain(PrintPlain out) const {
    auto &dom = this->dom();
    auto &atm = dom[id_.offset()];
    auto &data = dom.data(atm.data()).value();
    Symbol repr = atm;
    out << id_.sign();
    out << data.fun() << "{";
    print_comma(out, data.elems(), ";", printBodyElem);
    auto const &args = repr.args();
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    out << "}=" << args.first[args.size - 1];
}

LiteralId AssignmentAggregateLiteral::toId() const {
    return id_;
}

LiteralId AssignmentAggregateLiteral::translate(Translator &x) {
    auto &dom = this->dom();
    auto &atm = dom[id_.offset()];
    auto &data = dom.data(atm.data()).value();
    if (!atm.translated()) {
        atm.setTranslated();
        assert(atm.defined());
        // NOTE: for assignment aggregates with many values a better translation could be implemented
        Symbol repr = atm;
        DisjunctiveBounds bounds;
        auto args = repr.args();
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        auto back = args.first[args.size - 1];
        bounds.add(back, true, back, true);
        auto aggrLit = getEqualAggregate(data_, x, data.fun(), id_.sign(), bounds, data.range(), data.elems(), atm.recursive());
        auto lit = atm.lit();
        if (lit) {
            Rule().addHead(lit).addBody(aggrLit).translate(data_, x);
        }
        else {
            atm.setLit(aggrLit);
        }
    }
    return atm.lit();
}
int AssignmentAggregateLiteral::uid() const {
    throw std::logic_error("AssignmentAggregateLiteral::uid must be called after AssignmentAggregateLiteral::translate");
}

bool AssignmentAggregateLiteral::isIncomplete() const {
    return dom()[id_.offset()].recursive();
}

std::pair<LiteralId,bool> AssignmentAggregateLiteral::delayedLit() {
    auto &atm = data_.getAtom<AssignmentAggregateDomain>(id_.domain(), id_.offset());
    bool found = atm.lit();
    if (!found) {
        atm.setLit(data_.newDelayed());
    }
    return {atm.lit(), !found};
}

// {{{1 definition of DisjunctionLiteral

LiteralId DisjunctionLiteral::toId() const {
    return id_;
}

void DisjunctionLiteral::printPlain(PrintPlain out) const {
    auto &atm = dom()[id_.offset()];
    if (!atm.elems().empty()) {
        print_comma(out, atm.elems(), ";",
                    [](PrintPlain &out, std::pair<Symbol, DisjunctionElement> const &x) { x.second.print(out); });
    }
    else {
        out << "#false";
    }
}

LiteralId DisjunctionLiteral::translate(Translator &x) {
    auto &atm = dom()[id_.offset()];
    if (!atm.translated()) {
        atm.setTranslated();
        if (!atm.lit()) {
            atm.setLit(data_.newAux());
        }
        bool headFact{false};
        atm.simplify(headFact);
        if (headFact) {
            return atm.lit();
        }
        Rule dj;
        dj.addBody(atm.lit());
        for (auto const &item : atm.elems()) {
            auto const &elem = item.second;
            assert (!elem.bodies().empty());
            LiteralId cond; // cond :- elem.bodies()
            if (!elem.bodyIsTrue()) {
                cond = getEqualFormula(data_, x, elem.bodies(), false, false);
            }
            if (elem.heads().empty()) {
                assert(cond);
                // head is a fact
                dj.addBody(cond.negate());
            }
            else if (elem.heads().size() == 1) {
                auto headClause = data_.clause(elem.heads().front());
                if (cond) {
                    LiteralId headAtom = data_.newAux();
                    for (auto const &lit : headClause) {
                        Rule().addHead(headAtom).addBody(lit).addBody(cond).translate(data_, x);
                    }
                    Rule().addHead(headClause).addBody(cond).addBody(headAtom).translate(data_, x);
                    Rule().addBody(cond.negate()).addBody(headAtom).translate(data_, x);
                    dj.addHead(headAtom);
                }
                else {
                    // the head literals can be pushed directly into djHead
                    dj.addHead(*headClause.first);
                }
            }
            else {
                LiteralId conjunctionAtom = data_.newAux();
                Rule conjunction;
                for (auto const &headId : elem.heads()) {
                    auto head = data_.clause(headId);
                    if (head.size == 1) {
                        Rule().addHead(*head.first).addBody(conjunctionAtom).translate(data_, x);
                        conjunction.addBody(*head.first);
                    }
                    else {
                        LiteralId disjunctionAtom = data_.newAux();
                        Rule().addHead(head).addBody(disjunctionAtom).translate(data_, x);
                        for (auto const &lit : head) {
                            Rule().addHead(disjunctionAtom).addBody(lit).translate(data_, x);
                        }
                        conjunction.addBody(disjunctionAtom);
                    }
                }
                conjunction.addHead(conjunctionAtom).translate(data_, x);
                if (cond) {
                    LiteralId headAtom = data_.newAux();
                    Rule().addHead(headAtom).addBody(conjunctionAtom).addBody(cond).translate(data_, x);
                    Rule().addHead(conjunctionAtom).addBody(headAtom).addBody(cond).translate(data_, x);
                    Rule().addBody(cond.negate()).addBody(headAtom).translate(data_, x);
                    dj.addHead(headAtom);
                }
                else {
                    dj.addHead(conjunctionAtom);
                }
            }
        }
        dj.translate(data_, x);
    }
    return atm.lit();
}

Lit_t DisjunctionLiteral::uid() const {
    throw std::logic_error("DisjunctionLiteral::uid: must be called before DisjunctionLiteral::translate");
}

bool DisjunctionLiteral::isIncomplete() const {
    return dom()[id_.offset()].recursive();
}

DisjunctionDomain &DisjunctionLiteral::dom() const {
    return data_.getDom<DisjunctionDomain>(id_.domain());
}

std::pair<LiteralId,bool> DisjunctionLiteral::delayedLit() {
    auto &atm = data_.getAtom<DisjunctionDomain>(id_.domain(), id_.offset());
    bool found = atm.lit();
    if (!found) {
        atm.setLit(data_.newDelayed());
    }
    return {atm.lit(), !found};
}

// {{{1 definition of ConjunctionLiteral

LiteralId ConjunctionLiteral::toId() const {
    return id_;
}

void ConjunctionLiteral::printPlain(PrintPlain out) const {
    auto &atm = dom()[id_.offset()];
    if (!atm.elems().empty()) {
        int sep = 0;
        for (auto const &x : atm.elems()) {
            switch (sep) {
                case 1: {
                    out << ",";
                    break;
                }
                case 2: {
                    out << ";";
                    break;
                }
            }
            out << x.second;
            sep = x.second.needsSemicolon() ? 2 : 1;
        }
    }
    else {
        out << "#true";
    }
}

LiteralId ConjunctionLiteral::translate(Translator &x) {
    auto &atm = dom()[id_.offset()];
    if (!atm.translated()) {
        atm.setTranslated();
        LitVec bd;
        for (auto const &sym_lit : atm.elems()) {
            auto const &lit = sym_lit.second;
            if ((lit.heads().size() == 1 && lit.heads().front().second == 0) || lit.bodies().empty()) {
                // this part of the conditional literal is a fact
                continue;
            }
            if (lit.isSimple(data_)) {
                if (lit.bodies().size() == 1 && lit.bodies().front().second == 0) {
                    assert(lit.heads().size() <= 1);
                    if (lit.heads().empty()) {
                        if (!atm.lit()) {
                            atm.setLit(data_.newAux());
                        }
                        return atm.lit();
                    }
                    bd.emplace_back(*data_.clause(lit.heads().front()).first);
                }
                else {
                    bd.emplace_back(data_.clause(lit.bodies().front()).first->negate());
                }
            }
            else {
                LiteralId aux = data_.newAux();
                LiteralId auxHead = data_.newAux();
                for (auto const &head : lit.heads()) {
                    // auxHead :- lit.head.
                    Rule().addHead(auxHead).addBody(data_.clause(head)).translate(data_, x);
                    Rule().addHead(aux).addBody(auxHead).translate(data_, x);
                }
                // chk <=> body.
                LiteralId chk = getEqualFormula(data_, x, lit.bodies(), false, atm.nonmonotone() && !lit.heads().empty());
                Rule().addHead(aux).addBody(chk.negate()).translate(data_, x);
                if (atm.nonmonotone() && !lit.heads().empty()) {
                    // aux | chk | ~x.first.
                    Rule().addHead(aux).addHead(chk).addHead(auxHead.negate()).translate(data_, x);
                }
                // body += aux
                bd.emplace_back(aux);
            }
        }
        // bdLit :- body.
        if (bd.size() == 1 && !atm.lit()) {
            bd.front() = call(data_, bd.front(), &Literal::translate, x);
            atm.setLit(bd.front());
        }
        else {
            if (!atm.lit()) {
                atm.setLit(data_.newAux());
            }
            Rule().addHead(atm.lit()).addBody(bd).translate(data_, x);
        }
    }
    return atm.lit();
}

Lit_t ConjunctionLiteral::uid() const {
    throw std::logic_error("ConjunctionLiteral::uid: must be called before ConjunctionLiteral::translate");
}

bool ConjunctionLiteral::isIncomplete() const {
    return dom()[id_.offset()].recursive();
}

ConjunctionDomain &ConjunctionLiteral::dom() const {
    return data_.getDom<ConjunctionDomain>(id_.domain());
}

std::pair<LiteralId,bool> ConjunctionLiteral::delayedLit() {
    auto &atm = data_.getAtom<ConjunctionDomain>(id_.domain(), id_.offset());
    bool found = atm.lit();
    if (!found) {
        atm.setLit(data_.newDelayed());
    }
    return {atm.lit(), !found};
}

bool ConjunctionLiteral::needsSemicolon() const {
    auto &atm = dom()[id_.offset()];
    return !atm.elems().empty() && atm.elems().back().second.needsSemicolon();
}

// {{{1 definition of HeadAggregateLiteral

LiteralId HeadAggregateLiteral::toId() const {
    return id_;
}

LiteralId HeadAggregateLiteral::translate(Translator &x) {
    auto &atm = dom()[id_.offset()];
    if (!atm.translated()) {
        atm.setTranslated();
        if (!atm.lit()) {
            atm.setLit(data_.newAux());
        }
        auto range(atm.range());
        if (!atm.satisfiable()) {
            Rule().addBody(atm.lit()).translate(data_, x);
            return atm.lit();
        }
        using GroupedByCond = std::vector<std::pair<ClauseId, std::pair<TupleId, LiteralId>>>;
        GroupedByCond groupedByCond;
        for (auto const &y : atm.elems()) {
            for (auto const &z : y.second) {
                groupedByCond.emplace_back(z.second, std::make_pair(y.first, z.first));
            }
        }
        sort_unique(groupedByCond);
        BodyAggregateElements bdElems;
        for (auto it = groupedByCond.begin(), ie = groupedByCond.end(); it != ie; ) {
            auto condId = it->first;
            LiteralId condLit;
            if (condId.second > 0) {
                // c :- C.
                condLit = getEqualClause(data_, x, condId, true, false);
            }
            Rule choice(true);
            do {
                // { heads } :- c, b.
                LiteralId head = it->second.second;
                if (head) {
                    choice.addHead(head);
                }
                // e :- h, c.
                auto bdElem = bdElems.try_emplace(it->second.first);
                LitVec cond;
                if (condLit) {
                    cond.emplace_back(condLit);
                }
                if (head) {
                    cond.emplace_back(head);
                }
                bdElem.first.value().emplace_back(data_.clause(cond));
                ++it;
            }
            while (it != ie && it->first == condId);
            if (choice.numHeads() > 0) {
                choice.addBody(atm.lit());
                if (condLit) {
                    choice.addBody(condLit);
                }
                choice.translate(data_, x);
            }
        }
        // :- b, not aggr.
        if (!atm.bounds().contains(range)) {
            LiteralId aggr = getEqualAggregate(data_, x, atm.fun(), NAF::NOT, atm.bounds(), range, bdElems, false);
            Rule check;
            check.addBody(atm.lit());
            check.addBody(aggr).translate(data_, x);
        }
    }
    return atm.lit();
}

void HeadAggregateLiteral::printPlain(PrintPlain out) const {
    auto &atm = dom()[id_.offset()];
    auto bounds = atm.plainBounds();
    out << id_.sign();
    auto it = bounds.begin();
    auto ie = bounds.end();
    if (it != ie) {
        out << it->second << inv(it->first);
        ++it;
    }
    out << atm.fun() << "{";
    print_comma(out, atm.elems(), ";", printHeadElem);
    out << "}";
    for (; it != ie; ++it) {
        out << it->first << it->second;
    }
}

bool HeadAggregateLiteral::isIncomplete() const {
    return true;
}

HeadAggregateDomain &HeadAggregateLiteral::dom() const {
    return data_.getDom<HeadAggregateDomain>(id_.domain());
}

int HeadAggregateLiteral::uid() const {
    throw std::logic_error("HeadAggregateLiteral::uid must be called after BodyAggregateLiteral::translate");
}

std::pair<LiteralId,bool> HeadAggregateLiteral::delayedLit() {
    auto &atm = data_.getAtom<HeadAggregateDomain>(id_.domain(), id_.offset());
    bool found = atm.lit();
    if (!found) {
        atm.setLit(data_.newDelayed());
    }
    return {atm.lit(), !found};
}

// {{{1 definition of DomainData

TheoryTermType DomainData::termType(Id_t value) const {
    auto const &term = theory_.data().getTerm(value);
    switch (term.type()) {
        case Potassco::Theory_t::Number: {
            return TheoryTermType::Number;
        }
        case Potassco::Theory_t::Symbol: {
            return TheoryTermType::Symbol;
        }
        case Potassco::Theory_t::Compound: {
            if (term.isFunction()) {
                return TheoryTermType::Function;
            }
            switch (term.tuple()) {
                case Potassco::Tuple_t::Paren: {
                    return TheoryTermType::Tuple;
                }
                case Potassco::Tuple_t::Bracket: {
                    return TheoryTermType::List;
                }
                case Potassco::Tuple_t::Brace: {
                    return TheoryTermType::Set;
                }
            }
            return TheoryTermType::Number;
        }
    }
    throw std::logic_error("must not happen");
}

int DomainData::termNum(Id_t value) const {
    return theory_.data().getTerm(value).number();
}

char const *DomainData::termName(Id_t value) const {
    if (theory_.data().getTerm(value).isFunction()) {
        return theory_.data().getTerm(theory_.data().getTerm(value).function()).symbol();
    }
    return theory_.data().getTerm(value).symbol();
}

Potassco::IdSpan DomainData::termArgs(Id_t value) const {
    return theory_.data().getTerm(value).terms();
}

Potassco::IdSpan DomainData::elemTuple(Id_t value) const {
    return theory_.data().getElement(value).terms();
}

Potassco::LitSpan DomainData::elemCond(Id_t value) const {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
    auto &data = const_cast<DomainData&>(*this);
    data.tempLits_.clear();
    for (auto const &lit : theory_.getCondition(value)) {
        data.tempLits_.emplace_back(call(data, lit, &Literal::uid));
    }
    return Potassco::toSpan(data.tempLits_);
}

Potassco::Lit_t DomainData::elemCondLit(Id_t value) const {
    return static_cast<Potassco::Lit_t>(theory_.data().getElement(value).condition());
}

Potassco::IdSpan DomainData::atomElems(Id_t value) const {
    return theory_.getAtom(value).elements();
}

Potassco::Id_t DomainData::atomTerm(Id_t value) const {
    return theory_.getAtom(value).term();
}

bool DomainData::atomHasGuard(Id_t value) const {
    return theory_.getAtom(value).guard() != nullptr;
}

Potassco::Lit_t DomainData::atomLit(Id_t value) const {
    return static_cast<Potassco::Lit_t>(theory_.getAtom(value).atom());
}

std::pair<char const *, Id_t> DomainData::atomGuard(Id_t value) const {
    return {termName(*theory_.getAtom(value).guard()), *theory_.getAtom(value).rhs()};
}

Potassco::Id_t DomainData::numAtoms() const {
    return theory_.data().numAtoms();
}

std::string DomainData::termStr(Id_t value) const {
    std::ostringstream oss;
    theory_.printTerm(oss, value);
    return oss.str();
}

std::string DomainData::elemStr(Id_t value) const {
    std::ostringstream oss;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
    auto &data = const_cast<DomainData&>(*this);
    theory_.printElem(oss, value, [&data](std::ostream &out, LiteralId lit) { call(data, lit, &Literal::printPlain, PrintPlain{data, out}); });
    return oss.str();
}

std::string DomainData::atomStr(Id_t value) const {
    std::ostringstream oss;
    oss << "&";
    auto const &atom = theory_.getAtom(value);
    theory_.printTerm(oss, atom.term());
    oss << "{";
    bool comma = false;
    for (auto const &elem : atom.elements()) {
        if (comma) {
            oss << ";";
        }
        else {
            comma = true;
        }
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
        auto &data = const_cast<DomainData&>(*this);
        theory_.printElem(oss, elem, [&data](std::ostream &out, LiteralId lit) { call(data, lit, &Literal::printPlain, PrintPlain{data, out}); });
    }
    oss << "}";
    if (atom.guard() != nullptr) {
        theory_.printTerm(oss, *atom.guard());
        theory_.printTerm(oss, *atom.rhs());
    }
    return oss.str();
}

// }}}1

} } // namespace Output Gringo
