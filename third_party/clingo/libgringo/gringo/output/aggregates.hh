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

#ifndef GRINGO_OUTPUT_AGGREGATES_HH
#define GRINGO_OUTPUT_AGGREGATES_HH

#include <gringo/terms.hh>
#include <gringo/domain.hh>
#include <gringo/intervals.hh>
#include <gringo/output/literal.hh>

namespace Gringo { namespace Output {

using BodyAggregateElements = ordered_map<TupleId, Formula>;
using HeadFormula = std::vector<std::pair<LiteralId, ClauseId>>;
using HeadAggregateElements = ordered_map<TupleId, HeadFormula>;
using DisjunctiveBounds = IntervalSet<Symbol>;
using Interval = DisjunctiveBounds::Interval;
using ConjunctiveBounds = std::vector<std::pair<Interval, Interval>>;
using PlainBounds = std::vector<std::pair<Relation, Symbol>>;
using LitValVec = std::vector<std::pair<LiteralId, Symbol>>;
using LitUintVec = std::vector<std::pair<LiteralId, unsigned>>;

class AggregateAnalyzer {
public:
    enum Monotonicity { MONOTONE, ANTIMONOTONE, CONVEX, NONMONOTONE };
    enum WeightType { MIXED, POSITIVE, NEGATIVE };
    enum Truth { True, False, Open };
    using ConjunctiveBounds = std::vector<std::pair<Interval, Interval>>;

    AggregateAnalyzer(DomainData &data, NAF naf, DisjunctiveBounds const &disjunctiveBounds, AggregateFunction fun, Interval range, BodyAggregateElements const &elems);
    void print(std::ostream &out);
    LitValVec translateElems(DomainData &data, Translator &x, AggregateFunction fun, BodyAggregateElements const &bdElems, bool incomplete) const;

    Monotonicity monotonicity;
    WeightType weightType;
    Truth truth;
    ConjunctiveBounds bounds;
    Interval range;
};

inline Symbol getNeutral(AggregateFunction fun) {
    switch (fun) {
        case AggregateFunction::COUNT:
        case AggregateFunction::SUMP:
        case AggregateFunction::SUM: {
            return Symbol::createNum(0);
        }
        case AggregateFunction::MIN: {
            return Symbol::createSup();
        }
        case AggregateFunction::MAX: {
            return Symbol::createInf();
        }
    }
    assert(false);
    return {};
}

LiteralId getEqualClause(DomainData &data, Translator &x, ClauseId clause, bool conjunctive, bool equivalence);
LiteralId getEqualFormula(DomainData &data, Translator &x, Formula const &formula, bool conjunctive, bool equivalence);
LiteralId getEqualAggregate(DomainData &data, Translator &x, AggregateFunction fun, NAF naf, DisjunctiveBounds const &bounds, Interval const &range, BodyAggregateElements const &bdElems, bool recursive);

} } // namespace Output Gringo

#endif // GRINGO_OUTPUT_AGGREGATES_HH
