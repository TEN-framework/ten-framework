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

#include "gringo/input/programbuilder.hh"
#include "gringo/input/literals.hh"
#include "gringo/input/aggregates.hh"
#include "gringo/input/program.hh"
#include "gringo/input/theory.hh"
#include "gringo/output/output.hh"

#ifdef _MSC_VER
#pragma warning (disable : 4503) // decorated name length exceeded
#endif

namespace Gringo { namespace Input {

// {{{1 definition of NongroundProgramBuilder

NongroundProgramBuilder::NongroundProgramBuilder(Context &context, Program &prg, OutputPredicates &output_preds, Defines &defs, bool rewriteMinimize)
: context_(context)
, prg_(prg)
, output_preds_(output_preds)
, defs_(defs)
, rewriteMinimize_(rewriteMinimize)
{ }

// {{{2 terms

TermUid NongroundProgramBuilder::term(Location const &loc, Symbol val) {
    return terms_.insert(make_locatable<ValTerm>(loc, val));
}

TermUid NongroundProgramBuilder::term(Location const &loc, String name) {
    if (name == "_") {
        return terms_.insert(make_locatable<VarTerm>(loc, name, nullptr));
    }
    auto &ret(vals_[name]);
    if (!ret) {
        ret = std::make_shared<Symbol>();
    }
    return terms_.insert(make_locatable<VarTerm>(loc, name, ret));
}

TermUid NongroundProgramBuilder::term(Location const &loc, UnOp op, TermUid a) {
    return term(loc, op, termvec(termvec(), a));
}

TermUid NongroundProgramBuilder::term(Location const &loc, UnOp op, TermVecUid a) {
    UTermVec vec(termvecs_.erase(a));
    if (vec.size() == 1) {
        return terms_.insert(make_locatable<UnOpTerm>(loc, op, std::move(vec.front())));
    }
    UTermVec pool;
    for (auto &terms : vec) {
        pool.emplace_back(make_locatable<UnOpTerm>(loc, op, std::move(terms)));
    }
    return terms_.insert(make_locatable<PoolTerm>(loc, std::move(pool)));
}

TermUid NongroundProgramBuilder::term(Location const &loc, BinOp op, TermUid a, TermUid b) {
    return terms_.insert(make_locatable<BinOpTerm>(loc, op, terms_.erase(a), terms_.erase(b)));
}

TermUid NongroundProgramBuilder::term(Location const &loc, TermUid a, TermUid b) {
    return terms_.insert(make_locatable<DotsTerm>(loc, terms_.erase(a), terms_.erase(b)));
}

TermUid NongroundProgramBuilder::term(Location const &loc, String name, TermVecVecUid a, bool lua) {
    assert(name != "");
    auto create = [&lua, &name, &loc](UTermVec &&vec) -> UTerm {
        // lua terms
        if (lua) {
            return make_locatable<LuaTerm>(loc, name, std::move(vec));
        }
        // constant symbols
        if (vec.empty()) {
            return make_locatable<ValTerm>(loc, Symbol::createId(name));
        }
        // function terms
        return make_locatable<FunctionTerm>(loc, name, std::move(vec));
    };
    TermVecVecs::ValueType vec(termvecvecs_.erase(a));
    // no pooling
    if (vec.size() == 1) {
        return terms_.insert(create(std::move(vec.front())));
    }
    // pooling
    UTermVec pool;
    for (auto &terms : vec) {
        pool.emplace_back(create(std::move(terms)));
    }
    return terms_.insert(make_locatable<PoolTerm>(loc, std::move(pool)));
}

TermUid NongroundProgramBuilder::term(Location const &loc, TermVecUid args, bool forceTuple) {
    UTermVec a(termvecs_.erase(args));
    UTerm ret(!forceTuple && a.size() == 1 ? std::move(a.front()) : make_locatable<FunctionTerm>(loc, "", std::move(a)));
    return terms_.insert(std::move(ret));
}

TermUid NongroundProgramBuilder::pool(Location const &loc, TermVecUid args) {
    return terms_.insert(make_locatable<PoolTerm>(loc, termvecs_.erase(args)));
}

// {{{2 id vectors

IdVecUid NongroundProgramBuilder::idvec() {
    return idvecs_.emplace();
}
IdVecUid NongroundProgramBuilder::idvec(IdVecUid uid, Location const &loc, String id) {
    idvecs_[uid].emplace_back(loc, id);
    return uid;
}

// {{{2 termvecs

TermVecUid NongroundProgramBuilder::termvec() {
    return termvecs_.emplace();
}

TermVecUid NongroundProgramBuilder::termvec(TermVecUid uid, TermUid term) {
    termvecs_[uid].emplace_back(terms_.erase(term));
    return uid;
}

// {{{2 termvecvecs

TermVecVecUid NongroundProgramBuilder::termvecvec() {
    return termvecvecs_.emplace();
}

TermVecVecUid NongroundProgramBuilder::termvecvec(TermVecVecUid uid, TermVecUid termvecUid) {
    termvecvecs_[uid].emplace_back(termvecs_.erase(termvecUid));
    return uid;
}

// {{{2 literals

LitUid NongroundProgramBuilder::boollit(Location const &loc, bool type) {
    return rellit(loc, NAF::POS, term(loc, Symbol::createNum(0)), rellitvec(loc, type ? Relation::EQ : Relation::NEQ, term(loc, Symbol::createNum(0))));
}

LitUid NongroundProgramBuilder::predlit(Location const &loc, NAF naf, TermUid term) {
    return lits_.insert(make_locatable<PredicateLiteral>(loc, naf, terms_.erase(term)));
}

RelLitVecUid NongroundProgramBuilder::rellitvec(Location const &loc, Relation rel, TermUid termUidLeft) {
    auto uid = rellitvecs_.emplace();
    rellitvecs_[uid].emplace_back(rel, terms_.erase(termUidLeft));
    return uid;
}

RelLitVecUid NongroundProgramBuilder::rellitvec(Location const &loc, RelLitVecUid vecUidLeft, Relation rel, TermUid termUidRight) {
    rellitvecs_[vecUidLeft].emplace_back(rel, terms_.erase(termUidRight));
    return vecUidLeft;
}

LitUid NongroundProgramBuilder::rellit(Location const &loc, NAF naf, TermUid termUidLeft, RelLitVecUid vecUidRight) {
    return lits_.insert(make_locatable<RelationLiteral>(loc, naf, terms_.erase(termUidLeft), rellitvecs_.erase(vecUidRight)));
}

// {{{2 literal vectors

LitVecUid NongroundProgramBuilder::litvec() {
    return litvecs_.emplace();
}

LitVecUid NongroundProgramBuilder::litvec(LitVecUid uid, LitUid literalUid) {
    litvecs_[uid].emplace_back(lits_.erase(literalUid));
    return uid;
}

// {{{2 body aggregate element vectors

BdAggrElemVecUid NongroundProgramBuilder::bodyaggrelemvec() {
    return bodyaggrelemvecs_.emplace();
}

BdAggrElemVecUid NongroundProgramBuilder::bodyaggrelemvec(BdAggrElemVecUid uid, TermVecUid termvec, LitVecUid litvec) {
    bodyaggrelemvecs_[uid].emplace_back(termvecs_.erase(termvec), litvecs_.erase(litvec));
    return uid;
}

CondLitVecUid NongroundProgramBuilder::condlitvec() {
    return condlitvecs_.emplace();
}

CondLitVecUid NongroundProgramBuilder::condlitvec(CondLitVecUid uid, LitUid lit, LitVecUid litvec) {
    condlitvecs_[uid].emplace_back(lits_.erase(lit), litvecs_.erase(litvec));
    return uid;
}

// {{{2 head aggregate element vectors

HdAggrElemVecUid NongroundProgramBuilder::headaggrelemvec() {
    return headaggrelemvecs_.emplace();
}

HdAggrElemVecUid NongroundProgramBuilder::headaggrelemvec(HdAggrElemVecUid uid, TermVecUid termvec, LitUid lit, LitVecUid litvec) {
    headaggrelemvecs_[uid].emplace_back(termvecs_.erase(termvec), lits_.erase(lit), litvecs_.erase(litvec));
    return uid;
}

// {{{2 bounds

BoundVecUid NongroundProgramBuilder::boundvec() {
    return bounds_.emplace();
}

BoundVecUid NongroundProgramBuilder::boundvec(BoundVecUid uid, Relation rel, TermUid term) {
    bounds_[uid].emplace_back(rel, terms_.erase(term));
    return uid;
}

// {{{2 rule bodies

BdLitVecUid NongroundProgramBuilder::body() {
    return bodies_.emplace();
}

BdLitVecUid NongroundProgramBuilder::bodylit(BdLitVecUid body, LitUid bodylit) {
    bodies_[body].push_back(gringo_make_unique<SimpleBodyLiteral>(lits_.erase(bodylit)));
    return body;
}

BdLitVecUid NongroundProgramBuilder::bodyaggr(BdLitVecUid body, Location const &loc, NAF naf, AggregateFunction fun, BoundVecUid bounds, BdAggrElemVecUid bodyaggrelemvec) {
    bodies_[body].push_back(make_locatable<TupleBodyAggregate>(loc, naf, fun, bounds_.erase(bounds), bodyaggrelemvecs_.erase(bodyaggrelemvec)));
    return body;
}

BdLitVecUid NongroundProgramBuilder::bodyaggr(BdLitVecUid body, Location const &loc, NAF naf, AggregateFunction fun, BoundVecUid bounds, CondLitVecUid condlitvecuid) {
    bodies_[body].push_back(make_locatable<LitBodyAggregate>(loc, naf, fun, bounds_.erase(bounds), condlitvecs_.erase(condlitvecuid)));
    return body;
}

BdLitVecUid NongroundProgramBuilder::conjunction(BdLitVecUid body, Location const &loc, LitUid head, LitVecUid litvec) {
    bodies_[body].push_back(make_locatable<Conjunction>(loc, lits_.erase(head), litvecs_.erase(litvec)));
    return body;
}

// {{{2 rule heads

HdLitUid NongroundProgramBuilder::headlit(LitUid lit) {
    return heads_.insert(gringo_make_unique<SimpleHeadLiteral>(lits_.erase(lit)));
}

HdLitUid NongroundProgramBuilder::headaggr(Location const &loc, AggregateFunction fun, BoundVecUid bounds, HdAggrElemVecUid headaggrelemvec) {
    return heads_.insert(make_locatable<TupleHeadAggregate>(loc, fun, bounds_.erase(bounds), headaggrelemvecs_.erase(headaggrelemvec)));
}

HdLitUid NongroundProgramBuilder::headaggr(Location const &loc, AggregateFunction fun, BoundVecUid bounds, CondLitVecUid condlitvec) {
    return heads_.insert(make_locatable<LitHeadAggregate>(loc, fun, bounds_.erase(bounds), condlitvecs_.erase(condlitvec)));
}

HdLitUid NongroundProgramBuilder::disjunction(Location const &loc, CondLitVecUid condlitvec) {
    return heads_.insert(make_locatable<Disjunction>(loc, condlitvecs_.erase(condlitvec)));
}

// {{{2 statements

void NongroundProgramBuilder::rule(Location const &loc, HdLitUid head) {
    rule(loc, head, body());
}

void NongroundProgramBuilder::rule(Location const &loc, HdLitUid head, BdLitVecUid body) {
    prg_.add(make_locatable<Statement>(loc, heads_.erase(head), bodies_.erase(body)));
}

void NongroundProgramBuilder::define(Location const &loc, String name, TermUid value, bool defaultDef, Logger &log) {
    defs_.add(loc, name, terms_.erase(value), defaultDef, log);
}

void NongroundProgramBuilder::optimize(Location const &loc, TermUid weight, TermUid priority, TermVecUid cond, BdLitVecUid body) {
    if (rewriteMinimize_) {
        auto argsUid = termvec(termvec(termvec(), priority), weight);
        termvec(argsUid, term(loc, cond, true));
        auto predUid = predlit(loc, NAF::POS, term(loc, "_criteria", termvecvec(termvecvec(), argsUid), false));
        rule(loc, headlit(predUid), body);
        output_preds_.add(loc, Sig("_criteria", 3, false), true);
    }
    else {
        prg_.add(make_locatable<Statement>(loc, make_locatable<MinimizeHeadLiteral>(loc, terms_.erase(weight), terms_.erase(priority), termvecs_.erase(cond)), bodies_.erase(body)));
    }
}

void NongroundProgramBuilder::showsig(Location const &loc, Sig sig) {
    output_preds_.add(loc, sig, false);
}

void NongroundProgramBuilder::defined(Location const &loc, Sig sig) {
    static_cast<void>(loc);
    prg_.addInput(sig);
}

void NongroundProgramBuilder::show(Location const &loc, TermUid t, BdLitVecUid body) {
    prg_.add(make_locatable<Statement>(loc, make_locatable<ShowHeadLiteral>(loc, terms_.erase(t)), bodies_.erase(body)));
}

void NongroundProgramBuilder::script(Location const &loc, String type, String code) {
    context_.exec(type, loc, code);
}

void NongroundProgramBuilder::block(Location const &loc, String name, IdVecUid args) {
    prg_.begin(loc, name, idvecs_.erase(args));
}

void NongroundProgramBuilder::external(Location const &loc, TermUid head, BdLitVecUid body, TermUid type) {
    prg_.add(make_locatable<Statement>(loc, make_locatable<ExternalHeadAtom>(loc, terms_.erase(head), terms_.erase(type)), bodies_.erase(body)));
}

void NongroundProgramBuilder::edge(Location const &loc, TermVecVecUid edgesUid, BdLitVecUid body) {
    auto edges = termvecvecs_.erase(edgesUid);
    for (auto it = edges.begin(), end = edges.end(), last = end-1; it != end; ++it) {
        prg_.add(make_locatable<Statement>(
            loc,
            make_locatable<EdgeHeadAtom>(
                loc,
                std::move(it->front()),
                std::move(it->back())
            ),
            it == last ? bodies_.erase(body) : get_clone(bodies_[body])
        ));
    }
}

void NongroundProgramBuilder::heuristic(Location const &loc, TermUid termUid, BdLitVecUid body, TermUid a, TermUid b, TermUid mod) {
    prg_.add(make_locatable<Statement>(loc, make_locatable<HeuristicHeadAtom>(loc, terms_.erase(termUid), terms_.erase(a), terms_.erase(b), terms_.erase(mod)), bodies_.erase(body)));
}

void NongroundProgramBuilder::project(Location const &loc, TermUid termUid, BdLitVecUid body) {
    prg_.add(make_locatable<Statement>(loc, make_locatable<ProjectHeadAtom>(loc, terms_.erase(termUid)), bodies_.erase(body)));
}

void NongroundProgramBuilder::project(Location const &loc, Sig sig) {
    Sig s = sig;
    auto tv = termvec();
    for (unsigned i = 0; i < s.arity(); ++i) {
        std::ostringstream ss;
        ss << "X" << i;
        tv = termvec(tv, term(loc, ss.str().c_str()));
    }
    project(loc, predRep(loc, s.sign(), s.name(), termvecvec(termvecvec(), tv)), body());
}

// {{{2 theory

TheoryTermUid NongroundProgramBuilder::theorytermset(Location const &loc, TheoryOptermVecUid args) {
    (void)loc;
    return theoryTerms_.emplace(gringo_make_unique<Output::TupleTheoryTerm>(Output::TupleTheoryTerm::Type::Brace, theoryOptermVecs_.erase(args)));
}

TheoryTermUid NongroundProgramBuilder::theoryoptermlist(Location const &loc, TheoryOptermVecUid args) {
    (void)loc;
    return theoryTerms_.emplace(gringo_make_unique<Output::TupleTheoryTerm>(Output::TupleTheoryTerm::Type::Bracket, theoryOptermVecs_.erase(args)));
}

TheoryTermUid NongroundProgramBuilder::theorytermtuple(Location const &loc, TheoryOptermVecUid args) {
    (void)loc;
    return theoryTerms_.emplace(gringo_make_unique<Output::TupleTheoryTerm>(Output::TupleTheoryTerm::Type::Paren, theoryOptermVecs_.erase(args)));
}

TheoryTermUid NongroundProgramBuilder::theorytermopterm(Location const &loc, TheoryOptermUid opterm) {
    (void)loc;
    return theoryTerms_.emplace(gringo_make_unique<Output::RawTheoryTerm>(theoryOpterms_.erase(opterm)));
}

TheoryTermUid NongroundProgramBuilder::theorytermfun(Location const &loc, String name, TheoryOptermVecUid args) {
    (void)loc;
    return theoryTerms_.emplace(gringo_make_unique<Output::FunctionTheoryTerm>(name, theoryOptermVecs_.erase(args)));
}

TheoryTermUid NongroundProgramBuilder::theorytermvalue(Location const &loc, Symbol val) {
    return theoryTerms_.emplace(gringo_make_unique<Output::TermTheoryTerm>(make_locatable<ValTerm>(loc, val)));
}

TheoryTermUid NongroundProgramBuilder::theorytermvar(Location const &loc, String var) {
    auto &ret(vals_[var]);
    if (!ret) {
        ret = std::make_shared<Symbol>();
    }
    return theoryTerms_.emplace(gringo_make_unique<Output::TermTheoryTerm>(make_locatable<VarTerm>(loc, var, ret)));
}

TheoryOptermUid NongroundProgramBuilder::theoryopterm(TheoryOpVecUid ops, TheoryTermUid term) {
    auto ret = theoryOpterms_.emplace();
    theoryOpterms_[ret].append(theoryOpVecs_.erase(ops), theoryTerms_.erase(term));
    return ret;
}

TheoryOptermUid NongroundProgramBuilder::theoryopterm(TheoryOptermUid opterm, TheoryOpVecUid ops, TheoryTermUid term) {
    theoryOpterms_[opterm].append(theoryOpVecs_.erase(ops), theoryTerms_.erase(term));
    return opterm;
}

TheoryOpVecUid NongroundProgramBuilder::theoryops() {
    return theoryOpVecs_.emplace();
}

TheoryOpVecUid NongroundProgramBuilder::theoryops(TheoryOpVecUid ops, String op) {
    theoryOpVecs_[ops].emplace_back(op);
    return ops;
}

TheoryOptermVecUid NongroundProgramBuilder::theoryopterms() {
    return theoryOptermVecs_.emplace();
}

TheoryOptermVecUid NongroundProgramBuilder::theoryopterms(TheoryOptermVecUid opterms, Location const &loc, TheoryOptermUid opterm) {
    static_cast<void>(loc);
    theoryOptermVecs_[opterms].emplace_back(gringo_make_unique<Output::RawTheoryTerm>(theoryOpterms_.erase(opterm)));
    return opterms;
}

TheoryOptermVecUid NongroundProgramBuilder::theoryopterms(Location const &loc, TheoryOptermUid opterm, TheoryOptermVecUid opterms) {
    static_cast<void>(loc);
    theoryOptermVecs_[opterms].insert(theoryOptermVecs_[opterms].begin(), gringo_make_unique<Output::RawTheoryTerm>(theoryOpterms_.erase(opterm)));
    return opterms;
}

TheoryElemVecUid NongroundProgramBuilder::theoryelems() {
    return theoryElems_.emplace();
}

TheoryElemVecUid NongroundProgramBuilder::theoryelems(TheoryElemVecUid elems, TheoryOptermVecUid opterms, LitVecUid cond) {
    theoryElems_[elems].emplace_back(theoryOptermVecs_.erase(opterms), litvecs_.erase(cond));
    return elems;
}

TheoryAtomUid NongroundProgramBuilder::theoryatom(TermUid term, TheoryElemVecUid elems) {
    return theoryAtoms_.emplace(terms_.erase(term), theoryElems_.erase(elems));
}

TheoryAtomUid NongroundProgramBuilder::theoryatom(TermUid term, TheoryElemVecUid elems, String op, Location const &loc, TheoryOptermUid opterm) {
    static_cast<void>(loc);
    return theoryAtoms_.emplace(terms_.erase(term), theoryElems_.erase(elems), op, gringo_make_unique<Output::RawTheoryTerm>(theoryOpterms_.erase(opterm)));
}

BdLitVecUid NongroundProgramBuilder::bodyaggr(BdLitVecUid body, Location const &loc, NAF naf, TheoryAtomUid atom) {
    bodies_[body].emplace_back(make_locatable<BodyTheoryLiteral>(loc, naf, theoryAtoms_.erase(atom)));
    return body;
}

HdLitUid NongroundProgramBuilder::headaggr(Location const &loc, TheoryAtomUid atom) {
    return heads_.emplace(make_locatable<HeadTheoryLiteral>(loc, theoryAtoms_.erase(atom)));
}

// {{{2 theory definitions

TheoryOpDefUid NongroundProgramBuilder::theoryopdef(Location const &loc, String op, unsigned priority, TheoryOperatorType type) {
    return theoryOpDefs_.emplace(loc, op, priority, type);
}

TheoryOpDefVecUid NongroundProgramBuilder::theoryopdefs() {
    return theoryOpDefVecs_.emplace();
}

TheoryOpDefVecUid NongroundProgramBuilder::theoryopdefs(TheoryOpDefVecUid defs, TheoryOpDefUid def) {
    theoryOpDefVecs_[defs].emplace_back(theoryOpDefs_.erase(def));
    return defs;
}

TheoryTermDefUid NongroundProgramBuilder::theorytermdef(Location const &loc, String name, TheoryOpDefVecUid defs, Logger &log) {
    TheoryTermDef def(loc, name);
    for (auto &opDef : theoryOpDefVecs_.erase(defs)) {
        def.addOpDef(std::move(opDef), log);
    }
    return theoryTermDefs_.insert(std::move(def));
}

TheoryAtomDefUid NongroundProgramBuilder::theoryatomdef(Location const &loc, String name, unsigned arity, String termDef, TheoryAtomType type) {
    return theoryAtomDefs_.emplace(loc, name, arity, termDef, type);
}

TheoryAtomDefUid NongroundProgramBuilder::theoryatomdef(Location const &loc, String name, unsigned arity, String termDef, TheoryAtomType type, TheoryOpVecUid ops, String guardDef) {
    return theoryAtomDefs_.emplace(loc, name, arity, termDef, type, theoryOpVecs_.erase(ops), guardDef);
}

TheoryDefVecUid NongroundProgramBuilder::theorydefs() {
    return theoryDefVecs_.emplace();
}

TheoryDefVecUid NongroundProgramBuilder::theorydefs(TheoryDefVecUid defs, TheoryTermDefUid def) {
    theoryDefVecs_[defs].first.emplace_back(theoryTermDefs_.erase(def));
    return defs;
}

TheoryDefVecUid NongroundProgramBuilder::theorydefs(TheoryDefVecUid defs, TheoryAtomDefUid def) {
    theoryDefVecs_[defs].second.emplace_back(theoryAtomDefs_.erase(def));
    return defs;
}

void NongroundProgramBuilder::theorydef(Location const &loc, String name, TheoryDefVecUid defs, Logger &log) {
    TheoryDef def(loc, name);
    auto defsVec = theoryDefVecs_.erase(defs);
    for (auto &termDef : defsVec.first) {
        def.addTermDef(std::move(termDef), log);
    }
    for (auto &atomDef : defsVec.second) {
        def.addAtomDef(std::move(atomDef), log);
    }
    prg_.add(std::move(def), log);
}

// }}}2

NongroundProgramBuilder::~NongroundProgramBuilder() noexcept = default;

// }}}1

} } // namespace Input Gringo

