//
// Copyright (c) 2016-2017 Benjamin Kaufmann
//
// This file is part of Potassco.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//

#include "catch.hpp"
#include "test_common.h"
#include <potassco/aspif_text.h>
#include <potassco/aspif.h>
#include <sstream>
namespace Potassco {
namespace Test {
namespace Text {

bool read(AspifTextInput& in, std::stringstream& str) {
	return in.accept(str) && in.parse();
}
TEST_CASE("Text reader ", "[text]") {
	std::stringstream input, output;
	AspifOutput out(output);
	AspifTextInput prg(&out);

	SECTION("read empty") {
		REQUIRE(read(prg, input));
		REQUIRE(output.str() == "asp 1 0 0\n0\n");
	}
	SECTION("read empty integrity constraint") {
		input << ":- .";
		REQUIRE(read(prg, input));
		REQUIRE(output.str().find("1 0 0 0 0") != std::string::npos);
	}
	SECTION("read fact") {
		input << "x1.";
		REQUIRE(read(prg, input));
		REQUIRE(output.str().find("1 0 1 1 0 0") != std::string::npos);
	}
	SECTION("read basic rule") {
		input << "x1 :- not   x2.";
		REQUIRE(read(prg, input));
		REQUIRE(output.str().find("1 0 1 1 0 1 -2") != std::string::npos);
	}
	SECTION("read choice rule") {
		input << "{x1} :- not x2.\n";
		input << "{x2, x3}.";
		REQUIRE(read(prg, input));
		REQUIRE(output.str().find("1 1 1 1 0 1 -2") != std::string::npos);
		REQUIRE(output.str().find("1 1 2 2 3 0 0") != std::string::npos);
	}
	SECTION("ready empty choice rule") {
		input << "{}.\n";
		REQUIRE(read(prg, input));
		REQUIRE(output.str().find("1 1 0 0 0") != std::string::npos);
	}
	SECTION("read disjunctive rule") {
		input << "x1 | x2 :- not x3.";
		input << "x1 ; x2 :- not x4.";
		REQUIRE(read(prg, input));
		REQUIRE(output.str().find("1 0 2 1 2 0 1 -3") != std::string::npos);
		REQUIRE(output.str().find("1 0 2 1 2 0 1 -4") != std::string::npos);
	}
	SECTION("read weight rule") {
		input << "x1 :- 2 {x2, x3=2, not x4 = 3, x5}.";
		REQUIRE(read(prg, input));
		REQUIRE(output.str().find("1 0 1 1 1 2 4 2 1 3 2 -4 3 5 1") != std::string::npos);
	}
	SECTION("read alternative atom names") {
		input << "a :- not b, x_3.";
		REQUIRE(read(prg, input));
		REQUIRE(output.str().find("1 0 1 1 0 2 -2 3") != std::string::npos);
	}
	SECTION("read integrity constraint") {
		input << ":- x1, not x2.";
		REQUIRE(read(prg, input));
		REQUIRE(output.str().find("1 0 0 0 2 1 -2") != std::string::npos);
	}
	SECTION("read minimize constraint") {
		input << "#minimize {x1, x2, x3}.\n";
		input << "#minimize {not x1=2, x4, not x5 = 3}@1.\n";
		REQUIRE(read(prg, input));
		REQUIRE(output.str().find("2 0 3 1 1 2 1 3 1") != std::string::npos);
		REQUIRE(output.str().find("2 1 3 -1 2 4 1 -5 3") != std::string::npos);
	}
	SECTION("read project") {
		input << "#project {a,x2}.";
		REQUIRE(read(prg, input));
		REQUIRE(output.str().find("3 2 1 2") != std::string::npos);
	}
	SECTION("read empty project") {
		input << "#project {}.";
		REQUIRE(read(prg, input));
		REQUIRE(output.str().find("3 0") != std::string::npos);
	}
	SECTION("read output") {
		input << "#output foo (1, \"x1, x2\") : x1, x2.";
		REQUIRE(read(prg, input));
		REQUIRE(output.str().find("4 15 foo(1,\"x1, x2\") 2 1 2") != std::string::npos);
	}
	SECTION("read external") {
		input << "#external x1.\n";
		REQUIRE(read(prg, input));
		REQUIRE(output.str().find("5 1 2") != std::string::npos);
	}
	SECTION("read external with value") {
		input << "#external x2. [true]\n";
		input << "#external x3. [false]\n";
		input << "#external x4. [free]\n";
		input << "#external x5. [release]\n";
		REQUIRE(read(prg, input));
		REQUIRE(output.str().find("5 2 1") != std::string::npos);
		REQUIRE(output.str().find("5 3 2") != std::string::npos);
		REQUIRE(output.str().find("5 4 0") != std::string::npos);
		REQUIRE(output.str().find("5 5 3") != std::string::npos);
	}
	SECTION("read external with unknown value") {
		input << "#external x2. [open]\n";
		REQUIRE_THROWS(read(prg, input));
	}
	SECTION("read assume") {
		input << "#assume {a, not x2}.";
		REQUIRE(read(prg, input));
		REQUIRE(output.str().find("6 2 1 -2") != std::string::npos);
	}
	SECTION("read empty assume") {
		input << "#assume {}.";
		REQUIRE(read(prg, input));
		REQUIRE(output.str().find("6 0") != std::string::npos);
	}
	SECTION("read heuristic") {
		input << "#heuristic x1. [1, level]";
		input << "#heuristic x2 : x1. [2@1, true]";
		input << "#heuristic x3 :. [1,level]";
		REQUIRE(read(prg, input));
		REQUIRE(output.str().find("7 0 1 1 0 0") != std::string::npos);
		REQUIRE(output.str().find("7 4 2 2 1 1 1") != std::string::npos);
		REQUIRE(output.str().find("7 0 3 1 0 0") != std::string::npos);
	}
	SECTION("read edge") {
		input << "#edge (1,2) : x1.";
		input << "#edge (2,1).";
		REQUIRE(read(prg, input));
		REQUIRE(output.str().find("8 1 2 1 1") != std::string::npos);
		REQUIRE(output.str().find("8 2 1 0") != std::string::npos);
	}
	SECTION("read incremental") {
		input << "#incremental.\n";
		input << "{x1}.\n";
		input << "#step.\n";
		input << "{x2}.\n";
		REQUIRE(read(prg, input));
		REQUIRE(prg.parse());
		REQUIRE(output.str() ==
			"asp 1 0 0 incremental\n"
			"1 1 1 1 0 0\n"
			"0\n"
			"1 1 1 2 0 0\n"
			"0\n");
	}
	SECTION("read error") {
		input << "#incremental.\n";
		input << "#foo.\n";
		try { read(prg, input); REQUIRE(false); }
		catch (const std::logic_error& e) {
			REQUIRE(std::strstr(e.what(), "parse error in line 2: ") != 0);
		}
	}
}
/////////////////////////////////////////////////////////////////////////////////////////
// AspifTextOutput
/////////////////////////////////////////////////////////////////////////////////////////
TEST_CASE("Text writer ", "[text]") {
	std::stringstream input, output;
	AspifTextOutput out(output);
	AspifTextInput  prg(&out);
	SECTION("empty program is empty") {
		REQUIRE(read(prg, input));
		REQUIRE(output.str() == "");
	}
	SECTION("simple fact") {
		input << "x1.";
		read(prg, input);
		REQUIRE(output.str() == "x_1.\n#show.\n");
	}
	SECTION("named fact") {
		input << "x1.\n#output foo : x1.";
		read(prg, input);
		REQUIRE(output.str() == "foo.\n");
	}
	SECTION("simple choice") {
		input << "{x1,x2}.\n#output foo : x1.";
		read(prg, input);
		REQUIRE(output.str() == "{foo;x_2}.\n#show foo/0.\n");
	}
	SECTION("empty choice") {
		input << "{}.";
		read(prg, input);
		REQUIRE(output.str() == "{}.\n");
	}
	SECTION("integrity constraint") {
		input << ":- x1,x2.\n#output foo : x1.";
		read(prg, input);
		REQUIRE(output.str() == ":- foo, x_2.\n#show foo/0.\n");
	}
	SECTION("empty integrity constraint") {
		input << ":-.";
		read(prg, input);
		REQUIRE(output.str() == ":- .\n");
	}
	SECTION("classical negation") {
		Atom_t head(1);
		Lit_t cond(1);
		out.beginStep();
		out.rule(Head_t::Choice, toSpan(&head, 1), {});
		out.output(toSpan("-a"), toSpan(&cond, 1));
		out.output(toSpan("-8"), toSpan(&cond, 1));
		out.endStep();
		REQUIRE(output.str() == "{-a}.\n#show -8 : -a.\n");
	}
	SECTION("classical negation tricky") {
		std::vector<Atom_t> head{1,2};
		Lit_t cond1(1);
		Lit_t cond2(2);
		out.beginStep();
		out.rule(Head_t::Choice, toSpan(head), {});
		out.output(toSpan("-a"), toSpan(&cond1, 1));
		out.output(toSpan("x_1"), toSpan(&cond2, 1));
		out.endStep();
		REQUIRE(output.str() == "{-a;x_2}.\n#show x_1 : x_2.\n#show -a/0.\n");
	}
	SECTION("basic rule") {
		input << "x1 :- x2, not x3, x4.\n#output foo : x1.\n#output bar : x3.";
		read(prg, input);
		REQUIRE(output.str() == "foo :- x_2, not bar, x_4.\n#show foo/0.\n#show bar/0.\n");
	}
	SECTION("choice rule") {
		input << "{x1,x2} :- not x3, x4.\n#output foo : x1.\n#output bar : x3.";
		read(prg, input);
		REQUIRE(output.str() == "{foo;x_2} :- not bar, x_4.\n#show foo/0.\n#show bar/0.\n");
	}
	SECTION("disjunctive rule") {
		input << "x1;x2 :- not x3, x4.\n#output foo : x1.\n#output bar : x3.";
		read(prg, input);
		REQUIRE(output.str() == "foo|x_2 :- not bar, x_4.\n#show foo/0.\n#show bar/0.\n");
	}
	SECTION("cardinality rule") {
		input << "x1;x2 :- 1{not x3, x4}.\n#output foo : x1.\n#output bar : x3.";
		read(prg, input);
		REQUIRE(output.str() == "foo|x_2 :- 1 #count{1 : not bar; 2 : x_4}.\n#show foo/0.\n#show bar/0.\n");
	}
	SECTION("sum rule") {
		input << "x1;x2 :- 3{not x3=2, x4, x5=1,x6=2}.\n#output foo : x1.\n#output bar : x3.";
		read(prg, input);
		REQUIRE(output.str() == "foo|x_2 :- 3 #sum{2,1 : not bar; 1,2 : x_4; 1,3 : x_5; 2,4 : x_6}.\n#show foo/0.\n#show bar/0.\n");
	}
	SECTION("convert sum rule to cardinality rule") {
		input << "x1;x2 :- 3{not x3=2, x4=2, x5=2,x6=2}.\n#output foo : x1.\n#output bar : x3.";
		read(prg, input);
		REQUIRE(output.str() == "foo|x_2 :- 2 #count{1 : not bar; 2 : x_4; 3 : x_5; 4 : x_6}.\n#show foo/0.\n#show bar/0.\n");
	}
	SECTION("convert sum rule with duplicate to cardinality rule") {
		input << "x2 :- 3{x3=1, x4=1, x5=1,x3=1}.\n";
		read(prg, input);
		REQUIRE(output.str() == "x_2 :- 3 #count{1 : x_3; 2 : x_4; 3 : x_5; 4 : x_3}.\n#show.\n");
	}
	SECTION("minimize rule") {
		input << "#minimize{x1,x2=2,x3}.\n#minimize{not x1=3,not x2,not x3}@1.";
		read(prg, input);
		REQUIRE(output.str() ==
		        "#minimize{1@0,1 : x_1; 2@0,2 : x_2; 1@0,3 : x_3}.\n#minimize{3@1,1 : not x_1; 1@1,2 : not "
		        "x_2; 1@1,3 : not x_3}.\n#show.\n");
	}
	SECTION("output statements") {
		input << "{x1;x2}.\n#output foo.\n#output bar : x1.\n#output \"Hello World\" : x2, not x1.";
		read(prg, input);
		REQUIRE(output.str() == "{bar;x_2}.\n#show foo.\n#show \"Hello World\" : x_2, not bar.\n"
								"#show bar/0.\n");
	}
	SECTION("duplicate output condition") {
		input << "{a}.\n#output x:a.\n#output y:a.\n";
		read(prg, input);
		REQUIRE(output.str() == "{x_1}.\n#show x : x_1.\n#show y : x_1.\n#show.\n");
	}
	SECTION("bogus duplicate output condition") {
		input << "{a}.\n#output x:a.\n#output x:a.\n";
		read(prg, input);
		REQUIRE(output.str() == "{x}.\n");
	}
	SECTION("duplicate output term") {
		input << "{x1;x2}.\n";
		SECTION("simple") {
			input << "#output a:x1.\n#output a:x2.\n";
			read(prg, input);
			REQUIRE(output.str() == "{x_1;x_2}.\n#show a : x_1.\n#show a : x_2.\n#show.\n");
		}
		SECTION("complex") {
			input << "#output a:x1.\n#output a:x1,x2.\n";
			read(prg, input);
			REQUIRE(output.str() == "{a;x_2}.\n#show a : a, x_2.\n#show a/0.\n");
		}
		SECTION("complex reversed") {
			input << "#output a:x1,x2.\n#output a:x1.\n";
			read(prg, input);
			REQUIRE(output.str() == "{a;x_2}.\n#show a : a, x_2.\n#show a/0.\n");
		}
		SECTION("duplicate condition first") {
			input << "#output a:x1.\n#output b:x1.\n#output a:x2.\n";
			read(prg, input);
			REQUIRE(output.str() == "{x_1;x_2}.\n#show a : x_1.\n#show b : x_1.\n#show a : x_2.\n#show.\n");
		}
		SECTION("duplicate condition all") {
			input << "#output a:x1.\n#output b:x1.\n#output a:x2.\n#output f:x2.\n";
			read(prg, input);
			REQUIRE(output.str() == "{x_1;x_2}.\n#show a : x_1.\n#show b : x_1.\n#show a : x_2.\n#show f : x_2.\n#show.\n");
		}
	}
	SECTION("implicit show") {
		input << "{a;b}. #output a:a.\n#output b:b.\n";
		read(prg, input);
		REQUIRE(output.str() == "{a;b}.\n");
	}
	SECTION("explicit show") {
		input << "{a;b}.\n";
		SECTION("empty") {
			read(prg, input);
			REQUIRE(output.str() == "{x_1;x_2}.\n#show.\n");
		}
		SECTION("one") {
			input << "#output a:a.\n";
			read(prg, input);
			REQUIRE(output.str() == "{a;x_2}.\n#show a/0.\n");
		}
		SECTION("duplicate one") {
			input << "#output a:b.\n#output b:b.\n";
			read(prg, input);
			REQUIRE(output.str() == "{x_1;x_2}.\n#show a : x_2.\n#show b : x_2.\n#show.\n");
		}
		SECTION("duplicate two") {
			input << "#output b:b.\n#output a:b.\n";
			read(prg, input);
			REQUIRE(output.str() == "{x_1;x_2}.\n#show b : x_2.\n#show a : x_2.\n#show.\n");
		}
		SECTION("duplicate three") {
			input << "#output a:a.\n#output b:b.\n#output c:b.\n";
			read(prg, input);
			REQUIRE(output.str() == "{a;x_2}.\n#show b : x_2.\n#show c : x_2.\n#show a/0.\n");
		}
	}
	SECTION("output reserved name") {
		input << "{a;b}.\n";
		SECTION("all") {
			SECTION("match") {
				input << "#output x_2:b.\n#output x_1:a.";
				read(prg, input);
				REQUIRE(output.str() == "{x_1;x_2}.\n#show x_2 : x_2.\n#show x_1 : x_1.\n#show.\n");
			}
			SECTION("mismatch") {
				input << "#output x_2:a.\n#output x_1:b.";
				read(prg, input);
				REQUIRE(output.str() == "{x_1;x_2}.\n#show x_2 : x_1.\n#show x_1 : x_2.\n#show.\n");
			}
		}
		SECTION("some") {
			SECTION("match") {
				input << "#output x_2:b.";
				read(prg, input);
				REQUIRE(output.str() == "{x_1;x_2}.\n#show x_2 : x_2.\n#show.\n");
			}
			SECTION("mismatch") {
				input << "#output x_2:a.";
				read(prg, input);
				REQUIRE(output.str() == "{x_1;x_2}.\n#show x_2 : x_1.\n#show.\n");
			}
		}
		SECTION("incremental handled as some") {
			out.initProgram(true);
			out.beginStep();
			std::vector<Atom_t> head;
			std::vector<Lit_t>  cond;
			out.rule(Head_t::Choice, toSpan(head = {1, 2}), {}); // {1;2}.
			out.output(toSpan("x_3"), toSpan(cond = {1}));       // #show x_3 : 1.
			out.output(toSpan("x_2"), toSpan(cond = {2}));       // #show x_2 : 2.
			out.endStep();
			REQUIRE(output.str() == "% #program base.\n"
			                        "{x_1;x_2}.\n"
			                        "#show x_3 : x_1.\n"
			                        "#show x_2 : x_2.\n"
			                        "#show.\n");
			output.str("");
			out.beginStep();
			out.rule(Head_t::Choice, toSpan(head = {3}), toSpan(cond = {1, 2})); // {3} :- 1, 2.
			out.endStep();
			REQUIRE(output.str() == "% #program step(1).\n"
			                        "{x_3} :- x_1, x_2.\n");
		}
	}
	SECTION("output duplicate reserved") {
		out.initProgram(false);
		out.beginStep();
		Atom_t a  = 1;
		auto   al = lit(a);
		out.rule(Head_t::Choice, {&a, 1}, {});     // {x_1}.
		out.output(toSpan("x_1"), toSpan(&al, 1)); // #show x_1 : x_1. NOTE: uses reserved name "x_1"
		out.output(toSpan("a"), toSpan(&al, 1));   // #show a : x_1.
		SECTION("unique alternative") {
			out.endStep();
			REQUIRE(output.str() == "{x_1}.\n#show x_1 : x_1.\n#show a : x_1.\n#show.\n");
		}
		SECTION("two alternatives") {
			out.output(toSpan("b"), toSpan(&al, 1)); // #show b : x_1.
			out.endStep();
			REQUIRE(output.str() == "{x_1}.\n#show x_1 : x_1.\n#show a : x_1.\n#show b : x_1.\n#show.\n");
		}
	}
	SECTION("output predicate") {
		std::vector<Atom_t> head;
		std::vector<Lit_t>  cond;
		out.initProgram(false);
		out.beginStep();
		out.rule(Head_t::Choice, toSpan(head = {1, 2, 3, 4, 5}), {});
		out.output(toSpan("a"), toSpan(cond = {1}));
		out.output(toSpan("a(1,2,3,4,5,6,7,8,9,10,11,12)"), toSpan(cond = {2}));
		out.output(toSpan("b(t(1,2,3))"), toSpan(cond = {3}));
		out.output(toSpan("b()"), toSpan(cond = {4}));
		out.endStep();
		REQUIRE(output.str() == "{a;a(1,2,3,4,5,6,7,8,9,10,11,12);b(t(1,2,3));b;x_5}.\n#show a/0.\n#show "
		                        "a/12.\n#show b/1.\n#show b/0.\n");
	}
	SECTION("output invalid predicate") {
		std::vector<Atom_t> head;
		std::vector<Lit_t>  cond;
		out.initProgram(false);
		out.beginStep();
		out.rule(Head_t::Choice, toSpan(head = {1}), {});
		SECTION("missing close") { REQUIRE_THROWS_AS(out.output(toSpan("a("), toSpan(cond = {1})), std::logic_error); }
		SECTION("missing arg") { REQUIRE_THROWS_AS(out.output(toSpan("a(1,"), toSpan(cond = {1})), std::logic_error); }
		SECTION("missing arg on close") { REQUIRE_THROWS_AS(out.output(toSpan("a(1,)"), toSpan(cond = {1})), std::logic_error); }
		SECTION("empty arg") { REQUIRE_THROWS_AS(out.output(toSpan("a(1,,2)"), toSpan(cond = {1})), std::logic_error); }
		SECTION("unmatched close") { REQUIRE_THROWS_AS(out.output(toSpan("a(x()"), toSpan(cond = {1})), std::logic_error); }
		SECTION("empty arg 2") { REQUIRE_THROWS_AS(out.output(toSpan("b(,)"), toSpan(cond = {1})), std::logic_error); }
	}
	SECTION("write external - ") {
		SECTION("default") {
			input << "#external x1.";
			read(prg, input);
			REQUIRE(output.str() == "#external x_1.\n#show.\n");
		}
		SECTION("false is default") {
			input << "#external x1. [false]";
			read(prg, input);
			REQUIRE(output.str() == "#external x_1.\n#show.\n");
		}
		SECTION("with value") {
			input << "#external x1. [true]";
			input << "#external x2. [free]";
			input << "#external x3. [release]";
			read(prg, input);
			REQUIRE(output.str() == "#external x_1. [true]\n#external x_2. [free]\n#external x_3. [release]\n#show.\n");
		}
	}
	SECTION("empty assumption directive") {
		input << "#assume{}.";
		read(prg, input);
		REQUIRE(output.str() == "#assume{}.\n");
	}
	SECTION("assumption directive") {
		input << "#assume{x1,not x2,x3}.";
		read(prg, input);
		REQUIRE(output.str() == "#assume{x_1, not x_2, x_3}.\n#show.\n");
	}
	SECTION("empty projection directive") {
		input << "#project{}.";
		read(prg, input);
		REQUIRE(output.str() == "#project{}.\n");
	}
	SECTION("projection directive") {
		input << "#project{x1,x2,x3}.";
		read(prg, input);
		REQUIRE(output.str() == "#project{x_1, x_2, x_3}.\n#show.\n");
	}
	SECTION("edge directive") {
		input << "#edge (0,1) : x1, not x2.";
		input << "#edge (1,0).";
		read(prg, input);
		REQUIRE(output.str() == "#edge(0,1) : x_1, not x_2.\n#edge(1,0).\n#show.\n");
	}

	SECTION("heuristic directive -") {
		SECTION("simple") {
			input << "#heuristic a. [1,true]";
			read(prg, input);
			REQUIRE(output.str() == "#heuristic x_1. [1, true]\n#show.\n");
		}
		SECTION("simple with priority") {
			input << "#heuristic a. [1@2,true]";
			read(prg, input);
			REQUIRE(output.str() == "#heuristic x_1. [1@2, true]\n#show.\n");
		}
		SECTION("with condition") {
			input << "#heuristic a : b, not c. [1@2,true]";
			read(prg, input);
			REQUIRE(output.str() == "#heuristic x_1 : x_2, not x_3. [1@2, true]\n#show.\n");
		}
	}
	SECTION("incremental program") {
		input << "#incremental.\n";
		input << "{x1;x2}.";
		input << "#external x3.";
		input << "#output a(1) : x1.";
		input << "#step.";
		input << "x3 :- x1.";
		input << "#step.";
		input << "x4 :- x2,not x3.";
		read(prg, input);
		prg.parse();
		prg.parse();
		REQUIRE(output.str() ==
			"% #program base.\n"
			"{a(1);x_2}.\n"
			"#external x_3.\n"
		        "#show a/1.\n"
			"% #program step(1).\n"
			"x_3 :- a(1).\n"
			"% #program step(2).\n"
			"x_4 :- x_2, not x_3.\n");
	}
	SECTION("incremental output") {
		input << "#incremental.\n";
		input << "{x1;x2}.";
		input << "#output f : x2.";
		input << "#step.";
		input << "#output foo : x1.";
		input << "x3 :- x1, x2.";
		input << "#output f : x3.";
		input << "#output nX2 : x2.";
		read(prg, input);
		prg.parse();
		REQUIRE(output.str() == "% #program base.\n"
		                        "{x_1;f}.\n"
		                        "#show f/0.\n"
		                        "% #program step(1).\n"
		                        "#show foo : x_1.\n"
		                        "x_3 :- x_1, f.\n"
		                        "#show f : x_3.\n"
		                        "#show nX2 : f.\n");
	}
}

TEST_CASE("Text writer writes theory", "[text]") {
	std::stringstream output;
	AspifTextOutput out(output);
	out.initProgram(false);
	out.beginStep();
	SECTION("write empty atom") {
		out.theoryAtom(0, 0, Potassco::toSpan<Id_t>());
		out.theoryTerm(0, Potassco::toSpan("t"));
		out.endStep();
		REQUIRE(output.str() == "&t{}.\n");
	}
	SECTION("write operators") {
		out.theoryTerm(0, Potassco::toSpan("t"));
		out.theoryTerm(1, Potassco::toSpan("x"));
		out.theoryTerm(2, Potassco::toSpan("y"));
		out.theoryTerm(3, Potassco::toSpan("^~\\?."));
		std::vector<Id_t> ids;
		out.theoryTerm(4, 3, Potassco::toSpan(ids ={1,2}));
		out.theoryElement(0, Potassco::toSpan(ids ={4}), Potassco::toSpan<Lit_t>());
		out.theoryAtom(0, 0, Potassco::toSpan(ids ={0}));
		out.endStep();
		REQUIRE(output.str() == "&t{x ^~\\?. y}.\n");
	}
	SECTION("write complex atom") {
		out.theoryTerm(1, 200);
		out.theoryTerm(3, 400);
		out.theoryTerm(6, 1);
		out.theoryTerm(11, 2);
		out.theoryTerm(0, Potassco::toSpan("diff"));
		out.theoryTerm(2, Potassco::toSpan("<="));
		out.theoryTerm(4, Potassco::toSpan("-"));
		out.theoryTerm(5, Potassco::toSpan("end"));
		out.theoryTerm(8, Potassco::toSpan("start"));
		std::vector<Id_t> ids;
		out.theoryTerm(7, 5, Potassco::toSpan(ids = {6}));
		out.theoryTerm(9, 8, Potassco::toSpan(ids = {6}));
		out.theoryTerm(10, 4, Potassco::toSpan(ids = {7, 9}));
		out.theoryTerm(12, 5, Potassco::toSpan(ids = {11}));
		out.theoryTerm(13, 8, Potassco::toSpan(ids = {11}));
		out.theoryTerm(14, 4, Potassco::toSpan(ids = {12, 13}));

		out.theoryElement(0, Potassco::toSpan(ids = {10}), Potassco::toSpan<Lit_t>());
		out.theoryElement(1, Potassco::toSpan(ids = {14}), Potassco::toSpan<Lit_t>());

		out.theoryAtom(0, 0, Potassco::toSpan(ids = {0}), 2, 1);
		out.theoryAtom(0, 0, Potassco::toSpan(ids = {1}), 2, 3);
		out.endStep();
		REQUIRE(output.str() ==
			"&diff{end(1) - start(1)} <= 200.\n"
			"&diff{end(2) - start(2)} <= 400.\n");
	}
	SECTION("Use theory atom in rule") {
		Atom_t head = 2;
		Lit_t  body = 1;
		out.rule(Head_t::Disjunctive, toSpan(&head, 1), toSpan(&body, 1));
		out.theoryTerm(0, Potassco::toSpan("atom"));
		out.theoryTerm(1, Potassco::toSpan("x"));
		out.theoryTerm(2, Potassco::toSpan("y"));
		std::vector<Id_t> ids;
		out.theoryElement(0, Potassco::toSpan(ids = {1, 2}), Potassco::toSpan<Lit_t>());
		out.theoryElement(1, Potassco::toSpan(ids = {2}), Potassco::toSpan<Lit_t>());
		out.theoryAtom(1, 0, Potassco::toSpan(ids = {0, 1}));
		out.endStep();
		REQUIRE(output.str() == "x_2 :- &atom{x, y; y}.\n#show.\n");
	}
	SECTION("Fail on duplicate theory atom") {
		out.theoryTerm(0, Potassco::toSpan("t"));
		out.theoryTerm(1, Potassco::toSpan("x"));
		out.theoryAtom(1, 0, {});
		out.theoryAtom(1, 1, {});
		REQUIRE_THROWS_AS(out.endStep(), std::logic_error);
	}
	SECTION("Theory element with condition") {
		std::vector<Atom_t> head;
		std::vector<Id_t> ids;
		std::vector<Lit_t> body;
		out.rule(Head_t::Choice, toSpan(head = {1, 2}), toSpan<Lit_t>());
		out.rule(Head_t::Disjunctive, toSpan(head = {4}), toSpan(body = {3}));
		out.theoryTerm(0, Potassco::toSpan("atom"));
		out.theoryTerm(1, Potassco::toSpan("elem"));
		out.theoryTerm(2, Potassco::toSpan("p"));
		out.theoryElement(0, Potassco::toSpan(ids = {1}), Potassco::toSpan(body = {1, -2}));
		out.theoryElement(1, Potassco::toSpan(ids = {2}), Potassco::toSpan(body = {1}));
		out.theoryAtom(3, 0, Potassco::toSpan(ids = {0, 1}));
		SECTION("default") {
			out.endStep();
			REQUIRE(output.str() == "{x_1;x_2}.\n"
			                        "x_4 :- &atom{elem : x_1, not x_2; p : x_1}.\n#show.\n");
		}
		SECTION("override output") {
			out.output(Potassco::toSpan("foo"), Potassco::toSpan((body = {3})));
			SECTION("once") {
				out.endStep();
				// Ensure that we don't use "foo" for x_3.
				REQUIRE(output.str() == "{x_1;x_2}.\n"
				                        "x_4 :- &atom{elem : x_1, not x_2; p : x_1}.\n"
				                        "#show foo : &atom{elem : x_1, not x_2; p : x_1}.\n#show.\n");
			}
			SECTION("twice") {
				out.output(Potassco::toSpan("bar"), Potassco::toSpan((body = {3})));
				out.endStep();
				REQUIRE(output.str() == "{x_1;x_2}.\n"
				                        "x_4 :- &atom{elem : x_1, not x_2; p : x_1}.\n"
				                        "#show foo : &atom{elem : x_1, not x_2; p : x_1}.\n"
				                        "#show bar : &atom{elem : x_1, not x_2; p : x_1}.\n#show.\n");
			}
		}
	}
	SECTION("write complex atom incrementally") {
		out.initProgram(true);
		out.beginStep();
		out.theoryTerm(1, 200);
		out.theoryTerm(6, 1);
		out.theoryTerm(11, 2);
		out.theoryTerm(0, Potassco::toSpan("diff"));
		out.theoryTerm(2, Potassco::toSpan("<="));
		out.theoryTerm(4, Potassco::toSpan("-"));
		out.theoryTerm(5, Potassco::toSpan("end"));
		out.theoryTerm(8, Potassco::toSpan("start"));
		std::vector<Id_t> ids;
		out.theoryTerm(7, 5, Potassco::toSpan(ids = {6}));
		out.theoryTerm(9, 8, Potassco::toSpan(ids = {6}));
		out.theoryTerm(10, 4, Potassco::toSpan(ids = {7, 9}));

		out.theoryElement(0, Potassco::toSpan(ids = {10}), Potassco::toSpan<Lit_t>());
		out.theoryAtom(0, 0, Potassco::toSpan(ids = {0}), 2, 1);
		out.endStep();
		REQUIRE(output.str() ==
			"% #program base.\n"
			"&diff{end(1) - start(1)} <= 200.\n");
		output.str("");
		out.beginStep();
		out.theoryTerm(1, 600);
		out.theoryTerm(12, 5, Potassco::toSpan(ids = {11}));
		out.theoryTerm(13, 8, Potassco::toSpan(ids = {11}));
		out.theoryTerm(14, 4, Potassco::toSpan(ids = {12, 13}));
		out.theoryElement(0, Potassco::toSpan(ids = {14}), Potassco::toSpan<Lit_t>());
		out.theoryAtom(0, 0, Potassco::toSpan(ids = {0}), 2, 1);
		out.endStep();
		REQUIRE(output.str() == "% #program step(1).\n&diff{end(2) - start(2)} <= 600.\n");
	}
	SECTION("invalid increment") {
		out.initProgram(true);
		out.beginStep();
		out.theoryTerm(0, Potassco::toSpan("t"));
		std::vector<Atom_t> head;
		std::vector<Lit_t>  body;
		out.rule(Head_t::Choice, Potassco::toSpan((head = {1, 2})), {});
		out.output(Potassco::toSpan("a"), Potassco::toSpan((body = {1})));
		out.output(Potassco::toSpan("b"), Potassco::toSpan((body = {2})));
		out.endStep();
		REQUIRE(output.str() == "% #program base.\n"
		                        "{a;b}.\n");

		output.str("");
		out.beginStep();
		out.theoryAtom(2, 0, {});
		out.rule(Head_t::Choice, Potassco::toSpan((head = {4})), Potassco::toSpan((body = {2})));
		REQUIRE_THROWS_AS(out.endStep(), std::logic_error);
	}
}
}}}
