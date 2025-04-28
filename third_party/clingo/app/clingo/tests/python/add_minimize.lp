#script (python)

import clingo

class Propagator:
    def init(self, init):
        a = init.solver_literal(init.symbolic_atoms[clingo.Function("a")].literal)
        b = init.solver_literal(init.symbolic_atoms[clingo.Function("b")].literal)
        c = init.solver_literal(init.symbolic_atoms[clingo.Function("c")].literal)
        d = init.solver_literal(init.symbolic_atoms[clingo.Function("d")].literal)
        init.add_minimize(a, 1)
        init.add_minimize(b, 1)
        init.add_minimize(c, 1)
        init.add_minimize(d, 1)

def main(prg):
    prg.register_propagator(Propagator())
    prg.ground([("base", [])])
    prg.solve()

#end.

2 { a; b; c }.
2 { b; c; d }.

