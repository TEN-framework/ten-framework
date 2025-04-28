import sys
from typing import Deque, Dict, Set, List, Tuple
from collections import deque

from networkx import DiGraph # type: ignore
from networkx.algorithms.components import strongly_connected_components # type: ignore
from networkx.algorithms.dag import topological_sort # type: ignore

from clingo.control import Control
from clingo.symbol import Symbol
from clingo.application import Application, clingo_main
from clingox.program import Program, ProgramObserver, Rule

Atom = int
Literal = int
RuleIndex = int


def _analyze(rules: List[Rule]) -> List[List[Rule]]:
    # build rule dependency graph
    occ: Dict[Atom, Set[RuleIndex]] = {}
    dep_graph = DiGraph()
    for u, rule in enumerate(rules):
        dep_graph.add_node(u)
        for lit in rule.body:
            occ.setdefault(abs(lit), set()).add(u)

    for u, rule in enumerate(rules):
        atm, = rule.head
        for v in occ.get(atm, []):
            dep_graph.add_edge(u, v)

    sccs = list(strongly_connected_components(dep_graph))

    # build scc dependency graph
    # (this part only exists because the networkx library does not document the
    # order of components; in principle, the tarjan algorithm guarentees a
    # topological order)
    scc_rule: Dict[RuleIndex, RuleIndex] = {}
    scc_graph = DiGraph()
    for i, scc in enumerate(sccs):
        scc_graph.add_node(i)
        for u in scc:
            scc_rule[u] = i

    for i, scc in enumerate(sccs):
        for u in scc:
            for v in dep_graph[u]:
                j = scc_rule[u]
                if i != j:
                    scc_graph.add_edge(i, j)

    return [[rules[j] for j in sccs[i]] for i in topological_sort(scc_graph)]


def _well_founded(interpretation: Set[Literal], scc: List[Rule]) -> None:
    watches: Dict[Literal, List[RuleIndex]] = {}
    counters: Dict[RuleIndex, int] = {}
    todo: List[Literal] = []
    unfounded: List[Literal] = []
    need_source: Set[Atom] = set()
    has_source: Set[Atom] = set()
    can_source: Dict[Atom, List[RuleIndex]] = {}
    counters_source: Dict[RuleIndex, int] = dict()
    todo_source: Deque[Atom] = deque()
    is_source: Set[RuleIndex] = set()

    def is_true(*args):
        return all(lit in interpretation for lit in args)

    def is_false(*args):
        return any(-lit in interpretation for lit in args)

    def is_supported(lit):
        return not is_false(lit) and (lit < 0 or is_true(lit) or lit in has_source)

    def enqueue_source(idx: RuleIndex):
        atm, = scc[idx].head
        if counters_source[idx] == 0 and atm not in has_source:
            has_source.add(atm)
            is_source.add(idx)
            todo_source.append(atm)

    def enqueue_lit(lit: Literal):
        if lit not in interpretation:
            interpretation.add(lit)
            todo.append(lit)

    # initialize the above data structures
    for i, rule in enumerate(scc):
        atm, = rule.head

        if is_false(*rule.body) or is_true(atm):
            continue

        # initialize fact propagation
        count = 0
        for lit in rule.body:
            if not is_true(lit):
                count += 1
                watches.setdefault(lit, []).append(i)
        counters[i] = count
        if count == 0:
            enqueue_lit(atm)

        # initialize source propagation
        count = 0
        for lit in rule.body:
            if not is_supported(lit):
                count += 1
            if abs(lit) not in need_source:
                need_source.add(abs(lit))
                unfounded.append(-abs(lit))
        counters_source[i] = count
        enqueue_source(i)
        can_source.setdefault(atm, []).append(i)

    while todo or unfounded:
        # forward propagate facts
        idx = 0
        while idx < len(todo):
            lit = todo[idx]
            idx += 1
            for i in watches.get(lit, []):
                counters[i] -= 1
                if counters[i] == 0:
                    enqueue_lit(*scc[i].head)

        # remove sources
        idx = 0
        while idx < len(todo):
            lit = todo[idx]
            idx += 1
            # Note that in this case, the literal already lost its source earlier
            # and has already been made false at the end of the loop.
            if lit < 0 and lit in interpretation:
                continue
            for i in watches.get(-lit, []):
                counters_source[i] += 1
                if i in is_source:
                    atm, = scc[i].head
                    is_source.remove(i)
                    has_source.remove(atm)
                    if -atm not in interpretation:
                        todo.append(-atm)

        # initialize sources
        for lit in todo:
            for i in can_source.get(-lit, []):
                enqueue_source(i)

        # forward propagate sources
        while todo_source:
            atm = todo_source.popleft()
            for i in watches.get(atm, []):
                counters_source[i] -= 1
                enqueue_source(i)

        # set literals without sources to false
        if not unfounded:
            unfounded, todo = todo, unfounded
        todo.clear()
        for lit in unfounded:
            if lit < 0 and -lit in need_source and -lit not in has_source:
                enqueue_lit(lit)
        unfounded.clear()


def well_founded(prg: Program) -> Tuple[List[Symbol], List[Symbol]]:
    '''
    Computes the well-founded model of the given program returning a pair of
    facts and unknown atoms.

    This function assumes that the program contains only normal rules.
    '''
    for rule in prg.rules:
        if len(rule.head) != 1 or rule.choice:
            raise RuntimeError('only normal rules are supported')
    if prg.weight_rules:
        raise RuntimeError('only normal rules are supported')

    # analyze program and compute well-founded model
    interpretation: Set[Literal] = set()
    for scc in _analyze(prg.rules):
        _well_founded(interpretation, scc)

    # compute facts
    fct = [atm.symbol for atm in prg.facts]
    fct.extend(prg.output_atoms[lit] for lit in interpretation if lit > 0 and lit in prg.output_atoms)
    # compute unknowns
    ukn = set()
    for rule in prg.rules:
        atm, = rule.head
        not_false = any(-lit in interpretation for lit in rule.body)
        if atm not in interpretation and not not_false and atm in prg.output_atoms:
            ukn.add(prg.output_atoms[atm])
    return sorted(fct), sorted(ukn)


class LevelApp(Application):
    def __init__(self):
        self.program_name = "level"
        self.version = "1.0"

    def main(self, ctl: Control, files):
        prg = Program()
        ctl.register_observer(ProgramObserver(prg))

        for f in files:
            ctl.load(f)
        if not files:
            ctl.load('-')

        ctl.ground([("base", [])])

        fct, ukn = well_founded(prg)
        print('Facts:')
        print(f'{" ".join(map(str, fct))}')
        print('Unknown:')
        print(f'{" ".join(map(str, ukn))}')

        ctl.solve()


sys.exit(clingo_main(LevelApp(), sys.argv[1:]))
