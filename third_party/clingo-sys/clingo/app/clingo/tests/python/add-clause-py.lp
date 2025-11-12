#script (python)

import clingo

class Propagator:
    def __init__(self):
        self.step = 0

    def init(self, init):
        a = init.solver_literal(init.symbolic_atoms[clingo.Function("a")].literal)
        b = init.solver_literal(init.symbolic_atoms[clingo.Function("b")].literal)
        c = init.solver_literal(init.symbolic_atoms[clingo.Function("c")].literal)
        if self.step == 0:
            l = init.add_literal()
            init.add_clause([-b, l])
            init.propagate()
            init.add_clause([-l, b])
            init.propagate()
            init.add_clause([-c, l])
            init.propagate()
            init.add_clause([-l, c])
            init.propagate()
            init.add_clause([-a, -b])
            init.propagate()
            init.add_clause([-a,  b])
            init.propagate()
            init.add_clause([-a, -a, -b])
            init.propagate()
            init.add_clause([-a, a])
            init.propagate()
        if self.step == 1:
            init.add_clause([b])
            init.propagate()
            init.add_clause([b])
            init.propagate()
        if self.step == 2:
            init.add_clause([])
            init.propagate()
            init.add_clause([a])
            init.propagate()
            init.add_clause([b])
            init.propagate()
            init.add_clause([-a])
            init.propagate()
            init.add_clause([-b])
            init.propagate()
        self.step += 1

def main(prg):
    prg.register_propagator(Propagator())
    prg.ground([("base", [])])
    prg.solve()
    prg.solve()
    prg.solve()

#end.

{ a; b; c }.
