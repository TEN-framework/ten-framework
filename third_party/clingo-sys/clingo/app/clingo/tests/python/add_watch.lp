#script (python)

import clingo

class Propagator:
    def init(self, init):
        a = init.solver_literal(init.symbolic_atoms[clingo.Function('a')].literal)
        b = init.solver_literal(init.symbolic_atoms[clingo.Function('b')].literal)
        c = init.solver_literal(init.symbolic_atoms[clingo.Function('c')].literal)
        init.add_watch(a, 0)
        ass = init.assignment
        assert(ass.value(a) == None)
        assert(ass.is_false(b))
        assert(ass.is_true(c))
        self.__a = a

    def propagate(self, ctl, changes):
        assert(ctl.thread_id == 0)
        for lit in changes:
            assert(lit == self.__a)

def main(prg):
    prg.register_propagator(Propagator())
    prg.ground([("base", [])])
    prg.solve()

#end.

{a;b}.
:- b.
c.
