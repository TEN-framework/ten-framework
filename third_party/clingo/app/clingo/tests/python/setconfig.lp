#script (python)

import clingo

def main(prg):
    prg.ground([("step1", [])])
    prg.assign_external(clingo.Function("f"), True)
    prg.configuration.solve.models = "1"
    prg.solve()
    prg.ground([("step2", [])])
    prg.assign_external(clingo.Function("f"), False)
    prg.configuration.solve.models = "0"
    prg.solve()

#end.

#program step1.

#external f.
{ a; b; c }.

#show f/0.

#program step2.

#show a : a.
#show b : b.
#show c : c.

