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

#include "gringo/ground/statements.hh"
#include "gringo/safetycheck.hh"
#include "gringo/output/literals.hh"
#include "gringo/output/output.hh"
#include "gringo/ground/binders.hh"
#include "gringo/logger.hh"
#include <limits>

namespace Gringo { namespace Ground {

// FIXME: too much c&p similarities in the implementations below

// {{{1 Helpers

namespace {

// {{{2 definition of _initBounds

Output::DisjunctiveBounds _initBounds(BoundVec const &bounds, Logger &log) {
    Output::DisjunctiveBounds set;
    set.add({{Symbol::createInf(),true}, {Symbol::createSup(),true}});
    for (auto const &x : bounds) {
        bool undefined = false;
        Symbol v(x.bound->eval(undefined, log));
        assert(!undefined);
        switch (x.rel) {
            case Relation::GEQ: {
                set.remove({{Symbol::createInf(), true}, {v, false}});
                break;
            }
            case Relation::GT:  {
                set.remove({{Symbol::createInf(), true}, {v, true}});
                break;
            }
            case Relation::LEQ: {
                set.remove({{v, false}, {Symbol::createSup(), true}});
                break;
            }
            case Relation::LT:  {
                set.remove({{v, true}, {Symbol::createSup(), true}});
                break;
            }
            case Relation::NEQ: {
                set.remove({{v, true}, {v, true}});
                break;
            }
            case Relation::EQ: {
                set.remove({{v, false}, {Symbol::createSup(), true}});
                set.remove({{Symbol::createInf(), true}, {v, false}});
                break;
            }
        }
    }
    return set;
}

// {{{2 definition of _linearize (Note: could be moved into AbstractStatement)

struct Ent {
    Ent(BinderType type, Literal &lit) : type(type), lit(lit) { }
    std::vector<unsigned> depends;
    std::vector<unsigned> vars;
    BinderType type;
    Literal &lit;
};
using SC  = SafetyChecker<unsigned, Ent>;

InstVec _linearize(Logger &log, Context &context, bool positive, SolutionCallback &cb, Term::VarSet &&important, ULitVec const &lits, Term::VarSet const &boundInitially = Term::VarSet()) {
    InstVec insts;
    std::vector<unsigned> rec;
    std::vector<std::vector<std::pair<BinderType,Literal*>>> todo{1};
    unsigned i{0};
    for (auto const &x : lits) {
        todo.back().emplace_back(BinderType::ALL, x.get());
        if (x->isRecursive() && x->occurrence() != nullptr && !x->occurrence()->isNegative()) {
            rec.emplace_back(i);
        }
        ++i;
    }
    todo.reserve(std::max(std::vector<unsigned>::size_type(1), rec.size()));
    insts.reserve(todo.capacity()); // Note: preserve references
    for (auto i : rec) {
        todo.back()[i].first = BinderType::NEW;
        if (i != rec.back()) {
            todo.emplace_back(todo.back());
            todo.back()[i].first = BinderType::OLD;
        }
    }
    if (!positive) {
        for (auto const &lit : lits) {
            if (!lit->auxiliary()) {
                lit->collectImportant(important);
            }
        }
    }
    for (auto &x : todo) {
        Term::VarSet bound = boundInitially;
        insts.emplace_back(cb);
        SC s;
        std::unordered_map<String, SC::VarNode*> varMap;
        std::vector<std::pair<String, std::vector<unsigned>>> boundBy;
        for (auto &lit : x) {
            auto &entNode(s.insertEnt(lit.first, *lit.second));
            VarTermBoundVec vars;
            lit.second->collect(vars);
            for (auto &occ : vars) {
                if (bound.find(occ.first->name) == bound.end()) {
                    auto &varNode(varMap[occ.first->name]);
                    if (varNode == nullptr)   {
                        varNode = &s.insertVar(numeric_cast<unsigned>(boundBy.size()));
                        boundBy.emplace_back(occ.first->name, std::vector<unsigned>{});
                    }
                    if (occ.second) {
                        s.insertEdge(entNode, *varNode);
                    }
                    else {
                        s.insertEdge(*varNode, entNode);
                    }
                    entNode.data.vars.emplace_back(varNode->data);
                }
            }
        }
        Instantiator::DependVec depend;
        unsigned uid = 0;
        auto pred = [&bound, &log](Ent const &x, Ent const &y) -> bool {
            double sx(x.lit.score(bound, log));
            double sy(y.lit.score(bound, log));
            //std::cerr << "  " << x.lit << "@" << sx << " < " << y.lit << "@" << sy << " with " << bound.size() << std::endl;
            if (sx < 0 || sy < 0) {
                return sx < sy;
            }
            if ((x.type == BinderType::NEW || y.type == BinderType::NEW) && x.type != y.type) {
                return x.type < y.type;
            }
            return sx < sy;
        };

        SC::EntVec open;
        s.init(open);
        while (!open.empty()) {
            for (auto it = open.begin(), end = open.end() - 1; it != end; ++it) {
                if (pred((*it)->data, open.back()->data)) {
                    std::swap(open.back(), *it);
                }
            }
            auto *y = open.back();
            for (auto &var : y->data.vars) {
                auto &bb(boundBy[var]);
                if (bound.find(bb.first) == bound.end()) {
                    bb.second.emplace_back(uid);
                    if ((depend.empty() || depend.back() != uid) && important.find(bb.first) != important.end()) {
                        depend.emplace_back(uid);
                    }
                }
                else {
                    y->data.depends.insert(y->data.depends.end(), bb.second.begin(), bb.second.end());
                }
            }
            auto index(y->data.lit.index(context, y->data.type, bound));
            if (auto *update = index->getUpdater()) {
                if (BodyOcc *occ = y->data.lit.occurrence()) {
                    for (HeadOccurrence &x : occ->definedBy()) {
                        x.defines(*update, y->data.type == BinderType::NEW ? &insts.back() : nullptr);
                    }
                }
            }
            std::sort(y->data.depends.begin(), y->data.depends.end());
            y->data.depends.erase(std::unique(y->data.depends.begin(), y->data.depends.end()), y->data.depends.end());
            insts.back().add(std::move(index), std::move(y->data.depends));
            uid++;
            open.pop_back();
            s.propagate(y, open);
        }
        insts.back().finalize(std::move(depend));
    }
    return insts;
}

// {{{2 definition of completeRepr_

UTerm completeRepr_(UTerm const &repr) {
    UTermVec ret;
    ret.emplace_back(get_clone(repr));
    return make_locatable<FunctionTerm>(repr->loc(), String{"#complete"}, std::move(ret));
}

// {{{2 definition of BindOnce

class BindOnce : public Binder, public IndexUpdater {
public:
    IndexUpdater *getUpdater() override {
        return this;
    }

    bool update() override {
        return true;
    }

    void match(Logger &log) override {
        static_cast<void>(log);
        once_ = true;
    }

    bool next() override {
        bool ret = once_;
        once_ = false;
        return ret;
    }

    void print(std::ostream &out) const override {
        out << "#once_";
    }

private:
    bool once_ = false;
};

// {{{2 operator <<

template <class T>
std::ostream &operator <<(std::ostream &out, std::unique_ptr<T> const &x) {
    out << *x;
    return out;
}

template <class T>
std::ostream &operator <<(std::ostream &out, std::vector<T> const &x) {
    if (!x.empty()) {
        for (auto it = x.begin(), ie = x.end(); ; ) {
            out << *it;
            if (++it != ie) {
                out << ",";
            }
            else {
                break;
            }
        }
    }
    return out;
}
std::ostream &operator <<(std::ostream &out, OccurrenceType x) {
    switch (x) {
        case OccurrenceType::POSITIVELY_STRATIFIED: {
            break;
        }
        case OccurrenceType::STRATIFIED: {
            out << "!";
            break;
        }
        case OccurrenceType::UNSTRATIFIED: {
            out << "?";
            break;
        }
    }
    return out;
}

// }}}2

} // namespace

// }}}1
// {{{1 definition of HeadDefinition

HeadDefinition::HeadDefinition(UTerm repr, Domain *domain)
: repr_(std::move(repr))
, domain_(domain) { }

void HeadDefinition::defines(IndexUpdater &update, Instantiator *inst) {
    auto ret(offsets_.emplace(&update, numeric_cast<unsigned>(enqueueVec_.size())));
    if (ret.second) {
        enqueueVec_.emplace_back(&update, RInstVec{});
    }
    if (active_ && inst != nullptr) {
        enqueueVec_[ret.first->second].second.emplace_back(*inst);
    }
}

void HeadDefinition::enqueue(Queue &queue) {
    if (domain_ != nullptr) {
        queue.enqueue(*domain_);
    }
    for (auto &x : enqueueVec_) {
        if (x.first->update()) {
            for (Instantiator &y : x.second) {
                y.enqueue(queue);
            }
        }
    }
}

void HeadDefinition::collectImportant(Term::VarSet &vars) {
    if (repr_) {
        VarTermBoundVec x;
        repr_->collect(x, false);
        for (auto &occ : x) {
            vars.emplace(occ.first->name);
        }
    }
}

//{{{1 definition of AbstractStatement

AbstractStatement::AbstractStatement(UTerm repr, Domain *domain, ULitVec lits)
: def_(std::move(repr), domain)
, lits_(std::move(lits)) { }

bool AbstractStatement::isOutputRecursive() const {
    for (auto const &x : lits_) {
        // TODO: only postive literals are relevant here!
        if (!x->auxiliary() && x->isRecursive()) {
            return true;
        }
    }
    return false;
}

bool AbstractStatement::isNormal() const {
    return false;
}

void AbstractStatement::analyze(Dep::Node &node, Dep &dep) {
    def_.analyze(node, dep);
    for (auto &x : lits_) {
        auto *occ(x->occurrence());
        if (occ != nullptr) {
            dep.depends(node, *occ, false);
        }
    }
}

void AbstractStatement::startLinearize(bool active) {
    def_.setActive(active);
    if (active) {
        insts_.clear();
    }
}

void AbstractStatement::collectImportant(Term::VarSet &vars) {
    def_.collectImportant(vars);
}
void AbstractStatement::linearize(Context &context, bool positive, Logger &log) {
    Term::VarSet important;
    collectImportant(important);
    insts_ = _linearize(log, context, positive, *this, std::move(important), lits_);
}

void AbstractStatement::enqueue(Queue &q) {
    def_.init();
    for (auto &x : insts_) {
        x.enqueue(q);
    }
}

void AbstractStatement::printHead(std::ostream &out) const {
    if (def_) {
        out << *def_.domRepr();
    }
    else {
        out << "#false";
    }
}
void AbstractStatement::printBody(std::ostream &out) const {
    out << lits_;
}

void AbstractStatement::print(std::ostream &out) const {
    printHead(out);
    if (!lits_.empty()) {
        out << ":-";
        printBody(out);
    }
    out << ".";
}

void AbstractStatement::propagate(Queue &queue) {
    def_.enqueue(queue);
}

//}}}1

// Statements

// {{{1 definition of ExternalRule

ExternalRule::ExternalRule()
: def_(make_locatable<ValTerm>(Location("#external", 1, 1, "#external", 1, 1), Symbol::createId("#external")), nullptr) { }

bool ExternalRule::isNormal() const {
    return false;
}

void ExternalRule::analyze(Dep::Node &node, Dep &dep) {
    def_.analyze(node, dep);
}

void ExternalRule::startLinearize(bool active) {
    def_.setActive(active);
}

void ExternalRule::linearize(Context &context, bool positive, Logger &logger) {
    static_cast<void>(context);
    static_cast<void>(positive);
    static_cast<void>(logger);
}

void ExternalRule::enqueue(Queue &queue) {
    static_cast<void>(queue);
}

void ExternalRule::print(std::ostream &out) const {
    out << "#external.";
}

//{{{1 definition of AbstractRule

AbstractRule::AbstractRule(HeadVec heads, ULitVec lits)
: lits_(std::move(lits)) {
    defs_.reserve(heads.size());
    for (auto &head : heads) {
        assert(head.first && head.second);
        defs_.emplace_back(std::move(head.first), head.second);
    }
}

void AbstractRule::analyze(Dep::Node &node, Dep &dep) {
    for (auto &def : defs_) {
        def.analyze(node, dep);
    }
    for (auto &x : lits_) {
        auto *occ(x->occurrence());
        if (occ != nullptr) {
            dep.depends(node, *occ, false);
        }
    }
}

void AbstractRule::startLinearize(bool active) {
    for (auto &def : defs_) {
        def.setActive(active);
    }
    if (active) {
        insts_.clear();
    }
}

void AbstractRule::linearize(Context &context, bool positive, Logger &log) {
    Term::VarSet important;
    for (auto &def : defs_) {
        def.collectImportant(important);
    }
    insts_ = _linearize(log, context, positive, *this, std::move(important), lits_);
}

void AbstractRule::enqueue(Queue &q) {
    for (auto &def : defs_) {
        def.init();
    }
    for (auto &x : insts_) {
        x.enqueue(q);
    }
}

void AbstractRule::propagate(Queue &queue) {
    for (auto &def : defs_) {
        def.enqueue(queue);
    }
}

//{{{1 definition of Rule

template <bool disjunctive>
Rule<disjunctive>::Rule(HeadVec heads, ULitVec lits)
: AbstractRule(std::move(heads), std::move(lits)) { }

template <bool disjunctive>
void Rule<disjunctive>::printHead(std::ostream &out) const {
    if (!disjunctive) {
        out << "{";
    }
    else if (defs_.empty()) {
        out << "#false";
    }
    bool sep = false;
    for (auto &def : defs_) {
        if (sep) {
            out << ";";
        }
        else {
            sep = true;
        }
        out << *def.domRepr();
    }
    if (!disjunctive) {
        out << "}";
    }
}

template <bool disjunctive>
void Rule<disjunctive>::print(std::ostream &out) const {
    printHead(out);
    if (!lits_.empty()) {
        out << ":-";
        out << lits_;
    }
    out << ".";
}

template <bool disjunctive>
bool Rule<disjunctive>::isNormal() const {
    return defs_.size() == 1 && disjunctive;
}

template <bool disjunctive>
void Rule<disjunctive>::report(Output::OutputBase &out, Logger &log) {
    Output::Rule &rule(out.tempRule(!disjunctive));
    bool fact = true;
    for (auto &x : lits_) {
        if (x->auxiliary()) {
            continue;
        }
        auto ret = x->toOutput(log);
        if (ret.first.valid() && (out.keepFacts || !ret.second)) {
            rule.addBody(ret.first);
        }
        if (!ret.second) {
            fact = false;
        }
    }
    for (auto &def : defs_) {
        bool undefined = false;
        Symbol val = def.domRepr()->eval(undefined, log);
        if (undefined) {
            if (!disjunctive) {
                continue;
            }
            return;
        }
        auto &dom = static_cast<PredicateDomain&>(def.dom());
        auto ret(dom.define(val));
        if (!ret.first->fact()) {
            Potassco::Id_t offset = static_cast<Potassco::Id_t>(ret.first - dom.begin());
            rule.addHead({NAF::POS, Output::AtomType::Predicate, offset, dom.domainOffset()});
        }
        else if (disjunctive) {
            return;
        }
    }
    if (!disjunctive && rule.numHeads() == 0) {
        return;
    }
    if (disjunctive && fact && rule.numHeads() == 1) {
        Output::LiteralId head = rule.heads().front();
        out.predDom(head.domain())[head.offset()].setFact(true);
    }
    out.output(rule);
}

template class Rule<true>;
template class Rule<false>;

//{{{1 definition of ExternalStatement

ExternalStatement::ExternalStatement(HeadVec heads, ULitVec lits, UTerm type)
: AbstractRule(std::move(heads), std::move(lits))
, type_(std::move(type)) { }

void ExternalStatement::printHead(std::ostream &out) const {
    out << "#external ";
    bool sep = false;
    for (auto const &def : defs_) {
        if (sep) {
            out << ";";
        }
        else {
            sep = true;
        }
        out << *def.domRepr();
    }
}

void ExternalStatement::print(std::ostream &out) const {
    printHead(out);
    if (!lits_.empty()) {
        out << ":";
        out << lits_;
    }
    out << ".";
}

bool ExternalStatement::isNormal() const {
    return false;
}

void ExternalStatement::report(Output::OutputBase &out, Logger &log) {
    for (auto &def : defs_) {
        bool undefined = false;
        Symbol val(def.domRepr()->eval(undefined, log));
        if (undefined) {
            continue;
        }
        Gringo::Symbol term = type_->eval(undefined, log);
        if (undefined) {
            continue;
        }
        bool id = term.type() == Gringo::SymbolType::Fun && term.arity() == 0;
        Potassco::Value_t value;
        if (id && term.name() == "false") {
            value = Potassco::Value_t::False;
        }
        else if (id && term.name() == "true") {
            value = Potassco::Value_t::True;
        }
        else if (id && term.name() == "free") {
            value = Potassco::Value_t::Free;
        }
        else if (id && term.name() == "release") {
            value = Potassco::Value_t::Release;
        }
        else {
            // TODO: report something
            continue;
        }
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
        auto &dom = static_cast<PredicateDomain&>(def.dom());
        auto ret = dom.define(val, false);
        Potassco::Id_t offset = static_cast<Potassco::Id_t>(std::get<0>(ret) - dom.begin());
        std::get<0>(ret)->setExternal(true);
        Output::External external({NAF::POS, Output::AtomType::Predicate, offset, dom.domainOffset()}, value);
        out.output(external);
    }
}


// }}}1

// {{{1 definition of ShowStatement

ShowStatement::ShowStatement(UTerm term, ULitVec body)
: AbstractStatement(nullptr, nullptr, std::move(body))
, term_(std::move(term)) { }

void ShowStatement::report(Output::OutputBase &out, Logger &log) {
    bool undefined = false;
    Symbol term = term_->eval(undefined, log);
    if (!undefined) {
        Output::LitVec &cond = out.tempLits();
        for (auto &x : lits_) {
            if (x->auxiliary()) {
                continue;
            }
            auto lit = x->toOutput(log);
            if (!lit.second) {
                cond.emplace_back(lit.first);
            }
        }
        Output::ShowStatement ss(term, cond);
        out.output(ss);
    }
    else {
        GRINGO_REPORT(log, Warnings::OperationUndefined)
            << term_->loc() << ": info: tuple ignored:\n"
            << "  " << term << "\n";
    }
}

void ShowStatement::collectImportant(Term::VarSet &vars) {
    term_->collect(vars);
}

void ShowStatement::printHead(std::ostream &out) const {
    out << "#show ";
    out << *term_;
}

void ShowStatement::print(std::ostream &out) const {
    printHead(out);
    out << ":" << lits_ << ".";
}

// {{{1 definition of EdgeStatement

EdgeStatement::EdgeStatement(UTerm u, UTerm v, ULitVec body)
: AbstractStatement(nullptr, nullptr, std::move(body))
, u_(std::move(u))
, v_(std::move(v))
{ }

void EdgeStatement::report(Output::OutputBase &out, Logger &log) {
    bool undefined = false;
    Symbol u = u_->eval(undefined, log);
    if (undefined) {
        GRINGO_REPORT(log, Warnings::OperationUndefined)
            << u_->loc() << ": info: edge ignored\n";
        return;
    }
    Symbol v = v_->eval(undefined, log);
    if (undefined) {
        GRINGO_REPORT(log, Warnings::OperationUndefined)
            << v_->loc() << ": info: edge ignored\n";
        return;
    }
    Output::LitVec &cond = out.tempLits();
    for (auto &x : lits_) {
        if (x->auxiliary()) {
            continue;
        }
        auto lit = x->toOutput(log);
        if (!lit.second) {
            cond.emplace_back(lit.first);
        }
    }
    Output::EdgeStatement es(u, v, cond);
    out.output(es);
}

void EdgeStatement::collectImportant(Term::VarSet &vars) {
    u_->collect(vars);
    v_->collect(vars);
}

void EdgeStatement::printHead(std::ostream &out) const {
    out << "#edge (" << *u_ << "," << *v_ << ")";
}

void EdgeStatement::print(std::ostream &out) const {
    printHead(out);
    out << ":" << lits_ << ".";
}

// {{{1 definition of ProjectStatement

ProjectStatement::ProjectStatement(UTerm atom, ULitVec body)
: AbstractStatement(nullptr, nullptr, std::move(body))
, atom_(std::move(atom))
{ }

void ProjectStatement::report(Output::OutputBase &out, Logger &log) {
    bool undefined = false;
    Symbol term = atom_->eval(undefined, log);
    assert(!undefined && term.hasSig());
    auto domain = out.data.predDoms().find(term.sig());
    assert(domain != out.data.predDoms().end());
    auto atom = (*domain)->find(term);
    Id_t offset = numeric_cast<Id_t>(atom - (*domain)->begin());
    Output::ProjectStatement ps(Output::LiteralId{NAF::POS, Output::AtomType::Predicate, offset, (*domain)->domainOffset()});
    out.output(ps);
}

void ProjectStatement::collectImportant(Term::VarSet &vars) {
    atom_->collect(vars);
}

void ProjectStatement::printHead(std::ostream &out) const {
    out << "#project ";
    out << *atom_;
}

void ProjectStatement::print(std::ostream &out) const {
    printHead(out);
    out << ":" << lits_ << ".";
}

// {{{1 definition of HeuristicStatement

HeuristicStatement::HeuristicStatement(UTerm atom, UTerm value, UTerm bias, UTerm mod, ULitVec body)
: AbstractStatement(nullptr, nullptr, std::move(body))
, atom_(std::move(atom))
, value_(std::move(value))
, priority_(std::move(bias))
, mod_(std::move(mod))
{ }

void HeuristicStatement::report(Output::OutputBase &out, Logger &log) {
    bool undefined = false;
    // determine atom
    Symbol term = atom_->eval(undefined, log);
    assert(!undefined && term.hasSig());
    auto domain = out.data.predDoms().find(term.sig());
    assert(domain != out.data.predDoms().end());
    auto atom = (*domain)->find(term);
    assert (atom != (*domain)->end());
    // check value
    Symbol value = value_->eval(undefined, log);
    if (undefined || value.type() != SymbolType::Num) {
        GRINGO_REPORT(log, Warnings::OperationUndefined)
            << value_->loc() << ": info: heuristic directive ignored\n";
        return;
    }
    // check priority
    Symbol priority = priority_->eval(undefined, log);
    if (undefined || priority.type() != SymbolType::Num || priority.num() < 0) {
        GRINGO_REPORT(log, Warnings::OperationUndefined)
            << priority_->loc() << ": info: heuristic directive ignored\n";
        return;
    }
    // check modifier
    Symbol mod = mod_->eval(undefined, log);
    if (undefined) {
        mod = Symbol::createId("");
    }
    Potassco::Heuristic_t heuMod;
    if      (mod == Symbol::createId("true")) {
        heuMod = Potassco::Heuristic_t::True;
    }
    else if (mod == Symbol::createId("false")) {
        heuMod = Potassco::Heuristic_t::False;
    }
    else if (mod == Symbol::createId("level"))  {
        heuMod = Potassco::Heuristic_t::Level;
    }
    else if (mod == Symbol::createId("factor")) {
        heuMod = Potassco::Heuristic_t::Factor;
    }
    else if (mod == Symbol::createId("init"))   {
        heuMod = Potassco::Heuristic_t::Init;
    }
    else if (mod == Symbol::createId("sign")) {
        heuMod = Potassco::Heuristic_t::Sign;
    }
    else {
        GRINGO_REPORT(log, Warnings::OperationUndefined)
            << mod_->loc() << ": info: heuristic directive ignored\n";
        return;
    }
    // TODO: this loop appears far too often...
    Output::LitVec &cond = out.tempLits();
    for (auto &x : lits_) {
        if (x->auxiliary()) {
            continue;
        }
        auto lit = x->toOutput(log);
        if (!lit.second) {
            cond.emplace_back(lit.first);
        }
    }
    Id_t offset = numeric_cast<Id_t>(atom - (*domain)->begin());
    auto atomId = Output::LiteralId{NAF::POS, Output::AtomType::Predicate, offset, (*domain)->domainOffset()};
    Output::HeuristicStatement hs(atomId, value.num(), priority.num(), heuMod, cond);
    out.output(hs);
}

void HeuristicStatement::collectImportant(Term::VarSet &vars) {
    atom_->collect(vars);
    value_->collect(vars);
    priority_->collect(vars);
    mod_->collect(vars);
}

void HeuristicStatement::printHead(std::ostream &out) const {
    out << "#heuristic ";
    out << *atom_ << "[" << *value_ << "@" << *priority_ << "," << *mod_ << "]";
}

void HeuristicStatement::print(std::ostream &out) const {
    printHead(out);
    out << ":" << lits_ << ".";
}

// {{{1 definition of WeakConstraint

WeakConstraint::WeakConstraint(UTermVec tuple, ULitVec body)
: AbstractStatement(nullptr, nullptr, std::move(body))
, tuple_(std::move(tuple)) { }

void WeakConstraint::report(Output::OutputBase &out, Logger &log) {
    SymVec &tempVals = out.tempVals();
    bool undefined = false;
    for (auto &x : tuple_) {
        tempVals.emplace_back(x->eval(undefined, log));
    }
    if (!undefined && tempVals[0].type() == SymbolType::Num && tempVals[1].type() == SymbolType::Num) {
        Output::LitVec &tempLits = out.tempLits();
        for (auto &x : lits_) {
            if (x->auxiliary()) {
                continue;
            }
            auto lit = x->toOutput(log);
            if (!lit.second) {
                tempLits.emplace_back(lit.first);
            }
        }
        Output::WeakConstraint min(tempVals, tempLits);
        out.output(min);
    }
    else if (!undefined) {
        GRINGO_REPORT(log, Warnings::OperationUndefined)
            << tuple_.front()->loc() << ": info: tuple ignored:\n"
            << "  " << out.tempVals_.front() << "@" << out.tempVals_[1] << "\n";
    }
}

void WeakConstraint::collectImportant(Term::VarSet &vars) {
    for (auto &x : tuple_) {
        x->collect(vars);
    }
}

void WeakConstraint::printHead(std::ostream &out) const {
    assert(tuple_.size() > 1);
    out << "[";
    out << tuple_.front() << "@" << tuple_[1];
    for (auto it(tuple_.begin() + 2), ie(tuple_.end()); it != ie; ++it) {
        out << "," << *it;
    }
    out << "]";
}

void WeakConstraint::print(std::ostream &out) const {
    out << ":~" << lits_ << ".";
    printHead(out);
}

// }}}1

// Body Aggregates

// {{{1 definition of BodyAggregateComplete

BodyAggregateComplete::BodyAggregateComplete(DomainData &data, UTerm repr, AggregateFunction fun, BoundVec bounds)
: def_(std::move(repr), &data.add<BodyAggregateDomain>())
, accuRepr_(completeRepr_(def_.domRepr()))
, fun_(fun)
, bounds_(std::move(bounds))
, inst_(*this) {
    switch (fun) {
        case AggregateFunction::COUNT:
        case AggregateFunction::SUMP:
        case AggregateFunction::MAX: {
            for (auto &x : bounds_) {
                if (x.rel != Relation::GT && x.rel != Relation::GEQ) {
                    monotone_ = false;
                    break;
                }
            }
            break;
        }
        case AggregateFunction::MIN: {
            for (auto &x : bounds_) {
                if (x.rel != Relation::LT && x.rel != Relation::LEQ) {
                    monotone_ = false;
                    break;
                }
            }
            break;
        }
        default: {
            monotone_ = false;
            break;
        }
    }
}

bool BodyAggregateComplete::isNormal() const {
    return true;
}

void BodyAggregateComplete::analyze(Dep::Node &node, Dep &dep) {
    dep.depends(node, *this);
    def_.analyze(node, dep);
}

void BodyAggregateComplete::startLinearize(bool active) {
    def_.setActive(active);
    if (active) {
        inst_ = Instantiator(*this);
    }
}

void BodyAggregateComplete::linearize(Context &context, bool positive, Logger &log) {
    static_cast<void>(context);
    static_cast<void>(positive);
    static_cast<void>(log);
    auto binder  = gringo_make_unique<BindOnce>();
    for (HeadOccurrence &x : defBy_) {
        x.defines(*binder->getUpdater(), &inst_);
    }
    inst_.add(std::move(binder), Instantiator::DependVec{});
    inst_.finalize(Instantiator::DependVec{});
}

void BodyAggregateComplete::enqueue(Queue &q) {
    def_.init();
    q.enqueue(inst_);
}

void BodyAggregateComplete::printHead(std::ostream &out) const {
    out << *def_.domRepr();
}

void BodyAggregateComplete::propagate(Queue &queue) {
    def_.enqueue(queue);
}

void BodyAggregateComplete::report(Output::OutputBase &out, Logger &log) {
    static_cast<void>(out);
    static_cast<void>(log);
    auto &dom = this->dom();
    for (auto &offset : todo_) {
        auto &atm = dom[offset];
        assert(offset < dom.size());
        if (atm.satisfiable()) {
            dom.define(offset);
        }
        atm.setRecursive(outputRecursive_);
        atm.setEnqueued(false);
    }
    todo_.clear();
}

void BodyAggregateComplete::print(std::ostream &out) const {
    printHead(out);
    out << ":-";
    print_comma(out, accuDoms_, ",", [this](std::ostream &out, BodyAggregateAccumulate const &x) -> void { x.printHead(out); out << occType_; });
    out << ".";
}

UGTerm BodyAggregateComplete::getRepr() const {
    return accuRepr_->gterm();
}

bool BodyAggregateComplete::isPositive() const {
    return true;
}

bool BodyAggregateComplete::isNegative() const {
    return false;
}

void BodyAggregateComplete::setType(OccurrenceType x) {
    occType_ = x;
}

OccurrenceType BodyAggregateComplete::getType() const {
    return occType_;
}

BodyAggregateComplete::DefinedBy &BodyAggregateComplete::definedBy() {
    return defBy_;
}

void BodyAggregateComplete::checkDefined(LocSet &done, SigSet const &edb, UndefVec &undef) const {
    static_cast<void>(done);
    static_cast<void>(edb);
    static_cast<void>(undef);
}

void BodyAggregateComplete::enqueue(BodyAggregateDomain::Iterator atom) {
    if (!atom->defined() && !atom->enqueued()) {
        atom->setEnqueued(true);
        todo_.emplace_back(numeric_cast<TodoVec::value_type>(atom - dom().begin()));
    }
}

// {{{1 definition of BodyAggregateAccumulate

BodyAggregateAccumulate::BodyAggregateAccumulate(BodyAggregateComplete &complete, UTermVec tuple, ULitVec lits)
: AbstractStatement(get_clone(complete.accuRepr()), nullptr, std::move(lits))
, complete_(complete)
, tuple_(std::move(tuple)) {}

void BodyAggregateAccumulate::collectImportant(Term::VarSet &vars) {
    VarTermBoundVec bound;
    def_.domRepr()->collect(bound, false);
    for (auto &x : tuple_) {
        x->collect(bound, false);
    }
    for (auto &x : bound) {
        vars.emplace(x.first->name);
    }
}

void BodyAggregateAccumulate::report(Output::OutputBase &out, Logger &log) {
    auto &vals = out.tempVals();
    bool undefined = false;
    for (auto &x : tuple_) {
        vals.emplace_back(x->eval(undefined, log));
    }
    Symbol repr(complete_.domRepr()->eval(undefined, log));
    if (!undefined) {
        auto &tempLits = out.tempLits();
        for (auto &x : lits_) {
            if (x->auxiliary()) {
                continue;
            }
            auto lit = x->toOutput(log);
            if (!lit.second) {
                tempLits.emplace_back(lit.first);
            }
        }
        auto &dom = complete_.dom();
        auto atom = dom.reserve(repr);
        if (!atom->initialized()) {
            atom->init(complete_.fun(), _initBounds(complete_.bounds(), log), complete_.monotone());
        }
        atom->accumulate(out.data, tuple_.empty() ? def_.domRepr()->loc() : tuple_.front()->loc(), vals, tempLits, log);
        complete_.enqueue(atom);
    }
}

void BodyAggregateAccumulate::linearize(Context &context, bool positive, Logger &log) {
    AbstractStatement::linearize(context, positive, log);
    if (isOutputRecursive()) {
        complete_.setOutputRecursive();
    }
}

void BodyAggregateAccumulate::printHead(std::ostream &out) const {
    out << "#accu(" << *complete_.domRepr() << ",tuple(" << tuple_ << "))";
}

// {{{1 definition of BodyAggregateLiteral

BodyAggregateLiteral::BodyAggregateLiteral(BodyAggregateComplete &complete, NAF naf, bool auxiliary)
: complete_(complete)
, naf_(naf)
, auxiliary_(auxiliary) { }

UGTerm BodyAggregateLiteral::getRepr() const {
    return complete_.domRepr()->gterm();
}

bool BodyAggregateLiteral::isPositive() const {
    return naf_ == NAF::POS && complete_.monotone();
}

bool BodyAggregateLiteral::isNegative() const {
    return naf_ != NAF::POS;
}

void BodyAggregateLiteral::setType(OccurrenceType x) {
    type_ = x;
}

OccurrenceType BodyAggregateLiteral::getType() const {
    return type_;
}

BodyOcc::DefinedBy &BodyAggregateLiteral::definedBy() {
    return defs_;
}

void BodyAggregateLiteral::checkDefined(LocSet &done, SigSet const &edb, UndefVec &undef) const {
    static_cast<void>(done);
    static_cast<void>(edb);
    static_cast<void>(undef);
}

void BodyAggregateLiteral::print(std::ostream &out) const {
    out << naf_;
    auto it = std::begin(complete_.bounds());
    auto ie = std::end(complete_.bounds());
    if (it != ie) {
        it->bound->print(out);
        out << inv(it->rel);
        ++it;
    }
    out << complete_.fun() << "{" << *complete_.domRepr() << type_ << "}";
    if (it != ie) {
        out << it->rel;
        it->bound->print(out);
    }
}

bool BodyAggregateLiteral::isRecursive() const {
    return type_ == OccurrenceType::UNSTRATIFIED;
}

BodyOcc *BodyAggregateLiteral::occurrence() {
    return this;
}

void BodyAggregateLiteral::collect(VarTermBoundVec &vars) const {
    complete_.domRepr()->collect(vars, naf_ == NAF::POS);
}

UIdx BodyAggregateLiteral::index(Context &context, BinderType type, Term::VarSet &bound) {
    static_cast<void>(context);
    return make_binder(complete_.dom(), naf_, *complete_.domRepr(), offset_, type, isRecursive(), bound, 0);
}

Literal::Score BodyAggregateLiteral::score(Term::VarSet const &bound, Logger &log) {
    static_cast<void>(log);
    return naf_ == NAF::POS ? estimate(complete_.dom().size(), *complete_.domRepr(), bound) : 0;
}

std::pair<Output::LiteralId, bool> BodyAggregateLiteral::toOutput(Logger &log) {
    static_cast<void>(log);
    if (offset_ == std::numeric_limits<PredicateDomain::SizeType>::max()) {
        assert(naf_ == NAF::NOT);
        return {Output::LiteralId(), true};
    }
    auto &dom = complete_.dom();
    auto &atm = dom[offset_];
    switch (naf_) {
        case NAF::POS:
        case NAF::NOTNOT: {
            return atm.fact()
                ? std::make_pair(Output::LiteralId(), true)
                : std::make_pair(Output::LiteralId{naf_, Output::AtomType::BodyAggregate, offset_, dom.domainOffset()}, false);
        }
        case NAF::NOT: {
            return !atm.recursive() && !atm.satisfiable()
                ? std::make_pair(Output::LiteralId(), true)
                : std::make_pair(Output::LiteralId{naf_, Output::AtomType::BodyAggregate, offset_, dom.domainOffset()}, false);
        }
    }
    assert(false);
    return {Output::LiteralId(),true};
}

// }}}1

// {{{1 definition of AssignmentAggregateComplete

AssignmentAggregateComplete::AssignmentAggregateComplete(DomainData &data, UTerm repr, UTerm dataRepr, AggregateFunction fun)
: def_(std::move(repr), &data.add<AssignmentAggregateDomain>())
, dataRepr_(std::move(dataRepr))
, fun_(fun)
, inst_(*this) {
}

bool AssignmentAggregateComplete::isNormal() const {
  return true;
}

void AssignmentAggregateComplete::analyze(Dep::Node &node, Dep &dep) {
    dep.depends(node, *this);
    def_.analyze(node, dep);
}

void AssignmentAggregateComplete::startLinearize(bool active) {
    def_.setActive(active);
    if (active) {
        inst_ = Instantiator(*this);
    }
}

void AssignmentAggregateComplete::linearize(Context &context, bool positive, Logger &log) {
    static_cast<void>(context);
    static_cast<void>(positive);
    static_cast<void>(log);
    auto binder  = gringo_make_unique<BindOnce>();
    for (HeadOccurrence &x : defBy_) {
        x.defines(*binder->getUpdater(), &inst_);
    }
    inst_.add(std::move(binder), Instantiator::DependVec{});
    inst_.finalize(Instantiator::DependVec{});
}

void AssignmentAggregateComplete::enqueue(Queue &q) {
    def_.init();
    q.enqueue(inst_);
}

void AssignmentAggregateComplete::printHead(std::ostream &out) const {
    out << *def_.domRepr();
}

void AssignmentAggregateComplete::propagate(Queue &queue) {
    def_.enqueue(queue);
}

void AssignmentAggregateComplete::report(Output::OutputBase &out, Logger &log) {
    static_cast<void>(log);
    for (auto &dataOffset : todo_) {
        auto &dom = this->dom();
        auto it = dom.data(dataOffset);
        auto &data = it.value();
        auto val = it.key();
        auto values = data.values();

        SymVec &atmArgs = out.tempVals();
        if (val.type() == SymbolType::Fun) {
            atmArgs.assign(begin(val.args()), end(val.args()));
        }
        atmArgs.emplace_back();
        for (auto const &y : values) {
            atmArgs.back() = y;
            auto ret = dom.define(Symbol::createFun(val.name(), Potassco::toSpan(atmArgs)));
            if (values.size() == 1) {
                ret.first->setFact(true);
            }
            std::get<0>(ret)->setData(dataOffset);
            std::get<0>(ret)->setRecursive(outputRecursive_);
        }
        data.setEnqueued(false);
    }
    todo_.clear();
}

void AssignmentAggregateComplete::print(std::ostream &out) const {
    printHead(out);
    out << ":-";
    print_comma(out, accuDoms_, ";", [this](std::ostream &out, AssignmentAggregateAccumulate const &x) -> void { x.printHead(out); out << occType_; });
    out << ".";
}

UGTerm AssignmentAggregateComplete::getRepr() const {
    return dataRepr_->gterm();
}

bool AssignmentAggregateComplete::isPositive() const {
    return true;
}

bool AssignmentAggregateComplete::isNegative() const {
    return false;
}

void AssignmentAggregateComplete::setType(OccurrenceType x) {
    occType_ = x;
}

OccurrenceType AssignmentAggregateComplete::getType() const {
    return occType_;
}

AssignmentAggregateComplete::DefinedBy &AssignmentAggregateComplete::definedBy() {
    return defBy_;
}

void AssignmentAggregateComplete::checkDefined(LocSet &done, SigSet const &edb, UndefVec &undef) const {
    static_cast<void>(done);
    static_cast<void>(edb);
    static_cast<void>(undef);
}

void AssignmentAggregateComplete::enqueue(Id_t dataId) {
    auto &data = dom().data(dataId).value();
    if (!data.enqueued()) {
        data.setEnqueued(true);
        todo_.emplace_back(dataId);
    }
}

// {{{1 Definition of AssignmentAggregateAccumulate

AssignmentAggregateAccumulate::AssignmentAggregateAccumulate(AssignmentAggregateComplete &complete, UTermVec tuple, ULitVec lits)
: AbstractStatement(get_clone(complete.dataRepr()), nullptr, std::move(lits))
, complete_(complete)
, tuple_(std::move(tuple)) { }

void AssignmentAggregateAccumulate::linearize(Context &context, bool positive, Logger &log) {
    AbstractStatement::linearize(context, positive, log);
    if (isOutputRecursive()) {
        complete_.setOutputRecursive();
    }
}

void AssignmentAggregateAccumulate::collectImportant(Term::VarSet &vars) {
    VarTermBoundVec bound;
    def_.domRepr()->collect(bound, false);
    for (auto &x : tuple_) {
        x->collect(bound, false);
    }
    for (auto &x : bound) {
        vars.emplace(x.first->name);
    }
}

void AssignmentAggregateAccumulate::report(Output::OutputBase &out, Logger &log) {
    auto &tempVals = out.tempVals();
    bool undefined = false;
    for (auto &x : tuple_) {
        tempVals.emplace_back(x->eval(undefined, log));
    }
    Symbol dataRepr(complete_.dataRepr()->eval(undefined, log));
    if (undefined) {
        return;
    }
    auto &tempLits = out.tempLits();
    for (auto &x : lits_) {
        if (x->auxiliary()) {
            continue;
        }
        auto lit = x->toOutput(log);
        if (!lit.second) {
            tempLits.emplace_back(lit.first);
        }
    }
    auto &dom = complete_.dom();
    auto dataId = dom.data(dataRepr, complete_.fun());
    auto &data = dom.data(dataId).value();
    data.accumulate(out.data, tuple_.empty() ? def_.domRepr()->loc() : tuple_.front()->loc(), tempVals, tempLits, log);
    complete_.enqueue(dataId);
}

void AssignmentAggregateAccumulate::printHead(std::ostream &out) const {
    out << "#accu(" << *complete_.domRepr() << ",tuple(" << tuple_ << "))";
}

// {{{1 Definition of AssignmentAggregateLiteral

AssignmentAggregateLiteral::AssignmentAggregateLiteral(AssignmentAggregateComplete &complete, bool auxiliary)
: complete_(complete)
, auxiliary_(auxiliary) { }

UGTerm AssignmentAggregateLiteral::getRepr() const {
    return complete_.domRepr()->gterm();
}

bool AssignmentAggregateLiteral::isPositive() const {
    return false;
}

bool AssignmentAggregateLiteral::isNegative() const {
    return false;
}

void AssignmentAggregateLiteral::setType(OccurrenceType x) {
    type_ = x;
}

OccurrenceType AssignmentAggregateLiteral::getType() const {
    return type_;
}

BodyOcc::DefinedBy &AssignmentAggregateLiteral::definedBy() {
    return defs_;
}

void AssignmentAggregateLiteral::checkDefined(LocSet &done, SigSet const &edb, UndefVec &undef) const {
    static_cast<void>(done);
    static_cast<void>(edb);
    static_cast<void>(undef);
}

void AssignmentAggregateLiteral::print(std::ostream &out) const {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
    out << static_cast<FunctionTerm&>(*complete_.domRepr()).arguments().back() << "=" << complete_.fun() << "{" << *complete_.domRepr() << "}" << type_;
}

bool AssignmentAggregateLiteral::isRecursive() const {
    return type_ == OccurrenceType::UNSTRATIFIED;
}

BodyOcc *AssignmentAggregateLiteral::occurrence() {
    return this;
}

void AssignmentAggregateLiteral::collect(VarTermBoundVec &vars) const {
    complete_.domRepr()->collect(vars, true);
}

UIdx AssignmentAggregateLiteral::index(Context &context, BinderType type, Term::VarSet &bound) {
    static_cast<void>(context);
    return make_binder(complete_.dom(), NAF::POS, *complete_.domRepr(), offset_, type, isRecursive(), bound, 0);
}

Literal::Score AssignmentAggregateLiteral::score(Term::VarSet const &bound, Logger &log) {
    static_cast<void>(log);
    return estimate(complete_.dom().size(), *complete_.domRepr(), bound);
}

std::pair<Output::LiteralId, bool> AssignmentAggregateLiteral::toOutput(Logger &log) {
    static_cast<void>(log);
    assert(offset_ != InvalidId);
    auto &atm = complete_.dom()[offset_];
    return atm.fact()
        ? std::make_pair(Output::LiteralId(), true)
        : std::make_pair(Output::LiteralId{NAF::POS, Output::AtomType::AssignmentAggregate, offset_, complete_.dom().domainOffset()}, false);
}

///}}}1

// {{{1 definition of ConjunctionLiteral

ConjunctionLiteral::ConjunctionLiteral(ConjunctionComplete &complete, bool auxiliary)
: complete_(complete)
, auxiliary_(auxiliary) { }

UGTerm ConjunctionLiteral::getRepr() const {
    return complete_.domRepr()->gterm();
}

bool ConjunctionLiteral::isPositive() const {
    return true;
}

bool ConjunctionLiteral::isNegative() const {
    return false;
}

void ConjunctionLiteral::setType(OccurrenceType x) {
    type_ = x;
}

OccurrenceType ConjunctionLiteral::getType() const {
    return type_;
}

BodyOcc::DefinedBy &ConjunctionLiteral::definedBy() {
    return defs_;
}

void ConjunctionLiteral::checkDefined(LocSet &done, SigSet const &edb, UndefVec &undef) const {
    static_cast<void>(done);
    static_cast<void>(edb);
    static_cast<void>(undef);
}

void ConjunctionLiteral::print(std::ostream &out) const {
    out << *complete_.domRepr() << type_;
}

bool ConjunctionLiteral::isRecursive() const {
    return type_ == OccurrenceType::UNSTRATIFIED;
}

BodyOcc *ConjunctionLiteral::occurrence() {
    return this;
}

void ConjunctionLiteral::collect(VarTermBoundVec &vars) const {
    complete_.domRepr()->collect(vars, true);
}

UIdx ConjunctionLiteral::index(Context &context, BinderType type, Term::VarSet &bound) {
    static_cast<void>(context);
    return make_binder(complete_.dom(), NAF::POS, *complete_.domRepr(), offset_, type, isRecursive(), bound, 0);
}

Literal::Score ConjunctionLiteral::score(Term::VarSet const &bound, Logger &log) {
    static_cast<void>(log);
    return estimate(complete_.dom().size(), *complete_.domRepr(), bound);
}

std::pair<Output::LiteralId, bool> ConjunctionLiteral::toOutput(Logger &log) {
    static_cast<void>(log);
    assert(offset_ != InvalidId);
    auto &atm = complete_.dom()[offset_];
    return atm.fact()
        ? std::make_pair(Output::LiteralId(), true)
        : std::make_pair(Output::LiteralId{NAF::POS, Output::AtomType::Conjunction, offset_, complete_.dom().domainOffset()}, false);
}

// {{{1 definition of ConjunctionAccumulateEmpty

ConjunctionAccumulateEmpty::ConjunctionAccumulateEmpty(ConjunctionComplete &complete, ULitVec lits)
: AbstractStatement(complete.emptyRepr(), &complete.emptyDom(), std::move(lits))
, complete_(complete) { }

bool ConjunctionAccumulateEmpty::isNormal() const {
    return true;
}

void ConjunctionAccumulateEmpty::report(Output::OutputBase &out, Logger &log) {
    static_cast<void>(log);
    complete_.reportEmpty(log);
    bool undefined = false;
    complete_.emptyDom().define(def_.domRepr()->eval(undefined, log), false);
    assert(!undefined);
}

// {{{1 definition of ConjunctionAccumulateCond

ConjunctionAccumulateCond::ConjunctionAccumulateCond(ConjunctionComplete &complete, ULitVec lits)
: AbstractStatement(complete.condRepr(), &complete.condDom(), std::move(lits))
, complete_(complete) {
    lits_.emplace_back(gringo_make_unique<PredicateLiteral>(true, complete_.emptyDom(), NAF::POS, complete.emptyRepr()));
}

void ConjunctionAccumulateCond::linearize(Context &context, bool positive, Logger &log) {
    AbstractStatement::linearize(context, positive, log);
    if (isOutputRecursive()) {
        complete_.setCondRecursive();
    }
}

void ConjunctionAccumulateCond::analyze(Dep::Node &node, Dep &dep) {
    def_.analyze(node, dep);
    for (auto &x : lits_) {
        auto *occ(x->occurrence());
        if (occ != nullptr) {
            dep.depends(node, *occ, true);
        }
    }
}

bool ConjunctionAccumulateCond::isNormal() const {
    return true;
}

void ConjunctionAccumulateCond::report(Output::OutputBase &out, Logger &log) {
    bool undefined = false;
    Symbol condRepr(def_.domRepr()->eval(undefined, log));
    assert(!undefined);

    Output::LitVec &cond = out.tempLits();
    for (auto &x : lits_) {
        if (x->auxiliary()) {
            continue;
        }
        auto y = x->toOutput(log);
        if (!y.second) {
            cond.emplace_back(y.first);
        }
    }
    complete_.condDom().define(condRepr, cond.empty());
    complete_.reportCond(out.data, condRepr.args()[2], cond, log);
}

// {{{1 definition of ConjunctionAccumulateHead

ConjunctionAccumulateHead::ConjunctionAccumulateHead(ConjunctionComplete &complete, ULitVec lits)
: AbstractStatement(complete.headRepr(), nullptr, std::move(lits))
, complete_(complete) {
    lits_.emplace_back(gringo_make_unique<PredicateLiteral>(true, complete_.condDom(), NAF::POS, complete.condRepr()));
}

void ConjunctionAccumulateHead::linearize(Context &context, bool positive, Logger &log) {
    AbstractStatement::linearize(context, positive, log);
    if (isOutputRecursive()) {
        complete_.setHeadRecursive();
    }
}

bool ConjunctionAccumulateHead::isNormal() const {
    return true;
}

void ConjunctionAccumulateHead::report(Output::OutputBase &out, Logger &log) {
    bool undefined = false;
    Symbol condRepr(def_.domRepr()->eval(undefined, log));
    assert(!undefined);

    Output::LitVec head;
    for (auto &x : lits_) {
        if (x->auxiliary()) {
            continue;
        }
        auto y = x->toOutput(log);
        if (!y.second) {
            head.emplace_back(y.first);
        }
    }

    complete_.reportHead(out.data, condRepr.args()[2], head, log);
}

// {{{1 definition of ConjunctionComplete

ConjunctionComplete::ConjunctionComplete(DomainData &data, UTerm repr, UTermVec local)
: def_(std::move(repr), &data.add<ConjunctionDomain>())
, domEmpty_(def_.domRepr()->getSig()) // Note: any sig will do
, domCond_(def_.domRepr()->getSig()) // Note: any sig will do
, inst_(*this)
, local_(std::move(local)) { }

UTerm ConjunctionComplete::emptyRepr() const {
    UTermVec args;
    args.emplace_back(make_locatable<ValTerm>(def_.domRepr()->loc(), Symbol::createId("empty")));
    args.emplace_back(get_clone(def_.domRepr()));
    args.emplace_back(make_locatable<FunctionTerm>(def_.domRepr()->loc(), "", UTermVec()));
    return make_locatable<FunctionTerm>(def_.domRepr()->loc(), "#accu", std::move(args));
}

UTerm ConjunctionComplete::condRepr() const {
    UTermVec args;
    args.emplace_back(make_locatable<ValTerm>(def_.domRepr()->loc(), Symbol::createId("cond")));
    args.emplace_back(get_clone(def_.domRepr()));
    args.emplace_back(make_locatable<FunctionTerm>(def_.domRepr()->loc(), "", get_clone(local_)));
    return make_locatable<FunctionTerm>(def_.domRepr()->loc(), "#accu", std::move(args));
}

UTerm ConjunctionComplete::headRepr() const {
    UTermVec args;
    args.emplace_back(make_locatable<ValTerm>(def_.domRepr()->loc(), Symbol::createId("head")));
    args.emplace_back(get_clone(def_.domRepr()));
    args.emplace_back(make_locatable<FunctionTerm>(def_.domRepr()->loc(), "", get_clone(local_)));
    return make_locatable<FunctionTerm>(def_.domRepr()->loc(), "#accu", std::move(args));
}

UTerm ConjunctionComplete::accuRepr() const {
    UTermVec args;
    args.emplace_back(make_locatable<VarTerm>(def_.domRepr()->loc(), "#Any1", std::make_shared<Symbol>(Symbol::createNum(0))));
    args.emplace_back(get_clone(def_.domRepr()));
    args.emplace_back(make_locatable<VarTerm>(def_.domRepr()->loc(), "#Any2", std::make_shared<Symbol>(Symbol::createNum(0))));
    return make_locatable<FunctionTerm>(def_.domRepr()->loc(), "#accu", std::move(args));
}

bool ConjunctionComplete::isNormal() const {
    return true;
}

void ConjunctionComplete::analyze(Dep::Node &node, Dep &dep) {
    dep.depends(node, *this);
    def_.analyze(node, dep);
}

void ConjunctionComplete::startLinearize(bool active) {
    def_.setActive(active);
    if (active) {
        inst_ = Instantiator(*this);
    }
}

void ConjunctionComplete::linearize(Context &context, bool positive, Logger &log) {
    static_cast<void>(context);
    static_cast<void>(positive);
    static_cast<void>(log);
    auto binder  = gringo_make_unique<BindOnce>();
    for (HeadOccurrence &x : defBy_) {
        x.defines(*binder->getUpdater(), &inst_);
    }
    inst_.add(std::move(binder), Instantiator::DependVec{});
    inst_.finalize(Instantiator::DependVec{});
}

void ConjunctionComplete::enqueue(Queue &q) {
    def_.init();
    q.enqueue(inst_);
}

void ConjunctionComplete::printHead(std::ostream &out) const{
    out << *def_.domRepr();
}

void ConjunctionComplete::propagate(Queue &queue) {
    def_.enqueue(queue);
}

template <class F>
void ConjunctionComplete::reportOther(F f, Logger &log) {
    bool undefined = false;
    auto atom = dom().reserve(domRepr()->eval(undefined, log));
    assert(!undefined);
    f(atom);
    if (!atom->blocked() && !atom->defined() && !atom->enqueued()) {
        atom->setEnqueued(true);
        todo_.emplace_back(numeric_cast<TodoVec::value_type>(atom - dom().begin()));
    }
}

void ConjunctionComplete::reportEmpty(Logger &log) {
    reportOther([](ConjunctionDomain::Iterator) { }, log);
}

void ConjunctionComplete::reportCond(DomainData &data, Symbol cond, Output::LitVec &lits, Logger &log) {
    reportOther([&](ConjunctionDomain::Iterator atom) { atom->accumulateCond(data, cond, lits); }, log);
}

void ConjunctionComplete::reportHead(DomainData &data, Symbol cond, Output::LitVec &lits, Logger &log) {
    reportOther([&](ConjunctionDomain::Iterator atom) { atom->accumulateHead(data, cond, lits); }, log);
}

void ConjunctionComplete::report(Output::OutputBase &out, Logger &log) {
    static_cast<void>(out);
    static_cast<void>(log);
    // NOTE: this code relies on priority set to 1!
    // NOTE about handling incompleteness:
    //   - if both the condition and the head are stratified
    //     then the conjunction is complete
    //   - if the condition is stratified but the head is recursive,
    //     then the conjunction is complete if all heads are fact
    for (auto &offset : todo_) {
        auto &atom = dom()[offset];
        if (!atom.blocked()) {
            dom().define(offset);
            atom.init(headRecursive_, condRecursive_);
        }
        atom.setEnqueued(false);
    }
    todo_.clear();
}

void ConjunctionComplete::print(std::ostream &out) const{
    printHead(out);
    out << ":-" << *accuRepr();
}

UGTerm ConjunctionComplete::getRepr() const {
    return accuRepr()->gterm();
}

bool ConjunctionComplete::isPositive() const {
    return true;
}

bool ConjunctionComplete::isNegative() const {
    return false;
}

void ConjunctionComplete::setType(OccurrenceType x) {
    occType_ = x;
}

OccurrenceType ConjunctionComplete::getType() const {
    return occType_;
}

ConjunctionComplete::DefinedBy &ConjunctionComplete::definedBy() {
    return defBy_;
}

void ConjunctionComplete::checkDefined(LocSet &done, SigSet const &edb, UndefVec &undef) const {
    static_cast<void>(done);
    static_cast<void>(edb);
    static_cast<void>(undef);
}

// }}}1

// {{{1 definition of TheoryComplete

TheoryComplete::TheoryComplete(DomainData &data, UTerm repr, TheoryAtomType type, UTerm name)
: def_(std::move(repr), &data.add<TheoryDomain>())
, accuRepr_(completeRepr_(def_.domRepr()))
, op_("")
, name_(std::move(name))
, inst_(*this)
, type_(type) { }

TheoryComplete::TheoryComplete(DomainData &data, UTerm repr, TheoryAtomType type, UTerm name, String op, Output::UTheoryTerm guard)
: def_(std::move(repr), &data.add<TheoryDomain>())
, accuRepr_(completeRepr_(def_.domRepr()))
, op_(op)
, guard_(std::move(guard))
, name_(std::move(name))
, inst_(*this)
, type_(type) { }

bool TheoryComplete::isNormal() const {
    return false;
}

void TheoryComplete::analyze(Dep::Node &node, Dep &dep) {
    dep.depends(node, *this);
    def_.analyze(node, dep);
}

void TheoryComplete::startLinearize(bool active) {
    def_.setActive(active);
    if (active) {
        inst_ = Instantiator(*this);
    }
}

void TheoryComplete::linearize(Context &context, bool positive, Logger &log) {
    static_cast<void>(context);
    static_cast<void>(positive);
    static_cast<void>(log);
    auto binder  = gringo_make_unique<BindOnce>();
    for (HeadOccurrence &x : defBy_) {
        x.defines(*binder->getUpdater(), &inst_);
    }
    inst_.add(std::move(binder), Instantiator::DependVec{});
    inst_.finalize(Instantiator::DependVec{});
}

void TheoryComplete::enqueue(Queue &q) {
    def_.init();
    q.enqueue(inst_);
}

void TheoryComplete::enqueue(TheoryDomain::Iterator atom) {
    if (!atom->enqueued() && !atom->defined()) {
        todo_.emplace_back(numeric_cast<TodoVec::value_type>(atom - dom().begin()));
        atom->setEnqueued(true);
    }
}

void TheoryComplete::printHead(std::ostream &out) const {
    out << *def_.domRepr();
    if (guard_) {
        out << op_;
        guard_->print(out);
    }
}

void TheoryComplete::propagate(Queue &queue) {
    def_.enqueue(queue);
}

void TheoryComplete::report(Output::OutputBase &out, Logger &log) {
    static_cast<void>(out);
    static_cast<void>(log);
    for (auto &x : todo_) {
        auto &atm = dom()[x];
        dom().define(x);
        atm.setEnqueued(false);
        atm.setRecursive(outputRecursive_);
    }
    todo_.clear();
}

void TheoryComplete::print(std::ostream &out) const {
    printHead(out);
    out << ":-";
    print_comma(out, accuDoms_, ",", [](std::ostream &out, TheoryAccumulate const &x) { x.printHead(out); });
    out << ".";
}

UGTerm TheoryComplete::getRepr() const {
    return accuRepr_->gterm();
}

bool TheoryComplete::isPositive() const {
    return true;
}

bool TheoryComplete::isNegative() const {
    return false;
}

void TheoryComplete::setType(OccurrenceType x) {
    occType_ = x;
}

OccurrenceType TheoryComplete::getType() const {
    return occType_;
}

TheoryComplete::DefinedBy &TheoryComplete::definedBy() {
    return defBy_;
}

void TheoryComplete::checkDefined(LocSet &done, SigSet const &edb, UndefVec &undef) const {
    static_cast<void>(done);
    static_cast<void>(edb);
    static_cast<void>(undef);
}

// {{{1 definition of TheoryAccumulate

TheoryAccumulate::TheoryAccumulate(TheoryComplete &complete, ULitVec lits)
: AbstractStatement(get_clone(complete.accuRepr()), nullptr, std::move(lits))
, complete_(complete)
, neutral_(true) { }

TheoryAccumulate::TheoryAccumulate(TheoryComplete &complete, Output::UTheoryTermVec tuple, ULitVec lits)
: AbstractStatement(get_clone(complete.accuRepr()), nullptr, std::move(lits))
, complete_(complete)
, tuple_(std::move(tuple))
, neutral_(false) { }

void TheoryAccumulate::linearize(Context &context, bool positive, Logger &log) {
    AbstractStatement::linearize(context, positive, log);
    if (isOutputRecursive()) {
        complete_.setOutputRecursive();
    }
}

void TheoryAccumulate::collectImportant(Term::VarSet &vars) {
    VarTermBoundVec bound;
    def_.domRepr()->collect(bound, false);
    for (auto &x : tuple_) {
        x->collect(bound);
    }
    for (auto &x : bound) {
        vars.emplace(x.first->name);
    }
}

void TheoryAccumulate::report(Output::OutputBase &out, Logger &log) {
    bool undefined = false;
    Symbol repr(complete_.domRepr()->eval(undefined, log));
    Symbol name(complete_.name()->eval(undefined, log));
    if (!undefined) {
        auto atom = complete_.dom().reserve(repr);
        if (!atom->initialized()) {
            Id_t n = out.data.theory().addTerm(name);
            Id_t g = complete_.hasGuard() ? complete_.guard()->eval(out.data.theory(), log) : InvalidId;
            Id_t o = complete_.hasGuard() ? out.data.theory().addTerm(complete_.op().c_str()) : InvalidId;
            atom->init(complete_.type(), n, o, g);
        }
        if (!neutral_) {
            std::vector<Potassco::Id_t> outTuple;
            for (auto &x : tuple_) {
                outTuple.emplace_back(x->eval(out.data.theory(), log));
            }
            Output::LitVec outLits;
            for (auto &x : lits_) {
                if (x->auxiliary()) {
                    continue;
                }
                auto lit = x->toOutput(log);
                if (!lit.second) {
                    outLits.emplace_back(lit.first);
                }
            }
            auto elemId = out.data.theory().addElem(Potassco::toSpan(outTuple), std::move(outLits));
            atom->accumulate(elemId);
        }
        complete_.enqueue(atom);
    }
}

void TheoryAccumulate::printHead(std::ostream &out) const {
    out << "#accu(" << *complete_.domRepr() << ",";
    if (!tuple_.empty()) {
        out << "tuple(" << tuple_ << ")";
    }
    else {
        out << "#neutral";
    }
    out << ")";
}

// {{{1 definition of TheoryLiteral

TheoryLiteral::TheoryLiteral(TheoryComplete &complete, NAF naf, bool auxiliary)
: complete_(complete)
, naf_(naf)
, auxiliary_(auxiliary) { }

UGTerm TheoryLiteral::getRepr() const {
    return complete_.domRepr()->gterm();
}

bool TheoryLiteral::isPositive() const {
    return false;
}

bool TheoryLiteral::isNegative() const {
    return naf_ != NAF::POS;
}

void TheoryLiteral::setType(OccurrenceType x) {
    type_ = x;
}

OccurrenceType TheoryLiteral::getType() const {
    return type_;
}

BodyOcc::DefinedBy &TheoryLiteral::definedBy() {
    return defs_;
}

void TheoryLiteral::checkDefined(LocSet &done, SigSet const &edb, UndefVec &undef) const {
    static_cast<void>(done);
    static_cast<void>(edb);
    static_cast<void>(undef);
}

void TheoryLiteral::print(std::ostream &out) const {
    out << naf_ << "&";
    complete_.name()->print(out);
    out << " {" << *complete_.domRepr() << type_ << "}";
    if (complete_.hasGuard()) {
        out << complete_.op();
        complete_.guard()->print(out);
    }
}

bool TheoryLiteral::isRecursive() const {
    return type_ == OccurrenceType::UNSTRATIFIED;
}

BodyOcc *TheoryLiteral::occurrence() {
    return this;
}

void TheoryLiteral::collect(VarTermBoundVec &vars) const {
    complete_.domRepr()->collect(vars, naf_ == NAF::POS);
}

UIdx TheoryLiteral::index(Context &context, BinderType type, Term::VarSet &bound) {
    static_cast<void>(context);
    return make_binder(complete_.dom(), naf_, *complete_.domRepr(), offset_, type, isRecursive(), bound, 0);
}

Literal::Score TheoryLiteral::score(Term::VarSet const &bound, Logger &log) {
    static_cast<void>(log);
    return naf_ == NAF::POS ? estimate(complete_.dom().size(), *complete_.domRepr(), bound) : 0;
}

std::pair<Output::LiteralId, bool> TheoryLiteral::toOutput(Logger &log) {
    static_cast<void>(log);
    if (offset_ == std::numeric_limits<PredicateDomain::SizeType>::max()) {
        assert(naf_ == NAF::NOT);
        return {Output::LiteralId(), true};
    }
    auto &dom = complete_.dom();
    return std::make_pair(Output::LiteralId{naf_, Output::AtomType::Theory, offset_, dom.domainOffset()}, false);
}

// {{{1 definition of TheoryRule

TheoryRule::TheoryRule(TheoryLiteral &lit, ULitVec lits)
    : AbstractStatement(nullptr, nullptr, std::move(lits))
    , lit_(lit) { }

void TheoryRule::collectImportant(Term::VarSet &vars) {
    lit_.collectImportant(vars);
}

void TheoryRule::report(Output::OutputBase &out, Logger &log) {
    if (lit_.type() == TheoryAtomType::Directive) {
        Output::TheoryDirective td(lit_.toOutput(log).first);
        out.output(td);
    }
    else {
        Output::Rule rule;
        for (auto &x : lits_) {
            if (x->auxiliary()) {
                continue;
            }
            auto ret = x->toOutput(log);
            if (ret.first && (out.keepFacts || !ret.second)) {
                rule.addBody(ret.first);
            }
        }
        rule.addHead(lit_.toOutput(log).first);
        out.output(rule);
    }
}

void TheoryRule::printHead(std::ostream &out) const {
    lit_.print(out);
}

// }}}1

// Head Aggregates

// {{{1 definition of HeadAggregateRule

HeadAggregateRule::HeadAggregateRule(HeadAggregateComplete &complete, ULitVec lits)
: AbstractStatement(get_clone(complete.domRepr()), &complete.dom(), std::move(lits))
, complete_(complete) { }

void HeadAggregateRule::report(Output::OutputBase &out, Logger &log) {
    Output::Rule &rule(out.tempRule(false));
    for (auto &x : lits_) {
        if (x->auxiliary()) {
            continue;
        }
        auto ret = x->toOutput(log);
        if (ret.first.valid() && (out.keepFacts || !ret.second)) {
            rule.addBody(ret.first);
        }
    }

    auto &dom = complete_.dom();
    bool undefined = false;
    auto ret(dom.define(def_.domRepr()->eval(undefined, log)));
    if (!ret.first->initialized()) {
        ret.first->init(complete_.fun(), _initBounds(complete_.bounds(), log));
    }
    // Note: init bounds and all that stuff should be done here
    assert(!undefined);
    Id_t offset = numeric_cast<Id_t>(ret.first - dom.begin());
    rule.addHead(Output::LiteralId{NAF::POS, Output::AtomType::HeadAggregate, offset, dom.domainOffset()});
    out.output(rule);
}

void HeadAggregateRule::print(std::ostream &out) const {
    auto it = complete_.bounds().begin();
    auto ie = complete_.bounds().end();
    if (it != ie) {
        out << *it->bound;
        out << inv(it->rel);
        ++it;
    }
    out << complete_.fun() << "(" << *def_.domRepr() << ")";
    for (; it != ie; ++it) {
        out << it->rel;
        out << *it->bound;
    }
    if (!lits_.empty()) {
        out << ":-";
        auto g = [](std::ostream &out, ULit const &x) { if (x) { out << *x; } else { out << "#null?"; }  };
        print_comma(out, lits_, ",", g);
    }
    out << ".";
}

// {{{1 definition of HeadAggregateAccumulate

HeadAggregateAccumulate::HeadAggregateAccumulate(HeadAggregateComplete &complete, UTermVec tuple, PredicateDomain *predDom, UTerm predRepr, ULitVec lits)
: AbstractStatement(completeRepr_(complete.domRepr()), nullptr, std::move(lits))
, complete_(complete)
, predDef_(std::move(predRepr), predDom)
, tuple_(std::move(tuple)) { }

void HeadAggregateAccumulate::collectImportant(Term::VarSet &vars) {
    VarTermBoundVec bound;
    def_.domRepr()->collect(bound, false);
    predDef_.collectImportant(vars);
    for (auto &x : tuple_) {
        x->collect(bound, false);
    }
    for (auto &x : bound) {
        vars.emplace(x.first->name);
    }
}

void HeadAggregateAccumulate::report(Output::OutputBase &out, Logger &log) {
    auto &vals = out.tempVals();
    bool undefined = false;
    for (auto &x : tuple_) {
        vals.emplace_back(x->eval(undefined, log));
    }
    if (undefined) {
        return;
    }
    Symbol predVal(predDef_ ? predDef_.domRepr()->eval(undefined, log) : Symbol());
    if (undefined) {
        return;
    }
    auto &tempLits = out.tempLits();
    for (auto &x : lits_) {
        if (x->auxiliary()) {
            continue;
        }
        auto lit = x->toOutput(log);
        if (!lit.second) {
            tempLits.emplace_back(lit.first);
        }
    }
    Symbol headVal(complete_.domRepr()->eval(undefined, log));
    assert(!undefined);
    auto &dom = complete_.dom();
    auto atm = dom.find(headVal);
    assert(atm != dom.end());
    Output::LiteralId lit;
    if (predDef_) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
        auto &predDom = static_cast<PredicateDomain&>(predDef_.dom());
        auto predAtm = predDom.reserve(predVal);
        if (!predAtm->fact()) {
            lit = Output::LiteralId(NAF::POS, Output::AtomType::Predicate, numeric_cast<Id_t>(predAtm - predDom.begin()), predDom.domainOffset());
        }
    }
    atm->accumulate(out.data, tuple_.empty() ? def_.domRepr()->loc() : tuple_.front()->loc(), vals, lit, tempLits, log);
    complete_.enqueue(atm);
}

void HeadAggregateAccumulate::printHead(std::ostream &out) const {
    out << "#accu(" << *complete_.domRepr() << ",";
    if (predDef_) {
        out << *predDef_.domRepr() << ",tuple(" << tuple_ << ")";
    }
    else {
        out << "#true";
    }
    out << ")";
}

// {{{1 definition of HeadAggregateComplete

HeadAggregateComplete::HeadAggregateComplete(DomainData &data, UTerm repr, AggregateFunction fun, BoundVec bounds)
: repr_(std::move(repr))
, domain_(data.add<HeadAggregateDomain>())
, inst_(*this)
, fun_(fun)
, bounds_(std::move(bounds)) { }

void HeadAggregateComplete::enqueue(HeadAggregateDomain::Iterator atom) {
    if (!atom->enqueued()) {
        todo_.emplace_back(numeric_cast<TodoVec::value_type>(atom - dom().begin()));
        atom->setEnqueued(true);
    }
}

bool HeadAggregateComplete::isNormal() const {
    return false;
}

void HeadAggregateComplete::analyze(Dep::Node &node, Dep &dep) {
    for (HeadAggregateAccumulate &x : accuDoms_) {
        x.predDef().analyze(node, dep);
    }
    dep.depends(node, *this);
}

void HeadAggregateComplete::startLinearize(bool active) {
    for (HeadAggregateAccumulate &x : accuDoms_) {
        x.predDef().setActive(active);
    }
    if (active) {
        inst_ = Instantiator(*this);
    }
}

void HeadAggregateComplete::linearize(Context &context, bool positive, Logger &log) {
    static_cast<void>(context);
    static_cast<void>(positive);
    static_cast<void>(log);
    auto binder  = gringo_make_unique<BindOnce>();
    for (HeadOccurrence &x : defBy_) {
        x.defines(*binder->getUpdater(), &inst_);
    }
    inst_.add(std::move(binder), Instantiator::DependVec{});
    inst_.finalize(Instantiator::DependVec{});
}

void HeadAggregateComplete::enqueue(Queue &q) {
    for (HeadAggregateAccumulate &x : accuDoms_) {
        if (x.predDef()) {
            x.predDef().init();
        }
    }
    q.enqueue(inst_);
}

void HeadAggregateComplete::printHead(std::ostream &out) const {
    auto it = bounds_.begin();
    auto ie = bounds_.end();
    if (it != ie) {
        out << *it->bound;
        out << inv(it->rel);
        ++it;
    }
    out << fun_ << "{";
    print_comma(out, accuDoms_, ";", [](std::ostream &out, HeadAggregateAccumulate const &x) -> void {
        out << x.tuple();
        out << ":";
        if (x.predDef()) {
            out << x.predDef().domRepr();
        } else {
            out << "#true";
        }
        out << ":";
        x.printHead(out);
    });

    out << "}";
    for (; it != ie; ++it) {
        out << it->rel;
        out << *it->bound;
    }
}

void HeadAggregateComplete::propagate(Queue &queue) {
    for (HeadAggregateAccumulate &x : accuDoms_) {
        if (x.predDef()) {
            x.predDef().enqueue(queue);
        }
    }
}

void HeadAggregateComplete::print(std::ostream &out) const {
    printHead(out);
    out << ":-" << repr_ << occType_ << ".";
}

void HeadAggregateComplete::report(Output::OutputBase &out, Logger &log) {
    static_cast<void>(log);
    for (auto &x : todo_) {
        auto &atm = dom()[x];
        if (atm.satisfiable()) {
            // Note: this is not as lazy at is could be
            //       in the recursive case this would define atoms more that once ...
            for (auto const &elem : atm.elems()) {
                for (auto const &cond : elem.second) {
                    if (cond.first) {
                        out.data.predDom(cond.first.domain()).define(cond.first.offset());
                    }
                }
            }
        }
        atm.setEnqueued(false);
    }
    todo_.clear();
}

UGTerm HeadAggregateComplete::getRepr() const {
    return completeRepr_(repr_)->gterm();
}

bool HeadAggregateComplete::isPositive() const {
    return true;
}

bool HeadAggregateComplete::isNegative() const {
    return false;
}

void HeadAggregateComplete::setType(OccurrenceType x) {
    occType_ = x;
}

OccurrenceType HeadAggregateComplete::getType() const {
    return occType_;
}

HeadAggregateComplete::DefinedBy &HeadAggregateComplete::definedBy() {
    return defBy_;
}

HeadAggregateDomain &HeadAggregateComplete::dom() {
    return domain_;
}

void HeadAggregateComplete::checkDefined(LocSet &done, SigSet const &edb, UndefVec &undef) const {
    static_cast<void>(done);
    static_cast<void>(edb);
    static_cast<void>(undef);
}

// {{{1 definition of HeadAggregateLiteral

HeadAggregateLiteral::HeadAggregateLiteral(HeadAggregateComplete &complete)
: complete_(complete) { }

UGTerm HeadAggregateLiteral::getRepr() const {
    return complete_.domRepr()->gterm();
}

bool HeadAggregateLiteral::isPositive() const {
    return true;
}

bool HeadAggregateLiteral::isNegative() const {
    return false;
}

void HeadAggregateLiteral::setType(OccurrenceType x) {
    type_ = x;
}

OccurrenceType HeadAggregateLiteral::getType() const {
    return type_;
}

BodyOcc::DefinedBy &HeadAggregateLiteral::definedBy() {
    return defs_;
}

void HeadAggregateLiteral::checkDefined(LocSet &done, SigSet const &edb, UndefVec &undef) const {
    static_cast<void>(done);
    static_cast<void>(edb);
    static_cast<void>(undef);
}

void HeadAggregateLiteral::print(std::ostream &out) const {
    out << *complete_.domRepr() << type_;
}

bool HeadAggregateLiteral::isRecursive() const {
    return type_ == OccurrenceType::UNSTRATIFIED;
}

BodyOcc *HeadAggregateLiteral::occurrence() {
    return this;
}

void HeadAggregateLiteral::collect(VarTermBoundVec &vars) const {
    complete_.domRepr()->collect(vars, true);
}

UIdx HeadAggregateLiteral::index(Context &context, BinderType type, Term::VarSet &bound) {
    static_cast<void>(context);
    return make_binder(complete_.dom(), NAF::POS, *complete_.domRepr(), offset_, type, isRecursive(), bound, 0);
}

Literal::Score HeadAggregateLiteral::score(Term::VarSet const &bound, Logger &log) {
    static_cast<void>(log);
    return estimate(complete_.dom().size(), *complete_.domRepr(), bound);
}

std::pair<Output::LiteralId, bool> HeadAggregateLiteral::toOutput(Logger &log) {
    static_cast<void>(log);
    return {Output::LiteralId{}, true};
}

// }}}1

// {{{1 definition of DisjunctionRule

DisjunctionRule::DisjunctionRule(DisjunctionComplete &complete, ULitVec lits)
: AbstractStatement(get_clone(complete.domRepr()), &complete.dom(), std::move(lits))
, complete_(complete) {
}

bool DisjunctionRule::isNormal() const {
    return false;
}

void DisjunctionRule::report(Output::OutputBase &out, Logger &log) {
    Output::Rule &rule(out.tempRule(false));
    bool fact = true;
    for (auto &x : lits_) {
        if (x->auxiliary()) {
            continue;
        }
        auto ret = x->toOutput(log);
        if (ret.first.valid() && (out.keepFacts || !ret.second)) {
            rule.addBody(ret.first);
            if (!ret.second) {
                fact = false;
            }
        }
    }
    auto &dom = complete_.dom();
    bool undefined = false;
    auto ret(dom.define(def_.domRepr()->eval(undefined, log)));
    if (fact) {
        ret.first->setFact(true);
    }
    assert(!undefined);
    complete_.enqueue(ret.first);
    Id_t offset = numeric_cast<Id_t>(ret.first - dom.begin());
    rule.addHead(Output::LiteralId{NAF::POS, Output::AtomType::Disjunction, offset, dom.domainOffset()});
    out.output(rule);
}

// {{{1 definition of DisjunctionLiteral

DisjunctionLiteral::DisjunctionLiteral(DisjunctionComplete &complete)
: complete_(complete) { }

UGTerm DisjunctionLiteral::getRepr() const {
    return complete_.domRepr()->gterm();
}

bool DisjunctionLiteral::isPositive() const {
    return true;
}

bool DisjunctionLiteral::isNegative() const {
    return false;
}

void DisjunctionLiteral::setType(OccurrenceType x) {
    type_ = x;
}

OccurrenceType DisjunctionLiteral::getType() const {
    return type_;
}

BodyOcc::DefinedBy &DisjunctionLiteral::definedBy() {
    return defs_;
}

void DisjunctionLiteral::checkDefined(LocSet &done, SigSet const &edb, UndefVec &undef) const {
    static_cast<void>(done);
    static_cast<void>(edb);
    static_cast<void>(undef);
}

void DisjunctionLiteral::print(std::ostream &out) const {
    out << "[" << *complete_.domRepr() << type_ << "]";
}

bool DisjunctionLiteral::isRecursive() const {
    return type_ == OccurrenceType::UNSTRATIFIED;
}

BodyOcc *DisjunctionLiteral::occurrence() {
    return this;
}

void DisjunctionLiteral::collect(VarTermBoundVec &vars) const {
    complete_.domRepr()->collect(vars, true);
}

UIdx DisjunctionLiteral::index(Context &context, BinderType type, Term::VarSet &bound) {
    static_cast<void>(context);
    return make_binder(complete_.dom(), NAF::POS, *complete_.domRepr(), offset_, type, isRecursive(), bound, 0);
}

Literal::Score DisjunctionLiteral::score(Term::VarSet const &bound, Logger &log) {
    static_cast<void>(log);
    return estimate(complete_.dom().size(), *complete_.domRepr(), bound);
}

std::pair<Output::LiteralId, bool> DisjunctionLiteral::toOutput(Logger &log) {
    static_cast<void>(log);
    return {Output::LiteralId{}, true};
}

// {{{1 definition of DisjunctionComplete

DisjunctionComplete::DisjunctionComplete(DomainData &data, UTerm repr)
: repr_(std::move(repr))
, domain_(data.add<DisjunctionDomain>())
, inst_(*this) { }

bool DisjunctionComplete::isNormal() const {
    return true;
}

void DisjunctionComplete::analyze(Dep::Node &node, Dep &dep) {
    dep.depends(node, *this);
    for (DisjunctionAccumulate &accu : accu_) {
        accu.predDef().analyze(node, dep);
    }
}

void DisjunctionComplete::startLinearize(bool active) {
    for (DisjunctionAccumulate &accu : accu_) {
        accu.predDef().setActive(active);
    }
    if (active) {
        inst_ = Instantiator(*this);
    }
}

void DisjunctionComplete::linearize(Context &context, bool positive, Logger &log) {
    static_cast<void>(context);
    static_cast<void>(positive);
    static_cast<void>(log);
    auto binder  = gringo_make_unique<BindOnce>();
    for (HeadOccurrence &x : defBy_) {
        x.defines(*binder->getUpdater(), &inst_);
    }
    inst_.add(std::move(binder), Instantiator::DependVec{});
    inst_.finalize(Instantiator::DependVec{});
}

void DisjunctionComplete::enqueue(Queue &q) {
    for (DisjunctionAccumulate &x : accu_) {
        x.predDef().init();
    }
    q.enqueue(inst_);
}

void DisjunctionComplete::printHead(std::ostream &out) const {
    bool comma = false;
    for (DisjunctionAccumulate &accu : accu_) {
        if (comma) {
            out << ";";
        }
        comma = true;
        accu.printPred(out);
    }
}

void DisjunctionComplete::propagate(Queue &queue) {
    for (DisjunctionAccumulate &accu : accu_) {
        accu.predDef().enqueue(queue);
    }
}

void DisjunctionComplete::enqueue(DisjunctionDomain::Iterator atom) {
    if (!atom->enqueued()) {
        atom->setEnqueued(true);
        todo_.emplace_back(numeric_cast<TodoVec::value_type>(atom - dom().begin()));
    }
}

void DisjunctionComplete::report(Output::OutputBase &out, Logger &log) {
    static_cast<void>(log);
    for (auto &offset : todo_) {
        auto &atm = dom()[offset];
        atm.init(occType_ == OccurrenceType::UNSTRATIFIED);
        // Note: it is complete wrong to use the fact method of the atom interface like this
        //       this just works because the disjunction literal is auxiliary and never negative
        //       this method should be renamed!!!
        if (!atm.headFact()) {
            // Note: this is not as lazy at is could be
            //       in the recursive case this would define atoms more that once ...
            for (auto const &elem : atm.elems()) {
                for (auto const &clauseId : elem.second.heads()) {
                    for (auto const &lit : out.data.clause(clauseId)) {
                        // Note: for singleton disjunctions with empty bodies facts could be derived here
                        //       (it wouldn't even be that difficult)
                        if (lit.sign() == NAF::POS && lit.type() == Gringo::Output::AtomType::Predicate) {
                            out.data.predDom(lit.domain()).define(lit.offset());
                        }
                    }
                }
            }
        }
    }
}

void DisjunctionComplete::print(std::ostream &out) const{
    printHead(out);
    out << ":-" << *completeRepr_(repr_) << occType_;
}

UGTerm DisjunctionComplete::getRepr() const {
    return completeRepr_(repr_)->gterm();
}

bool DisjunctionComplete::isPositive() const {
    return true;
}

bool DisjunctionComplete::isNegative() const {
    return false;
}

void DisjunctionComplete::setType(OccurrenceType x) {
    occType_ = x;
}

OccurrenceType DisjunctionComplete::getType() const {
    return occType_;
}

DisjunctionComplete::DefinedBy &DisjunctionComplete::definedBy() {
    return defBy_;
}

void DisjunctionComplete::checkDefined(LocSet &done, SigSet const &edb, UndefVec &undef) const {
    static_cast<void>(done);
    static_cast<void>(edb);
    static_cast<void>(undef);
}

// {{{1 definition of DisjunctionAccumulate

void DisjunctionAccumulateHead::report(Output::OutputBase &out, Logger &log) {
    accu_.reportHead(out, log);
}
void DisjunctionAccumulateHead::printHead(std::ostream &out) const {
    if (accu_.predDef()) {
        out << *accu_.predDef().domRepr();
    }
    else {
        out << "#false";
    }
}

DisjunctionAccumulate::DisjunctionAccumulate(DisjunctionComplete &complete, PredicateDomain *predDom, UTerm predRepr, ULitVec headCond, UTerm elemRepr, ULitVec lits)
: AbstractStatement(completeRepr_(complete.domRepr()), nullptr, std::move(lits))
, complete_(complete)
, elemRepr_(std::move(elemRepr))
, predDef_(std::move(predRepr), predDom)
, headCond_(std::move(headCond))
, accuHead_(*this)
, instHead_(accuHead_) {
    complete_.addAccu(*this);
    lits_.emplace_back(gringo_make_unique<DisjunctionLiteral>(complete_));
}

void DisjunctionAccumulate::analyze(Dep::Node &node, Dep &dep) {
    AbstractStatement::analyze(node, dep);
    for (auto &x : headCond_) {
        auto *occ(x->occurrence());
        if (occ != nullptr) {
            dep.depends(node, *occ, false);
        }
    }
}

void DisjunctionAccumulate::linearize(Context &context, bool positive, Logger &log) {
    AbstractStatement::linearize(context, positive, log);
    Term::VarSet important;
    if (predDef_) {
        predDef_.collectImportant(important);
    }
    Term::VarSet boundInitially;
    // Note: this performs a nested instantiation
    //       to do this, the linearization is initialized with the variables bound by the outer instantiator
    //       by construction, there cannot be positive recursive literals in it
    elemRepr_->collect(boundInitially);
    complete_.domRepr()->collect(boundInitially);
    InstVec insts = _linearize(log, context, positive, accuHead_, std::move(important), headCond_, boundInitially);
    assert(insts.size() == 1);
    instHead_ = std::move(insts.front());
}

void DisjunctionAccumulate::collectImportant(Term::VarSet &vars) {
    predDef_.collectImportant(vars);
    AbstractStatement::collectImportant(vars);
    for (auto &x : headCond_) {
        x->collectImportant(vars);
    }
}

void DisjunctionAccumulate::printPred(std::ostream &out) const {
    if (predDef_) {
        out << *predDef_.domRepr();
    }
    else {
        out << "#false";
    }
    bool sep = false;
    for (auto const &x : headCond_) {
        out << (sep ? "," : ":");
        sep = true;
        x->print(out);
    }
}

void DisjunctionAccumulate::reportHead(Output::OutputBase &out, Logger &log) {
    bool undefined = false;
    Symbol predRepr;
    if (predDef_) {
        predRepr = predDef_.domRepr()->eval(undefined, log);
    }
    if (undefined) {
        return;
    }
    Symbol domRepr(complete_.domRepr()->eval(undefined, log));
    Symbol elemRepr(elemRepr_->eval(undefined, log));
    assert(!undefined);
    auto &dom = complete_.dom();
    auto atm = dom.find(domRepr);
    assert(atm != dom.end());
    auto &tempLits = out.tempLits();
    for (auto &x : headCond_) {
        if (x->auxiliary()) {
            continue;
        }
        auto lit = x->toOutput(log);
        if (!lit.second) {
            tempLits.emplace_back(lit.first.negate());
        }
    }
    if (predDef_) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
        auto &predDom = static_cast<PredicateDomain&>(predDef_.dom());
        auto predAtm = predDom.reserve(predRepr);
        if (predAtm->fact()) {
            return;
        }
        tempLits.emplace_back(Output::LiteralId(NAF::POS, Output::AtomType::Predicate, numeric_cast<Id_t>(predAtm - predDom.begin()), predDom.domainOffset()));
    }
    complete_.enqueue(atm);
    atm->accumulateHead(out.data, elemRepr, tempLits);
}

void DisjunctionAccumulate::report(Output::OutputBase &out, Logger &log) {
    bool undefined = false;
    Symbol domRepr(complete_.domRepr()->eval(undefined, log));
    Symbol elemRepr(elemRepr_->eval(undefined, log));
    assert(!undefined);
    auto &tempLits = out.tempLits();
    for (auto &x : lits_) {
        if (x->auxiliary()) {
            continue;
        }
        auto lit = x->toOutput(log);
        if (!lit.second) {
            tempLits.emplace_back(lit.first);
        }
    }
    auto &dom = complete_.dom();
    auto atm = dom.find(domRepr);
    assert(atm != dom.end());
    atm->accumulateCond(out.data, elemRepr, tempLits);
    instHead_.instantiate(out, log);
}

// }}}1

} } // namespace Ground Gringo
