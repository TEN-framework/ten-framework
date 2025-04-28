#script (python)

import clingo
import sys

def writeln(s):
    sys.stdout.write(str(s))
    sys.stdout.write("\n")
    sys.stdout.flush()

def on_model(x):
    writeln("Answer: 42")
    writeln(
          "hasA(" +        str(x.contains(clingo.Function("a"))).lower() + ") "
        + "hasVolatile(" + str(x.contains(clingo.Function("volatile", [clingo.Number(9)]))).lower() + ") "
        + "model(" +       ",".join(sorted(map(str, x.symbols(shown=True)))) + ") ")

def on_finish(ret):
    writeln("Answer: 42")
    writeln("on_finish")

def main (prg):
    n = prg.get_const("n").number
    parts = []
    parts.append(("base", []))
    prg.ground(parts)
    prg.solve()
    prg.add("test", ["x"], "test(x).")
    parts.append(("test", [clingo.Function("f", [clingo.Number(1),clingo.Number(2)])]))
    for i in range(1,n+1):
        parts.append(("cumulative", [clingo.Number(i)]))
        prg.ground(parts)
        parts = []
        with prg.solve(on_model=on_model, on_finish=on_finish, async_=True) as handle:
            handle.wait(0)
            ret = handle.get()
        writeln("Answer: 42")
        if ret.satisfiable:
            writeln("SAT")
        elif ret.unsatisfiable:
            writeln("UNSAT")
        else:
            writeln("UNKNOWN")

#end.

#const n = 10.

#program base.

a.

#program cumulative(k).

1 { c(k) } 1.
q(k) :- c(k).
r(k,X) :- test(X).

#external volatile(k).
 :- not c(5), not volatile(k).
volatile(k-1).

