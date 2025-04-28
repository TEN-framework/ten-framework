"""
Tests for the ast module.
"""
from unittest import TestCase
from typing import cast
from textwrap import dedent
from collections.abc import Sequence
from copy import copy, deepcopy

from .. import ast
from ..ast import (
    AST,
    ASTSequence,
    Id,
    Location,
    Program,
    ProgramBuilder,
    Position,
    StrSequence,
    SymbolicTerm,
    TheoryUnparsedTermElement,
    parse_string,
)
from ..symbol import Function, Number
from ..control import Control


class VariableRenamer(ast.Transformer):
    """
    Add an underscore to all names of variables in an ast.
    """

    def visit_Variable(self, node):  # pylint: disable=no-self-use,invalid-name
        """
        Rename a variable.
        """
        return ast.Variable(node.location, "_" + node.name)


class TestAST(TestCase):
    """
    Tests for the ast module.
    """

    def _deepcopy(self, node: AST) -> AST:
        """
        This functions tests manual deep copying of ast nodes using all the
        constructor functions in the ast module.

        Since this functions visits all possible AST nodes, further
        functionality of the AST is tested here.
        """
        cons_name = str(node.ast_type).split(".")[1]
        cons = getattr(ast, cons_name)
        args = cast(dict, dict(node.items()))

        def to_list(val):
            return list(val) if isinstance(val, Sequence) else val

        items = [to_list(val) for key, val in node.items()]
        zipped = [to_list(val) for key, val in zip(node.keys(), node.values())]
        self.assertEqual(items, zipped)

        for key in node.child_keys:
            if isinstance(args[key], Sequence):
                args[key] = [self._deepcopy(cast(AST, child)) for child in args[key]]
            elif isinstance(args[key], AST):
                args[key] = self._deepcopy(cast(AST, args[key]))

        cpy = cons(**args)
        self.assertEqual(cpy, node)

        for key in node.keys():
            setattr(cpy, key, getattr(cpy, key))

        cpz = copy(cpy)
        for key in node.keys():
            setattr(cpy, key, getattr(cpz, key))

        self.assertEqual(cpy, node)

        return cpy

    def _str(self, s, alt=None):
        prg = []
        parse_string(s, prg.append)
        cpy = copy(deepcopy(self._deepcopy(prg[-1])))
        self.assertEqual(str(cpy), s if alt is None else alt)

        prg = []
        parse_string(str(cpy), prg.append)
        self.assertEqual(str(prg[-1]), s if alt is None else alt)

        try:
            ctl = Control()
            with ProgramBuilder(ctl) as bld:
                bld.add(prg[-1])
        except RuntimeError as e:
            msg = e.args[0]
            if (
                "error: python support not available" not in msg
                and "error: lua support not available" not in msg
            ):
                raise RuntimeError from e

    def test_terms(self):
        """
        Test terms.
        """
        self._str("a.")
        self._str("-a.")
        self._str("a(X).")
        self._str("a(-X).")
        self._str("a(|X|).")
        self._str("a(~X).")
        self._str("a((X^Y)).")
        self._str("a((X?Y)).")
        self._str("a((X&Y)).")
        self._str("a((X+Y)).")
        self._str("a((X-Y)).")
        self._str("a((X*Y)).")
        self._str("a((X/Y)).")
        self._str("a((X\\Y)).")
        self._str("a((X**Y)).")
        self._str("a((X..Y)).")
        self._str("-a(f).")
        self._str("-a(-f).")
        self._str("-a(f(X)).")
        self._str("-a(f(X,Y)).")
        self._str("-a(()).")
        self._str("-a((a,)).")
        self._str("-a((a,b)).")
        self._str("-a(@f(a,b)).")
        self._str("-a(@f).")
        self._str("-a(a;b;c).")
        self._str("-a((a;b;c)).")
        self._str("-a(f(a);f(b);f(c)).")

    def test_theory_terms(self):
        """
        Test theory terms.
        """
        self._str("&a { 1 }.")
        self._str("&a { (- 1) }.")
        self._str("&a { X }.")
        self._str("&a { () }.")
        self._str("&a { (1,) }.")
        self._str("&a { (1,2) }.")
        self._str("&a { [] }.")
        self._str("&a { [1] }.")
        self._str("&a { [1,2] }.")
        self._str("&a { {} }.")
        self._str("&a { {1} }.")
        self._str("&a { {1,2} }.")
        self._str("&a { f }.")
        self._str("&a { f(X) }.")
        self._str("&a { f(X,Y) }.")
        self._str("&a { (+ a + - * b + c) }.")

    def test_literals(self):
        """
        Test literals.
        """
        self._str("a.")
        self._str("not a.")
        self._str("not not a.")
        self._str("1 < 2.")
        self._str("1 <= 2.")
        self._str("1 > 2.")
        self._str("1 >= 2.")
        self._str("1 = 2.")
        self._str("not 1 = 2.")
        self._str("not not 1 = 2.")
        self._str("1 != 2.")
        self._str("#false.")
        self._str("#true.")

    def test_head_literals(self):
        """
        Test head literals.
        """
        self._str("{ }.")
        self._str("{ } < 2.", "2 > { }.")
        self._str("1 < { }.")
        self._str("1 < { } < 2.")
        self._str("{ b }.")
        self._str("{ a; b }.")
        self._str("{ a; b: c, d }.")
        self._str("#count { }.")
        self._str("#count { } < 2.", "2 > #count { }.")
        self._str("1 < #count { }.")
        self._str("1 < #count { } < 2.")
        self._str("#count { b: a }.")
        self._str("#count { b,c: a }.")
        self._str("#count { a: a; b: c }.")
        self._str("#count { a: d; b: x: c, d }.")
        self._str("#min { }.")
        self._str("#max { }.")
        self._str("#sum { }.")
        self._str("#sum+ { }.")
        self._str("a; b.")
        self._str("a; b: c.")
        self._str("a; b: c, d.")
        self._str("&a { }.")
        self._str("&a { 1 }.")
        self._str("&a { 1; 2 }.")
        self._str("&a { 1,2 }.")
        self._str("&a { 1,2: a }.")
        self._str("&a { 1,2: a, b }.")
        self._str("&a { } != x.")
        self._str("&a(x) { }.")

    def test_body_literals(self):
        """
        Test body literals.
        """
        self._str("a :- { }.")
        self._str("a :- not { }.")
        self._str("a :- not not { }.")
        self._str("a :- { } < 2.", "a :- 2 > { }.")
        self._str("a :- 1 < { }.")
        self._str("a :- 1 < { } < 2.")
        self._str("a :- { b }.")
        self._str("a :- { a; b }.")
        self._str("a :- { a; b: c, d }.")
        self._str("a :- #count { }.")
        self._str("a :- not #count { }.")
        self._str("a :- not not #count { }.")
        self._str("a :- #count { } < 2.", "a :- 2 > #count { }.")
        self._str("a :- 1 < #count { }.")
        self._str("a :- 1 < #count { } < 2.")
        self._str("a :- #count { b }.")
        self._str("a :- #count { b,c }.")
        self._str("a :- #count { a; b }.")
        self._str("a :- #count { a; b: c, d }.")
        self._str("a :- #min { }.")
        self._str("a :- #max { }.")
        self._str("a :- #sum { }.")
        self._str("a :- #sum+ { }.")
        self._str("a :- a; b.")
        self._str("a :- a; b: c.")
        self._str("a :- a; b: c, d.")
        self._str("a :- &a { }.")
        self._str("a :- &a { 1 }.")
        self._str("a :- &a { 1; 2 }.")
        self._str("a :- &a { 1,2 }.")
        self._str("a :- &a { 1,2: a }.")
        self._str("a :- &a { 1,2: a, b }.")
        self._str("a :- &a { } != x.")
        self._str("a :- &a(x) { }.")
        self._str("a :- a.")
        self._str("a :- not a.")
        self._str("a :- not not a.")
        self._str("a :- 1 < 2.")
        self._str("a :- 1 <= 2.")
        self._str("a :- 1 > 2.")
        self._str("a :- 1 >= 2.")
        self._str("a :- 1 = 2.")
        self._str("a :- 1 != 2.")
        self._str("a :- #false.")
        self._str("a :- #true.")

    def test_statements(self):
        """
        Test statements.
        """
        self._str("a.")
        self._str("#false.")
        self._str("#false :- a.")
        self._str("a :- a; b.")
        self._str("#const x = 10.")
        self._str("#const x = 10. [override]")
        self._str("#show.")
        self._str("#show p/1.")
        self._str("#show -p/1.")
        self._str("#defined p/1.")
        self._str("#defined -p/1.")
        self._str("#show x.")
        self._str("#show x : y; z.")
        self._str(":~ . [1@0]")
        self._str(":~ b; c. [1@2,s,t]")
        self._str("#script (lua)\ncode\n#end.")
        self._str("#script (python)\ncode\n#end.")
        self._str("#program x(y, z).")
        self._str("#program x.")
        self._str("#external a. [X]")
        self._str("#external a : b; c. [false]")
        self._str("#edge (1,2).")
        self._str("#edge (1,2) : x; y.")
        self._str("#heuristic a. [b@p,m]")
        self._str("#heuristic a : b; c. [b@p,m]")
        self._str("#project a.")
        self._str("#project a : b; c.")
        self._str("#project -a/0.")
        self._str("#project a/0.")
        self._str("#theory x {\n}.")
        self._str(
            dedent(
                """\
                         #theory x {
                           t {
                             + : 0, unary;
                             - : 1, binary, left;
                             * : 2, binary, right
                           };
                           &a/0: t, head;
                           &b/0: t, body;
                           &c/0: t, directive;
                           &d/0: t, { }, t, any;
                           &e/0: t, { =, !=, + }, t, any
                         }."""
            )
        )
        self._str("%* test *%")
        self._str("%* test *%\n", "%* test *%")
        self._str("% test")
        self._str("% test\n", "% test")

    def test_compare(self):
        """
        Test comparison and hashing.
        """
        pos = Position("<string>", 1, 1)
        loc = Location(pos, pos)
        x = Id(location=loc, name="x")
        y = Id(location=Location(pos, Position("<string>", 1, 2)), name="x")
        z = Id(location=loc, name="z")
        self.assertEqual(x, y)
        self.assertEqual(x, x)
        self.assertNotEqual(x, z)
        self.assertEqual(hash(x), hash(x))
        self.assertEqual(hash(x), hash(y))
        self.assertNotEqual(hash(x), hash(z))
        self.assertLess(x, z)
        self.assertNotEqual(x, z)
        self.assertGreater(z, x)
        self.assertLessEqual(y, x)
        self.assertLessEqual(x, y)
        self.assertGreaterEqual(y, x)
        self.assertGreaterEqual(x, y)

    def test_compare_bug(self):
        """
        Test comparisons.
        """
        r1, r2 = [], []
        parse_string(":- b.", r1.append)
        parse_string(":- not a.", r2.append)
        self.assertTrue((r1[-1] < r2[-1]) != (r2[-1] < r1[-1]))

    def test_ast_sequence(self):
        """
        Test ast sequences.
        """
        pos = Position("<string>", 1, 1)
        loc = Location(pos, pos)
        lst = [Id(loc, "x"), Id(loc, "y"), Id(loc, "z")]
        prg = Program(loc, "p", lst)
        seq = prg.parameters
        self.assertIsInstance(seq, ASTSequence)
        self.assertEqual(len(seq), 3)
        self.assertEqual(list(seq), lst)
        self.assertEqual(seq[0], lst[0])
        seq.insert(0, Id(loc, "i"))
        self.assertEqual(list(seq), [Id(loc, "i")] + lst)
        seq.insert(0, seq[3])
        self.assertEqual(list(seq), [Id(loc, "z"), Id(loc, "i")] + lst)
        del seq[2]
        self.assertEqual(list(seq), [Id(loc, "z"), Id(loc, "i")] + lst[1:])

    def test_str_sequence(self):
        """
        Test ast sequences.
        """
        pos = Position("<string>", 1, 1)
        loc = Location(pos, pos)

        lst = ["x", "y", "z"]
        sym = SymbolicTerm(loc, Function("a", [Number(1)]))
        tue = TheoryUnparsedTermElement(lst, sym)
        seq = tue.operators
        self.assertIsInstance(seq, StrSequence)
        self.assertEqual(len(seq), 3)
        self.assertEqual(list(seq), lst)
        self.assertEqual(seq[0], lst[0])
        seq.insert(0, "i")
        self.assertEqual(list(seq), ["i"] + lst)
        seq.insert(0, seq[3])
        self.assertEqual(list(seq), ["z", "i"] + lst)
        del seq[2]
        self.assertEqual(list(seq), ["z", "i"] + lst[1:])

    def test_unpool(self):
        """
        Test unpooling.
        """
        prg = []
        parse_string("%comment\n:- a(1;2): a(3;4).", prg.append)
        com = prg[-2]
        lit = prg[-1].body[0]

        def unpool(other=True, condition=True):
            return [str(x) for x in lit.unpool(other, condition)]

        self.assertEqual([str(x) for x in com.unpool()], ["%comment"])
        self.assertEqual(
            unpool(), ["a(1): a(3)", "a(1): a(4)", "a(2): a(3)", "a(2): a(4)"]
        )
        self.assertEqual(unpool(other=False), ["a(1;2): a(3)", "a(1;2): a(4)"])
        self.assertEqual(unpool(condition=False), ["a(1): a(3;4)", "a(2): a(3;4)"])
        self.assertEqual(unpool(other=False, condition=False), ["a(1;2): a(3;4)"])

    def test_transformer(self):
        """
        Test the transformer class.
        """
        prg = []
        vrt = VariableRenamer()
        parse_string("p(X) :- q(X).", lambda stm: prg.append(str(vrt(stm))))
        self.assertEqual(prg[-1], "p(_X) :- q(_X).")

    def test_comment_order(self):
        """
        Test if comments are reported in proper order.
        """
        prg = dedent(
            """\
            % comment before `x=10`
            #const x=10.
            % comment after `x=10`
            a.
            % comment before `y=10`
            #const y=10. [override]
            % comment after `y=10`
            b.
            % comment before `#external a`
            #external a.
            % comment after `#external a`
            a.
            % comment before `#external b`
            #external b. [true]
            % comment after `#external b`
            """
        )

        expected = [
            "#program base.",
            "% comment before `x=10`",
            "#const x = 10.",
            "% comment after `x=10`",
            "a.",
            "% comment before `y=10`",
            "#const y = 10. [override]",
            "% comment after `y=10`",
            "b.",
            "% comment before `#external a`",
            "#external a. [false]",
            "% comment after `#external a`",
            "a.",
            "% comment before `#external b`",
            "#external b. [true]",
            "% comment after `#external b`",
        ]
        result = []
        parse_string(prg, lambda stm: result.append(str(stm)))
        self.assertEqual(result, expected)

    def test_repr(self):
        """
        Test the string representation of AST nodes.
        """
        # pylint: disable=eval-used
        stms = []
        prg = dedent(
            """\
            a :- &sum(body,second) { foo; (1 - 3) } > (4 * 17).
            &theory { (X * a ** stuff): dom(X); (1 - 3) }.
            &diff { (foo - bar) } <= 42.
            1 #max { X : a } :- a, X = #count { a }.
            """
        )
        parse_string(prg, stms.append)
        self.assertEqual(stms, eval(repr(stms)))
