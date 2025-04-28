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

#ifndef GRINGO_SAFETYCHECK_HH
#define GRINGO_SAFETYCHECK_HH

#include <vector>
#include <forward_list>
#include <unordered_map>

namespace Gringo {

// {{{ declaration of SafetyChecker<Ent, Var>

template <class Var, class Ent>
struct SafetyChecker {
    struct EntNode;
    struct VarNode {
        template <class... T>
        VarNode(T&&... args);
        bool bound = false;
        std::vector<EntNode*> provides;
        Var data;
    };

    struct EntNode {
        template <class... T>
        EntNode(T&&... args);
        std::vector<VarNode*> provides;
        unsigned depends = 0;
        Ent data;
    };

    using EntVec = std::vector<EntNode*>;
    using VarVec = std::vector<VarNode*>;

    SafetyChecker() = default;
    SafetyChecker(SafetyChecker const &other) = delete;
    SafetyChecker(SafetyChecker &&other) noexcept = default;
    SafetyChecker &operator=(SafetyChecker const &other) = delete;
    SafetyChecker &operator=(SafetyChecker &&other) noexcept = default;
    ~SafetyChecker() noexcept = default;

    template <class... T>
    VarNode &insertVar(T&&... args);
    template <class... T>
    EntNode &insertEnt(T&&... args);

    //! Edge (x, y) implies that y depends on x being bound.
    //! E.g., variable x occurs on the rhs of assignment y.
    void insertEdge(VarNode &x, EntNode &y);
    //! Edge (x, y) implies that y is bound by x.
    //! E.g., variable y occurs in a positive body element x.
    void insertEdge(EntNode &x, VarNode &y);

    void init(EntVec &open);
    void propagate(EntNode *x, EntVec &open, VarVec *bound = nullptr);
    template <class Pred = std::less<Ent>>
    EntVec order(Pred pred = Pred());
    VarVec open();

    std::forward_list<EntNode> entNodes_;
    std::forward_list<VarNode> varNodes_;
};

// }}}

// {{{ definition of SafetyChecker<Var, Ent>::VarNode

template <class Var, class Ent>
template <class... T>
SafetyChecker<Var, Ent>::VarNode::VarNode(T&&... args)
    : data(std::forward<T>(args)...)
{
}

// }}}
// {{{ definition of SafetyChecker<Var, Ent>::EntNode

template <class Var, class Ent>
template <class... T>
SafetyChecker<Var, Ent>::EntNode::EntNode(T&&... args)
    : data(std::forward<T>(args)...)
{
}

// }}}
// {{{ definition of SafetyChecker<Var, Ent>

template <class Var, class Ent>
void SafetyChecker<Var, Ent>::insertEdge(VarNode &x, EntNode &y) {
    x.provides.emplace_back(&y);
    y.depends++;
}

template <class Var, class Ent>
void SafetyChecker<Var, Ent>::insertEdge(EntNode &x, VarNode &y) {
    x.provides.emplace_back(&y);
}

template <class Var, class Ent>
template <class... T>
typename SafetyChecker<Var, Ent>::VarNode &SafetyChecker<Var, Ent>::insertVar(T&&... args) {
    varNodes_.emplace_front(std::forward<T>(args)...);
    return varNodes_.front();
}

template <class Var, class Ent>
template <class... T>
typename SafetyChecker<Var, Ent>::EntNode &SafetyChecker<Var, Ent>::insertEnt(T&&... args) {
    entNodes_.emplace_front(std::forward<T>(args)...);
    return entNodes_.front();
}

template <class Var, class Ent>
template <class Pred>
typename SafetyChecker<Var, Ent>::EntVec SafetyChecker<Var, Ent>::order(Pred pred) {
    EntVec open;
    init(open);
    std::vector<EntNode*> done;
    while (!open.empty()) {
        for (auto it = open.begin(), end = open.end() - 1; it != end; ++it) {
            if (pred((*it)->data, open.back()->data)) { std::swap(open.back(), *it); }
        }
        auto x = open.back();
        open.pop_back();
        propagate(x, open);
        done.emplace_back(x);
    }
    return done;
}

template <class Var, class Ent>
void SafetyChecker<Var, Ent>::init(EntVec &open) {
    for (auto &x : entNodes_) {
        if (!x.depends) { open.emplace_back(&x); }
    }
}

template <class Var, class Ent>
void SafetyChecker<Var, Ent>::propagate(EntNode *x, EntVec &open, VarVec *bound) {
    for (auto &y : x->provides) {
        if (!y->bound) {
            y->bound = true;
            if (bound) { bound->emplace_back(y); }
            for (auto &z : y->provides) {
                if (!--z->depends) { open.emplace_back(z); }
            }
        }
    }
}

template <class Var, class Ent>
typename SafetyChecker<Var, Ent>::VarVec SafetyChecker<Var, Ent>::open() {
    VarVec open;
    for (auto &x : varNodes_) {
        if (!x.bound) { open.emplace_back(&x); }
    }
    return open;
}

// }}}

} // namespace Gringo

#endif // GRINGO_SAFETYCHECK_HH
