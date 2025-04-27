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

#include "clingo/clingocontrol.hh"
#include <gringo/input/programbuilder.hh>
#include "clasp/clingo.h"
#include "clasp/solver.h"
#include <potassco/program_opts/typed_value.h>
#include <potassco/basic_types.h>
#include <clasp/clause.h>
#include <clasp/weight_constraint.h>
#include "clingo.h"
#include "gringo/backend.hh"
#include "gringo/hash_set.hh"
#include "gringo/output/literal.hh"
#include "gringo/output/literals.hh"
#include "gringo/output/output.hh"
#include "gringo/output/statements.hh"
#include "potassco/theory_data.h"
#include <csignal>
#include <clingo/incmode.hh>
#include <stdexcept>

namespace Gringo {

// {{{1 definition of ClaspAPIBackend

void ClaspAPIBackend::initProgram(bool incremental) {
    static_cast<void>(incremental);
}

void ClaspAPIBackend::endStep() { }

void ClaspAPIBackend::beginStep() { }

void ClaspAPIBackend::rule(Potassco::Head_t ht, const Potassco::AtomSpan& head, const Potassco::LitSpan& body) {
    if (auto *p = prg()) { p->addRule(ht, head, body); }
}

void ClaspAPIBackend::rule(Potassco::Head_t ht, const Potassco::AtomSpan& head, Potassco::Weight_t bound, const Potassco::WeightLitSpan& body) {
    if (auto *p = prg()) { p->addRule(ht, head, bound, body); }
}

void ClaspAPIBackend::minimize(Potassco::Weight_t prio, const Potassco::WeightLitSpan& lits) {
    if (auto *p = prg()) { p->addMinimize(prio, lits); }
}

void ClaspAPIBackend::project(const Potassco::AtomSpan& atoms) {
    if (auto *p = prg()) { p->addProject(atoms); }
}

void ClaspAPIBackend::output(Symbol sym, Potassco::Atom_t atom) {
    std::ostringstream out;
    out << sym;
    if (atom != 0) {
        Potassco::Lit_t lit = atom;
        if (auto *p = prg()) { p->addOutput(Potassco::toSpan(out.str().c_str()), Potassco::LitSpan{&lit, 1}); }
    }
    else {
        if (auto *p = prg()) { p->addOutput(Potassco::toSpan(out.str().c_str()), Potassco::LitSpan{nullptr, 0}); }
    }
}

void ClaspAPIBackend::output(Symbol sym, Potassco::LitSpan const& condition) {
    std::ostringstream out;
    out << sym;
    if (auto *p = prg()) { p->addOutput(Potassco::toSpan(out.str().c_str()), condition); }
}

void ClaspAPIBackend::acycEdge(int s, int t, const Potassco::LitSpan& condition) {
    if (auto *p = prg()) { p->addAcycEdge(s, t, condition); }
}

void ClaspAPIBackend::heuristic(Potassco::Atom_t a, Potassco::Heuristic_t t, int bias, unsigned prio, const Potassco::LitSpan& condition) {
    if (auto *p = prg()) { p->addDomHeuristic(a, t, bias, prio, condition); }
}

void ClaspAPIBackend::assume(const Potassco::LitSpan& lits) {
    if (auto *p = prg()) { p->addAssumption(lits); }
}

void ClaspAPIBackend::external(Potassco::Atom_t a, Potassco::Value_t v) {
    if (auto *p = prg()) {
        switch (v) {
            case Potassco::Value_t::False:   { p->freeze(a, Clasp::value_false); break; }
            case Potassco::Value_t::True:    { p->freeze(a, Clasp::value_true); break; }
            case Potassco::Value_t::Free:    { p->freeze(a, Clasp::value_free); break; }
            case Potassco::Value_t::Release: { p->unfreeze(a); break; }
        }
    }
}

void ClaspAPIBackend::theoryTerm(Potassco::Id_t, int) { }

void ClaspAPIBackend::theoryTerm(Potassco::Id_t, const Potassco::StringSpan&) { }

void ClaspAPIBackend::theoryTerm(Potassco::Id_t, int, const Potassco::IdSpan&) { }

void ClaspAPIBackend::theoryElement(Potassco::Id_t e, const Potassco::IdSpan&, const Potassco::LitSpan& cond) {
    if (auto p = prg()) {
        Potassco::TheoryElement const &elem = p->theoryData().getElement(e);
        if (elem.condition() == Potassco::TheoryData::COND_DEFERRED) { p->theoryData().setCondition(e, p->newCondition(cond)); }
    }
}

void ClaspAPIBackend::theoryAtom(Potassco::Id_t, Potassco::Id_t, const Potassco::IdSpan&) { }

void ClaspAPIBackend::theoryAtom(Potassco::Id_t, Potassco::Id_t, const Potassco::IdSpan&, Potassco::Id_t, Potassco::Id_t){ }

Clasp::Asp::LogicProgram *ClaspAPIBackend::prg() {
    return ctl_.update() ? static_cast<Clasp::Asp::LogicProgram*>(ctl_.clasp_->program()) : nullptr;
}

ClaspAPIBackend::~ClaspAPIBackend() noexcept = default;

// {{{1 definition of ClingoControl

class ClingoControl::ControlBackend : public Output::ASPIFOutBackend {
public:
    ControlBackend(ClingoControl &ctl)
    : ctl_{ctl} {
    }

    Output::OutputBase &beginOutput() override {
        ctl_.beginAddBackend();
        return *ctl_.out_;
    }

    void endOutput() override {
        ctl_.endAddBackend();
    }
private:
    ClingoControl &ctl_;
};

#define LOG if (verbose_) std::cerr
ClingoControl::ClingoControl(Scripts &scripts, bool clingoMode, Clasp::ClaspFacade *clasp, Clasp::Cli::ClaspCliConfig &claspConfig, PostGroundFunc pgf, PreSolveFunc psf, Logger::Printer printer, unsigned messageLimit)
: scripts_(scripts)
, clasp_(clasp)
, claspConfig_(claspConfig)
, pgf_(std::move(pgf))
, psf_(std::move(psf))
, logger_(std::move(printer), messageLimit)
, clingoMode_(clingoMode) {
    clasp->ctx.output.theory = &theory_;
}

void ClingoControl::parse() {
    if (!parser_->empty()) {
        switch (parser_->parse(logger_)) {
            case Input::ParseResult::Gringo: {
                defs_.init(logger_);
                parsed_ = true;
                break;
            }
            case Input::ParseResult::ASPIF: {
                break;
            }
        }
    }
    if (logger_.hasError()) {
        throw std::runtime_error("parsing failed");
    }
}

void ClingoControl::parse(const StringVec& files, const ClingoOptions& opts, Clasp::Asp::LogicProgram* claspOut, bool addStdIn) {
    using namespace Gringo;
    logger_.enable(Warnings::OperationUndefined, !opts.wNoOperationUndefined);
    logger_.enable(Warnings::AtomUndefined, !opts.wNoAtomUndef);
    logger_.enable(Warnings::FileIncluded, !opts.wNoFileIncluded);
    logger_.enable(Warnings::GlobalVariable, !opts.wNoGlobalVariable);
    logger_.enable(Warnings::Other, !opts.wNoOther);
    verbose_ = opts.verbose;
    Output::OutputPredicates outPreds;
    for (auto const &x : opts.sigvec) {
        outPreds.add(Location("<cmd>",1,1,"<cmd>", 1,1), x, false);
    }
    if (claspOut != nullptr) {
        out_ = gringo_make_unique<Output::OutputBase>(claspOut->theoryData(), std::move(outPreds), gringo_make_unique<ClaspAPIBackend>(*this), opts.outputOptions);
    }
    else {
        data_ = gringo_make_unique<Potassco::TheoryData>();
        out_ = gringo_make_unique<Output::OutputBase>(*data_, std::move(outPreds), std::cout, opts.outputFormat, opts.outputOptions);
    }
    out_->keepFacts = opts.keepFacts;
    aspif_bck_ = gringo_make_unique<ControlBackend>(*this);
    pb_ = gringo_make_unique<Input::NongroundProgramBuilder>(scripts_, prg_, out_->outPreds, defs_, opts.rewriteMinimize);
    parser_ = gringo_make_unique<Input::NonGroundParser>(*pb_, *aspif_bck_, incmode_);
    for (auto const &x : opts.defines) {
        LOG << "define: " << x << std::endl;
        parser_->parseDefine(x, logger_);
    }
    for (auto x : files) {
        LOG << "file: " << x << std::endl;
        parser_->pushFile(std::move(x), logger_);
    }
    if (files.empty() && addStdIn) {
        LOG << "reading from stdin" << std::endl;
        parser_->pushFile("-", logger_);
    }
    parse();
}

bool ClingoControl::update() {
    if (clingoMode_) {
        if (enableCleanup_) { cleanup(); }
        else                { canClean_ = false; }
        clasp_->update(configUpdate_);
        configUpdate_ = false;
        if (!clasp_->ok()) { return false; }
    }
    if (!grounded_) {
        if (!initialized_) {
            out_->init(clasp_->incremental());
            initialized_ = true;
        }
        out_->beginStep();
        grounded_ = true;
    }
    return true;
}

void ClingoControl::ground(Control::GroundVec const &parts, Context *context) {
    if (!update()) { return; }
    if (parsed_) {
        LOG << "************** parsed program **************" << std::endl << prg_;
        prg_.rewrite(defs_, logger_);
        LOG << "************* rewritten program ************" << std::endl << prg_;
        prg_.check(logger_);
        if (logger_.hasError()) {
            throw std::runtime_error("grounding stopped because of errors");
        }
        parsed_ = false;
    }
    if (!parts.empty()) {
        Ground::Parameters params;
        std::set<Sig> sigs;
        for (auto const &x : parts) {
            params.add(x.first, SymVec(x.second));
            sigs.emplace(x.first, numeric_cast<uint32_t>(x.second.size()), false);
        }
        auto gPrg = prg_.toGround(sigs, out_->data, logger_);
        LOG << "*********** intermediate program ***********" << std::endl
            << gPrg << std::endl;
        LOG << "************* grounded program *************" << std::endl;
        gPrg.prepare(params, *out_, logger_);
        scripts_.withContext(context, [&, this](Context &ctx) { gPrg.ground(ctx, *out_, logger_); });
    }
}

void ClingoControl::main(IClingoApp &app, StringVec const &files, const ClingoOptions& opts, Clasp::Asp::LogicProgram* out) {
    if (app.has_main()) {
        parse({}, opts, out, false);
        if (opts.singleShot) { clasp_->keepProgram(); }
        else { clasp_->enableProgramUpdates(); }
        app.main(*this, files);
    }
    else {
        parse(files, opts, out);
        if (scripts_.callable("main")) {
            if (opts.singleShot) { clasp_->keepProgram(); }
            else { clasp_->enableProgramUpdates(); }
            scripts_.main(*this);
        }
        else if (incmode_) {
            if (opts.singleShot) { clasp_->keepProgram(); }
            else { clasp_->enableProgramUpdates(); }
            incmode(*this);
        }
        else {
            claspConfig_.releaseOptions();
            Control::GroundVec parts;
            parts.emplace_back("base", SymVec{});
            ground(parts, nullptr);
            solve({nullptr, 0}, 0, nullptr)->get();
        }
    }
}
bool ClingoControl::onModel(Clasp::Model const &m) {
    bool ret = true;
    if (eventHandler_) {
        theory_.reset();
        std::lock_guard<decltype(propLock_)> lock(propLock_);
        ClingoModel model(*this, &m);
        ret = eventHandler_->on_model(model);
    }
    return ret;
}

bool ClingoControl::onUnsat(Potassco::Span<int64_t> optimization) {
    if (eventHandler_) {
        std::lock_guard<decltype(propLock_)> lock(propLock_);
        return eventHandler_->on_unsat(optimization);
    }
    return true;
}

void ClingoControl::onFinish(Clasp::ClaspFacade::Result ret) {
    if (eventHandler_) {
        eventHandler_->on_finish(convert(ret), &step_stats_, &accu_stats_);
        eventHandler_ = nullptr;
    }
}
Symbol ClingoControl::getConst(std::string const &name) const {
    auto ret = defs_.defs().find(name.c_str());
    if (ret != defs_.defs().end()) {
        bool undefined = false;
        Symbol val = std::get<2>(ret->second)->eval(undefined, const_cast<Logger&>(logger_));
        if (!undefined) { return val; }
    }
    return Symbol();
}
void ClingoControl::add(std::string const &name, Gringo::StringVec const &params, std::string const &part) {
    Location loc("<block>", 1, 1, "<block>", 1, 1);
    Input::IdVec idVec;
    for (auto &x : params) { idVec.emplace_back(loc, x); }
    parser_->pushBlock(name, std::move(idVec), part, logger_);
    parse();
}
void ClingoControl::load(std::string const &filename) {
    parser_->pushFile(std::string(filename), logger_);
    parse();
}
bool ClingoControl::hasSubKey(unsigned key, char const *name) const {
    unsigned subkey = claspConfig_.getKey(key, name);
    return subkey != Clasp::Cli::ClaspCliConfig::KEY_INVALID;
}
unsigned ClingoControl::getSubKey(unsigned key, char const *name) const {
    unsigned ret = claspConfig_.getKey(key, name);
    if (ret == Clasp::Cli::ClaspCliConfig::KEY_INVALID) {
        throw std::runtime_error("invalid key");
    }
    return ret;
}
unsigned ClingoControl::getArrKey(unsigned key, unsigned idx) const {
    unsigned ret = claspConfig_.getArrKey(key, idx);
    if (ret == Clasp::Cli::ClaspCliConfig::KEY_INVALID) {
        throw std::runtime_error("invalid key");
    }
    return ret;
}
void ClingoControl::getKeyInfo(unsigned key, int* nSubkeys, int* arrLen, const char** help, int* nValues) const {
    if (claspConfig_.getKeyInfo(key, nSubkeys, arrLen, help, nValues) < 0) {
        throw std::runtime_error("could not get key info");
    }
}
const char* ClingoControl::getSubKeyName(unsigned key, unsigned idx) const {
    char const *ret = claspConfig_.getSubkey(key, idx);
    if (!ret) {
        throw std::runtime_error("could not get subkey");
    }
    return ret;
}
bool ClingoControl::getKeyValue(unsigned key, std::string &value) const {
    int ret = claspConfig_.getValue(key, value);
    if (ret < -1) {
        throw std::runtime_error("could not get option value");
    }
    return ret >= 0;
}
void ClingoControl::setKeyValue(unsigned key, const char *val) {
    configUpdate_ = true;
    if (claspConfig_.setValue(key, val) <= 0) {
        throw std::runtime_error("could not set option value");
    }
}
unsigned ClingoControl::getRootKey() const {
    return Clasp::Cli::ClaspCliConfig::KEY_ROOT;
}
ConfigProxy &ClingoControl::getConf() {
    return *this;
}
USolveFuture ClingoControl::solve(Assumptions ass, clingo_solve_mode_bitset_t mode, USolveEventHandler cb) {
    canClean_ = false;
    prepare(ass);
    canClean_ = true;
    if (clingoMode_) {
        static_assert(clingo_solve_mode_yield == static_cast<clingo_solve_mode_bitset_t>(Clasp::SolveMode_t::Yield), "");
        static_assert(clingo_solve_mode_async == static_cast<clingo_solve_mode_bitset_t>(Clasp::SolveMode_t::Async), "");
        if (cb) {
            step_stats_.init(clasp_->getStats(), "user_step");
            accu_stats_.init(clasp_->getStats(), "user_accu");
        }
        eventHandler_ = std::move(cb);
        return gringo_make_unique<ClingoSolveFuture>(*this, static_cast<Clasp::SolveMode_t>(mode));
    }
    else {
        return gringo_make_unique<DefaultSolveFuture>(std::move(cb));
    }
}
void ClingoControl::interrupt() {
    clasp_->interrupt(65);
}
bool ClingoControl::blocked() {
    return clasp_->solving();
}

namespace {

class ClingoPropagateInit : public PropagateInit {
public:
    using Lit_t = Potassco::Lit_t;
    ClingoPropagateInit(Control &c, Clasp::ClingoPropagatorInit &p)
    : c_{c}, p_{p}, a_{*facade_().ctx.master()}, cc_{facade_().ctx.master()} {
        p_.enableHistory(false);
    }
    Output::DomainData const &theory() const override { return c_.theory(); }
    SymbolicAtoms const &getDomain() const override { return c_.getDomain(); }
    Potassco::Lit_t mapLit(Lit_t lit) const override {
        const auto& prg = static_cast<Clasp::Asp::LogicProgram&>(*facade_().program());
        return Clasp::encodeLit(prg.getLiteral(lit, Clasp::Asp::MapLit_t::Refined));
    }
    int threads() const override { return facade_().ctx.concurrency(); }
    void addWatch(Lit_t lit) override { p_.addWatch(Clasp::decodeLit(lit)); }
    void addWatch(uint32_t solverId, Lit_t lit) override { p_.addWatch(solverId, Clasp::decodeLit(lit)); }
    void removeWatch(Lit_t lit) override { p_.removeWatch(Clasp::decodeLit(lit)); }
    void removeWatch(uint32_t solverId, Lit_t lit) override { p_.removeWatch(solverId, Clasp::decodeLit(lit)); }
    void freezeLiteral(Lit_t lit) override { p_.freezeLit(Clasp::decodeLit(lit)); }
    void enableHistory(bool b) override { p_.enableHistory(b); };
    Potassco::Lit_t addLiteral(bool freeze) override {
        auto &ctx = facade_().ctx;
        auto var = ctx.addVar(Clasp::Var_t::Atom);
        if (freeze) {
            ctx.setFrozen(var, true);
        }
        return Clasp::encodeLit(Clasp::Literal(var, false));
    }
    bool addClause(Potassco::LitSpan lits) override {
        auto &ctx = static_cast<Clasp::ClaspFacade*>(c_.claspFacade())->ctx;
        if (ctx.master()->hasConflict()) { return false; }
        cc_.start();
        for (auto &lit : lits) { cc_.add(Clasp::decodeLit(lit)); }
        return cc_.end(Clasp::ClauseCreator::clause_force_simplify).ok();
    }
    bool addWeightConstraint(Potassco::Lit_t lit, Potassco::WeightLitSpan lits, Potassco::Weight_t bound, int type, bool eq) override {
        auto &ctx = static_cast<Clasp::ClaspFacade*>(c_.claspFacade())->ctx;
        auto &master = *ctx.master();
        if (master.hasConflict()) { return false; }
        Clasp::WeightLitVec claspLits;
        claspLits.reserve(lits.size);
        for (auto &x : lits) {
            claspLits.push_back({Clasp::decodeLit(x.lit), x.weight});
        }
        uint32_t creationFlags = 0;
        if (eq) {
            creationFlags |= Clasp::WeightConstraint::create_eq_bound;
        }
        if (type < 0) {
            creationFlags |= Clasp::WeightConstraint::create_only_bfb;
        }
        else if (type > 0) {
            creationFlags |= Clasp::WeightConstraint::create_only_btb;
        }
        return Clasp::WeightConstraint::create(*ctx.master(), Clasp::decodeLit(lit), claspLits, bound, creationFlags).ok();
    }
    void addMinimize(Potassco::Lit_t literal, Potassco::Weight_t weight, Potassco::Weight_t priority) override {
        auto &ctx = static_cast<Clasp::ClaspFacade*>(c_.claspFacade())->ctx;
        if (ctx.master()->hasConflict()) { return; }
        ctx.addMinimize({Clasp::decodeLit(literal), weight}, priority);
    }
    bool propagate() override {
        auto &ctx = static_cast<Clasp::ClaspFacade*>(c_.claspFacade())->ctx;
        if (ctx.master()->hasConflict()) { return false; }
        return ctx.master()->propagate();
    }
    void setCheckMode(clingo_propagator_check_mode_t checkMode) override {
        p_.enableClingoPropagatorCheck(static_cast<Clasp::ClingoPropagatorCheck_t::Type>(checkMode));
    }
    void setUndoMode(clingo_propagator_undo_mode_t undoMode) override {
        p_.enableClingoPropagatorUndo(static_cast<Clasp::ClingoPropagatorUndo_t::Type>(undoMode));
    }
    Potassco::AbstractAssignment const &assignment() const override { return a_; }
    clingo_propagator_check_mode_t getCheckMode() const override { return p_.checkMode(); }
    clingo_propagator_undo_mode_t getUndoMode() const override { return p_.undoMode(); }
private:
    Clasp::ClaspFacade &facade_() const { return *static_cast<ClingoControl&>(c_).clasp_; }

    Control &c_;
    Clasp::ClingoPropagatorInit &p_;
    Clasp::ClingoAssignment a_;
    Clasp::ClauseCreator cc_;
};

} // namespace

void ClingoControl::prepare(Assumptions ass) {
    eventHandler_ = nullptr;
    // finalize the program
    if (update()) { out_->endStep(ass); }
    grounded_ = false;
    if (clingoMode_) {
        Clasp::ProgramBuilder *prg = clasp_->program();
        postGround(*prg);
        if (!propagators_.empty()) {
            clasp_->program()->endProgram();
            for (auto&& pp : propagators_) {
                ClingoPropagateInit i(*this, *pp);
                static_cast<Propagator*>(pp->propagator())->init(i);
            }
            propLock_.init(clasp_->ctx.concurrency());
        }
        prePrepare(*clasp_);
        clasp_->prepare(enableEnumAssupmption_ ? Clasp::ClaspFacade::enum_volatile : Clasp::ClaspFacade::enum_static);
        preSolve(*clasp_);
    }
    out_->reset(data_ || (clasp_ && clasp_->program()));
}

void *ClingoControl::claspFacade() {
    return clasp_;
}

const char* TheoryOutput::first(const Clasp::Model&) {
    index_ = 0;
    return next();
}

const char* TheoryOutput::next() {
    if (index_ < symbols_.size()) {
        std::stringstream ss;
        symbols_[index_].print(ss);
        current_ = ss.str();
        ++index_;
        return current_.c_str();
    }
    return nullptr;
}

Potassco::Lit_t ClingoControl::decide(Id_t solverId, Potassco::AbstractAssignment const &assignment, Potassco::Lit_t fallback) {
    for (auto &heu : heus_) {
        auto ret = heu->decide(solverId, assignment, fallback);
        if (ret != 0) { return ret; }
    }
    return fallback;
}

void ClingoControl::registerPropagator(UProp p, bool sequential) {
    propagators_.emplace_back(gringo_make_unique<Clasp::ClingoPropagatorInit>(*p, propLock_.add(sequential)));
    claspConfig_.addConfigurator(propagators_.back().get(), Clasp::Ownership_t::Retain);
    static_cast<Clasp::Asp::LogicProgram*>(clasp_->program())->enableDistinctTrue();
    props_.emplace_back(std::move(p));
    if (props_.back()->hasHeuristic()) {
        if (heus_.empty()) {
            claspConfig_.setHeuristicCreator(new Clasp::ClingoHeuristic::Factory(*this, propLock_.add(sequential)));
        }
        heus_.emplace_back(props_.back().get());
    }
}

void ClingoControl::cleanup() {
    if (!clingoMode_ || !canClean_) {
        return;
    }
    canClean_ = false;
    Clasp::Asp::LogicProgram &prg = static_cast<Clasp::Asp::LogicProgram&>(*clasp_->program());
    Clasp::Solver &solver = *clasp_->ctx.master();
    auto assignment = [&prg, &solver](unsigned uid) {
        Potassco::Value_t truth{Potassco::Value_t::Free};
        bool external{false};
        if (prg.validAtom(uid)) {
            external = prg.isExternal(uid);
            Clasp::Literal lit = prg.getLiteral(uid);
            if (solver.isTrue(lit)) { truth = Potassco::Value_t::True; }
            else if (solver.isFalse(lit)) { truth = Potassco::Value_t::False; }
        }
        return std::make_pair(external, truth);
    };
    auto stats = out_->simplify(assignment);
    LOG << stats.first << " atom" << (stats.first == 1 ? "" : "s") << " became facts" << std::endl;
    LOG << stats.second << " atom" << (stats.second == 1 ? "" : "s") << " deleted" << std::endl;
}

std::string ClingoControl::str() {
    return "[object:IncrementalControl]";
}

void ClingoControl::assignExternal(Potassco::Atom_t ext, Potassco::Value_t val) {
    if (update()) {
        auto *backend = out_->backend();
        if (backend != nullptr) {
            backend->external(ext, val);
        }
    }
}

bool ClingoControl::isConflicting() const noexcept {
    return !clasp_->ok();
}

Potassco::AbstractStatistics const *ClingoControl::statistics() const {
    return clasp_->getStats();
}

void ClingoControl::useEnumAssumption(bool enable) {
    enableEnumAssupmption_ = enable;
}

bool ClingoControl::useEnumAssumption() const {
    return enableEnumAssupmption_;
}

void ClingoControl::enableCleanup(bool enable) {
    enableCleanup_ = enable;
}

bool ClingoControl::enableCleanup() const {
    return enableCleanup_;
}

SymbolicAtoms const &ClingoControl::getDomain() const {
    return *this;
}

namespace {

union SymbolicAtomOffset {
    SymbolicAtomOffset(clingo_symbolic_atom_iterator_t repr)
    : repr(repr) { }
    SymbolicAtomOffset(uint32_t domain_offset, bool domain_advance, uint32_t atom_offset, bool atom_advance)
    : data{domain_offset, domain_advance, atom_offset, atom_advance} { }
    clingo_symbolic_atom_iterator_t repr;
    struct {
        clingo_symbolic_atom_iterator_t domain_offset : 31;
        clingo_symbolic_atom_iterator_t domain_advance : 1;
        clingo_symbolic_atom_iterator_t atom_offset : 31;
        clingo_symbolic_atom_iterator_t atom_advance : 1;
    } data;
};

bool operator==(SymbolicAtomOffset const &a, SymbolicAtomOffset const &b) {
    return a.data.domain_offset == b.data.domain_offset && a.data.atom_offset == b.data.atom_offset;
}

SymbolicAtomOffset &toOffset(clingo_symbolic_atom_iterator_t &it) {
    return reinterpret_cast<SymbolicAtomOffset &>(it);
}

bool skipDomain(Sig sig)                                          { return sig.name().startsWith("#"); }

SymbolicAtomIter init(Output::OutputBase &out, uint32_t domainOffset, bool advance) {
    SymbolicAtomOffset it(domainOffset, advance, 0, false);
    for (auto domIt = out.predDoms().begin() + domainOffset, domIe = out.predDoms().end(); domIt != domIe; ++domIt, ++it.data.domain_offset) {
        auto &dom = **domIt;
        if (!skipDomain(dom) && dom.size() > 0) { return it.repr; }
        if (!it.data.domain_advance) { break; }
    }
    it.data.domain_offset = out.predDoms().size();
    return it.repr;
}

void advance(Output::OutputBase &out, SymbolicAtomIter &it) {
    auto &off = toOffset(it).data;
    auto domIt  = out.predDoms().begin() + off.domain_offset;
    auto domIe  = out.predDoms().end();
    auto elemIt = (*domIt)->begin() + off.atom_offset;
    auto elemIe = (*domIt)->end();
    ++elemIt; ++off.atom_offset;
    while (elemIt == elemIe) {
        off.atom_offset = 0;
        if (!off.domain_advance) {
            off.domain_offset = out.predDoms().size();
            return;
        }
        ++domIt; ++off.domain_offset;
        if (domIt == domIe) { return; }
        if (!skipDomain(**domIt)) {
            elemIt = (*domIt)->begin();
            elemIe = (*domIt)->end();
        }
    }
}

Output::PredicateAtom &domainElem(Output::PredDomMap &map, SymbolicAtomIter it) {
    auto &off = toOffset(it).data;
    return map.nth(off.domain_offset).key()->operator[](off.atom_offset);
}

} // namespace

Symbol ClingoControl::atom(SymbolicAtomIter it) const {
    return domainElem(out_->predDoms(), it);
}

Potassco::Lit_t ClingoControl::literal(SymbolicAtomIter it) const {
    auto &elem = domainElem(out_->predDoms(), it);
    return elem.hasUid() ? elem.uid() : 0;
}

bool ClingoControl::fact(SymbolicAtomIter it) const {
    return domainElem(out_->predDoms(), it).fact();
}

bool ClingoControl::external(SymbolicAtomIter it) const {
    auto &elem = domainElem(out_->predDoms(), it);
    return elem.hasUid() && elem.isExternal() && (!clingoMode_ || static_cast<Clasp::Asp::LogicProgram*>(clasp_->program())->isExternal(elem.uid()));
}

SymbolicAtomIter ClingoControl::next(SymbolicAtomIter it) const {
    advance(*out_, it);
    return it;
}

bool ClingoControl::valid(SymbolicAtomIter it) const {
    auto &off = toOffset(it).data;
    return off.domain_offset < out_->predDoms().size() && off.atom_offset < out_->predDom(off.domain_offset).size();
}

std::vector<Sig> ClingoControl::signatures() const {
    std::vector<Sig> ret;
    for (auto &dom : out_->predDoms()) {
        if (!skipDomain(*dom)) { ret.emplace_back(*dom); }
    }
    return ret;
}

SymbolicAtomIter ClingoControl::begin(Sig sig) const {
    auto &doms = out_->predDoms();
    auto it = doms.find(sig);
    if (it != doms.end()) {
        return init(*out_, it.key()->domainOffset(), false);
    }
    return init(*out_, numeric_cast<uint32_t>(doms.size()), false);
}

SymbolicAtomIter ClingoControl::begin() const {
    return init(*out_, 0, true);
}

SymbolicAtomIter ClingoControl::end() const {
    return SymbolicAtomOffset{numeric_cast<uint32_t>(out_->predDoms().size()), false, 0, false}.repr;
}

bool ClingoControl::eq(SymbolicAtomIter it, SymbolicAtomIter jt) const {
    return toOffset(it) == toOffset(jt);
}

SymbolicAtomIter ClingoControl::lookup(Symbol atom) const {
    if (atom.hasSig()) {
        auto it = out_->predDoms().find(atom.sig());
        if (it != out_->predDoms().end()) {
            auto jt = (*it)->find(atom);
            if (jt != (*it)->end()) {
                return SymbolicAtomOffset(it.key()->domainOffset(), true, numeric_cast<uint32_t>(jt - (*it)->begin()), true).repr;
            }
        }
    }
    return SymbolicAtomOffset(out_->predDoms().size(), true, 0, true).repr;
}

size_t ClingoControl::length() const {
    size_t ret = 0;
    for (auto &dom : out_->predDoms()) {
        if (!skipDomain(*dom)) {
            ret += dom->size();
        }
    }
    return ret;
}

bool ClingoControl::beginAddBackend() {
    update();
    backend_prg_ = std::make_unique<Ground::Program>(prg_.toGround({}, out_->data, logger_));
    backend_prg_->prepare({}, *out_, logger_);
    backend_ = out_->backend();
    return backend_ != nullptr;
}

Id_t ClingoControl::addAtom(Symbol sym) {
    bool added = false;
    auto atom  = out_->addAtom(sym, &added);
    if (added) { added_atoms_.emplace_back(sym); }
    return atom;
}

void ClingoControl::addFact(Potassco::Atom_t uid) {
    added_facts_.emplace(uid);
}

void ClingoControl::endAddBackend() {
    for (auto &sym : added_atoms_) {
        auto it = out_->predDoms().find(sym.sig());
        assert(it != out_->predDoms().end());
        auto jt = (*it)->find(sym);
        assert(jt != (*it)->end());
        assert(jt->hasUid());
        if (added_facts_.find(jt->uid()) != added_facts_.end()) {
            jt->setFact(true);
        }
    }
    added_atoms_.clear();
    added_facts_.clear();
    backend_prg_->ground(scripts_, *out_, logger_);
    backend_prg_.reset(nullptr);
    backend_ = nullptr;
}

Potassco::Atom_t ClingoControl::addProgramAtom() { return out_->data.newAtom(); }

ClingoControl::~ClingoControl() noexcept = default;

// {{{1 definition of ClingoSolveFuture

SolveResult convert(Clasp::ClaspFacade::Result res) {
    SolveResult::Satisfiabily sat = SolveResult::Satisfiable;
    switch (res) {
        case Clasp::ClaspFacade::Result::SAT:     { sat = SolveResult::Satisfiable; break; }
        case Clasp::ClaspFacade::Result::UNSAT:   { sat = SolveResult::Unsatisfiable; break; }
        case Clasp::ClaspFacade::Result::UNKNOWN: { sat = SolveResult::Unknown; break; }
    }
    return {sat, res.exhausted(), res.interrupted()};
}

ClingoSolveFuture::ClingoSolveFuture(ClingoControl &ctl, Clasp::SolveMode_t mode)
: model_{ctl}
, handle_{model_.context().clasp_->solve(mode)} { }

SolveResult ClingoSolveFuture::get() {
    auto res = handle_.get();
    if (res.interrupted() && res.signal != 0 && res.signal != 9 && res.signal != 65) {
        throw std::runtime_error("solving stopped by signal");
    }
    return convert(res);
}
Model const *ClingoSolveFuture::model() {
    if (auto m = handle_.model()) {
        model_.reset(*m);
        return &model_;
    }
    else { return nullptr; }
}
Potassco::LitSpan ClingoSolveFuture::unsatCore() {
    auto &facade = *model_.context().clasp_;
    auto &summary = facade.summary();
    if (!summary.unsat()) {
        return {nullptr, 0};
    }
    auto core = summary.unsatCore();
    if (core == nullptr) {
        return {nullptr, 0};
    }
    auto *prg = static_cast<Clasp::Asp::LogicProgram*>(facade.program());
    prg->extractCore(*core, core_);
    // Note: this is just to make writing library code easier, a user probably
    // never has to worry about this.
    if (core_.empty()) {
        static Potassco::Lit_t sentinel{0};
        return Potassco::toSpan(&sentinel, 0);
    }
    return Potassco::toSpan(core_);
}
bool ClingoSolveFuture::wait(double timeout) {
    if (timeout == 0)      { return handle_.ready(); }
    else if (timeout < 0)  { return handle_.wait(), true; }
    else                   { return handle_.waitFor(timeout); }
}
void ClingoSolveFuture::cancel() {
    handle_.cancel();
}
void ClingoSolveFuture::resume() {
    handle_.resume();
}

// {{{1 definition of ClingoLib

ClingoLib::ClingoLib(Scripts &scripts, int argc, char const * const *argv, Logger::Printer printer, unsigned messageLimit)
        : ClingoControl(scripts, true, &clasp_, claspConfig_, nullptr, nullptr, printer, messageLimit) {
    using namespace Potassco::ProgramOptions;
    OptionContext allOpts("<libclingo>");
    initOptions(allOpts);
    ParsedValues values = parseCommandArray(argv, argc, allOpts, false, parsePositional);
    ParsedOptions parsed;
    parsed.assign(values);
    allOpts.assignDefaults(parsed);
    claspConfig_.finalize(parsed, Clasp::Problem_t::Asp, true);
    clasp_.ctx.setEventHandler(this);
    Clasp::Asp::LogicProgram* lp = &clasp_.startAsp(claspConfig_, !grOpts_.singleShot);
    if (grOpts_.singleShot) { clasp_.keepProgram(); }
    parse({}, grOpts_, lp, false);
}


static bool parseConst(const std::string& str, std::vector<std::string>& out) {
    out.push_back(str);
    return true;
}

void ClingoLib::initOptions(Potassco::ProgramOptions::OptionContext& root) {
    using namespace Potassco::ProgramOptions;
    grOpts_.defines.clear();
    grOpts_.verbose = false;
    OptionGroup gringo("Gringo Options");
    gringo.addOptions()
        ("verbose,V"                , flag(grOpts_.verbose = false), "Enable verbose output")
        ("const,c"                  , storeTo(grOpts_.defines, parseConst)->composing()->arg("<id>=<term>"), "Replace term occurrences of <id> with <term>")
        ("output-debug"             , storeTo(grOpts_.outputOptions.debug = Output::OutputDebug::NONE, values<Output::OutputDebug>()
          ("none", Output::OutputDebug::NONE)
          ("text", Output::OutputDebug::TEXT)
          ("translate", Output::OutputDebug::TRANSLATE)
          ("all", Output::OutputDebug::ALL)),
         "Print debug information during output:\n"
         "      none     : no additional info\n"
         "      text     : print rules as plain text (prefix %%)\n"
         "      translate: print translated rules as plain text (prefix %%%%)\n"
         "      all      : combines text and translate")
        ("warn,W"                   , storeTo(grOpts_, parseWarning)->arg("<warn>")->composing(),
         "Enable/disable warnings:\n"
         "      none                    : disable all warnings\n"
         "      all                     : enable all warnings\n"
         "      [no-]atom-undefined     : a :- b.\n"
         "      [no-]file-included      : #include \"a.lp\". #include \"a.lp\".\n"
         "      [no-]operation-undefined: p(1/0).\n"
         "      [no-]global-variable    : :- #count { X } = 1, X = 1.\n"
         "      [no-]other              : clasp related and uncategorized warnings")
        ("rewrite-minimize"         , flag(grOpts_.rewriteMinimize = false), "Rewrite minimize constraints into rules")
        // for backward compatibility
        ("keep-facts"               , flag(grOpts_.keepFacts = false), "Preserve facts in rule bodies")
        ("preserve-facts"           , storeTo(grOpts_, parsePreserveFacts),
         "Preserve facts in output:\n"
         "      none  : do not preserve\n"
         "      body  : do not preserve\n"
         "      symtab: do not preserve\n"
         "      all   : preserve all facts")
        ("single-shot"              , flag(grOpts_.singleShot = false), "Force single-shot solving mode")
        ("show-preds"               , storeTo(grOpts_.sigvec, parseSigVec), "Show the given signatures")
        ;
    root.add(gringo);
    claspConfig_.addOptions(root);
}

bool ClingoLib::onModel(Clasp::Solver const&, Clasp::Model const& m) {
    return ClingoControl::onModel(m);
}

bool ClingoLib::onUnsat(Clasp::Solver const &s, Clasp::Model const &m) {
    if (m.ctx != nullptr && m.ctx->optimize() && s.lower.active()) {
        std::vector<int64_t> optimization;
        auto const *costs = m.num > 0 ? m.costs : nullptr;
        if (costs != nullptr && costs->size() > s.lower.level) {
            optimization.insert(optimization.end(), costs->begin(), costs->begin() + s.lower.level);
        }
        optimization.emplace_back(s.lower.bound);
        return ClingoControl::onUnsat(Potassco::toSpan(optimization));
    }
    return true;
}

void ClingoLib::onEvent(Clasp::Event const& ev) {
    Clasp::ClaspFacade::StepReady const *r = Clasp::event_cast<Clasp::ClaspFacade::StepReady>(ev);
    if (r) { onFinish(r->summary->result); }
    const Clasp::LogEvent* log = Clasp::event_cast<Clasp::LogEvent>(ev);
    if (log && log->isWarning()) { logger_.print(Warnings::Other, log->msg); }
}

bool ClingoLib::parsePositional(const std::string& t, std::string& out) {
    int num;
    if (Potassco::string_cast(t, num)) {
        out = "number";
        return true;
    }
    return false;
}

ClingoLib::~ClingoLib() {
    clasp_.shutdown();
}

// }}}1

} // namespace Gringo
