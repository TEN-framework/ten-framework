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

#ifndef REIFY_PROGRAM_HH
#define REIFY_PROGRAM_HH

#include <reify/util.hh>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <gringo/graph.hh>
#include <potassco/aspif.h>

namespace Reify {

using Potassco::Id_t;
using Potassco::Lit_t;
using Potassco::Atom_t;
using Potassco::Weight_t;
using Potassco::WeightLit_t;
using Potassco::Heuristic_t;
using Potassco::Head_t;
using Potassco::Value_t;
using Potassco::LitSpan;
using Potassco::IdSpan;
using Potassco::AtomSpan;
using Potassco::WeightLitSpan;
using Potassco::StringSpan;

class Reifier : public Potassco::AbstractProgram {
public:
    Reifier(std::ostream &out, bool calculateSCCs, bool reifyStep);
    virtual ~Reifier() noexcept;

    void parse(std::istream &in);

    void initProgram(bool incremental) override;
    void beginStep() override;
    void rule(Head_t ht, const AtomSpan& head, const LitSpan& body) override;
    void rule(Head_t ht, const AtomSpan& head, Weight_t bound, const WeightLitSpan& body) override;
    void minimize(Weight_t prio, const WeightLitSpan& lits) override;
    void project(const AtomSpan& atoms) override;
    void output(const StringSpan& str, const LitSpan& condition) override;
    void external(Atom_t a, Value_t v) override;
    void assume(const LitSpan& lits) override;
    void heuristic(Atom_t a, Heuristic_t t, int bias, unsigned prio, const LitSpan& condition) override;
    void acycEdge(int s, int t, const LitSpan& condition) override;

    void theoryTerm(Id_t termId, int number) override;
    void theoryTerm(Id_t termId, const StringSpan& name) override;
    void theoryTerm(Id_t termId, int cId, IdSpan const &args) override;
    void theoryElement(Id_t elementId, IdSpan const &terms, const LitSpan& cond) override;
    void theoryAtom(Id_t atomOrZero, Id_t termId, IdSpan const &elements) override;
    void theoryAtom(Id_t atomOrZero, Id_t termId, IdSpan const &elements, Id_t op, Id_t rhs) override;

    void endStep() override;

private:
    using Graph = Gringo::Graph<Atom_t>;
    template <class L>
    void calculateSCCs(const AtomSpan& head, const Potassco::Span<L>& body);
    template<typename... T>
    void printFact(char const *name, T const &...args);
    template<typename... T>
    void printStepFact(char const *name, T const &...args);
    template <class M, class T>
    size_t tuple(M &map, char const *name, T const &args);
    template <class M, class T>
    size_t tuple(M &map, char const *name, std::vector<T> &&args);
    template <class M, class T>
    size_t ordered_tuple(M &map, char const *name, T const &args);
    template <class M, class T>
    size_t ordered_tuple(M &map, char const *name, std::vector<T> &&args);
    size_t theoryTuple(IdSpan const &args);
    size_t litTuple(LitSpan const &args);
    size_t atomTuple(AtomSpan const &args);
    size_t theoryElementTuple(IdSpan const &args);
    size_t weightLitTuple(WeightLitSpan const &args);
    Graph::Node &addNode(Atom_t atom);

private:
    using WLVec = std::vector<std::pair<Lit_t, Weight_t>>;
    struct StepData {
        std::unordered_map<std::vector<Id_t>, size_t, Hash<std::vector<Id_t>>> theoryTuples;
        std::unordered_map<std::vector<Id_t>, size_t, Hash<std::vector<Id_t>>> theoryElementTuples;
        std::unordered_map<std::vector<Lit_t>, size_t, Hash<std::vector<Lit_t>>> litTuples;
        std::unordered_map<std::vector<Atom_t>, size_t, Hash<std::vector<Atom_t>>> atomTuples;
        std::unordered_map<WLVec, size_t, Hash<WLVec>> weightLitTuples;
        Graph graph_;
        std::unordered_map<Atom_t, Graph::Node*> nodes_;
    } stepData_;
    std::ostream &out_;
    size_t step_ = 0;
    bool calculateSCCs_;
    bool reifyStep_;
};

} // namespace Reify

#endif // REIFY_PROGRAM_HH

