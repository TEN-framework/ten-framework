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

#ifndef GRINGO_INPUT_AGGREGATE_HH
#define GRINGO_INPUT_AGGREGATE_HH

#include <gringo/input/literal.hh>
#include <gringo/safetycheck.hh>
#include <gringo/terms.hh>
#include <list>

namespace Gringo { namespace Ground {

enum class RuleType : unsigned short;
using ULitVec = std::vector<ULit>;
class Statement;
using UStm = std::unique_ptr<Statement>;
using UStmVec = std::vector<UStm>;

} // namespace Ground

namespace Input {

// {{{ declaration of auxiliary types

using CondLit = std::pair<ULit, ULitVec>;
using CondLitVec = std::vector<CondLit>;
class BodyAggrElem;
using BodyAggrElemVec = std::vector<BodyAggrElem>;

class HeadAggrElem;
using HeadAggrElemVec = std::vector<HeadAggrElem>;

class BodyAggregate;
using UBodyAggr = std::unique_ptr<BodyAggregate>;
using UBodyAggrVec = std::vector<UBodyAggr>;
using UBodyAggrVecVec = std::vector<UBodyAggrVec>;

class HeadAggregate;
using UHeadAggr = std::unique_ptr<HeadAggregate>;
using UHeadAggrVec = std::vector<UHeadAggr>;

// }}}

// {{{ declaration of AssignLevel

class AssignLevel {
public:
    using BoundSet = std::unordered_map<Term::SVal, unsigned>;

    AssignLevel() = default;
    AssignLevel(AssignLevel const &other) = delete;
    AssignLevel(AssignLevel &&other) noexcept = delete;
    AssignLevel &operator=(AssignLevel const &other) = delete;
    AssignLevel &operator=(AssignLevel &&other) noexcept = delete;
    ~AssignLevel() noexcept = default;


    void add(VarTermBoundVec &vars);
    AssignLevel &subLevel();
    void assignLevels();
    void assignLevels(unsigned level, BoundSet const &bound);

private:
    std::list<AssignLevel> children_;
    std::unordered_map<Term::SVal, std::vector<VarTerm*>> occurr_;
};

// }}}
// {{{ declaration of CheckLevel

class CheckLevel {
public:
    struct Ent {
        bool operator<(Ent const &x) const;
    };
    using SC     = SafetyChecker<VarTerm*, Ent>;
    using VarMap = std::unordered_map<String, SC::VarNode *>;

    CheckLevel(Location const &loc, Printable const &p);
    CheckLevel(CheckLevel const &other) = delete;
    CheckLevel(CheckLevel &&other) noexcept;
    CheckLevel &operator=(CheckLevel const &other) = delete;
    CheckLevel &operator=(CheckLevel &&other) noexcept = delete;
    ~CheckLevel() noexcept = default;

    SC::VarNode &var(VarTerm &var);
    void check(Logger &log);

    Location         loc;
    Printable const &p;
    SC               dep;
    SC::EntNode     *current = nullptr;
    VarMap           vars;
};
using ChkLvlVec = std::vector<CheckLevel>;
void addVars(ChkLvlVec &levels, VarTermBoundVec &vars);

// }}}
// {{{ declaration of ToGroundArg

using CreateLit     = std::function<void (Ground::ULitVec &, bool)>;
using CreateStm     = std::function<Ground::UStm (Ground::ULitVec &&)>;
using CreateStmVec  = std::vector<CreateStm>;
using CreateBody    = std::pair<CreateLit, CreateStmVec>;
using CreateBodyVec = std::vector<CreateBody>;
using CreateHead    = CreateStm;

class ToGroundArg {
public:
    ToGroundArg(unsigned &auxNames, DomainData &domains);
    ToGroundArg(ToGroundArg const &other) = delete;
    ToGroundArg(ToGroundArg &&other) = delete;
    ToGroundArg &operator=(ToGroundArg const &other) = delete;
    ToGroundArg &operator=(ToGroundArg &&other) = delete;
    ~ToGroundArg() noexcept = default;

    String newId(bool increment = true);
    UTermVec getGlobal(VarTermBoundVec const &vars);
    UTermVec getLocal(VarTermBoundVec const &vars);
    UTerm newId(UTermVec &&global, Location const &loc, bool increment = true);
    template <class T>
    UTerm newId(T const &x) {
        VarTermBoundVec vars;
        x.collect(vars);
        return newId(getGlobal(vars), x.loc());
    }

    unsigned   &auxNames;
    DomainData &domains;
};

// }}}

// {{{ declaration of BodyAggregate

class BodyAggregate : public Printable, public Hashable, public Locatable, public Clonable<BodyAggregate>, public Comparable<BodyAggregate> {
public:
    BodyAggregate() = default;
    BodyAggregate(BodyAggregate const &other) = default;
    BodyAggregate(BodyAggregate &&other) noexcept = default;
    BodyAggregate &operator=(BodyAggregate const &other) = default;
    BodyAggregate &operator=(BodyAggregate &&other) noexcept = default;
    ~BodyAggregate() noexcept override = default;

    //! Add inequalities bounding variables to the given solver.
    virtual void addToSolver(IESolver &solver);
    //! Removes RawTheoryTerms from TheoryLiterals
    virtual void initTheory(TheoryDefs &def, Logger &log) { (void)def; (void)log; };
    virtual unsigned projectScore() const { return 2; }
    //! Check if the aggregate needs unpooling.
    virtual bool hasPool() const = 0;
    //! Unpool the aggregate and aggregate elements.
    virtual void unpool(UBodyAggrVec &x) = 0;
    //! Check if the aggregate needs unpooling.
    virtual bool hasUnpoolComparison() const = 0;
    //! Unpool the aggregate and aggregate elements.
    virtual UBodyAggrVecVec unpoolComparison() const = 0;
    //! Simplify the aggregate and aggregate elements.
    //! \pre Must be called after unpool.
    virtual bool simplify(Projections &project, SimplifyState &state, bool singleton, Logger &log) = 0;
    //! Assign levels to variables using the VarCollector.
    //! \pre Must be called after simplify.
    virtual void assignLevels(AssignLevel &lvl) = 0;
    virtual void check(ChkLvlVec &lvl, Logger &log) const = 0;
    //! Rewrite arithmetics.
    //! \pre Requires variables assigned to levels.
    virtual void rewriteArithmetics(Term::ArithmeticsMap &arith, Literal::RelationVec &assign, AuxGen &auxGen) = 0;
    //! Rewrite aggregates.
    //! Separates assignment aggregates from ordinary bounds, and
    //! transforms literal aggregates into tuple aggregates.
    //! Returns false iff the aggregates must be removed.
    //! Argument aggr contains the rewritten aggregates.
    virtual bool rewriteAggregates(UBodyAggrVec &aggr) = 0;
    //! Returns true if the aggregate is an assignment aggregate.
    //! \note Does not consider relation literals.
    virtual void removeAssignment() = 0;
    virtual bool isAssignment() const = 0;
    //! Collects all variables occuring in the aggregate.
    //! Occurrences bound by the aggregate are marked as such
    //! (occurrences bound in nested scopes are not marked).
    virtual void collect(VarTermBoundVec &vars) const = 0;
    virtual void replace(Defines &dx) = 0;
    virtual CreateBody toGround(ToGroundArg &x, Ground::UStmVec &stms) const = 0;
};

// }}}

// {{{ declaration of HeadAggregate

class HeadAggregate : public Printable, public Hashable, public Locatable, public Clonable<HeadAggregate>, public Comparable<HeadAggregate> {
public:
    HeadAggregate() = default;
    HeadAggregate(HeadAggregate const &other) = default;
    HeadAggregate(HeadAggregate &&other) noexcept = default;
    HeadAggregate &operator=(HeadAggregate const &other) = default;
    HeadAggregate &operator=(HeadAggregate &&other) noexcept = default;
    ~HeadAggregate() noexcept override = default;

    //! Add inequalities bounding variables to the given solver.
    virtual void addToSolver(IESolver &solver);
    //! Removes RawTheoryTerms from TheoryLiterals
    virtual void initTheory(TheoryDefs &def, bool hasBody, Logger &log);
    virtual bool isPredicate() const { return false; }
    //! Check if the aggregate needs unpooling.
    virtual bool hasPool() const = 0;
    //! Unpool the aggregate and aggregate elements.
    virtual void unpool(UHeadAggrVec &x) = 0;
    //! Unpool comparisons within the aggregate.
    virtual UHeadAggr unpoolComparison(UBodyAggrVec &body) = 0;
    //! Simplify the aggregate and aggregate elements.
    //! \pre Must be called after unpool.
    virtual bool simplify(Projections &project, SimplifyState &state, Logger &log) = 0;
    //! Assign levels to variables using the VarCollector.
    //! \pre Must be called after simplify.
    virtual void assignLevels(AssignLevel &lvl) = 0;
    virtual void check(ChkLvlVec &lvl, Logger &log) const = 0;
    //! Rewrite arithmetics.
    //! \pre Requires variables assigned to levels.
    virtual void rewriteArithmetics(Term::ArithmeticsMap &arith, AuxGen &auxGen) = 0;
    virtual UHeadAggr rewriteAggregates(UBodyAggrVec &aggr) = 0;
    //! Collects all variables occuring in the aggregate.
    //! Occurrences bound by the aggregate are marked as such
    //! (occurrences bound in nested scopes are not marked).
    virtual void collect(VarTermBoundVec &vars) const = 0;
    virtual void replace(Defines &dx) = 0;
    virtual CreateHead toGround(ToGroundArg &x, Ground::UStmVec &stms) const = 0;
    virtual Symbol isEDB() const;
    virtual void printWithCondition(std::ostream &out, UBodyAggrVec const &condition) const;
};

// }}}

std::vector<ULitVec> unpoolComparison_(ULitVec const &cond);

} } // namespace Input Gringo

GRINGO_HASH(Gringo::Input::BodyAggregate)
GRINGO_HASH(Gringo::Input::HeadAggregate)

#endif // GRINGO_INPUT_AGGREGATE_HH
