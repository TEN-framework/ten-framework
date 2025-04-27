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

#include <gringo/output/statements.hh>
#include <gringo/output/theory.hh>
#include <gringo/output/aggregates.hh>
#include <gringo/output/output.hh>
#include <gringo/logger.hh>

namespace Gringo { namespace Output {

// {{{1 definition of helpers to print interval sets

namespace Debug {

std::ostream &operator<<(std::ostream &out, IntervalSet<Symbol>::LBound const &x) {
    out << (x.inclusive ? "[" : "(") << x.bound;
    return out;
}

std::ostream &operator<<(std::ostream &out, IntervalSet<Symbol>::RBound const &x) {
    out << x.bound << (x.inclusive ? "]" : ")");
    return out;
}

std::ostream &operator<<(std::ostream &out, IntervalSet<Symbol>::Interval const &x) {
    out << x.left << "," << x.right;
    return out;
}

std::ostream &operator<<(std::ostream &out, IntervalSet<Symbol> const &x) {
    out << "{";
    print_comma(out, x, ",", [](std::ostream &out, IntervalSet<Symbol>::Interval const &x) { out << x; });
    out << "}";
    return out;
}

} // namespace Debug

namespace {

void printPlainBody(PrintPlain out, LitVec const &body) {
    int sep = 0;
    for (auto const &x : body) {
        switch (sep) {
            case 1: {
                out << ",";
                break;
            }
            case 2: {
                out << ";";
                break;
            }
        }
        call(out.domain, x, &Literal::printPlain, out);
        sep = call(out.domain, x, &Literal::needsSemicolon) ? 2 : 1;
    }
}

void printPlainHead(PrintPlain out, LitVec const &body, bool choice) {
    bool sep = false;
    if (choice) {
        out << "{";
    }
    for (auto const &x : body) {
        if (sep) {
            out << ";";
        }
        call(out.domain, x, &Literal::printPlain, out);
        sep = true;
    }
    if (choice) {
        out << "}";
    }
}

bool showSig(OutputPredicates const &outPreds, Sig sig) {
    return outPreds.contains(sig);
}

} // namespace


// }}}1

// {{{1 definition of Rule

Rule::Rule(bool choice)
: choice_(choice) { }

Rule &Rule::reset(bool choice) {
    choice_ = choice;
    head_.clear();
    body_.clear();
    return *this;
}

Rule &Rule::addHead(LiteralId lit) {
    head_.emplace_back(lit);
    return *this;
}

Rule &Rule::addBody(LiteralId lit) {
    body_.emplace_back(lit);
    return *this;
}

Rule &Rule::negatePrevious(DomainData &data) {
    for (auto &lit : head_) {
        if (call(data, lit, &Literal::isAtomFromPreviousStep)) {
            lit = lit.negate();
            lit = lit.negate();
        }
    }
    return *this;
}

void Rule::print(PrintPlain out, char const *prefix) const {
    out << prefix;
    printPlainHead(out, head_, choice_);
    if (!body_.empty() || head_.empty()) {
        out << ":-";
    }
    printPlainBody(out, body_);
    out << ".\n";
}

void Rule::translate(DomainData &data, Translator &trans) {
    head_.erase(std::remove_if(head_.begin(), head_.end(), [&](LiteralId &lit) {
        if (!call(data, lit, &Literal::isHeadAtom)) {
            if (!choice_) {
                body_.emplace_back(lit.negate());
            }
            return true;
        }
        return false;
    }), head_.end());
    Gringo::Output::translate(data, trans, head_);
    Gringo::Output::translate(data, trans, body_);
    trans.output(data, *this);
}

void Rule::output(DomainData &data, UBackend &out) const {
    BackendAtomVec &hd = data.tempAtoms();
    for (auto const &x : head_) {
        Potassco::Lit_t lit = call(data, x, &Literal::uid);
        assert(lit > 0);
        hd.emplace_back(static_cast<Potassco::Atom_t>(lit));
    }
    BackendLitVec &bd = data.tempLits();
    for (auto const &x : body_) {
        bd.emplace_back(call(data, x, &Literal::uid));
    }
    outputRule(*out, choice_, hd, bd);
}

void Rule::replaceDelayed(DomainData &data, LitVec &delayed) {
    Gringo::Output::replaceDelayed(data, head_, delayed);
    Gringo::Output::replaceDelayed(data, body_, delayed);
}

// {{{1 definition of External

External::External(LiteralId head, Potassco::Value_t type)
: head_(head)
, type_(type) { }

void External::print(PrintPlain out, char const *prefix) const {
    out << prefix << "#external ";
    call(out.domain, head_, &Literal::printPlain, out);
    switch (type_) {
        case Potassco::Value_t::False: {
            out << ".\n";
            break;
        }
        case Potassco::Value_t::True: {
            out << ".[true]\n";
            break;
        }
        case Potassco::Value_t::Free: {
            out << ".[free]\n";
            break;
        }
        case Potassco::Value_t::Release: {
            out << ".[release]\n";
            break;
        }
    }
}

void External::translate(DomainData &data, Translator &trans) {
    trans.output(data, *this);
}

void External::output(DomainData &data, UBackend &out) const {
    Atom_t head = call(data, head_, &Literal::uid);
    out->external(head, type_);
}

void External::replaceDelayed(DomainData &data, LitVec &delayed) {
    static_cast<void>(delayed);
    assert(!call(data, head_, &Literal::isIncomplete));
}

// {{{1 definition of ShowStatement

ShowStatement::ShowStatement(Symbol term, LitVec body)
: term_(term)
, body_(std::move(body)) { }

void ShowStatement::print(PrintPlain out, char const *prefix) const {
    out << prefix;
    out << "#show " << term_;
    if (!body_.empty()) {
        out << ":";
    }
    printPlainBody(out, body_);
    out << ".\n";
}

void ShowStatement::replaceDelayed(DomainData &data, LitVec &delayed) {
    Gringo::Output::replaceDelayed(data, body_, delayed);
}

void ShowStatement::translate(DomainData &data, Translator &trans) {
    Gringo::Output::translate(data, trans, body_);
    trans.showTerm(data, term_, std::move(body_));
}

void ShowStatement::output(DomainData &data, UBackend &out) const {
    // Show statements are taken care of in the translator.
    static_cast<void>(data);
    static_cast<void>(out);
}

// {{{1 definition of ProjectStatement

ProjectStatement::ProjectStatement(LiteralId atom)
: atom_(atom) {
    assert(atom.sign() == NAF::POS);
    assert(atom.type() == AtomType::Predicate);
}

void ProjectStatement::print(PrintPlain out, char const *prefix) const {
    out << prefix << "#project ";
    call(out.domain, atom_, &Literal::printPlain, out);
    out << ".\n";
}

void ProjectStatement::translate(DomainData &data, Translator &trans) {
    trans.output(data, *this);
}

void ProjectStatement::output(DomainData &data, UBackend &out) const {
    BackendAtomVec atoms;
    atoms.emplace_back(call(data, atom_, &Literal::uid));
    out->project(Potassco::toSpan(atoms));
}

void ProjectStatement::replaceDelayed(DomainData &data, LitVec &delayed) {
    static_cast<void>(data);
    static_cast<void>(delayed);
}

// {{{1 definition of HeuristicStatement

HeuristicStatement::HeuristicStatement(LiteralId atom, int value, int priority, Potassco::Heuristic_t mod, LitVec body)
: atom_(atom)
, value_(value)
, priority_(priority)
, mod_(mod)
, body_(std::move(body)) {
    assert(atom.sign() == NAF::POS);
    assert(atom.type() == AtomType::Predicate);
}

void HeuristicStatement::print(PrintPlain out, char const *prefix) const {
    out << prefix << "#heuristic ";
    call(out.domain, atom_, &Literal::printPlain, out);
    if (!body_.empty()) {
        out << ":";
    }
    printPlainBody(out, body_);
    out << ".[" << value_ << "@" << priority_ << "," << toString(mod_) << "]\n";
}

void HeuristicStatement::translate(DomainData &data, Translator &trans) {
    Gringo::Output::translate(data, trans, body_);
    trans.output(data, *this);
}

void HeuristicStatement::output(DomainData &data, UBackend &out) const {
    auto uid = call(data, atom_, &Literal::uid);
    BackendLitVec bd;
    for (auto const &lit : body_) {
        bd.emplace_back(call(data, lit, &Literal::uid));
    }
    out->heuristic(uid, mod_, value_, priority_, Potassco::toSpan(bd));
}

void HeuristicStatement::replaceDelayed(DomainData &data, LitVec &delayed) {
    Gringo::Output::replaceDelayed(data, body_, delayed);
}

// {{{1 definition of EdgeStatement

EdgeStatement::EdgeStatement(Symbol u, Symbol v, LitVec body)
: u_(u)
, v_(v)
, uidU_(0)
, uidV_(0)
, body_(std::move(body))
{ }

void EdgeStatement::print(PrintPlain out, char const *prefix) const {
    out << prefix;
    out << "#edge(" << u_ << "," << v_ << ")";
    if (!body_.empty()) {
        out << ":";
    }
    printPlainBody(out, body_);
    out << ".\n";
}

void EdgeStatement::translate(DomainData &data, Translator &trans) {
    Gringo::Output::translate(data, trans, body_);
    uidU_ = trans.nodeUid(u_);
    uidV_ = trans.nodeUid(v_);
    trans.output(data, *this);
}

void EdgeStatement::output(DomainData &data, UBackend &out) const {
    BackendLitVec bd;
    for (auto const &x : body_) {
        bd.emplace_back(call(data, x, &Literal::uid));
    }
    out->acycEdge(numeric_cast<int>(uidU_), numeric_cast<int>(uidV_), Potassco::toSpan(bd));
}

void EdgeStatement::replaceDelayed(DomainData &data, LitVec &delayed) {
    Gringo::Output::replaceDelayed(data, body_, delayed);
}

// {{{1 definition of TheoryDirective

TheoryDirective::TheoryDirective(LiteralId theoryLit)
: theoryLit_(theoryLit) {
    assert(theoryLit_.type() == AtomType::Theory);
}

void TheoryDirective::print(PrintPlain out, char const *prefix) const {
    out << prefix;
    call(out.domain, theoryLit_, &Literal::printPlain, out);
    out << ".\n";
}

void TheoryDirective::translate(DomainData &data, Translator &x) {
    x.output(data, *this);
    assert(!data.getAtom<TheoryDomain>(theoryLit_).recursive() && data.getAtom<TheoryDomain>(theoryLit_).type() == TheoryAtomType::Directive);
    call(data, theoryLit_, &Literal::translate, x);
}

void TheoryDirective::output(DomainData &data, UBackend &out) const {
    // Note: taken care of in translate
    static_cast<void>(data);
    static_cast<void>(out);
}

void TheoryDirective::replaceDelayed(DomainData &data, LitVec &delayed) {
    static_cast<void>(data);
    static_cast<void>(delayed);
}

// {{{1 definition of Minimize

void WeakConstraint::translate(DomainData &data, Translator &x) {
    for (auto &z : lits_) {
        z = call(data, z, &Literal::translate, x);
    }
    x.addMinimize(data.tuple(tuple_), getEqualClause(data, x, data.clause(std::move(lits_)), true, false));
}

void WeakConstraint::print(PrintPlain out, char const *prefix) const {
    out << prefix;
    out << ":~";
    printPlainBody(out, lits_);
    out << ".[";
    auto it(tuple_.begin());
    out << *it++ << "@";
    out << *it++;
    for (auto ie(tuple_.end()); it != ie; ++it) {
        out << "," << *it;
    }
    out << "]\n";
}

void WeakConstraint::output(DomainData &data, UBackend &out) const {
    static_cast<void>(data);
    static_cast<void>(out);
    throw std::logic_error("WeakConstraint::output: must not be called");
}

void WeakConstraint::replaceDelayed(DomainData &data, LitVec &delayed) {
    Gringo::Output::replaceDelayed(data, lits_, delayed);
}

// }}}

// {{{1 definition of Translator

Translator::Translator(UAbstractOutput out, bool preserveFacts)
: out_(std::move(out))
, preserveFacts_{preserveFacts}
{ }

void Translator::addMinimize(TupleId tuple, LiteralId cond) {
    minimize_.emplace_back(tuple, cond);
}

void Translator::translate(DomainData &data, OutputPredicates const &outPreds, Logger &log) {
    translateMinimize(data);
    outputSymbols(data, outPreds, log);
}

void Translator::outputSymbols(DomainData &data, OutputPredicates const &outPreds, Logger &log) {
    // show what was requested
    if (outPreds.active()) {
        for (auto const &x : outPreds) {
            auto it(data.predDoms().find(std::get<1>(x)));
            if (it != data.predDoms().end()) {
                showAtom(data, it);
            }
        }
    }
    // show everything non-internal
    else {
        for (auto it = data.predDoms().begin(), ie = data.predDoms().end(); it != ie; ++it) {
            Sig sig = **it;
            auto name(sig.name());
            if (!name.startsWith("#")) { showAtom(data, it); }
        }
    }
    // show terms
    for (auto const &todo : termOutput_.todo) {
        if (todo.second.empty()) { continue; }
        showValue(data, todo.first, updateCond(data, todo));
    }
    termOutput_.todo.clear();
}

LitVec Translator::updateCond(DomainData &data, OutputTable::Todo::value_type const &todo) {
    LiteralId excludeOldCond;
    auto entry = termOutput_.table.try_emplace(todo.first);
    if (!entry.second) {
        LiteralId oldCond = entry.first.value();
        LiteralId newCond = getEqualFormula(data, *this, todo.second, false, false);
        LiteralId includeOldCond = getEqualClause(data, *this, data.clause(LitVec{oldCond, newCond}), false, false);
        excludeOldCond = getEqualClause(data, *this, data.clause(LitVec{oldCond.negate(), newCond}), true, false);
        entry.first.value() = includeOldCond;
    }
    else {
        excludeOldCond = getEqualFormula(data, *this, todo.second, false, false);
        entry.first.value() = excludeOldCond;
    }
    return {excludeOldCond};
}

void Translator::atoms(DomainData &data, unsigned atomset, IsTrueLookup const &isTrue, SymVec &atoms, OutputPredicates const &outPreds) {
    auto isComp = [isTrue, atomset](unsigned x) {
        return (atomset & static_cast<unsigned>(ShowType::Complement)) != 0 ? !isTrue(x) : isTrue(x);
    };
    bool showAtoms = (atomset & static_cast<unsigned>(ShowType::Atoms)) != 0;
    bool showShown = (atomset & static_cast<unsigned>(ShowType::Shown)) != 0;
    bool showTerms = (atomset & static_cast<unsigned>(ShowType::Terms)) != 0;
    if (showAtoms || showShown) {
        for (auto const &x : data.predDoms()) {
            Sig sig = *x;
            auto name = sig.name();
            bool show = showAtoms || (showShown && showSig(outPreds, sig));
            if (show && !name.empty() && !name.startsWith("#")) {
                for (auto &y: *x) {
                    if (y.defined() && y.hasUid() && isComp(y.uid())) {
                        atoms.emplace_back(y);
                    }
                }
            }
        }
    }
    if (showTerms || showShown) {
        for (auto const &entry : termOutput_.table) {
            if (isComp(call(data, entry.second, &Literal::uid))) {
                atoms.emplace_back(entry.first);
            }
        }
    }
}

void Translator::simplify(DomainData &data, Mappings &mappings, AssignmentLookup assignment) {
    minimize_.erase(std::remove_if(minimize_.begin(), minimize_.end(), [&](MinimizeList::value_type &elem) {
        elem.second = call(data, elem.second, &Literal::simplify, mappings, assignment);
        return elem.second == data.getTrueLit().negate();
    }), minimize_.end());
    for (auto it = tuples_.begin(); it != tuples_.end();) {
        it.value() = call(data, it.value(), &Literal::simplify, mappings, assignment);
        if (it.value() == data.getTrueLit().negate()) {
            it = tuples_.unordered_erase(it);
        }
        else {
            ++it;
        }
    }
    for (auto it = termOutput_.table.begin(); it != termOutput_.table.end();) {
        it.value() = call(data, it.value(), &Literal::simplify, mappings, assignment);
        if (it.value() == data.getTrueLit().negate()) {
            it = termOutput_.table.unordered_erase(it);
        }
        else {
            ++it;
        }
    }
}

void Translator::output(DomainData &data, Statement &x) {
    out_->output(data, x);
}

namespace {

class Atomtab : public Statement {
public:
    Atomtab(PredicateDomain::Iterator atom, bool preserveFacts)
    : atom_(atom), preserveFacts_{preserveFacts} { }

    void output(DomainData &data, UBackend &out) const override {
        static_cast<void>(data);
        out->output(*atom_, !preserveFacts_ && atom_->fact() ? 0 : atom_->uid());
    }

    void print(PrintPlain out, char const *prefix) const override {
        out << prefix << "#show " << (Symbol)*atom_;
        if (!atom_->fact()) {
            out << ":" << (Symbol)*atom_;
        }
        out << ".\n";
    }

    void translate(DomainData &data, Translator &trans) override {
        if (!atom_->hasUid()) {
            atom_->setUid(data.newAtom());
        }
        trans.output(data, *this);
    }

    void replaceDelayed(DomainData &data, LitVec &delayed) override {
        static_cast<void>(data);
        static_cast<void>(delayed);
    }

private:
    PredicateDomain::Iterator atom_;
    bool preserveFacts_;
};

} // namespace

void Translator::showAtom(DomainData &data, PredDomMap::iterator it) {
    for (auto jt = (*it)->begin() + (*it)->showOffset(), je = (*it)->end(); jt != je; ++jt) {
        if (jt->defined()) {
            Atomtab{jt, preserveFacts_}.translate(data, *this);
        }
    }
    (*it)->showNext();
}

void Translator::showValue(DomainData &data, Symbol value, LitVec const &cond) {
    Symtab(value, get_clone(cond)).translate(data, *this);
}

void Translator::translateMinimize(DomainData &data) {
    sort_unique(minimize_, [&data](TupleLit const &a, TupleLit const &b) {
        auto aa = data.tuple(a.first);
        auto ab = data.tuple(b.first);
        if (aa[1] != ab[1]) {
            return aa[1] < ab[1];
        }
        return a < b;
    });
    for (auto it = minimize_.begin(), iE = minimize_.end(); it != iE;) {
        int priority = data.tuple(it->first)[1].num();
        Minimize lm(priority);
        do {
            LitVec condLits;
            auto tuple = it->first;
            do {
                condLits.emplace_back(it++->second);
            }
            while (it != iE && it->first == tuple);
            int weight(data.tuple(tuple).first->num());
            // Note: extends the minimize constraint incrementally
            auto ret = tuples_.try_emplace(tuple);
            if (!ret.second) {
                lm.add(ret.first->second, -weight);
                condLits.emplace_back(ret.first->second);
            }
            LiteralId lit = getEqualClause(data, *this, data.clause(std::move(condLits)), false, false);
            ret.first.value() = lit;
            lm.add(lit, weight);
        }
        while (it != iE && data.tuple(it->first)[1].num() == priority);
        out_->output(data, lm);
    }
    minimize_.clear();
}

void Translator::showTerm(DomainData &data, Symbol term, LitVec cond) {
    termOutput_.todo.try_emplace(term, Formula{}).first.value().emplace_back(data.clause(std::move(cond)));
}

unsigned Translator::nodeUid(Symbol v) {
    return nodeUids_.try_emplace(v, nodeUids_.size()).first.value();
}

LiteralId Translator::removeNotNot(DomainData &data, LiteralId lit) {
    if (lit.sign() == NAF::NOTNOT) {
        LiteralId aux = data.newAux();
        Rule().addHead(aux).addBody(lit.negate()).translate(data, *this);
        return aux.negate();
    }
    return lit;
}

constexpr Translator::ClauseKey Translator::ClauseKeyLiterals::open;
constexpr Translator::ClauseKey Translator::ClauseKeyLiterals::deleted;

LiteralId Translator::clause(ClauseId id, bool conjunctive, bool equivalence) {
    auto it = clauses_.find(ClauseKey{id.first, id.second, conjunctive ? 1U : 0U, equivalence ? 1U : 0U, LiteralId().repr()});
    return it != clauses_.end() ? LiteralId{it->literal} : LiteralId{};
}

void Translator::clause(LiteralId lit, ClauseId id, bool conjunctive, bool equivalence) {
    auto ret = clauses_.insert(ClauseKey{id.first, id.second, conjunctive ? 1U : 0U, equivalence ? 1U : 0U, lit.repr()});
    static_cast<void>(ret);
    assert(ret.second);
}

// }}}1

// {{{1 definition of Symtab

Symtab::Symtab(Symbol symbol, LitVec body)
: symbol_(symbol)
, body_(std::move(body)) { }

void Symtab::print(PrintPlain out, char const *prefix) const {
    out << prefix << "#show " << symbol_;
    if (!body_.empty()) {
        out << ":";
    }
    printPlainBody(out, body_);
    out << ".\n";
}

void Symtab::translate(DomainData &data, Translator &trans) {
    for (auto &y : body_) {
        y = call(data, y, &Literal::translate, trans);
    }
    trans.output(data, *this);
}

void Symtab::output(DomainData &data, UBackend &out) const {
    BackendLitVec &bd = data.tempLits();
    for (auto const &x : body_) {
        bd.emplace_back(call(data, x, &Literal::uid));
    }
    std::ostringstream oss;
    oss << symbol_;
    out->output(symbol_, Potassco::toSpan(bd));
}

void Symtab::replaceDelayed(DomainData &data, LitVec &delayed) {
    Gringo::Output::replaceDelayed(data, body_, delayed);
}

// {{{1 definition of Minimize

Minimize::Minimize(int priority)
: priority_(priority) { }

Minimize &Minimize::add(LiteralId lit, Potassco::Weight_t weight) {
    lits_.emplace_back(lit, weight);
    return *this;
}

void Minimize::translate(DomainData &data, Translator &x) {
    for (auto &y : lits_) {
        y.first = call(data, y.first, &Literal::translate, x);
    }
    x.output(data, *this);
}

void Minimize::print(PrintPlain out, char const *prefix) const {
    int i = 0;
    out << prefix << "#minimize{";
    auto f = [&i, this](PrintPlain &out, LitWeightVec::value_type const &x) {
        out << x.second << "@" << priority_ << "," << i++ << ":";
        call(out.domain, x.first, &Literal::printPlain, out);
    };
    print_comma(out, lits_, ";", f);
    out << "}.\n";
}

void Minimize::output(DomainData &data, UBackend &out) const {
    BackendLitWeightVec &body = data.tempWLits();
    for (auto const &y : lits_) {
        body.push_back({call(data, y.first, &Literal::uid), y.second});
    }
    out->minimize(priority_, Potassco::toSpan(body));
}

void Minimize::replaceDelayed(DomainData &data, LitVec &delayed) {
    for (auto &x : lits_) {
        Gringo::Output::replaceDelayed(data, x.first, delayed);
    }
}

// {{{1 definition of WeightRule

WeightRule::WeightRule(LiteralId head, Potassco::Weight_t lower, LitUintVec body)
: head_(head)
, body_(std::move(body))
, lower_(lower) { }

void WeightRule::print(PrintPlain out, char const *prefix) const {
    out << prefix;
    call(out.domain, head_, &Literal::printPlain, out);
    out << ":-" << lower_ << "{";
    if (!body_.empty()) {
        auto it = body_.begin();
        auto ie = body_.end();
        call(out.domain, it->first, &Literal::printPlain, out);
        out << "=" << it->second;
        for (++it; it != ie; ++it) {
            out << ",";
            call(out.domain, it->first, &Literal::printPlain, out);
            out << "=" << it->second;
        }
    }
    out << "}.\n";
}

void WeightRule::translate(DomainData &data, Translator &trans) {
    head_ = call(data, head_, &Literal::translate, trans);
    if (!call(data, head_, &Literal::isHeadAtom)) {
        LiteralId aux = data.newAux();
        Rule().addHead(head_).addBody(aux).translate(data, trans);
        head_ = aux;
    }
    for (auto &y : body_) {
        y.first = call(data, y.first, &Literal::translate, trans);
    }
    trans.output(data, *this);
}

void WeightRule::output(DomainData &data, UBackend &out) const {
    BackendLitWeightVec lits;
    for (auto const &x : body_) {
        lits.push_back({call(data, x.first, &Literal::uid), static_cast<Potassco::Weight_t>(x.second)});
    }
    BackendAtomVec heads({static_cast<Potassco::Atom_t>(call(data, head_, &Literal::uid))});
    outputRule(*out, false, heads, lower_, lits);
}

void WeightRule::replaceDelayed(DomainData &data, LitVec &delayed) {
    Gringo::Output::replaceDelayed(data, head_, delayed);
    for (auto &x : body_) {
        Gringo::Output::replaceDelayed(data, x.first, delayed);
    }
}

// }}}1

} } // namespace Output Gringo
