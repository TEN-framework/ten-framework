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

#ifndef GRINGO_INPUT_PROGRAMBUILDER_HH
#define GRINGO_INPUT_PROGRAMBUILDER_HH

#include "gringo/output/types.hh"
#include <gringo/locatable.hh>
#include <gringo/symbol.hh>
#include <gringo/indexed.hh>

#include <gringo/base.hh>

#include <vector>
#include <memory>
#include <forward_list>

namespace Gringo {

class TheoryOpDef;
class TheoryTermDef;
class TheoryAtomDef;
class TheoryDef;

namespace Input {

class Program;
class Statement;
class BodyAggregate;
class HeadAggregate;
class Literal;
class TheoryElement;
class TheoryAtom;

} // namespace Input

namespace Output {

class TheoryTerm;
class RawTheoryTerm;
class OutputPredicates;
using UTheoryTerm = std::unique_ptr<TheoryTerm>;

} } // namespace Output Gringo

namespace Gringo { namespace Input {

// {{{1 declaration of unique ids of program elements

enum IdVecUid           : unsigned { };
enum TermUid            : unsigned { };
enum TermVecUid         : unsigned { };
enum TermVecVecUid      : unsigned { };
enum LitUid             : unsigned { };
enum LitVecUid          : unsigned { };
enum RelLitVecUid       : unsigned { };
enum CondLitVecUid      : unsigned { };
enum BdAggrElemVecUid   : unsigned { };
enum HdAggrElemVecUid   : unsigned { };
enum HdLitUid           : unsigned { };
enum BdLitVecUid        : unsigned { };
enum BoundVecUid        : unsigned { };
enum TheoryOpVecUid     : unsigned { };
enum TheoryTermUid      : unsigned { };
enum TheoryOptermUid    : unsigned { };
enum TheoryOptermVecUid : unsigned { };
enum TheoryElemVecUid   : unsigned { };
enum TheoryAtomUid      : unsigned { };
enum TheoryOpDefUid     : unsigned { };
enum TheoryOpDefVecUid  : unsigned { };
enum TheoryTermDefUid   : unsigned { };
enum TheoryAtomDefUid   : unsigned { };
enum TheoryDefVecUid    : unsigned { };

// {{{1 declaration of INongroundProgramBuilder

class INongroundProgramBuilder {
public:
    INongroundProgramBuilder() = default;
    INongroundProgramBuilder(INongroundProgramBuilder const &other) = default;
    INongroundProgramBuilder(INongroundProgramBuilder &&other) noexcept = default;
    INongroundProgramBuilder &operator=(INongroundProgramBuilder const &other) = default;
    INongroundProgramBuilder &operator=(INongroundProgramBuilder &&other) noexcept = default;
    virtual ~INongroundProgramBuilder() noexcept = default;

    TermUid predRep(Location const &loc, bool neg, String name, TermVecVecUid tvvUid) {
        TermUid t = term(loc, name, tvvUid, false);
        if (neg) { t = term(loc, UnOp::NEG, t); }
        return t;
    }

    // {{{2 terms
    virtual TermUid term(Location const &loc, Symbol val) = 0;                             // constant
    virtual TermUid term(Location const &loc, String name) = 0;                            // variable
    virtual TermUid term(Location const &loc, UnOp op, TermUid a) = 0;                     // unary operation
    virtual TermUid term(Location const &loc, UnOp op, TermVecUid a) = 0;                  // unary operation
    virtual TermUid term(Location const &loc, BinOp op, TermUid a, TermUid b) = 0;         // binary operation
    virtual TermUid term(Location const &loc, TermUid a, TermUid b) = 0;                   // dots
    virtual TermUid term(Location const &loc, String name, TermVecVecUid b, bool lua) = 0; // function or lua function
    virtual TermUid term(Location const &loc, TermVecUid args, bool forceTuple) = 0;       // a tuple term (or simply a term)
    virtual TermUid pool(Location const &loc, TermVecUid args) = 0;                        // a pool term
    // {{{2 id vectors
    virtual IdVecUid idvec() = 0;
    virtual IdVecUid idvec(IdVecUid uid, Location const &loc, String id) = 0;
    // {{{2 term vectors
    virtual TermVecUid termvec() = 0;
    virtual TermVecUid termvec(TermVecUid uid, TermUid term) = 0;
    // {{{2 term vector vectors
    virtual TermVecVecUid termvecvec() = 0;
    virtual TermVecVecUid termvecvec(TermVecVecUid uid, TermVecUid termvecUid) = 0;
    // {{{2 literals
    virtual LitUid boollit(Location const &loc, bool type) = 0;
    virtual LitUid predlit(Location const &loc, NAF naf, TermUid atom) = 0;
    virtual RelLitVecUid rellitvec(Location const &loc, Relation rel, TermUid termUidLeft) = 0;
    virtual RelLitVecUid rellitvec(Location const &loc, RelLitVecUid vecUidLeft, Relation rel, TermUid termUidRight) = 0;
    virtual LitUid rellit(Location const &loc, NAF naf, TermUid termUidLeft, RelLitVecUid vecUidRight) = 0;
    // {{{2 literal vectors
    virtual LitVecUid litvec() = 0;
    virtual LitVecUid litvec(LitVecUid uid, LitUid literalUid) = 0;
    // {{{2 conditional literals
    virtual CondLitVecUid condlitvec() = 0;
    virtual CondLitVecUid condlitvec(CondLitVecUid uid, LitUid lit, LitVecUid litvec) = 0;
    // {{{2 body aggregate elements
    virtual BdAggrElemVecUid bodyaggrelemvec() = 0;
    virtual BdAggrElemVecUid bodyaggrelemvec(BdAggrElemVecUid uid, TermVecUid termvec, LitVecUid litvec) = 0;
    // {{{2 head aggregate elements
    virtual HdAggrElemVecUid headaggrelemvec() = 0;
    virtual HdAggrElemVecUid headaggrelemvec(HdAggrElemVecUid uid, TermVecUid termvec, LitUid lit, LitVecUid litvec) = 0;
    // {{{2 bounds
    virtual BoundVecUid boundvec() = 0;
    virtual BoundVecUid boundvec(BoundVecUid uid, Relation rel, TermUid term) = 0;
    // {{{2 heads
    virtual HdLitUid headlit(LitUid lit) = 0;
    virtual HdLitUid headaggr(Location const &loc, TheoryAtomUid atom) = 0;
    virtual HdLitUid headaggr(Location const &loc, AggregateFunction fun, BoundVecUid bounds, HdAggrElemVecUid headaggrelemvec) = 0;
    virtual HdLitUid headaggr(Location const &loc, AggregateFunction fun, BoundVecUid bounds, CondLitVecUid headaggrelemvec) = 0;
    virtual HdLitUid disjunction(Location const &loc, CondLitVecUid condlitvec) = 0;
    // {{{2 bodies
    virtual BdLitVecUid body() = 0;
    virtual BdLitVecUid bodylit(BdLitVecUid body, LitUid bodylit) = 0;
    virtual BdLitVecUid bodyaggr(BdLitVecUid body, Location const &loc, NAF naf, TheoryAtomUid atom) = 0;
    virtual BdLitVecUid bodyaggr(BdLitVecUid body, Location const &loc, NAF naf, AggregateFunction fun, BoundVecUid bounds, BdAggrElemVecUid bodyaggrelemvec) = 0;
    virtual BdLitVecUid bodyaggr(BdLitVecUid body, Location const &loc, NAF naf, AggregateFunction fun, BoundVecUid bounds, CondLitVecUid bodyaggrelemvec) = 0;
    virtual BdLitVecUid conjunction(BdLitVecUid body, Location const &loc, LitUid head, LitVecUid litvec) = 0;
    // {{{2 statements
    virtual void rule(Location const &loc, HdLitUid head) = 0;
    virtual void rule(Location const &loc, HdLitUid head, BdLitVecUid body) = 0;
    virtual void define(Location const &loc, String name, TermUid value, bool defaultDef, Logger &log) = 0;
    virtual void optimize(Location const &loc, TermUid weight, TermUid priority, TermVecUid cond, BdLitVecUid body) = 0;
    virtual void showsig(Location const &loc, Sig) = 0;
    virtual void defined(Location const &loc, Sig) = 0;
    virtual void show(Location const &loc, TermUid t, BdLitVecUid body) = 0;
    virtual void script(Location const &loc, String type, String code) = 0;
    virtual void block(Location const &loc, String name, IdVecUid args) = 0;
    virtual void external(Location const &loc, TermUid head, BdLitVecUid body, TermUid type) = 0;
    virtual void edge(Location const &loc, TermVecVecUid edges, BdLitVecUid body) = 0;
    virtual void heuristic(Location const &loc, TermUid termUid, BdLitVecUid body, TermUid a, TermUid b, TermUid mod) = 0;
    virtual void project(Location const &loc, TermUid termUid, BdLitVecUid body) = 0;
    virtual void project(Location const &loc, Sig sig) = 0;
    virtual bool reportComment() const = 0;
    virtual void comment(Location const &loc, String value, bool block) = 0;

    // {{{2 theory atoms

    virtual TheoryTermUid theorytermset(Location const &loc, TheoryOptermVecUid args) = 0;
    virtual TheoryTermUid theoryoptermlist(Location const &loc, TheoryOptermVecUid args) = 0;
    virtual TheoryTermUid theorytermopterm(Location const &loc, TheoryOptermUid opterm) = 0;
    virtual TheoryTermUid theorytermtuple(Location const &loc, TheoryOptermVecUid args) = 0;
    virtual TheoryTermUid theorytermfun(Location const &loc, String name, TheoryOptermVecUid args) = 0;
    virtual TheoryTermUid theorytermvalue(Location const &loc, Symbol val) = 0;
    virtual TheoryTermUid theorytermvar(Location const &loc, String var) = 0;

    virtual TheoryOptermUid theoryopterm(TheoryOpVecUid ops, TheoryTermUid term) = 0;
    virtual TheoryOptermUid theoryopterm(TheoryOptermUid opterm, TheoryOpVecUid ops, TheoryTermUid term) = 0;

    virtual TheoryOpVecUid theoryops() = 0;
    virtual TheoryOpVecUid theoryops(TheoryOpVecUid ops, String op) = 0;

    virtual TheoryOptermVecUid theoryopterms() = 0;
    virtual TheoryOptermVecUid theoryopterms(TheoryOptermVecUid opterms, Location const &loc, TheoryOptermUid opterm) = 0;
    virtual TheoryOptermVecUid theoryopterms(Location const &loc, TheoryOptermUid opterm, TheoryOptermVecUid opterms) = 0;

    virtual TheoryElemVecUid theoryelems() = 0;
    virtual TheoryElemVecUid theoryelems(TheoryElemVecUid elems, TheoryOptermVecUid opterms, LitVecUid cond) = 0;

    virtual TheoryAtomUid theoryatom(TermUid term, TheoryElemVecUid elems) = 0;
    virtual TheoryAtomUid theoryatom(TermUid term, TheoryElemVecUid elems, String op, Location const &loc, TheoryOptermUid opterm) = 0;

    // {{{2 theory definitions

    virtual TheoryOpDefUid theoryopdef(Location const &loc, String op, unsigned priority, TheoryOperatorType type) = 0;
    virtual TheoryOpDefVecUid theoryopdefs() = 0;
    virtual TheoryOpDefVecUid theoryopdefs(TheoryOpDefVecUid defs, TheoryOpDefUid def) = 0;

    virtual TheoryTermDefUid theorytermdef(Location const &loc, String name, TheoryOpDefVecUid defs, Logger &log) = 0;
    virtual TheoryAtomDefUid theoryatomdef(Location const &loc, String name, unsigned arity, String termDef, TheoryAtomType type) = 0;
    virtual TheoryAtomDefUid theoryatomdef(Location const &loc, String name, unsigned arity, String termDef, TheoryAtomType type, TheoryOpVecUid ops, String guardDef) = 0;

    virtual TheoryDefVecUid theorydefs() = 0;
    virtual TheoryDefVecUid theorydefs(TheoryDefVecUid defs, TheoryTermDefUid def) = 0;
    virtual TheoryDefVecUid theorydefs(TheoryDefVecUid defs, TheoryAtomDefUid def) = 0;

    virtual void theorydef(Location const &loc, String name, TheoryDefVecUid defs, Logger &log) = 0;

    // }}}2
};

// {{{1 declaration of NongroundProgramBuilder

using ULit = std::unique_ptr<Literal>;
using ULitVec = std::vector<ULit>;
using UHeadAggr = std::unique_ptr<HeadAggregate>;
using UBodyAggr = std::unique_ptr<BodyAggregate>;
using UStm = std::unique_ptr<Statement>;
using BoundVec = std::vector<Bound>;
class BodyAggrElem;
using BodyAggrElemVec = std::vector<BodyAggrElem>;
using CondLit = std::pair<ULit, ULitVec>;
using CondLitVec = std::vector<CondLit>;
class HeadAggrElem;
using HeadAggrElemVec = std::vector<HeadAggrElem>;
using UBodyAggrVec = std::vector<UBodyAggr>;
using IdVec = std::vector<std::pair<Location, String>>;

class NongroundProgramBuilder : public INongroundProgramBuilder {
public:
    using OutputPredicates = Output::OutputPredicates;

    NongroundProgramBuilder(Context &context, Program &prg, OutputPredicates &out_preds, Defines &defs, bool rewriteMinimize = false);
    NongroundProgramBuilder(NongroundProgramBuilder const &other) = delete;
    NongroundProgramBuilder(NongroundProgramBuilder &&other) noexcept = default;
    NongroundProgramBuilder &operator=(NongroundProgramBuilder &&other) noexcept = delete;
    NongroundProgramBuilder &operator=(NongroundProgramBuilder const &other) = delete;
    ~NongroundProgramBuilder() noexcept override;

    // {{{2 terms
    TermUid term(Location const &loc, Symbol val) override;                             // constant
    TermUid term(Location const &loc, String name) override;                            // variable
    TermUid term(Location const &loc, UnOp op, TermUid a) override;                     // unary operation
    TermUid term(Location const &loc, UnOp op, TermVecUid a) override;                  // unary operation
    TermUid term(Location const &loc, BinOp op, TermUid a, TermUid b) override;         // binary operation
    TermUid term(Location const &loc, TermUid a, TermUid b) override;                   // assignment
    TermUid term(Location const &loc, String name, TermVecVecUid a, bool lua) override; // function or lua function
    TermUid term(Location const &loc, TermVecUid args, bool forceTuple) override;       // a tuple term (or simply a term)
    TermUid pool(Location const &loc, TermVecUid args) override;                        // a pool term

    // {{{2 term vectors
    TermVecUid termvec() override;
    TermVecUid termvec(TermVecUid uid, TermUid term) override;
    // {{{2 id vectors
    IdVecUid idvec() override;
    IdVecUid idvec(IdVecUid uid, Location const &loc, String id) override;
    // {{{2 term vector vectors
    TermVecVecUid termvecvec() override;
    TermVecVecUid termvecvec(TermVecVecUid uid, TermVecUid termvecUid) override;
    // {{{2 literals
    LitUid boollit(Location const &loc, bool type) override;
    LitUid predlit(Location const &loc, NAF naf, TermUid term) override;
    RelLitVecUid rellitvec(Location const &loc, Relation rel, TermUid termUidLeft) override;
    RelLitVecUid rellitvec(Location const &loc, RelLitVecUid vecUidLeft, Relation rel, TermUid termUidRight) override;
    LitUid rellit(Location const &loc, NAF naf, TermUid termUidLeft, RelLitVecUid vecUidRight) override;
    // {{{2 literal vectors
    LitVecUid litvec() override;
    LitVecUid litvec(LitVecUid uid, LitUid literalUid) override;
    // {{{2 conditional literal vectors
    CondLitVecUid condlitvec() override;
    CondLitVecUid condlitvec(CondLitVecUid uid, LitUid lit, LitVecUid litvec) override;
    // {{{2 body aggregate elements
    BdAggrElemVecUid bodyaggrelemvec() override;
    BdAggrElemVecUid bodyaggrelemvec(BdAggrElemVecUid uid, TermVecUid termvec, LitVecUid litvec) override;
    // {{{2 head aggregate elements
    HdAggrElemVecUid headaggrelemvec() override;
    HdAggrElemVecUid headaggrelemvec(HdAggrElemVecUid uid, TermVecUid termvec, LitUid lit, LitVecUid litvec) override;
    // {{{2 bounds
    BoundVecUid boundvec() override;
    BoundVecUid boundvec(BoundVecUid uid, Relation rel, TermUid term) override;
    // {{{2 heads
    HdLitUid headlit(LitUid lit) override;
    HdLitUid headaggr(Location const &loc, TheoryAtomUid atom) override;
    HdLitUid headaggr(Location const &loc, AggregateFunction fun, BoundVecUid bounds, HdAggrElemVecUid headaggrelemvec) override;
    HdLitUid headaggr(Location const &loc, AggregateFunction fun, BoundVecUid bounds, CondLitVecUid condlitvec) override;
    HdLitUid disjunction(Location const &loc, CondLitVecUid condlitvec) override;
    // {{{2 bodies
    BdLitVecUid body() override;
    BdLitVecUid bodylit(BdLitVecUid body, LitUid bodylit) override;
    BdLitVecUid bodyaggr(BdLitVecUid body, Location const &loc, NAF naf, TheoryAtomUid atom) override;
    BdLitVecUid bodyaggr(BdLitVecUid body, Location const &loc, NAF naf, AggregateFunction fun, BoundVecUid bounds, BdAggrElemVecUid bodyaggrelemvec) override;
    BdLitVecUid bodyaggr(BdLitVecUid body, Location const &loc, NAF naf, AggregateFunction fun, BoundVecUid bounds, CondLitVecUid condlitvec) override;
    BdLitVecUid conjunction(BdLitVecUid body, Location const &loc, LitUid head, LitVecUid litvec) override;
    // {{{2 statements
    void rule(Location const &loc, HdLitUid head) override;
    void rule(Location const &loc, HdLitUid head, BdLitVecUid body) override;
    void define(Location const &loc, String name, TermUid value, bool defaultDef, Logger &log) override;
    void optimize(Location const &loc, TermUid weight, TermUid priority, TermVecUid cond, BdLitVecUid body) override;
    void showsig(Location const &loc, Sig sig) override;
    void defined(Location const &loc, Sig sig) override;
    void show(Location const &loc, TermUid t, BdLitVecUid body) override;
    void script(Location const &loc, String type, String code) override;
    void block(Location const &loc, String name, IdVecUid args) override;
    void external(Location const &loc, TermUid head, BdLitVecUid body, TermUid type) override;
    void edge(Location const &loc, TermVecVecUid edges, BdLitVecUid body) override;
    void heuristic(Location const &loc, TermUid termUid, BdLitVecUid body, TermUid a, TermUid b, TermUid mod) override;
    void project(Location const &loc, TermUid termUid, BdLitVecUid body) override;
    void project(Location const &loc, Sig sig) override;
    bool reportComment() const override { return false; }
    void comment(Location const &loc, String value, bool block) override {
        static_cast<void>(loc);
        static_cast<void>(value);
        static_cast<void>(block);
    }
    // }}}2
    // {{{2 theory atoms
    TheoryTermUid theorytermset(Location const &loc, TheoryOptermVecUid args) override;
    TheoryTermUid theoryoptermlist(Location const &loc, TheoryOptermVecUid args) override;
    TheoryTermUid theorytermopterm(Location const &loc, TheoryOptermUid opterm) override;
    TheoryTermUid theorytermtuple(Location const &loc, TheoryOptermVecUid args) override;
    TheoryTermUid theorytermfun(Location const &loc, String name, TheoryOptermVecUid args) override;
    TheoryTermUid theorytermvalue(Location const &loc, Symbol val) override;
    TheoryTermUid theorytermvar(Location const &loc, String var) override;

    TheoryOptermUid theoryopterm(TheoryOpVecUid ops, TheoryTermUid term) override;
    TheoryOptermUid theoryopterm(TheoryOptermUid opterm, TheoryOpVecUid ops, TheoryTermUid term) override;

    TheoryOpVecUid theoryops() override;
    TheoryOpVecUid theoryops(TheoryOpVecUid ops, String op) override;

    TheoryOptermVecUid theoryopterms() override;
    TheoryOptermVecUid theoryopterms(TheoryOptermVecUid opterms, Location const &loc, TheoryOptermUid opterm) override;
    TheoryOptermVecUid theoryopterms(Location const &loc, TheoryOptermUid opterm, TheoryOptermVecUid opterms) override;

    TheoryElemVecUid theoryelems() override;
    TheoryElemVecUid theoryelems(TheoryElemVecUid elems, TheoryOptermVecUid opterms, LitVecUid cond) override;

    TheoryAtomUid theoryatom(TermUid term, TheoryElemVecUid elems) override;
    TheoryAtomUid theoryatom(TermUid term, TheoryElemVecUid elems, String op, Location const &loc, TheoryOptermUid opterm) override;

    // {{{2 theory definitions

    TheoryOpDefUid theoryopdef(Location const &loc, String op, unsigned priority, TheoryOperatorType type) override;
    TheoryOpDefVecUid theoryopdefs() override;
    TheoryOpDefVecUid theoryopdefs(TheoryOpDefVecUid defs, TheoryOpDefUid def) override;

    TheoryTermDefUid theorytermdef(Location const &loc, String name, TheoryOpDefVecUid defs, Logger &log) override;
    TheoryAtomDefUid theoryatomdef(Location const &loc, String name, unsigned arity, String termDef, TheoryAtomType type) override;
    TheoryAtomDefUid theoryatomdef(Location const &loc, String name, unsigned arity, String termDef, TheoryAtomType type, TheoryOpVecUid ops, String guardDef) override;

    TheoryDefVecUid theorydefs() override;
    TheoryDefVecUid theorydefs(TheoryDefVecUid defs, TheoryTermDefUid def) override;
    TheoryDefVecUid theorydefs(TheoryDefVecUid defs, TheoryAtomDefUid def) override;

    void theorydef(Location const &loc, String name, TheoryDefVecUid defs, Logger &log) override;

    // }}}2

private:
    using Terms            = Indexed<UTerm, TermUid>;
    using TermVecs         = Indexed<UTermVec, TermVecUid>;
    using TermVecVecs      = Indexed<UTermVecVec, TermVecVecUid>;
    using IdVecs           = Indexed<IdVec, IdVecUid>;
    using Lits             = Indexed<ULit, LitUid>;
    using LitVecs          = Indexed<ULitVec, LitVecUid>;
    using RelLitVecs       = Indexed<std::vector<std::pair<Relation, UTerm>>, RelLitVecUid>;
    using BodyAggrElemVecs = Indexed<BodyAggrElemVec, BdAggrElemVecUid>;
    using CondLitVecs      = Indexed<CondLitVec, CondLitVecUid>;
    using HeadAggrElemVecs = Indexed<HeadAggrElemVec, HdAggrElemVecUid>;
    using Bodies           = Indexed<UBodyAggrVec, BdLitVecUid>;
    using Heads            = Indexed<UHeadAggr, HdLitUid>;
    using Statements       = std::vector<UStm>;
    using Bounds           = Indexed<BoundVec, BoundVecUid>;
    using VarVals          = std::unordered_map<String, Term::SVal>;

    using TheoryOpVecs      = Indexed<std::vector<String>, TheoryOpVecUid>;
    using TheoryTerms       = Indexed<Output::UTheoryTerm, TheoryTermUid>;
    using RawTheoryTerms    = Indexed<Output::RawTheoryTerm, TheoryOptermUid>;
    using RawTheoryTermVecs = Indexed<std::vector<Output::UTheoryTerm>, TheoryOptermVecUid>;
    using TheoryElementVecs = Indexed<std::vector<TheoryElement>, TheoryElemVecUid>;
    using TheoryAtoms       = Indexed<TheoryAtom, TheoryAtomUid>;

    using TheoryOpDefs = Indexed<TheoryOpDef, TheoryOpDefUid>;
    using TheoryOpDefVecs = Indexed<std::vector<TheoryOpDef>, TheoryOpDefVecUid>;
    using TheoryTermDefs = Indexed<TheoryTermDef, TheoryTermDefUid>;
    using TheoryAtomDefs = Indexed<TheoryAtomDef, TheoryAtomDefUid>;
    using TheoryDefVecs = Indexed<std::pair<std::vector<TheoryTermDef>, std::vector<TheoryAtomDef>>, TheoryDefVecUid>;

    Terms               terms_;
    TermVecs            termvecs_;
    TermVecVecs         termvecvecs_;
    IdVecs              idvecs_;
    Lits                lits_;
    LitVecs             litvecs_;
    RelLitVecs          rellitvecs_;
    BodyAggrElemVecs    bodyaggrelemvecs_;
    HeadAggrElemVecs    headaggrelemvecs_;
    CondLitVecs         condlitvecs_;
    Bounds              bounds_;
    Bodies              bodies_;
    Heads               heads_;
    VarVals             vals_;
    TheoryOpVecs        theoryOpVecs_;
    TheoryTerms         theoryTerms_;
    RawTheoryTerms      theoryOpterms_;
    RawTheoryTermVecs   theoryOptermVecs_;
    TheoryElementVecs   theoryElems_;
    TheoryAtoms         theoryAtoms_;
    TheoryOpDefs        theoryOpDefs_;
    TheoryOpDefVecs     theoryOpDefVecs_;
    TheoryTermDefs      theoryTermDefs_;
    TheoryAtomDefs      theoryAtomDefs_;
    TheoryDefVecs       theoryDefVecs_;
    Context            &context_;
    Program            &prg_;
    OutputPredicates   &output_preds_;
    Defines            &defs_;
    bool                rewriteMinimize_;
};

// }}}1

} } // namespace Input Gringo

#endif // _GRINGO_PROGRAMBUILDER_HH
