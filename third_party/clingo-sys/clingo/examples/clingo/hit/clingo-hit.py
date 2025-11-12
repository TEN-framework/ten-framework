#!/usr/bin/env python3
"""
Extends clingo with simple hitting set based optimization.
"""

import sys
from collections.abc import Callable, KeysView, Sequence
from typing import cast

from clingo.application import Application, ApplicationOptions, Flag, clingo_main
from clingo.backend import Observer
from clingo.configuration import Configuration
from clingo.control import Control
from clingo.solving import Model

WeightedLit = tuple[int, int]
Minimize = dict[int, list[WeightedLit]]


class HSSolver:
    """
    An incremental hitting set solver.
    """

    _map: dict[int, int]
    _ctl: Control

    def __init__(self, mini: Minimize):
        """
        Initialize the solver.
        """
        self._ctl = Control(["0", "-t4"])
        self._map = {}
        with self._ctl.backend() as bck:
            for prio, wlits in mini.items():
                hs_wlits = []
                for lit, weight in wlits:
                    if -lit not in self._map:
                        hs_lit = bck.add_atom()
                        self._map[-lit] = hs_lit
                        bck.add_rule([hs_lit], [], True)
                    hs_wlits.append((self._map[-lit], weight))
                bck.add_minimize(prio, hs_wlits)

    def solve(self) -> list[int]:
        """
        Compute a hitting set.
        """
        hs: list[int] = []

        def extract(model: Model):
            hs.clear()
            for lit, hs_lit in self._map.items():
                if model.is_true(hs_lit):
                    hs.append(lit)

        self._ctl.solve(on_model=extract)
        return hs

    def lits(self) -> KeysView[int]:
        """
        Get the literals subject to minimization.
        """
        return self._map.keys()

    def add(self, core: Sequence[int]) -> None:
        """
        Add a core.
        """
        with self._ctl.backend() as bck:
            bck.add_rule([], [-self._map[lit] for lit in core])


class Obs(Observer):
    """
    Observer to extract a minimize constraint.
    """

    _mini: Minimize

    def __init__(self, mini: Minimize):
        self._mini = mini

    def minimize(self, priority: int, literals: Sequence[WeightedLit]) -> None:
        """
        Gather minimize constraints in the program.
        """
        self._mini.setdefault(priority, []).extend(literals)


class OptApt(Application):
    """
    Application class implementing hitting set based minimization.
    """

    _trace: Flag

    program_name = "clingo-hit"

    def __init__(self):
        self._trace = Flag()

    def register_options(self, options: ApplicationOptions) -> None:
        """
        Register clingo-hit options.
        """
        options.add_flag(
            "Clingo.Hit", "--trace", "trace minimization progress", self._trace
        )

    def main(self, control: Control, files: Sequence[str]) -> None:
        """
        Runs the minimization algorithm.
        """
        cast(Configuration, control.configuration.solve).opt_mode = "ignore"

        for file_ in files:
            control.load(file_)
        if not files:
            control.load("-")

        mini: Minimize = {}
        obs = Obs(mini)
        control.register_observer(obs, False)
        control.ground([("base", [])])

        cores: list[list[int]] = []
        hss = HSSolver(mini)

        while True:
            hs = hss.solve()
            assume = [lit for lit in hss.lits() if lit not in hs]
            if self._trace:
                print("........................")
                print("cores:", cores)
                print("hitting set:", hs)
                print("assumptions:", assume)
            res = control.solve(
                assumptions=assume,
                on_core=cast(Callable[[Sequence[int]], None], cores.append),
            )
            if res.satisfiable:
                break
            if not cores[-1]:
                break
            hss.add(cores[-1])


sys.exit(clingo_main(OptApt(), sys.argv[1:]))
