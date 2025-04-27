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

#ifndef GRINGO_INPUT_LITERALS_HH
#define GRINGO_INPUT_LITERALS_HH

#include <gringo/terms.hh>
#include <gringo/input/literal.hh>

namespace Gringo { namespace Input {

// {{{1 declaration of PredicateLiteral

class PredicateLiteral : public Literal {
public:
    PredicateLiteral(NAF naf, UTerm &&repr, bool auxiliary = false);
    PredicateLiteral(PredicateLiteral const &other) = delete;
    PredicateLiteral(PredicateLiteral &&other) noexcept = default;
    PredicateLiteral &operator=(PredicateLiteral const &other) = delete;
    PredicateLiteral &operator=(PredicateLiteral &&other) noexcept = delete;
    ~PredicateLiteral() noexcept override = default;

    unsigned projectScore() const override;
    void collect(VarTermBoundVec &vars, bool bound) const override;
    void toTuple(UTermVec &tuple, int &id) const override;
    PredicateLiteral *clone() const override;
    void print(std::ostream &out) const override;
    bool operator==(Literal const &other) const override;
    size_t hash() const override;
    bool simplify(Logger &log, Projections &project, SimplifyState &state, bool positional = true, bool singleton = false) override;
    void rewriteArithmetics(Term::ArithmeticsMap &arith, RelationVec &assign, AuxGen &auxGen) override;
    ULitVec unpool(bool head) const override;
    Symbol isEDB() const override;
    bool hasPool(bool head) const override;
    void replace(Defines &dx) override;
    Ground::ULit toGround(DomainData &x, bool auxiliary) const override;
    ULit shift(bool negate) override;
    UTerm headRepr() const override;
    bool auxiliary() const override;
    void auxiliary(bool auxiliary) override;

private:
    NAF naf_;
    bool auxiliary_;
    UTerm repr_;
};

class ProjectionLiteral : public PredicateLiteral {
public:
    ProjectionLiteral(UTerm &&repr);
    ProjectionLiteral(ProjectionLiteral const &other) = delete;
    ProjectionLiteral(ProjectionLiteral &&other) noexcept = default;
    ProjectionLiteral &operator=(ProjectionLiteral const &other) = delete;
    ProjectionLiteral &operator=(ProjectionLiteral &&other) noexcept = delete;
    ~ProjectionLiteral() noexcept override = default;

    ProjectionLiteral *clone() const override;
    ULitVec unpool(bool head) const override;
    Ground::ULit toGround(DomainData &x, bool auxiliary) const override;
    ULit shift(bool negate) override;

private:
    mutable bool initialized_;
};

// }}}
// {{{ declaration of RelationLiteral

class RelationLiteral : public Literal {
public:
    using Terms = std::vector<std::pair<Relation, UTerm>>;

    RelationLiteral(NAF naf, UTerm &&left, Terms &&right);
    RelationLiteral(Relation rel, UTerm &&left, UTerm &&right);
    RelationLiteral(NAF naf, Relation rel, UTerm &&left, UTerm &&right);
    RelationLiteral(RelationLiteral const &other) = delete;
    RelationLiteral(RelationLiteral &&other) noexcept = default;
    RelationLiteral &operator=(RelationLiteral const &other) = delete;
    RelationLiteral &operator=(RelationLiteral &&other) noexcept = delete;
    ~RelationLiteral() noexcept override = default;

    static ULit make(Term::LevelMap::value_type &x);
    static ULit make(Literal::RelationVec::value_type &x);

    // {{{ Term interface
    void addToSolver(IESolver &solver, bool invert) const override;
    bool needSetShift() const override;
    unsigned projectScore() const override;
    void collect(VarTermBoundVec &vars, bool bound) const override;
    void toTuple(UTermVec &tuple, int &id) const override;
    RelationLiteral *clone() const override;
    void print(std::ostream &out) const override;
    bool operator==(Literal const &other) const override;
    size_t hash() const override;
    bool simplify(Logger &log, Projections &project, SimplifyState &state, bool positional = true, bool singleton = false) override;
    void rewriteArithmetics(Term::ArithmeticsMap &arith, RelationVec &assign, AuxGen &auxGen) override;
    ULitVec unpool(bool head) const override;
    ULitVecVec unpoolComparison() const override;
    bool hasUnpoolComparison() const override;
    bool hasPool(bool head) const override;
    void replace(Defines &dx) override;
    Ground::ULit toGround(DomainData &x, bool auxiliary) const override;
    UTerm headRepr() const override;
    ULit shift(bool negate) override;
    bool auxiliary() const override;
    void auxiliary(bool aux) override;
    // }}}

private:
    UTerm left_;
    Terms right_;
    NAF naf_;
};

// }}}
// {{{ declaration of RangeLiteral

class RangeLiteral : public Literal {
public:
    RangeLiteral(UTerm &&assign, UTerm &&lower, UTerm &&upper);
    RangeLiteral(RangeLiteral const &other) = delete;
    RangeLiteral(RangeLiteral &&other) noexcept = default;
    RangeLiteral &operator=(RangeLiteral const &other) = delete;
    RangeLiteral &operator=(RangeLiteral &&other) noexcept = delete;
    ~RangeLiteral() noexcept override = default;

    static ULit make(SimplifyState::DotsMap::value_type &dots);
    static ULit make(VarTerm const &var, IEBound const &bound);

    void addToSolver(IESolver &solver, bool invert) const override;
    void collect(VarTermBoundVec &vars, bool bound) const override;
    void toTuple(UTermVec &tuple, int &id) const override;
    RangeLiteral *clone() const override;
    void print(std::ostream &out) const override;
    bool operator==(Literal const &other) const override;
    size_t hash() const override;
    bool simplify(Logger &log, Projections &project, SimplifyState &state, bool positional = true, bool singleton = false) override;
    void rewriteArithmetics(Term::ArithmeticsMap &arith, RelationVec &assign, AuxGen &auxGen) override;
    ULitVec unpool(bool head) const override;
    bool hasPool(bool head) const override;
    void replace(Defines &dx) override;
    Ground::ULit toGround(DomainData &x, bool auxiliary) const override;
    ULit shift(bool negate) override;
    UTerm headRepr() const override;
    bool auxiliary() const override;
    void auxiliary(bool aux) override;

private:
    UTerm assign_;
    UTerm lower_;
    UTerm upper_;
};

// }}}
// {{{ declaration of ScriptLiteral

class ScriptLiteral : public Literal {
public:
    ScriptLiteral(UTerm &&assign, String name, UTermVec &&args);
    ScriptLiteral(ScriptLiteral const &other) = delete;
    ScriptLiteral(ScriptLiteral &&other) noexcept = default;
    ScriptLiteral &operator=(ScriptLiteral const &other) = delete;
    ScriptLiteral &operator=(ScriptLiteral &&other) noexcept = delete;
    ~ScriptLiteral() noexcept override = default;

    static ULit make(SimplifyState::ScriptMap::value_type &script);

    void collect(VarTermBoundVec &vars, bool bound) const override;
    void toTuple(UTermVec &tuple, int &id) const override;
    ScriptLiteral *clone() const override;
    void print(std::ostream &out) const override;
    bool operator==(Literal const &other) const override;
    size_t hash() const override;
    bool simplify(Logger &log, Projections &project, SimplifyState &state, bool positional = true, bool singleton = false) override;
    void rewriteArithmetics(Term::ArithmeticsMap &arith, RelationVec &assign, AuxGen &auxGen) override;
    ULitVec unpool(bool head) const override;
    bool hasPool(bool head) const override;
    void replace(Defines &dx) override;
    Ground::ULit toGround(DomainData &x, bool auxiliary) const override;
    ULit shift(bool negate) override;
    UTerm headRepr() const override;
    bool auxiliary() const override;
    void auxiliary(bool aux) override;

private:
    UTerm assign_;
    String name_;
    UTermVec args_;
};

// }}}
// {{{ declaration of VoidLiteral

class VoidLiteral : public Literal {
public:
    VoidLiteral() = default;
    VoidLiteral(VoidLiteral const &other) = delete;
    VoidLiteral(VoidLiteral &&other) noexcept = default;
    VoidLiteral &operator=(VoidLiteral const &other) = delete;
    VoidLiteral &operator=(VoidLiteral &&other) noexcept = delete;
    ~VoidLiteral() noexcept override = default;

    unsigned projectScore() const override { return 0; }
    void collect(VarTermBoundVec &vars, bool bound) const override;
    void toTuple(UTermVec &tuple, int &id) const override;
    VoidLiteral *clone() const override;
    void print(std::ostream &out) const override;
    bool operator==(Literal const &other) const override;
    size_t hash() const override;
    bool simplify(Logger &log, Projections &project, SimplifyState &state, bool positional = true, bool singleton = false) override;
    void rewriteArithmetics(Term::ArithmeticsMap &arith, RelationVec &assign, AuxGen &auxGen) override;
    ULitVec unpool(bool head) const override;
    bool hasPool(bool head) const override;
    void replace(Defines &dx) override;
    Ground::ULit toGround(DomainData &x, bool auxiliary) const override;
    ULit shift(bool negate) override;
    UTerm headRepr() const override;
    bool auxiliary() const override;
    void auxiliary(bool aux) override;
};

// }}}

} } // namespace Input Gringo

#endif // GRINGO_INPUT_LITERALS_HH
