#script (python)

from clingo import Function, Number

def main(prg):
    prg.ground([("base", [])])
    print ("universe: " + str(len(prg.symbolic_atoms)))
    for x in prg.symbolic_atoms:
        print (x.symbol, x.is_fact, x.is_external)
    print ("p(2) is in domain: " + str(prg.symbolic_atoms[Function("p", [Number(2)])] is not None))
    print ("p(4) is in domain: " + str(prg.symbolic_atoms[Function("p", [Number(4)])] is not None))
    print ("domain of p/1")
    for x in prg.symbolic_atoms.by_signature("p", 1):
        print (x.symbol, x.is_fact, x.is_external)
    print ("signatures")
    for x in prg.symbolic_atoms.signatures:
        print (("~" if not x[2] else "") + x[0] + "/" + str(x[1]))

#end. 
