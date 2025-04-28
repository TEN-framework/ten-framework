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

#include "gringo/ground/program.hh"
#include "gringo/input/nongroundparser.hh"
#include "gringo/input/program.hh"
#include "gringo/output/output.hh"

#include "tests/tests.hh"

namespace Gringo { namespace Ground { namespace Test {

using namespace Gringo::IO;

// {{{ definition of auxiliary functions

namespace {

typedef std::string S;

Program parse(std::string const &str) {
    Gringo::Test::TestGringoModule module;
    std::ostringstream oss;
    Potassco::TheoryData td;
    Output::OutputBase out(td, {}, oss);
    Input::Program prg;
    Defines defs;
    Gringo::Test::TestContext context;
    NullBackend bck;
    Input::NongroundProgramBuilder pb{ context, prg, out.outPreds, defs };
    bool incmode;
    Input::NonGroundParser ngp{pb, bck, incmode};
    ngp.pushStream("-", gringo_make_unique<std::stringstream>(str), module.logger);
    ngp.parse(module.logger);
    prg.rewrite(defs, module.logger);
    return prg.toGround({Sig{"base", 0, false}}, out.data, module.logger);
}

std::string toString(Program const &p) {
    std::string str = to_string(p);
    replace_all(str, ",[#inc_base]", "");
    replace_all(str, ":-[#inc_base].", ".");
    replace_all(str, ":-[#inc_base],", ":-");
    replace_all(str, ":-[#inc_base];", ":-");
    replace_all(str, "% component\n#external.\n", "");
    replace_all(str, "\n% component\n#external.", "");
    replace_all(str, "% component\n#external.", "");
    return str;
}

} // namespace

// }}}

TEST_CASE("ground-program", "[ground]") {

    SECTION("toGround") {
        REQUIRE(
            "" == toString(parse("p(1;2).")));
        REQUIRE(
            "% positive component\n"
            "p(X):-q(X)." ==
            toString(parse("p(X):-q(X).")));
        REQUIRE(
            "% positive component\n"
            "p((#Range0+0)):-#Range0=1..2." ==
            toString(parse("p(1..2).")));
        REQUIRE(
            "% positive component\n"
            "p(X):-X=1." ==
            toString(parse("p(X):-X=1.")));
        REQUIRE(
            "% component\n"
            "#false:-0=0." ==
            toString(parse(":-.")));
        REQUIRE(
            "% component\n"
            "#false:-not not p." ==
            toString(parse("not p.")));
        REQUIRE(
            "% positive component\n"
            "p(X,Y):-p(Y),p(X),X=Y,Y=X." ==
            toString(parse("p(X,Y):-X=Y,p(X),p(Y).")));
        REQUIRE(
            "% positive component\n"
            "#accu(#d0(Z,X,Y),tuple(#special)):-[p(X,Y,Z)],0>Z.\n"
            "% positive component\n"
            "#accu(#d0(Z,X,Y),tuple(A)):-[p(X,Y,Z)],q(A),r(A,X).\n"
            "% positive component\n"
            "#accu(#d0(Z,X,Y),tuple(B,Y)):-[p(X,Y,Z)],a(B,Y).\n"
            "% positive component\n"
            "#d0(Z,X,Y):-#accu(#d0(Z,X,Y),tuple(#special)),#accu(#d0(Z,X,Y),tuple(A)),#accu(#d0(Z,X,Y),tuple(B,Y)).\n"
            "% positive component\n"
            "x:-p(X,Y,Z),Z<#count{#d0(Z,X,Y)}." ==
            toString(parse("x:-p(X,Y,Z),Z<#count{A:q(A),r(A,X);B,Y:a(B,Y)}.")));
        REQUIRE(
            "% positive component\n"
            "#accu(#d0(Z,X,Y),tuple(#special)):-[p(X,Y,Z)],0=Z.\n"
            "% positive component\n"
            "#accu(#d0(Z,X,Y),tuple(A)):-[p(X,Y,Z)],q(A),r(A,X).\n"
            "% positive component\n"
            "#accu(#d0(Z,X,Y),tuple(B,Y)):-[p(X,Y,Z)],a(B,Y).\n"
            "% positive component\n"
            "#d0(Z,X,Y):-#accu(#d0(Z,X,Y),tuple(#special)),#accu(#d0(Z,X,Y),tuple(A)),#accu(#d0(Z,X,Y),tuple(B,Y)).\n"
            "% positive component\n"
            "x:-p(X,Y,Z),Z=#count{#d0(Z,X,Y)}." ==
            toString(parse("x:-p(X,Y,Z),Z=#count{A:q(A),r(A,X);B,Y:a(B,Y)}.")));
        REQUIRE(
            "% positive component\n"
            "#accu(#d0(X,Y,ZZ),tuple(#special)):-[p(X,Y,Z)].\n"
            "% positive component\n"
            "#accu(#d0(X,Y,ZZ),tuple(A)):-[p(X,Y,Z)],q(A),r(A,X).\n"
            "% positive component\n"
            "#accu(#d0(X,Y,ZZ),tuple(B,Y)):-[p(X,Y,Z)],a(B,Y).\n"
            "% positive component\n"
            "#d0(X,Y,ZZ):-#accu(#d0(X,Y,ZZ),tuple(#special));#accu(#d0(X,Y,ZZ),tuple(A));#accu(#d0(X,Y,ZZ),tuple(B,Y)).\n"
            "% positive component\n"
            "x:-p(X,Y,Z),ZZ=#count{#d0(X,Y,ZZ)}." ==
            toString(parse("x:-p(X,Y,Z),ZZ=#count{A:q(A),r(A,X);B,Y:a(B,Y)}.")));
        REQUIRE(
            "% component\n"
            "Z<#count(#d0(Z,X,Y)):-p(X,Y,Z).\n"
            "% component\n"
            "#accu(#d0(Z,X,Y),x,tuple(B,Y)):-a(B,Y),#d0(Z,X,Y)!.\n"
            "% component\n"
            "#accu(#d0(Z,X,Y),q(A),tuple(A)):-r(A,X),#d0(Z,X,Y)!.\n"
            "% component\n"
            "Z<#count{A:q(A):#accu(#d0(Z,X,Y),q(A),tuple(A));B,Y:x:#accu(#d0(Z,X,Y),x,tuple(B,Y))}:-#d0(Z,X,Y)!." ==
            toString(parse("Z<#count{A:q(A):r(A,X);B,Y:x:a(B,Y)}:-p(X,Y,Z).")));
        REQUIRE(
            "% positive component\n"
            "#accu(#d0(Z,X,Y),tuple(#special)):-[p(X,Y,Z)],0>Z.\n"
            "% positive component\n"
            "#accu(#d0(Z,X,Y),tuple(1,q(A))):-[p(X,Y,Z)],r(A,X),not q(A).\n"
            "% positive component\n"
            "#accu(#d0(Z,X,Y),tuple(0,a(B,Y))):-[p(X,Y,Z)],a(B,Y).\n"
            "% positive component\n"
            "#accu(#d0(Z,X,Y),tuple(3,X)):-[p(X,Y,Z)],q(X),X>1.\n"
            "% positive component\n"
            "#d0(Z,X,Y):-#accu(#d0(Z,X,Y),tuple(#special)),#accu(#d0(Z,X,Y),tuple(1,q(A))),#accu(#d0(Z,X,Y),tuple(0,a(B,Y))),#accu(#d0(Z,X,Y),tuple(3,X)).\n"
            "% positive component\n"
            "x:-p(X,Y,Z),Z<#count{#d0(Z,X,Y)}." ==
            toString(parse("x:-p(X,Y,Z),Z<{not q(A):r(A,X);a(B,Y);X>1:q(X)}.")));
        REQUIRE(
            "% component\n"
            "Z<#count(#d0(Z,X,Y)):-p(X,Y,Z).\n"
            "% component\n"
            "#accu(#d0(Z,X,Y),#true):-q(X),X>1,#d0(Z,X,Y)!.\n"
            "% component\n"
            "#accu(#d0(Z,X,Y),a(B,Y),tuple(0,a(B,Y))):-#d0(Z,X,Y)!.\n"
            "% component\n"
            "#accu(#d0(Z,X,Y),#true):-r(A,X),not q(A),#d0(Z,X,Y)!.\n"
            "% component\n"
            "Z<#count{1,q(A):#true:#accu(#d0(Z,X,Y),#true);0,a(B,Y):a(B,Y):#accu(#d0(Z,X,Y),a(B,Y),tuple(0,a(B,Y)));3,X:#true:#accu(#d0(Z,X,Y),#true)}:-#d0(Z,X,Y)!." ==
            toString(parse("Z<{not q(A):r(A,X);a(B,Y);X>1:q(X)}:-p(X,Y,Z).")));
        REQUIRE(
            "% component\n"
            "#d0(B,Y,X):-p(X,Y,Z).\n"
            "% component\n"
            "#complete(#d0(B,Y,X)):-r(A,X),[#d0(B,Y,X)!].\n"
            "% component\n"
            "#complete(#d0(B,Y,X)):-[#d0(B,Y,X)!].\n"
            "% component\n"
            "a(B,Y);q(A):-#complete(#d0(B,Y,X))!" ==
            toString(parse("a(B,Y);q(A):r(A,X):-p(X,Y,Z).")));
        REQUIRE(
            "% component\n"
            "#d0(B,Y,X):-p(X,Y,Z).\n"
            "% component\n"
            "#complete(#d0(B,Y,X)):-r(A,X),[#d0(B,Y,X)!].\n"
            "% component\n"
            "#complete(#d0(B,Y,X)):-q(X),[#d0(B,Y,X)!].\n"
            "% component\n"
            "#complete(#d0(B,Y,X)):-[#d0(B,Y,X)!].\n"
            "% component\n"
            "a(B,Y);#false:X<=1;#false:not not q(A):-#complete(#d0(B,Y,X))!" ==
            toString(parse("a(B,Y);X>1:q(X);not q(A):r(A,X):-p(X,Y,Z).")));
        REQUIRE(
            "% positive component\n"
            "#accu(#d0(Z,X,Y),tuple(#special)):-[p(X,Y,Z)],0>Z.\n"
            "% positive component\n"
            "#accu(#d0(Z,X,Y),tuple(A)):-[p(X,Y,Z)],q(A),r(A,X).\n"
            "% positive component\n"
            "#accu(#d0(Z,X,Y),tuple(B,Y)):-[p(X,Y,Z)],a(B,Y).\n"
            "% positive component\n"
            "#d0(Z,X,Y):-#accu(#d0(Z,X,Y),tuple(#special)),#accu(#d0(Z,X,Y),tuple(A)),#accu(#d0(Z,X,Y),tuple(B,Y)).\n"
            "% positive component\n"
            "x:-p(X,Y,Z),Z<#count{#d0(Z,X,Y)}." ==
            toString(parse("x:-p(X,Y,Z),Z<#count{A:q(A),r(A,X);B,Y:a(B,Y)}.")));
    }

    SECTION("analyze") {
        REQUIRE(
            "% positive component\n"
            "x:-x?.\n"
            "% positive component\n"
            "a:-not y,x,b?.\n"
            "b:-a?.\n"
            "% positive component\n"
            "c:-b,a." ==
            toString(parse("x:-x.a:-b,x,not y.b:-a.c:-a,b.")));
        REQUIRE(
            "% positive component\n"
            "x:-x?.\n"
            "% component\n"
            "a:-not b?.\n"
            "% component\n"
            "b:-x,a!,not a!." ==
            toString(parse("x:-x.a:-not b.b:-not a,a,x.")));
    }

}

} } } // namespace Test Ground Gringo
