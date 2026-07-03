#program step(k).

#external vol(k).
p(k).
:- vol(k), not 5 { p(K) : K <= k }.

#script (python)

from clingo import Number, Function

def main(prg):
    step = 0
    while True:
        if step > 0: prg.release_external(Function("vol", [Number(step-1)]))
        prg.ground([("step", [Number(step)])])
        prg.assign_external(Function("vol", [Number(step)]), True)
        ret = prg.solve()
        if ret.satisfiable is True:
            break
        step += 1

#end.
