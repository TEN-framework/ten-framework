"""
Tests for aspif parsing.
"""
from unittest import TestCase
from textwrap import dedent

from ..control import Control
from .util import solve


def solve_aspif(prg):
    ctl = Control()
    ctl.add(dedent(prg))
    return solve(ctl)


def theory(prg):
    ctl = Control()
    ctl.add(dedent(prg))
    tab = {}
    for atm in ctl.symbolic_atoms:
        tab[atm.literal] = str(atm.symbol)

    thy = []
    for x in ctl.theory_atoms:
        elems = []
        for elem in x.elements:
            terms = ", ".join(str(term) for term in elem.terms)
            cond = ", ".join(sorted(tab[lit] for lit in elem.condition))
            elems.append(f"{terms} : {cond}")
        thy.append(f"&{x.term} {{ {'; '.join(sorted(elems))} }}")
        if x.guard is not None:
            thy[-1] += f" {x.guard[0]} {x.guard[1]}"
    return sorted(thy)


class TestASPIF(TestCase):
    """
    Test ASPIF parsing.
    """

    def test_preamble(self):
        """
        Test preamble parsing.
        """
        ctl = Control()
        ctl.add(
            dedent(
                """\
            asp 1 0 0
            0
            """
            )
        )
        ctl = Control([], lambda msg, code: None)
        self.assertRaises(
            RuntimeError,
            ctl.add,
            dedent(
                """\
            asp 1 0 0 unknown
            0
            """
            ),
        )
        ctl = Control([], lambda msg, code: None)
        ctl.add(
            dedent(
                """\
            asp 1 0 0 incremental
            0
            """
            )
        )
        self.assertRaises(
            RuntimeError,
            ctl.add,
            dedent(
                """\
            asp 1 0 0 incremental
            0
            """
            ),
        )
        ctl = Control([], lambda msg, code: None)
        self.assertRaises(
            RuntimeError,
            ctl.add,
            dedent(
                """\
            asp 1 0 0 incremental
            0
            0
            """
            ),
        )

    def test_rule(self):
        """
        Test rule parsing.
        """
        # a fact
        self.assertEqual(
            solve_aspif(
                """\
            asp 1 0 0
            1 0 1 1 0 0
            4 1 a 1 1
            0
            """
            ),
            [["a"]],
        )
        # a choice
        self.assertEqual(
            solve_aspif(
                """\
            asp 1 0 0
            1 1 1 1 0 0
            4 1 a 1 1
            0
            """
            ),
            [[], ["a"]],
        )
        # a rule with normal body
        self.assertEqual(
            solve_aspif(
                """\
            asp 1 0 0
            1 1 1 1 0 0
            1 0 1 2 0 1 1
            1 0 1 3 0 1 -2
            4 1 a 1 1
            4 1 b 1 2
            4 1 c 1 3
            0
            """
            ),
            [["a", "b"], ["c"]],
        )
        # a rule with weight body
        self.assertEqual(
            solve_aspif(
                """\
            asp 1 0 0
            1 1 2 1 2 0 0
            1 0 1 3 1 2 2 1 1 2 1
            1 0 0 0 1 -3
            4 1 a 1 1
            4 1 b 1 2
            0
            """
            ),
            [["a", "b"]],
        )

    def test_minimize(self):
        """
        Test minimize parsing.
        """
        self.assertEqual(
            solve_aspif(
                """\
            asp 1 0 0
            1 1 2 1 2 0 0
            1 0 0 0 1 -2
            1 0 0 0 1 -1
            2 1 1 1 1
            2 2 1 2 2
            4 1 a 1 1
            4 1 b 1 2
            0
            """
            ),
            [["a", "b"]],
        )

    def test_external(self):
        """
        Test external parsing.
        """
        self.assertEqual(
            solve_aspif(
                """\
            asp 1 0 0
            5 1 0
            4 1 a 1 1
            0
            """
            ),
            [[], ["a"]],
        )
        self.assertEqual(
            solve_aspif(
                """\
            asp 1 0 0
            5 1 1
            4 1 a 1 1
            0
            """
            ),
            [["a"]],
        )
        self.assertEqual(
            solve_aspif(
                """\
            asp 1 0 0
            5 1 2
            4 1 a 1 1
            0
            """
            ),
            [[]],
        )

        self.assertEqual(
            solve_aspif(
                """\
            asp 1 0 0
            5 1 3
            4 1 a 1 1
            0
            """
            ),
            [[]],
        )

    def test_assume(self):
        """
        Test assumption parsing.
        """
        self.assertEqual(
            solve_aspif(
                """\
            asp 1 0 0
            1 1 2 1 2 0 0
            6 2 1 -2
            4 1 a 1 1
            4 1 b 1 2
            0
            """
            ),
            [["a"]],
        )

    def test_heuristic(self):
        """
        Test heuristic parsing.
        """
        self.assertEqual(
            solve_aspif(
                """\
            asp 1 0 0
            1 1 1 1 0 0
            7 4 1 1 2 0
            4 1 a 1 1
            0
            """
            ),
            [[], ["a"]],
        )

    def test_edge(self):
        """
        Test edge parsing.
        """
        self.assertEqual(
            solve_aspif(
                """\
            asp 1 0 0
            1 1 1 1 0 0
            8 0 1 1 1
            8 1 0 0
            4 1 a 1 1
            0
            """
            ),
            [[]],
        )

    def test_theory(self):
        """
        Test theory parsing.
        """
        # no guard
        self.assertEqual(
            theory(
                """\
            asp 1 0 0
            1 1 1 1 0 0
            1 0 1 2 0 0
            9 1 0 1 b
            9 0 1 1
            9 4 0 1 1 1 1
            9 5 2 0 1 0
            4 1 x 1 1
            0
            """
            ),
            ["&b { 1 : x }"],
        )
        # with guard
        self.assertEqual(
            theory(
                """\
            asp 1 0 0
            1 1 1 1 0 0
            1 0 1 2 0 0
            9 1 0 1 a
            9 0 3 1
            9 4 0 1 3 1 1
            9 1 2 3 <=<
            9 0 1 2
            9 6 2 0 1 0 2 1
            4 1 x 1 1
            0
            """
            ),
            ["&a { 1 : x } <=< 2"],
        )
        # complex terms
        self.assertEqual(
            theory(
                """\
            asp 1 0 0
            1 0 1 1 0 0
            9 1 0 1 b
            9 0 3 1
            9 0 4 2
            9 1 2 3 +-*
            9 2 5 2 2 3 4
            9 2 6 -1 2 3 4
            9 0 7 3
            9 0 8 4
            9 2 9 -3 2 7 8
            9 0 10 5
            9 0 11 6
            9 2 12 -2 2 10 11
            9 1 1 1 f
            9 2 13 1 4 5 6 9 12
            9 4 0 1 13 0
            9 5 1 0 1 0
            0
            """
            ),
            ["&b { f((1+-*2),(1,2),[3,4],{5,6}) :  }"],
        )

    def test_comment(self):
        """
        Test comment parsing.
        """
        ctl = Control()
        ctl.add(
            dedent(
                """\
            asp 1 0 0
            10 nothing!
            0
            """
            )
        )
