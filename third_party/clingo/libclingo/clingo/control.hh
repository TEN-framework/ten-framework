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

#ifndef CLINGO_CONTROL_HH
#define CLINGO_CONTROL_HH

#include <gringo/symbol.hh>
#include <gringo/types.hh>
#include <gringo/locatable.hh>
#include <gringo/backend.hh>
#include <gringo/logger.hh>
#include <gringo/output/literals.hh>
#include <potassco/clingo.h>
#include <clingo.h>

namespace Gringo {

char const *clingo_version_string() noexcept;

// {{{1 declaration of SymbolicAtoms

using SymbolicAtoms = clingo_symbolic_atoms;
using SymbolicAtomIter = clingo_symbolic_atom_iterator_t;

} // namespace Gringo

struct clingo_symbolic_atoms {
    virtual Gringo::Symbol atom(Gringo::SymbolicAtomIter it) const = 0;
    virtual Potassco::Lit_t literal(Gringo::SymbolicAtomIter it) const = 0;
    virtual bool fact(Gringo::SymbolicAtomIter it) const = 0;
    virtual bool external(Gringo::SymbolicAtomIter it) const = 0;
    virtual Gringo::SymbolicAtomIter next(Gringo::SymbolicAtomIter it) const = 0;
    virtual bool valid(Gringo::SymbolicAtomIter it) const = 0;
    virtual Gringo::SymbolicAtomIter begin(Gringo::Sig sig) const = 0;
    virtual Gringo::SymbolicAtomIter begin() const = 0;
    virtual Gringo::SymbolicAtomIter lookup(Gringo::Symbol atom) const = 0;
    virtual bool eq(Gringo::SymbolicAtomIter it, Gringo::SymbolicAtomIter jt) const = 0;
    virtual Gringo::SymbolicAtomIter end() const = 0;
    virtual std::vector<Gringo::Sig> signatures() const = 0;
    virtual size_t length() const = 0;
    virtual ~clingo_symbolic_atoms() noexcept = default;
};

namespace Gringo {

// {{{1 declaration of SolveResult

class SolveResult {
public:
    enum Satisfiabily : unsigned { Unknown=0, Satisfiable=1, Unsatisfiable=2 };
    SolveResult(Satisfiabily status, bool exhausted, bool interrupted)
    : repr_(static_cast<unsigned>(status) | (exhausted << 2) | (interrupted << 3)) { }
    SolveResult(unsigned repr)
    : repr_(repr) { }
    Satisfiabily satisfiable() const { return static_cast<Satisfiabily>(repr_ & 3); }
    bool exhausted() const { return (repr_ >> 2) & 1; }
    bool interrupted() const { return (repr_ >> 3) & 1; }
    operator unsigned() const { return repr_; }
private:
    unsigned repr_;
};

// {{{1 declaration of Model

enum class ModelType : clingo_model_type_t {
    StableModel = clingo_model_type_stable_model,
    BraveConsequences = clingo_model_type_brave_consequences,
    CautiousConsequences = clingo_model_type_cautious_consequences
};
using Model = clingo_model;
using Int64Vec = std::vector<Potassco::Weight_t>;

} // namespace Gringo

using ShowType = clingo_show_type_bitset_t;
enum class ConsequenceType {
    True = clingo_consequence_true,
    False = clingo_consequence_false,
    Unknown = clingo_consequence_unknown,
};

struct clingo_model {
    using LitVec = std::vector<std::pair<Gringo::Symbol, bool>>;
    virtual bool contains(Gringo::Symbol atom) const = 0;
    virtual Gringo::SymSpan atoms(ShowType showset) const = 0;
    virtual Gringo::Int64Vec optimization() const = 0;
    virtual std::vector<int> priorities() const = 0;
    virtual bool optimality_proven() const = 0;
    virtual void addClause(Potassco::LitSpan const &lits) const = 0;
    virtual uint64_t number() const = 0;
    virtual Potassco::Id_t threadId() const = 0;
    virtual Gringo::ModelType type() const = 0;
    virtual bool isTrue(Potassco::Lit_t literal) const = 0;
    virtual ConsequenceType isConsequence(Potassco::Lit_t literal) const = 0;
    virtual Gringo::SymbolicAtoms const &getDomain() const = 0;
    virtual void add(Potassco::Span<Gringo::Symbol> symbols) = 0;
    virtual ~clingo_model() { }
};

namespace Gringo {

// {{{1 declaration of SolveFuture

struct SolveEventHandler {
    virtual bool on_model(Model &model);
    virtual bool on_unsat(Potassco::Span<int64_t> optimization);
    virtual void on_finish(SolveResult ret, Potassco::AbstractStatistics *step, Potassco::AbstractStatistics *accu);
    virtual ~SolveEventHandler() = default;
};
using USolveEventHandler = std::unique_ptr<SolveEventHandler>;
inline bool SolveEventHandler::on_model(Model &) { return true; }
inline bool SolveEventHandler::on_unsat(Potassco::Span<int64_t> optimization) { return true; }
inline void SolveEventHandler::on_finish(SolveResult, Potassco::AbstractStatistics *, Potassco::AbstractStatistics *) { }

struct SolveFuture {
    virtual SolveResult get() = 0;
    virtual Model const *model() = 0;
    virtual Potassco::LitSpan unsatCore() = 0;
    virtual bool wait(double timeout) = 0;
    virtual void cancel() = 0;
    virtual void resume() = 0;
    virtual ~SolveFuture() { }
};
using USolveFuture = std::unique_ptr<SolveFuture>;

struct DefaultSolveFuture : SolveFuture {
    DefaultSolveFuture(USolveEventHandler cb) : cb_(std::move(cb)) { }
    SolveResult get() override { resume(); return {SolveResult::Unknown, false, false}; }
    Model const *model() override { resume(); return nullptr; }
    Potassco::LitSpan unsatCore() override {
        throw std::runtime_error("no core available");
    }
    bool wait(double) override { resume(); return true; }
    void cancel() override { resume(); }
    void resume() override {
        if (!done_) {
            done_ = true;
            if (cb_) { cb_->on_finish({SolveResult::Unknown, false, false}, nullptr, nullptr); }
        }
    }

    ~DefaultSolveFuture() override { resume(); }
private:
    USolveEventHandler cb_;
    bool done_ = false;
};

// {{{1 declaration of ConfigProxy

struct ConfigProxy {
    virtual bool hasSubKey(unsigned key, char const *name) const = 0;
    virtual unsigned getSubKey(unsigned key, char const *name) const = 0;
    virtual unsigned getArrKey(unsigned key, unsigned idx) const = 0;
    virtual void getKeyInfo(unsigned key, int* nSubkeys = 0, int* arrLen = 0, const char** help = 0, int* nValues = 0) const = 0;
    virtual const char* getSubKeyName(unsigned key, unsigned idx) const = 0;
    virtual bool getKeyValue(unsigned key, std::string &value) const = 0;
    virtual void setKeyValue(unsigned key, const char *val) = 0;
    virtual unsigned getRootKey() const = 0;
    virtual ~ConfigProxy() { }
};

// {{{1 declaration of Propagator

using PropagateControl = clingo_propagate_control;
using PropagateInit = clingo_propagate_init;

} // namespace Gringo

struct clingo_propagate_init {
    virtual Potassco::Lit_t addLiteral(bool freeze) = 0;
    virtual bool addClause(Potassco::LitSpan lits) = 0;
    virtual bool addWeightConstraint(Potassco::Lit_t lit, Potassco::WeightLitSpan lits, Potassco::Weight_t bound, int type, bool eq) = 0;
    virtual void addMinimize(Potassco::Lit_t literal, Potassco::Weight_t weight, Potassco::Weight_t priority) = 0;
    virtual bool propagate() = 0;
    virtual Gringo::Output::DomainData const &theory() const = 0;
    virtual Gringo::SymbolicAtoms const &getDomain() const = 0;
    virtual Gringo::Lit_t mapLit(Gringo::Lit_t lit) const = 0;
    virtual void addWatch(Gringo::Lit_t lit) = 0;
    virtual void addWatch(uint32_t solverId, Gringo::Lit_t lit) = 0;
    virtual void removeWatch(Gringo::Lit_t lit) = 0;
    virtual void removeWatch(uint32_t solverId, Gringo::Lit_t lit) = 0;
    virtual void freezeLiteral(Gringo::Lit_t lit) = 0;
    virtual void enableHistory(bool b) = 0;
    virtual Potassco::AbstractAssignment const &assignment() const = 0;
    virtual int threads() const = 0;
    virtual clingo_propagator_check_mode_t getCheckMode() const = 0;
    virtual clingo_propagator_undo_mode_t getUndoMode() const = 0;
    virtual void setCheckMode(clingo_propagator_check_mode_t checkMode) = 0;
    virtual void setUndoMode(clingo_propagator_undo_mode_t undoMode) = 0;
    virtual ~clingo_propagate_init() noexcept = default;
};

namespace Gringo {

struct Propagator : Potassco::AbstractPropagator, Potassco::AbstractHeuristic {
    virtual ~Propagator() noexcept = default;
    virtual void init(Gringo::PropagateInit &init) = 0;
    virtual bool hasHeuristic() const = 0;
};
using UProp = std::unique_ptr<Propagator>;

// {{{1 declaration of Control

using StringVec = std::vector<String>;
using Control = clingo_control;

} // namespace Gringo

struct clingo_control {
    using Assumptions = Potassco::LitSpan;
    using GroundVec = std::vector<std::pair<Gringo::String, Gringo::SymVec>>;
    using NewControlFunc = Gringo::Control* (*)(int, char const **);
    using FreeControlFunc = void (*)(Gringo::Control *);

    virtual Gringo::ConfigProxy &getConf() = 0;
    virtual Gringo::SymbolicAtoms const &getDomain() const = 0;

    virtual void ground(GroundVec const &vec, Gringo::Context *context) = 0;
    virtual Gringo::USolveFuture solve(Assumptions assumptions, clingo_solve_mode_bitset_t mode, Gringo::USolveEventHandler cb = nullptr) = 0;
    virtual void interrupt() = 0;
    virtual void *claspFacade() = 0;
    virtual void add(std::string const &name, Gringo::StringVec const &params, std::string const &part) = 0;
    virtual void load(std::string const &filename) = 0;
    virtual Gringo::Symbol getConst(std::string const &name) const = 0;
    virtual bool blocked() = 0;
    virtual void assignExternal(Potassco::Atom_t ext, Potassco::Value_t val) = 0;
    virtual bool isConflicting() const noexcept = 0;
    virtual Potassco::AbstractStatistics const *statistics() const = 0;
    virtual void useEnumAssumption(bool enable) = 0;
    virtual bool useEnumAssumption() const = 0;
    virtual void cleanup() = 0;
    virtual void enableCleanup(bool enable) = 0;
    virtual bool enableCleanup() const = 0;
    virtual Gringo::Output::DomainData const &theory() const = 0;
    virtual void registerPropagator(Gringo::UProp p, bool sequential) = 0;
    virtual void registerObserver(Gringo::UBackend program, bool replace) = 0;
    virtual Potassco::Atom_t addProgramAtom() = 0;
    virtual bool beginAddBackend() = 0;
    virtual Gringo::Output::TheoryData &theoryData() = 0;
    virtual Gringo::Id_t addAtom(Gringo::Symbol sym) = 0;
    virtual void addFact(Potassco::Atom_t uid) = 0;
    virtual Gringo::Backend *getBackend() = 0;
    virtual Gringo::Backend &getASPIFBackend() = 0;
    virtual void endAddBackend() = 0;
    virtual Gringo::Logger &logger() = 0;
    virtual void beginAdd() = 0;
    virtual void add(clingo_ast_t const &ast) = 0;
    virtual void endAdd() = 0;
    virtual ~clingo_control() noexcept = default;
};

namespace Gringo {

// }}}1

} // namespace Gringo

#endif // CLINGO_CONTROL_HH
