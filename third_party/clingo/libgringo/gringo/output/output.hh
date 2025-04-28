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

#ifndef GRINGO_OUTPUT_OUTPUT_HH
#define GRINGO_OUTPUT_OUTPUT_HH

#include "gringo/types.hh"
#include <cstdio>
#include <gringo/output/types.hh>
#include <gringo/output/statements.hh>
#include <gringo/output/theory.hh>

namespace Gringo { namespace Output {

class TranslatorOutput : public AbstractOutput {
public:
    TranslatorOutput(UAbstractOutput out, bool preserveFacts);
    void output(DomainData &data, Statement &stm) override;
private:
    Translator trans_;
};

class TextOutput : public AbstractOutput {
public:
    TextOutput(std::string prefix, std::ostream &stream, UAbstractOutput out = nullptr);
    void output(DomainData &data, Statement &stm) override;
private:
    std::string     prefix_;
    std::ostream   &stream_;
    UAbstractOutput out_;
};

class BackendOutput : public AbstractOutput {
public:
    BackendOutput(UBackend out);
    void output(DomainData &data, Statement &stm) override;
private:
    UBackend out_;
};

struct OutputOptions {
    OutputDebug debug = OutputDebug::NONE;
    bool reifySCCs  = false;
    bool reifySteps = false;
    bool preserveFacts = false;
};

class OutputPredicates {
public:
    struct SigCmp {
        using is_transparent = void;
        bool operator()(std::pair<Location, Sig> const &a, std::pair<Location, Sig> const &b) const{
            return a.second < b.second;
        }
        bool operator()(Sig const &a, std::pair<Location, Sig> const &b) const{
            return a < b.second;
        }
        bool operator()(std::pair<Location, Sig> const &a, Sig const &b) const{
            return a.second < b;
        }
    };
    using SigSet = std::set<std::pair<Location, Sig>, SigCmp>;
    using value_type =  SigSet::value_type;
    void add(Location loc, Sig sig, bool force) {
        if (!force) {
            active_ = true;
        }
        preds_.emplace(loc, sig);
    }
    bool active() const {
        return active_;
    }
    SigSet::const_iterator begin() const {
        return active_ ? preds_.begin() : preds_.end();
    }
    SigSet::const_iterator end() const {
        return preds_.end();
    }
    bool contains(Sig sig) const {
        return !active_ || preds_.find(sig) != preds_.end();
    }
private:
    SigSet preds_;
    bool active_ = false;
};

using Assumptions = Potassco::LitSpan;
class OutputBase {
public:
    OutputBase(Potassco::TheoryData &data, OutputPredicates outPreds, std::ostream &out, OutputFormat format = OutputFormat::INTERMEDIATE, OutputOptions opts = OutputOptions());
    OutputBase(Potassco::TheoryData &data, OutputPredicates outPreds, UBackend out, OutputOptions opts = OutputOptions());
    OutputBase(Potassco::TheoryData &data, OutputPredicates outPreds, UAbstractOutput out);

    std::pair<Id_t, Id_t> simplify(AssignmentLookup assignment);
    void incremental();
    void output(Statement &x);
    void init(bool incremental);
    void beginStep();
    void endGround(Logger &log);
    void endStep(Assumptions const &ass);
    void checkOutPreds(Logger &log);
    SymVec atoms(unsigned atomset, IsTrueLookup lookup) const;
    std::pair<PredicateDomain::Iterator, PredicateDomain*> find(Symbol val);
    std::pair<PredicateDomain::ConstIterator, PredicateDomain const *> find(Symbol val) const;
    PredDomMap &predDoms() { return data.predDoms(); }
    PredicateDomain &predDom(uint32_t idx) { return **data.predDoms().nth(idx); }
    PredDomMap const &predDoms() const { return data.predDoms(); }
    Rule &tempRule(bool choice) { return tempRule_.reset(choice); }
    SymVec &tempVals() { tempVals_.clear(); return tempVals_; }
    LitVec &tempLits() { tempLits_.clear(); return tempLits_; }
    Backend *backend();
    void registerObserver(UBackend prg, bool replace);
    void reset(bool resetData);
    bool addAtom(Symbol sym, Atom_t id, bool fact) {
        auto &atm = *data.add(sym.sig()).define(sym).first;
        if (fact) {
            atm.setFact(true);
        }
        if (!atm.hasUid()) {
            atm.setUid(id);
            return true;
        }
        return false;
    }
    Id_t addAtom(Symbol sym, bool *added = nullptr) {
        auto &atm = *data.add(sym.sig()).define(sym).first;
        if (!atm.hasUid()) {
            atm.setUid(data.newAtom());
            if (added != nullptr) {
                *added = true;
            }
        }
        else if (added != nullptr) {
            *added = false;
        }
        return atm.uid();
    }
private:
    static UAbstractOutput fromFormat(std::ostream &out, OutputFormat format, OutputOptions opts);
    static UAbstractOutput fromBackend(UBackend out, OutputOptions opts);

public:
    SymVec tempVals_;
    LitVec tempLits_;
    Rule tempRule_;
    LitVec delayed_;
    OutputPredicates outPreds;
    DomainData data;
    UAbstractOutput out_;
    bool keepFacts = false;
};

class ASPIFOutBackend : public Backend, private Potassco::TheoryData::Visitor {
public:
    virtual OutputBase &beginOutput() = 0;
    virtual void endOutput() = 0;

    void initProgram(bool incremental) override;
    void beginStep() override;

    void rule(Head_t ht, AtomSpan const &head, LitSpan const &body) override;
    void rule(Head_t ht, AtomSpan const &head, Weight_t bound, WeightLitSpan const &body) override;
    void minimize(Weight_t prio, WeightLitSpan const &lits) override;

    void project(AtomSpan const &atoms) override;
    void output(Symbol sym, Potassco::Atom_t atom) override;
    void output(Symbol sym, Potassco::LitSpan const &condition) override;
    void external(Atom_t a, Value_t v) override;
    void assume(const LitSpan& lits) override;
    void heuristic(Atom_t a, Heuristic_t t, int bias, unsigned prio, LitSpan const &condition) override;
    void acycEdge(int s, int t, LitSpan const &condition) override;

    void theoryTerm(Id_t termId, int number) override;
    void theoryTerm(Id_t termId, StringSpan const &name) override;
    void theoryTerm(Id_t termId, int cId, IdSpan const &args) override;
    void theoryElement(Id_t elementId, IdSpan const &terms, LitSpan const &cond) override;
    void theoryAtom(Id_t atomOrZero, Id_t termId, IdSpan const &elements) override;
    void theoryAtom(Id_t atomOrZero, Id_t termId, IdSpan const &elements, Id_t op, Id_t rhs) override;

    void endStep() override;
private:
    void update_(Atom_t const &atom);
    void update_(Lit_t const &lit);
    void update_(AtomSpan const &atoms);
    void update_(LitSpan const &lits);
    void update_(WeightLitSpan const &wlits);
    template <class T, class... Args>
    void update_(T const &x, Args const &... args);

    void visit(const Potassco::TheoryData &data, Id_t termId, const Potassco::TheoryTerm &t) override;
    void visit(const Potassco::TheoryData &data, Id_t elemId, const Potassco::TheoryElement &e) override;
    void visit(const Potassco::TheoryData &data, const Potassco::TheoryAtom &a) override;

    void ensure_term(Id_t termId);

    Atom_t fact_id();

    Potassco::TheoryData theory_;
    std::vector<std::pair<Id_t, std::vector<Potassco::Lit_t>>> elements_;
    std::vector<Id_t> terms_;
    ordered_map<Gringo::Symbol, std::vector<std::vector<Lit_t>>> sym_tab_;
    hash_set<Id_t> facts_;
    OutputBase *out_ = nullptr;
    Backend *bck_ = nullptr;
    size_t steps_ = 0;
    Atom_t fact_id_ = 0;
};

} } // namespace Output Gringo

#endif // GRINGO_OUTPUT_OUTPUT_HH
