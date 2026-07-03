{a;b}.

#script (python)

import clingo

def main(prg):
    prg.configuration.solve.models = "1"
    prg.configuration.solve.parallel_mode = "1"
    prg.configuration.solver.heuristic = "domain"
    prg.ground([("base", [])])
    with prg.backend() as be:
        a = be.add_atom(clingo.Function("a"))
        b = be.add_atom(clingo.Function("b"))
        be.add_heuristic(a, clingo.HeuristicType.True_, 1, 1, []);
        be.add_heuristic(b, clingo.HeuristicType.False_, 1, 1, []);
    prg.solve()

#end.
