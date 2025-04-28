"""
Tests control.
"""
from unittest import TestCase
from typing import cast
from clingo import Control, Function, Number, SolveResult


class TestError(Exception):
    """
    Test exception.
    """


class Context:
    """
    Simple context with some test functions.
    """

    def cb_num(self, c):
        """
        Simple test callback.
        """
        return [Number(c.number + 1), Number(c.number - 1)]

    def cb_error(self):
        """
        Simple test raising an error.
        """
        raise TestError("test")


class TestControl(TestCase):
    """
    Tests basic functions of the control object.
    """

    def test_default(self):
        """
        Test grounding with context and parameters.
        """
        ctl = Control()
        ctl.add("p.")
        ctl.ground()
        symbols = [atom.symbol for atom in ctl.symbolic_atoms]
        self.assertEqual(sorted(symbols), [Function("p", [])])

    def test_ground(self):
        """
        Test grounding with context and parameters.
        """
        ctx = Context()
        ctl = Control()
        ctl.add("part", ["c"], "p(@cb_num(c)).")
        ctl.ground([("part", [Number(1)])], ctx)
        symbols = [atom.symbol for atom in ctl.symbolic_atoms]
        self.assertEqual(
            sorted(symbols), [Function("p", [Number(0)]), Function("p", [Number(2)])]
        )

    def test_ground_error(self):
        """
        Test grounding with context and parameters.
        """
        ctx = Context()
        ctl = Control()
        ctl.add("part", ["c"], "p(@cb_error()).")
        self.assertRaisesRegex(
            TestError, "test", ctl.ground, [("part", [Number(1)])], ctx
        )

    def test_lower(self):
        """
        Test lower bounds reported during optimization.
        """
        ctl = Control(["--opt-str=usc,oll,0", "--stats=2"])
        ctl.add(
            "base",
            [],
            "1 { p(X); q(X) } 1 :- X=1..3. #minimize { 1,p,X: p(X); 1,q,X: q(X) }.",
        )
        ctl.ground([("base", [])])
        lower = []
        self.assertTrue(cast(SolveResult, ctl.solve(on_unsat=lower.append)).satisfiable)
        self.assertEqual(lower, [[1], [2], [3]])
        self.assertEqual(ctl.statistics["summary"]["lower"], [3.0])

    def test_error_handling(self):
        """
        Test basic error handling during solving.
        """
        ctl = Control()
        ctl.add("base", [], "1 {a; b} 1.")
        ctl.ground([("base", [])])

        self.assertRaises(
            ZeroDivisionError, lambda: ctl.solve(on_model=lambda m: 1 / 0)
        )

        with ctl.solve(on_model=lambda m: 1 / 0, yield_=True) as handle:
            self.assertRaises(ZeroDivisionError, lambda: [_ for _ in handle])

        # Note: currently clasp does not store and re-raise the exception in
        # asynchronous mode, so we get a runtime error instead
        with ctl.solve(on_model=lambda m: 1 / 0, async_=True) as handle:
            self.assertRaises(RuntimeError, handle.get)
