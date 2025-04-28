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

#ifndef _GRINGO_TEST_AGGREGATE_HELPER_HH
#define _GRINGO_TEST_AGGREGATE_HELPER_HH

#include "tests/tests.hh"
#include "gringo/input/aggregates.hh"

namespace Gringo { namespace Input { namespace Test {

using namespace Gringo::Test;

// {{{ definition of functions to create aggregates

template <class... T>
BoundVec boundvec(T&&... args) {
    return init<2, BoundVec>(std::forward<T>(args)...);
}

template <class... T>
CondLitVec condlitvec(T&&... args) {
    return init<2,CondLitVec>(std::forward<T>(args)...);
}

template <class... T>
BodyAggrElemVec bdelemvec(T&&... args) {
    return init<2,BodyAggrElemVec>(std::forward<T>(args)...);
}

template <class... T>
HeadAggrElemVec hdelemvec(T&&... args) {
    return init<3,HeadAggrElemVec>(std::forward<T>(args)...);
}

inline UBodyAggr bdaggr(NAF naf, AggregateFunction fun, BoundVec &&bounds, BodyAggrElemVec &&elems) {
    Location loc(String("dummy"), 1, 1, String("dummy"), 1, 1);
    return make_locatable<TupleBodyAggregate>(loc, naf, fun, std::move(bounds), std::move(elems));
}

inline UBodyAggr bdaggr(NAF naf, AggregateFunction fun, BoundVec &&bounds, CondLitVec &&elems) {
    Location loc(String("dummy"), 1, 1, String("dummy"), 1, 1);
    return make_locatable<LitBodyAggregate>(loc, naf, fun, std::move(bounds), std::move(elems));
}

inline UBodyAggr bdaggr(ULit &&lit, ULitVec &&cond) {
    Location loc(String("dummy"), 1, 1, String("dummy"), 1, 1);
    return make_locatable<Conjunction>(loc, std::move(lit), std::move(cond));
}

inline UHeadAggr hdaggr(AggregateFunction fun, BoundVec &&bounds, HeadAggrElemVec &&elems) {
    Location loc(String("dummy"), 1, 1, String("dummy"), 1, 1);
    return make_locatable<TupleHeadAggregate>(loc, fun, std::move(bounds), std::move(elems));
}

inline UHeadAggr hdaggr(AggregateFunction fun, BoundVec &&bounds, CondLitVec &&elems) {
    Location loc(String("dummy"), 1, 1, String("dummy"), 1, 1);
    return make_locatable<LitHeadAggregate>(loc, fun, std::move(bounds), std::move(elems));
}

inline UHeadAggr hdaggr(CondLitVec &&elems) {
    Location loc(String("dummy"), 1, 1, String("dummy"), 1, 1);
    return make_locatable<Disjunction>(loc, std::move(elems));
}

// }}}

} } } // namespace Test Input Gringo

#endif // _GRINGO_TEST_AGGREGATE_HELPER_HH


