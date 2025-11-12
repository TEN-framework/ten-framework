"""
Tests for solving.
"""

from typing import cast
from unittest import TestCase

from clingo import (
    Control,
    Function,
    Model,
    ModelType,
    SolveHandle,
    SolveResult,
    SymbolicAtom,
)

from .util import _MCB, _check_sat, _p


class TestSolving(TestCase):
    """
    Tests basic solving and related functions.
    """

    def setUp(self):
        self.mcb = _MCB()
        self.mit = _MCB()
        self.ctl = Control(["0"])

    def tearDown(self):
        self.mcb = None
        self.mit = None
        self.ctl = None

    def test_solve_result_str(self):
        """
        Test string representation of solve results.
        """
        ret = self.ctl.solve()
        self.assertEqual(str(ret), "SAT")
        self.assertRegex(repr(ret), "SolveResult(.*)")

    def test_model_str(self):
        """
        Test string representation of models.
        """
        self.ctl.add("base", [], "a.")
        self.ctl.ground([("base", [])])
        with cast(SolveHandle, self.ctl.solve(yield_=True)) as hnd:
            for mdl in hnd:
                self.assertEqual(str(mdl), "a")
                self.assertRegex(repr(mdl), "Model(.*)")

    def test_solve_cb(self):
        """
        Test solving using callback.
        """
        self.ctl.add("base", [], "1 {a; b} 1. c.")
        self.ctl.ground([("base", [])])
        _check_sat(
            self,
            cast(
                SolveResult,
                self.ctl.solve(
                    on_model=self.mcb.on_model,
                    on_last=self.mcb.on_last,
                    yield_=False,
                    async_=False,
                ),
            ),
        )
        self.assertEqual(self.mcb.models, _p(["a", "c"], ["b", "c"]))
        self.assertEqual(self.mcb.last[0], ModelType.StableModel)
        self.assertEqual(self.mcb.last, self.mcb.final)

    def test_solve_async(self):
        """
        Test asynchronous solving.
        """
        self.ctl.add("base", [], "1 {a; b} 1. c.")
        self.ctl.ground([("base", [])])
        with cast(
            SolveHandle,
            self.ctl.solve(on_model=self.mcb.on_model, yield_=False, async_=True),
        ) as hnd:
            _check_sat(self, hnd.get())
            self.assertEqual(self.mcb.models, _p(["a", "c"], ["b", "c"]))
            self.mit.on_last(hnd.last())
            self.assertEqual(self.mcb.last, self.mit.final)

    def test_solve_yield(self):
        """
        Test solving yielding models.
        """
        self.ctl.add("base", [], "1 {a; b} 1. c.")
        self.ctl.ground([("base", [])])
        with cast(
            SolveHandle,
            self.ctl.solve(on_model=self.mcb.on_model, yield_=True, async_=False),
        ) as hnd:
            for m in hnd:
                self.mit.on_model(m)
                self.assertEqual(hnd.last(), None)  # Not yet finished
            _check_sat(self, hnd.get())
            self.mit.on_last(hnd.last())
            self.assertEqual(self.mcb.last, self.mit.final)
            self.assertEqual(self.mcb.models, _p(["a", "c"], ["b", "c"]))
            self.assertEqual(self.mit.models, _p(["a", "c"], ["b", "c"]))

    def test_solve_async_yield(self):
        """
        Test solving yielding models asynchronously.
        """
        self.ctl.add("base", [], "1 {a; b} 1. c.")
        self.ctl.ground([("base", [])])
        with cast(
            SolveHandle,
            self.ctl.solve(on_model=self.mcb.on_model, yield_=True, async_=True),
        ) as hnd:
            while True:
                hnd.resume()
                _ = hnd.wait()
                m = hnd.model()
                if m is None:
                    break
                self.mit.on_model(m)
                self.assertEqual(hnd.last(), None)  # Not yet finished
            _check_sat(self, hnd.get())
            self.mit.on_last(hnd.last())
            self.assertEqual(self.mcb.last, self.mit.final)
            self.assertEqual(self.mcb.models, _p(["a", "c"], ["b", "c"]))
            self.assertEqual(self.mit.models, _p(["a", "c"], ["b", "c"]))

    def test_solve_interrupt(self):
        """
        Test interrupting solving.
        """
        self.ctl.add(
            "base",
            [],
            "1 { p(P,H): H=1..99 } 1 :- P=1..100.\n1 { p(P,H): P=1..100 } 1 :- H=1..99.",
        )
        self.ctl.ground([("base", [])])
        with cast(SolveHandle, self.ctl.solve(async_=True)) as hnd:
            hnd.resume()
            hnd.cancel()
            ret = hnd.get()
            self.assertTrue(ret.interrupted)

        with cast(SolveHandle, self.ctl.solve(async_=True)) as hnd:
            hnd.resume()
            self.ctl.interrupt()
            ret = hnd.get()
            self.assertTrue(ret.interrupted)

    def test_solve_core(self):
        """
        Test core retrieval.
        """
        self.ctl.add("base", [], "3 { p(1..10) } 3.")
        self.ctl.ground([("base", [])])
        ass = []
        for atom in self.ctl.symbolic_atoms.by_signature("p", 1):
            ass.append(-atom.literal)
        ret = cast(
            SolveResult, self.ctl.solve(on_core=self.mcb.on_core, assumptions=ass)
        )
        self.assertTrue(ret.unsatisfiable)
        self.assertTrue(len(self.mcb.core) > 7)

    def test_enum(self):
        """
        Test core retrieval.
        """
        self.ctl = Control(["0", "-e", "cautious"])
        self.ctl.add("base", [], "1 {a; b} 1. c.")
        self.ctl.ground([("base", [])])
        self.ctl.solve(on_model=self.mcb.on_model, on_last=self.mcb.on_last)
        self.assertEqual(self.mcb.last[0], ModelType.CautiousConsequences)
        self.assertEqual([self.mcb.last[1]], _p(["c"]))
        self.assertEqual(self.mcb.last, self.mcb.final)

        self.mcb.final = None
        self.ctl = Control(["0", "-e", "brave"])
        self.ctl.add("base", [], "1 {a; b} 1. c.")
        self.ctl.ground([("base", [])])
        self.ctl.solve(on_model=self.mcb.on_model, on_last=self.mcb.on_last)
        self.assertEqual(self.mcb.last[0], ModelType.BraveConsequences)
        self.assertEqual([self.mcb.last[1]], _p(["a", "b", "c"]))
        self.assertEqual(self.mcb.last, self.mcb.final)

    def test_model(self):
        """
        Test functions of model.
        """

        def on_model(m: Model, last: bool):
            self.assertTrue(m.contains(Function("a")))
            self.assertTrue(
                m.is_true(
                    cast(SymbolicAtom, m.context.symbolic_atoms[Function("a")]).literal
                )
            )
            self.assertFalse(m.is_true(1000))
            self.assertEqual(m.thread_id, 0)
            self.assertEqual(m.number, 1)
            self.assertEqual(m.optimality_proven, last)
            self.assertEqual(m.cost, [3])
            self.assertEqual(m.priority, [5])
            if not last:
                m.extend([Function("e")])
            self.assertSequenceEqual(m.symbols(theory=True), [Function("e")])

        self.ctl.add("base", [], "a. b. c. #minimize { 1@5,a:a; 1@5,b:b; 1@5,c:c }.")
        self.ctl.ground([("base", [])])
        self.ctl.solve(
            on_model=lambda m: on_model(m, False), on_last=lambda m: on_model(m, True)
        )

    def test_remove_minimize(self):
        self.ctl.add("base", [], "a. #minimize { 1,t : a }.")
        self.ctl.ground([("base", [])])

        self.ctl.solve(on_model=lambda m: self.assertEqual(m.cost, [1]))

        self.ctl.add("s1", [], "b. #minimize { 1,t : b }.")
        self.ctl.ground([("s1", [])])
        self.ctl.solve(on_model=lambda m: self.assertEqual(m.cost, [1]))

        self.ctl.remove_minimize()
        self.ctl.add("s2", [], "c. #minimize { 1,x : c ; 1,t : c}.")
        self.ctl.ground([("s2", [])])
        self.ctl.solve(on_model=lambda m: self.assertEqual(m.cost, [2]))

        self.ctl.remove_minimize()
        self.ctl.solve(on_model=lambda m: self.assertEqual(m.cost, []))

    def test_cautious_consequences(self):
        """
        Test is_consequence function of model.
        """

        def lookup(m: Model, name: str):
            return cast(SymbolicAtom, m.context.symbolic_atoms[Function(name)]).literal

        def on_model(m: Model):
            a = lookup(m, "a")
            b = lookup(m, "b")
            c = lookup(m, "c")
            ca = m.is_consequence(a)
            cb = m.is_consequence(b)
            cc = m.is_consequence(c)
            nca = m.is_consequence(-a)
            ncb = m.is_consequence(-b)
            ncc = m.is_consequence(-c)
            self.assertTrue(ca is True)
            self.assertTrue(nca is False)
            self.assertTrue(ncb is False)
            self.assertTrue(ncc is False)
            if m.number == 1:
                self.assertTrue(ncb is None or ncb is False)
                self.assertTrue(ncc is None or ncc is False)
                self.assertTrue(cb is None or cb is False)
                self.assertTrue(cc is None or cc is False)
                self.assertTrue(cb != cc)
            if m.number == 2:
                self.assertTrue(cb is False)
                self.assertTrue(cc is False)

        self.ctl.configuration.solve.enum_mode = "cautious"
        self.ctl.add("base", [], "a. b | c.")
        self.ctl.ground([("base", [])])
        self.ctl.solve(on_model=on_model)

    def test_update_projection(self):
        self.ctl.configuration.solve.project = "auto"
        self.ctl.configuration.solve.models = "0"
        self.ctl.add("base", [], "{a;b;c;d}. #project a/0. #project b/0.")
        self.ctl.ground([("base", [])])
        self.ctl.solve(on_model=self.mcb.on_model)
        self.assertEqual(self.mcb.models, _p([], ["a"], ["a", "b"], ["b"]))

        pro = []
        for atom in self.ctl.symbolic_atoms.by_signature("c", 0):
            pro.append(atom.literal)
        pro.append(Function("d"))

        self.ctl.replace_project(pro)
        self.mcb = _MCB()
        self.ctl.solve(on_model=self.mcb.on_model)
        self.assertEqual(self.mcb.models, _p([], ["c"], ["c", "d"], ["d"]))

        self.mcb = _MCB()
        pro = [Function("a")]
        self.ctl.add_project(pro)
        self.ctl.solve(on_model=self.mcb.on_model)
        self.assertEqual(
            self.mcb.models,
            _p(
                [],
                ["a"],
                ["a", "c"],
                ["a", "c", "d"],
                ["a", "d"],
                ["c"],
                ["c", "d"],
                ["d"],
            ),
        )

    def test_control_clause(self):
        """
        Test adding clauses while solving.
        """
        self.ctl.add("base", [], "1 {a; b; c} 1.")
        self.ctl.ground([("base", [])])
        with cast(
            SolveHandle,
            self.ctl.solve(on_model=self.mcb.on_model, yield_=True, async_=False),
        ) as hnd:
            for m in hnd:
                clause = []
                if m.contains(Function("a")):
                    clause.append((Function("b"), False))
                else:
                    clause.append((Function("a"), False))
                m.context.add_clause(clause)

            _check_sat(self, hnd.get())
            self.assertEqual(len(self.mcb.models), 2)

    def test_control_nogood(self):
        """
        Test adding nogoods while solving.
        """
        self.ctl.add("base", [], "1 {a; b; c} 1.")
        self.ctl.ground([("base", [])])
        with cast(
            SolveHandle,
            self.ctl.solve(on_model=self.mcb.on_model, yield_=True, async_=False),
        ) as hnd:
            for m in hnd:
                clause = []
                if m.contains(Function("a")):
                    clause.append((Function("b"), True))
                else:
                    clause.append((Function("a"), True))
                m.context.add_nogood(clause)

            _check_sat(self, hnd.get())
            self.assertEqual(len(self.mcb.models), 2)
