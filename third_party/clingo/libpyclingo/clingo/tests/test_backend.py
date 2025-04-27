"""
Tests for the backend/observer.
"""

from unittest import TestCase
from typing import Sequence, Tuple
from clingo.symbol import parse_term
from clingo import (
    Control,
    Function,
    HeuristicType,
    Observer,
    Symbol,
    TheorySequenceType,
    TruthValue,
)


class TestObserverBackend(Observer):
    """
    Test Observer.
    """

    def __init__(self, case):
        self._case = case
        self.called = set()

    def init_program(self, incremental: bool) -> None:
        self.called.add("init_program")

    def begin_step(self) -> None:
        self.called.add("begin_step")

    def rule(self, choice: bool, head: Sequence[int], body: Sequence[int]) -> None:
        self.called.add("rule")
        self._case.assertTrue(choice)
        self._case.assertEqual(head, [1])
        self._case.assertEqual(body, [2, 3])

    def weight_rule(
        self,
        choice: bool,
        head: Sequence[int],
        lower_bound: int,
        body: Sequence[Tuple[int, int]],
    ) -> None:
        self.called.add("weight_rule")
        self._case.assertFalse(choice)
        self._case.assertEqual(head, [2])
        self._case.assertEqual(lower_bound, 1)
        self._case.assertEqual(body, [(2, 3), (4, 5)])

    def minimize(self, priority: int, literals: Sequence[Tuple[int, int]]) -> None:
        self.called.add("minimize")
        self._case.assertEqual(priority, 0)
        self._case.assertEqual(literals, [(2, 3), (4, 5)])

    def project(self, atoms: Sequence[int]) -> None:
        self.called.add("project")
        self._case.assertEqual(atoms, [2, 4])

    def output_atom(self, symbol: Symbol, atom: int) -> None:
        self.called.add("output_atom")
        self._case.assertEqual(symbol, Function("a"))
        self._case.assertEqual(atom, 2)

    def external(self, atom: int, value: TruthValue) -> None:
        self.called.add("external")
        self._case.assertEqual(atom, 3)
        self._case.assertEqual(value, TruthValue.Release)

    def assume(self, literals: Sequence[int]) -> None:
        self.called.add("assume")
        self._case.assertEqual(literals, [2, 3])

    def heuristic(
        self,
        atom: int,
        type_: HeuristicType,
        bias: int,
        priority: int,
        condition: Sequence[int],
    ) -> None:
        self.called.add("heuristic")
        self._case.assertEqual(atom, 2)
        self._case.assertEqual(type_, HeuristicType.Level)
        self._case.assertEqual(bias, 5)
        self._case.assertEqual(priority, 7)
        self._case.assertEqual(condition, [1, 3])

    def acyc_edge(self, node_u: int, node_v: int, condition: Sequence[int]) -> None:
        self.called.add("acyc_edge")
        self._case.assertEqual(node_u, 1)
        self._case.assertEqual(node_v, 2)
        self._case.assertEqual(condition, [3, 4])

    def end_step(self) -> None:
        self.called.add("end_step")


class TestObserverTheory(Observer):
    """
    Test Observer.
    """

    def __init__(self, case):
        self._case = case
        self.called = set()

    def output_term(self, symbol: Symbol, condition: Sequence[int]) -> None:
        self.called.add("output_term")
        self._case.assertEqual(symbol, Function("t"))
        self._case.assertGreaterEqual(len(condition), 1)

    def theory_term_number(self, term_id: int, number: int) -> None:
        self.called.add("theory_term_number")
        self._case.assertEqual(number, 1)

    def theory_term_string(self, term_id: int, name: str) -> None:
        self.called.add("theory_term_string")
        self._case.assertEqual(name, "a")

    def theory_term_compound(
        self, term_id: int, name_id_or_type: int, arguments: Sequence[int]
    ) -> None:
        self.called.add("theory_term_compound")
        self._case.assertEqual(name_id_or_type, -1)
        self._case.assertGreaterEqual(len(arguments), 2)

    def theory_element(
        self, element_id: int, terms: Sequence[int], condition: Sequence[int]
    ) -> None:
        self.called.add("theory_element")
        self._case.assertEqual(len(terms), 1)
        self._case.assertEqual(len(condition), 2)

    def theory_atom(
        self, atom_id_or_zero: int, term_id: int, elements: Sequence[int]
    ) -> None:
        self.called.add("theory_atom")
        self._case.assertEqual(len(elements), 1)


class TestObserverTheoryWithGuard(Observer):
    """
    Test Observer.
    """

    def __init__(self, case):
        self._case = case
        self.called = set()

    def theory_term_string(self, term_id: int, name: str) -> None:
        self.called.add(f"theory_term_string: {name}")

    def theory_atom_with_guard(
        self,
        atom_id_or_zero: int,
        term_id: int,
        elements: Sequence[int],
        operator_id: int,
        right_hand_side_id: int,
    ) -> None:
        self.called.add("theory_atom_with_guard")
        self._case.assertEqual(len(elements), 0)


class TestBackend(TestCase):
    """
    Tests basic solving and related functions.
    """

    def test_backend(self):
        """
        Test backend via observer.
        """
        ctl = Control()
        obs = TestObserverBackend(self)
        ctl.register_observer(obs)
        with ctl.backend() as backend:
            self.assertIn("init_program", obs.called)
            self.assertIn("begin_step", obs.called)
            backend.add_atom()
            backend.add_atom(Function("a"))
            backend.add_rule([1], [2, 3], True)
            self.assertIn("rule", obs.called)
            backend.add_weight_rule([2], 1, [(2, 3), (4, 5)])
            self.assertIn("weight_rule", obs.called)
            backend.add_minimize(0, [(2, 3), (4, 5)])
            self.assertIn("minimize", obs.called)
            backend.add_project([2, 4])
            self.assertIn("project", obs.called)
            backend.add_heuristic(2, HeuristicType.Level, 5, 7, [1, 3])
            self.assertIn("heuristic", obs.called)
            backend.add_assume([2, 3])
            self.assertIn("assume", obs.called)
            backend.add_acyc_edge(1, 2, [3, 4])
            self.assertIn("acyc_edge", obs.called)
            backend.add_external(3, TruthValue.Release)
            self.assertIn("external", obs.called)
        self.assertIn("output_atom", obs.called)
        ctl.solve()
        self.assertIn("end_step", obs.called)

    def test_theory(self):
        """
        Test observer via grounding.
        """
        ctl = Control()
        obs = TestObserverTheory(self)
        ctl.register_observer(obs)
        ctl.add(
            "base",
            [],
            """\
        #theory test {
            t { };
            &a/0 : t, head
        }.
        {a; b}.
        #show t : a, b.
        &a { (1,a): a,b }.
        """,
        )
        ctl.ground([("base", [])])
        self.assertIn("output_term", obs.called)
        self.assertIn("theory_term_number", obs.called)
        self.assertIn("theory_term_string", obs.called)
        self.assertIn("theory_term_compound", obs.called)
        self.assertIn("theory_element", obs.called)
        self.assertIn("theory_atom", obs.called)
        ctl.solve()

    def test_adding_theory(self):
        """
        Test theory related functions in backend.
        """
        ctl = Control()
        with ctl.backend() as backend:
            num_one = backend.add_theory_term_number(1)
            num_two = backend.add_theory_term_number(2)
            str_x = backend.add_theory_term_string("x")
            fun = backend.add_theory_term_function("f", [num_one, num_two, str_x])
            seq = backend.add_theory_term_sequence(
                TheorySequenceType.Set, [num_one, num_two, str_x]
            )
            lst = backend.add_theory_term_sequence(TheorySequenceType.List, [num_one, num_two])
            tup = backend.add_theory_term_sequence(TheorySequenceType.Tuple, [num_one, num_two])
            fseq = backend.add_theory_term_function("f", [seq])

            self.assertEqual(num_one, backend.add_theory_term_number(1))
            self.assertEqual(num_two, backend.add_theory_term_number(2))
            self.assertEqual(str_x, backend.add_theory_term_string("x"))
            self.assertEqual(num_one, backend.add_theory_term_symbol(parse_term("1")))
            self.assertEqual(num_two, backend.add_theory_term_symbol(parse_term("2")))
            self.assertEqual(
                fun, backend.add_theory_term_symbol(parse_term("f(1,2,x)"))
            )

            elem = backend.add_theory_element([num_one, num_two, seq, fun, lst, tup], [1, -2, 3])

            # Tests with guards and elements
            backend.add_theory_atom(fun, [], 0)
            backend.add_theory_atom_with_guard(fun, [], "=", num_one, 0)
            backend.add_theory_atom(
                backend.add_theory_term_symbol(parse_term("g(1,2)")), [], 0
            )
            backend.add_theory_atom(fseq, [elem], 0)

        atoms = list(ctl.theory_atoms)
        self.assertListEqual(
            [str(atom) for atom in atoms],
            [
                "&f(1,2,x){}",
                "&f(1,2,x){}=1",
                "&g(1,2){}",
                "&f({1,2,x}){1,2,{1,2,x},f(1,2,x),[1,2],(1,2): #aux(1),not #aux(2),#aux(3)}",
            ],
        )
        self.assertEqual(atoms[-1].elements[-1].condition, [1, -2, 3])

    def test_theory_with_guard(self):
        """
        Test observer via grounding.
        """
        ctl = Control()
        obs = TestObserverTheoryWithGuard(self)
        ctl.register_observer(obs)
        ctl.add(
            "base",
            [],
            """\
        #theory test {
            t { };
            &a/0 : t, {=}, t, head
        }.
        &a { } = a.
        """,
        )
        ctl.ground([("base", [])])
        self.assertIn("theory_term_string: a", obs.called)
        self.assertIn("theory_term_string: =", obs.called)
        self.assertIn("theory_atom_with_guard", obs.called)
        ctl.solve()
