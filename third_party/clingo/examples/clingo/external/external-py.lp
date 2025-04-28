#script(python)

from clingo import Number, Function

def main(prg):
    prg.ground([("base", [])])
    prg.assign_external(Function("p", [Number(3)]), True)
    prg.solve()
    prg.assign_external(Function("p", [Number(3)]), False)
    prg.solve()
    prg.ground([("succ", [Number(1)]), ("succ", [Number(2)])])
    prg.solve()
    prg.ground([("succ", [Number(3)])])
    prg.solve()
#end.

