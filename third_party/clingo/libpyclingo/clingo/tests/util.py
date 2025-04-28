"""
Helpers to simplify testing.
"""

from typing import List, cast
from unittest import TestCase

from ..control import Control
from ..configuration import Configuration
from .. import SolveResult, parse_term


def _p(*models):
    return [[parse_term(symbol) for symbol in model] for model in models]


class _MCB:
    # pylint: disable=missing-function-docstring
    def __init__(self):
        self._models = []
        self._core = None
        self.last = None

    def on_core(self, c):
        self._core = c

    def on_model(self, m):
        self.last = (m.type, sorted(m.symbols(shown=True)))
        self._models.append(self.last[1])

    @property
    def core(self):
        return sorted(self._core)

    @property
    def models(self):
        return sorted(self._models)


def _check_sat(case: TestCase, ret: SolveResult) -> None:
    case.assertTrue(ret.satisfiable is True)
    case.assertTrue(ret.unsatisfiable is False)
    case.assertTrue(ret.unknown is False)
    case.assertTrue(ret.exhausted is True)


def solve(ctl: Control) -> List[List[str]]:
    """
    Solve with the given control ojbect and return models as sorted lists of
    strings.
    """
    cast(Configuration, ctl.configuration.solve).models = "0"
    mcb = _MCB()
    ctl.solve(on_model=mcb.on_model)
    return [[str(s) for s in m] for m in mcb.models]
