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

#include "tests/term_helper.hh"
#include "tests/tests.hh"
#include "tests/term_helper.hh"
#include "tests/output/solver_helper.hh"

namespace Gringo { namespace Output { namespace Test {

using namespace Gringo::Test;

// {{{ definition of auxiliary functions

namespace {

std::string iground(std::string in, int last = 3) {
    std::stringstream ss;
    Gringo::Test::TestGringoModule module;
    Potassco::TheoryData td;
    Output::OutputBase out(td, {}, ss, OutputFormat::INTERMEDIATE);
    Input::Program prg;
    Defines defs;
    Gringo::Test::TestContext context;
    NullBackend bck;
    Input::NongroundProgramBuilder pb(context, prg, out.outPreds, defs);
    bool incmode;
    Input::NonGroundParser parser(pb, bck, incmode);
    parser.pushStream("-", gringo_make_unique<std::stringstream>(in), module.logger);
    Models models;
    parser.parse(module.logger);
    prg.rewrite(defs, module.logger);
    prg.check(module.logger);
    //std::cerr << prg;
    auto ground = [&](std::set<Sig> const &sigs, Ground::Parameters const &params) {
        auto gPrg = prg.toGround(sigs, out.data, module.logger);
        gPrg.prepare(params, out, module.logger);
        gPrg.ground(context, out, module.logger);
    };
    if (!module.logger.hasError()) {
        out.init(true);
        {
            Ground::Parameters params;
            params.add("base", {});
            out.beginStep();
            ground({Sig{"base", 0, false}}, params);
            out.endStep({});
            out.reset(true);
        }
        for (int i=1; i < last; ++i) {
            Ground::Parameters params;
            params.add("step", {NUM(i)});
            out.beginStep();
            ground({Sig{"step", 1, false}}, params);
            out.endStep({});
            out.reset(true);
        }
        {
            Ground::Parameters params;
            params.add("last", {});
            out.beginStep();
            ground({Sig{"last", 0, false}}, params);
            out.endStep({});
            out.reset(true);
        }
    }
    return ss.str();
}

} // namespace

// }}}
// {{{ definition of TestIncremental

TEST_CASE("output-incremental", "[output]") {
    SECTION("assign") {
        REQUIRE(
            "asp 1 0 0 incremental\n"
            "0\n"
            "1 0 1 1 0 0\n"
            "4 8 p(1,0,0) 0\n"
            "0\n"
            "1 0 1 2 0 0\n"
            "4 8 p(2,0,0) 0\n"
            "0\n"
            "0\n" == iground(
                "#program base."
                "#program step(k)."
                "p(k,N,Y) :- N = #sum { Y }, Y = #sum { }."
                "#program last."));
        REQUIRE(
            "asp 1 0 0 incremental\n"
            "0\n"
            "1 0 1 1 0 0\n"
            "4 8 p(1,2,1) 0\n"
            "0\n"
            "1 0 1 2 0 0\n"
            "4 8 p(2,4,2) 0\n"
            "0\n"
            "0\n" == iground(
                "#program base."
                "#program step(k)."
                "p(k,N,Y) :- N = #sum { 2*Y }, Y = #sum { k }."
                "#program last."));
    }
    SECTION("projection") {
        REQUIRE(
            "asp 1 0 0 incremental\n"
            "1 1 1 1 0 0\n"
            "1 1 1 2 0 0\n"
            "1 1 1 3 0 0\n"
            "1 0 1 4 0 1 1\n"
            "1 0 1 4 0 1 2\n"
            "1 0 1 4 0 1 3\n"
            "4 4 p(1) 1 1\n"
            "4 4 p(2) 1 2\n"
            "4 4 p(3) 1 3\n"
            "0\n"
            "1 0 1 5 0 1 4\n"
            "1 1 1 6 0 1 5\n"
            "4 4 q(1) 1 6\n"
            "0\n"
            "1 0 1 7 0 1 5\n"
            "1 1 1 8 0 1 7\n"
            "4 4 q(2) 1 8\n"
            "0\n"
            "1 0 1 9 0 1 7\n"
            "0\n" == iground(
                "#program base."
                "{p(1..3)}."
                "#program step(k)."
                "{q(k)} :- p(_)."
                "#program last."));
    }

    SECTION("lp") {
        REQUIRE(
            "asp 1 0 0 incremental\n"
            "4 4 base 1 -1\n"
            "0\n"
            "1 0 1 2 0 0\n"
            "4 4 r(1) 0\n"
            "4 4 q(1) 1 -1\n"
            "4 4 p(1) 1 -1\n"
            "0\n"
            "1 0 1 3 0 0\n"
            "4 4 r(2) 0\n"
            "4 4 q(2) 1 -1\n"
            "4 4 p(2) 1 -1\n"
            "0\n"
            "4 4 last 1 -1\n"
            "0\n" == iground(
                "#program base."
                "#show base."
                "#program step(k)."
                "#show p(k)."
                "#show q(k)."
                "r(k)."
                "#program last."
                "#show last."));
        REQUIRE(
            "asp 1 0 0 incremental\n"
            "1 0 1 1 0 0\n"
            "4 4 base 0\n"
            "0\n"
            "1 0 1 2 0 0\n"
            "4 7 step(1) 0\n"
            "0\n"
            "1 0 1 3 0 0\n"
            "4 7 step(2) 0\n"
            "0\n"
            "1 0 1 4 0 0\n"
            "4 4 last 0\n"
            "0\n" == iground(
                "#program base."
                "base."
                "#program step(k)."
                "step(k)."
                "#program last."
                "last."));
    }

    SECTION("projectionBug") {
        REQUIRE(
            "asp 1 0 0 incremental\n"
            "1 1 1 1 0 0\n"
            "1 0 1 2 0 1 1\n"
            "4 6 p(0,0) 1 1\n"
            "0\n"
            "1 0 1 3 0 1 2\n"
            "1 1 1 4 0 1 3\n"
            "1 0 1 5 0 1 4\n"
            "4 6 p(1,1) 1 4\n"
            "0\n"
            "1 0 1 6 0 1 3\n"
            "1 0 1 7 0 1 5\n"
            "1 1 1 8 0 1 7\n"
            "1 0 1 9 0 1 8\n"
            "4 6 p(2,2) 1 8\n"
            "0\n"
            "1 0 1 10 0 1 6\n"
            "1 0 1 11 0 1 7\n"
            "1 0 1 12 0 1 9\n"
            "1 1 1 13 0 0\n"
            "1 0 1 10 0 1 13\n"
            "1 1 1 14 0 1 10\n"
            "1 1 1 15 0 1 11\n"
            "1 1 1 16 0 1 12\n"
            "4 6 p(1,0) 1 13\n"
            "4 4 r(0) 1 14\n"
            "4 4 r(1) 1 15\n"
            "4 4 r(2) 1 16\n"
            "0\n" == iground(
                "#program base."
                "{p(0,0)}."
                "#program step(k)."
                "{p(k,k)} :- p(_,k-1)."
                "#program last."
                "{p(1,0)}."
                "{r(X)} :- p(_,X)."
                ));
    }

    SECTION("mapping") {
        Mapping m;
        m.add(1,0);
        m.add(2,1);
        m.add(4,2);
        m.add(5,3);

        REQUIRE(InvalidId == m.get(0));
        REQUIRE(Id_t(0) == m.get(1));
        REQUIRE(Id_t(1) == m.get(2));
        REQUIRE(InvalidId == m.get(3));
        REQUIRE(Id_t(2) == m.get(4));
        REQUIRE(Id_t(3) == m.get(5));
        REQUIRE(InvalidId == m.get(6));
    }
}

} } } // namespace Test Output Gringo
