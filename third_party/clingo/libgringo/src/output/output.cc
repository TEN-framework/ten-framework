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

#include "gringo/output/output.hh"
#include "gringo/logger.hh"
#include "gringo/output/aggregates.hh"
#include "gringo/output/backends.hh"
#include "reify/program.hh"
#include <cstring>
#include <stdexcept>

namespace Gringo { namespace Output {

// {{{1 internal functions and classes

namespace {

// {{{2 definition of DelayedStatement

class DelayedStatement : public Statement {
public:
    DelayedStatement(LiteralId lit)
    : lit_(lit) { }

    void output(DomainData &data, UBackend &out) const override {
        static_cast<void>(data);
        static_cast<void>(out);
    }

    void print(PrintPlain out, char const *prefix) const override {
        auto lit = call(out.domain, lit_, &Literal::delayedLit).first;
        out << prefix;
        call(out.domain, lit, &Literal::printPlain, out);
        out << " <=> ";
        call(out.domain, lit_, &Literal::printPlain, out);
        out << "\n";
    }

    void translate(DomainData &data, Translator &trans) override {
        trans.output(data, *this);
        call(data, lit_, &Literal::translate, trans);
    }

    void replaceDelayed(DomainData &data, LitVec &delayed) override {
        static_cast<void>(data);
        static_cast<void>(delayed);
    }

private:
    LiteralId lit_;
};

// {{{2 definition of TranslateStatement

template <class T>
class TranslateStatement : public Statement {
public:
    TranslateStatement(T const &lambda)
    : lambda_(lambda) { }

    void output(DomainData &data, UBackend &out) const override {
        static_cast<void>(data);
        static_cast<void>(out);
    }

    void print(PrintPlain out, char const *prefix) const override {
        static_cast<void>(out);
        static_cast<void>(prefix);
    }

    void translate(DomainData &data, Translator &trans) override {
        trans.output(data, *this);
        lambda_(data, trans);
    }

    void replaceDelayed(DomainData &data, LitVec &delayed) override {
        static_cast<void>(data);
        static_cast<void>(delayed);
    }

private:
    T const &lambda_;
};

template <class T>
void translateLambda(DomainData &data, AbstractOutput &out, T const &lambda) {
    TranslateStatement<T>(lambda).passTo(data, out);
}

// {{{2 definition of EndGroundStatement

class EndGroundStatement : public Statement {
public:
    EndGroundStatement(OutputPredicates const &outPreds, Logger &log)
    : outPreds_(outPreds)
    , log_(log) { }

    void output(DomainData &data, UBackend &out) const override {
        static_cast<void>(data);
        static_cast<void>(out);
    }

    void print(PrintPlain out, char const *prefix) const override {
        for (auto const &x : outPreds_) {
            if (x.second.match("", 0)) {
                out << prefix << "#show.\n";
            }
            else {
                out << prefix << "#show " << x.second << ".\n";
            }
        }
    }

    void translate(DomainData &data, Translator &trans) override {
        trans.translate(data, outPreds_, log_);
        trans.output(data, *this);
    }

    void replaceDelayed(DomainData &data, LitVec &delayed) override {
        static_cast<void>(data);
        static_cast<void>(delayed);
    }

private:
    OutputPredicates const &outPreds_;
    Logger &log_;
};

// {{{2 definition of BackendAdapter

template <class T>
class BackendAdapter : public Backend {
public:
    template <class... U>
    BackendAdapter(U&&... args)
    : prg_(std::forward<U>(args)...) {
    }

    void initProgram(bool incremental)  override {
        prg_.initProgram(incremental);
    }

    void beginStep()  override {
        prg_.beginStep();
    }

    void rule(Head_t ht, const AtomSpan& head, const LitSpan& body)  override {
        prg_.rule(ht, head, body);
    }

    void rule(Head_t ht, const AtomSpan& head, Weight_t bound, const WeightLitSpan& body)  override {
        prg_.rule(ht, head, bound, body);
    }

    void minimize(Weight_t prio, const WeightLitSpan& lits)  override {
        prg_.minimize(prio, lits);
    }

    void project(const AtomSpan& atoms) override {
        prg_.project(atoms);
    }

    void output(Symbol sym, Potassco::Atom_t atom) override {
        std::ostringstream out;
        out << sym;
        if (atom != 0) {
            auto lit = numeric_cast<Potassco::Lit_t>(atom);
            prg_.output(Potassco::toSpan(out.str().c_str()), Potassco::LitSpan{&lit, 1});
        }
        else {
            prg_.output(Potassco::toSpan(out.str().c_str()), Potassco::LitSpan{nullptr, 0});
        }
    }

    void output(Symbol sym, Potassco::LitSpan const& condition) override {
        std::ostringstream out;
        out << sym;
        prg_.output(Potassco::toSpan(out.str().c_str()), condition);
    }

    void external(Atom_t a, Value_t v)  override {
        prg_.external(a, v);
    }

    void assume(const LitSpan& lits)  override {
        prg_.assume(lits);
    }

    void heuristic(Atom_t a, Heuristic_t t, int bias, unsigned prio, const LitSpan& condition)  override {
        prg_.heuristic(a, t, bias, prio, condition);
    }

    void acycEdge(int s, int t, const LitSpan& condition)  override {
        prg_.acycEdge(s, t, condition);
    }

    void theoryTerm(Id_t termId, int number)  override {
        prg_.theoryTerm(termId, number);
    }

    void theoryTerm(Id_t termId, const StringSpan& name)  override {
        prg_.theoryTerm(termId, name);
    }

    void theoryTerm(Id_t termId, int cId, const IdSpan& args)  override {
        prg_.theoryTerm(termId, cId, args);
    }

    void theoryElement(Id_t elementId, const IdSpan& terms, const LitSpan& cond)  override {
        prg_.theoryElement(elementId, terms, cond);
    }

    void theoryAtom(Id_t atomOrZero, Id_t termId, const IdSpan& elements)  override {
        prg_.theoryAtom(atomOrZero, termId, elements);
    }

    void theoryAtom(Id_t atomOrZero, Id_t termId, const IdSpan& elements, Id_t op, Id_t rhs)  override {
        prg_.theoryAtom(atomOrZero, termId, elements, op, rhs);
    }

    void endStep() override {
        prg_.endStep();
    }
private:
    T prg_;
};

// {{{2 definition of BackendTheoryOutput

class BackendTheoryOutput : public TheoryOutput {
public:
    BackendTheoryOutput(DomainData &data, AbstractOutput &out)
    : data_{data}
    , out_{out} {
    }

    void theoryTerm(Id_t termId, int number) override {
        backendLambda(data_, out_, [&](DomainData &, UBackend &out) { out->theoryTerm(termId, number); });
    }

    void theoryTerm(Id_t termId, const StringSpan& name) override {
        backendLambda(data_, out_, [&](DomainData &, UBackend &out) { out->theoryTerm(termId, name); });
    }

    void theoryTerm(Id_t termId, int cId, const IdSpan& args) override {
        backendLambda(data_, out_, [&](DomainData &, UBackend &out) { out->theoryTerm(termId, cId, args); });
    }

    void theoryElement(Id_t elementId, IdSpan const & terms, LitVec const &cond) override {
        backendLambda(data_, out_, [&](DomainData &, UBackend &out) {
            BackendLitVec bc;
            bc.reserve(cond.size());
            for (auto const &lit : cond) {
                bc.emplace_back(call(data_, lit, &Literal::uid));
            }
            out->theoryElement(elementId, terms, Potassco::toSpan(bc));
        });
    }

    void theoryAtom(Id_t atomOrZero, Id_t termId, const IdSpan& elements) override {
        backendLambda(data_, out_, [&](DomainData &, UBackend &out) { out->theoryAtom(atomOrZero, termId, elements); });
    }

    void theoryAtom(Id_t atomOrZero, Id_t termId, const IdSpan& elements, Id_t op, Id_t rhs) override {
        backendLambda(data_, out_, [&](DomainData &, UBackend &out) { out->theoryAtom(atomOrZero, termId, elements, op, rhs); });
    }

private:
    DomainData &data_;
    AbstractOutput &out_;
};

// {{{2 definition of BackendTee

class BackendTee : public Backend {
public:
    using Head_t = Potassco::Head_t;
    using Value_t = Potassco::Value_t;
    using Heuristic_t = Potassco::Heuristic_t;
    using IdSpan = Potassco::IdSpan;
    using AtomSpan = Potassco::AtomSpan;
    using LitSpan = Potassco::LitSpan;
    using WeightLitSpan = Potassco::WeightLitSpan;

    BackendTee(UBackend a, UBackend b)
    : a_(std::move(a))
    , b_(std::move(b)) { }

    void initProgram(bool incremental) override {
        a_->initProgram(incremental);
        b_->initProgram(incremental);
    }
    void beginStep() override {
        a_->beginStep();
        b_->beginStep();
    }
    void endStep() override {
        a_->endStep();
        b_->endStep();
    }

    void rule(Head_t ht, const AtomSpan& head, const LitSpan& body) override {
        a_->rule(ht, head, body);
        b_->rule(ht, head, body);
    }
    void rule(Head_t ht, const AtomSpan& head, Weight_t bound, const WeightLitSpan& body) override {
        a_->rule(ht, head, bound, body);
        b_->rule(ht, head, bound, body);
    }
    void minimize(Weight_t prio, const WeightLitSpan& lits) override {
        a_->minimize(prio, lits);
        b_->minimize(prio, lits);
    }

    void project(const AtomSpan& atoms) override {
        a_->project(atoms);
        b_->project(atoms);
    }
    void output(Symbol sym, Potassco::Atom_t atom) override {
        a_->output(sym, atom);
        b_->output(sym, atom);
    }
    void output(Symbol sym, Potassco::LitSpan const& condition) override {
        a_->output(sym, condition);
        b_->output(sym, condition);
    }
    void external(Atom_t a, Value_t v) override {
        a_->external(a, v);
        b_->external(a, v);
    }
    void assume(const LitSpan& lits) override {
        a_->assume(lits);
        b_->assume(lits);
    }
    void heuristic(Atom_t a, Heuristic_t t, int bias, unsigned prio, const LitSpan& condition) override {
        a_->heuristic(a, t, bias, prio, condition);
        b_->heuristic(a, t, bias, prio, condition);
    }
    void acycEdge(int s, int t, const LitSpan& condition) override {
        a_->acycEdge(s, t, condition);
        b_->acycEdge(s, t, condition);
    }

    void theoryTerm(Id_t termId, int number) override {
        a_->theoryTerm(termId, number);
        b_->theoryTerm(termId, number);
    }
    void theoryTerm(Id_t termId, const StringSpan& name) override {
        a_->theoryTerm(termId, name);
        b_->theoryTerm(termId, name);
    }
    void theoryTerm(Id_t termId, int cId, const IdSpan& args) override {
        a_->theoryTerm(termId, cId, args);
        b_->theoryTerm(termId, cId, args);
    }
    void theoryElement(Id_t elementId, const IdSpan& terms, const LitSpan& cond) override {
        a_->theoryElement(elementId, terms, cond);
        b_->theoryElement(elementId, terms, cond);
    }
    void theoryAtom(Id_t atomOrZero, Id_t termId, const IdSpan& elements) override {
        a_->theoryAtom(atomOrZero, termId, elements);
        b_->theoryAtom(atomOrZero, termId, elements);
    }
    void theoryAtom(Id_t atomOrZero, Id_t termId, const IdSpan& elements, Id_t op, Id_t rhs) override {
        a_->theoryAtom(atomOrZero, termId, elements, op, rhs);
        b_->theoryAtom(atomOrZero, termId, elements, op, rhs);
    }

private:
    UBackend a_;
    UBackend b_;
};

// }}}2

} // namespace

// {{{1 definition of TranslatorOutput

TranslatorOutput::TranslatorOutput(UAbstractOutput out, bool preserveFacts)
: trans_(std::move(out), preserveFacts) { }

void TranslatorOutput::output(DomainData &data, Statement &stm) {
    stm.translate(data, trans_);
}

// {{{1 definition of TextOutput

TextOutput::TextOutput(std::string prefix, std::ostream &stream, UAbstractOutput out)
: prefix_(std::move(prefix))
, stream_(stream)
, out_(std::move(out)) { }

void TextOutput::TextOutput::output(DomainData &data, Statement &stm) {
    stm.print({data, stream_}, prefix_.c_str());
    if (out_) {
        out_->output(data, stm);
    }
}

// {{{1 definition of BackendOutput

BackendOutput::BackendOutput(UBackend out)
: out_(std::move(out)) { }

void BackendOutput::output(DomainData &data, Statement &stm) {
    stm.output(data, out_);
}

// {{{1 definition of OutputBase

OutputBase::OutputBase(Potassco::TheoryData &data, OutputPredicates outPreds, std::ostream &out, OutputFormat format, OutputOptions opts)
: outPreds(std::move(outPreds))
, data(data)
, out_(fromFormat(out, format, opts))
{ }

OutputBase::OutputBase(Potassco::TheoryData &data, OutputPredicates outPreds, UBackend out, OutputOptions opts)
: outPreds(std::move(outPreds))
, data(data)
, out_(fromBackend(std::move(out), opts))
{ }

OutputBase::OutputBase(Potassco::TheoryData &data, OutputPredicates outPreds, UAbstractOutput out)
: outPreds(std::move(outPreds))
, data(data)
, out_(std::move(out))
{ }

UAbstractOutput OutputBase::fromFormat(std::ostream &out, OutputFormat format, OutputOptions opts) {
    if (format == OutputFormat::TEXT) {
        UAbstractOutput output;
        output = gringo_make_unique<TextOutput>("", out);
        if (opts.debug == OutputDebug::TEXT) {
            output = gringo_make_unique<TextOutput>("% ", std::cerr, std::move(output));
        }
        return output;
    }
    UBackend backend;
    switch (format) {
        case OutputFormat::REIFY: {
            backend = gringo_make_unique<BackendAdapter<Reify::Reifier>>(out, opts.reifySCCs, opts.reifySteps);
            break;
        }
        case OutputFormat::INTERMEDIATE: {
            backend = gringo_make_unique<BackendAdapter<IntermediateFormatBackend>>(out);
            break;
        }
        case OutputFormat::SMODELS: {
            backend = gringo_make_unique<BackendAdapter<SmodelsFormatBackend>>(out);
            break;
        }
        case OutputFormat::TEXT: {
            throw std::logic_error("cannot happen");
        }
    }
    return fromBackend(std::move(backend), opts);
}

UAbstractOutput OutputBase::fromBackend(UBackend out, OutputOptions opts) {
    UAbstractOutput output;
    output = gringo_make_unique<BackendOutput>(std::move(out));
    if (opts.debug == OutputDebug::TRANSLATE || opts.debug == OutputDebug::ALL) {
        output = gringo_make_unique<TextOutput>("%% ", std::cerr, std::move(output));
    }
    output = gringo_make_unique<TranslatorOutput>(std::move(output), opts.preserveFacts);
    if (opts.debug == OutputDebug::TEXT || opts.debug == OutputDebug::ALL) {
        output = gringo_make_unique<TextOutput>("% ", std::cerr, std::move(output));
    }
    return output;
}

void OutputBase::init(bool incremental) {
    backendLambda(data, *out_, [incremental](DomainData &, UBackend &out) { out->initProgram(incremental); });
}

void OutputBase::output(Statement &x) {
    x.replaceDelayed(data, delayed_);
    out_->output(data, x);
}

void OutputBase::beginStep() {
    backendLambda(data, *out_, [](DomainData &, UBackend &out) { out->beginStep(); });
}

void OutputBase::endGround(Logger &log) {
    auto &doms = predDoms();
    for (auto const &neg : doms) {
        auto sig = neg->sig();
        if (!sig.sign()) {
            continue;
        }
        auto it_pos = doms.find(sig.flipSign());
        if (it_pos == doms.end()) {
            continue;
        }
        auto const &pos = *it_pos;
        auto rule = [&](auto it, auto jt) {
            output(tempRule(false)
                .addBody({NAF::POS, AtomType::Predicate, static_cast<Potassco::Id_t>(it - pos->begin()), pos->domainOffset()})
                .addBody({NAF::POS, AtomType::Predicate, static_cast<Potassco::Id_t>(jt - neg->begin()), neg->domainOffset()}));
        };
        auto neg_begin = neg->begin() + neg->incOffset();
        for (auto it = neg_begin, ie = neg->end(); it != ie; ++it) {
            auto jt = pos->find(static_cast<Symbol>(*it).flipSign());
            if (jt != pos->end() && jt->defined()) { rule(jt, it); }
        }
        for (auto it = pos->begin() + pos->incOffset(), ie = pos->end(); it != ie; ++it) {
            auto jt = neg->find(static_cast<Symbol>(*it).flipSign());
            if (jt != neg->end() && jt->defined() && jt < neg_begin) { rule(it, jt); }
        }
    }
    for (auto &lit : delayed_) {
        DelayedStatement(lit).passTo(data, *out_);
    }
    delayed_.clear();

    BackendTheoryOutput bto{data, *out_};
    data.theory().output(bto);

    EndGroundStatement(outPreds, log).passTo(data, *out_);
}

void OutputBase::endStep(Assumptions const &ass) {
    if (ass.size > 0) {
        auto *b = backend();
        if (b != nullptr) {
            b->assume(ass);
        }
    }
    backendLambda(data, *out_, [](DomainData &, UBackend &out) { out->endStep(); });
}

void OutputBase::reset(bool resetData) {
    data.reset(resetData);
    translateLambda(data, *out_, [](DomainData &, Translator &x) { x.reset(); });
}

void OutputBase::checkOutPreds(Logger &log) {
    for (auto const &x : outPreds) {
        if (!x.second.match("", 0)) {
            auto it(predDoms().find(std::get<1>(x)));
            if (it == predDoms().end()) {
                GRINGO_REPORT(log, Warnings::AtomUndefined)
                    << std::get<0>(x) << ": info: no atoms over signature occur in program:\n"
                    << "  " << std::get<1>(x) << "\n";
            }
        }
    }
}

SymVec OutputBase::atoms(unsigned atomset, IsTrueLookup lookup) const {
    SymVec atoms;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
    translateLambda(const_cast<DomainData&>(data), *out_, [&](DomainData &data, Translator &trans) {
        trans.atoms(data, atomset, lookup, atoms, outPreds);
    });
    return atoms;
}

std::pair<PredicateDomain::ConstIterator, PredicateDomain const *> OutputBase::find(Symbol val) const {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
    return const_cast<OutputBase*>(this)->find(val);
}

std::pair<PredicateDomain::Iterator, PredicateDomain*> OutputBase::find(Symbol val) {
    if (val.type() == SymbolType::Fun) {
        auto it = predDoms().find(val.sig());
        if (it != predDoms().end()) {
            auto jt = (*it)->find(val);
            if (jt != (*it)->end() && jt->defined()) {
                return {jt, it->get()};
            }
        }
    }
    return {PredicateDomain::Iterator(), nullptr};
}

std::pair<Id_t, Id_t> OutputBase::simplify(AssignmentLookup assignment) {
    Id_t facts = 0;
    Id_t deleted = 0;
    if (data.canSimplify()) {
        std::vector<Mapping> mappings;
        for (auto const &dom : data.predDoms()) {
            mappings.emplace_back();
            auto ret = dom->cleanup(assignment, mappings.back());
            facts+= ret.first;
            deleted+= ret.second;
        }
        translateLambda(data, *out_, [&](DomainData &data, Translator &trans) { trans.simplify(data, mappings, assignment); });
    }
    return {facts, deleted};
}

Backend *OutputBase::backend() {
    Backend *backend = nullptr;
    backendLambda(data, *out_, [&backend](DomainData &, UBackend &out) { backend = out.get(); });
    return backend;
}

namespace {

} // namespace

void OutputBase::registerObserver(UBackend prg, bool replace) {
    backendLambda(data, *out_, [&prg, replace](DomainData &, UBackend &out) {
        if (prg) {
            if (replace) {
                out = std::move(prg);
            }
            else {
                out = gringo_make_unique<BackendTee>(std::move(prg), std::move(out));
            }
        }
    });
}

// }}}1

void ASPIFOutBackend::initProgram(bool incremental) {
    static_cast<void>(incremental);
}

void ASPIFOutBackend::beginStep() {
    out_ = &beginOutput();
    bck_ = out_->backend();
    if (bck_ == nullptr) {
        throw std::runtime_error("backend not available");
    }
    if (steps_ > 0 || !out_->data.empty()) {
        // In hindsight, one could probably have designed this differently
        // by not exposing the backend at all. The aspif statements could
        // have been passed to the output by specialized statements as
        // well. The whole process would then look like the standard
        // grounding process.
        throw std::runtime_error("incremental aspif programs are not supported");
    }
    ++steps_;
}

void ASPIFOutBackend::rule(Head_t ht, AtomSpan const &head, LitSpan const &body) {
    update_(head, body);
    if (ht == Head_t::Disjunctive && body.size == 0 && head.size == 1) {
        facts_.emplace(*head.first);
    }
    bck_->rule(ht, head, body);
}
void ASPIFOutBackend::rule(Head_t ht, AtomSpan const &head, Weight_t bound, WeightLitSpan const &body) {
    update_(head, body);
    bck_->rule(ht, head, bound, body);
}
void ASPIFOutBackend::minimize(Weight_t prio, WeightLitSpan const &lits) {
    update_(lits);
    bck_->minimize(prio, lits);
}

void ASPIFOutBackend::project(AtomSpan const &atoms) {
    update_(atoms);
    bck_->project(atoms);
}
void ASPIFOutBackend::output(Symbol sym, Potassco::Atom_t atom) {
    update_(atom);
    auto res = sym_tab_.try_emplace(sym);
    res.first.value().emplace_back();
    res.first.value().back().emplace_back(atom);
}
void ASPIFOutBackend::output(Symbol sym, Potassco::LitSpan const &condition) {
    update_(condition);
    auto res = sym_tab_.try_emplace(sym);
    res.first.value().emplace_back(Potassco::begin(condition), Potassco::end(condition));
}
void ASPIFOutBackend::external(Atom_t a, Value_t v) {
    update_(a);
    bck_->external(a, v);
}
void ASPIFOutBackend::assume(const LitSpan& lits) {
    update_(lits);
    bck_->assume(lits);
}
void ASPIFOutBackend::heuristic(Atom_t a, Heuristic_t t, int bias, unsigned prio, LitSpan const &condition) {
    update_(a, condition);
    bck_->heuristic(a, t, bias, prio, condition);
}
void ASPIFOutBackend::acycEdge(int s, int t, LitSpan const &condition) {
    update_(condition);
    bck_->acycEdge(s, t, condition);
}

void ASPIFOutBackend::theoryTerm(Id_t termId, int number) {
    ensure_term(termId);
    theory_.addTerm(termId, number);
}
void ASPIFOutBackend::theoryTerm(Id_t termId, StringSpan const &name) {
    ensure_term(termId);
    theory_.addTerm(termId, name);
}
void ASPIFOutBackend::theoryTerm(Id_t termId, int cId, IdSpan const &args) {
    ensure_term(termId);
    theory_.addTerm(termId, cId, args);
}
void ASPIFOutBackend::theoryElement(Id_t elementId, IdSpan const &terms, LitSpan const &cond) {
    update_(cond);
    while (elements_.size() <= elementId) {
        elements_.emplace_back(std::numeric_limits<Id_t>::max(), std::vector<Potassco::Lit_t>{});
    }
    elements_[elementId].second.assign(Potassco::begin(cond), Potassco::end(cond));
    theory_.addElement(elementId, terms);
}
void ASPIFOutBackend::theoryAtom(Id_t atomOrZero, Id_t termId, IdSpan const &elements) {
    theory_.addAtom(atomOrZero, termId, elements);
}
void ASPIFOutBackend::theoryAtom(Id_t atomOrZero, Id_t termId, IdSpan const &elements, Id_t op, Id_t rhs) {
    update_(atomOrZero);
    theory_.addAtom(atomOrZero, termId, elements, op, rhs);
}

void ASPIFOutBackend::endStep() {
    // transfer stored theory atoms to the ouputs theory class
    theory_.accept(*this);
    for (auto it = sym_tab_.begin(), ie = sym_tab_.end(); it != ie; ++it) {
        auto const &sym = it.key();
        auto &formula = it.value();
        if (sym.hasSig() && !sym.name().empty()) {
            if (formula.size() == 1) {
                // show atom-like symbols directly associated with an atom
                auto &clause = formula.front();
                if (clause.size() == 1 && clause.front() > 0) {
                    auto atom = clause.front();
                    out_->addAtom(sym, atom, facts_.find(atom) != facts_.end());
                    continue;
                }
                // show atom-like symbols directly associated with facts
                if (clause.empty()) {
                    out_->addAtom(sym, fact_id(), true);
                    continue;
                }
            }
            // show atom-like symbols associated with formulas
            auto atom = out_->data.newAtom();
            for (auto &clause : formula) {
                rule(Potassco::Head_t::Disjunctive, {&atom, 1}, make_span(clause));
            }
            out_->addAtom(sym, atom, false);
            continue;
        }
        // show symbols associated with formulas
        // (could be made a function of the output to hide the mess)
        for (auto &clause : formula) {
            LitVec cond;
            cond.reserve(clause.size());
            for (auto &lit : clause) {
                Atom_t atom = std::abs(lit);
                LiteralId aux{lit > 0 ? NAF::POS : NAF::NOT, AtomType::Aux, atom, 0};
                cond.emplace_back(aux);
            }
            ShowStatement stm{sym, std::move(cond)};
            out_->output(stm);
        }
    }
    bck_ = nullptr;
    out_ = nullptr;
    endOutput();
}

void ASPIFOutBackend::update_(Atom_t const &atom) {
    out_->data.ensureAtom(atom);
}
void ASPIFOutBackend::update_(Lit_t const &lit) {
    out_->data.ensureAtom(std::abs(lit));
}
void ASPIFOutBackend::update_(AtomSpan const &atoms) {
    for (auto const &atom : atoms) {
        update_(atom);
    }
}
void ASPIFOutBackend::update_(LitSpan const &lits) {
    for (auto const &lit : lits) {
        update_(lit);
    }
}
void ASPIFOutBackend::update_(WeightLitSpan const &wlits) {
    for (auto const &wlit : wlits) {
        update_(wlit.lit);
    }
}
template <class T, class... Args>
void ASPIFOutBackend::update_(T const &x, Args const &... args) {
    update_(x);
    update_(args...);
}

void ASPIFOutBackend::visit(const Potassco::TheoryData &data, Id_t termId, const Potassco::TheoryTerm &t) {
    if (terms_[termId] == std::numeric_limits<Id_t>::max()) {
        theory_.accept(t, *this);
        auto &theory = out_->data.theory();
        switch (t.type()) {
            case Potassco::Theory_t::Number: {
                terms_[termId] = theory.addTerm(t.number());
                break;
            }
            case Potassco::Theory_t::Symbol: {
                terms_[termId] = theory.addTerm(t.symbol());
                break;
            }
            case Potassco::Theory_t::Compound: {
                std::vector<Id_t> terms;
                terms.reserve(t.terms().size);
                for (auto const &x : t.terms()) {
                    terms.emplace_back(terms_[x]);
                }
                if (t.isTuple()) {
                    terms_[termId] = theory.addTermTup(t.tuple(), make_span(terms));
                }
                else {
                    terms_[termId] = theory.addTermFun(terms_[t.function()], make_span(terms));
                }
                break;
            }
        }
    }
}
void ASPIFOutBackend::visit(const Potassco::TheoryData &data, Id_t elemId, const Potassco::TheoryElement &e) {
    if (elements_[elemId].first == std::numeric_limits<Id_t>::max()) {
        theory_.accept(e, *this);
        auto &theory = out_->data.theory();
        std::vector<Id_t> tuple;
        tuple.reserve(e.size());
        for (auto const &x : e) {
            tuple.emplace_back(terms_[x]);
        }
        // one could alse remap to named literals but would have to store the mapping
        elements_[elemId].first = theory.addElem(make_span(tuple), make_span(elements_[elemId].second));
    }
}
void ASPIFOutBackend::visit(const Potassco::TheoryData &data, const Potassco::TheoryAtom &a) {
    theory_.accept(a, *this);
    auto &theory = out_->data.theory();
    std::vector<Id_t> elements;
    elements.reserve(a.elements().size);
    for (auto const &x : a.elements()) {
        elements.emplace_back(elements_[x].first);
    }
    if (a.rhs() == nullptr) {
        theory.addAtom([&a]() { return a.atom(); }, terms_[a.term()], make_span(elements));
    }
    else {
        theory.addAtom([&a]() { return a.atom(); }, terms_[a.term()], make_span(elements), terms_[*a.guard()], terms_[*a.rhs()]);
    }
}

void ASPIFOutBackend::ensure_term(Id_t termId) {
    while (terms_.size() <= termId) {
        terms_.emplace_back(std::numeric_limits<Id_t>::max());
    }
}

Atom_t ASPIFOutBackend::fact_id() {
    if (fact_id_ == 0) {
        auto it = std::min_element(facts_.begin(), facts_.end());
        if (it == facts_.end()) {
            auto atom = out_->data.newAtom();
            rule(Potassco::Head_t::Disjunctive, {&atom, 1}, {nullptr, 0});
            it = facts_.begin();
        }
        fact_id_ = *it;
    }
    return fact_id_;
}

} } // namespace Output Gringo
