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

#include "gringo/input/nongroundparser.hh"
#include "gringo/input/programbuilder.hh"
#include "gringo/input/program.hh"
#include "gringo/output/output.hh"
#include "input/nongroundgrammar/grammar.hh"
#include "gringo/symbol.hh"

#include "tests/tests.hh"

namespace Gringo { namespace Input { namespace Test {

TEST_CASE("input-nongroundlexer", "[input]") {
    Gringo::Test::TestGringoModule module;
    std::ostringstream oss;
    Potassco::TheoryData td;
    Output::OutputBase out(td, {}, oss);
    Program prg;
    Defines defs;
    Gringo::Test::TestContext context;
    NongroundProgramBuilder pb(context, prg, out.outPreds, defs);
    bool incmode;
    NullBackend bck;
    NonGroundParser ngp(pb, bck, incmode);
    ngp.parse(module); // Just to set the logger
    std::string in =
        "#script (python) #end "
        "%*xyz\nxyz\n*%"
        "%xyz\n"
        "#minimize "
        "#minimise "
        "#infimum "
        "#inf "
        ";* "
        ";\n * "
        "not "
        "xyz "
        "_xyz "
        "__xyz "
        "___xyz "
        "'___xyz' "
        "'___x'yz' "
        "'___Xyz' "
        "'___X'yz' "
        // TODO: check errors too: "# "
        ;
    ngp.pushStream("-", std::unique_ptr<std::istream>(new std::stringstream(in)), module.logger);

    Location loc("<undef>", 0, 0, "<undef>", 0, 0);
    NonGroundGrammar::parser::semantic_type val;

    REQUIRE(int(NonGroundGrammar::parser::token::PARSE_LP) == ngp.lex(&val, loc));
    REQUIRE(int(NonGroundGrammar::parser::token::SCRIPT) == ngp.lex(&val, loc));
    REQUIRE(int(NonGroundGrammar::parser::token::LPAREN) == ngp.lex(&val, loc));
    REQUIRE(int(NonGroundGrammar::parser::token::IDENTIFIER) == ngp.lex(&val, loc));
    REQUIRE(int(NonGroundGrammar::parser::token::RPAREN) == ngp.lex(&val, loc));
    REQUIRE(int(NonGroundGrammar::parser::token::CODE) == ngp.lex(&val, loc));
    REQUIRE(int(NonGroundGrammar::parser::token::MINIMIZE) == ngp.lex(&val, loc));
    REQUIRE(int(NonGroundGrammar::parser::token::MINIMIZE) == ngp.lex(&val, loc));
    REQUIRE(int(NonGroundGrammar::parser::token::INFIMUM) == ngp.lex(&val, loc));
    REQUIRE(int(NonGroundGrammar::parser::token::INFIMUM) == ngp.lex(&val, loc));
    REQUIRE(int(NonGroundGrammar::parser::token::SEM) == ngp.lex(&val, loc));
    REQUIRE(int(NonGroundGrammar::parser::token::MUL) == ngp.lex(&val, loc));
    REQUIRE(int(NonGroundGrammar::parser::token::SEM) == ngp.lex(&val, loc));
    REQUIRE(int(NonGroundGrammar::parser::token::MUL) == ngp.lex(&val, loc));
    REQUIRE(int(NonGroundGrammar::parser::token::NOT) == ngp.lex(&val, loc));
    REQUIRE(int(NonGroundGrammar::parser::token::IDENTIFIER) == ngp.lex(&val, loc));
    REQUIRE(String("xyz") == String::fromRep(val.str));
    REQUIRE(int(NonGroundGrammar::parser::token::IDENTIFIER) == ngp.lex(&val, loc));
    REQUIRE(String("_xyz") == String::fromRep(val.str));
    REQUIRE(int(NonGroundGrammar::parser::token::IDENTIFIER) == ngp.lex(&val, loc));
    REQUIRE(String("__xyz") == String::fromRep(val.str));
    REQUIRE(int(NonGroundGrammar::parser::token::IDENTIFIER) == ngp.lex(&val, loc));
    REQUIRE(String("___xyz") == String::fromRep(val.str));
    REQUIRE(5 == loc.beginLine);
    REQUIRE(23 == loc.beginColumn);
    REQUIRE(int(NonGroundGrammar::parser::token::IDENTIFIER) == ngp.lex(&val, loc));
    REQUIRE(String("'___xyz'") == String::fromRep(val.str));
    REQUIRE(int(NonGroundGrammar::parser::token::IDENTIFIER) == ngp.lex(&val, loc));
    REQUIRE(String("'___x'yz'") == String::fromRep(val.str));
    REQUIRE(int(NonGroundGrammar::parser::token::VARIABLE) == ngp.lex(&val, loc));
    REQUIRE(String("'___Xyz'") == String::fromRep(val.str));
    REQUIRE(int(NonGroundGrammar::parser::token::VARIABLE) == ngp.lex(&val, loc));
    REQUIRE(String("'___X'yz'") == String::fromRep(val.str));
    REQUIRE(int(NonGroundGrammar::parser::token::SYNC) == ngp.lex(&val, loc));
    REQUIRE(0 == ngp.lex(&val, loc));
}

// }}}

} } } // namespace Test Input Gringo

