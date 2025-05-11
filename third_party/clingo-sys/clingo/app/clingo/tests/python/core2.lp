#script (python)

from clingo import Function

def main(prg):
    cores = []
    prg.ground([("base", [])])
    a = prg.symbolic_atoms[Function("a")].literal
    prg.solve(assumptions=[-a], on_core=lambda core: cores.append(core))
    with prg.backend() as backend:
        for lit in cores[-1]:
            backend.add_rule([], [-lit])
    cores = []
    prg.solve(on_core=lambda core: cores.append(core))
    assert cores and not cores[-1]

#end.

a.
