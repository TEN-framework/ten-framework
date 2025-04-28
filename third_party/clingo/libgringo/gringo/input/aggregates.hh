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

#ifndef GRINGO_INPUT_AGGREGATES_HH
#define GRINGO_INPUT_AGGREGATES_HH

#include <gringo/input/aggregate.hh>
#include <gringo/terms.hh>
#include <ostream>

namespace Gringo { namespace Input {

// {{{1 declaration of BodyAggrElem

class BodyAggrElem : public IEContext {
public:
    BodyAggrElem(UTermVec tuple, ULitVec condition)
    : tuple_{std::move(tuple)}
    , condition_{std::move(condition)} { }

    bool hasPool() const;
    void unpool(BodyAggrElemVec &pool);
    bool hasUnpoolComparison() const;
    void unpoolComparison(BodyAggrElemVec &elems) const;
    void collect(VarTermBoundVec &vars, bool tupleOnly = false) const;
    bool simplify(Projections &project, SimplifyState &state, Logger &log);
    void rewriteArithmetics(Term::ArithmeticsMap &arith, Literal::RelationVec &assign, AuxGen &auxGen);
    void check(ChkLvlVec &levels) const;
    void replace(Defines &defs);
    template <class T, class C>
    std::unique_ptr<T> toGround(ToGroundArg &x, C &completeRef, Ground::ULitVec &&lits) const;

    void gatherIEs(IESolver &solver) const override;
    void addIEBound(VarTerm const &var, IEBound const &bound) override;

    friend std::ostream &operator<<(std::ostream &out, BodyAggrElem const &elem);
    friend bool operator==(BodyAggrElem const &a, BodyAggrElem const &b);
    friend size_t get_value_hash(BodyAggrElem const &elem);
    friend BodyAggrElem get_clone(BodyAggrElem const &elem);

private:
    UTermVec tuple_;
    ULitVec condition_;
};

// {{{1 declaration of TupleBodyAggregate

class TupleBodyAggregate : public BodyAggregate {
public:
    TupleBodyAggregate(NAF naf, bool removedAssignment, bool translated, AggregateFunction fun, BoundVec &&bounds, BodyAggrElemVec &&elems); // NOTE: private
    TupleBodyAggregate(NAF naf, AggregateFunction fun, BoundVec &&bounds, BodyAggrElemVec &&elems);

    void addToSolver(IESolver &solver) override;
    bool rewriteAggregates(UBodyAggrVec &aggr) override;
    bool isAssignment() const override;
    void removeAssignment() override;
    void collect(VarTermBoundVec &vars) const override;
    bool operator==(BodyAggregate const &other) const override;
    void print(std::ostream &out) const override;
    size_t hash() const override;
    TupleBodyAggregate *clone() const override;
    bool hasPool() const override;
    void unpool(UBodyAggrVec &x) override;
    bool hasUnpoolComparison() const override;
    UBodyAggrVecVec unpoolComparison() const override;
    bool simplify(Projections &project, SimplifyState &state, bool singleton, Logger &log) override;
    void assignLevels(AssignLevel &lvl) override;
    void rewriteArithmetics(Term::ArithmeticsMap &arith, Literal::RelationVec &assign, AuxGen &auxGen) override;
    void check(ChkLvlVec &levels, Logger &log) const override;
    void replace(Defines &dx) override;
    CreateBody toGround(ToGroundArg &x, Ground::UStmVec &stms) const override;

private:
    NAF naf_;
    bool removedAssignment_ = false;
    bool translated_ = false;
    AggregateFunction fun_;
    BoundVec bounds_;
    BodyAggrElemVec elems_;
};

// {{{1 declaration of LitBodyAggregate

class LitBodyAggregate : public BodyAggregate {
public:
    LitBodyAggregate(NAF naf, AggregateFunction fun, BoundVec &&bounds, CondLitVec &&elems);

    bool rewriteAggregates(UBodyAggrVec &aggr) override;
    bool isAssignment() const override;
    void removeAssignment() override;
    void collect(VarTermBoundVec &vars) const override;
    bool operator==(BodyAggregate const &other) const override;
    void print(std::ostream &out) const override;
    size_t hash() const override;
    LitBodyAggregate *clone() const override;
    bool hasPool() const override;
    void unpool(UBodyAggrVec &x) override;
    bool hasUnpoolComparison() const override;
    UBodyAggrVecVec unpoolComparison() const override;
    bool simplify(Projections &project, SimplifyState &state, bool singleton, Logger &log) override;
    void assignLevels(AssignLevel &lvl) override;
    void rewriteArithmetics(Term::ArithmeticsMap &arith, Literal::RelationVec &assign, AuxGen &auxGen) override;
    void check(ChkLvlVec &levels, Logger &log) const override;
    void replace(Defines &dx) override;
    CreateBody toGround(ToGroundArg &x, Ground::UStmVec &stms) const override;

private:
    NAF naf_;
    AggregateFunction fun_;
    BoundVec bounds_;
    CondLitVec elems_;
};

// {{{1 declaration of ConjunctionElem

class ConjunctionElem;
using ConjunctionElemVec = std::vector<ConjunctionElem>;
using ConjunctionElemVecVec = std::vector<ConjunctionElemVec>;

class ConjunctionElem : public IEContext {
public:
    using ULitVecVec = std::vector<ULitVec>;
    ConjunctionElem(ULit head, ULitVec cond)
    : cond_{std::move(cond)} {
        head_.emplace_back();
        head_.back().emplace_back(std::move(head));
    }
    ConjunctionElem(ULitVecVec head, ULitVec cond)
    : head_{std::move(head)}
    , cond_{std::move(cond)} { }

    void print(std::ostream &out) const;
    bool hasPool() const;
    void unpool(ConjunctionElemVec &elems) const;
    bool hasUnpoolComparison() const;
    ConjunctionElemVecVec unpoolComparison() const;
    void collect(VarTermBoundVec &vars) const;
    bool simplify(Projections &project, SimplifyState &state, Logger &log);
    void rewriteArithmetics(Term::ArithmeticsMap &arith, AuxGen &auxGen);
    void assignLevels(AssignLevel &lvl) const;
    void check(BodyAggregate const &parent, ChkLvlVec &levels, Logger &log) const;
    void replace(Defines &x);
    CreateBody toGround(UTerm id, ToGroundArg &x, Ground::UStmVec &stms) const;

    void gatherIEs(IESolver &solver) const override;
    void addIEBound(VarTerm const &var, IEBound const &bound) override;

    friend std::ostream &operator<<(std::ostream &out, ConjunctionElem const &elem);
    friend bool operator==(ConjunctionElem const &a, ConjunctionElem const &b);
    friend size_t get_value_hash(ConjunctionElem const &elem);
    friend ConjunctionElem get_clone(ConjunctionElem const &elem);

private:
    ULitVecVec head_;
    ULitVec cond_;
};

// {{{1 declaration of Conjunction

class Conjunction : public BodyAggregate {
public:
    Conjunction(ULit head, ULitVec cond) {
        elems_.emplace_back(std::move(head), std::move(cond));
    }
    Conjunction(ConjunctionElemVec elems)
    : elems_{std::move(elems)} { }

    void addToSolver(IESolver &solver) override;
    bool rewriteAggregates(UBodyAggrVec &aggr) override;
    bool isAssignment() const override;
    void removeAssignment() override;
    void collect(VarTermBoundVec &vars) const override;
    bool operator==(BodyAggregate const &other) const override;
    void print(std::ostream &out) const override;
    size_t hash() const override;
    Conjunction *clone() const override;
    bool hasPool() const override;
    void unpool(UBodyAggrVec &x) override;
    bool hasUnpoolComparison() const override;
    UBodyAggrVecVec unpoolComparison() const override;
    bool simplify(Projections &project, SimplifyState &state, bool singleton, Logger &log) override;
    void assignLevels(AssignLevel &lvl) override;
    void rewriteArithmetics(Term::ArithmeticsMap &arith, Literal::RelationVec &assign, AuxGen &auxGen) override;
    void check(ChkLvlVec &levels, Logger &log) const override;
    void replace(Defines &dx) override;
    CreateBody toGround(ToGroundArg &x, Ground::UStmVec &stms) const override;

private:
    ConjunctionElemVec elems_;
};

// {{{1 declaration of SimpleBodyLiteral

class SimpleBodyLiteral : public BodyAggregate {
public:
    SimpleBodyLiteral(ULit &&lit);

    void addToSolver(IESolver &solver) override;
    unsigned projectScore() const override { return lit_->projectScore(); }
    Location const &loc() const override;
    void loc(Location const &loc) override;
    bool rewriteAggregates(UBodyAggrVec &aggr) override;
    void removeAssignment() override;
    bool isAssignment() const override;
    void collect(VarTermBoundVec &vars) const override;
    bool operator==(BodyAggregate const &other) const override;
    void print(std::ostream &out) const override;
    size_t hash() const override;
    SimpleBodyLiteral *clone() const override;
    bool hasPool() const override;
    void unpool(UBodyAggrVec &x) override;
    bool hasUnpoolComparison() const override;
    UBodyAggrVecVec unpoolComparison() const override;
    bool simplify(Projections &project, SimplifyState &state, bool singleton, Logger &log) override;
    void assignLevels(AssignLevel &lvl) override;
    void rewriteArithmetics(Term::ArithmeticsMap &arith, Literal::RelationVec &assign, AuxGen &auxGen) override;
    void check(ChkLvlVec &levels, Logger &log) const override;
    void replace(Defines &dx) override;
    CreateBody toGround(ToGroundArg &x, Ground::UStmVec &stms) const override;

private:
    ULit lit_;
};

// }}}1

// {{{1 declaration of HeadAggrElem

class HeadAggrElem : public IEContext {
public:
    HeadAggrElem(UTermVec tuple, ULit lit, ULitVec condition)
    : tuple_{std::move(tuple)}
    , lit_{std::move(lit)}
    , condition_{std::move(condition)} { }

    bool hasPool() const;
    void unpool(HeadAggrElemVec &pool);
    bool hasUnpoolComparison() const;
    void unpoolComparison(HeadAggrElemVec &elems) const;
    void collect(VarTermBoundVec &vars, bool tupleOnly = false) const;
    void shiftLit();
    void shiftCondition(UBodyAggrVec &aggr, bool weight);
    bool simplify(Projections &project, SimplifyState &state, Logger &log);
    void rewriteArithmetics(Term::ArithmeticsMap &arith, Literal::RelationVec &assign, AuxGen &auxGen);
    void check(ChkLvlVec &levels) const;
    void replace(Defines &defs);
    bool isSimple() const;
    UTerm headRepr() const;
    template <class T, class C>
    std::unique_ptr<T> toGround(ToGroundArg &x, C &completeRef) const;

    void gatherIEs(IESolver &solver) const override;
    void addIEBound(VarTerm const &var, IEBound const &bound) override;

    friend std::ostream &operator<<(std::ostream &out, HeadAggrElem const &elem);
    friend bool operator==(HeadAggrElem const &a, HeadAggrElem const &b);
    friend size_t get_value_hash(HeadAggrElem const &elem);
    friend HeadAggrElem get_clone(HeadAggrElem const &elem);

private:
    template <class T>
    void zeroLevel_(VarTermBoundVec &bound, T const &x);


    UTermVec tuple_;
    ULit lit_;
    ULitVec condition_;
};

// {{{1 declaration of TupleHeadAggregate

class TupleHeadAggregate : public HeadAggregate {
public:
    TupleHeadAggregate(AggregateFunction fun, bool translated, BoundVec &&bounds, HeadAggrElemVec &&elems);
    TupleHeadAggregate(AggregateFunction fun, BoundVec &&bounds, HeadAggrElemVec &&elems);

    void addToSolver(IESolver &solver) override;
    UHeadAggr rewriteAggregates(UBodyAggrVec &aggr) override;
    void collect(VarTermBoundVec &vars) const override;
    bool operator==(HeadAggregate const &other) const override;
    void print(std::ostream &out) const override;
    size_t hash() const override;
    TupleHeadAggregate *clone() const override;
    bool hasPool() const override;
    void unpool(UHeadAggrVec &x) override;
    UHeadAggr unpoolComparison(UBodyAggrVec &body) override;
    bool simplify(Projections &project, SimplifyState &state, Logger &log) override;
    void assignLevels(AssignLevel &lvl) override;
    void rewriteArithmetics(Term::ArithmeticsMap &arith, AuxGen &auxGen) override;
    void check(ChkLvlVec &levels, Logger &log) const override;
    void replace(Defines &dx) override;
    CreateHead toGround(ToGroundArg &x, Ground::UStmVec &stms) const override;

private:
    AggregateFunction fun_;
    bool translated_;
    BoundVec bounds_;
    HeadAggrElemVec elems_;
};

// {{{1 declaration of LitHeadAggregate

class LitHeadAggregate : public HeadAggregate {
public:
    LitHeadAggregate(AggregateFunction fun, BoundVec &&bounds, CondLitVec &&elems);

    UHeadAggr rewriteAggregates(UBodyAggrVec &aggr) override;
    void collect(VarTermBoundVec &vars) const override;
    bool operator==(HeadAggregate const &other) const override;
    void print(std::ostream &out) const override;
    size_t hash() const override;
    LitHeadAggregate *clone() const override;
    bool hasPool() const override;
    void unpool(UHeadAggrVec &x) override;
    UHeadAggr unpoolComparison(UBodyAggrVec &body) override;
    bool simplify(Projections &project, SimplifyState &state, Logger &log) override;
    void assignLevels(AssignLevel &lvl) override;
    void rewriteArithmetics(Term::ArithmeticsMap &arith, AuxGen &auxGen) override;
    void check(ChkLvlVec &levels, Logger &log) const override;
    void replace(Defines &dx) override;
    CreateHead toGround(ToGroundArg &x, Ground::UStmVec &stms) const override;

private:
    AggregateFunction fun_;
    BoundVec bounds_;
    CondLitVec elems_;
};

// {{{1 declaration of DisjunctionElem

class DisjunctionElem;
using DisjunctionElemVec = std::vector<DisjunctionElem>;

class DisjunctionElem : public IEContext {
    using Head = std::pair<ULit, ULitVec>;
    using HeadVec = std::vector<Head>;
public:
    DisjunctionElem(HeadVec head, ULitVec cond)
    : heads_{std::move(head)}
    , cond_{std::move(cond)} {}
    DisjunctionElem(CondLit lit)
    : cond_{std::move(lit.second)} {
        heads_.emplace_back();
        heads_.back().first = std::move(lit.first);
    }
    void print(std::ostream &out) const;
    void unpool(DisjunctionElemVec &elems);
    bool hasUnpoolComparison() const;
    void unpoolComparison(DisjunctionElemVec &elems);
    void collect(VarTermBoundVec &vars) const;
    void rewriteAggregates(Location const &loc, UBodyAggrVec &aggr);
    bool simplify(Projections &project, SimplifyState &state, Logger &log);
    void rewriteArithmetics(Term::ArithmeticsMap &arith, AuxGen &auxGen);
    void assignLevels(AssignLevel &lvl);
    void check(HeadAggregate const &parent, ChkLvlVec &levels, Logger &log) const;
    bool hasPool() const;
    void replace(Defines &x);
    bool isSimple() const;
    template <class T>
    void toGroundSimple(ToGroundArg &x, T &heads) const;
    template <class T>
    void toGround(Location const &loc, T &complete, ToGroundArg &x, Ground::UStmVec &stms) const;

    void gatherIEs(IESolver &solver) const override;
    void addIEBound(VarTerm const &var, IEBound const &bound) override;

    friend std::ostream &operator<<(std::ostream &out, DisjunctionElem const &elem);
    friend bool operator==(DisjunctionElem const &a, DisjunctionElem const &b);
    friend size_t get_value_hash(DisjunctionElem const &elem);
    friend DisjunctionElem get_clone(DisjunctionElem const &elem);

private:
    HeadVec heads_;
    ULitVec cond_;
};

// {{{1 declaration of Disjunction

class Disjunction : public HeadAggregate {
public:

    Disjunction(CondLitVec elems);
    Disjunction(DisjunctionElemVec elems);

    void addToSolver(IESolver &solver) override;
    UHeadAggr rewriteAggregates(UBodyAggrVec &aggr) override;
    void collect(VarTermBoundVec &vars) const override;
    bool operator==(HeadAggregate const &other) const override;
    void print(std::ostream &out) const override;
    size_t hash() const override;
    Disjunction *clone() const override;
    bool hasPool() const override;
    void unpool(UHeadAggrVec &x) override;
    UHeadAggr unpoolComparison(UBodyAggrVec &body) override;
    bool simplify(Projections &project, SimplifyState &state, Logger &log) override;
    void assignLevels(AssignLevel &lvl) override;
    void rewriteArithmetics(Term::ArithmeticsMap &arith, AuxGen &auxGen) override;
    void check(ChkLvlVec &levels, Logger &log) const override;
    void replace(Defines &dx) override;
    CreateHead toGround(ToGroundArg &x, Ground::UStmVec &stms) const override;

private:
    DisjunctionElemVec elems_;
};

// {{{1 declaration of SimpleHeadLiteral

class SimpleHeadLiteral : public HeadAggregate {
public:
    SimpleHeadLiteral(ULit &&lit);

    void addToSolver(IESolver &solver) override;
    bool isPredicate() const override { return true; }
    Location const &loc() const override;
    void loc(Location const &loc) override;
    UHeadAggr rewriteAggregates(UBodyAggrVec &aggr) override;
    void collect(VarTermBoundVec &vars) const override;
    bool operator==(HeadAggregate const &other) const override;
    void print(std::ostream &out) const override;
    size_t hash() const override;
    SimpleHeadLiteral *clone() const override;
    bool hasPool() const override;
    void unpool(UHeadAggrVec &x) override;
    UHeadAggr unpoolComparison(UBodyAggrVec &body) override;
    bool simplify(Projections &project, SimplifyState &state, Logger &log) override;
    void assignLevels(AssignLevel &lvl) override;
    void rewriteArithmetics(Term::ArithmeticsMap &arith, AuxGen &auxGen) override;
    void check(ChkLvlVec &levels, Logger &log) const override;
    void replace(Defines &dx) override;
    CreateHead toGround(ToGroundArg &x, Ground::UStmVec &stms) const override;
    Symbol isEDB() const override;

private:
    ULit lit_;
};

// }}}1
// {{{1 declaration of MinimizeHeadLiteral

class MinimizeHeadLiteral : public HeadAggregate {
public:
    MinimizeHeadLiteral(UTerm &&weight, UTerm &&priority, UTermVec &&tuple);
    MinimizeHeadLiteral(UTermVec &&tuple);

    UHeadAggr rewriteAggregates(UBodyAggrVec &aggr) override;
    void collect(VarTermBoundVec &vars) const override;
    bool operator==(HeadAggregate const &other) const override;
    void print(std::ostream &out) const override;
    void printWithCondition(std::ostream &out, UBodyAggrVec const &condition) const override;
    size_t hash() const override;
    MinimizeHeadLiteral *clone() const override;
    bool hasPool() const override;
    void unpool(UHeadAggrVec &x) override;
    UHeadAggr unpoolComparison(UBodyAggrVec &body) override;
    bool simplify(Projections &project, SimplifyState &state, Logger &log) override;
    void assignLevels(AssignLevel &lvl) override;
    void rewriteArithmetics(Term::ArithmeticsMap &arith, AuxGen &auxGen) override;
    void check(ChkLvlVec &levels, Logger &log) const override;
    void replace(Defines &x) override;
    CreateHead toGround(ToGroundArg &x, Ground::UStmVec &stms) const override;

private:
    Term &weight() const;
    Term &priority() const;

    UTermVec tuple_;
};

// }}}1
// {{{1 declaration of EdgeHeadAtom

class EdgeHeadAtom : public HeadAggregate {
public:
    EdgeHeadAtom(UTerm &&u, UTerm &&v);

    UHeadAggr rewriteAggregates(UBodyAggrVec &aggr) override;
    void collect(VarTermBoundVec &vars) const override;
    bool operator==(HeadAggregate const &other) const override;
    void print(std::ostream &out) const override;
    size_t hash() const override;
    EdgeHeadAtom *clone() const override;
    bool hasPool() const override;
    void unpool(UHeadAggrVec &x) override;
    UHeadAggr unpoolComparison(UBodyAggrVec &body) override;
    bool simplify(Projections &project, SimplifyState &state, Logger &log) override;
    void assignLevels(AssignLevel &lvl) override;
    void rewriteArithmetics(Term::ArithmeticsMap &arith, AuxGen &auxGen) override;
    void check(ChkLvlVec &levels, Logger &log) const override;
    void replace(Defines &x) override;
    CreateHead toGround(ToGroundArg &x, Ground::UStmVec &stms) const override;

private:
    UTerm u_;
    UTerm v_;
};

// {{{1 declaration of ProjectHeadAtom

class ProjectHeadAtom : public HeadAggregate {
public:
    ProjectHeadAtom(UTerm &&atom);

    UHeadAggr rewriteAggregates(UBodyAggrVec &aggr) override;
    void collect(VarTermBoundVec &vars) const override;
    bool operator==(HeadAggregate const &other) const override;
    void print(std::ostream &out) const override;
    size_t hash() const override;
    ProjectHeadAtom *clone() const override;
    bool hasPool() const override;
    void unpool(UHeadAggrVec &x) override;
    UHeadAggr unpoolComparison(UBodyAggrVec &body) override;
    bool simplify(Projections &project, SimplifyState &state, Logger &log) override;
    void assignLevels(AssignLevel &lvl) override;
    void rewriteArithmetics(Term::ArithmeticsMap &arith, AuxGen &auxGen) override;
    void check(ChkLvlVec &levels, Logger &log) const override;
    void replace(Defines &x) override;
    CreateHead toGround(ToGroundArg &x, Ground::UStmVec &stms) const override;

private:
    UTerm atom_;
};

// {{{1 declaration of ExternalHeadAtom

class ExternalHeadAtom : public HeadAggregate {
public:
    ExternalHeadAtom(UTerm &&atom, UTerm &&type);

    UHeadAggr rewriteAggregates(UBodyAggrVec &aggr) override;
    void collect(VarTermBoundVec &vars) const override;
    bool operator==(HeadAggregate const &other) const override;
    void print(std::ostream &out) const override;
    void printWithCondition(std::ostream &out, UBodyAggrVec const &condition) const override;
    size_t hash() const override;
    ExternalHeadAtom *clone() const override;
    bool hasPool() const override;
    void unpool(UHeadAggrVec &x) override;
    UHeadAggr unpoolComparison(UBodyAggrVec &body) override;
    bool simplify(Projections &project, SimplifyState &state, Logger &log) override;
    void assignLevels(AssignLevel &lvl) override;
    void rewriteArithmetics(Term::ArithmeticsMap &arith, AuxGen &auxGen) override;
    void check(ChkLvlVec &levels, Logger &log) const override;
    void replace(Defines &x) override;
    CreateHead toGround(ToGroundArg &x, Ground::UStmVec &stms) const override;

private:
    UTerm atom_;
    UTerm type_;
};

// {{{1 declaration of HeuristicHeadAtom

class HeuristicHeadAtom : public HeadAggregate {
public:
    HeuristicHeadAtom(UTerm &&atom, UTerm &&bias, UTerm &&priority, UTerm &&mod);

    UHeadAggr rewriteAggregates(UBodyAggrVec &aggr) override;
    void collect(VarTermBoundVec &vars) const override;
    bool operator==(HeadAggregate const &other) const override;
    void print(std::ostream &out) const override;
    size_t hash() const override;
    HeuristicHeadAtom *clone() const override;
    bool hasPool() const override;
    void unpool(UHeadAggrVec &x) override;
    UHeadAggr unpoolComparison(UBodyAggrVec &body) override;
    bool simplify(Projections &project, SimplifyState &state, Logger &log) override;
    void assignLevels(AssignLevel &lvl) override;
    void rewriteArithmetics(Term::ArithmeticsMap &arith, AuxGen &auxGen) override;
    void check(ChkLvlVec &levels, Logger &log) const override;
    void replace(Defines &x) override;
    CreateHead toGround(ToGroundArg &x, Ground::UStmVec &stms) const override;

private:
    UTerm atom_;
    UTerm value_;
    UTerm priority_;
    UTerm mod_;
};

// {{{1 declaration of ShowHeadLiteral

class ShowHeadLiteral : public HeadAggregate {
public:
    ShowHeadLiteral(UTerm &&term);

    UHeadAggr rewriteAggregates(UBodyAggrVec &aggr) override;
    void collect(VarTermBoundVec &vars) const override;
    bool operator==(HeadAggregate const &other) const override;
    void print(std::ostream &out) const override;
    size_t hash() const override;
    ShowHeadLiteral *clone() const override;
    bool hasPool() const override;
    void unpool(UHeadAggrVec &x) override;
    UHeadAggr unpoolComparison(UBodyAggrVec &body) override;
    bool simplify(Projections &project, SimplifyState &state, Logger &log) override;
    void assignLevels(AssignLevel &lvl) override;
    void rewriteArithmetics(Term::ArithmeticsMap &arith, AuxGen &auxGen) override;
    void check(ChkLvlVec &levels, Logger &log) const override;
    void replace(Defines &x) override;
    CreateHead toGround(ToGroundArg &x, Ground::UStmVec &stms) const override;

private:
    UTerm term_;
};

// }}}1

} } // namespace Input Gringo

#endif // GRINGO_INPUT_AGGREGATES_HH
