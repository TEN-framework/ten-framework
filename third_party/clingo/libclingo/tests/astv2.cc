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
#include <iostream>
#include <fstream>
#include <sstream>

namespace Clingo { namespace Test {

namespace {

template <class V>
std::vector<typename V::value_type> vec(V const &vec) {
    std::vector<typename V::value_type> ret;
    for (auto &&x : vec) {
        ret.emplace_back(x);
    }
    return ret;
}

template <class V, class W>
bool cmp(V const &a, W const &b) {
    return std::equal(a.begin(), a.end(), b.begin(), b.end(), [](auto &&x, auto &&y) {
        return std::strcmp(x, y) == 0;
    });
}

struct CB {
    CB(std::string &ret)
    : ret(ret) { ret.clear(); }
    void operator()(AST::Node const &x) {
        std::ostringstream oss;
        oss << x;
        if (first) {
            REQUIRE(oss.str() == "#program base.");
            first = false;
        }
        else {
            if (!ret.empty()) { ret+= "\n"; }
            ret+= oss.str();
        }
    }
    bool first = true;
    std::string &ret;
};

std::string parse(char const *prg) {
    std::string ret;
    AST::parse_string(prg, CB(ret));
    return ret;
}

std::string unpool(char const *prg) {
    std::string ret;
    auto cb = CB(ret);
    AST::parse_string(prg, [&](AST::Node const &ast) {
        for (auto &unpooled : ast.unpool()) {
            cb(unpooled);
        }
    });
    return ret;
}

ModelVec solve(char const *prg, PartSpan parts = {{"base", {}}}) {
    MessageVec messages;
    ModelVec models;
    Control ctl{{"0"}, [&messages](WarningCode code, char const *msg) { messages.emplace_back(code, msg); }};

    AST::with_builder(ctl, [prg](AST::ProgramBuilder &b){
        AST::parse_string(prg, [&b](AST::Node const &stm) { b.add(stm); });
    });
    ctl.ground(parts);
    test_solve(ctl.solve(), models);
    return models;
}

using StringVec = std::vector<std::string>;
StringVec parse_theory(char const *prg, char const *theory) {
    MessageVec messages;
    ModelVec models;
    Control ctl{{"0"}, [&messages](WarningCode code, char const *msg) { messages.emplace_back(code, msg); }};
    AST::with_builder(ctl, [prg, theory](AST::ProgramBuilder &b){
        AST::parse_string(theory, [&b](AST::Node const &ast) { b.add(ast); });
        AST::parse_string(prg, [&b](AST::Node const &ast) { b.add(ast); });
    });
    ctl.ground({{"base", {}}});
    StringVec ret;
    for (auto x : ctl.theory_atoms()) {
        std::ostringstream oss;
        oss << x;
        ret.emplace_back(oss.str());
    }
    return ret;
}

} // namespace

TEST_CASE("parse-ast-v2", "[clingo]") {
    SECTION("statement") {
        REQUIRE(parse("a.") == "a.");
        REQUIRE(parse("a:-b.") == "a :- b.");
        REQUIRE(parse("#const a=10. [override]") == "#const a = 10. [override]");
        REQUIRE(parse("#const a=10. [default]") == "#const a = 10.");
        REQUIRE(parse("#const a=10.") == "#const a = 10.");
        REQUIRE(parse("#show a/1.") == "#show a/1.");
        REQUIRE(parse("#show a : b.") == "#show a : b.");
        REQUIRE(parse("#minimize{ 1:b }.") == ":~ b. [1@0]");
        REQUIRE(parse("#script (python)\n42\n  #end.") == "#script (python)\n42\n#end.");
        REQUIRE(parse("#script (lua) \n42\n #end.") == "#script (lua)\n42\n#end.");
        REQUIRE(parse("#script (other)  \n42\n#end.") == "#script (other)\n42\n#end.");
        REQUIRE(parse("#program p(k).") == "#program p(k).");
        REQUIRE(parse("#external p(k).") == "#external p(k). [false]");
        REQUIRE(parse("#external p(k). [true]") == "#external p(k). [true]");
        REQUIRE(parse("#external p(k) : a, b.") == "#external p(k) : a; b. [false]");
        REQUIRE(parse("#edge (u,v) : a, b.") == "#edge (u,v) : a; b.");
        REQUIRE(parse("#heuristic a : b, c. [L@P,level]") == "#heuristic a : b; c. [L@P,level]");
        REQUIRE(parse("#project a : b.") == "#project a : b.");
        REQUIRE(parse("#project a/2.") == "#project a/2.");
        REQUIRE(parse("#theory x {}.") == "#theory x {\n}.");
        REQUIRE(parse("%* test *%\n") == "%* test *%");
        REQUIRE(parse("%* test *%") == "%* test *%");
        REQUIRE(parse("% test\n") == "% test");
        REQUIRE(parse("% test") == "% test");
    }
    SECTION("theory definition") {
        REQUIRE(parse("#theory x { t { ++ : 1, unary } }.") == "#theory x {\n" "  t {\n" "    ++ : 1, unary\n" "  }\n" "}.");
        REQUIRE(parse("#theory x { &a/0: t, any }.") == "#theory x {\n  &a/0: t, any\n}.");
        REQUIRE(parse("#theory x { &a/0: t, {+, -}, u, any }.") == "#theory x {\n  &a/0: t, { +, - }, u, any\n}.");
    }
    SECTION("body literal") {
        REQUIRE(parse(":-a.") == "#false :- a.");
        REQUIRE(parse(":-not a.") == "#false :- not a.");
        REQUIRE(parse(":-not not a.") == "#false :- not not a.");
        REQUIRE(parse(":-a:b.") == "#false :- a: b.");
        REQUIRE(parse(":-a:b,c;d.") == "#false :- a: b, c; d.");
        REQUIRE(parse(":-1{a:b,c;e}2.") == "#false :- 1 <= { a: b, c; e } <= 2.");
        REQUIRE(parse(":-{a:b,c;e}2.") == "#false :- 2 >= { a: b, c; e }.");
        REQUIRE(parse(":-1#min{1,2:b,c;1:e}2.") == "#false :- 1 <= #min { 1,2: b, c; 1: e } <= 2.");
        REQUIRE(parse(":-&p { 1: a,b; 2: c }.") == "#false :- &p { 1: a, b; 2: c }.");
    }
    SECTION("head literal") {
        REQUIRE(parse("a.") == "a.");
        REQUIRE(parse("a:b.") == "a: b.");
        REQUIRE(parse("a:b,c;d.") == "a: b, c; d.");
        REQUIRE(parse("1{a:b,c;e}2.") == "1 <= { a: b, c; e } <= 2.");
        REQUIRE(parse("{a:b,c;e}2.") == "2 >= { a: b, c; e }.");
        REQUIRE(parse("1#min{1,2:h:b,c;1:e}2.") == "1 <= #min { 1,2: h: b, c; 1: e } <= 2.");
        REQUIRE(parse("&p { 1 : a,b; 2 : c }.") == "&p { 1: a, b; 2: c }.");
        REQUIRE(parse("&p { 1 : a,b; 2 : c } ** 33.") == "&p { 1: a, b; 2: c } ** 33.");
    }
    SECTION("literal") {
        REQUIRE(parse("#true.") == "#true.");
        REQUIRE(parse("#false.") == "#false.");
        REQUIRE(parse("a.") == "a.");
        REQUIRE(parse("not a.") == "not a.");
        REQUIRE(parse("not not a.") == "not not a.");
        REQUIRE(parse("1 != 3.") == "1 != 3.");
    }
    SECTION("terms") {
        REQUIRE(parse("p(a).") == "p(a).");
        REQUIRE(parse("p(X).") == "p(X).");
        REQUIRE(parse("p(-a).") == "p(-a).");
        REQUIRE(parse("p(~a).") == "p(~a).");
        REQUIRE(parse("p(|a|).") == "p(|a|).");
        REQUIRE(parse("p((a+b)).") == "p((a+b)).");
        REQUIRE(parse("p((a-b)).") == "p((a-b)).");
        REQUIRE(parse("p((a*b)).") == "p((a*b)).");
        REQUIRE(parse("p((a/b)).") == "p((a/b)).");
        REQUIRE(parse("p((a\\b)).") == "p((a\\b)).");
        REQUIRE(parse("p((a?b)).") == "p((a?b)).");
        REQUIRE(parse("p((a^b)).") == "p((a^b)).");
        REQUIRE(parse("p(a..b).") == "p((a..b)).");
        REQUIRE(parse("p((),(1,),f(),f(1,2)).") == "p((),(1,),f,f(1,2)).");
        REQUIRE(parse("p(@f(a;b)).") == "p(@f(a;b)).");
        REQUIRE(parse("p(a;b).") == "p(a;b).");
        REQUIRE(parse("p((a,;b)).") == "p(((a,);b)).");
        REQUIRE(parse("p(((a,);b)).") == "p(((a,);b)).");
    }
    SECTION("theory terms") {
        REQUIRE(parse("&p{ } !! a.") == "&p { } !! a.");
        REQUIRE(parse("&p{ } !! X.") == "&p { } !! X.");
        REQUIRE(parse("&p{ } !! [].") == "&p { } !! [].");
        REQUIRE(parse("&p{ } !! [1].") == "&p { } !! [1].");
        REQUIRE(parse("&p{ } !! [1,2].") == "&p { } !! [1,2].");
        REQUIRE(parse("&p{ } !! ().") == "&p { } !! ().");
        REQUIRE(parse("&p{ } !! (a).") == "&p { } !! a.");
        REQUIRE(parse("&p{ } !! (a,).") == "&p { } !! (a,).");
        REQUIRE(parse("&p{ } !! {}.") == "&p { } !! {}.");
        REQUIRE(parse("&p{ } !! f().") == "&p { } !! f.");
        REQUIRE(parse("&p{ } !! f(a,1).") == "&p { } !! f(a,1).");
        REQUIRE(parse("&p{ } !! 1 + (x + y * z).") == "&p { } !! (1 + (x + y * z)).");
    }
}

TEST_CASE("add-ast-v2", "[clingo]") {
    SECTION("statement") {
        REQUIRE(solve("a.") == ModelVec({{Id("a")}}));
        REQUIRE(solve("a. c. b :- a, c.") == ModelVec({{Id("a"), Id("b"), Id("c")}}));
        REQUIRE(solve("#const a=10. p(a).") == ModelVec({{Function("p", {Number(10)})}}));
        REQUIRE(solve("a. b. #show a/0.") == ModelVec({{Id("a")}}));
        REQUIRE(solve("a. #show b : a.") == ModelVec({{Id("a"), Id("b")}}));
        REQUIRE(solve("#minimize{ 1:b; 2:a }. {a;b}. :- not a, not b.") == ModelVec({{Id("b")}}));
#ifdef WITH_LUA
        // NOTE: at the moment it is not possible to add additional script code - it is simply ignored
        REQUIRE(solve("#script (lua) function x() return 32; end #end. p(@x()).") == ModelVec({SymbolVector{}}));
#endif
        REQUIRE(solve("#edge (u,v) : a. #edge (v,u) : b. {a;b}.") == ModelVec({{}, {Id("a")}, {Id("b")}}));
        REQUIRE(solve("#theory x {}.") == ModelVec({SymbolVector{}}));
        // these just test parsing...
        REQUIRE(solve("#external a.") == ModelVec({SymbolVector{}}));
        REQUIRE(solve("#heuristic a : b, c. [1@2,level]") == ModelVec({SymbolVector{}}));
        REQUIRE(solve("#project a.") == ModelVec({SymbolVector{}}));
        REQUIRE(solve("#project a/0.") == ModelVec({SymbolVector{}}));
    }
    SECTION("body literal") {
        REQUIRE(solve("{a}. :-a.") == ModelVec({SymbolVector{}}));
        REQUIRE(solve("{a}. :-not a.") == ModelVec({{Id("a")}}));
        REQUIRE(solve("{a}. :-not not a.") == ModelVec({SymbolVector{}}));
        REQUIRE(solve(":-a:b.") == ModelVec({}));
        REQUIRE(solve(":-0{a:b;c}1. {a;b;c}.") == ModelVec({{Id("a"), Id("b"), Id("c")}}));
        REQUIRE(solve(":-0#min{1,2:a,b;2:c}2. {a;b;c}.") == ModelVec({{}, {Id("a")}, {Id("b")}}));
    }
    SECTION("head literal") {
        REQUIRE(solve("a.") == ModelVec({{Id("a")}}));
        REQUIRE(solve("not a.") == ModelVec({SymbolVector{}}));
        REQUIRE(solve("not not a.") == ModelVec({}));
        REQUIRE(solve("a:b;c.{b}.") == ModelVec({{Id("a"), Id("b")}, {Id("b"), Id("c")}, {Id("c")}}));
        REQUIRE(solve("1{a:b;b}2.") == ModelVec({{Id("a"), Id("b")}, {Id("b")}}));
        REQUIRE(solve("#min{1,2:a;2:c}1.") == ModelVec({{Id("a")}, {Id("a"), Id("c")}}));
    }
    SECTION("literal") {
        REQUIRE(solve("a.") == ModelVec({{Id("a")}}));
        REQUIRE(solve("1=1.") == ModelVec({SymbolVector{}}));
        REQUIRE(solve("1!=1.") == ModelVec({}));
        REQUIRE(solve("#true.") == ModelVec({SymbolVector{}}));
        REQUIRE(solve("#false.") == ModelVec({}));
    }
    SECTION("terms") {
        auto t = [](char const *s) { return ModelVec({{parse_term(s)}}); };
        auto tt = [](std::initializer_list<char const *> l) {
            SymbolVector ret;
            for (auto const &s : l) { ret.emplace_back(parse_term(s)); }
            return ModelVec{ret};
        };
        REQUIRE(solve("p(a).") == t("p(a)"));
        REQUIRE(solve("p(X) :- X=a.") == t("p(a)"));
        REQUIRE(solve("p(-1).") == t("p(-1)"));
        REQUIRE(solve("p(|1|).") == t("p(|1|)"));
        REQUIRE(solve("p((3+2)).") == t("p(5)"));
        REQUIRE(solve("p((3-2)).") == t("p(1)"));
        REQUIRE(solve("p((3*2)).") == t("p(6)"));
        REQUIRE(solve("p((7/2)).") == t("p(3)"));
        REQUIRE(solve("p((7\\2)).") == t("p(1)"));
        REQUIRE(solve("p((7?2)).") == t("p(7)"));
        REQUIRE(solve("p((7^2)).") == t("p(5)"));
        REQUIRE(solve("p(3..3).") == t("p(3)"));
        REQUIRE(solve("p(a;b).") == tt({"p(a)", "p(b)"}));
        REQUIRE(solve("p((),(1,),f(),f(1,2)).") == t("p((),(1,),f,f(1,2))"));
        REQUIRE(solve("p((a,;b)).") == tt({"p(b)", "p((a,))"}));
    }
    SECTION("theory") {
        char const *theory = R"(
#theory x {
    t {
        * : 1, binary, left;
        ^ : 2, binary, right;
        - : 3, unary
    };
    &a/0 : t, directive;
    &b/0 : t, {=}, t, any
}.
)";
        REQUIRE(parse("&p{ } !! 1 + (x + y * z).") == "&p { } !! (1 + (x + y * z)).");
        REQUIRE(parse_theory("&a{}.", theory) == StringVec({"&a{}"}));
        REQUIRE(parse_theory("&a{1,2,3:a,b}. {a;b}.", theory) == StringVec({"&a{1,2,3: a,b}"}));
        REQUIRE(parse_theory("&b{} = a.", theory) == StringVec({"&b{}=a"}));
        REQUIRE(parse_theory("&b{} = X:-X=1.", theory) == StringVec({"&b{}=1"}));
        REQUIRE(parse_theory("&b{} = [].", theory) == StringVec({"&b{}=[]"}));
        REQUIRE(parse_theory("&b{} = [1].", theory) == StringVec({"&b{}=[1]"}));
        REQUIRE(parse_theory("&b{} = [1,2].", theory) == StringVec({"&b{}=[1,2]"}));
        REQUIRE(parse_theory("&b{} = ().", theory) == StringVec({"&b{}=()"}));
        REQUIRE(parse_theory("&b{} = (a).", theory) == StringVec({"&b{}=a"}));
        REQUIRE(parse_theory("&b{} = (a,).", theory) == StringVec({"&b{}=(a,)"}));
        REQUIRE(parse_theory("&b{} = {}.", theory) == StringVec({"&b{}={}"}));
        REQUIRE(parse_theory("&b{} = f().", theory) == StringVec({"&b{}=f()"}));
        REQUIRE(parse_theory("&b{} = f(a,1).", theory) == StringVec({"&b{}=f(a,1)"}));
        REQUIRE(parse_theory("&b{} = a*x.", theory) == StringVec({"&b{}=(a*x)"}));
        REQUIRE(parse_theory("&b{} = -a*x*y^z^u.", theory) == StringVec({"&b{}=(((-a)*x)*(y^(z^u)))"}));
        REQUIRE(parse_theory("&b{} = -(a*x*y^z^u).", theory) == StringVec({"&b{}=(-((a*x)*(y^(z^u))))"}));
    }
}

TEST_CASE("build-ast-v2", "[clingo]") {
    Location loc{"<string>", "<string>", 1, 1, 1, 1};
    SECTION("string array") {
        auto sym = AST::Node{AST::Type::SymbolicTerm, loc, Function("a", {})};
        auto lst = std::vector<char const *>{"x", "y", "z"};
        auto tue = AST::Node{AST::Type::TheoryUnparsedTermElement, lst, sym};

        auto seq = tue.get(AST::Attribute::Operators).get<AST::StringVector>();
        REQUIRE(!seq.empty());
        REQUIRE(seq.size() == 3);
        REQUIRE(cmp(seq, lst));
        seq.insert(seq.begin(), "i");
        REQUIRE(std::strcmp(seq[0], "i") == 0);
        REQUIRE(seq.size() == 4);
        seq.insert(seq.begin(), seq[3]);
        REQUIRE(std::strcmp(seq[0], "z") == 0);
        seq.erase(seq.begin());
        seq.erase(seq.begin());
        REQUIRE(cmp(seq, lst));
        tue.set(AST::Attribute::Operators, seq);
        REQUIRE(cmp(seq, lst));
        tue.set(AST::Attribute::Operators, tue.deep_copy().get(AST::Attribute::Operators));
        REQUIRE(cmp(seq, lst));
        tue.set(AST::Attribute::Operators, tue.copy().get(AST::Attribute::Operators));
        REQUIRE(cmp(seq, lst));
    }
    SECTION("ast array") {
        auto lst = std::vector<AST::Node>{
            AST::Node(AST::Type::Id, loc, "x"),
            AST::Node(AST::Type::Id, loc, "y"),
            AST::Node(AST::Type::Id, loc, "z")};
        auto prg = AST::Node(AST::Type::Program, loc, "p", lst);
        auto seq = prg.get(AST::Attribute::Parameters).get<AST::NodeVector>();
        REQUIRE(seq.size() == 3);
        REQUIRE(vec(seq) == lst);
        REQUIRE(seq[0] == lst[0]);
        seq.insert(seq.begin(), AST::Node(AST::Type::Id, loc, "i"));
        lst.insert(lst.begin(), AST::Node(AST::Type::Id, loc, "i"));
        REQUIRE(vec(seq) == lst);
        seq.insert(seq.begin(), seq[3]);
        lst.insert(lst.begin(), lst[3]);
        REQUIRE(vec(seq) == lst);
        seq.erase(seq.begin() + 2);
        lst.erase(lst.begin() + 2);
        REQUIRE(vec(seq) == lst);
    }
    SECTION("ast compare") {
        Location alt{"<string>", "<string>", 1, 1, 1, 2};
        auto x = AST::Node(AST::Type::Id, loc, "x");
        auto y = AST::Node(AST::Type::Id, alt, "x");
        auto z = AST::Node(AST::Type::Id, loc, "z");
        REQUIRE(x == y);
        REQUIRE(x == x);
        REQUIRE(x != z);
        REQUIRE(x.hash() == x.hash());
        REQUIRE(x.hash() == y.hash());
        REQUIRE(x.hash() != z.hash());
        REQUIRE(x < z);
        REQUIRE(x <= z);
        REQUIRE(z > x);
        REQUIRE(z >= x);
        REQUIRE(y <= x);
        REQUIRE(y >= x);
        REQUIRE(x <= y);
        REQUIRE(x >= y);
    }
}

TEST_CASE("unpool-ast-v2", "[clingo]") {
    SECTION("terms") {
        REQUIRE(unpool("a(f(1;2)).") ==
            "a(f(1)).\n"
            "a(f(2)).");
        REQUIRE(unpool("a((1,;2,)).") ==
            "a((1,)).\n"
            "a((2,)).");
        REQUIRE(unpool("a((1;2)).") ==
            "a(1).\n"
            "a(2).");
        REQUIRE(unpool("a((X;1)).") ==
            "a(X).\n"
            "a(1).");
        REQUIRE(unpool("a((1;2;3;4;5)).") ==
            "a(1).\n"
            "a(2).\n"
            "a(3).\n"
            "a(4).\n"
            "a(5).");
        REQUIRE(unpool("a(|X;Y|).") ==
            "a(|X|).\n"
            "a(|Y|).");
        REQUIRE(unpool("a(1+(2;3)).") ==
            "a((1+2)).\n"
            "a((1+3)).");
        REQUIRE(unpool("a((1;2)+3).") ==
            "a((1+3)).\n"
            "a((2+3)).");
        REQUIRE(unpool("a((1;2)+(3;4)).") ==
            "a((1+3)).\n"
            "a((1+4)).\n"
            "a((2+3)).\n"
            "a((2+4)).");
        REQUIRE(unpool("a((1;2)..(3;4)).") ==
            "a((1..3)).\n"
            "a((1..4)).\n"
            "a((2..3)).\n"
            "a((2..4)).");
    }
    SECTION("head literal") {
        REQUIRE(unpool("a(1;2).") ==
            "a(1).\n"
            "a(2).");
        REQUIRE(unpool("a(1;2):a(3).") ==
            "a(1): a(3).\n"
            "a(2): a(3).");
        // Note: I hope that this one matches what clingo currently does
        //       (in any case, it won't affect existing programs)
        REQUIRE(unpool("a(1):a(2;3).") == "a(1): a(2); a(1): a(3).");
        REQUIRE(unpool("a(1;2):a(3;4).") ==
            "a(1): a(3); a(1): a(4).\n"
            "a(2): a(3); a(1): a(4).\n"
            "a(1): a(3); a(2): a(4).\n"
            "a(2): a(3); a(2): a(4).");
        REQUIRE(unpool("(1;2) { a(2;3): a(4;5) } (6;7).") ==
            "1 <= { a(2): a(4); a(2): a(5); a(3): a(4); a(3): a(5) } <= 6.\n"
            "1 <= { a(2): a(4); a(2): a(5); a(3): a(4); a(3): a(5) } <= 7.\n"
            "2 <= { a(2): a(4); a(2): a(5); a(3): a(4); a(3): a(5) } <= 6.\n"
            "2 <= { a(2): a(4); a(2): a(5); a(3): a(4); a(3): a(5) } <= 7.");
        REQUIRE(unpool("(1;2) #min { (2;3): a(4;5): a(6;7) } (8;9).") ==
            "1 <= #min { 2: a(4): a(6); 2: a(4): a(7); 2: a(5): a(6); 2: a(5): a(7); 3: a(4): a(6); 3: a(4): a(7); 3: a(5): a(6); 3: a(5): a(7) } <= 8.\n"
            "1 <= #min { 2: a(4): a(6); 2: a(4): a(7); 2: a(5): a(6); 2: a(5): a(7); 3: a(4): a(6); 3: a(4): a(7); 3: a(5): a(6); 3: a(5): a(7) } <= 9.\n"
            "2 <= #min { 2: a(4): a(6); 2: a(4): a(7); 2: a(5): a(6); 2: a(5): a(7); 3: a(4): a(6); 3: a(4): a(7); 3: a(5): a(6); 3: a(5): a(7) } <= 8.\n"
            "2 <= #min { 2: a(4): a(6); 2: a(4): a(7); 2: a(5): a(6); 2: a(5): a(7); 3: a(4): a(6); 3: a(4): a(7); 3: a(5): a(6); 3: a(5): a(7) } <= 9.");
        REQUIRE(unpool("&a(1;2) { 1 : a(3;4), a(5;6) }.") ==
            "&a(1) { 1: a(3), a(5); 1: a(4), a(5); 1: a(3), a(6); 1: a(4), a(6) }.\n"
            "&a(2) { 1: a(3), a(5); 1: a(4), a(5); 1: a(3), a(6); 1: a(4), a(6) }.");
        REQUIRE(unpool("&a(0) { 1 : X=(1;2;3) }.") ==
            "&a(0) { 1: X = 1; 1: X = 2; 1: X = 3 }.");
        REQUIRE(unpool("(1;2) < (3;4).") ==
            "1 < 3.\n"
            "1 < 4.\n"
            "2 < 3.\n"
            "2 < 4.");
    }
    SECTION("body literal") {
        REQUIRE(unpool(":- a(1;2).") ==
            "#false :- a(1).\n"
            "#false :- a(2).");
        REQUIRE(unpool(":- a(1;2):a(3).") ==
            "#false :- a(1): a(3).\n"
            "#false :- a(2): a(3).");
        REQUIRE(unpool(":- a(1):a(2;3).") ==
            "#false :- a(1): a(2); a(1): a(3).");
        REQUIRE(unpool(":- a(1;2):a(3;4).") ==
            "#false :- a(1): a(3); a(1): a(4).\n"
            "#false :- a(2): a(3); a(1): a(4).\n"
            "#false :- a(1): a(3); a(2): a(4).\n"
            "#false :- a(2): a(3); a(2): a(4).");
        REQUIRE(unpool(":- (1;2) { a(2;3): a(4;5) } (6;7).") ==
            "#false :- 1 <= { a(2): a(4); a(2): a(5); a(3): a(4); a(3): a(5) } <= 6.\n"
            "#false :- 1 <= { a(2): a(4); a(2): a(5); a(3): a(4); a(3): a(5) } <= 7.\n"
            "#false :- 2 <= { a(2): a(4); a(2): a(5); a(3): a(4); a(3): a(5) } <= 6.\n"
            "#false :- 2 <= { a(2): a(4); a(2): a(5); a(3): a(4); a(3): a(5) } <= 7.");
        REQUIRE(unpool(":- (1;2) #min { (2;3): a(4;5), a(6;7) } (8;9).") ==
            "#false :- 1 <= #min { 2: a(4), a(6); 2: a(5), a(6); 2: a(4), a(7); 2: a(5), a(7); 3: a(4), a(6); 3: a(5), a(6); 3: a(4), a(7); 3: a(5), a(7) } <= 8.\n"
            "#false :- 1 <= #min { 2: a(4), a(6); 2: a(5), a(6); 2: a(4), a(7); 2: a(5), a(7); 3: a(4), a(6); 3: a(5), a(6); 3: a(4), a(7); 3: a(5), a(7) } <= 9.\n"
            "#false :- 2 <= #min { 2: a(4), a(6); 2: a(5), a(6); 2: a(4), a(7); 2: a(5), a(7); 3: a(4), a(6); 3: a(5), a(6); 3: a(4), a(7); 3: a(5), a(7) } <= 8.\n"
            "#false :- 2 <= #min { 2: a(4), a(6); 2: a(5), a(6); 2: a(4), a(7); 2: a(5), a(7); 3: a(4), a(6); 3: a(5), a(6); 3: a(4), a(7); 3: a(5), a(7) } <= 9.");
        REQUIRE(unpool(":- &a(1;2) { 1 : a(3;4), a(5;6) }.") ==
            "#false :- &a(1) { 1: a(3), a(5); 1: a(4), a(5); 1: a(3), a(6); 1: a(4), a(6) }.\n"
            "#false :- &a(2) { 1: a(3), a(5); 1: a(4), a(5); 1: a(3), a(6); 1: a(4), a(6) }.");
        REQUIRE(unpool(":- (1;2) < (3;4).") ==
            "#false :- 1 < 3.\n"
            "#false :- 1 < 4.\n"
            "#false :- 2 < 3.\n"
            "#false :- 2 < 4.");
    }
    SECTION("statements") {
        REQUIRE(unpool("a(1;2) :- a(3;4); a(5;6).") ==
            "a(1) :- a(3); a(5).\n"
            "a(2) :- a(3); a(5).\n"
            "a(1) :- a(4); a(5).\n"
            "a(2) :- a(4); a(5).\n"
            "a(1) :- a(3); a(6).\n"
            "a(2) :- a(3); a(6).\n"
            "a(1) :- a(4); a(6).\n"
            "a(2) :- a(4); a(6).");
        REQUIRE(unpool("#show a(1;2) : a(3;4).") ==
            "#show a(1) : a(3).\n"
            "#show a(2) : a(3).\n"
            "#show a(1) : a(4).\n"
            "#show a(2) : a(4).");
        REQUIRE(unpool(":~ a(1;2). [(3;4)@(5;6),(7;8)]") ==
            ":~ a(1). [3@5,7]\n"
            ":~ a(1). [3@5,8]\n"
            ":~ a(1). [3@6,7]\n"
            ":~ a(1). [3@6,8]\n"
            ":~ a(1). [4@5,7]\n"
            ":~ a(1). [4@5,8]\n"
            ":~ a(1). [4@6,7]\n"
            ":~ a(1). [4@6,8]\n"
            ":~ a(2). [3@5,7]\n"
            ":~ a(2). [3@5,8]\n"
            ":~ a(2). [3@6,7]\n"
            ":~ a(2). [3@6,8]\n"
            ":~ a(2). [4@5,7]\n"
            ":~ a(2). [4@5,8]\n"
            ":~ a(2). [4@6,7]\n"
            ":~ a(2). [4@6,8]");
        REQUIRE(unpool("#external a(1;2) : a(3;4). [(5;6)]") ==
            "#external a(1) : a(3). [5]\n"
            "#external a(1) : a(3). [6]\n"
            "#external a(2) : a(3). [5]\n"
            "#external a(2) : a(3). [6]\n"
            "#external a(1) : a(4). [5]\n"
            "#external a(1) : a(4). [6]\n"
            "#external a(2) : a(4). [5]\n"
            "#external a(2) : a(4). [6]");
        REQUIRE(unpool("#edge ((1;2),(3;4)) : a(5;6).") ==
            "#edge (1,3) : a(5).\n"
            "#edge (1,4) : a(5).\n"
            "#edge (2,3) : a(5).\n"
            "#edge (2,4) : a(5).\n"
            "#edge (1,3) : a(6).\n"
            "#edge (1,4) : a(6).\n"
            "#edge (2,3) : a(6).\n"
            "#edge (2,4) : a(6).");
        REQUIRE(unpool("#heuristic a(1;2) : a(3;4). [a(5;6)@a(7;8),a(9;10)]") ==
            "#heuristic a(1) : a(3). [a(5)@a(7),a(9)]\n"
            "#heuristic a(1) : a(3). [a(5)@a(7),a(10)]\n"
            "#heuristic a(1) : a(3). [a(5)@a(8),a(9)]\n"
            "#heuristic a(1) : a(3). [a(5)@a(8),a(10)]\n"
            "#heuristic a(1) : a(3). [a(6)@a(7),a(9)]\n"
            "#heuristic a(1) : a(3). [a(6)@a(7),a(10)]\n"
            "#heuristic a(1) : a(3). [a(6)@a(8),a(9)]\n"
            "#heuristic a(1) : a(3). [a(6)@a(8),a(10)]\n"
            "#heuristic a(2) : a(3). [a(5)@a(7),a(9)]\n"
            "#heuristic a(2) : a(3). [a(5)@a(7),a(10)]\n"
            "#heuristic a(2) : a(3). [a(5)@a(8),a(9)]\n"
            "#heuristic a(2) : a(3). [a(5)@a(8),a(10)]\n"
            "#heuristic a(2) : a(3). [a(6)@a(7),a(9)]\n"
            "#heuristic a(2) : a(3). [a(6)@a(7),a(10)]\n"
            "#heuristic a(2) : a(3). [a(6)@a(8),a(9)]\n"
            "#heuristic a(2) : a(3). [a(6)@a(8),a(10)]\n"
            "#heuristic a(1) : a(4). [a(5)@a(7),a(9)]\n"
            "#heuristic a(1) : a(4). [a(5)@a(7),a(10)]\n"
            "#heuristic a(1) : a(4). [a(5)@a(8),a(9)]\n"
            "#heuristic a(1) : a(4). [a(5)@a(8),a(10)]\n"
            "#heuristic a(1) : a(4). [a(6)@a(7),a(9)]\n"
            "#heuristic a(1) : a(4). [a(6)@a(7),a(10)]\n"
            "#heuristic a(1) : a(4). [a(6)@a(8),a(9)]\n"
            "#heuristic a(1) : a(4). [a(6)@a(8),a(10)]\n"
            "#heuristic a(2) : a(4). [a(5)@a(7),a(9)]\n"
            "#heuristic a(2) : a(4). [a(5)@a(7),a(10)]\n"
            "#heuristic a(2) : a(4). [a(5)@a(8),a(9)]\n"
            "#heuristic a(2) : a(4). [a(5)@a(8),a(10)]\n"
            "#heuristic a(2) : a(4). [a(6)@a(7),a(9)]\n"
            "#heuristic a(2) : a(4). [a(6)@a(7),a(10)]\n"
            "#heuristic a(2) : a(4). [a(6)@a(8),a(9)]\n"
            "#heuristic a(2) : a(4). [a(6)@a(8),a(10)]");
        REQUIRE(unpool("#project a(1;2) : a(3;4).") ==
            "#project a(1) : a(3).\n"
            "#project a(2) : a(3).\n"
            "#project a(1) : a(4).\n"
            "#project a(2) : a(4).");
    }
    SECTION("options") {
        std::vector<AST::Node> prg;
        AST::parse_string(":- a(1;2): a(3;4).", [&prg](AST::Node &&ast) { prg.emplace_back(std::move(ast)); });
        AST::Node lit = *prg.back().get(AST::Attribute::Body).get<AST::NodeVector>().begin();
        auto unpool = [&lit](bool other, bool condition) {
            std::vector<std::string> ret;
            for (auto &ast : lit.unpool(other, condition)) {
                ret.emplace_back(ast.to_string());
            }
            return ret;
        };

        REQUIRE(unpool(true, true) == std::vector<std::string>{
            "a(1): a(3)",
            "a(1): a(4)",
            "a(2): a(3)",
            "a(2): a(4)"});
        REQUIRE(unpool(false, true) == std::vector<std::string>{
            "a(1;2): a(3)",
            "a(1;2): a(4)"});
        REQUIRE(unpool(true, false) == std::vector<std::string>{
            "a(1): a(3;4)",
            "a(2): a(3;4)"});
        REQUIRE(unpool(false, false) == std::vector<std::string>{
            "a(1;2): a(3;4)"});
    }
}

} } // namespace Test Clingo
