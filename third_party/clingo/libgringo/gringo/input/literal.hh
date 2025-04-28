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

#ifndef GRINGO_INPUT_LITERAL_HH
#define GRINGO_INPUT_LITERAL_HH

#include <gringo/utility.hh>
#include <gringo/term.hh>
#include <gringo/domain.hh>
#include <gringo/input/types.hh>
#include <gringo/ground/literal.hh>

namespace Gringo { namespace Input {

using Output::PredicateDomain;
using Output::PredDomMap;
using Output::DomainData;

// {{{ declaration of Literal

class Projections {
public:
    using ProjectionMap = ordered_map<UTerm, std::pair<UTerm, bool>, mix_value_hash<UTerm>, value_equal_to<UTerm>>; // NOLINT

    Projections() = default;
    Projections(Projections const &other) = delete;
    Projections(Projections &&other) noexcept = default;
    Projections &operator=(Projections const &other) = delete;
    Projections &operator=(Projections &&other) noexcept = default;
    ~Projections() noexcept = default;

    ProjectionMap::iterator begin();
    ProjectionMap::iterator end();

    UTerm add(Term &term);

    ProjectionMap proj;
};

class Literal;
using ULit    = std::unique_ptr<Literal>;
using ULitVec = std::vector<ULit>;
using ULitVecVec = std::vector<ULitVec>;

class Literal : public Printable, public Hashable, public Locatable, public Comparable<Literal>, public Clonable<Literal> {
public:
    using RelationVec = std::vector<std::tuple<Relation, UTerm, UTerm>>;

    Literal() = default;
    Literal(Literal const &other) = default;
    Literal(Literal && other) noexcept = default;
    Literal &operator=(Literal const &other) = default;
    Literal &operator=(Literal &&other) noexcept = default;
    ~Literal() noexcept override = default;

    //! Add inequalities bounding variables to the given solver.
    virtual void addToSolver(IESolver &solver, bool invert) const;
    virtual unsigned projectScore() const { return 2; }
    //! Returns true if the literal needs to be shifted before unpooling to
    //! support counting in aggregates.
    virtual bool needSetShift() const { return false; }
    //! Return true if the literal is trivially satisfied.
    virtual bool triviallyTrue() const { return false; }
    //! Removes all occurrences of PoolTerm instances.
    //! Returns all unpooled incarnations of the literal.
    //! \note The literal becomes unusable after the method returns.
    //! \post The returned pool does not contain PoolTerm instances.
    virtual ULitVec unpool(bool head) const = 0;
    //! Check if the literal has occurrences of pool terms.
    virtual bool hasPool(bool head) const = 0;
    //! Unpool a comparision.
    //!
    //! Comparisons with more than one relation have to be unpooled after the
    //! simplification process to not duplicate pools. A comparison of form
    //! `A < B < C` is unpooled into a conjunction `A < B & B < C` and a
    //! comparison of form `not A < B < C` is unpooleld into a disjunction
    //! `A >= B | B >= C`.
    virtual ULitVecVec unpoolComparison() const;
    //! Check if there is a comparison that needs unpooling.
    virtual bool hasUnpoolComparison() const;
    //! Simplifies the literal.
    //!
    //! Flag positional=true indicates that anonymous variables in this literal can be projected.
    //! Flag singleton=true disables projections for positive predicate literals (e.g., in non-monotone aggregates)
    virtual bool simplify(Logger &log, Projections &project, SimplifyState &state, bool positional = true, bool singleton = false) = 0;
    //! Collects variables.
    //! \pre Must be called after simplify to properly account for bound variables.
    virtual void collect(VarTermBoundVec &vars, bool bound) const = 0;
    //! Removes non-invertible arithmetics.
    //! \note This method will not be called for head literals.
    virtual void rewriteArithmetics(Term::ArithmeticsMap &arith, RelationVec &assign, AuxGen &auxGen) = 0;
    //! Convert the literal into a tuple to be used in a set based aggregate.
    //!
    //! Symbolic literals use `(naf, repr)` where 0-2 is used to encode the
    //! negation and repr is the symbolic representation of the atom.
    //!
    //! Comparisons use `(3, uid, vars...)` where uid is the position where the
    //! comparision occurs in the arregate and vars are the variables that
    //! occur in the comparison. This ensures something like a multi-set
    //! semantics for comparisions.
    virtual void toTuple(UTermVec &tuple, int &id) const = 0;
    virtual Symbol isEDB() const;
    virtual void replace(Defines &dx) = 0;
    virtual Ground::ULit toGround(DomainData &x, bool auxiliary) const = 0;
    virtual ULit shift(bool negate) = 0;
    virtual UTerm headRepr() const = 0;
    virtual bool auxiliary() const = 0;
    virtual void auxiliary(bool auxiliary) = 0;
};

// }}}

} } // namespace Input Gringo

GRINGO_HASH(Gringo::Input::Literal)

#endif // GRINGO_INPUT_LITERAL_HH
