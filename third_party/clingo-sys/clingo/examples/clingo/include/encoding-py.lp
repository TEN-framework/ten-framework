#script (python)

from clingo import Number

def main(prg):
    prg.ground([("base", []), ("p", [Number(42)])])
    prg.solve()

#end.
#program p(t).

a(t).

#include "include.lp".
