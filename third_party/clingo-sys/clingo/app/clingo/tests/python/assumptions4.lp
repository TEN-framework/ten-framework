#script (python)

import sys
from clingo import Function

def main(prg):
    prg.ground([("base", [])])
    prg.solve(assumptions=[(Function("a"), True)])

    with prg.solve(assumptions=[(Function("a"), True)], async_=True) as handle:
        handle.get()

    with prg.solve(assumptions=[(Function("a"), True)], yield_=True) as handle:
        for model in handle:
            pass

#end.

{ a }.
b :- a.
