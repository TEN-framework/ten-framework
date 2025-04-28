#script (python) 

import clingo

def main(prg):
    prg.ground([("pigeon", [])])
    for i in range(0,2):
        prg.assign_external(clingo.Function("p"), True)
        prg.solve()
        prg.assign_external(clingo.Function("p"), False)
        with prg.solve(async_=True) as handle:
            handle.wait(0.01)
            prg.interrupt()

#end.

#const n = 190.

#program pigeon.
#external p.

1 { p(X); q(X) } 1 :- X = 1..n, not p.
:- not n+1 { p(1..n); q(1..n) }, not p.
