#script (python)

def main(prg):
    prg.configuration.solve.models = 0
    prg.ground([("base", [])])
    prg.enable_enumeration_assumption = True
    print("===================== Solve ========================")
    prg.solve()
    prg.enable_enumeration_assumption = False
    print("===================== Solve ========================")
    prg.solve()
    print("===================== Solve ========================")
    prg.solve()

#end.

{ a; b; c }.
