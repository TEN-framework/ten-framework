{a;b}.

#script (python)

import clingo

def main(prg):
    prg.ground([("base", [])])
    with prg.backend() as be:
        a = be.add_atom(clingo.Function("a"))
        b = be.add_atom(clingo.Function("b"))
        be.add_assume([-a, b]);
    prg.solve()

#end.
