#script (python)

import clingo

def p(n):
    return clingo.Function("p", [clingo.Number(n)])

def main(prg):
    bot = 100
    lit = lambda sym: prg.symbolic_atoms[sym].literal
    prg.ground([("base", [])])
    prg.solve(assumptions=[(p(3), False), lit(p(4))])
    prg.solve(assumptions=[(p(3), True), -lit(p(4))])
    
    prg.assign_external(p(1), True)
    prg.solve(assumptions=[-lit(p(3)), -lit(p(4))])
    prg.assign_external(-lit(p(1)), True)
    prg.solve(assumptions=[-lit(p(3)), -lit(p(4))])
    prg.assign_external(lit(p(1)), True)
    prg.solve(assumptions=[-lit(p(3)), -lit(p(4))])

    prg.solve(assumptions=[-lit(p(3)), -lit(p(4)), bot])
    prg.solve(assumptions=[-lit(p(3)), -lit(p(4)), -bot])
    
    # note: this actually creates the external
    #       hence, the subsequent call with the assumption is unsatisfiable
    prg.assign_external(bot, True)
    prg.solve(assumptions=[-lit(p(3)), -lit(p(4)), -bot])

#end.

#external p(1..2).
{p(3..4)}.
