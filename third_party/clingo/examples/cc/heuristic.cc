// This example is the C++ implementation of ./examples/clingo/heuristic/. To
// run this example, use the instances provided with the other example.

#include <clingo.hh>
#include <unordered_map>
#include <limits>

namespace Detail {
    template <typename H, typename I>
    void heap_rise_(H &h, I x) {
        if (x > 0) {
            auto y = (x - 1) / 2;
            if (h.priority(y) < h.priority(x)) {
                h.swap(x, y);
                heap_rise_(h, y);
            }
        }
    }
    template <typename H, typename I>
    void heap_sink_(H &h, I i) {
        auto j = i, l = 2 * j + 1, r = l + 1, s = h.size();
        if (l < s && h.priority(j) < h.priority(l)) {
            j = l;
        }
        if (r < s && h.priority(j) < h.priority(r)) {
            j = r;
        }
        if (i != j) {
            h.swap(i, j);
            heap_sink_(h, j);
        }
    }
}

template <typename H>
void heap_init(H &h) {
    for (auto s = h.size(), i = s / 2; i > 0; --i) {
        Detail::heap_sink_(h, i - 1);
    }
}

template <typename H, typename I>
void heap_remove(H &h, I x) {
    auto y = h.size() - 1;
    if (x != y) {
        auto px = h.priority(x), py = h.priority(y);
        h.swap(x, y);
        h.pop();
        if (px < py)      { Detail::heap_rise_(h, x); }
        else if (py < px) { Detail::heap_sink_(h, x); }
    }
}

template <typename H, typename I, typename P>
void heap_update(H &h, I x, P p) {
    auto o = h.priority(x);
    h.priority(x) = p;
    if (o < p)      { Detail::heap_rise_(h, x); }
    else if (p < o) { Detail::heap_sink_(h, x); }
}

template <typename H, typename... V>
void heap_insert(H &h, V &&...x) {
    h.push(std::forward<V>(x)...);
    Detail::heap_rise_(h, h.size() - 1);
}

struct Entry {
    int degree;
    unsigned position;
};
using EntryMap = std::unordered_map<Clingo::Symbol, Entry>;

class Heap {
public:
    int &priority(unsigned x) {
        assert(data[x]->second.position == x);
        return data[x]->second.degree;
    }
    unsigned size() const {
        return data.size();
    }
    void swap(unsigned x, unsigned y) {
        using std::swap;
        swap(data[x], data[y]);
        data[x]->second.position = x;
        data[y]->second.position = y;
    }
    void pop() {
        data.back()->second.position = std::numeric_limits<unsigned>::max();
        data.pop_back();
    }
    void push(EntryMap::iterator x) {
        data.emplace_back(x);
        data.back()->second.position = data.size() - 1;
    }

    EntryMap::iterator front() const {
        return data.front();
    }

private:
    std::vector<EntryMap::iterator> data;
};

class State {
public:
    void add(Clingo::Symbol sym, int x) {
        auto ret = entries_.emplace(sym, Entry{0, 0});
        if (ret.second) { heap_.push(ret.first); }
        ret.first->second.degree += x;
    }
    void init() {
        heap_init(heap_);
    }
    void update(Clingo::Symbol sym, int x) {
        auto &e = entries_.at(sym);
        if (member(e)) { heap_update(heap_, e.position, e.degree + x); }
        else           { e.degree += x; }
    }
    void remove(Clingo::Symbol sym) {
        auto e = entries_.at(sym);
        if (member(e)) { heap_remove(heap_, e.position); }
    }
    void insert(Clingo::Symbol sym) {
        auto e = entries_.find(sym);
        if (!member(e->second)) { heap_insert(heap_, e); }
    }
    bool empty() const {
        return heap_.size() == 0;
    }
    Clingo::Symbol peak() const {
        return heap_.front()->first;
    }

private:
    bool member(Entry &e) {
        return e.position < std::numeric_limits<unsigned>::max();
    }

private:
    EntryMap entries_;
    Heap heap_;
};

class ColoringHeuristic : public Clingo::Heuristic {
public:
    void init(Clingo::PropagateInit &init) override {
        states_.resize(init.number_of_threads());
        for (auto &&a : init.symbolic_atoms()) {
            if (a.match("edge", 2)) {
                auto u = a.symbol().arguments()[0];
                auto v = a.symbol().arguments()[1];
                graph_[u].second.emplace_back(v);
                graph_[v].second.emplace_back(u);
                for (auto &&state : states_) {
                    state.add(u, 1);
                    state.add(v, 1);
                }
            }
            else if (a.match("assign", 2)) {
                auto u = a.symbol().arguments()[0];
                auto l = init.solver_literal(a.literal());
                init.add_watch(l);
                assign_[l].emplace_back(u);
                graph_[u].first.emplace_back(l);
                for (auto &&state : states_) {
                    state.add(u, 0);
                }
            }
        }
        for (auto &&state : states_) {
            state.init();
        }
    }

    void propagate(Clingo::PropagateControl &ctl, Clingo::LiteralSpan changes) override {
        auto &&state = states_[ctl.thread_id()];
        for (auto &&l : changes) {
            for (auto &&u : assign_[l]) {
                state.remove(u);
                for (auto &&v : graph_[u].second) {
                    state.update(v, -1);
                }
            }
        }
    }

    void undo(Clingo::PropagateControl const &ctl, Clingo::LiteralSpan changes) noexcept override {
        auto &&state = states_[ctl.thread_id()];
        for (auto &&l : changes) {
            for (auto &&u : assign_[l]) {
                state.insert(u);
                for (auto &&v : graph_[u].second) {
                    state.update(v, 1);
                }
            }
        }
    }

    Clingo::literal_t decide(Clingo::id_t thread_id, Clingo::Assignment const &assign, Clingo::literal_t) override {
        auto &&state = states_[thread_id];
        if (!state.empty()) {
            auto sym = state.peak();
            for (auto &&l : graph_[sym].first) {
                if (assign.truth_value(l) == Clingo::TruthValue::Free) {
                    return l;
                }
            }
        }
        return 0;
    }

private:
    std::unordered_map<Clingo::Symbol, std::pair<std::vector<Clingo::literal_t>, Clingo::SymbolVector>> graph_;
    std::unordered_map<Clingo::literal_t, Clingo::SymbolVector> assign_;
    std::vector<State> states_;
};

class GraphColorizer : Clingo::Application {
public:
    static int run(int argc, char const **argv) {
        GraphColorizer gc;
        return Clingo::clingo_main(gc, {argv+1, argv+argc});
    }
private:
    char const *program_name() const noexcept override {
        return "colorizer";
    };
    char const *version() const noexcept override { return "1.0"; }
    void main(Clingo::Control &ctl, Clingo::StringSpan files) override {
        for (auto &&file : files) {
            ctl.load(file);
        }
        if (files.empty()) { ctl.load("-"); }
        ctl.add("base", {},
            "1 { assign(U,C) : color(C) } 1 :- vertex(U).\n"
            " :- edge(U,V), assign(U,C), assign(V,C).\n");
        ColoringHeuristic heu;
        ctl.register_propagator(heu);
        ctl.ground({{"base", {}}});
        ctl.solve(Clingo::LiteralSpan{}, nullptr, false, false).get();
    }
};

int main(int argc, char const **argv) {
    return GraphColorizer::run(argc, argv);
}
