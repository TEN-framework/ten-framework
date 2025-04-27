#script(python)

from clingo import Number

brave = set()

def on_model(m):
    for x in m.symbols(atoms=True):
        if x.name == "holds": brave.add(x.arguments[0])

def is_brave(x):
    return Number(x in brave)

def main(prg):
    prg.ground([("base", [])])
    while prg.solve(on_model=on_model).satisfiable:
        prg.ground([("brave", [])])
    print ("brave consequences: {0}".format(", ".join(str(x) for x in sorted(brave))))

#end.

#program brave.

:- not holds(X) : atom(X), @is_brave(X) == 0.

