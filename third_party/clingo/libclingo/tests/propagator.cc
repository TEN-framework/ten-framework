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

#include "tests.hh"
#include <unordered_map>
#include <map>
#include <unordered_set>
#include <cassert>
#include <mutex>
#include <condition_variable>
#include <set>

#include <iostream>

namespace Clingo { namespace Test {

template <class K, class V>
std::ostream &operator<<(std::ostream &out, std::unordered_map<K, V> const &map);
template <class T>
std::ostream &operator<<(std::ostream &out, std::vector<T> const &vec);
template <class K, class V>
std::ostream &operator<<(std::ostream &out, std::pair<K, V> const &pair);

template <class T>
std::ostream &operator<<(std::ostream &out, std::vector<T> const &vec) {
    out << "{";
    for (auto &x : vec) { out << " " << x; }
    out << " }";
    return out;
}

template <class K, class V>
std::ostream &operator<<(std::ostream &out, std::unordered_map<K, V> const &map) {
    using T = std::pair<K, V>;
    std::vector<T> vec;
    vec.assign(map.begin(), map.end());
    std::sort(vec.begin(), vec.end(), [](T const &a, T const &b) { return a.first < b.first; });
    out << vec;
    return out;
}

template <class K, class V>
std::ostream &operator<<(std::ostream &out, std::pair<K, V> const &pair) {
    out << "( " << pair.first << " " << pair.second << " )";
    return out;
}

class SequenceMiningPropagator : public Propagator {
private:
    // {{{2 data

    struct TrailItem {
        TrailItem(uint32_t dl, int si)
        : decision_level(dl)
        , stack_index(si) { }
        uint32_t decision_level;
        int stack_index;
    };
    struct PatternItem {
        PatternItem()
        : PatternItem(0, 0) { }
        PatternItem(int lit, int idx)
        : literal(lit)
        , item_index(idx) { }
        operator bool() const { return literal != 0; }
        literal_t literal;
        int item_index;
    };
    enum class IndexType { SequenceIndex, PatternIndex };
    class StackItem {
    public:
        StackItem(IndexType type, int idx)
        : pattern_or_sequence_index_(type == IndexType::PatternIndex ? idx : -idx - 1) { }
        IndexType type() const { return pattern_or_sequence_index_ >= 0 ? IndexType::PatternIndex : IndexType::SequenceIndex; }
        int pattern_index() const { return pattern_or_sequence_index_; }
        int sequence_index() const { return -pattern_or_sequence_index_ - 1; }
    private:
        int pattern_or_sequence_index_;
    };
    struct State {
        State(int pat_len, int seq_len) {
            pattern.resize(pat_len, {0, 0});
            seq_active.resize(seq_len, true);
        }
        std::vector<bool> seq_active;
        std::vector<StackItem> stack;
        std::vector<TrailItem> trail;
        std::vector<PatternItem> pattern;
        int pattern_assigned = 0;
    };
    struct SequenceAtoms {
        SequenceAtoms()
        : literal(0) { }
        int literal;
        std::vector<int> items;
        friend std::ostream &operator<<(std::ostream &out, SequenceAtoms const &a) {
            out << "(" << a.literal << " " << a.items << ")";
            return out;
        }
    };
    struct PatternAtom {
        PatternAtom(int pidx, int iidx)
        : pattern_index(pidx)
        , item_index(iidx) { }
        int pattern_index;
        int item_index;
        friend std::ostream &operator<<(std::ostream &out, PatternAtom const &a) {
            out << "(" << a.pattern_index << " " << a.item_index << ")";
            return out;
        }
    };

    std::vector<State> states_;
    std::vector<SequenceAtoms> sequence_atoms_;
    std::unordered_map<literal_t, std::vector<PatternAtom>> pattern_atoms_;
    std::vector<std::vector<int>> occurrence_list_;
    std::unordered_map<std::string, int> item_map_;
    int pattern_length_ = 0;

    // {{{2 initialization

    template <class T>
    void reserve(T &vec, size_t s, typename T::value_type v = typename T::value_type()) {
        if (s >= vec.size()) { vec.resize(s + 1, v); }
    }

    int map_item(std::string &&item) {
        auto &ret = *item_map_.emplace(std::move(item), static_cast<int>(item_map_.size())).first;
        return ret.second;
    }

    void add_sequence_atom(PropagateInit &init, int sid, TheoryAtom const &atom) {
        reserve(sequence_atoms_, sid);
        assert(sequence_atoms_[sid].literal == 0 && sequence_atoms_[sid].items.empty());
        sequence_atoms_[sid].literal = init.solver_literal(atom.literal());
        for (auto elem : atom.elements()) {
            int index = elem.tuple()[0].number();
            assert(index >= 0);
            int item = map_item(elem.tuple()[1].to_string());
            reserve(sequence_atoms_[sid].items, index, -1);
            sequence_atoms_[sid].items[index] = item;
        }
    }

    void add_pattern_atoms(PropagateInit &init, TheoryAtom const &atom) {
        for (auto elem : atom.elements()) {
            literal_t lit = init.solver_literal(elem.condition_id());
            auto args = elem.tuple();
            int index = args[0].number();
            assert(index >= 0);
            int item = map_item(args[1].to_string());
            auto ret = pattern_atoms_.emplace(lit, std::vector<PatternAtom>{});
            if (ret.second) { init.add_watch(lit); }
            ret.first->second.emplace_back(index, item);
            pattern_length_ = std::max(pattern_length_, index + 1);
        }
    }

    void initialize_occurrence_lists() {
        occurrence_list_.resize(occurrence_list_.size() + item_map_.size());
        int sid = 0;
        std::unordered_set<int> seen;
        for (auto &seq : sequence_atoms_) {
            for (auto &item : seq.items) {
                assert(item >= 0);
                if (seen.emplace(item).second) { occurrence_list_[item].emplace_back(sid); }
            }
            seen.clear();
            ++sid;
        }
    }

    void initialize_states(PropagateInit &init) {
        for (int i = 0; i < init.number_of_threads(); ++i) { states_.emplace_back(pattern_length_, static_cast<int>(sequence_atoms_.size())); }
    }

    // {{{2 propagation

    bool propagate_sequence_literal(State &state, PropagateControl &ctl, int sid, literal_t lit) {
        state.seq_active[sid] = false;
        state.stack.emplace_back(IndexType::SequenceIndex, sid);
        if (!ctl.assignment().is_true(lit)) {
            std::vector<literal_t> klaus;
            klaus.reserve(1 + pattern_length_);
            klaus.emplace_back(lit);
            for (auto &pat : state.pattern) {
                if (pat.literal != 0) { klaus.emplace_back(-pat.literal); }
            }
            return ctl.add_clause(klaus) && ctl.propagate();
        }
        return true;
    }

    bool propagate_sequence(State &state, PropagateControl &ctl, int sid, SequenceAtoms &atoms) {
        if (state.pattern_assigned < pattern_length_ && ctl.assignment().is_false(atoms.literal)) {
            return true;
        }
        auto it = atoms.items.begin(), ie = atoms.items.end();
        for (auto &pat : state.pattern) {
            while (true) {
                if (it == ie) { return propagate_sequence_literal(state, ctl, sid, -atoms.literal); }
                ++it;
                if (pat.literal == 0 || *(it-1) == pat.item_index) { break; }
            }
        }
        return state.pattern_assigned < pattern_length_ || propagate_sequence_literal(state, ctl, sid, atoms.literal);
    }

    // }}}2
public:
    SequenceMiningPropagator() { }

    void init(PropagateInit &init) override {
        for (auto atom : init.theory_atoms()) {
            auto term = atom.term();
            auto args = term.arguments();
            if (std::strcmp(term.name(), "seq") == 0 && args.size() == 1) { add_sequence_atom(init, args[0].number(), atom); }
            else if (std::strcmp(term.name(), "pat") == 0 && args.empty()) { add_pattern_atoms(init, atom); }
        }
        initialize_occurrence_lists();
        initialize_states(init);
    }

    void propagate(PropagateControl &ctl, LiteralSpan changes) override {
        auto &state = states_[ctl.thread_id()];
        uint32_t dl = ctl.assignment().decision_level();
        if (state.trail.size() == 0 || state.trail.back().decision_level < dl) {
            state.trail.emplace_back(dl, static_cast<int>(state.stack.size()));
        }
        for (auto &lit : changes) {
            for (auto &pat_atom : pattern_atoms_[lit]) {
                auto &pat = state.pattern[pat_atom.pattern_index];
                if (pat) {
                    // this case should not occur if the pattern is generated properly
                    auto old = pat.literal;
                    assert(ctl.assignment().is_true(old));
                    ctl.add_clause({-lit, -old}) && ctl.propagate();
                    return;

                }
                else {
                    state.stack.emplace_back(IndexType::PatternIndex, pat_atom.pattern_index);
                    ++state.pattern_assigned;
                    pat = {lit, pat_atom.item_index};
                    for (auto &sid : occurrence_list_[pat_atom.item_index]) {
                        if (state.seq_active[sid] && !propagate_sequence(state, ctl, sid, sequence_atoms_[sid])) {
                            return;
                        }
                    }
                }
            }
        }
    }

    void undo(PropagateControl const &ctl , LiteralSpan) noexcept override {
        auto &state = states_[ctl.thread_id()];
        int sid = state.trail.back().stack_index;
        auto ib = state.stack.begin() + sid, ie = state.stack.end();
        for (auto it = ib; it != ie; ++it) {
            if (it->type() == IndexType::PatternIndex) {
                state.pattern[it->pattern_index()] = {};
                --state.pattern_assigned;
            }
            else {
                state.seq_active[it->sequence_index()] = true;
            }
        }
        state.stack.erase(ib, ie);
        state.trail.pop_back();
    }

};

constexpr char const * const sequence_mining_encoding = R"(
#show pat/2.

#theory seq {
    term { };
    &seq/1 : term, body;
    &pat/0 : term, directive
}.

1 { pat(P,I) : seq(_,_,I) } 1 :- P = 0..(n-1).

&pat { P,I : pat(P,I) }.
sup(U) :- &seq(U) { P,I : seq(U,P,I) }, seq(U,_,_).

#maximize { 1,U : sup(U) }.

% abaca
seq(0,0,a).
seq(0,1,b).
seq(0,2,a).
seq(0,3,c).
seq(0,4,a).

% abdca
seq(1,0,a).
seq(1,1,b).
seq(1,2,d).
seq(1,3,c).
seq(1,4,a).

% aedca
seq(2,0,a).
seq(2,1,e).
seq(2,2,d).
seq(2,3,c).
seq(2,4,a).
)";

class PigeonPropagator : public Propagator {
public:
    void init(PropagateInit &init) override {
        unsigned nHole = 0, nPig = 0, nWatch = 0, p, h;
        state_.clear();
        p2h_[0] = 0;
        for (auto it = init.symbolic_atoms().begin(Signature("place", 2)), ie = init.symbolic_atoms().end(); it != ie; ++it) {
            literal_t lit = init.solver_literal(it->literal());
            p = it->symbol().arguments()[0].number();
            h = it->symbol().arguments()[1].number();
            p2h_[lit] = h;
            init.add_watch(lit);
            nHole = std::max(h, nHole);
            nPig  = std::max(p, nPig);
            ++nWatch;
        }
        assert(p2h_[0] == 0);
        for (unsigned i = 0, end = init.number_of_threads(); i != end; ++i) {
            state_.emplace_back(nHole + 1, 0);
            assert(state_.back().size() == nHole + 1);
        }
    }
    void propagate(PropagateControl &ctl, LiteralSpan changes) override {
        assert(ctl.thread_id() < state_.size());
        Hole2Lit& holes = state_[ctl.thread_id()];
        for (literal_t lit : changes) {
            literal_t& prev = holes[ p2h_[lit] ];
            if (prev == 0) { prev = lit; }
            else {
                if (!ctl.add_clause({-lit, -prev}) || !ctl.propagate()) {
                    return;
                }
                assert(false);
            }
        }
    }
    void undo(PropagateControl const &ctl, LiteralSpan undo) noexcept override {
        assert(ctl.thread_id() < state_.size());
        Hole2Lit& holes = state_[ctl.thread_id()];
        for (literal_t lit : undo) {
            unsigned hole = p2h_[lit];
            if (holes[hole] == lit) {
                holes[hole] = 0;
            }
        }
    }
private:
    using Lit2Hole = std::unordered_map<literal_t, unsigned>;
    using Hole2Lit = std::vector<literal_t>;
    using State    = std::vector<Hole2Lit>;

    Lit2Hole p2h_;
    State    state_;
};

class TestAssignment : public Propagator {
public:
    void init(PropagateInit &init) override {
        a_ = init.solver_literal(init.symbolic_atoms().find(Id("a"))->literal());
        b_ = init.solver_literal(init.symbolic_atoms().find(Id("b"))->literal());
        c_ = init.solver_literal(init.symbolic_atoms().find(Id("c"))->literal());
        init.add_watch(a_);
        init.add_watch(b_);

        auto ass = init.assignment();
        auto trail = ass.trail();

        REQUIRE(ass.decision_level() == 0);
        REQUIRE(trail.begin_offset(0) == 0);
        REQUIRE(trail.end_offset(0) == trail.size());

        std::set<Clingo::literal_t> lits;
        std::set<Clingo::literal_t> check_lits{1, c_};

        lits.clear();
        for (auto lit : ass) {
            lits.emplace(lit);
        }
        REQUIRE(lits == std::set<Clingo::literal_t>{1, a_, b_, c_});

        lits.clear();
        for (auto lit : trail) {
            lits.emplace(lit);
        }
        REQUIRE(lits == std::set<Clingo::literal_t>{1, c_});

        lits.clear();
        for (auto lit : Clingo::make_range(trail.begin(0), trail.end(0))) {
            lits.emplace(lit);
        }
        REQUIRE(lits == std::set<Clingo::literal_t>{1, c_});
    }
    void propagate(PropagateControl &ctl, LiteralSpan changes) override {
        auto ass = ctl.assignment();
        auto trail = ass.trail();
        count_+= changes.size();
        REQUIRE(ass.is_fixed(c_));
        REQUIRE(!ass.is_fixed(a_));
        REQUIRE(!ass.is_fixed(b_));
        REQUIRE(!ass.has_conflict());
        REQUIRE(ass.has_literal(a_));
        REQUIRE(ass.has_literal(b_));
        REQUIRE(!ass.has_literal(1000));
        auto level = ass.decision_level();
        auto decision = ass.decision(level);
        REQUIRE(ass.level(decision) == level);
        if (count_ == 1) {
            auto a = changes[0];
            REQUIRE(changes.size() == 1);
            REQUIRE(!ass.is_fixed(a_));
            REQUIRE(ass.is_true(a));
            REQUIRE(ass.truth_value(a) == TruthValue::True);
            REQUIRE((ass.is_true(a_) ^ ass.is_true(b_)));
            REQUIRE(ass.level(a) == level);
            REQUIRE(trail.end_offset(level) - trail.begin_offset(level) >= 1);
            REQUIRE(trail.end_offset(level) - trail.begin_offset(level) <= 2);
            bool found = false;
            for (auto lit : Clingo::make_range(trail.begin(level), trail.end(level))) {
                if (lit == a) {
                    found = true;
                }
            }
            REQUIRE(found);
        }
        if (count_ == 2) {
            REQUIRE(!ass.is_fixed(a_));
            REQUIRE(!ass.is_fixed(b_));
            REQUIRE(ass.is_true(a_));
            REQUIRE(ass.is_true(b_));
        }
    }
    void undo(PropagateControl const &, LiteralSpan undo) noexcept override {
        count_-= undo.size();
    }
private:
    literal_t a_;
    literal_t b_;
    literal_t c_;
    size_t count_ = 0;
};

literal_t get_literal(PropagateInit &init, char const *name) {
    return init.solver_literal(init.symbolic_atoms().find(Id(name))->literal());
}

template <typename T>
class TestInit : public Propagator {
public:
    TestInit(T &&f) : f_{std::forward<T>(f)} { }
    void init(PropagateInit &init) override {
        f_(init);
    }
private:
    T f_;
};

template <typename T>
static TestInit<T> make_init(T &&f) {
    return {std::forward<T>(f)};
}


class TestAddClause : public Propagator {
public:
    void init(PropagateInit &init) override {
        a_ = init.solver_literal(init.symbolic_atoms().find(Id("a"))->literal());
        b_ = init.solver_literal(init.symbolic_atoms().find(Id("b"))->literal());
        init.add_watch(a_);
        init.add_watch(b_);
    }
    void propagate(PropagateControl &ctl, LiteralSpan changes) override {
        count_+= changes.size();
        REQUIRE_FALSE((enable && count_ == 2 && ctl.add_clause({-a_, -b_}, type) && ctl.propagate()));
    }
    void undo(PropagateControl const &, LiteralSpan undo) noexcept override {
        count_-= undo.size();
    }
public:
    ClauseType type = ClauseType::Learnt;
    bool enable = true;
private:
    literal_t a_;
    literal_t b_;
    size_t count_ = 0;
};

class TestAddWatch : public Propagator {
public:
    void init(PropagateInit &init) override {
        REQUIRE(init.number_of_threads() == 2);
        a = init.solver_literal(init.symbolic_atoms().find(Id("a"))->literal());
        b = init.solver_literal(init.symbolic_atoms().find(Id("b"))->literal());
        auto c = init.solver_literal(init.symbolic_atoms().find(Id("c"))->literal());
        auto d = init.solver_literal(init.symbolic_atoms().find(Id("d"))->literal());
        init.add_watch(a, 0);
        init.add_watch(-a, 0);
        init.add_watch(b, 0);
        init.add_watch(-b, 0);
        init.add_watch(-b, 1);
        init.add_watch(b, 1);
        auto assignment = init.assignment();
        REQUIRE(assignment.truth_value(a) == Clingo::TruthValue::Free);
        REQUIRE(assignment.truth_value(b) == Clingo::TruthValue::Free);
        REQUIRE(assignment.truth_value(c) == Clingo::TruthValue::True);
        REQUIRE(assignment.truth_value(d) == Clingo::TruthValue::False);
        done_ = false;
    }
    void propagate(PropagateControl &ctl, LiteralSpan changes) override {
        if (ctl.thread_id() == 0) {
            // wait for thread 1 to propagate b
            std::unique_lock<decltype(mut_)> lock(mut_);
            cv.wait(lock, [this]() { return done_; });
        }
        else {
            for (auto lit : changes) {
                std::lock_guard<decltype(mut_)> lock(mut_);
                done_ = true;
                propagated.insert(std::abs(lit));
            }
            cv.notify_one();
        }
    }
public:
    std::set<literal_t> propagated;
    literal_t a;
    literal_t b;
private:
    std::mutex mut_;
    std::condition_variable cv;
    bool done_;
};

class TestException : public Propagator {
public:
    void check(PropagateControl &) override {
        throw std::runtime_error("the answer is 42");
    }
};

class TestMode : public Propagator {
public:
    void init(PropagateInit &init) override {
        auto atoms = init.symbolic_atoms();
        auto sig = Clingo::Signature("p", 1);
        for (auto atom : Clingo::make_range(atoms.begin(sig), atoms.end())) {
            lits_.insert(init.solver_literal(atom.literal()));
        }
        init.set_check_mode(Clingo::PropagatorCheckMode::Partial);
    }
    void check(PropagateControl &ctl) override {
        //if (!ctl.assignment().is_total()) { throw std::logic_error("unexpected total check"); }
        for (auto &lit : lits_) {
            if (ctl.assignment().truth_value(lit) == Clingo::TruthValue::Free) {
                ctl.add_clause({lit});
                break;
            }
        }
    }
private:
    std::set<Clingo::literal_t> lits_;
};

TEST_CASE("propagator", "[clingo][propagator]") {
    MessageVec messages;
    ModelVec models;
    Logger logger = [&messages](WarningCode code, char const *msg) { messages.emplace_back(code, msg); };
    Control ctl{{"0"}, logger, 20};
    SECTION("pigeon") {
        PigeonPropagator p;
        ctl.register_propagator(p);
        ctl.add("pigeon", {"h", "p"}, "1 { place(P,H) : H = 1..h } 1 :- P = 1..p.");
        auto place = [](int p, int h) { return Function("place", {Number(p), Number(h)}); };
        SECTION("unsat") {
            ctl.ground({{"pigeon", {Number(5), Number(6)}}}, nullptr);
            test_solve(ctl.solve(), models);
            REQUIRE(models.empty());
        }
        SECTION("sat") {
            ctl.ground({{"pigeon", {Number(2), Number(2)}}}, nullptr);
            test_solve(ctl.solve(), models);
            REQUIRE(models == ModelVec({{place(1,1), place(2,2)}, {place(1,2), place(2,1)}}));
        }
    }
    SECTION("assignment") {
        TestAssignment p;
        ctl.register_propagator(p, false);
        ctl.add("base", {}, "{a; b}. c.");
        ctl.ground({{"base", {}}}, nullptr);
        test_solve(ctl.solve(), models);
        REQUIRE(models.size() == 4);
    }
    SECTION("mode") {
        TestMode prop;
        ctl.register_propagator(prop, false);
        ctl.add("base", {}, "{p(1..9)}.");
        ctl.ground({{"base", {}}}, nullptr);
        test_solve(ctl.solve(), models);
        auto p = [](int n) { return Function("p", {Number(n)}); };
        REQUIRE(models == ModelVec({{ p(1), p(2), p(3), p(4), p(5), p(6), p(7), p(8), p(9) }}));
    }
    SECTION("add_watch") {
        TestAddWatch prop;
        ctl.configuration()["solve"]["parallel_mode"] = "2";
        ctl.register_propagator(prop, false);
        ctl.add("base", {}, "{a;b;c;d}. c. :- d.");
        ctl.ground({{"base", {}}}, nullptr);
        test_solve(ctl.solve(), models);
        auto a = Function("a", {});
        auto b = Function("b", {});
        auto c = Function("c", {});
        REQUIRE(models == ModelVec({{ a, b, c }, { a, c }, { b, c }, { c }}));
        REQUIRE(prop.propagated == std::set<literal_t>{prop.b});
    }
    SECTION("exception") {
        TestException p;
        ctl.register_propagator(p, false);
        ctl.add("base", {}, "{a}.");
        ctl.ground({{"base", {}}}, nullptr);
        try {
            test_solve(ctl.solve(), models);
            FAIL("solve must throw");
        }
        catch (std::runtime_error const &e) { REQUIRE(e.what() == S("the answer is 42")); }
    }
    SECTION("exception-t2") {
        bool skip = false;
        try { ctl.configuration()["solve.parallel_mode"] = "2"; }
        catch (std::exception const &e) {
            if (std::strcmp(e.what(), "invalid key") == 0) { skip = true; }
            else { throw; }
        }
        if (!skip) {
            TestException p;
            ctl.register_propagator(p, false);
            ctl.add("base", {}, "{a}.");
            ctl.ground({{"base", {}}}, nullptr);
            try {
                test_solve(ctl.solve(), models);
                FAIL("solve must throw");
            }
            catch (std::runtime_error const &e) {
                REQUIRE(e.what() == S("the answer is 42"));
            }
        }
    }
    SECTION("add_clause") {
        TestAddClause p;
        ctl.register_propagator(p, false);
        SECTION("learnt") {
            p.type = ClauseType::Learnt;
            ctl.add("base", {}, "{a; b}.");
            ctl.ground({{"base", {}}}, nullptr);
            test_solve(ctl.solve(), models);
            REQUIRE(models.size() == 3);
            p.enable = false;
            test_solve(ctl.solve(), models);
            REQUIRE(models.size() >= 3);
        }
        SECTION("static") {
            p.type = ClauseType::Static;
            ctl.add("base", {}, "{a; b}.");
            ctl.ground({{"base", {}}}, nullptr);
            test_solve(ctl.solve(), models);
            REQUIRE(models.size() == 3);
            p.enable = false;
            test_solve(ctl.solve(), models);
            REQUIRE(models.size() == 3);
        }
        SECTION("volatile") {
            p.type = ClauseType::Volatile;
            ctl.add("base", {}, "{a; b}.");
            ctl.ground({{"base", {}}}, nullptr);
            test_solve(ctl.solve(), models);
            REQUIRE(models.size() == 3);
            p.enable = false;
            test_solve(ctl.solve(), models);
            REQUIRE(models.size() == 4);
        }
        SECTION("volatile static") {
            p.type = ClauseType::VolatileStatic;
            ctl.add("base", {}, "{a; b}.");
            ctl.ground({{"base", {}}}, nullptr);
            test_solve(ctl.solve(), models);
            REQUIRE(models.size() == 3);
            p.enable = false;
            test_solve(ctl.solve(), models);
            REQUIRE(models.size() == 4);
        }
    }
    SECTION("add_clause_init") {
        // NOTE: some of the tests would fail if sat preprocessing were activated
        //       because propagation has no effect in this case
        ctl.configuration()["sat_prepro"] = "0";

        ctl.add("base", {}, "{a; b}. c. :- a, b.");
        ctl.ground({{"base", {}}}, nullptr);
        SECTION("conflict") {
            auto p = make_init([](Clingo::PropagateInit &init){
                REQUIRE_FALSE(init.add_clause({-get_literal(init, "c")}));
            });
            ctl.register_propagator(p, false);
            test_solve(ctl.solve(), models);
            REQUIRE(models.size() == 0);
        }
        SECTION("propagate") {
            auto p = make_init([](Clingo::PropagateInit &init){
                REQUIRE(init.add_clause({get_literal(init, "a")}));
                REQUIRE(init.assignment().is_true(get_literal(init, "a")));
                REQUIRE(init.propagate());
                REQUIRE(init.assignment().is_false(get_literal(init, "b")));
            });
            ctl.register_propagator(p, false);
            test_solve(ctl.solve(), models);
            REQUIRE(models.size() == 1);
        }
        SECTION("propagate") {
            auto p = make_init([](Clingo::PropagateInit &init){
                auto ass = init.assignment();
                auto lit = init.add_literal();
                auto a = get_literal(init, "a");
                REQUIRE(init.add_clause({lit}));
                REQUIRE(ass.is_true(lit));
                REQUIRE(init.add_clause({-lit, a}));
                REQUIRE(ass.is_true(a));
                REQUIRE(init.propagate());
                REQUIRE(init.assignment().is_false(get_literal(init, "b")));
            });
            ctl.register_propagator(p, false);
            test_solve(ctl.solve(), models);
            REQUIRE(models.size() == 1);
        }
        SECTION("propagate") {
            auto p = make_init([](Clingo::PropagateInit &init){
                auto ass = init.assignment();
                auto lit = init.add_literal();
                auto a = get_literal(init, "a");
                REQUIRE(init.add_clause({-lit, a}));
                REQUIRE(init.add_clause({lit}));
                REQUIRE(ass.is_true(lit));
                REQUIRE(init.propagate());
                REQUIRE(ass.is_true(a));
                REQUIRE(init.assignment().is_false(get_literal(init, "b")));
            });
            ctl.register_propagator(p, false);
            test_solve(ctl.solve(), models);
            REQUIRE(models.size() == 1);
        }
        SECTION("propagate") {
            auto p = make_init([](Clingo::PropagateInit &init){
                auto ass = init.assignment();
                auto lit = init.add_literal();
                auto a = get_literal(init, "a");
                auto b = get_literal(init, "b");
                REQUIRE(init.add_clause({lit}));
                REQUIRE(ass.is_true(lit));
                REQUIRE(init.add_clause({-lit, a}));
                REQUIRE(init.add_clause({-lit, b}));
                REQUIRE_FALSE(init.propagate());
            });
            ctl.register_propagator(p, false);
            test_solve(ctl.solve(), models);
            REQUIRE(models.size() == 0);
        }
    }
    SECTION("add_weight_constraint") {
        ctl.add("base", {}, "{a; b}.");
        ctl.ground({{"base", {}}}, nullptr);
        SECTION("equal") {
            auto p = make_init([](Clingo::PropagateInit &init){
                auto t = init.add_literal();
                auto a = get_literal(init, "a");
                auto b = get_literal(init, "b");
                REQUIRE(init.add_clause({t}));
                auto l = init.add_literal();
                REQUIRE(init.add_weight_constraint(t, {{a,1}, {b,1}, {l,1}}, 2, Clingo::WeightConstraintType::Equivalence, true));
            });
            ctl.register_propagator(p, false);
            test_solve(ctl.solve(), models);
            REQUIRE(models.size() == 3);
        }
    }
    SECTION("add_minimize") {
        ctl.add("base", {}, "2 {a; b; c}. 2 {b; c; d}.");
        ctl.ground({{"base", {}}}, nullptr);
        ctl.configuration()["solve"]["opt_mode"] = "optN";
            SECTION("minimize") {
            auto p = make_init([](Clingo::PropagateInit &init){
                init.add_minimize(get_literal(init, "a"), 1, 0);
                init.add_minimize(get_literal(init, "b"), 1, 0);
                init.add_minimize(get_literal(init, "c"), 1, 0);
                init.add_minimize(get_literal(init, "d"), 1, 0);
            });
            ctl.register_propagator(p, false);
            {
                MCB mcb{models};
                for (auto &m : ctl.solve()) {
                    if (m.optimality_proven()) { mcb(m); }
                }
            }
            REQUIRE(models.size() == 1);
        }
    }
}

TEST_CASE("propgator-sequence-mining", "[clingo][propagator]") {
    MessageVec messages;
    ModelVec models;
    Logger logger = [&messages](WarningCode code, char const *msg) { messages.emplace_back(code, msg); };
#if defined(CLASP_HAS_THREADS) && CLASP_HAS_THREADS == 1
    Control ctl{{"-t14"}, logger, 20};
#else
    Control ctl{{}, logger, 20};
#endif
    SECTION("sequence mining") {
        ctl.configuration()["solve"]["opt_mode"] = "optN";
        SequenceMiningPropagator p;
        ctl.register_propagator(p, false);
        ctl.add("base", {"n"}, sequence_mining_encoding);
        int n = 5;
        ctl.ground({{"base", {Number(n)}}}, nullptr);
        // NOTE: maps and sets are not necessary
        //       this also works with vectors + sorting avoiding node based containers
        // :- not sup(U), seq(U,_,_), n == 0.
        if (n == 0) {
            std::map<Symbol, std::vector<literal_t>> grouped;
            for (auto it = ctl.symbolic_atoms().begin({"seq", 3}), ie = ctl.symbolic_atoms().end(); it != ie; ++it) {
                grouped[it->symbol().arguments().front()].emplace_back(it->literal());
            }
            for (auto &elem : grouped) {
                auto atom = ctl.backend().add_atom();
                for (auto &lit : elem.second) {
                    ctl.backend().rule(false, {atom}, {lit});
                }
                ctl.backend().rule(false, {}, {-ctl.symbolic_atoms()[Function("sup", {elem.first})].literal(), literal_t(atom)});
            }
        }
        // :- sup(U), pat(_,I), not seq(U,_,I).
        std::map<Symbol, std::vector<literal_t>> grouped_pat;
        std::unordered_set<Symbol> grouped_seq;
        for (auto it = ctl.symbolic_atoms().begin({"pat", 2}), ie = ctl.symbolic_atoms().end(); it != ie; ++it) {
            grouped_pat[it->symbol().arguments()[1]].emplace_back(it->literal());
        }
        for (auto it = ctl.symbolic_atoms().begin({"seq", 3}), ie = ctl.symbolic_atoms().end(); it != ie; ++it) {
            grouped_seq.emplace(Function("", {it->symbol().arguments()[0], it->symbol().arguments()[2]}));
        }
        std::map<Symbol, atom_t> projected_pat;
        for (auto &pat : grouped_pat) {
            auto atom = ctl.backend().add_atom();
            for (auto &lit : pat.second) {
                ctl.backend().rule(false, {atom}, {lit});
            }
            projected_pat.emplace(pat.first, atom);
        }
        for (auto it = ctl.symbolic_atoms().begin({"sup", 1}), ie = ctl.symbolic_atoms().end(); it != ie; ++it) {
            for (auto &pat : projected_pat) {
                if (grouped_seq.find(Function("", {it->symbol().arguments().front(), pat.first})) == grouped_seq.end()) {
                    ctl.backend().rule(false, {}, {it->literal(), literal_t(pat.second)});
                }
            }
        }
        int64_t optimum = std::numeric_limits<int64_t>::max();
        for (auto &m : ctl.solve()) {
            int64_t opt = m.cost()[0];
            if (opt == optimum) {
                models.emplace_back();
                for (auto sym : m.symbols(ShowType::Shown)) {
                    models.back().emplace_back(sym);
                }
                std::sort(models.back().begin(), models.back().end());
            }
            else { optimum = opt; }
        }
        std::sort(models.begin(), models.end());
        auto pat = [](int num, char const *item) { return Function("pat", {Number(num), Id(item)}); };
        ModelVec solution = {
            { pat(0,"a"), pat(1,"b"), pat(2,"a"), pat(3,"c"), pat(4,"a") },
            { pat(0,"a"), pat(1,"b"), pat(2,"d"), pat(3,"c"), pat(4,"a") },
            { pat(0,"a"), pat(1,"e"), pat(2,"d"), pat(3,"c"), pat(4,"a") } };
        REQUIRE(models == solution);
    }
}

} } // namespace Test Clingo
