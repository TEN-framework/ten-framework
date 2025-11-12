#script (python)

import clingo

def main(prg):
    def on_model(m):
        m.context.add_clause([(clingo.Function("a"), False), (clingo.Function("b"), False)])
        m.context.add_nogood([(clingo.Function("c"), True), (clingo.Function("d"), True)])

    prg.configuration.solve.models = 0
    prg.ground([("base", [])])
    prg.solve(on_model=on_model)
    print ("and again")
    prg.solve()

#end.

2 { a; b; c; d }.
