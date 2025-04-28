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

#include "gringo/input/nongroundparser.hh"
#include "gringo/input/programbuilder.hh"
#include "input/nongroundgrammar/grammar.hh"
#include "gringo/symbol.hh"
#include "tests/tests.hh"

#include <climits>
#include <sstream>

namespace Gringo { namespace Input { namespace Test {

namespace {

// {{{ declaration of TestNongroundProgramBuilder

class TestNongroundProgramBuilder : public INongroundProgramBuilder {
public:

    // {{{ terms
    virtual TermUid term(Location const &loc, Symbol val) override;
    virtual TermUid term(Location const &loc, String name) override;
    virtual TermUid term(Location const &loc, UnOp op, TermUid a) override;
    virtual TermUid term(Location const &loc, UnOp op, TermVecUid a) override;
    virtual TermUid term(Location const &loc, BinOp op, TermUid a, TermUid b) override;
    virtual TermUid term(Location const &loc, TermUid a, TermUid b) override;
    virtual TermUid term(Location const &loc, String name, TermVecVecUid b, bool lua) override;
    virtual TermUid term(Location const &loc, TermVecUid args, bool forceTuple) override;
    virtual TermUid pool(Location const &loc, TermVecUid args) override;
    // }}}
    // }}}
    // {{{ id vectors
    virtual IdVecUid idvec() override;
    virtual IdVecUid idvec(IdVecUid uid, Location const &loc, String id) override;
    // }}}
    // {{{ term vectors
    virtual TermVecUid termvec() override;
    virtual TermVecUid termvec(TermVecUid uid, TermUid term) override;
    // }}}
    // {{{ term vector vectors
    virtual TermVecVecUid termvecvec() override;
    virtual TermVecVecUid termvecvec(TermVecVecUid uid, TermVecUid termvecUid) override;
    // }}}
    // {{{ literals
    virtual LitUid boollit(Location const &loc, bool type) override;
    virtual LitUid predlit(Location const &loc, NAF naf, TermUid termUid) override;
    virtual RelLitVecUid rellitvec(Location const &loc, Relation rel, TermUid termUidLeft) override;
    virtual RelLitVecUid rellitvec(Location const &loc, RelLitVecUid vecUidLeft, Relation rel, TermUid termUidRight) override;
    virtual LitUid rellit(Location const &loc, NAF naf, TermUid termUidLeft, RelLitVecUid vecUidRight) override;
    // }}}
    // {{{ literal vectors
    virtual LitVecUid litvec() override;
    virtual LitVecUid litvec(LitVecUid uid, LitUid literalUid) override;
    // }}}
    // {{{ count aggregate elements (body/head)
    virtual CondLitVecUid condlitvec() override;
    virtual CondLitVecUid condlitvec(CondLitVecUid uid, LitUid lit, LitVecUid litvec) override;
    // }}}
    // {{{ body aggregate elements
    virtual BdAggrElemVecUid bodyaggrelemvec() override;
    virtual BdAggrElemVecUid bodyaggrelemvec(BdAggrElemVecUid uid, TermVecUid termvec, LitVecUid litvec) override;
    // }}}
    // {{{ head aggregate elements
    virtual HdAggrElemVecUid headaggrelemvec() override;
    virtual HdAggrElemVecUid headaggrelemvec(HdAggrElemVecUid uid, TermVecUid termvec, LitUid lit, LitVecUid litvec) override;
    // }}}
    // {{{ bounds
    virtual BoundVecUid boundvec() override;
    virtual BoundVecUid boundvec(BoundVecUid uid, Relation rel, TermUid term) override;
    // }}}
    // {{{ heads
    virtual HdLitUid headlit(LitUid lit) override;
    virtual HdLitUid headaggr(Location const &loc, TheoryAtomUid atom) override;
    virtual HdLitUid headaggr(Location const &loc, AggregateFunction fun, BoundVecUid bounds, HdAggrElemVecUid headaggrelemvec) override;
    virtual HdLitUid headaggr(Location const &loc, AggregateFunction fun, BoundVecUid bounds, CondLitVecUid headaggrelemvec) override;
    virtual HdLitUid disjunction(Location const &loc, CondLitVecUid condlitvec) override;
    // }}}
    // {{{ bodies
    virtual BdLitVecUid body() override;
    virtual BdLitVecUid bodylit(BdLitVecUid body, LitUid bodylit) override;
    virtual BdLitVecUid bodyaggr(BdLitVecUid body, Location const &loc, NAF naf, TheoryAtomUid atom) override;
    virtual BdLitVecUid bodyaggr(BdLitVecUid body, Location const &loc, NAF naf, AggregateFunction fun, BoundVecUid bounds, BdAggrElemVecUid bodyaggrelemvec) override;
    virtual BdLitVecUid bodyaggr(BdLitVecUid body, Location const &loc, NAF naf, AggregateFunction fun, BoundVecUid bounds, CondLitVecUid bodyaggrelemvec) override;
    virtual BdLitVecUid conjunction(BdLitVecUid body, Location const &loc, LitUid head, LitVecUid litvec) override;
    // }}}
    // {{{ statements
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
    bool reportComment() const override { return true; }
    void comment(Location const &loc, String value, bool block) override;

    // }}}
    // {{{ theory atoms
    virtual TheoryTermUid theorytermset(Location const &loc, TheoryOptermVecUid args) override;
    virtual TheoryTermUid theoryoptermlist(Location const &loc, TheoryOptermVecUid args) override;
    virtual TheoryTermUid theorytermopterm(Location const &loc, TheoryOptermUid opterm) override;
    virtual TheoryTermUid theorytermtuple(Location const &loc, TheoryOptermVecUid args) override;
    virtual TheoryTermUid theorytermfun(Location const &loc, String name, TheoryOptermVecUid args) override;
    virtual TheoryTermUid theorytermvalue(Location const &loc, Symbol val) override;
    virtual TheoryTermUid theorytermvar(Location const &loc, String var) override;

    virtual TheoryOptermUid theoryopterm(TheoryOpVecUid ops, TheoryTermUid term) override;
    virtual TheoryOptermUid theoryopterm(TheoryOptermUid opterm, TheoryOpVecUid ops, TheoryTermUid term) override;

    virtual TheoryOpVecUid theoryops() override;
    virtual TheoryOpVecUid theoryops(TheoryOpVecUid ops, String op) override;

    virtual TheoryOptermVecUid theoryopterms() override;
    virtual TheoryOptermVecUid theoryopterms(TheoryOptermVecUid opterms, Location const &loc, TheoryOptermUid opterm) override;
    virtual TheoryOptermVecUid theoryopterms(Location const &loc, TheoryOptermUid opterm, TheoryOptermVecUid opterms) override;

    virtual TheoryElemVecUid theoryelems() override;
    virtual TheoryElemVecUid theoryelems(TheoryElemVecUid elems, TheoryOptermVecUid opterms, LitVecUid cond) override;

    virtual TheoryAtomUid theoryatom(TermUid term, TheoryElemVecUid elems) override;
    virtual TheoryAtomUid theoryatom(TermUid term, TheoryElemVecUid elems, String op, Location const &loc, TheoryOptermUid opterm) override;
    // }}}
    // {{{ theory definitions

    virtual TheoryOpDefUid theoryopdef(Location const &loc, String op, unsigned priority, TheoryOperatorType type) override;
    virtual TheoryOpDefVecUid theoryopdefs() override;
    virtual TheoryOpDefVecUid theoryopdefs(TheoryOpDefVecUid defs, TheoryOpDefUid def) override;

    virtual TheoryTermDefUid theorytermdef(Location const &loc, String name, TheoryOpDefVecUid defs, Logger &log) override;
    virtual TheoryAtomDefUid theoryatomdef(Location const &loc, String name, unsigned arity, String termDef, TheoryAtomType type) override;
    virtual TheoryAtomDefUid theoryatomdef(Location const &loc, String name, unsigned arity, String termDef, TheoryAtomType type, TheoryOpVecUid ops, String guardDef) override;

    virtual TheoryDefVecUid theorydefs() override;
    virtual TheoryDefVecUid theorydefs(TheoryDefVecUid defs, TheoryTermDefUid def) override;
    virtual TheoryDefVecUid theorydefs(TheoryDefVecUid defs, TheoryAtomDefUid def) override;

    virtual void theorydef(Location const &loc, String name, TheoryDefVecUid defs, Logger &log) override;

    // }}}

    std::string toString();

    virtual ~TestNongroundProgramBuilder();

private:
    // {{{ typedefs
    using StringVec = std::vector<std::string>;
    using StringVecVec = std::vector<StringVec>;
    using StringPair = std::pair<std::string, std::string>;
    using StringPairVec = std::vector<StringPair>;
    using TermUidVec = std::vector<TermUid>;
    using TermVecUidVec = std::vector<TermVecUid>;
    using LitUidVec = std::vector<LitUid>;

    using IdVecs = Indexed<StringVec, IdVecUid>;
    using Terms = Indexed<std::string, TermUid>;
    using TermVecs = Indexed<StringVec, TermVecUid>;
    using TermVecVecs = Indexed<StringVecVec, TermVecVecUid>;
    using Lits = Indexed<std::string, LitUid>;
    using LitVecs = Indexed<StringVec, LitVecUid>;
    using RelLitVecs = Indexed<StringVec, RelLitVecUid>;
    using BodyAggrElemVecs = Indexed<StringVec, BdAggrElemVecUid>;
    using CondLitVecs = Indexed<StringVec, CondLitVecUid>;
    using HeadAggrElemVecs = Indexed<StringVec, HdAggrElemVecUid>;
    using Bodies = Indexed<StringVec, BdLitVecUid>;
    using Heads = Indexed<std::string, HdLitUid>;
    using Bounds = Indexed<StringPairVec, BoundVecUid>;
    using Statements = std::vector<std::string>;

    using TheoryOps = Indexed<StringVec, TheoryOpVecUid>;
    using TheoryTerms = Indexed<std::string, TheoryTermUid>;
    using TheoryOpterms = Indexed<std::string, TheoryOptermUid>;
    using TheoryOptermVecs = Indexed<StringVec, TheoryOptermVecUid>;
    using TheoryElemVecs = Indexed<StringVec, TheoryElemVecUid>;
    using TheoryAtoms = Indexed<std::string, TheoryAtomUid>;

    using TheoryOpDefs = Indexed<std::string, TheoryOpDefUid>;
    using TheoryOpDefVecs = Indexed<StringVec, TheoryOpDefVecUid>;
    using TheoryTermDefs = Indexed<std::string, TheoryTermDefUid>;
    using TheoryAtomDefs = Indexed<std::string, TheoryAtomDefUid>;
    using TheoryDefVecs = Indexed<StringVec, TheoryDefVecUid>;

    // }}}
    // {{{ auxiliary functions
    std::string str();
    void print(StringVec const &vec, char const *sep);
    void print(StringVecVec const &vec);
    void print(AggregateFunction fun, BoundVecUid boundvecuid, StringVec const &elems);
    void print(NAF naf);
    // }}}
    // {{{ member variables
    IdVecs idvecs_;
    Terms terms_;
    TermVecs termvecs_;
    TermVecVecs termvecvecs_;
    Lits lits_;
    LitVecs litvecs_;
    RelLitVecs rellitvecs_;
    BodyAggrElemVecs bodyaggrelemvecs_;
    CondLitVecs condlitvecs_;
    HeadAggrElemVecs headaggrelemvecs_;
    Bodies bodies_;
    Heads heads_;
    Bounds bounds_;
    Statements statements_;
    std::stringstream current_;

    TheoryOps theoryOps_;
    TheoryTerms theoryTerms_;
    TheoryOpterms theoryOpterms_;
    TheoryOptermVecs theoryOptermVecs_;
    TheoryElemVecs theoryElemVecs_;
    TheoryAtoms theoryAtoms_;

    TheoryOpDefs theoryOpDefs_;
    TheoryOpDefVecs theoryOpDefVecs_;
    TheoryTermDefs theoryTermDefs_;
    TheoryAtomDefs theoryAtomDefs_;
    TheoryDefVecs theoryDefVecs_;
    // }}}
};

// }}}
// {{{ definition of TestNongroundProgramBuilder

// {{{ id vectors

IdVecUid TestNongroundProgramBuilder::idvec() {
    return idvecs_.emplace();
}

IdVecUid TestNongroundProgramBuilder::idvec(IdVecUid uid, Location const &, String id) {
    idvecs_[uid].emplace_back(id.c_str());
    return uid;
}

// }}}
// {{{ terms

TermUid TestNongroundProgramBuilder::term(Location const &, Symbol val) {
    current_ << val;
    return terms_.emplace(str());
}

TermUid TestNongroundProgramBuilder::term(Location const &, String name) {
    current_ << name;
    return terms_.emplace(str());
}

TermUid TestNongroundProgramBuilder::term(Location const &, UnOp op, TermUid a) {
    if(op == UnOp::ABS) { current_ << "|"; }
    else { current_ << op; }
    current_ << terms_.erase(a) << (op == UnOp::ABS ? "|" : "");
    return terms_.emplace(str());
}

TermUid TestNongroundProgramBuilder::term(Location const &, UnOp op, TermVecUid a) {
    auto v = termvecs_.erase(a);
    if(op == UnOp::ABS) { current_ << "|"; }
    else { current_ << op << (v.size() > 1 ? "(" : ""); }
    print(v, ";");
    if(op == UnOp::ABS) { current_ << "|"; }
    else if (v.size() > 1) { current_ << ")"; }
    return terms_.emplace(str());
}

TermUid TestNongroundProgramBuilder::term(Location const &, BinOp op, TermUid a, TermUid b) {
    current_ << "(" << terms_.erase(a) << op << terms_.erase(b) << ")";
    return terms_.emplace(str());
}

TermUid TestNongroundProgramBuilder::term(Location const &, TermUid a, TermUid b) {
    current_ << "(" << terms_.erase(a) << ".." << terms_.erase(b) << ")";
    return terms_.emplace(str());
}

TermUid TestNongroundProgramBuilder::term(Location const &, String name, TermVecVecUid a, bool lua) {
    assert(name != "");
    assert(!termvecvecs_[a].empty());
    bool nempty = lua || termvecvecs_[a].size() > 1 || !termvecvecs_[a].front().empty();
    if (lua) { current_ << "@"; }
    current_ << name;
    if (nempty) { current_ << "("; }
    print(termvecvecs_.erase(a));
    if (nempty) { current_ << ")"; }
    return terms_.emplace(str());
}

TermUid TestNongroundProgramBuilder::term(Location const &, TermVecUid args, bool forceTuple) {
    current_ << "(";
    print(termvecs_.erase(args), ",");
    if (forceTuple) { current_ << ","; }
    current_ << ")";
    return terms_.emplace(str());
}

TermUid TestNongroundProgramBuilder::pool(Location const &, TermVecUid args) {
    current_ << "(";
    print(termvecs_.erase(args), ";");
    current_ << ")";
    return terms_.emplace(str());
}

// }}}
// {{{ term vectors

TermVecUid TestNongroundProgramBuilder::termvec() {
    return termvecs_.emplace();
}

TermVecUid TestNongroundProgramBuilder::termvec(TermVecUid uid, TermUid term) {
    termvecs_[uid].emplace_back(terms_.erase(term));
    return uid;
}

// }}}
// {{{ term vector vectors

TermVecVecUid TestNongroundProgramBuilder::termvecvec() {
    return termvecvecs_.emplace();
}

TermVecVecUid TestNongroundProgramBuilder::termvecvec(TermVecVecUid uid, TermVecUid termvecUid) {
    termvecvecs_[uid].push_back(termvecs_.erase(termvecUid));
    return uid;
}

// }}}
// {{{ literals

LitUid TestNongroundProgramBuilder::boollit(Location const &, bool type) {
    return lits_.emplace(type ? "#true" : "#false");
}

LitUid TestNongroundProgramBuilder::predlit(Location const &, NAF naf, TermUid termUid) {
    print(naf);
    current_ << terms_.erase(termUid);
    return lits_.emplace(str());
}

RelLitVecUid TestNongroundProgramBuilder::rellitvec(Location const &loc, Relation rel, TermUid termUidLeft) {
    auto uid = rellitvecs_.emplace();
    return rellitvec(loc, uid, rel, termUidLeft);
}

RelLitVecUid TestNongroundProgramBuilder::rellitvec(Location const &loc, RelLitVecUid vecUidLeft, Relation rel, TermUid termUidRight) {
    current_<< rel << terms_.erase(termUidRight);
    rellitvecs_[vecUidLeft].emplace_back(str());
    return vecUidLeft;
}

LitUid TestNongroundProgramBuilder::rellit(Location const &loc, NAF naf, TermUid termUidLeft, RelLitVecUid vecUidRight) {
    print(naf);
    current_ << terms_.erase(termUidLeft);
    print(rellitvecs_.erase(vecUidRight), "");
    return lits_.emplace(str());
}

// }}}
// {{{ literal vectors

LitVecUid TestNongroundProgramBuilder::litvec() {
    return litvecs_.emplace();
}

LitVecUid TestNongroundProgramBuilder::litvec(LitVecUid uid, LitUid literalUid) {
    litvecs_[uid].emplace_back(lits_.erase(literalUid));
    return uid;
}

// }}}
// {{{ count aggregate elements (body/head)

CondLitVecUid TestNongroundProgramBuilder::condlitvec() {
    return condlitvecs_.emplace();
}

CondLitVecUid TestNongroundProgramBuilder::condlitvec(CondLitVecUid uid, LitUid lit, LitVecUid litvec) {
    current_ << lits_.erase(lit);
    current_ << ":";
    print(litvecs_.erase(litvec), ",");
    condlitvecs_[uid].emplace_back(str());
    return uid;
}

// }}}
// {{{ body aggregate elements

BdAggrElemVecUid TestNongroundProgramBuilder::bodyaggrelemvec() {
    return bodyaggrelemvecs_.emplace();
}

BdAggrElemVecUid TestNongroundProgramBuilder::bodyaggrelemvec(BdAggrElemVecUid uid, TermVecUid termvec, LitVecUid litvec) {
    print(termvecs_.erase(termvec), ",");
    current_ << ":";
    print(litvecs_.erase(litvec), ",");
    bodyaggrelemvecs_[uid].emplace_back(str());
    return uid;
}

// }}}
// {{{ head aggregate elements

HdAggrElemVecUid TestNongroundProgramBuilder::headaggrelemvec() {
    return headaggrelemvecs_.emplace();
}

HdAggrElemVecUid TestNongroundProgramBuilder::headaggrelemvec(HdAggrElemVecUid uid, TermVecUid termvec, LitUid lit, LitVecUid litvec) {
    print(termvecs_.erase(termvec), ",");
    current_ << ":" << lits_.erase(lit) << ":";
    print(litvecs_.erase(litvec), ",");
    headaggrelemvecs_[uid].emplace_back(str());
    return uid;
}

// }}}
// {{{ bounds

BoundVecUid TestNongroundProgramBuilder::boundvec() {
    return bounds_.emplace();
}

BoundVecUid TestNongroundProgramBuilder::boundvec(BoundVecUid uid, Relation rel, TermUid term) {
    std::stringstream ss;
    current_ << terms_.erase(term);
    ss << rel << current_.str();
    current_ << inv(rel);
    bounds_[uid].emplace_back(str(), ss.str());
    return uid;
}

// }}}
// {{{ heads

HdLitUid TestNongroundProgramBuilder::headlit(LitUid lit) {
    return heads_.emplace(lits_.erase(lit));
}

HdLitUid TestNongroundProgramBuilder::headaggr(Location const &, TheoryAtomUid atom) {
    return heads_.emplace(theoryAtoms_.erase(atom));
}

HdLitUid TestNongroundProgramBuilder::headaggr(Location const &, AggregateFunction fun, BoundVecUid boundvecuid, HdAggrElemVecUid headaggrelemvec) {
    print(fun, boundvecuid, headaggrelemvecs_.erase(headaggrelemvec));
    return heads_.emplace(str());
}

HdLitUid TestNongroundProgramBuilder::headaggr(Location const &, AggregateFunction fun, BoundVecUid boundvecuid, CondLitVecUid headaggrelemvec) {
    print(fun, boundvecuid, condlitvecs_.erase(headaggrelemvec));
    return heads_.emplace(str());
}

HdLitUid TestNongroundProgramBuilder::disjunction(Location const &, CondLitVecUid condlitvec) {
    print(condlitvecs_.erase(condlitvec), ";");
    return heads_.emplace(str());
}

// }}}
// {{{ bodies

BdLitVecUid TestNongroundProgramBuilder::body() {
    return bodies_.emplace();
}

BdLitVecUid TestNongroundProgramBuilder::bodylit(BdLitVecUid uid, LitUid lit) {
    bodies_[uid].emplace_back(lits_.erase(lit));
    return uid;
}

BdLitVecUid TestNongroundProgramBuilder::bodyaggr(BdLitVecUid body, Location const &, NAF naf, TheoryAtomUid atom) {
    print(naf);
    current_ << theoryAtoms_.erase(atom);
    bodies_[body].emplace_back(str());
    return body;
}

BdLitVecUid TestNongroundProgramBuilder::bodyaggr(BdLitVecUid uid, Location const &, NAF naf, AggregateFunction fun, BoundVecUid bounds, BdAggrElemVecUid bodyaggrelemvec) {
    print(naf);
    print(fun, bounds, bodyaggrelemvecs_.erase(bodyaggrelemvec));
    bodies_[uid].emplace_back(str());
    return uid;
}

BdLitVecUid TestNongroundProgramBuilder::bodyaggr(BdLitVecUid uid, Location const &, NAF naf, AggregateFunction fun, BoundVecUid bounds, CondLitVecUid bodyaggrelemvec) {
    print(naf);
    print(fun, bounds, condlitvecs_.erase(bodyaggrelemvec));
    bodies_[uid].emplace_back(str());
    return uid;
}

BdLitVecUid TestNongroundProgramBuilder::conjunction(BdLitVecUid uid, Location const &, LitUid head, LitVecUid litvec) {
    current_ << lits_.erase(head) << ":";
    print(litvecs_.erase(litvec), ",");
    bodies_[uid].emplace_back(str());
    return uid;
}

// }}}
// {{{ statements

void TestNongroundProgramBuilder::rule(Location const &, HdLitUid head) {
    current_ << heads_.erase(head) << ".";
    statements_.emplace_back(str());
}

void TestNongroundProgramBuilder::rule(Location const &, HdLitUid head, BdLitVecUid bodyuid) {
    current_ << heads_.erase(head);
    StringVec body(bodies_.erase(bodyuid));
    if (!body.empty()) {
        current_ << ":-";
        print(body, ";");
    }
    current_ << ".";
    statements_.emplace_back(str());
}

void TestNongroundProgramBuilder::define(Location const &, String name, TermUid value, bool def, Logger &) {
    current_ << "#const " << name << "=" << terms_.erase(value) << ".";
    if (!def) { current_ << " [override]"; }
    statements_.emplace_back(str());
}

void TestNongroundProgramBuilder::optimize(Location const &, TermUid weight, TermUid priority, TermVecUid cond, BdLitVecUid body) {
    current_ << ":~";
    StringVec bd(bodies_.erase(body));
    print(bd, ";");
    current_ << ".[" << terms_.erase(weight) << "@" << terms_.erase(priority);
    StringVec cd(termvecs_.erase(cond));
    if (!cd.empty()) {
        current_ << ",";
        print(cd, ",");
    }
    current_ << "]";
    statements_.emplace_back(str());
}

void TestNongroundProgramBuilder::showsig(Location const &, Sig sig) {
    current_ << "#showsig " << sig << ".";
    statements_.emplace_back(str());
}

void TestNongroundProgramBuilder::defined(Location const &, Sig sig) {
    current_ << "#defined " << sig << ".";
    statements_.emplace_back(str());
}

void TestNongroundProgramBuilder::show(Location const &, TermUid t, BdLitVecUid bodyuid) {
    current_ << "#show " << terms_.erase(t);
    StringVec body(bodies_.erase(bodyuid));
    if (!body.empty()) {
        current_ << ":";
        print(body, ";");
    }
    current_ << ".";
    statements_.emplace_back(str());
}

void TestNongroundProgramBuilder::script(Location const &, String type, String code) {
    current_ << "#script(" << type << ")\n" << code << "\n#end.";
    statements_.emplace_back(str());
}

void TestNongroundProgramBuilder::block(Location const &, String name, IdVecUid args) {
    current_ << "#program " << name << "(";
    print(idvecs_.erase(args), ",");
    current_ << ").";
    statements_.emplace_back(str());
}

void TestNongroundProgramBuilder::external(Location const &, TermUid head, BdLitVecUid bodyuid, TermUid type) {
    current_ << "#external " << terms_.erase(head);
    StringVec body(bodies_.erase(bodyuid));
    if (!body.empty()) {
        current_ << ":";
        print(body, ";");
    }
    current_ << ". [" << terms_.erase(type) << "]";
    statements_.emplace_back(str());
}

void TestNongroundProgramBuilder::edge(Location const &, TermVecVecUid edges, BdLitVecUid bodyuid) {
    current_ << "#edge(";
    print(termvecvecs_.erase(edges));
    current_ << ")";
    StringVec body(bodies_.erase(bodyuid));
    if (!body.empty()) {
        current_ << ":";
        print(body, ";");
    }
    current_ << ".";
    statements_.emplace_back(str());
}

void TestNongroundProgramBuilder::heuristic(Location const &, TermUid termUid, BdLitVecUid bodyuid, TermUid a, TermUid b, TermUid mod) {
    current_ << "#heuristic " << terms_.erase(termUid);
    StringVec body(bodies_.erase(bodyuid));
    if (!body.empty()) {
        current_ << ":";
        print(body, ";");
    }
    current_ << ".[" << terms_.erase(a) << "@" << terms_.erase(b) << "," << terms_.erase(mod) << "]";
    statements_.emplace_back(str());
}

void TestNongroundProgramBuilder::project(Location const &, TermUid termUid, BdLitVecUid bodyuid) {
    current_ << "#project " << terms_.erase(termUid);
    StringVec body(bodies_.erase(bodyuid));
    if (!body.empty()) {
        current_ << ":";
        print(body, ";");
    }
    current_ << ".";
    statements_.emplace_back(str());

}

void TestNongroundProgramBuilder::project(Location const &, Sig sig) {
    current_ << "#project " << sig << ".";
    statements_.emplace_back(str());
}

void TestNongroundProgramBuilder::comment(Location const &loc, String value, bool block) {
    static_cast<void>(loc);
    static_cast<void>(block);
    current_ << value;
    statements_.emplace_back(str());
}

// }}}
// {{{ theory atoms

TheoryTermUid TestNongroundProgramBuilder::theorytermset(Location const &, TheoryOptermVecUid args) {
    current_ << "{";
    print(theoryOptermVecs_.erase(args), " ; ");
    current_ << "}";
    return theoryTerms_.emplace(str());
}
TheoryTermUid TestNongroundProgramBuilder::theoryoptermlist(Location const &, TheoryOptermVecUid args) {
    current_ << "[";
    print(theoryOptermVecs_.erase(args), " ; ");
    current_ << "]";
    return theoryTerms_.emplace(str());
}

TheoryTermUid TestNongroundProgramBuilder::theorytermopterm(Location const &, TheoryOptermUid opterm) {
    current_ << "(" << theoryOpterms_.erase(opterm) << ")";
    return theoryTerms_.emplace(str());
}

TheoryTermUid TestNongroundProgramBuilder::theorytermtuple(Location const &, TheoryOptermVecUid args) {
    current_ << "(";
    auto size = theoryOptermVecs_[args].size();
    print(theoryOptermVecs_.erase(args), ",");
    if (size == 1) { current_ << ","; }
    current_ << ")";
    return theoryTerms_.emplace(str());
}

TheoryTermUid TestNongroundProgramBuilder::theorytermfun(Location const &, String name, TheoryOptermVecUid args) {
    current_ << name << "(";
    print(theoryOptermVecs_.erase(args), ",");
    current_ << ")";
    return theoryTerms_.emplace(str());
}

TheoryTermUid TestNongroundProgramBuilder::theorytermvalue(Location const &, Symbol val) {
    current_ << val;
    return theoryTerms_.emplace(str());
}

TheoryTermUid TestNongroundProgramBuilder::theorytermvar(Location const &, String var) {
    current_ << var;
    return theoryTerms_.emplace(str());
}

TheoryOptermUid TestNongroundProgramBuilder::theoryopterm(TheoryOpVecUid ops, TheoryTermUid term) {
    auto to = theoryOps_.erase(ops);
    print(to, " ");
    if (!to.empty()) { current_ << " "; }
    current_ << theoryTerms_.erase(term);
    return theoryOpterms_.emplace(str());

}

TheoryOptermUid TestNongroundProgramBuilder::theoryopterm(TheoryOptermUid opterm, TheoryOpVecUid ops, TheoryTermUid term) {
    auto to = theoryOps_.erase(ops);
    if (!to.empty()) { current_ << " "; }
    print(to, " ");
    current_ << " " << theoryTerms_.erase(term);
    theoryOpterms_[opterm] += str();
    return opterm;
}

TheoryOpVecUid TestNongroundProgramBuilder::theoryops() {
    return theoryOps_.emplace();
}

TheoryOpVecUid TestNongroundProgramBuilder::theoryops(TheoryOpVecUid ops, String op) {
    theoryOps_[ops].emplace_back(op.c_str());
    return ops;
}

TheoryOptermVecUid TestNongroundProgramBuilder::theoryopterms() {
    return theoryOptermVecs_.emplace();
}

TheoryOptermVecUid TestNongroundProgramBuilder::theoryopterms(TheoryOptermVecUid opterms, Location const &, TheoryOptermUid opterm) {
    theoryOptermVecs_[opterms].emplace_back(theoryOpterms_.erase(opterm));
    return opterms;
}

TheoryOptermVecUid TestNongroundProgramBuilder::theoryopterms(Location const &, TheoryOptermUid opterm, TheoryOptermVecUid opterms) {
    theoryOptermVecs_[opterms].emplace(theoryOptermVecs_[opterms].begin(), theoryOpterms_.erase(opterm));
    return opterms;
}

TheoryElemVecUid TestNongroundProgramBuilder::theoryelems() {
    return theoryElemVecs_.emplace();
}

TheoryElemVecUid TestNongroundProgramBuilder::theoryelems(TheoryElemVecUid elems, TheoryOptermVecUid opterms, LitVecUid cond) {
    print(theoryOptermVecs_.erase(opterms), ",");
    current_ << ":";
    print(litvecs_.erase(cond), ",");
    theoryElemVecs_[elems].emplace_back(str());
    return elems;
}

TheoryAtomUid TestNongroundProgramBuilder::theoryatom(TermUid term, TheoryElemVecUid elems) {
    current_ << "&" << terms_.erase(term) << "{";
    print(theoryElemVecs_.erase(elems), " ; ");
    current_ << "}";
    return theoryAtoms_.emplace(str());
}

TheoryAtomUid TestNongroundProgramBuilder::theoryatom(TermUid term, TheoryElemVecUid elems, String op, Location const &, TheoryOptermUid opterm) {
    current_ << "&" << terms_.erase(term) << "{";
    print(theoryElemVecs_.erase(elems), " ; ");
    current_ << "} " << op << " " << theoryOpterms_.erase(opterm);
    return theoryAtoms_.emplace(str());
}

// }}}
// {{{ theory definitions

TheoryOpDefUid TestNongroundProgramBuilder::theoryopdef(Location const &, String op, unsigned priority, TheoryOperatorType type) {
    current_ << op << " :" << priority << "," << type;
    return theoryOpDefs_.emplace(str());
}

TheoryOpDefVecUid TestNongroundProgramBuilder::theoryopdefs() {
    return theoryOpDefVecs_.emplace();
}

TheoryOpDefVecUid TestNongroundProgramBuilder::theoryopdefs(TheoryOpDefVecUid defs, TheoryOpDefUid def) {
    theoryOpDefVecs_[defs].emplace_back(theoryOpDefs_.erase(def));
    return defs;
}

TheoryTermDefUid TestNongroundProgramBuilder::theorytermdef(Location const &, String name, TheoryOpDefVecUid defs, Logger &) {
    current_ << name << "{";
    print(theoryOpDefVecs_.erase(defs), ",");
    current_ << "}";
    return theoryTermDefs_.emplace(str());
}

TheoryAtomDefUid TestNongroundProgramBuilder::theoryatomdef(Location const &, String name, unsigned arity, String termDef, TheoryAtomType type) {
    current_ << "&" << name << "/" << arity << ":" << termDef << "," << type;
    return theoryAtomDefs_.emplace(str());
}

TheoryAtomDefUid TestNongroundProgramBuilder::theoryatomdef(Location const &, String name, unsigned arity, String termDef, TheoryAtomType type, TheoryOpVecUid ops, String guardDef) {
    current_ << "&" << name << "/" << arity << ":" << termDef << ",{";
    print(theoryOps_.erase(ops), ",");
    current_ << "}," << guardDef << "," << type;
    return theoryAtomDefs_.emplace(str());
}

TheoryDefVecUid TestNongroundProgramBuilder::theorydefs() {
    return theoryDefVecs_.emplace();
}

TheoryDefVecUid TestNongroundProgramBuilder::theorydefs(TheoryDefVecUid defs, TheoryTermDefUid def) {
    theoryDefVecs_[defs].emplace_back(theoryTermDefs_.erase(def));
    return defs;
}

TheoryDefVecUid TestNongroundProgramBuilder::theorydefs(TheoryDefVecUid defs, TheoryAtomDefUid def) {
    theoryDefVecs_[defs].emplace_back(theoryAtomDefs_.erase(def));
    return defs;
}

void TestNongroundProgramBuilder::theorydef(Location const &, String name, TheoryDefVecUid defs, Logger &) {
    current_ << "#theory " << name << "{";
    print(theoryDefVecs_.erase(defs), ";");
    current_ << "}.";
    statements_.emplace_back(str());
}

// }}}
// {{{ auxiliary functions

void TestNongroundProgramBuilder::print(AggregateFunction fun, BoundVecUid boundvecuid, StringVec const &elems) {
    StringPairVec bounds(bounds_.erase(boundvecuid));
    auto it = bounds.begin(), end = bounds.end();
    if (it != end) { current_ << it->first; ++it; }
    current_ << fun << "{";
    print(elems, ";");
    current_ << "}";
    for (; it != end; ++it) { current_ << it->second; }
}

void TestNongroundProgramBuilder::print(NAF naf) {
    switch (naf) {
        case NAF::NOT:    { current_ << "#not "; break; }
        case NAF::NOTNOT: { current_ << "#not #not "; break; }
        case NAF::POS:    { break; }
    }
}

std::string TestNongroundProgramBuilder::str() {
    std::string str(current_.str());
    current_.str("");
    return str;
}

void TestNongroundProgramBuilder::print(StringVec const &vec, char const *sep) {
    auto it = vec.begin(), end = vec.end();
    if (it != end) {
        current_ << *it++;
        for (; it != end; ++it) { current_ << sep << *it; }
    }
}

void TestNongroundProgramBuilder::print(StringVecVec const &vec) {
    auto it = vec.begin(), end = vec.end();
    if (it != end) {
        print(*it++, ",");
        for (; it != end; ++it) {
            current_ << ";";
            print(*it, ",");
        }
    }
}

// }}}

std::string TestNongroundProgramBuilder::toString() {
    auto it = statements_.begin(), end = statements_.end();
    if (it != end) {
        current_ << *it;
        for (++it; it != end; ++it) { current_ << '\n' << *it; }
    }
    statements_.clear();
    return str();
}

TestNongroundProgramBuilder::~TestNongroundProgramBuilder() { }

// }}}

std::string parse(std::string const &str) {
    Gringo::Test::TestGringoModule log;
    TestNongroundProgramBuilder pb;
    bool incmode;
    NullBackend bck;
    NonGroundParser ngp(pb, bck, incmode);
    ngp.pushStream("-", std::unique_ptr<std::istream>(new std::stringstream(str)), log);
    ngp.parse(log);
    return pb.toString();
}

} // namespace

TEST_CASE("input-nongroundprogrambuilder", "[input]") {

    SECTION("term") {
        // testing constants
        REQUIRE("#program base().\np(x)." == parse("p(x)."));
        REQUIRE("#program base().\np(1)." == parse("p(1)."));
        REQUIRE("#program base().\np(\"1\")." == parse("p(\"1\")."));
        REQUIRE("#program base().\np(#inf)." == parse("p(#inf)."));
        REQUIRE("#program base().\np(#sup)." == parse("p(#sup)."));
        REQUIRE("#program base().\np(X)." == parse("p(X)."));
        REQUIRE("#program base().\np(_)." == parse("p(_)."));
        // absolute
        REQUIRE("#program base().\np(|1|)." == parse("p(|1|)."));
        REQUIRE("#program base().\np(|1;2;3|)." == parse("p(|1;2;3|)."));
        // lua function calls
        REQUIRE("#program base().\np(@f())." == parse("p(@f())."));
        REQUIRE("#program base().\np(@f())." == parse("p(@f)."));
        REQUIRE("#program base().\np(@f(1))." == parse("p(@f(1))."));
        REQUIRE("#program base().\np(@f(1,2))." == parse("p(@f(1,2))."));
        REQUIRE("#program base().\np(@f(1,2,3))." == parse("p(@f(1,2,3))."));
        REQUIRE("#program base().\np(@f(;;;1,2;3))." == parse("p(@f(;;;1,2;3))."));
        // function symbols
        REQUIRE("#program base().\np(f)." == parse("p(f())."));
        REQUIRE("#program base().\np(f(1))." == parse("p(f(1))."));
        REQUIRE("#program base().\np(f(1,2))." == parse("p(f(1,2))."));
        REQUIRE("#program base().\np(f(1,2,3))." == parse("p(f(1,2,3))."));
        REQUIRE("#program base().\np(f(;;;1,2;3))." == parse("p(f(;;;1,2;3))."));
        // tuples / parenthesis
        REQUIRE("#program base().\np((()))." == parse("p(())."));
        REQUIRE("#program base().\np(((1)))." == parse("p((1))."));
        REQUIRE("#program base().\np(((1,2)))." == parse("p((1,2))."));
        REQUIRE("#program base().\np(((1,2,3)))." == parse("p((1,2,3))."));
        REQUIRE("#program base().\np((();();();(1,2);(3,)))." == parse("p((;;;1,2;3,))."));
        // unary operations
        REQUIRE("#program base().\np(-1)." == parse("p(-1)."));
        REQUIRE("#program base().\np(~1)." == parse("p(~1)."));
        // binary operations
        REQUIRE("#program base().\np((1**2))." == parse("p(1**2)."));
        REQUIRE("#program base().\np((1\\2))." == parse("p(1\\2)."));
        REQUIRE("#program base().\np((1/2))." == parse("p(1/2)."));
        REQUIRE("#program base().\np((1*2))." == parse("p(1*2)."));
        REQUIRE("#program base().\np((1-2))." == parse("p(1-2)."));
        REQUIRE("#program base().\np((1+2))." == parse("p(1+2)."));
        REQUIRE("#program base().\np((1&2))." == parse("p(1&2)."));
        REQUIRE("#program base().\np((1?2))." == parse("p(1?2)."));
        REQUIRE("#program base().\np((1^2))." == parse("p(1^2)."));
        REQUIRE("#program base().\np((1..2))." == parse("p(1..2)."));
        // precedence
        REQUIRE("#program base().\np(((1+2)+((3*4)*(5**(6**7)))))." == parse("p(1+2+3*4*5**6**7)."));
        // nesting
        REQUIRE("#program base().\np((f(1,(();();();(1,(x..Y));(3)),3)+p(f(1,#sup,3))))." == parse("p(f(1,(;;;1,x..Y;3),3)+p(f(1,#sup,3)))."));
    }

    SECTION("atomargs") {
        // identifier
        REQUIRE("#program base().\np." == parse("p."));
        // identifier LPAREN argvec RPAREN
        REQUIRE("#program base().\np." == parse("p()."));
        REQUIRE("#program base().\np(1)." == parse("p(1)."));
        REQUIRE("#program base().\np(;;;1,2;3,4)." == parse("p(;;;1,2;3,4)."));
        // identifier LPAREN MUL RPAREN
        REQUIRE("#program base().\np(_)." == parse("p(_)."));
        // identifier LPAREN MUL cpredargvec RPAREN
        REQUIRE("#program base().\np(_,1)." == parse("p(_,1)."));
        REQUIRE("#program base().\np(_,1,2)." == parse("p(_,1,2)."));
        REQUIRE("#program base().\np(_,1,2;2,3)." == parse("p(_,1,2;2,3)."));
        // identifier LPAREN MUL spredargvec RPAREN
        REQUIRE("#program base().\np(_;1)." == parse("p(_;1)."));
        REQUIRE("#program base().\np(_;1,2)." == parse("p(_;1,2)."));
        REQUIRE("#program base().\np(_;1,2;2,3)." == parse("p(_;1,2;2,3)."));
        // identifier LPAREN argvec COMMAMUL MUL RPAREN
        REQUIRE("#program base().\np(1,_)." == parse("p(1,_)."));
        REQUIRE("#program base().\np(1,2,_)." == parse("p(1,2,_)."));
        // identifier LPAREN argvec COMMAMUL MUL cpredargvec RPAREN
        REQUIRE("#program base().\np(1,2,_,3,_;_)." == parse("p(1,2,_,3,_;_)."));
        REQUIRE("#program base().\np(1,2,_,3,4,_;_)." == parse("p(1,2,_,3,4,_;_)."));
        // identifier LPAREN argvec COMMAMUL MUL spredargvec RPAREN
        REQUIRE("#program base().\np(1,2,_;3,_;_)." == parse("p(1,2,_;3,_;_)."));
        REQUIRE("#program base().\np(1,2,_;3,4,_;_)." == parse("p(1,2,_;3,4,_;_)."));
        // identifier LPAREN argvec SEMMUL MUL RPAREN
        REQUIRE("#program base().\np(1;_)." == parse("p(1;_)."));
        REQUIRE("#program base().\np(1,2;_)." == parse("p(1,2;_)."));
        // identifier LPAREN argvec SEMMUL MUL cpredargvec RPAREN
        REQUIRE("#program base().\np(1,2;_,3,_;_)." == parse("p(1,2;_,3,_;_)."));
        REQUIRE("#program base().\np(1,2;_,3,4,_;_)." == parse("p(1,2;_,3,4,_;_)."));
        // identifier LPAREN argvec SEMMUL MUL spredargvec RPAREN
        REQUIRE("#program base().\np(1,2;_;3,_;_)." == parse("p(1,2;_;3,_;_)."));
        REQUIRE("#program base().\np(1,2;_;3,4,_;_)." == parse("p(1,2;_;3,4,_;_)."));
    }

    SECTION("literal") {
        // TRUE
        REQUIRE("#program base().\n#true." == parse("#true."));
        // FALSE
        REQUIRE("#program base().\n#false." == parse("#false."));
        // atom
        REQUIRE("#program base().\np." == parse("p."));
        REQUIRE("#program base().\n-p." == parse("-p."));
        // NOT atom
        REQUIRE("#program base().\n#not p." == parse("not p."));
        // NOT NOT atom
        REQUIRE("#program base().\n#not #not p." == parse("not not p."));
        // term cmp term
        REQUIRE("#program base().\n(1+2)!=3." == parse("1+2!=3."));
        REQUIRE("#program base().\n(1+2)<3<4." == parse("1+2<3<4."));
        REQUIRE("#program base().\n(1+2)<3<4<=5." == parse("1+2<3<4<=5."));
        REQUIRE("#program base().\n#not (1+2)<3<4<=5." == parse("not 1+2<3<4<=5."));
        REQUIRE("#program base().\n#not #not (1+2)<3<4<=5." == parse("not not 1+2<3<4<=5."));
    }

    SECTION("bdaggr") {
        // aggregatefunction
        REQUIRE("#program base().\n#false:-#count{}." == parse(":-{}."));
        REQUIRE("#program base().\n#false:-#sum{}." == parse(":-#sum{}."));
        REQUIRE("#program base().\n#false:-#sum+{}." == parse(":-#sum+{}."));
        REQUIRE("#program base().\n#false:-#min{}." == parse(":-#min{}."));
        REQUIRE("#program base().\n#false:-#max{}." == parse(":-#max{}."));
        REQUIRE("#program base().\n#false:-#count{}." == parse(":-#count{}."));
        // LBRACE altbodyaggrelemvec RBRACE
        REQUIRE("#program base().\n#false:-#count{p:;p:;p:p;p:p,q}." == parse(":-{p;p:;p:p;p:p,q}."));
        // aggregatefunction LBRACE bodyaggrelemvec RBRACE
        REQUIRE("#program base().\n#false:-#count{:;:p;:p,q}." == parse(":-#count{:;:p;:p,q}."));
        REQUIRE("#program base().\n#false:-#count{p:;p,q:}." == parse(":-#count{p;p,q}."));
        REQUIRE("#program base().\n#false:-#count{p:;p,q:}." == parse(":-#count{p:;p,q:}."));
        REQUIRE("#program base().\n#false:-#count{p:q,r,s}." == parse(":-#count{p:q,r,s}."));
        // test lower
        REQUIRE("#program base().\n#false:-1<=#count{}." == parse(":-1{}."));
        REQUIRE("#program base().\n#false:-1<#count{}." == parse(":-1<{}."));
        REQUIRE("#program base().\n#false:-1<=#count{}." == parse(":-1<={}."));
        REQUIRE("#program base().\n#false:-1>#count{}." == parse(":-1>{}."));
        REQUIRE("#program base().\n#false:-1>=#count{}." == parse(":-1>={}."));
        REQUIRE("#program base().\n#false:-1=#count{}." == parse(":-1=={}."));
        REQUIRE("#program base().\n#false:-1=#count{}." == parse(":-1={}."));
        // test upper
        REQUIRE("#program base().\n#false:-1>=#count{}." == parse(":-{}1."));
        REQUIRE("#program base().\n#false:-1<#count{}." == parse(":-{}>1."));
        REQUIRE("#program base().\n#false:-1<=#count{}." == parse(":-{}>=1."));
        REQUIRE("#program base().\n#false:-1>#count{}." == parse(":-{}<1."));
        REQUIRE("#program base().\n#false:-1>=#count{}." == parse(":-{}<=1."));
        REQUIRE("#program base().\n#false:-1=#count{}." == parse(":-{}==1."));
        REQUIRE("#program base().\n#false:-1=#count{}." == parse(":-{}=1."));
        // test both
        REQUIRE("#program base().\n#false:-1<=#count{}<=2." == parse(":-1{}2."));
        REQUIRE("#program base().\n#false:-1<#count{}<2." == parse(":-1<{}<2."));
    }

    SECTION("hdaggr") {
        // aggregatefunction
        REQUIRE("#program base().\n#count{}." == parse("{}."));
        REQUIRE("#program base().\n#sum{}." == parse("#sum{}."));
        REQUIRE("#program base().\n#sum+{}." == parse("#sum+{}."));
        REQUIRE("#program base().\n#min{}." == parse("#min{}."));
        REQUIRE("#program base().\n#max{}." == parse("#max{}."));
        REQUIRE("#program base().\n#count{}." == parse("#count{}."));
        // LBRACE altheadaggrelemvec RBRACE
        REQUIRE("#program base().\n#count{p:;p:;p:p;p:p,q}." == parse("{p;p:;p:p;p:p,q}."));
        // aggregatefunction LBRACE headaggrelemvec RBRACE
        REQUIRE("#program base().\n#count{:p:;:q:r;:s:t,u}." == parse("#count{:p;:q:r;:s:t,u}."));
        REQUIRE("#program base().\n#count{p:q:;r,s:t:}." == parse("#count{p:q;r,s:t}."));
        REQUIRE("#program base().\n#count{p:q:;r,s:t:}." == parse("#count{p:q:;r,s:t:}."));
        REQUIRE("#program base().\n#count{p:q:x,y,z;r,s:t:q,w,e}." == parse("#count{p:q:x,y,z;r,s:t:q,w,e}."));
        // test lower
        REQUIRE("#program base().\n1<=#count{}." == parse("1{}."));
        REQUIRE("#program base().\n1<#count{}." == parse("1<{}."));
        REQUIRE("#program base().\n1<=#count{}." == parse("1<={}."));
        REQUIRE("#program base().\n1>#count{}." == parse("1>{}."));
        REQUIRE("#program base().\n1>=#count{}." == parse("1>={}."));
        REQUIRE("#program base().\n1=#count{}." == parse("1=={}."));
        REQUIRE("#program base().\n1=#count{}." == parse("1={}."));
        // test upper
        REQUIRE("#program base().\n1>=#count{}." == parse("{}1."));
        REQUIRE("#program base().\n1<#count{}." == parse("{}>1."));
        REQUIRE("#program base().\n1<=#count{}." == parse("{}>=1."));
        REQUIRE("#program base().\n1>#count{}." == parse("{}<1."));
        REQUIRE("#program base().\n1>=#count{}." == parse("{}<=1."));
        REQUIRE("#program base().\n1=#count{}." == parse("{}==1."));
        REQUIRE("#program base().\n1=#count{}." == parse("{}=1."));
        // test both
        REQUIRE("#program base().\n1<=#count{}<=2." == parse("1{}2."));
        REQUIRE("#program base().\n1<#count{}<2." == parse("1<{}<2."));
    }

    SECTION("conjunction") {
        REQUIRE("#program base().\n#false:-a:." == parse(":-a:."));
        REQUIRE("#program base().\n#false:-a:b." == parse(":-a:b."));
        REQUIRE("#program base().\n#false:-a:b,c." == parse(":-a:b,c."));
        REQUIRE("#program base().\n#false:-a:b,c;x." == parse(":-a:b,c;x."));
    }

    SECTION("disjunction") {
        // Note: first disjunction element moves to the end (parsing related)
        // literal COLON litvec
        REQUIRE("#program base().\na." == parse("a."));
        REQUIRE("#program base().\na:b." == parse("a:b."));
        REQUIRE("#program base().\na:b,c." == parse("a:b,c."));
        // literal COMMA disjunctionsep literal optcondition
        REQUIRE("#program base().\na:;b:;c:." == parse("a,b,c."));
        REQUIRE("#program base().\na:;b:;c:;d:." == parse("a,b;c;d."));
        REQUIRE("#program base().\na:;b:;c:d,e." == parse("a,b,c:d,e."));
        REQUIRE("#program base().\na:;b:d,e;c:." == parse("a,b:d,e;c."));
        // literal SEM disjunctionsep literal optcondition
        REQUIRE("#program base().\na:;b:;c:." == parse("a;b,c."));
        REQUIRE("#program base().\na:;b:;c:;d:." == parse("a;b;c;d."));
        REQUIRE("#program base().\na:;b:;c:d,e." == parse("a;b,c:d,e."));
        REQUIRE("#program base().\na:;b:d,e;c:." == parse("a;b:d,e;c."));
        // literal COLON litvec SEM disjunctionsep literal optcondition
        REQUIRE("#program base().\na:;c:." == parse("a;c."));
        REQUIRE("#program base().\na:x;c:." == parse("a:x;c."));
        REQUIRE("#program base().\na:x,y;c:." == parse("a:x,y;c."));
        REQUIRE("#program base().\na:x,y;b:;c:." == parse("a:x,y;b,c."));
        REQUIRE("#program base().\na:x,y;b:;c:;d:." == parse("a:x,y;b;c;d."));
        REQUIRE("#program base().\na:x,y;b:;c:d,e." == parse("a:x,y;b,c:d,e."));
        REQUIRE("#program base().\na:x,y;b:d,e;c:." == parse("a:x,y;b:d,e;c."));
        // empty condition
        REQUIRE("#program base().\na:;b:;c:." == parse("a:;b:;c:."));
    }

    SECTION("external") {
        REQUIRE("#program base().\n#external p(X):q(X). [false]" == parse("#external p(X) : q(X)."));
        REQUIRE("#program base().\n#external p(X):q(X). [X]" == parse("#external p(X) : q(X). [X]"));
    }

    SECTION("rule") {
        // body literal
        REQUIRE("#program base().\n#false:-a;x." == parse(":-a,x."));
        REQUIRE("#program base().\n#false:-a;x." == parse(":-a;x."));
        REQUIRE("#program base().\n#false:-a;x." == parse(":-a,x."));
        // body aggregate
        REQUIRE("#program base().\n#false:-#count{};#count{}." == parse(":-{},{}."));
        REQUIRE("#program base().\n#false:-#count{};#count{}." == parse(":-{};{}."));
        REQUIRE("#program base().\n#false:-#not #count{};#not #count{}." == parse(":-not{},not{}."));
        REQUIRE("#program base().\n#false:-#not #count{};#not #count{}." == parse(":-not{};not{}."));
        REQUIRE("#program base().\n#false:-#not #not #count{};#not #not #count{}." == parse(":-not not{},not not{}."));
        REQUIRE("#program base().\n#false:-#not #not #count{};#not #not #count{}." == parse(":-not not{};not not{}."));
        REQUIRE("#program base().\n#false:-#not #not nott<=#count{};#not #not nott<=#count{}." == parse(":-not not nott{},not not nott{}."));
        REQUIRE("#program base().\n#false:-#not #not nott<=#count{};#not #not nott<=#count{}." == parse(":-not not nott{};not not nott{}."));
        // conjunction
        REQUIRE("#program base().\n#false:-a:;b:." == parse(":-a:;b:."));
        // head literal
        REQUIRE("#program base().\na." == parse("a."));
        // head aggregate
        REQUIRE("#program base().\n#count{}." == parse("{}."));
        REQUIRE("#program base().\nnott<=#count{}." == parse("nott{}."));
        // disjunction
        REQUIRE("#program base().\na:;b:." == parse("a,b."));
        // rules
        REQUIRE("#program base().\na." == parse("a."));
        REQUIRE("#program base().\na:-b." == parse("a:-b."));
        REQUIRE("#program base().\na:-b;c." == parse("a:-b,c."));
        REQUIRE("#program base().\n#false." == parse(":-."));
        REQUIRE("#program base().\n#false:-b." == parse(":-b."));
        REQUIRE("#program base().\n#false:-b;c." == parse(":-b,c."));
    }

    SECTION("define") {
        REQUIRE("#program base().\n#const a=10." == parse("#const a=10."));
        REQUIRE("#program base().\n#const a=x." == parse("#const a=x."));
        REQUIRE("#program base().\n#const a=1." == parse("#const a=1."));
        REQUIRE("#program base().\n#const a=\"1\"." == parse("#const a=\"1\"."));
        REQUIRE("#program base().\n#const a=#inf." == parse("#const a=#inf."));
        REQUIRE("#program base().\n#const a=#sup." == parse("#const a=#sup."));
        // absolute
        REQUIRE("#program base().\n#const a=|1|." == parse("#const a=|1|."));
        // lua function calls
        REQUIRE("#program base().\n#const a=@f()." == parse("#const a=@f()."));
        REQUIRE("#program base().\n#const a=@f()." == parse("#const a=@f."));
        REQUIRE("#program base().\n#const a=@f(1)." == parse("#const a=@f(1)."));
        REQUIRE("#program base().\n#const a=@f(1,2)." == parse("#const a=@f(1,2)."));
        REQUIRE("#program base().\n#const a=@f(1,2,3)." == parse("#const a=@f(1,2,3)."));
        // function symbols
        REQUIRE("#program base().\n#const a=f." == parse("#const a=f()."));
        REQUIRE("#program base().\n#const a=f(1)." == parse("#const a=f(1)."));
        REQUIRE("#program base().\n#const a=f(1,2)." == parse("#const a=f(1,2)."));
        REQUIRE("#program base().\n#const a=f(1,2,3)." == parse("#const a=f(1,2,3)."));
        // tuples / parenthesis
        REQUIRE("#program base().\n#const a=()." == parse("#const a=()."));
        REQUIRE("#program base().\n#const a=(1)." == parse("#const a=(1)."));
        REQUIRE("#program base().\n#const a=(1,2)." == parse("#const a=(1,2)."));
        REQUIRE("#program base().\n#const a=(1,2,3)." == parse("#const a=(1,2,3)."));
        // unary operations
        REQUIRE("#program base().\n#const a=-1." == parse("#const a=-1."));
        REQUIRE("#program base().\n#const a=~1." == parse("#const a=~1."));
        // binary operations
        REQUIRE("#program base().\n#const a=(1**2)." == parse("#const a=1**2."));
        REQUIRE("#program base().\n#const a=(1\\2)." == parse("#const a=1\\2."));
        REQUIRE("#program base().\n#const a=(1/2)." == parse("#const a=1/2."));
        REQUIRE("#program base().\n#const a=(1*2)." == parse("#const a=1*2."));
        REQUIRE("#program base().\n#const a=(1-2)." == parse("#const a=1-2."));
        REQUIRE("#program base().\n#const a=(1+2)." == parse("#const a=1+2."));
        REQUIRE("#program base().\n#const a=(1&2)." == parse("#const a=1&2."));
        REQUIRE("#program base().\n#const a=(1?2)." == parse("#const a=1?2."));
        REQUIRE("#program base().\n#const a=(1^2)." == parse("#const a=1^2."));
        // precedence
        REQUIRE("#program base().\n#const a=((1+2)+((3*4)*(5**(6**7))))." == parse("#const a=1+2+3*4*5**6**7."));
        // attributes
        REQUIRE("#program base().\n#const a=10." == parse("#const a=10. [default]"));
        REQUIRE("#program base().\n#const a=10. [override]" == parse("#const a=10. [override]"));
    }

    SECTION("optimize") {
        REQUIRE("#program base().\n:~p(X,Y).[X@Y,a]" == parse(":~ p(X,Y). [X@Y,a]"));
        REQUIRE("#program base().\n:~p(X,Y);r;s.[X@0]" == parse(":~ p(X,Y),r,s. [X]"));
        REQUIRE("#program base().\n:~p(X,Y);s.[X@Y,a]\n:~q(Y).[Y@f]\n:~.[1@0]" == parse("#minimize { X@Y,a : p(X,Y),s; Y@f : q(Y); 1 }."));
        REQUIRE("#program base()." == parse("#minimize { }."));
        REQUIRE("#program base().\n:~p(X,Y);r.[-X@Y,a]\n:~q(Y).[-Y@f]\n:~.[-2@0]" == parse("#maximize { X@Y,a : p(X,Y), r; Y@f : q(Y); 2 }."));
        REQUIRE("#program base()." == parse("#maximize { }."));
    }

    SECTION("show") {
        REQUIRE("#program base().\n#showsig p/1." == parse("#show p/1."));
        REQUIRE("#program base().\n#showsig p/1." == parse("#show p  /  1."));
        REQUIRE("#program base().\n#showsig -p/1." == parse("#show -p/1."));
        REQUIRE("#program base().\n#show (-p/-1)." == parse("#show -p/-1."));
        REQUIRE("#program base().\n#show X:p(X);1<=#count{q(X):p(X)}." == parse("#show X:p(X), 1 { q(X):p(X) }."));
    }

    SECTION("input") {
        REQUIRE("#program base().\n#defined p/1." == parse("#defined p/1."));
    }

    SECTION("include") {
        struct Del {
            Del()  { std::ofstream("test_include.lp") << "b.\n"; }
            ~Del() { std::remove("test_include.lp"); }
        } del;
        REQUIRE("#program base().\na.\nb.\n#program base().\nc.\nd." == parse("a.\n#include \"test_include.lp\".\nc.\nd.\n"));
    }

    SECTION("edge") {
        REQUIRE("#program base().\n#edge(a,b)." == parse("#edge (a, b)."));
        REQUIRE("#program base().\n#edge(a,b;e,f)." == parse("#edge (a, b;e,f):."));
        REQUIRE("#program base().\n#edge(a,b;e,f):p;q." == parse("#edge (a, b;e,f):p,q."));
    }

    SECTION("project") {
        REQUIRE("#program base().\n#project a/1." == parse("#project a/1."));
        REQUIRE("#program base().\n#project -a/1." == parse("#project -a/1."));
        REQUIRE("#program base().\n#project a." == parse("#project a."));
        REQUIRE("#program base().\n#project a." == parse("#project a:."));
        REQUIRE("#program base().\n#project a:b;c." == parse("#project a:b,c."));
    }

    SECTION("heuristic") {
        REQUIRE("#program base().\n#heuristic p:q.[1@2,level]" == parse("#heuristic p : q. [1@2,level]"));
        REQUIRE("#program base().\n#heuristic p:q.[1@2,sign]" == parse("#heuristic p : q. [1@2,sign]"));
        REQUIRE("#program base().\n#heuristic p:q.[1@2,true]" == parse("#heuristic p : q. [1@2,true]"));
        REQUIRE("#program base().\n#heuristic p:q.[1@2,false]" == parse("#heuristic p : q. [1@2,false]"));
        REQUIRE("#program base().\n#heuristic p:q.[1@2,factor]" == parse("#heuristic p : q. [1@2,factor]"));
        REQUIRE("#program base().\n#heuristic p:q;r.[1@2,init]" == parse("#heuristic p : q,r. [1@2,init]"));
        REQUIRE("#program base().\n#heuristic p.[1@0,init]" == parse("#heuristic p:. [1,init]"));
        REQUIRE("#program base().\n#heuristic p.[1@0,init]" == parse("#heuristic p. [1,init]"));
    }

    SECTION("theory") {
        // NOTE: things would be less error prone if : and ; would not be valid theory connectives
        REQUIRE("#program base().\n&x{}." == parse("&x { }."));
        REQUIRE("#program base().\n&x{}." == parse("&x."));
        REQUIRE("#program base().\n#false:-&x{}." == parse(":-&x { }."));
        REQUIRE("#program base().\n#false:-&x{}." == parse(":-&x."));
        REQUIRE("#program base().\n&x{} < 42." == parse("&x { } < 42."));
        REQUIRE("#program base().\n#false:-&x{} < 42." == parse(":-&x { } < 42."));
        REQUIRE("#program base().\n#false:-&x{} < 42 + 17 ^ (- 1)." == parse(":-&x { } < 42+17^(-1)."));
        REQUIRE("#program base().\n#false:-&x{} < 42 + 17 ^ - 1." == parse(":-&x { } < 42+17^ -1."));
        REQUIRE("#program base().\n#false:-&x{} < 42 + 17 ^- 1." == parse(":-&x { } < 42+17^-1."));
        REQUIRE("#program base().\n&x{u,v: ; u: ; u: ; : ; :p ; :p,q}." == parse("&x { u,v; u ; u: ; : ; :p; :p,q }."));
        REQUIRE("#program base().\n&x{u: ; u: ; : ; :p ; :p,q}." == parse("&x { u ; u: ; : ; :p; :p,q }."));
        REQUIRE("#program base().\n&x{i(u + v ^ (a +- 3 * b),7 + (1,) *** (2) - () + [a ; b] ? {1 ; 2 ; 3}):}." == parse("&x { i(u+v^(a+- 3 * b),7+(1,)***(2)-()+[a,b]?{1,2,3}) }."));
    }

    SECTION("theoryDefinition") {
        REQUIRE("#program base().\n#theory t{}." == parse("#theory t { }."));
        REQUIRE("#program base().\n#theory t{x{}}." == parse("#theory t { x { } }."));
        REQUIRE("#program base().\n#theory t{x{};y{}}." == parse("#theory t { x { }; y{} }."));
        REQUIRE("#program base().\n#theory t{x{++ :42,unary}}." == parse("#theory t { x { ++ : 42, unary } }."));
        REQUIRE("#program base().\n#theory t{x{++ :42,unary,** :21,binary,left,-* :1,binary,right}}." == parse("#theory t { x { ++ : 42, unary; ** : 21, binary, left; -* : 1, binary, right } }."));
        REQUIRE("#program base().\n#theory t{x{};&a/0:x,any}." == parse("#theory t { x{}; &a/0: x, any }."));
        REQUIRE("#program base().\n#theory t{x{};&a/0:x,any;&b/0:x,head;&c/0:x,body;&d/0:x,directive}." == parse("#theory t { x{}; &a/0: x, any; &b/0: x, head; &c/0: x, body; &d/0: x, directive }."));
        REQUIRE("#program base().\n#theory t{x{};&a/0:x,{+,-,++,**},x,any}." == parse("#theory t { x{}; &a/0: x, {+,-, ++, **}, x, any }."));
    }

    SECTION("comment") {
        REQUIRE("#program base().\n%* test *%" == parse("%* test *%"));
        REQUIRE("#program base().\n% test" == parse("% test"));
        REQUIRE("#program base().\n% before\na:-b.\n% after" == parse("a :- % before\n b. % after"));
    }

}

} } } // namespace Test Input Gringo

