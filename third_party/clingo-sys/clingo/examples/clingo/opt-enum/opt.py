"""
Simple example showing how to resume optimization to enumerate all solutions in
lexicographical order where costs are determined by the minimize constraints.

The program simply adds weight constraints ensuring that the next best
solutions, after all optimal solutions have been enumerated, are enumerated.
This process is repeated until the program becomes unsatisfiable.
"""

import sys
from typing import Dict, List, Optional, Sequence, Tuple, cast

from clingo.application import Application, ApplicationOptions, Flag, clingo_main
from clingo.backend import Backend, Observer
from clingo.configuration import Configuration
from clingo.control import Control
from clingo.propagator import Assignment, PropagateControl, Propagator
from clingo.solving import Model, SolveResult
from clingo.statistics import StatisticsMap


class MinObs(Observer):
    """
    Observer to extract ground minimize constraint.
    """

    literals: Dict[int, List[Tuple[int, int]]]

    def __init__(self):
        self.literals = {}

    def minimize(self, priority: int, literals: Sequence[Tuple[int, int]]):
        """
        Intercept minimize constraint and add it to member `literals`.
        """
        self.literals.setdefault(priority, []).extend(literals)


class RestoreHeu(Propagator):
    """
    Heuristic to restore solutions when resuming optimal model enumeration.
    """

    costs: Dict[Tuple[int], Sequence[int]]
    models: Dict[int, Sequence[int]]
    restore: Optional[Sequence[int]]

    def __init__(self):
        self.costs = {}
        self.models = {}
        self.restore = None

    def set_restore(self, bound: Tuple[int]):
        """
        Set the best previously found solution with a cost worse than bound to
        restore.
        """
        min_costs = bound
        for costs, model in self.costs.items():
            if bound < costs < min_costs:
                min_costs = costs
                self.restore = model

    def on_model(self, model: Model):
        """
        If a model has been found store the corresponding bound/solution.
        """
        idx = cast(Tuple[int], tuple(model.cost))
        self.costs[idx] = self.models[model.thread_id]

    def check(self, control: PropagateControl):
        """
        Store the last solution found by a thread.
        """
        self.models[control.thread_id] = list(control.assignment.trail)
        self.restore = None

    def decide(self, thread_id: int, assignment: Assignment, fallback: int) -> int:
        """
        Either restores the last solution or falls back to the solvers
        heuristic.
        """
        if self.restore:
            for lit in self.restore:
                if assignment.is_free(lit):
                    return lit
        return fallback


class OptApt(Application):
    """
    Application class implementing optimal model enumeration.
    """

    _restore: Flag
    _aux_level: Dict[Tuple[int, int], int]
    _heu: Optional[RestoreHeu]
    _proven: int
    _intermediate: int

    def __init__(self):
        self._restore = Flag()
        self._aux_level = {}
        self._proven = 0
        self._intermediate = 0
        self._heu = None

    def register_options(self, options: ApplicationOptions):
        """
        Register enumeration specific heuristics.
        """
        options.add_flag(
            "Enumerate",
            "restore",
            "heuristically restore last solution when resuming optimization",
            self._restore,
        )

    def _add_upper_bound(
        self,
        backend: Backend,
        wlits: Sequence[Tuple[int, int]],
        bound: int,
        level: Optional[int],
    ):
        """
        Adds the constraint `a <> { wlits } < bound` and returns literal `a`.

        If level is None, then an integrity constraint is added and no
        auxiliary literal is introduced.

        This function reuses literals introduced in earlier iterations.
        """
        hd = []
        if level is not None:
            if (level, bound) in self._aux_level:
                return self._aux_level[(level, bound)]
            hd.append(backend.add_atom())
            self._aux_level[(level, bound)] = hd[0]

        lower = -bound
        wlits_lower = []
        for l, w in wlits:
            if w > 0:
                lower += w
                l = -l
            else:
                w = -w
            wlits_lower.append((l, w))

        backend.add_weight_rule(hd, lower, wlits_lower)
        return hd[0] if hd else None

    def _set_upper_bound(
        self,
        backend: Backend,
        minimize: Sequence[Sequence[Tuple[int, int]]],
        bound: Sequence[int],
    ):
        """
        Adds constraints discarding solutions lexicographically smaller or
        equal than the bound.

        The weighted literals in the minimize variable directly correspond to
        how the solver represents minimize constraints.
        """
        assert minimize and len(minimize) == len(bound)
        if len(minimize) == 1:
            self._add_upper_bound(backend, minimize[0], bound[0], None)
        else:
            # Note: we could also introduce a chain. But then there are
            # typically few priorities and this should resolve nicely.
            # :- l0 <= b0-1
            # :- l0 <= b0 && l1 <= b1-1
            # :- l0 <= b0 && l1 <= b1 && l2 <= b2-1
            # ...
            # :- l0 <= b0 && l1 <= b1 && l2 <= b2 && ... && ln <= bn
            prefix = []
            for i, (wlits, value) in enumerate(zip(minimize, bound)):
                if i == len(minimize) - 1:
                    prefix.append(self._add_upper_bound(backend, wlits, value, i))
                    backend.add_rule([], prefix)
                else:
                    prefix.append(self._add_upper_bound(backend, wlits, value - 1, i))
                    backend.add_rule([], prefix)
                    prefix[-1] = self._add_upper_bound(backend, wlits, value, i)

    def _on_model(self, model: Model) -> bool:
        """
        Intercept models.

        This function counts optimal and intermediate models as well as passes
        model to the restore heuristic.
        """
        if self._heu:
            self._heu.on_model(model)
        if model.optimality_proven:
            self._proven += 1
        else:
            self._intermediate += 1
        return True

    def _on_statistics(self, step: StatisticsMap, accu: StatisticsMap):
        """
        Sets optimization specific statistics.
        """
        # pylint: disable=unused-argument
        accu.update(
            {
                "Enumerate": {
                    "Enumerated": self._proven,
                    "Intermediate": self._intermediate,
                }
            }
        )

    def _optimize(self, control: Control):
        """
        Run optimal solution enumeration algorithm.
        """
        obs = MinObs()
        control.register_observer(obs)

        if self._restore:
            self._heu = RestoreHeu()
            control.register_propagator(self._heu)

        control.ground([("base", [])])
        res = cast(
            SolveResult,
            control.solve(on_model=self._on_model, on_statistics=self._on_statistics),
        )

        solve_config = cast(Configuration, control.configuration.solve)

        num_models = int(cast(str, solve_config.models))

        minimize = [x[1] for x in sorted(obs.literals.items(), key=lambda x: -x[0])]

        while (
            res.satisfiable
            and not res.interrupted
            and minimize
            and "costs" in control.statistics["summary"]
        ):
            summary = control.statistics["summary"]
            if num_models > 0:
                num_models -= int(summary["models"]["optimal"])
                if num_models <= 0:
                    break
                solve_config.models = num_models

            costs = cast(
                Tuple[int],
                tuple(int(x) for x in control.statistics["summary"]["costs"]),
            )
            with control.backend() as backend:
                self._set_upper_bound(backend, minimize, costs)

            if self._heu is not None:
                self._heu.set_restore(costs)

            res = cast(
                SolveResult,
                control.solve(
                    on_model=self._on_model, on_statistics=self._on_statistics
                ),
            )

    def main(self, control: Control, files: Sequence[str]):
        """
        Runs the main ground/solve algorithm.
        """
        for file_ in files:
            control.load(file_)
        if not files:
            control.load("-")

        solve_config = cast(Configuration, control.configuration.solve)

        if solve_config.opt_mode == "optN":
            self._optimize(control)
        else:
            control.ground([("base", [])])
            control.solve()


sys.exit(clingo_main(OptApt()))
