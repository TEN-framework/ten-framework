{a;b}.

#script (python)

import clingo

def main(prg):
    prg.ground([("base", [])])
    with prg.backend() as be:
        a = be.add_atom(clingo.Function("a"))
        b = be.add_atom(clingo.Function("b"))
        be.add_acyc_edge(1, 2, [a]);
        be.add_acyc_edge(2, 1, [b]);
    prg.solve()

#end.
