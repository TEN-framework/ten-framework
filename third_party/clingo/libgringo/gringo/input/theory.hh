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

#ifndef GRINGO_INPUT_THEORY_HH
#define GRINGO_INPUT_THEORY_HH

#include <gringo/terms.hh>
#include <gringo/output/theory.hh>
#include <gringo/input/aggregate.hh>

namespace Gringo {

namespace Ground {

// TODO: these could be avoided by putting the ground::theory into a separate header.
class TheoryAccumulate;
class TheoryComplete;

} // namespace Ground

namespace Input {

// {{{1 declaration of TheoryElement

class TheoryElement;
using TheoryElementVec = std::vector<TheoryElement>;

class TheoryElement {
public:
    TheoryElement(Output::UTheoryTermVec &&tuple, ULitVec &&cond);
    TheoryElement(TheoryElement const &other) = delete;
    TheoryElement(TheoryElement &&other) noexcept = default;
    TheoryElement &operator=(TheoryElement const &other) = delete;
    TheoryElement &operator=(TheoryElement &&other) noexcept = default;
    ~TheoryElement() noexcept = default;
    TheoryElement clone() const;
    void print(std::ostream &out) const;
    bool operator==(TheoryElement const &other) const;
    size_t hash() const;
    bool hasPool() const;
    void unpool(TheoryElementVec &elems);
    bool hasUnpoolComparison() const;
    TheoryElementVec unpoolComparison();
    void replace(Defines &x);
    void collect(VarTermBoundVec &vars) const;
    void assignLevels(AssignLevel &lvl);
    void check(Location const &loc, Printable const &p, ChkLvlVec &levels, Logger &log) const;
    bool simplify(Projections &project, SimplifyState &state, Logger &log);
    void rewriteArithmetics(Term::ArithmeticsMap &arith, AuxGen &auxGen);
    void initTheory(Output::TheoryParser &p, Logger &log);
    std::unique_ptr<Ground::TheoryAccumulate> toGround(ToGroundArg &x, Ground::TheoryComplete &completeRef, Ground::ULitVec &&lits) const;

private:
    Output::UTheoryTermVec tuple_;
    ULitVec cond_;
};
inline std::ostream &operator<<(std::ostream &out, TheoryElement const &elem) { elem.print(out); return out; }

// {{{1 declaration of TheoryAtom

class TheoryAtom {
public:
    // Note: name must be a term that (after unpooling) has a signature
    TheoryAtom(UTerm &&name, TheoryElementVec &&elems);
    TheoryAtom(UTerm &&name, TheoryElementVec &&elems, String op, Output::UTheoryTerm &&guard, TheoryAtomType type = TheoryAtomType::Any);
    TheoryAtom(TheoryAtom const &other) = delete;
    TheoryAtom(TheoryAtom &&other) noexcept = default;
    TheoryAtom &operator=(TheoryAtom const &other) = delete;
    TheoryAtom &operator=(TheoryAtom &&other) noexcept = default;
    ~TheoryAtom() noexcept = default;

    TheoryAtom clone() const;
    bool operator==(TheoryAtom const &other) const;
    void print(std::ostream &out) const;
    bool hasGuard() const;
    size_t hash() const;
    bool hasPool() const;
    template <class T>
    void unpool(T f);
    bool hasUnpoolComparison() const;
    void unpoolComparison();
    void replace(Defines &x);
    void collect(VarTermBoundVec &vars) const;
    void assignLevels(AssignLevel &lvl);
    void check(Location const &loc, Printable const &p, ChkLvlVec &levels, Logger &log) const;
    bool simplify(Projections &project, SimplifyState &state, Logger &log);
    void rewriteArithmetics(Term::ArithmeticsMap &arith, AuxGen &auxGen);
    void initTheory(Location const &loc, TheoryDefs &def, bool inBody, bool hasBody, Logger &log);
    TheoryAtomType type() const { return type_; }
    CreateBody toGroundBody(ToGroundArg &x, Ground::UStmVec &stms, NAF naf, UTerm &&id) const;
    static CreateHead toGroundHead() ;

private:
    UTerm               name_;
    TheoryElementVec    elems_;
    String              op_;
    Output::UTheoryTerm guard_;
    TheoryAtomType      type_ = TheoryAtomType::Any;
};
inline std::ostream &operator<<(std::ostream &out, TheoryAtom const &atom) { atom.print(out); return out; }

// {{{1 declaration of BodyTheoryLiteral

class BodyTheoryLiteral : public BodyAggregate {
public:
    BodyTheoryLiteral(NAF naf, TheoryAtom &&atom, bool rewritten = false);
    BodyTheoryLiteral(BodyTheoryLiteral const &other) = delete;
    BodyTheoryLiteral(BodyTheoryLiteral &&other) noexcept = default;
    BodyTheoryLiteral &operator=(BodyTheoryLiteral const &other) = delete;
    BodyTheoryLiteral &operator=(BodyTheoryLiteral &&other) noexcept = default;
    ~BodyTheoryLiteral() noexcept override = default;
    // {{{2 BodyAggregate interface
    void unpool(UBodyAggrVec &x) override;
    bool hasUnpoolComparison() const override;
    UBodyAggrVecVec unpoolComparison() const override;
    bool simplify(Projections &project, SimplifyState &state, bool singleton, Logger &log) override;
    void assignLevels(AssignLevel &lvl) override;
    void check(ChkLvlVec &lvl, Logger &log) const override;
    void rewriteArithmetics(Term::ArithmeticsMap &arith, Literal::RelationVec &assign, AuxGen &auxGen) override;
    bool rewriteAggregates(UBodyAggrVec &aggr) override;
    void removeAssignment() override;
    bool isAssignment() const override;
    void collect(VarTermBoundVec &vars) const override;
    bool hasPool() const override;
    void replace(Defines &dx) override;
    CreateBody toGround(ToGroundArg &x, Ground::UStmVec &stms) const override;
    void initTheory(TheoryDefs &def, Logger &log) override;
    // {{{2 Hashable
    size_t hash() const override;
    // {{{2 Printable
    void print(std::ostream &out) const override;
    // {{{2 Clonable
    BodyTheoryLiteral *clone() const override;
    // {{{2 Comparable interface
    bool operator==(BodyAggregate const &other) const override;
    // }}}2

private:
    TheoryAtom atom_;
    NAF naf_;
    bool rewritten_;
};

// }}}1

// {{{1 declaration of HeadTheoryLiteral

class HeadTheoryLiteral : public HeadAggregate {
public:
    HeadTheoryLiteral(TheoryAtom &&atom, bool rewritten = false);
    HeadTheoryLiteral(HeadTheoryLiteral const &other) = delete;
    HeadTheoryLiteral(HeadTheoryLiteral &&other) noexcept = default;
    HeadTheoryLiteral &operator=(HeadTheoryLiteral const &other) = delete;
    HeadTheoryLiteral &operator=(HeadTheoryLiteral &&other) noexcept = default;
    ~HeadTheoryLiteral() noexcept override = default;
    // {{{2 HeadAggregate interface
    CreateHead toGround(ToGroundArg &x, Ground::UStmVec &stms) const override;
    UHeadAggr rewriteAggregates(UBodyAggrVec &aggr) override;
    void collect(VarTermBoundVec &vars) const override;
    bool hasPool() const override;
    void unpool(UHeadAggrVec &x) override;
    UHeadAggr unpoolComparison(UBodyAggrVec &body) override;
    bool simplify(Projections &project, SimplifyState &state, Logger &log) override;
    void assignLevels(AssignLevel &lvl) override;
    void rewriteArithmetics(Term::ArithmeticsMap &arith, AuxGen &auxGen) override;
    void check(ChkLvlVec &lvl, Logger &log) const override;
    void replace(Defines &x) override;
    void initTheory(TheoryDefs &def, bool hasBody, Logger &log) override;
    // {{{2 Hashable
    size_t hash() const override;
    // {{{2 Printable
    void print(std::ostream &out) const override;
    // {{{2 Clonable
    // precondition: must not be called after rewriteAggregates
    HeadTheoryLiteral *clone() const override;
    // {{{2 Comparable interface
    bool operator==(HeadAggregate const &other) const override;
    // }}}2

private:
    TheoryAtom atom_;
    bool rewritten_;
};

} } // namespace Gringo Input

GRINGO_CALL_HASH(Gringo::Input::TheoryAtom)
GRINGO_CALL_HASH(Gringo::Input::TheoryElement)
GRINGO_CALL_CLONE(Gringo::Input::TheoryElement)
GRINGO_CALL_CLONE(Gringo::Input::TheoryAtom)

#endif // GRINGO_INPUT_THEORY_HH
