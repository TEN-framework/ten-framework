#script (python)

import clingo

class Propagator:
    def __init__(self):
        self.__lits = set()

    def init(self, init):
        for atom in init.symbolic_atoms:
            sym = atom.symbol
            if sym.name == "p" and len(sym.arguments) == 1:
                self.__lits.add(init.solver_literal(atom.literal))
        assert(init.check_mode == clingo.PropagatorCheckMode.Total)
        assert(init.check_mode != clingo.PropagatorCheckMode.Fixpoint)
        assert(init.check_mode != clingo.PropagatorCheckMode.Off)
        init.check_mode = clingo.PropagatorCheckMode.Fixpoint
        assert(init.check_mode == clingo.PropagatorCheckMode.Fixpoint)

    def check(self, ctl):
        # assert(not ctl.assignment.is_total)
        for lit in self.__lits:
            if ctl.assignment.value(lit) is None:
                ctl.add_clause([lit])
                break

def main(prg):
    p = Propagator()
    prg.register_propagator(p)
    prg.ground([("base", [])])
    prg.solve()

#end.

{ p(1..10) }.
