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

#ifndef GRINGO_BASE_HH
#define GRINGO_BASE_HH

#include <cassert>
#include <gringo/term.hh>
#include <deque>

namespace Gringo {

// {{{1 declaration of Context

class Context {
public:
    Context() = default;
    Context(Context const &other) = default;
    Context(Context &&other) noexcept = default;
    Context &operator=(Context const &other) = default;
    Context &operator=(Context &&other) noexcept = default;
    virtual ~Context() noexcept = default;

    virtual bool callable(String name) = 0;
    virtual SymVec call(Location const &loc, String name, SymSpan args, Logger &log) = 0;
    virtual void exec(String type, Location loc, String code) = 0;
};

// {{{1 declaration of TheoryAtomType

enum class TheoryAtomType { Head, Body, Any, Directive };
std::ostream &operator<<(std::ostream &out, TheoryAtomType type);

// {{{1 declaration of TheoryOperatorType

enum class TheoryOperatorType { Unary, BinaryLeft, BinaryRight };
std::ostream &operator<<(std::ostream &out, TheoryOperatorType type);

// {{{1 declaration of NAF

enum class NAF { POS = 0, NOT = 1, NOTNOT = 2 };
std::ostream &operator<<(std::ostream &out, NAF naf);
NAF inv(NAF naf, bool recursive = true);

enum class RECNAF { POS, NOT, NOTNOT, RECNOT };
RECNAF recnaf(NAF naf, bool recursive);
std::ostream &operator<<(std::ostream &out, RECNAF naf);

// {{{1 declaration of Relation

enum class Relation : unsigned { GT, LT, LEQ, GEQ, NEQ, EQ };

Relation inv(Relation rel);
Relation neg(Relation rel);

std::ostream &operator<<(std::ostream &out, Relation rel);

// {{{1 declaration of AggregateFunction

enum class AggregateFunction { COUNT, SUM, SUMP, MIN, MAX };

std::ostream &operator<<(std::ostream &out, AggregateFunction fun);

// {{{1 declaration of Bound

struct Bound;
using BoundVec = std::vector<Bound>;

struct Bound {
    Bound(Relation rel, UTerm &&bound);
    Bound(Bound const &other) = delete;
    Bound(Bound &&other) noexcept = default;
    Bound &operator=(Bound const &other) = delete;
    Bound &operator=(Bound &&other) noexcept = default;
    ~Bound() noexcept = default;

    size_t hash() const;
    bool operator==(Bound const &other) const;
    //! Unpool the terms in the bound.
    BoundVec unpool();
    //! Simplify the terms in the bound.
    //! \pre Must be called after unpool.
    bool simplify(SimplifyState &state, Logger &log);
    //! Rewrite arithmetics.
    //! \pre Must be called after assignLevels.
    void rewriteArithmetics(Term::ArithmeticsMap &arith, AuxGen &auxGen);

    Relation rel;
    UTerm bound;
};

// }}}1

// {{{1 definition of TheoryAtomType

inline std::ostream &operator<<(std::ostream &out, TheoryAtomType type) {
    switch (type) {
        case TheoryAtomType::Head:      { out << "head"; break; }
        case TheoryAtomType::Body:      { out << "body"; break; }
        case TheoryAtomType::Any:       { out << "any"; break; }
        case TheoryAtomType::Directive: { out << "directive"; break; }
    }
    return out;
}

// {{{1 definition of TheoryOperatorType

inline std::ostream &operator<<(std::ostream &out, TheoryOperatorType type) {
    switch (type) {
        case TheoryOperatorType::Unary:       { out << "unary"; break; }
        case TheoryOperatorType::BinaryLeft:  { out << "binary,left"; break; }
        case TheoryOperatorType::BinaryRight: { out << "binary,right"; break; }
    }
    return out;
}

// {{{1 definition of NAF

inline NAF inv(NAF naf, bool recursive) {
    switch (naf) {
        case NAF::NOTNOT: { return NAF::NOT; }
        case NAF::NOT:    { return recursive ? NAF::NOTNOT : NAF::POS; }
        case NAF::POS:    { return NAF::NOT; }
    }
    assert(false);
    return NAF::POS;
}

inline std::ostream &operator<<(std::ostream &out, NAF naf) {
    switch (naf) {
        case NAF::NOTNOT: { out << "not "; } // NOLINT
        case NAF::NOT:    { out << "not "; }
        case NAF::POS:    { }
    }
    return out;
}

inline std::ostream &operator<<(std::ostream &out, RECNAF naf) {
    switch (naf) {
        case RECNAF::NOTNOT: { out << "not "; } // NOLINT
        case RECNAF::RECNOT:
        case RECNAF::NOT:    { out << "not "; }
        case RECNAF::POS:    { }
    }
    return out;
}

inline RECNAF recnaf(NAF naf, bool recursive) {
    switch (naf) {
        case NAF::POS:    { return RECNAF::POS; }
        case NAF::NOT:    { return recursive ? RECNAF::RECNOT : RECNAF::NOT; }
        case NAF::NOTNOT: { return recursive ? RECNAF::NOTNOT : RECNAF::POS; }
    }
    return RECNAF::POS;
}

// {{{1 definition of Relation

inline std::ostream &operator<<(std::ostream &out, Relation rel) {
    switch (rel) {
        case Relation::GT:     { out << ">"; break; }
        case Relation::LT:     { out << "<"; break; }
        case Relation::LEQ:    { out << "<="; break; }
        case Relation::GEQ:    { out << ">="; break; }
        case Relation::NEQ:    { out << "!="; break; }
        case Relation::EQ: { out << "="; break; }
    }
    return out;
}

inline Relation inv(Relation rel) {
    switch (rel) {
        case Relation::NEQ:    { return Relation::NEQ; }
        case Relation::GEQ:    { return Relation::LEQ; }
        case Relation::LEQ:    { return Relation::GEQ; }
        case Relation::EQ: { return Relation::EQ; }
        case Relation::LT:     { return Relation::GT; }
        case Relation::GT:     { return Relation::LT; }
    }
    assert(false);
    return Relation(-1);
}

inline Relation neg(Relation rel) {
    switch (rel) {
        case Relation::NEQ:    { return Relation::EQ; }
        case Relation::GEQ:    { return Relation::LT; }
        case Relation::LEQ:    { return Relation::GT; }
        case Relation::EQ: { return Relation::NEQ; }
        case Relation::LT:     { return Relation::GEQ; }
        case Relation::GT:     { return Relation::LEQ; }
    }
    assert(false);
    return Relation(-1);
}

// {{{1 definition of AggregateFunction

inline std::ostream &operator<<(std::ostream &out, AggregateFunction fun) {
    switch (fun) {
        case AggregateFunction::MIN:   { out << "#min"; break; }
        case AggregateFunction::MAX:   { out << "#max"; break; }
        case AggregateFunction::SUM:   { out << "#sum"; break; }
        case AggregateFunction::SUMP:  { out << "#sum+"; break; }
        case AggregateFunction::COUNT: { out << "#count"; break; }
    }
    return out;
}

// {{{1 definition of Bound

inline Bound::Bound(Relation rel, UTerm &&bound)
    : rel(rel)
    , bound(std::move(bound)) { }

inline size_t Bound::hash() const {
    return get_value_hash(size_t(rel), bound);
}

inline bool Bound::operator==(Bound const &other) const {
    return rel == other.rel && is_value_equal_to(bound, other.bound);
}

inline BoundVec Bound::unpool() {
    BoundVec pool;
    auto f = [&](UTerm &&x) { pool.emplace_back(rel, std::move(x)); };
    Term::unpool(bound, Gringo::unpool, f);
    return pool;
}

inline bool Bound::simplify(SimplifyState &state, Logger &log) {
    return !bound->simplify(state, false, false, log).update(bound, false).undefined();
}

inline void Bound::rewriteArithmetics(Term::ArithmeticsMap &arith, AuxGen &auxGen) {
    // NOTE: this replaces all arithmetics to ensure that all terms in bounds are defined when evaluating
    Term::replace(bound, bound->rewriteArithmetics(arith, auxGen, true));
}

template <>
struct clone<Bound> {
    Bound operator()(Bound const &bound) const {
        return {bound.rel, get_clone(bound.bound)};
    }
};

// }}}1

} // namespace Gringo

namespace std {

// {{{1 definition of hash<Gringo::Bound>

template <>
struct hash<Gringo::Bound> {
    size_t operator()(Gringo::Bound const &x) const { return x.hash(); }
};

// {{{1 definition of hash<Gringo::Relation>

template <>
struct hash<Gringo::Relation> {
    size_t operator()(Gringo::Relation const &x) const { return static_cast<unsigned>(x); }
};

// {{{1 definition of hash<Gringo::NAF>

template <>
struct hash<Gringo::NAF> {
    size_t operator()(Gringo::NAF const &x) const { return static_cast<unsigned>(x); }
};

// }}}1

} // namespace std

#endif // GRINGO_BASE_HH
