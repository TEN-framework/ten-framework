#script (python)

import clingo

class Propagator:
    def init(self, init):
        a = init.solver_literal(init.symbolic_atoms[clingo.Function("a")].literal)
        b = init.solver_literal(init.symbolic_atoms[clingo.Function("b")].literal)
        c = init.solver_literal(init.symbolic_atoms[clingo.Function("c")].literal)
        l = init.add_literal()
        init.add_weight_constraint( l, ((a, 1), (b, 1)), 2)
        init.add_weight_constraint(-l, ((b, 1), (c, 1)), 2)

def main(prg):
    prg.register_propagator(Propagator())
    prg.ground([("base", [])])
    prg.solve()

#end.

{ a; b; c }.

