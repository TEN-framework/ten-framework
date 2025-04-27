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

#include <gringo/output/aggregates.hh>
#include <gringo/output/statements.hh>

namespace Gringo { namespace Output {

namespace {
}

// {{{1 definition of AggregateAnalyzer

void AggregateAnalyzer::print(std::ostream &out) {
    auto printInterval = [&](Interval const &x) {
        out << (x.left.inclusive ? "[" : "(");
        out << x.left.bound;
        out << ",";
        out << x.right.bound;
        out << (x.right.inclusive ? "]" : ")");
    };
    out << "analyze result: " << std::endl;
    out << "  range: ";
    printInterval(range);
    std::cerr << std::endl;
    std::cerr << "  bounds:" << std::endl;
    for (auto &x : bounds) {
        out << "    ";
        printInterval(x.first);
        out << " ";
        printInterval(x.second);
        out << std::endl;
    }
    out << "  monotonicity: ";
    switch (monotonicity) {
        case Monotonicity::MONOTONE: {
            out << "monotone";
            break;
        }
        case Monotonicity::ANTIMONOTONE: {
            out << "antimonotone";
            break;
        }
        case Monotonicity::CONVEX: {
            out << "convex";
            break;
        }
        case Monotonicity::NONMONOTONE: {
            out << "nonmonotone";
            break;
        }
    }
    out << std::endl;
    out << "  weights: ";
    switch (weightType) {
        case WeightType::POSITIVE: {
            out << "positive";
            break;
        }
        case WeightType::NEGATIVE: {
            out << "negative";
            break;
        }
        case WeightType::MIXED: {
            out << "mixed";
            break;
        }
    }
    out << std::endl;
    out << "  truth: ";
    switch (truth) {
        case Truth::True: {
            out << "true";
            break;
        }
        case Truth::Open: {
            out << "open";
            break;
        }
        case Truth::False: {
            out << "false";
            break;
        }
    }
    out << std::endl;
}

AggregateAnalyzer::AggregateAnalyzer(DomainData &data, NAF naf, DisjunctiveBounds const &disjunctiveBounds, AggregateFunction fun, Interval range, BodyAggregateElements const &elems)
: weightType{POSITIVE}
, range(range) {
    // NOTE: considers everything that is fixed in a reduct as ANTIMONOTONE
    IntervalSet<Symbol> numBounds;
    for (auto const &y : disjunctiveBounds) {
        Interval x = y;
        if (!x.right.inclusive && x.right.bound.type() == SymbolType::Num) {
            x.right.inclusive = true;
            x.right.bound = Symbol::createNum(x.right.bound.num() - 1);
        }
        if (!x.left.inclusive && x.left.bound.type() == SymbolType::Num) {
            x.left.inclusive = true;
            x.left.bound = Symbol::createNum(x.left.bound.num() + 1);
        }
        numBounds.add(x);
    }
    IntervalSet<Symbol> complement(IntervalSet<Symbol>(range).difference(numBounds));
    bool nonMonotone = false;
    for (auto const &y : complement) {
        Interval x = y;
        if (x.right.inclusive && x.right.bound.type() == SymbolType::Num) {
            x.right.inclusive = false;
            x.right.bound = Symbol::createNum(x.right.bound.num() + 1);
        }
        if (x.left.inclusive && x.left.bound.type() == SymbolType::Num) {
            x.left.inclusive = false;
            x.left.bound = Symbol::createNum(x.left.bound.num() - 1);
        }
        IntervalSet<Symbol>::Interval a;
        IntervalSet<Symbol>::Interval b;
        a.left  = range.left;
        a.right = x.left;
        b.left  = x.right;
        b.right = range.right;
        if (a.empty() && b.empty()) {
            truth        = False;
            monotonicity = ANTIMONOTONE;
            bounds.clear();
            bounds.emplace_back(a, b);
            return;
        }
        bounds.emplace_back(a, b);
        if (!a.empty() && !b.empty()) {
            nonMonotone = true;
        }
    }
    if (bounds.empty()) {
        truth        = True;
        monotonicity = MONOTONE;
    }
    else {
        truth = Open;
        bool hasPositive       = false;
        bool hasPositiveWeight = false;
        bool hasNegativeWeight = false;
        for (auto const &x : elems) {
            bool hasPositiveLiteral = false;
            for (auto const &y : x.second) {
                for (auto const &z : data.clause(y)) {
                    if (z.sign() == NAF::POS) {
                        hasPositive        = true;
                        hasPositiveLiteral = true;
                        break;
                    }
                }
                auto tuple = data.tuple(x.first);
                if (hasPositiveLiteral && tuple.size > 0 && tuple.first->type() == SymbolType::Num) {
                    if (tuple.first->num() < 0) {
                        hasNegativeWeight = true;
                    }
                    if (tuple.first->num() > 0) {
                        hasPositiveWeight = true;
                    }
                }
            }
        }
        if (fun == AggregateFunction::SUM) {
            if (hasNegativeWeight && hasPositiveWeight) {
                weightType = MIXED;
                nonMonotone = true;
            }
            if (hasNegativeWeight && !hasPositiveWeight) {
                weightType = NEGATIVE;
            }
        }
        if (naf != NAF::POS || !hasPositive)  {
            monotonicity = ANTIMONOTONE;
        }
        else if (nonMonotone) {
            monotonicity = NONMONOTONE;
        }
        else if (bounds.size() == 1) {
            bool flip = fun == AggregateFunction::MIN || (fun == AggregateFunction::SUM && weightType == NEGATIVE);
            if (bounds.front().first.empty()) {
                flip = !flip;
            }
            monotonicity = flip ? MONOTONE : ANTIMONOTONE;;
        }
        else {
            monotonicity = CONVEX;
        }
    }
}

LitValVec AggregateAnalyzer::translateElems(DomainData &data, Translator &x, AggregateFunction fun, BodyAggregateElements const &bdElems, bool incomplete) const {
    LitValVec elems;
    for (auto const &y : bdElems) {
        Symbol weight(getWeight(fun, data.tuple(y.first)));
        LiteralId lit = getEqualFormula(data, x, y.second, false, monotonicity == AggregateAnalyzer::NONMONOTONE && incomplete);
        elems.emplace_back(lit, weight);
    }
    return elems;
}

// {{{1 translation of min/max aggregates

namespace {

LiteralId translateMinMax(DomainData &data, Translator &x, AggregateAnalyzer &res, bool isMin, LitValVec &&elems, bool incomplete) {
    // NOTE: passing the elems vec as a list of weighted formulas
    //       could be exploited to add fewer disjunctions than translateElems adds at the moment
    //       (same goes for sum aggregates)
    LitVec conjunction;
    for (auto &bound : res.bounds) {
        // translation overview (min):
        // |--A--|  B  |--C--|
        // (B => A)
        // note that B is true if C is empty!
        // if not incomplete or not nonmonotone:
        // (~B | A)
        bool hasAntecedent = isMin ? !bound.second.empty() : !bound.first.empty();
        LitVec antecedent;
        LitVec consequent;
        for (auto &elem : elems) {
            if (res.range.contains(elem.second)) {
                if (bound.first.contains(elem.second)) {
                    if (isMin) {
                        consequent.emplace_back(elem.first);
                    }
                }
                else if (bound.second.contains(elem.second)) {
                    if (!isMin) {
                        consequent.emplace_back(elem.first);
                    }
                }
                else {
                    if (hasAntecedent) {
                        antecedent.emplace_back(elem.first);
                    }
                }
            }
        }
        if (hasAntecedent) {
            assert(!antecedent.empty());
            if (consequent.empty()) {
                for (auto &lit : antecedent) {
                    conjunction.emplace_back(lit.negate());
                }
            }
            else {
                if (incomplete && res.monotonicity == AggregateAnalyzer::NONMONOTONE) {
                    // (a | b | c => x | y | z) => q
                    // is equivalent to
                    //   a | b | c <=> a1.
                    //   x | y | z  => a2.
                    //     a2 => q
                    //    ~a1 => q
                    //   ~~a2 => a1 | q
                    LiteralId q  = data.newAux();
                    LiteralId a1 = getEqualClause(data, x, data.clause(std::move(antecedent)), false, true);
                    LiteralId a2 = getEqualClause(data, x, data.clause(std::move(consequent)), false, false);
                    Rule().addHead(q).addBody(a2).translate(data, x);
                    Rule().addHead(q).addBody(a1.negate()).translate(data, x);
                    Rule().addHead(q).addHead(a1).addBody(a2.negate().negate()).translate(data, x);
                    conjunction.emplace_back(q);
                }
                else {
                    LiteralId aux = data.newAux();
                    for (auto &lit : consequent) {
                        Rule().addHead(aux).addBody(lit).translate(data, x);
                    }
                    Rule negated;
                    for (auto &lit : antecedent) {
                        negated.addBody(lit.negate());
                    }
                    negated.addHead(aux).translate(data, x);
                    conjunction.emplace_back(aux);
                }
            }
        }
        else {
            assert(!consequent.empty());
            conjunction.emplace_back(getEqualClause(data, x, data.clause(std::move(consequent)), false, false));
        }
    }
    LiteralId ret = getEqualClause(data, x, data.clause(std::move(conjunction)), true, false);
    return call(data, ret, &Literal::translate, x);
}

} // namespace

// {{{1 translation of sum aggregates

namespace {

class SumTranslator {
public:
    void addLiteral(DomainData &data, LiteralId const &lit, Potassco::Weight_t weight, bool recursive) {
        if (weight > 0) {
            if (!recursive || lit.invertible() || call(data, lit, &Literal::isAtomFromPreviousStep)) {
                litsPosStrat_.emplace_back(get_clone(lit), weight);
            }
            else {
                litsPosRec_.emplace_back(get_clone(lit), weight);
            }
        }
        else if (weight < 0) {
            if (!recursive || lit.invertible() || call(data, lit, &Literal::isAtomFromPreviousStep)) {
                litsNegStrat_.emplace_back(get_clone(lit), -weight);
            }
            else {
                litsNegRec_.emplace_back(get_clone(lit), -weight);
            }
        }
    }
    LiteralId translate(DomainData &data, Translator &x, ConjunctiveBounds &bounds, bool convex, bool invert) const {
        LitVec clause;
        for (auto &bound : bounds) {
            assert(!bound.first.empty() || !bound.second.empty());
            LiteralId pos;
            LiteralId neg;
            if (!bound.second.empty()) {
                if (invert && convex) {
                    if (!neg) {
                        neg = data.newAux();
                    }
                    Potassco::Weight_t lower = 1 - bound.second.left.bound.num();
                    translate(data, x, neg, lower, litsNegRec_, litsPosRec_, litsNegStrat_, litsPosStrat_);
                }
                else {
                    if (!pos) {
                        pos = data.newAux();
                    }
                    Potassco::Weight_t lower = bound.second.left.bound.num();
                    translate(data, x, pos, lower, litsPosRec_, litsNegRec_, litsPosStrat_, litsNegStrat_);
                }
            }
            if (!bound.first.empty()) {
                if (!invert && convex) {
                    if (!neg) {
                        neg = data.newAux();
                    }
                    Potassco::Weight_t lower = 1 + bound.first.right.bound.num();
                    translate(data, x, neg,  lower, litsPosRec_, litsNegRec_, litsPosStrat_, litsNegStrat_);
                }
                else {
                    if (!pos) {
                        pos = data.newAux();
                    }
                    Potassco::Weight_t lower = -bound.first.right.bound.num();
                    translate(data, x, pos, lower, litsNegRec_, litsPosRec_, litsNegStrat_, litsPosStrat_);
                }
            }
            LitVec disjunction;
            if (pos) {
                disjunction.emplace_back(pos);
            }
            if (neg) {
                disjunction.emplace_back(neg.negate());
            }
            clause.emplace_back(getEqualClause(data, x, data.clause(std::move(disjunction)), false, false));
        }
        auto ret = getEqualClause(data, x, data.clause(std::move(clause)), true, false);
        return call(data, ret, &Literal::translate, x);
    }

private:
    static void translate(DomainData &data, Translator &x, LiteralId const &head, Potassco::Weight_t bound, LitUintVec const &litsPosRec, LitUintVec const &litsNegRec, LitUintVec const &litsPosStrat, LitUintVec const &litsNegStrat) {
        LitUintVec elems;
        for (auto const &wLit : litsPosRec)   {
            elems.emplace_back(get_clone(wLit.first), wLit.second);
        }
        for (auto const &wLit : litsPosStrat) {
            elems.emplace_back(get_clone(wLit.first), wLit.second);
        }
        for (auto const &wLit : litsNegStrat) {
            bound += static_cast<Potassco::Weight_t>(wLit.second);
            elems.emplace_back(wLit.first.negate(), wLit.second);
        }
        for (auto const &wLit : litsNegRec) {
            bound += static_cast<Potassco::Weight_t>(wLit.second);
            LiteralId aux = data.newAux();
            elems.emplace_back(get_clone(aux), wLit.second);
            Rule().addHead(aux).addBody(wLit.first.negate()).translate(data, x);
            Rule().addHead(aux).addBody(head).translate(data, x);
            Rule().addHead(aux).addHead(wLit.first).addHead(head.negate()).translate(data, x);
        }
        WeightRule(head, bound, std::move(elems)).translate(data, x);
    }

    LitUintVec litsPosRec_;
    LitUintVec litsNegRec_;
    LitUintVec litsPosStrat_;
    LitUintVec litsNegStrat_;
};

} // namespace

// {{{1 definition of translation functions

namespace {

LiteralId getEqualClause(DomainData &data, Translator &x, LitSpan clause, bool conjunctive, bool equivalence) {
    if (clause.size == 0) {
        return conjunctive ? data.getTrueLit() : data.getTrueLit().negate();
    }
    if (clause.size == 1) {
        LiteralId lit = *clause.first;
        if (equivalence && call(data, *clause.first, &Literal::isAtomFromPreviousStep)) {
            lit = lit.negate();
            lit = lit.negate();
        }
        return *clause.first;
    }
    LiteralId aux = data.newAux();
    if (conjunctive) {
        if (equivalence) {
            for (auto const &lit : clause) {
                Rule().addHead(lit).addBody(aux).negatePrevious(data).translate(data, x);
            }
        }
        Rule().addHead(aux).addBody(clause).translate(data, x);
    }
    else {
        for (auto const &lit : clause) {
            Rule().addHead(aux).addBody(lit).translate(data, x);
        }
        if (equivalence) {
            Rule().addHead(clause).addBody(aux).negatePrevious(data).translate(data, x);
        }
    }
    return aux;
}

} // namespace

LiteralId getEqualClause(DomainData &data, Translator &x, std::pair<Id_t, Id_t> clause, bool conjunctive, bool equivalence) {
    if (clause.second > 1) {
        auto ret = x.clause(clause, conjunctive, equivalence);
        if (!ret) {
            ret = getEqualClause(data, x, data.clause(clause), conjunctive, equivalence);
            x.clause(ret, clause, conjunctive, equivalence);
        }
        return ret;
    }
    return getEqualClause(data, x, data.clause(clause), conjunctive, equivalence);
}

LiteralId getEqualFormula(DomainData &data, Translator &x, Formula const &formula, bool conjunctive, bool equivalence) {
    LitVec disjunction;
    for (auto const &conjunction : formula) {
        disjunction.emplace_back(getEqualClause(data, x, data.clause(conjunction), !conjunctive, equivalence));
    }
    return getEqualClause(data, x, data.clause(std::move(disjunction)), conjunctive, equivalence);
}

LiteralId getEqualAggregate(DomainData &data, Translator &x, AggregateFunction fun, NAF naf, DisjunctiveBounds const &bounds, Interval const &range, BodyAggregateElements const &bdElems, bool recursive) {
    AggregateAnalyzer res(data, naf, bounds, fun, range, bdElems);
    //res.print(std::cerr);
    LiteralId aux;
    if (res.truth == AggregateAnalyzer::True) {
        aux = data.getTrueLit();
    }
    else if (res.truth == AggregateAnalyzer::False) {
        aux = data.getTrueLit().negate();
    }
    else {
        recursive = res.monotonicity == AggregateAnalyzer::NONMONOTONE && recursive;
        LitValVec elems = res.translateElems(data, x, fun, bdElems, recursive);
        if (elems.size() == 1) {
            bool hasElem  = bounds.contains({{elems.front().second, true}, {elems.front().second, true}});
            bool hasEmpty = bounds.contains({{getNeutral(fun), true}, {getNeutral(fun), true}});
            if (hasElem && hasEmpty) {
                aux = data.getTrueLit();
            }
            else if (hasElem) {
                aux = elems.front().first;
            }
            else if (hasEmpty) {
                aux = elems.front().first.negate();
            }
            else {
                aux = data.getTrueLit().negate();
            }
        }
        else {
            switch (fun) {
                case AggregateFunction::COUNT:
                case AggregateFunction::SUMP:
                case AggregateFunction::SUM: {
                    SumTranslator trans;
                    for (auto &elem : elems) {
                        trans.addLiteral(data, elem.first, elem.second.num(), recursive);
                    }
                    aux = trans.translate(data, x, res.bounds, !recursive, res.weightType == AggregateAnalyzer::NEGATIVE);
                    break;
                }
                case AggregateFunction::MIN:
                case AggregateFunction::MAX: {
                    aux = translateMinMax(data, x, res, fun == AggregateFunction::MIN, std::move(elems), recursive);
                    break;
                }
            }
        }
    }
    switch (naf) {
        // NOLINTNEXTLINE(bugprone-branch-clone)
        case NAF::NOTNOT:
            aux = aux.negate();
        case NAF::NOT:
            aux = aux.negate();
        default:
            return call(data, aux, &Literal::translate, x);
    }
}

// }}}1

} } // namespace Output Gringo
