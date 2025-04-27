#include <catch2/catch_test_macros.hpp>
#include <potassco/aspif_text.h>
#include <reify/program.hh>

#include <sstream>

namespace Reify { namespace Test {

bool read(std::stringstream &in, std::stringstream &out, bool scc = false, bool step = false) {
    Reifier prg(out, scc, step);
    Potassco::AspifTextInput parser(&prg);
    return Potassco::readProgram(in, parser, 0) == 0;
}

TEST_CASE("program", "[program]") {
    std::stringstream input, output;

    SECTION("empty") {
        REQUIRE(read(input, output));
        REQUIRE(output.str() == "");
    }
    SECTION("incremental") {
        input << "#incremental.";
        REQUIRE(read(input, output));
        REQUIRE(output.str() == "tag(incremental).\n");
    }
    SECTION("normal") {
        input << "a:-b.";
        REQUIRE(read(input, output));
        REQUIRE(output.str() == "atom_tuple(0).\natom_tuple(0,1).\nliteral_tuple(0).\nliteral_tuple(0,2).\nrule(disjunction(0),normal(0)).\n");
    }
    SECTION("step") {
        input << "#incremental.";
        input << "a:-b.";
        input << "#step.";
        input << "c:-b.";
        auto result =
            "tag(incremental).\n"
            "atom_tuple(0,0).\n"
            "atom_tuple(0,1,0).\n"
            "literal_tuple(0,0).\n"
            "literal_tuple(0,2,0).\n"
            "rule(disjunction(0),normal(0),0).\n"
            "atom_tuple(0,1).\n"
            "atom_tuple(0,3,1).\n"
            "literal_tuple(0,1).\n"
            "literal_tuple(0,2,1).\n"
            "rule(disjunction(0),normal(0),1).\n";
        REQUIRE(read(input, output, false, true));
        REQUIRE(output.str() == result);
    }
    SECTION("cycle") {
        input << "a:-b."
              << "b:-a.";
        auto result =
            "atom_tuple(0).\n"
            "atom_tuple(0,1).\n"
            "literal_tuple(0).\n"
            "literal_tuple(0,2).\n"
            "rule(disjunction(0),normal(0)).\n"
            "atom_tuple(1).\n"
            "atom_tuple(1,2).\n"
            "literal_tuple(1).\n"
            "literal_tuple(1,1).\n"
            "rule(disjunction(1),normal(1)).\n"
            "scc(0,1).\n"
            "scc(0,2).\n";
        REQUIRE(read(input, output, true));
        REQUIRE(output.str() == result);
    }
    SECTION("choice") {
        input << "{a, b}.";
        REQUIRE(read(input, output));
        REQUIRE(output.str() == "atom_tuple(0).\natom_tuple(0,1).\natom_tuple(0,2).\nliteral_tuple(0).\nrule(choice(0),normal(0)).\n");
    }
    SECTION("sum") {
        input << ":-1 {a, b}.";
        REQUIRE(read(input, output));
        REQUIRE(output.str() == "atom_tuple(0).\nweighted_literal_tuple(0).\nweighted_literal_tuple(0,1,1).\nweighted_literal_tuple(0,2,1).\nrule(disjunction(0),sum(0,1)).\n");
    }
    SECTION("minimize") {
        input << "#minimize {a=10, b=20}.";
        REQUIRE(read(input, output));
        REQUIRE(output.str() == "weighted_literal_tuple(0).\nweighted_literal_tuple(0,1,10).\nweighted_literal_tuple(0,2,20).\nminimize(0,0).\n");
    }
    SECTION("project") {
        input << "#project {a}.";
        REQUIRE(read(input, output));
        REQUIRE(output.str() == "project(1).\n");
    }
    SECTION("output") {
        input << "#output a:b,c.";
        REQUIRE(read(input, output));
        REQUIRE(output.str() == "literal_tuple(0).\nliteral_tuple(0,2).\nliteral_tuple(0,3).\noutput(a,0).\n");
    }
    SECTION("external") {
        input << "#external a.";
        REQUIRE(read(input, output));
        REQUIRE(output.str() == "external(1,false).\n");
    }
    SECTION("assume") {
        input << "#assume {a}.";
        REQUIRE(read(input, output));
        REQUIRE(output.str() == "assume(1).\n");
    }
    SECTION("heuristic") {
        input << "#heuristic a. [1, level]";
        input << "#heuristic b : c. [2@1, true]";
        REQUIRE(read(input, output));
        REQUIRE(output.str() == "literal_tuple(0).\nheuristic(1,level,1,0,0).\nliteral_tuple(1).\nliteral_tuple(1,3).\nheuristic(2,true,2,1,1).\n");
    }
    SECTION("edge") {
        input << "#edge (1,2) : a.";
        input << "#edge (2,1).";
        REQUIRE(read(input, output));
        REQUIRE(output.str() == "literal_tuple(0).\nliteral_tuple(0,1).\nedge(1,2,0).\nliteral_tuple(1).\nedge(2,1,1).\n");
    }
}

} } // namespace Test Reify
