"""
Example guess and check based solver for second level problems.
"""

import sys
from typing import Sequence, Tuple, List, cast

from clingo import ast
from clingo.solving import SolveResult
from clingo.ast import ProgramBuilder
from clingo.control import Control
from clingo.application import clingo_main, Application
from clingo.propagator import PropagateControl, PropagateInit, Propagator
from clingo.backend import Backend
from clingo.ast import parse_files, AST, ASTType


class Transformer:
    """
    Transformer for the guess and check solver.
    """
    _builder: ProgramBuilder
    _state: str
    _check: List[AST]

    def __init__(self, builder: ProgramBuilder, check: List[AST]):
        self._builder = builder
        self._state = "guess"
        self._check = check

    def add(self, stm: AST):
        """
        Add the given statement to the guess or check programs.
        """
        if stm.ast_type == ASTType.Program:
            if stm.name == "check" and not stm.parameters:
                self._state = "check"
            elif (stm.name == "base" or stm.name == "guess") and not stm.parameters:
                self._state = "guess"
            else:
                raise RuntimeError("unexpected program part")

        else:
            if self._state == "guess":
                self._builder.add(stm)
            else:
                self._check.append(stm)


class Checker:
    """
    Class wrapping a solver to perform the second level check.
    """
    _ctl: Control
    _map: List[Tuple[int, int]]

    def __init__(self):
        self._ctl = Control()
        self._map = []

    def backend(self) -> Backend:
        """
        Return the backend of the underlying solver.
        """
        return self._ctl.backend()

    def add(self, guess_lit: int, check_lit: int):
        """
        Map the given solver literal to the corresponding program literal in
        the checker.
        """
        self._map.append((guess_lit, check_lit))

    def ground(self, check: Sequence[ast.AST]):
        """
        Ground the check program.
        """
        with ProgramBuilder(self._ctl) as bld:
            for stm in check:
                bld.add(stm)

        self._ctl.ground([("base", [])])

    def check(self, control: PropagateControl) -> bool:
        """
        Return true if the check program is unsatisfiable w.r.t. to the atoms
        of the guess program.

        The truth values of the atoms of the guess program are stored in the
        assignment of the given control object.
        """
        assignment = control.assignment

        assumptions = []
        for guess_lit, check_lit in self._map:
            guess_truth = assignment.value(guess_lit)
            assumptions.append(check_lit if guess_truth else -check_lit)

        ret = cast(SolveResult, self._ctl.solve(assumptions))
        if ret.unsatisfiable is not None:
            return ret.unsatisfiable

        raise RuntimeError("search interrupted")


class CheckPropagator(Propagator):
    """
    Simple propagator verifying that a check program holds on total
    assignments.
    """
    _check: List[ast.AST]
    _checkers: List[Checker]

    def __init__(self, check: List[ast.AST]):
        self._check = check
        self._checkers = []

    def init(self, init: PropagateInit):
        """
        Initialize the solvers for the check programs.
        """
        # we need a checker for each thread (to be able to solve in parallel)
        for _ in range(init.number_of_threads):
            checker = Checker()
            self._checkers.append(checker)

            with checker.backend() as backend:
                for atom in init.symbolic_atoms:
                    guess_lit = init.solver_literal(atom.literal)
                    guess_truth = init.assignment.value(guess_lit)

                    # ignore false atoms
                    if guess_truth is False:
                        continue

                    check_lit = backend.add_atom(atom.symbol)

                    # fix true atoms
                    if guess_truth is True:
                        backend.add_rule([check_lit], [])

                    # add a choice rule for unknow atoms and add them to the
                    # mapping table of the checker
                    else:
                        backend.add_rule([check_lit], [], True)
                        checker.add(guess_lit, check_lit)

            checker.ground(self._check)


    def check(self, control: PropagateControl):
        """
        Check total assignments.
        """
        assignment = control.assignment
        checker = self._checkers[control.thread_id]

        if not checker.check(control):
            conflict = []
            for level in range(1, assignment.decision_level+1):
                conflict.append(-assignment.decision(level))

            control.add_clause(conflict)


class GACApp(Application):
    """
    Application class implementing a custom solver.
    """
    program_name: str
    version: str

    def __init__(self):
        self.program_name = "guess-and-check"
        self.version = "1.0"

    def main(self, ctl: Control, files: Sequence[str]):
        """
        The main function called with a Control object and a list of files
        passed on the command line.
        """
        if not files:
            files = ["-"]

        check: List[ast.AST] = []
        with ProgramBuilder(ctl) as bld:
            trans = Transformer(bld, check)
            parse_files(files, trans.add)

        ctl.register_propagator(CheckPropagator(check))

        ctl.ground([("base", [])])
        ctl.solve()


sys.exit(clingo_main(GACApp(), sys.argv[1:]))
