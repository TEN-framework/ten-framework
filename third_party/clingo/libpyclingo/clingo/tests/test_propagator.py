"""
Tests for the propagator.
"""

from unittest import TestCase
from typing import cast
from clingo import (
    Assignment,
    Control,
    Function,
    PropagateControl,
    PropagateInit,
    Propagator,
    PropagatorCheckMode,
    PropagatorUndoMode,
    SolveResult,
    SymbolicAtom,
)

from .util import _MCB, _check_sat, _p


class TestPropagatorControl(Propagator):
    """
    Test functions in PropagateControl.
    """

    def __init__(self, case: TestCase):
        self._case = case
        self._lit_a = 0

    def init(self, init: PropagateInit):
        init.check_mode = PropagatorCheckMode.Off
        self._case.assertEqual(init.check_mode, PropagatorCheckMode.Off)
        self._case.assertEqual(init.number_of_threads, 1)
        a = init.symbolic_atoms[Function("a")]
        self._case.assertIsNotNone(a)
        self._lit_a = init.solver_literal(cast(SymbolicAtom, a).literal)
        init.add_watch(-self._lit_a)

    def propagate(self, control: PropagateControl, changes):
        ass = control.assignment
        trail = ass.trail
        lvl = ass.decision_level
        self._case.assertIn(-self._lit_a, changes)
        self._case.assertGreaterEqual(lvl, 1)
        self._case.assertGreaterEqual(ass.level(self._lit_a), 1)
        self._case.assertGreaterEqual(len(trail), 1)
        self._case.assertGreaterEqual(len(list(trail)), 1)
        self._case.assertEqual(trail[trail.begin(lvl)], -self._lit_a)
        self._case.assertEqual(
            list(trail[trail.begin(lvl) : trail.end(lvl)]), [-self._lit_a]
        )
        self._case.assertEqual(ass.decision(lvl), -self._lit_a)
        self._case.assertEqual(control.thread_id, 0)
        self._case.assertTrue(control.has_watch(-self._lit_a))
        self._case.assertTrue(control.propagate())
        self._case.assertFalse(control.add_clause([self._lit_a]))

    def undo(self, thread_id, assignment, changes):
        self._case.assertEqual(thread_id, 0)
        self._case.assertIn(-self._lit_a, changes)


class TestPropagatorMode(Propagator):
    """
    Test check/undo mode.
    """
    def __init__(self, case: TestCase):
        self._case = case
        self.num_check = 0
        self.num_undo = 0

    def init(self, init: PropagateInit):
        init.check_mode = PropagatorCheckMode.Fixpoint
        init.undo_mode = PropagatorUndoMode.Always

        self._case.assertEqual(init.check_mode, PropagatorCheckMode.Fixpoint)
        self._case.assertEqual(init.undo_mode, PropagatorUndoMode.Always)

    def check(self, control: PropagateControl):
        self.num_check += 1

    def undo(self, thread_id, assignment, changes):
        self.num_undo += 1


class TestPropagatorInit(Propagator):
    """
    Test functions in PropagateInit.
    """

    def __init__(self, case: TestCase):
        self._case = case

    def init(self, init: PropagateInit):
        a = init.symbolic_atoms[Function("a")]
        b = init.symbolic_atoms[Function("b")]
        c = init.symbolic_atoms[Function("c")]
        self._case.assertIsNotNone(a)
        self._case.assertIsNotNone(b)
        self._case.assertIsNotNone(c)
        lit_a = init.solver_literal(cast(SymbolicAtom, a).literal)
        lit_b = init.solver_literal(cast(SymbolicAtom, b).literal)
        lit_c = init.solver_literal(cast(SymbolicAtom, c).literal)
        lit = init.add_literal()
        # a <=> b
        init.add_clause([lit_a, -lit])
        init.add_clause([lit, -lit_a])
        init.add_clause([lit_b, -lit])
        init.add_clause([lit, -lit_b])
        # c <=> {a, b} >= 2
        init.add_weight_constraint(lit_c, [(lit_a, 1), (lit_b, 1)], 2, 0)
        init.add_minimize(lit_a, -1)
        self._case.assertTrue(init.propagate())
        self._case.assertEqual(list(init.theory_atoms), [])
        # test assignment
        self._case.assertIsNone(init.assignment.value(lit_a))
        self._case.assertFalse(init.assignment.is_true(lit_a))
        self._case.assertFalse(init.assignment.is_false(lit_a))
        self._case.assertFalse(init.assignment.is_fixed(lit_a))
        self._case.assertEqual(init.assignment.decision_level, 0)
        self._case.assertTrue(init.assignment.has_literal(lit_a))
        self._case.assertFalse(init.assignment.has_conflict)
        self._case.assertFalse(init.assignment.is_total)
        self._case.assertEqual(init.assignment.root_level, 0)
        self._case.assertGreaterEqual(len(init.assignment), 4)
        self._case.assertGreaterEqual(len(list(init.assignment)), 4)
        self._case.assertEqual(len(init.assignment[0:4][0:4:2]), 2)
        self._case.assertEqual(init.assignment[0:4][0:4:2][1], init.assignment[2])


class TestPropagator(Propagator):
    """
    Test adding literals while solving.
    """

    def __init__(self, case: TestCase):
        self._case = case
        self._added = False

    def check(self, control: PropagateControl):
        if not self._added:
            self._added = True
            lit = control.add_literal()
            self._case.assertFalse(control.has_watch(lit))
            control.add_watch(lit)
            self._case.assertTrue(control.has_watch(lit))
            control.remove_watch(lit)
            self._case.assertFalse(control.has_watch(lit))


class TestHeuristic(Propagator):
    """
    Test decide.
    """

    def __init__(self, case: TestCase):
        self._case = case
        self._lit_a = 0
        self._lit_b = 0

    def init(self, init: PropagateInit):
        a = init.symbolic_atoms[Function("a")]
        b = init.symbolic_atoms[Function("b")]
        self._case.assertIsNotNone(a)
        self._case.assertIsNotNone(b)
        self._lit_a = init.solver_literal(cast(SymbolicAtom, a).literal)
        self._lit_b = init.solver_literal(cast(SymbolicAtom, b).literal)

    def decide(self, thread_id: int, assignment: Assignment, fallback: int) -> int:
        self._case.assertEqual(thread_id, 0)
        if assignment.is_free(self._lit_a):
            return self._lit_a
        if assignment.is_free(self._lit_b):
            return -self._lit_b
        return fallback


class TestSymbol(TestCase):
    """
    Tests basic solving and related functions.
    """

    def setUp(self):
        self.mcb = _MCB()
        self.ctl = Control(["0"])

    def tearDown(self):
        self.mcb = None
        self.ctl = None

    def test_propagator_control(self):
        """
        Test PropagateControl.
        """
        self.ctl.add("base", [], "{a}.")
        self.ctl.ground([("base", [])])
        self.ctl.register_propagator(TestPropagatorControl(self))
        _check_sat(
            self,
            cast(
                SolveResult,
                self.ctl.solve(on_model=self.mcb.on_model, yield_=False, async_=False),
            ),
        )
        self.assertEqual(self.mcb.models, _p(["a"]))

    def test_propagator_init(self):
        """
        Test PropagateInit and Assignment.
        """
        self.ctl.add("base", [], "{a; b; c}.")
        self.ctl.ground([("base", [])])
        self.ctl.register_propagator(TestPropagatorInit(self))
        _check_sat(
            self,
            cast(
                SolveResult,
                self.ctl.solve(on_model=self.mcb.on_model, yield_=False, async_=False),
            ),
        )
        self.assertEqual(self.mcb.models[-1:], _p(["a", "b", "c"]))

    def test_propagator_mode(self):
        """
        Test check and undo mode.
        """
        self.ctl = Control([])
        self.ctl.add("base", [], "{a; b}.")
        self.ctl.ground([("base", [])])
        tpm = TestPropagatorMode(self)
        self.ctl.register_propagator(tpm)
        ret = self.ctl.solve()
        self.assertTrue(ret.satisfiable)
        self.assertGreaterEqual(tpm.num_check, 3)
        self.assertEqual(tpm.num_undo + 1, tpm.num_check)

    def test_propagator(self):
        """
        Test adding literals while solving.
        """
        self.ctl.add("base", [], "")
        self.ctl.ground([("base", [])])
        self.ctl.register_propagator(TestPropagator(self))
        _check_sat(
            self,
            cast(
                SolveResult,
                self.ctl.solve(on_model=self.mcb.on_model, yield_=False, async_=False),
            ),
        )
        self.assertEqual(self.mcb.models, _p([], []))

    def test_heurisitc(self):
        """
        Test decide.
        """
        self.ctl = Control(["1"])
        self.ctl.add("base", [], "{a;b}.")
        self.ctl.ground([("base", [])])
        self.ctl.register_propagator(TestHeuristic(self))
        self.ctl.solve(on_model=self.mcb.on_model)
        self.assertEqual(self.mcb.models, _p(["a"]))
