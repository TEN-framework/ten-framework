#script (python)

import clingo
from itertools import chain

def check_lit(assignment, l):
    trail = assignment.trail
    t = assignment.value(l)
    if t is not None:
        s = 1
        if not t:
            s = -1
        n = 0
        for k in trail:
            if k == s * l:
                n = n + 1
        assert n == 1

def check_trail(assignment):
    trail = assignment.trail

    n = 0
    for i in range(0, assignment.decision_level + 1):
        check = False
        for j in range(trail.begin(i), trail.end(i)):
            check = True
            assert assignment.is_true(trail[j])
            n = n + 1
        assert check
    assert n == len(trail)

    n = 0
    for l in assignment:
        n = n + 1
        check_lit(assignment, l)
    assert n == len(assignment)

    n = 0
    for l in chain(assignment[0::2], assignment[1::2]):
        n = n + 1
    assert n == len(assignment)

    n = 0
    for l in chain(trail[0::2], trail[1::2]):
        n = n + 1
    assert n == len(trail)

    n = 1
    for l in trail[0:-1]:
        n = n + 1
    assert n == len(trail)

    n = 0
    for l in trail[-1:-1-len(trail):-1]:
        n = n + 1
    assert n == len(trail)

    n = 0
    for l in chain(trail[-1:-1-len(trail):-2], trail[-2:-1-len(trail):-2]):
        n = n + 1
    assert n == len(trail)

class Propagator:
    def init(self, init):
        check_trail(init.assignment)

        init.check_mode = clingo.PropagatorCheckMode.Fixpoint

    def check(self, control):
        check_trail(control.assignment)

def main(prg):
    prg.register_propagator(Propagator())
    prg.ground([("base", [])])
    prg.solve()

#end.

{ a; b; c }.
