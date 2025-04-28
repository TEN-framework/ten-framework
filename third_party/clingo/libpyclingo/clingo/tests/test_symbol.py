"""
Test clingo's Symbol class.
"""
from unittest import TestCase

from clingo import (
    Function,
    Infimum,
    Number,
    String,
    Supremum,
    SymbolType,
    Tuple_,
    parse_term,
)


class TestSymbol(TestCase):
    """
    Tests for the symbol class.
    """

    def test_parse(self):
        """
        Test term parsing.
        """
        self.assertEqual(str(parse_term("p(3)")), "p(3)")
        log = []
        self.assertRaises(
            RuntimeError,
            parse_term,
            "p(1/0)",
            lambda code, message: log.append((code, message)),
        )
        self.assertEqual(len(log), 0)

    def test_str(self):
        """
        Test string representation of symbols.
        """
        self.assertEqual(str(Number(10)), "10")

    def test_repr(self):
        """
        Test representation of symbols.
        """
        self.assertEqual(
            repr(
                Function("test", [Number(10), Infimum, Supremum, String("test")], False)
            ),
            "Function('test', [Number(10), Infimum, Supremum, String('test')], False)",
        )

    def test_cmp(self):
        """
        Test hashing and comparisons.
        """
        self.assertEqual(hash(Number(10)), hash(Number(10)))
        self.assertNotEqual(hash(Number(1)), hash(Number(2)))
        self.assertTrue(Number(10) == Number(10))
        self.assertTrue(Number(1) != Number(2))
        self.assertTrue(Number(1) < Number(2))
        self.assertFalse(Number(2) < Number(1))
        self.assertTrue(Number(2) > Number(1))
        self.assertFalse(Number(1) > Number(2))

    def test_match(self):
        """
        Test symbol matching.
        """
        self.assertTrue(Function("f", [Number(1)]).match("f", 1))
        self.assertFalse(Function("f", [Number(1), Number(2)]).match("f", 1))
        self.assertFalse(Number(1).match("f", 1))

    def test_number(self):
        """
        Test numbers.
        """
        n = Number(1)
        self.assertEqual(n.number, 1)
        self.assertEqual(n.type, SymbolType.Number)

    def test_function(self):
        """
        Test functions.
        """
        f = Function("f", [Number(1)], False)
        self.assertEqual(f.arguments, [Number(1)])
        self.assertFalse(f.positive)
        self.assertTrue(f.negative)
        self.assertEqual(f.name, "f")
        self.assertEqual(f.type, SymbolType.Function)
        self.assertEqual(Tuple_([]).type, SymbolType.Function)

    def test_infsup(self):
        """
        Test infimum and supremum.
        """
        self.assertEqual(Infimum.type, SymbolType.Infimum)
        self.assertEqual(Supremum.type, SymbolType.Supremum)

    def test_string(self):
        """
        Test strings.
        """
        s = String("blub")
        self.assertEqual(s.string, "blub")
        self.assertEqual(s.type, SymbolType.String)
