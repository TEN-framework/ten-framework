#script(python)

def main(prg):
    print("#const y={0}".format(prg.get_const("y")))
    prg.load("program.lp")
    print("#const y={0}".format(prg.get_const("y")))
    print("#const x={0}".format(prg.get_const("x")))
    prg.add("base", [], "#const x=10. b.")
    print("#const x={0}".format(prg.get_const("x")))
    prg.ground([("base", [])])
    prg.solve()

#end.

c.
