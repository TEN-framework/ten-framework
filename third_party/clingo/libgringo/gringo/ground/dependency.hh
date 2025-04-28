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

#ifndef GRINGO_GROUND_DEPENDENCY_HH
#define GRINGO_GROUND_DEPENDENCY_HH

#include <gringo/utility.hh>
#include <gringo/graph.hh>
#include <gringo/term.hh>
#include <gringo/hash_set.hh>
#include <iterator>

namespace Gringo { namespace Ground {

// {{{ declaration of Lookup

template <class Occ>
class Lookup {
public:
    using SigLookup = std::unordered_multimap<Sig, GTerm*>;
    using Occurrences = std::unordered_multimap<GTerm*, Occ, value_hash<GTerm*>, value_equal_to<GTerm*>>;
    using iterator = typename Occurrences::iterator;
    using const_iterator = typename Occurrences::const_iterator;
    //! Adds an occurrence associated with a term.
    //! If there is already an occurrence with a structurally equivalent term,
    //! then the method returns true. Otherwise, the method returns false and
    //! the freshly inserted occurrence is associated with the (representative)
    //! term already present.
    bool add(GTerm &term, Occ occ);
    template <class Callback>
    void match(Symbol x, Callback const &c);
    template <class Callback>
    void unify(GTerm &x, Callback const &c);
    const_iterator begin() const {
        return occs.begin();
    }
    const_iterator end() const {
        return occs.end();
    }

private:
    template <class Callback>
    void unify(GTerm &x, SigLookup &y, Callback const &c);

    SigLookup terms;
    SigLookup constTerms;
    Occurrences occs;
};

// }}}
// {{{ declaration of BodyOccurrence

using LocSet = std::set<Location>;
using SigSet = ordered_set<Sig>;
using UndefVec = std::vector<std::pair<Location, Printable const *>>;

enum class OccurrenceType { POSITIVELY_STRATIFIED, STRATIFIED, UNSTRATIFIED };
template <class HeadOcc>
class BodyOccurrence {
public:
    using DefinedBy = std::vector<std::reference_wrapper<HeadOcc>>;
    BodyOccurrence() = default;
    BodyOccurrence(BodyOccurrence const &other) = default;
    BodyOccurrence(BodyOccurrence &&other) noexcept = default;
    BodyOccurrence &operator=(BodyOccurrence const &other) = default;
    BodyOccurrence &operator=(BodyOccurrence &&other) noexcept = default;
    virtual ~BodyOccurrence() noexcept = default;

    virtual UGTerm getRepr() const = 0;
    virtual bool isPositive() const = 0;
    virtual bool isNegative() const = 0;
    virtual void setType(OccurrenceType x) = 0;
    virtual OccurrenceType getType() const = 0;
    virtual DefinedBy &definedBy() = 0;
    virtual void checkDefined(LocSet &done, SigSet const &edb, UndefVec &undef) const = 0;
};

// }}}
// {{{ declaration of Dependency

template <class Stm, class HeadOcc>
struct Dependency {
    struct Node;
    using UNode = std::unique_ptr<Node>;
    using NodeVec = std::vector<Node*>;
    using Depend = std::tuple<BodyOccurrence<HeadOcc>*, NodeVec, bool>;
    using Provide = std::pair<Node*, typename NodeVec::size_type>;
    using Lookup = Ground::Lookup<Provide>;
    using G = Graph<Node*>;
    using ComponentVec = std::vector<std::pair<std::vector<Stm>, bool>>;
    struct Node {
        Node(Stm stm, bool normal) : stm(std::forward<Stm>(stm)), normal(normal) { }

        Stm stm;
        bool normal;
        std::vector<Depend> depend;
        std::vector<std::pair<HeadOcc*, UGTerm>> provide;
        typename G::Node *graphNode = nullptr;
        unsigned negSCC = 0;
        unsigned posSCC = 0;
    };

    Node &add(Stm stm, bool normal);
    void depends(Node& n, BodyOccurrence<HeadOcc> &occ, bool forceNegative = false);
    void provides(Node& n, HeadOcc &occ, UGTerm term);
    std::tuple<ComponentVec, UGTermVec, UGTermVec> analyze();

    UGTermVec terms;
    Lookup depend;
    std::vector<UNode> nodes;
};

// }}}

// {{{ definition of Lookup

template <class Occ>
bool Lookup<Occ>::add(GTerm &term, Occ occ) {
    auto it = occs.find(&term);
    if (it == occs.end()) {
        if (term.eval().first) {
            constTerms.emplace(term.sig(), &term);
        }
        else {
            terms.emplace(term.sig(), &term);
        }
        occs.emplace(&term, std::forward<Occ>(occ));
        return true;
    }
    occs.emplace_hint(it, it->first, std::forward<Occ>(occ));
    return false;
}

template <class Occ>
template <class Callback>
void Lookup<Occ>::match(Symbol x, Callback const &c) {
    if (x.type() == SymbolType::Fun) {
        auto ir(terms.equal_range(x.sig()));
        for (auto it = ir.first; it != ir.second; ++it) {
            if (it->second->match(x)) {
                auto rng(occs.equal_range(it->second));
                assert(rng.first != rng.second);
                c(rng.first, rng.second);
            }
            it->second->reset();
        }
        GValTerm y(x);
        auto rng(occs.equal_range(&y));
        if (rng.first != rng.second) {
            c(rng.first, rng.second);
        }
    }
}

template <class Occ>
template <class Callback>
void Lookup<Occ>::unify(GTerm &x, SigLookup &y, Callback const &c) {
    auto ir(y.equal_range(x.sig()));
    for (auto it = ir.first; it != ir.second; ++it) {
        if (it->second->unify(x)) {
            auto rng(occs.equal_range(it->second));
            assert(rng.first != rng.second);
            c(rng.first, rng.second);
        }
        it->second->reset();
        x.reset();
    }
}

template <class Occ>
template <class Callback>
void Lookup<Occ>::unify(GTerm &x, Callback const &c) {
    auto r = x.eval();
    if (r.first) {
        match(r.second, c);
    }
    else {
        unify(x, terms, c);
        unify(x, constTerms, c);
    }
}

// }}}
// {{{ definition of Dependency

template <class Stm, class HeadOcc>
typename Dependency<Stm, HeadOcc>::Node &Dependency<Stm, HeadOcc>::add(Stm stm, bool normal) {
    nodes.emplace_back(gringo_make_unique<Node>(std::forward<Stm>(stm), normal));
    return *nodes.back();
}

template <class Stm, class HeadOcc>
void Dependency<Stm, HeadOcc>::depends(Node& n, BodyOccurrence<HeadOcc> &occ, bool forceNegative) {
    //std::cerr << *n.stm << " depends on " << *occ.getRepr() << (occ.isPositive() ? " pos" : "")  << (forceNegative || occ.isNegative() ? " neg" : "") << std::endl;
    terms.emplace_back(occ.getRepr());
    depend.add(*terms.back(), Provide(&n, n.depend.size()));
    n.depend.emplace_back(&occ, NodeVec(), forceNegative);
    occ.definedBy().clear();
}

template <class Stm, class HeadOcc>
void Dependency<Stm, HeadOcc>::provides(Node& n, HeadOcc& occ, UGTerm term) {
    //std::cerr << *n.stm << " provides   " << *term << std::endl;
    n.provide.emplace_back(&occ, std::move(term));
}

template <class Stm, class HeadOcc>
std::tuple<typename Dependency<Stm, HeadOcc>::ComponentVec, UGTermVec, UGTermVec> Dependency<Stm, HeadOcc>::analyze() {
    // initialize nodes
    for (auto &node : nodes) {
        for (auto &x : node->provide) {
            auto f = [&node, &x](typename Lookup::iterator begin, typename Lookup::iterator end) -> void {
                for (auto it = begin; it != end; ++it) {
                    auto &dep(it->second.first->depend[it->second.second]);
                    std::get<1>(dep).emplace_back(node.get());
                    std::get<0>(dep)->definedBy().emplace_back(*x.first);
                }
            };
            depend.unify(*x.second, f);
        }
    }
    // build dependency graph
    G g;
    for (auto &x : nodes) {
        x->graphNode = &g.insertNode(x.get());
    }
    for (auto &x : nodes) {
        for (auto &y : x->depend) {
            for (auto &z : std::get<1>(y)) {
                x->graphNode->insertEdge(*z->graphNode);
            }
        }
    }
    std::vector<bool> positive;
    ComponentVec components;
    ordered_set<UGTerm, mix_value_hash<UGTerm>, value_equal_to<UGTerm>> phead;
    ordered_set<UGTerm, mix_value_hash<UGTerm>, value_equal_to<UGTerm>> nhead;
    positive.push_back(true);
    for (auto &scc : g.tarjan()) {
        // dependency analysis
        auto negSCC = numeric_cast<unsigned>(positive.size());
        for (auto &graphNode : scc) {
            graphNode->data->negSCC = negSCC;
        }
        positive.push_back(true);
        for (auto &graphNode : scc) {
            positive.back() = positive.back() && graphNode->data->normal;
            for (auto &x : graphNode->data->depend) {
                for (auto &y : std::get<1>(x)) {
                    positive.back() = positive.back() && (y->negSCC != negSCC ? positive[y->negSCC] : std::get<0>(x)->isPositive() && !std::get<2>(x));
                }
            }
        }
        // build positive dependency graph
        G gg;
        for (auto &graphNode : scc) {
            graphNode->data->graphNode = &gg.insertNode(graphNode->data);
        }
        for (auto &graphNode : scc) {
            for (auto &x : graphNode->data->depend) {
                if (!std::get<0>(x)->isNegative()) {
                    for (auto &y : std::get<1>(x)) {
                        if (y->negSCC == negSCC) {
                            graphNode->data->graphNode->insertEdge(*y->graphNode);
                        }
                    }
                }
            }
        }
        unsigned posSCC = 0;
        auto posSccs(gg.tarjan());
        for (auto &scc : posSccs) {
            // positive dependency analysis
            for (auto &graphNode : scc) {
                graphNode->data->posSCC = posSCC;
            }
            posSCC++;
        }
        posSCC = 0;
        for (auto &scc : posSccs) {
            components.emplace_back();
            for (auto &graphNode : scc) {
                for (auto &x : graphNode->data->depend) {
                    OccurrenceType t = OccurrenceType::POSITIVELY_STRATIFIED;
                    for (auto &y : std::get<1>(x)) {
                        if (y->negSCC != negSCC) {
                            if (t == OccurrenceType::POSITIVELY_STRATIFIED && !positive[y->negSCC]) {
                                t = OccurrenceType::STRATIFIED;
                            }
                        }
                        else if (y->posSCC < posSCC) {
                            assert(y->negSCC == negSCC);
                            if (t == OccurrenceType::POSITIVELY_STRATIFIED)  {
                                t = OccurrenceType::STRATIFIED;
                            }
                        }
                        else {
                            t = OccurrenceType::UNSTRATIFIED;
                            break;
                        }
                    }
                    std::get<0>(x)->setType(t);
                }
                for (auto &x : graphNode->data->provide) {
                    if (std::strncmp("#", x.second->sig().name().c_str(), 1) != 0) {
                        if (positive.back()) {
                            phead.insert(std::move(x.second));
                        }
                        else {
                            nhead.insert(std::move(x.second));
                        }
                    }
                }
                components.back().second = positive.back();
                components.back().first.emplace_back(std::move(graphNode->data->stm));
            }
            posSCC++;
        }
    }

    // NOTE: This hijacks the vector from the ordered_set This is safe because
    // the destructor of the ordered_set just uses default generated
    // destructors. Maybe open a PR to request something like a release
    // function for the ordered set.
    UGTermVec phead_vec = std::move(const_cast<std::vector<UGTerm>&>(phead.values_container())); // NOLINT
    phead_vec.erase(
        std::remove_if(
            phead_vec.begin(),
            phead_vec.end(),
            [&nhead](UGTerm const &term) { return nhead.find(term) != nhead.end(); }),
        phead_vec.end());
    UGTermVec nhead_vec = std::move(const_cast<std::vector<UGTerm>&>(nhead.values_container())); // NOLINT
    return std::make_tuple( std::move(components), std::move(phead_vec), std::move(nhead_vec) );
}
// }}}

} } // namespace Ground Gringo

#endif // GRINGO_GROUND_DEPENDENCY_HH
