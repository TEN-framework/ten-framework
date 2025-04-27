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

#include "tests/tests.hh"
#include "tests/term_helper.hh"
#include "tests/output/solver_helper.hh"

namespace Gringo { namespace Output { namespace Test {

TEST_CASE("output-lparse", "[output]") {
    SECTION("bug-disjunction-range") {
        // positive
        REQUIRE("([[p(1),p(2)],[q]],[])" == IO::to_string(solve("p(1..2); q.")));
        REQUIRE("([[]],[-:1:3-11: info: operation undefined:\n  (#Range0/X)\n,-:1:3-11: info: operation undefined:\n  (#Range0/X)\n,-:1:3-11: info: operation undefined:\n  (#Range0/X)\n])" == IO::to_string(solve("p((0..2)/X); q :- X=0.")));
        REQUIRE("([[]],[-:1:3-11: info: operation undefined:\n  (#Range0/0)\n])" == IO::to_string(solve("p((0..2)/0); q :- X=0.")));
        REQUIRE("([[p(2),p(4)],[q]],[-:1:3-11: info: operation undefined:\n  (4/#Range0)\n])" == IO::to_string(solve("p(4/(0..2)); q.")));
        // negation
        REQUIRE("([[],[p(1),p(2),q],[p(1),q],[p(2),q]],[])" == IO::to_string(solve("not p(1..2); q. {p(1)}. {p(2)}.")));
        REQUIRE("([[],[p(1)]],[-:1:7-15: info: operation undefined:\n  (#Range0/X)\n,-:1:7-15: info: operation undefined:\n  (#Range0/X)\n,-:1:7-15: info: operation undefined:\n  (#Range0/X)\n])" == IO::to_string(solve("not p((0..2)/X); q :- X=0. {p(1)}.")));
        REQUIRE("([[]],[-:1:7-15: info: operation undefined:\n  (#Range0/0)\n])" == IO::to_string(solve("not p((0..2)/0); q :- X=0.")));
        REQUIRE("([[],[p(2),p(4),q],[p(2),q],[p(4),q]],[-:1:7-15: info: operation undefined:\n  (4/#Range0)\n])" == IO::to_string(solve("not p(4/(0..2)); q. {p(4)}. {p(2)}.")));
        // double negation
        REQUIRE("([[p(1),p(2)],[p(1),q],[p(2),q],[q]],[])" == IO::to_string(solve("not not p(1..2); q. {p(1)}. {p(2)}.")));
        REQUIRE("([[],[p(1)]],[-:1:11-19: info: operation undefined:\n  (#Range0/X)\n,-:1:11-19: info: operation undefined:\n  (#Range0/X)\n,-:1:11-19: info: operation undefined:\n  (#Range0/X)\n])" == IO::to_string(solve("not not p((0..2)/X); q :- X=0. {p(1)}.")));
        REQUIRE("([[]],[-:1:11-19: info: operation undefined:\n  (#Range0/0)\n])" == IO::to_string(solve("not not p((0..2)/0); q :- X=0.")));
        REQUIRE("([[p(2),p(4)],[p(2),q],[p(4),q],[q]],[-:1:11-19: info: operation undefined:\n  (4/#Range0)\n])" == IO::to_string(solve("not not p(4/(0..2)); q. {p(4)}. {p(2)}.")));

        // positive
        REQUIRE("([[p(1),p(2)],[q]],[])" == IO::to_string(solve("p(1..2):#true; q:#true.")));
        REQUIRE("([[]],[-:1:3-11: info: operation undefined:\n  (#Range0/X)\n,-:1:3-11: info: operation undefined:\n  (#Range0/X)\n,-:1:3-11: info: operation undefined:\n  (#Range0/X)\n])" == IO::to_string(solve("p((0..2)/X); q :- X=0.")));
        REQUIRE("([[]],[-:1:3-11: info: operation undefined:\n  (#Range0/0)\n])" == IO::to_string(solve("p((0..2)/0):#true; q:#true :- X=0.")));
        REQUIRE("([[p(2),p(4)],[q]],[-:1:3-11: info: operation undefined:\n  (4/#Range0)\n])" == IO::to_string(solve("p(4/(0..2)):#true; q:#true.")));
        // negation
        REQUIRE("([[],[p(1),p(2),q],[p(1),q],[p(2),q]],[])" == IO::to_string(solve("not p(1..2):#true; q:#true. {p(1)}. {p(2)}.")));
        REQUIRE("([[],[p(1)]],[-:1:7-15: info: operation undefined:\n  (#Range0/X)\n,-:1:7-15: info: operation undefined:\n  (#Range0/X)\n,-:1:7-15: info: operation undefined:\n  (#Range0/X)\n])" == IO::to_string(solve("not p((0..2)/X):#true; q:#true :- X=0. {p(1)}.")));
        REQUIRE("([[]],[-:1:7-15: info: operation undefined:\n  (#Range0/0)\n])" == IO::to_string(solve("not p((0..2)/0):#true; q:#true :- X=0.")));
        REQUIRE("([[],[p(2),p(4),q],[p(2),q],[p(4),q]],[-:1:7-15: info: operation undefined:\n  (4/#Range0)\n])" == IO::to_string(solve("not p(4/(0..2)):#true; q:#true. {p(4)}. {p(2)}.")));
        // double negation
        REQUIRE("([[p(1),p(2)],[p(1),q],[p(2),q],[q]],[])" == IO::to_string(solve("not not p(1..2):#true; q:#true. {p(1)}. {p(2)}.")));
        REQUIRE("([[],[p(1)]],[-:1:11-19: info: operation undefined:\n  (#Range0/X)\n,-:1:11-19: info: operation undefined:\n  (#Range0/X)\n,-:1:11-19: info: operation undefined:\n  (#Range0/X)\n])" == IO::to_string(solve("not not p((0..2)/X):#true; q:#true :- X=0. {p(1)}.")));
        REQUIRE("([[]],[-:1:11-19: info: operation undefined:\n  (#Range0/0)\n])" == IO::to_string(solve("not not p((0..2)/0):#true; q:#true :- X=0.")));
        REQUIRE("([[p(2),p(4)],[p(2),q],[p(4),q],[q]],[-:1:11-19: info: operation undefined:\n  (4/#Range0)\n])" == IO::to_string(solve("not not p(4/(0..2)):#true; q:#true. {p(4)}. {p(2)}.")));
    }
    SECTION("bug-disjunction-pools") {
        // positive
        REQUIRE("([[p(1),p(2)],[q]],[])" == IO::to_string(solve("p(1;2); q.")));
        REQUIRE("([[p(1),p(2)],[q]],[-:1:3-6: info: operation undefined:\n  (0/X)\n])" == IO::to_string(solve("p(0/X;1;2); q :- X=0.")));
        REQUIRE("([[p(1),p(2)],[q]],[-:1:3-6: info: operation undefined:\n  (X/0)\n])" == IO::to_string(solve("p(X/0;1;2); q :- X=0.")));
        REQUIRE("([[]],[-:1:3-6: info: operation undefined:\n  (0/X)\n])" == IO::to_string(solve("p(0/X); q :- X=0.")));
        REQUIRE("([[]],[-:1:3-6: info: operation undefined:\n  (X/0)\n])" == IO::to_string(solve("p(X/0); q :- X=0.")));
        // negation
        REQUIRE("([[],[p(1),p(2),q],[p(1),q],[p(2),q]],[])" == IO::to_string(solve("not p(1;2); q. {p(1)}. {p(2)}.")));
        REQUIRE("([[],[p(1),p(2),q],[p(1),q],[p(2),q]],[-:1:7-10: info: operation undefined:\n  (0/X)\n])" == IO::to_string(solve("not p(0/X;1;2); q :- X=0. {p(1)}. {p(2)}.")));
        REQUIRE("([[],[p(1),p(2),q],[p(1),q],[p(2),q]],[-:1:7-10: info: operation undefined:\n  (X/0)\n])" == IO::to_string(solve("not p(X/0;1;2); q :- X=0. {p(1)}. {p(2)}.")));
        REQUIRE("([[]],[-:1:5-11: info: atom does not occur in any rule head:\n  p((0/X))\n,-:1:7-10: info: operation undefined:\n  (0/X)\n])" == IO::to_string(solve("not p(0/X); q :- X=0.")));
        REQUIRE("([[]],[-:1:7-10: info: operation undefined:\n  (X/0)\n])" == IO::to_string(solve("not p(X/0); q :- X=0.")));
        // double negation
        REQUIRE("([[p(1),p(2)],[p(1),q]],[])" == IO::to_string(solve("not not p(1;2); q. p(1). {p(2)}.")));
        REQUIRE("([[p(1),p(2)],[p(1),q]],[-:1:11-14: info: operation undefined:\n  (0/X)\n])" == IO::to_string(solve("not not p(0/X;1;2); q :- X=0. p(1). {p(2)}.")));
        REQUIRE("([[p(1),p(2)],[p(1),q]],[-:1:11-14: info: operation undefined:\n  (X/0)\n])" == IO::to_string(solve("not not p(X/0;1;2); q :- X=0. p(1). {p(2)}.")));
        REQUIRE("([[]],[-:1:9-15: info: atom does not occur in any rule head:\n  p((0/X))\n,-:1:11-14: info: operation undefined:\n  (0/X)\n])" == IO::to_string(solve("not not p(0/X); q :- X=0.")));
        REQUIRE("([[]],[-:1:11-14: info: operation undefined:\n  (X/0)\n])" == IO::to_string(solve("not not p(X/0); q :- X=0.")));

        // positive
        REQUIRE("([[p(1),p(2)],[q]],[])" == IO::to_string(solve("p(1;2):#true; q:#true.")));
        REQUIRE("([[p(1),p(2)],[q]],[-:1:3-6: info: operation undefined:\n  (0/X)\n])" == IO::to_string(solve("p(0/X;1;2):#true; q:#true :- X=0.")));
        REQUIRE("([[p(1),p(2)],[q]],[-:1:3-6: info: operation undefined:\n  (X/0)\n])" == IO::to_string(solve("p(X/0;1;2):#true; q:#true :- X=0.")));
        REQUIRE("([[]],[-:1:3-6: info: operation undefined:\n  (0/X)\n])" == IO::to_string(solve("p(0/X):#true; q:#true :- X=0.")));
        REQUIRE("([[]],[-:1:3-6: info: operation undefined:\n  (X/0)\n])" == IO::to_string(solve("p(X/0):#true; q:#true :- X=0.")));
        // negation
        REQUIRE("([[],[p(1),p(2),q],[p(1),q],[p(2),q]],[])" == IO::to_string(solve("not p(1;2):#true; q:#true. {p(1)}. {p(2)}.")));
        REQUIRE("([[],[p(1),p(2),q],[p(1),q],[p(2),q]],[-:1:7-10: info: operation undefined:\n  (0/X)\n])" == IO::to_string(solve("not p(0/X;1;2):#true; q:#true :- X=0. {p(1)}. {p(2)}.")));
        REQUIRE("([[],[p(1),p(2),q],[p(1),q],[p(2),q]],[-:1:7-10: info: operation undefined:\n  (X/0)\n])" == IO::to_string(solve("not p(X/0;1;2):#true; q:#true :- X=0. {p(1)}. {p(2)}.")));
        REQUIRE("([[]],[-:1:5-11: info: atom does not occur in any rule head:\n  p((0/X))\n,-:1:7-10: info: operation undefined:\n  (0/X)\n])" == IO::to_string(solve("not p(0/X):#true; q:#true :- X=0.")));
        REQUIRE("([[]],[-:1:7-10: info: operation undefined:\n  (X/0)\n])" == IO::to_string(solve("not p(X/0):#true; q:#true :- X=0.")));
        // double negation
        REQUIRE("([[p(1),p(2)],[p(1),q]],[])" == IO::to_string(solve("not not p(1;2):#true; q:#true. p(1). {p(2)}.")));
        REQUIRE("([[p(1),p(2)],[p(1),q]],[-:1:11-14: info: operation undefined:\n  (0/X)\n])" == IO::to_string(solve("not not p(0/X;1;2):#true; q:#true :- X=0. p(1). {p(2)}.")));
        REQUIRE("([[p(1),p(2)],[p(1),q]],[-:1:11-14: info: operation undefined:\n  (X/0)\n])" == IO::to_string(solve("not not p(X/0;1;2):#true; q:#true :- X=0. p(1). {p(2)}.")));
        REQUIRE("([[]],[-:1:9-15: info: atom does not occur in any rule head:\n  p((0/X))\n,-:1:11-14: info: operation undefined:\n  (0/X)\n])" == IO::to_string(solve("not not p(0/X):#true; q:#true :- X=0.")));
        REQUIRE("([[]],[-:1:11-14: info: operation undefined:\n  (X/0)\n])" == IO::to_string(solve("not not p(X/0):#true; q:#true :- X=0.")));
    }
    SECTION("bug-classify-conditional-literal") {
        REQUIRE(
            "([[p,p(1),p(2),q(1),q(2)],[p,p(1),q(1),q(2),r(2)],[p,p(2),q(1),q(2),r(1)]],[])" ==
            IO::to_string(solve(
                "q(1;2).\n"

                "p(X) :- q(X); p(X) : r(X).\n"
                "r(X) :- q(X); r(X) : p(X).\n"

                "p :- p(X).\n"
                "  :- not p.\n"
            ))
        );
    }
    SECTION("bug-classical-negation") {
        REQUIRE(
            "([],[])" ==
            IO::to_string(solve(
                "p. -p."
            ))
        );
        REQUIRE(
            "([[-p]],[])" ==
            IO::to_string(solve(
                "-p."
            ))
        );
    }
    SECTION("unpool") {
        REQUIRE(
            "([[a(1),a(2),a(4)]],[])" ==
            IO::to_string(solve(
                "a(X) :- X=(1;2;4)."
            ))
        );
        REQUIRE(
            "([[p(1),p(2),q(3)],[p(1),p(2),q(4)]],[])" ==
            IO::to_string(solve(
                "1 {q(3;4)} 1."
                "p(1;2) :- q(3;4)."
            ))
        );
        REQUIRE(
            "([[a,p(1),q(3)],[a,p(1),q(4)],[a,p(2),q(3)],[a,p(2),q(4)]],[])" ==
            IO::to_string(solve(
                "1 { p(1;2) } 1."
                "1 { q(3;4) } 1."
                "a :- p(1;2) : q(3;4)."
            ))
        );
        REQUIRE(
            "([[p(1),p(2),q(3)],[p(1),p(2),q(4)]],[])" ==
            IO::to_string(solve(
                "1 { q(3;4) } 1."
                "p(1;2) : q(3;4)."
            ))
        );
        REQUIRE(
            "([[a,p(1),q(3)],[a,p(1),q(4)],[a,p(2),q(3)],[a,p(2),q(4)],[q(3)],[q(4)]],[])" ==
            IO::to_string(solve(
                "1 { q(3;4) } 1."
                "0 { p(1;2) } 1."
                "a :- (1;2) { q(3;4) : p(1;2) }."
            ))
        );
        REQUIRE(
            "([[],[a,p(1)],[a,p(2)]],[])" ==
            IO::to_string(solve(
                "0 { p(1;2) } 1."
                "a :- (2;3) #count { q(3;4) : p(1;2) }."
            ))
        );
        REQUIRE(
            "([[p(1),q(3),q(4)],[p(2),q(3),q(4)]],[])" ==
            IO::to_string(solve(
                "0 { p(1;2) } 1."
                "(1;2) { q(3;4) : p(1;2) }."
            ))
        );
        REQUIRE(
            "([[p(1),q(3)],[p(1),q(3),q(4)],[p(1),q(4)],[p(2),q(3)],[p(2),q(3),q(4)],[p(2),q(4)]],[])" ==
            IO::to_string(solve(
                "0 { p(1;2) } 1."
                "(1;2) #count { t(5;6) : q(3;4) : p(1;2) }."
            ))
        );
    }
    SECTION("rewrite-bug1") {
        REQUIRE(
            "([[p(1),p(2),q(1),q(2)],[p(1),q(1),q(2)],[p(2),q(1),q(2)],[q(1),q(2)]],[])" ==
            IO::to_string(solve(
                "q(1..2)."
                "{ p(X) : q(X) } :- 2 { q(X) }."
            ))
        );
    }
    SECTION("rewrite-bug2") {
        REQUIRE(
            "([[x]],[-:1:19-25: info: atom does not occur in any rule head:\n"
            "  p(#Arith0)\n"
            ",-:1:21-24: info: operation undefined:\n"
            "  (X+Y)\n"
            "])" ==
            IO::to_string(solve(
                "x :- #count { 1 : p(X+Y) } >= 0, X=1, Y=a."
            ))
        );
    }
    SECTION("recCondBug") {
        REQUIRE(
            "([[0,2,4,5,9],[0,2,5,9],[0,4,9],[0,9]],[])" ==
            IO::to_string(solve(
                "holds(atom(A)) :- rule(lit(pos,atom(A)),B); holds(B).\n"
                "{ holds(atom(A)) : head_aggregate_element_set(I, head_aggregate_element(_,lit(pos,atom(A)),C))\n"
                "                 , holds(C)\n"
                "} :- rule(head_aggregate(left(#inf,less_equal),count,head_aggregate_element_set(I),right(less_equal,#sup)),C)\n"
                "   ; holds(C)\n"
                "   .\n"
                "% :- rule(lit(pos,false),B); holds(B) .\n"
                "% NOTE: there are more heads but this is enough to support --lparse-rewrite\n"
                "\n"
                "holds(conjunction(I)) :- conjunction(I)\n"
                "                       ;         holds(A) : conjunction(I,lit(pos,A))\n"
                "                       ; not     holds(A) : conjunction(I,lit(neg,A))\n"
                "                       ; not not holds(A) : conjunction(I,lit(neg_neg,A))\n"
                "                       .\n"
                "\n"
                "body_aggregate(L,F,S,U) :- conjunction(_,lit(_,body_aggregate(L,F,S,U))). % TODO: generate in meta\n"
                "\n"
                "holds(body_aggregate(left(#inf,less_equal),sump,body_aggregate_element_set(S),right(greater_equal,U)))\n"
                "    :- body_aggregate(left(#inf,less_equal),sump,body_aggregate_element_set(S),right(greater_equal,U))\n"
                "     ; #sum+ { W,T : body_aggregate_element_set(S,body_aggregate_element((W,T),conjunction(C))), holds(C) } >= U % TODO: if the holds is omitted strange things appear to happen\n"
                "     .\n"
                "% NOTE: there are more body aggregates but this is enough to support --lparse-rewrite\n"
                "\n"
                "% TODO: handle minimize constraint\n"
                "\n"
                "#show.\n"
                "#show I : holds(conjunction(I)), conjunction(I).\n"
                "rule(head_aggregate(left(#inf,less_equal),count,head_aggregate_element_set(0),right(less_equal,#sup)),conjunction(0)).\n"
                "conjunction(0).\n"
                "head_aggregate_element_set(0).\n"
                "head_aggregate_element_set(0,head_aggregate_element((),lit(pos,atom(p(1))),conjunction(0))).\n"
                "head_aggregate_element_set(0,head_aggregate_element((),lit(pos,atom(p(2))),conjunction(0))).\n"
                "rule(lit(pos,atom(__aux(1))),conjunction(1)).\n"
                "conjunction(1).\n"
                "conjunction(1,lit(pos,atom(p(2)))).\n"
                "conjunction(1,lit(neg,atom(p(2)))).\n"
                "rule(lit(pos,atom(__aux(2))),conjunction(2)).\n"
                "conjunction(2).\n"
                "conjunction(2,lit(pos,atom(p(2)))).\n"
                "rule(lit(pos,atom(__aux(2))),conjunction(3)).\n"
                "conjunction(3).\n"
                "conjunction(3,lit(pos,atom(__aux(1)))).\n"
                "rule(lit(pos,atom(__aux(4))),conjunction(6)).\n"
                "conjunction(4).\n"
                "conjunction(4,lit(pos,atom(p(1)))).\n"
                "conjunction(5).\n"
                "conjunction(5,lit(pos,atom(__aux(2)))).\n"
                "body_aggregate_element_set(0).\n"
                "body_aggregate_element_set(0,body_aggregate_element((1,(0,())),conjunction(4))).\n"
                "body_aggregate_element_set(0,body_aggregate_element((2,(1,())),conjunction(5))).\n"
                "conjunction(6).\n"
                "conjunction(6,lit(pos,body_aggregate(left(#inf,less_equal),sump,body_aggregate_element_set(0),right(greater_equal,1)))).\n"
                "rule(lit(pos,atom(__aux(5))),conjunction(7)).\n"
                "conjunction(7).\n"
                "conjunction(7,lit(pos,body_aggregate(left(#inf,less_equal),sump,body_aggregate_element_set(0),right(greater_equal,3)))).\n"
                "rule(lit(pos,atom(__aux(3))),conjunction(8)).\n"
                "conjunction(8).\n"
                "conjunction(8,lit(pos,atom(__aux(4)))).\n"
                "conjunction(8,lit(neg,atom(__aux(5)))).\n"
                "rule(lit(pos,false),conjunction(9)).\n"
                "conjunction(9).\n"
                "conjunction(9,lit(neg,atom(__aux(3)))).\n"
                "show_atom(p(1)).\n"
                "show_atom(p(2)).\n"
            ))
        );
    }

    SECTION("empty") {
        REQUIRE("([[]],[])" == IO::to_string(solve("")));
    }

    SECTION("projectionBug") {
        REQUIRE("([[p(1),p(2)]],[])" == IO::to_string(solve(
            "q((1,x),2).\n"
            "p(A) :- q((A,_),_).\n"
            "p(B) :- q((A,_),B).\n"
            , {"p("})));
    }
    SECTION("aggregateBug") {
        REQUIRE(
            "([[a(1),a(2),b(1)],[a(1),a(2),b(1),b(2)]],[])" ==
            IO::to_string(solve(
            "a(1)."
            "a(2)."
            "b(1)."
            "{ b(X) } :- a(X).")));
    }

    SECTION("aggregateMinBug") {
        REQUIRE(
            "([[a(20),a(50),a(60),output(20)],[a(20),a(50),output(20)],[a(50),a(60),output(50)],[a(50),output(50)]],[])" ==
            IO::to_string(solve(
                "a(50)."
                "{ a(20) ; a(60) }."
                "output(X) :- X = #min { C : a(C) }."
            )));
    }

    SECTION("recHeadAggregateBug") {
        REQUIRE(
            "(["
            "[r(v(x13),e(r1,n(a11))),r(v(x13),e(r1,v(x3))),r(v(x13),e(r1,v(x7))),r(v(x13),n(a11)),r(v(x7),e(r1,n(a11))),r(v(x7),e(r1,v(x4))),r(v(x7),n(a11)),r(v(x9),e(r1,n(a11))),r(v(x9),e(r1,v(x3))),r(v(x9),e(r1,v(x7)))],"
            "[r(v(x13),e(r1,n(a11))),r(v(x13),e(r1,v(x3))),r(v(x13),e(r1,v(x7))),r(v(x13),n(a11)),r(v(x7),e(r1,n(a11))),r(v(x7),e(r1,v(x4))),r(v(x9),e(r1,v(x3))),r(v(x9),e(r1,v(x7)))],"
            "[r(v(x13),e(r1,n(a11))),r(v(x13),e(r1,v(x3))),r(v(x13),e(r1,v(x7))),r(v(x7),e(r1,n(a11))),r(v(x7),e(r1,v(x4))),r(v(x9),e(r1,v(x3))),r(v(x9),e(r1,v(x7)))],"
            "[r(v(x13),e(r1,n(a11))),r(v(x13),e(r1,v(x3))),r(v(x13),n(a11)),r(v(x7),e(r1,n(a11))),r(v(x7),e(r1,v(x4))),r(v(x7),n(a11)),r(v(x9),e(r1,n(a11))),r(v(x9),e(r1,v(x3)))],"
            "[r(v(x13),e(r1,n(a11))),r(v(x13),e(r1,v(x3))),r(v(x13),n(a11)),r(v(x7),e(r1,n(a11))),r(v(x7),e(r1,v(x4))),r(v(x7),n(a11)),r(v(x9),e(r1,n(a11))),r(v(x9),e(r1,v(x3))),r(v(x9),e(r1,v(x7)))],"
            "[r(v(x13),e(r1,n(a11))),r(v(x13),e(r1,v(x3))),r(v(x13),n(a11)),r(v(x7),e(r1,n(a11))),r(v(x7),e(r1,v(x4))),r(v(x9),e(r1,v(x3)))],"
            "[r(v(x13),e(r1,n(a11))),r(v(x13),e(r1,v(x3))),r(v(x13),n(a11)),r(v(x7),e(r1,n(a11))),r(v(x7),e(r1,v(x4))),r(v(x9),e(r1,v(x3))),r(v(x9),e(r1,v(x7)))],"
            "[r(v(x13),e(r1,n(a11))),r(v(x13),e(r1,v(x3))),r(v(x7),e(r1,n(a11))),r(v(x7),e(r1,v(x4))),r(v(x9),e(r1,v(x3)))],"
            "[r(v(x13),e(r1,n(a11))),r(v(x13),e(r1,v(x3))),r(v(x7),e(r1,n(a11))),r(v(x7),e(r1,v(x4))),r(v(x9),e(r1,v(x3))),r(v(x9),e(r1,v(x7)))],"
            "[r(v(x13),e(r1,n(a11))),r(v(x13),n(a11)),r(v(x7),e(r1,n(a11))),r(v(x7),e(r1,v(x4))),r(v(x7),n(a11)),r(v(x9),e(r1,n(a11))),r(v(x9),e(r1,v(x3)))],"
            "[r(v(x13),e(r1,n(a11))),r(v(x13),n(a11)),r(v(x7),e(r1,n(a11))),r(v(x7),e(r1,v(x4))),r(v(x7),n(a11)),r(v(x9),e(r1,n(a11))),r(v(x9),e(r1,v(x3))),r(v(x9),e(r1,v(x7)))],"
            "[r(v(x13),e(r1,n(a11))),r(v(x13),n(a11)),r(v(x7),e(r1,n(a11))),r(v(x7),e(r1,v(x4))),r(v(x9),e(r1,v(x3)))],"
            "[r(v(x13),e(r1,n(a11))),r(v(x13),n(a11)),r(v(x7),e(r1,n(a11))),r(v(x7),e(r1,v(x4))),r(v(x9),e(r1,v(x3))),r(v(x9),e(r1,v(x7)))],"
            "[r(v(x13),e(r1,n(a11))),r(v(x7),e(r1,n(a11))),r(v(x7),e(r1,v(x4))),r(v(x9),e(r1,v(x3)))],"
            "[r(v(x13),e(r1,n(a11))),r(v(x7),e(r1,n(a11))),r(v(x7),e(r1,v(x4))),r(v(x9),e(r1,v(x3))),r(v(x9),e(r1,v(x7)))],"
            "[r(v(x13),e(r1,v(x3))),r(v(x13),e(r1,v(x7))),r(v(x13),n(a11)),r(v(x7),e(r1,n(a11))),r(v(x7),e(r1,v(x4))),r(v(x9),e(r1,v(x3))),r(v(x9),e(r1,v(x7)))],"
            "[r(v(x13),e(r1,v(x3))),r(v(x13),e(r1,v(x7))),r(v(x7),e(r1,n(a11))),r(v(x7),e(r1,v(x4))),r(v(x9),e(r1,v(x3))),r(v(x9),e(r1,v(x7)))],"
            "[r(v(x13),e(r1,v(x3))),r(v(x13),n(a11)),r(v(x7),e(r1,n(a11))),r(v(x7),e(r1,v(x4))),r(v(x7),n(a11)),r(v(x9),e(r1,n(a11))),r(v(x9),e(r1,v(x3)))],"
            "[r(v(x13),e(r1,v(x3))),r(v(x13),n(a11)),r(v(x7),e(r1,n(a11))),r(v(x7),e(r1,v(x4))),r(v(x7),n(a11)),r(v(x9),e(r1,n(a11))),r(v(x9),e(r1,v(x3))),r(v(x9),e(r1,v(x7)))],"
            "[r(v(x13),e(r1,v(x3))),r(v(x13),n(a11)),r(v(x7),e(r1,n(a11))),r(v(x7),e(r1,v(x4))),r(v(x9),e(r1,v(x3)))],"
            "[r(v(x13),e(r1,v(x3))),r(v(x13),n(a11)),r(v(x7),e(r1,n(a11))),r(v(x7),e(r1,v(x4))),r(v(x9),e(r1,v(x3))),r(v(x9),e(r1,v(x7)))],"
            "[r(v(x13),e(r1,v(x3))),r(v(x7),e(r1,n(a11))),r(v(x7),e(r1,v(x4))),r(v(x9),e(r1,v(x3)))],"
            "[r(v(x13),e(r1,v(x3))),r(v(x7),e(r1,n(a11))),r(v(x7),e(r1,v(x4))),r(v(x9),e(r1,v(x3))),r(v(x9),e(r1,v(x7)))],"
            "[r(v(x13),n(a11)),r(v(x7),e(r1,n(a11))),r(v(x7),e(r1,v(x4))),r(v(x7),n(a11)),r(v(x9),e(r1,n(a11))),r(v(x9),e(r1,v(x3)))],"
            "[r(v(x13),n(a11)),r(v(x7),e(r1,n(a11))),r(v(x7),e(r1,v(x4))),r(v(x7),n(a11)),r(v(x9),e(r1,n(a11))),r(v(x9),e(r1,v(x3))),r(v(x9),e(r1,v(x7)))],"
            "[r(v(x13),n(a11)),r(v(x7),e(r1,n(a11))),r(v(x7),e(r1,v(x4))),r(v(x9),e(r1,v(x3)))],"
            "[r(v(x13),n(a11)),r(v(x7),e(r1,n(a11))),r(v(x7),e(r1,v(x4))),r(v(x9),e(r1,v(x3))),r(v(x9),e(r1,v(x7)))],"
            "[r(v(x7),e(r1,n(a11))),r(v(x7),e(r1,v(x4))),r(v(x9),e(r1,v(x3)))],"
            "[r(v(x7),e(r1,n(a11))),r(v(x7),e(r1,v(x4))),r(v(x9),e(r1,v(x3))),r(v(x9),e(r1,v(x7)))]"
            "],[])" == IO::to_string(solve(
            "atom(A) :- hasatom(A,_,_).\n"
            "atom(A) :- hasatom(e(_,A),_,_).\n"
            "nonvatom(X) :- atom(X), X!=v(V):atom(v(V)).\n"
            "\n"
            "triviallyfalse(n(X),n(Y)) :- atom(n(X)), atom(n(Y)), X!=Y.\n"
            "triviallyfalse(e(R1,C1),e(R2,C2)) :- atom(e(R1,C1)),\n"
            "                                               atom(e(R2,C2)), R1!=R2. \n"
            "triviallyfalse(n(C),e(R,C1)) :- atom(n(C)), atom(e(R,C1)).\n"
            "triviallyfalse(e(R,C1),n(C)) :- atom(n(C)), atom(e(R,C1)).\n"
            "\n"
            "subs(X,Y) :- subs(e(R,X),e(R,Y)).\n"
            "\n"
            "1 { subs(Y,X):hasatom(Y,|P-1|,E),not triviallyfalse(Y,X) } :- hasatom(X,P,E), nonvatom(X).\n"
            "\n"
            "1 { subs(Y,C):hasatom(Y,|P-1|,E),not triviallyfalse(Y,C) } :- subs(X,C), hasatom(X,P,E),\n"
            "                  nonvatom(C), not hasatom(C,PX,E):hasatom(C,PX,E).\n"
            "\n"
            "subs(X,Z) :- subs(X,Y), subs(Y,Z), X!=Z, X!=Y, Z!=Y.\n"
            "\n"
            ":- subs(X,Y), triviallyfalse(X,Y).\n"
            "\n"
            "greater(X,Y) :- subs(v(X),e(R,v(Y))).\n"
            ":- greater(X,X).\n"
            "greater(X,Z) :- greater(X,Y), greater(Y,Z), X!=Y, X!=Z, Y!=Z.\n"
            "\n"
            "atom(C) :- diseq(C,D).\n"
            "atom(D) :- diseq(C,D).\n"
            "\n"
            "1 { dissubs(X,Y); dissubs(Y,X) } :- diseq(X,Y).\n"
            "ok(C,D) :- dissubs(C,D), subs(D,D'), nonvatom(D'), not subs(C,D'), not subs(C',D'):subs(C,C').\n"
            ":- dissubs(C,D), not ok(C,D).\n"
            "\n"
            "r(v(X),C) :- subs(v(X),C), relevant(X), nonvatom(C).\n"
            "\n"
            "%equation 1\n"
            "hasatom(v(x3), 0, 1).\n"
            "hasatom(e(r1, n(a11)), 1, 1).\n"
            "\n"
            "%equation 2\n"
            "hasatom(v(x9), 0, 2).\n"
            "hasatom(v(x5), 1, 2).\n"
            "hasatom(v(x9), 1, 2).\n"
            "\n"
            "%equation 3\n"
            "hasatom(v(x6), 0, 3).\n"
            "hasatom(v(x4), 1, 3).\n"
            "hasatom(v(x9), 1, 3).\n"
            "\n"
            "%equation 4\n"
            "hasatom(v(x1), 0, 4).\n"
            "hasatom(v(x1), 1, 4).\n"
            "hasatom(v(x9), 1, 4).\n"
            "\n"
            "%equation 5\n"
            "hasatom(v(x1), 0, 5).\n"
            "hasatom(e(r1, v(x7)), 1, 5).\n"
            "\n"
            "%equation 6\n"
            "hasatom(v(x4), 0, 6).\n"
            "hasatom(v(x3), 1, 6).\n"
            "hasatom(v(x13), 1, 6).\n"
            "\n"
            "%equation 7\n"
            "hasatom(v(x2), 0, 7).\n"
            "hasatom(e(r1, v(x4)), 1, 7).\n"
            "\n"
            "%equation 8\n"
            "hasatom(v(x7), 0, 8).\n"
            "hasatom(v(x2), 1, 8).\n"
            "hasatom(v(x7), 1, 8).\n"
            "\n"
            "%equation 9\n"
            "hasatom(v(x5), 0, 9).\n"
            "hasatom(e(r1, v(x3)), 1, 9).\n"
            "\n"
            "%disequation 1\n"
            "diseq(v(x6), v(x9)).\n"
            "\n"
            "relevant(x7).\n"
            "relevant(x9).\n"
            "relevant(x13).\n"
            , {"r("})));

    }

    SECTION("disjunctionBug") {
        REQUIRE(
            "([],[-:1:21-22: info: atom does not occur in any rule head:\n  d\n])" ==
            IO::to_string(solve(
                "b."
                "c :- b."
                "a :- c."
                "c : d :- a."
            )));
    }

    SECTION("disjunctionBug2") {
        REQUIRE(
            "([[b(0),h(0),h(1),p(0)]],[])" ==
            IO::to_string(solve(
                "p(0)."
                "h(0;1)."
                "b(0)."
                "p(X) : h(X) :- p(Y), b(Y)."
            )));
    }

    SECTION("aggregateNotNot") {
        REQUIRE(
            "([[p(2)],[p(2),p(3)],[p(2),p(3),p(4)],[p(2),p(3),p(4),p(5)],[p(2),p(3),p(5)],[p(2),p(4)],[p(2),p(4),p(5)],[p(2),p(5)],[p(4)],[p(4),p(5)]],[])" ==
            IO::to_string(solve(
            "{ p(1..5) }."
            ":- not not 2 != #min { X:p(X) } != 4.")));
        REQUIRE(
            "([[p(2)],[p(2),p(3)],[p(2),p(3),p(4)],[p(2),p(3),p(4),p(5)],[p(2),p(3),p(5)],[p(2),p(4)],[p(2),p(4),p(5)],[p(2),p(5)],[p(4)],[p(4),p(5)]],[])" ==
            IO::to_string(solve(
            "{ p(1..5) }."
            ":- 2 != #min { X:p(X) } != 4.")));
        REQUIRE(
            "([[p(2)],[p(2),p(3)],[p(2),p(3),p(4)],[p(2),p(3),p(4),p(5)],[p(2),p(3),p(5)],[p(2),p(4)],[p(2),p(4),p(5)],[p(2),p(5)],[p(4)],[p(4),p(5)]],[])" ==
            IO::to_string(solve(
            "{ p(1..5) }."
            "h :- not 2 != #min { X:p(X) } != 4."
            ":- not h.", {"p("})));
        REQUIRE(
            "([[p(2)],[p(2),p(3)],[p(2),p(3),p(4)],[p(2),p(3),p(4),p(5)],[p(2),p(3),p(5)],[p(2),p(4)],[p(2),p(4),p(5)],[p(2),p(5)],[p(4)],[p(4),p(5)]],[])" ==
            IO::to_string(solve(
            "{ p(1..5) }."
            ":- not 2 = #min { X:p(X) }, not #min { X:p(X) } = 4.")));
    }
    SECTION("aggregateRecBug") {
        REQUIRE(
            "([],[])" ==
            IO::to_string(solve(
            "a :- {a}!=1."
            )));
        REQUIRE(
            "([],[])" ==
            IO::to_string(solve(
            "a :- #sum {1:a}!=1."
            )));
        REQUIRE(
            "([[b]],[])" ==
            IO::to_string(solve(
                "b :- 0  #sum+ { 1: b }."
            )));
        REQUIRE(
            "([[b]],[])" ==
            IO::to_string(solve(
                "b :- 0  #sum { 1: b }."
            )));
    }
    SECTION("symTabBug") {
        REQUIRE(
            "([[does(a,0),does(a,1)],[does(a,0),does(b,1)]],[])" ==
            IO::to_string(solve(
                "time(0..1).\n"
                "1 { does(M,T) : legal(M,T) } 1 :- time(T).\n"
                "legal(a,T) :- time(T).\n"
                "legal(b,T) :- does(a,0), time(T).\n",
                {"does"})));
    }
    SECTION("recAntiAggr") {
        REQUIRE(
            "([[p],[r]],[])" ==
            IO::to_string(solve(
                "r :- #sum { 1:p } < 1.\n"
                "p :- not r.")));
        REQUIRE(
            "([[],[p]],[])" ==
            IO::to_string(solve("p :- #sum { 1:not p } < 1.")));
        REQUIRE(
            "([[],[p]],[])" ==
            IO::to_string(solve("p :- not #sum { 1:p } < 1.")));
        REQUIRE(
            "([[],[p]],[])" ==
            IO::to_string(solve("p :- not #sum { 1:not p } > 0.")));
        REQUIRE(
            "([[],[p]],[])" ==
            IO::to_string(solve("p :- not not #sum { 1:p } > 0.")));
        REQUIRE(
            "([[],[p]],[])" ==
            IO::to_string(solve("p :- not not #sum { 1:not p } < 1.")));
    }
    SECTION("headAggrPropagateBug") {
        REQUIRE(
            "([[a(c),a(p),b(c,d),b(c,e),b(c,f),b(c,g),b(p,d),b(p,e),b(p,f),b(p,g)],"
            "[a(c),a(p),b(c,d),b(c,e),b(c,g),b(p,d),b(p,e),b(p,g),e(3)],"
            "[b(c,d),b(c,e),b(p,d),b(p,e),e(2),e(3)]],[-:1:19-20: info: atom does not occur in any rule head:\n  c\n])" == IO::to_string(solve(
                "b(S,h) :- b(S,X), c.\n"
                "b(c,d).\n"
                "b(p,X) :- b(c,X).\n"
                "b(c,e).\n"
                "a(S)   :- b(S,g).\n"
                "1 = { e(3); b(S,f) : a(S) } :- b(S,e).\n"
                "1 = { e(2); b(S,g) } :- b(S,d).\n")));
        //REQUIRE("[-:1:19-20: info: atom does not occur in any rule head:\n  c\n]" == IO::to_string(msg.messages));
    }
    SECTION("headAggregateBug") {
        REQUIRE("([[q(a),r(a)]],[-:1:18-22: info: atom does not occur in any rule head:\n  z(X)\n])" == IO::to_string(solve("1 { q(a); p(X) : z(X) }. r(X) :- q(X).")));
        REQUIRE("([[q(a),r(a)]],[-:1:12-16: info: atom does not occur in any rule head:\n  z(X)\n])" == IO::to_string(solve("1 { p(X) : z(X); q(a) }. r(X) :- q(X).")));
    }
    SECTION("head") {
        REQUIRE("([[],[a],[a,b],[b]],[])" == IO::to_string(solve("{a;b}.")));
        REQUIRE("([[a],[b]],[])" == IO::to_string(solve("1{a;b}1.")));
        REQUIRE("([[p(1)],[p(1),p(2)],[p(1),p(3)],[p(1),p(4)],[p(2)],[p(2),p(3)],[p(2),p(4)],[p(3)],[p(3),p(4)],[p(4)]],[])" == IO::to_string(solve("1#count{X:p(X):X=1..4}2.")));
        REQUIRE("([[p(1)],[p(2)]],[])" == IO::to_string(solve("1#sum+{X:p(X):X=1..4}2.")));
        REQUIRE("([[p(1)],[p(2)]],[])" == IO::to_string(solve("1#sum {X:p(X):X=1..4}2.")));
        REQUIRE(
            "([[p(1)],[p(1),p(2)],[p(1),p(2),p(3)],[p(1),p(2),p(3),p(4)],[p(1),p(2),p(4)],[p(1),p(3)],[p(1),p(3),p(4)],[p(1),p(4)],"
            "[p(2)],[p(2),p(3)],[p(2),p(3),p(4)],[p(2),p(4)]],[])" == IO::to_string(solve("1#min{X:p(X):X=1..4}2.")));
        REQUIRE("([[p(1)],[p(1),p(2)],[p(2)]],[])" == IO::to_string(solve("1#max{X:p(X):X=1..4}2.")));
        REQUIRE("([[c,p]],[])" == IO::to_string(solve("{p}. 1 {c:p}.")));
    }

    SECTION("assign") {
        REQUIRE("([[p,q(1)],[q(0)]],[])" ==IO::to_string(solve("{p}. q(M):-M=#count{1:p}.")));
        REQUIRE("([[p,q(1)],[q(0)]],[])" ==IO::to_string(solve("{p}. q(M):-M=#sum+{1:p}.")));
        REQUIRE("([[p,q(1)],[q(0)]],[])" ==IO::to_string(solve("{p}. q(M):-M=#sum{1:p}.")));
        REQUIRE("([[p,q(p)],[q(#sup)]],[])" ==IO::to_string(solve("{p}. q(M):-M=#min{p:p}.")));
        REQUIRE("([[p,q(p)],[q(#inf)]],[])" ==IO::to_string(solve("{p}. q(M):-M=#max{p:p}.")));
        REQUIRE(
            "([[p(1),p(2),q(1)],"
            "[p(1),p(3),q(1)],"
            "[p(1),p(4),q(1)],"
            "[p(2),p(3),q(2)],"
            "[p(2),p(4),q(2)],"
            "[p(3),p(4),q(3)]],[])" == IO::to_string(solve("2{p(1..4)}2. q(M):-M=#min{X:p(X)}.")));
        REQUIRE(
            "([[p(1),p(2),q(2)],"
            "[p(1),p(3),q(3)],"
            "[p(1),p(4),q(4)],"
            "[p(2),p(3),q(3)],"
            "[p(2),p(4),q(4)],"
            "[p(3),p(4),q(4)]],[])" == IO::to_string(solve("2{p(1..4)}2. q(M):-M=#max{X:p(X)}.")));
    }

    SECTION("conjunction") {
        REQUIRE(
            "([],[-:1:4-5: info: atom does not occur in any rule head:\n  b\n])" ==
            IO::to_string(solve(
                "a:-b:c.\n"
                "c:-a.\n")));
        REQUIRE(
            "([[a,b,c]],[])" ==
            IO::to_string(solve(
                "a:-b:c.\n"
                "c:-a.\n"
                "b:-c.\n", {"a", "b", "c"})));
        REQUIRE(
            "([[a,b,c,d]],[])" ==
            IO::to_string(solve(
                "a:-b:c,d.\n"
                "c:-a.\n"
                "d:-a.\n"
                "b:-c.\n"
                "b:-d.\n", {"a","b","c","d"})));
        REQUIRE(
            "([[a(1),a(2),a(3),c,q],[a(3)]],[])" ==
            IO::to_string(solve(
                "{c}.\n"
                "a(1):-c.\n"
                "a(2):-c.\n"
                "a(3).\n"
                "q:-a(X):X=1..3.\n")));
        REQUIRE(
            "([[p],[q]],[])" ==
            IO::to_string(solve(
                "p :- p:q.\n"
                "q :- q:p.\n")));
        REQUIRE(
            "([[p,q]],[])" ==
            IO::to_string(solve(
                "p :- p:q.\n"
                "q :- q:p.\n"
                "p :- q.\n"
                "q :- p.\n")));
        REQUIRE(
            "([[],[p(1),r,s],[p(2),q,r],[p(3),r,s]],[])" ==
            IO::to_string(solve(
                "{ p(1..3) } 1.\n"
                "q :- p(Y..X) : Y = 2, X = 2.\n"
                "r :- p(X) : #true.\n"
                "s :- p(X-1;2*(X..X+1)-3) : X=2.\n"
                )));
        REQUIRE(
            "([[d(a),d(b),q(1,a),q(2,a)],[d(a),d(b),q(1,b),q(2,b)]],[])" ==
            IO::to_string(solve(
                "d(a;b).\n"
                "2 { q(1,a); q(1,b); q(2,a); q(2,b) } 2.\n"
                " :- q(1,A;2,A) : d(A).\n"
                )));
    }
    SECTION("disjunction") {
        REQUIRE(
            "([[],[a,b,c,d],[c,x,y]],[])" ==
            IO::to_string(solve(
                "{ y; d } 1.\n"
                "c :- y.\n"
                "c :- d.\n"
                "b :- d.\n"
                "x:y | a:b :- c.\n"
                "b :- a.\n"
                )));
        REQUIRE(
            "([[a,b,c,d,x5]],[])" ==
            IO::to_string(solve(
                "x5:-b.\n"
                "x5:-not c.\n"
                "d:-c.\n"
                "c:-d.\n"
                "\n"
                "x5|d:-not not b.\n"
                "a:-x5.\n"
                "\n"
                "c:-a.\n"
                "b:-c.\n"
                )));

        REQUIRE(
            "([[a,b,c],[b],[b,c],[c]],[])" ==
            IO::to_string(solve(
                "1{b;c}.\n"
                "a:b,c;not a.\n")));
        REQUIRE(
            "([[],[p(1)],[p(1),p(2)],[p(2)]],[])" ==
            IO::to_string(solve(
                "q(1..2).\n"
                "p(X); not p(X) :- q(X).\n", {"p("})));
        REQUIRE(
            "([[],[p(1),r(1)],[r(1)]],[])" ==
            IO::to_string(solve(
                "q(1).\n"
                "p(X); not p(X); not r(X) :- q(X).\n"
                "r(X); not r(X) :- q(X).\n", {"p(", "r("})));
        REQUIRE(
            "([[a,aux,b,c,p,s_b1,s_b2,s_c2]],[])" ==
            IO::to_string(solve(
                "aux :- { a; b; c } > 2.\n"

                "p :- aux.\n"
                "p :- not s_c2.\n"

                "s_b1 :- a.\n"
                "s_b1 :- b.\n"
                "a; b :- s_b1.\n"

                "s_b2 :- a, b.\n"
                "a :- s_b2.\n"
                "b :- s_b2.\n"

                "s_c2 :- c, s_b1.\n"
                "s_c2 :- s_b2.\n"
                "c; s_b2 :- s_c2.\n"
                "s_b1; s_b2 :- s_c2.\n"

                "p; s_c2 :- not not aux.\n"

                "a :- p.\n"
                "b :- p.\n"
                "c :- p.\n"

                "p :- a.\n"
                "p :- b.\n"
                "p :- c.\n")));
        REQUIRE(
            "([[p(1,a),p(2,a)],[p(1,b),p(2,b)]],[])" ==
            IO::to_string(solve(
                "d(a;b).\n"
                "p(1,A;2,A) : d(A).\n", {"p("})));
        REQUIRE(
            "([[],[p(1),p(2)]],[])" ==
            IO::to_string(solve(
                "r(1).\n"
                "q(1,2).\n"
                "p(1..Y) : q(X,Y) | not p(1..Y) : q(X,Y) :- r(X).\n", {"p("})));
    }

    SECTION("show") {
        REQUIRE(
            "(["
            "[(1,2,3),-q(1),42],"
            "[(1,2,3),-q(1),42],"
            "[(1,2,3),-q(1),42,p(1)],"
            "[(1,2,3),42],"
            "[(1,2,3),42],"
            "[(1,2,3),42,boo(1)],"
            "[(1,2,3),42,boo(1)],"
            "[(1,2,3),42,boo(1),p(1)],"
            "[(1,2,3),42,p(1)]"
            "],[])" == IO::to_string(solve(
                "#show p/1.\n"
                "#show -q/1.\n"
                "#show boo(X):q(X).\n"
                "#show -p/-1.\n"
                "#show (1,2,3).\n"
                "\n"
                "{p(1); q(1); -p(1); -q(1)}.\n"
                "\n"
                "#const p=42.\n")));
    }

    SECTION("aggregates") {
        REQUIRE("([[]],[])" == IO::to_string(solve("#sum { 1:b; 2:c } < 1.\n")));
        REQUIRE("([[p(1),p(2)],[p(1),p(3)],[p(2),p(3)]],[])" == IO::to_string(solve("{p(1..3)}.\n:-{p(X)}!=2.")));
        REQUIRE("([[],[a,b],[b]],[])" == IO::to_string(solve("#sum { -1:a; 1:b } >= 0.")));
        REQUIRE("([[],[a,b],[b]],[])" == IO::to_string(solve("#sum { 1:a; 2:b } != 1.")));
        REQUIRE("([],[])" == IO::to_string(solve("a. {a} 0.")));
    }

    SECTION("aggregates2") {
        REQUIRE(
            "([[c]],[])" == IO::to_string(solve(
                "a :- not { c } >= 1, not c."
                "b :- a, #false."
                "c :- not b, {b; not b} >= 1."
                )));
        REQUIRE(
            "([[c]],[])" == IO::to_string(solve(
                "a :- not not { c } >= 1, not c."
                "b :- a, #false."
                "c :- not b, {b; not b} >= 1."
                )));
    }

    SECTION("invert") {
        std::string prg =
            "p(-1;2).\n"
            "q(X) :- p((-(X+3))+2).\n"
            "a(a;-b).\n"
            "b(X) :- a(---X).\n"
            "c(X) :- a(----X).\n";
        REQUIRE("([[a(-b),a(a),b(-a),b(b),c(-b),c(a),p(-1),p(2),q(-3),q(0)]],[])" == IO::to_string(solve(prg)));
    }

    SECTION("minimize") {
        std::string prg;
        prg =
            "{a; b; c; d}.\n"
            "#minimize {1,a:a; 1,b:b; 1,c:c; 1,d:d}.\n"
            "ok :- a,    b,not c,not d.\n"
            "ok :- not a,b,    c,    d.\n"
            "ok :- a,not b,    c,    d.\n"
            ":- not ok.\n";
        REQUIRE("([[a,b]],[])" == IO::to_string(solve(prg, {"a", "b", "c", "d"}, {2})));
        REQUIRE("([],[])" == IO::to_string(solve(prg, {"a", "b", "c", "d"}, {1})));
        prg =
            "{a; b; c; d}.\n"
            ":~ a. [1,a]\n"
            ":~ b. [1,b]\n"
            ":~ c. [1,c]\n"
            ":~ d. [1,d]\n"
            "ok :- a,    b,not c,not d.\n"
            "ok :- not a,b,    c,    d.\n"
            "ok :- a,not b,    c,    d.\n"
            ":- not ok.\n";
        REQUIRE("([[a,b]],[])" == IO::to_string(solve(prg, {"a", "b", "c", "d"}, {2})));
        REQUIRE("([],[])" == IO::to_string(solve(prg, {"a", "b", "c", "d"}, {1})));
        prg =
            "{a; b; c; d}.\n"
            "#maximize {-1,a:a; -1,b:b; -1,c:c; -1,d:d}.\n"
            "ok :- a,    b,not c,not d.\n"
            "ok :- not a,b,    c,    d.\n"
            "ok :- a,not b,    c,    d.\n"
            ":- not ok.\n";
        REQUIRE("([[a,b]],[])" == IO::to_string(solve(prg, {"a", "b", "c", "d"}, {2})));
        REQUIRE("([],[])" == IO::to_string(solve(prg, {"a", "b", "c", "d"}, {1})));
        prg =
            "{a; b; c; d}.\n"
            "#minimize {3,a:a; 3,b:b; 1,x:c; 1,x:d}.\n"
            "ok :- a,    b,not c,not d.\n"
            "ok :- not a,b,    c,    d.\n"
            "ok :- a,not b,    c,    d.\n"
            ":- not ok.\n"
            ;
        REQUIRE("([[a,c,d],[b,c,d]],[])" == IO::to_string(solve(prg, {"a", "b", "c", "d"}, {4})));
        REQUIRE("([],[])" == IO::to_string(solve(prg, {"a", "b", "c", "d"}, {3})));
        prg =
            "{a; b; c; d}.\n"
            "#minimize {3@2,a:a; 3@2,b:b; 1@2,x:c; 1@2,x:d}.\n"
            "#minimize {1@1,x:a; 1@1,x:c; 1@1,y:b}.\n"
            "ok :- a,    b,not c,not d.\n"
            "ok :- not a,b,    c,    d.\n"
            "ok :- a,not b,    c,    d.\n"
            ":- not ok.\n"
            ;
        REQUIRE("([[a,c,d]],[])" == IO::to_string(solve(prg, {"a", "b", "c", "d"}, {4, 1})));
        REQUIRE("([],[])" == IO::to_string(solve(prg, {"a", "b", "c", "d"}, {4, 0})));
        REQUIRE("([[]],[])" == IO::to_string(solve("{p}. #maximize{1:not p}.", {"p"}, {-1})));
    }

    SECTION("queens") {
        REQUIRE(
            "([[q(1,2),q(2,4),q(3,6),q(4,1),q(5,3),q(6,5)],"
            "[q(1,3),q(2,6),q(3,2),q(4,5),q(5,1),q(6,4)],"
            "[q(1,4),q(2,1),q(3,5),q(4,2),q(5,6),q(6,3)],"
            "[q(1,5),q(2,3),q(3,1),q(4,6),q(5,4),q(6,2)]],[])" ==
            IO::to_string(solve(
                "#const n = 6.\n"
                "n(1..n).\n"
                "\n"
                "q(X,Y) :- n(X), n(Y), not not q(X,Y).\n"
                "\n"
                "        c(r,X; c,Y) :- q(X,Y).\n"
                "not not c(r,N; c,N) :- n(N).\n"
                "\n"
                "n(r,X,Y-1,X,Y; c,X-1,Y,X,Y; d1,X-1,Y-1,X,Y;     d2,X-1,Y+1,X,Y      ) :- n(X), n(Y).\n"
                "c(r,N,0;       c,0,N;       d1,N-1,0; d1,0,N-1; d2,N-1,n+1; d2,0,N+1) :- n(N).\n"
                "\n"
                "c(C,XX,YY) :-     c(C,X,Y), n(C,X,Y,XX,YY), not q(XX,YY).\n"
                "           :- not c(C,X,Y), n(C,X,Y,XX,YY),     q(XX,YY).\n", {"q("})));
    }

    SECTION("undefinedRule") {
        REQUIRE(
            "([[q(1)]],[-:3:3-4: info: operation undefined:\n  (A+0)\n])" ==
            IO::to_string(solve(
                "a(a).\n"
                "a(1).\n"
                "q(A+0) :- a(A).\n", {"q("})));
        REQUIRE(
            "([[q(2)]],[-:4:3-6: info: operation undefined:\n  (A+B)\n])" ==
            IO::to_string(solve(
                "a(a).\n"
                "a(1).\n"
                "b(1).\n"
                "q(A+B) :- a(A), b(B).\n", {"q("})));
        REQUIRE(
            "([[q(1,1,2)]],[-:4:27-30: info: operation undefined:\n  (A+B)\n])" ==
            IO::to_string(solve(
                "a(a).\n"
                "a(1).\n"
                "b(1).\n"
                "q(A,B,X) :- a(A), b(B), X=A+B.\n", {"q("})));
        REQUIRE(
            "([[q(1,1)]],[-:4:29-32: info: operation undefined:\n  (A+B)\n])" ==
            IO::to_string(solve(
                "a(a).\n"
                "a(1).\n"
                "b(1).\n"
                "q(A,B) :- a(A), b(B), not b(A+B).\n", {"q("})));
        REQUIRE(
            "([[q(1)]],[-:4:3-7: info: interval undefined:\n  A..B\n])" ==
            IO::to_string(solve(
                "a(a).\n"
                "a(1).\n"
                "b(1).\n"
                "q(A..B) :- a(A), b(B).\n", {"q("})));
    }

    SECTION("undefinedBodyAggregate") {
        REQUIRE(
            "([[a(1),a(a),h]],[-:3:15-16: info: operation undefined:\n  (X+1)\n])" ==
            IO::to_string(solve(
                "a(a).\n"
                "a(1).\n"
                "h :- #count { X+1 : a(X) } < 2.\n")));
        REQUIRE(
            "([[a(1),a(a),h]],[-:3:14-15: info: operation undefined:\n  (X+1)\n])" ==
            IO::to_string(solve(
                "a(a).\n"
                "a(1).\n"
                "h :- { not a(X+1) : a(X) } < 2.\n")));
        REQUIRE(
            "([[a(1),a(a),g(1)]],[-:4:30-31: info: operation undefined:\n  (X+1)\n,-:3:30-31: info: operation undefined:\n  (X+1)\n])" ==
            IO::to_string(solve(
                "a(a).\n"
                "a(1).\n"
                "g(X) :- not X < #count { } < X+1, a(X).\n"
                "h(X) :-     X < #count { } < X+1, a(X).\n")));
    }

    SECTION("undefinedAssignmentAggregate") {
        REQUIRE(
            "([[a(1),a(a),h(1)]],[-:3:22-23: info: operation undefined:\n  (X+1)\n])" ==
            IO::to_string(solve(
                "a(a).\n"
                "a(1).\n"
                "h(C) :- C = #count { X+1 : a(X) }.\n")));
        REQUIRE(
            "([[a(1),a(a),h(1)]],[-:3:21-22: info: operation undefined:\n  (X+1)\n])" ==
            IO::to_string(solve(
                "a(a).\n"
                "a(1).\n"
                "h(C) :- C = { not a(X+1) : a(X) }.\n")));
        REQUIRE(
            "([[]],[])" ==
            IO::to_string(solve(
                "h(C) :- C+1 = #min { a }.\n")));
    }

    SECTION("undefinedHeadAggregate") {
        REQUIRE(
            "([[a(1),a(a)],[a(1),a(a),p(1)]],[-:3:10-11: info: operation undefined:\n  (X+1)\n])" ==
            IO::to_string(solve(
                "a(a).\n"
                "a(1).\n"
                "#count { X+1 : p(X) : a(X) }.\n")));
        REQUIRE(
            "([[a(1),a(a)],[a(1),a(a),p(2)]],[-:3:16-17: info: operation undefined:\n  (X+1)\n])" ==
            IO::to_string(solve(
                "a(a).\n"
                "a(1).\n"
                "#count { X : p(X+1) : a(X) }.\n")));
        REQUIRE(
            "([[a(1),a(a)],[a(1),a(a),p(2)]],[-:3:5-6: info: operation undefined:\n  (X+1)\n])" ==
            IO::to_string(solve(
                "a(a).\n"
                "a(1).\n"
                "{ p(X+1) : a(X) }.\n")));
        REQUIRE(
            "([[p(1)]],[-:3:17-18: info: operation undefined:\n  (X+1)\n])" ==
            IO::to_string(solve(
                "a(a).\n"
                "a(1).\n"
                "X <= { p(X) } < X+1 :- a(X).\n", { "p(" })));
        REQUIRE(
            "([[p(1)]],[-:3:1-2: info: operation undefined:\n  (X+-1)\n])" ==
            IO::to_string(solve(
                "a(a).\n"
                "a(1).\n"
                "X-1 < { p(X) } <= X :- a(X).\n", {"p("})));
    }

    SECTION("undefinedConjunction") {
        REQUIRE(
            "([[a(a)],[h]],[-:4:10-11: info: operation undefined:\n  (A+1)\n])" ==
            IO::to_string(solve(
                "{a(a)}.\n"
                "a(1..2).\n"
                "p(2..3).\n"
                "h :- p(1+A):a(A).\n", {"h", "a(a)"})));
        REQUIRE(
            "([[a(a)],[a(a),p(2),p(3)],[h],[p(2),p(3)]],[-:4:14-15: info: operation undefined:\n  (A+1)\n])" ==
            IO::to_string(solve(
                "{a(a)}.\n"
                "a(1..2).\n"
                "{p(2..3)} != 1.\n"
                "h :- not p(1+A):a(A).\n", {"h", "a(a)", "p("})));
        REQUIRE(
            "([[a(a),h],[a(a),p(2),p(3)],[h],[p(2),p(3)]],[-:4:24-25: info: operation undefined:\n  (A+1)\n])" ==
            IO::to_string(solve(
                "{a(a)}.\n"
                "a(1..2).\n"
                "{p(2..3)} != 1.\n"
                "h :- not p(X):a(A),X=1+A.\n", {"h", "a(a)", "p("})));
    }

    SECTION("undefinedDisjunction") {
        REQUIRE(
            "([[]],[-:3:5-6: info: operation undefined:\n  (A+1)\n])" ==
            IO::to_string(solve(
                "a(a).\n"
                "a(1..2).\n"
                "p(1+A):a(A).\n", {"p("})));
        REQUIRE(
            "([[p(2)],[p(3)]],[-:3:15-16: info: operation undefined:\n  (A+1)\n])" ==
            IO::to_string(solve(
                "a(a).\n"
                "a(1..2).\n"
                "p(X):a(A),X=1+A.\n", {"p("})));
    }

    SECTION("undefinedScript") {
        REQUIRE_THROWS_AS(IO::to_string(solve("a(@failure()).\n")), std::runtime_error);
    }

    SECTION("minMax") {
        REQUIRE(
            "([[a],[b]],[])" ==
            IO::to_string(solve(
                "a :- #min { 1:a; 2:b } != 2.\n"
                "b :- #max { 1:a; 2:b } != 1.\n")));
        REQUIRE(
            "([[a,b]],[])" ==
            IO::to_string(solve(
                "a :- b.\n"
                "b :- a.\n"
                "a :- #min { 1:a; 2:b } != 2.\n"
                "b :- #max { 1:a; 2:b } != 1.\n")));
        REQUIRE(
            "([[a,b,c,w,z],[a,b,w,z],[a,c,w,z],[a,w,z],[b,c,x,y,z],[b,x,y,z],[c,w,y],[w,y]],[])" ==
            IO::to_string(solve(
                "{a;b;c}.\n"
                "w :- #min { 1:a;2:b;3:c } != 2.\n"
                "x :- #min { 1:a;2:b;3:c } = 2.\n"
                "y :- #min { 1:a;2:b;3:c } >= 2.\n"
                "z :- #min { 1:a;2:b;3:c } <= 2.\n")));
    }

    SECTION("nonmon") {
        REQUIRE(
            "([[true(e,x1,1)]],[])" ==
            IO::to_string(solve(
                "{ true(e,L,C) } :- var(e,L,C).\n"
                "true(a,L,C) :- unequal, var(a,L,C).\n"
                "unequal :- int(V), #sum{ C,Q,L : true(Q,L,C) } != V.\n"
                ":- not unequal.\n"

                "var(e,x1,1).\n"
                "var(e,x2,2).\n"
                "var(a,y1,2).\n"
                "var(a,y2,3).\n"
                "int(5).\n", {"true(e"})));
        REQUIRE(
            "([[true(e,x1,1)]],[])" ==
            IO::to_string(solve(
                "{ true(e,L,C) } :- var(e,L,C).\n"
                "true(a,L,C) :- unequal, var(a,L,C).\n"
                "unequal :- int(V), #sum{ C,Q,L : true(Q,L,C) } != V.\n"
                ":- not unequal.\n"

                "var(e,x1,1).\n"
                "var(e,x2,2).\n"
                "var(a,y1,2).\n"
                "var(a,y2,3).\n"
                "int(5).\n", {"true(e"})));

        REQUIRE(
            "([],[])" ==
            IO::to_string(solve(
                "{ true(e,L,C) } :- var(e,L,C).\n"
                "true(a,L,C) :- unequal, var(a,L,C).\n"
                "unequal :- int(V), #sum{ C,Q,L : true(Q,L,C) } != V.\n"
                ":- not unequal.\n"

                "var(e,x1,2).\n"
                "var(e,x2,4).\n"
                "var(a,y1,-2).\n"
                "var(a,y2,4).\n"
                "int(4).\n", {"true(e"})));

        REQUIRE(
            "([[],[true(e,x1,6),true(e,x2,3)]],[])" ==
            IO::to_string(solve(
                "{ true(e,L,C) } :- var(e,L,C).\n"
                "true(a,L,C) :- unequal, var(a,L,C).\n"
                "unequal :- int(V), #sum{ C,Q,L : true(Q,L,C) } != V.\n"
                ":- not unequal.\n"

                "var(e,x1,6).\n"
                "var(e,x2,3).\n"
                "var(a,y1,-2).\n"
                "var(a,y2,1).\n"
                "int(4).\n", {"true(e"})));
    }

    SECTION("edge") {
        REQUIRE(
            "([[path(a,b),path(b,c),path(c,d),path(d,a)]],[])" ==
            IO::to_string(solve(
                "node(a;b;c;d).\n"
                "edge(a,b;b,(c;d);c,(a;d);d,a).\n"
                "1 { path(X,Y) : edge(X,Y) } 1 :- node(X).\n"
                "1 { path(X,Y) : edge(X,Y) } 1 :- node(Y).\n"
                "start(X) :- X = #min { Y : node(Y) }.\n"
                "#edge (X,Y) : path(X,Y), not start(Y).\n", {"path("})));
    }

    SECTION("bugRewriteCond") {
        REQUIRE(
            "([[p(1)],[p(1),p(2)],[p(2)]],[])" ==
            IO::to_string(solve(
                "{p(1..2)}.\n"
                ":- #false:p(X).\n", {"p("})));
    }
    SECTION("bugLargeCond") {
        REQUIRE(
            "([[],[a(a)],[a(b)]],[])" ==
            IO::to_string(solve(
                "{ a(a;b) }.\n"
                "b(X) :- a(X).\n"
                "c(X) :- a(X).\n"
                ":- #count {A,B: a(A),  b(A),  c(A), a(B), b(B), c(B) } >= 2.\n", {"a("})));
    }
    SECTION("bugEmptyAggr") {
        REQUIRE(
            "([[a(1),a(3),a(4),a(6)]],[])" ==
            IO::to_string(solve(
                "a(1) :- #count { }.\n"
                "a(2) :- not #count { }.\n"
                "a(3) :- not not #count { }.\n"
                "a(4) :- { }.\n"
                "a(5) :- not { }.\n"
                "a(6) :- not not { }.\n"
                , {"a("})));
    }
    SECTION("IESolver") {
        // clean
        REQUIRE(
            "([[p(1),p(a),q]],[])" ==
            IO::to_string(solve(
                "p(1).\n"
                "p(a).\n"
                "q :- p(X), -0x7FFFFFFF-1 <= X <= 0x7FFFFFFF.\n"
                , {"p", "q"})));
        // relies on two's complemenet
        REQUIRE(
            "([[p(1),p(a),q]],[])" ==
            IO::to_string(solve(
                "p(1).\n"
                "p(a).\n"
                "q :- p(X), 0x80000000 <= X <= 0x7FFFFFFF.\n"
                , {"p", "q"})));
    }

}

} } } // namespace Test Output Gringo
