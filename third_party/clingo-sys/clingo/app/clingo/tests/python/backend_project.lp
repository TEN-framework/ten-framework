{a;b}.

#show.

#script (python)

import clingo

def main(prg):
    prg.ground([("base", [])])
    with prg.backend() as be:
        a = be.add_atom(clingo.Function("a"))
        be.add_project([a]);
    prg.solve()

#end.
