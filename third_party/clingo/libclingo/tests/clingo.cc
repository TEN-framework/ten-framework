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

#include "clingo.hh"
#include "tests.hh"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iterator>
#ifdef _MSC_VER
#pragma warning (disable : 4996) // 'tmpnam': may be unsafe.
#endif
namespace Clingo { namespace Test {

TEST_CASE("parse_term", "[clingo]") {
    MessageVec messages;
    Logger logger = [&messages](WarningCode code, char const *msg) { messages.emplace_back(code, msg); };
    REQUIRE(parse_term("10+1").number() == 11);
    REQUIRE_THROWS(parse_term("10+", logger));
    //REQUIRE(messages == (MessageVec{{WarningCode::Runtime, "<string>:1:5: error: syntax error, unexpected <EOF>\n"}}));
    messages.clear();
    REQUIRE_THROWS(parse_term("10+a", logger));
    REQUIRE(messages.size() == 0);
}

class Observer : public GroundProgramObserver {
public:
    Observer(std::vector<std::string> &trail)
    : trail_(trail) { }

    void init_program(bool incremental) override {
        trail_.emplace_back(incremental ? "IP: incremental" : "IP");
    }
    void begin_step() override {
        trail_.emplace_back("BS");
    }
    void end_step() override {
        trail_.emplace_back("ES");
    }

    void rule(bool choice, AtomSpan head, LiteralSpan body) override {
        std::ostringstream out;
        out << "R: ";
        if (choice) { out << "{"; }
        bool comma = false;
        for (auto &h : head) {
            if (comma) { out << (choice ? "," : "|"); }
            else       { comma = true; }
            out << h;
        }
        if (choice) { out << "}"; }
        out << ":-";
        comma = false;
        for (auto &b : body) {
            if (comma) { out << ","; }
            else       { comma = true; }
            if (b > 0) { out << b; }
            else       { out << "~" << -b; }
        }
        trail_.emplace_back(out.str());
    }
private:
    std::vector<std::string> &trail_;
};

TEST_CASE("solving", "[clingo]") {
    SECTION("with control") {
        MessageVec messages;
        ModelVec models;
        Control ctl{{"0"}, [&messages](WarningCode code, char const *msg) { messages.emplace_back(code, msg); }, 20};
        SECTION("solve") {
            static int n = 0;
            if (++n < 3) { // workaround for some bug with catch
                SECTION("add") { ctl.add("base", {}, "{a}."); }
                SECTION("load") {
                    struct Temp {
                        Temp() : file(std::tmpnam(temp)) { }
                        ~Temp() { if (file) { std::remove(temp); } }
                        char temp[L_tmpnam];
                        char *file;
                    } t;
                    REQUIRE(t.file != nullptr);
                    std::ofstream(t.file) << "{a}.\n";
                    ctl.load(t.file);
                }
                ctl.ground({{"base", {}}});
                REQUIRE(test_solve(ctl.solve(), models).is_satisfiable());
                REQUIRE(models == ModelVec({{},{Id("a")}}));
                REQUIRE(messages.empty());
            }
        }
        SECTION("solve") {
            ctl.add("base", {}, "x(1).");
            ctl.ground({{"base", {}}});
            int n = 0;
            auto atms = ctl.symbolic_atoms();
            for (auto it = atms.begin(Signature{"x", 1, false}), ie = atms.end(); it != ie; ++it) {
                ++n;
            }
            REQUIRE(n == 0);
            for (auto it = atms.begin(Signature{"x", 1, true}), ie = atms.end(); it != ie; ++it) {
                ++n;
            }
            REQUIRE(n == 1);
        }
        SECTION("get_statistics") {
            ctl.add("pigeon", {"p", "h"}, "1 {p(P,H) : P=1..p}1 :- H=1..h."
                                          "1 {p(P,H) : H=1..h}1 :- P=1..p.");
            ctl.ground({{"pigeon", {Number(6), Number(5)}}});
            auto conf = ctl.configuration();
            conf["stats"] = "2";
            REQUIRE(test_solve(ctl.solve(), models).is_unsatisfiable());
            std::vector<std::string> keys_root;
            auto stats = ctl.statistics();
            std::copy(stats.keys().begin(), stats.keys().end(), std::back_inserter(keys_root));
            std::sort(keys_root.begin(), keys_root.end());
            std::vector<std::string> keys_check = { "accu", "problem", "solving", "summary" };
            REQUIRE(keys_root == keys_check);
            auto solving = stats["solving"];
            REQUIRE(solving["solver"].type() == StatisticsType::Array);
            REQUIRE(solving["solvers"].type() == StatisticsType::Map);
            REQUIRE(stats["summary.times.cpu"].type() == StatisticsType::Value);
            REQUIRE(stats["summary.times.cpu"] >= 0.0);
            REQUIRE(solving["solver"].size() == 1);
            REQUIRE(solving["solver"][size_t(0)]["conflicts"] > 0);
            int nloop = 0;
            for (auto s : stats["solving.solver"]) {
                REQUIRE(s["conflicts"] > 0);
                ++nloop;
            }
            REQUIRE(nloop == 1);
        }
        SECTION("set_statistics") {
            ctl.ground({{"base", {}}});
            struct H : Clingo::SolveEventHandler {
                void on_statistics(Clingo::UserStatistics step, Clingo::UserStatistics accu) override {
                    auto map = step.add_subkey("map", StatisticsType::Map);
                    map.add_subkey("x", StatisticsType::Value).set_value(1);
                    map.add_subkey("y", StatisticsType::Value).set_value(2);
                    map = accu.add_subkey("map", StatisticsType::Map);
                    map.add_subkey("x", StatisticsType::Value).set_value(3);
                    map.add_subkey("y", StatisticsType::Value).set_value(4);
                }
            } handler;
            REQUIRE(test_solve(ctl.solve(Clingo::LiteralSpan{}, &handler), models).is_satisfiable());
            auto stats = ctl.statistics();
            REQUIRE(stats["user_step.map"].type() == StatisticsType::Map);
            REQUIRE(stats["user_step.map.x"].type() == StatisticsType::Value);
            REQUIRE(stats["user_step.map.y"].type() == StatisticsType::Value);
            REQUIRE(stats["user_step.map.x"].value() == 1);
            REQUIRE(stats["user_step.map.y"].value() == 2);
            REQUIRE(stats["user_accu.map"].type() == StatisticsType::Map);
            REQUIRE(stats["user_accu.map.x"].type() == StatisticsType::Value);
            REQUIRE(stats["user_accu.map.y"].type() == StatisticsType::Value);
            REQUIRE(stats["user_accu.map.x"].value() == 3);
            REQUIRE(stats["user_accu.map.y"].value() == 4);
        }
        SECTION("configuration") {
            auto conf = ctl.configuration();
            std::vector<std::string> keys_root;
            std::copy(conf.keys().begin(), conf.keys().end(), std::back_inserter(keys_root));
            std::sort(keys_root.begin(), keys_root.end());
            std::vector<std::string> keys_check = { "asp", "configuration", "learn_explicit", "parse_ext", "parse_maxsat", "sat_prepro", "share", "solve", "solver", "stats", "tester" };
            REQUIRE(keys_root == keys_check);
            REQUIRE(conf["solve"]["models"].is_value());
            conf["solve"]["models"] = "2";
            REQUIRE(conf["solve"].decription() == S("Solve Options"));
            REQUIRE(conf["solve"]["models"].is_assigned());
            REQUIRE(conf["solve"]["models"].value() == "2");
            REQUIRE(conf["solver"].is_array());
            REQUIRE(conf["solver"].size() == 1);
            int nloop = 0;
            for (auto s : conf["solver"]) {
                s["heuristic"] = "berkmin,100";
                ++nloop;
            }
            REQUIRE(nloop == 1);
            CHECK(conf["solver"][size_t(0)]["heuristic"].value() == "berkmin,100");
            CHECK(conf["solver"]["heuristic"].value() == "berkmin,100");
            ctl.add("base", {}, "{a; b; c}.");
            ctl.ground({{"base", {}}});
            REQUIRE(test_solve(ctl.solve(), models).is_satisfiable());
            REQUIRE(models.size() == 2);
        }
        SECTION("backend") {
            // NOTE: ground has to be called before using the backend
            literal_t a, b;
            {
                auto backend = ctl.backend();
                a = backend.add_atom();
                b = backend.add_atom();
                backend.rule(true, {atom_t(a)}, {});
                backend.rule(false, {atom_t(b)}, {-a});
            }
            test_solve(ctl.solve(), models);
            REQUIRE(models == (ModelVec{{},{}}));
            {
                auto backend = ctl.backend();
                backend.assume({a});
            }
            test_solve(ctl.solve(), models);
            REQUIRE(models == (ModelVec{{}}));
            {
                auto backend = ctl.backend();
                backend.minimize(1, {{a,1},{b,1}});
            }
            test_solve(ctl.solve(), models);
            REQUIRE(ctl.statistics()["summary.costs"].size() == 1);
            REQUIRE(ctl.statistics()["summary.costs"][size_t(0)] == 1);
            REQUIRE(ctl.statistics()["summary.call"] == 2);
            // Note: I don't have a good idea how to test this one
            // void heuristic(atom_t atom, HeuristicType type, int bias, unsigned priority, LitSpan condition);
        }
        SECTION("backend-project") {
            ctl.configuration()["solve.project"] = "auto";
            {
                auto backend = ctl.backend();
                atom_t a = backend.add_atom(), b = backend.add_atom();
                backend.rule(true, {a, b}, {});
                backend.project({a});
            }
            test_solve(ctl.solve(), models);
            REQUIRE(models == (ModelVec{{},{}}));
        }
        SECTION("backend-external") {
            {
                auto backend = ctl.backend();
                atom_t a = backend.add_atom();
                backend.external(a, ExternalType::Free);
                test_solve(ctl.solve(), models);
                REQUIRE(models == (ModelVec{{},{}}));
                backend.external(a, ExternalType::Release);
            }
            test_solve(ctl.solve(), models);
            REQUIRE(models == (ModelVec{{}}));
        }
        SECTION("backend-acyc") {
            {
                auto backend = ctl.backend();
                atom_t a = backend.add_atom(), b = backend.add_atom();
                backend.rule(true, {a, b}, {});
                backend.acyc_edge(1, 2, {literal_t(a)});
                backend.acyc_edge(2, 1, {literal_t(b)});
            }
            test_solve(ctl.solve(), models);
            REQUIRE(models == (ModelVec{{},{},{}}));
        }
        SECTION("backend-weight-rule") {
            {
                auto backend = ctl.backend();
                atom_t a = backend.add_atom(), b = backend.add_atom();
                backend.rule(true, {a, b}, {});
                backend.weight_rule(false, {}, 1, {{literal_t(a),1}, {literal_t(b),1}});
            }
            test_solve(ctl.solve(), models);
            REQUIRE(models == (ModelVec{{}}));
        }
        SECTION("backend-add-atom") {
            auto a = [](int x) { return Function("a", {Number(x)}); };
            auto b = [](int x) { return Function("b", {Number(x)}); };
            auto e = [](int x) { return Function("e", {Number(x)}); };
            {
                auto backend = ctl.backend();
                auto aa = backend.add_atom(a(2));
                auto ab = backend.add_atom(b(1));
                backend.rule(true, {aa}, {});
                backend.rule(true, {ab}, {});
            }

            ctl.ground({{"base", {}}});

            {
                auto backend = ctl.backend();
                auto ae = backend.add_atom(e(3));
                backend.rule(false, {ae}, {});
            }
            test_solve(ctl.solve(), models);
            REQUIRE(models == (ModelVec{{ a(2), b(1), e(3) }, { a(2), e(3) }, { b(1), e(3) }, { e(3) }}));

            ctl.ground({{"multi", {}}});
            test_solve(ctl.solve(), models);
            REQUIRE(models == (ModelVec{{ a(2), b(1), e(3) }, { a(2), e(3) }, { b(1), e(3) }, { e(3) }}));

        }
        SECTION("backend-theory") {
            ctl.with_backend([](Backend &backend) {
                auto num_one = backend.add_theory_term_number(1);
                auto num_two = backend.add_theory_term_number(2);
                auto str_x = backend.add_theory_term_string("x");
                auto fun = backend.add_theory_term_function("f", {num_one, num_two, str_x});
                auto seq = backend.add_theory_term_sequence(TheorySequenceType::Set, {num_one, num_two, str_x});
                auto fseq = backend.add_theory_term_function("f", {seq});

                REQUIRE(num_one == backend.add_theory_term_number(1));
                REQUIRE(num_two == backend.add_theory_term_number(2));
                REQUIRE(str_x == backend.add_theory_term_string("x"));
                REQUIRE(num_one == backend.add_theory_term_symbol(parse_term("1")));
                REQUIRE(num_two == backend.add_theory_term_symbol(parse_term("2")));
                REQUIRE(fun == backend.add_theory_term_symbol(parse_term("f(1,2,x)")));

                auto elem = backend.add_theory_element({num_one, num_two, seq, fun}, {1, -2, 3});

                backend.theory_atom(0, fun, {});
                backend.theory_atom(0, backend.add_theory_term_symbol(parse_term("g(1,2)")), {});
                backend.theory_atom(0, fseq, {elem});
            });
            auto atoms = ctl.theory_atoms();
            using SV = std::vector<S>;
            SV str_atoms;
            TheoryAtom last{nullptr, 0};
            for (auto atom : atoms) {
                str_atoms.emplace_back(atom.to_string());
                last = atom;
            }
            REQUIRE(str_atoms == SV{
                "&f(1,2,x){}",
                "&g(1,2){}",
                "&f({1,2,x}){1,2,{1,2,x},f(1,2,x): #aux(1),not #aux(2),#aux(3)}"});
            REQUIRE(last.elements().back().condition() == LiteralSpan{{1, -2, 3}});
        }
        SECTION("optimize") {
            ctl.add("base", {}, "2 {a; b; c; d}.\n"
                                ":- a, b.\n"
                                ":- a, c.\n"
                                "#minimize {2@2:a; 3@2:b; 4@2:c; 5@2:d}.\n"
                                "#minimize {3@1:a; 4@1:b; 5@1:c; 2@1:d}.\n");
            ctl.ground({{"base", {}}});
            SymbolVector model;
            CostVector optimum;
            PriorityVector priorities;
            for (auto const &m : ctl.solve()) {
                model = m.symbols();
                optimum = m.cost();
                priorities = m.priorities();
            }
            REQUIRE(model == SymbolVector({ Id("a"), Id("d") }));
            REQUIRE(optimum == (CostVector{7,5}));
            REQUIRE(priorities == (std::vector<int>{2, 1}));
            REQUIRE(messages.empty());
        }
#if defined(CLASP_HAS_THREADS) && CLASP_HAS_THREADS == 1
        SECTION("async") {
            ctl.add("base", {}, "{a}.");
            ctl.ground({{"base", {}}});
            auto async = ctl.solve_async(MCB(models));
            REQUIRE(async.get().is_satisfiable());
            REQUIRE(models == ModelVec({{},{Id("a")}}));
            REQUIRE(messages.empty());
        }
#endif
        SECTION("model") {
            ctl.add("base", {}, "a. #show b.");
            ctl.ground({{"base", {}}});
            bool sat = false;
            for (auto &m : ctl.solve()) {
                REQUIRE(m.type() == ModelType::StableModel);
                REQUIRE(m.symbols(ShowType::Atoms) == (SymbolVector{Id("a")}));
                REQUIRE(m.symbols(ShowType::Terms) == (SymbolVector{Id("b")}));
                REQUIRE(m.symbols(ShowType::Shown) == (SymbolVector{Id("a"), Id("b")}));
                REQUIRE(m.symbols(ShowType::Atoms | ShowType::Complement).size() == 0);
                REQUIRE( m.contains(Id("a")));
                REQUIRE(!m.contains(Id("b")));
                sat = true;
            };
            REQUIRE(sat);
            REQUIRE(messages.empty());
        }
        SECTION("model-add-clause") {
            ctl.add("base", {}, "1{a;b}1.");
            ctl.ground({{"base", {}}});
            int n = 0;
            for (auto &m : ctl.solve()) {
                REQUIRE(m.type() == ModelType::StableModel);
                REQUIRE(m.thread_id() == 0);
                ++n;
                char const *name = m.contains(Id("a")) ? "b" : "a";
                m.context().add_clause({{Id(name), false}});
            };
            REQUIRE(n == 1);
            REQUIRE(messages.empty());
        }
        SECTION("model-cautious") {
            ctl.configuration()["solve.enum_mode"] = "cautious";
            ctl.add("base", {}, "a.");
            ctl.ground({{"base", {}}});
            int n = 0;
            for (auto &m : ctl.solve()) {
                REQUIRE(m.type() == ModelType::CautiousConsequences);
                REQUIRE(m.thread_id() == 0);
                ++n;
            }
            REQUIRE(n == 1);
            REQUIRE(messages.empty());
        }
        SECTION("model-cost") {
            ctl.configuration()["solve.opt_mode"] = "optN";
            ctl.add("base", {}, "{a}. #minimize { 1:a }.");
            ctl.ground({{"base", {}}});
            int n = 0;
            for (auto &m : ctl.solve()) {
                REQUIRE(m.type() == ModelType::StableModel);
                REQUIRE(m.thread_id() == 0);
                if (m.optimality_proven()) {
                    REQUIRE(m.number() == 1);
                    REQUIRE(m.cost() == (CostVector{0}));
                    ++n;
                }
            }
            REQUIRE(n == 1);
            REQUIRE(messages.empty());
        }
        SECTION("assumptions") {
            ctl.add("base", {}, "{a;b;c}.");
            ctl.ground({{"base", {}}});
            REQUIRE(test_solve(ctl.solve({{Id("a"), true}, {Id("b"), false}}), models).is_satisfiable());
            REQUIRE(models == (ModelVec{{Id("a")}, {Id("a"), Id("c")}}));
            REQUIRE(messages.empty());
        }
        SECTION("theory-atoms") {
            char const *theory =
                "#theory t {\n"
                "  group {\n"
                "    + : 4, unary;\n"
                "    - : 4, unary;\n"
                "    ^ : 3, binary, right;\n"
                "    * : 2, binary, left;\n"
                "    + : 1, binary, left;\n"
                "    - : 1, binary, left\n"
                "  };\n"
                "  &a/0 : group, head;\n"
                "  &b/0 : group, {=}, group, directive\n"
                "}.\n"
                "{p; q}.\n"
                "&a { 1,[1,a],f(1) : p, q }.\n";
            ctl.add("base", {}, theory);
            ctl.ground({{"base", {}}});
            auto atoms = ctl.theory_atoms();
            REQUIRE(atoms.size() == 1);
            int count = 0;
            for (auto atom : atoms) {
                ++count;
                REQUIRE(atom.to_string() == "&a{1,[1,a],f(1): p,q}");
                REQUIRE(!atom.has_guard());
                REQUIRE(atom.term().type() == TheoryTermType::Symbol);
                REQUIRE(atom.term().name() == S("a"));
                REQUIRE(atom.elements().size() == 1);
                REQUIRE(atom.literal() > 0);
                for (auto elem : atom.elements()) {
                    ++count;
                    REQUIRE(elem.condition_id() > 0);
                    REQUIRE(elem.tuple().size() == 3);
                    auto it = elem.tuple().begin();
                    REQUIRE(it->type() == TheoryTermType::Number);
                    REQUIRE(it->number() == 1);
                    ++it;
                    REQUIRE(it->type() == TheoryTermType::List);
                    REQUIRE(it->arguments().size() == 2);
                    auto jt = it->arguments().begin();
                    REQUIRE(jt->type() == TheoryTermType::Number);
                    REQUIRE((*++jt).type() == TheoryTermType::Symbol);
                    ++it;
                    REQUIRE(it->type() == TheoryTermType::Function);
                    REQUIRE(it->name() == S("f"));
                    REQUIRE(elem.condition().size() == 2);
                    auto kt = elem.condition().begin();
                    REQUIRE(*kt++ > 0);
                    REQUIRE(*kt++ > 0);
                    REQUIRE(kt == elem.condition().end());
                }
            }
            REQUIRE(count == 2);
            REQUIRE(test_solve(ctl.solve({{Id("p"), true}, {Id("q"), true}}), models).is_satisfiable());
            REQUIRE(models == ModelVec({{Id("p"), Id("q")}}));
            REQUIRE(atoms.size() == 0);
            ctl.add("next", {}, "&b {} = 42.");
            ctl.ground({{"next", {}}});
            REQUIRE(atoms.size() == 1);
            auto atom = *atoms.begin();
            REQUIRE(atom.term().name() == S("b"));
            REQUIRE(atom.has_guard());
            REQUIRE(atom.guard().first == S("="));
            REQUIRE(atom.guard().second.number() == 42);
            REQUIRE(atom.literal() == 0);
        }
        SECTION("theory-not") {
            char const *theory =
                "#theory t {\n"
                "  term {\n"
                "    not : 0, unary;\n"
                "    not : 1, binary, left\n"
                "  };\n"
                "  &atom/0 : term, directive\n"
                "}.\n"
                "&atom { not (1 not 2) }.\n";
            ctl.add("base", {}, theory);
            ctl.ground({{"base", {}}});
            auto atoms = ctl.theory_atoms();
            REQUIRE(atoms.size() == 1);
            REQUIRE(atoms.begin()->to_string() == "&atom{(not (1 not 2))}");
        }
        SECTION("symbolic atoms") {
            ctl.add("base", {}, "p(1). {p(2)}. #external p(3). q.");
            ctl.ground({{"base", {}}});
            REQUIRE(test_solve(ctl.solve(), models).is_satisfiable());
            REQUIRE(messages.empty());
            auto atoms = ctl.symbolic_atoms();
            Symbol p1 = Function("p", {Number(1)}), p2 = Function("p", {Number(2)}), p3 = Function("p", {Number(3)}), q = Id("q");
            REQUIRE( atoms.find(p1)->is_fact()); REQUIRE(!atoms.find(p1)->is_external());
            REQUIRE(!atoms.find(p2)->is_fact()); REQUIRE(!atoms.find(p2)->is_external());
            REQUIRE(!atoms.find(p3)->is_fact()); REQUIRE( atoms.find(p3)->is_external());
            REQUIRE(atoms.length() == 4);
            SymbolVector symbols;
            for (auto atom : atoms) { symbols.emplace_back(atom.symbol()); }
            std::sort(symbols.begin(), symbols.end());
            REQUIRE(symbols == SymbolVector({q, p1, p2, p3}));
            symbols.clear();
            for (auto it = atoms.begin(Signature("p", 1)); it; ++it) { symbols.emplace_back(it->symbol()); }
            std::sort(symbols.begin(), symbols.end());
            REQUIRE(symbols == SymbolVector({p1, p2, p3}));
        }
        SECTION("incremental") {
            ctl.add("base", {}, "#external query(0).");
            ctl.add("acid", {"k"}, "#external query(k).");
            ctl.ground({{"base", {}}});
            ctl.assign_external(Function("query", {Number(0)}), TruthValue::True);
            REQUIRE(test_solve(ctl.solve(), models).is_satisfiable());
            REQUIRE(models == (ModelVec{{Function("query", {Number(0)})}}));
            REQUIRE(messages.empty());

            ctl.ground({{"acid", {Number(1)}}});
            ctl.release_external(Function("query", {Number(0)}));
            ctl.assign_external(Function("query", {Number(1)}), TruthValue::Free);
            REQUIRE(test_solve(ctl.solve(), models).is_satisfiable());
            REQUIRE(models == (ModelVec{{}, {Function("query", {Number(1)})}}));
            REQUIRE(messages.empty());
        }
        SECTION("solve_iter") {
            static int n = 0;
            if (++n < 3) { // workaround for some bug with catch
                ctl.add("base", {}, "a.");
                ctl.ground({{"base", {}}});
                {
                    auto iter = ctl.solve();
                    {
                        MCB mcb(models);
                        SECTION("c++") { for (auto &m : iter) { mcb(m); }; }
                        SECTION("java") { while (auto &m = iter.next()) { mcb(m); }; }
                    }
                    REQUIRE(models == ModelVec{{Id("a")}});
                    REQUIRE(iter.get().is_satisfiable());
                }
                REQUIRE(test_solve(ctl.solve(), models).is_satisfiable());
                REQUIRE(models == ModelVec{{Id("a")}});
                REQUIRE(messages.empty());
            }
        }
        SECTION("logging") {
            ctl.add("base", {}, "a(1+a).");
            ctl.ground({{"base", {}}});
            REQUIRE(test_solve(ctl.solve(), models).is_satisfiable());
            REQUIRE(models == ModelVec{{}});
            REQUIRE(messages == MessageVec({{WarningCode::OperationUndefined, "<block>:1:3-6: info: operation undefined:\n  (1+a)\n"}}));
        }
        SECTION("enable_enumeration_assumption") {
            ctl.add("base", {}, "{p;q}.");
            ctl.ground({{"base", {}}});
            ctl.enable_enumeration_assumption(false);
            REQUIRE(test_solve(ctl.solve(), models).is_satisfiable());
            REQUIRE(models.size() == 4);
            REQUIRE(test_solve(ctl.solve(), models).is_satisfiable());
            REQUIRE(models.size() <= 4);
        }
        SECTION("enable_enumeration_assumption") {
            ctl.add("base", {}, "{p;q}.");
            ctl.ground({{"base", {}}});
            ctl.enable_enumeration_assumption(false);
            REQUIRE(test_solve(ctl.solve(), models).is_satisfiable());
            REQUIRE(models.size() == 4);
            REQUIRE(test_solve(ctl.solve(), models).is_satisfiable());
            REQUIRE(models.size() <= 4);
        }
        SECTION("cleanup") {
            ctl.add("base", {}, "{a}. :- a.");
            ctl.ground({{"base", {}}});
            auto dom = ctl.symbolic_atoms();
            REQUIRE(dom.find(Id("a")) != dom.end());
            REQUIRE(test_solve(ctl.solve(), models).is_satisfiable());
            REQUIRE(models.size() == 1);
            ctl.cleanup();
            REQUIRE(dom.find(Id("a")) == dom.end());
            REQUIRE(test_solve(ctl.solve(), models).is_satisfiable());
            REQUIRE(models.size() == 1);
        }
        SECTION("cleanup again") {
            ctl.add("base", {}, R"(
                p(X) :- not q(X), X=2..3.
                q(X) :- not p(X), X=1..2.

                a :- not p(3).
                b :- not q(1).)");

            ctl.ground({{"base", {}}});
            ctl.cleanup(); // no cleanup with current implementation
            auto sas = ctl.symbolic_atoms();
            auto itA = sas.find(Clingo::Id("a"));
            auto itB = sas.find(Clingo::Id("b"));
            REQUIRE((itA != sas.end() || itB != sas.end()));
            REQUIRE((itA == sas.end() || itB == sas.end()));

            ctl.configuration()["solve"]["solve_limit"] = "1";
            ctl.solve();
            ctl.cleanup();
            itA = sas.find(Clingo::Id("a"));
            itB = sas.find(Clingo::Id("b"));
            REQUIRE((itA == sas.end() && itB == sas.end()));
        }
        SECTION("const") {
            ctl.add("base", {}, "#const a=10.");
            REQUIRE(ctl.has_const("a"));
            REQUIRE(!ctl.has_const("b"));
            REQUIRE(ctl.get_const("a") == Number(10));
            REQUIRE(ctl.get_const("b") == Id("b"));
        }
#if defined(CLASP_HAS_THREADS) && CLASP_HAS_THREADS == 1
        SECTION("async and cancel") {
            static int n = 0;
            if (++n < 3) { // workaround for some bug with catch
                ctl.add("pigeon", {"p", "h"}, "1 {p(P,H) : P=1..p}1 :- H=1..h."
                                              "1 {p(P,H) : H=1..h}1 :- P=1..p.");
                ctl.ground({{"pigeon", {Number(21), Number(20)}}});
                int fcalled = 0;
                SolveResult fret;
                FinishCallback fh = [&fret, &fcalled](SolveResult ret) { ++fcalled; fret = ret; };
                auto async = ctl.solve_async(nullptr, fh);
                REQUIRE(!async.wait(0.01));
                SECTION("interrupt") { ctl.interrupt(); }
                SECTION("cancel") { async.cancel(); }
                auto ret = async.get();
                REQUIRE((ret.is_unknown() && ret.is_interrupted()));
                REQUIRE(fcalled == 1);
                REQUIRE(fret == ret);
            }
        }
#endif
        SECTION("ground callback") {
            ctl.add("base", {}, "a(@f(X)):- X=1..2. b(@f()).");
            ctl.ground({{"base", {}}}, [&messages](Location loc, char const *name, SymbolSpan args, SymbolSpanCallback report) {
                if (strcmp(name, "f") == 0 && args.size() == 1) {
                    Symbol front = *args.begin();
                    report({Number(front.number() + 1), Number(front.number() + 10)});
                    report({Number(front.number() + 20)});
                }
                else {
                    std::ostringstream oss;
                    oss << "invalid call: " << name << "/" << args.size() << " at " << loc;
                    messages.emplace_back(WarningCode::OperationUndefined, oss.str());
                }
            });
            REQUIRE(test_solve(ctl.solve(), models).is_satisfiable());
            REQUIRE(models == ModelVec({{Function("a", {Number(2)}), Function("a", {Number(3)}), Function("a", {Number(11)}), Function("a", {Number(12)}), Function("a", {Number(21)}), Function("a", {Number(22)})}}));
            REQUIRE(messages == MessageVec({{WarningCode::OperationUndefined, "invalid call: f/0 at <block>:1:22-26"}}));
        }
        SECTION("ground callback fail") {
            ctl.add("base", {}, "a(@f()).");
            REQUIRE_THROWS_AS(ctl.ground({{"base", {}}}, [](Location, char const *, SymbolSpan, SymbolSpanCallback) { throw std::runtime_error("fail"); }), std::runtime_error);
        }
        SECTION("ground program observer") {
            std::vector<std::string> trail;
            Observer obs(trail);
            ctl.register_observer(obs);
            ctl.add("base", {}, "a :- not a.");
            ctl.ground({{"base", {}}});
            REQUIRE(test_solve(ctl.solve(), models).is_unsatisfiable());
            REQUIRE(models == ModelVec({}));
            REQUIRE(trail == std::vector<std::string>({"IP: incremental", "BS", "R: 1:-~1", "ES"}));
        }
        SECTION("theory data bug") {
            struct Observer : Clingo::GroundProgramObserver {
                Observer(std::vector<id_t> &atoms) : atoms_{atoms} { }
                void theory_atom(id_t atom_id_or_zero, id_t , IdSpan ) override {
                    atoms_.emplace_back(atom_id_or_zero);
                }
                std::vector<id_t> &atoms_;
            };

            std::vector<id_t> atoms;
            Observer o{atoms};
            ctl.register_observer(o);
            ctl.add("base",{}, R"(#theory csp {
                                   dom_term {};
                                   &dom/0 : dom_term, any}.)");
            ctl.add("base",{}, "&dom {0}. &dom {1}.");
            ctl.add("acid",{}, "&dom {1}. &dom {2}.");
            REQUIRE(atoms == std::vector<id_t>({}));
            ctl.ground({{"base", {}}});
            REQUIRE(atoms == std::vector<id_t>({1, 2}));
            ctl.ground({{"acid", {}}});
            REQUIRE(atoms == std::vector<id_t>({1, 2, 3}));
            ctl.cleanup();
            REQUIRE(atoms == std::vector<id_t>({1, 2, 3}));
            { ctl.backend(); }
            REQUIRE(atoms == std::vector<id_t>({1, 2, 3}));
            { ctl.backend(); }
            REQUIRE(atoms == std::vector<id_t>({1, 2, 3}));
        }
        SECTION("events") {
            ctl.add("base", {}, "{a}.");
            ctl.ground({{"base", {}}});
            int m = 0;
            int f = 0;
            bool goon = true;
            SECTION("stop") { goon = false; }
            SECTION("goon") { goon = true; }
            struct EH : Clingo::SolveEventHandler {
                EH(bool goon, int &m, int &f) : goon(goon), m(m), f(f) { }
                bool on_model(Model &) override {
                    REQUIRE(f == 0);
                    ++m;
                    return goon;
                }
                void on_finish(SolveResult) override {
                    ++f;
                }
                bool goon;
                int &m;
                int &f;
            };
            EH handler{goon, m, f};
            auto handle = ctl.solve(LiteralSpan{}, &handler);
            REQUIRE(test_solve(std::move(handle), models).is_satisfiable());
            REQUIRE(m == (goon ? 2 : 1));
            REQUIRE(f == 1);
        }
        SECTION("on_unsat") {
            struct EH : Clingo::SolveEventHandler {
                EH(std::vector<int64_t> &v) : v(v) { }
                void on_unsat(Clingo::Span<int64_t> lower_bound) override {
                    REQUIRE(lower_bound.size() == 1);
                    v.push_back(lower_bound.back());
                }
                std::vector<int64_t> &v;
            };
            std::vector<int64_t> values;
            EH handler{values};
            auto conf = ctl.configuration();
            conf["solver"]["opt_strategy"] = "usc,oll,0";
            ctl.add("base", {}, "1 { p(X); q(X) } 1 :- X=1..3. #minimize { 1,p,X: p(X); 1,q,X: q(X) }.");
            ctl.ground({{"base", {}}});
            auto handle = ctl.solve(LiteralSpan{}, &handler);
            REQUIRE(test_solve(std::move(handle), models).is_satisfiable());
            REQUIRE(values == std::vector<int64_t>{1,2,3});
        }
        SECTION("pos_strat") {
            ctl.with_backend([](Clingo::Backend &b){
                b.rule(true, {b.add_atom(Clingo::Function("a", {Clingo::Number(1)}))}, {});
                b.rule(true, {b.add_atom(Clingo::Function("a", {Clingo::Number(2)}))}, {});
            });
            ctl.add("base", {}, ":- a(X).");
            ctl.ground({{"base", {}}});
            REQUIRE(test_solve(ctl.solve(), models).is_satisfiable());
            REQUIRE(models == ModelVec({{}}));
        }
        SECTION("bug_classical_1") {
            ctl.add("a", {}, "-a.");
            ctl.add("b", {}, "a.");

            ctl.ground({{"a", {}}});
            REQUIRE(test_solve(ctl.solve(), models).is_satisfiable());
            REQUIRE(models == ModelVec({{Function("a", {}, false)}}));

            ctl.ground({{"b", {}}});
            REQUIRE(test_solve(ctl.solve(), models).is_unsatisfiable());
            REQUIRE(models == ModelVec({}));
        }
        SECTION("bug_classical_2") {
            ctl.add("a", {}, "-a.");
            ctl.add("b", {}, "a.");

            ctl.with_backend([](Backend &bck) {
                auto a = bck.add_atom(Function("a", {}, true));
                auto b = bck.add_atom(Function("a", {}, false));
                bck.rule(true, {a, b}, {});
            });

            REQUIRE(test_solve(ctl.solve(), models).is_satisfiable());
            REQUIRE(models == ModelVec({{}, {Function("a", {}, true)}, {Function("a", {}, false)}}));
        }
        SECTION("bug_on_model") {
            class OnModel : public SolveEventHandler {
            public:
                OnModel(std::vector<std::string> &events) : events_{events} { }
                bool on_model(Model &model) override {
                    static_cast<void>(model);
                    events_.emplace_back("on_model");
                    return true;
                }
            private:
                std::vector<std::string> &events_;
            };

            std::vector<std::string> events;
            OnModel handler{events};
            {
                auto hnd = ctl.solve(LiteralSpan{}, &handler, true, true);
                for (auto &&m : hnd) {
                    events.emplace_back("on_yield");
                }
                hnd.get();
            }
            REQUIRE(events == std::vector<std::string>{"on_model", "on_yield"});
            events.clear();
            {
                auto hnd = ctl.solve(LiteralSpan{}, &handler, false, true);
                for (auto &&m : hnd) {
                    events.emplace_back("on_yield");
                }
                hnd.get();
            }
            REQUIRE(events == std::vector<std::string>{"on_model", "on_yield"});
        }
    }
    SECTION("with single-shot control") {
        MessageVec messages;
        ModelVec models;
        Control ctl{{"0", "--single-shot"}, [&messages](WarningCode code, char const *msg) { messages.emplace_back(code, msg); }, 20};
        SECTION("single-shot") {
            ctl.add("step", {"k"}, "p(k).");
            ctl.ground({{"step", {Number(1)}}});
            REQUIRE(test_solve(ctl.solve(), models).is_satisfiable());
            REQUIRE(models == ModelVec({{Function("p", {Number(1)})}}));
            REQUIRE_THROWS_AS(ctl.ground({{"step", {Number(2)}}}), std::logic_error);
        }
    }
}

} } // namespace Test Clingo
