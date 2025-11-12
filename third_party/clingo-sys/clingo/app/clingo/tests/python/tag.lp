#script(python)
class Tag:
    def __init__(self):
        self.__add = True

    def flip(self):
        self.__add = not self.__add

    def init(self, init):
        for atom in init.symbolic_atoms:
            init.add_watch(init.solver_literal(atom.literal))
            init.add_watch(-init.solver_literal(atom.literal))

    def propagate(self, control, changes):
        if self.__add: control.add_clause([], tag=True)
        return True

    def undo(self, thread_id, assign, undo):
        pass

    def check(self, control):
        return True

def main(prg):
    prg.ground([("base", [])])
    p = Tag()
    prg.register_propagator(p)
    prg.solve()
    p.flip()
    prg.solve()

#end.

{a}.
