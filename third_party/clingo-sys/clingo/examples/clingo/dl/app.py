"""
This is a scaled down version of clingo-dl show casing how to implement a
propagator for difference logic.
"""

import heapq
import sys
from typing import List, MutableMapping, Optional, Sequence, Set, Tuple, cast

from clingo import SolveResult, ast, parse_term
from clingo.application import Application, ApplicationOptions, clingo_main
from clingo.ast import AST, ProgramBuilder, Transformer, parse_files
from clingo.control import Control
from clingo.propagator import Assignment, PropagateControl, PropagateInit, Propagator
from clingo.solving import Model
from clingo.symbol import Function, Number, Symbol, SymbolType, Tuple_
from clingo.theory_atoms import TheoryTerm, TheoryTermType

Node = Symbol  # pylint: disable=invalid-name
Weight = int
Level = int
Edge = Tuple[Node, Node]
WeightedEdge = Tuple[Node, Node, Weight]
MapNodeWeight = MutableMapping[Node, Weight]

THEORY = """
#theory dl{
    diff_term {
    -  : 3, unary;
    ** : 2, binary, right;
    *  : 1, binary, left;
    /  : 1, binary, left;
    \\ : 1, binary, left;
    +  : 0, binary, left;
    -  : 0, binary, left
    };
    &diff/1 : diff_term, {<=}, diff_term, any
}.
"""

_BOP = {
    "+": lambda a, b: a + b,
    "-": lambda a, b: a - b,
    "*": lambda a, b: a * b,
    "**": lambda a, b: a**b,
    "\\": lambda a, b: a % b,
    "/": lambda a, b: a // b,
}


def _evaluate(term: TheoryTerm) -> Symbol:
    """
    Evaluates the operators in a theory term in the same fashion as clingo
    evaluates its arithmetic functions.
    """
    # tuples
    if term.type == TheoryTermType.Tuple:
        return Tuple_([_evaluate(x) for x in term.arguments])

    # functions and arithmetic operations
    if term.type == TheoryTermType.Function:
        # binary operations
        if term.name in _BOP and len(term.arguments) == 2:
            term_a = _evaluate(term.arguments[0])
            term_b = _evaluate(term.arguments[1])

            if term_a.type != SymbolType.Number or term_b.type != SymbolType.Number:
                raise RuntimeError("Invalid Binary Operation")

            if term.name in ("/", "\\") and term_b.number == 0:
                raise RuntimeError("Division by Zero")

            return Number(_BOP[term.name](term_a.number, term_b.number))

        # unary operations
        if term.name == "-" and len(term.arguments) == 1:
            term_a = _evaluate(term.arguments[0])

            if term_a.type == SymbolType.Number:
                return Number(-term_a.number)

            if term_a.type == SymbolType.Function and term_a.name:
                return Function(term_a.name, term_a.arguments, not term_a.positive)

            raise RuntimeError("Invalid Unary Operation")

        # functions
        return Function(term.name, [_evaluate(x) for x in term.arguments])

    # constants
    if term.type == TheoryTermType.Symbol:
        return Function(term.name)

    # numbers
    if term.type == TheoryTermType.Number:
        return Number(term.number)

    raise RuntimeError("Invalid Syntax")


class HeadBodyTransformer(Transformer):
    """
    Transformer to tag head and body occurrences of `&diff` atoms.
    """

    def visit_Literal(self, lit: AST, in_lit: bool = False) -> AST:
        """
        Visit literal; any theory atom in a literal is a body literal.
        """
        return lit.update(**self.visit_children(lit, True))

    def visit_TheoryAtom(self, atom: AST, in_lit: bool = False) -> AST:
        """
        Visit theory atom and tag as given by in_lit.
        """
        # pylint: disable=invalid-name,no-self-use
        term = atom.term
        if term.name == "diff" and not term.arguments:
            loc = "body" if in_lit else "head"
            atom.term = ast.Function(
                term.location,
                term.name,
                [ast.Function(term.location, loc, [], False)],
                False,
            )
        return atom


class Graph:
    """
    This class captures a graph with weighted edges that can be extended
    incrementally.

    Adding an edge triggers a cycle check that will report negative cycles.
    """

    _potential: MapNodeWeight
    _graph: MutableMapping[Node, MapNodeWeight]
    _gamma: MapNodeWeight
    _last_edges: MutableMapping[Node, WeightedEdge]
    _previous_edge: MutableMapping[Level, MutableMapping[Edge, Weight]]
    _previous_potential: MutableMapping[Level, MapNodeWeight]

    def __init__(self):
        self._potential = {}  # {node: potential}
        self._graph = {}  # {node: {node : weight}}
        self._gamma = {}  # {node: gamma}
        self._last_edges = {}  # {node: edge}
        self._previous_edge = {}  # {level: {(node, node): weight}}
        self._previous_potential = {}  # {level: {node: potential}}

    @staticmethod
    def _set(level, key, val, previous, get_current):
        p = previous.setdefault(level, {})
        c, k = get_current(key)
        if not key in p:
            p[key] = c[k] if k in c else None
        c[k] = val

    @staticmethod
    def _reset(level, previous, get_current):
        if level in previous:
            for key, val in previous[level].items():
                c, k = get_current(key)
                if val is None:
                    del c[k]
                else:
                    c[k] = val
            del previous[level]

    def _reset_edge(self, level: Level):
        self._reset(
            level, self._previous_edge, lambda key: (self._graph[key[0]], key[1])
        )

    def _reset_potential(self, level: Level):
        self._reset(level, self._previous_potential, lambda key: (self._potential, key))

    def _set_edge(self, level: Level, key: Edge, val: Weight):
        self._set(
            level,
            key,
            val,
            self._previous_edge,
            lambda key: (self._graph[key[0]], key[1]),
        )

    def _set_potential(self, level: Level, key: Node, val: Weight):
        self._set(
            level,
            key,
            val,
            self._previous_potential,
            lambda key: (self._potential, key),
        )

    def add_edge(
        self, level: Level, edge: WeightedEdge
    ) -> Optional[List[WeightedEdge]]:
        """
        Add an edge to the graph and return a negative cycle (if there is one).
        """
        u, v, d = edge
        # If edge already exists from u to v with lower weight, new edge is redundant
        if u in self._graph and v in self._graph[u] and self._graph[u][v] <= d:
            return None

        # Initialize potential and graph
        if u not in self._potential:
            self._set_potential(level, u, 0)
        if v not in self._potential:
            self._set_potential(level, v, 0)
        self._graph.setdefault(u, {})
        self._graph.setdefault(v, {})

        changed: Set[Node] = set()  # Set of nodes for which potential has been changed
        min_gamma: List[Tuple[Weight, Node]] = []

        # Update potential change induced by new edge, 0 for other nodes
        self._gamma[u] = 0
        self._gamma[v] = self._potential[u] + d - self._potential[v]

        if self._gamma[v] < 0:
            heapq.heappush(min_gamma, (self._gamma[v], v))
            self._last_edges[v] = (u, v, d)

        # Propagate negative potential change
        while len(min_gamma) > 0 and self._gamma[u] == 0:
            _, s = heapq.heappop(min_gamma)
            if s not in changed:
                self._set_potential(level, s, self._potential[s] + self._gamma[s])
                self._gamma[s] = 0
                changed.add(s)
                for t in self._graph[s]:
                    if t not in changed:
                        gamma_t = (
                            self._potential[s] + self._graph[s][t] - self._potential[t]
                        )
                        if gamma_t < self._gamma[t]:
                            self._gamma[t] = gamma_t
                            heapq.heappush(min_gamma, (gamma_t, t))
                            self._last_edges[t] = (s, t, self._graph[s][t])

        cycle = None
        # Check if there is a negative cycle
        if self._gamma[u] < 0:
            cycle = []
            x, y, c = self._last_edges[v]
            cycle.append((x, y, c))
            while v != x:
                x, y, c = self._last_edges[x]
                cycle.append((x, y, c))
        else:
            self._set_edge(level, (u, v), d)

        # Ensure that all gamma values are zero
        self._gamma[v] = 0
        while len(min_gamma) > 0:
            _, s = heapq.heappop(min_gamma)
            self._gamma[s] = 0

        return cycle

    def get_assignment(self) -> List[Tuple[Node, Weight]]:
        """
        Get the current assignment to integer variables.
        """
        zero = Number(0)
        adjust = self._potential[zero] if zero in self._potential else 0
        return [
            (node, adjust - self._potential[node])
            for node in self._potential
            if node != zero
        ]

    def backtrack(self, level):
        """
        Backtrack the given level.
        """
        self._reset_edge(level)
        self._reset_potential(level)


class DLPropagator(Propagator):
    """
    A propagator for difference constraints.
    """

    _l2e: MutableMapping[int, List[WeightedEdge]]
    _e2l: MutableMapping[WeightedEdge, List[int]]
    _states: List[Graph]

    def __init__(self):
        self._l2e = {}  # {literal: [(node, node, weight)]}
        self._e2l = {}  # {(node, node, weight): [literal]}
        self._states = []  # [Graph]

    def _add_edge(self, init: PropagateInit, lit: int, u: Node, v: Node, w: Weight):
        edge = (u, v, w)
        self._l2e.setdefault(lit, []).append(edge)
        self._e2l.setdefault(edge, []).append(lit)
        init.add_watch(lit)

    def init(self, init: PropagateInit):
        """
        Initialize the propagator extracting difference constraints from the
        theory data.
        """
        for atom in init.theory_atoms:
            term = atom.term
            if term.name == "diff" and len(term.arguments) == 1:
                assert atom.guard is not None
                u = _evaluate(atom.elements[0].terms[0].arguments[0])
                v = _evaluate(atom.elements[0].terms[0].arguments[1])
                w = _evaluate(atom.guard[1]).number
                lit = init.solver_literal(atom.literal)
                self._add_edge(init, lit, u, v, w)
                if term.arguments[0].name == "body":
                    self._add_edge(init, -lit, v, u, -w - 1)

    def propagate(self, control: PropagateControl, changes: Sequence[int]):
        """
        Add edges that became true to the graph to check for negative cycles.
        """
        state = self._state(control.thread_id)
        level = control.assignment.decision_level
        for lit in changes:
            for edge in self._l2e[lit]:
                cycle = state.add_edge(level, edge)
                if cycle is not None:
                    c = [self._literal(control, e) for e in cycle]
                    if control.add_nogood(c):
                        control.propagate()
                    return

    def undo(self, thread_id: int, assign: Assignment, changes: Sequence[int]):
        """
        Backtrack the last decision level propagated.
        """
        # pylint: disable=unused-argument
        self._state(thread_id).backtrack(assign.decision_level)

    def on_model(self, model: Model):
        """
        This function should be called when a model has been found to extend it
        with the integer variable assignments.
        """
        assignment = self._state(model.thread_id).get_assignment()
        model.extend(
            [Function("dl", [var, Number(value)]) for var, value in assignment]
        )

    def _state(self, thread_id: int) -> Graph:
        while len(self._states) <= thread_id:
            self._states.append(Graph())
        return self._states[thread_id]

    def _literal(self, control, edge):
        for lit in self._e2l[edge]:
            if control.assignment.is_true(lit):
                return lit
        raise RuntimeError("must not happen")


class DLApp(Application):
    """
    Application extending clingo with difference constraints.
    """

    program_name: str = "clingo-dl"
    version: str = "1.0"

    _propagator: DLPropagator
    _minimize: Optional[Symbol]
    _bound: Optional[int]

    def __init__(self):
        self._propagator = DLPropagator()
        self._minimize = None
        self._bound = None

    def _parse_minimize(self, val):
        var = parse_term(val)

        if var.type == SymbolType.Number:
            return False

        self._minimize = var
        return True

    def register_options(self, options: ApplicationOptions):
        """
        Register application options.
        """
        group = "Clingo.DL Options"
        options.add(
            group,
            "minimize-variable",
            "Minimize the given variable",
            self._parse_minimize,
            argument="<var>",
        )

    def _read(self, path: str):
        if path == "-":
            return sys.stdin.read()
        with open(path) as file_:
            return file_.read()

    def _rewrite(self, ctl: Control, files: Sequence[str]):
        with ProgramBuilder(ctl) as bld:
            hbt = HeadBodyTransformer()
            parse_files(files, lambda stm: bld.add(cast(AST, hbt.visit(stm))))

    def _on_model(self, model: Model):
        self._propagator.on_model(model)
        for symbol in model.symbols(theory=True):
            if symbol.match("dl", 2):
                n, v = symbol.arguments
                if n == self._minimize:
                    self._bound = v.number
                    break

    def main(self, ctl: Control, files: Sequence[str]):
        """
        Register the difference constraint propagator, and then ground and
        solve.
        """
        ctl.register_propagator(self._propagator)
        ctl.add("base", [], THEORY)

        if not files:
            files = ["-"]
        self._rewrite(ctl, files)

        ctl.ground([("base", [])])
        if self._minimize is None:
            ctl.solve(on_model=self._propagator.on_model)
        else:
            ctl.add("bound", ["b", "v"], "&diff(head) { v-0 } <= b.")

            while cast(SolveResult, ctl.solve(on_model=self._on_model)).satisfiable:
                print("Found new bound: {}".format(self._bound))
                if self._bound is None:
                    break
                ctl.ground(
                    [("bound", [Number(cast(int, self._bound) - 1), self._minimize])]
                )

            if self._bound is not None:
                print("Optimum found")


if __name__ == "__main__":
    sys.exit(int(clingo_main(DLApp(), sys.argv[1:])))
