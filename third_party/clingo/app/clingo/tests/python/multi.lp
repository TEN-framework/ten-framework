#script (python)

def main(prg):
    prg.ground([("a", [])])
    prg.solve()
    prg.ground([("b", [])])
    prg.solve()

#end.

#program a.

2 { p(1..3) }.

#program b.

:- p(X).

