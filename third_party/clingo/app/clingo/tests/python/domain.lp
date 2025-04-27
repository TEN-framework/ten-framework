p(1).
{ p(3) }. 
#external p(1..3).

q(X) :- p(X).

#script (python)

import sys
from clingo import Number, Function

def main(prg):
    prg.ground([("base", [])])
    print ("Solving...")
    print ("Answer: 1")
    out = ("universe(" + str(len(prg.symbolic_atoms)) + ")")
    for x in prg.symbolic_atoms:
        out = out + " domain(" + str(x.symbol) + "," + str(x.is_fact).lower() + "," + str(x.is_external).lower() + ")"
    out = out + " in_domain(p(2)," + str(prg.symbolic_atoms[Function("p", [Number(2)])] is not None).lower() + ")"
    out = out + " in_domain(p(4)," + str(prg.symbolic_atoms[Function("p", [Number(4)])] is not None).lower() + ")"
    for x in prg.symbolic_atoms.by_signature("p", 1):
        out = out + " domain_of_p(" + str(x.symbol) + "," + str(x.is_fact).lower() + "," + str(x.is_external).lower() + ")"
    for x in prg.symbolic_atoms.signatures:
        out = out + " sig(" + x[0] + "," + str(x[1]) + ")"
    print (out)
    sys.stdout.flush()

#end. 
