"""
Tests for configuration and statistics.
"""

from unittest import TestCase
from typing import cast
from clingo import Configuration, Control


class TestConfig(TestCase):
    """
    Tests for configuration and statistics.
    """

    def test_config(self):
        """
        Test configuration.
        """
        ctl = Control(["-t", "2"])
        self.assertIn("solver", ctl.configuration.keys)
        self.assertEqual(len(ctl.configuration.solver), 2)
        self.assertIsInstance(ctl.configuration.solver[0], Configuration)
        conf = cast(Configuration, ctl.configuration.solver[0])
        self.assertIsInstance(conf.heuristic, str)
        self.assertIsInstance(conf.description("heuristic"), str)
        conf.heuristic = "berkmin"
        self.assertTrue(conf.heuristic.startswith("berkmin"))

    def test_simple_stats(self):
        """
        Test simple statistics.
        """
        ctl = Control(["-t", "2", "--stats=2"])
        ctl.add("base", [], "1 { a; b }.")
        ctl.ground([("base", [])])
        ctl.solve()
        stats = ctl.statistics
        self.assertGreaterEqual(stats["problem"]["lp"]["atoms"], 2)
        self.assertGreaterEqual(stats["solving"]["solvers"]["choices"], 1)

    def test_user_stats(self):
        """
        Test user statistics.
        """

        def on_statistics(step, accu):
            step["test"] = {"a": 0, "b": [1, 2], "c": {"d": 3}}
            accu["test"] = step["test"]
            step["test"] = {
                "a": lambda a: a + 1,
                "e": lambda a: 4 if a is None else 0,
                "b": [-1, 2, 3],
            }
            self.assertEqual(len(step["test"]), 4)
            self.assertEqual(len(step["test"]["b"]), 3)
            self.assertEqual(len(step["test"]["c"]), 1)
            self.assertIn("a", step["test"])
            self.assertEqual(sorted(step["test"]), ["a", "b", "c", "e"])
            self.assertEqual(sorted(step["test"].keys()), ["a", "b", "c", "e"])
            self.assertEqual(sorted(step["test"]["c"].items()), [("d", 3.0)])
            self.assertEqual(sorted(step["test"]["c"].values()), [3.0])

            step["test"]["b"][1] = 99
            self.assertEqual(step["test"]["b"][1], 99)
            step["test"]["b"].extend([3, 4])
            step["test"]["b"] += [3, 4]

        ctl = Control(["-t", "2", "--stats=2"])
        ctl.add("base", [], "1 { a; b }.")
        ctl.ground([("base", [])])
        ctl.solve(on_statistics=on_statistics)
        stats = ctl.statistics
        self.assertEqual(
            stats["user_step"]["test"],
            {
                "a": 1.0,
                "b": [-1.0, 99.0, 3.0, 3.0, 4.0, 3.0, 4.0],
                "c": {"d": 3.0},
                "e": 4.0,
            },
        )
        self.assertEqual(
            stats["user_accu"]["test"], {"a": 0, "b": [1, 2], "c": {"d": 3}}
        )
