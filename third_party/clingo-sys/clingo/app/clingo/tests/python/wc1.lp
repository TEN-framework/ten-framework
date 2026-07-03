#script (python)

import clingo

class Propagator:
    def init(self, init):
        a = init.solver_literal(init.symbolic_atoms[clingo.Function("a")].literal)
        b = init.solver_literal(init.symbolic_atoms[clingo.Function("b")].literal)
        c = init.solver_literal(init.symbolic_atoms[clingo.Function("c")].literal)
        init.add_weight_constraint(a, [(b, 1), (c, 1)], 1, 1, False)

def main(ctl):
    ctl.register_propagator(Propagator())
    ctl.ground([("base", [])])
    ctl.solve()

#end.

{ a;b;c }.
