#script (python)

import sys
from clingo import Function

def main(prg):
    prg.ground([("base", [])])
    prg.solve(assumptions=[(Function("a"), True)])

#end.

{ a }.
b :- a.
