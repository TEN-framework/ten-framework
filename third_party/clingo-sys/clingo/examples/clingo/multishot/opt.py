"""
Exmaple to show branch and bound based optimization using multi-shot solving.
"""

import sys
from typing import Optional, Sequence, cast

from clingo.application import Application, clingo_main
from clingo.control import Control
from clingo.solving import Model, SolveResult
from clingo.symbol import Number, SymbolType


class OptApp(Application):
    """
    Example application.
    """

    program_name: str = "opt-example"
    version: str = "1.0"
    _bound: Optional[int]

    def __init__(self):
        self._bound = None

    def _on_model(self, model: Model):
        self._bound = 0
        for atom in model.symbols(atoms=True):
            if (
                atom.match("_minimize", 2)
                and atom.arguments[0].type is SymbolType.Number
            ):
                self._bound += atom.arguments[0].number

    def main(self, ctl: Control, files: Sequence[str]):
        """
        Main function implementing branch and bound optimization.
        """
        if not files:
            files = ["-"]
        for file_ in files:
            ctl.load(file_)
        ctl.add("bound", ["b"], ":- #sum { V,I: _minimize(V,I) } >= b.")

        ctl.ground([("base", [])])
        while cast(SolveResult, ctl.solve(on_model=self._on_model)).satisfiable:
            print("Found new bound: {}".format(self._bound))
            ctl.ground([("bound", [Number(cast(int, self._bound))])])

        if self._bound is not None:
            print("Optimum found")


clingo_main(OptApp(), sys.argv[1:])
