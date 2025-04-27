"""
Tests for theory and symbolic atoms.
"""

from unittest import TestCase
from typing import cast
from clingo import Control, Function, Number, TheoryTermType
from clingo.symbolic_atoms import SymbolicAtom

THEORY = """
#theory test {
    t { };
    &a/0 : t, head;
    &b/0 : t, {=}, t, head
}.
"""


class TestAtoms(TestCase):
    """
    Tests for theory and symbolic atoms.
    """

    def setUp(self):
        self.ctl = Control()

    def test_symbolic_atom(self):
        """
        Test symbolic atom.
        """
        self.ctl.add("base", [], "p(1). {p(2)}. #external p(3).")
        self.ctl.ground([("base", [])])

        atoms = self.ctl.symbolic_atoms

        p1 = cast(SymbolicAtom, atoms[Function("p", [Number(1)])])
        self.assertIsNotNone(p1)
        self.assertTrue(p1.is_fact)
        self.assertFalse(p1.is_external)
        self.assertTrue(p1.literal >= 1)
        self.assertEqual(p1.symbol, Function("p", [Number(1)]))
        self.assertTrue(p1.match("p", 1, True))
        self.assertFalse(p1.match("p", 2, True))

        p2 = cast(SymbolicAtom, atoms[Function("p", [Number(2)])])
        self.assertIsNotNone(p2)
        self.assertFalse(p2.is_fact)
        self.assertFalse(p2.is_external)
        self.assertTrue(p2.literal >= 2)
        self.assertEqual(p2.symbol, Function("p", [Number(2)]))
        self.assertTrue(p2.match("p", 1, True))
        self.assertFalse(p2.match("p", 2, True))

        p3 = cast(SymbolicAtom, atoms[Function("p", [Number(3)])])
        self.assertIsNotNone(p3)
        self.assertFalse(p3.is_fact)
        self.assertTrue(p3.is_external)
        self.assertTrue(p3.literal >= 2)
        self.assertEqual(p3.symbol, Function("p", [Number(3)]))
        self.assertTrue(p3.match("p", 1, True))
        self.assertFalse(p3.match("p", 2, True))

        p4 = atoms[Function("p", [Number(4)])]
        self.assertIsNone(p4)

    def test_symbolic_atoms(self):
        """
        Test symbolic atoms.
        """
        self.ctl.add(
            "base",
            [],
            "p(1). {p(2)}. #external p(3). q(1). -p(1). {q(2)}. #external q(3).",
        )
        self.ctl.ground([("base", [])])

        atoms = self.ctl.symbolic_atoms
        self.assertEqual(
            sorted(atoms.signatures), [("p", 1, False), ("p", 1, True), ("q", 1, True)]
        )

        ps = list(atoms.by_signature("p", 1, True))
        self.assertEqual(len(ps), 3)
        for p in ps:
            self.assertEqual(p.symbol.name, "p")
            self.assertTrue(p.symbol.positive)
            self.assertEqual(len(p.symbol.arguments), 1)

        nps = list(atoms.by_signature("p", 1, False))
        self.assertEqual(len(nps), 1)
        for p in nps:
            self.assertEqual(p.symbol.name, "p")
            self.assertTrue(p.symbol.negative)
            self.assertEqual(p.symbol.arguments, [Number(1)])

        self.assertEqual(len(atoms), 7)
        self.assertEqual(len(list(atoms)), 7)

        self.assertIn(Function("p", [Number(1)], True), atoms)
        self.assertIn(Function("p", [Number(1)], False), atoms)
        self.assertIn(Function("q", [Number(2)], True), atoms)
        self.assertNotIn(Function("q", [Number(2)], False), atoms)

    def test_theory_term(self):
        """
        Test theory term.
        """
        self.ctl.add("base", [], THEORY)
        self.ctl.add("base", [], "&a { 1,a,f(a),{1},(1,),[1] }.")
        self.ctl.ground([("base", [])])

        terms = next(self.ctl.theory_atoms).elements[0].terms
        self.assertEqual(
            [str(term) for term in terms], ["1", "a", "f(a)", "{1}", "(1,)", "[1]"]
        )
        num, sym, fun, set_, tup, lst = terms
        self.assertEqual(num.type, TheoryTermType.Number)
        self.assertEqual(num.number, 1)
        self.assertEqual(sym.type, TheoryTermType.Symbol)
        self.assertEqual(sym.name, "a")
        self.assertEqual(fun.type, TheoryTermType.Function)
        self.assertEqual(fun.name, "f")
        self.assertEqual(fun.arguments, [sym])
        self.assertEqual(set_.type, TheoryTermType.Set)
        self.assertEqual(set_.arguments, [num])
        self.assertEqual(tup.type, TheoryTermType.Tuple)
        self.assertEqual(tup.arguments, [num])
        self.assertEqual(lst.type, TheoryTermType.List)
        self.assertEqual(lst.arguments, [num])

        self.assertNotEqual(hash(num), hash(sym))
        self.assertEqual(hash(num), hash(lst.arguments[0]))
        self.assertNotEqual(num < sym, sym < num)

        self.assertRegex(repr(num), "TheoryTerm(.*)")

    def test_theory_element(self):
        """
        Test theory element.
        """
        self.ctl.add("base", [], THEORY)
        self.ctl.add("base", [], "{a; b}.")
        self.ctl.add("base", [], "&a { 1; 2,3: a,b }.")
        self.ctl.ground([("base", [])])

        atom = next(self.ctl.theory_atoms)
        elements = sorted(atom.elements, key=lambda elem: len(elem.terms))
        self.assertEqual([str(elem) for elem in elements], ["1", "2,3: a,b"])

        a, b = elements
        self.assertEqual(len(a.condition), 0)
        self.assertEqual(len(b.condition), 2)
        self.assertTrue(all(lit >= 1 for lit in b.condition))
        self.assertGreaterEqual(b.condition_id, 1)

        self.assertEqual(a, a)
        self.assertNotEqual(a, b)
        self.assertNotEqual(hash(a), hash(b))
        self.assertNotEqual(a < b, b < a)

        self.assertRegex(repr(a), "TheoryElement(.*)")

    def test_theory_atom(self):
        """
        Test theory atom.
        """
        self.ctl.add("base", [], THEORY)
        self.ctl.add("base", [], "&a {}.")
        self.ctl.add("base", [], "&b {} = 1.")
        self.ctl.ground([("base", [])])

        atoms = sorted(list(self.ctl.theory_atoms), key=lambda atom: atom.term.name)
        self.assertEqual([str(atom) for atom in atoms], ["&a{}", "&b{}=1"])

        a, b = atoms
        self.assertTrue(a.literal >= 1)
        self.assertIsNone(a.guard)
        self.assertIsNotNone(b.guard)
        self.assertEqual(b.guard[0], "=")
        self.assertEqual(str(b.guard[1]), "1")
        self.assertEqual(len(a.elements), 0)

        self.assertEqual(a, a)
        self.assertNotEqual(a, b)
        self.assertNotEqual(hash(a), hash(b))
        self.assertNotEqual(a < b, b < a)

        self.assertRegex(repr(a), "TheoryAtom(.*)")
