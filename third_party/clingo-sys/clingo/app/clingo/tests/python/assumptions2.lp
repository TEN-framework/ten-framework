#script (python)

import sys
from clingo import Function

def main(prg):
    prg.ground([("base", [])])
    prg.solve(assumptions=[(Function("b"), True)])

#end.

{ a }.
b :- a.
