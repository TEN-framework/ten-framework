#script (python)

import sys
from clingo import Function

def main(prg):
    prg.ground([("base", [])])
    prg.assign_external(Function("a"), True)
    prg.solve()
    prg.assign_external(Function("a"), False)
    prg.solve()
    prg.assign_external(Function("a"), None)
    prg.solve()
    prg.release_external(Function("a"))
    prg.solve()

#end.

#external a.
