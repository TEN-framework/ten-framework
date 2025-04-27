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

#ifndef CLINGO_CLINGOCONTROL_HH
#define CLINGO_CLINGOCONTROL_HH

#include "clingo.h"
#include <potassco/basic_types.h>
#include <clingo/control.hh>
#include <clingo/scripts.hh>
#include <clingo/astv2.hh>
#include <gringo/output/output.hh>
#include <gringo/input/program.hh>
#include <gringo/input/programbuilder.hh>
#include <gringo/input/nongroundparser.hh>
#include <gringo/input/groundtermparser.hh>
#include <gringo/logger.hh>
#include <clasp/logic_program.h>
#include <clasp/clasp_facade.h>
#include <clasp/solver.h>
#include <clasp/clingo.h>
#include <clasp/cli/clasp_options.h>
#include <potassco/application.h>
#include <potassco/string_convert.h>
#include <mutex>
#include <cstdlib>

namespace Gringo {

// {{{1 declaration of ClaspAPIBackend

class ClingoControl;
class ClaspAPIBackend : public Backend {
public:
    ClaspAPIBackend(ClingoControl& ctl) : ctl_(ctl) { }
    ClaspAPIBackend(const ClaspAPIBackend&) = delete;
    ClaspAPIBackend& operator=(const ClaspAPIBackend&) = delete;
    void initProgram(bool incremental) override;
    void beginStep() override;
    void rule(Potassco::Head_t ht, const Potassco::AtomSpan& head, const Potassco::LitSpan& body) override;
    void rule(Potassco::Head_t ht, const Potassco::AtomSpan& head, Potassco::Weight_t bound, const Potassco::WeightLitSpan& body) override;
    void minimize(Potassco::Weight_t prio, const Potassco::WeightLitSpan& lits) override;
    void project(const Potassco::AtomSpan& atoms) override;
    void output(Symbol sym, Potassco::Atom_t atom) override;
    void output(Symbol sym, Potassco::LitSpan const& condition) override;
    void external(Potassco::Atom_t a, Potassco::Value_t v) override;
    void assume(const Potassco::LitSpan& lits) override;
    void heuristic(Potassco::Atom_t a, Potassco::Heuristic_t t, int bias, unsigned prio, const Potassco::LitSpan& condition) override;
    void acycEdge(int s, int t, const Potassco::LitSpan& condition) override;
    void theoryTerm(Potassco::Id_t termId, int number) override;
    void theoryTerm(Potassco::Id_t termId, const Potassco::StringSpan& name) override;
    void theoryTerm(Potassco::Id_t termId, int cId, const Potassco::IdSpan& args) override;
    void theoryElement(Potassco::Id_t elementId, const Potassco::IdSpan& terms, const Potassco::LitSpan& cond) override;
    void theoryAtom(Potassco::Id_t atomOrZero, Potassco::Id_t termId, const Potassco::IdSpan& elements) override;
    void theoryAtom(Potassco::Id_t atomOrZero, Potassco::Id_t termId, const Potassco::IdSpan& elements, Potassco::Id_t op, Potassco::Id_t rhs) override;
    void endStep() override;
    ~ClaspAPIBackend() noexcept override;
private:
    Clasp::Asp::LogicProgram *prg();
    ClingoControl& ctl_;
};

// {{{1 declaration of ClingoOptions

struct ClingoOptions {
    using SigVec = std::vector<Sig>;
    std::vector<std::string>      defines;
    Output::OutputOptions outputOptions;
    Output::OutputFormat  outputFormat          = Output::OutputFormat::INTERMEDIATE;
    bool                          verbose               = false;
    bool                          wNoOperationUndefined = false;
    bool                          wNoAtomUndef          = false;
    bool                          wNoFileIncluded       = false;
    bool                          wNoGlobalVariable     = false;
    bool                          wNoOther              = false;
    bool                          rewriteMinimize       = false;
    bool                          keepFacts             = false;
    bool                          singleShot            = false;
    SigVec                        sigvec;
};

inline void enableAll(ClingoOptions& out, bool enable) {
    out.wNoAtomUndef          = !enable;
    out.wNoFileIncluded       = !enable;
    out.wNoOperationUndefined = !enable;
    out.wNoGlobalVariable     = !enable;
    out.wNoOther              = !enable;
}

inline bool parseWarning(const std::string& str, ClingoOptions& out) {
    if (str == "none")                     { enableAll(out, false);             return true; }
    if (str == "all")                      { enableAll(out, true);              return true; }
    if (str == "no-atom-undefined")        { out.wNoAtomUndef          = true;  return true; }
    if (str ==    "atom-undefined")        { out.wNoAtomUndef          = false; return true; }
    if (str == "no-file-included")         { out.wNoFileIncluded       = true;  return true; }
    if (str ==    "file-included")         { out.wNoFileIncluded       = false; return true; }
    if (str == "no-operation-undefined")   { out.wNoOperationUndefined = true;  return true; }
    if (str ==    "operation-undefined")   { out.wNoOperationUndefined = false; return true; }
    if (str == "no-global-variable")       { out.wNoGlobalVariable     = true;  return true; }
    if (str ==    "global-variable")       { out.wNoGlobalVariable     = false; return true; }
    if (str == "no-other")                 { out.wNoOther              = true;  return true; }
    if (str ==    "other")                 { out.wNoOther              = false; return true; }
    return false;
}

inline bool parsePreserveFacts(const std::string& str, ClingoOptions& out) {
    if (str == "none")   { out.keepFacts = false; out.outputOptions.preserveFacts = false; return true; }
    if (str == "body")   { out.keepFacts = true;  out.outputOptions.preserveFacts = false; return true; }
    if (str == "symtab") { out.keepFacts = false; out.outputOptions.preserveFacts = true;  return true; }
    if (str == "all")    { out.keepFacts = true;  out.outputOptions.preserveFacts = true;  return true; }
    return false;
}

inline std::vector<std::string> split(std::string const &source, char const *delimiter = " ", bool keepEmpty = false) {
    std::vector<std::string> results;
    size_t prev = 0;
    size_t next = 0;
    while ((next = source.find_first_of(delimiter, prev)) != std::string::npos) {
        if (keepEmpty || (next - prev != 0)) { results.push_back(source.substr(prev, next - prev)); }
        prev = next + 1;
    }
    if (prev < source.size()) { results.push_back(source.substr(prev)); }
    return results;
}

inline bool parseSigVec(const std::string& str, ClingoOptions::SigVec& sigvec) {
    for (auto &x : split(str, ",")) {
        auto y = split(x, "/");
        if (y.size() != 2) { return false; }
        unsigned a;
        if (!Potassco::string_cast<unsigned>(y[1], a)) { return false; }
        bool sign = !y[0].empty() && y[0][0] == '-';
        if (sign) { y[0] = y[0].substr(1); }
        sigvec.emplace_back(y[0].c_str(), a, sign);
    }
    return true;
}

// {{{1 declaration of ClingoControl

class ClingoPropagatorLock : public Clasp::ClingoPropagatorLock {
public:
    ClingoPropagatorLock() : seq_(0) {}
    void lock()   override { if (mut_) mut_->lock(); }
    void unlock() override { if (mut_) mut_->unlock(); }
    ClingoPropagatorLock* add(bool seq) {
        if (seq) { ++seq_; return this; }
        return 0;
    }
    void init(unsigned nThreads) {
        if      (nThreads < 2 || !seq_) { mut_ = nullptr; }
        else if (!mut_.get())           { mut_ = gringo_make_unique<std::mutex>(); }
    }
private:
    using OptLock = std::unique_ptr<std::mutex>;
    OptLock  mut_;
    unsigned seq_;
};

class ClingoApp;
class IClingoApp {
public:
    virtual unsigned message_limit() const { return 20; }
    virtual char const *program_name() const { return "clingo"; }
    virtual char const *version() const { return clingo_version_string(); }
    virtual bool has_main() const { return false; }
    virtual void main(ClingoControl &ctl, std::vector<std::string> const &files) {
        static_cast<void>(ctl);
        static_cast<void>(files);
        throw std::runtime_error("not implemented");
    }
    virtual bool has_log() const { return false; }
    virtual void log(Gringo::Warnings code, char const *message) noexcept {
        static_cast<void>(code);
        static_cast<void>(message);
        std::terminate();
    }
    virtual bool has_printer() const { return false; }
    virtual void print_model(Model *model, std::function<void()> printer) {
        static_cast<void>(model);
        printer();
    }
    virtual void register_options(ClingoApp &app) { static_cast<void>(app); }
    virtual void validate_options() { }
    virtual ~IClingoApp() = default;
};
using UIClingoApp = std::unique_ptr<IClingoApp>;

class TheoryOutput : public Clasp::OutputTable::Theory {
public:
    char const * first(const Clasp::Model&) override;
    char const * next() override;
    void add(Potassco::Span<Symbol> symbols) {
        symbols_.insert(symbols_.end(), begin(symbols), end(symbols));
    }
    void copy_symbols(std::vector<Symbol> &symbols) {
        symbols.insert(symbols.end(), symbols_.begin(), symbols_.end());
    }
    void reset() {
        symbols_.clear();
        index_ = 0;
    }

private:
    std::vector<Symbol> symbols_;
    std::string         current_;
    size_t              index_;
};

class UserStatistics : public Potassco::AbstractStatistics {
public:
    explicit UserStatistics()
    : stats_{nullptr}
    , root_{0} { }
    void init(Potassco::AbstractStatistics *stats, char const *root) {
        stats_ = stats;
        root_ = stats_->add(stats_->root(), root, Potassco::Statistics_t::Map);
    }

    Key_t root() const override { return root_; }
    Potassco::Statistics_t type(Key_t key) const override { return stats_->type(key); };
    size_t size(Key_t key) const override { return stats_->size(key); }
    bool writable(Key_t key) const override { return stats_->writable(key); }

    Key_t at(Key_t arr, size_t index) const override { return stats_->at(arr, index); }
    Key_t push(Key_t arr, Potassco::Statistics_t type) override { return stats_->push(arr, type); }

    const char* key(Key_t mapK, size_t i) const override { return stats_->key(mapK, i); }
    Key_t get(Key_t mapK, const char* at) const override { return stats_->get(mapK, at); }
    bool find(Key_t mapK, const char* element, Key_t* outKey) const override { return stats_->find(mapK, element, outKey); }

    Key_t add(Key_t mapK, const char* name, Potassco::Statistics_t type) override { return stats_->add(mapK, name, type); }
    double value(Key_t key) const override { return stats_->value(key); }
    void set(Key_t key, double value) override { return stats_->set(key, value); }
private:
    Potassco::AbstractStatistics *stats_;
    Key_t root_;
};

class ClingoSolveFuture;
class ClingoControl : public clingo_control, private ConfigProxy, private SymbolicAtoms, private Potassco::AbstractHeuristic {
    class ControlBackend;
public:
    using StringVec        = std::vector<std::string>;
    using ExternalVec      = std::vector<std::pair<Symbol, Potassco::Value_t>>;
    using PostGroundFunc   = std::function<bool (Clasp::ProgramBuilder &)>;
    using PreSolveFunc     = std::function<bool (Clasp::ClaspFacade &)>;
    enum class ConfigUpdate { KEEP, REPLACE };

    ClingoControl(Scripts &scripts, bool clingoMode, Clasp::ClaspFacade *clasp, Clasp::Cli::ClaspCliConfig &claspConfig, PostGroundFunc pgf, PreSolveFunc psf, Logger::Printer printer, unsigned messageLimit);
    ~ClingoControl() noexcept override;
    void prepare(Assumptions ass);
    void commitExternals();
    void parse();
    void parse(const StringVec& files, const ClingoOptions& opts, Clasp::Asp::LogicProgram* out, bool addStdIn = true);
    void main(IClingoApp &app, StringVec const &files, const ClingoOptions& opts, Clasp::Asp::LogicProgram* out);
    bool onModel(Clasp::Model const &m);
    bool onUnsat(Potassco::Span<int64_t> optimization);
    void onFinish(Clasp::ClaspFacade::Result ret);
    bool update();

    virtual void postGround(Clasp::ProgramBuilder& prg) {
        if (pgf_ && !pgf_(prg)) {
            fflush(stderr);
            fflush(stdout);
            std::_Exit(0);
        }
    }
    virtual void prePrepare(Clasp::ClaspFacade& ) { }
    virtual void preSolve(Clasp::ClaspFacade& clasp) { if (psf_) { psf_(clasp);} }
    virtual void postSolve(Clasp::ClaspFacade& ) { }

    // {{{2 SymbolicAtoms interface

    SymbolicAtomIter begin(Sig sig) const override;
    SymbolicAtomIter begin() const override;
    SymbolicAtomIter end() const override;
    bool eq(SymbolicAtomIter it, SymbolicAtomIter jt) const override;
    SymbolicAtomIter lookup(Symbol atom) const override;
    size_t length() const override;
    std::vector<Sig> signatures() const override;
    Symbol atom(SymbolicAtomIter it) const override;
    Potassco::Lit_t literal(SymbolicAtomIter it) const override;
    bool fact(SymbolicAtomIter it) const override;
    bool external(SymbolicAtomIter it) const override;
    SymbolicAtomIter next(SymbolicAtomIter it) const override;
    bool valid(SymbolicAtomIter it) const override;

    // {{{2 ConfigProxy interface

    bool hasSubKey(unsigned key, char const *name) const override;
    unsigned getSubKey(unsigned key, char const *name) const override;
    unsigned getArrKey(unsigned key, unsigned idx) const override;
    void getKeyInfo(unsigned key, int* nSubkeys = 0, int* arrLen = 0, const char** help = 0, int* nValues = 0) const override;
    const char* getSubKeyName(unsigned key, unsigned idx) const override;
    bool getKeyValue(unsigned key, std::string &value) const override;
    void setKeyValue(unsigned key, const char *val) override;
    unsigned getRootKey() const override;

    // {{{2 Control interface

    SymbolicAtoms const &getDomain() const override;
    void ground(Control::GroundVec const &vec, Context *ctx) override;
    void add(std::string const &name, Gringo::StringVec const &params, std::string const &part) override;
    void load(std::string const &filename) override;
    bool blocked() override;
    std::string str();
    void assignExternal(Potassco::Atom_t ext, Potassco::Value_t) override;
    Symbol getConst(std::string const &name) const override;
    bool isConflicting() const noexcept override;
    Potassco::AbstractStatistics const *statistics() const override;
    ConfigProxy &getConf() override;
    void useEnumAssumption(bool enable) override;
    bool useEnumAssumption() const override;
    void cleanup() override;
    void enableCleanup(bool enable) override;
    bool enableCleanup() const override;
    USolveFuture solve(Assumptions ass, clingo_solve_mode_bitset_t mode, USolveEventHandler cb) override;
    Output::DomainData const &theory() const override { return out_->data; }
    void registerPropagator(UProp p, bool sequential) override;
    void interrupt() override;
    void *claspFacade() override;
    bool beginAddBackend() override;
    Gringo::Output::TheoryData &theoryData() override {
        return out_->data.theory();
    }
    Id_t addAtom(Symbol sym) override;
    void addFact(Potassco::Atom_t uid) override;
    Backend *getBackend() override {
        if (!backend_) { throw std::runtime_error("backend not available"); }
        return backend_;
    };
    Backend &getASPIFBackend() override {
        return *aspif_bck_;
    };
    void endAddBackend() override;
    Potassco::Atom_t addProgramAtom() override;
    Logger &logger() override { return logger_; }
    void beginAdd() override { parse(); }
    void add(clingo_ast_t const &ast) override { Input::parse(*pb_, logger_, ast.ast); }
    void endAdd() override {
        parser_->disable_aspif();
        defs_.init(logger_);
        parsed_ = true;
    }
    void registerObserver(UBackend obs, bool replace) override {
        if (replace) { clingoMode_ = false; }
        out_->registerObserver(std::move(obs), replace);
    }

    // }}}2
    Lit decide(Id_t solverId, Potassco::AbstractAssignment const &assignment, Lit fallback) override;

    std::unique_ptr<Output::OutputBase>                        out_;
    Scripts                                                   &scripts_;
    Input::Program                                             prg_;
    Defines                                                    defs_;
    std::unique_ptr<Backend>                                   aspif_bck_;
    std::unique_ptr<Input::NongroundProgramBuilder>            pb_;
    std::unique_ptr<Input::NonGroundParser>                    parser_;
    USolveEventHandler                                         eventHandler_;
    Clasp::ClaspFacade                                        *clasp_                 = nullptr;
    Clasp::Cli::ClaspCliConfig                                &claspConfig_;
    PostGroundFunc                                             pgf_;
    PreSolveFunc                                               psf_;
    std::unique_ptr<Potassco::TheoryData>                      data_;
    std::vector<UProp>                                         props_;
    std::vector<Potassco::AbstractHeuristic*>                  heus_;
    std::vector<std::unique_ptr<Clasp::ClingoPropagatorInit>>  propagators_;
    std::vector<Symbol>                                        added_atoms_;
    std::unordered_set<Potassco::Atom_t>                       added_facts_;
    ClingoPropagatorLock                                       propLock_;
    Logger                                                     logger_;
    TheoryOutput                                               theory_;
    Backend                                                   *backend_               = nullptr;
    std::unique_ptr<Ground::Program>                           backend_prg_;
    UserStatistics                                             step_stats_;
    UserStatistics                                             accu_stats_;
    bool                                                       enableEnumAssupmption_ = true;
    bool                                                       enableCleanup_         = true;
    bool                                                       clingoMode_;
    bool                                                       verbose_               = false;
    bool                                                       parsed_                = false;
    bool                                                       configUpdate_          = false;
    bool                                                       grounded_              = false;
    bool                                                       initialized_           = false;
    bool                                                       incmode_               = false;
    bool                                                       canClean_              = false;

};

// {{{1 declaration of ClingoModel

class ClingoModel : public Model {
public:
    ClingoModel(ClingoControl &ctl, Clasp::Model const *model = nullptr)
    : ctl_(ctl), model_(model) { }
    void reset(Clasp::Model const &m) { model_ = &m; }
    bool contains(Symbol atom) const override {
        auto atm = out().find(atom);
        return atm.second && atm.first->hasUid() && model_->isTrue(lp().getLiteral(atm.first->uid()));
    }
    SymSpan atoms(unsigned atomset) const override {
        atms_ = out().atoms(atomset, [this](unsigned uid) { return model_->isTrue(lp().getLiteral(uid)); });
        if (atomset & clingo_show_type_theory) {
            ctl_.theory_.copy_symbols(atms_);
        }
        return Potassco::toSpan(atms_);
    }
    Int64Vec optimization() const override {
        return model_->costs ? Int64Vec(model_->costs->begin(), model_->costs->end()) : Int64Vec();
    }
    std::vector<Potassco::Weight_t> priorities() const override {
        std::vector<Potassco::Weight_t> ret;
        auto const *mini = model_->ctx != nullptr ? model_->ctx->minimizer() : nullptr;
        if (mini != nullptr) {
            ret.assign(mini->prios.begin(), mini->prios.end());
        }
        return ret;
    }
    void addClause(Potassco::LitSpan const &lits) const override {
        Clasp::LitVec claspLits;
        for (auto &x : lits) {
            auto lit = lp().getLiteral(x);
            if (lit != Clasp::lit_true()) {
                claspLits.push_back(lit);
            }
            else { return; }
        }
        claspLits.push_back(~ctx().stepLiteral());
        std::sort(claspLits.begin(), claspLits.end());
        claspLits.erase(std::unique(claspLits.begin(), claspLits.end()), claspLits.end());
        auto it = std::adjacent_find(claspLits.begin(), claspLits.end(), [](Clasp::Literal const &a, Clasp::Literal const &b) {
            return a.var() == b.var();
        });
        if (it == claspLits.end()) {
            model_->ctx->commitClause(claspLits);
        }
    }
    Gringo::SymbolicAtoms const &getDomain() const override {
        return ctl_.getDomain();
    }
    bool isTrue(Potassco::Lit_t lit) const override {
      return model_->isTrue(lp().getLiteral(lit));
    }
    ConsequenceType isConsequence(Potassco::Lit_t literal) const override {
      if (model_->isDef(lp().getLiteral(literal))) {
          return ConsequenceType::True;
      }
      if (model_->isEst(lp().getLiteral(literal))) {
          return ConsequenceType::Unknown;
      }
      return ConsequenceType::False;
    }
    ModelType type() const override {
        if (model_->type & Clasp::Model::Brave) { return ModelType::BraveConsequences; }
        if (model_->type & Clasp::Model::Cautious) { return ModelType::CautiousConsequences; }
        return ModelType::StableModel;
    }
    void add(Potassco::Span<Symbol> symbols) override { ctl_.theory_.add(symbols); }
    uint64_t number() const override { return model_->num; }
    Potassco::Id_t threadId() const override { return model_->sId; }
    bool optimality_proven() const override { return model_->opt; }
    ClingoControl &context() { return ctl_; }
private:
    Clasp::Asp::LogicProgram const &lp() const    { return *static_cast<Clasp::Asp::LogicProgram*>(ctl_.clasp_->program()); };
    Output::OutputBase const &out() const { return *ctl_.out_; };
    Clasp::SharedContext const &ctx() const       { return ctl_.clasp_->ctx; };
    ClingoControl          &ctl_;
    Clasp::Model const     *model_;
    mutable SymVec  atms_;
};

// {{{1 declaration of ClingoSolveFuture

SolveResult convert(Clasp::ClaspFacade::Result res);
class ClingoSolveFuture : public Gringo::SolveFuture {
public:
    ClingoSolveFuture(ClingoControl &ctl, Clasp::SolveMode_t mode);

    SolveResult  get()  override;
    Model const *model() override;
    Potassco::LitSpan unsatCore() override;
    bool wait(double timeout) override;
    void resume() override;
    void cancel() override;
private:
    Potassco::LitVec                core_;
    ClingoModel                     model_;
    Clasp::ClaspFacade::SolveHandle handle_;
};

// {{{1 declaration of ClingoLib

class ClingoLib : public Clasp::EventHandler, public ClingoControl {
public:
    ClingoLib(Scripts &scripts, int argc, char const * const *argv, Logger::Printer printer, unsigned messageLimit);
    ~ClingoLib() override;
protected:
    void initOptions(Potassco::ProgramOptions::OptionContext& root);
    static bool parsePositional(const std::string& s, std::string& out);
    // -------------------------------------------------------------------------------------------
    // Event handler
    void onEvent(const Clasp::Event& ev) override;
    bool onModel(const Clasp::Solver& s, const Clasp::Model& m) override;
    bool onUnsat(const Clasp::Solver&, const Clasp::Model&) override;
private:
    ClingoLib(const ClingoLib&);
    ClingoLib& operator=(const ClingoLib&);
    ClingoOptions                       grOpts_;
    Clasp::Cli::ClaspCliConfig          claspConfig_;
    Clasp::ClaspFacade                  clasp_;
};

// }}}1

} // namespace Gringo

#endif // CLINGO_CLINGOCONTROL_HH
